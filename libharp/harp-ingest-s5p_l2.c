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

typedef enum s5p_product_type_enum
{
    s5p_type_o3_pr,
    s5p_type_o3_tpr,
    s5p_type_no2,
    s5p_type_co,
    s5p_type_ch4,
    s5p_type_aer_lh,
    s5p_type_aer_ai,
    s5p_type_cloud,
    s5p_type_fresco,
    s5p_type_so2,
    s5p_type_o3,
    s5p_type_hcho
} s5p_product_type;

#define S5P_NUM_PRODUCT_TYPES (((int)s5p_type_hcho) + 1)

typedef enum s5p_dimension_type_enum
{
    s5p_dim_time,
    s5p_dim_scanline,
    s5p_dim_pixel,
    s5p_dim_corner,
    s5p_dim_layer,
    s5p_dim_level
} s5p_dimension_type;

#define S5P_NUM_DIM_TYPES (((int)s5p_dim_level) + 1)

static const char *s5p_dimension_name[S5P_NUM_PRODUCT_TYPES][S5P_NUM_DIM_TYPES] = {
    {"time", "scanline", "ground_pixel", "corner", NULL, "level"},
    {"time", "scanline", "ground_pixel", "corner", NULL, NULL},
    {"time", "scanline", "ground_pixel", "corner", "profile_layers", "pressure_levels"},
    {"time", "scanline", "ground_pixel", "corner", "layer", NULL},
    {"time", "scanline", "ground_pixel", "corner", "layer", "level"},
    {"time", "scanline", "ground_pixel", "corner", "layer", "level"},
    {"time", "scanline", "ground_pixel", "corner", NULL, NULL},
    {"time", "scanline", "ground_pixel", "corner", NULL, NULL},
    {"time", "scanline", "ground_pixel", "corner", NULL, NULL},
    {"time", "scanline", "ground_pixel", "corner", "layers", NULL},
    {"time", "scanline", "ground_pixel", "corner", "layers", "layer_boundaries"},
    {"time", "scanline", "ground_pixel", "corner", "layers", NULL}
};

typedef struct ingest_info_struct
{
    coda_product *product;

    s5p_product_type product_type;
    long num_times;
    long num_scanlines;
    long num_pixels;
    long num_corners;
    long num_layers;
    long num_levels;

    coda_cursor product_cursor;
    coda_cursor geolocation_cursor;
    coda_cursor detailed_results_cursor;
    coda_cursor input_data_cursor;

    int wavelength_ratio;
} ingest_info;

static void broadcast_array_float(long num_scanlines, long num_pixels, float *data)
{
    long i;

    /* Repeat the value for each scanline for all pixels in that scanline. Iterate in reverse to avoid overwriting
     * scanline values.
     */
    for (i = num_scanlines - 1; i >= 0; --i)
    {
        float *pixel = data + i * num_pixels;
        float *pixel_end = pixel + num_pixels;
        const float scanline_value = data[i];

        for (; pixel != pixel_end; pixel++)
        {
            *pixel = scanline_value;
        }
    }
}

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

static const char *get_product_type_name(s5p_product_type product_type)
{
    switch (product_type)
    {
        case s5p_type_o3_pr:
            return "L2__O3__PR";
        case s5p_type_o3_tpr:
            return "L2__O3_TPR";
        case s5p_type_no2:
            return "L2__NO2___";
        case s5p_type_co:
            return "L2__CO____";
        case s5p_type_ch4:
            return "L2__CH4___";
        case s5p_type_aer_lh:
            return "L2__AER_LH";
        case s5p_type_aer_ai:
            return "L2__AER_AI";
        case s5p_type_cloud:
            return "L2__CLOUD_";
        case s5p_type_fresco:
            return "L2__FRESCO";
        case s5p_type_so2:
            return "L2__SO2____";
        case s5p_type_o3:
            return "L2__O3____";
        case s5p_type_hcho:
            return "L2__HCHO__";
    }

    assert(0);
    exit(1);
}

static int strendswith(const char *str, const char *suffix)
{
    size_t str_length;
    size_t suffix_length;

    str_length = strlen(str);
    suffix_length = strlen(suffix);

    if (str_length < suffix_length)
    {
        return 0;
    }

    return (strcmp(str + str_length - suffix_length, suffix) == 0);
}

