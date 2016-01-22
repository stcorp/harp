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
#include "harp-ingestion.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Number of seconds between 2000/01/01 TAI and 2010/01/01 UTC. */
#define SECONDS_FROM_2000_TAI_TO_2010_UTC (315619200 + 34)

/* Default fill value taken from "Input/output data specification for the TROPOMI L-1b data processor",
 * S5P-KNMI-L01B-0012-SD.
 */
#define DEFAULT_FILL_VALUE_INT (-2147483647)

/* Macro to determine the number of elements in a one dimensional C array. */
#define ARRAY_SIZE(X) (sizeof((X))/sizeof((X)[0]))

typedef enum s5p_product_type_enum
{
    s5p_type_o3_pr,
    s5p_type_o3_tpr,
    s5p_type_no2,
    s5p_type_co,
    s5p_type_ch4,
    s5p_type_aer_lh,
    s5p_type_aer_ai,
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
    {"time", "scanline", "ground_pixel", "corner", NULL, NULL},
    {"time", "scanline", "ground_pixel", "corner", NULL, NULL},
    {"time", "scanline", "ground_pixel", "corner", "profile_layers", NULL},
    {"time", "scanline", "ground_pixel", "corner", "layer", NULL},
    {"time", "scanline", "ground_pixel", "corner", "layer", "level"},
    {"time", "scanline", "ground_pixel", "corner", NULL, NULL},
    {"time", "scanline", "ground_pixel", "corner", NULL, NULL},
    {"time", "scanline", "ground_pixel", "corner", NULL, NULL},
    {"time", "scanline", "ground_pixel", "corner", NULL, "levels"},
    {"time", "scanline", "ground_pixel", "corner", "layers", "levels"},
    {"time", "scanline", "ground_pixel", "corner", "layers", NULL}
};

typedef struct ingest_info_struct
{
    coda_product *product;

    s5p_product_type product_type;
    long dimension[S5P_NUM_DIM_TYPES];

    coda_cursor product_cursor;
    coda_cursor geolocation_cursor;
    coda_cursor detailed_results_cursor;
    coda_cursor input_data_cursor;

    int wavelength_ratio;
} ingest_info;

static void filter_array_float(long num_elements, float *data, float fill_value)
{
    float *data_end;

    for (data_end = data + num_elements; data != data_end; ++data)
    {
        if (*data == fill_value)
        {
            *data = coda_NaN();
        }
    }
}

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

        for (; pixel != pixel_end; ++pixel)
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

        for (; pixel != pixel_end; ++pixel)
        {
            *pixel = scanline_value;
        }
    }
}

static const char *get_variable_name_from_cursor(coda_cursor *cursor)
{
    coda_cursor parent_cursor;
    coda_type *parent_type;
    const char *variable_name;
    long index;

    variable_name = "<unknown variable name>";
    if (coda_cursor_get_index(cursor, &index) != 0)
    {
        return variable_name;
    }

    parent_cursor = *cursor;
    if (coda_cursor_goto_parent(&parent_cursor) != 0)
    {
        return variable_name;
    }
    if (coda_cursor_get_type(&parent_cursor, &parent_type) != 0)
    {
        return variable_name;
    }
    if (coda_type_get_record_field_real_name(parent_type, index, &variable_name) != 0)
    {
        return variable_name;
    }

    return variable_name;
}

