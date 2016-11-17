/*
 * Copyright (C) 2015-2016 S[&]T, The Netherlands.
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
#include "harp-ingestion.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef struct ingest_info_struct
{
    coda_product *product;

    long num_times;
    long num_scanlines;
    long num_pixels;
    long num_corners;
    long num_layers;

    coda_cursor product_cursor;
    coda_cursor geolocation_cursor;
    coda_cursor detailed_results_cursor;
    coda_cursor input_data_cursor;

} ingest_info;

static void broadcast_array_double(long num_scanlines, long num_pixels, double *data)
{
    long i;

    /* Repeat the value for each scanline for all pixels in that scanline. Iterate in reverse to avoid overwriting
     * scanline values.
     */
    for (i = num_scanlines - 1; i >= 0; --i)
    {
        double *pixel = data + i * num_pixels;
        double *pixel_end = pixel + num_pixels;
        const double scanline_value = data[i];

        for (; pixel != pixel_end; pixel++)
        {
            *pixel = scanline_value;
        }
    }
}

static int get_dimension_length(ingest_info *info, const char *name, long *length)
{
    coda_cursor cursor;
    long coda_dim[CODA_MAX_NUM_DIMS];
    int num_coda_dims;

    cursor = info->product_cursor;
    if (coda_cursor_goto_record_field_by_name(&cursor, name) != 0)
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
        harp_set_error(HARP_ERROR_INGESTION, "cannot determine length of dimension '%s'", name);
        return -1;
    }
    *length = coda_dim[0];

    return 0;
}

