/*
 * Copyright (C) 2015-2025 S[&]T, The Netherlands.
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

#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct ingest_info_struct
{
    coda_product *product;
    long num_time;
    long num_latitude;
    long num_longitude;
    long num_vertical;
} ingest_info;

static int init_dimensions_mzm(ingest_info *info)
{
    coda_cursor cursor;
    long coda_dim[CODA_MAX_NUM_DIMS];
    int num_coda_dims;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, "/ozone_mole_concentation") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_array_dim(&cursor, &num_coda_dims, coda_dim) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (num_coda_dims != 3)
    {
        harp_set_error(HARP_ERROR_INGESTION, "dataset has %d dimensions, expected 3", num_coda_dims);
        harp_add_coda_cursor_path_to_error_message(&cursor);
        return -1;
    }
    info->num_time = coda_dim[0];
    info->num_latitude = coda_dim[2];
    info->num_longitude = 0;
    info->num_vertical = coda_dim[1];

    return 0;
}

static int init_dimensions_mmzm(ingest_info *info)
{
    coda_cursor cursor;
    long coda_dim[CODA_MAX_NUM_DIMS];
    int num_coda_dims;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, "/merged_ozone_concentration") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_array_dim(&cursor, &num_coda_dims, coda_dim) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (num_coda_dims != 2)
    {
        harp_set_error(HARP_ERROR_INGESTION, "dataset has %d dimensions, expected 2", num_coda_dims);
        harp_add_coda_cursor_path_to_error_message(&cursor);
        return -1;
    }
    info->num_time = 1;
    info->num_latitude = coda_dim[1];
    info->num_longitude = 0;
    info->num_vertical = coda_dim[0];

    return 0;
}

static int init_dimensions_msmm(ingest_info *info)
{
    coda_cursor cursor;
    long coda_dim[CODA_MAX_NUM_DIMS];
    int num_coda_dims;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, "/merged_ozone_concentration") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_array_dim(&cursor, &num_coda_dims, coda_dim) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (num_coda_dims != 4)
    {
        harp_set_error(HARP_ERROR_INGESTION, "dataset has %d dimensions, expected 4", num_coda_dims);
        harp_add_coda_cursor_path_to_error_message(&cursor);
        return -1;
    }
    info->num_time = coda_dim[0];
    info->num_latitude = coda_dim[3];
    info->num_longitude = coda_dim[2];
    info->num_vertical = coda_dim[1];

    return 0;
}

static void ingestion_done(void *user_data)
{
    free(user_data);
}

static int ingestion_init_mzm(const harp_ingestion_module *module, coda_product *product,
                              const harp_ingestion_options *options, harp_product_definition **definition,
                              void **user_data)
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

    info->product = product;

    if (init_dimensions_mzm(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }

    *definition = *module->product_definition;
    *user_data = info;

    return 0;
}

static int ingestion_init_mmzm(const harp_ingestion_module *module, coda_product *product,
                               const harp_ingestion_options *options, harp_product_definition **definition,
                               void **user_data)
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

    info->product = product;

    if (init_dimensions_mmzm(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }

    *definition = *module->product_definition;
    *user_data = info;

    return 0;
}

static int ingestion_init_msmm(const harp_ingestion_module *module, coda_product *product,
                               const harp_ingestion_options *options, harp_product_definition **definition,
                               void **user_data)
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

    info->product = product;

    if (init_dimensions_msmm(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }

    *definition = *module->product_definition;
    *user_data = info;

    return 0;
}

static int read_dataset(ingest_info *info, const char *path, long num_elements, harp_array data)
{
    coda_cursor cursor;
    long coda_num_elements;

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

    return 0;
}

static int read_reordered_dataset(ingest_info *info, const char *path, int num_dimensions, const long *dimension,
                                  const int *order, harp_array data)
{
    if (read_dataset(info, path, harp_get_num_elements(num_dimensions, dimension), data) != 0)
    {
        return -1;
    }

    if (harp_array_transpose(harp_type_double, num_dimensions, dimension, order, data) != 0)
    {
        return -1;
    }

    return 0;
}

static int read_as_uncertainty(ingest_info *info, const char *path, const char *path_relerr, long num_elements,
                               harp_array data)
{
    harp_array relerr;
    long i;

    if (read_dataset(info, path, num_elements, data) != 0)
    {
        return -1;
    }

    relerr.ptr = malloc(num_elements * sizeof(double));
    if (relerr.ptr == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_elements * sizeof(double), __FILE__, __LINE__);
        return -1;
    }

    if (read_dataset(info, path_relerr, num_elements, relerr) != 0)
    {
        free(relerr.ptr);
        return -1;
    }

    /* Convert relative error (in percent) to standard deviation (same unit as the associated quantity). */
    for (i = 0; i < num_elements; i++)
    {
        data.double_data[i] *= relerr.double_data[i] * 0.01;    /* relative error is a percentage */
    }

    free(relerr.ptr);

    return 0;
}

