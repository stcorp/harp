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

#define MAX_UNIT_LENGTH 30

#define MAX_NAME_LENGTH 80
#define MAX_DESCRIPTION_LENGTH 100
#define MAX_PATH_LENGTH 100
#define MAX_MAPPING_LENGTH 100

typedef enum mwr_gas_enum
{
    mwr_ClO,
    mwr_CO,
    mwr_H2O,
    mwr_HCN,
    mwr_HNO3,
    mwr_N2O,
    mwr_O3,
    num_mwr_gas
} mwr_gas;

static const char *gas_name[num_mwr_gas] = {
    "ClO",
    "CO",
    "H2O",
    "HCN",
    "HNO3",
    "N2O",
    "O3",
};

typedef struct ingest_info_struct
{
    const harp_product_definition *definition;
    int product_version;
    coda_product *product;
    mwr_gas gas;
    const char *template;
    long num_time;
    long num_vertical;
    int has_h2o_column;
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

static int read_variable_double_replicated(void *user_data, const char *path, long num_time, long num_elements,
                                           harp_array data)
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
    if (actual_num_elements != num_elements && actual_num_elements != (num_elements / num_time))
    {
        harp_set_error(HARP_ERROR_INGESTION, "variable %s has %ld elements (expected %ld or %ld)", path,
                       actual_num_elements, (num_elements / num_time), num_elements);
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
        for (i = 0; i < actual_num_elements; i++)
        {
            if (data.double_data[i] == fill_value)
            {
                data.double_data[i] = harp_nan();
            }
        }
    }
    if (actual_num_elements < num_elements)
    {
        for (i = 1; i < num_time; i++)
        {
            memcpy(&data.double_data[i * actual_num_elements], data.double_data, actual_num_elements * sizeof(double));
        }
    }

    return 0;
}

static int read_variable_double_scaled(void *user_data, const char *path1, const char *path2, long num_elements,
                                       harp_array data)
{
    harp_array buffer;
    int i;

    if (read_variable_double(user_data, path1, num_elements, data) != 0)
    {
        return -1;
    }

    buffer.double_data = malloc(num_elements * sizeof(double));
    if (buffer.double_data == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_elements * sizeof(double), __FILE__, __LINE__);
        return -1;
    }

    if (read_variable_double(user_data, path2, num_elements, buffer) != 0)
    {
        free(buffer.double_data);
        return -1;
    }
    for (i = 0; i < num_elements; i++)
    {
        data.double_data[i] *= buffer.double_data[i] * 0.01;    /* scale factor is a percentage */
    }
    free(buffer.double_data);

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

static int read_datetime(void *user_data, harp_array data)
{
    return read_variable_double(user_data, "DATETIME", ((ingest_info *)user_data)->num_time, data);
}

static int read_viewing_azimuth_angle(void *user_data, harp_array data)
{
    return read_variable_double_replicated(user_data, "ANGLE_VIEW_AZIMUTH", ((ingest_info *)user_data)->num_time,
                                           ((ingest_info *)user_data)->num_time, data);
}

static int read_viewing_zenith_angle(void *user_data, harp_array data)
{
    return read_variable_double(user_data, "ANGLE_VIEW_ZENITH_MEAN", ((ingest_info *)user_data)->num_time, data);
}

static int read_solar_zenith_angle(void *user_data, harp_array data)
{
    return read_variable_double(user_data, "ANGLE_SOLAR_ZENITH_MEAN", ((ingest_info *)user_data)->num_time, data);
}

static int read_datetime_start(void *user_data, harp_array data)
{
    return read_variable_double(user_data, "DATETIME_START", ((ingest_info *)user_data)->num_time, data);
}

static int read_datetime_stop(void *user_data, harp_array data)
{
    return read_variable_double(user_data, "DATETIME_STOP", ((ingest_info *)user_data)->num_time, data);
}

static int read_altitude(void *user_data, harp_array data)
{
    return read_variable_double(user_data, "ALTITUDE", ((ingest_info *)user_data)->num_vertical, data);
}

static int read_pressure_ind(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_double_replicated(user_data, "PRESSURE_INDEPENDENT", info->num_time,
                                           info->num_time * info->num_vertical, data);
}

static int read_temperature_ind(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_double_replicated(user_data, "TEMPERATURE_INDEPENDENT", info->num_time,
                                           info->num_time * info->num_vertical, data);
}