static int init_cursors(ingest_info *info)
{
    coda_cursor cursor;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&cursor, "PRODUCT") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    info->product_cursor = cursor;

    if (coda_cursor_goto_record_field_by_name(&cursor, "SUPPORT_DATA") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    if (coda_cursor_goto_record_field_by_name(&cursor, "GEOLOCATIONS") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    info->geolocation_cursor = cursor;

    coda_cursor_goto_parent(&cursor);
    if (coda_cursor_goto_record_field_by_name(&cursor, "DETAILED_RESULTS") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    info->detailed_results_cursor = cursor;

    coda_cursor_goto_parent(&cursor);
    if (coda_cursor_goto_record_field_by_name(&cursor, "INPUT_DATA") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    info->input_data_cursor = cursor;

    return 0;
}

static int init_dimensions(ingest_info *info)
{
    if (get_dimension_length(info, "time", &info->num_times) != 0)
    {
        return -1;
    }

    if (get_dimension_length(info, "scanline", &info->num_scanlines) != 0)
    {
        return -1;
    }

    if (get_dimension_length(info, "ground_pixel", &info->num_pixels) != 0)
    {
        return -1;
    }

    if (get_dimension_length(info, "corner", &info->num_corners) != 0)
    {
        return -1;
    }

    if (get_dimension_length(info, "layer", &info->num_layers) != 0)
    {
        return -1;
    }

    if (info->num_times != 1)
    {
        harp_set_error(HARP_ERROR_INGESTION, "dimension 'time' has length %ld; expected 1", info->num_times);
        return -1;
    }

    if (info->num_corners != 4)
    {
        harp_set_error(HARP_ERROR_INGESTION, "dimension 'corner' has length %ld; expected 4", info->num_corners);
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

    info = (ingest_info *)malloc(sizeof(ingest_info));
    if (info == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(ingest_info), __FILE__, __LINE__);
        return -1;
    }

    info->product = product;
    info->num_times = 0;
    info->num_scanlines = 0;
    info->num_pixels = 0;
    info->num_corners = 0;
    info->num_layers = 0;

    if (init_cursors(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }

    if (init_dimensions(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }

    *definition = *module->product_definition;
    *user_data = info;

    return 0;
}

static int read_dataset(coda_cursor cursor, const char *dataset_name, harp_data_type data_type, long num_elements,
                        harp_array data)
{
    long coda_num_elements;
    harp_scalar fill_value;

    if (coda_cursor_goto_record_field_by_name(&cursor, dataset_name) != 0)
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
        harp_set_error(HARP_ERROR_INGESTION, "dataset has %ld elements; expected %ld", coda_num_elements, num_elements);
        harp_add_coda_cursor_path_to_error_message(&cursor);
        return -1;
    }

    switch (data_type)
    {
        case harp_type_int32:
            if (coda_cursor_read_uint32_array(&cursor, (uint32_t *)data.int32_data, coda_array_ordering_c) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            break;
        case harp_type_float:
            if (coda_cursor_read_float_array(&cursor, data.float_data, coda_array_ordering_c) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            if (coda_cursor_goto(&cursor, "@FillValue[0]") != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            if (coda_cursor_read_float(&cursor, &fill_value.float_data) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            /* Replace values equal to the _FillValue variable attribute by NaN. */
            harp_array_replace_fill_value(data_type, num_elements, data, fill_value);
            break;
        case harp_type_double:
            if (coda_cursor_read_double_array(&cursor, data.double_data, coda_array_ordering_c) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            if (coda_cursor_goto(&cursor, "@FillValue[0]") != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            if (coda_cursor_read_double(&cursor, &fill_value.double_data) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            /* Replace values equal to the _FillValue variable attribute by NaN. */
            harp_array_replace_fill_value(data_type, num_elements, data, fill_value);
            break;
        default:
            assert(0);
            exit(1);
    }

    return 0;
}

static int read_dimensions(void *user_data, long dimension[HARP_NUM_DIM_TYPES])
{
    ingest_info *info = (ingest_info *)user_data;

    dimension[harp_dimension_time] = info->num_times * info->num_scanlines * info->num_pixels;
    dimension[harp_dimension_vertical] = info->num_layers;

    return 0;
}

static int read_scanline_pixel_index(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    index = index - (index / info->num_pixels) * info->num_pixels;
    *data.int16_data = (int16_t)index;

    return 0;
}

static int read_datetime(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    harp_array time_reference_array;
    double time_reference;
    long i;

    /* Even though the product specification may not accurately describe this, S5P treats all days as having 86400
     * seconds (as does HARP). The time value is thus the sum of:
     * - the S5P time reference as seconds since 2010 (using 86400 seconds per day)
     * - the number of seconds since the S5P time reference
     */

    /* Read reference time in seconds since 2010-01-01 */
    time_reference_array.ptr = &time_reference;
    if (read_dataset(info->product_cursor, "time", harp_type_double, 1, time_reference_array) != 0)
    {
        return -1;
    }

    /* Read difference in milliseconds (ms) between the time reference and the start of the observation. */
    if (read_dataset(info->product_cursor, "delta_time", harp_type_double, info->num_scanlines, data) != 0)
    {
        return -1;
    }

    /* Convert observation start time to seconds since 2010-01-01 */
    for (i = 0; i < info->num_scanlines; i++)
    {
        data.double_data[i] = time_reference + data.double_data[i] / 1e3;
    }

    /* Broadcast the result along the pixel dimension. */
    broadcast_array_double(info->num_scanlines, info->num_pixels, data.double_data);

    return 0;
}

static int read_longitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "longitude", harp_type_float, info->num_scanlines * info->num_pixels,
                        data);
}

static int read_latitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "latitude", harp_type_float, info->num_scanlines * info->num_pixels,
                        data);
}

static int read_longitude_bounds(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->geolocation_cursor, "longitude_bounds", harp_type_float,
                        info->num_scanlines * info->num_pixels * info->num_corners, data);
}

static int read_latitude_bounds(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->geolocation_cursor, "latitude_bounds", harp_type_float,
                        info->num_scanlines * info->num_pixels * info->num_corners, data);
}


static int read_solar_zenith_angle(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->geolocation_cursor, "solar_zenith_angle", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_relative_azimuth_angle(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->geolocation_cursor, "relative_azimuth_angle", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_viewing_zenith_angle(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->geolocation_cursor, "viewing_zenith_angle", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_pressure_bounds(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    harp_array hybride_coef_a;
    harp_array hybride_coef_b;
    long num_profiles;
    long num_layers;
    long i;

    num_profiles = info->num_scanlines * info->num_pixels;
    num_layers = info->num_layers;

    /* The air pressure boundaries are interpolated from the position dependent surface air pressure using a
     * position independent set of coefficients a and b.
     */
    hybride_coef_a.ptr = malloc(num_layers * 2 * sizeof(double));
    if (hybride_coef_a.ptr == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_layers * 2 * sizeof(double), __FILE__, __LINE__);
        return -1;
    }

    hybride_coef_b.ptr = malloc(num_layers * 2 * sizeof(double));
    if (hybride_coef_b.ptr == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_layers * 2 * sizeof(double), __FILE__, __LINE__);
        free(hybride_coef_a.ptr);
        return -1;
    }

    if (read_dataset(info->product_cursor, "tm5_pressure_level_a", harp_type_double, num_layers * 2, hybride_coef_a) !=
        0)
    {
        free(hybride_coef_b.ptr);
        free(hybride_coef_a.ptr);
        return -1;
    }

    if (read_dataset(info->product_cursor, "tm5_pressure_level_b", harp_type_double, num_layers * 2, hybride_coef_b) !=
        0)
    {
        free(hybride_coef_b.ptr);
        free(hybride_coef_a.ptr);
        return -1;
    }

    if (read_dataset(info->input_data_cursor, "surface_pressure", harp_type_double, num_profiles, data) != 0)
    {
        free(hybride_coef_b.ptr);
        free(hybride_coef_a.ptr);
        return -1;
    }

    for (i = num_profiles - 1; i >= 0; i--)
    {
        double *bounds = &data.double_data[i * num_layers * 2]; /* bounds for specific (time, lat, lon) */
        double surface_pressure = data.double_data[i] * 100.0;  /* surface pressure at specific (time, lat, lon) */
        long j;

        for (j = 0; j < num_layers; j++)
        {
            bounds[j * 2] = hybride_coef_a.double_data[j * 2] + hybride_coef_b.double_data[j * 2] * surface_pressure;
            bounds[j * 2 + 1] = hybride_coef_a.double_data[j * 2 + 1] + hybride_coef_b.double_data[j * 2 + 1] *
                surface_pressure;
        }
    }

    free(hybride_coef_b.ptr);
    free(hybride_coef_a.ptr);

    return 0;
}

static int read_no2_column_tropospheric(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "tropospheric_no2_vertical_column", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_no2_column_tropospheric_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "tropospheric_no2_vertical_column_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_no2_column_tropospheric_amf(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "amf_trop", harp_type_float, info->num_scanlines * info->num_pixels,
                        data);
}

static int read_no2_column_tropospheric_validity(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "processing_quality_flags", harp_type_int32,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_no2_column(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "total_no2_vertical_column", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_no2_column_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "total_no2_vertical_column_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_no2_column_amf(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "amf_total", harp_type_float, info->num_scanlines * info->num_pixels,
                        data);
}

static int read_no2_column_avk(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "averaging_kernel", harp_type_float,
                        info->num_scanlines * info->num_pixels * info->num_layers, data);
}

static int read_hcho_column_tropospheric(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "tropospheric_hcho_vertical_column", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_hcho_column_tropospheric_uncertainty_random(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "tropospheric_hcho_vertical_column_uncertainty_random", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_hcho_column_tropospheric_uncertainty_systematic(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "tropospheric_hcho_vertical_column_uncertainty_systematic",
                        harp_type_float, info->num_scanlines * info->num_pixels, data);
}

static int read_hcho_column_tropospheric_validity(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "processing_quality_flags", harp_type_int32,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_hcho_column_avk(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "averaging_kernel", harp_type_float,
                        info->num_scanlines * info->num_pixels * info->num_layers, data);
}

static int read_hcho_column_apriori(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "hcho_profile_apriori", harp_type_float,
                        info->num_scanlines * info->num_pixels * info->num_layers, data);
}

static void register_core_variables(harp_product_definition *product_definition)
{
    const char *path;
    const char *description;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };

    description = "pixel index (0-based) within the scanline";
    variable_definition =
        harp_ingestion_register_variable_sample_read(product_definition, "scanline_pixel_index", harp_type_int16, 1,
                                                     dimension_type, NULL, description, NULL, NULL,
                                                     read_scanline_pixel_index);
    description =
        "the scanline and pixel dimensions are collapsed into a temporal dimension; the index of the pixel within the "
        "scanline is computed as the index on the temporal dimension modulo the number of scanlines";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, NULL, description);

    description = "start time of the measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double, 1, dimension_type,
                                                   NULL, description, "seconds since 2010-01-01", NULL, read_datetime);
    path = "/PRODUCT/time, /PRODUCT/delta_time[]";
    description =
        "time converted from milliseconds since a reference time (given as seconds since 2010-01-01) to seconds since "
        "2010-01-01 (using 86400 seconds per day); the time associated with a scanline is repeated for each pixel in "
        "the scanline";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);
}