static int read_dimensions(void *user_data, long dimension[HARP_NUM_DIM_TYPES])
{
    ingest_info *info = (ingest_info *)user_data;

    dimension[harp_dimension_time] = info->num_time;
    dimension[harp_dimension_latitude] = info->num_latitude;
    dimension[harp_dimension_longitude] = info->num_longitude;
    dimension[harp_dimension_vertical] = info->num_vertical;

    return 0;
}

static int read_datetime(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "/time", info->num_time, data);
}

static int read_datetime_mmzm(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    coda_cursor cursor;
    char str[9];
    long length;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, "/@year") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_string_length(&cursor, &length) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (length != 4)
    {
        harp_set_error(HARP_ERROR_INGESTION, "year value has length %ld; expected 4", length);
        harp_add_coda_cursor_path_to_error_message(&cursor);
        return -1;
    }
    if (coda_cursor_read_string(&cursor, str, 5) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, "/@month") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_string_length(&cursor, &length) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (length != 2)
    {
        harp_set_error(HARP_ERROR_INGESTION, "month value has length %ld; expected 2", length);
        harp_add_coda_cursor_path_to_error_message(&cursor);
        return -1;
    }
    if (coda_cursor_read_string(&cursor, &str[4], 3) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    strcat(str, "01");

    if (coda_time_string_to_double("yyyyMMdd", str, data.double_data) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    return 0;
}

static int read_latitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "/latitude_centers", info->num_latitude, data);
}

static int read_longitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "/longitude_centers", info->num_longitude, data);
}

static int read_altitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "/approximate_altitude", info->num_vertical, data);
}

static int read_pressure(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "/air_pressure", info->num_vertical, data);
}

static int read_o3_volume_mixing_ratio_mzm(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int order[3] = { 0, 2, 1 };
    long dimension[3];

    dimension[0] = info->num_time;
    dimension[1] = info->num_vertical;
    dimension[2] = info->num_latitude;
    return read_reordered_dataset(info, "/ozone_mixing_ratio", 3, dimension, order, data);
}

static int read_o3_volume_mixing_ratio_mmzm(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int order[3] = { 0, 2, 1 };
    long dimension[3];

    dimension[0] = info->num_time;
    dimension[1] = info->num_vertical;
    dimension[2] = info->num_latitude;
    return read_reordered_dataset(info, "/merged_ozone_vmr", 3, dimension, order, data);
}

static int read_o3_volume_mixing_ratio_uncertainty_mmzm(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int order[3] = { 0, 2, 1 };
    long dimension[3];

    dimension[0] = info->num_time;
    dimension[1] = info->num_vertical;
    dimension[2] = info->num_latitude;
    if (read_as_uncertainty(info, "/merged_ozone_vmr", "/uncertainty_of_merged_ozone",
                            harp_get_num_elements(3, dimension), data) != 0)
    {
        return -1;
    }

    return harp_array_transpose(harp_type_double, 3, dimension, order, data);
}

static int read_o3_volume_mixing_ratio_msmm(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int order[4] = { 0, 3, 2, 1 };
    long dimension[4];

    dimension[0] = info->num_time;
    dimension[1] = info->num_vertical;
    dimension[2] = info->num_longitude;
    dimension[3] = info->num_latitude;
    return read_reordered_dataset(info, "/merged_ozone_vmr", 4, dimension, order, data);
}

