/*
 * Copyright (C) 2015-2020 S[&]T, The Netherlands.
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

#define MAX_NAME_LENGTH 80
#define MAX_DESCRIPTION_LENGTH 100

typedef struct ingest_info_struct
{
    const harp_product_definition *definition;
    coda_product *product;
    const char *template;
    long num_time;
    long num_vertical;
} ingest_info;

static int read_dimensions(void *user_data, long dimension[HARP_NUM_DIM_TYPES])
{
    ingest_info *info = (ingest_info *)user_data;

    dimension[harp_dimension_time] = info->num_time;
    dimension[harp_dimension_vertical] = info->num_vertical;

    return 0;
}

static int read_attribute(void *user_data, const char *path, harp_array data)
{
    coda_cursor cursor;
    long length;

    if (coda_cursor_set_product(&cursor, ((ingest_info *)user_data)->product) != 0)
    {
        return -1;
    }
    if (coda_cursor_goto(&cursor, path) != 0)
    {
        return -1;
    }
    if (coda_cursor_get_string_length(&cursor, &length) != 0)
    {
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

static int read_altitude(void *user_data, harp_array data)
{
    return read_variable_double(user_data, "ALTITUDE", ((ingest_info *)user_data)->num_vertical, data);
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

static int read_datetime_start(void *user_data, harp_array data)
{
    return read_variable_double(user_data, "DATETIME_START", ((ingest_info *)user_data)->num_time, data);
}

static int read_datetime_stop(void *user_data, harp_array data)
{
    return read_variable_double(user_data, "DATETIME_STOP", ((ingest_info *)user_data)->num_time, data);
}

static int read_instrument_altitude(void *user_data, harp_array data)
{
    return read_variable_double(user_data, "ALTITUDE_INSTRUMENT", 1, data);
}

static int read_instrument_latitude(void *user_data, harp_array data)
{
    return read_variable_double(user_data, "LATITUDE_INSTRUMENT", 1, data);
}

static int read_instrument_longitude(void *user_data, harp_array data)
{
    return read_variable_double(user_data, "LONGITUDE_INSTRUMENT", 1, data);
}

static int read_o3_nd_ad(void *user_data, harp_array data)
{
    return read_variable_double(user_data, "O3_NUMBER_DENSITY_ABSORPTION_DIFFERENTIAL",
                                ((ingest_info *)user_data)->num_vertical, data);
}

static int read_o3_nd_ad_uncertainty(void *user_data, harp_array data)
{
    return read_variable_double(user_data, "O3_NUMBER_DENSITY_ABSORPTION_DIFFERENTIAL_UNCERTAINTY_COMBINED_STANDARD",
                                ((ingest_info *)user_data)->num_vertical, data);
}

static int read_nd_bs(void *user_data, harp_array data)
{
    return read_variable_double(user_data, "NUMBER_DENSITY_BACKSCATTER",
                                ((ingest_info *)user_data)->num_vertical, data);
}

static int read_nd_bs_uncertainty(void *user_data, harp_array data)
{
    return read_variable_double(user_data, "NUMBER_DENSITY_BACKSCATTER_UNCERTAINTY_COMBINED_STANDARD",
                                ((ingest_info *)user_data)->num_vertical, data);
}

static int read_temp_bs(void *user_data, harp_array data)
{
    return read_variable_double(user_data, "TEMPERATURE_BACKSCATTER", ((ingest_info *)user_data)->num_vertical, data);
}

static int read_temp_bs_uncertainty(void *user_data, harp_array data)
{
    return read_variable_double(user_data, "TEMPERATURE_BACKSCATTER_UNCERTAINTY_COMBINED_STANDARD",
                                ((ingest_info *)user_data)->num_vertical, data);
}

static int read_h2o_vmr_bs(void *user_data, harp_array data)
{
    return read_variable_double(user_data, "H2O_MIXING_RATIO_VOLUME_BACKSCATTER",
                                ((ingest_info *)user_data)->num_time * ((ingest_info *)user_data)->num_vertical, data);
}

static int read_h2o_vmr_bs_uncertainty(void *user_data, harp_array data)
{
    return read_variable_double(user_data, "H2O_MIXING_RATIO_VOLUME_BACKSCATTER_UNCERTAINTY_COMBINED_STANDARD",
                                ((ingest_info *)user_data)->num_time * ((ingest_info *)user_data)->num_vertical, data);
}

static int read_relative_humidity(void *user_data, harp_array data)
{
    return read_variable_double(user_data, "HUMIDITY_RELATIVE_DERIVED",
                                ((ingest_info *)user_data)->num_time * ((ingest_info *)user_data)->num_vertical, data);
}

static int read_relative_humidity_uncertainty(void *user_data, harp_array data)
{
    return read_variable_double(user_data, "HUMIDITY_RELATIVE_DERIVED_UNCERTAINTY_COMBINED_STANDARD",
                                ((ingest_info *)user_data)->num_time * ((ingest_info *)user_data)->num_vertical, data);
}

static int read_pressure_ind(void *user_data, harp_array data)
{
    return read_variable_double(user_data, "PRESSURE_INDEPENDENT", ((ingest_info *)user_data)->num_vertical, data);
}

static int read_temperature_ind(void *user_data, harp_array data)
{
    return read_variable_double(user_data, "TEMPERATURE_INDEPENDENT", ((ingest_info *)user_data)->num_vertical, data);
}

static void ingestion_done(void *user_data)
{
    ingest_info *info = (ingest_info *)user_data;

    free(info);
}

static int get_product_definition(const harp_ingestion_module *module, coda_product *product,
                                  harp_product_definition **definition)
{
    coda_cursor cursor;
    char template_name[100];
    int i;

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
    if (coda_cursor_read_string(&cursor, template_name, 100) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    for (i = 0; i < module->num_product_definitions; i++)
    {
        if (strcmp(template_name, module->product_definition[i]->name) == 0)
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
        return -1;
    }
    if (coda_cursor_goto(&cursor, "/DATETIME") != 0)
    {
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &info->num_time) != 0)
    {
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

    if (coda_cursor_goto(&cursor, "/ALTITUDE") != 0)
    {
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &info->num_vertical) != 0)
    {
        return -1;
    }
    if (info->num_vertical > 1)
    {
        if (coda_cursor_read_double_partial_array(&cursor, 0, 2, values) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (values[1] < values[0])
        {
            harp_set_error(HARP_ERROR_INGESTION, "vertical dimension should be ordered using increasing altitude");
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

    *user_data = info;
    return 0;
}

static void register_common_variables(harp_product_definition *product_definition, int with_temperature)
{
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1];
    const char *description;

    dimension_type[0] = harp_dimension_time;

    /* sensor_name */
    description = "name of the sensor";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "sensor_name",
                                                                     harp_type_string, 0, NULL, NULL, description, NULL,
                                                                     NULL, read_data_source);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/@DATA.SOURCE", NULL);

    /* site_name */
    description = "name of the site at which the sensor is located";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "site_name", harp_type_string,
                                                                     0, NULL, NULL, description, NULL, NULL,
                                                                     read_data_location);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/@DATA.LOCATION", NULL);

    /* sensor_latitude */
    description = "latitude of the sensor";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "sensor_latitude",
                                                                     harp_type_double, 0, NULL, NULL, description,
                                                                     "degree_north", NULL, read_instrument_latitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/LATITUDE.INSTRUMENT", NULL);

    /* sensor_longitude */
    description = "longitude of the sensor";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "sensor_longitude",
                                                                     harp_type_double, 0, NULL, NULL, description,
                                                                     "degree_east", NULL, read_instrument_longitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0, 180.0);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/LONGITUDE.INSTRUMENT", NULL);

    /* sensor_altitude */
    description = "altitude of the sensor";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "sensor_altitude",
                                                                     harp_type_double, 0, NULL, NULL, description, "m",
                                                                     NULL, read_instrument_altitude);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ALTITUDE.INSTRUMENT", NULL);

    /* datetime */
    description = "time of the measurement";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double,
                                                                     1, dimension_type, NULL, description,
                                                                     "days since 2000-01-01", NULL, read_datetime);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/DATETIME", NULL);

    /* datetime_start */
    description = "start time of the measurement";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "datetime_start",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, "days since 2000-01-01", NULL,
                                                                     read_datetime_start);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/DATETIME.START", NULL);

    /* datetime_stop */
    description = "stop time of the measurement";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "datetime_stop",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, "days since 2000-01-01", NULL,
                                                                     read_datetime_stop);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/DATETIME.STOP", NULL);

    dimension_type[0] = harp_dimension_vertical;

    /* altitude */
    description = "altitude of the measurement";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "altitude", harp_type_double,
                                                                     1, dimension_type, NULL, description, "m", NULL,
                                                                     read_altitude);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ALTITUDE", NULL);

    /* pressure */
    description = "pressure profile from independent source";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "pressure", harp_type_double,
                                                                     1, dimension_type, NULL, description, "hPa", NULL,
                                                                     read_pressure_ind);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/PRESSURE_INDEPENDENT", NULL);

    if (with_temperature)
    {
        /* temperature */
        description = "temperature profile from independent source";
        variable_definition = harp_ingestion_register_variable_full_read(product_definition, "temperature",
                                                                         harp_type_double, 1, dimension_type, NULL,
                                                                         description, "K", NULL, read_temperature_ind);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/TEMPERATURE_INDEPENDENT", NULL);
    }
}