static int read_vmr(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char path[MAX_PATH_LENGTH];

    snprintf(path, MAX_PATH_LENGTH, "/%s_MIXING_RATIO%s_EMISSION", gas_name[info->gas],
             info->product_version == 1 ? "" : "_VOLUME");
    return read_variable_double(user_data, path, info->num_time * info->num_vertical, data);
}

static int read_vmr_relerr_random(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char vmr_path[MAX_PATH_LENGTH];
    char path[MAX_PATH_LENGTH];

    assert(info->product_version == 1);
    snprintf(vmr_path, MAX_PATH_LENGTH, "/%s_MIXING_RATIO_EMISSION", gas_name[info->gas]);
    snprintf(path, MAX_PATH_LENGTH, "/%s_MIXING_RATIO_EMISSION_UNCERTAINTY_RANDOM", gas_name[info->gas]);
    return read_variable_double_scaled(user_data, path, vmr_path, info->num_time * info->num_vertical, data);
}

static int read_vmr_relerr_systematic(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char vmr_path[MAX_PATH_LENGTH];
    char path[MAX_PATH_LENGTH];

    assert(info->product_version == 1);
    snprintf(vmr_path, MAX_PATH_LENGTH, "/%s_MIXING_RATIO_EMISSION", gas_name[info->gas]);
    snprintf(path, MAX_PATH_LENGTH, "/%s_MIXING_RATIO_EMISSION_UNCERTAINTY_SYSTEMATIC", gas_name[info->gas]);
    return read_variable_double_scaled(user_data, path, vmr_path, info->num_time * info->num_vertical, data);
}

static int read_vmr_uncertainty_random(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char path[MAX_PATH_LENGTH];

    assert(info->product_version >= 2);
    snprintf(path, MAX_PATH_LENGTH, "/%s_MIXING_RATIO_VOLUME_EMISSION_UNCERTAINTY_RANDOM_STANDARD",
             gas_name[info->gas]);
    return read_variable_double(user_data, path, info->num_time * info->num_vertical, data);
}

static int read_vmr_uncertainty_systematic(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char path[MAX_PATH_LENGTH];

    assert(info->product_version >= 2);
    snprintf(path, MAX_PATH_LENGTH, "/%s_MIXING_RATIO_VOLUME_EMISSION_UNCERTAINTY_SYSTEMATIC_STANDARD",
             gas_name[info->gas]);
    return read_variable_double(user_data, path, info->num_time * info->num_vertical, data);
}

static int read_vmr_apriori(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char path[MAX_PATH_LENGTH];

    snprintf(path, MAX_PATH_LENGTH, "/%s_MIXING_RATIO%s_EMISSION_APRIORI", gas_name[info->gas],
             info->product_version == 1 ? "" : "_VOLUME");
    return read_variable_double(user_data, path, info->num_time * info->num_vertical, data);
}

static int read_vmr_avk(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char path[MAX_PATH_LENGTH];

    snprintf(path, MAX_PATH_LENGTH, "/%s_MIXING_RATIO%s_EMISSION_AVK", gas_name[info->gas],
             info->product_version == 1 ? "" : "_VOLUME");
    return read_variable_double(user_data, path, info->num_time * info->num_vertical * info->num_vertical, data);
}

static int read_h2o_column(void *user_data, harp_array data)
{
    return read_variable_double(user_data, "/H2O_COLUMN_DERIVED", ((ingest_info *)user_data)->num_time, data);
}

static int include_h2o_column(void *user_data)
{
    return ((ingest_info *)user_data)->has_h2o_column;
}

static void ingestion_done(void *user_data)
{
    free(user_data);
}