static int get_product_type(coda_product *product, s5p_product_type *product_type)
{
    coda_cursor cursor;
    char product_short_name[20];
    long length;
    int i;

    if (coda_cursor_set_product(&cursor, product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, "/METADATA/GRANULE_DESCRIPTION@ProductShortName") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_string_length(&cursor, &length) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (length > 19)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_string(&cursor, product_short_name, 20) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    for (i = 0; i < S5P_NUM_PRODUCT_TYPES; i++)
    {
        if (strendswith(product_short_name, get_product_type_name((s5p_product_type)i)))
        {
            *product_type = ((s5p_product_type)i);
            return 0;
        }
    }

    harp_set_error(HARP_ERROR_INGESTION, "unsupported product type '%s'", product_short_name);

    return -1;
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

    if (info->product_type == s5p_type_fresco)
    {
        if (coda_cursor_goto_record_field_by_name(&cursor, "GEOLOCATION") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
    }
    else
    {
        if (coda_cursor_goto_record_field_by_name(&cursor, "GEOLOCATIONS") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
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
    if (s5p_dimension_name[info->product_type][s5p_dim_time] != NULL)
    {
        if (get_dimension_length(info, s5p_dimension_name[info->product_type][s5p_dim_time], &info->num_times) != 0)
        {
            return -1;
        }
    }

    if (s5p_dimension_name[info->product_type][s5p_dim_scanline] != NULL)
    {
        if (get_dimension_length(info, s5p_dimension_name[info->product_type][s5p_dim_scanline],
                                 &info->num_scanlines) != 0)
        {
            return -1;
        }
    }

    if (s5p_dimension_name[info->product_type][s5p_dim_pixel] != NULL)
    {
        if (get_dimension_length(info, s5p_dimension_name[info->product_type][s5p_dim_pixel], &info->num_pixels) != 0)
        {
            return -1;
        }
    }

    if (s5p_dimension_name[info->product_type][s5p_dim_corner] != NULL)
    {
        if (get_dimension_length(info, s5p_dimension_name[info->product_type][s5p_dim_corner], &info->num_corners) != 0)
        {
            return -1;
        }
    }

    if (s5p_dimension_name[info->product_type][s5p_dim_layer] != NULL)
    {
        if (get_dimension_length(info, s5p_dimension_name[info->product_type][s5p_dim_layer], &info->num_layers) != 0)
        {
            return -1;
        }
    }

    if (s5p_dimension_name[info->product_type][s5p_dim_level] != NULL)
    {
        if (get_dimension_length(info, s5p_dimension_name[info->product_type][s5p_dim_level], &info->num_levels) != 0)
        {
            return -1;
        }
    }

    if (info->num_times != 1)
    {
        harp_set_error(HARP_ERROR_INGESTION, "dimension '%s' has length %ld; expected 1",
                       s5p_dimension_name[info->product_type][s5p_dim_time], info->num_times);
        return -1;
    }

    if (info->num_corners != 4)
    {
        harp_set_error(HARP_ERROR_INGESTION, "dimension '%s' has length %ld; expected 4",
                       s5p_dimension_name[info->product_type][s5p_dim_corner], info->num_corners);
        return -1;
    }

    if (info->num_layers > 0 && info->num_levels > 0)
    {
        if (info->num_levels != info->num_layers + 1)
        {
            harp_set_error(HARP_ERROR_INGESTION, "dimension '%s' has length %ld; expected %ld",
                           s5p_dimension_name[info->product_type][s5p_dim_level], info->num_levels,
                           info->num_layers + 1);
            return -1;
        }
    }
    else if (info->num_layers > 0)
    {
        info->num_levels = info->num_layers + 1;
    }
    else if (info->num_levels > 0)
    {
        if (info->num_levels < 2)
        {
            harp_set_error(HARP_ERROR_INGESTION, "dimension '%s' has length %ld; expected >= 2",
                           s5p_dimension_name[info->product_type][s5p_dim_level], info->num_levels);
            return -1;
        }

        info->num_layers = info->num_levels - 1;
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
    info->num_levels = 0;
    info->wavelength_ratio = 354;

    if (get_product_type(info->product, &info->product_type) != 0)
    {
        ingestion_done(info);
        return -1;
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
            break;
        default:
            assert(0);
            exit(1);
    }

    /* Replace values equal to the _FillValue variable attribute by NaN. */
    harp_array_replace_fill_value(data_type, num_elements, data, fill_value);

    return 0;
}

static int read_dimensions(void *user_data, long dimension[HARP_NUM_DIM_TYPES])
{
    ingest_info *info = (ingest_info *)user_data;

    dimension[harp_dimension_time] = info->num_times * info->num_scanlines * info->num_pixels;
    switch (info->product_type)
    {
        case s5p_type_no2:
        case s5p_type_co:
        case s5p_type_ch4:
        case s5p_type_o3:
        case s5p_type_so2:
        case s5p_type_hcho:
            dimension[harp_dimension_vertical] = info->num_layers;
            break;
        case s5p_type_o3_pr:
        case s5p_type_o3_tpr:
            dimension[harp_dimension_vertical] = info->num_levels;
            break;
        case s5p_type_aer_lh:
        case s5p_type_aer_ai:
        case s5p_type_cloud:
        case s5p_type_fresco:
            break;
        default:
            assert(0);
            exit(1);
    }

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

static int read_sensor_longitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (read_dataset(info->geolocation_cursor, "satellite_longitude", harp_type_float, info->num_scanlines, data) != 0)
    {
        return -1;
    }

    broadcast_array_float(info->num_scanlines, info->num_pixels, data.float_data);

    return 0;
}

static int read_sensor_latitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (read_dataset(info->geolocation_cursor, "satellite_latitude", harp_type_float, info->num_scanlines, data) != 0)
    {
        return -1;
    }

    broadcast_array_float(info->num_scanlines, info->num_pixels, data.float_data);

    return 0;
}

static int read_sensor_altitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (read_dataset(info->geolocation_cursor, "satellite_altitude", harp_type_float, info->num_scanlines, data) != 0)
    {
        return -1;
    }

    broadcast_array_float(info->num_scanlines, info->num_pixels, data.float_data);

    return 0;
}

static int read_solar_azimuth_angle(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->geolocation_cursor, "solar_azimuth_angle", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_solar_zenith_angle(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->geolocation_cursor, "solar_zenith_angle", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_viewing_azimuth_angle(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->geolocation_cursor, "viewing_azimuth_angle", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_viewing_zenith_angle(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->geolocation_cursor, "viewing_zenith_angle", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_cloud_fraction(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "cloud_radiometric_fraction", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_cloud_fraction_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "cloud_radiometric_fraction_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_cloud_top_pressure(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "cloud_top_pressure", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_cloud_top_pressure_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "cloud_top_pressure_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_cloud_top_height(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "cloud_top_height", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_cloud_top_height_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "cloud_top_height_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_cloud_optical_thickness(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "cloud_optical_thickness", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_cloud_optical_thickness_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "cloud_optical_thickness_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_fresco_cloud_fraction(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "cloud_fraction", harp_type_float, info->num_scanlines * info->num_pixels,
                        data);
}

static int read_fresco_cloud_fraction_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "cloud_fraction_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_fresco_cloud_pressure(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "cloud_pressure", harp_type_float, info->num_scanlines * info->num_pixels,
                        data);
}

static int read_fresco_cloud_pressure_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "cloud_pressure_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_fresco_cloud_height(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "cloud_height", harp_type_float, info->num_scanlines * info->num_pixels,
                        data);
}

static int read_fresco_cloud_height_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "cloud_height_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_fresco_cloud_albedo(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "cloud_albedo", harp_type_float, info->num_scanlines * info->num_pixels,
                        data);
}

static int read_fresco_cloud_albedo_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "cloud_albedo_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_o3_pr_volume_mixing_ratio(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "O3", harp_type_float,
                        info->num_scanlines * info->num_pixels * info->num_levels, data);
}

static int read_o3_pr_volume_mixing_ratio_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "O3_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels * info->num_levels, data);
}

static int read_o3_pr_volume_mixing_ratio_apriori(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "O3_apriori", harp_type_float,
                        info->num_scanlines * info->num_pixels * info->num_levels, data);
}

