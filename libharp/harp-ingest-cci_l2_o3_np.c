/*
 * Copyright (C) 2015 S[&]T, The Netherlands.
 *
 * This file is part of HARP.
 *
 * HARP is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * HARP is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with HARP; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "coda.h"
#include "harp-constants.h"
#include "harp-ingestion.h"

#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct ingest_info_struct
{
    coda_product *product;
    long num_time;
    long num_levels;
    coda_cursor geo_bounds_cursor;
    float geo_bounds_fill_value;
} ingest_info;

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
    if (coda_cursor_goto(&cursor, "/o3_nd") != 0)
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
        harp_set_error(HARP_ERROR_PRODUCT, "dataset has %d dimensions, expected 2", num_coda_dims);
        harp_add_coda_cursor_path_to_error_message(&cursor);
        return -1;
    }
    info->num_time = coda_dim[0];
    info->num_levels = coda_dim[1];

    return 0;
}

static int init_geo_bounds(ingest_info *info)
{
    coda_cursor cursor;
    long coda_num_elements;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, "/ll") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    info->geo_bounds_cursor = cursor;

    if (coda_cursor_get_num_elements(&cursor, &coda_num_elements) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_num_elements != info->num_time * 8)
    {
        harp_set_error(HARP_ERROR_PRODUCT, "dataset has %ld elements (expected %ld)", coda_num_elements,
                       info->num_time * 8);
        harp_add_coda_cursor_path_to_error_message(&cursor);
        harp_add_error_message(" (%s:%lu)", __FILE__, __LINE__);
        return -1;
    }
    if (coda_cursor_goto(&cursor, "@FillValue") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_float(&cursor, &info->geo_bounds_fill_value) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

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

    if (init_geo_bounds(info) != 0)
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
        harp_set_error(HARP_ERROR_PRODUCT, "dataset has %ld elements (expected %ld)", coda_num_elements, num_elements);
        harp_add_coda_cursor_path_to_error_message(&cursor);
        harp_add_error_message(" (%s:%lu)", __FILE__, __LINE__);
        return -1;
    }
    switch (data_type)
    {
        case harp_type_int16:
            {
                if (coda_cursor_read_int16_array(&cursor, data.int16_data, coda_array_ordering_c) != 0)
                {
                    harp_set_error(HARP_ERROR_CODA, NULL);
                    return -1;
                }
                if (coda_cursor_goto(&cursor, "@FillValue") != 0)
                {
                    harp_set_error(HARP_ERROR_CODA, NULL);
                    return -1;
                }
                if (coda_cursor_read_int16(&cursor, &fill_value.int16_data) != 0)
                {
                    harp_set_error(HARP_ERROR_CODA, NULL);
                    return -1;
                }
                harp_array_replace_fill_value(data_type, num_elements, data, fill_value);
            }
            break;
        case harp_type_float:
            {
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
            }
            break;
        case harp_type_double:
            {
                if (coda_cursor_read_double_array(&cursor, data.double_data, coda_array_ordering_c) != 0)
                {
                    harp_set_error(HARP_ERROR_CODA, NULL);
                    return -1;
                }
                if (coda_cursor_goto(&cursor, "@FillValue") != 0)
                {
                    harp_set_error(HARP_ERROR_CODA, NULL);
                    return -1;
                }
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

static int read_relerr_as_stdev_float(ingest_info *info, const char *path_quantity, const char *path_relerr,
                                      long num_elements, harp_array data)
{
    harp_array relerr;
    long i;

    if (read_dataset(info, path_quantity, harp_type_float, num_elements, data) != 0)
    {
        return -1;
    }

    relerr.ptr = malloc(num_elements * sizeof(float));
    if (relerr.ptr == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_elements * sizeof(float), __FILE__, __LINE__);
        return -1;
    }

    if (read_dataset(info, path_relerr, harp_type_float, num_elements, relerr) != 0)
    {
        free(relerr.ptr);
        return -1;
    }

    /* Convert relative error (in percent) to standard deviation (same unit as the associated quantity). */
    for (i = 0; i < num_elements; i++)
    {
        data.float_data[i] *= relerr.float_data[i] * 0.01f;     /* relative error is a percentage */
    }

    free(relerr.ptr);

    return 0;
}

static int read_dimensions(void *user_data, long dimension[HARP_NUM_DIM_TYPES])
{
    ingest_info *info = (ingest_info *)user_data;

    dimension[harp_dimension_time] = info->num_time;
    dimension[harp_dimension_vertical] = info->num_levels;

    return 0;
}

