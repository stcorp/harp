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

#define MAX_NAME_LENGTH 90
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

/* read variable with an optional time dimension; if there is no time dimension then replicate the data */
static int read_variable_float_opt_time_dep(void *user_data, const char *path, long num_elements, harp_array data)
{
    coda_cursor cursor;
    long num_time = ((ingest_info *)user_data)->num_time;
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
    if (num_time > 1 && num_time * actual_num_elements == num_elements)
    {
        long i, k;

        if (read_variable_float(user_data, path, actual_num_elements, data) != 0)
        {
            return -1;
        }

        /* replicate data in the time dimension */
        for (k = 1; k < num_time; k++)
        {
            for (i = 0; i < actual_num_elements; i++)
            {
                data.float_data[k * actual_num_elements + i] = data.float_data[i];
            }
        }

        return 0;
    }

    return read_variable_float(user_data, path, num_elements, data);
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
    coda_cursor cursor;
    long actual_num_elements;

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
    if (coda_cursor_get_num_elements(&cursor, &actual_num_elements) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (actual_num_elements != ((ingest_info *)user_data)->num_time)
    {
        harp_set_error(HARP_ERROR_INGESTION, "variable DATETIME has %ld elements (expected %ld)", actual_num_elements,
                       ((ingest_info *)user_data)->num_time);
        return -1;
    }
    if (coda_cursor_read_double_array(&cursor, data.double_data, coda_array_ordering_c) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    return 0;
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

static int read_surface_pressure(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(user_data, "SURFACE_PRESSURE_INDEPENDENT", info->num_time, data);
}

static int read_surface_temperature(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(user_data, "SURFACE_TEMPERATURE_INDEPENDENT", info->num_time, data);
}

static int read_altitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float_opt_time_dep(user_data, "ALTITUDE", info->num_time * info->num_vertical, data);
}

static int read_pressure(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(user_data, "PRESSURE_INDEPENDENT", info->num_time * info->num_vertical, data);
}

static int read_temperature(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(user_data, "TEMPERATURE_INDEPENDENT", info->num_time * info->num_vertical, data);
}

static int read_n2o_column_vmr(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(user_data, "N2O_COLUMN_MIXING_RATIO_VOLUME_DRY_ABSORPTION_SOLAR", info->num_time, data);
}

static int read_n2o_column_vmr_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(user_data,
                               "N2O_COLUMN_MIXING_RATIO_VOLUME_DRY_ABSORPTION_SOLAR_UNCERTAINTY_RANDOM_STANDARD",
                               info->num_time, data);
}

static int read_n2o_apriori(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(user_data, "N2O_MIXING_RATIO_VOLUME_APRIORI", info->num_time * info->num_vertical, data);
}

static int read_n2o_avk(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(user_data, "N2O_COLUMN_ABSORPTION_SOLAR_AVK", info->num_time * info->num_vertical, data);
}

static int read_hf_column_vmr(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(user_data, "HF_COLUMN_MIXING_RATIO_VOLUME_DRY_ABSORPTION_SOLAR", info->num_time, data);
}

static int read_hf_column_vmr_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(user_data,
                               "HF_COLUMN_MIXING_RATIO_VOLUME_DRY_ABSORPTION_SOLAR_UNCERTAINTY_RANDOM_STANDARD",
                               info->num_time, data);
}

static int read_hf_apriori(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(user_data, "HF_MIXING_RATIO_VOLUME_APRIORI", info->num_time * info->num_vertical, data);
}

static int read_hf_avk(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(user_data, "HF_COLUMN_ABSORPTION_SOLAR_AVK", info->num_time * info->num_vertical, data);
}

static int read_hdo_column_vmr(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(user_data, "HDO_COLUMN_MIXING_RATIO_VOLUME_DRY_ABSORPTION_SOLAR", info->num_time, data);
}