static int init_o3_product_definition(harp_ingestion_module *module, int version)
{
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2];
    char product_name[MAX_NAME_LENGTH];
    char product_description[MAX_DESCRIPTION_LENGTH];
    const char *description;

    snprintf(product_name, MAX_NAME_LENGTH, "GEOMS-TE-LIDAR-O3-%03d", version);
    snprintf(product_description, MAX_DESCRIPTION_LENGTH, "GEOMS template for LIDAR ozone v%03d", version);
    product_definition = harp_ingestion_register_product(module, product_name, product_description, read_dimensions);

    dimension_type[0] = harp_dimension_time;
    dimension_type[1] = harp_dimension_vertical;

    register_common_variables(product_definition, 1);

    /* O3_number_density */
    description = "absorption differential O3 number density";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "O3_number_density",
                                                                     harp_type_double, 2, dimension_type, NULL,
                                                                     description, "molec/m3", NULL, read_o3_nd_ad);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/O3.NUMBER.DENSITY_ABSORPTION.DIFFERENTIAL",
                                         NULL);

    /* O3_number_density_uncertainty */
    description = "standard deviation of the absorption differential O3 number density";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition,
                                                                     "O3_number_density_uncertainty", harp_type_double,
                                                                     2, dimension_type, NULL, description, "molec/m3",
                                                                     NULL, read_o3_nd_ad_uncertainty);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/O3.NUMBER.DENSITY_ABSORPTION.DIFFERENTIAL_UNCERTAINTY.COMBINED.STANDARD",
                                         NULL);

    return 0;
}

