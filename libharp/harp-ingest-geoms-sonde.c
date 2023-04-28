/*
 * Copyright (C) 2015-2023 S[&]T, The Netherlands.
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
    int convert_to_profile;
    int has_scalar_latlon;
    int has_o3;
    int has_wind_speed;
    int has_wind_direction;
    int has_potential_temperature;
    int has_h2o;
} ingest_info;

static int read_dimensions(void *user_data, long dimension[HARP_NUM_DIM_TYPES])
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->convert_to_profile)
    {
        dimension[harp_dimension_time] = 1;
        dimension[harp_dimension_vertical] = info->num_time;
    }
    else
    {
        dimension[harp_dimension_time] = info->num_time;
    }

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

static int read_datetime_start(void *user_data, harp_array data)
{
    coda_cursor cursor;

    if (coda_cursor_set_product(&cursor, ((ingest_info *)user_data)->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, "DATETIME") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_first_array_element(&cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_double(&cursor, data.double_data) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    return 0;
}

static int read_datetime_stop(void *user_data, harp_array data)
{
    long num_elements;
    coda_cursor cursor;

    if (coda_cursor_set_product(&cursor, ((ingest_info *)user_data)->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, "DATETIME") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &num_elements) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_array_element_by_index(&cursor, num_elements - 1) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_double(&cursor, data.double_data) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    return 0;
}

static int read_datetime(void *user_data, harp_array data)
{
    coda_cursor cursor;

    if (coda_cursor_set_product(&cursor, ((ingest_info *)user_data)->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, "DATETIME") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_double_array(&cursor, data.double_data, coda_array_ordering_c) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    return 0;
}

static int read_latitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->has_scalar_latlon)
    {
        long i;

        if (read_variable_float(user_data, "LATITUDE", 1, data) != 0)
        {
            return -1;
        }
        for (i = 1; i < info->num_time; i++)
        {
            data.float_data[i] = data.float_data[0];
        }

        return 0;
    }

    return read_variable_float(user_data, "LATITUDE", info->num_time, data);
}

static int read_longitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->has_scalar_latlon)
    {
        long i;

        if (read_variable_float(user_data, "LONGITUDE", 1, data) != 0)
        {
            return -1;
        }
        for (i = 1; i < info->num_time; i++)
        {
            data.float_data[i] = data.float_data[0];
        }

        return 0;
    }

    return read_variable_float(user_data, "LONGITUDE", info->num_time, data);
}

static int read_pressure(void *user_data, harp_array data)
{
    return read_variable_float(user_data, "PRESSURE_INSITU", ((ingest_info *)user_data)->num_time, data);
}

static int read_pressure_uncertainty(void *user_data, harp_array data)
{
    return read_variable_float(user_data, "PRESSURE_INSITU_UNCERTAINTY_COMBINED_STANDARD",
                               ((ingest_info *)user_data)->num_time, data);
}

static int read_altitude_gph(void *user_data, harp_array data)
{
    return read_variable_float(user_data, "ALTITUDE_GPH", ((ingest_info *)user_data)->num_time, data);
}

static int read_altitude_gph_uncertainty(void *user_data, harp_array data)
{
    return read_variable_float(user_data, "ALTITUDE_GPH_UNCERTAINTY_COMBINED_STANDARD",
                               ((ingest_info *)user_data)->num_time, data);
}

static int read_temperature(void *user_data, harp_array data)
{
    return read_variable_float(user_data, "TEMPERATURE_INSITU", ((ingest_info *)user_data)->num_time, data);
}

static int read_temperature_uncertainty(void *user_data, harp_array data)
{
    return read_variable_float(user_data, "TEMPERATURE_INSITU_UNCERTAINTY_COMBINED_STANDARD",
                               ((ingest_info *)user_data)->num_time, data);
}

static int read_relative_humidity(void *user_data, harp_array data)
{
    return read_variable_float(user_data, "HUMIDITY_RELATIVE_INSITU", ((ingest_info *)user_data)->num_time, data);
}

static int read_relative_humidity_uncertainty(void *user_data, harp_array data)
{
    return read_variable_float(user_data, "HUMIDITY_RELATIVE_INSITU_UNCERTAINTY_COMBINED_STANDARD",
                               ((ingest_info *)user_data)->num_time, data);
}

static int read_O3_partial_pressure(void *user_data, harp_array data)
{
    return read_variable_float(user_data, "O3_PARTIAL_PRESSURE_INSITU", ((ingest_info *)user_data)->num_time, data);
}

static int read_O3_partial_pressure_uncertainty(void *user_data, harp_array data)
{
    return read_variable_float(user_data, "O3_PARTIAL_PRESSURE_INSITU_UNCERTAINTY_COMBINED_STANDARD",
                               ((ingest_info *)user_data)->num_time, data);
}

static int read_O3_volume_mixing_ratio(void *user_data, harp_array data)
{
    return read_variable_float(user_data, "O3_MIXING_RATIO_VOLUME_INSITU", ((ingest_info *)user_data)->num_time, data);
}

static int read_O3_number_density(void *user_data, harp_array data)
{
    return read_variable_float(user_data, "O3_NUMBER_DENSITY_INSITU", ((ingest_info *)user_data)->num_time, data);
}

static int read_wind_speed(void *user_data, harp_array data)
{
    return read_variable_float(user_data, "WIND_SPEED_INSITU", ((ingest_info *)user_data)->num_time, data);
}

static int read_wind_direction(void *user_data, harp_array data)
{
    return read_variable_float(user_data, "WIND_DIRECTION_INSITU", ((ingest_info *)user_data)->num_time, data);
}

static int read_potential_temperature(void *user_data, harp_array data)
{
    return read_variable_float(user_data, "POTENTIAL_TEMPERATURE_INSITU", ((ingest_info *)user_data)->num_time, data);
}

static int read_h2o_volume_mixing_ratio(void *user_data, harp_array data)
{
    return read_variable_float(user_data, "H2O_MIXING_RATIO_VOLUME_INSITU", ((ingest_info *)user_data)->num_time, data);
}

static int include_o3(void *user_data)
{
    return ((ingest_info *)user_data)->has_o3;
}

static int include_wind_speed(void *user_data)
{
    return ((ingest_info *)user_data)->has_wind_speed;
}

static int include_wind_direction(void *user_data)
{
    return ((ingest_info *)user_data)->has_wind_direction;
}

static int include_potential_temperature(void *user_data)
{
    return ((ingest_info *)user_data)->has_potential_temperature;
}

static int include_h2o(void *user_data)
{
    return ((ingest_info *)user_data)->has_h2o;
}

static void ingestion_done(void *user_data)
{
    free(user_data);
}

static int get_product_definition(const harp_ingestion_module *module, coda_product *product, int convert_to_profile,
                                  harp_product_definition **definition)
{
    coda_cursor cursor;
    char template_name[18];
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
    /* template should match the pattern "GEOMS-TE-SONDE-xxx" */
    if (length < 18)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, "invalid string length for DATA_TEMPLATE global attribute");
        return -1;
    }
    if (coda_cursor_read_string(&cursor, template_name, 19) != 0)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, NULL);
        return -1;
    }

    for (i = 0; i < module->num_product_definitions; i++)
    {
        /* match against product definition name: '<template_name>-<profile|points>' */
        if (strncmp(template_name, module->product_definition[i]->name, 18) == 0)
        {
            if (strcmp(&module->product_definition[i]->name[19], convert_to_profile ? "profile" : "points") == 0)
            {
                *definition = module->product_definition[i];
                return 0;
            }
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

static int get_optional_variable_availability(ingest_info *info)
{
    coda_cursor cursor;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, "/O3_PARTIAL_PRESSURE_INSITU") == 0)
    {
        info->has_o3 = 1;
    }
    if (coda_cursor_goto(&cursor, "/WIND_SPEED_INSITU") == 0)
    {
        info->has_wind_speed = 1;
    }
    if (coda_cursor_goto(&cursor, "/WIND_DIRECTION_INSITU") == 0)
    {
        info->has_wind_direction = 1;
    }
    if (coda_cursor_goto(&cursor, "/POTENTIAL_TEMPERATURE_INSITU") == 0)
    {
        info->has_potential_temperature = 1;
    }
    if (coda_cursor_goto(&cursor, "/H2O_MIXING_RATIO_VOLUME_INSITU") == 0)
    {
        info->has_h2o = 1;
    }

    return 0;
}