static int read_hdo_column_vmr_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(user_data,
                               "HDO_COLUMN_MIXING_RATIO_VOLUME_DRY_ABSORPTION_SOLAR_UNCERTAINTY_RANDOM_STANDARD",
                               info->num_time, data);
}

static int read_hdo_apriori(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(user_data, "HDO_MIXING_RATIO_VOLUME_APRIORI", info->num_time * info->num_vertical, data);
}

static int read_hdo_avk(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(user_data, "HDO_COLUMN_ABSORPTION_SOLAR_AVK", info->num_time * info->num_vertical, data);
}

static int read_h2o_column_vmr(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(user_data, "H2O_COLUMN_MIXING_RATIO_VOLUME_DRY_ABSORPTION_SOLAR", info->num_time, data);
}

static int read_h2o_column_vmr_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(user_data,
                               "H2O_COLUMN_MIXING_RATIO_VOLUME_DRY_ABSORPTION_SOLAR_UNCERTAINTY_RANDOM_STANDARD",
                               info->num_time, data);
}

static int read_h2o_apriori(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(user_data, "H2O_MIXING_RATIO_VOLUME_APRIORI", info->num_time * info->num_vertical, data);
}

static int read_h2o_avk(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(user_data, "H2O_COLUMN_ABSORPTION_SOLAR_AVK", info->num_time * info->num_vertical, data);
}

static int read_co_column_vmr(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(user_data, "CO_COLUMN_MIXING_RATIO_VOLUME_DRY_ABSORPTION_SOLAR", info->num_time, data);
}

static int read_co_column_vmr_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(user_data,
                               "CO_COLUMN_MIXING_RATIO_VOLUME_DRY_ABSORPTION_SOLAR_UNCERTAINTY_RANDOM_STANDARD",
                               info->num_time, data);
}

static int read_co_apriori(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(user_data, "CO_MIXING_RATIO_VOLUME_APRIORI", info->num_time * info->num_vertical, data);
}

static int read_co_avk(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(user_data, "CO_COLUMN_ABSORPTION_SOLAR_AVK", info->num_time * info->num_vertical, data);
}

static int read_co2_column_vmr(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(user_data, "CO2_COLUMN_MIXING_RATIO_VOLUME_DRY_ABSORPTION_SOLAR", info->num_time, data);
}

static int read_co2_column_vmr_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(user_data,
                               "CO2_COLUMN_MIXING_RATIO_VOLUME_DRY_ABSORPTION_SOLAR_UNCERTAINTY_RANDOM_STANDARD",
                               info->num_time, data);
}

static int read_co2_apriori(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(user_data, "CO2_MIXING_RATIO_VOLUME_APRIORI", info->num_time * info->num_vertical, data);
}

static int read_co2_avk(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(user_data, "CO2_COLUMN_ABSORPTION_SOLAR_AVK", info->num_time * info->num_vertical, data);
}

static int read_ch4_column_vmr(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(user_data, "CH4_COLUMN_MIXING_RATIO_VOLUME_DRY_ABSORPTION_SOLAR", info->num_time, data);
}

static int read_ch4_column_vmr_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(user_data,
                               "CH4_COLUMN_MIXING_RATIO_VOLUME_DRY_ABSORPTION_SOLAR_UNCERTAINTY_RANDOM_STANDARD",
                               info->num_time, data);
}

static int read_ch4_apriori(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(user_data, "CH4_MIXING_RATIO_VOLUME_APRIORI", info->num_time * info->num_vertical, data);
}

static int read_ch4_avk(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(user_data, "CH4_COLUMN_ABSORPTION_SOLAR_AVK", info->num_time * info->num_vertical, data);
}

static int read_o2_apriori(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(user_data, "O2_MIXING_RATIO_VOLUME_APRIORI", info->num_time * info->num_vertical, data);
}

static int read_o2_avk(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(user_data, "O2_COLUMN_ABSORPTION_SOLAR_AVK", info->num_time * info->num_vertical, data);
}

