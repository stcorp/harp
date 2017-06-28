/*
 * Copyright (C) 2015-2017 S[&]T, The Netherlands.
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef enum s5p_product_type_enum
{
    s5p_type_o3_pr,
    s5p_type_o3_tcl,
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
    s5p_dim_level,
} s5p_dimension_type;

#define S5P_NUM_DIM_TYPES (((int)s5p_dim_level) + 1)

static const char *s5p_dimension_name[S5P_NUM_PRODUCT_TYPES][S5P_NUM_DIM_TYPES] = {
    {"time", "scanline", "ground_pixel", "corner", NULL, "level"},
    {"time", NULL, NULL, NULL, NULL, NULL},
    {"time", "scanline", "ground_pixel", "corner", NULL, NULL},
    {"time", "scanline", "ground_pixel", "corner", "layer", NULL},
    {"time", "scanline", "ground_pixel", "corner", "layer", NULL},
    {"time", "scanline", "ground_pixel", "corner", "layer", "level"},
    {"time", "scanline", "ground_pixel", "corner", NULL, NULL},
    {"time", "scanline", "ground_pixel", "corner", NULL, NULL},
    {"time", "scanline", "ground_pixel", "corner", NULL, NULL},
    {"time", "scanline", "ground_pixel", "corner", NULL, NULL},
    {"time", "scanline", "ground_pixel", "corner", "layer", NULL},
    {"time", "scanline", "ground_pixel", "corner", "layer", "level"},
    {"time", "scanline", "ground_pixel", "corner", "layer", NULL}
};

static const int s5p_delta_time_num_dims[S5P_NUM_PRODUCT_TYPES] = { 2, 0, 2, 2, 2, 2, 2, 2, 3, 2, 3, 3, 3 };

typedef struct ingest_info_struct
{
    coda_product *product;
    int use_summed_total_column;
    int use_radiance_cloud_fraction;
    int use_ch4_bias_corrected;
    int use_o3_tcl_upper;
    int so2_column_type;        /* 0: total (tm5 profile), 1: 1km box profile, 2: 7km box profile, 3: 15km box profile */

    s5p_product_type product_type;
    long num_times;
    long num_scanlines;
    long num_pixels;
    long num_corners;
    long num_layers;
    long num_levels;
    long num_latitudes;
    long num_longitudes;

    coda_cursor product_cursor;
    coda_cursor geolocation_cursor;
    coda_cursor detailed_results_cursor;
    coda_cursor input_data_cursor;

    int wavelength_ratio;
    int is_nrti;
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
        case s5p_type_o3_tcl:
            return "L2__O3_TCL";
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
            return "L2__SO2___";
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

    if (info->product_type != s5p_type_o3_tcl)
    {
        if (coda_cursor_goto_record_field_by_name(&cursor, "GEOLOCATIONS") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        info->geolocation_cursor = cursor;
        coda_cursor_goto_parent(&cursor);
    }

    if (coda_cursor_goto_record_field_by_name(&cursor, "DETAILED_RESULTS") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    info->detailed_results_cursor = cursor;
    coda_cursor_goto_parent(&cursor);

    if (info->product_type != s5p_type_o3_tcl)
    {
        if (coda_cursor_goto_record_field_by_name(&cursor, "INPUT_DATA") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        info->input_data_cursor = cursor;
    }

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
        if (info->num_times != 1)
        {
            harp_set_error(HARP_ERROR_INGESTION, "dimension '%s' has length %ld; expected 1",
                           s5p_dimension_name[info->product_type][s5p_dim_time], info->num_times);
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
        if (info->num_corners != 4)
        {
            harp_set_error(HARP_ERROR_INGESTION, "dimension '%s' has length %ld; expected 4",
                           s5p_dimension_name[info->product_type][s5p_dim_corner], info->num_corners);
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

    if (info->product_type == s5p_type_o3_tcl)
    {
        if (info->use_o3_tcl_upper)
        {
            if (get_dimension_length(info, "lat", &info->num_latitudes) != 0)
            {
                return -1;
            }
            if (get_dimension_length(info, "lon", &info->num_longitudes) != 0)
            {
                return -1;
            }
        }
        else
        {
            if (get_dimension_length(info, "latitude", &info->num_latitudes) != 0)
            {
                return -1;
            }
            if (get_dimension_length(info, "longitude", &info->num_longitudes) != 0)
            {
                return -1;
            }
        }
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

static int init_processing_mode(ingest_info *info)
{
    coda_cursor cursor;
    char mode[5];

    if (info->product_type == s5p_type_o3_tcl)
    {
        /* The O3 TCL product has no ProcessingMode attribute, but we also don't care about it being NRTI or OFFL */
        return 0;
    }

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, "/METADATA/GRANULE_DESCRIPTION@ProcessingMode") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_string(&cursor, mode, 5) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (strcmp(mode, "NRTI") == 0)
    {
        info->is_nrti = 1;
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
    info->use_ch4_bias_corrected = 0;
    info->use_o3_tcl_upper = 0;
    info->so2_column_type = 0;
    info->num_times = 0;
    info->num_scanlines = 0;
    info->num_pixels = 0;
    info->num_corners = 0;
    info->num_layers = 0;
    info->num_levels = 0;
    info->wavelength_ratio = 354;
    info->is_nrti = 0;

    if (get_product_type(info->product, &info->product_type) != 0)
    {
        ingestion_done(info);
        return -1;
    }

    if (init_processing_mode(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }

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
    if (harp_ingestion_options_has_option(options, "ch4"))
    {
        info->use_ch4_bias_corrected = 1;
    }
    if (harp_ingestion_options_has_option(options, "o3"))
    {
        info->use_o3_tcl_upper = 1;
    }
    if (harp_ingestion_options_has_option(options, "so2_column"))
    {
        if (!info->is_nrti)
        {
            /* alternative SO2 columns are only available for NRTI, so return an empty product if not NRTI */
            /* (i.e. just pick the first definition and leave num_times set to 0) */
            *definition = *module->product_definition;
            *user_data = info;
            return 0;
        }
        if (harp_ingestion_options_get_option(options, "so2_column", &option_value) != 0)
        {
            ingestion_done(info);
            return -1;
        }
        if (strcmp(option_value, "1km") == 0)
        {
            info->so2_column_type = 1;
        }
        else if (strcmp(option_value, "7km") == 0)
        {
            info->so2_column_type = 2;
        }
        else if (strcmp(option_value, "15km") == 0)
        {
            info->so2_column_type = 3;
        }
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
    if (strcmp(module->name, "S5P_L2_CLOUD") == 0)
    {
        if (harp_ingestion_options_has_option(options, "model"))
        {
            if (harp_ingestion_options_get_option(options, "model", &option_value) != 0)
            {
                ingestion_done(info);
                return -1;
            }
            if (strcmp(option_value, "CRB") == 0)
            {
                /* use the seond product definition, which is the one for the CRB model */
                *definition = module->product_definition[1];
            }
        }
    }
    *user_data = info;

    return 0;
}

static int read_double_attribute(coda_cursor cursor, const char *dataset_name, const char *attribute, double *value)
{
    if (coda_cursor_goto_record_field_by_name(&cursor, dataset_name) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_attributes(&cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&cursor, attribute) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_first_array_element(&cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_double(&cursor, value) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

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
            if (coda_cursor_read_int32_array(&cursor, data.int32_data, coda_array_ordering_c) != 0)
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

    if (info->product_type == s5p_type_o3_tcl)
    {
        dimension[harp_dimension_time] = info->num_times;
    }
    else
    {
        dimension[harp_dimension_time] = info->num_times * info->num_scanlines * info->num_pixels;
    }
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
        case s5p_type_o3_tcl:
            dimension[harp_dimension_latitude] = info->num_latitudes;
            dimension[harp_dimension_longitude] = info->num_longitudes;
            break;
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
    if (s5p_delta_time_num_dims[info->product_type] == 2)
    {
        if (read_dataset(info->product_cursor, "delta_time", harp_type_double, info->num_scanlines, data) != 0)
        {
            return -1;
        }
        /* Broadcast the result along the pixel dimension. */
        broadcast_array_double(info->num_scanlines, info->num_pixels, data.double_data);
    }
    else
    {
        if (read_dataset(info->product_cursor, "delta_time", harp_type_double, info->num_scanlines * info->num_pixels,
                         data) != 0)
        {
            return -1;
        }
    }

    /* Convert observation start time to seconds since 2010-01-01 */
    for (i = 0; i < info->num_scanlines * info->num_pixels; i++)
    {
        data.double_data[i] = time_reference + data.double_data[i] / 1e3;
    }


    return 0;
}

static int read_time_coverage_resolution(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char string_value[32];
    coda_cursor cursor;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, "@time_coverage_resolution") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_string(&cursor, string_value, 32) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (sscanf(string_value, "PT%lfS", data.double_data) != 1)
    {
        harp_set_error(HARP_ERROR_INGESTION, "could not extract value from time_coverage_resolution attribute ('%s')",
                       string_value);
        return -1;
    }
    return 0;
}

static int read_geolocation_latitude_bounds(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->geolocation_cursor, "latitude_bounds", harp_type_float,
                        info->num_scanlines * info->num_pixels * info->num_corners, data);
}

static int read_geolocation_longitude_bounds(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->geolocation_cursor, "longitude_bounds", harp_type_float,
                        info->num_scanlines * info->num_pixels * info->num_corners, data);
}

static int read_geolocation_satellite_altitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (read_dataset(info->geolocation_cursor, "satellite_altitude", harp_type_float, info->num_scanlines, data) != 0)
    {
        return -1;
    }

    broadcast_array_float(info->num_scanlines, info->num_pixels, data.float_data);

    return 0;
}

static int read_geolocation_satellite_latitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (read_dataset(info->geolocation_cursor, "satellite_latitude", harp_type_float, info->num_scanlines, data) != 0)
    {
        return -1;
    }

    broadcast_array_float(info->num_scanlines, info->num_pixels, data.float_data);

    return 0;
}


static int read_geolocation_satellite_longitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (read_dataset(info->geolocation_cursor, "satellite_longitude", harp_type_float, info->num_scanlines, data) != 0)
    {
        return -1;
    }

    broadcast_array_float(info->num_scanlines, info->num_pixels, data.float_data);

    return 0;
}

static int read_geolocation_solar_azimuth_angle(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->geolocation_cursor, "solar_azimuth_angle", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_geolocation_solar_zenith_angle(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->geolocation_cursor, "solar_zenith_angle", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_geolocation_viewing_azimuth_angle(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->geolocation_cursor, "viewing_azimuth_angle", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_geolocation_viewing_zenith_angle(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->geolocation_cursor, "viewing_zenith_angle", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_input_aerosol_index_354_388(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "aerosol_index_354_388", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_input_altitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "altitude", harp_type_float,
                        info->num_scanlines * info->num_pixels * info->num_levels, data);
}

static int read_input_altitude_bounds(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    harp_array surfalt_data;
    long num_layers;
    long i, j;

    if (read_dataset(info->input_data_cursor, "height_levels", harp_type_float,
                     info->num_scanlines * info->num_pixels * info->num_levels, data) != 0)
    {
        return -1;
    }
    /* read surface altitude in buffer after height levels (which will fit if 2*num_layers>num_levels) */
    assert(info->num_layers * 2 > info->num_levels);
    surfalt_data.float_data = &data.float_data[info->num_scanlines * info->num_pixels * info->num_levels];
    if (read_dataset(info->input_data_cursor, "surface_altitude", harp_type_float,
                     info->num_scanlines * info->num_pixels, surfalt_data) != 0)
    {
        return -1;
    }
    /* convert level heights to level altitudes using surface altitude */
    for (i = 0; i < info->num_scanlines * info->num_pixels; i++)
    {
        for (j = 0; j < info->num_levels; j++)
        {
            data.float_data[i * info->num_levels + j] += surfalt_data.float_data[i];
        }
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

        for (j = num_layers - 1; j >= 0; --j)
        {
            /* NB. The order of the following two lines is important to ensure correct results. */
            altitude_bounds[j * 2 + 1] = altitude[j + 1];
            altitude_bounds[j * 2] = altitude[j];
        }
    }

    return 0;
}

static int read_input_cloud_albedo_crb(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "cloud_albedo_crb", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_input_cloud_albedo_crb_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "cloud_albedo_crb_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_input_cloud_base_height(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "cloud_base_height", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_input_cloud_base_height_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "cloud_base_height_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_input_cloud_base_pressure(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "cloud_base_pressure", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_input_cloud_base_pressure_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "cloud_base_pressure_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_input_cloud_height_crb(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "cloud_height_crb", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_input_cloud_height_crb_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "cloud_height_crb_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_input_cloud_optical_thickness(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "cloud_optical_thickness", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_input_cloud_optical_thickness_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "cloud_optical_thickness_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_input_cloud_pressure_crb(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "cloud_pressure_crb", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_input_cloud_pressure_crb_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "cloud_pressure_crb_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_input_cloud_top_height(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "cloud_top_height", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_input_cloud_top_height_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "cloud_top_height_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_input_cloud_top_pressure(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "cloud_top_pressure", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_input_cloud_top_pressure_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "cloud_top_pressure_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_input_ozone_total_vertical_column(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "ozone_total_vertical_column", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_input_ozone_total_vertical_column_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "ozone_total_vertical_column_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_input_ozone_profile_apriori(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "ozone_profile_apriori", harp_type_float,
                        info->num_scanlines * info->num_pixels * info->num_levels, data);
}

static int read_input_ozone_profile_apriori_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "ozone_profile_apriori_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels * info->num_levels, data);
}

static int read_input_ozone_profile_apriori_covariance(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long num_elements = info->num_scanlines * info->num_pixels;
    long num_levels = info->num_levels;
    harp_array altitude;
    harp_array stddev;
    double correlation_length;
    long i, j, k;

    if (read_double_attribute(info->input_data_cursor, "ozone_profile_apriori_precision", "correlation_length",
                              &correlation_length) != 0)
    {
        return -1;
    }

    altitude.float_data = malloc(num_elements * num_levels * sizeof(float));
    if (altitude.float_data == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_elements * num_levels * sizeof(float), __FILE__, __LINE__);
        return -1;
    }
    if (read_input_altitude(user_data, altitude) != 0)
    {
        free(altitude.float_data);
        return -1;
    }

    stddev.float_data = malloc(num_elements * num_levels * sizeof(float));
    if (stddev.float_data == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_elements * num_levels * sizeof(float), __FILE__, __LINE__);
        free(altitude.float_data);
        return -1;
    }
    if (read_input_ozone_profile_apriori_uncertainty(user_data, stddev) != 0)
    {
        free(altitude.float_data);
        free(stddev.float_data);
        return -1;
    }

    for (i = 0; i < num_elements; i++)
    {
        long offset = i * num_levels * num_levels;

        for (j = 0; j < num_levels; j++)
        {
            for (k = 0; k < num_levels; k++)
            {
                double value;

                if (k <= j)
                {
                    value = data.float_data[offset + j] * data.float_data[offset + k];
                    if (k < j)
                    {
                        value *= exp(-fabs(altitude.float_data[j] - altitude.float_data[k]) / correlation_length);
                    }
                }
                else
                {
                    /* since the matrix is symetric, use the value from the lower triangular part of the matrix */
                    value = data.float_data[offset + k * num_levels + j];
                }
                data.float_data[offset + j * num_levels + k] = (float)value;
            }
        }
    }

    free(altitude.float_data);
    free(stddev.float_data);

    return 0;
}

static int read_input_pressure(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "pressure", harp_type_float,
                        info->num_scanlines * info->num_pixels * info->num_levels, data);
}

static int read_input_pressure_at_tropopause(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "pressure_at_tropopause", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_input_pressure_bounds(void *user_data, harp_array data)
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

    if (read_dataset(info->input_data_cursor, "pressure_interval", harp_type_double, num_elements, delta_pressure) != 0)
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

static int read_input_surface_albedo(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "surface_albedo", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_input_surface_albedo_nitrogendioxide_window(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "surface_albedo_nitrogendioxide_window", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_input_surface_altitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "surface_altitude", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_input_surface_altitude_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "surface_altitude_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_input_surface_pressure(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "surface_pressure", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_input_temperature(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "temperature", harp_type_float,
                        info->num_scanlines * info->num_pixels * info->num_levels, data);
}

static int read_product_air_mass_factor_total(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "air_mass_factor_total", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_product_air_mass_factor_troposphere(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "air_mass_factor_troposphere", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_product_aerosol_index(void *user_data, harp_array data)
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

static int read_product_aerosol_index_precision(void *user_data, harp_array data)
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

static int read_product_aerosol_mid_height(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "aerosol_mid_height", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_product_aerosol_mid_height_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "aerosol_mid_height_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_product_aerosol_mid_pressure(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "aerosol_mid_pressure", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_product_aerosol_mid_pressure_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "aerosol_mid_pressure_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_product_averaging_kernel(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "averaging_kernel", harp_type_float,
                        info->num_scanlines * info->num_pixels * info->num_layers, data);
}

static int read_product_carbonmonoxide_total_column(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "carbonmonoxide_total_column", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_product_carbonmonoxide_total_column_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "carbonmonoxide_total_column_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_product_cloud_albedo_crb(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "cloud_albedo_crb", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_product_cloud_albedo_crb_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "cloud_albedo_crb_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_product_cloud_base_height(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "cloud_base_height", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_product_cloud_base_height_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "cloud_base_height_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_product_cloud_base_pressure(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "cloud_base_pressure", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_product_cloud_base_pressure_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "cloud_base_pressure_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_product_cloud_fraction(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "cloud_fraction", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_product_cloud_fraction_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "cloud_fraction_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_product_cloud_fraction_crb(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "cloud_fraction_crb", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_product_cloud_fraction_crb_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "cloud_fraction_crb_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_product_cloud_height_crb(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "cloud_height_crb", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_product_cloud_height_crb_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "cloud_height_crb_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_product_cloud_optical_thickness(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "cloud_optical_thickness", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_product_cloud_optical_thickness_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "cloud_optical_thickness_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_product_cloud_pressure_crb(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "cloud_pressure_crb", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_product_cloud_pressure_crb_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "cloud_pressure_crb_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_product_cloud_top_height(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "cloud_top_height", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_product_cloud_top_height_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "cloud_top_height_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_product_cloud_top_pressure(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "cloud_top_pressure", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_product_cloud_top_pressure_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "cloud_top_pressure_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_product_formaldehyde_tropospheric_vertical_column(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "formaldehyde_tropospheric_vertical_column", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_product_formaldehyde_tropospheric_vertical_column_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "formaldehyde_tropospheric_vertical_column_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_product_layer(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "layer", harp_type_float, info->num_layers, data);
}

static int read_product_latitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "latitude", harp_type_float, info->num_scanlines * info->num_pixels,
                        data);
}

static int read_product_longitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "longitude", harp_type_float, info->num_scanlines * info->num_pixels,
                        data);
}

static int read_product_methane_mixing_ratio_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "methane_mixing_ratio_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_product_nitrogendioxide_tropospheric_column(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "nitrogendioxide_tropospheric_column", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_product_nitrogendioxide_tropospheric_column_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "nitrogendioxide_tropospheric_column_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_product_ozone_profile(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "ozone_profile", harp_type_float,
                        info->num_scanlines * info->num_pixels * info->num_levels, data);
}

static int read_product_ozone_profile_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "ozone_profile_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels * info->num_levels, data);
}

static int read_product_ozone_total_column(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "ozone_total_column", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_product_ozone_total_column_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "ozone_total_column_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_product_ozone_total_vertical_column(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "ozone_total_vertical_column", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_product_ozone_total_vertical_column_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "ozone_total_vertical_column_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_product_ozone_tropospheric_column(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "ozone_tropospheric_column", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_product_ozone_tropospheric_column_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "ozone_tropospheric_column_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_results_aerosol_optical_thickness(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "aerosol_optical_thickness", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_results_aerosol_optical_thickness_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "aerosol_optical_thickness_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_results_air_mass_factor_stratosphere(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "air_mass_factor_stratosphere", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_results_averaging_kernel_2d(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "averaging_kernel", harp_type_float,
                        info->num_scanlines * info->num_pixels * info->num_levels * info->num_levels, data);
}

static int read_results_averaging_kernel_1d(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "averaging_kernel", harp_type_float,
                        info->num_scanlines * info->num_pixels * info->num_layers, data);
}

static int read_results_cloud_albedo_crb(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "cloud_albedo_crb", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_results_cloud_albedo_crb_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "cloud_albedo_crb_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_results_cloud_fraction_apriori(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "cloud_fraction_apriori", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_results_cloud_fraction_crb(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "cloud_fraction_crb", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_results_cloud_fraction_crb_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "cloud_fraction_crb_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_results_cloud_fraction_nitrogendioxide_window(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->use_radiance_cloud_fraction)
    {
        return read_dataset(info->detailed_results_cursor, "cloud_radiance_fraction_nitrogendioxide_window",
                            harp_type_float, info->num_scanlines * info->num_pixels, data);
    }
    return read_dataset(info->detailed_results_cursor, "cloud_fraction_crb_nitrogendioxide_window", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_results_cloud_height_crb(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "cloud_height_crb", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_results_cloud_height_crb_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "cloud_height_crb_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_results_cloud_pressure_crb(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "cloud_pressure_crb", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_results_cloud_pressure_crb_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "cloud_pressure_crb_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_results_column_averaging_kernel(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "column_averaging_kernel", harp_type_float,
                        info->num_scanlines * info->num_pixels * info->num_layers, data);
}

static int read_results_formaldehyde_profile_apriori(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "formaldehyde_profile_apriori", harp_type_float,
                        info->num_scanlines * info->num_pixels * info->num_layers, data);
}

static int read_results_formaldehyde_tropospheric_air_mass_factor(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "formaldehyde_tropospheric_air_mass_factor", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_results_formaldehyde_tropospheric_air_mass_factor_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "formaldehyde_tropospheric_air_mass_factor_precision",
                        harp_type_float, info->num_scanlines * info->num_pixels, data);
}

static int read_results_formaldehyde_tropospheric_air_mass_factor_trueness(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "formaldehyde_tropospheric_air_mass_factor_trueness",
                        harp_type_float, info->num_scanlines * info->num_pixels, data);
}

static int read_results_formaldehyde_tropospheric_vertical_column_trueness(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "formaldehyde_tropospheric_vertical_column_trueness",
                        harp_type_float, info->num_scanlines * info->num_pixels, data);
}

static int read_results_nitrogendioxide_stratospheric_column(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "nitrogendioxide_stratospheric_column", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_results_nitrogendioxide_stratospheric_column_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "nitrogendioxide_stratospheric_column_precision",
                        harp_type_float, info->num_scanlines * info->num_pixels, data);
}

static int read_results_ozone_total_air_mass_factor(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "ozone_total_air_mass_factor", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_results_ozone_total_air_mass_factor_trueness(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "ozone_total_air_mass_factor_trueness", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_results_ozone_profile_apriori(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "ozone_profile_apriori", harp_type_float,
                        info->num_scanlines * info->num_pixels * info->num_layers, data);
}

static int read_results_ozone_profile_error_covariance_matrix(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "ozone_profile_error_covariance_matrix", harp_type_float,
                        info->num_scanlines * info->num_pixels * info->num_levels * info->num_levels, data);
}

static int read_results_pressure_bounds(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long num_layers;
    long i;

    if (read_dataset(info->detailed_results_cursor, "pressure_grid", harp_type_float,
                     info->num_scanlines * info->num_pixels * info->num_levels, data) != 0)
    {
        return -1;
    }

    /* Convert from #levels (== #layers + 1) consecutive pressures to #layers x 2 pressure bounds.
     * Iterate in reverse to ensure correct results (conversion is performed in place).
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

static int read_results_pressure_levels(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "pressure_levels", harp_type_float,
                        info->num_scanlines * info->num_pixels * info->num_layers, data);
}

static int read_results_processing_quality_flags(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    coda_cursor cursor = info->detailed_results_cursor;
    long coda_num_elements;

    if (coda_cursor_goto_record_field_by_name(&cursor, "processing_quality_flags") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &coda_num_elements) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_num_elements != info->num_scanlines * info->num_pixels)
    {
        harp_set_error(HARP_ERROR_INGESTION, "dataset has %ld elements; expected %ld", coda_num_elements,
                       info->num_scanlines * info->num_pixels);
        harp_add_coda_cursor_path_to_error_message(&cursor);
        return -1;
    }

    if (coda_cursor_read_uint32_array(&cursor, (uint32_t *)data.int32_data, coda_array_ordering_c) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    return 0;
}

static int read_results_sulfurdioxide_profile_apriori(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "sulfurdioxide_profile_apriori", harp_type_float,
                        info->num_scanlines * info->num_pixels * info->num_layers, data);
}

static int read_results_surface_albedo_fitted(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "surface_albedo_fitted", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_results_surface_albedo_fitted_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "surface_albedo_fitted_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_results_surface_albedo_fitted_crb(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "surface_albedo_fitted_crb", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_results_surface_albedo_fitted_crb_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "surface_albedo_fitted_crb_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_ch4_methane_mixing_ratio(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->use_ch4_bias_corrected)
    {
        return read_dataset(info->product_cursor, "methane_mixing_ratio_bias_corrected", harp_type_float,
                            info->num_scanlines * info->num_pixels, data);
    }
    return read_dataset(info->product_cursor, "methane_mixing_ratio", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_hcho_cloud_fraction(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->use_radiance_cloud_fraction)
    {
        return read_dataset(info->detailed_results_cursor, "cloud_fraction_intensity_weighted",
                            harp_type_float, info->num_scanlines * info->num_pixels, data);
    }
    return read_dataset(info->input_data_cursor, "cloud_fraction_crb", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_hcho_cloud_fraction_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->use_radiance_cloud_fraction)
    {
        return read_dataset(info->detailed_results_cursor, "cloud_fraction_intensity_weighted_precision",
                            harp_type_float, info->num_scanlines * info->num_pixels, data);
    }
    return read_dataset(info->input_data_cursor, "cloud_fraction_crb_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_o3_cloud_fraction(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, info->is_nrti ? "cloud_fraction" : "cloud_fraction_crb",
                        harp_type_float, info->num_scanlines * info->num_pixels, data);
}

static int read_o3_cloud_fraction_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor,
                        info->is_nrti ? "cloud_fraction_precision" : "cloud_fraction_crb_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_o3_tcl_time_value(void *user_data, const char *path, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char value[50];
    coda_cursor cursor;

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
    if (coda_cursor_read_string(&cursor, value, 50) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_time_string_to_double("yyyy-MM-dd'T'HH:mm:ss", value, data.double_data) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    return 0;
}

static int read_o3_tcl_datetime_start(void *user_data, harp_array data)
{
    return read_o3_tcl_time_value(user_data, "/@time_coverage_start", data);
}

static int read_o3_tcl_datetime_stop(void *user_data, harp_array data)
{
    return read_o3_tcl_time_value(user_data, "/@time_coverage_end", data);
}

static int read_o3_tcl_latitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->use_o3_tcl_upper)
    {
        return read_dataset(info->product_cursor, "lat", harp_type_float, info->num_latitudes, data);
    }
    return read_dataset(info->product_cursor, "latitude", harp_type_float, info->num_latitudes, data);
}

static int read_o3_tcl_longitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->use_o3_tcl_upper)
    {
        return read_dataset(info->product_cursor, "lon", harp_type_float, info->num_longitudes, data);
    }
    return read_dataset(info->product_cursor, "longitude", harp_type_float, info->num_longitudes, data);
}

static int read_o3_tcl_ozone_tropospheric_mixing_ratio(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->use_o3_tcl_upper)
    {
        return read_dataset(info->product_cursor, "ozone_upper_tropospheric_mixing_ratio", harp_type_float,
                            info->num_latitudes * info->num_longitudes, data);
    }
    return read_dataset(info->product_cursor, "ozone_tropospheric_mixing_ratio", harp_type_float,
                        info->num_latitudes * info->num_longitudes, data);
}

static int read_o3_tcl_ozone_tropospheric_mixing_ratio_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->use_o3_tcl_upper)
    {
        return read_dataset(info->product_cursor, "ozone_upper_tropospheric_mixing_ratio_precision", harp_type_float,
                            info->num_latitudes * info->num_longitudes, data);
    }
    return read_dataset(info->product_cursor, "ozone_tropospheric_mixing_ratio_precision", harp_type_float,
                        info->num_latitudes * info->num_longitudes, data);
}

static int read_o3_tcl_ozone_tropospheric_mixing_ratio_flag(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "ozone_upper_tropospheric_mixing_ratio_flag", harp_type_int32,
                        info->num_latitudes * info->num_longitudes, data);
}

static int read_o3_tcl_ozone_tropospheric_vertical_column(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "ozone_tropospheric_vertical_column", harp_type_float,
                        info->num_latitudes * info->num_longitudes, data);
}

static int read_o3_tcl_ozone_tropospheric_vertical_column_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "ozone_tropospheric_vertical_column_precision", harp_type_float,
                        info->num_latitudes * info->num_longitudes, data);
}

static int read_no2_column(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char *variable_name;

    variable_name = info->use_summed_total_column ? "nitrogendioxide_summed_total_column" :
        "nitrogendioxide_total_column";
    return read_dataset(info->detailed_results_cursor, variable_name, harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_no2_column_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char *variable_name;

    variable_name = info->use_summed_total_column ? "nitrogendioxide_summed_total_column_precision" :
        "nitrogendioxide_total_column_precision";
    return read_dataset(info->detailed_results_cursor, variable_name, harp_type_float,
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
    if (read_product_air_mass_factor_total(user_data, amf_data) != 0)
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

    if (read_results_air_mass_factor_stratosphere(user_data, amf_data) != 0)
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
    if (read_product_air_mass_factor_total(user_data, amf_data) != 0)
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

    if (read_product_air_mass_factor_troposphere(user_data, amf_data) != 0)
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

static int read_no2_pressure_bounds(void *user_data, harp_array data)
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

    if (read_dataset(info->product_cursor, "tm5_constant_a", harp_type_double, num_layers * 2, hybride_coef_a) != 0)
    {
        free(hybride_coef_b.ptr);
        free(hybride_coef_a.ptr);
        return -1;
    }

    if (read_dataset(info->product_cursor, "tm5_constant_b", harp_type_double, num_layers * 2, hybride_coef_b) != 0)
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
        double surface_pressure = data.double_data[i];  /* surface pressure at specific (time, lat, lon) */
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

static int read_no2_tropopause_pressure(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    harp_array hybride_coef_a;
    harp_array hybride_coef_b;
    harp_array layer_index;
    long num_profiles;
    long num_layers;
    long i;

    num_profiles = info->num_scanlines * info->num_pixels;
    num_layers = info->num_layers;

    layer_index.ptr = malloc(num_profiles * sizeof(int32_t));
    if (layer_index.ptr == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_profiles * sizeof(int32_t), __FILE__, __LINE__);
        return -1;
    }

    hybride_coef_a.ptr = malloc(num_layers * 2 * sizeof(double));
    if (hybride_coef_a.ptr == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_layers * 2 * sizeof(double), __FILE__, __LINE__);
        free(layer_index.ptr);
        return -1;
    }

    hybride_coef_b.ptr = malloc(num_layers * 2 * sizeof(double));
    if (hybride_coef_b.ptr == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_layers * 2 * sizeof(double), __FILE__, __LINE__);
        free(hybride_coef_a.ptr);
        free(layer_index.ptr);
        return -1;
    }

    if (read_dataset(info->product_cursor, "tm5_tropopause_layer_index", harp_type_int32, num_profiles, layer_index) !=
        0)
    {
        free(hybride_coef_b.ptr);
        free(hybride_coef_a.ptr);
        free(layer_index.ptr);
        return -1;
    }

    if (read_dataset(info->product_cursor, "tm5_constant_a", harp_type_double, num_layers * 2, hybride_coef_a) != 0)
    {
        free(hybride_coef_b.ptr);
        free(hybride_coef_a.ptr);
        free(layer_index.ptr);
        return -1;
    }

    if (read_dataset(info->product_cursor, "tm5_constant_b", harp_type_double, num_layers * 2, hybride_coef_b) != 0)
    {
        free(hybride_coef_b.ptr);
        free(hybride_coef_a.ptr);
        free(layer_index.ptr);
        return -1;
    }

    if (read_dataset(info->input_data_cursor, "surface_pressure", harp_type_double, num_profiles, data) != 0)
    {
        free(hybride_coef_b.ptr);
        free(hybride_coef_a.ptr);
        free(layer_index.ptr);
        return -1;
    }

    for (i = 0; i < num_profiles; i++)
    {
        long index = layer_index.int32_data[i];

        if (index >= 0 && index < num_layers)
        {
            double surface_pressure = data.double_data[i];      /* surface pressure at specific (time, lat, lon) */

            /* the tropause level is the upper boundary of the layer defined by layer_index */
            data.double_data[i] = hybride_coef_a.double_data[index * 2 + 1] +
                hybride_coef_b.double_data[index * 2 + 1] * surface_pressure;
        }
        else
        {
            data.double_data[i] = harp_nan();
        }
    }

    free(hybride_coef_b.ptr);
    free(hybride_coef_a.ptr);
    free(layer_index.ptr);

    return 0;
}

static int read_so2_total_air_mass_factor(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    switch (info->so2_column_type)
    {
        case 0:
            return read_dataset(info->detailed_results_cursor, "sulfurdioxide_total_air_mass_factor_polluted",
                                harp_type_float, info->num_scanlines * info->num_pixels, data);
        case 1:
            return read_dataset(info->detailed_results_cursor, "sulfurdioxide_total_air_mass_factor_1km",
                                harp_type_float, info->num_scanlines * info->num_pixels, data);
        case 2:
            return read_dataset(info->detailed_results_cursor, "sulfurdioxide_total_air_mass_factor_7km",
                                harp_type_float, info->num_scanlines * info->num_pixels, data);
        case 3:
            return read_dataset(info->detailed_results_cursor, "sulfurdioxide_total_air_mass_factor_15km",
                                harp_type_float, info->num_scanlines * info->num_pixels, data);
    }

    assert(0);
    exit(1);
}

static int read_so2_total_air_mass_factor_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    switch (info->so2_column_type)
    {
        case 0:
            return read_dataset(info->detailed_results_cursor, "sulfurdioxide_total_air_mass_factor_polluted_precision",
                                harp_type_float, info->num_scanlines * info->num_pixels, data);
        case 1:
            return read_dataset(info->detailed_results_cursor, "sulfurdioxide_total_air_mass_factor_1km_precision",
                                harp_type_float, info->num_scanlines * info->num_pixels, data);
        case 2:
            return read_dataset(info->detailed_results_cursor, "sulfurdioxide_total_air_mass_factor_7km_precision",
                                harp_type_float, info->num_scanlines * info->num_pixels, data);
        case 3:
            return read_dataset(info->detailed_results_cursor, "sulfurdioxide_total_air_mass_factor_15km_precision",
                                harp_type_float, info->num_scanlines * info->num_pixels, data);
    }

    assert(0);
    exit(1);
}

static int read_so2_total_air_mass_factor_trueness(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    switch (info->so2_column_type)
    {
        case 0:
            return read_dataset(info->detailed_results_cursor, "sulfurdioxide_total_air_mass_factor_polluted_trueness",
                                harp_type_float, info->num_scanlines * info->num_pixels, data);
        case 1:
            return read_dataset(info->detailed_results_cursor, "sulfurdioxide_total_air_mass_factor_1km_trueness",
                                harp_type_float, info->num_scanlines * info->num_pixels, data);
        case 2:
            return read_dataset(info->detailed_results_cursor, "sulfurdioxide_total_air_mass_factor_7km_trueness",
                                harp_type_float, info->num_scanlines * info->num_pixels, data);
        case 3:
            return read_dataset(info->detailed_results_cursor, "sulfurdioxide_total_air_mass_factor_15km_trueness",
                                harp_type_float, info->num_scanlines * info->num_pixels, data);
    }

    assert(0);
    exit(1);
}

static int read_so2_total_vertical_column(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    switch (info->so2_column_type)
    {
        case 0:
            return read_dataset(info->product_cursor, "sulfurdioxide_total_vertical_column", harp_type_float,
                                info->num_scanlines * info->num_pixels, data);
        case 1:
            return read_dataset(info->detailed_results_cursor, "sulfurdioxide_total_vertical_column_1km",
                                harp_type_float, info->num_scanlines * info->num_pixels, data);
        case 2:
            return read_dataset(info->detailed_results_cursor, "sulfurdioxide_total_vertical_column_7km",
                                harp_type_float, info->num_scanlines * info->num_pixels, data);
        case 3:
            return read_dataset(info->detailed_results_cursor, "sulfurdioxide_total_vertical_column_15km",
                                harp_type_float, info->num_scanlines * info->num_pixels, data);
    }

    assert(0);
    exit(1);
}

static int read_so2_total_vertical_column_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    switch (info->so2_column_type)
    {
        case 0:
            return read_dataset(info->product_cursor, "sulfurdioxide_total_vertical_column_precision", harp_type_float,
                                info->num_scanlines * info->num_pixels, data);
        case 1:
            return read_dataset(info->detailed_results_cursor, "sulfurdioxide_total_vertical_column_1km_precision",
                                harp_type_float, info->num_scanlines * info->num_pixels, data);
        case 2:
            return read_dataset(info->detailed_results_cursor, "sulfurdioxide_total_vertical_column_7km_precision",
                                harp_type_float, info->num_scanlines * info->num_pixels, data);
        case 3:
            return read_dataset(info->detailed_results_cursor, "sulfurdioxide_total_vertical_column_15km_precision",
                                harp_type_float, info->num_scanlines * info->num_pixels, data);
    }

    assert(0);
    exit(1);
}

static int read_so2_total_vertical_column_trueness(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    switch (info->so2_column_type)
    {
        case 0:
            return read_dataset(info->detailed_results_cursor, "sulfurdioxide_total_vertical_column_trueness",
                                harp_type_float, info->num_scanlines * info->num_pixels, data);
        case 1:
            return read_dataset(info->detailed_results_cursor, "sulfurdioxide_total_vertical_column_1km_trueness",
                                harp_type_float, info->num_scanlines * info->num_pixels, data);
        case 2:
            return read_dataset(info->detailed_results_cursor, "sulfurdioxide_total_vertical_column_7km_trueness",
                                harp_type_float, info->num_scanlines * info->num_pixels, data);
        case 3:
            return read_dataset(info->detailed_results_cursor, "sulfurdioxide_total_vertical_column_15km_trueness",
                                harp_type_float, info->num_scanlines * info->num_pixels, data);
    }

    assert(0);
    exit(1);
}


static int read_so2_surface_albedo(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    harp_array surface_albedo_328;
    harp_array surface_albedo_376;
    harp_array selected_fitting_window_flag;
    long num_elements = info->num_scanlines * info->num_pixels;
    long i;

    surface_albedo_328.ptr = malloc(num_elements * sizeof(float));
    if (surface_albedo_328.ptr == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_elements * sizeof(float), __FILE__, __LINE__);
        return -1;
    }

    surface_albedo_376.ptr = malloc(num_elements * sizeof(float));
    if (surface_albedo_376.ptr == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_elements * sizeof(float), __FILE__, __LINE__);
        free(surface_albedo_328.ptr);
        return -1;
    }

    selected_fitting_window_flag.ptr = malloc(num_elements * sizeof(int32_t));
    if (selected_fitting_window_flag.ptr == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_elements * sizeof(int32_t), __FILE__, __LINE__);
        free(surface_albedo_328.ptr);
        free(surface_albedo_376.ptr);
        return -1;
    }

    if (read_dataset(info->input_data_cursor, "surface_albedo_328nm", harp_type_float, num_elements, surface_albedo_328)
        != 0)
    {
        free(surface_albedo_328.ptr);
        free(surface_albedo_376.ptr);
        free(selected_fitting_window_flag.ptr);
        return -1;
    }

    if (read_dataset(info->input_data_cursor, "surface_albedo_376nm", harp_type_float, num_elements, surface_albedo_376)
        != 0)
    {
        free(surface_albedo_328.ptr);
        free(surface_albedo_376.ptr);
        free(selected_fitting_window_flag.ptr);
        return -1;
    }

    if (read_dataset(info->detailed_results_cursor, "selected_fitting_window_flag", harp_type_int32, num_elements,
                     selected_fitting_window_flag) != 0)
    {
        free(surface_albedo_328.ptr);
        free(surface_albedo_376.ptr);
        free(selected_fitting_window_flag.ptr);
        return -1;
    }

    for (i = 0; i < num_elements; i++)
    {
        int flag = (int)selected_fitting_window_flag.int32_data[i];

        if (flag == 1 || flag == 2)
        {
            data.float_data[i] = surface_albedo_328.float_data[i];
        }
        else if (flag == 3)
        {
            data.float_data[i] = surface_albedo_376.float_data[i];
        }
        else
        {
            data.float_data[i] = (float)harp_nan();
        }
    }

    free(surface_albedo_328.ptr);
    free(surface_albedo_376.ptr);
    free(selected_fitting_window_flag.ptr);

    return 0;
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