static int read_o3_pr_volume_mixing_ratio_apriori_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long num_elements = info->num_scanlines * info->num_pixels * info->num_levels;
    long i;

    if (read_dataset(info->input_data_cursor, "O3_apriori_error_covariance_matrix", harp_type_float, num_elements, data)
        != 0)
    {
        return -1;
    }

    for (i = 0; i < num_elements; i++)
    {
        data.float_data[i] = sqrtf(data.float_data[i]);
    }

    return 0;
}

static int read_o3_pr_volume_mixing_ratio_avk(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "averaging_kernel", harp_type_float,
                        info->num_scanlines * info->num_pixels * info->num_levels * info->num_levels, data);
}

static int read_o3_pr_volume_mixing_ratio_covariance(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "O3_error_covariance_matrix", harp_type_float,
                        info->num_scanlines * info->num_pixels * info->num_levels * info->num_levels, data);
}

static int read_o3_pr_column_number_density(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "O3_total_column", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_o3_pr_column_number_density_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "O3_total_column_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_o3_pr_tropospheric_column_number_density(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "O3_tropospheric_column", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_o3_pr_tropospheric_column_number_density_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "O3_tropospheric_column_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_o3_pr_pressure(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "pressure", harp_type_float,
                        info->num_scanlines * info->num_pixels * info->num_levels, data);
}

static int read_o3_pr_altitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "altitude", harp_type_float,
                        info->num_scanlines * info->num_pixels * info->num_levels, data);
}

static int read_o3_pr_temperature(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "temperature", harp_type_float,
                        info->num_scanlines * info->num_pixels * info->num_levels, data);
}

static int read_no2_pressure_bounds(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    harp_array hybride_coef_a;
    harp_array hybride_coef_b;
    long num_profiles;
    long num_layers;
    long num_levels;
    long i;

    num_profiles = info->num_scanlines * info->num_pixels;
    num_layers = info->num_layers;
    num_levels = info->num_levels;

    /* The air pressure boundaries are interpolated from the position dependent surface air pressure (/Psurf[]) using a
     * position independent set of coefficients (/Hybride_coef_a[], /Hybride_coef_b[]).
     */
    hybride_coef_a.ptr = malloc(num_levels * sizeof(double));
    if (hybride_coef_a.ptr == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_levels * sizeof(double), __FILE__, __LINE__);
        return -1;
    }

    hybride_coef_b.ptr = malloc(num_levels * sizeof(double));
    if (hybride_coef_b.ptr == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_levels * sizeof(double), __FILE__, __LINE__);
        free(hybride_coef_a.ptr);
        return -1;
    }

    if (read_dataset(info->product_cursor, "tm5_pressure_level_a", harp_type_double, num_levels, hybride_coef_a) != 0)
    {
        free(hybride_coef_b.ptr);
        free(hybride_coef_a.ptr);
        return -1;
    }

    if (read_dataset(info->product_cursor, "tm5_pressure_level_b", harp_type_double, num_levels, hybride_coef_b) != 0)
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
        double surface_pressure = data.double_data[i] * 100.0;  /* surface pressure at specific (time, lat, lon) in
                                                                 * Pa (converted from hPa).
                                                                 */
        long j;

        bounds[(num_layers - 1) * 2 + 1] =
            hybride_coef_a.double_data[num_layers] + hybride_coef_b.double_data[num_layers] * surface_pressure;

        for (j = num_layers - 1; j > 0; j--)
        {
            bounds[j * 2] = bounds[(j - 1) * 2 + 1] =
                hybride_coef_a.double_data[j] + hybride_coef_b.double_data[j] * surface_pressure;
        }

        bounds[0] = hybride_coef_a.double_data[0] + hybride_coef_b.double_data[0] * surface_pressure;
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

static int read_co_altitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "layer", harp_type_float, info->num_layers, data);
}

static int read_co_pressure(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "pressure_levels", harp_type_float,
                        info->num_scanlines * info->num_pixels * info->num_layers, data);
}

static int read_co_column(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "CO_total_vertical_column", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_co_column_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "CO_total_vertical_column_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_co_column_avk(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "column_averaging_kernel", harp_type_float,
                        info->num_scanlines * info->num_pixels * info->num_layers, data);
}

static int read_so2_column(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "so2_vertical_column", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_so2_column_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "so2_vertical_column_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_so2_column_avk(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "averaging_kernels", harp_type_float,
                        info->num_scanlines * info->num_pixels * info->num_layers, data);
}

static int read_o3_pressure_bounds(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long num_layers;
    long i;

    if (read_dataset
        (info->detailed_results_cursor, "pressure_grid", harp_type_float,
         info->num_scanlines * info->num_pixels * info->num_levels, data) != 0)
    {
        return -1;
    }

    /* Convert from #levels (== #layers + 1) consecutive pressures to #layers x 2 pressure bounds. Iterate in reverse to
     * ensure correct results (conversion is performed in place).
     */
    num_layers = info->num_layers;
    assert((num_layers + 1) == info->num_levels);

    for (i = info->num_scanlines * info->num_pixels - 1; i >= 0; --i)
    {
        float *pressure = &data.float_data[i * (num_layers + 1)];
        float *pressure_bounds = &data.float_data[i * num_layers * 2];
        long j;

        for (j = num_layers - 1; j >= 0; --j)
        {
            /* NB. The order of the following two lines is important to ensure correct results. */
            pressure_bounds[j * 2 + 1] = pressure[j + 1];
            pressure_bounds[j * 2] = pressure[j];
        }
    }

    return 0;
}

static int read_o3_column(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "o3", harp_type_float, info->num_scanlines * info->num_pixels, data);
}

static int read_o3_column_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "o3_precision", harp_type_float, info->num_scanlines * info->num_pixels,
                        data);
}

static int read_o3_column_apriori(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "o3_profile_apriori", harp_type_float,
                        info->num_scanlines * info->num_pixels * info->num_layers, data);
}

static int read_o3_column_avk(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "averaging_kernels", harp_type_float,
                        info->num_scanlines * info->num_pixels * info->num_layers, data);
}

static int read_hcho_column(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "hcho_vertical_column", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_hcho_column_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "hcho_vertical_column_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_hcho_column_avk(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "averaging_kernels", harp_type_float,
                        info->num_scanlines * info->num_pixels * info->num_layers, data);
}

static int read_hcho_column_apriori(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "hcho_profile_apriori", harp_type_float,
                        info->num_scanlines * info->num_pixels * info->num_layers, data);
}

