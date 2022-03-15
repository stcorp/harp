/*
 * Copyright (C) 2015-2022 S[&]T, The Netherlands.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "coda.h"
#include "harp-ingestion.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_UNIT_LENGTH 30

#define MAX_NAME_LENGTH 80
#define MAX_DESCRIPTION_LENGTH 100
#define MAX_PATH_LENGTH 100
#define MAX_MAPPING_LENGTH 100

typedef struct ingest_info_struct
{
    const harp_product_definition *definition;
    int product_version;
    coda_product *product;
    const char *template;
    long num_time;
    int has_effective_temperature;
} ingest_info;

static int read_dimensions(void *user_data, long dimension[HARP_NUM_DIM_TYPES])
{
    ingest_info *info = (ingest_info *)user_data;

    dimension[harp_dimension_time] = info->num_time;

    return 0;
}

static int get_optional_variable_availability(ingest_info *info)
{
    coda_cursor cursor;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    info->has_effective_temperature = (coda_cursor_goto(&cursor, "/TEMPERATURE_EFFECTIVE_O3") == 0);

    return 0;
}

static int read_attribute(void *user_data, const char *path, harp_array data)
{
    coda_cursor cursor;
    long length;

    if (coda_cursor_set_product(&cursor, ((ingest_info *)user_data)->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, path) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_string_length(&cursor, &length) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    data.string_data[0] = malloc(length + 1);
    if (data.string_data[0] == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)", length + 1,
                       __FILE__, __LINE__);
        return -1;
    }
    if (coda_cursor_read_string(&cursor, data.string_data[0], length + 1) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    return 0;
}

static int read_variable_double(void *user_data, const char *path, long num_elements, harp_array data)
{
    coda_cursor cursor;
    long actual_num_elements, i;
    double fill_value;

    if (coda_cursor_set_product(&cursor, ((ingest_info *)user_data)->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, path) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &actual_num_elements) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (actual_num_elements != num_elements)
    {
        harp_set_error(HARP_ERROR_INGESTION, "variable %s has %ld elements (expected %ld)", path, actual_num_elements,
                       num_elements);
        return -1;
    }
    if (coda_cursor_read_double_array(&cursor, data.double_data, coda_array_ordering_c) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, "@VAR_FILL_VALUE") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_double(&cursor, &fill_value) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (!harp_isnan(fill_value))
    {
        for (i = 0; i < num_elements; i++)
        {
            if (data.double_data[i] == fill_value)
            {
                data.double_data[i] = harp_nan();
            }
        }
    }

    return 0;
}

static int read_variable_float(void *user_data, const char *path, long num_elements, harp_array data)
{
    coda_cursor cursor;
    long actual_num_elements, i;
    float fill_value;

    if (coda_cursor_set_product(&cursor, ((ingest_info *)user_data)->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, path) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &actual_num_elements) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (actual_num_elements != num_elements)
    {
        harp_set_error(HARP_ERROR_INGESTION, "variable %s has %ld elements (expected %ld)", path, actual_num_elements,
                       num_elements);
        return -1;
    }
    if (coda_cursor_read_float_array(&cursor, data.float_data, coda_array_ordering_c) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, "@VAR_FILL_VALUE") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_float(&cursor, &fill_value) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (!harp_isnan(fill_value))
    {
        for (i = 0; i < num_elements; i++)
        {
            if (data.float_data[i] == fill_value)
            {
                data.float_data[i] = harp_nan();
            }
        }
    }

    return 0;
}

static int read_data_source(void *user_data, harp_array data)
{
    return read_attribute(user_data, "@DATA_SOURCE", data);
}

static int read_data_location(void *user_data, harp_array data)
{
    return read_attribute(user_data, "@DATA_LOCATION", data);
}

static int read_datetime(void *user_data, harp_array data)
{
    return read_variable_double(user_data, "DATETIME", ((ingest_info *)user_data)->num_time, data);
}

static int read_instrument_latitude(void *user_data, harp_array data)
{
    return read_variable_float(user_data, "LATITUDE_INSTRUMENT", 1, data);
}

static int read_instrument_longitude(void *user_data, harp_array data)
{
    return read_variable_float(user_data, "LONGITUDE_INSTRUMENT", 1, data);
}

static int read_instrument_altitude(void *user_data, harp_array data)
{
    return read_variable_float(user_data, "ALTITUDE_INSTRUMENT", 1, data);
}

static int read_solar_azimuth_angle(void *user_data, harp_array data)
{
    return read_variable_float(user_data, "ANGLE_SOLAR_AZIMUTH", ((ingest_info *)user_data)->num_time, data);
}

static int read_solar_zenith_angle(void *user_data, harp_array data)
{
    return read_variable_float(user_data, "ANGLE_SOLAR_ZENITH", ((ingest_info *)user_data)->num_time, data);
}

static int read_column(void *user_data, harp_array data)
{
    return read_variable_float(user_data, "O3_COLUMN_ABSORPTION_SOLAR", ((ingest_info *)user_data)->num_time, data);
}

static int read_column_uncertainty(void *user_data, harp_array data)
{
    return read_variable_float(user_data, "O3_COLUMN_ABSORPTION_SOLAR_UNCERTAINTY_COMBINED_STANDARD",
                               ((ingest_info *)user_data)->num_time, data);
}

static int read_column_amf(void *user_data, harp_array data)
{
    return read_variable_float(user_data, "O3_COLUMN_ABSORPTION_SOLAR_AMF", ((ingest_info *)user_data)->num_time, data);
}

static int read_effective_temperature(void *user_data, harp_array data)
{
    return read_variable_float(user_data, "TEMPERATURE_EFFECTIVE_O3", ((ingest_info *)user_data)->num_time, data);
}

static int include_effective_temperature(void *user_data)
{
    return ((ingest_info *)user_data)->has_effective_temperature;
}

static void ingestion_done(void *user_data)
{
    free(user_data);
}

static int get_product_definition(const harp_ingestion_module *module, coda_product *product,
                                  harp_product_definition **definition)
{
    coda_cursor cursor;
    char template_name[35];
    char data_source[30];
    long length;
    long i;

    if (coda_cursor_set_product(&cursor, product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, "@DATA_TEMPLATE") != 0)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, "could not find DATA_TEMPLATE global attribute");
        return -1;
    }
    if (coda_cursor_get_string_length(&cursor, &length) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    /* template should match the pattern "GEOMS-TE-UVVIS-BREWER-TOTALCOL-xxx" */
    if (length != 34)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, "invalid string length for DATA_TEMPLATE global attribute");
        return -1;
    }
    if (coda_cursor_read_string(&cursor, template_name, 35) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (strncmp(template_name, "GEOMS-TE-UVVIS-BREWER-TOTALCOL-", 31) != 0)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, "invalid GEOMS template name '%s", template_name);
        return -1;
    }

    if (coda_cursor_goto(&cursor, "/@DATA_SOURCE") != 0)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, "could not find DATA_SOURCE global attribute");
        return -1;
    }
    if (coda_cursor_read_string(&cursor, data_source, 30) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    /* data source should match the pattern "UVVIS.BREWER_xxxx" */
    if (strncmp(data_source, "UVVIS.BREWER_", 13) != 0)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, "DATA_SOURCE global attribute has an invalid value");
        return -1;
    }

    for (i = 0; i < module->num_product_definitions; i++)
    {
        if (strncmp(template_name, module->product_definition[i]->name, length) == 0)
        {
            *definition = module->product_definition[i];
            return 0;
        }
    }
    harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, "GEOMS template '%s' not supported", template_name);

    return -1;
}