static int verify_variable_dimensions(coda_cursor *cursor, int num_dimensions, const long *dimension)
{
    long coda_dimension[CODA_MAX_NUM_DIMS];
    int num_coda_dimensions;
    int i;

    if (coda_cursor_get_array_dim(cursor, &num_coda_dimensions, coda_dimension) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    if (num_coda_dimensions != num_dimensions)
    {
        harp_set_error(HARP_ERROR_INGESTION, "product error detected (variable '%s' has %d dimensions, expected %d)",
                       get_variable_name_from_cursor(cursor), num_coda_dimensions, num_dimensions);
        return -1;
    }

    for (i = 0; i < num_dimensions; i++)
    {
        if (coda_dimension[i] != dimension[i])
        {
            harp_set_error(HARP_ERROR_INGESTION,
                           "product error detected (dimension %d of variable '%s' has %ld elements," " expected %ld)",
                           i, get_variable_name_from_cursor(cursor), coda_dimension[i], dimension[i]);
            return -1;
        }
    }

    return 0;
}

static int get_fill_value_float(coda_cursor *cursor, float *fill_value)
{
    if (coda_cursor_goto_attributes(cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(cursor, "FillValue") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_first_array_element(cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_float(cursor, fill_value) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    coda_cursor_goto_parent(cursor);
    coda_cursor_goto_parent(cursor);
    coda_cursor_goto_parent(cursor);

    return 0;
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
        case s5p_type_fresco:
            return "L2__FRESCO";
        case s5p_type_so2:
            return "SO2____";
        case s5p_type_o3:
            return "O3____";
        case s5p_type_hcho:
            return "HCHO__";
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

static int get_dimension_length(ingest_info *info, const char *dimension_name, long *dimension_length)
{
    coda_cursor cursor;
    long dim[CODA_MAX_NUM_DIMS];
    int num_dims;

    cursor = info->product_cursor;
    if (coda_cursor_goto_record_field_by_name(&cursor, dimension_name) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_array_dim(&cursor, &num_dims, dim) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    if (num_dims != 1)
    {
        harp_set_error(HARP_ERROR_INGESTION, "product error detected (cannot determine length of dimension '%s')",
                       dimension_name);
        return -1;
    }

    *dimension_length = *dim;

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
        if (coda_cursor_goto_record_field_by_name(&cursor, "GEOLOCATION") != 0)
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
    int i;

    for (i = 0; i < S5P_NUM_DIM_TYPES; i++)
    {
        const char *dimension_name;

        dimension_name = s5p_dimension_name[info->product_type][i];
        if (dimension_name == NULL)
        {
            continue;
        }

        if (get_dimension_length(info, dimension_name, &info->dimension[i]) != 0)
        {
            return -1;
        }
    }

    if (info->dimension[s5p_dim_time] != 1)
    {
        harp_set_error(HARP_ERROR_INGESTION, "product error detected ('time' dimension has length %d, expected 1)",
                       info->dimension[s5p_dim_time]);
        return -1;
    }

    if (info->dimension[s5p_dim_corner] != 4)
    {
        harp_set_error(HARP_ERROR_INGESTION, "product error detected ('corner' dimension has length %d, expected 4)",
                       info->dimension[s5p_dim_corner]);
        return -1;
    }

    if (info->dimension[s5p_dim_level] > 0 && info->dimension[s5p_dim_layer] > 0)
    {
        if (info->dimension[s5p_dim_level] != info->dimension[s5p_dim_layer] + 1)
        {
            harp_set_error(HARP_ERROR_INGESTION, "product error detected ('%s' dimension has length %d, expected %d)",
                           s5p_dimension_name[info->product_type][s5p_dim_level], info->dimension[s5p_dim_level],
                           info->dimension[s5p_dim_layer] + 1);
            return -1;
        }
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
    memset(info->dimension, 0, S5P_NUM_DIM_TYPES * sizeof(long));
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

static int read_variable_float(ingest_info *info, coda_cursor *cursor, const char *name, int num_dimensions,
                               const long *dimension, harp_array data)
{
    long default_dimension[4] = { 1, info->dimension[s5p_dim_scanline], info->dimension[s5p_dim_pixel], 4 };
    long num_elements;
    float fill_value;

    if (coda_cursor_goto_record_field_by_name(cursor, name) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    assert(dimension != NULL || num_dimensions <= 4);
    if (verify_variable_dimensions(cursor, num_dimensions, (dimension == NULL ? default_dimension : dimension)) != 0)
    {
        return -1;
    }
    if (get_fill_value_float(cursor, &fill_value) != 0)
    {
        return -1;
    }
    if (coda_cursor_read_float_array(cursor, data.float_data, coda_array_ordering_c) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    coda_cursor_goto_parent(cursor);

    /* Replace values equal to the variable specific _FillValue attribute by NaN. */
    num_elements = harp_get_num_elements(num_dimensions, (dimension == NULL ? default_dimension : dimension));
    filter_array_float(num_elements, data.float_data, fill_value);

    return 0;
}

static int read_dimensions(void *user_data, long dimension[HARP_NUM_DIM_TYPES])
{
    ingest_info *info = (ingest_info *)user_data;

    dimension[harp_dimension_time] = info->dimension[s5p_dim_time] * info->dimension[s5p_dim_scanline]
        * info->dimension[s5p_dim_pixel];

    switch (info->product_type)
    {
        case s5p_type_no2:
        case s5p_type_co:
        case s5p_type_ch4:
        case s5p_type_o3:
        case s5p_type_hcho:
            dimension[harp_dimension_vertical] = info->dimension[s5p_dim_layer];
            break;
        case s5p_type_so2:
            dimension[harp_dimension_vertical] = info->dimension[s5p_dim_level];
            break;
        case s5p_type_o3_pr:
        case s5p_type_o3_tpr:
        case s5p_type_aer_lh:
        case s5p_type_aer_ai:
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

    index = index - (index / info->dimension[s5p_dim_pixel]) * info->dimension[s5p_dim_pixel];
    *data.int16_data = (int16_t)index;

    return 0;
}

static int read_datetime(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long dimension[2] = { 1, info->dimension[s5p_dim_scanline] };
    double *datetime;
    double *datetime_end;
    coda_cursor cursor;
    double time_reference;

    /* NB. The HARP ingest module for S5P L1B products uses the same approach to compute datetime values as used here.
     * For S5P L1B products, it seems that the contents of the "time" variable is computed without proper handling of
     * leap seconds. This may also be the case for S5P L2 products, but that has not been investigated.
     */

    /* Read reference time in seconds since 2010-01-01 00:00:00 UTC. */
    cursor = info->product_cursor;
    if (coda_cursor_goto_record_field_by_name(&cursor, "time") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (verify_variable_dimensions(&cursor, 1, dimension) != 0)
    {
        return -1;
    }
    if (coda_cursor_goto_first_array_element(&cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_double(&cursor, &time_reference) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    /* Read difference in milliseconds (ms) between the time reference and the start of the observation. */
    cursor = info->product_cursor;
    if (coda_cursor_goto_record_field_by_name(&cursor, "delta_time") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (verify_variable_dimensions(&cursor, 2, dimension) != 0)
    {
        return -1;
    }
    if (coda_cursor_read_double_array(&cursor, data.double_data, coda_array_ordering_c) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    /* Convert observation start time to seconds since 2000-01-01 00:00:00 TAI. */
    for (datetime = data.double_data, datetime_end = datetime + info->dimension[s5p_dim_scanline];
         datetime != datetime_end; ++datetime)
    {
        if (time_reference == DEFAULT_FILL_VALUE_INT || *datetime == DEFAULT_FILL_VALUE_INT)
        {
            *datetime = coda_NaN();
        }
        else
        {
            *datetime = SECONDS_FROM_2000_TAI_TO_2010_UTC + time_reference + *datetime / 1e3;
        }
    }

    /* Broadcast the result along the pixel dimension. */
    broadcast_array_double(info->dimension[s5p_dim_scanline], info->dimension[s5p_dim_pixel], data.double_data);

    return 0;
}

static int read_longitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(info, &info->product_cursor, "longitude", 3, NULL, data);
}

static int read_latitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(info, &info->product_cursor, "latitude", 3, NULL, data);
}

static int read_dlr_longitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long dimension[2] = { info->dimension[s5p_dim_scanline], info->dimension[s5p_dim_pixel] };

    return read_variable_float(info, &info->product_cursor, "longitude", 2, dimension, data);
}

static int read_dlr_latitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long dimension[2] = { info->dimension[s5p_dim_scanline], info->dimension[s5p_dim_pixel] };

    return read_variable_float(info, &info->product_cursor, "latitude", 2, dimension, data);
}

static int read_longitude_bounds(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(info, &info->geolocation_cursor, "longitude_bounds", 4, NULL, data);
}

static int read_latitude_bounds(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(info, &info->geolocation_cursor, "latitude_bounds", 4, NULL, data);
}

static int read_instrument_longitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (read_variable_float(info, &info->geolocation_cursor, "satellite_longitude", 2, NULL, data) != 0)
    {
        return -1;
    }

    broadcast_array_float(info->dimension[s5p_dim_scanline], info->dimension[s5p_dim_pixel], data.float_data);

    return 0;
}

static int read_instrument_latitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (read_variable_float(info, &info->geolocation_cursor, "satellite_latitude", 2, NULL, data) != 0)
    {
        return -1;
    }

    broadcast_array_float(info->dimension[s5p_dim_scanline], info->dimension[s5p_dim_pixel], data.float_data);

    return 0;
}

static int read_instrument_altitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (read_variable_float(info, &info->geolocation_cursor, "satellite_altitude", 2, NULL, data) != 0)
    {
        return -1;
    }

    broadcast_array_float(info->dimension[s5p_dim_scanline], info->dimension[s5p_dim_pixel], data.float_data);

    return 0;
}

static int read_solar_azimuth_angle(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(info, &info->geolocation_cursor, "solar_azimuth_angle", 3, NULL, data);
}

static int read_solar_zenith_angle(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(info, &info->geolocation_cursor, "solar_zenith_angle", 3, NULL, data);
}

static int read_viewing_azimuth_angle(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(info, &info->geolocation_cursor, "viewing_azimuth_angle", 3, NULL, data);
}

static int read_viewing_zenith_angle(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(info, &info->geolocation_cursor, "viewing_zenith_angle", 3, NULL, data);
}

static int read_fresco_cloud_fraction(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(info, &info->product_cursor, "cloud_fraction", 3, NULL, data);
}

static int read_fresco_cloud_fraction_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(info, &info->product_cursor, "cloud_fraction_precision", 3, NULL, data);
}

static int read_fresco_cloud_pressure(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(info, &info->product_cursor, "cloud_pressure", 3, NULL, data);
}

static int read_fresco_cloud_pressure_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(info, &info->product_cursor, "cloud_pressure_precision", 3, NULL, data);
}

static int read_fresco_cloud_height(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(info, &info->product_cursor, "cloud_height", 3, NULL, data);
}

static int read_fresco_cloud_height_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(info, &info->product_cursor, "cloud_height_precision", 3, NULL, data);
}

static int read_fresco_cloud_albedo(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(info, &info->product_cursor, "cloud_albedo", 3, NULL, data);
}

static int read_fresco_cloud_albedo_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(info, &info->product_cursor, "cloud_albedo_precision", 3, NULL, data);
}

static int read_fresco_scene_albedo(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(info, &info->product_cursor, "scene_albedo", 3, NULL, data);
}

static int read_fresco_scene_albedo_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(info, &info->product_cursor, "scene_albedo_precision", 3, NULL, data);
}

static int read_fresco_scene_pressure(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(info, &info->product_cursor, "apparent_scene_pressure", 3, NULL, data);
}

static int read_fresco_scene_pressure_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(info, &info->product_cursor, "apparent_scene_pressure_precision", 3, NULL, data);
}

static int read_no2_column_tropospheric(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(info, &info->product_cursor, "tropospheric_no2_vertical_column", 3, NULL, data);
}

static int read_no2_column_tropospheric_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(info, &info->product_cursor, "tropospheric_no2_vertical_column_precision", 3,
                               NULL, data);
}

static int read_no2_column_stratospheric(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(info, &info->product_cursor, "stratospheric_no2_vertical_column", 3, NULL, data);
}

static int read_no2_column_stratospheric_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(info, &info->product_cursor, "stratospheric_no2_vertical_column_precision", 3,
                               NULL, data);
}

static int read_no2_column(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(info, &info->product_cursor, "total_no2_vertical_column", 3, NULL, data);
}

static int read_no2_column_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(info, &info->product_cursor, "total_no2_vertical_column_precision", 3, NULL, data);
}