static int read_ch4_altitude_bounds(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long num_layers;
    long i;

    if (read_dataset
        (info->input_data_cursor, "height_levels", harp_type_float,
         info->num_scanlines * info->num_pixels * info->num_levels, data) != 0)
    {
        return -1;
    }

    /* Convert from #levels (== #layers + 1) consecutive altitudes to #layers x 2 altitude bounds. Iterate in reverse to
     * ensure correct results (conversion is performed in place).
     */
    num_layers = info->num_layers;
    assert((num_layers + 1) == info->num_levels);

    for (i = info->num_scanlines * info->num_pixels - 1; i >= 0; --i)
    {
        float *altitude = &data.float_data[i * (num_layers + 1)];
        float *altitude_bounds = &data.float_data[i * num_layers * 2];
        long j;

        for (j = num_layers - 1; j >= 0; --j)
        {
            /* NB. The order of the following two lines is important to ensure correct results. */
            altitude_bounds[j * 2 + 1] = altitude[j + 1];
            altitude_bounds[j * 2] = altitude[j];
        }
    }

    return 0;
}

static int read_ch4_pressure_bounds(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    harp_array delta_pressure;
    long num_elements;
    long num_layers;
    long i;

    /* Total number of samples (i.e. length of the time axis of the ingested product). */
    num_elements = info->num_times * info->num_scanlines * info->num_pixels;

    /* Number of profile layers. */
    num_layers = info->num_layers;

    /* Pressure is stored in the product as the combination of surface pressure and the pressure difference between
     * retrieval levels. To minimize the amount of auxiliary storage required, the surface pressure data is read into
     * the output buffer and auxiliary storage is only allocated for the pressure difference data only.
     *
     * NB. Although the output buffer has enough space to storage both the surface pressure data and the pressure
     * difference data, correct in-place conversion to pressure bounds is not trivial in that scenario. Performing the
     * conversion back to front does not work in general (for example, consider the case where #layers == 1).
     *
     * An approach would be to first interleave the surface pressure and pressure difference data, and then perform the
     * conversion back to front. However, interleaving is equivalent to the in-place transposition of an 2 x M matrix,
     * and this is a non-trivial operation.
     *
     * If we could assume that #layers > 1, that provides enough extra space in the output buffer to perform the
     * transposition in a trivial way.
     */
    if (read_dataset(info->input_data_cursor, "surface_pressure", harp_type_double, num_elements, data) != 0)
    {
        return -1;
    }

    /* Allocate auxiliary storage for the pressure difference data. */
    delta_pressure.ptr = malloc(num_elements * sizeof(double));
    if (delta_pressure.ptr == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_elements * sizeof(double), __FILE__, __LINE__);
        return -1;
    }

    if (read_dataset(info->input_data_cursor, "dp", harp_type_double, num_elements, delta_pressure) != 0)
    {
        free(delta_pressure.ptr);
        return -1;
    }

    /* Convert from surface pressure and pressure difference to #layers x 2 pressure bounds. The pressure levels are
     * equidistant, separated by the pressure difference. Iterate in reverse to ensure correct results (the conversion
     * is performed in place).
     *
     * NB. The pressure differences provided in the product seem to be positive, yet pressure decreases with increasing
     * altitude. Therefore, the pressure differences read from the product are subtracted from (instead of added to) the
     * surface pressure.
     */
    for (i = num_elements - 1; i >= 0; --i)
    {
        double *pressure_bounds = &data.double_data[i * num_layers * 2];
        double surface_pressure = data.double_data[i];
        double delta = delta_pressure.double_data[i];
        long j;

        for (j = num_layers - 1; j >= 0; --j)
        {
            pressure_bounds[j * 2 + 1] = surface_pressure - (j + 1) * delta;
            pressure_bounds[j * 2] = surface_pressure - j * delta;
        }
    }

    free(delta_pressure.ptr);

    return 0;
}

static int read_ch4_column(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "XCH4", harp_type_float, info->num_scanlines * info->num_pixels, data);
}

static int read_ch4_column_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "XCH4_precision", harp_type_float, info->num_scanlines * info->num_pixels,
                        data);
}

static int read_ch4_column_avk(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "column_averaging_kernel", harp_type_float,
                        info->num_scanlines * info->num_pixels * info->num_layers, data);
}

static int read_aerosol_index(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    const char *variable_name = NULL;

    switch (info->wavelength_ratio)
    {
        case 354:
            variable_name = "aerosol_index_354_388";
            break;
        case 340:
            variable_name = "aerosol_index_340_380";
            break;
        default:
            assert(0);
            exit(1);
    }

    return read_dataset(info->product_cursor, variable_name, harp_type_float, info->num_scanlines * info->num_pixels,
                        data);
}

static int read_aerosol_index_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    const char *variable_name = NULL;

    switch (info->wavelength_ratio)
    {
        case 354:
            variable_name = "aerosol_index_354_388_precision";
            break;
        case 340:
            variable_name = "aerosol_index_340_380_precision";
            break;
        default:
            assert(0);
            exit(1);
    }

    return read_dataset(info->product_cursor, variable_name, harp_type_float, info->num_scanlines * info->num_pixels,
                        data);
}

static int parse_option_wavelength_ratio(ingest_info *info, const harp_ingestion_options *options)
{
    const char *value;

    if (harp_ingestion_options_get_option(options, "wavelength_ratio", &value) == 0)
    {
        if (strcmp(value, "354_388nm") == 0)
        {
            info->wavelength_ratio = 354;
        }
        else
        {
            /* Option values are guaranteed to be legal if present. */
            assert(strcmp(value, "340_380nm") == 0);
            info->wavelength_ratio = 340;
        }
    }

    return 0;
}