static int init_temperature_product_definition(harp_ingestion_module *module, int version)
{
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2];
    char product_name[MAX_NAME_LENGTH];
    char product_description[MAX_DESCRIPTION_LENGTH];
    const char *description;

    snprintf(product_name, MAX_NAME_LENGTH, "GEOMS-TE-LIDAR-TEMPERATURE-%03d", version);
    snprintf(product_description, MAX_DESCRIPTION_LENGTH, "GEOMS template for LIDAR temperature v%03d", version);
    product_definition = harp_ingestion_register_product(module, product_name, product_description, read_dimensions);

    dimension_type[0] = harp_dimension_time;
    dimension_type[1] = harp_dimension_vertical;

    register_common_variables(product_definition, 0);

    /* temperature */
    description = "backscatter temperature";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "temperature",
                                                                     harp_type_double, 2, dimension_type, NULL,
                                                                     description, "K", NULL, read_temp_bs);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/TEMPERATURE_BACKSCATTER", NULL);

    /* temperature_uncertainty */
    description = "standard deviation of the backscatter temperature";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition,
                                                                     "temperature_uncertainty", harp_type_double,
                                                                     2, dimension_type, NULL, description, "K",
                                                                     NULL, read_temp_bs_uncertainty);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/TEMPERATURE_BACKSCATTER_UNCERTAINTY.COMBINED.STANDARD", NULL);

    /* number_density */
    description = "backscatter number density";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "number_density",
                                                                     harp_type_double, 2, dimension_type, NULL,
                                                                     description, "molec/m3", NULL, read_nd_bs);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/NUMBER.DENSITY_BACKSCATTER", NULL);

    /* number_density_uncertainty */
    description = "standard deviation of the backscatter number density";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition,
                                                                     "number_density_uncertainty", harp_type_double,
                                                                     2, dimension_type, NULL, description, "molec/m3",
                                                                     NULL, read_nd_bs_uncertainty);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/NUMBER.DENSITY_BACKSCATTER_UNCERTAINTY.COMBINED.STANDARD", NULL);

    return 0;
}