static int read_no2_column_avk(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    long dimension[4] = { info->dimension[s5p_dim_time], info->dimension[s5p_dim_scanline],
        info->dimension[s5p_dim_pixel], info->dimension[s5p_dim_layer]
    };

    return read_variable_float(info, &info->detailed_results_cursor, "averaging_kernel", 4, dimension, data);
}

static int read_co_altitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long dimension[1] = { info->dimension[s5p_dim_layer] };

    return read_variable_float(info, &info->product_cursor, "layer", 1, dimension, data);
}

static int read_co_pressure(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    long dimension[4] = { info->dimension[s5p_dim_time], info->dimension[s5p_dim_scanline],
        info->dimension[s5p_dim_pixel], info->dimension[s5p_dim_layer]
    };

    return read_variable_float(info, &info->product_cursor, "pressure_levels", 4, dimension, data);
}

static int read_co_column(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(info, &info->product_cursor, "CO_total_vertical_column", 3, NULL, data);
}

static int read_co_column_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(info, &info->product_cursor, "CO_total_vertical_column_precision", 3, NULL, data);
}

static int read_co_column_avk(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    long dimension[4] = { info->dimension[s5p_dim_time], info->dimension[s5p_dim_scanline],
        info->dimension[s5p_dim_pixel], info->dimension[s5p_dim_layer]
    };

    return read_variable_float(info, &info->detailed_results_cursor, "column_averaging_kernel", 4, dimension, data);
}

static int read_so2_column(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(info, &info->product_cursor, "so2", 3, NULL, data);
}

static int read_so2_column_avk(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    long dimension[4] = { info->dimension[s5p_dim_time], info->dimension[s5p_dim_scanline],
        info->dimension[s5p_dim_pixel], info->dimension[s5p_dim_level]
    };

    return read_variable_float(info, &info->detailed_results_cursor, "averaging_kernel", 4, dimension, data);
}

