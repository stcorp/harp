/*
 * Copyright (C) 2015-2019 S[&]T, The Netherlands.
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
#include "harp-constants.h"
#include "harp-ingestion.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ------------------- Defines ------------------ */

#define SECONDS_PER_DAY                  86400
#define SECONDS_FROM_1990_TO_2000    315532800

#define CHECKED_MALLOC(v, s) v = malloc(s); if (v == NULL) { harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)", s, __FILE__, __LINE__); return -1;}

#define MAX_WAVELENGTHS                     10

/* ------------------ Typedefs ------------------ */

typedef struct ingest_info_struct
{
    coda_product *product;
    long num_time;
    long num_latitude;
    long num_longitude;
} ingest_info;

/* -------------------- Code -------------------- */

static void ingestion_done(void *user_data)
{
    ingest_info *info = (ingest_info *)user_data;

    free(info);
}

static int read_dataset(ingest_info *info, const char *path, long num_elements, harp_array data)
{
    coda_cursor cursor;
    long coda_num_elements;
    harp_scalar fill_value;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, path) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &coda_num_elements) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_num_elements != num_elements)
    {
        harp_set_error(HARP_ERROR_INGESTION, "dataset has %ld elements (expected %ld)", coda_num_elements,
                       num_elements);
        harp_add_coda_cursor_path_to_error_message(&cursor);
        harp_add_error_message(" (%s:%lu)", __FILE__, __LINE__);
        return -1;
    }
    if (coda_cursor_read_double_array(&cursor, data.double_data, coda_array_ordering_c) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    fill_value.double_data = 1.0E20;
    harp_array_replace_fill_value(harp_type_double, num_elements, data, fill_value);

    return 0;
}

static int read_datetime(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    double *double_data = data.double_data;
    long i;
    int retval;

    retval = read_dataset(info, "time", info->num_time, data);
    for (i = 0; i < info->num_time; i++)
    {
        *double_data = (*double_data * SECONDS_PER_DAY) - SECONDS_FROM_1990_TO_2000;
        double_data++;
    }
    return retval;
}

static int read_latitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "lat", info->num_latitude, data);
}

static int read_longitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "lon", info->num_longitude, data);
}

static int read_CH4_column_volume_mixing_ratio(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    double *double_data;
    long i;
    int retval;

    retval = read_dataset(info, "xch4", info->num_time * info->num_latitude * info->num_longitude, data);

    /* Convert from unit ratio to parts per million */
    double_data = data.double_data;
    for (i = 0; i < (info->num_time * info->num_latitude * info->num_longitude); i++)
    {
        *double_data *= 1E6;
        double_data++;
    }

    return retval;
}

static int read_CH4_column_volume_mixing_ratio_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    double *double_data;
    long i;
    int retval;

    retval = read_dataset(info, "xch4_stddev", info->num_time * info->num_latitude * info->num_longitude, data);

    /* Convert from unit ratio to parts per million */
    double_data = data.double_data;
    for (i = 0; i < (info->num_time * info->num_latitude * info->num_longitude); i++)
    {
        *double_data *= 1E6;
        double_data++;
    }

    return retval;
}

static int read_CO2_column_volume_mixing_ratio(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    double *double_data;
    long i;
    int retval;

    retval = read_dataset(info, "xco2", info->num_time * info->num_latitude * info->num_longitude, data);

    /* Convert from unit ratio to parts per million */
    double_data = data.double_data;
    for (i = 0; i < (info->num_time * info->num_latitude * info->num_longitude); i++)
    {
        *double_data *= 1E6;
        double_data++;
    }

    return retval;
}

static int read_CO2_column_volume_mixing_ratio_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    double *double_data;
    long i;
    int retval;

    retval = read_dataset(info, "xco2_stddev", info->num_time * info->num_latitude * info->num_longitude, data);

    /* Convert from unit ratio to parts per million */
    double_data = data.double_data;
    for (i = 0; i < (info->num_time * info->num_latitude * info->num_longitude); i++)
    {
        *double_data *= 1E6;
        double_data++;
    }

    return retval;
}

static int read_dimensions(void *user_data, long dimension[HARP_NUM_DIM_TYPES])
{
    ingest_info *info = (ingest_info *)user_data;

    dimension[harp_dimension_time] = info->num_time;
    dimension[harp_dimension_latitude] = info->num_latitude;
    dimension[harp_dimension_longitude] = info->num_longitude;

    return 0;
}

