/*
 * Copyright (C) 2015-2018 S[&]T, The Netherlands.
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
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static const char *snow_ice_type_values[] = { "snow_free_land", "sea_ice", "permanent_ice", "snow", "ocean" };

typedef struct ingest_info_struct
{
    coda_product *product;
    int use_summed_total_column;
    int use_radiance_cloud_fraction;

    long num_times;
    long num_scanlines;
    long num_pixels;
    long num_corners;
    long num_layers;

    coda_cursor product_cursor;
    coda_cursor geolocation_cursor;
    coda_cursor detailed_results_cursor;
    coda_cursor input_data_cursor;

    harp_array hybride_coef_a;
    harp_array hybride_coef_b;
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
        case harp_type_int8:
            {
                coda_native_type read_type;

                if (coda_cursor_goto_first_array_element(&cursor) != 0)
                {
                    harp_set_error(HARP_ERROR_CODA, NULL);
                    return -1;
                }
                if (coda_cursor_get_read_type(&cursor, &read_type) != 0)
                {
                    harp_set_error(HARP_ERROR_CODA, NULL);
                    return -1;
                }
                coda_cursor_goto_parent(&cursor);
                if (read_type == coda_native_type_uint8)
                {
                    if (coda_cursor_read_uint8_array(&cursor, (uint8_t *)data.int8_data, coda_array_ordering_c) != 0)
                    {
                        harp_set_error(HARP_ERROR_CODA, NULL);
                        return -1;
                    }
                }
                else
                {
                    if (coda_cursor_read_int8_array(&cursor, data.int8_data, coda_array_ordering_c) != 0)
                    {
                        harp_set_error(HARP_ERROR_CODA, NULL);
                        return -1;
                    }
                }
            }
            break;
        case harp_type_int32:
            {
                coda_native_type read_type;

                if (coda_cursor_goto_first_array_element(&cursor) != 0)
                {
                    harp_set_error(HARP_ERROR_CODA, NULL);
                    return -1;
                }
                if (coda_cursor_get_read_type(&cursor, &read_type) != 0)
                {
                    harp_set_error(HARP_ERROR_CODA, NULL);
                    return -1;
                }
                coda_cursor_goto_parent(&cursor);
                if (read_type == coda_native_type_uint32)
                {
                    if (coda_cursor_read_uint32_array(&cursor, (uint32_t *)data.int32_data, coda_array_ordering_c) != 0)
                    {
                        harp_set_error(HARP_ERROR_CODA, NULL);
                        return -1;
                    }
                }
                else
                {
                    if (coda_cursor_read_int32_array(&cursor, data.int32_data, coda_array_ordering_c) != 0)
                    {
                        harp_set_error(HARP_ERROR_CODA, NULL);
                        return -1;
                    }
                }
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

static int init_hybride_coef(ingest_info *info)
{
    info->hybride_coef_a.ptr = malloc(info->num_layers * 2 * sizeof(double));
    if (info->hybride_coef_a.ptr == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       info->num_layers * 2 * sizeof(double), __FILE__, __LINE__);
        return -1;
    }

    info->hybride_coef_b.ptr = malloc(info->num_layers * 2 * sizeof(double));
    if (info->hybride_coef_b.ptr == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       info->num_layers * 2 * sizeof(double), __FILE__, __LINE__);
        return -1;
    }

    if (read_dataset(info->product_cursor, "tm5_pressure_level_a", harp_type_double, info->num_layers * 2,
                     info->hybride_coef_a) != 0)
    {
        return -1;
    }

    if (read_dataset(info->product_cursor, "tm5_pressure_level_b", harp_type_double, info->num_layers * 2,
                     info->hybride_coef_b) != 0)
    {
        return -1;
    }

    return 0;
}

static void ingestion_done(void *user_data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->hybride_coef_a.ptr != NULL)
    {
        free(info->hybride_coef_a.ptr);
    }
    if (info->hybride_coef_b.ptr != NULL)
    {
        free(info->hybride_coef_b.ptr);
    }

    free(info);
}

static int ingestion_init(const harp_ingestion_module *module, coda_product *product,
                          const harp_ingestion_options *options, harp_product_definition **definition, void **user_data)
{
    const char *option_value;
    ingest_info *info;

    info = (ingest_info *)malloc(sizeof(ingest_info));
    if (info == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(ingest_info), __FILE__, __LINE__);
        return -1;
    }

    info->product = product;
    info->use_summed_total_column = 1;
    info->use_radiance_cloud_fraction = 0;
    info->num_times = 0;
    info->num_scanlines = 0;
    info->num_pixels = 0;
    info->num_corners = 0;
    info->num_layers = 0;
    info->hybride_coef_a.ptr = NULL;
    info->hybride_coef_b.ptr = NULL;

    if (harp_ingestion_options_has_option(options, "total_column"))
    {
        if (harp_ingestion_options_get_option(options, "total_column", &option_value) != 0)
        {
            ingestion_done(info);
            return -1;
        }
        if (strcmp(option_value, "total") == 0)
        {
            info->use_summed_total_column = 0;
        }
    }
    if (harp_ingestion_options_has_option(options, "cloud_fraction"))
    {
        info->use_radiance_cloud_fraction = 1;
    }

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

    if (init_hybride_coef(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }

    *definition = *module->product_definition;
    *user_data = info;

    return 0;
}

static int read_scan_subindex(void *user_data, long index, harp_array data)
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

    /* Read reference time in seconds since 1995-01-01 */
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

    /* Convert observation start time to seconds since 1995-01-01 */
    for (i = 0; i < info->num_scanlines; i++)
    {
        data.double_data[i] = time_reference + data.double_data[i] / 1e3;
    }

    /* Broadcast the result along the pixel dimension. */
    broadcast_array_double(info->num_scanlines, info->num_pixels, data.double_data);

    return 0;
}