static int ingestion_init_aer_ai(const harp_ingestion_module *module, coda_product *product,
                                 const harp_ingestion_options *options, harp_product_definition **definition,
                                 void **user_data)
{
    ingest_info *info;
    harp_product_definition *tmp_definition;

    if (ingestion_init(module, product, options, &tmp_definition, (void **)&info) != 0)
    {
        return -1;
    }

    if (parse_option_wavelength_ratio(info, options) != 0)
    {
        ingestion_done((void *)info);
        return -1;
    }

    *user_data = (void *)info;
    *definition = tmp_definition;

    return 0;
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

    description = "longitude of the goedetic sub-satellite point (WGS84)";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "sensor_longitude",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     description, "degree_east", NULL,
                                                                     read_sensor_longitude);
    harp_variable_definition_set_valid_range_float(variable_definition, -180.0f, 180.0f);
    path = "/PRODUCT/SUPPORT_DATA/GEOLOCATIONS/satellite_longitude[]";
    description = "the satellite longitude associated with a scanline is repeated for each pixel in the scanline";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    description = "latitude of the geodetic sub-satellite point (WGS84)";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "sensor_latitude",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     description, "degree_north", NULL,
                                                                     read_sensor_latitude);
    harp_variable_definition_set_valid_range_float(variable_definition, -90.0f, 90.0f);
    path = "/PRODUCT/SUPPORT_DATA/GEOLOCATIONS/satellite_latitude[]";
    description = "the satellite latitude associated with a scanline is repeated for each pixel in the scanline";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    description = "altitude of the satellite with respect to the geodetic sub-satellite point (WGS84)";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "sensor_altitude",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     description, "m", NULL, read_sensor_altitude);
    harp_variable_definition_set_valid_range_float(variable_definition, 700000.0f, 900000.0f);
    path = "/PRODUCT/SUPPORT_DATA/GEOLOCATIONS/satellite_altitude[]";
    description = "the satellite altitude associated with a scanline is repeated for each pixel in the scanline";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* Angles. */
    description = "zenith angle of the Sun at the ground pixel location (WGS84); angle measured away from the vertical";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "solar_zenith_angle",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     description, "degree", NULL,
                                                                     read_solar_zenith_angle);
    harp_variable_definition_set_valid_range_float(variable_definition, 0.0f, 180.0f);
    path = "/PRODUCT/SUPPORT_DATA/GEOLOCATIONS/solar_zenith_angle[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "azimuth angle of the Sun at the ground pixel location (WGS84); angle measured East-of-North";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "solar_azimuth_angle",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     description, "degree", NULL,
                                                                     read_solar_azimuth_angle);
    harp_variable_definition_set_valid_range_float(variable_definition, -180.0f, 180.0f);
    path = "/PRODUCT/SUPPORT_DATA/GEOLOCATIONS/solar_azimuth_angle[]";
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

    description = "azimuth angle of the satellite at the ground pixel location (WGS84); angle measured East-of-North";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "sensor_azimuth_angle",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     description, "degree", NULL,
                                                                     read_viewing_azimuth_angle);
    harp_variable_definition_set_valid_range_float(variable_definition, -180.0f, 180.0f);
    path = "/PRODUCT/SUPPORT_DATA/GEOLOCATIONS/viewing_azimuth_angle[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

static void register_aer_ai_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };
    const char *wavelength_ratio_option_values[2] = { "354_388nm", "340_380nm" };
    const char *description;

    module = harp_ingestion_register_module_coda("S5P_L2_AER_AI", "Sentinel-5P", "Sentinel5P", "L2__AER_AI",
                                                 "Sentinel-5P L2 aerosol index", ingestion_init_aer_ai, ingestion_done);

    description = "ingest aerosol index retrieved at wavelengths 354/388 nm, or 340/388 nm";
    harp_ingestion_register_option(module, "wavelength_ratio", description, 2, wavelength_ratio_option_values);

    product_definition = harp_ingestion_register_product(module, "S5P_L2_AER_AI", NULL, read_dimensions);
    register_core_variables(product_definition);
    register_geolocation_variables(product_definition);
    register_additional_geolocation_variables(product_definition);

    description = "aerosol index";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "aerosol_index", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_aerosol_index);
    harp_variable_definition_add_mapping(variable_definition, "wavelength_ratio=354_388nm (default)", NULL,
                                         "/PRODUCT/aerosol_index_354_388", NULL);
    harp_variable_definition_add_mapping(variable_definition, "wavelength_ratio=340_380nm", NULL,
                                         "/PRODUCT/aerosol_index_340_380", NULL);

    description = "uncertainty of the aerosol index";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "aerosol_index_uncertainty", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_aerosol_index_uncertainty);
    harp_variable_definition_add_mapping(variable_definition, "wavelength_ratio=354_388nm (default)", NULL,
                                         "/PRODUCT/aerosol_index_354_388_precision", NULL);
    harp_variable_definition_add_mapping(variable_definition, "wavelength_ratio=340_380nm", NULL,
                                         "/PRODUCT/aerosol_index_340_380_precision", NULL);
}

#if 0
static void register_aer_lh_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };

    module = harp_ingestion_register_module_coda("S5P_L2_AER_LH", "Sentinel-5P", "Sentinel5P", "L2__AER_LH",
                                                 "Sentinel-5P L2 aerosol layer height", ingestion_init, ingestion_done);

    product_definition = harp_ingestion_register_product(module, "S5P_L2_AER_LH", NULL, read_dimensions);
    register_core_variables(product_definition);
    register_geolocation_variables(product_definition);
    register_additional_geolocation_variables(product_definition);
}
#endif