static int read_o3_pressure(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    long dimension[4] = { info->dimension[s5p_dim_time], info->dimension[s5p_dim_scanline],
        info->dimension[s5p_dim_pixel], info->dimension[s5p_dim_level]
    };
    long num_layers;
    long i;

    if (read_variable_float(info, &info->detailed_results_cursor, "pressure_grid", 4, dimension, data) != 0)
    {
        return -1;
    }

    /* Convert from #levels (== #layers + 1) consecutive pressures to #layers x 2 pressure bounds. Iterate in reverse to
     * ensure correct results (conversion is performed in place).
     */
    num_layers = info->dimension[s5p_dim_layer];
    assert((num_layers + 1) == info->dimension[s5p_dim_level]);

    for (i = harp_get_num_elements(3, dimension) - 1; i >= 0; --i)
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

    return read_variable_float(info, &info->product_cursor, "o3", 3, NULL, data);
}

static int read_o3_column_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(info, &info->product_cursor, "o3_precision", 3, NULL, data);
}

static int read_o3_column_avk(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    long dimension[4] = { info->dimension[s5p_dim_time], info->dimension[s5p_dim_scanline],
        info->dimension[s5p_dim_pixel], info->dimension[s5p_dim_layer]
    };

    return read_variable_float(info, &info->detailed_results_cursor, "averaging_kernels", 4, dimension, data);
}

static int read_hcho_column(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(info, &info->product_cursor, "hcho", 3, NULL, data);
}

static int read_hcho_column_avk(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    long dimension[4] = { info->dimension[s5p_dim_time], info->dimension[s5p_dim_scanline],
        info->dimension[s5p_dim_pixel], info->dimension[s5p_dim_layer]
    };

    return read_variable_float(info, &info->detailed_results_cursor, "averaging_kernels", 4, dimension, data);
}

static int read_ch4_altitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    long dimension[4] = { info->dimension[s5p_dim_time], info->dimension[s5p_dim_scanline],
        info->dimension[s5p_dim_pixel], info->dimension[s5p_dim_level]
    };
    long num_layers;
    long i;

    if (read_variable_float(info, &info->input_data_cursor, "height_levels", 4, dimension, data) != 0)
    {
        return -1;
    }

    /* Convert from #levels (== #layers + 1) consecutive altitudes to #layers x 2 altitude bounds. Iterate in reverse to
     * ensure correct results (conversion is performed in place).
     */
    num_layers = info->dimension[s5p_dim_layer];
    assert((num_layers + 1) == info->dimension[s5p_dim_level]);

    for (i = harp_get_num_elements(3, dimension) - 1; i >= 0; --i)
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

static int read_ch4_pressure(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    harp_array delta_pressure;
    long num_elements;
    long num_layers;
    long i;

    /* Total number of samples (i.e. length of the time axis of the ingested product). */
    num_elements = info->dimension[s5p_dim_time] * info->dimension[s5p_dim_scanline] * info->dimension[s5p_dim_pixel];

    /* Number of profile layers. */
    num_layers = info->dimension[s5p_dim_layer];

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
    if (read_variable_float(info, &info->input_data_cursor, "surface_pressure", 3, NULL, data) != 0)
    {
        return -1;
    }

    /* Allocate auxiliary storage for the pressure difference data. */
    delta_pressure.ptr = (float *)malloc(num_elements * sizeof(float));
    if (delta_pressure.ptr == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_elements * sizeof(float), __FILE__, __LINE__);
        return -1;
    }

    if (read_variable_float(info, &info->input_data_cursor, "dp", 3, NULL, delta_pressure) != 0)
    {
        free(delta_pressure.ptr);
        return -1;
    }

    /* Convert from surface pressure and pressure difference to #layers x 2 pressure bounds. The pressure levels are
     * equidistant, separated by the pressure difference. Iterate in reverse to ensure correct results (the conversion
     * is performed in place).
     */
    for (i = num_elements - 1; i >= 0; --i)
    {
        float *pressure = &data.float_data[i * num_layers * 2];
        double surface_pressure = data.float_data[i];
        double delta = delta_pressure.float_data[i];
        long j;

        for (j = num_layers - 1; j >= 0; --j)
        {
            pressure[j * 2 + 1] = surface_pressure + (j + 1) * delta;
            pressure[j * 2] = surface_pressure + j * delta;
        }
    }

    free(delta_pressure.ptr);

    return 0;
}

static int read_ch4_column(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(info, &info->product_cursor, "XCH4", 3, NULL, data);
}

static int read_ch4_column_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(info, &info->product_cursor, "XCH4_precision", 3, NULL, data);
}

static int read_ch4_column_avk(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    long dimension[4] = { info->dimension[s5p_dim_time], info->dimension[s5p_dim_scanline],
        info->dimension[s5p_dim_pixel], info->dimension[s5p_dim_layer]
    };

    return read_variable_float(info, &info->detailed_results_cursor, "column_averaging_kernel", 4, dimension, data);
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

    return read_variable_float(info, &info->product_cursor, variable_name, 3, NULL, data);
}

static int verify_product_type(coda_product *product, s5p_product_type product_type)
{
    coda_cursor cursor;
    s5p_product_type actual_product_type;
    char buffer[8];
    long length;

    if (coda_cursor_set_product(&cursor, product) != 0)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, "/METADATA/GRANULE_DESCRIPTION@InstrumentName") != 0)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, NULL);
        return -1;
    }
    if (coda_cursor_get_string_length(&cursor, &length) != 0)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, NULL);
        return -1;
    }
    if (length != 7)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, NULL);
        return -1;
    }
    if (coda_cursor_read_string(&cursor, buffer, 8) != 0)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, NULL);
        return -1;
    }
    if (strcmp(buffer, "TROPOMI") != 0)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, "../MissionShortName") != 0)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, NULL);
        return -1;
    }
    if (coda_cursor_get_string_length(&cursor, &length) != 0)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, NULL);
        return -1;
    }
    if (length != 3)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, NULL);
        return -1;
    }
    if (coda_cursor_read_string(&cursor, buffer, 4) != 0)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, NULL);
        return -1;
    }
    if (strcmp(buffer, "S5P") != 0)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, "../ProcessLevel") != 0)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, NULL);
        return -1;
    }
    if (coda_cursor_get_string_length(&cursor, &length) != 0)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, NULL);
        return -1;
    }
    if (length != 1)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, NULL);
        return -1;
    }
    if (coda_cursor_read_string(&cursor, buffer, 2) != 0)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, NULL);
        return -1;
    }
    if (strcmp(buffer, "2") != 0)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, NULL);
        return -1;
    }
    if (get_product_type(product, &actual_product_type) != 0)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, NULL);
        return -1;
    }
    if (actual_product_type != product_type)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, NULL);
        return -1;
    }

    return 0;
}