static int init_h2o_product_definition(harp_ingestion_module *module, int version)
{
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2];
    char product_name[MAX_NAME_LENGTH];
    char product_description[MAX_DESCRIPTION_LENGTH];
    const char *description;

    snprintf(product_name, MAX_NAME_LENGTH, "GEOMS-TE-LIDAR-H2O-%03d", version);
    snprintf(product_description, MAX_DESCRIPTION_LENGTH, "GEOMS template for LIDAR water vapor v%03d", version);
    product_definition = harp_ingestion_register_product(module, product_name, product_description, read_dimensions);

    dimension_type[0] = harp_dimension_time;
    dimension_type[1] = harp_dimension_vertical;

    register_common_variables(product_definition, 1);

    /* H2O_volume_mixing_ratio */
    description = "backscatter H2O volume mixing ratio";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "H2O_volume_mixing_ratio",
                                                                     harp_type_double, 2, dimension_type, NULL,
                                                                     description, "ppmv", NULL, read_h2o_vmr_bs);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/H2O.MIXING.RATIO.VOLUME_BACKSCATTER", NULL);

    /* H2O_number_density_uncertainty */
    description = "combined uncertainty of the absorption differential O3 number density";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition,
                                                                     "H2O_volume_mixing_ratio_uncertainty",
                                                                     harp_type_double, 2, dimension_type, NULL,
                                                                     description, "ppmv", NULL,
                                                                     read_h2o_vmr_bs_uncertainty);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/H2O.MIXING.RATIO.VOLUME_BACKSCATTER_UNCERTAINTY.COMBINED.STANDARD", NULL);

    /* relative_humidity */
    description = "derived relative humidity";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "relative_humidity",
                                                                     harp_type_double, 2, dimension_type, NULL,
                                                                     description, "%", NULL, read_relative_humidity);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/HUMIDITY.RELATIVE_DERIVED", NULL);

    /* relative_humidity_uncertainty */
    description = "combined uncertainty of the derived relative humidity";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition,
                                                                     "relative_humidity_uncertainty", harp_type_double,
                                                                     2, dimension_type, NULL, description, "%", NULL,
                                                                     read_relative_humidity_uncertainty);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/HUMIDITY.RELATIVE_DERIVED_UNCERTAINTY.COMBINED.STANDARD", NULL);

    return 0;
}

int harp_ingestion_module_geoms_lidar_init(void)
{
    harp_ingestion_module *module;

    module = harp_ingestion_register_module("GEOMS-TE-LIDAR-O3", "GEOMS", "GEOMS", "LIDAR_O3",
                                                 "GEOMS template for LIDAR ozone", ingestion_init, ingestion_done);

    init_o3_product_definition(module, 3);
    init_o3_product_definition(module, 4);
    init_o3_product_definition(module, 5);

    module = harp_ingestion_register_module("GEOMS-TE-LIDAR-TEMPERATURE", "GEOMS", "GEOMS", "LIDAR_TEMPERATURE",
                                                 "GEOMS template for LIDAR temperature", ingestion_init,
                                                 ingestion_done);

    init_temperature_product_definition(module, 3);
    init_temperature_product_definition(module, 4);
    init_temperature_product_definition(module, 5);

    module = harp_ingestion_register_module("GEOMS-TE-LIDAR-H2O", "GEOMS", "GEOMS", "LIDAR_H2O",
                                                 "GEOMS template for LIDAR water vapor (Raman)", ingestion_init,
                                                 ingestion_done);

    init_h2o_product_definition(module, 4);
    init_h2o_product_definition(module, 5);

    return 0;
}