static void register_ch4_product(void)
{
    const char *path;
    const char *description;
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;

    harp_dimension_type dimension_type[3] = { harp_dimension_time, harp_dimension_vertical,
        harp_dimension_independent
    };
    long dimension[3] = { -1, -1, 2 };

    module = harp_ingestion_register_module_coda("S5P_L2_CH4", "Sentinel-5P", "Sentinel5P", "L2__CH4___",
                                                 "Sentinel-5P L2 CH4 total column", ingestion_init, ingestion_done);

    product_definition = harp_ingestion_register_product(module, "S5P_L2_CH4", NULL, read_dimensions);
    register_core_variables(product_definition);
    register_geolocation_variables(product_definition);
    register_additional_geolocation_variables(product_definition);

    description = "altitude bounds per profile layer; altitude is measured as the vertical distance to the surface";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "altitude_bounds_surface", harp_type_float, 3,
                                                   dimension_type, dimension, description, "m", NULL,
                                                   read_ch4_altitude_bounds);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/height_levels[]";
    description =
        "derived from altitude per level (layer boundary) by repeating the inner levels; the upper bound of layer k is equal to the lower bound of layer k+1";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    description = "pressure bounds per profile layer";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "pressure_bounds", harp_type_double, 3,
                                                   dimension_type, dimension, description, "hPa", NULL,
                                                   read_ch4_pressure_bounds);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/surface_pressure[],/PRODUCT/SUPPORT_DATA/INPUT_DATA/dp[]";
    description = "derived from surface pressure and pressure difference between retrieval levels (the pressure grid "
        "is equidistant between the surface pressure and a fixed top pressure); given a zero-based layer "
        "index k, the pressure bounds for layer k are derived as: (surface_pressure - k * dp, "
        "surface_pressure - (k + 1) * dp)";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    description = "column averaged dry air mixing ratio of methane";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "CH4_column_volume_mixing_ratio_dry_air",
                                                   harp_type_float, 1, dimension_type, NULL, description, "ppbv", NULL,
                                                   read_ch4_column);
    path = "/PRODUCT/XCH4[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "uncertainty of the column averaged dry air mixing ratio of methane (1 sigma error)";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "CH4_column_volume_mixing_ratio_dry_air_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description, "ppbv", NULL,
                                                   read_ch4_column_precision);
    path = "/PRODUCT/XCH4_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "column averaging kernel for methane retrieval";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "CH4_column_volume_mixing_ratio_dry_air_avk",
                                                   harp_type_float, 2, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL, read_ch4_column_avk);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/column_averaging_kernel[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

static void register_co_product(void)
{
    const char *path;
    const char *description;
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_vertical };
    harp_dimension_type dimension_type_altitude[1] = { harp_dimension_vertical };

    module = harp_ingestion_register_module_coda("S5P_L2_CO", "Sentinel-5P", "Sentinel5P", "L2__CO____",
                                                 "Sentinel-5P L2 CO total column", ingestion_init, ingestion_done);

    product_definition = harp_ingestion_register_product(module, "S5P_L2_CO", NULL, read_dimensions);
    register_core_variables(product_definition);
    register_geolocation_variables(product_definition);
    register_additional_geolocation_variables(product_definition);

    description = "fixed altitude grid on which the radiative transfer calculations are done; altitude is measured"
        " relative to the surface";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "altitude", harp_type_float, 1,
                                                   dimension_type_altitude, NULL, description, "m", NULL,
                                                   read_co_altitude);
    path = "/PRODUCT/layer[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "pressure of the layer interfaces of the vertical grid";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "pressure", harp_type_float, 2, dimension_type,
                                                   NULL, description, "hPa", NULL, read_co_pressure);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/pressure_levels[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "vertically integrated CO column density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "CO_column_number_density", harp_type_float, 1,
                                                   dimension_type, NULL, description, "mol/m^2", NULL, read_co_column);
    path = "/PRODUCT/CO_total_vertical_column[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "uncertainty of the vertically integrated CO column density (standard error)";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "CO_column_number_density_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description, "mol/m^2",
                                                   NULL, read_co_column_precision);
    path = "/PRODUCT/CO_total_vertical_column_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "averaging kernel for the vertically integrated CO column density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "CO_column_number_density_avk", harp_type_float,
                                                   2, dimension_type, NULL, description, "m", NULL, read_co_column_avk);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/column_averaging_kernel[]";
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

    module = harp_ingestion_register_module_coda("S5P_L2_HCHO", "Sentinel-5P", "Sentinel5P", "L2__HCHO__",
                                                 "Sentinel-5P L2 HCHO total column", ingestion_init, ingestion_done);

    product_definition = harp_ingestion_register_product(module, "S5P_L2_HCHO", NULL, read_dimensions);
    register_core_variables(product_definition);
    register_geolocation_variables(product_definition);
    register_additional_geolocation_variables(product_definition);

    description = "total column number density of tropospheric HCHO";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "HCHO_column_number_density", harp_type_float, 1,
                                                   dimension_type, NULL, description, "mol/cm^2", NULL,
                                                   read_hcho_column);
    path = "/PRODUCT/hcho_vertical_column[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "uncertainty of the total column number density of tropospheric HCHO";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "HCHO_column_number_density_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description, "mol/cm^2",
                                                   NULL, read_hcho_column_precision);
    path = "/PRODUCT/hcho_vertical_column_precision[]";
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
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/hcho_profile_apriori[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

static void register_o3_product(void)
{
    const char *path;
    const char *description;
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;

    harp_dimension_type dimension_type[3] = { harp_dimension_time, harp_dimension_vertical,
        harp_dimension_independent
    };
    long dimension[3] = { -1, -1, 2 };

    module = harp_ingestion_register_module_coda("S5P_L2_O3", "Sentinel-5P", "Sentinel5P", "L2__O3____",
                                                 "Sentinel-5P L2 O3 total column", ingestion_init, ingestion_done);

    product_definition = harp_ingestion_register_product(module, "S5P_L2_O3", NULL, read_dimensions);
    register_core_variables(product_definition);
    register_geolocation_variables(product_definition);
    register_additional_geolocation_variables(product_definition);

    description = "pressure bounds per profile layer";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "pressure_bounds", harp_type_float, 3,
                                                   dimension_type, dimension, description, NULL, NULL,
                                                   read_o3_pressure_bounds);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/pressure_grid[]";
    description =
        "derived from pressure per level (layer boundary) by repeating the inner levels; the upper bound of layer k is equal to the lower bound of layer k+1";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    description = "O3 column number density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_column_number_density", harp_type_float, 1,
                                                   dimension_type, NULL, description, NULL, NULL, read_o3_column);
    path = "/PRODUCT/o3[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "uncertainty of the O3 column number density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_column_number_density_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description, NULL, NULL,
                                                   read_o3_column_precision);
    path = "/PRODUCT/o3_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "O3 column number density apriori";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_column_number_density_apriori",
                                                   harp_type_float, 2, dimension_type, NULL, description, "DU", NULL,
                                                   read_o3_column_apriori);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/o3_profile_apriori[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "averaging kernel for the O3 column number density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_column_number_density_avk", harp_type_float,
                                                   2, dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_o3_column_avk);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/averaging_kernels[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

static void register_o3_pr_product(void)
{
    const char *path;
    const char *description;
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[3] = { harp_dimension_time, harp_dimension_vertical, harp_dimension_vertical };

    module = harp_ingestion_register_module_coda("S5P_L2_O3_PR", "Sentinel-5P", "Sentinel5P", "L2__O3__PR",
                                                 "Sentinel-5P L2 O3 profile", ingestion_init, ingestion_done);

    product_definition = harp_ingestion_register_product(module, "S5P_L2_O3_PR", NULL, read_dimensions);
    register_core_variables(product_definition);
    register_geolocation_variables(product_definition);
    register_additional_geolocation_variables(product_definition);

    description = "pressure";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "pressure", harp_type_float, 2, dimension_type,
                                                   NULL, description, "hPa", NULL, read_o3_pr_pressure);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/pressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "altitude";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "altitude", harp_type_float, 2, dimension_type,
                                                   NULL, description, "m", NULL, read_o3_pr_altitude);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/altitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "temperature";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "temperature", harp_type_float, 2,
                                                   dimension_type, NULL, description, "K", NULL,
                                                   read_o3_pr_temperature);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/temperature[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "O3 volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_volume_mixing_ratio", harp_type_float, 2,
                                                   dimension_type, NULL, description, "ppmv", NULL,
                                                   read_o3_pr_volume_mixing_ratio);
    path = "/PRODUCT/O3[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "uncertainty of the O3 volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_volume_mixing_ratio_uncertainty",
                                                   harp_type_float, 2, dimension_type, NULL, description, "ppmv", NULL,
                                                   read_o3_pr_volume_mixing_ratio_uncertainty);
    path = "/PRODUCT/O3_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "O3 volume mixing ratio averaging kernel";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_volume_mixing_ratio_avk", harp_type_float, 3,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_o3_pr_volume_mixing_ratio_avk);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/averaging_kernel[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "O3 volume mixing ratio apriori";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_volume_mixing_ratio_apriori",
                                                   harp_type_float, 2, dimension_type, NULL, description, "ppmv", NULL,
                                                   read_o3_pr_volume_mixing_ratio_apriori);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/O3_apriori[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "uncertainty of the O3 volume mixing ratio apriori";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_volume_mixing_ratio_apriori_uncertainty",
                                                   harp_type_float, 2, dimension_type, NULL, description, "ppmv", NULL,
                                                   read_o3_pr_volume_mixing_ratio_apriori_uncertainty);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/O3_apriori_error_covariance_matrix[]";
    description = "uncertainty derived from variance as: sqrt(O3_apriori_error_covariance_matrix[])";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    description = "O3 volume mixing ratio covariance";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_volume_mixing_ratio_covariance",
                                                   harp_type_float, 3, dimension_type, NULL, description, "pptv", NULL,
                                                   read_o3_pr_volume_mixing_ratio_covariance);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/O3_error_covariance_matrix[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "O3 total column";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_column_number_density", harp_type_float, 1,
                                                   dimension_type, NULL, description, "mol/m^2", NULL,
                                                   read_o3_pr_column_number_density);
    path = "/PRODUCT/O3_total_column[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "uncertainty of the O3 total column";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_column_number_density_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description, "mol/m^2",
                                                   NULL, read_o3_pr_column_number_density_uncertainty);
    path = "/PRODUCT/O3_total_column_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "O3 tropospheric column";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "tropospheric_O3_column_number_density",
                                                   harp_type_float, 1, dimension_type, NULL, description, "mol/m^2",
                                                   NULL, read_o3_pr_tropospheric_column_number_density);
    path = "/PRODUCT/O3_tropospheric_column[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "uncertainty of the O3 tropospheric column";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "tropospheric_O3_column_number_density_uncertainty", harp_type_float,
                                                   1, dimension_type, NULL, description, "mol/m^2", NULL,
                                                   read_o3_pr_tropospheric_column_number_density_uncertainty);
    path = "/PRODUCT/O3_tropospheric_column_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