static int get_dimensions(ingest_info *info)
{
    coda_cursor cursor;
    double values[2];

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, "/DATETIME") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &info->num_time) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (info->num_time > 1)
    {
        if (coda_cursor_read_double_partial_array(&cursor, 0, 2, values) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (values[1] < values[0])
        {
            harp_set_error(HARP_ERROR_INGESTION, "time dimension should use a chronological ordering");
            return -1;
        }
    }

    return 0;
}

static int ingestion_init(const harp_ingestion_module *module, coda_product *product,
                          const harp_ingestion_options *options, harp_product_definition **definition, void **user_data)
{
    ingest_info *info;

    (void)options;

    info = malloc(sizeof(ingest_info));
    if (info == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(ingest_info), __FILE__, __LINE__);
        return -1;
    }

    info->definition = NULL;
    info->product = product;

    if (coda_get_product_version(product, &info->product_version) != 0)
    {
        ingestion_done(info);
        return -1;
    }
    /* the lower 3 digits provide the template version number */
    info->product_version = info->product_version % 1000;

    if (get_product_definition(module, info->product, definition) != 0)
    {
        ingestion_done(info);
        return -1;
    }
    info->definition = *definition;

    if (get_dimensions(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }

    if (get_optional_variable_availability(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }

    *user_data = info;
    return 0;
}

static int init_product_definition(harp_ingestion_module *module, int version)
{
    harp_variable_definition *variable_definition;
    harp_product_definition *product_definition;
    harp_dimension_type dimension_type[4];
    char product_name[MAX_NAME_LENGTH];
    char product_description[MAX_DESCRIPTION_LENGTH];
    const char *description;

    snprintf(product_name, MAX_NAME_LENGTH, "GEOMS-TE-UVVIS-BREWER-TOTALCOL-%03d", version);
    snprintf(product_description, MAX_NAME_LENGTH, "GEOMS template for UVVIS Brewer measurements v%03d", version);
    product_definition = harp_ingestion_register_product(module, product_name, product_description, read_dimensions);

    dimension_type[0] = harp_dimension_time;

    /* sensor_name */
    description = "name of the sensor";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "sensor_name",
                                                                     harp_type_string, 0, NULL, NULL, description, NULL,
                                                                     NULL, read_data_source);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/@DATA.SOURCE", NULL);

    /* location_name */
    description = "name of the site at which the sensor is located";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "location_name",
                                                                     harp_type_string, 0, NULL, NULL, description, NULL,
                                                                     NULL, read_data_location);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/@DATA.LOCATION", NULL);

    /* datetime */
    description = "mean time of the measurement";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double,
                                                                     1, dimension_type, NULL, description,
                                                                     "days since 2000-01-01", NULL, read_datetime);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/DATETIME", NULL);

    /* sensor_latitude */
    description = "latitude of the sensor";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "sensor_latitude",
                                                                     harp_type_float, 0, NULL, NULL, description,
                                                                     "degree_north", NULL, read_instrument_latitude);
    harp_variable_definition_set_valid_range_float(variable_definition, -90.0, 90.0);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/LATITUDE.INSTRUMENT", NULL);

    /* sensor_longitude */
    description = "longitude of the sensor";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "sensor_longitude",
                                                                     harp_type_float, 0, NULL, NULL, description,
                                                                     "degree_east", NULL, read_instrument_longitude);
    harp_variable_definition_set_valid_range_float(variable_definition, -180.0, 180.0);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/LONGITUDE.INSTRUMENT", NULL);

    /* sensor_altitude */
    description = "altitude of the sensor relative to the location site";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "sensor_altitude",
                                                                     harp_type_float, 0, NULL, NULL, description, "m",
                                                                     NULL, read_instrument_altitude);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ALTITUDE.INSTRUMENT", NULL);

    /* solar_zenith_angle */
    description = "solar zenith angle";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "solar_zenith_angle",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     description, "degree", NULL,
                                                                     read_solar_zenith_angle);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ANGLE.SOLAR_ZENITH", NULL);

    /* solar_azimuth_angle */
    description = "solar azimuth angle";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "solar_azimuth_angle",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     description, "degree", NULL,
                                                                     read_solar_azimuth_angle);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ANGLE.SOLAR_AZIMUTH", NULL);

    /* O3_column_number_density */
    description = "O3 column number density";
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, "O3_column_number_density", harp_type_float, 1, dimension_type, NULL, description, "DU",
         NULL, read_column);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/O3.COLUMN_ABSORPTION.SOLAR", NULL);

    /* O3_column_number_density_uncertainty */
    description = "uncertainty of the O3 column number density";
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, "O3_column_number_density_uncertainty", harp_type_float, 1, dimension_type, NULL,
         description, "DU", NULL, read_column_uncertainty);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/O3.COLUMN_ABSORPTION.SOLAR_UNCERTAINTY.COMBINED.STANDARD", NULL);

    /* O3_column_number_density_amf */
    description = "air mass factor of the O3 column number density";
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, "O3_column_number_density_amf", harp_type_float, 1, dimension_type, NULL,
         description, "1", NULL, read_column_amf);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/O3.COLUMN_ABSORPTION.SOLAR_AMF", NULL);

    /* O3_effective_temperature */
    description = "Effective temperature of the ozone layer";
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, "O3_effective_temperature", harp_type_float, 1, dimension_type, NULL,
         description, "1", include_effective_temperature, read_effective_temperature);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/TEMPERATURE.EFFECTIVE.O3", NULL);

    return 0;
}

int harp_ingestion_module_geoms_uvvis_brewer_init()
{
    harp_ingestion_module *module;

    module = harp_ingestion_register_module("GEOMS-TE-UVVIS-BREWER-TOTALCOL", "GEOMS", "GEOMS",
                                            "UVVIS_BREWER_TOTALCOL",
                                            "GEOMS template for UVVIS Brewer measurements", ingestion_init,
                                            ingestion_done);

    init_product_definition(module, 1);

    return 0;
}