static void register_geolocation_variables(harp_product_definition *product_definition)
{
    const char *path;
    const char *description;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };

    description = "longitude of the ground pixel center (WGS84)";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_float,
                                                                     1, dimension_type, NULL, description,
                                                                     "degree_east", NULL, read_longitude);
    harp_variable_definition_set_valid_range_float(variable_definition, -180.0f, 180.0f);
    path = "/PRODUCT/longitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "latitude of the ground pixel center (WGS84)";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_float, 1,
                                                                     dimension_type, NULL, description, "degree_north",
                                                                     NULL, read_latitude);
    harp_variable_definition_set_valid_range_float(variable_definition, -90.0f, 90.0f);
    path = "/PRODUCT/latitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

static void register_additional_geolocation_variables(harp_product_definition *product_definition)
{
    const char *path;
    const char *description;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };
    harp_dimension_type bounds_dimension_type[2] = { harp_dimension_time, harp_dimension_independent };
    long bounds_dimension[2] = { -1, 4 };

    description = "longitudes of the ground pixel corners (WGS84)";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "longitude_bounds",
                                                                     harp_type_float, 2, bounds_dimension_type,
                                                                     bounds_dimension, description, "degree_east",
                                                                     NULL, read_longitude_bounds);
    harp_variable_definition_set_valid_range_float(variable_definition, -180.0f, 180.0f);
    path = "/PRODUCT/SUPPORT_DATA/GEOLOCATIONS/longitude_bounds[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "latitudes of the ground pixel corners (WGS84)";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "latitude_bounds",
                                                                     harp_type_float, 2, bounds_dimension_type,
                                                                     bounds_dimension, description, "degree_north",
                                                                     NULL, read_latitude_bounds);
    harp_variable_definition_set_valid_range_float(variable_definition, -90.0f, 90.0f);
    path = "/PRODUCT/SUPPORT_DATA/GEOLOCATIONS/latitude_bounds[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* Angles. */
    description = "zenith angle of the Sun at the ground pixel location (WGS84); angle measured away from the vertical";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "solar_zenith_angle",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     description, "degree", NULL,
                                                                     read_solar_zenith_angle);
    harp_variable_definition_set_valid_range_float(variable_definition, 0.0f, 180.0f);
    path = "/PRODUCT/SUPPORT_DATA/GEOLOCATIONS/solar_zenith_angle[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "relative azimuth angle at the ground pixel location (WGS84); angle measured East-of-North";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "relative_azimuth_angle",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     description, "degree", NULL,
                                                                     read_relative_azimuth_angle);
    harp_variable_definition_set_valid_range_float(variable_definition, -180.0f, 180.0f);
    path = "/PRODUCT/SUPPORT_DATA/GEOLOCATIONS/relative_azimuth_angle[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "zenith angle of the satellite at the ground pixel location (WGS84); angle measured away from the "
        "vertical";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "sensor_zenith_angle",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     description, "degree", NULL,
                                                                     read_viewing_zenith_angle);
    harp_variable_definition_set_valid_range_float(variable_definition, 0.0f, 180.0f);
    path = "/PRODUCT/SUPPORT_DATA/GEOLOCATIONS/viewing_zenith_angle[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

