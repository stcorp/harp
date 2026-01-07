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
#include "harp-constants.h"
#include "harp-ingestion.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct ingest_info_struct
{
    coda_product *product;
    long num_time;
    long num_vertical;
    long num_latitude;
    long num_longitude;
} ingest_info;

static int read_date(ingest_info *info, const char *path, double *date)
{
    coda_cursor cursor;
    char buffer[17];
    long length;

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
    if (coda_cursor_get_string_length(&cursor, &length) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (length != 16)
    {
        harp_set_error(HARP_ERROR_INGESTION, "datetime value has length %ld; expected 16 (yyyyMMdd'T'HHmmss'Z')",
                       length);
        harp_add_coda_cursor_path_to_error_message(&cursor);
        return -1;
    }
    if (coda_cursor_read_string(&cursor, buffer, 17) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (strncmp(&buffer[9], "000000", 6) != 0)
    {
        harp_set_error(HARP_ERROR_INGESTION, "datetime value '%s' is not a pure date (the time part is non-zero)",
                       buffer);
        harp_add_coda_cursor_path_to_error_message(&cursor);
        return -1;
    }
    if (coda_time_string_to_double("yyyyMMdd'T'HHmmss'Z'", buffer, date) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        harp_add_coda_cursor_path_to_error_message(&cursor);
        return -1;
    }

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
    info->num_time = coda_dim[0];

    if (coda_cursor_goto(&cursor, "/lon") != 0)
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
    info->num_longitude = coda_dim[0];

    if (coda_cursor_goto(&cursor, "/lat") != 0)
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
    info->num_latitude = coda_dim[0];

    if (coda_cursor_goto(&cursor, "/layers") != 0)
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
    info->num_vertical = coda_dim[0];

    return 0;
}

static void ingestion_done(void *user_data)
{
    free(user_data);
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
    info->product = product;

    if (init_dimensions(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }

    *definition = *module->product_definition;
    *user_data = info;

    return 0;
}

static int read_dataset(ingest_info *info, const char *path, harp_data_type data_type, long num_elements,
                        harp_array data)
{
    coda_cursor cursor;
    harp_scalar fill_value;
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

    switch (data_type)
    {
        case harp_type_float:
            if (coda_cursor_read_float_array(&cursor, data.float_data, coda_array_ordering_c) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            if (coda_cursor_goto(&cursor, "@FillValue") == 0)
            {
                if (coda_cursor_read_float(&cursor, &fill_value.float_data) != 0)
                {
                    harp_set_error(HARP_ERROR_CODA, NULL);
                    return -1;
                }

                harp_array_replace_fill_value(data_type, num_elements, data, fill_value);
            }
            break;
        case harp_type_double:
            if (coda_cursor_read_double_array(&cursor, data.double_data, coda_array_ordering_c) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            if (coda_cursor_goto(&cursor, "@FillValue") == 0)
            {
                if (coda_cursor_read_double(&cursor, &fill_value.double_data) != 0)
                {
                    harp_set_error(HARP_ERROR_CODA, NULL);
                    return -1;
                }

                harp_array_replace_fill_value(data_type, num_elements, data, fill_value);
            }
            break;
        default:
            assert(0);
            exit(1);
    }

    return 0;
}

static int read_and_reorder_dataset_4d(ingest_info *info, const char *path, harp_data_type data_type, harp_array data)
{
    int order[4] = { 0, 3, 1, 2 };
    long dimension[4];

    dimension[0] = info->num_time;
    dimension[1] = info->num_vertical;
    dimension[2] = info->num_latitude;
    dimension[3] = info->num_longitude;
    if (read_dataset(info, path, data_type, harp_get_num_elements(4, dimension), data) != 0)
    {
        return -1;
    }

    /* Reorder array dimensions from [num_time, num_vertical, num_latitude, num_longitude] to [num_time, num_latitude,
     * num_longitude, num_vertical].
     */
    if (harp_array_transpose(data_type, 4, dimension, order, data) != 0)
    {
        return -1;
    }

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
    double epoch;
    long i;

    if (read_date(info, "/@time_coverage_start", &epoch) != 0)
    {
        return -1;
    }

    if (read_dataset(info, "/time", harp_type_double, info->num_time, data) != 0)
    {
        return -1;
    }

    for (i = 0; i < info->num_time; i++)
    {
        data.double_data[i] = (data.double_data[i] * CONST_HOUR) + epoch;
    }

    return 0;
}

static int read_longitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "/lon", harp_type_float, info->num_longitude, data);
}

static int read_latitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "/lat", harp_type_float, info->num_latitude, data);
}

static int read_geopotential_height(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_and_reorder_dataset_4d(info, "/Gph", harp_type_float, data);
}

static int read_temperature(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_and_reorder_dataset_4d(info, "/Temperature", harp_type_float, data);
}

static int read_pressure(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    harp_array hybride_coef_a;
    harp_array hybride_coef_b;
    long num_profiles;
    long i;

    num_profiles = info->num_time * info->num_latitude * info->num_longitude;

    /* The air pressure is interpolated from the position dependent surface air pressure (/Psurf[]) using a position
     * independent set of coefficients (/Hybride_coef_fa[], /Hybride_coef_fb[]).
     */
    hybride_coef_a.ptr = malloc(info->num_vertical * sizeof(float));
    if (hybride_coef_a.ptr == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       info->num_vertical * sizeof(float), __FILE__, __LINE__);
        return -1;
    }

    hybride_coef_b.ptr = malloc(info->num_vertical * sizeof(float));
    if (hybride_coef_b.ptr == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       info->num_vertical * sizeof(float), __FILE__, __LINE__);
        free(hybride_coef_a.ptr);
        return -1;
    }

    if (read_dataset(info, "/Hybride_coef_fa", harp_type_float, info->num_vertical, hybride_coef_a) != 0)
    {
        free(hybride_coef_b.ptr);
        free(hybride_coef_a.ptr);
        return -1;
    }

    if (read_dataset(info, "/Hybride_coef_fb", harp_type_float, info->num_vertical, hybride_coef_b) != 0)
    {
        free(hybride_coef_b.ptr);
        free(hybride_coef_a.ptr);
        return -1;
    }

    if (read_dataset(info, "/Psurf", harp_type_float, num_profiles, data) != 0)
    {
        free(hybride_coef_b.ptr);
        free(hybride_coef_a.ptr);
        return -1;
    }

    for (i = num_profiles - 1; i >= 0; i--)
    {
        float *profile = data.float_data + i * info->num_vertical;      /* pressure profile for specific time, lat, lon */
        float surface_pressure = data.float_data[i];    /* surface pressure at specific time, lat, lon */
        long j;

        for (j = info->num_vertical - 1; j >= 0; j--)
        {
            profile[j] = hybride_coef_a.float_data[j] + hybride_coef_b.float_data[j] * surface_pressure;
        }
    }

    free(hybride_coef_b.ptr);
    free(hybride_coef_a.ptr);

    return 0;
}