static int exclude_non_nrti(void *user_data)
{
    return !((ingest_info *)user_data)->is_nrti;
}

static int exclude_nrti(void *user_data)
{
    return ((ingest_info *)user_data)->is_nrti;
}

static int exclude_o3_tcl(void *user_data)
{
    return !((ingest_info *)user_data)->use_o3_tcl_upper;
}

static int exclude_o3_tcl_upper(void *user_data)
{
    return ((ingest_info *)user_data)->use_o3_tcl_upper;
}

static int exclude_so2_apriori_profile(void *user_data)
{
    return ((ingest_info *)user_data)->so2_column_type != 0;
}

static void register_core_variables(harp_product_definition *product_definition, int delta_time_num_dims)
{
    const char *path;
    const char *description;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };

    /* scanline_pixel_index */
    description = "pixel index (0-based) within the scanline";
    variable_definition =
        harp_ingestion_register_variable_sample_read(product_definition, "scanline_pixel_index", harp_type_int16, 1,
                                                     dimension_type, NULL, description, NULL, NULL,
                                                     read_scanline_pixel_index);
    description =
        "the scanline and pixel dimensions are collapsed into a temporal dimension; the index of the pixel within the "
        "scanline is computed as the index on the temporal dimension modulo the number of scanlines";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, NULL, description);

    /* datetime_start */
    description = "start time of the measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime_start", harp_type_double, 1,
                                                   dimension_type, NULL, description, "seconds since 2010-01-01", NULL,
                                                   read_datetime);
    path = "/PRODUCT/time, /PRODUCT/delta_time[]";
    if (delta_time_num_dims == 2)
    {
        description = "time converted from milliseconds since a reference time (given as seconds since 2010-01-01) to "
            "seconds since 2010-01-01 (using 86400 seconds per day); the time associated with a scanline is repeated "
            "for each pixel in the scanline";
    }
    else
    {
        description = "time converted from milliseconds since a reference time (given as seconds since 2010-01-01) to "
            "seconds since 2010-01-01 (using 86400 seconds per day)";
    }
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* datetime_length */
    description = "duration of the measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime_length", harp_type_double, 0,
                                                   dimension_type, NULL, description, "s", NULL,
                                                   read_time_coverage_resolution);
    path = "/@time_coverage_resolution";
    description = "the measurement length is parsed assuming the ISO 8601 'PT%(interval_seconds)fS' format";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* validity */
    description = "processing quality flag";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "validity", harp_type_int32, 1, dimension_type,
                                                   NULL, description, NULL, NULL,
                                                   read_results_processing_quality_flags);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/processing_quality_flags[]";
    description = "the uint32 data is cast to int32";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);
}