static int get_latlon_length(ingest_info *info)
{
    coda_cursor cursor;
    long num_elements;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, "/LATITUDE") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &num_elements) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    info->has_scalar_latlon = (num_elements == 1);

    return 0;
}

static int ingestion_init(const harp_ingestion_module *module, coda_product *product,
                          const harp_ingestion_options *options, harp_product_definition **definition, void **user_data)
{
    ingest_info *info;

    info = malloc(sizeof(ingest_info));
    if (info == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(ingest_info), __FILE__, __LINE__);
        return -1;
    }

    info->definition = NULL;
    info->product = product;
    info->convert_to_profile = 1;

    if (harp_ingestion_options_has_option(options, "profile"))
    {
        info->convert_to_profile = 0;
    }

    if (get_product_definition(module, info->product, info->convert_to_profile, definition) != 0)
    {
        ingestion_done(info);
        return -1;
    }
    info->definition = *definition;
    info->product_version = info->definition->name[16] - '0';
    info->has_scalar_latlon = 0;
    info->has_o3 = 0;
    info->has_wind_speed = 0;
    info->has_wind_direction = 0;
    info->has_potential_temperature = 0;
    info->has_h2o = 0;

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

    if (get_latlon_length(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }

    *user_data = info;
    return 0;
}

static int init_product_definition(harp_ingestion_module *module, int convert_to_profile, int version)
{
    harp_variable_definition *variable_definition;
    harp_product_definition *product_definition;
    harp_dimension_type dimension_type[1];
    char product_name[MAX_NAME_LENGTH];
    char product_description[MAX_DESCRIPTION_LENGTH];
    const char *description;

    snprintf(product_name, MAX_NAME_LENGTH, "GEOMS-TE-SONDE-%03d-%s", version,
             convert_to_profile ? "profile" : "points");
    snprintf(product_description, MAX_DESCRIPTION_LENGTH, "GEOMS template for Sonde v%03d (%s)", version,
             convert_to_profile ? "as single profile" : "as timeseries of points");
    product_definition = harp_ingestion_register_product(module, product_name, product_description, read_dimensions);
    harp_product_definition_add_mapping(product_definition, NULL,
                                        convert_to_profile ? "profile unset" : "profile=false");

    /* sensor_name */
    description = "name of the sensor";
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, "sensor_name", harp_type_string, 0, NULL, NULL, description, NULL, NULL, read_data_source);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/@DATA.SOURCE", NULL);

    /* location_name */
    description = "name of the site at which the sensor is located";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "location_name",
                                                                     harp_type_string, 0, NULL, NULL, description, NULL,
                                                                     NULL, read_data_location);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/@DATA.LOCATION", NULL);

    if (convert_to_profile)
    {
        dimension_type[0] = harp_dimension_time;

        /* datetime_start */
        description = "time of first measurement of the profile";
        variable_definition = harp_ingestion_register_variable_full_read
            (product_definition, "datetime_start", harp_type_double, 1, dimension_type, NULL, description,
             "days since 2000-01-01", NULL, read_datetime_start);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/DATETIME[0]", NULL);

        /* datetime_stop */
        description = "time of last measurement of the profile";
        variable_definition = harp_ingestion_register_variable_full_read
            (product_definition, "datetime_stop", harp_type_double, 1, dimension_type, NULL, description,
             "days since 2000-01-01", NULL, read_datetime_stop);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/DATETIME[N-1]", NULL);

        dimension_type[0] = harp_dimension_vertical;
    }
    else
    {
        dimension_type[0] = harp_dimension_time;

        /* datetime */
        description = "time of the measurement";
        variable_definition = harp_ingestion_register_variable_full_read
            (product_definition, "datetime", harp_type_double, 1, dimension_type, NULL, description,
             "days since 2000-01-01", NULL, read_datetime);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/DATETIME", NULL);
    }

    /* latitude */
    description = "latitude";
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, "latitude", harp_type_float, 1, dimension_type, NULL, description, "degree_north", NULL,
         read_latitude);
    description = "if the latitude is a scalar it is replicated for each profile point";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/LATITUDE", description);

    /* longitude */
    description = "longitude";
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, "longitude", harp_type_float, 1, dimension_type, NULL, description, "degree_east", NULL,
         read_longitude);
    description = "if the longitude is a scalar it is replicated for each profile point";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/LONGITUDE", description);

    /* pressure */
    description = "pressure measurement from PTU sonde";
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, "pressure", harp_type_float, 1, dimension_type, NULL, description, "hPa", NULL,
         read_pressure);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/PRESSURE_INSITU", NULL);

    /* pressure_uncertainty */
    description = "1 sigma uncertainty estimate of the pressure measurement";
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, "pressure_uncertainty", harp_type_float, 1, dimension_type, NULL, description, "hPa", NULL,
         read_pressure_uncertainty);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/PRESSURE_INSITU_UNCERTAINTY.COMBINED.STANDARD", NULL);

    /* geopotential_height */
    description = "calculated sonde GPH";
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, "geopotential_height", harp_type_float, 1, dimension_type, NULL, description, "m", NULL,
         read_altitude_gph);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ALTITUDE.GPH", NULL);

    /* geopotential_height_uncertainty */
    description = "1 sigma uncertainty estimate of the altitude measurement";
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, "geopotential_height_uncertainty", harp_type_float, 1, dimension_type, NULL, description,
         "m", NULL, read_altitude_gph_uncertainty);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ALTITUDE.GPH_UNCERTAINTY.COMBINED.STANDARD",
                                         NULL);

    /* temperature */
    description = "temperature measurement from PTU sonde";
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, "temperature", harp_type_float, 1, dimension_type, NULL, description, "K", NULL,
         read_temperature);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/TEMPERATURE_INSITU", NULL);

    /* temperature_uncertainty */
    description = "1 sigma uncertainty estimate of the temperature measurement";
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, "temperature_uncertainty", harp_type_float, 1, dimension_type, NULL, description, "K",
         NULL, read_temperature_uncertainty);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/TEMPERATURE_INSITU_UNCERTAINTY.COMBINED.STANDARD", NULL);

    /* relative_humidity */
    description = "relative humidity from PTU sonde";
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, "relative_humidity", harp_type_float, 1, dimension_type, NULL, description, "%", NULL,
         read_relative_humidity);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/HUMIDITY.RELATIVE_INSITU", NULL);

    /* relative_humidity_uncertainty */
    description = "1 sigma uncertainty estimate of the relative humidity";
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, "relative_humidity_uncertainty", harp_type_float, 1, dimension_type, NULL, description,
         "%", NULL, read_relative_humidity_uncertainty);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/HUMIDITY.RELATIVE_INSITU_UNCERTAINTY.COMBINED.STANDARD", NULL);

    /* O3_partial_pressure */
    description = "in situ partial pressure";
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, "O3_partial_pressure", harp_type_float, 1, dimension_type, NULL, description, "mPa",
         include_o3, read_O3_partial_pressure);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/O3.PARTIAL.PRESSURE_INSITU", NULL);

    /* O3_partial_pressure_uncertainty */
    description = "1 sigma uncertainty estimate of the partial pressure";
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, "O3_partial_pressure_uncertainty", harp_type_float, 1, dimension_type, NULL, description,
         "mPa", include_o3, read_O3_partial_pressure_uncertainty);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/O3.PARTIAL.PRESSURE_INSITU_UNCERTAINTY.COMBINED.STANDARD", NULL);

    /* O3_volume_mixing_ratio */
    description = "calculated in situ ozone volumetric mixing ratio from ozone sonde";
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, "O3_volume_mixing_ratio", harp_type_float, 1, dimension_type, NULL, description, "ppmv",
         include_o3, read_O3_volume_mixing_ratio);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/O3.MIXING.RATIO.VOLUME_INSITU", NULL);

    /* O3_number_density */
    description = "calculated in situ ozone number density from ozone sonde";
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, "O3_number_density", harp_type_float, 1, dimension_type, NULL, description, "molec/m3",
         include_o3, read_O3_number_density);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/O3.NUMBER.DENSITY_INSITU", NULL);

    /* wind_speed */
    description = "wind speed from instrument package";
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, "wind_speed", harp_type_float, 1, dimension_type, NULL, description, "m/s",
         include_wind_speed, read_wind_speed);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/WIND.SPEED_INSITU", NULL);

    /* wind_direction */
    description = "wind direction from instrument package";
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, "wind_direction", harp_type_float, 1, dimension_type, NULL, description, "degree",
         include_wind_direction, read_wind_direction);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/WIND.DIRECTION_INSITU", NULL);

    /* potential_temperature */
    description = "calculated in situ potential temperature from sonde";
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, "potential_temperature", harp_type_float, 1, dimension_type, NULL, description, "K",
         include_potential_temperature, read_potential_temperature);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/POTENTIAL.TEMPERATURE_INSITU", NULL);

    /* h2o_volume_mixing_ratio */
    description = "calculated in situ water vapor volumetric mixing ratio from sonde";
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, "h2o_volume_mixing_ratio", harp_type_float, 1, dimension_type, NULL, description, "ppmv",
         include_h2o, read_h2o_volume_mixing_ratio);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/H2O.MIXING.RATIO.VOLUME_INSITU", NULL);

    return 0;
}

int harp_ingestion_module_geoms_sonde_init(void)
{
    const char *profile_options[] = { "false" };
    harp_ingestion_module *module;

    module = harp_ingestion_register_module("GEOMS-TE-SONDE", "GEOMS", "GEOMS", "SONDE",
                                            "GEOMS template for Sondes", ingestion_init, ingestion_done);

    harp_ingestion_register_option(module, "profile", "whether to ingest the sonde profile as a vertical profile "
                                   "(default) or as a timeseries of points (profile=false)", 1, profile_options);

    init_product_definition(module, 0, 2);
    init_product_definition(module, 1, 2);

    return 0;
}