static int read_orbit_index(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    coda_cursor cursor;
    coda_type_class type_class;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, "/@orbit") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_type_class(&cursor, &type_class) != 0)
    {
        return -1;
    }
    if (type_class == coda_array_class)
    {
        if (coda_cursor_goto_first_array_element(&cursor) != 0)
        {
            return -1;
        }
    }
    if (coda_cursor_read_int32(&cursor, data.int32_data) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

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

static int read_surface_altitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "surface_altitude", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_surface_pressure(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "tm5_surface_pressure", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_tropopause_pressure(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    harp_array layer_index;
    long num_profiles;
    long i;

    num_profiles = info->num_scanlines * info->num_pixels;

    layer_index.ptr = malloc(num_profiles * sizeof(int32_t));
    if (layer_index.ptr == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_profiles * sizeof(int32_t), __FILE__, __LINE__);
        return -1;
    }

    if (read_dataset(info->product_cursor, "tm5_tropopause_layer_index", harp_type_int32, num_profiles, layer_index) !=
        0)
    {
        free(layer_index.ptr);
        return -1;
    }

    if (read_dataset(info->product_cursor, "tm5_surface_pressure", harp_type_double, num_profiles, data) != 0)
    {
        free(layer_index.ptr);
        return -1;
    }

    for (i = 0; i < num_profiles; i++)
    {
        long index = layer_index.int32_data[i];

        if (index >= 0 && index < info->num_layers)
        {
            double surface_pressure = data.double_data[i] * 100.0;      /* surface pressure at specific (time, lat, lon) */

            /* the tropause level is the upper boundary of the layer defined by layer_index */
            data.double_data[i] = info->hybride_coef_a.double_data[index * 2 + 1] +
                info->hybride_coef_b.double_data[index * 2 + 1] * surface_pressure;
        }
        else
        {
            data.double_data[i] = harp_nan();
        }
    }

    free(layer_index.ptr);

    return 0;
}

static int read_pressure_bounds(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long num_profiles;
    long i;

    num_profiles = info->num_scanlines * info->num_pixels;

    /* The air pressure boundaries are interpolated from the position dependent surface air pressure using a
     * position independent set of coefficients a and b.
     */
    if (read_dataset(info->product_cursor, "tm5_surface_pressure", harp_type_double, num_profiles, data) != 0)
    {
        return -1;
    }

    for (i = num_profiles - 1; i >= 0; i--)
    {
        double *bounds = &data.double_data[i * info->num_layers * 2];   /* bounds for specific (time, lat, lon) */
        double surface_pressure = data.double_data[i] * 100.0;  /* surface pressure at specific (time, lat, lon) */
        long j;

        for (j = 0; j < info->num_layers; j++)
        {
            bounds[j * 2] = info->hybride_coef_a.double_data[j * 2] +
                info->hybride_coef_b.double_data[j * 2] * surface_pressure;
            bounds[j * 2 + 1] = info->hybride_coef_a.double_data[j * 2 + 1] +
                info->hybride_coef_b.double_data[j * 2 + 1] * surface_pressure;
        }
        /* to prevent TOA pressures of zero we make sure the TOA pressure is >= 1e-3 Pa */
        if (bounds[info->num_layers * 2 - 1] < 1e-3)
        {
            bounds[info->num_layers * 2 - 1] = 1e-3;
        }
    }

    return 0;
}

static int read_cloud_fraction_hcho(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->use_radiance_cloud_fraction)
    {
        return read_dataset(info->detailed_results_cursor, "cloud_radiance_fraction_hcho", harp_type_float,
                            info->num_scanlines * info->num_pixels, data);
    }
    return read_dataset(info->input_data_cursor, "cloud_fraction", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_cloud_fraction_no2(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->use_radiance_cloud_fraction)
    {
        return read_dataset(info->detailed_results_cursor, "cloud_radiance_fraction_no2", harp_type_float,
                            info->num_scanlines * info->num_pixels, data);
    }
    return read_dataset(info->input_data_cursor, "cloud_fraction", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_cloud_fraction_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "cloud_fraction_uncertainty", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_cloud_pressure(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "cloud_pressure", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_cloud_pressure_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "cloud_pressure_uncertainty", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_snow_ice_type(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long i;

    if (read_dataset(info->input_data_cursor, "snow_ice_flag", harp_type_int8, info->num_scanlines * info->num_pixels,
                     data) != 0)
    {
        return -1;
    }
    for (i = 0; i < info->num_scanlines * info->num_pixels; i++)
    {
        if (data.int8_data[i] < 0)
        {
            if (data.int8_data[i] == -1)        /* == int8 representation of 255 */
            {
                data.int8_data[i] = 4;
            }
            else
            {
                data.int8_data[i] = -1;
            }
        }
        else if (data.int8_data[i] > 0)
        {
            if (data.int8_data[i] <= 100)       /* 1..100 is mapped to sea_ice */
            {
                data.int8_data[i] = 1;
            }
            else if (data.int8_data[i] == 101)
            {
                data.int8_data[i] = 2;
            }
            else if (data.int8_data[i] == 103)
            {
                data.int8_data[i] = 3;
            }
            else
            {
                data.int8_data[i] = -1;
            }
        }
    }

    return 0;
}

static int read_sea_ice_fraction(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long i;

    if (read_dataset(info->input_data_cursor, "snow_ice_flag", harp_type_float, info->num_scanlines * info->num_pixels,
                     data) != 0)
    {
        return -1;
    }
    for (i = 0; i < info->num_scanlines * info->num_pixels; i++)
    {
        if (data.float_data[i] > 0 && data.float_data[i] <= 100)
        {
            data.float_data[i] /= 100.0;
        }
        else
        {
            data.float_data[i] = 0.0;
        }
    }

    return 0;
}

static int read_surface_albedo_hcho(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "surface_albedo_hcho", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_surface_albedo_no2(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "surface_albedo_no2", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_validity(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "processing_quality_flags", harp_type_int32,
                        info->num_scanlines * info->num_pixels, data);
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

static int read_hcho_column_tropospheric_amf(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "amf_trop", harp_type_float, info->num_scanlines * info->num_pixels,
                        data);
}

static int read_hcho_column_avk(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "averaging_kernel", harp_type_float,
                        info->num_scanlines * info->num_pixels * info->num_layers, data);
}

static int read_hcho_vmr_apriori(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "hcho_profile_apriori", harp_type_float,
                        info->num_scanlines * info->num_pixels * info->num_layers, data);
}

static int read_no2_column(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char *variable_name;

    variable_name = info->use_summed_total_column ? "summed_no2_total_vertical_column" : "total_no2_vertical_column";
    return read_dataset(info->detailed_results_cursor, variable_name, harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_no2_column_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char *variable_name;

    variable_name = info->use_summed_total_column ? "summed_no2_total_vertical_column_uncertainty" :
        "total_no2_vertical_column_uncertainty";

    return read_dataset(info->detailed_results_cursor, variable_name, harp_type_float,
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

static int read_no2_column_stratospheric(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "stratospheric_no2_vertical_column", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_no2_column_stratospheric_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "stratospheric_no2_vertical_column_uncertainty", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_no2_column_stratospheric_amf(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "amf_strat", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_no2_column_stratospheric_avk(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    harp_array layer_data;
    harp_array amf_data;
    long i, j;

    if (read_dataset(info->product_cursor, "averaging_kernel", harp_type_float,
                     info->num_scanlines * info->num_pixels * info->num_layers, data) != 0)
    {
        return -1;
    }

    layer_data.int32_data = malloc(info->num_scanlines * info->num_pixels * sizeof(int32_t));
    if (layer_data.int32_data == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       info->num_scanlines * info->num_pixels * sizeof(int32_t), __FILE__, __LINE__);
        return -1;
    }
    if (read_dataset(info->product_cursor, "tm5_tropopause_layer_index", harp_type_int32,
                     info->num_scanlines * info->num_pixels, layer_data) != 0)
    {
        free(layer_data.int32_data);
        return -1;
    }

    amf_data.float_data = malloc(info->num_scanlines * info->num_pixels * sizeof(float));
    if (amf_data.float_data == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       info->num_scanlines * info->num_pixels * sizeof(float), __FILE__, __LINE__);
        free(layer_data.int32_data);
        return -1;
    }
    if (read_no2_column_amf(user_data, amf_data) != 0)
    {
        free(amf_data.float_data);
        free(layer_data.int32_data);
        return -1;
    }
    for (i = 0; i < info->num_scanlines * info->num_pixels; i++)
    {
        if (layer_data.int32_data[i] < 0 || layer_data.int32_data[i] >= info->num_layers)
        {
            for (j = 0; j < info->num_layers; j++)
            {
                data.float_data[i * info->num_layers + j] = (float)harp_nan();
            }
        }
        else
        {
            for (j = 0; j < layer_data.int32_data[i]; j++)
            {
                data.float_data[i * info->num_layers + j] = 0;
            }
            for (j = layer_data.int32_data[i]; j < info->num_layers; j++)
            {
                data.float_data[i * info->num_layers + j] *= amf_data.float_data[i];
            }
        }
    }
    free(layer_data.int32_data);

    if (read_no2_column_stratospheric_amf(user_data, amf_data) != 0)
    {
        free(amf_data.float_data);
        return -1;
    }
    for (i = 0; i < info->num_scanlines * info->num_pixels; i++)
    {
        for (j = 0; j < info->num_layers; j++)
        {
            data.float_data[i * info->num_layers + j] /= amf_data.float_data[i];
        }
    }
    free(amf_data.float_data);

    return 0;
}

static int read_no2_column_tropospheric(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "tropospheric_no2_vertical_column", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_no2_column_tropospheric_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "tropospheric_no2_vertical_column_uncertainty", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_no2_column_tropospheric_amf(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "amf_trop", harp_type_float, info->num_scanlines * info->num_pixels,
                        data);
}

static int read_no2_column_tropospheric_avk(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    harp_array layer_data;
    harp_array amf_data;
    long i, j;

    if (read_dataset(info->product_cursor, "averaging_kernel", harp_type_float,
                     info->num_scanlines * info->num_pixels * info->num_layers, data) != 0)
    {
        return -1;
    }

    layer_data.int32_data = malloc(info->num_scanlines * info->num_pixels * sizeof(int32_t));
    if (layer_data.int32_data == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       info->num_scanlines * info->num_pixels * sizeof(int32_t), __FILE__, __LINE__);
        return -1;
    }
    if (read_dataset(info->product_cursor, "tm5_tropopause_layer_index", harp_type_int32,
                     info->num_scanlines * info->num_pixels, layer_data) != 0)
    {
        free(layer_data.int32_data);
        return -1;
    }

    amf_data.float_data = malloc(info->num_scanlines * info->num_pixels * sizeof(float));
    if (amf_data.float_data == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       info->num_scanlines * info->num_pixels * sizeof(float), __FILE__, __LINE__);
        free(layer_data.int32_data);
        return -1;
    }
    if (read_no2_column_amf(user_data, amf_data) != 0)
    {
        free(amf_data.float_data);
        free(layer_data.int32_data);
        return -1;
    }
    for (i = 0; i < info->num_scanlines * info->num_pixels; i++)
    {
        if (layer_data.int32_data[i] < 0 || layer_data.int32_data[i] >= info->num_layers)
        {
            for (j = 0; j < info->num_layers; j++)
            {
                data.float_data[i * info->num_layers + j] = (float)harp_nan();
            }
        }
        else
        {
            for (j = 0; j < layer_data.int32_data[i]; j++)
            {
                data.float_data[i * info->num_layers + j] *= amf_data.float_data[i];
            }
            for (j = layer_data.int32_data[i]; j < info->num_layers; j++)
            {
                data.float_data[i * info->num_layers + j] = 0;
            }
        }
    }
    free(layer_data.int32_data);

    if (read_no2_column_tropospheric_amf(user_data, amf_data) != 0)
    {
        free(amf_data.float_data);
        return -1;
    }
    for (i = 0; i < info->num_scanlines * info->num_pixels; i++)
    {
        for (j = 0; j < info->num_layers; j++)
        {
            data.float_data[i * info->num_layers + j] /= amf_data.float_data[i];
        }
    }
    free(amf_data.float_data);

    return 0;
}

static int include_cloud_fraction_uncertainty(void *user_data)
{
    return !((ingest_info *)user_data)->use_radiance_cloud_fraction;
}

static void register_common_variables(harp_product_definition *product_definition, int no2)
{
    const char *path;
    const char *description;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };
    harp_dimension_type bounds_dimension_type[2] = { harp_dimension_time, harp_dimension_independent };
    long bounds_dimension[2] = { -1, 4 };
    harp_dimension_type pressure_bounds_dimension_type[3] =
        { harp_dimension_time, harp_dimension_vertical, harp_dimension_independent };
    long pressure_bounds_dimension[3] = { -1, -1, 2 };

    /* scan_subindex */
    description = "pixel index (0-based) within the scanline";
    variable_definition =
        harp_ingestion_register_variable_block_read(product_definition, "scan_subindex", harp_type_int16, 1,
                                                    dimension_type, NULL, description, NULL, NULL, read_scan_subindex);
    description =
        "the scanline and pixel dimensions are collapsed into a temporal dimension; the index of the pixel within the "
        "scanline is computed as the index on the temporal dimension modulo the number of scanlines";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, NULL, description);

    /* datetime */
    description = "start time of the measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double, 1, dimension_type,
                                                   NULL, description, "seconds since 1995-01-01", NULL, read_datetime);
    path = "/PRODUCT/time, /PRODUCT/delta_time[]";
    description =
        "time converted from milliseconds since a reference time (with the reference time being 1995-01-01) to seconds "
        "since 1995-01-01; the time associated with a scanline is repeated for each pixel in the scanline";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* orbit_index */
    description = "absolute orbit number";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "orbit_index", harp_type_int32, 0, NULL, NULL,
                                                   description, NULL, NULL, read_orbit_index);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/@orbit", NULL);

    /* latitude */
    description = "latitude of the ground pixel center (WGS84)";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_float, 1,
                                                                     dimension_type, NULL, description, "degree_north",
                                                                     NULL, read_latitude);
    harp_variable_definition_set_valid_range_float(variable_definition, -90.0f, 90.0f);
    path = "/PRODUCT/latitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* longitude */
    description = "longitude of the ground pixel center (WGS84)";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_float,
                                                                     1, dimension_type, NULL, description,
                                                                     "degree_east", NULL, read_longitude);
    harp_variable_definition_set_valid_range_float(variable_definition, -180.0f, 180.0f);
    path = "/PRODUCT/longitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* latitude_bounds */
    description = "latitudes of the ground pixel corners (WGS84)";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "latitude_bounds",
                                                                     harp_type_float, 2, bounds_dimension_type,
                                                                     bounds_dimension, description, "degree_north",
                                                                     NULL, read_latitude_bounds);
    harp_variable_definition_set_valid_range_float(variable_definition, -90.0f, 90.0f);
    path = "/PRODUCT/SUPPORT_DATA/GEOLOCATIONS/latitude_bounds[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* longitude_bounds */
    description = "longitudes of the ground pixel corners (WGS84)";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "longitude_bounds",
                                                                     harp_type_float, 2, bounds_dimension_type,
                                                                     bounds_dimension, description, "degree_east",
                                                                     NULL, read_longitude_bounds);
    harp_variable_definition_set_valid_range_float(variable_definition, -180.0f, 180.0f);
    path = "/PRODUCT/SUPPORT_DATA/GEOLOCATIONS/longitude_bounds[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* solar_zenith_angle */
    description = "zenith angle of the Sun at the ground pixel location (WGS84); angle measured away from the vertical";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "solar_zenith_angle",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     description, "degree", NULL,
                                                                     read_solar_zenith_angle);
    harp_variable_definition_set_valid_range_float(variable_definition, 0.0f, 180.0f);
    path = "/PRODUCT/SUPPORT_DATA/GEOLOCATIONS/solar_zenith_angle[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* relative_azimuth_angle */
    description = "relative azimuth angle at the ground pixel location (WGS84); angle measured East-of-North";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "relative_azimuth_angle",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     description, "degree", NULL,
                                                                     read_relative_azimuth_angle);
    harp_variable_definition_set_valid_range_float(variable_definition, -180.0f, 180.0f);
    path = "/PRODUCT/SUPPORT_DATA/GEOLOCATIONS/relative_azimuth_angle[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* sensor_zenith_angle */
    description = "zenith angle of the satellite at the ground pixel location (WGS84); angle measured away from the "
        "vertical";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "sensor_zenith_angle",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     description, "degree", NULL,
                                                                     read_viewing_zenith_angle);
    harp_variable_definition_set_valid_range_float(variable_definition, 0.0f, 180.0f);
    path = "/PRODUCT/SUPPORT_DATA/GEOLOCATIONS/viewing_zenith_angle[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
    description = "pressure boundaries";

    /* surface_altitude */
    description = "surface altitude";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "surface_altitude",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     description, "m", NULL, read_surface_altitude);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/surface_altitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* surface_pressure */
    description = "surface pressure";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "surface_pressure",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     description, "hPa", NULL, read_surface_pressure);
    path = "/PRODUCT/tm5_surface_pressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* pressure_bounds */
    description = "pressure boundaries for each layer";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "pressure_bounds", harp_type_double, 3,
                                                   pressure_bounds_dimension_type, pressure_bounds_dimension,
                                                   description, "Pa", NULL, read_pressure_bounds);
    path = "/PRODUCT/tm5_pressure_level_a[], /PRODUCT/tm5_pressure_level_b[], /PRODUCT/tm5_surface_pressure[]";
    description = "pressure in Pa at level k is derived from surface pressure in hPa as: tm5_pressure_level_a[k] + "
        "tm5_pressure_level_b[k] * tm5_surface_pressure[] * 100.0; the TOA pressure is clamped to 1e-3 Pa";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* cloud_fraction */
    description = "cloud fraction";
    if (no2)
    {
        variable_definition = harp_ingestion_register_variable_full_read(product_definition, "cloud_fraction",
                                                                         harp_type_float, 1, dimension_type, NULL,
                                                                         description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                                         read_cloud_fraction_no2);
    }
    else
    {
        variable_definition = harp_ingestion_register_variable_full_read(product_definition, "cloud_fraction",
                                                                         harp_type_float, 1, dimension_type, NULL,
                                                                         description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                                         read_cloud_fraction_hcho);

    }
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_fraction[]";
    harp_variable_definition_add_mapping(variable_definition, "cloud_fraction unset", NULL, path, NULL);
    if (no2)
    {
        path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/cloud_radiance_fraction_no2[]";
    }
    else
    {
        path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/cloud_radiance_fraction_hcho[]";
    }
    harp_variable_definition_add_mapping(variable_definition, "cloud_fraction=radiance", NULL, path, NULL);

    /* cloud_fraction_uncertainty */
    description = "effective cloud fraction uncertainty";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "cloud_fraction_uncertainty",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     description, HARP_UNIT_DIMENSIONLESS,
                                                                     include_cloud_fraction_uncertainty,
                                                                     read_cloud_fraction_uncertainty);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_fraction_uncertainty[]";
    harp_variable_definition_add_mapping(variable_definition, "cloud_fraction unset", NULL, path, NULL);

    /* cloud_pressure */
    description = "cloud optical centroid pressure from the cloud product";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "cloud_pressure",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     description, "hPa", NULL, read_cloud_pressure);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_pressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_pressure_uncertainty */
    description = "cloud optical centroid pressure from the cloud product";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "cloud_pressure_uncertainty",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     description, "hPa", NULL,
                                                                     read_cloud_pressure_uncertainty);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_pressure_uncertainty[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* snow_ice_type */
    description = "surface snow/ice type";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "snow_ice_type", harp_type_int8, 1,
                                                   dimension_type, NULL, description, NULL, NULL, read_snow_ice_type);
    harp_variable_definition_set_enumeration_values(variable_definition, 5, snow_ice_type_values);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/snow_ice_flag[]";
    description = "0: snow_free_land (0), 1-100: sea_ice (1), 101: permanent_ice (2), 103: snow (3), 255: ocean (4), "
        "other values map to -1";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* sea_ice_fraction */
    description = "sea-ice concentration (as a fraction)";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "sea_ice_fraction", harp_type_float, 1,
                                                   dimension_type, NULL, description, "", NULL, read_sea_ice_fraction);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/snow_ice_flag[]";
    description = "if 1 <= snow_ice_flag <= 100 then snow_ice_flag/100.0 else 0.0";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);
}