static int read_datetime(void *user_data, harp_array data)
{
    ingest_info *info;
    coda_cursor cursor;
    long string_length;
    char date[11];
    double epoch;
    long i;

    info = (ingest_info *)user_data;
    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, "/@Data_date") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_string_length(&cursor, &string_length) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (string_length < 10)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_string(&cursor, date, 11) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_time_string_to_double_utc("yyyy-MM-dd", date, &epoch) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    /* Convert epoch to hours since 2000/01/01 TAI. */
    epoch /= CONST_HOUR;

    if (read_dataset(info, "/time", harp_type_double, info->num_time, data) != 0)
    {
        return -1;
    }
    for (i = 0; i < info->num_time; i++)
    {
        data.double_data[i] += epoch;
    }

    return 0;
}

static int read_scanline_pixel_index(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "/scp", harp_type_int16, info->num_time, data);
}

static int read_longitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "/lon", harp_type_float, info->num_time, data);
}

static int read_latitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "/lat", harp_type_float, info->num_time, data);
}

static int read_longitude_bounds(void *user_data, long index, harp_array data)
{
    ingest_info *info;
    int i;

    info = (ingest_info *)user_data;
    if (coda_cursor_goto_array_element_by_index(&info->geo_bounds_cursor, index * 8 + 1) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_float(&info->geo_bounds_cursor, &data.float_data[0]) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_next_array_element(&info->geo_bounds_cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_next_array_element(&info->geo_bounds_cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_float(&info->geo_bounds_cursor, &data.float_data[1]) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_next_array_element(&info->geo_bounds_cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_next_array_element(&info->geo_bounds_cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_float(&info->geo_bounds_cursor, &data.float_data[3]) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_next_array_element(&info->geo_bounds_cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_next_array_element(&info->geo_bounds_cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_float(&info->geo_bounds_cursor, &data.float_data[2]) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    for (i = 0; i < 4; i++)
    {
        if (data.float_data[i] == info->geo_bounds_fill_value)
        {
            data.float_data[i] = coda_NaN();
        }
    }
    coda_cursor_goto_parent(&info->geo_bounds_cursor);

    return 0;
}

static int read_latitude_bounds(void *user_data, long index, harp_array data)
{
    ingest_info *info;
    int i;

    info = (ingest_info *)user_data;
    if (coda_cursor_goto_array_element_by_index(&info->geo_bounds_cursor, index * 8) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_float(&info->geo_bounds_cursor, &data.float_data[0]) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_next_array_element(&info->geo_bounds_cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_next_array_element(&info->geo_bounds_cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_float(&info->geo_bounds_cursor, &data.float_data[1]) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_next_array_element(&info->geo_bounds_cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_next_array_element(&info->geo_bounds_cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_float(&info->geo_bounds_cursor, &data.float_data[3]) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_next_array_element(&info->geo_bounds_cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_next_array_element(&info->geo_bounds_cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_float(&info->geo_bounds_cursor, &data.float_data[2]) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    for (i = 0; i < 4; i++)
    {
        if (data.float_data[i] == info->geo_bounds_fill_value)
        {
            data.float_data[i] = coda_NaN();
        }
    }
    coda_cursor_goto_parent(&info->geo_bounds_cursor);

    return 0;
}

static int read_solar_zenith_angle(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "/sza", harp_type_float, info->num_time, data);
}

static int read_viewing_zenith_angle(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "/lza", harp_type_float, info->num_time, data);
}

static int read_surface_pressure(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "/spres", harp_type_float, info->num_time, data);
}

static int read_pressure(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "/levs", harp_type_float, info->num_levels, data);
}

static int read_o3_number_density(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "/o3_nd", harp_type_float, info->num_time * info->num_levels, data);
}

static int read_o3_number_density_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_relerr_as_stdev_float(info, "/o3_nd", "/o3_error", info->num_time * info->num_levels, data);
}

static int read_o3_number_density_cov(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "/sx", harp_type_float, info->num_time * info->num_levels * info->num_levels, data);
}

static int read_o3_number_density_cov_random(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "/sn", harp_type_float, info->num_time * info->num_levels * info->num_levels, data);
}

static int read_o3_volume_mixing_ratio(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "/o3_vmr", harp_type_float, info->num_time * info->num_levels, data);
}

static int read_o3_volume_mixing_ratio_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_relerr_as_stdev_float(info, "/o3_vmr", "/o3_error", info->num_time * info->num_levels, data);
}

static int read_o3_volume_mixing_ratio_apriori(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "/o3_ap", harp_type_float, info->num_time * info->num_levels, data);
}

static int read_o3_volume_mixing_ratio_apriori_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_relerr_as_stdev_float(info, "/o3_ap", "/o3_ap_error", info->num_time * info->num_levels, data);
}

static int read_cloud_fraction(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "/cloudf", harp_type_double, info->num_time, data);
}

static int read_cloud_top_pressure(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "/cloudp", harp_type_double, info->num_time, data);
}

