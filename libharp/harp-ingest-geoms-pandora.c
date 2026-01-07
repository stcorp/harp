/*
 * Copyright (C) 2015-2026 S[&]T, The Netherlands.
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

typedef enum gas_enum
{
    gas_NO2,
    gas_O3,
    gas_SO2,
    gas_H2CO,
    num_gas
} gas_type;

static const char *geoms_gas_name[num_gas] = {
    "NO2",
    "O3",
    "SO2",
    "H2CO",
};

static const char *harp_gas_name[num_gas] = {
    "NO2",
    "O3",
    "SO2",
    "HCHO",
};

typedef struct ingest_info_struct
{
    const harp_product_definition *definition;
    int product_version;
    coda_product *product;
    gas_type gas;
    const char *template;
    long num_time;
} ingest_info;

static int read_dimensions(void *user_data, long dimension[HARP_NUM_DIM_TYPES])
{
    ingest_info *info = (ingest_info *)user_data;

    dimension[harp_dimension_time] = info->num_time;

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

static int read_variable_int32(void *user_data, const char *path, long num_elements, harp_array data)
{
    coda_cursor cursor;
    long actual_num_elements;

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
    if (coda_cursor_read_int32_array(&cursor, data.int32_data, coda_array_ordering_c) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
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

static int read_datetime_start(void *user_data, harp_array data)
{
    return read_variable_double(user_data, "DATETIME_START", ((ingest_info *)user_data)->num_time, data);
}

static int read_datetime_stop(void *user_data, harp_array data)
{
    return read_variable_double(user_data, "DATETIME_STOP", ((ingest_info *)user_data)->num_time, data);
}

static int read_integration_time(void *user_data, harp_array data)
{
    return read_variable_double(user_data, "INTEGRATION_TIME", ((ingest_info *)user_data)->num_time, data);
}

static int read_instrument_latitude(void *user_data, harp_array data)
{
    return read_variable_double(user_data, "LATITUDE_INSTRUMENT", 1, data);
}

static int read_instrument_longitude(void *user_data, harp_array data)
{
    return read_variable_double(user_data, "LONGITUDE_INSTRUMENT", 1, data);
}

static int read_instrument_altitude(void *user_data, harp_array data)
{
    return read_variable_double(user_data, "ALTITUDE_INSTRUMENT", 1, data);
}

static int read_solar_azimuth_angle(void *user_data, harp_array data)
{
    return read_variable_double(user_data, "ANGLE_SOLAR_AZIMUTH", ((ingest_info *)user_data)->num_time, data);
}

static int read_solar_zenith_angle(void *user_data, harp_array data)
{
    return read_variable_double(user_data, "ANGLE_SOLAR_ZENITH_ASTRONOMICAL", ((ingest_info *)user_data)->num_time,
                                data);
}

static int read_column_solar(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char path[MAX_PATH_LENGTH];

    snprintf(path, MAX_PATH_LENGTH, "/%s_COLUMN_ABSORPTION_SOLAR", geoms_gas_name[info->gas]);
    return read_variable_double(user_data, path, info->num_time, data);
}

static int read_column_solar_uncertainty_combined(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char path[MAX_PATH_LENGTH];

    snprintf(path, MAX_PATH_LENGTH, "/%s_COLUMN_ABSORPTION_SOLAR_UNCERTAINTY_COMBINED_STANDARD",
             geoms_gas_name[info->gas]);
    return read_variable_double(user_data, path, info->num_time, data);
}

static int read_column_solar_uncertainty_random(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char path[MAX_PATH_LENGTH];

    snprintf(path, MAX_PATH_LENGTH, "/%s_COLUMN_ABSORPTION_SOLAR_UNCERTAINTY_RANDOM_STANDARD",
             geoms_gas_name[info->gas]);
    return read_variable_double(user_data, path, info->num_time, data);
}

static int read_column_solar_uncertainty_systematic(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char path[MAX_PATH_LENGTH];

    snprintf(path, MAX_PATH_LENGTH, "/%s_COLUMN_ABSORPTION_SOLAR_UNCERTAINTY_SYSTEMATIC_STANDARD",
             geoms_gas_name[info->gas]);
    return read_variable_double(user_data, path, info->num_time, data);
}

static int read_column_solar_amf(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char path[MAX_PATH_LENGTH];

    snprintf(path, MAX_PATH_LENGTH, "/%s_COLUMN_ABSORPTION_SOLAR_AMF", geoms_gas_name[info->gas]);
    return read_variable_double(user_data, path, info->num_time, data);
}

static int read_column_solar_flag(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char path[MAX_PATH_LENGTH];

    snprintf(path, MAX_PATH_LENGTH, "/%s_COLUMN_ABSORPTION_SOLAR_FLAG", geoms_gas_name[info->gas]);
    return read_variable_int32(user_data, path, info->num_time, data);
}

static int read_effective_temperature(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char path[MAX_PATH_LENGTH];

    snprintf(path, MAX_PATH_LENGTH, "/TEMPERATURE_EFFECTIVE_%s", geoms_gas_name[info->gas]);
    return read_variable_double(user_data, path, info->num_time, data);
}

static int read_effective_temperature_uncertainty_combined(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char path[MAX_PATH_LENGTH];

    snprintf(path, MAX_PATH_LENGTH, "/TEMPERATURE_EFFECTIVE_%s_UNCERTAINTY_COMBINED_STANDARD",
             geoms_gas_name[info->gas]);
    return read_variable_double(user_data, path, info->num_time, data);
}

static int read_effective_temperature_uncertainty_random(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char path[MAX_PATH_LENGTH];

    snprintf(path, MAX_PATH_LENGTH, "/TEMPERATURE_EFFECTIVE_%s_UNCERTAINTY_RANDOM_STANDARD", geoms_gas_name[info->gas]);
    return read_variable_double(user_data, path, info->num_time, data);
}

static int read_effective_temperature_uncertainty_systematic(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char path[MAX_PATH_LENGTH];

    snprintf(path, MAX_PATH_LENGTH, "/TEMPERATURE_EFFECTIVE_%s_UNCERTAINTY_SYSTEMATIC_STANDARD",
             geoms_gas_name[info->gas]);
    return read_variable_double(user_data, path, info->num_time, data);
}

static void ingestion_done(void *user_data)
{
    free(user_data);
}

static gas_type get_gas_from_string(const char *str)
{
    int i;

    for (i = 0; i < num_gas; i++)
    {
        if (strcmp(str, geoms_gas_name[i]) == 0)
        {
            return i;
        }
    }

    assert(0);
    exit(1);
}

static int get_product_definition(const harp_ingestion_module *module, coda_product *product,
                                  harp_product_definition **definition)
{
    coda_cursor cursor;
    char template_name[36];
    char data_source[30];
    char *gas;
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
    /* template should match the pattern "GEOMS-TE-PANDORA-DIRECTSUN-GAS-xxx" */
    if (coda_cursor_read_string(&cursor, template_name, 36) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    length = strlen(template_name);
    if (length != 34)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, "invalid string length for DATA_TEMPLATE global attribute");
        return -1;
    }
    if (strncmp(template_name, "GEOMS-TE-PANDORA-DIRECTSUN-GAS-", 31) != 0)
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
    /* data source should match the pattern "UVVIS_DOAS.[DIRECTSUN|OFFAXIS|ZENITH].<SPECIES>_xxxx" */
    if (strncmp(data_source, "UVVIS.DOAS.DIRECTSUN.", 21) != 0)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, "DATA_SOURCE global attribute has an invalid value");
        return -1;
    }
    i = 21;

    /* truncate data_source at first '_' occurrence */
    gas = &data_source[21];
    while (data_source[i] != '\0')
    {
        if (data_source[i] == '_')
        {
            data_source[i] = '\0';
        }
        else
        {
            i++;
        }
    }

    for (i = 0; i < module->num_product_definitions; i++)
    {
        /* match against product definition name: '<template_name>-<gas>' */
        if (strncmp(template_name, module->product_definition[i]->name, length) == 0 &&
            strcmp(gas, &module->product_definition[i]->name[length + 1]) == 0)
        {
            *definition = module->product_definition[i];
            return 0;
        }
    }
    harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, "GEOMS template '%s' for gas '%s' not supported", template_name,
                   gas);

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
    info->gas = get_gas_from_string(&info->definition->name[35]);

    if (get_dimensions(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }

    *user_data = info;
    return 0;
}