#if 0
static int verify_o3_pr(const harp_ingestion_module *module, coda_product *product)
{
    (void)module;
    return verify_product_type(product, s5p_type_o3_pr);
}

static int verify_o3_tpr(const harp_ingestion_module *module, coda_product *product)
{
    (void)module;
    return verify_product_type(product, s5p_type_o3_tpr);
}
#endif

static int verify_no2(const harp_ingestion_module *module, coda_product *product)
{
    (void)module;
    return verify_product_type(product, s5p_type_no2);
}

static int verify_co(const harp_ingestion_module *module, coda_product *product)
{
    (void)module;
    return verify_product_type(product, s5p_type_co);
}

static int verify_ch4(const harp_ingestion_module *module, coda_product *product)
{
    (void)module;
    return verify_product_type(product, s5p_type_ch4);
}

#if 0
static int verify_aer_lh(const harp_ingestion_module *module, coda_product *product)
{
    (void)module;
    return verify_product_type(product, s5p_type_aer_lh);
}
#endif

static int verify_aer_ai(const harp_ingestion_module *module, coda_product *product)
{
    (void)module;
    return verify_product_type(product, s5p_type_aer_ai);
}

static int verify_fresco(const harp_ingestion_module *module, coda_product *product)
{
    (void)module;
    return verify_product_type(product, s5p_type_fresco);
}

static int verify_so2(const harp_ingestion_module *module, coda_product *product)
{
    (void)module;
    return verify_product_type(product, s5p_type_so2);
}

static int verify_o3(const harp_ingestion_module *module, coda_product *product)
{
    (void)module;
    return verify_product_type(product, s5p_type_o3);
}

static int verify_hcho(const harp_ingestion_module *module, coda_product *product)
{
    (void)module;
    return verify_product_type(product, s5p_type_hcho);
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
    const char *description;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };

    description = "pixel index (0-based) within the scanline";
    harp_ingestion_register_variable_sample_read(product_definition, "scanline_pixel_index", harp_type_int16, 1,
                                                 dimension_type, NULL, description, NULL, NULL,
                                                 read_scanline_pixel_index);

    description = "start time of the measurement";
    harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double, 1, dimension_type,
                                               NULL, description, "seconds since 2000-01-01", NULL, read_datetime);
}

static void register_geolocation_variables(harp_product_definition *product_definition)
{
    const char *description;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };

    description = "longitude of the ground pixel center (WGS84)";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_float,
                                                                     1, dimension_type, NULL, description,
                                                                     "degree_east", NULL, read_longitude);
    harp_variable_definition_set_valid_range_float(variable_definition, -180.0f, 180.0f);

    description = "latitude of the ground pixel center (WGS84)";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_float, 1,
                                                                     dimension_type, NULL, description, "degree_north",
                                                                     NULL, read_latitude);
    harp_variable_definition_set_valid_range_float(variable_definition, -90.0f, 90.0f);
}

static void register_dlr_geolocation_variables(harp_product_definition *product_definition)
{
    const char *description;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };

    /* DLR uses non-standard dimensions for the latitude and longitude variables. */
    description = "longitude of the ground pixel center (WGS84)";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_float,
                                                                     1, dimension_type, NULL, description,
                                                                     "degree_east", NULL, read_dlr_longitude);
    harp_variable_definition_set_valid_range_float(variable_definition, -180.0f, 180.0f);

    description = "latitude of the ground pixel center (WGS84)";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_float, 1,
                                                                     dimension_type, NULL, description, "degree_north",
                                                                     NULL, read_dlr_latitude);
    harp_variable_definition_set_valid_range_float(variable_definition, -90.0f, 90.0f);
}

static void register_additional_geolocation_variables(harp_product_definition *product_definition)
{
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

    description = "latitudes of the ground pixel corners (WGS84)";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "latitude_bounds",
                                                                     harp_type_float, 2, bounds_dimension_type,
                                                                     bounds_dimension, description, "degree_north",
                                                                     NULL, read_latitude_bounds);
    harp_variable_definition_set_valid_range_float(variable_definition, -90.0f, 90.0f);

    description = "longitude of the sub-instrument point (WGS84)";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "instrument_longitude",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     description, "degree_east", NULL,
                                                                     read_instrument_longitude);
    harp_variable_definition_set_valid_range_float(variable_definition, -180.0f, 180.0f);

    description = "latitude of the sub-instrument point (WGS84)";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "instrument_latitude",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     description, "degree_north", NULL,
                                                                     read_instrument_latitude);
    harp_variable_definition_set_valid_range_float(variable_definition, -90.0f, 90.0f);

    description = "altitude of the instrument (WGS84)";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "instrument_altitude",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     description, "m", NULL, read_instrument_altitude);
    harp_variable_definition_set_valid_range_float(variable_definition, 700000.0f, 900000.0f);

    /* Angles. */
    description = "zenith angle of the Sun at the ground pixel location (WGS84)";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "solar_zenith_angle",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     description, "degree", NULL,
                                                                     read_solar_zenith_angle);
    harp_variable_definition_set_valid_range_float(variable_definition, 0.0, 180.0);

    description = "azimuth angle of the Sun at the ground pixel location (WGS84), measured East-of-North";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "solar_azimuth_angle",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     description, "degree", NULL,
                                                                     read_solar_azimuth_angle);
    harp_variable_definition_set_valid_range_float(variable_definition, 0.0, 360.0);

    description = "zenith angle of the instrument at the ground pixel location (WGS84)";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "viewing_zenith_angle",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     description, "degree", NULL,
                                                                     read_viewing_zenith_angle);
    harp_variable_definition_set_valid_range_float(variable_definition, 0.0, 180.0);

    description = "azimuth angle of the instrument at the ground pixel location (WGS84), measured East-of-North";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "viewing_azimuth_angle",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     description, "degree", NULL,
                                                                     read_viewing_azimuth_angle);
    harp_variable_definition_set_valid_range_float(variable_definition, 0.0, 360.0);
}