static int read_o3_volume_mixing_ratio_uncertainty_msmm(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int order[4] = { 0, 3, 2, 1 };
    long dimension[4];

    dimension[0] = info->num_time;
    dimension[1] = info->num_vertical;
    dimension[2] = info->num_longitude;
    dimension[3] = info->num_latitude;
    if (read_as_uncertainty(info, "/merged_ozone_vmr", "/uncertainty_of_merged_ozone",
                            harp_get_num_elements(4, dimension), data) != 0)
    {
        return -1;
    }

    return harp_array_transpose(harp_type_double, 4, dimension, order, data);
}

static int read_o3_number_density_mzm(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int order[3] = { 0, 2, 1 };
    long dimension[3];

    dimension[0] = info->num_time;
    dimension[1] = info->num_vertical;
    dimension[2] = info->num_latitude;
    return read_reordered_dataset(info, "/ozone_mole_concentation", 3, dimension, order, data);
}

static int read_o3_number_density_mmzm(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int order[3] = { 0, 2, 1 };
    long dimension[3];

    dimension[0] = info->num_time;
    dimension[1] = info->num_vertical;
    dimension[2] = info->num_latitude;
    return read_reordered_dataset(info, "/merged_ozone_concentration", 3, dimension, order, data);
}

static int read_o3_number_density_uncertainty_mmzm(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int order[3] = { 0, 2, 1 };
    long dimension[3];

    dimension[0] = info->num_time;
    dimension[1] = info->num_vertical;
    dimension[2] = info->num_latitude;
    if (read_as_uncertainty(info, "/merged_ozone_concentration", "/uncertainty_of_merged_ozone",
                            harp_get_num_elements(3, dimension), data) != 0)
    {
        return -1;
    }

    return harp_array_transpose(harp_type_double, 3, dimension, order, data);
}

static int read_o3_number_density_msmm(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int order[4] = { 0, 3, 2, 1 };
    long dimension[4];

    dimension[0] = info->num_time;
    dimension[1] = info->num_vertical;
    dimension[2] = info->num_longitude;
    dimension[3] = info->num_latitude;
    return read_reordered_dataset(info, "/merged_ozone_concentration", 4, dimension, order, data);
}

static int read_o3_number_density_uncertainty_msmm(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int order[4] = { 0, 3, 2, 1 };
    long dimension[4];

    dimension[0] = info->num_time;
    dimension[1] = info->num_vertical;
    dimension[2] = info->num_longitude;
    dimension[3] = info->num_latitude;
    if (read_as_uncertainty(info, "/merged_ozone_concentration", "/uncertainty_of_merged_ozone",
                            harp_get_num_elements(4, dimension), data) != 0)
    {
        return -1;
    }

    return harp_array_transpose(harp_type_double, 4, dimension, order, data);
}

static void register_mzm_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[3] = { harp_dimension_time, harp_dimension_latitude, harp_dimension_vertical };
    harp_dimension_type latitude_dimension_type[1] = { harp_dimension_latitude };
    harp_dimension_type vertical_dimension_type[1] = { harp_dimension_vertical };
    const char *description;
    const char *path;

    module = harp_ingestion_register_module("ESACCI_OZONE_L3_LP_MZM", "Ozone CCI", "ESACCI_OZONE", "L3_LP_MZM",
                                            "CCI O3 monthly zonal mean limb " "profile on a 10 degree latitude "
                                            "grid", ingestion_init_mzm, ingestion_done);

    /* ESACCI_OZONE_L3_LP_MZM product */
    product_definition = harp_ingestion_register_product(module, "ESACCI_OZONE_L3_LP_MZM", NULL, read_dimensions);

    /* datetime */
    description = "time of the measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double, 1, dimension_type,
                                                   NULL, description, "days since 1990-01-01", NULL, read_datetime);
    path = "/time[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* latitude */
    description = "latitude of the bin center";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_double, 1,
                                                   latitude_dimension_type, NULL, description, "degree_north", NULL,
                                                   read_latitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    path = "/latitude_centers[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* altitude */
    description =
        "approximate altitude at pressure levels computed as 16 * log10(1013 / pressure), with pressure in hPa";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "altitude", harp_type_double, 1,
                                                   vertical_dimension_type, NULL, description, "km", NULL,
                                                   read_altitude);
    path = "/approximate_altitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* pressure */
    description = "pressure";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "pressure", harp_type_double, 1,
                                                   vertical_dimension_type, NULL, description, "hPa", NULL,
                                                   read_pressure);
    path = "/air_pressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* O3_volume_mixing_ratio */
    description = "monthly zonal mean ozone mixing ratio vertical profiles";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_volume_mixing_ratio", harp_type_double, 3,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_o3_volume_mixing_ratio_mzm);
    path = "/ozone_mixing_ratio[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* O3_number_density */
    description = "monthly zonal mean ozone mole concentration vertical profiles";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_number_density", harp_type_double, 3,
                                                   dimension_type, NULL, description, "mol/cm^3", NULL,
                                                   read_o3_number_density_mzm);
    path = "/ozone_mole_concentation[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