#if 0
static void register_o3_tpr_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;

    module = harp_ingestion_register_module_coda("S5P_L2_O3_TPR", "Sentinel-5P", "Sentinel5P", "L2__O3_TPR",
                                                 "Sentinel-5P L2 O3 tropospheric profile", ingestion_init,
                                                 ingestion_done);

    product_definition = harp_ingestion_register_product(module, "S5P_L2_O3_TPR", NULL, read_dimensions);
    register_core_variables(product_definition);
    register_geolocation_variables(product_definition);
}
#endif

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

    module = harp_ingestion_register_module_coda("S5P_L2_NO2", "Sentinel-5P", "Sentinel5P", "L2__NO2___",
                                                 "Sentinel-5P L2 NO2 tropospheric column", ingestion_init,
                                                 ingestion_done);

    product_definition = harp_ingestion_register_product(module, "S5P_L2_NO2", NULL, read_dimensions);
    register_core_variables(product_definition);
    register_geolocation_variables(product_definition);
    register_additional_geolocation_variables(product_definition);

    description = "pressure boundaries";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "pressure_bounds", harp_type_double, 3,
                                                   pressure_bounds_dimension_type, pressure_bounds_dimension,
                                                   description, "Pa", NULL, read_no2_pressure_bounds);
    path =
        "/PRODUCT/tm5_pressure_level_a[],/PRODUCT/tm5_pressure_level_b[],/PRODUCT/SUPPORT_DATA/INPUT_DATA/surface_pressure[]";
    description =
        "pressure in Pa at level k is derived from surface pressure in hPa as: tm5_pressure_level_a[k] + tm5_pressure_level_b[k] * surface_pressure[] * 100.0";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    description = "tropospheric vertical column of NO2";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "tropospheric_NO2_column_number_density",
                                                   harp_type_float, 1, dimension_type, NULL, description, "mol/m^2",
                                                   NULL, read_no2_column_tropospheric);
    path = "/PRODUCT/tropospheric_no2_vertical_column[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "uncertainty of the tropospheric vertical column of NO2 (standard error)";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "tropospheric_NO2_column_number_density_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description, "mol/m^2",
                                                   NULL, read_no2_column_tropospheric_precision);
    path = "/PRODUCT/tropospheric_no2_vertical_column_precision[]";
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
                                                   dimension_type, NULL, description, "mol/m^2", NULL, read_no2_column);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/total_no2_vertical_column[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "uncertainty of the total vertical column of NO2 (standard error)";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "NO2_column_number_density_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description, "mol/m^2",
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

static void register_so2_product(void)
{
    const char *path;
    const char *description;
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_vertical };

    module = harp_ingestion_register_module_coda("S5P_L2_SO2", "Sentinel-5P", "Sentinel5P", "L2__SO2___",
                                                 "Sentinel-5P L2 SO2 total column", ingestion_init, ingestion_done);

    product_definition = harp_ingestion_register_product(module, "S5P_L2_SO2", NULL, read_dimensions);
    register_core_variables(product_definition);
    register_geolocation_variables(product_definition);
    register_additional_geolocation_variables(product_definition);

    description = "SO2 vertical column density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "SO2_column_number_density", harp_type_float, 1,
                                                   dimension_type, NULL, description, NULL, NULL, read_so2_column);
    path = "/PRODUCT/so2_vertical_column[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "uncertainty of the SO2 vertical column density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "SO2_column_number_density_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description, NULL, NULL,
                                                   read_so2_column_uncertainty);
    path = "/PRODUCT/so2_vertical_column_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "averaging kernel for the SO2 vertical column density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "SO2_column_number_density_avk", harp_type_float,
                                                   2, dimension_type, NULL, description, NULL, NULL,
                                                   read_so2_column_avk);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/averaging_kernels[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

static void register_cloud_product(void)
{
    const char *path;
    const char *description;
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };

    module = harp_ingestion_register_module_coda("S5P_L2_CLOUD", "Sentinel-5P", "Sentinel5P", "L2__CLOUD_",
                                                 "Sentinel-5P L2 cloud properties", ingestion_init, ingestion_done);

    product_definition = harp_ingestion_register_product(module, "S5P_L2_CLOUD", NULL, read_dimensions);
    register_core_variables(product_definition);
    register_geolocation_variables(product_definition);
    register_additional_geolocation_variables(product_definition);

    /* cloud_fraction */
    description = "retrieved fraction of horizontal area occupied by clouds using the OCRA/ROCINN CAL model";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_fraction", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_cloud_fraction);
    path = "/PRODUCT/cloud_radiometric_fraction[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_fraction_uncertainty */
    description =
        "uncertainty of the retrieved fraction of horizontal area occupied by clouds using the OCRA/ROCINN CAL model";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_fraction_uncertainty", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_cloud_fraction_precision);
    path = "/PRODUCT/cloud_radiometric_fraction_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_top_pressure */
    description = "retrieved atmospheric pressure at the level of cloud using the OCRA/ROCINN CAL model";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_top_pressure", harp_type_float, 1,
                                                   dimension_type, NULL, description, "hPa", NULL,
                                                   read_cloud_top_pressure);
    path = "/PRODUCT/cloud_top_pressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_top_pressure_uncertainty */
    description =
        "uncertainty of the retrieved atmospheric pressure at the level of cloud using the OCRA/ROCINN CAL model";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_top_pressure_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description, "hPa", NULL,
                                                   read_cloud_top_pressure_precision);
    path = "/PRODUCT/cloud_top_pressure_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_top_height */
    description = "retrieved vertical distance above the surface of the cloud top using the OCRA/ROCINN CAL model";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_top_height", harp_type_float, 1,
                                                   dimension_type, NULL, description, "m", NULL, read_cloud_top_height);
    path = "/PRODUCT/cloud_top_height[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_top_height_uncertainty */
    description =
        "uncertainty of the retrieved vertical distance above the surface of the cloud top using the OCRA/ROCINN CAL model";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_top_height_uncertainty", harp_type_float,
                                                   1, dimension_type, NULL, description, "m", NULL,
                                                   read_cloud_top_height_precision);
    path = "/PRODUCT/cloud_top_height_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_optical_depth */
    description = "retrieved cloud optical depth using the OCRA/ROCINN CAL model";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_optical_depth", harp_type_float, 1,
                                                   dimension_type, NULL, description, "m", NULL,
                                                   read_cloud_optical_thickness);
    path = "/PRODUCT/cloud_optical_thickness[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_optical_depth_uncertainty */
    description = "uncertainty of the retrieved cloud optical depth using the OCRA/ROCINN CAL model";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_optical_depth_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description, "m", NULL,
                                                   read_cloud_optical_thickness_precision);
    path = "/PRODUCT/cloud_optical_thickness_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