#if 0
static void register_o3_pr_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;

    module = harp_ingestion_register_module_coda("S5P_L2_O3_PR", NULL, NULL, "Sentinel 5P L2 O3 full profile",
                                                 verify_o3_pr, ingestion_init, ingestion_done);

    product_definition = harp_ingestion_register_product(module, "S5P_L2_O3_PR", NULL, read_dimensions);
    register_core_variables(product_definition);
    register_geolocation_variables(product_definition);
}

static void register_o3_tpr_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;

    module = harp_ingestion_register_module_coda("S5P_L2_O3_TPR", NULL, NULL, "Sentinel 5P L2 O3 tropospheric profile",
                                                 verify_o3_tpr, ingestion_init, ingestion_done);

    product_definition = harp_ingestion_register_product(module, "S5P_L2_O3_TPR", NULL, read_dimensions);
    register_core_variables(product_definition);
    register_geolocation_variables(product_definition);
}
#endif

static void register_fresco_product(void)
{
    const char *description;
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };

    module = harp_ingestion_register_module_coda("S5P_L2_FRESCO", NULL, NULL,
                                                 "Sentinel 5P L2 KNMI cloud support product", verify_fresco,
                                                 ingestion_init, ingestion_done);

    product_definition = harp_ingestion_register_product(module, "S5P_L2_FRESCO", NULL, read_dimensions);
    register_core_variables(product_definition);
    register_geolocation_variables(product_definition);
    // register_additional_geolocation_variables(product_definition);

    description = "effective cloud fraction";
    harp_ingestion_register_variable_full_read(product_definition, "cloud_fraction", harp_type_float, 1, dimension_type,
                                               NULL, description, NULL, NULL, read_fresco_cloud_fraction);

    description = "effective cloud fraction precision (1 sigma error)";
    harp_ingestion_register_variable_full_read(product_definition, "cloud_fraction_stdev", harp_type_float, 1,
                                               dimension_type, NULL, description, NULL, NULL,
                                               read_fresco_cloud_fraction_precision);

    description = "cloud pressure, at approximately the mid-level of the cloud layer";
    harp_ingestion_register_variable_full_read(product_definition, "cloud_pressure", harp_type_float, 1, dimension_type,
                                               NULL, description, "hPa", NULL, read_fresco_cloud_pressure);

    description = "cloud pressure precision (1 sigma error)";
    harp_ingestion_register_variable_full_read(product_definition, "cloud_pressure_stdev", harp_type_float, 1,
                                               dimension_type, NULL, description, "hPa", NULL,
                                               read_fresco_cloud_pressure_precision);

    description = "cloud height, at the optical centroid level, measured from the surface";
    harp_ingestion_register_variable_full_read(product_definition, "cloud_height", harp_type_float, 1, dimension_type,
                                               NULL, description, "m", NULL, read_fresco_cloud_height);

    description = "cloud height precision (1 sigma error)";
    harp_ingestion_register_variable_full_read(product_definition, "cloud_height_stdev", harp_type_float, 1,
                                               dimension_type, NULL, description, "m", NULL,
                                               read_fresco_cloud_height_precision);

    description = "cloud albedo; this is a fixed value for FRESCO";
    harp_ingestion_register_variable_full_read(product_definition, "cloud_albedo", harp_type_float, 1, dimension_type,
                                               NULL, description, NULL, NULL, read_fresco_cloud_albedo);

    description = "cloud albedo error; since cloud albedo is fixed for FRESCO, this value is set to NaN";
    harp_ingestion_register_variable_full_read(product_definition, "cloud_albedo_stdev", harp_type_float, 1,
                                               dimension_type, NULL, description, NULL, NULL,
                                               read_fresco_cloud_albedo_precision);

    description = "scene albedo when FRESCO is running in snow/ice mode (this quantity is required by the CH4 "
        "processor)";
    harp_ingestion_register_variable_full_read(product_definition, "scene_albedo", harp_type_float, 1, dimension_type,
                                               NULL, description, NULL, NULL, read_fresco_scene_albedo);

    description = "scene albedo precision (1 sigma error) when FRESCO is running in snow/ice mode";
    harp_ingestion_register_variable_full_read(product_definition, "scene_albedo_stdev", harp_type_float, 1,
                                               dimension_type, NULL, description, NULL, NULL,
                                               read_fresco_scene_albedo_precision);

    description = "apparent scene pressure when FRESCO is running in snow/ice mode (this quantity is required by the "
        "CH4 processor)";
    harp_ingestion_register_variable_full_read(product_definition, "scene_pressure", harp_type_float, 1, dimension_type,
                                               NULL, description, "hPa", NULL, read_fresco_scene_pressure);

    description = "apparent scene precision (1 sigma error) when FRESCO is running in snow/ice mode";
    harp_ingestion_register_variable_full_read(product_definition, "scene_pressure_stdev", harp_type_float, 1,
                                               dimension_type, NULL, description, "hPa", NULL,
                                               read_fresco_scene_pressure_precision);
}