static void register_mmzm_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[3] = { harp_dimension_time, harp_dimension_latitude, harp_dimension_vertical };
    harp_dimension_type latitude_dimension_type[1] = { harp_dimension_latitude };
    harp_dimension_type vertical_dimension_type[1] = { harp_dimension_vertical };
    const char *description;
    const char *path;

    module = harp_ingestion_register_module("ESACCI_OZONE_L3_LP_MMZM", "Ozone CCI", "ESACCI_OZONE", "L3_LP_MMZM",
                                            "CCI O3 merged monthly zonal mean limb profile on a 10 degree "
                                            "latitude grid", ingestion_init_mmzm, ingestion_done);

    /* ESACCI_OZONE_L3_LP_MMZM product */
    product_definition = harp_ingestion_register_product(module, "ESACCI_OZONE_L3_LP_MMZM", NULL, read_dimensions);

    /* datetime */
    description = "time of the measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double, 1, dimension_type,
                                                   NULL, description, "seconds since 2000-01-01", NULL,
                                                   read_datetime_mmzm);
    path = "/@year, /@month";
    description =
        "year and month are taken from the global attributes of the product; the start of the first day of the month "
        "is used as the time of the measurement";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* longitude */
    description = "longitude of the bin center";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_double, 1,
                                                   latitude_dimension_type, NULL, description, "degree_east", NULL,
                                                   read_latitude);
    path = "/longitude_centers[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* latitude */
    description = "latitude of the bin center";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_double, 1,
                                                   latitude_dimension_type, NULL, description, "degree_north", NULL,
                                                   read_latitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    path = "/latitude_centers[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* altitude */
    description =
        "approximate altitude at pressure levels computed as 16 * log10(1013 / pressure), with pressure in hPa";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "altitude", harp_type_double, 1,
                                                   vertical_dimension_type, NULL, description, "km", NULL,
                                                   read_altitude);
    path = "/approximate_altitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* pressure */
    description = "pressure";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "pressure", harp_type_double, 1,
                                                   vertical_dimension_type, NULL, description, "hPa", NULL,
                                                   read_pressure);
    path = "/air_pressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* O3_volume_mixing_ratio */
    description = "merged monthly zonal mean ozone mixing ratio vertical profiles";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_volume_mixing_ratio", harp_type_double, 3,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_o3_volume_mixing_ratio_mmzm);
    path = "/merged_ozone_vmr[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* O3_volume_mixing_ratio_uncertainty */
    description = "uncertainty of the merged monthly zonal mean ozone mixing ratio vertical profiles";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_volume_mixing_ratio_uncertainty",
                                                   harp_type_double, 3, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_o3_volume_mixing_ratio_uncertainty_mmzm);
    path = "/merged_ozone_vmr[], /uncertainty_of_merged_ozone[]";
    description =
        "derived from the relative uncertainty in percent as: uncertainty_of_merged_ozone[] * 0.01 * "
        "merged_ozone_vmr[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* O3_number_density */
    description = "merged monthly zonal mean ozone mole concentration vertical profiles";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_number_density", harp_type_double, 3,
                                                   dimension_type, NULL, description, "mol/cm^3", NULL,
                                                   read_o3_number_density_mmzm);
    path = "/merged_ozone_concentration[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* O3_number_density_uncertainty */
    description = "uncertainty of the merged monthly zonal mean ozone mole concentration vertical profiles";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_number_density_uncertainty",
                                                   harp_type_double, 3, dimension_type, NULL, description, "mol/cm^3",
                                                   NULL, read_o3_number_density_uncertainty_mmzm);
    path = "/merged_ozone_concentration[], /uncertainty_of_merged_ozone[]";
    description =
        "derived from the relative uncertainty in percent as: uncertainty_of_merged_ozone[] * 0.01 * "
        "merged_ozone_concentration[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);
}