static void register_geolocation_variables(harp_product_definition *product_definition)
{
    const char *path;
    const char *description;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };

    /* latitude */
    description = "latitude of the ground pixel center (WGS84)";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_float, 1,
                                                                     dimension_type, NULL, description, "degree_north",
                                                                     NULL, read_product_latitude);
    harp_variable_definition_set_valid_range_float(variable_definition, -90.0f, 90.0f);
    path = "/PRODUCT/latitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* longitude */
    description = "longitude of the ground pixel center (WGS84)";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_float,
                                                                     1, dimension_type, NULL, description,
                                                                     "degree_east", NULL, read_product_longitude);
    harp_variable_definition_set_valid_range_float(variable_definition, -180.0f, 180.0f);
    path = "/PRODUCT/longitude[]";
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

    /* latitude_bounds */
    description = "latitudes of the ground pixel corners (WGS84)";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "latitude_bounds",
                                                                     harp_type_float, 2, bounds_dimension_type,
                                                                     bounds_dimension, description, "degree_north",
                                                                     NULL, read_geolocation_latitude_bounds);
    harp_variable_definition_set_valid_range_float(variable_definition, -90.0f, 90.0f);
    path = "/PRODUCT/SUPPORT_DATA/GEOLOCATIONS/latitude_bounds[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* longitude_bounds */
    description = "longitudes of the ground pixel corners (WGS84)";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "longitude_bounds",
                                                                     harp_type_float, 2, bounds_dimension_type,
                                                                     bounds_dimension, description, "degree_east",
                                                                     NULL, read_geolocation_longitude_bounds);
    harp_variable_definition_set_valid_range_float(variable_definition, -180.0f, 180.0f);
    path = "/PRODUCT/SUPPORT_DATA/GEOLOCATIONS/longitude_bounds[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* sensor_latitude */
    description = "latitude of the geodetic sub-satellite point (WGS84)";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "sensor_latitude",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     description, "degree_north", NULL,
                                                                     read_geolocation_satellite_latitude);
    harp_variable_definition_set_valid_range_float(variable_definition, -90.0f, 90.0f);
    path = "/PRODUCT/SUPPORT_DATA/GEOLOCATIONS/satellite_latitude[]";
    description = "the satellite latitude associated with a scanline is repeated for each pixel in the scanline";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* sensor_longitude */
    description = "longitude of the goedetic sub-satellite point (WGS84)";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "sensor_longitude",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     description, "degree_east", NULL,
                                                                     read_geolocation_satellite_longitude);
    harp_variable_definition_set_valid_range_float(variable_definition, -180.0f, 180.0f);
    path = "/PRODUCT/SUPPORT_DATA/GEOLOCATIONS/satellite_longitude[]";
    description = "the satellite longitude associated with a scanline is repeated for each pixel in the scanline";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* sensor_altitude */
    description = "altitude of the satellite with respect to the geodetic sub-satellite point (WGS84)";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "sensor_altitude",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     description, "m", NULL,
                                                                     read_geolocation_satellite_altitude);
    harp_variable_definition_set_valid_range_float(variable_definition, 700000.0f, 900000.0f);
    path = "/PRODUCT/SUPPORT_DATA/GEOLOCATIONS/satellite_altitude[]";
    description = "the satellite altitude associated with a scanline is repeated for each pixel in the scanline";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* solar_zenith_angle */
    description = "zenith angle of the Sun at the ground pixel location (WGS84); angle measured away from the vertical";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "solar_zenith_angle",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     description, "degree", NULL,
                                                                     read_geolocation_solar_zenith_angle);
    harp_variable_definition_set_valid_range_float(variable_definition, 0.0f, 180.0f);
    path = "/PRODUCT/SUPPORT_DATA/GEOLOCATIONS/solar_zenith_angle[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* solar_azimuth_angle */
    description = "azimuth angle of the Sun at the ground pixel location (WGS84); angle measured East-of-North";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "solar_azimuth_angle",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     description, "degree", NULL,
                                                                     read_geolocation_solar_azimuth_angle);
    harp_variable_definition_set_valid_range_float(variable_definition, -180.0f, 180.0f);
    path = "/PRODUCT/SUPPORT_DATA/GEOLOCATIONS/solar_azimuth_angle[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* sensor_zenith_angle */
    description = "zenith angle of the satellite at the ground pixel location (WGS84); angle measured away from the "
        "vertical";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "sensor_zenith_angle",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     description, "degree", NULL,
                                                                     read_geolocation_viewing_zenith_angle);
    harp_variable_definition_set_valid_range_float(variable_definition, 0.0f, 180.0f);
    path = "/PRODUCT/SUPPORT_DATA/GEOLOCATIONS/viewing_zenith_angle[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* sensor_azimuth_angle */
    description = "azimuth angle of the satellite at the ground pixel location (WGS84); angle measured East-of-North";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "sensor_azimuth_angle",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     description, "degree", NULL,
                                                                     read_geolocation_viewing_azimuth_angle);
    harp_variable_definition_set_valid_range_float(variable_definition, -180.0f, 180.0f);
    path = "/PRODUCT/SUPPORT_DATA/GEOLOCATIONS/viewing_azimuth_angle[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