static int read_pressure_bounds(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    harp_array hybride_coef_a;
    harp_array hybride_coef_b;
    long num_profiles;
    long num_levels;
    long i;

    num_profiles = info->num_time * info->num_latitude * info->num_longitude;
    num_levels = info->num_vertical + 1;

    /* The air pressure boundaries are interpolated from the position dependent surface air pressure (/Psurf[]) using a
     * position independent set of coefficients (/Hybride_coef_a[], /Hybride_coef_b[]).
     */
    hybride_coef_a.ptr = malloc(num_levels * sizeof(float));
    if (hybride_coef_a.ptr == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_levels * sizeof(float), __FILE__, __LINE__);
        return -1;
    }

    hybride_coef_b.ptr = malloc(num_levels * sizeof(float));
    if (hybride_coef_b.ptr == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_levels * sizeof(float), __FILE__, __LINE__);
        free(hybride_coef_a.ptr);
        return -1;
    }

    if (read_dataset(info, "/Hybride_coef_a", harp_type_float, num_levels, hybride_coef_a) != 0)
    {
        free(hybride_coef_b.ptr);
        free(hybride_coef_a.ptr);
        return -1;
    }

    if (read_dataset(info, "/Hybride_coef_b", harp_type_float, num_levels, hybride_coef_b) != 0)
    {
        free(hybride_coef_b.ptr);
        free(hybride_coef_a.ptr);
        return -1;
    }

    if (read_dataset(info, "/Psurf", harp_type_float, num_profiles, data) != 0)
    {
        free(hybride_coef_b.ptr);
        free(hybride_coef_a.ptr);
        return -1;
    }

    for (i = num_profiles - 1; i >= 0; i--)
    {
        float *bounds = data.float_data + i * info->num_vertical * 2;   /* bounds for specific time, lat, lon */
        float surface_pressure = data.float_data[i];    /* surface pressure at specific time, lat, lon */
        long j;

        bounds[(info->num_vertical - 1) * 2 + 1] =
            hybride_coef_a.float_data[info->num_vertical] +
            hybride_coef_b.float_data[info->num_vertical] * surface_pressure;

        for (j = info->num_vertical - 1; j > 0; j--)
        {
            bounds[j * 2] = bounds[(j - 1) * 2 + 1] =
                hybride_coef_a.float_data[j] + hybride_coef_b.float_data[j] * surface_pressure;
        }

        bounds[0] = hybride_coef_a.float_data[0] + hybride_coef_b.float_data[0] * surface_pressure;
    }

    free(hybride_coef_b.ptr);
    free(hybride_coef_a.ptr);

    return 0;
}

