/*
 * Copyright (C) 2015-2021 S[&]T, The Netherlands.
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
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ---------- typedefs ---------- */

typedef struct ingest_info_struct
{
    coda_product *product;
    long num_times;
} ingest_info;

/* ---------- defines ---------- */

#define CHECKED_MALLOC(v, s) v = malloc(s); if (v == NULL) { harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)", s, __FILE__, __LINE__); return -1;}

#define MOL_PER_M2_TO_DU             2241.15

/* ---------- global variables ---------- */

static double coda_nan;

/* ---------- code ---------- */

static void ingestion_done(void *user_data)
{
    free(user_data);
}

static int read_dataset(ingest_info *info, const char *path, long num_elements, double factor, harp_array data)
{
    coda_cursor cursor;
    double *double_data;
    long coda_num_elements, i;

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
    if (factor != 1.0)
    {
        double_data = data.double_data;
        for (i = 0; i < num_elements; i++)
        {
            *double_data = *double_data * factor;
            double_data++;
        }
    }

    return 0;
}

static int read_dimensions(void *user_data, long dimension[HARP_NUM_DIM_TYPES])
{
    ingest_info *info = (ingest_info *)user_data;

    dimension[harp_dimension_time] = info->num_times;

    return 0;
}

static int read_datetime(void *user_data, harp_array data)
{
    coda_cursor cursor;
    ingest_info *info = (ingest_info *)user_data;
    double *double_data;
    char time_string[81];
    long i;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, "/string_time") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_first_array_element(&cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    double_data = data.double_data;
    for (i = 0; i < info->num_times; i++)
    {
        if (coda_cursor_read_string(&cursor, time_string, sizeof(time_string)) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        time_string[16] = '\0';
        if (coda_time_string_to_double("yyyyMMdd'T'HHmmss'Z'", time_string, double_data) != 0)
        {
            *double_data = coda_nan;
        }
        double_data++;
        if (coda_cursor_goto_next_array_element(&cursor) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
    }
    return 0;
}

static int read_longitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "/longitude", info->num_times, 1.0, data);
}

static int read_latitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "/latitude", info->num_times, 1.0, data);
}

static int read_tropopause_altitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "/tropopause_altitude", info->num_times, 1.0, data);
}

static int read_ozone_column_number_density(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "/total_ozone_column", info->num_times, MOL_PER_M2_TO_DU, data);
}

static int read_ozone_column_number_density_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "/total_ozone_column_standard_error", info->num_times, MOL_PER_M2_TO_DU, data);
}

static int read_stratospheric_ozone_column_number_density(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "/stratospheric_ozone_column", info->num_times, MOL_PER_M2_TO_DU, data);
}

static int read_stratospheric_ozone_column_number_density_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "/stratospheric_ozone_column_standard_error", info->num_times, MOL_PER_M2_TO_DU, data);
}

static int read_tropospheric_ozone_column_number_density(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "/tropospheric_ozone_column", info->num_times, MOL_PER_M2_TO_DU, data);
}

static int read_tropospheric_ozone_column_number_density_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "/tropospheric_ozone_column_standard_error", info->num_times, MOL_PER_M2_TO_DU, data);
}

static int read_cloud_top_height(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "/cloud_height", info->num_times, 1.0, data);
}

static int read_solar_zenith_angle(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "/sza_tanpnt", info->num_times, 1.0, data);
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

    if (coda_cursor_goto(&cursor, "/time") != 0)
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
        harp_set_error(HARP_ERROR_INGESTION, "dataset has %d dimensions; expected 1", num_coda_dims);
        harp_add_coda_cursor_path_to_error_message(&cursor);
        return -1;
    }
    info->num_times = coda_dim[0];

    return 0;
}