static void register_cloud_variables(harp_product_definition *product_definition)
{
    const char *path;
    const char *description;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };

    /* cloud_albedo */
    description = "cloud albedo";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_albedo", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_input_cloud_albedo_crb);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_albedo_crb";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_albedo_uncertainty */
    description = "uncertainty of the cloud albedo";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_albedo_uncertainty", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_input_cloud_albedo_crb_precision);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_albedo_crb_precision";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_fraction */
    description = "cloud fraction";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_fraction", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_hcho_cloud_fraction);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_fraction_crb[]";
    harp_variable_definition_add_mapping(variable_definition, "cloud_fraction unset", NULL, path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/cloud_fraction_intensity_weighted[]";
    harp_variable_definition_add_mapping(variable_definition, "cloud_fraction=radiance", NULL, path, NULL);

    /* cloud_fraction_uncertainty */
    description = "uncertainty of the cloud fraction";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_fraction_uncertainty", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_hcho_cloud_fraction_precision);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_fraction_crb_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "cloud_fraction unset", NULL, path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/cloud_fraction_intensity_weighted_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "cloud_fraction=radiance", NULL, path, NULL);

    /* cloud_altitude */
    description = "cloud altitude";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_altitude", harp_type_float, 1,
                                                   dimension_type, NULL, description, "km", NULL,
                                                   read_input_cloud_height_crb);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_height_crb";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_altitude_uncertainty */
    description = "uncertainty of the cloud altitude";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_altitude_uncertainty", harp_type_float, 1,
                                                   dimension_type, NULL, description, "km", NULL,
                                                   read_input_cloud_height_crb_precision);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_height_crb_precision";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_pressure */
    description = "cloud pressure";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_pressure", harp_type_float, 1,
                                                   dimension_type, NULL, description, "Pa", NULL,
                                                   read_input_cloud_pressure_crb);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_pressure_crb";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_pressure_uncertainty */
    description = "uncertainty of the cloud pressure";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_pressure_uncertainty", harp_type_float, 1,
                                                   dimension_type, NULL, description, "Pa", NULL,
                                                   read_input_cloud_pressure_crb_precision);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_pressure_crb_precision";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

static void register_surface_variables(harp_product_definition *product_definition)
{
    const char *path;
    const char *description;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };

    /* surface_altitude */
    description = "surface altitude";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "surface_altitude", harp_type_float, 1,
                                                   dimension_type, NULL, description, "m", NULL,
                                                   read_input_surface_altitude);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/surface_altitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* surface_altitude_uncertainty */
    description = "surface altitude precision";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "surface_altitude_uncertainty", harp_type_float,
                                                   1, dimension_type, NULL, description, "m", NULL,
                                                   read_input_surface_altitude_precision);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/surface_altitude_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* surface_pressure */
    description = "surface pressure";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "surface_pressure", harp_type_float, 1,
                                                   dimension_type, NULL, description, "Pa", NULL,
                                                   read_input_surface_pressure);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/surface_pressure[]";
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

    description = "ingest aerosol index retrieved at wavelengths 354/388 nm (default), or 340/388 nm";
    harp_ingestion_register_option(module, "wavelength_ratio", description, 2, wavelength_ratio_option_values);

    product_definition = harp_ingestion_register_product(module, "S5P_L2_AER_AI", NULL, read_dimensions);
    register_core_variables(product_definition, s5p_delta_time_num_dims[s5p_type_aer_ai]);
    register_geolocation_variables(product_definition);
    register_additional_geolocation_variables(product_definition);

    /* absorbing_aerosol_index */
    description = "aerosol index";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "absorbing_aerosol_index", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_product_aerosol_index);
    harp_variable_definition_add_mapping(variable_definition, "wavelength_ratio=354_388nm or wavelength_ratio unset",
                                         NULL, "/PRODUCT/aerosol_index_354_388", NULL);
    harp_variable_definition_add_mapping(variable_definition, "wavelength_ratio=340_380nm", NULL,
                                         "/PRODUCT/aerosol_index_340_380", NULL);

    /* absorbing_aerosol_index_uncertainty */
    description = "uncertainty of the aerosol index";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "absorbing_aerosol_index_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL, read_product_aerosol_index_precision);
    harp_variable_definition_add_mapping(variable_definition, "wavelength_ratio=354_388nm (default)", NULL,
                                         "/PRODUCT/aerosol_index_354_388_precision", NULL);
    harp_variable_definition_add_mapping(variable_definition, "wavelength_ratio=340_380nm", NULL,
                                         "/PRODUCT/aerosol_index_340_380_precision", NULL);
}

static void register_aer_lh_product(void)
{
    const char *path;
    const char *description;
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };

    module = harp_ingestion_register_module_coda("S5P_L2_AER_LH", "Sentinel-5P", "Sentinel5P", "L2__AER_LH",
                                                 "Sentinel-5P L2 aerosol layer height", ingestion_init, ingestion_done);

    product_definition = harp_ingestion_register_product(module, "S5P_L2_AER_LH", NULL, read_dimensions);
    register_core_variables(product_definition, s5p_delta_time_num_dims[s5p_type_aer_lh]);
    register_geolocation_variables(product_definition);
    register_additional_geolocation_variables(product_definition);

    /* aerosol_height */
    description = "altitude of center of aerosol layer";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "aerosol_height", harp_type_float, 1,
                                                   dimension_type, NULL, description, "m", NULL,
                                                   read_product_aerosol_mid_height);
    path = "/PRODUCT/aerosol_mid_height[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* aerosol_height_uncertainty */
    description = "uncertainty of altitude of center of aerosol layer";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "aerosol_height_uncertainty", harp_type_float, 1,
                                                   dimension_type, NULL, description, "m", NULL,
                                                   read_product_aerosol_mid_height_precision);
    path = "/PRODUCT/aerosol_mid_height_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* aerosol_pressure */
    description = "pressure at center of aerosol layer";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "aerosol_pressure", harp_type_float, 1,
                                                   dimension_type, NULL, description, "Pa", NULL,
                                                   read_product_aerosol_mid_pressure);
    path = "/PRODUCT/aerosol_mid_pressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* aerosol_pressure_uncertainty */
    description = "uncertainty of pressure at center of aerosol layer";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "aerosol_pressure_uncertainty", harp_type_float,
                                                   1, dimension_type, NULL, description, "Pa", NULL,
                                                   read_product_aerosol_mid_pressure_precision);
    path = "/PRODUCT/aerosol_mid_pressure_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* aerosol_optical_depth */
    description = "aerosol optical thickness";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "aerosol_optical_depth", harp_type_float, 1,
                                                   dimension_type, NULL, description, "m", NULL,
                                                   read_results_aerosol_optical_thickness);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/aerosol_optical_thickness[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* aerosol_optical_depth_uncertainty */
    description = "uncertainty of the aerosol optical thickness";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "aerosol_optical_depth_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description, "m", NULL,
                                                   read_results_aerosol_optical_thickness_precision);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/aerosol_optical_thickness_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