static int read_O3_column_number_density(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_and_reorder_dataset_4d(info, "/O3_dens", harp_type_float, data);
}

static int read_O3_column_number_density_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_and_reorder_dataset_4d(info, "/O3s_dens", harp_type_float, data);
}

static int read_O3_volume_mixing_ratio(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_and_reorder_dataset_4d(info, "/O3_vmr", harp_type_float, data);
}

static int read_O3_volume_mixing_ratio_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_and_reorder_dataset_4d(info, "/O3s_vmr", harp_type_float, data);
}

int harp_ingestion_module_cci_l4_o3_np_init(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type datetime_dimension_type[1] = { harp_dimension_time };
    harp_dimension_type longitude_dimension_type[1] = { harp_dimension_longitude };
    harp_dimension_type latitude_dimension_type[1] = { harp_dimension_latitude };
    harp_dimension_type dimension_type[5] = { harp_dimension_time, harp_dimension_latitude, harp_dimension_longitude,
        harp_dimension_vertical, harp_dimension_independent
    };
    long pressure_bounds_dimension[5] = { -1, -1, -1, -1, 2 };
    const char *description;
    const char *path;

    module = harp_ingestion_register_module("ESACCI_OZONE_L4_NP", "Ozone CCI", "ESACCI_OZONE", "L4_NP",
                                            "CCI L4 O3 nadir profile", ingestion_init, ingestion_done);

    /* ESACCI_OZONE_L4_NP product */
    product_definition = harp_ingestion_register_product(module, "ESACCI_OZONE_L4_NP", NULL, read_dimensions);

    /* datetime */
    description = "time of the measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double, 1,
                                                   datetime_dimension_type, NULL, description,
                                                   "seconds since 2000-01-01", NULL, read_datetime);
    path = "/@time_coverage_start, /time[]";
    description = "datetime converted from time in hours (time[]) since the start of the product "
        "(@time_coverage_start) to seconds since 2000-01-01";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* longitude */
    description = "longitude of the grid cell center";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_float, 1,
                                                   longitude_dimension_type, NULL, description, "degree_east", NULL,
                                                   read_longitude);
    path = "/lon[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* latitude */
    description = "latitude of the grid cell center";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_float, 1,
                                                   latitude_dimension_type, NULL, description, "degree_north", NULL,
                                                   read_latitude);
    harp_variable_definition_set_valid_range_float(variable_definition, -90.0f, 90.0f);
    path = "/lat[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* geopotential_height */
    description = "geopotential height";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "geopotential_height", harp_type_float, 4,
                                                   dimension_type, NULL, description, "m", NULL,
                                                   read_geopotential_height);
    path = "/Gph[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* temperature */
    description = "temperature";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "temperature", harp_type_float, 4,
                                                   dimension_type, NULL, description, "K", NULL, read_temperature);
    path = "/Temperature[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* pressure */
    description = "air pressure profile";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "pressure", harp_type_float, 4, dimension_type,
                                                   NULL, description, "Pa", NULL, read_pressure);
    description = "pressure at the center of layer k is derived from surface air pressure as: Hybride_coef_fa[k] + "
        "Hybride_coef_fb[k] * Psurf[]";
    path = "/Psurf[], /Hybride_coef_fa[], /Hybride_coef_fb[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* pressure_bounds */
    description = "air pressure boundaries for each profile layer";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "pressure_bounds", harp_type_float, 5,
                                                   dimension_type, pressure_bounds_dimension, description, "Pa", NULL,
                                                   read_pressure_bounds);
    description = "pressure at level m is derived from surface air pressure as: Hybride_coef_a[m] + Hybride_coef_b[m] "
        "* Psurf[]";
    path = "/Psurf[], /Hybride_coef_a[], /Hybride_coef_b[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* O3_column_number_density */
    description = "O3 column number density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_column_number_density", harp_type_float, 4,
                                                   dimension_type, NULL, description, "molec/m^2", NULL,
                                                   read_O3_column_number_density);
    path = "/O3_dens[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* O3_column_number_density_uncertainty */
    description = "uncertainty of the O3 column number density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_column_number_density_uncertainty",
                                                   harp_type_float, 4, dimension_type, NULL, description, "molec/m^2",
                                                   NULL, read_O3_column_number_density_uncertainty);
    path = "/O3s_dens[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);


    /* O3_volume_mixing_ratio */
    description = "O3 volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_volume_mixing_ratio", harp_type_float, 4,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_O3_volume_mixing_ratio);
    path = "/O3_vmr[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* O3_volume_mixing_ratio_uncertainty */
    description = "uncertainty of the O3 volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_volume_mixing_ratio_uncertainty",
                                                   harp_type_float, 4, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_O3_volume_mixing_ratio_uncertainty);
    path = "/O3s_vmr[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    return 0;
}