static void register_fresco_product(void)
{
    const char *path;
    const char *description;
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };

    module = harp_ingestion_register_module_coda("S5P_L2_FRESCO", "Sentinel-5P", "Sentinel5P", "L2__FRESCO",
                                                 "Sentinel-5P L2 KNMI cloud support product", ingestion_init,
                                                 ingestion_done);

    product_definition = harp_ingestion_register_product(module, "S5P_L2_FRESCO", NULL, read_dimensions);
    register_core_variables(product_definition);
    register_geolocation_variables(product_definition);
    register_additional_geolocation_variables(product_definition);

    description = "effective cloud fraction retrieved from the O2 A-band";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_fraction", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_fresco_cloud_fraction);
    path = "/PRODUCT/cloud_fraction[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "uncertainty of the effective cloud fraction";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_fraction_uncertainty", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_fresco_cloud_fraction_precision);
    path = "/PRODUCT/cloud_fraction_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "cloud optical centroid pressure retrieved from the O2 A-band";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_pressure", harp_type_float, 1,
                                                   dimension_type, NULL, description, "hPa", NULL,
                                                   read_fresco_cloud_pressure);
    path = "/PRODUCT/cloud_pressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "uncertainty of the cloud optical centroid pressure";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_pressure_uncertainty", harp_type_float, 1,
                                                   dimension_type, NULL, description, "hPa", NULL,
                                                   read_fresco_cloud_pressure_precision);
    path = "/PRODUCT/cloud_pressure_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "cloud optical centroid height with respect to the surface";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_height", harp_type_float, 1,
                                                   dimension_type, NULL, description, "m", NULL,
                                                   read_fresco_cloud_height);
    path = "/PRODUCT/cloud_height[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "uncertainty of the cloud optical centroid height";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_height_uncertainty", harp_type_float, 1,
                                                   dimension_type, NULL, description, "m", NULL,
                                                   read_fresco_cloud_height_precision);
    path = "/PRODUCT/cloud_height_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "cloud albedo; this is a fixed value for FRESCO";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_albedo", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_fresco_cloud_albedo);
    path = "/PRODUCT/cloud_albedo[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "cloud albedo error; since cloud albedo is fixed for FRESCO, this value is set to NaN";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_albedo_uncertainty", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_fresco_cloud_albedo_precision);
    path = "/PRODUCT/cloud_albedo_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

int harp_ingestion_module_s5p_l2_init(void)
{
    register_aer_ai_product();
    // register_aer_lh_product();
    register_ch4_product();
    register_co_product();
    register_hcho_product();
    register_o3_product();
    register_o3_pr_product();
    // register_o3_tpr_product();
    register_no2_product();
    register_so2_product();
    register_cloud_product();
    register_fresco_product();

    return 0;
}