static void register_ch4_product(void)
{
    const char *ch4_options[] = { "bias_corrected" };
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

    harp_ingestion_register_option(module, "ch4", "whether to ingest the 'normal' CH4 column vmr (default) or the "
                                   "bias corrected CH4 column vmr (ch4=bias_corrected)", 1, ch4_options);

    product_definition = harp_ingestion_register_product(module, "S5P_L2_CH4", NULL, read_dimensions);
    register_core_variables(product_definition, s5p_delta_time_num_dims[s5p_type_ch4]);
    register_geolocation_variables(product_definition);
    register_additional_geolocation_variables(product_definition);

    /* altitude_bounds */
    description = "altitude bounds per profile layer; altitude is measured as the vertical distance to the surface";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "altitude_bounds", harp_type_float, 3,
                                                   dimension_type, dimension, description, "m", NULL,
                                                   read_input_altitude_bounds);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/height_levels[], /PRODUCT/SUPPORT_DATA/INPUT_DATA/surface_altitude[]";
    description = "derived from height per level (layer boundary) by repeating the inner levels; the upper bound of "
        "layer k is equal to the lower bound of layer k+1; height is converted to altitude by adding surface_altitude";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* pressure_bounds */
    description = "pressure bounds per profile layer";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "pressure_bounds", harp_type_double, 3,
                                                   dimension_type, dimension, description, "Pa", NULL,
                                                   read_input_pressure_bounds);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/surface_pressure[],/PRODUCT/SUPPORT_DATA/INPUT_DATA/pressure_interval[]";
    description = "derived from surface pressure and pressure difference between retrieval levels (the pressure grid "
        "is equidistant between the surface pressure and a fixed top pressure); given a zero-based layer "
        "index k, the pressure bounds for layer k are derived as: (surface_pressure - k * pressure_interval, "
        "surface_pressure - (k + 1) * pressure_interval)";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* CH4_column_volume_mixing_ratio_dry_air */
    description = "column averaged dry air mixing ratio of methane";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "CH4_column_volume_mixing_ratio_dry_air",
                                                   harp_type_float, 1, dimension_type, NULL, description, "ppbv", NULL,
                                                   read_ch4_methane_mixing_ratio);
    path = "/PRODUCT/methane_mixing_ratio[]";
    harp_variable_definition_add_mapping(variable_definition, "ch4 unset", NULL, path, NULL);
    path = "/PRODUCT/methane_mixing_ratio_bias_corrected[]";
    harp_variable_definition_add_mapping(variable_definition, "ch4=bias_corrected", NULL, path, NULL);

    /* CH4_column_volume_mixing_ratio_dry_air_uncertainty */
    description = "uncertainty of the column averaged dry air mixing ratio of methane (1 sigma error)";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "CH4_column_volume_mixing_ratio_dry_air_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description, "ppbv", NULL,
                                                   read_product_methane_mixing_ratio_precision);
    path = "/PRODUCT/methane_mixing_ratio_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* CH4_column_volume_mixing_ratio_dry_air_avk */
    description = "column averaging kernel for methane retrieval";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "CH4_column_volume_mixing_ratio_dry_air_avk",
                                                   harp_type_float, 2, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL, read_results_column_averaging_kernel);
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
    register_core_variables(product_definition, s5p_delta_time_num_dims[s5p_type_co]);
    register_geolocation_variables(product_definition);
    register_additional_geolocation_variables(product_definition);

    /* altitude */
    description = "fixed altitude grid on which the radiative transfer calculations are done; altitude is measured"
        " relative to the surface";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "altitude", harp_type_float, 1,
                                                   dimension_type_altitude, NULL, description, "m", NULL,
                                                   read_product_layer);
    path = "/PRODUCT/layer[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* pressure */
    description = "pressure of the layer interfaces of the vertical grid";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "pressure", harp_type_float, 2, dimension_type,
                                                   NULL, description, "Pa", NULL, read_results_pressure_levels);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/pressure_levels[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* CO_column_number_density */
    description = "vertically integrated CO column density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "CO_column_number_density", harp_type_float, 1,
                                                   dimension_type, NULL, description, "mol/m^2", NULL,
                                                   read_product_carbonmonoxide_total_column);
    path = "/PRODUCT/carbonmonoxide_total_column[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* CO_column_number_density_uncertainty */
    description = "uncertainty of the vertically integrated CO column density (standard error)";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "CO_column_number_density_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description, "mol/m^2",
                                                   NULL, read_product_carbonmonoxide_total_column_precision);
    path = "/PRODUCT/carbonmonoxide_total_column_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* CO_column_number_density_avk */
    description = "averaging kernel for the vertically integrated CO column density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "CO_column_number_density_avk", harp_type_float,
                                                   2, dimension_type, NULL, description, "m", NULL,
                                                   read_results_column_averaging_kernel);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/column_averaging_kernel[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
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

    module = harp_ingestion_register_module_coda("S5P_L2_HCHO", "Sentinel-5P", "Sentinel5P", "L2__HCHO__",
                                                 "Sentinel-5P L2 HCHO total column", ingestion_init, ingestion_done);

    harp_ingestion_register_option(module, "cloud_fraction", "whether to ingest the cloud fraction (default) or the "
                                   "radiance cloud fraction (cloud_fraction=radiance)", 1, cloud_fraction_options);

    product_definition = harp_ingestion_register_product(module, "S5P_L2_HCHO", NULL, read_dimensions);
    register_core_variables(product_definition, s5p_delta_time_num_dims[s5p_type_hcho]);
    register_geolocation_variables(product_definition);
    register_additional_geolocation_variables(product_definition);

    register_cloud_variables(product_definition);

    /* surface_albedo */
    description = "surface albedo";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "surface_albedo", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_input_surface_albedo);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/surface_albedo";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    register_surface_variables(product_definition);

    /* tropospheric_HCHO_column_number_density */
    description = "tropospheric HCHO column number density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "tropospheric_HCHO_column_number_density",
                                                   harp_type_float, 1, dimension_type, NULL, description, "mol/cm^2",
                                                   NULL, read_product_formaldehyde_tropospheric_vertical_column);
    path = "/PRODUCT/formaldehyde_tropospheric_vertical_column[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* tropospheric_HCHO_column_number_density_uncertainty_random */
    description = "uncertainty of the tropospheric HCHO column number density due to random effects";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "tropospheric_HCHO_column_number_density_uncertainty_random",
                                                   harp_type_float, 1, dimension_type, NULL, description, "mol/cm^2",
                                                   NULL,
                                                   read_product_formaldehyde_tropospheric_vertical_column_precision);
    path = "/PRODUCT/formaldehyde_tropospheric_vertical_column_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* tropospheric_HCHO_column_number_density_uncertainty_systematic */
    description = "uncertainty of the tropospheric HCHO column number density due to systematic effects";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "tropospheric_HCHO_column_number_density_uncertainty_systematic",
                                                   harp_type_float, 1, dimension_type, NULL, description, "mol/cm^2",
                                                   NULL,
                                                   read_results_formaldehyde_tropospheric_vertical_column_trueness);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/formaldehyde_tropospheric_vertical_column_trueness[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* HCHO_column_number_density_avk */
    description = "averaging kernel for the total HCHO column number density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "HCHO_column_number_density_avk",
                                                   harp_type_float, 2, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL, read_results_averaging_kernel_1d);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/averaging_kernel[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* HCHO_volume_mixing_ratio_apriori */
    description = "HCHO apriori profile in volume mixing ratios";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "HCHO_volume_mixing_ratio_apriori",
                                                   harp_type_float, 2, dimension_type, NULL, description, "ppv",
                                                   exclude_non_nrti, read_results_formaldehyde_profile_apriori);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/formaldehyde_profile_apriori[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "NRTI", path, NULL);

    /* tropospheric_HCHO_column_number_density_amf */
    description = "tropospheric air mass factor";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "tropospheric_HCHO_column_number_density_amf",
                                                   harp_type_float, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_results_formaldehyde_tropospheric_air_mass_factor);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/formaldehyde_tropospheric_air_mass_factor[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* tropospheric_HCHO_column_number_density_amf_uncertainty_random */
    description = "random part of the tropospheric air mass factor uncertainty";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "tropospheric_HCHO_column_number_density_amf_uncertainty_random",
                                                   harp_type_float, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_results_formaldehyde_tropospheric_air_mass_factor_precision);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/formaldehyde_tropospheric_air_mass_factor_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* tropospheric_HCHO_column_number_density_amf_uncertainty_systematic */
    description = "systematic part of the tropospheric air mass factor uncertainty";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "tropospheric_HCHO_column_number_density_amf_uncertainty_systematic",
                                                   harp_type_float, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_results_formaldehyde_tropospheric_air_mass_factor_trueness);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/formaldehyde_tropospheric_air_mass_factor_trueness[]";
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
    register_core_variables(product_definition, s5p_delta_time_num_dims[s5p_type_o3]);
    register_geolocation_variables(product_definition);
    register_additional_geolocation_variables(product_definition);

    /* cloud_base_height */
    description = "cloud base height calculated using the OCRA/ROCINN CAL model";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_base_height", harp_type_float, 1,
                                                   dimension_type, NULL, description, "m", exclude_non_nrti,
                                                   read_input_cloud_base_height);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_base_height[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "NRTI", path, NULL);

    /* cloud_base_height_uncertainty */
    description = "error of the cloud base height calculated using the OCRA/ROCINN CAL model";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_base_height_uncertainty", harp_type_float,
                                                   1, dimension_type, NULL, description, "m", exclude_non_nrti,
                                                   read_input_cloud_base_height_precision);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_base_height_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "NRTI", path, NULL);

    /* cloud_base_pressure */
    description = "cloud base pressure calculated using the OCRA/ROCINN CAL model";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_base_pressure", harp_type_float, 1,
                                                   dimension_type, NULL, description, "Pa", exclude_non_nrti,
                                                   read_input_cloud_base_pressure);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_base_pressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "NRTI", path, NULL);

    /* cloud_base_pressure_uncertainty */
    description = "error of the cloud base pressure calculated using the OCRA/ROCINN CAL model";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_base_pressure_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description, "Pa",
                                                   exclude_non_nrti, read_input_cloud_base_pressure_precision);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_base_pressure_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "NRTI", path, NULL);

    /* cloud_fraction */
    description = "cloud fraction from either the OCRA/ROCINN CAL or CRB model";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_fraction", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS,
                                                   NULL, read_o3_cloud_fraction);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_fraction[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "NRTI", path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_fraction_crb[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "OFFL", path, NULL);

    /* cloud_fraction_uncertainty */
    description = "uncertainty of the cloud fraction";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_fraction_uncertainty", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS,
                                                   NULL, read_o3_cloud_fraction_precision);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_fraction_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "NRTI", path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_fraction_crb_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "OFFL", path, NULL);

    /* cloud_optical_depth */
    description = "retrieved cloud optical depth using the OCRA/ROCINN CAL model";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_optical_depth", harp_type_float, 1,
                                                   dimension_type, NULL, description, "m", exclude_non_nrti,
                                                   read_input_cloud_optical_thickness);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_optical_thickness[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "NRTI", path, NULL);

    /* cloud_optical_depth_uncertainty */
    description = "uncertainty of the retrieved cloud optical depth using the OCRA/ROCINN CAL model";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_optical_depth_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description, "m",
                                                   exclude_non_nrti, read_input_cloud_optical_thickness_precision);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_optical_thickness_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "NRTI", path, NULL);

    /* cloud_top_pressure */
    description = "retrieved atmospheric pressure at the level of cloud top using the OCRA/ROCINN CAL model";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_top_pressure", harp_type_float, 1,
                                                   dimension_type, NULL, description, "Pa", exclude_non_nrti,
                                                   read_input_cloud_top_pressure);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_top_pressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "NRTI", path, NULL);

    /* cloud_top_pressure_uncertainty */
    description = "uncertainty of the retrieved atmospheric pressure at the level of cloud top using the OCRA/ROCINN "
        "CAL model";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_top_pressure_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description, "Pa",
                                                   exclude_non_nrti, read_input_cloud_top_pressure_precision);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_top_pressure_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "NRTI", path, NULL);

    /* cloud_top_height */
    description = "retrieved vertical distance above the surface of the cloud top using the OCRA/ROCINN CAL model";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_top_height", harp_type_float, 1,
                                                   dimension_type, NULL, description, "m", exclude_non_nrti,
                                                   read_input_cloud_top_height);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_top_height[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "NRTI", path, NULL);

    /* cloud_top_height_uncertainty */
    description = "uncertainty of the retrieved vertical distance above the surface of the cloud top using the "
        "OCRA/ROCINN CAL model";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_top_height_uncertainty", harp_type_float,
                                                   1, dimension_type, NULL, description, "m", exclude_non_nrti,
                                                   read_input_cloud_top_height_precision);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_top_height_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "NRTI", path, NULL);

    /* cloud_albedo */
    description = "albedo of cloud using the OCRA/ROCINN CRB model";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_albedo", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS,
                                                   exclude_nrti, read_input_cloud_albedo_crb);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_albedo_crb[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "OFFL", path, NULL);

    /* cloud_albedo_uncertainty */
    description = "uncertainty of the albedo of cloud using the OCRA/ROCINN CRB model";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_albedo_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, exclude_nrti,
                                                   read_input_cloud_albedo_crb_precision);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_albedo_crb_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "OFFL", path, NULL);

    /* cloud_height */
    description = "retrieved atmospheric height at the level of cloud using the OCRA/ROCINN CRB model";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_height", harp_type_float, 1,
                                                   dimension_type, NULL, description, "m", exclude_nrti,
                                                   read_input_cloud_height_crb);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_height_crb[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "OFFL", path, NULL);

    /* cloud_height_uncertainty */
    description = "error of the retrieved atmospheric height at the level of cloud using the OCRA/ROCINN CRB model";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_height_uncertainty", harp_type_float,
                                                   1, dimension_type, NULL, description, "m", exclude_nrti,
                                                   read_input_cloud_height_crb_precision);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_height_crb_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "OFFL", path, NULL);

    /* cloud_pressure */
    description = "retrieved atmospheric pressure at the level of cloud using the OCRA/ROCINN CRB model";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_pressure", harp_type_float, 1,
                                                   dimension_type, NULL, description, "Pa", exclude_nrti,
                                                   read_input_cloud_pressure_crb);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_pressure_crb[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "OFFL", path, NULL);

    /* cloud_pressure_uncertainty */
    description = "error of the retrieved atmospheric pressure at the level of cloud using the OCRA/ROCINN CRB model";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_pressure_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description, "Pa",
                                                   exclude_nrti, read_input_cloud_pressure_crb_precision);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_pressure_crb_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "OFFL", path, NULL);

    /* surface_albedo */
    description = "surface albedo";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "surface_albedo", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_input_surface_albedo);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/surface_albedo[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    register_surface_variables(product_definition);

    /* pressure_bounds */
    description = "pressure bounds per profile layer";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "pressure_bounds", harp_type_float, 3,
                                                   dimension_type, dimension, description, "Pa", NULL,
                                                   read_results_pressure_bounds);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/pressure_grid[]";
    description = "derived from pressure per level (layer boundary) by repeating the inner levels; "
        "the upper bound of layer k is equal to the lower bound of layer k+1";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* O3_column_number_density */
    description = "O3 column number density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_column_number_density", harp_type_float, 1,
                                                   dimension_type, NULL, description, "mol/m^2", NULL,
                                                   read_product_ozone_total_vertical_column);
    path = "/PRODUCT/ozone_total_vertical_column[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* O3_column_number_density_uncertainty */
    description = "uncertainty of the O3 column number density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_column_number_density_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description, "mol/m^2",
                                                   NULL, read_product_ozone_total_vertical_column_precision);
    path = "/PRODUCT/ozone_total_vertical_column_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* O3_column_number_density_apriori */
    description = "O3 column number density apriori";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_column_number_density_apriori",
                                                   harp_type_float, 2, dimension_type, NULL, description, "mol/m^2",
                                                   NULL, read_results_ozone_profile_apriori);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/ozone_profile_apriori[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* O3_column_number_density_avk */
    description = "averaging kernel for the O3 column number density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_column_number_density_avk", harp_type_float,
                                                   2, dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_results_averaging_kernel_1d);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/averaging_kernel[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* O3_column_number_density_amf */
    description = "O3 column number density total air mass factor";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_column_number_density_amf", harp_type_float,
                                                   1, dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS,
                                                   exclude_non_nrti, read_results_ozone_total_air_mass_factor);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/ozone_total_air_mass_factor[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "NRTI", path, NULL);

    /* O3_column_number_density_amf_uncertainty */
    description = "uncertainty of the O3 column number density total air mass factor";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_column_number_density_amf_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, exclude_non_nrti,
                                                   read_results_ozone_total_air_mass_factor_trueness);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/ozone_total_air_mass_factor_trueness[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "NRTI", path, NULL);
}