static mwr_gas get_gas_from_string(const char *str)
{
    int i;

    for (i = 0; i < num_mwr_gas; i++)
    {
        if (strcmp(str, gas_name[i]) == 0)
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
    char template_name[17];
    char data_source[20];
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
    /* template should match the pattern "GEOMS-TE-MWR-xxx" */
    if (length != 16)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, "invalid string length for DATA_TEMPLATE global attribute");
        return -1;
    }
    if (coda_cursor_read_string(&cursor, template_name, 17) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    if (coda_cursor_goto(&cursor, "/@DATA_SOURCE") != 0)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, "could not find DATA_SOURCE global attribute");
        return -1;
    }
    if (coda_cursor_read_string(&cursor, data_source, 20) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    /* data source should match the pattern "MWR.<SPECIES>_xxxx" */
    if (strncmp(data_source, "MWR.", 4) != 0)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, "DATA_SOURCE global attribute has an invalid value");
        return -1;
    }
    /* truncate data_source at first '_' occurrence */
    i = 4;
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
    gas = &data_source[4];

    for (i = 0; i < module->num_product_definitions; i++)
    {
        /* match against product definition name: '<template_name>-<gas>' */
        if (strncmp(template_name, module->product_definition[i]->name, 16) == 0 &&
            strcmp(gas, &module->product_definition[i]->name[17]) == 0)
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

    if (coda_cursor_goto(&cursor, "/ALTITUDE") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &info->num_vertical) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
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

static int get_optional_variable_availability(ingest_info *info)
{
    coda_cursor cursor;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    info->has_h2o_column = (coda_cursor_goto(&cursor, "/H2O_COLUMN_DERIVED") == 0);

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
    info->product_version = info->definition->name[15] - '0';
    info->gas = get_gas_from_string(&info->definition->name[17]);
    info->has_h2o_column = 0;

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

static int init_product_definition(harp_ingestion_module *module, mwr_gas gas, int version)
{
    harp_variable_definition *variable_definition;
    harp_product_definition *product_definition;
    harp_dimension_type dimension_type[3];
    char gas_product_name[MAX_NAME_LENGTH];
    char gas_product_description[MAX_DESCRIPTION_LENGTH];
    char gas_var_name[MAX_NAME_LENGTH];
    char gas_mapping_path[MAX_PATH_LENGTH];
    char gas_description[MAX_DESCRIPTION_LENGTH];
    const char *description;

    snprintf(gas_product_name, MAX_NAME_LENGTH, "GEOMS-TE-MWR-%03d-%s", version, gas_name[gas]);
    snprintf(gas_product_description, MAX_NAME_LENGTH, "GEOMS template for MWR v%03d - %s", version, gas_name[gas]);
    product_definition = harp_ingestion_register_product(module, gas_product_name, gas_product_description,
                                                         read_dimensions);

    dimension_type[0] = harp_dimension_time;
    dimension_type[1] = harp_dimension_vertical;
    dimension_type[2] = harp_dimension_vertical;

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

    /* sensor_longitude */
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

    /* viewing_azimuth_angle */
    description = "viewing azimuth angle";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "viewing_azimuth_angle",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, "degree", NULL,
                                                                     read_viewing_azimuth_angle);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ANGLE.VIEW_AZIMUTH", NULL);

    /* viewing_zenith_angle */
    description = "mean viewing zenith angle";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "viewing_zenith_angle",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, "degree", NULL,
                                                                     read_viewing_zenith_angle);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ANGLE.VIEW_ZENITH_MEAN", NULL);

    /* solar_zenith_angle */
    description = "mean solar zenith angle";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "solar_zenith_angle",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, "degree", NULL,
                                                                     read_solar_zenith_angle);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ANGLE.SOLAR_ZENITH_MEAN", NULL);

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

    /* altitude */
    description = "altitude";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "altitude", harp_type_double,
                                                                     1, &dimension_type[1], NULL, description, "m",
                                                                     NULL, read_altitude);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ALTITUDE", NULL);

    /* pressure */
    description = "independent pressure profile";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "pressure", harp_type_double,
                                                                     2, dimension_type, NULL, description, "hPa", NULL,
                                                                     read_pressure_ind);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/PRESSURE_INDEPENDENT", NULL);

    /* temperature */
    description = "independent temperature profile";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "temperature",
                                                                     harp_type_double, 2, dimension_type, NULL,
                                                                     description, "K", NULL, read_temperature_ind);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/TEMPERATURE_INDEPENDENT", NULL);

    /* <gas>_volume_mixing_ratio */
    snprintf(gas_var_name, MAX_NAME_LENGTH, "%s_volume_mixing_ratio", gas_name[gas]);
    snprintf(gas_description, MAX_DESCRIPTION_LENGTH, "%s volume mixing ratio", gas_name[gas]);
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, gas_var_name, harp_type_double, 2, dimension_type, NULL, gas_description,
         version == 1 ? "ppv" : "ppmv", NULL, read_vmr);
    snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.MIXING.RATIO%s_EMISSION", gas_name[gas],
             version == 1 ? "" : ".VOLUME");
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, gas_mapping_path, NULL);

    /* <gas>_volume_mixing_ratio_uncertainty_random */
    snprintf(gas_var_name, MAX_NAME_LENGTH, "%s_volume_mixing_ratio_uncertainty_random", gas_name[gas]);
    snprintf(gas_description, MAX_DESCRIPTION_LENGTH, "random standard deviation of the %s volume mixing ratio",
             gas_name[gas]);
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, gas_var_name, harp_type_double, 2, dimension_type, NULL, gas_description,
         version == 1 ? "ppv" : "ppmv", NULL, version == 1 ? read_vmr_relerr_random : read_vmr_uncertainty_random);
    snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.MIXING.RATIO%s_EMISSION_UNCERTAINTY.RANDOM%s", gas_name[gas],
             version == 1 ? "" : ".VOLUME", version == 1 ? "" : ".STANDARD");
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, gas_mapping_path, NULL);

    /* <gas>_volume_mixing_ratio_uncertainty_systematic */
    snprintf(gas_var_name, MAX_NAME_LENGTH, "%s_volume_mixing_ratio_uncertainty_systematic", gas_name[gas]);
    snprintf(gas_description, MAX_DESCRIPTION_LENGTH, "systematic standard deviation of the %s volume mixing ratio",
             gas_name[gas]);
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, gas_var_name, harp_type_double, 2, dimension_type, NULL, gas_description,
         version == 1 ? "ppv" : "ppmv", NULL,
         version == 1 ? read_vmr_relerr_systematic : read_vmr_uncertainty_systematic);
    snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.MIXING.RATIO%s_EMISSION_UNCERTAINTY.SYSTEMATIC%s", gas_name[gas],
             version == 1 ? "" : ".VOLUME", version == 1 ? "" : ".STANDARD");
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, gas_mapping_path, NULL);

    /* <gas>_volume_mixing_ratio_apriori */
    snprintf(gas_var_name, MAX_NAME_LENGTH, "%s_volume_mixing_ratio_apriori", gas_name[gas]);
    snprintf(gas_description, MAX_DESCRIPTION_LENGTH, "a priori %s volume mixing ratio", gas_name[gas]);
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, gas_var_name, harp_type_double, 2, dimension_type, NULL, gas_description,
         version == 1 ? "ppv" : "ppmv", NULL, read_vmr_apriori);
    snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.MIXING.RATIO%s_EMISSION_APRIORI", gas_name[gas],
             version == 1 ? "" : ".VOLUME");
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, gas_mapping_path, NULL);

    /* <gas>_volume_mixing_ratio_avk */
    snprintf(gas_var_name, MAX_NAME_LENGTH, "%s_volume_mixing_ratio_avk", gas_name[gas]);
    snprintf(gas_description, MAX_DESCRIPTION_LENGTH, "averaging kernel for the %s volume mixing ratio", gas_name[gas]);
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, gas_var_name, harp_type_double, 3, dimension_type, NULL, gas_description,
         HARP_UNIT_DIMENSIONLESS, NULL, read_vmr_avk);
    snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.MIXING.RATIO%s_EMISSION_AVK", gas_name[gas],
             version == 1 ? "" : ".VOLUME");
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, gas_mapping_path, NULL);

    /* H2O_column_number_density */
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, "H2O_column_number_density", harp_type_double, 1, dimension_type, NULL,
         "derived integrated water vapor partial column from retrieval", "molec/cm2", include_h2o_column,
         read_h2o_column);
    harp_variable_definition_add_mapping(variable_definition, NULL, "variable is available", "/H2O.COLUMN_DERIVED",
                                         NULL);

    return 0;
}

int harp_ingestion_module_geoms_mwr_init()
{
    harp_ingestion_module *module;
    int i;

    module = harp_ingestion_register_module("GEOMS-TE-MWR", "GEOMS", "GEOMS", "MWR", "GEOMS template for MWR",
                                            ingestion_init, ingestion_done);

    for (i = 0; i < num_mwr_gas; i++)
    {
        init_product_definition(module, i, 1);
        init_product_definition(module, i, 2);
        init_product_definition(module, i, 3);
    }

    return 0;
}