static void register_no2_product(void)
{
    const char *description;
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_vertical };

    module = harp_ingestion_register_module_coda("S5P_L2_NO2", NULL, NULL, "Sentinel 5P L2 NO2 tropospheric column",
                                                 verify_no2, ingestion_init, ingestion_done);

    product_definition = harp_ingestion_register_product(module, "S5P_L2_NO2", NULL, read_dimensions);
    register_core_variables(product_definition);
    register_geolocation_variables(product_definition);

    register_additional_geolocation_variables(product_definition);

    description = "tropospheric vertical column of NO2";
    harp_ingestion_register_variable_full_read(product_definition, "tropospheric_NO2_column_number_density",
                                               harp_type_float, 1, dimension_type, NULL, description, "molec/cm^2",
                                               NULL, read_no2_column_tropospheric);

    description = "uncertainty of the tropospheric vertical column of NO2 (standard error)";
    harp_ingestion_register_variable_full_read(product_definition, "tropospheric_NO2_column_number_density_stdev",
                                               harp_type_float, 1, dimension_type, NULL, description, "molec/cm^2",
                                               NULL, read_no2_column_tropospheric_precision);

    description = "stratospheric vertical column of NO2";
    harp_ingestion_register_variable_full_read(product_definition, "stratospheric_NO2_column_number_density",
                                               harp_type_float, 1, dimension_type, NULL, description, "molec/cm^2",
                                               NULL, read_no2_column_stratospheric);

    description = "uncertainty of the stratospheric vertical column of NO2 (standard error)";
    harp_ingestion_register_variable_full_read(product_definition, "stratospheric_NO2_column_number_density_stdev",
                                               harp_type_float, 1, dimension_type, NULL, description, "molec/cm^2",
                                               NULL, read_no2_column_stratospheric_precision);

    description = "total vertical column of NO2 (ratio of the slant column density of NO2 and the total air mass "
        "factor)";
    harp_ingestion_register_variable_full_read(product_definition, "NO2_column_number_density", harp_type_float, 1,
                                               dimension_type, NULL, description, "molec/cm^2", NULL, read_no2_column);

    description = "uncertainty of the total vertical column of NO2 (standard error)";
    harp_ingestion_register_variable_full_read(product_definition, "NO2_column_number_density_stdev", harp_type_float,
                                               1, dimension_type, NULL, description, "molec/cm^2", NULL,
                                               read_no2_column_precision);

    description = "averaging kernel for the air mass factor correction, describing the NO2 profile sensitivity of the "
        "vertical column density";
    harp_ingestion_register_variable_full_read(product_definition, "NO2_column_number_density_avk", harp_type_float, 2,
                                               dimension_type, NULL, description, "molec/cm^2", NULL,
                                               read_no2_column_avk);
}

static void register_co_product(void)
{
    const char *description;
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_vertical };
    harp_dimension_type dimension_type_altitude[1] = { harp_dimension_vertical };

    module = harp_ingestion_register_module_coda("S5P_L2_CO", NULL, NULL, "Sentinel 5P L2 CO total column", verify_co,
                                                 ingestion_init, ingestion_done);

    product_definition = harp_ingestion_register_product(module, "S5P_L2_CO", NULL, read_dimensions);
    register_core_variables(product_definition);
    register_geolocation_variables(product_definition);
    register_additional_geolocation_variables(product_definition);

    description = "fixed altitude grid on which the radiative transfer calculations are done; altitude is measured"
        " relative to the surface";
    harp_ingestion_register_variable_full_read(product_definition, "altitude", harp_type_float, 1,
                                               dimension_type_altitude, NULL, description, "m", NULL, read_co_altitude);

    description = "pressure of the layer interfaces of the vertical grid";
    harp_ingestion_register_variable_full_read(product_definition, "pressure", harp_type_float, 2, dimension_type, NULL,
                                               description, "hPa", NULL, read_co_pressure);

    description = "vertically integrated CO column density";
    harp_ingestion_register_variable_full_read(product_definition, "CO_column_number_density", harp_type_float, 1,
                                               dimension_type, NULL, description, "mol/cm^2", NULL, read_co_column);

    description = "uncertainty of the vertically integrated CO column density (standard error)";
    harp_ingestion_register_variable_full_read(product_definition, "CO_column_number_density_stdev", harp_type_float, 1,
                                               dimension_type, NULL, description, "mol/cm^2", NULL,
                                               read_co_column_precision);

    description = "averaging kernel for the vertically integrated CO column density";
    harp_ingestion_register_variable_full_read(product_definition, "CO_column_number_density_avk", harp_type_float, 2,
                                               dimension_type, NULL, description, "cm", NULL, read_co_column_avk);
}

static void register_ch4_product(void)
{
    const char *description;
    harp_ingestion_module *module;
    harp_product_definition *product_definition;

    harp_dimension_type dimension_type[3] = { harp_dimension_time, harp_dimension_vertical,
        harp_dimension_independent
    };
    long dimension[3] = { -1, -1, 2 };

    module = harp_ingestion_register_module_coda("S5P_L2_CH4", NULL, NULL, "Sentinel 5P L2 CH4 total column",
                                                 verify_ch4, ingestion_init, ingestion_done);

    product_definition = harp_ingestion_register_product(module, "S5P_L2_CH4", NULL, read_dimensions);
    register_core_variables(product_definition);
    register_geolocation_variables(product_definition);
    register_additional_geolocation_variables(product_definition);

    description = "altitude bounds per profile layer; altitude is measured as the vertical distance to the surface";
    harp_ingestion_register_variable_full_read(product_definition, "altitude_bounds_surface", harp_type_float, 3,
                                               dimension_type, dimension, description, "m", NULL, read_ch4_altitude);

    description = "pressure bounds per profile layer";
    harp_ingestion_register_variable_full_read(product_definition, "pressure_bounds", harp_type_float, 3,
                                               dimension_type, dimension, description, "hPa", NULL, read_ch4_pressure);

    description = "column averaged dry air mixing ratio of methane";
    harp_ingestion_register_variable_full_read(product_definition, "CH4_column_mass_mixing_ratio", harp_type_float, 1,
                                               dimension_type, NULL, description, "ng/g", NULL, read_ch4_column);

    description = "uncertainty of the column averaged dry air mixing ratio of methane (1 sigma error)";
    harp_ingestion_register_variable_full_read(product_definition, "CH4_column_mass_mixing_ratio_stdev",
                                               harp_type_float, 1, dimension_type, NULL, description, "ng/g", NULL,
                                               read_ch4_column_precision);

    description = "column averaging kernel for methane retrieval";
    harp_ingestion_register_variable_full_read(product_definition, "CH4_column_mass_mixing_ratio_avk", harp_type_float,
                                               2, dimension_type, NULL, description, NULL, NULL, read_ch4_column_avk);
}