static void register_o3_profile_variables(harp_product_definition *product_definition)
{
    const char *path;
    const char *description;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[3] = { harp_dimension_time, harp_dimension_vertical, harp_dimension_vertical };

    /* cloud_pressure */
    description = "air pressure at cloud optical centroid";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_pressure", harp_type_float, 1,
                                                   dimension_type, NULL, description, "Pa", NULL,
                                                   read_input_cloud_pressure_crb);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_pressure_crb[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_fraction */
    description = "effective wavelenght-dependent cloud fraction";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_fraction", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_results_cloud_fraction_crb);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/cloud_fraction_crb[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_fraction_uncertainty */
    description = "precision of the effective wavelenght-dependent cloud fraction";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_fraction_uncertainty", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_results_cloud_fraction_crb_precision);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/cloud_fraction_crb_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* pressure */
    description = "pressure";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "pressure", harp_type_float, 2, dimension_type,
                                                   NULL, description, "Pa", NULL, read_input_pressure);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/pressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* tropopause_pressure */
    description = "tropopause pressure";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "tropopause_pressure", harp_type_float, 1,
                                                   dimension_type, NULL, description, "Pa", NULL,
                                                   read_input_pressure_at_tropopause);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/pressure_at_tropopause[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* altitude */
    description = "altitude";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "altitude", harp_type_float, 2, dimension_type,
                                                   NULL, description, "m", NULL, read_input_altitude);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/altitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* temperature */
    description = "temperature";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "temperature", harp_type_float, 2,
                                                   dimension_type, NULL, description, "K", NULL,
                                                   read_input_temperature);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/temperature[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* O3_volume_mixing_ratio */
    description = "O3 volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_volume_mixing_ratio", harp_type_float, 2,
                                                   dimension_type, NULL, description, "ppmv", NULL,
                                                   read_product_ozone_profile);
    path = "/PRODUCT/ozone_profile[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* O3_volume_mixing_ratio_uncertainty */
    description = "uncertainty of the O3 volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_volume_mixing_ratio_uncertainty",
                                                   harp_type_float, 2, dimension_type, NULL, description, "ppmv", NULL,
                                                   read_product_ozone_profile_precision);
    path = "/PRODUCT/ozone_profile_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* O3_volume_mixing_ratio_avk */
    description = "O3 volume mixing ratio averaging kernel";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_volume_mixing_ratio_avk", harp_type_float, 3,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_results_averaging_kernel_2d);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/averaging_kernel[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* O3_volume_mixing_ratio_apriori */
    description = "O3 volume mixing ratio apriori";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_volume_mixing_ratio_apriori",
                                                   harp_type_float, 2, dimension_type, NULL, description, "ppmv", NULL,
                                                   read_input_ozone_profile_apriori);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/ozone_profile_apriori[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* O3_volume_mixing_ratio_apriori_covariance */
    description = "covariance of the O3 volume mixing ratio apriori";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_volume_mixing_ratio_apriori_covariance",
                                                   harp_type_float, 3, dimension_type, NULL, description, "ppmv^2",
                                                   NULL, read_input_ozone_profile_apriori_covariance);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/ozone_profile_apriori_precision[], "
        "/PRODUCT/SUPPORT_DATA/INPUT_DATA/ozone_profile_apriori_precision@correlation_length, "
        "/PRODUCT/SUPPORT_DATA/INPUT_DATA/altitude[]";
    description = "covariance[i,j] = exp(-(latitude[i]-latitude[j])/correlation_length) * precision[i] * precision[j]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* O3_volume_mixing_ratio_covariance */
    description = "O3 volume mixing ratio covariance";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_volume_mixing_ratio_covariance",
                                                   harp_type_float, 3, dimension_type, NULL, description, "ppmv^2",
                                                   NULL, read_results_ozone_profile_error_covariance_matrix);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/ozone_profile_error_covariance_matrix[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* O3_column_number_density */
    description = "O3 total column";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_column_number_density", harp_type_float, 1,
                                                   dimension_type, NULL, description, "mol/m^2", NULL,
                                                   read_product_ozone_total_column);
    path = "/PRODUCT/ozone_total_column[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* O3_column_number_density_uncertainty */
    description = "uncertainty of the O3 total column";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_column_number_density_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description, "mol/m^2",
                                                   NULL, read_product_ozone_total_column_precision);
    path = "/PRODUCT/ozone_total_column_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* tropospheric_O3_column_number_density */
    description = "O3 tropospheric column";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "tropospheric_O3_column_number_density",
                                                   harp_type_float, 1, dimension_type, NULL, description, "mol/m^2",
                                                   NULL, read_product_ozone_tropospheric_column);
    path = "/PRODUCT/ozone_tropospheric_column[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* tropospheric_O3_column_number_density_uncertainty */
    description = "uncertainty of the O3 tropospheric column";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "tropospheric_O3_column_number_density_uncertainty", harp_type_float,
                                                   1, dimension_type, NULL, description, "mol/m^2", NULL,
                                                   read_product_ozone_tropospheric_column_precision);
    path = "/PRODUCT/ozone_tropospheric_column_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

static void register_o3_pr_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;

    module = harp_ingestion_register_module_coda("S5P_L2_O3_PR", "Sentinel-5P", "Sentinel5P", "L2__O3__PR",
                                                 "Sentinel-5P L2 O3 profile", ingestion_init, ingestion_done);

    product_definition = harp_ingestion_register_product(module, "S5P_L2_O3_PR", NULL, read_dimensions);
    register_core_variables(product_definition, s5p_delta_time_num_dims[s5p_type_o3_pr]);
    register_geolocation_variables(product_definition);
    register_additional_geolocation_variables(product_definition);
    register_o3_profile_variables(product_definition);
    register_surface_variables(product_definition);
}

static void register_o3_tcl_product(void)
{
    const char *o3_options[] = { "upper" };
    const char *path;
    const char *description;
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[3] = { harp_dimension_time, harp_dimension_latitude, harp_dimension_longitude };

    module = harp_ingestion_register_module_coda("S5P_L2_O3_TCL", "Sentinel-5P", "Sentinel5P", "L2__O3_TCL",
                                                 "Sentinel-5P L2 O3 tropospheric column", ingestion_init,
                                                 ingestion_done);

    harp_ingestion_register_option(module, "o3", "whether to ingest the 'normal' tropospheric O3 column grid (default) "
                                   "or the upper tropospheric column grid (o3=upper)", 1, o3_options);

    product_definition = harp_ingestion_register_product(module, "S5P_L2_O3_TCL", NULL, read_dimensions);

    /* datetime_start */
    description = "coverage start time";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime_start", harp_type_double, 1,
                                                   dimension_type, NULL, description, "seconds since 2010-01-01", NULL,
                                                   read_o3_tcl_datetime_start);
    path = "/@time_coverage_start";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* datetime_stop */
    description = "coverage stop time";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime_stop", harp_type_double, 1,
                                                   dimension_type, NULL, description, "seconds since 2010-01-01", NULL,
                                                   read_o3_tcl_datetime_stop);
    path = "/@time_coverage_end";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* latitude */
    description = "grid center latitudes";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_float, 1,
                                                                     &dimension_type[1], NULL, description,
                                                                     "degree_north", NULL, read_o3_tcl_latitude);
    harp_variable_definition_set_valid_range_float(variable_definition, -90.0f, 90.0f);
    path = "/PRODUCT/latitude[]";
    harp_variable_definition_add_mapping(variable_definition, "o3 unset", NULL, path, NULL);
    path = "/PRODUCT/lat[]";
    harp_variable_definition_add_mapping(variable_definition, "o3=upper", NULL, path, NULL);

    /* longitude */
    description = "grid center longitudes";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_float,
                                                                     1, &dimension_type[2], NULL, description,
                                                                     "degree_east", NULL, read_o3_tcl_longitude);
    harp_variable_definition_set_valid_range_float(variable_definition, -180.0f, 180.0f);
    path = "/PRODUCT/longitude[]";
    harp_variable_definition_add_mapping(variable_definition, "o3 unset", NULL, path, NULL);
    path = "/PRODUCT/lon[]";
    harp_variable_definition_add_mapping(variable_definition, "o3=upper", NULL, path, NULL);

    /* tropospheric_O3_column_volume_mixing_ratio_dry_air */
    description = "average tropospheric ozone mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "tropospheric_O3_column_volume_mixing_ratio_dry_air",
                                                   harp_type_float, 3, dimension_type, NULL, description, "ppbv", NULL,
                                                   read_o3_tcl_ozone_tropospheric_mixing_ratio);
    path = "/PRODUCT/ozone_tropospheric_mixing_ratio[]";
    harp_variable_definition_add_mapping(variable_definition, "o3 unset", NULL, path, NULL);
    path = "/PRODUCT/ozone_upper_tropospheric_mixing_ratio[]";
    harp_variable_definition_add_mapping(variable_definition, "o3=upper", NULL, path, NULL);

    /* tropospheric_O3_column_volume_mixing_ratio_dry_air_uncertainty */
    description = "uncertainty of the average tropospheric ozone mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "tropospheric_O3_column_volume_mixing_ratio_dry_air_uncertainty",
                                                   harp_type_float, 3, dimension_type, NULL, description, "ppbv", NULL,
                                                   read_o3_tcl_ozone_tropospheric_mixing_ratio_precision);
    path = "/PRODUCT/ozone_tropospheric_mixing_ratio_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "o3 unset", NULL, path, NULL);
    path = "/PRODUCT/ozone_upper_tropospheric_mixing_ratio_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "o3=upper", NULL, path, NULL);

    /* tropospheric_O3_column_volume_mixing_ratio_dry_air_validity */
    description = "validity of the average tropospheric ozone mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "tropospheric_O3_column_volume_mixing_ratio_dry_air_validity",
                                                   harp_type_int32, 3, dimension_type, NULL, description, NULL,
                                                   exclude_o3_tcl, read_o3_tcl_ozone_tropospheric_mixing_ratio_flag);
    path = "/PRODUCT/ozone_upper_tropospheric_mixing_ratio_flag[]";
    harp_variable_definition_add_mapping(variable_definition, "o3=upper", NULL, path, NULL);

    /* tropospheric_O3_column_number_density */
    description = "average tropospheric ozone column number density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "tropospheric_O3_column_number_density",
                                                   harp_type_float, 3, dimension_type, NULL, description, "mol/m2",
                                                   exclude_o3_tcl_upper,
                                                   read_o3_tcl_ozone_tropospheric_vertical_column);
    path = "/PRODUCT/ozone_tropospheric_vertical_column[]";
    harp_variable_definition_add_mapping(variable_definition, "o3 unset", NULL, path, NULL);

    /* tropospheric_O3_column_number_density_precision */
    description = "uncertainty of the average tropospheric ozone column number density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "tropospheric_O3_column_number_density_uncertainty",
                                                   harp_type_float, 3, dimension_type, NULL, description, "mol/m2",
                                                   exclude_o3_tcl_upper,
                                                   read_o3_tcl_ozone_tropospheric_vertical_column_precision);
    path = "/PRODUCT/ozone_tropospheric_vertical_column_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "o3 unset", NULL, path, NULL);
}