static void register_hcho_product(void)
{
    const char *path;
    const char *description;
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_vertical };
    harp_dimension_type pressure_bounds_dimension_type[3] =
        { harp_dimension_time, harp_dimension_vertical, harp_dimension_independent };
    long pressure_bounds_dimension[3] = { -1, -1, 2 };

    module = harp_ingestion_register_module_coda("QA4ECV_L2_HCHO", "QA4ECV", "QA4ECV", "L2_HCHO",
                                                 "QA4ECV L2 HCHO total column", ingestion_init, ingestion_done);

    product_definition = harp_ingestion_register_product(module, "QA4ECV_L2_HCHO", NULL, read_dimensions);
    register_core_variables(product_definition);
    register_geolocation_variables(product_definition);
    register_additional_geolocation_variables(product_definition);

    description = "pressure boundaries";
    variable_definition =
    harp_ingestion_register_variable_full_read(product_definition, "pressure_bounds", harp_type_double, 3,
                                               pressure_bounds_dimension_type, pressure_bounds_dimension,
                                               description, "Pa", NULL, read_pressure_bounds);
    path = "/PRODUCT/tm5_pressure_level_a[],/PRODUCT/tm5_pressure_level_b[],"
        "/PRODUCT/SUPPORT_DATA/INPUT_DATA/surface_pressure[]";
    description = "pressure in Pa at level k is derived from surface pressure in hPa as: tm5_pressure_level_a[k] + "
        "tm5_pressure_level_b[k] * surface_pressure[] * 100.0";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    description = "tropospheric vertical column of HCHO";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "tropospheric_HCHO_column_number_density",
                                                   harp_type_float, 1, dimension_type, NULL, description, "molec/cm^2",
                                                   NULL, read_hcho_column_tropospheric);
    path = "/PRODUCT/tropospheric_hcho_vertical_column[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "uncertainty of the tropospheric vertical column of HCHO due to random effects";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "tropospheric_HCHO_column_number_density_uncertainty_random",
                                                   harp_type_float, 1, dimension_type, NULL, description, "molec/cm^2",
                                                   NULL, read_hcho_column_tropospheric_uncertainty_random);
    path = "/PRODUCT/tropospheric_hcho_vertical_column_uncertainty_random[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "uncertainty of the tropospheric vertical column of HCHO due to systematic effects";
    variable_definition =
    harp_ingestion_register_variable_full_read(product_definition,
                                               "tropospheric_HCHO_column_number_density_uncertainty_systematic",
                                               harp_type_float, 1, dimension_type, NULL, description, "molec/cm^2",
                                               NULL, read_hcho_column_tropospheric_uncertainty_systematic);
    path = "/PRODUCT/tropospheric_hcho_vertical_column_uncertainty_systematic[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "processing quality flag of the tropospheric vertical column of HCHO";
    variable_definition =
    harp_ingestion_register_variable_full_read(product_definition,
                                               "tropospheric_HCHO_column_number_density_validity",
                                               harp_type_int32, 1, dimension_type, NULL, description, NULL,
                                               NULL, read_hcho_column_tropospheric_validity);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/processing_validity_flags[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "averaging kernel for the total column number density of tropospheric HCHO";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "HCHO_column_number_density_avk",
                                                   harp_type_float, 2, dimension_type, NULL, description, NULL, NULL,
                                                   read_hcho_column_avk);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/averaging_kernels[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "apriori profile for the total column number density of tropospheric HCHO";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "HCHO_column_number_density_apriori",
                                                   harp_type_float, 2, dimension_type, NULL, description, NULL, NULL,
                                                   read_hcho_column_apriori);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/hcho_profile_apriori[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

static void register_no2_product(void)
{
    const char *path;
    const char *description;
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_vertical };
    harp_dimension_type pressure_bounds_dimension_type[3] =
        { harp_dimension_time, harp_dimension_vertical, harp_dimension_independent };
    long pressure_bounds_dimension[3] = { -1, -1, 2 };

    module = harp_ingestion_register_module_coda("QA4ECV_L2_NO2", "QA4ECV", "QA4ECV", "L2_NO2",
                                                 "QA4ECV NO2 tropospheric column", ingestion_init, ingestion_done);

    product_definition = harp_ingestion_register_product(module, "QA4ECV_L2_NO2", NULL, read_dimensions);
    register_core_variables(product_definition);
    register_geolocation_variables(product_definition);
    register_additional_geolocation_variables(product_definition);

    description = "pressure boundaries";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "pressure_bounds", harp_type_double, 3,
                                                   pressure_bounds_dimension_type, pressure_bounds_dimension,
                                                   description, "Pa", NULL, read_pressure_bounds);
    path = "/PRODUCT/tm5_pressure_level_a[],/PRODUCT/tm5_pressure_level_b[],"
        "/PRODUCT/SUPPORT_DATA/INPUT_DATA/surface_pressure[]";
    description = "pressure in Pa at level k is derived from surface pressure in hPa as: tm5_pressure_level_a[k] + "
        "tm5_pressure_level_b[k] * surface_pressure[] * 100.0";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    description = "tropospheric vertical column of NO2";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "tropospheric_NO2_column_number_density",
                                                   harp_type_float, 1, dimension_type, NULL, description, "molec/cm^2",
                                                   NULL, read_no2_column_tropospheric);
    path = "/PRODUCT/tropospheric_no2_vertical_column[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "uncertainty of the tropospheric vertical column of NO2 (standard error)";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "tropospheric_NO2_column_number_density_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description, "molec/cm^2",
                                                   NULL, read_no2_column_tropospheric_precision);
    path = "/PRODUCT/tropospheric_no2_vertical_column_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "processing quality flag of the tropospheric vertical column of NO2";
    variable_definition =
    harp_ingestion_register_variable_full_read(product_definition,
                                               "tropospheric_NO2_column_number_density_validity",
                                               harp_type_int32, 1, dimension_type, NULL, description, NULL,
                                               NULL, read_no2_column_tropospheric_validity);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/processing_validity_flags[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "tropospheric air mass factor, computed by integrating the altitude dependent air mass factor over "
        "the atmospheric layers from the surface up to and including the layer with the tropopause";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "tropospheric_NO2_column_number_density_amf",
                                                   harp_type_float, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL, read_no2_column_tropospheric_amf);
    path = "/PRODUCT/amf_trop[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "total vertical column of NO2 (ratio of the slant column density of NO2 and the total air mass "
        "factor)";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "NO2_column_number_density", harp_type_float, 1,
                                                   dimension_type, NULL, description, "molec/cm^2", NULL,
                                                   read_no2_column);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/total_no2_vertical_column[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "uncertainty of the total vertical column of NO2 (standard error)";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "NO2_column_number_density_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description, "molec/cm^2",
                                                   NULL, read_no2_column_precision);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/total_no2_vertical_column_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "total air mass factor, computed by integrating the altitude dependent air mass factor over the "
        "atmospheric layers from the surface to top-of-atmosphere";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "NO2_column_number_density_amf", harp_type_float,
                                                   1, dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_no2_column_amf);
    path = "/PRODUCT/amf_total[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "averaging kernel for the air mass factor correction, describing the NO2 profile sensitivity of the "
        "vertical column density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "NO2_column_number_density_avk", harp_type_float,
                                                   2, dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_no2_column_avk);
    path = "/PRODUCT/averaging_kernel[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

int harp_ingestion_module_qa4ecv_init(void)
{
    register_hcho_product();
    register_no2_product();

    return 0;
}