static int init_product_definition(harp_ingestion_module *module, gas_type gas, int version)
{
    harp_variable_definition *variable_definition;
    harp_product_definition *product_definition;
    harp_dimension_type dimension_type[4];
    char product_name[MAX_NAME_LENGTH];
    char product_description[MAX_DESCRIPTION_LENGTH];
    char gas_var_name[MAX_NAME_LENGTH];
    char gas_mapping_path[MAX_PATH_LENGTH];
    char gas_description[MAX_DESCRIPTION_LENGTH];
    const char *description;
    const char *gas_unit;

    if (version < 3)
    {
        gas_unit = "DU";
    }
    else
    {
        gas_unit = "mol/m2";
    }

    snprintf(product_name, MAX_NAME_LENGTH, "GEOMS-TE-PANDORA-DIRECTSUN-GAS-%03d-%s", version, geoms_gas_name[gas]);
    snprintf(product_description, MAX_NAME_LENGTH,
             "GEOMS template for Pandora direct-sun measurements v%03d - %s", version, geoms_gas_name[gas]);
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

    /* datetime_duration */
    description = "duration of the measurement";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "datetime_duration",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, "s", NULL, read_integration_time);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/INTEGRATION.TIME", NULL);

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
    description = "altitude of the sensor relative to the location site";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "sensor_altitude",
                                                                     harp_type_double, 0, NULL, NULL, description, "m",
                                                                     NULL, read_instrument_altitude);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ALTITUDE.INSTRUMENT", NULL);

    /* solar_zenith_angle */
    description = "solar astronomical zenith angle";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "solar_zenith_angle",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, "degree", NULL,
                                                                     read_solar_zenith_angle);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ANGLE.SOLAR_ZENITH.ASTRONOMICAL", NULL);

    /* solar_azimuth_angle */
    description = "solar azimuth angle";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "solar_azimuth_angle",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, "degree", NULL,
                                                                     read_solar_azimuth_angle);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ANGLE.SOLAR_AZIMUTH", NULL);

    /* <gas>_column_number_density */
    snprintf(gas_var_name, MAX_NAME_LENGTH, "%s_column_number_density", harp_gas_name[gas]);
    snprintf(gas_description, MAX_DESCRIPTION_LENGTH, "%s column number density", harp_gas_name[gas]);
    snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.COLUMN.ABSORPTION.SOLAR", geoms_gas_name[gas]);
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, gas_var_name, harp_type_double, 1, dimension_type, NULL, gas_description, gas_unit, NULL,
         read_column_solar);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, gas_mapping_path, NULL);

    /* <gas>_column_number_density_uncertainty */
    snprintf(gas_var_name, MAX_NAME_LENGTH, "%s_column_number_density_uncertainty", harp_gas_name[gas]);
    if (version < 3)
    {
        snprintf(gas_description, MAX_DESCRIPTION_LENGTH, "random uncertainty of the %s column number density",
                 harp_gas_name[gas]);
        snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.COLUMN.ABSORPTION.SOLAR_UNCERTAINTY.RANDOM.STANDARD",
                 geoms_gas_name[gas]);
        variable_definition = harp_ingestion_register_variable_full_read
            (product_definition, gas_var_name, harp_type_double, 1, dimension_type, NULL, gas_description, gas_unit,
             NULL, read_column_solar_uncertainty_random);
    }
    else
    {
        snprintf(gas_description, MAX_DESCRIPTION_LENGTH, "total uncertainty of the %s column number density",
                 harp_gas_name[gas]);
        snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.COLUMN.ABSORPTION.SOLAR_UNCERTAINTY.COMBINED.STANDARD",
                 geoms_gas_name[gas]);
        variable_definition = harp_ingestion_register_variable_full_read
            (product_definition, gas_var_name, harp_type_double, 1, dimension_type, NULL, gas_description, gas_unit,
             NULL, read_column_solar_uncertainty_combined);
    }
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, gas_mapping_path, NULL);

    if (version >= 3)
    {
        /* <gas>_column_number_density_uncertainty_random */
        snprintf(gas_var_name, MAX_NAME_LENGTH, "%s_column_number_density_uncertainty_random", harp_gas_name[gas]);
        snprintf(gas_description, MAX_DESCRIPTION_LENGTH, "random uncertainty of the %s column number density",
                 harp_gas_name[gas]);
        snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.COLUMN.ABSORPTION.SOLAR_UNCERTAINTY.RANDOM.STANDARD",
                 geoms_gas_name[gas]);
        variable_definition = harp_ingestion_register_variable_full_read
            (product_definition, gas_var_name, harp_type_double, 1, dimension_type, NULL, gas_description, gas_unit,
             NULL, read_column_solar_uncertainty_random);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, gas_mapping_path, NULL);

        /* <gas>_column_number_density_uncertainty_systematic */
        snprintf(gas_var_name, MAX_NAME_LENGTH, "%s_column_number_density_uncertainty_systematic", harp_gas_name[gas]);
        snprintf(gas_description, MAX_DESCRIPTION_LENGTH, "systematic uncertainty of the %s column number density",
                 harp_gas_name[gas]);
        snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.COLUMN.ABSORPTION.SOLAR_UNCERTAINTY.SYSTEMATIC.STANDARD",
                 geoms_gas_name[gas]);
        variable_definition = harp_ingestion_register_variable_full_read
            (product_definition, gas_var_name, harp_type_double, 1, dimension_type, NULL, gas_description, gas_unit,
             NULL, read_column_solar_uncertainty_systematic);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, gas_mapping_path, NULL);
    }

    /* <gas>_column_number_density_amf */
    snprintf(gas_var_name, MAX_NAME_LENGTH, "%s_column_number_density_amf", harp_gas_name[gas]);
    snprintf(gas_description, MAX_DESCRIPTION_LENGTH, "air mass factor of %s column number density",
             harp_gas_name[gas]);
    snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.COLUMN.ABSORPTION.SOLAR_AMF", geoms_gas_name[gas]);
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, gas_var_name, harp_type_double, 1, dimension_type, NULL, gas_description, "1", NULL,
         read_column_solar_amf);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, gas_mapping_path, NULL);

    /* <gas>_column_number_density_validity */
    snprintf(gas_var_name, MAX_NAME_LENGTH, "%s_column_number_density_validity", harp_gas_name[gas]);
    snprintf(gas_description, MAX_DESCRIPTION_LENGTH, "quality flag of %s column number density", harp_gas_name[gas]);
    snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.COLUMN.ABSORPTION.SOLAR_FLAG", geoms_gas_name[gas]);
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, gas_var_name, harp_type_int32, 1, dimension_type, NULL, gas_description, NULL, NULL,
         read_column_solar_flag);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, gas_mapping_path, NULL);

    if (version >= 3)
    {
        /* <gas>_effective_temperature */
        snprintf(gas_var_name, MAX_NAME_LENGTH, "%s_effective_temperature", harp_gas_name[gas]);
        snprintf(gas_description, MAX_DESCRIPTION_LENGTH, "%s effective temperature", harp_gas_name[gas]);
        snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/TEMPERATURE.EFFECTIVE.%s", geoms_gas_name[gas]);
        variable_definition = harp_ingestion_register_variable_full_read
            (product_definition, gas_var_name, harp_type_double, 1, dimension_type, NULL, gas_description, "K", NULL,
             read_effective_temperature);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, gas_mapping_path, NULL);

        /* <gas>_effective_temperature_uncertainty */
        snprintf(gas_var_name, MAX_NAME_LENGTH, "%s_effective_temperature_uncertainty", harp_gas_name[gas]);
        snprintf(gas_description, MAX_DESCRIPTION_LENGTH, "total uncertainty of the %s effective temperature",
                 harp_gas_name[gas]);
        snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/TEMPERATURE.EFFECTIVE.%s_UNCERTAINTY.COMBINED.STANDARD",
                 geoms_gas_name[gas]);
        variable_definition = harp_ingestion_register_variable_full_read
            (product_definition, gas_var_name, harp_type_double, 1, dimension_type, NULL, gas_description, "K", NULL,
             read_effective_temperature_uncertainty_combined);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, gas_mapping_path, NULL);

        /* <gas>_effective_temperature_uncertainty_random */
        snprintf(gas_var_name, MAX_NAME_LENGTH, "%s_effective_temperature_uncertainty_random", harp_gas_name[gas]);
        snprintf(gas_description, MAX_DESCRIPTION_LENGTH, "random uncertainty of the %s effective temperature",
                 harp_gas_name[gas]);
        snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/TEMPERATURE.EFFECTIVE.%s_UNCERTAINTY.RANDOM.STANDARD",
                 geoms_gas_name[gas]);
        variable_definition = harp_ingestion_register_variable_full_read
            (product_definition, gas_var_name, harp_type_double, 1, dimension_type, NULL, gas_description, "K", NULL,
             read_effective_temperature_uncertainty_random);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, gas_mapping_path, NULL);

        /* <gas>_effective_temperature_uncertainty_systematic */
        snprintf(gas_var_name, MAX_NAME_LENGTH, "%s_effective_temperature_uncertainty_systematic", harp_gas_name[gas]);
        snprintf(gas_description, MAX_DESCRIPTION_LENGTH, "systematic uncertainty of the %s effective temperature",
                 harp_gas_name[gas]);
        snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/TEMPERATURE.EFFECTIVE.%s_UNCERTAINTY.SYSTEMATIC.STANDARD",
                 geoms_gas_name[gas]);
        variable_definition = harp_ingestion_register_variable_full_read
            (product_definition, gas_var_name, harp_type_double, 1, dimension_type, NULL, gas_description, "K", NULL,
             read_effective_temperature_uncertainty_systematic);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, gas_mapping_path, NULL);
    }

    return 0;
}

int harp_ingestion_module_geoms_pandora_init(void)
{
    harp_ingestion_module *module;
    int i;

    module = harp_ingestion_register_module("GEOMS-TE-PANDORA-DIRECTSUN-GAS", "GEOMS", "GEOMS",
                                            "PANDORA_DIRECTSUN_GAS",
                                            "GEOMS template for Pandora UVVIS-DOAS direct sun measurements",
                                            ingestion_init, ingestion_done);

    for (i = 0; i < num_gas; i++)
    {
        if (i == gas_NO2 || i == gas_O3)
        {
            init_product_definition(module, i, 2);
        }
        init_product_definition(module, i, 3);
    }

    return 0;
}