static void register_o3_tpr_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;

    module = harp_ingestion_register_module_coda("S5P_L2_O3_TPR", "Sentinel-5P", "Sentinel5P", "L2__O3_TPR",
                                                 "Sentinel-5P L2 O3 tropospheric profile", ingestion_init,
                                                 ingestion_done);

    product_definition = harp_ingestion_register_product(module, "S5P_L2_O3_TPR", NULL, read_dimensions);
    register_core_variables(product_definition, s5p_delta_time_num_dims[s5p_type_o3_tpr]);
    register_geolocation_variables(product_definition);
    register_additional_geolocation_variables(product_definition);
    register_o3_profile_variables(product_definition);
    register_surface_variables(product_definition);
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
    harp_dimension_type pressure_bounds_dimension_type[3] =
        { harp_dimension_time, harp_dimension_vertical, harp_dimension_independent };
    long pressure_bounds_dimension[3] = { -1, -1, 2 };

    module = harp_ingestion_register_module_coda("S5P_L2_NO2", "Sentinel-5P", "Sentinel5P", "L2__NO2___",
                                                 "Sentinel-5P L2 NO2 tropospheric column", ingestion_init,
                                                 ingestion_done);

    harp_ingestion_register_option(module, "total_column", "whether to use nitrogendioxide_total_column (which is "
                                   "derived from the total slant column diveded by the total amf) or "
                                   "nitrogendioxide_summed_total_column (which is the sum of the retrieved "
                                   "tropospheric and statospheric columns); option values are 'summed' (default) and "
                                   "'total'", 2, total_column_options);

    harp_ingestion_register_option(module, "cloud_fraction", "whether to ingest the cloud fraction (default) or the "
                                   "radiance cloud fraction (cloud_fraction=radiance)", 1, cloud_fraction_options);

    product_definition = harp_ingestion_register_product(module, "S5P_L2_NO2", NULL, read_dimensions);
    register_core_variables(product_definition, s5p_delta_time_num_dims[s5p_type_no2]);
    register_geolocation_variables(product_definition);
    register_additional_geolocation_variables(product_definition);

    /* cloud_fraction */
    description = "cloud fraction for NO2 fitting window";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_fraction", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_results_cloud_fraction_nitrogendioxide_window);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/cloud_fraction_crb_nitrogendioxide_window[]";
    harp_variable_definition_add_mapping(variable_definition, "cloud_fraction unset", NULL, path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/cloud_radiance_fraction_nitrogendioxide_window[]";
    harp_variable_definition_add_mapping(variable_definition, "cloud_fraction=radiance", NULL, path, NULL);

    /* absorbing_aerosol_index */
    description = "aerosol index";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "absorbing_aerosol_index", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_input_aerosol_index_354_388);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/aerosol_index_354_388";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_albedo */
    description = "cloud albedo";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_albedo", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_input_cloud_albedo_crb);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_albedo_crb";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_pressure */
    description = "cloud pressure";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_pressure", harp_type_float, 1,
                                                   dimension_type, NULL, description, "Pa", NULL,
                                                   read_input_cloud_pressure_crb);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_pressure_crb";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* surface_albedo */
    description = "surface albedo";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "surface_albedo", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_input_surface_albedo_nitrogendioxide_window);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/surface_albedo_nitrogendioxide_window";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    register_surface_variables(product_definition);

    /* pressure_bounds */
    description = "pressure boundaries";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "pressure_bounds", harp_type_double, 3,
                                                   pressure_bounds_dimension_type, pressure_bounds_dimension,
                                                   description, "Pa", NULL, read_no2_pressure_bounds);
    path = "/PRODUCT/tm5_constant_a[], /PRODUCT/tm5_constant_b[], /PRODUCT/SUPPORT_DATA/INPUT_DATA/surface_pressure[]";
    description = "pressure in Pa at level k is derived from surface pressure in Pa as: tm5_constant_a[k] + "
        "tm5_constant_b[k] * surface_pressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* tropopause_pressure */
    description = "tropopause pressure";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "tropopause_pressure",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, "Pa", NULL,
                                                                     read_no2_tropopause_pressure);
    path = "/PRODUCT/tm5_constant_a[], /PRODUCT/tm5_constant_b[], /PRODUCT/tm5_tropopause_layer_index[], "
        "/PRODUCT/SUPPORT_DATA/INPUT_DATA/surface_pressure[]";
    description = "pressure in Pa at tropause is derived from the upper bound of the layer with tropopause layer index "
        "k: tm5_constant_a[k + 1] + tm5_constant_b[k + 1] * surface_pressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* tropospheric_NO2_column_number_density */
    description = "tropospheric vertical column of NO2";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "tropospheric_NO2_column_number_density",
                                                   harp_type_float, 1, dimension_type, NULL, description, "mol/m^2",
                                                   NULL, read_product_nitrogendioxide_tropospheric_column);
    path = "/PRODUCT/nitrogendioxide_tropospheric_column[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* tropospheric_NO2_column_number_density_uncertainty */
    description = "uncertainty of the tropospheric vertical column of NO2 (standard error)";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "tropospheric_NO2_column_number_density_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description, "mol/m^2",
                                                   NULL, read_product_nitrogendioxide_tropospheric_column_precision);
    path = "/PRODUCT/nitrogendioxide_tropospheric_column_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* tropospheric_NO2_column_number_density_amf */
    description = "tropospheric air mass factor, computed by integrating the altitude dependent air mass factor over "
        "the atmospheric layers from the surface up to and including the layer with the tropopause";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "tropospheric_NO2_column_number_density_amf",
                                                   harp_type_float, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_product_air_mass_factor_troposphere);
    path = "/PRODUCT/air_mass_factor_troposphere[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* tropospheric_NO2_column_number_density_avk */
    description = "averaging kernel for the tropospheric vertical column number density of NO2";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "tropospheric_NO2_column_number_density_avk",
                                                   harp_type_float, 2, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL, read_no2_column_tropospheric_avk);
    path = "/PRODUCT/averaging_kernel[], /PRODUCT/air_mass_factor_total[], /PRODUCT/air_mass_factor_troposphere[], "
        "/PRODUCT/tm5_tropopause_layer_index[]";
    description = "averaging_kernel[layer] = if layer <= tm5_tropopause_layer_index then "
        "averaging_kernel[layer] * air_mass_factor_total / air_mass_factor_troposphere else 0";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* NO2_column_number_density */
    description = "total vertical column of NO2 (ratio of the slant column density of NO2 and the total air mass "
        "factor)";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "NO2_column_number_density", harp_type_float, 1,
                                                   dimension_type, NULL, description, "mol/m^2", NULL, read_no2_column);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/nitrogendioxide_summed_total_column[]";
    harp_variable_definition_add_mapping(variable_definition, "total_column=summed or total_column unset", NULL, path,
                                         NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/nitrogendioxide_total_column[]";
    harp_variable_definition_add_mapping(variable_definition, "total_column=total", NULL, path, NULL);

    /* NO2_column_number_density_uncertainty */
    description = "uncertainty of the total vertical column of NO2 (standard error)";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "NO2_column_number_density_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description, "mol/m^2",
                                                   NULL, read_no2_column_precision);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/nitrogendioxide_summed_total_column_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "total_column=summed or total_column unset", NULL, path,
                                         NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/nitrogendioxide_total_column_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "total_column=total", NULL, path, NULL);

    /* NO2_column_number_density_amf */
    description = "total air mass factor, computed by integrating the altitude dependent air mass factor over the "
        "atmospheric layers from the surface to top-of-atmosphere";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "NO2_column_number_density_amf", harp_type_float,
                                                   1, dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_product_air_mass_factor_total);
    path = "/PRODUCT/air_mass_factor_total[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* NO2_column_number_density_avk */
    description = "averaging kernel for the air mass factor correction, describing the NO2 profile sensitivity of the "
        "vertical column density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "NO2_column_number_density_avk", harp_type_float,
                                                   2, dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_product_averaging_kernel);
    path = "/PRODUCT/averaging_kernel[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* stratospheric_NO2_column_number_density */
    description = "stratospheric vertical column of NO2";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "stratospheric_NO2_column_number_density",
                                                   harp_type_float, 1, dimension_type, NULL, description, "mol/m^2",
                                                   NULL, read_results_nitrogendioxide_stratospheric_column);
    path = "/PRODUCT/nitrogendioxide_tropospheric_column[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* stratospheric_NO2_column_number_density_uncertainty */
    description = "uncertainty of the stratospheric vertical column of NO2 (standard error)";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "stratospheric_NO2_column_number_density_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description, "mol/m^2",
                                                   NULL, read_results_nitrogendioxide_stratospheric_column_precision);
    path = "/PRODUCT/nitrogendioxide_tropospheric_column_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* stratospheric_NO2_column_number_density_amf */
    description = "stratospheric air mass factor";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "stratospheric_NO2_column_number_density_amf",
                                                   harp_type_float, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_results_air_mass_factor_stratosphere);
    path = "/PRODUCT/air_mass_factor_troposphere[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* stratospheric_NO2_column_number_density_avk */
    description = "averaging kernel for the stratospheric vertical column number density of NO2";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "stratospheric_NO2_column_number_density_avk",
                                                   harp_type_float, 2, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL, read_no2_column_stratospheric_avk);
    path = "/PRODUCT/averaging_kernel[], /PRODUCT/air_mass_factor_total[], "
        "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/air_mass_factor_stratosphere[], /PRODUCT/tm5_tropopause_layer_index[]";
    description = "averaging_kernel[layer] = if layer > tm5_tropopause_layer_index then "
        "averaging_kernel[layer] * air_mass_factor_total / air_mass_factor_stratosphere else 0";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);
}

static void register_so2_product(void)
{
    const char *so2_column_options[] = { "1km", "7km", "15km" };
    const char *cloud_fraction_options[] = { "radiance" };
    const char *path;
    const char *description;
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_vertical };

    module = harp_ingestion_register_module_coda("S5P_L2_SO2", "Sentinel-5P", "Sentinel5P", "L2__SO2___",
                                                 "Sentinel-5P L2 SO2 total column", ingestion_init, ingestion_done);

    harp_ingestion_register_option(module, "so2_column", "whether to ingest the SO2 column derived from the TM5 "
                                   "profile (default), from the 1km box profile (so2_column=1km), from the 7km box "
                                   "profile (so2_column=7km), or from the 15km box profile (so2_column=15km); "
                                   "providing this option will only work for NRTI data (otherwise an empty product is "
                                   "returned)", 3, so2_column_options);

    harp_ingestion_register_option(module, "cloud_fraction", "whether to ingest the cloud fraction (default) or the "
                                   "radiance cloud fraction (cloud_fraction=radiance)", 1, cloud_fraction_options);

    product_definition = harp_ingestion_register_product(module, "S5P_L2_SO2", NULL, read_dimensions);
    register_core_variables(product_definition, s5p_delta_time_num_dims[s5p_type_so2]);
    register_geolocation_variables(product_definition);
    register_additional_geolocation_variables(product_definition);

    register_cloud_variables(product_definition);

    /* surface_albedo */
    description = "surface albedo";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "surface_albedo", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_so2_surface_albedo);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/surface_albedo_328nm, "
        "/PRODUCT/SUPPORT_DATA/INPUT_DATA/surface_albedo_376nm, "
        "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/selected_fitting_window_flag";
    description = "if selected_fitting_window_flag is 1 or 2 then use surface_albedo_328, if "
        "selected_fitting_window_flag is 3 then use surface_albedo_376, else set to NaN";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    register_surface_variables(product_definition);

    /* SO2_column_number_density */
    description = "SO2 vertical column density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "SO2_column_number_density", harp_type_float, 1,
                                                   dimension_type, NULL, description, "mol/m^2", NULL,
                                                   read_so2_total_vertical_column);
    path = "/PRODUCT/sulfurdioxide_total_vertical_column[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column unset", NULL, path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfurdioxide_total_vertical_column_1km[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column=1km", "NRTI", path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfurdioxide_total_vertical_column_7km[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column=7km", "NRTI", path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfurdioxide_total_vertical_column_15km[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column=15km", "NRTI", path, NULL);

    /* SO2_column_number_density_uncertainty_random */
    description = "random component of the uncertainty of the SO2 vertical column density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "SO2_column_number_density_uncertainty_random",
                                                   harp_type_float, 1, dimension_type, NULL, description, "mol/m^2",
                                                   exclude_non_nrti, read_so2_total_vertical_column_precision);
    path = "/PRODUCT/sulfurdioxide_total_vertical_column_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column unset", "NRTI", path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfurdioxide_total_vertical_column_1km_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column=1km", "NRTI", path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfurdioxide_total_vertical_column_7km_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column=7km", "NRTI", path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfurdioxide_total_vertical_column_15km_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column=15km", "NRTI", path, NULL);

    /* SO2_column_number_density_uncertainty_systematic */
    description = "systematic component of the uncertainty of the SO2 vertical column density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "SO2_column_number_density_uncertainty_systematic", harp_type_float,
                                                   1, dimension_type, NULL, description, "mol/m^2",
                                                   exclude_non_nrti, read_so2_total_vertical_column_trueness);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfurdioxide_total_vertical_column_trueness[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column unset", "NRTI", path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfurdioxide_total_vertical_column_1km_trueness[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column=1km", "NRTI", path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfurdioxide_total_vertical_column_7km_trueness[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column=7km", "NRTI", path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfurdioxide_total_vertical_column_15km_trueness[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column=15km", "NRTI", path, NULL);

    /* SO2_column_number_density_amf */
    description = "total air mass factor";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "SO2_column_number_density_amf", harp_type_float,
                                                   1, dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS,
                                                   NULL, read_so2_total_air_mass_factor);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfurdioxide_total_air_mass_factor_polluted[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column unset", NULL, path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfurdioxide_total_air_mass_factor_1km[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column=1km", "NRTI", path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfurdioxide_total_air_mass_factor_7km[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column=7km", "NRTI", path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfurdioxide_total_air_mass_factor_15km[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column=15km", "NRTI", path, NULL);

    /* SO2_column_number_density_amf_uncertainty_random */
    description = "random component of the uncertainty of the total air mass factor";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "SO2_column_number_density_amf_uncertainty_random", harp_type_float,
                                                   1, dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS,
                                                   exclude_non_nrti, read_so2_total_air_mass_factor_precision);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfurdioxide_total_air_mass_factor_polluted_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column unset", "NRTI", path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfurdioxide_total_air_mass_factor_1km_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column=1km", "NRTI", path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfurdioxide_total_air_mass_factor_7km_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column=7km", "NRTI", path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfurdioxide_total_air_mass_factor_15km_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column=15km", "NRTI", path, NULL);

    /* SO2_column_number_density_amf_uncertainty_systematic */
    description = "systematic component of the uncertainty of the total air mass factor";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "SO2_column_number_density_amf_uncertainty_systematic",
                                                   harp_type_float, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, exclude_non_nrti,
                                                   read_so2_total_air_mass_factor_trueness);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfurdioxide_total_air_mass_factor_polluted_trueness[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column unset", "NRTI", path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfurdioxide_total_air_mass_factor_1km_trueness[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column=1km", "NRTI", path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfurdioxide_total_air_mass_factor_7km_trueness[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column=7km", "NRTI", path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfurdioxide_total_air_mass_factor_15km_trueness[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column=15km", "NRTI", path, NULL);

    /* SO2_column_number_density_avk */
    description = "averaging kernel for the SO2 vertical column density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "SO2_column_number_density_avk", harp_type_float,
                                                   2, dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_results_averaging_kernel_1d);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/averaging_kernel[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* SO2_volume_mixing_ratio_apriori */
    description = "SO2 apriori profile in volume mixing ratios";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "SO2_volume_mixing_ratio_apriori",
                                                   harp_type_float, 2, dimension_type, NULL, description, "ppv",
                                                   exclude_so2_apriori_profile,
                                                   read_results_sulfurdioxide_profile_apriori);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfurdioxide_profile_apriori[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column unset", NULL, path, NULL);

    /* O3_column_number_density */
    description = "O3 vertical column density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_column_number_density", harp_type_float, 1,
                                                   dimension_type, NULL, description, "mol/m^2", NULL,
                                                   read_input_ozone_total_vertical_column);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/ozone_total_vertical_column[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* O3_column_number_density_uncertainty */
    description = "random component of the uncertainty of the O3 vertical column density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_column_number_density_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description, "mol/m^2",
                                                   NULL, read_input_ozone_total_vertical_column_precision);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/ozone_total_vertical_column_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