static int read_o2_amf(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(user_data, "O2_COLUMN_ABSORPTION_SOLAR_AMF", info->num_time, data);
}

static int read_solar_zenith_angle(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(user_data, "ANGLE_SOLAR_ZENITH_ASTRONOMICAL", info->num_time, data);
}

static int read_solar_azimuth_angle(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(user_data, "ANGLE_SOLAR_AZIMUTH", info->num_time, data);
}

static int read_gravity(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(user_data, "GRAVITY_INDEPENDENT", info->num_time * info->num_vertical, data);
}

static int read_surface_wind_speed(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(user_data, "WIND_SPEED_SURFACE_INDEPENDENT", info->num_time, data);
}

static int read_surface_wind_direction(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(user_data, "WIND_DIRECTION_SURFACE_INDEPENDENT", info->num_time, data);
}

static int read_surface_relative_humidity(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(user_data, "HUMIDITY_RELATIVE_SURFACE_INDEPENDENT", info->num_time, data);
}

static int read_number_density(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(user_data, "NUMBER_DENSITY_INDEPENDENT", info->num_time * info->num_vertical, data);
}

static void ingestion_done(void *user_data)
{
    free(user_data);
}

static int get_product_definition(const harp_ingestion_module *module, coda_product *product,
                                  harp_product_definition **definition)
{
    coda_cursor cursor;
    char template_name[24];
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
    /* template should match the pattern "GEOMS-TE-FTIR-TCCON-xxx" */
    if (length != 23 && length != 24)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, "invalid string length for DATA_TEMPLATE global attribute");
        return -1;
    }
    if (coda_cursor_read_string(&cursor, template_name, 24) != 0)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, NULL);
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
    long dim[CODA_MAX_NUM_DIMS];
    int num_dims;

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
    if (coda_cursor_get_array_dim(&cursor, &num_dims, dim) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (num_dims == 1)
    {
        info->num_vertical = dim[0];
    }
    else if (num_dims == 2)
    {
        info->num_vertical = dim[1];
    }
    else
    {
        harp_set_error(HARP_ERROR_INGESTION, "ALTITUDE variable should be one or two dimensional");
        return -1;
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
    info->product_version = info->definition->name[16] - '0';

    if (get_dimensions(info) != 0)
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
    harp_dimension_type dimension_type[2];
    char product_name[MAX_NAME_LENGTH];
    char product_description[MAX_DESCRIPTION_LENGTH];
    const char *description;
    const char *unit;

    snprintf(product_name, MAX_NAME_LENGTH, "GEOMS-TE-FTIR-TCCON-%03d", version);
    snprintf(product_description, MAX_DESCRIPTION_LENGTH, "GEOMS template for FTIR TCCON v%03d", version);
    product_definition = harp_ingestion_register_product(module, product_name, product_description, read_dimensions);

    dimension_type[0] = harp_dimension_time;
    dimension_type[1] = harp_dimension_vertical;

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
    description = "effective measurement time";
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
    description = "altitude of the sensor";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "sensor_altitude",
                                                                     harp_type_float, 0, NULL, NULL, description, "km",
                                                                     NULL, read_instrument_altitude);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ALTITUDE.INSTRUMENT", NULL);

    /* n2o_column_volume_mixing_ratio_dry_air */
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, "N2O_column_volume_mixing_ratio_dry_air", harp_type_float, 1, dimension_type, NULL,
         "0.2095 * column_N2O/column_O2", "ppbv", NULL, read_n2o_column_vmr);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/N2O.COLUMN.MIXING.RATIO.VOLUME.DRY_ABSORPTION.SOLAR", NULL);

    /* n2o_column_volume_mixing_ratio_dry_air_uncertainty */
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, "N2O_column_volume_mixing_ratio_dry_air_uncertainty", harp_type_float, 1, dimension_type,
         NULL, "total random uncertainty on the retrieved total column (without smoothing error)", "ppbv", NULL,
         read_n2o_column_vmr_uncertainty);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/N2O.COLUMN.MIXING.RATIO.VOLUME.DRY_"
                                         "ABSORPTION.SOLAR_UNCERTAINTY.RANDOM.STANDARD", NULL);

    /* n2o_volume_mixing_ratio_apriori */
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, "N2O_volume_mixing_ratio_apriori", harp_type_float, 2, dimension_type, NULL,
         "apriori profile of N2O volume mixing ratios", "ppbv", NULL, read_n2o_apriori);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/N2O.MIXING.RATIO.VOLUME_APRIORI", NULL);

    /* n2o_column_number_density_avk */
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, "N2O_column_number_density_avk", harp_type_float, 2, dimension_type, NULL,
         "averaging kernel matrix for the total N2O vertical column", HARP_UNIT_DIMENSIONLESS, NULL, read_n2o_avk);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/N2O.COLUMN_ABSORPTION.SOLAR_AVK", NULL);

    /* HF_column_volume_mixing_ratio_dry_air */
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, "HF_column_volume_mixing_ratio_dry_air", harp_type_float, 1, dimension_type, NULL,
         "0.2095 * column_HF/column_O2", "pptv", NULL, read_hf_column_vmr);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/HF.COLUMN.MIXING.RATIO.VOLUME.DRY_ABSORPTION.SOLAR", NULL);

    /* HF_column_volume_mixing_ratio_dry_air_uncertainty */
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, "HF_column_volume_mixing_ratio_dry_air_uncertainty", harp_type_float, 1, dimension_type,
         NULL, "total random uncertainty on the retrieved total column (without smoothing error)", "pptv", NULL,
         read_hf_column_vmr_uncertainty);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/HF.COLUMN.MIXING.RATIO.VOLUME.DRY_"
                                         "ABSORPTION.SOLAR_UNCERTAINTY.RANDOM.STANDARD", NULL);

    /* HF_volume_mixing_ratio_apriori */
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, "HF_volume_mixing_ratio_apriori", harp_type_float, 2, dimension_type, NULL,
         "apriori profile of HF volume mixing ratios", "pptv", NULL, read_hf_apriori);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/HF.MIXING.RATIO.VOLUME_APRIORI", NULL);

    /* HF_column_number_density_avk */
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, "HF_column_number_density_avk", harp_type_float, 2, dimension_type, NULL,
         "averaging kernel matrix for the total HF vertical column", HARP_UNIT_DIMENSIONLESS, NULL, read_hf_avk);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/HF.COLUMN_ABSORPTION.SOLAR_AVK", NULL);

    /* H2O_162_column_volume_mixing_ratio_dry_air */
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, "H2O_162_column_volume_mixing_ratio_dry_air", harp_type_float, 1, dimension_type, NULL,
         "0.2095 * column_HDO/column_O2", "ppmv", NULL, read_hdo_column_vmr);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/HDO.COLUMN.MIXING.RATIO.VOLUME.DRY_ABSORPTION.SOLAR", NULL);

    /* H2O_162_column_volume_mixing_ratio_dry_air_uncertainty */
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, "H2O_162_column_volume_mixing_ratio_dry_air_uncertainty", harp_type_float, 1,
         dimension_type, NULL, "total random uncertainty on the retrieved total column (without smoothing error)",
         "ppmv", NULL, read_hdo_column_vmr_uncertainty);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/HDO.COLUMN.MIXING.RATIO.VOLUME.DRY_"
                                         "ABSORPTION.SOLAR_UNCERTAINTY.RANDOM.STANDARD", NULL);

    /* H2O_162_volume_mixing_ratio_apriori */
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, "H2O_162_volume_mixing_ratio_apriori", harp_type_float, 2, dimension_type, NULL,
         "apriori profile of HDO volume mixing ratios", "ppmv", NULL, read_hdo_apriori);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/HDO.MIXING.RATIO.VOLUME_APRIORI", NULL);

    /* H2O_162_column_number_density_avk */
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, "H2O_162_column_number_density_avk", harp_type_float, 2, dimension_type, NULL,
         "averaging kernel matrix for the total HDO vertical column", HARP_UNIT_DIMENSIONLESS, NULL, read_hdo_avk);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/HDO.COLUMN_ABSORPTION.SOLAR_AVK", NULL);

    /* H2O_column_volume_mixing_ratio_dry_air */
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, "H2O_column_volume_mixing_ratio_dry_air", harp_type_float, 1, dimension_type, NULL,
         "0.2095 * column_H2O/column_O2", "ppmv", NULL, read_h2o_column_vmr);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/H2O.COLUMN.MIXING.RATIO.VOLUME.DRY_ABSORPTION.SOLAR", NULL);

    /* H2O_column_volume_mixing_ratio_dry_air_uncertainty */
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, "H2O_column_volume_mixing_ratio_dry_air_uncertainty", harp_type_float, 1, dimension_type,
         NULL, "total random uncertainty on the retrieved total column (without smoothing error)", "ppmv", NULL,
         read_h2o_column_vmr_uncertainty);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/H2O.COLUMN.MIXING.RATIO.VOLUME.DRY_"
                                         "ABSORPTION.SOLAR_UNCERTAINTY.RANDOM.STANDARD", NULL);

    /* H2O_volume_mixing_ratio_apriori */
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, "H2O_volume_mixing_ratio_apriori", harp_type_float, 2, dimension_type, NULL,
         "apriori profile of H2O volume mixing ratios", "ppmv", NULL, read_h2o_apriori);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/H2O.MIXING.RATIO.VOLUME_APRIORI", NULL);

    /* H2O_column_number_density_avk */
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, "H2O_column_number_density_avk", harp_type_float, 2, dimension_type, NULL,
         "averaging kernel matrix for the total H2O vertical column", HARP_UNIT_DIMENSIONLESS, NULL, read_h2o_avk);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/H2O.COLUMN_ABSORPTION.SOLAR_AVK", NULL);

    /* CO_column_volume_mixing_ratio_dry_air */
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, "CO_column_volume_mixing_ratio_dry_air", harp_type_float, 1, dimension_type, NULL,
         "0.2095 * column_CO/column_O2", "ppbv", NULL, read_co_column_vmr);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/CO.COLUMN.MIXING.RATIO.VOLUME.DRY_ABSORPTION.SOLAR", NULL);

    /* CO_column_volume_mixing_ratio_dry_air_uncertainty */
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, "CO_column_volume_mixing_ratio_dry_air_uncertainty", harp_type_float, 1, dimension_type,
         NULL, "total random uncertainty on the retrieved total column (without smoothing error)", "ppbv", NULL,
         read_co_column_vmr_uncertainty);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/CO.COLUMN.MIXING.RATIO.VOLUME.DRY_"
                                         "ABSORPTION.SOLAR_UNCERTAINTY.RANDOM.STANDARD", NULL);

    /* CO_volume_mixing_ratio_apriori */
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, "CO_volume_mixing_ratio_apriori", harp_type_float, 2, dimension_type, NULL,
         "apriori profile of CO volume mixing ratios", "ppbv", NULL, read_co_apriori);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/CO.MIXING.RATIO.VOLUME_APRIORI", NULL);

    /* CO_column_number_density_avk */
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, "CO_column_number_density_avk", harp_type_float, 2, dimension_type, NULL,
         "averaging kernel matrix for the total CO vertical column", HARP_UNIT_DIMENSIONLESS, NULL, read_co_avk);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/CO.COLUMN_ABSORPTION.SOLAR_AVK", NULL);

    /* CO2_column_volume_mixing_ratio_dry_air */
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, "CO2_column_volume_mixing_ratio_dry_air", harp_type_float, 1, dimension_type, NULL,
         "0.2095 * column_CO2/column_O2", "ppmv", NULL, read_co2_column_vmr);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/CO2.COLUMN.MIXING.RATIO.VOLUME.DRY_ABSORPTION.SOLAR", NULL);

    /* CO2_column_volume_mixing_ratio_dry_air_uncertainty */
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, "CO2_column_volume_mixing_ratio_dry_air_uncertainty", harp_type_float, 1, dimension_type,
         NULL, "total random uncertainty on the retrieved total column (without smoothing error)", "ppmv", NULL,
         read_co2_column_vmr_uncertainty);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/CO2.COLUMN.MIXING.RATIO.VOLUME.DRY_"
                                         "ABSORPTION.SOLAR_UNCERTAINTY.RANDOM.STANDARD", NULL);

    /* CO2_volume_mixing_ratio_apriori */
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, "CO2_volume_mixing_ratio_apriori", harp_type_float, 2, dimension_type, NULL,
         "apriori profile of CO2 volume mixing ratios", "ppmv", NULL, read_co2_apriori);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/CO2.MIXING.RATIO.VOLUME_APRIORI", NULL);

    /* CO2_column_number_density_avk */
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, "CO2_column_number_density_avk", harp_type_float, 2, dimension_type, NULL,
         "averaging kernel matrix for the total CO2 vertical column", HARP_UNIT_DIMENSIONLESS, NULL, read_co2_avk);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/CO2.COLUMN_ABSORPTION.SOLAR_AVK", NULL);

    /* CH4_column_volume_mixing_ratio_dry_air */
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, "CH4_column_volume_mixing_ratio_dry_air", harp_type_float, 1, dimension_type, NULL,
         "0.2095 * column_CH4/column_O2", "ppmv", NULL, read_ch4_column_vmr);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/CH4.COLUMN.MIXING.RATIO.VOLUME.DRY_ABSORPTION.SOLAR", NULL);

    /* CH4_column_volume_mixing_ratio_dry_air_uncertainty */
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, "CH4_column_volume_mixing_ratio_dry_air_uncertainty", harp_type_float, 1, dimension_type,
         NULL, "total random uncertainty on the retrieved total column (without smoothing error)", "ppmv", NULL,
         read_ch4_column_vmr_uncertainty);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/CH4.COLUMN.MIXING.RATIO.VOLUME.DRY_"
                                         "ABSORPTION.SOLAR_UNCERTAINTY.RANDOM.STANDARD", NULL);

    /* CH4_volume_mixing_ratio_apriori */
    unit = version == 5 ? "ppbv" : "ppmv";
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, "CH4_volume_mixing_ratio_apriori", harp_type_float, 2, dimension_type, NULL,
         "apriori profile of CH4 volume mixing ratios", unit, NULL, read_ch4_apriori);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/CH4.MIXING.RATIO.VOLUME_APRIORI", NULL);

    /* CH4_column_number_density_avk */
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, "CH4_column_number_density_avk", harp_type_float, 2, dimension_type, NULL,
         "averaging kernel matrix for the total CH4 vertical column", HARP_UNIT_DIMENSIONLESS, NULL, read_ch4_avk);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/CH4.COLUMN_ABSORPTION.SOLAR_AVK", NULL);

    if (version >= 6)
    {
        /* O2_volume_mixing_ratio_apriori */
        variable_definition = harp_ingestion_register_variable_full_read
            (product_definition, "O2_volume_mixing_ratio_apriori", harp_type_float, 2, dimension_type, NULL,
             "apriori profile of O2 volume mixing ratios", "ppv", NULL, read_o2_apriori);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/O2.MIXING.RATIO.VOLUME_APRIORI", NULL);

        /* O2_column_number_density_avk */
        variable_definition = harp_ingestion_register_variable_full_read
            (product_definition, "O2_column_number_density_avk", harp_type_float, 2, dimension_type, NULL,
             "averaging kernel matrix for the total O2 vertical column", HARP_UNIT_DIMENSIONLESS, NULL, read_o2_avk);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/O2.COLUMN_ABSORPTION.SOLAR_AVK", NULL);

        /* O2_column_number_density_amf */
        variable_definition = harp_ingestion_register_variable_full_read
            (product_definition, "O2_column_number_density_amf", harp_type_float, 1, dimension_type, NULL,
             "airmass computed as the total vertical column of O2 divided by the total slant column of O2 retrieved "
             "from the window centered at 7885 cm-1", HARP_UNIT_DIMENSIONLESS, NULL, read_o2_amf);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/O2.COLUMN_ABSORPTION.SOLAR_AVK", NULL);
    }

    /* altitude */
    description = "a priori altitude profile";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "altitude", harp_type_float, 2,
                                                                     dimension_type, NULL, description, "km", NULL,
                                                                     read_altitude);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ALTITUDE", NULL);

    /* surface_pressure */
    description = "independent surface pressure";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "surface_pressure",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     description, "hPa", NULL, read_surface_pressure);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/SURFACE.PRESSURE_INDEPENDENT", NULL);

    /* surface_temperature */
    description = "independent surface temperature";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "surface_temperature",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     description, "K", NULL, read_surface_temperature);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/SURFACE.TEMPERATURE_INDEPENDENT", NULL);

    /* pressure */
    description = "independent pressure profile";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "pressure", harp_type_float,
                                                                     2, dimension_type, NULL, description, "hPa", NULL,
                                                                     read_pressure);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/PRESSURE_INDEPENDENT", NULL);

    /* temperature */
    description = "independent temperature profile";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "temperature", harp_type_float,
                                                                     2, dimension_type, NULL, description, "K", NULL,
                                                                     read_temperature);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/TEMPERATURE_INDEPENDENT", NULL);

    /* solar_azimuth_angle */
    description = "solar azimuth angle";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "solar_azimuth_angle",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     description, "degree", NULL,
                                                                     read_solar_azimuth_angle);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ANGLE.SOLAR_AZIMUTH", NULL);

    /* solar_zenith_angle */
    description = "solar zenith angle";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "solar_zenith_angle",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     description, "degree", NULL,
                                                                     read_solar_zenith_angle);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ANGLE.SOLAR_ZENITH.ASTRONOMICAL", NULL);

    /* gravity */
    description = "gravitational acceleration";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "gravity", harp_type_float, 2,
                                                                     dimension_type, NULL, description,
                                                                     HARP_UNIT_ACCELERATION, NULL, read_gravity);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/GRAVITY_INDEPENDENT", NULL);

    /* surface_wind_speed */
    description = "wind speed at the station";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "surface_wind_speed",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     description, "m/s", NULL, read_surface_wind_speed);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/WIND.SPEED.SURFACE_INDEPENDENT", NULL);

    /* surface_wind_direction */
    description = "wind direction at the station";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "surface_wind_direction",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     description, "degree", NULL,
                                                                     read_surface_wind_direction);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/WIND.DIRECTION.SURFACE_INDEPENDENT", NULL);

    /* surface_relative_humidity */
    description = "relative humidity at the station";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "surface_relative_humidity",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     description, "%", NULL,
                                                                     read_surface_relative_humidity);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/HUMIDITY.RELATIVE.SURFACE_INDEPENDENT",
                                         NULL);

    /* number_density */
    description = "independent air density profile";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "number_density",
                                                                     harp_type_float, 2, dimension_type, NULL,
                                                                     description, "molec/cm3", NULL,
                                                                     read_number_density);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/NUMBER.DENSITY_INDEPENDENT", NULL);

    return 0;
}

int harp_ingestion_module_geoms_tccon_init(void)
{
    harp_ingestion_module *module;

    module = harp_ingestion_register_module("GEOMS-TE-FTIR-TCCON", "GEOMS", "GEOMS", "FTIR_TCCON",
                                            "GEOMS template for TCCON FTIR", ingestion_init, ingestion_done);

    init_product_definition(module, 5);
    init_product_definition(module, 6);

    return 0;
}