static int init_dimensions(ingest_info *info)
{
    coda_cursor cursor;
    long coda_dim[CODA_MAX_NUM_DIMS];
    int num_coda_dims;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    if (coda_cursor_goto(&cursor, "time") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_array_dim(&cursor, &num_coda_dims, coda_dim) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (num_coda_dims != 1)
    {
        harp_set_error(HARP_ERROR_INGESTION, "dataset has %d dimensions, expected 1", num_coda_dims);
        harp_add_coda_cursor_path_to_error_message(&cursor);
        return -1;
    }
    info->num_time = coda_dim[0];
    coda_cursor_goto_parent(&cursor);

    if (coda_cursor_goto(&cursor, "lat") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_array_dim(&cursor, &num_coda_dims, coda_dim) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (num_coda_dims != 1)
    {
        harp_set_error(HARP_ERROR_INGESTION, "dataset has %d dimensions, expected 1", num_coda_dims);
        harp_add_coda_cursor_path_to_error_message(&cursor);
        return -1;
    }
    info->num_latitude = coda_dim[0];
    coda_cursor_goto_parent(&cursor);

    if (coda_cursor_goto(&cursor, "lon") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_array_dim(&cursor, &num_coda_dims, coda_dim) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (num_coda_dims != 1)
    {
        harp_set_error(HARP_ERROR_INGESTION, "dataset has %d dimensions, expected 1", num_coda_dims);
        harp_add_coda_cursor_path_to_error_message(&cursor);
        return -1;
    }
    info->num_longitude = coda_dim[0];

    return 0;
}

static int include_ch4(void *user_data)
{
    ingest_info *info = (ingest_info *)user_data;
    coda_cursor cursor;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        return 0;
    }
    if (coda_cursor_goto(&cursor, "xch4") != 0)
    {
        return 0;
    }
    return 1;
}

static int include_co2(void *user_data)
{
    ingest_info *info = (ingest_info *)user_data;
    coda_cursor cursor;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        return 0;
    }
    if (coda_cursor_goto(&cursor, "xco2") != 0)
    {
        return 0;
    }
    return 1;
}

static int ingestion_init(const harp_ingestion_module *module, coda_product *product,
                          const harp_ingestion_options *options, harp_product_definition **definition, void **user_data)
{
    ingest_info *info;
    coda_cursor cursor;

    (void)options;

    CHECKED_MALLOC(info, sizeof(ingest_info));
    info->product = product;
    if (init_dimensions(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }
    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    *definition = *module->product_definition;
    *user_data = info;

    return 0;
}

static void register_module_l3_Obs4MIPs(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[3] = { harp_dimension_time, harp_dimension_latitude, harp_dimension_longitude };
    const char *description;
    const char *path;

    module = harp_ingestion_register_module_coda("ESACCI_GHG_L3_Obs4MIPs", "Green House Gases CCI", "ESACCI_GHG",
                                                 "Obs4MIPs_L3", "CCI L3 Obs4MIPs Green House Gases profile",
                                                 ingestion_init, ingestion_done);
    product_definition = harp_ingestion_register_product(module, "ESACCI_GHG_L3_Obs4MIPs", NULL, read_dimensions);

    /* datetime */
    description = "time of the measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double, 1, dimension_type,
                                                   NULL, description, "seconds since 2000-01-01", NULL, read_datetime);
    path = "time[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* latitude */
    description = "latitude of the ground pixel center";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_double, 1,
                                                   &(dimension_type[1]), NULL, description, "degree_north", NULL,
                                                   read_latitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    path = "lat[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* longitude */
    description = "longitude of the ground pixel center";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_double, 1,
                                                   &(dimension_type[2]), NULL, description, "degree_east", NULL,
                                                   read_longitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0, 180.0);
    path = "long[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* CH4_column_volume_mixing_ratio */
    description = "CH4 column volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "CH4_column_volume_mixing_ratio",
                                                   harp_type_double, 3, dimension_type, NULL, description, "ppmv",
                                                   include_ch4, read_CH4_column_volume_mixing_ratio);
    path = "xch4[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* CH4_column_volume_mixing_ratio_uncertainty */
    description = "CH4 column volume mixing ratio standard deviation";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "CH4_column_volume_mixing_ratio_uncertainty",
                                                   harp_type_double, 3, dimension_type, NULL, description, "ppmv",
                                                   include_ch4, read_CH4_column_volume_mixing_ratio_uncertainty);
    path = "xch4_stddev[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* CO2_column_volume_mixing_ratio */
    description = "CO2 column volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "CO2_column_volume_mixing_ratio",
                                                   harp_type_double, 3, dimension_type, NULL, description, "ppmv",
                                                   include_co2, read_CO2_column_volume_mixing_ratio);
    path = "xco2[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* CO2_column_volume_mixing_ratio_uncertainty */
    description = "CO2 column volume mixing ratio standard deviation";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "CO2_column_volume_mixing_ratio_uncertainty",
                                                   harp_type_double, 3, dimension_type, NULL, description, "ppmv",
                                                   include_co2, read_CO2_column_volume_mixing_ratio_uncertainty);
    path = "xco2_stddev[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

int harp_ingestion_module_cci_l3_ghg_init(void)
{
    register_module_l3_Obs4MIPs();
    return 0;
}