static int read_cloud_top_albedo(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "/clouda", harp_type_double, info->num_time, data);
}

static int read_surface_albedo(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "/salb", harp_type_float, info->num_time, data);
}

static int read_o3_number_density_avk(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "/ak", harp_type_float, info->num_time * info->num_levels * info->num_levels, data);
}

static int verify_product_type(const harp_ingestion_module *module, coda_product *product)
{
    coda_cursor cursor;

    (void)module;
    if (coda_cursor_set_product(&cursor, product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, "/o3_nd") != 0)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, NULL);
        return -1;
    }

    return 0;
}

int harp_ingestion_module_cci_l2_o3_np_init(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[3] = { harp_dimension_time, harp_dimension_vertical, harp_dimension_vertical };
    harp_dimension_type bounds_dimension_type[2] = { harp_dimension_time, harp_dimension_independent };
    harp_dimension_type pressure_dimension_type[1] = { harp_dimension_vertical };
    long bounds_dimension[2] = { -1, 4 };
    const char *description;
    const char *path;

    module = harp_ingestion_register_module_coda("ESACCI_OZONE_L2_NP", NULL, NULL, "CCI (climate change initiative) "
                                                 "L2 O3 nadir profile products", verify_product_type, ingestion_init,
                                                 ingestion_done);

    /* ESACCI_OZONE_L2_NP product */
    product_definition = harp_ingestion_register_product(module, "ESACCI_OZONE_L2_NP", NULL, read_dimensions);

    /* scanline_pixel_index */
    description = "zero-based index of the instantaneous field of view within the swath";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "scanline_pixel_index", harp_type_int16, 1,
                                                   dimension_type, NULL, description, NULL, NULL,
                                                   read_scanline_pixel_index);
    path = "/scp[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* datetime */
    description = "time of the measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double, 1, dimension_type,
                                                   NULL, description, "hours since 2000-01-01", NULL, read_datetime);
    path = "/@Data_date, /time[]";
    description =
        "datetime converted from the UTC epoch of the product and an offset in hours to hours since 2000-01-01";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* longitude */
    description = "longitude of the ground pixel center";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_float, 1, dimension_type,
                                                   NULL, description, "degree_east", NULL, read_longitude);
    harp_variable_definition_set_valid_range_float(variable_definition, -180.0f, 180.0f);
    path = "/lon[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* latitude */
    description = "latitude of the ground pixel center";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_float, 1, dimension_type,
                                                   NULL, description, "degree_north", NULL, read_latitude);
    harp_variable_definition_set_valid_range_float(variable_definition, -90.0f, 90.0f);
    path = "/lat[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* longitude_bounds */
    description = "longitudes of the ground pixel corners";
    variable_definition =
        harp_ingestion_register_variable_sample_read(product_definition, "longitude_bounds", harp_type_float, 2,
                                                     bounds_dimension_type, bounds_dimension, description,
                                                     "degree_east", NULL, read_longitude_bounds);
    harp_variable_definition_set_valid_range_float(variable_definition, -180.0f, 180.0f);
    path = "/ll[]";
    description = "longitudes and latitudes of the ground pixel corners are stored interleaved; longitudes are "
        "ingested as [ll[,1], ll[,3], ll[,7], ll[,5]]; note the reordering of the last two values to ensure "
        "a simple polygon";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* latitude_bounds */
    description = "latitudes of the ground pixel corners";
    variable_definition =
        harp_ingestion_register_variable_sample_read(product_definition, "latitude_bounds", harp_type_float, 2,
                                                     bounds_dimension_type, bounds_dimension, description,
                                                     "degree_north", NULL, read_latitude_bounds);
    harp_variable_definition_set_valid_range_float(variable_definition, -90.0f, 90.0f);
    path = "/ll[]";
    description = "longitudes and latitudes of the ground pixel corners are stored interleaved; latitudes are ingested "
        "as [ll[,0], ll[,2], ll[,6], ll[,4]]; note the reordering of the last two values to ensure a simple polygon";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* solar_zenith_angle */
    description = "zenith angle of the Sun at the ground pixel center";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "solar_zenith_angle", harp_type_float, 1,
                                                   dimension_type, NULL, description, "degree", NULL,
                                                   read_solar_zenith_angle);
    harp_variable_definition_set_valid_range_float(variable_definition, 0.0, 180.0);
    path = "/sza[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* viewing_zenith_angle */
    description = "zenith angle of the instrument at the ground pixel center";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "viewing_zenith_angle", harp_type_float, 1,
                                                   dimension_type, NULL, description, "degree", NULL,
                                                   read_viewing_zenith_angle);
    harp_variable_definition_set_valid_range_float(variable_definition, 0.0, 180.0);
    path = "/lza[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* pressure */
    description = "pressure";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "pressure", harp_type_float, 1,
                                                   pressure_dimension_type, NULL, description, "hPa", NULL,
                                                   read_pressure);
    path = "/levs[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* O3_number_density */
    description = "O3 number density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_number_density", harp_type_float, 2,
                                                   dimension_type, NULL, description, "cm^-3", NULL,
                                                   read_o3_number_density);
    path = "/o3_nd[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* O3_number_density_stdev */
    description = "uncertainty of the O3 number density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_number_density_stdev", harp_type_float, 2,
                                                   dimension_type, NULL, description, "cm^-3", NULL,
                                                   read_o3_number_density_error);
    path = "/o3_nd[], /o3_error[]";
    description = "derived from the relative error in percent as: o3_error[] * 0.01 * o3_nd[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* O3_number_density_cov */
    description = "O3 number density solution covariance matrix";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_number_density_cov", harp_type_float, 3,
                                                   dimension_type, NULL, description, "cm^-6", NULL,
                                                   read_o3_number_density_cov);
    path = "/sx[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* O3_number_density_cov_random */
    description = "O3 number density measurement noise covariance matrix";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_number_density_cov_random", harp_type_float,
                                                   3, dimension_type, NULL, description, "cm^-6", NULL,
                                                   read_o3_number_density_cov_random);
    path = "/sn[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* O3_number_density_avk */
    description = "O3 number density averaging kernel";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_number_density_avk", harp_type_float, 3,
                                                   dimension_type, NULL, description, "1", NULL,
                                                   read_o3_number_density_avk);
    path = "/ak[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* O3_volume_mixing_ratio */
    description = "O3 volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_volume_mixing_ratio", harp_type_float, 2,
                                                   dimension_type, NULL, description, "ppv", NULL,
                                                   read_o3_volume_mixing_ratio);
    path = "/o3_vmr[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* O3_volume_mixing_ratio_stdev */
    description = "uncertainty of the O3 volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_volume_mixing_ratio_stdev", harp_type_float,
                                                   2, dimension_type, NULL, description, "ppv", NULL,
                                                   read_o3_volume_mixing_ratio_error);
    path = "/o3_vmr[], /o3_error[]";
    description = "derived from the relative error in percent as: o3_error[] * 0.01 * o3_vmr[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* O3_volume_mixing_ratio_apriori */
    description = "O3 volume mixing ratio apriori";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_volume_mixing_ratio_apriori",
                                                   harp_type_float, 2, dimension_type, NULL, description, "ppv", NULL,
                                                   read_o3_volume_mixing_ratio_apriori);
    path = "/o3_ap[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* O3_volume_mixing_ratio_apriori_stdev */
    description = "uncertainty of the O3 volume mixing ratio apriori";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_volume_mixing_ratio_apriori_stdev",
                                                   harp_type_float, 2, dimension_type, NULL, description, "ppv", NULL,
                                                   read_o3_volume_mixing_ratio_apriori_error);
    path = "/o3_ap[], /o3_ap_error[]";
    description = "derived from the relative error in percent as: o3_ap_error[] * 0.01 * o3_ap[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* cloud_fraction */
    description = "effective cloud fraction";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_fraction", harp_type_double, 1,
                                                   dimension_type, NULL, description, "1", NULL, read_cloud_fraction);
    path = "/cloudf[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_top_pressure */
    description = "cloud top pressure";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_top_pressure", harp_type_double, 1,
                                                   dimension_type, NULL, description, "hPa", NULL,
                                                   read_cloud_top_pressure);
    path = "/cloudp[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_top_albedo */
    description = "cloud top albedo";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_top_albedo", harp_type_double, 1,
                                                   dimension_type, NULL, description, "1", NULL, read_cloud_top_albedo);
    path = "/clouda[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* surface_albedo */
    description = "surface albedo";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "surface_albedo", harp_type_float, 1,
                                                   dimension_type, NULL, description, "1", NULL, read_surface_albedo);
    path = "/salb[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* surface_pressure */
    description = "surface pressure";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "surface_pressure", harp_type_float, 1,
                                                   dimension_type, NULL, description, "hPa", NULL,
                                                   read_surface_pressure);
    path = "/spres[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    return 0;
}
