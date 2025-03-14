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
#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct ingest_info_struct
{
    coda_product *product;
    long num_vertical;
    long num_latitude;
    long num_longitude;
} ingest_info;

static int read_datetime(ingest_info *info, const char *path, double *datetime)
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
    if (coda_time_string_to_double("yyyyMMdd'T'HHmmss'Z'", buffer, datetime) != 0)
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

    if (coda_cursor_goto(&cursor, "/air_pressure") != 0)
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

static int read_dataset(ingest_info *info, const char *path, long num_elements, harp_array data)
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
    if (coda_cursor_read_float_array(&cursor, data.float_data, coda_array_ordering_c) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, "@FillValue[0]") == 0)
    {
        if (coda_cursor_read_float(&cursor, &fill_value.float_data) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }

        harp_array_replace_fill_value(harp_type_float, num_elements, data, fill_value);
    }

    return 0;
}

static int read_and_reorder_dataset_3d(ingest_info *info, const char *path, harp_array data)
{
    int order[3] = { 2, 0, 1 };
    long dimension[3];

    dimension[0] = info->num_vertical;
    dimension[1] = info->num_latitude;
    dimension[2] = info->num_longitude;
    if (read_dataset(info, path, harp_get_num_elements(3, dimension), data) != 0)
    {
        return -1;
    }

    /* Reorder array dimensions from [num_vertical, num_latitude, num_longitude] to [num_latitude, num_longitude,
     * num_vertical].
     */
    if (harp_array_transpose(harp_type_float, 3, dimension, order, data) != 0)
    {
        return -1;
    }

    return 0;
}

static int read_dimensions(void *user_data, long dimension[HARP_NUM_DIM_TYPES])
{
    ingest_info *info = (ingest_info *)user_data;

    dimension[harp_dimension_time] = 1;
    dimension[harp_dimension_latitude] = info->num_latitude;
    dimension[harp_dimension_longitude] = info->num_longitude;
    dimension[harp_dimension_vertical] = info->num_vertical;

    return 0;
}

static int read_datetime_start(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_datetime(info, "/@time_coverage_start", &data.double_data[0]);
}

static int read_datetime_stop(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_datetime(info, "/@time_coverage_end", &data.double_data[0]);
}

static int read_longitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "/lon", info->num_longitude, data);
}

static int read_latitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "/lat", info->num_latitude, data);
}

static int read_pressure(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    harp_array air_pressure;
    long num_profiles;
    long i;

    air_pressure.ptr = malloc(info->num_vertical * sizeof(float));
    if (air_pressure.ptr == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       info->num_vertical * sizeof(float), __FILE__, __LINE__);
        return -1;
    }

    /* The air pressure profile used is independent from the position on Earth, except for the lowest level (surface
     * pressure). Therefore, the air pressure profile is repeated for each position while replacing the pressure at the
     * lowest level with the position dependent surface pressure.
     */
    if (read_dataset(info, "/air_pressure", info->num_vertical, air_pressure) != 0)
    {
        free(air_pressure.ptr);
        return -1;
    }

    num_profiles = info->num_latitude * info->num_longitude;
    if (read_dataset(info, "/surface_pressure", num_profiles, data) != 0)
    {
        free(air_pressure.ptr);
        return -1;
    }

    for (i = num_profiles - 1; i >= 0; i--)
    {
        data.float_data[i * info->num_vertical] = data.float_data[i];
        memcpy(&data.float_data[i * info->num_vertical + 1], &air_pressure.float_data[1],
               (info->num_vertical - 1) * sizeof(float));
    }

    free(air_pressure.ptr);
    return 0;
}

static int read_pressure_bounds(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long num_profiles;
    long i;

    if (read_pressure(user_data, data) != 0)
    {
        return -1;
    }

    num_profiles = info->num_latitude * info->num_longitude;
    for (i = num_profiles - 1; i >= 0; i--)
    {
        float *levels = data.float_data + i * info->num_vertical;
        float *bounds = data.float_data + i * info->num_vertical * 2;
        long j;

        bounds[(info->num_vertical - 1) * 2 + 1] = levels[info->num_vertical - 1];

        for (j = info->num_vertical - 1; j > 0; j--)
        {
            /* Log-linear interpolation. */
            bounds[j * 2] = bounds[(j - 1) * 2 + 1] = (float)sqrt(levels[j] * levels[j - 1]);
        }

        bounds[0] = levels[0];
    }

    return 0;
}