static int ingestion_init(const harp_ingestion_module *module, coda_product *product,
                          const harp_ingestion_options *options, harp_product_definition **definition, void **user_data)
{
    ingest_info *info;

    (void)options;

    CHECKED_MALLOC(info, sizeof(ingest_info));
    info->product = product;

    if (init_dimensions(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }

    *definition = *module->product_definition;
    *user_data = info;

    coda_nan = coda_NaN();

    return 0;
}

int harp_ingestion_module_cci_l3_o3_lntoc_init(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };
    const char *description;
    const char *path;

    module = harp_ingestion_register_module("ESACCI_OZONE_L3_LNTOC", "Ozone CCI", "ESACCI_OZONE", "L3_LNTOC",
                                            "CCI L3 O3 limb nadir tropospheric columns", ingestion_init,
                                            ingestion_done);

    /* ESACCI_OZONE_L3_LNTOC product */
    product_definition = harp_ingestion_register_product(module, "ESACCI_OZONE_L3_LNTOC", NULL, read_dimensions);

    /* datetime_start */
    description = "datetime";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double, 1,
                                                   dimension_type, NULL, description,
                                                   "seconds since 2000-01-01", NULL, read_datetime);
    path = "/string_time";
    description = "datetime converted from a string to seconds since 2000-01-01";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* latitude */
    description = "latitude";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_double, 1,
                                                   dimension_type, NULL, description, "degree_north", NULL,
                                                   read_latitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    path = "/latitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* longitude */
    description = "longitude";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_double, 1,
                                                   dimension_type, NULL, description, "degree_east", NULL,
                                                   read_longitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0, 180.0);
    path = "/longitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* tropopause_altitude */
    description =
        "geometric tropopause altitude measured from sea-level by using the WMO+Potential Vorticity definition for high latitudes > 30 degrees";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "tropopause_altitude", harp_type_double, 1,
                                                   dimension_type, NULL, description, "km", NULL,
                                                   read_tropopause_altitude);
    path = "/tropopause_altitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* O3_column_number_density */
    description = "total ozone column number density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_column_number_density", harp_type_double, 1,
                                                   dimension_type, NULL, description, "DU", NULL,
                                                   read_ozone_column_number_density);
    path = "/total_ozone_column[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* O3_column_number_density_uncertainty */
    description = "uncertainty of the total ozone column number density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_column_number_density_uncertainty",
                                                   harp_type_double, 1, dimension_type, NULL, description, "DU", NULL,
                                                   read_ozone_column_number_density_uncertainty);
    path = "/total_ozone_column_standard_error[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* stratospheric_O3_column_number_density */
    description = "stratospheric ozone column number density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "stratospheric_O3_column_number_density",
                                                   harp_type_double, 1, dimension_type, NULL, description, "DU", NULL,
                                                   read_stratospheric_ozone_column_number_density);
    path = "/stratospheric_ozone_column[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* stratospheric_O3_column_number_density_uncertainty */
    description = "uncertainty of the stratospheric ozone column number density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "stratospheric_O3_column_number_density_uncertainty",
                                                   harp_type_double, 1, dimension_type, NULL, description, "DU", NULL,
                                                   read_stratospheric_ozone_column_number_density_uncertainty);
    path = "/stratospheric_ozone_column_standard_error[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* tropospheric_O3_column_number_density */
    description = "tropospheric ozone column number density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "tropospheric_O3_column_number_density",
                                                   harp_type_double, 1, dimension_type, NULL, description, "DU", NULL,
                                                   read_tropospheric_ozone_column_number_density);
    path = "/tropospheric_ozone_column[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* tropospheric_O3_column_number_density_uncertainty */
    description = "uncertainty of the tropospheric ozone column number density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "tropospheric_O3_column_number_density_uncertainty",
                                                   harp_type_double, 1, dimension_type, NULL, description, "DU", NULL,
                                                   read_tropospheric_ozone_column_number_density_uncertainty);
    path = "/tropospheric_ozone_column_standard_error[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_top_height */
    description = "the altitude of the cloud top";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_top_height", harp_type_double, 1,
                                                   dimension_type, NULL, description, "km", NULL,
                                                   read_cloud_top_height);
    path = "/cloud_height[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* solar_zenith_angle */
    description = "solar zenith angle at the tangent point";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "solar_zenith_angle", harp_type_double, 1,
                                                   dimension_type, NULL, description, "degree", NULL,
                                                   read_solar_zenith_angle);
    path = "/sza_tanpnt[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    return 0;
}