static void register_so2_product(void)
{
    const char *description;
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_vertical };

    module = harp_ingestion_register_module_coda("S5P_L2_SO2", NULL, NULL, "Sentinel 5P L2 SO2 total column",
                                                 verify_so2, ingestion_init, ingestion_done);

    product_definition = harp_ingestion_register_product(module, "S5P_L2_SO2", NULL, read_dimensions);
    register_core_variables(product_definition);
    register_dlr_geolocation_variables(product_definition);
    register_additional_geolocation_variables(product_definition);

    description = "vertically integrated SO2 column density";
    harp_ingestion_register_variable_full_read(product_definition, "SO2_column_number_density", harp_type_float, 1,
                                               dimension_type, NULL, description, "mol/cm^2", NULL, read_so2_column);

    description = "averaging kernel for the vertically integrated SO2 column density";
    harp_ingestion_register_variable_full_read(product_definition, "SO2_column_number_density_avk", harp_type_float, 2,
                                               dimension_type, NULL, description, "cm", NULL, read_so2_column_avk);
}

static void register_o3_product(void)
{
    const char *description;
    harp_ingestion_module *module;
    harp_product_definition *product_definition;

    harp_dimension_type dimension_type[3] = { harp_dimension_time, harp_dimension_vertical,
        harp_dimension_independent
    };
    long dimension[3] = { -1, -1, 2 };

    module = harp_ingestion_register_module_coda("S5P_L2_O3", NULL, NULL, "Sentinel 5P L2 O3 total column", verify_o3,
                                                 ingestion_init, ingestion_done);

    product_definition = harp_ingestion_register_product(module, "S5P_L2_O3", NULL, read_dimensions);
    register_core_variables(product_definition);
    register_dlr_geolocation_variables(product_definition);
    register_additional_geolocation_variables(product_definition);

    description = "pressure bounds per profile layer";
    harp_ingestion_register_variable_full_read(product_definition, "pressure_bounds", harp_type_float, 3,
                                               dimension_type, dimension, description, "hPa", NULL, read_o3_pressure);

    description = "vertically integrated O3 column density";
    harp_ingestion_register_variable_full_read(product_definition, "O3_column_number_density", harp_type_float, 1,
                                               dimension_type, NULL, description, "mol/cm^2", NULL, read_o3_column);

    description = "uncertainty of the vertically integrated O3 column density (standard error)";
    harp_ingestion_register_variable_full_read(product_definition, "O3_column_number_density_stdev", harp_type_float, 1,
                                               dimension_type, NULL, description, "mol/cm^2", NULL,
                                               read_o3_column_precision);

    description = "averaging kernel for the vertically integrated O3 column density";
    harp_ingestion_register_variable_full_read(product_definition, "O3_column_number_density_avk", harp_type_float, 2,
                                               dimension_type, NULL, description, "cm", NULL, read_o3_column_avk);
}

static void register_hcho_product(void)
{
    const char *description;
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_vertical };

    module = harp_ingestion_register_module_coda("S5P_L2_HCHO", NULL, NULL, "Sentinel 5P L2 HCHO total column",
                                                 verify_hcho, ingestion_init, ingestion_done);

    product_definition = harp_ingestion_register_product(module, "S5P_L2_HCHO", NULL, read_dimensions);
    register_core_variables(product_definition);
    register_dlr_geolocation_variables(product_definition);
    register_additional_geolocation_variables(product_definition);

    description = "vertically integrated HCHO column density";
    harp_ingestion_register_variable_full_read(product_definition, "HCHO_column_number_density", harp_type_float, 1,
                                               dimension_type, NULL, description, "mol/cm^2", NULL, read_hcho_column);

    description = "averaging kernel for the vertically integrated HCHO column density";
    harp_ingestion_register_variable_full_read(product_definition, "HCHO_column_number_density_avk", harp_type_float, 2,
                                               dimension_type, NULL, description, "cm", NULL, read_hcho_column_avk);
}

#if 0
static void register_aer_lh_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };

    module = harp_ingestion_register_module_coda("S5P_L2_AER_LH", NULL, NULL, "Sentinel 5P L2 aerosol layer height",
                                                 verify_aer_lh, ingestion_init, ingestion_done);

    product_definition = harp_ingestion_register_product(module, "S5P_L2_AER_LH", NULL, read_dimensions);
    register_core_variables(product_definition);
    register_geolocation_variables(product_definition);
    register_additional_geolocation_variables(product_definition);
}
#endif

static void register_aer_ai_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;

    harp_dimension_type dimension_type[1] = { harp_dimension_time };
    const char *wavelength_ratio_option_values[2] = { "354_388nm", "340_380nm" };
    const char *description;

    module = harp_ingestion_register_module_coda("S5P_L2_AER_AI", NULL, NULL, "Sentinel 5P L2 aerosol index",
                                                 verify_aer_ai, ingestion_init_aer_ai, ingestion_done);

    description = "ingest aerosol index retrieved at wavelengths 354/388 nm, or 340/388 nm";
    harp_ingestion_register_option(module, "wavelength_ratio", description, 2, wavelength_ratio_option_values);

    product_definition = harp_ingestion_register_product(module, "S5P_L2_AER_AI", NULL, read_dimensions);
    register_core_variables(product_definition);
    register_geolocation_variables(product_definition);
    register_additional_geolocation_variables(product_definition);

    description = "aerosol index";
    harp_ingestion_register_variable_full_read(product_definition, "aerosol_index", harp_type_float, 1, dimension_type,
                                               NULL, description, "1", NULL, read_aerosol_index);
}

int harp_ingestion_module_s5p_l2_init(void)
{
    // register_o3_pr_product();
    // register_o3_tpr_product();
    register_no2_product();
    register_co_product();
    register_ch4_product();
    // register_aer_lh_product();
    register_aer_ai_product();
    register_fresco_product();
    register_so2_product();
    register_o3_product();
    register_hcho_product();

    return 0;
}