static void register_hcho_product(void)
{
    const char *cloud_fraction_options[] = { "radiance" };
    const char *path;
    const char *description;
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_vertical };

    module = harp_ingestion_register_module_coda("QA4ECV_L2_HCHO", "QA4ECV", "QA4ECV", "L2_HCHO",
                                                 "QA4ECV L2 HCHO total column", ingestion_init, ingestion_done);

    harp_ingestion_register_option(module, "cloud_fraction", "whether to ingest the cloud fraction (default) or the "
                                   "radiance cloud fraction (cloud_fraction=radiance)", 1, cloud_fraction_options);

    product_definition = harp_ingestion_register_product(module, "QA4ECV_L2_HCHO", NULL, read_dimensions);
    register_common_variables(product_definition, 0);

    /* tropospheric_HCHO_column_number_density */
    description = "tropospheric vertical column of HCHO";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "tropospheric_HCHO_column_number_density",
                                                   harp_type_float, 1, dimension_type, NULL, description, "molec/cm^2",
                                                   NULL, read_hcho_column_tropospheric);
    path = "/PRODUCT/tropospheric_hcho_vertical_column[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* tropospheric_HCHO_column_number_density_uncertainty_random */
    description = "uncertainty of the tropospheric vertical column of HCHO due to random effects";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "tropospheric_HCHO_column_number_density_uncertainty_random",
                                                   harp_type_float, 1, dimension_type, NULL, description, "molec/cm^2",
                                                   NULL, read_hcho_column_tropospheric_uncertainty_random);
    path = "/PRODUCT/tropospheric_hcho_vertical_column_uncertainty_random[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* tropospheric_HCHO_column_number_density_uncertainty_systematic */
    description = "uncertainty of the tropospheric vertical column of HCHO due to systematic effects";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "tropospheric_HCHO_column_number_density_uncertainty_systematic",
                                                   harp_type_float, 1, dimension_type, NULL, description, "molec/cm^2",
                                                   NULL, read_hcho_column_tropospheric_uncertainty_systematic);
    path = "/PRODUCT/tropospheric_hcho_vertical_column_uncertainty_systematic[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* tropospheric_HCHO_column_number_density_amf */
    description = "tropospheric air mass factor";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "tropospheric_HCHO_column_number_density_amf",
                                                   harp_type_float, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL, read_hcho_column_tropospheric_amf);
    path = "/PRODUCT/amf_trop[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* HCHO_column_number_density_avk */
    description = "averaging kernel for the total column number density of tropospheric HCHO";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "HCHO_column_number_density_avk",
                                                   harp_type_float, 2, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL, read_hcho_column_avk);
    path = "/PRODUCT/averaging_kernel[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* HCHO_volume_mixing_ratio_dry_air_apriori */
    description = "apriori profile for the volume mixing ratio of tropospheric HCHO";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "HCHO_volume_mixing_ratio_dry_air_apriori",
                                                   harp_type_float, 2, dimension_type, NULL, description, "ppv", NULL,
                                                   read_hcho_vmr_apriori);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/hcho_profile_apriori[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* surface_albedo */
    description = "surface albedo in the HCHO fitting window";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "surface_albedo",
                                                   harp_type_float, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL, read_surface_albedo_hcho);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/surface_albedo_hcho[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* validity */
    description = "processing quality flag";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "validity", harp_type_int32, 1, dimension_type,
                                                   NULL, description, NULL, NULL, read_validity);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/processing_quality_flags[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

static void register_no2_product(void)
{
    const char *total_column_options[] = { "summed", "total" };
    const char *cloud_fraction_options[] = { "radiance" };
    const char *path;
    const char *description;
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_vertical };

    module = harp_ingestion_register_module_coda("QA4ECV_L2_NO2", "QA4ECV", "QA4ECV", "L2_NO2",
                                                 "QA4ECV NO2 tropospheric column", ingestion_init, ingestion_done);

    harp_ingestion_register_option(module, "total_column", "whether to use total_no2_vertical_column (which is "
                                   "derived from the total slant column diveded by the total amf) or "
                                   "summed_no2_total_vertical_column (which is the sum of the retrieved tropospheric "
                                   "and statospheric columns); option values are 'summed' (default) and 'total'", 2,
                                   total_column_options);

    harp_ingestion_register_option(module, "cloud_fraction", "whether to ingest the cloud fraction (default) or the "
                                   "radiance cloud fraction (cloud_fraction=radiance)", 1, cloud_fraction_options);

    product_definition = harp_ingestion_register_product(module, "QA4ECV_L2_NO2", NULL, read_dimensions);
    register_common_variables(product_definition, 1);

    /* tropopause_pressure */
    description = "tropopause pressure";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "tropopause_pressure",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, "Pa", NULL, read_tropopause_pressure);
    path = "/PRODUCT/tm5_pressure_level_a[], /PRODUCT/tm5_pressure_level_b[], /PRODUCT/tm5_surface_pressure[], "
        "/PRODUCT/tm5_tropopause_layer_index[]";
    description = "pressure in Pa at tropause is derived from the upper bound of the layer with tropopause layer index "
        "k: tm5_pressure_level_a[k + 1] + tm5_pressure_level_b[k + 1] * tm5_surface_pressure[] * 100.0";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* tropospheric_NO2_column_number_density */
    description = "tropospheric vertical column of NO2";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "tropospheric_NO2_column_number_density",
                                                   harp_type_float, 1, dimension_type, NULL, description, "molec/cm^2",
                                                   NULL, read_no2_column_tropospheric);
    path = "/PRODUCT/tropospheric_no2_vertical_column[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* tropospheric_NO2_column_number_density_uncertainty */
    description = "uncertainty of the tropospheric vertical column of NO2 (standard error)";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "tropospheric_NO2_column_number_density_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description, "molec/cm^2",
                                                   NULL, read_no2_column_tropospheric_uncertainty);
    path = "/PRODUCT/tropospheric_no2_vertical_column_uncertainty[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* tropospheric_NO2_column_number_density_avk */
    description = "averaging kernel for the tropospheric vertical column number density of NO2";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "tropospheric_NO2_column_number_density_avk",
                                                   harp_type_float, 2, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL, read_no2_column_tropospheric_avk);
    path = "/PRODUCT/averaging_kernel[], /PRODUCT/amf_total[], /PRODUCT/amf_trop[], "
        "/PRODUCT/tm5_tropopause_layer_index[]";
    description = "averaging_kernel[layer] = if layer <= tm5_tropopause_layer_index then "
        "averaging_kernel[layer] * amf_total / amf_trop else 0";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* tropospheric_NO2_column_number_density_amf */
    description = "tropospheric air mass factor, computed by integrating the altitude dependent air mass factor over "
        "the atmospheric layers from the surface up to and including the layer with the tropopause";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "tropospheric_NO2_column_number_density_amf",
                                                   harp_type_float, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL, read_no2_column_tropospheric_amf);
    path = "/PRODUCT/amf_trop[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* stratospheric_NO2_column_number_density */
    description = "stratospheric vertical column of NO2";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "stratospheric_NO2_column_number_density",
                                                   harp_type_float, 1, dimension_type, NULL, description, "molec/cm^2",
                                                   NULL, read_no2_column_stratospheric);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/stratospheric_no2_vertical_column[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* stratospheric_NO2_column_number_density_uncertainty */
    description = "uncertainty of the stratospheric vertical column of NO2 (standard error)";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "stratospheric_NO2_column_number_density_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description, "molec/cm^2",
                                                   NULL, read_no2_column_stratospheric_uncertainty);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/stratospheric_no2_vertical_column_uncertainty[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* stratospheric_NO2_column_number_density_avk */
    description = "averaging kernel for the stratospheric vertical column number density of NO2";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "stratospheric_NO2_column_number_density_avk",
                                                   harp_type_float, 2, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL, read_no2_column_stratospheric_avk);
    path = "/PRODUCT/averaging_kernel[], /PRODUCT/amf_total[], /PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/amf_strat[], "
        "/PRODUCT/tm5_tropopause_layer_index[]";
    description = "averaging_kernel[layer] = if layer > tm5_tropopause_layer_index then "
        "averaging_kernel[layer] * amf_total / amf_strat else 0";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* stratospheric_NO2_column_number_density_amf */
    description = "stratospheric air mass factor";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "stratospheric_NO2_column_number_density_amf",
                                                   harp_type_float, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL, read_no2_column_stratospheric_amf);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/amf_strat[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* NO2_column_number_density */
    description = "total vertical column of NO2 (ratio of the slant column density of NO2 and the total air mass "
        "factor)";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "NO2_column_number_density", harp_type_float, 1,
                                                   dimension_type, NULL, description, "molec/cm^2", NULL,
                                                   read_no2_column);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/summed_no2_total_vertical_column[]";
    harp_variable_definition_add_mapping(variable_definition, "total_column=summed or total_column unset", NULL, path,
                                         NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/total_no2_vertical_column[]";
    harp_variable_definition_add_mapping(variable_definition, "total_column=total", NULL, path, NULL);

    /* NO2_column_number_density_uncertainty */
    description = "uncertainty of the total vertical column of NO2 (standard error)";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "NO2_column_number_density_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description, "molec/cm^2",
                                                   NULL, read_no2_column_uncertainty);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/summed_no2_total_vertical_column_uncertainty[]";
    harp_variable_definition_add_mapping(variable_definition, "total_column=summed  or total_column unset", NULL, path,
                                         NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/total_no2_vertical_column_uncertainty[]";
    harp_variable_definition_add_mapping(variable_definition, "total_column=total", NULL, path, NULL);

    /* NO2_column_number_density_amf */
    description = "total air mass factor, computed by integrating the altitude dependent air mass factor over the "
        "atmospheric layers from the surface to top-of-atmosphere";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "NO2_column_number_density_amf", harp_type_float,
                                                   1, dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_no2_column_amf);
    path = "/PRODUCT/amf_total[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* NO2_column_number_density_avk */
    description = "averaging kernel for the total column number density of NO2";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "NO2_column_number_density_avk", harp_type_float,
                                                   2, dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_no2_column_avk);
    path = "/PRODUCT/averaging_kernel[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* surface_albedo */
    description = "surface albedo in the NO2 fitting window";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "surface_albedo",
                                                   harp_type_float, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL, read_surface_albedo_no2);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/surface_albedo_no2[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* validity */
    description = "processing quality flag";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "validity", harp_type_int32, 1, dimension_type,
                                                   NULL, description, NULL, NULL, read_validity);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/processing_quality_flags[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

int harp_ingestion_module_qa4ecv_init(void)
{
    register_hcho_product();
    register_no2_product();

    return 0;
}