static int read_O3_number_density(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_and_reorder_dataset_3d(info, "/O3_ndens", data);
}

static int read_O3_number_density_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_and_reorder_dataset_3d(info, "/O3e_ndens", data);
}

static int read_O3_volume_mixing_ratio(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_and_reorder_dataset_3d(info, "/O3_vmr", data);
}

static int read_O3_volume_mixing_ratio_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_and_reorder_dataset_3d(info, "/O3e_vmr", data);
}

int harp_ingestion_module_cci_l3_o3_np_init(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type datetime_dimension_type[1] = { harp_dimension_time };
    harp_dimension_type longitude_dimension_type[1] = { harp_dimension_longitude };
    harp_dimension_type latitude_dimension_type[1] = { harp_dimension_latitude };
    harp_dimension_type dimension_type[4] = { harp_dimension_latitude, harp_dimension_longitude,
        harp_dimension_vertical, harp_dimension_independent
    };
    long pressure_bounds_dimension[4] = { -1, -1, -1, 2 };
    const char *description;
    const char *path;

    module = harp_ingestion_register_module("ESACCI_OZONE_L3_NP", "Ozone CCI", "ESACCI_OZONE", "L3_NP",
                                            "CCI L3 O3 nadir profile products", ingestion_init, ingestion_done);

    /* ESACCI_OZONE_L3_NP product */
    product_definition = harp_ingestion_register_product(module, "ESACCI_OZONE_L3_NP", NULL, read_dimensions);

    /* datetime_start */
    description = "time coverage start";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime_start", harp_type_double, 1,
                                                   datetime_dimension_type, NULL, description,
                                                   "seconds since 2000-01-01", NULL, read_datetime_start);
    path = "/@time_coverage_start";
    description = "datetime converted from a start date to seconds since 2000-01-01";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* datetime_start */
    description = "time coverage end";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime_stop", harp_type_double, 1,
                                                   datetime_dimension_type, NULL, description,
                                                   "seconds since 2000-01-01", NULL, read_datetime_stop);
    path = "/@time_coverage_end";
    description = "datetime converted from an end date to seconds since 2000-01-01";
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

    /* pressure */
    description = "air pressure profile";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "pressure", harp_type_float, 3, dimension_type,
                                                   NULL, description, "hPa", NULL, read_pressure);
    description = "repeat location independent air pressure profile (/air_pressure[]) for each grid point and "
        "replace value at the lowest level by location dependent surface pressure (/surface_pressure[])";
    path = "/surface_pressure[], /air_pressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* pressure_bounds */
    description = "air pressure boundaries for each profile level";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "pressure_bounds", harp_type_float, 4,
                                                   dimension_type, pressure_bounds_dimension, description, "hPa", NULL,
                                                   read_pressure_bounds);
    description = "repeat location independent air pressure profile (/air_pressure[]) for each grid point and "
        "replace value at the lowest level by location dependent surface pressure (/surface_pressure[]); "
        "upper and lower pressure for each profile is based on log-linear interpolation of profile (i.e. upper bound "
        "is sqrt(pressure[i+1] * pressure[i]), and lower bound is sqrt(pressure[i] * pressure[i-1])); the lower bound "
        "of level 0 is set to pressure[0] and the upper bound for level n-1 is set to pressure[n-1]";
    path = "/surface_pressure[], /air_pressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* O3_number_density */
    description = "O3 number density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_number_density", harp_type_float, 3,
                                                   dimension_type, NULL, description, "molec/cm^3", NULL,
                                                   read_O3_number_density);
    path = "/O3_ndens[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* O3_number_density_uncertainty */
    description = "uncertainty of the O3 number density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_number_density_uncertainty", harp_type_float,
                                                   3, dimension_type, NULL, description, "molec/cm^3", NULL,
                                                   read_O3_number_density_uncertainty);
    path = "/O3e_ndens[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* O3_volume_mixing_ratio */
    description = "O3 volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_volume_mixing_ratio", harp_type_float, 3,
                                                   dimension_type, NULL, description, "ppmv", NULL,
                                                   read_O3_volume_mixing_ratio);
    path = "/O3_vmr[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* O3_volume_mixing_ratio_uncertainty */
    description = "uncertainty of the O3 volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_volume_mixing_ratio_uncertainty",
                                                   harp_type_float, 3, dimension_type, NULL, description, "ppmv", NULL,
                                                   read_O3_volume_mixing_ratio_uncertainty);
    path = "/O3e_vmr[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    return 0;
}