static void register_cloud_cal_variables(harp_product_definition *product_definition)
{
    const char *path;
    const char *description;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };

    register_core_variables(product_definition, s5p_delta_time_num_dims[s5p_type_cloud]);
    register_geolocation_variables(product_definition);
    register_additional_geolocation_variables(product_definition);

    /* cloud_fraction */
    description = "retrieved fraction of horizontal area occupied by clouds using the OCRA/ROCINN CAL model";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_fraction", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_product_cloud_fraction);
    path = "/PRODUCT/cloud_fraction[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_fraction_uncertainty */
    description = "uncertainty of the retrieved fraction of horizontal area occupied by clouds using the OCRA/ROCINN "
        "CAL model";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_fraction_uncertainty", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_product_cloud_fraction_precision);
    path = "/PRODUCT/cloud_fraction_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_fraction_apriori */
    description = "effective radiometric cloud fraction a priori";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_fraction_apriori", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_results_cloud_fraction_apriori);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/cloud_fraction_apriori[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_base_pressure */
    description = "cloud base pressure calculated using the OCRA/ROCINN CAL model";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_base_pressure", harp_type_float, 1,
                                                   dimension_type, NULL, description, "Pa", NULL,
                                                   read_product_cloud_base_pressure);
    path = "/PRODUCT/cloud_base_pressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_base_pressure_uncertainty */
    description = "error of the cloud base pressure calculated using the OCRA/ROCINN CAL model";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_base_pressure_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description, "Pa", NULL,
                                                   read_product_cloud_base_pressure_precision);
    path = "/PRODUCT/cloud_base_pressure_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_base_height */
    description = "cloud base height calculated using the OCRA/ROCINN CAL model";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_base_height", harp_type_float, 1,
                                                   dimension_type, NULL, description, "m", NULL,
                                                   read_product_cloud_base_height);
    path = "/PRODUCT/cloud_base_height[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_base_height_uncertainty */
    description = "error of the cloud base height calculated using the OCRA/ROCINN CAL model";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_base_height_uncertainty", harp_type_float,
                                                   1, dimension_type, NULL, description, "m", NULL,
                                                   read_product_cloud_base_height_precision);
    path = "/PRODUCT/cloud_base_height_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_top_pressure */
    description = "retrieved atmospheric pressure at the level of cloud top using the OCRA/ROCINN CAL model";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_top_pressure", harp_type_float, 1,
                                                   dimension_type, NULL, description, "Pa", NULL,
                                                   read_product_cloud_top_pressure);
    path = "/PRODUCT/cloud_top_pressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_top_pressure_uncertainty */
    description = "uncertainty of the retrieved atmospheric pressure at the level of cloud top using the OCRA/ROCINN "
        "CAL model";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_top_pressure_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description, "Pa", NULL,
                                                   read_product_cloud_top_pressure_precision);
    path = "/PRODUCT/cloud_top_pressure_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_top_height */
    description = "retrieved vertical distance above the surface of the cloud top using the OCRA/ROCINN CAL model";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_top_height", harp_type_float, 1,
                                                   dimension_type, NULL, description, "m", NULL,
                                                   read_product_cloud_top_height);
    path = "/PRODUCT/cloud_top_height[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_top_height_uncertainty */
    description = "uncertainty of the retrieved vertical distance above the surface of the cloud top using the "
        "OCRA/ROCINN CAL model";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_top_height_uncertainty", harp_type_float,
                                                   1, dimension_type, NULL, description, "m", NULL,
                                                   read_product_cloud_top_height_precision);
    path = "/PRODUCT/cloud_top_height_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_optical_depth */
    description = "retrieved cloud optical depth using the OCRA/ROCINN CAL model";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_optical_depth", harp_type_float, 1,
                                                   dimension_type, NULL, description, "m", NULL,
                                                   read_product_cloud_optical_thickness);
    path = "/PRODUCT/cloud_optical_thickness[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_optical_depth_uncertainty */
    description = "uncertainty of the retrieved cloud optical depth using the OCRA/ROCINN CAL model";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_optical_depth_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description, "m", NULL,
                                                   read_product_cloud_optical_thickness_precision);
    path = "/PRODUCT/cloud_optical_thickness_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* surface_albedo */
    description = "surface albedo fitted using the OCRA/ROCINN CAL model";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "surface_albedo", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_results_surface_albedo_fitted);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/surface_albedo_fitted[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* surface_albedo_uncertainty */
    description = "uncertainty of the surface albedo fitted using the OCRA/ROCINN CAL model";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "surface_albedo_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_results_surface_albedo_fitted_precision);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/surface_albedo_fitted_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    register_surface_variables(product_definition);
}

static void register_cloud_crb_variables(harp_product_definition *product_definition)
{
    const char *path;
    const char *description;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };

    register_core_variables(product_definition, s5p_delta_time_num_dims[s5p_type_cloud]);
    register_geolocation_variables(product_definition);
    register_additional_geolocation_variables(product_definition);

    /* cloud_fraction */
    description = "retrieved effective radiometric cloud fraction using the OCRA/ROCINN CRB model";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_fraction", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_results_cloud_fraction_crb);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/cloud_fraction_crb[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_fraction_uncertainty */
    description = "uncertainty of the retrieved effective radiometric cloud fraction using the OCRA/ROCINN CRB model";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_fraction_uncertainty", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_results_cloud_fraction_crb_precision);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/cloud_fraction_crb_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_fraction_apriori */
    description = "effective radiometric cloud fraction a priori";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_fraction_apriori", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_results_cloud_fraction_apriori);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/cloud_fraction_apriori[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_pressure */
    description = "retrieved atmospheric pressure at the level of cloud using the OCRA/ROCINN CRB model";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_pressure", harp_type_float, 1,
                                                   dimension_type, NULL, description, "Pa", NULL,
                                                   read_results_cloud_pressure_crb);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/cloud_pressure_crb[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_pressure_uncertainty */
    description = "error of the retrieved atmospheric pressure at the level of cloud using the OCRA/ROCINN CRB model";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_pressure_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description, "Pa", NULL,
                                                   read_results_cloud_pressure_crb_precision);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/cloud_pressure_crb_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_height */
    description = "retrieved atmospheric height at the level of cloud using the OCRA/ROCINN CRB model";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_height", harp_type_float, 1,
                                                   dimension_type, NULL, description, "m", NULL,
                                                   read_results_cloud_height_crb);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/cloud_height_crb[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_height_uncertainty */
    description = "error of the retrieved atmospheric height at the level of cloud using the OCRA/ROCINN CRB model";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_height_uncertainty", harp_type_float,
                                                   1, dimension_type, NULL, description, "m", NULL,
                                                   read_results_cloud_height_crb_precision);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/cloud_height_crb_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_albedo */
    description = "albedo of cloud using the OCRA/ROCINN CRB model";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_albedo", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_results_cloud_albedo_crb);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/cloud_albedo_crb[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_albedo_uncertainty */
    description = "uncertainty of the albedo of cloud using the OCRA/ROCINN CRB model";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_albedo_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_results_cloud_albedo_crb_precision);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/cloud_albedo_crb_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* surface_albedo */
    description = "surface albedo fitted using the OCRA/ROCINN CRB model";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "surface_albedo", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_results_surface_albedo_fitted_crb);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/surface_albedo_fitted_crb[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* surface_albedo_uncertainty */
    description = "uncertainty of the surface albedo fitted using the OCRA/ROCINN CRB model";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "surface_albedo_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_results_surface_albedo_fitted_crb_precision);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/surface_albedo_fitted_crb_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    register_surface_variables(product_definition);
}

static void register_cloud_product(void)
{
    const char *model_options[] = { "CAL", "CRB" };
    harp_ingestion_module *module;
    harp_product_definition *product_definition;

    module = harp_ingestion_register_module_coda("S5P_L2_CLOUD", "Sentinel-5P", "Sentinel5P", "L2__CLOUD_",
                                                 "Sentinel-5P L2 cloud properties", ingestion_init, ingestion_done);

    harp_ingestion_register_option(module, "model", "whether to retrieve the cloud properties from the CAL model or "
                                   "the CRB model; option values are 'CAL' (default) and 'CRB'", 2, model_options);

    product_definition = harp_ingestion_register_product(module, "S5P_L2_CLOUD_CAL", NULL, read_dimensions);
    harp_product_definition_add_mapping(product_definition, NULL, "model=CAL or model unset");
    register_cloud_cal_variables(product_definition);

    product_definition = harp_ingestion_register_product(module, "S5P_L2_CLOUD_CRB", NULL, read_dimensions);
    harp_product_definition_add_mapping(product_definition, NULL, "model=CRB");
    register_cloud_crb_variables(product_definition);
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
    register_core_variables(product_definition, s5p_delta_time_num_dims[s5p_type_fresco]);
    register_geolocation_variables(product_definition);
    register_additional_geolocation_variables(product_definition);

    /* cloud_fraction */
    description = "effective cloud fraction retrieved from the O2 A-band";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_fraction", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_product_cloud_fraction_crb);
    path = "/PRODUCT/cloud_fraction_crb[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_fraction_uncertainty */
    description = "uncertainty of the effective cloud fraction";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_fraction_uncertainty", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_product_cloud_fraction_crb_precision);
    path = "/PRODUCT/cloud_fraction_crb_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_pressure */
    description = "cloud optical centroid pressure retrieved from the O2 A-band";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_pressure", harp_type_float, 1,
                                                   dimension_type, NULL, description, "Pa", NULL,
                                                   read_product_cloud_pressure_crb);
    path = "/PRODUCT/cloud_pressure_crb[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_pressure_uncertainty */
    description = "uncertainty of the cloud optical centroid pressure";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_pressure_uncertainty", harp_type_float, 1,
                                                   dimension_type, NULL, description, "Pa", NULL,
                                                   read_product_cloud_pressure_crb_precision);
    path = "/PRODUCT/cloud_pressure_crb_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_height */
    description = "cloud optical centroid height with respect to the surface";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_height", harp_type_float, 1,
                                                   dimension_type, NULL, description, "m", NULL,
                                                   read_product_cloud_height_crb);
    path = "/PRODUCT/cloud_height_crb[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_height_uncertainty */
    description = "uncertainty of the cloud optical centroid height";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_height_uncertainty", harp_type_float, 1,
                                                   dimension_type, NULL, description, "m", NULL,
                                                   read_product_cloud_height_crb_precision);
    path = "/PRODUCT/cloud_height_crb_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_albedo */
    description = "cloud albedo; this is a fixed value for FRESCO";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_albedo", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_product_cloud_albedo_crb);
    path = "/PRODUCT/cloud_albedo_crb[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_albedo_uncertainty */
    description = "cloud albedo error; since cloud albedo is fixed for FRESCO, this value is set to NaN";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_albedo_uncertainty", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_product_cloud_albedo_crb_precision);
    path = "/PRODUCT/cloud_albedo_crb_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

int harp_ingestion_module_s5p_l2_init(void)
{
    register_aer_ai_product();
    register_aer_lh_product();
    register_ch4_product();
    register_co_product();
    register_hcho_product();
    register_o3_product();
    register_o3_pr_product();
    register_o3_tcl_product();
    register_o3_tpr_product();
    register_no2_product();
    register_so2_product();
    register_cloud_product();
    register_fresco_product();

    return 0;
}
