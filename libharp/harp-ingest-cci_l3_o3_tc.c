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
#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct ingest_info_struct
{
    coda_product *product;
    double datetime_start;
    double datetime_stop;
    long num_latitude;
    long num_longitude;
} ingest_info;

static int read_datetime(ingest_info *info, const char *path, double *datetime)
{
    coda_cursor cursor;
    char buffer[9];
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
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, NULL);
        return -1;
    }
    if (length != 8)
    {
        harp_set_error(HARP_ERROR_PRODUCT, "datetime value has length %ld; expected 8 (yyyyMMdd)", length);
        harp_add_coda_cursor_path_to_error_message(&cursor);
        return -1;
    }
    if (coda_cursor_read_string(&cursor, buffer, 9) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_time_string_to_double_utc("yyyyMMdd", buffer, datetime) != 0)
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
    if (coda_cursor_goto(&cursor, "/longitude") != 0)
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
        harp_set_error(HARP_ERROR_PRODUCT, "dataset has %d dimensions; expected 1", num_coda_dims);
        harp_add_coda_cursor_path_to_error_message(&cursor);
        return -1;
    }
    info->num_longitude = coda_dim[0];

    if (coda_cursor_goto(&cursor, "/latitude") != 0)
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
        harp_set_error(HARP_ERROR_PRODUCT, "dataset has %d dimensions; expected 1", num_coda_dims);
        harp_add_coda_cursor_path_to_error_message(&cursor);
        return -1;
    }
    info->num_latitude = coda_dim[0];

    return 0;
}

static int init_datetime(ingest_info *info)
{
    if (read_datetime(info, "/@time_coverage_start", &info->datetime_start) != 0)
    {
        return -1;
    }

    if (read_datetime(info, "/@time_coverage_end", &info->datetime_stop) != 0)
    {
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

    if (init_datetime(info) != 0)
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
        harp_set_error(HARP_ERROR_PRODUCT, "dataset has %ld elements (expected %ld)", coda_num_elements, num_elements);
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

/* Transform an array of N interval centers to an array of 2N interval bounds. The transformation is performed in place.
 * The size of the array \a data should be at least 2N, where the first N elements should contain the interval centers.
 *
 * The interval bounds are assumed to be located halfway between the interval centers. The lower bound of the first
 * interval is determined by assuming that the distance between the lower bound of the first interval and the center of
 * the first interval is equal to the distance between the upper bound of the first interval and the center of the first
 * interval. The upper bound of the last interval is computed similarly.
 */
static void compute_interval_centers_from_interval_bounds(long num_elements, double default_interval, double *data)
{
    if (num_elements == 0)
    {
        return;
    }

    if (num_elements == 1)
    {
        data[1] = data[0] + 0.5 * default_interval;
        data[0] = data[0] - 0.5 * default_interval;
    }
    else
    {
        long i;

        /* Compute the bounds of the last interval. */
        data[(num_elements - 1) * 2 + 1] = 0.5 * (3.0 * data[num_elements - 1] - data[num_elements - 2]);
        data[(num_elements - 1) * 2] = 0.5 * (data[num_elements - 1] + data[num_elements - 2]);

        /* Compute all intermediate bounds. */
        for (i = num_elements - 2; i >= 1; --i)
        {
            /* The upper bound of interval i equals the lower bound of interval i + 1. */
            data[i * 2 + 1] = data[(i + 1) * 2];
            data[i * 2] = 0.5 * (data[i] + data[i - 1]);
        }

        /* Compute the bounds of the first interval. */
        data[0] = 0.5 * (3.0 * data[0] - data[1]);
        data[1] = data[2];
    }
}

static int read_dimensions(void *user_data, long dimension[HARP_NUM_DIM_TYPES])
{
    ingest_info *info = (ingest_info *)user_data;

    dimension[harp_dimension_time] = 1;
    dimension[harp_dimension_latitude] = info->num_latitude;
    dimension[harp_dimension_longitude] = info->num_longitude;

    return 0;
}

static int read_datetime_bounds(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    data.double_data[0] = info->datetime_start;
    data.double_data[1] = info->datetime_stop;

    return 0;
}

static int read_longitude_bounds(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (read_dataset(info, "/longitude", info->num_longitude, data) != 0)
    {
        return -1;
    }

    compute_interval_centers_from_interval_bounds(info->num_longitude, 360.0, data.double_data);

    return 0;
}

static int read_latitude_bounds(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (read_dataset(info, "/latitude", info->num_latitude, data) != 0)
    {
        return -1;
    }

    compute_interval_centers_from_interval_bounds(info->num_latitude, 180.0, data.double_data);

    return 0;
}

static int read_longitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "/longitude", info->num_longitude, data);
}