static void register_msmm_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[4] =
        { harp_dimension_time, harp_dimension_latitude, harp_dimension_longitude, harp_dimension_vertical };
    harp_dimension_type latitude_dimension_type[1] = { harp_dimension_latitude };
    harp_dimension_type longitude_dimension_type[1] = { harp_dimension_longitude };
    harp_dimension_type vertical_dimension_type[1] = { harp_dimension_vertical };
    const char *description;
    const char *path;

    module = harp_ingestion_register_module("ESACCI_OZONE_L3_LP_MSMM", "Ozone CCI", "ESACCI_OZONE", "L3_LP_MSMM",
                                            "CCI O3 merged semi-monthly zonal mean limb profile on a 10x20 "
                                            "degree grid", ingestion_init_msmm, ingestion_done);

    /* ESACCI_OZONE_L3_LP_MSMM product */
    product_definition = harp_ingestion_register_product(module, "ESACCI_OZONE_L3_LP_MSMM", NULL, read_dimensions);

    /* datetime */
    description = "time of the measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double, 1, dimension_type,
                                                   NULL, description, "seconds since 1990-01-01", NULL, read_datetime);
    path = "/time[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* longitude */
    description = "longitude of the bin center";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_double, 1,
                                                   longitude_dimension_type, NULL, description, "degree_east", NULL,
                                                   read_longitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0, 180.0);
    path = "/longitude_centers[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* latitude */
    description = "latitude of the bin center";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_double, 1,
                                                   latitude_dimension_type, NULL, description, "degree_north", NULL,
                                                   read_latitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    path = "/latitude_centers[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* altitude */
    description =
        "approximate altitude at pressure levels computed as 16 * log10(1013 / pressure), with pressure in hPa";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "altitude", harp_type_double, 1,
                                                   vertical_dimension_type, NULL, description, "km", NULL,
                                                   read_altitude);
    path = "/approximate_altitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* pressure */
    description = "pressure";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "pressure", harp_type_double, 1,
                                                   vertical_dimension_type, NULL, description, "hPa", NULL,
                                                   read_pressure);
    path = "/air_pressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* O3_volume_mixing_ratio */
    description = "merged semi-monthly zonal mean ozone mixing ratio vertical profiles";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_volume_mixing_ratio", harp_type_double, 4,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_o3_volume_mixing_ratio_msmm);
    path = "/merged_ozone_vmr[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* O3_volume_mixing_ratio_uncertainty */
    description = "uncertainty of the merged semi-monthly zonal mean ozone mixing ratio vertical profiles";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_volume_mixing_ratio_uncertainty",
                                                   harp_type_double, 4, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_o3_volume_mixing_ratio_uncertainty_msmm);
    path = "/merged_ozone_vmr[], /uncertainty_of_merged_ozone[]";
    description =
        "derived from the relative uncertainty in percent as: uncertainty_of_merged_ozone[] * 0.01 * "
        "merged_ozone_vmr[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* O3_number_density */
    description = "merged semi-monthly zonal mean ozone mole concentration vertical profiles";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_number_density", harp_type_double, 4,
                                                   dimension_type, NULL, description, "mol/cm^3", NULL,
                                                   read_o3_number_density_msmm);
    path = "/merged_ozone_concentration[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* O3_number_density_uncertainty */
    description = "uncertainty of the merged semi-monthly zonal mean ozone mole concentration vertical profiles";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_number_density_uncertainty",
                                                   harp_type_double, 4, dimension_type, NULL, description, "mol/cm^3",
                                                   NULL, read_o3_number_density_uncertainty_msmm);
    path = "/merged_ozone_concentration[], /uncertainty_of_merged_ozone[]";
    description =
        "derived from the relative uncertainty in percent as: uncertainty_of_merged_ozone[] * 0.01 * "
        "merged_ozone_concentration[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);
}

int harp_ingestion_module_cci_l3_o3_lp_init(void)
{
    register_mzm_product();
    register_mmzm_product();
    register_msmm_product();

    return 0;
}