static int read_latitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "/latitude", info->num_latitude, data);
}

static int read_O3_column_number_density(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "/atmosphere_mole_content_of_ozone", info->num_latitude * info->num_longitude, data);
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
    if (coda_cursor_goto(&cursor, "/atmosphere_mole_content_of_ozone") != 0)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, "/atmosphere_mole_content_of_ozone_number_of_observations") != 0)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, NULL);
        return -1;
    }

    return 0;
}

int harp_ingestion_module_cci_l3_o3_tc_init(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type datetime_dimension_type[2] = { harp_dimension_time, harp_dimension_independent };
    harp_dimension_type longitude_dimension_type[2] = { harp_dimension_longitude, harp_dimension_independent };
    harp_dimension_type latitude_dimension_type[2] = { harp_dimension_latitude, harp_dimension_independent };
    harp_dimension_type dimension_type[2] = { harp_dimension_latitude, harp_dimension_longitude };
    long bounds_dimension[2] = { -1, 2 };
    const char *description;
    const char *path;

    module = harp_ingestion_register_module_coda("ESACCI_OZONE_L3_TC", NULL, NULL, "CCI (climate change initiative) L3 "
                                                 "O3 total column products", verify_product_type, ingestion_init,
                                                 ingestion_done);

    /* ESACCI_OZONE_L3_TC product */
    product_definition = harp_ingestion_register_product(module, "ESACCI_OZONE_L3_TC", "CCI L3 O3 total column product",
                                                         read_dimensions);

    /* datetime */
    description = "time coverage";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double, 2,
                                                   datetime_dimension_type, bounds_dimension, description,
                                                   "seconds since 2000-01-01", NULL, read_datetime_bounds);
    path = "/@time_coverage_start, /@time_coverage_end";
    description = "datetime converted from a UTC start and end date to seconds since 2000-01-01 TAI";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* longitude */
    description = "longitude of the grid cell center";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_double, 1,
                                                   longitude_dimension_type, NULL, description, "degree_east", NULL,
                                                   read_longitude);
    path = "/longitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* latitude */
    description = "latitude of the grid cell center";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_double, 1,
                                                   latitude_dimension_type, NULL, description, "degree_north", NULL,
                                                   read_latitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0f, 90.0f);
    path = "/latitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* longitude_bounds */
    description = "longitudes of the grid cell boundaries";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "longitude_bounds", harp_type_double, 2,
                                                   longitude_dimension_type, bounds_dimension, description,
                                                   "degree_east", NULL, read_longitude_bounds);
    path = "/longitude[]";
    description = "The longitudes of the grid cell boundaries are not included in the product. HARP therefore provides"
                  "its own approximation. Each cell boundary is determined by the two cell centers surrounding it. The "
                  "cell boundary is placed halfway between the surrounding cell centers. For the cells at the edge of "
                  "the grid, virtual cell centers are created by extrapolation. Each virtual cell center is placed "
                  "such that the distance between the virtual cell center and the closest real cell center is equal to "
                  "the distance between the closest real cell center and second closest real cell center. It is "
                  "assumed that the longitudes from the product form either a strictly increasing or a strictly "
                  "decreasing sequence.";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* latitude_bounds */
    description = "latitudes of the grid cell boundaries";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "latitude_bounds", harp_type_double, 2,
                                                   latitude_dimension_type, bounds_dimension, description,
                                                   "degree_north", NULL, read_latitude_bounds);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0f, 90.0f);
    path = "/latitude[]";
    description = "The latitudes of the grid cell boundaries are not included in the product. HARP therefore provides"
                  "its own approximation. Each cell boundary is determined by the two cell centers surrounding it. The "
                  "cell boundary is placed halfway between the surrounding cell centers. For the cells at the edge of "
                  "the grid, virtual cell centers are created by extrapolation. Each virtual cell center is placed "
                  "such that the distance between the virtual cell center and the closest real cell center is equal to "
                  "the distance between the closest real cell center and second closest real cell center. It is "
                  "assumed that the latitudes from the product form either a strictly increasing or a strictly "
                  "decreasing sequence.";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* O3_column_number_density */
    description = "O3 total column number density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_column_number_density", harp_type_double, 2,
                                                   dimension_type, NULL, description, "DU", NULL,
                                                   read_O3_column_number_density);
    path = "/atmosphere_mole_content_of_ozone[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    return 0;
}
