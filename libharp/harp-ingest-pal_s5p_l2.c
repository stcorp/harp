/*
 * Copyright (C) 2015-2023 S[&]T, The Netherlands.
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

static const char *snow_ice_type_values[] = { "snow_free_land", "sea_ice", "permanent_ice", "snow", "ocean" };

typedef enum pal_s5p_product_type_enum
{
    pal_s5p_type_aer_ot,
    pal_s5p_type_bro,
    pal_s5p_type_chocho,
    pal_s5p_type_sif,
    pal_s5p_type_so2cbr,
    pal_s5p_type_tcwv
} pal_s5p_product_type;

/* The number of PAL-S5P products */
#define PAL_S5P_NUM_PRODUCT_TYPES (((int)pal_s5p_type_tcwv) + 1)

/* Type of dimensions */
typedef enum pal_s5p_dimension_type_enum
{
    pal_s5p_dim_time,
    pal_s5p_dim_scanline,
    pal_s5p_dim_pixel,
    pal_s5p_dim_corner,
    pal_s5p_dim_wavelength,
    pal_s5p_dim_layer
} pal_s5p_dimension_type;

/* The number of type of dimensions */
#define PAL_S5P_NUM_DIM_TYPES (((int)pal_s5p_dim_layer) + 1)

static const char *pal_s5p_dimension_name[PAL_S5P_NUM_PRODUCT_TYPES][PAL_S5P_NUM_DIM_TYPES] = {
    {"time", "scanline", "ground_pixel", "corner", "wavelength", NULL}, /* pal_s5p_type_aer_ot */
    {"time", "scanline", "ground_pixel", "corner", NULL, NULL}, /* pal_s5p_type_bro    */
    {"time", "scanline", "ground_pixel", "corner", NULL, NULL}, /* pal_s5p_type_chocho */
    {"time", "scanline", "ground_pixel", "corner", NULL, NULL}, /* pal_s5p_type_sif    */
    {"time", "scanline", "ground_pixel", "corner", NULL, "layer"},      /* pal_s5p_type_so2cbr */
    {"time", "scanline", "ground_pixel", "corner", NULL, "layer"}       /* pal_s5p_type_tcwv   */
};

typedef struct ingest_info_struct
{
    coda_product *product;

    int so2_column_type;        /* 0: total (tm5 profile), 1: 1km box profile, 2: 7km box profile, 3: 15km box profile */
    int use_sif_735;    /* 0: sif_743 (default), 1: sif_735 */

    int use_radiance_cloud_fraction;
    int use_custom_qa_filter;

    pal_s5p_product_type product_type;

    long num_times;
    long num_scanlines;
    long num_pixels;
    long num_corners;
    long num_wavelengths;
    long num_layers;

    coda_cursor product_cursor;
    coda_cursor geolocation_cursor;
    coda_cursor detailed_results_cursor;
    coda_cursor input_data_cursor;

    int processor_version;
    int collection_number;
    int wavelength_ratio;
} ingest_info;

static void broadcast_array_float(long num_scanlines, long num_pixels, float *data)
{
    long i;

    /* Repeat the value for each scanline for all pixels in that scanline.
     * Iterate in reverse to avoid overwriting scanline values.
     */
    for (i = num_scanlines - 1; i >= 0; i--)
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

static void ingestion_done(void *user_data)
{
    ingest_info *info = (ingest_info *)user_data;

    free(info);
}

static int read_dataset(coda_cursor cursor,
                        const char *dataset_name, harp_data_type data_type, long num_elements, harp_array data)
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

            /* Some variables have a fill value in their attributes. If this is available, then it is used.
             * Not having this attribute is not an error. */
            if (coda_cursor_goto(&cursor, "@FillValue[0]") == 0)
            {
                if (coda_cursor_read_float(&cursor, &fill_value.float_data) != 0)
                {
                    harp_set_error(HARP_ERROR_CODA, NULL);
                    return -1;
                }
                /* Replace values equal to the _FillValue variable attribute by NaN. */
                harp_array_replace_fill_value(data_type, num_elements, data, fill_value);
            }
            break;
        case harp_type_double:
            if (coda_cursor_read_double_array(&cursor, data.double_data, coda_array_ordering_c) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }

            /* Some variables have a fill value in their attributes. If this is available, then it is used.
             * Not having this attribute is not an error. */
            if (coda_cursor_goto(&cursor, "@FillValue[0]") == 0)
            {
                if (coda_cursor_read_double(&cursor, &fill_value.double_data) != 0)
                {
                    harp_set_error(HARP_ERROR_CODA, NULL);
                    return -1;
                }
                /* Replace values equal to the _FillValue variable attribute by NaN. */
                harp_array_replace_fill_value(data_type, num_elements, data, fill_value);
            }
            break;
        default:
            assert(0);
            exit(1);
    }

    return 0;
}

static int read_array(coda_cursor cursor,
                      const char *path, harp_data_type data_type, long num_elements, harp_array data)
{
    long coda_num_elements;

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
        harp_set_error(HARP_ERROR_INGESTION,
                       "variable has %ld elements; expected %ld", coda_num_elements, num_elements);
        harp_add_coda_cursor_path_to_error_message(&cursor);
        return -1;
    }

    switch (data_type)
    {
        case harp_type_int8:
            if (coda_cursor_read_int8_array(&cursor, data.int8_data, coda_array_ordering_c) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            break;
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
            break;
        case harp_type_double:
            if (coda_cursor_read_double_array(&cursor, data.double_data, coda_array_ordering_c) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            break;
        default:
            assert(0);
            exit(1);
    }

    return 0;
}

static const char *get_product_type_name(pal_s5p_product_type product_type)
{
    switch (product_type)
    {
        case pal_s5p_type_bro:
            return "L2__BRO___";
        case pal_s5p_type_tcwv:
            return "L2__TCWV__";
        case pal_s5p_type_aer_ot:
            return "L2__AER_OT";
        case pal_s5p_type_chocho:
            return "L2__CHOCHO";
        case pal_s5p_type_so2cbr:
            return "L2__SO2CBR";
        case pal_s5p_type_sif:
            return "L2__SIF___";
    }

    assert(0);
    exit(1);
}

static int get_product_type(coda_product *product, pal_s5p_product_type *product_type)
{
    const char *coda_product_type;
    int i;

    if (coda_get_product_type(product, &coda_product_type) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    for (i = 0; i < PAL_S5P_NUM_PRODUCT_TYPES; i++)
    {
        if (strcmp(get_product_type_name((pal_s5p_product_type)i), coda_product_type) == 0)
        {
            *product_type = ((pal_s5p_product_type)i);
            return 0;
        }
    }

    harp_set_error(HARP_ERROR_INGESTION, "unsupported product type '%s'", coda_product_type);
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
    /* time */
    if (pal_s5p_dimension_name[info->product_type][pal_s5p_dim_time] != NULL)
    {
        if (get_dimension_length(info, pal_s5p_dimension_name[info->product_type][pal_s5p_dim_time], &info->num_times)
            != 0)
        {
            return -1;
        }

        if (info->num_times != 1)
        {
            harp_set_error(HARP_ERROR_INGESTION, "dimension '%s' has length %ld; expected 1",
                           pal_s5p_dimension_name[pal_s5p_dim_time], info->num_times);
            return -1;
        }
    }

    /* scanline */
    if (pal_s5p_dimension_name[info->product_type][pal_s5p_dim_scanline] != NULL)
    {
        if (get_dimension_length(info, pal_s5p_dimension_name[info->product_type][pal_s5p_dim_scanline],
                                 &info->num_scanlines) != 0)
        {
            return -1;
        }
    }

    /* pixel */
    if (pal_s5p_dimension_name[info->product_type][pal_s5p_dim_pixel] != NULL)
    {
        if (get_dimension_length(info, pal_s5p_dimension_name[info->product_type][pal_s5p_dim_pixel], &info->num_pixels)
            != 0)
        {
            return -1;
        }
    }

    /* corners */
    if (pal_s5p_dimension_name[info->product_type][pal_s5p_dim_corner] != NULL)
    {
        if (get_dimension_length(info, pal_s5p_dimension_name[info->product_type][pal_s5p_dim_corner],
                                 &info->num_corners) != 0)
        {
            return -1;
        }

        if (info->num_corners != 4)
        {
            harp_set_error(HARP_ERROR_INGESTION, "dimension '%s' has length %ld; expected 4",
                           pal_s5p_dimension_name[info->product_type][pal_s5p_dim_corner], info->num_corners);
            return -1;
        }
    }

    /* wavelength */
    if (pal_s5p_dimension_name[info->product_type][pal_s5p_dim_wavelength] != NULL)
    {
        if (get_dimension_length(info, pal_s5p_dimension_name[info->product_type][pal_s5p_dim_wavelength],
                                 &info->num_wavelengths) != 0)
        {
            return -1;
        }
    }

    /* layer */
    if (pal_s5p_dimension_name[info->product_type][pal_s5p_dim_layer] != NULL)
    {
        if (get_dimension_length(info, pal_s5p_dimension_name[info->product_type][pal_s5p_dim_layer],
                                 &info->num_layers) != 0)
        {
            return -1;
        }
    }

    return 0;
}

static int init_versions(ingest_info *info)
{
    coda_cursor cursor;
    char product_name[84];

    /* since earlier S5P L2 products did not always have a valid 'id' global attribute
     * we will keep the version numbers at -1 if we can't extract the right information.
     */
    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, "/@id") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_string(&cursor, product_name, 84) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (strlen(product_name) != 83)
    {
        harp_set_error(HARP_ERROR_INGESTION, "'id' attribute does not contain a valid logical product name");
        return -1;
    }
    info->collection_number = (int)strtol(&product_name[58], NULL, 10);
    info->processor_version = (int)strtol(&product_name[61], NULL, 10);

    return 0;
}

static int ingestion_init(const harp_ingestion_module *module,
                          coda_product *product,
                          const harp_ingestion_options *options, harp_product_definition **definition, void **user_data)
{
    const char *option_value;
    ingest_info *info;

    info = malloc(sizeof(ingest_info));
    if (info == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(ingest_info), __FILE__, __LINE__);
        return -1;
    }

    info->product = product;
    info->so2_column_type = 0;
    info->use_sif_735 = 0;
    info->num_times = 0;
    info->num_scanlines = 0;
    info->num_pixels = 0;
    info->num_corners = 0;
    info->num_layers = 0;

    if (get_product_type(info->product, &info->product_type) != 0)
    {
        ingestion_done(info);
        return -1;
    }

    if (init_versions(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }

    if (harp_ingestion_options_has_option(options, "735"))
    {
        info->use_sif_735 = 1;
    }

    if (harp_ingestion_options_has_option(options, "so2_column"))
    {
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

static int read_dimensions(void *user_data, long dimension[HARP_NUM_DIM_TYPES])
{
    ingest_info *info = (ingest_info *)user_data;

    dimension[harp_dimension_time] = info->num_times * info->num_scanlines * info->num_pixels;

    if (info->product_type == pal_s5p_type_aer_ot)
    {
        dimension[harp_dimension_spectral] = info->num_wavelengths;
    }

    if ((info->product_type == pal_s5p_type_so2cbr) || (info->product_type == pal_s5p_type_tcwv))
    {
        dimension[harp_dimension_vertical] = info->num_layers;
    }

    return 0;
}

static void broadcast_array_double(long num_scanlines, long num_pixels, double *data)
{
    long i;

    /* Repeat the value for each scanline for all pixels in that scanline.
     * Iterate in reverse to avoid overwriting scanline values.
     */
    for (i = num_scanlines - 1; i >= 0; i--)
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
    /* Broadcast the result along the pixel dimension. */
    broadcast_array_double(info->num_scanlines, info->num_pixels, data.double_data);

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
        harp_set_error(HARP_ERROR_INGESTION,
                       "could not extract value from " "time_coverage_resolution attribute ('%s')", string_value);
        return -1;
    }
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

static int read_aot_qa_value(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    harp_array qa_value;
    long num_elements = info->num_scanlines * info->num_pixels * info->num_wavelengths;
    long i;
    int result;

    qa_value.ptr = malloc(num_elements * sizeof(float));
    if (qa_value.ptr == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_elements * sizeof(float), __FILE__, __LINE__);
        return -1;
    }
    /* we don't want the add_offset/scale_factor applied for the qa_value; we just want the raw 8bit value */
    coda_set_option_perform_conversions(0);
    result = read_dataset(info->product_cursor, "qa_value", harp_type_float, num_elements, qa_value);
    coda_set_option_perform_conversions(1);
    if (result != 0)
    {
        free(qa_value.ptr);
        return result;
    }

    for (i = 0; i < num_elements; i++)
    {
        data.int8_data[i] = (int)(100 * qa_value.float_data[i]);
    }
    free(qa_value.ptr);

    return result;
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

static int read_input_absorbing_aerosol_index(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "absorbing_aerosol_index", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_input_aerosol_index_340_380(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "aerosol_index_340_380", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_input_aerosol_index_354_388(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "aerosol_index_354_388", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_input_cloud_albedo(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "cloud_albedo", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
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

static int read_input_cloud_fraction(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "cloud_fraction", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_input_cloud_fraction_crb(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "cloud_fraction_crb", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_input_cloud_fraction_crb_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "cloud_fraction_crb_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_input_cloud_fraction_l2(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "cloud_fraction_L2", harp_type_float,
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

static int read_input_cloud_pressure(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "cloud_pressure", harp_type_float,
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

static int read_input_eastward_wind(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "eastward_wind", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_input_northward_wind(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "northward_wind", harp_type_float,
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

static int read_input_surface_albedo(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "surface_albedo", harp_type_float,
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

static int read_input_surface_temperature(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "surface_temperature", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_input_tm5_pressure(void *user_data, harp_array data)
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
    hybride_coef_a.ptr = malloc(num_layers * sizeof(double));
    if (hybride_coef_a.ptr == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_layers * sizeof(double), __FILE__, __LINE__);
        return -1;
    }

    hybride_coef_b.ptr = malloc(num_layers * sizeof(double));
    if (hybride_coef_b.ptr == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_layers * sizeof(double), __FILE__, __LINE__);
        free(hybride_coef_a.ptr);
        return -1;
    }

    if (read_dataset(info->input_data_cursor, "tm5_constant_a", harp_type_double, num_layers, hybride_coef_a) != 0)
    {
        free(hybride_coef_b.ptr);
        free(hybride_coef_a.ptr);
        return -1;
    }

    if (read_dataset(info->input_data_cursor, "tm5_constant_b", harp_type_double, num_layers, hybride_coef_b) != 0)
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
        double *pressure = &data.double_data[i * num_layers];   /* pressure for specific (time, lat, lon) */
        double surface_pressure = data.double_data[i];  /* surface pressure at specific (time, lat, lon) */
        long j;

        for (j = 0; j < num_layers; j++)
        {
            pressure[j] = hybride_coef_a.double_data[j] + hybride_coef_b.double_data[j] * surface_pressure;
        }
    }

    free(hybride_coef_b.ptr);
    free(hybride_coef_a.ptr);

    return 0;
}

static int read_input_wind_speed(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "wind_speed", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_product_aerosol_optical_thickness(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "aerosol_optical_thickness", harp_type_float,
                        info->num_scanlines * info->num_pixels * info->num_wavelengths, data);
}

static int read_product_aerosol_type(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->product_cursor, "aerosol_type", harp_type_int32, info->num_scanlines * info->num_pixels,
                      data);
}

static int read_product_brominemonoxide_total_vertical_column(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "brominemonoxide_total_vertical_column", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_product_brominemonoxide_total_vertical_column_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "brominemonoxide_total_vertical_column_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_product_glyoxal_tropospheric_vertical_column(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "glyoxal_tropospheric_vertical_column", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_product_glyoxal_tropospheric_vertical_column_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->product_cursor, "glyoxal_tropospheric_vertical_column_precision", harp_type_float,
                      info->num_scanlines * info->num_pixels, data);
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

static int read_product_qa_value(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int result;

    /* we don't want the add_offset/scale_factor applied for the qa_value; we just want the raw 8bit value */
    coda_set_option_perform_conversions(0);
    result = read_dataset(info->product_cursor, "qa_value", harp_type_int8, info->num_scanlines * info->num_pixels,
                          data);
    coda_set_option_perform_conversions(1);

    return result;
}

static int read_product_single_scattering_albedo(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "single_scattering_albedo", harp_type_float,
                        info->num_scanlines * info->num_pixels * info->num_wavelengths, data);
}

static int read_product_total_column_water_vapor(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "total_column_water_vapor", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_product_total_column_water_vapor_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "total_column_water_vapor_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_results_air_mass_factor_total(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "air_mass_factor_total", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_results_averaging_kernel(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "averaging_kernel", harp_type_float,
                        info->num_scanlines * info->num_pixels * info->num_layers, data);
}

static int read_results_brominemonoxide_geometric_air_mass_factor(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "brominemonoxide_geometric_air_mass_factor", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_results_brominemonoxide_total_vertical_column_trueness(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char *variable_name = "brominemonoxide_total_vertical_column_trueness";

    return read_dataset(info->detailed_results_cursor,
                        variable_name, harp_type_float, info->num_scanlines * info->num_pixels, data);
}

static int read_results_water_vapor_profile_apriori(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "water_vapor_profile_apriori", harp_type_float,
                        info->num_scanlines * info->num_pixels * info->num_layers, data);
}

static int read_sea_ice_fraction_from_flag(void *user_data, const char *variable_name, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long i;

    if (read_dataset(info->input_data_cursor, variable_name, harp_type_float, info->num_scanlines * info->num_pixels,
                     data) != 0)
    {
        return -1;
    }
    for (i = 0; i < info->num_scanlines * info->num_pixels; i++)
    {
        if (data.float_data[i] > 0 && data.float_data[i] <= 100)
        {
            data.float_data[i] /= (float)100.0;
        }
        else
        {
            data.float_data[i] = 0.0;
        }
    }

    return 0;
}

static int read_sea_ice_fraction(void *user_data, harp_array data)
{
    return read_sea_ice_fraction_from_flag(user_data, "snow_ice_flag", data);
}

static int read_sea_ice_fraction_nise(void *user_data, harp_array data)
{
    return read_sea_ice_fraction_from_flag(user_data, "snow_ice_flag_nise", data);
}

static int read_sif(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->use_sif_735)
    {
        return read_dataset(info->product_cursor, "SIF_735", harp_type_float, info->num_scanlines * info->num_pixels,
                            data);
    }

    return read_dataset(info->product_cursor, "SIF_743", harp_type_float, info->num_scanlines * info->num_pixels, data);
}

static int read_sif_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->use_sif_735)
    {
        return read_dataset(info->product_cursor, "SIF_ERROR_735", harp_type_float,
                            info->num_scanlines * info->num_pixels, data);
    }

    return read_dataset(info->product_cursor, "SIF_ERROR_743", harp_type_float, info->num_scanlines * info->num_pixels,
                        data);
}

static int read_sif_qa_value(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    harp_array qa_value;
    long num_elements = info->num_scanlines * info->num_pixels;
    long i;
    int result;

    qa_value.ptr = malloc(num_elements * sizeof(float));
    if (qa_value.ptr == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_elements * sizeof(float), __FILE__, __LINE__);
        return -1;
    }

    /* we don't want the add_offset/scale_factor applied for the qa_value; we just want the raw 8bit value */
    coda_set_option_perform_conversions(0);
    if (info->use_sif_735)
    {
        result = read_dataset(info->detailed_results_cursor, "QA_value_735", harp_type_float, num_elements, qa_value);
    }
    result = read_dataset(info->detailed_results_cursor, "QA_value_743", harp_type_float, num_elements, qa_value);
    coda_set_option_perform_conversions(1);
    if (result != 0)
    {
        free(qa_value.ptr);
        return result;
    }

    for (i = 0; i < num_elements; i++)
    {
        data.int8_data[i] = (int)(100 * qa_value.float_data[i]);
    }
    free(qa_value.ptr);

    return 0;
}

static int read_snow_ice_type_from_flag(void *user_data, const char *variable_name, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long i;

    if (read_dataset(info->input_data_cursor, variable_name, harp_type_int8, info->num_scanlines * info->num_pixels,
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

static int read_snow_ice_type(void *user_data, harp_array data)
{
    return read_snow_ice_type_from_flag(user_data, "snow_ice_flag", data);
}

static int read_snow_ice_type_nise(void *user_data, harp_array data)
{
    return read_snow_ice_type_from_flag(user_data, "snow_ice_flag_nise", data);
}

static int read_so2cbr_cloud_fraction(void *user_data, harp_array data)
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

static int read_so2cbr_cloud_fraction_precision(void *user_data, harp_array data)
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

static int read_so2cbr_surface_albedo(void *user_data, harp_array data)
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

static int read_so2cbr_tropopause_pressure(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    harp_array hybride_coef_a;
    harp_array hybride_coef_b;
    harp_array layer_index;
    long num_profiles = info->num_scanlines * info->num_pixels;
    long num_layers = info->num_layers;
    long i;

    layer_index.ptr = malloc(num_profiles * sizeof(int32_t));
    if (layer_index.ptr == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY,
                       "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_profiles * sizeof(int32_t), __FILE__, __LINE__);
        return -1;
    }

    hybride_coef_a.ptr = malloc(num_layers * sizeof(double));
    if (hybride_coef_a.ptr == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY,
                       "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_layers * sizeof(double), __FILE__, __LINE__);
        free(layer_index.ptr);
        return -1;
    }

    hybride_coef_b.ptr = malloc(num_layers * sizeof(double));
    if (hybride_coef_b.ptr == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY,
                       "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_layers * sizeof(double), __FILE__, __LINE__);
        free(hybride_coef_a.ptr);
        free(layer_index.ptr);
        return -1;
    }

    if (read_dataset(info->input_data_cursor,
                     "tm5_tropopause_layer_index", harp_type_int32, num_profiles, layer_index) != 0)
    {
        free(hybride_coef_b.ptr);
        free(hybride_coef_a.ptr);
        free(layer_index.ptr);
        return -1;
    }

    if (read_dataset(info->input_data_cursor, "tm5_constant_a", harp_type_double, num_layers, hybride_coef_a) != 0)
    {
        free(hybride_coef_b.ptr);
        free(hybride_coef_a.ptr);
        free(layer_index.ptr);
        return -1;
    }

    if (read_dataset(info->input_data_cursor, "tm5_constant_b", harp_type_double, num_layers, hybride_coef_b) != 0)
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

        if (index >= 0 && index < num_layers - 1)
        {
            /* surface pressure at specific (time, lat, lon)  */
            double surface_pressure = data.double_data[i];
            double layer_pressure, upper_layer_pressure;

            /* the tropause level is the upper boundary of
               the layer defined by layer_index  */
            layer_pressure = hybride_coef_a.double_data[index] + hybride_coef_b.double_data[index] * surface_pressure;
            upper_layer_pressure =
                hybride_coef_a.double_data[index + 1] + hybride_coef_b.double_data[index + 1] * surface_pressure;
            data.double_data[i] = exp((log(layer_pressure) + log(upper_layer_pressure)) / 2.0);
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

static int read_so2cbr_sulfurdioxide_total_air_mass_factor(void *user_data, harp_array data)
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

static int read_so2cbr_sulfurdioxide_total_air_mass_factor_trueness(void *user_data, harp_array data)
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

static int read_so2cbr_averaging_kernel(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    const char *scaling_variable_name = NULL;
    harp_array scaling;
    long num_elements = info->num_scanlines * info->num_pixels;
    long i, j;

    if (read_dataset(info->detailed_results_cursor, "averaging_kernel", harp_type_float,
                     info->num_scanlines * info->num_pixels * info->num_layers, data) != 0)
    {
        return -1;
    }

    switch (info->so2_column_type)
    {
        case 0:
            return 0;
        case 1:
            scaling_variable_name = "sulfurdioxide_averaging_kernel_scaling_box_1km";
            break;
        case 2:
            scaling_variable_name = "sulfurdioxide_averaging_kernel_scaling_box_7km";
            break;
        case 3:
            scaling_variable_name = "sulfurdioxide_averaging_kernel_scaling_box_15km";
            break;
    }

    scaling.ptr = malloc(num_elements * sizeof(float));
    if (scaling.ptr == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_elements * sizeof(float), __FILE__, __LINE__);
        return -1;
    }

    if (read_dataset(info->detailed_results_cursor, scaling_variable_name, harp_type_float, num_elements, scaling) != 0)
    {
        free(scaling.ptr);
        return -1;
    }

    for (i = 0; i < num_elements; i++)
    {
        long offset = i * info->num_layers;

        for (j = 0; j < info->num_layers; j++)
        {
            data.float_data[offset + j] *= scaling.float_data[i];
        }
    }
    free(scaling.ptr);

    return 0;
}

static int read_so2cbr_sulfurdioxide_profile_apriori(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "sulfurdioxide_profile_apriori", harp_type_float,
                        info->num_scanlines * info->num_pixels * info->num_layers, data);
}

static int read_so2cbr_sulfurdioxide_slant_column_corrected(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "sulfurdioxide_slant_column_corrected", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_so2cbr_sulfurdioxide_detection_flag(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    harp_array sulfurdioxide_detection_flag;
    long num_elements = info->num_scanlines * info->num_pixels;
    long i;

    sulfurdioxide_detection_flag.ptr = malloc(num_elements * sizeof(int32_t));
    if (sulfurdioxide_detection_flag.ptr == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_elements * sizeof(int32_t), __FILE__, __LINE__);
        return -1;
    }

    if (read_dataset(info->detailed_results_cursor, "sulfurdioxide_detection_flag", harp_type_int32, num_elements,
                     sulfurdioxide_detection_flag) != 0)
    {
        free(sulfurdioxide_detection_flag.ptr);
        return -1;
    }

    for (i = 0; i < num_elements; i++)
    {
        data.int8_data[i] = sulfurdioxide_detection_flag.int32_data[i];
    }

    free(sulfurdioxide_detection_flag.ptr);

    return 0;
}

static int read_so2cbr_sulfurdioxide_total_air_mass_factor_precision(void *user_data, harp_array data)
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

static int read_so2cbr_sulfurdioxide_total_vertical_column(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    switch (info->so2_column_type)
    {
        case 0:
            return read_dataset(info->product_cursor, "sulfurdioxide_total_vertical_column",
                                harp_type_float, info->num_scanlines * info->num_pixels, data);
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


static int read_so2cbr_sulfurdioxide_total_vertical_column_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    switch (info->so2_column_type)
    {
        case 0:
            return read_dataset(info->product_cursor, "sulfurdioxide_total_vertical_column_precision",
                                harp_type_float, info->num_scanlines * info->num_pixels, data);

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

static int read_so2cbr_sulfurdioxide_total_vertical_column_trueness(void *user_data, harp_array data)
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

static int read_tcwv_pressure_bounds(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    harp_array surface_pressure;
    harp_array pressure_constant_a_top;
    harp_array pressure_constant_a_bottom;
    harp_array pressure_constant_b_top;
    harp_array pressure_constant_b_bottom;
    long num_profiles;
    long num_layers;
    long i;
    long j;

    num_profiles = info->num_scanlines * info->num_pixels;
    num_layers = info->num_layers;

    surface_pressure.ptr = malloc(num_profiles * num_layers * 2);
    if (surface_pressure.ptr == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_layers * sizeof(double), __FILE__, __LINE__);
        return -1;
    }

    pressure_constant_a_top.ptr = malloc(num_layers * sizeof(double));
    if (pressure_constant_a_top.ptr == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_layers * sizeof(double), __FILE__, __LINE__);
        free(surface_pressure.ptr);
        return -1;
    }

    pressure_constant_a_bottom.ptr = malloc(num_layers * sizeof(double));
    if (pressure_constant_a_bottom.ptr == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_layers * sizeof(double), __FILE__, __LINE__);
        free(surface_pressure.ptr);
        free(pressure_constant_a_top.ptr);
        return -1;
    }

    pressure_constant_b_top.ptr = malloc(num_layers * sizeof(double));
    if (pressure_constant_b_top.ptr == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_layers * sizeof(double), __FILE__, __LINE__);
        free(surface_pressure.ptr);
        free(pressure_constant_a_top.ptr);
        free(pressure_constant_b_bottom.ptr);
        return -1;
    }

    pressure_constant_b_bottom.ptr = malloc(num_layers * sizeof(double));
    if (pressure_constant_b_bottom.ptr == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_layers * sizeof(double), __FILE__, __LINE__);
        free(surface_pressure.ptr);
        free(pressure_constant_a_top.ptr);
        free(pressure_constant_a_bottom.ptr);
        free(pressure_constant_b_top.ptr);
        return -1;
    }

    if (read_dataset(info->input_data_cursor, "surface_pressure", harp_type_float, num_profiles, surface_pressure) != 0)
    {
        free(surface_pressure.ptr);
        free(pressure_constant_a_top.ptr);
        free(pressure_constant_a_bottom.ptr);
        free(pressure_constant_b_top.ptr);
        free(pressure_constant_b_bottom.ptr);
        return -1;
    }

    if (read_dataset(info->input_data_cursor, "pressure_constant_a_top", harp_type_float, num_layers,
                     pressure_constant_a_top) != 0)
    {
        free(surface_pressure.ptr);
        free(pressure_constant_a_top.ptr);
        free(pressure_constant_a_bottom.ptr);
        free(pressure_constant_b_top.ptr);
        free(pressure_constant_b_bottom.ptr);
        return -1;
    }

    if (read_dataset(info->input_data_cursor, "pressure_constant_a_bottom", harp_type_float, num_layers,
                     pressure_constant_a_bottom) != 0)
    {
        free(surface_pressure.ptr);
        free(pressure_constant_a_top.ptr);
        free(pressure_constant_a_bottom.ptr);
        free(pressure_constant_b_top.ptr);
        free(pressure_constant_b_bottom.ptr);
        return -1;
    }

    if (read_dataset(info->input_data_cursor, "pressure_constant_b_top", harp_type_float, info->num_layers,
                     pressure_constant_b_top) != 0)
    {
        free(surface_pressure.ptr);
        free(pressure_constant_a_top.ptr);
        free(pressure_constant_a_bottom.ptr);
        free(pressure_constant_b_top.ptr);
        free(pressure_constant_b_bottom.ptr);
        return -1;
    }

    if (read_dataset(info->input_data_cursor, "pressure_constant_b_bottom", harp_type_float, num_layers,
                     pressure_constant_b_bottom) != 0)
    {
        free(surface_pressure.ptr);
        free(pressure_constant_a_top.ptr);
        free(pressure_constant_a_bottom.ptr);
        free(pressure_constant_b_top.ptr);
        free(pressure_constant_b_bottom.ptr);
        return -1;
    }

    for (i = 0; i < num_profiles; i++)
    {
        for (j = 0; j < num_layers; j++)
        {
            float *pressure_bounds = &data.float_data[i * num_layers * 2];

            pressure_bounds[j * 2] = pressure_constant_a_bottom.float_data[j] +
                pressure_constant_b_bottom.float_data[j] * data.float_data[i];
            pressure_bounds[j * 2 + 1] = pressure_constant_a_top.float_data[j] +
                pressure_constant_b_top.float_data[j] * data.float_data[i];
        }
    }

    free(surface_pressure.ptr);
    free(pressure_constant_a_top.ptr);
    free(pressure_constant_a_bottom.ptr);
    free(pressure_constant_b_top.ptr);
    free(pressure_constant_b_bottom.ptr);
    return 0;
}

static int include_so2cbr_apriori_profile(void *user_data)
{
    return ((ingest_info *)user_data)->so2_column_type == 0;
}

static void register_common_variables(harp_product_definition *product_definition, int include_sensor_variables)
{
    const char *path;
    const char *description;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };
    harp_dimension_type bounds_dimension_type[2] = { harp_dimension_time, harp_dimension_independent };
    long bounds_dimension[2] = { -1, 4 };

    /* scan_subindex */
    description = "pixel index (0-based) within the scanline";
    variable_definition =
        harp_ingestion_register_variable_block_read(product_definition, "scan_subindex", harp_type_int16, 1,
                                                    dimension_type, NULL, description, NULL, NULL, read_scan_subindex);
    description = "the scanline and pixel dimensions are collapsed into a temporal dimension; the index of the pixel "
        "within the scanline is computed as the index on the temporal " "dimension modulo the number of scanlines";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, NULL, description);

    /* datetime_start */
    description = "start time of the measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime_start", harp_type_double, 1,
                                                   dimension_type, NULL, description, "seconds since 2010-01-01", NULL,
                                                   read_datetime);
    path = "/PRODUCT/time, /PRODUCT/delta_time[]";
    description = "time converted from milliseconds since a reference time (given as seconds since 2010-01-01) to "
        "seconds since 2010-01-01 (using 86400 seconds per day); the time associated with a scanline is repeated for "
        "each pixel in the scanline";
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

    /* orbit_index */
    description = "absolute orbit number";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "orbit_index", harp_type_int32, 0, NULL, NULL,
                                                   description, NULL, NULL, read_orbit_index);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/@orbit", NULL);

    /* latitude */
    description = "latitude of the ground pixel center (WGS84)";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_float, 1, dimension_type,
                                                   NULL, description, "degree_north", NULL, read_product_latitude);
    harp_variable_definition_set_valid_range_float(variable_definition, -90.0f, 90.0f);
    path = "/PRODUCT/latitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* longitude */
    description = "longitude of the ground pixel center (WGS84)";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_float, 1, dimension_type,
                                                   NULL, description, "degree_east", NULL, read_product_longitude);
    harp_variable_definition_set_valid_range_float(variable_definition, -180.0f, 180.0f);
    path = "/PRODUCT/longitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* latitude_bounds */
    description = "latitudes of the ground pixel corners (WGS84)";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "latitude_bounds", harp_type_float, 2,
                                                   bounds_dimension_type, bounds_dimension, description, "degree_north",
                                                   NULL, read_geolocation_latitude_bounds);
    harp_variable_definition_set_valid_range_float(variable_definition, -90.0f, 90.0f);
    path = "/PRODUCT/SUPPORT_DATA/GEOLOCATIONS/latitude_bounds[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* longitude_bounds */
    description = "longitudes of the ground pixel corners (WGS84)";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "longitude_bounds", harp_type_float, 2,
                                                   bounds_dimension_type, bounds_dimension, description, "degree_east",
                                                   NULL, read_geolocation_longitude_bounds);
    harp_variable_definition_set_valid_range_float(variable_definition, -180.0f, 180.0f);
    path = "/PRODUCT/SUPPORT_DATA/GEOLOCATIONS/longitude_bounds[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    if (include_sensor_variables)
    {
        /* sensor_latitude */
        description = "latitude of the geodetic sub-satellite point (WGS84)";
        variable_definition =
            harp_ingestion_register_variable_full_read(product_definition, "sensor_latitude", harp_type_float, 1,
                                                       dimension_type, NULL, description, "degree_north", NULL,
                                                       read_geolocation_satellite_latitude);
        harp_variable_definition_set_valid_range_float(variable_definition, -90.0f, 90.0f);
        path = "/PRODUCT/SUPPORT_DATA/GEOLOCATIONS/satellite_latitude[]";
        description = "the satellite latitude associated with a scanline is repeated for each pixel in the scanline";
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

        /* sensor_longitude */
        description = "longitude of the goedetic sub-satellite point (WGS84)";
        variable_definition =
            harp_ingestion_register_variable_full_read(product_definition, "sensor_longitude", harp_type_float, 1,
                                                       dimension_type, NULL, description, "degree_east", NULL,
                                                       read_geolocation_satellite_longitude);
        harp_variable_definition_set_valid_range_float(variable_definition, -180.0f, 180.0f);
        path = "/PRODUCT/SUPPORT_DATA/GEOLOCATIONS/satellite_longitude[]";
        description = "the satellite longitude associated with a scanline is repeated for each pixel in the scanline";
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

        /* sensor_altitude */
        description = "altitude of the satellite with respect to the geodetic sub-satellite point (WGS84)";
        variable_definition =
            harp_ingestion_register_variable_full_read(product_definition, "sensor_altitude", harp_type_float, 1,
                                                       dimension_type, NULL, description, "m", NULL,
                                                       read_geolocation_satellite_altitude);
        harp_variable_definition_set_valid_range_float(variable_definition, 700000.0f, 900000.0f);
        path = "/PRODUCT/SUPPORT_DATA/GEOLOCATIONS/satellite_altitude[]";
        description = "the satellite altitude associated with a scanline is repeated for each pixel in the scanline";
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);
    }

    /* solar_zenith_angle */
    description = "zenith angle of the Sun at the ground pixel location (WGS84); angle measured away from the vertical";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "solar_zenith_angle", harp_type_float, 1,
                                                   dimension_type, NULL, description, "degree", NULL,
                                                   read_geolocation_solar_zenith_angle);
    harp_variable_definition_set_valid_range_float(variable_definition, 0.0f, 180.0f);
    path = "/PRODUCT/SUPPORT_DATA/GEOLOCATIONS/solar_zenith_angle[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* solar_azimuth_angle */
    description = "azimuth angle of the Sun at the ground pixel location (WGS84); angle measured East-of-North";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "solar_azimuth_angle", harp_type_float, 1,
                                                   dimension_type, NULL, description, "degree", NULL,
                                                   read_geolocation_solar_azimuth_angle);
    harp_variable_definition_set_valid_range_float(variable_definition, -180.0f, 180.0f);
    path = "/PRODUCT/SUPPORT_DATA/GEOLOCATIONS/solar_azimuth_angle[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* sensor_zenith_angle */
    description =
        "zenith angle of the satellite at the ground pixel location (WGS84); angle measured away from the vertical";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "sensor_zenith_angle", harp_type_float, 1,
                                                   dimension_type, NULL, description, "degree", NULL,
                                                   read_geolocation_viewing_zenith_angle);
    harp_variable_definition_set_valid_range_float(variable_definition, 0.0f, 180.0f);
    path = "/PRODUCT/SUPPORT_DATA/GEOLOCATIONS/viewing_zenith_angle[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* sensor_azimuth_angle */
    description = "azimuth angle of the satellite at the ground pixel location (WGS84); angle measured East-of-North";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "sensor_azimuth_angle", harp_type_float, 1,
                                                   dimension_type, NULL, description, "degree", NULL,
                                                   read_geolocation_viewing_azimuth_angle);
    harp_variable_definition_set_valid_range_float(variable_definition, -180.0f, 180.0f);
    path = "/PRODUCT/SUPPORT_DATA/GEOLOCATIONS/viewing_azimuth_angle[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

static void register_snow_ice_flag_variables(harp_product_definition *product_definition, int nise_extension)
{
    const char *path;
    const char *description;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };
    int (*read_snow_ice_type_function)(void *, harp_array);
    int (*read_sea_ice_fraction_function)(void *, harp_array);

    if (nise_extension)
    {
        path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/snow_ice_flag_nise[]";
        read_snow_ice_type_function = read_snow_ice_type_nise;
        read_sea_ice_fraction_function = read_sea_ice_fraction_nise;
    }
    else
    {
        path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/snow_ice_flag[]";
        read_snow_ice_type_function = read_snow_ice_type;
        read_sea_ice_fraction_function = read_sea_ice_fraction;
    }

    /* snow_ice_type */
    description = "surface snow/ice type";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "snow_ice_type", harp_type_int8, 1,
                                                   dimension_type, NULL, description, NULL, NULL,
                                                   read_snow_ice_type_function);
    harp_variable_definition_set_enumeration_values(variable_definition, 5, snow_ice_type_values);
    description = "0: snow_free_land (0), 1-100: sea_ice (1), 101: permanent_ice (2), 103: snow (3), 255: ocean (4), "
        "other values map to -1";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* sea_ice_fraction */
    description = "sea-ice concentration (as a fraction)";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "sea_ice_fraction", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_sea_ice_fraction_function);
    description = "if 1 <= snow_ice_flag <= 100 then snow_ice_flag/100.0 else 0.0";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);
}

static void register_aer_ot_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    const char *path;
    const char *description;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_spectral };

    module = harp_ingestion_register_module("S5P_PAL_L2_AER_OT", "Sentinel-5P PAL", "S5P_PAL", "L2__AER_OT",
                                            "Sentinel-5P L2 Aerosol Optical Thickness product", ingestion_init,
                                            ingestion_done);

    product_definition = harp_ingestion_register_product(module, "S5P_PAL_L2_AER_OT", NULL, read_dimensions);

    register_common_variables(product_definition, 1);

    /* cloud_fraction */
    description = "Geometrical cloud fraction from NPP-VIIRS regridded observations. "
        "Geometrical cloud fraction is defined as (probably+confidently cloudy)/(total) for nominal footprint.";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_fraction", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_input_cloud_fraction);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_fraction";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* surface_pressure */
    description = "surface air pressure";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "surface_pressure", harp_type_float, 1,
                                                   dimension_type, NULL, description, "Pa", NULL,
                                                   read_input_surface_pressure);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/surface_pressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    register_snow_ice_flag_variables(product_definition, 0);

    /* absorbing_aerosol_index */
    description = "Absorbing aerosol index at 340 and 380 nm.";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "absorbing_aerosol_index", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_input_absorbing_aerosol_index);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/absorbing_aerosol_index[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* wind_speed */
    description = "absolute wind speed computed from the wind vector at 10 meter height level";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "wind_speed", harp_type_float, 1, dimension_type,
                                                   NULL, description, "m/s", NULL, read_input_wind_speed);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/wind_speed[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* aerosol_optical_depth */
    description = "total aerosol optical thickness of the atmospheric column";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "aerosol_optical_depth", harp_type_float, 2,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_product_aerosol_optical_thickness);
    path = "/PRODUCT/aerosol_optical_thickness";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* aerosol_optical_depth_validity */
    description = "continuous quality descriptor, varying between 0 (no data) and 100 (full quality data)";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "aerosol_optical_depth_validity",
                                                   harp_type_int8, 2, dimension_type, NULL, description, NULL, NULL,
                                                   read_aot_qa_value);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/PRODUCT/qa_value", NULL);

    /* single_scattering_albedo */
    description = "Single scattering albedo; fraction of the aerosol scattering and absorption, according to the "
        "selected aerosol type.";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "single_scattering_albedo", harp_type_float, 2,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_product_single_scattering_albedo);
    path = "/PRODUCT/single_scattering_albedo";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* aerosol_type */
    description = "selected aerosol type";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "aerosol_type", harp_type_int32, 1,
                                                   dimension_type, NULL, description, NULL, NULL,
                                                   read_product_aerosol_type);
    path = "/PRODUCT/aerosol_type";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

static void register_bro_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    const char *path;
    const char *description;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };

    module = harp_ingestion_register_module("S5P_PAL_L2_BRO", "Sentinel-5P PAL", "S5P_PAL", "L2__BRO___",
                                            "Sentinel-5P L2 BrO product", ingestion_init, ingestion_done);

    product_definition = harp_ingestion_register_product(module, "S5P_PAL_L2_BRO", NULL, read_dimensions);

    register_common_variables(product_definition, 1);

    /* cloud_fraction */
    description = "cloud fraction";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_fraction", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_input_cloud_fraction_crb);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_fraction_crb";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_fraction_uncertainty */
    description = "uncertainty of the cloud fraction";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_fraction_uncertainty", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_input_cloud_fraction_crb_precision);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_fraction_crb_precision";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_pressure */
    description = "cloud pressure";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_pressure", harp_type_float, 1,
                                                   dimension_type, NULL, description, "Pa", NULL,
                                                   read_input_cloud_pressure_crb);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_pressure_crb[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_pressure_uncertainty */
    description = "cloud pressure uncertainty";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_pressure_uncertainty", harp_type_float, 1,
                                                   dimension_type, NULL, description, "Pa", NULL,
                                                   read_input_cloud_pressure_crb_precision);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_pressure_crb_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_height */
    description = "cloud height";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_height", harp_type_float, 1,
                                                   dimension_type, NULL, description, "m", NULL,
                                                   read_input_cloud_height_crb);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_height_crb[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_height_uncertainty */
    description = "cloud height uncertainty";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_height_uncertainty", harp_type_float, 1,
                                                   dimension_type, NULL, description, "m", NULL,
                                                   read_input_cloud_height_crb_precision);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_height_crb_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_albedo */
    description = "cloud albedo";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_albedo", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_input_cloud_albedo_crb);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_albedo_crb";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_albedo_uncertainty */
    description = "cloud albedo uncertainty";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_albedo_uncertainty", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_input_cloud_albedo_crb_precision);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_albedo_crb_precision";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* surface_altitude */
    description = "surface altitude";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "surface_altitude", harp_type_float, 1,
                                                   dimension_type, NULL, description, "m", NULL,
                                                   read_input_surface_altitude);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/surface_altitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* surface_altitude_uncertainty */
    description = "the standard deviation of sub-pixels used in calculating the mean surface altitude";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "surface_altitude_uncertainty", harp_type_float,
                                                   1, dimension_type, NULL, description, "m", NULL,
                                                   read_input_surface_altitude_precision);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/surface_altitude_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* surface_pressure */
    description = "surface air pressure";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "surface_pressure", harp_type_float, 1,
                                                   dimension_type, NULL, description, "Pa", NULL,
                                                   read_input_surface_pressure);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/surface_pressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* surface_temperature */
    description = "surface temperature";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "surface_temperature", harp_type_float, 1,
                                                   dimension_type, NULL, description, "K", NULL,
                                                   read_input_surface_temperature);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/surface_temperature[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* surface_meridional_wind_velocity */
    description = "Northward wind from ECMWF at 10 meter height level";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "surface_meridional_wind_velocity",
                                                   harp_type_float, 1, dimension_type, NULL, description, "m/s", NULL,
                                                   read_input_northward_wind);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/northward_wind[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* surface_zonal_wind_velocity */
    description = "Eastward wind from ECMWF at 10 meter height level";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "surface_zonal_wind_velocity", harp_type_float,
                                                   1, dimension_type, NULL, description, "m/s", NULL,
                                                   read_input_eastward_wind);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/eastward_wind[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    register_snow_ice_flag_variables(product_definition, 1);

    /* BrO_column_number_density */
    description = "vertical column of bromine monoxide";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "BrO_column_number_density", harp_type_double, 1,
                                                   dimension_type, NULL, description, "mol/m^2", NULL,
                                                   read_product_brominemonoxide_total_vertical_column);
    path = "/PRODUCT/brominemonoxide_total_vertical_column";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* BrO_column_number_density_uncertainty_random */
    description = "random error of vertical column density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "BrO_column_number_density_uncertainty_random",
                                                   harp_type_double, 1, dimension_type, NULL, description, "mol/m^2",
                                                   NULL, read_product_brominemonoxide_total_vertical_column_precision);
    path = "/PRODUCT/brominemonoxide_total_vertical_column_precision";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* BrO_column_number_density_uncertainty_systematic */
    description = "systematic error of vertical column density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "BrO_column_number_density_uncertainty_systematic", harp_type_double,
                                                   1, dimension_type, NULL, description, "mol/m^2", NULL,
                                                   read_results_brominemonoxide_total_vertical_column_trueness);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* BrO_column_number_density_validity */
    description = "continuous quality descriptor, varying between 0 (no data) and 100 (full quality data)";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "BrO_column_number_density_validity",
                                                   harp_type_int8, 1, dimension_type, NULL, description, NULL, NULL,
                                                   read_product_qa_value);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/PRODUCT/qa_value", NULL);

    /* BrO_column_number_density_amf */
    description = "geometric air mass factor";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "BrO_column_number_density_amf",
                                                   harp_type_double, 1, dimension_type, NULL, description, "mol/m^2",
                                                   NULL, read_results_brominemonoxide_geometric_air_mass_factor);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/brominemonoxide_geometric_air_mass_factor[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

static void register_chocho_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    const char *path;
    const char *description;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };

    module = harp_ingestion_register_module("S5P_PAL_L2_CHOCHO", "Sentinel-5P PAL", "S5P_PAL", "L2__CHOCHO",
                                            "Sentinel-5P L2 Glyoxal (CHOCHO) product", ingestion_init, ingestion_done);

    product_definition = harp_ingestion_register_product(module, "S5P_PAL_L2_CHOCHO", NULL, read_dimensions);

    register_common_variables(product_definition, 0);

    /* cloud_fraction */
    description = "Retrieved effective radiometric cloud fraction derived in NO2 fitting window";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_fraction", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_input_cloud_fraction_crb);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_fraction_crb";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_pressure */
    description = "cloud pressure";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_pressure", harp_type_float, 1,
                                                   dimension_type, NULL, description, "Pa", NULL,
                                                   read_input_cloud_pressure_crb);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_pressure_crb[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* surface_altitude */
    description = "surface altitude";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "surface_altitude", harp_type_float, 1,
                                                   dimension_type, NULL, description, "m", NULL,
                                                   read_input_surface_altitude);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/surface_altitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* surface_pressure */
    description = "surface air pressure";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "surface_pressure", harp_type_float, 1,
                                                   dimension_type, NULL, description, "Pa", NULL,
                                                   read_input_surface_pressure);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/surface_pressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    register_snow_ice_flag_variables(product_definition, 0);

    /* absorbing_aerosol_index */
    description = "Aerosol index from 388 and 354 nm";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "absorbing_aerosol_index", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_input_aerosol_index_354_388);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/aerosol_index_354_388[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* surface_albedo */
    description = "surface albedo";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "surface_albedo", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_input_surface_albedo);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/surface_albedo[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* C2H2O2_column_number_density */
    description = "vertical column of glyoxal";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "C2H2O2_column_number_density", harp_type_float,
                                                   1, dimension_type, NULL, description, NULL, NULL,
                                                   read_product_glyoxal_tropospheric_vertical_column);
    path = "/PRODUCT/glyoxal_tropospheric_vertical_column";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* C2H2O2_column_number_density_uncertainty */
    description = "random error of vertical column density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "C2H2O2_column_number_density_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description, NULL, NULL,
                                                   read_product_glyoxal_tropospheric_vertical_column_precision);
    path = "/PRODUCT/glyoxal_tropospheric_vertical_column_precision";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* C2H2O2_column_number_density_validity */
    description = "continuous quality descriptor, varying between 0 (no data) and 100 (full quality data)";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "C2H2O2_column_number_density_validity",
                                                   harp_type_int8, 1, dimension_type, NULL, description, NULL, NULL,
                                                   read_product_qa_value);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/PRODUCT/qa_value", NULL);
}

static void register_sif_product(void)
{
    const char *sif_options[] = { "735" };
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    const char *path;
    const char *description;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };

    module = harp_ingestion_register_module("S5P_PAL_L2_SIF", "Sentinel-5P PAL", "S5P_PAL", "L2__SIF___",
                                            "Sentinel-5P L2 Solar Induced Fluorescence product", ingestion_init,
                                            ingestion_done);

    harp_ingestion_register_option(module, "sif", "whether to ingest the SIF retrieved at 743nm (default) or at "
                                   "735nm (sif=735)", 1, sif_options);
    product_definition = harp_ingestion_register_product(module, "S5P_PAL_L2_SIF", NULL, read_dimensions);

    register_common_variables(product_definition, 1);

    /* cloud_fraction */
    description = "Coregistered effective radiometric cloud fraction using the OCRA/ROCINN CAL model.";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_fraction", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_input_cloud_fraction_l2);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_fraction_L2[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* solar_induced_fluorescence */
    description = "retrieved SIF";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "solar_induced_fluorescence", harp_type_float, 1,
                                                   dimension_type, NULL, description, "mW/m2/sr/nm", NULL, read_sif);
    path = "/PRODUCT/SIF_743";
    harp_variable_definition_add_mapping(variable_definition, "sif unset", NULL, path, NULL);
    path = "/PRODUCT/SIF_735";
    harp_variable_definition_add_mapping(variable_definition, "sif=735", NULL, path, NULL);

    /* solar_induced_fluorescence_uncertainty */
    description = "retrieved SIF";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "solar_induced_fluorescence_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description, "mW/m2/sr/nm",
                                                   NULL, read_sif_error);
    path = "/PRODUCT/SIF_ERROR_743";
    harp_variable_definition_add_mapping(variable_definition, "sif unset", NULL, path, NULL);
    path = "/PRODUCT/SIF_ERROR_735";
    harp_variable_definition_add_mapping(variable_definition, "sif=735", NULL, path, NULL);

    /* solar_induced_fluorescence_validity */
    description = "continuous quality descriptor, varying between 0 (no data) and 100 (full quality data)";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "solar_induced_fluorescence_validity",
                                                   harp_type_int8, 1, dimension_type, NULL, description, NULL, NULL,
                                                   read_sif_qa_value);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/QA_value_743";
    harp_variable_definition_add_mapping(variable_definition, "sif unset", NULL, path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/QA_value_735";
    harp_variable_definition_add_mapping(variable_definition, "sif=735", NULL, path, NULL);
}

static void register_so2cbr_product(void)
{
    const char *so2cbr_column_options[] = { "1km", "7km", "15km" };
    const char *cloud_fraction_options[] = { "radiance" };
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    const char *path;
    const char *description;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_vertical };
    const char *so2cbr_type_values[] = {
        "no_detection", "so2_detected", "volcanic_detection",
        "detection_near_anthropogenic_source", "detection_at_high_sza"
    };

    module = harp_ingestion_register_module("S5P_PAL_L2_SO2CBR", "Sentinel-5P PAL", "S5P_PAL", "L2__SO2CBR",
                                            "Sentinel-5P L2 SO2 COBRA product", ingestion_init, ingestion_done);

    harp_ingestion_register_option(module, "so2_column", "whether to ingest the anothropogenic SO2 column at the PBL "
                                   "(default), the SO2 column from the 1km box profile (so2_column=1km), from the 7km "
                                   "box profile (so2_column=7km), or from the 15km box profile (so2_column=15km)", 3,
                                   so2cbr_column_options);

    harp_ingestion_register_option(module, "cloud_fraction", "whether to ingest the cloud fraction (default) or the "
                                   "radiance cloud fraction (cloud_fraction=radiance)", 1, cloud_fraction_options);

    product_definition = harp_ingestion_register_product(module, "S5P_PAL_L2_SO2CBR", NULL, read_dimensions);

    register_common_variables(product_definition, 1);

    /* pressure */
    description = "pressure";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "pressure", harp_type_double, 2, dimension_type,
                                                   NULL, description, "Pa", NULL, read_input_tm5_pressure);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/tm5_constant_a[], /PRODUCT/SUPPORT_DATA/INPUT_DATA/tm5_constant_b[], "
        "/PRODUCT/SUPPORT_DATA/INPUT_DATA/surface_pressure[]";
    description = "pressure in Pa at layer k is derived from surface pressure in Pa as: tm5_constant_a[k] + "
        "tm5_constant_b[k] * surface_pressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* cloud_fraction */
    description = "cloud fraction";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_fraction", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_so2cbr_cloud_fraction);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_fraction_crb[]";
    harp_variable_definition_add_mapping(variable_definition, "cloud_fraction unset", NULL, path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/cloud_fraction_intensity_weighted[]";
    harp_variable_definition_add_mapping(variable_definition, "cloud_fraction=radiance", NULL, path, NULL);

    /* cloud_fraction_precission */
    description = "uncertainty of the cloud fraction";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_fraction_uncertainty", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_so2cbr_cloud_fraction_precision);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_fraction_crb_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "cloud_fraction unset", NULL, path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/cloud_fraction_intensity_weighted_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "cloud_fraction=radiance", NULL, path, NULL);

    /* cloud_pressure */
    description = "cloud pressure";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_pressure", harp_type_float, 1,
                                                   dimension_type, NULL, description, "Pa", NULL,
                                                   read_input_cloud_pressure_crb);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_pressure_crb[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_pressure_uncertainty */
    description = "cloud pressure uncertainty";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_pressure_uncertainty", harp_type_float, 1,
                                                   dimension_type, NULL, description, "Pa", NULL,
                                                   read_input_cloud_pressure_crb_precision);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_pressure_crb_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_height */
    description = "cloud height";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_height", harp_type_float, 1,
                                                   dimension_type, NULL, description, "m", NULL,
                                                   read_input_cloud_height_crb);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_height_crb[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_height_uncertainty */
    description = "cloud height uncertainty";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_height_uncertainty", harp_type_float, 1,
                                                   dimension_type, NULL, description, "m", NULL,
                                                   read_input_cloud_height_crb_precision);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_height_crb_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_albedo */
    description = "cloud albedo";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_albedo", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_input_cloud_albedo_crb);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_albedo_crb";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_albedo_uncertainty */
    description = "cloud albedo uncertainty";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_albedo_uncertainty", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_input_cloud_albedo_crb_precision);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_albedo_crb_precision";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* surface_altitude */
    description = "mean surface altitude";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "surface_altitude", harp_type_float, 1,
                                                   dimension_type, NULL, description, "m", NULL,
                                                   read_input_surface_altitude);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/surface_altitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* surface_altitude_uncertainty */
    description = "the standard deviation of sub-pixels used in calculating the mean surface altitude";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "surface_altitude_uncertainty", harp_type_float,
                                                   1, dimension_type, NULL, description, "m", NULL,
                                                   read_input_surface_altitude_precision);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/surface_altitude_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* surface_pressure */
    description = "surface air pressure";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "surface_pressure", harp_type_float, 1,
                                                   dimension_type, NULL, description, "Pa", NULL,
                                                   read_input_surface_pressure);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/surface_pressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* surface_meridional_wind_velocity */
    description = "Northward wind from ECMWF at 10 meter height level";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "surface_meridional_wind_velocity",
                                                   harp_type_float, 1, dimension_type, NULL, description, "m/s", NULL,
                                                   read_input_northward_wind);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/northward_wind[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* surface_zonal_wind_velocity */
    description = "Eastward wind from ECMWF at 10 meter height level";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "surface_zonal_wind_velocity", harp_type_float,
                                                   1, dimension_type, NULL, description, "m/s", NULL,
                                                   read_input_eastward_wind);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/eastward_wind[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* absorbing_aerosol_index */
    description = "Aerosol index from 380 and 340 nm";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "absorbing_aerosol_index", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_input_aerosol_index_340_380);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/aerosol_index_340_380";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* surface_albedo */
    description = "surface albedo";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "surface_albedo", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_so2cbr_surface_albedo);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/surface_albedo_328nm, "
        "/PRODUCT/SUPPORT_DATA/INPUT_DATA/surface_albedo_376nm, "
        "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/selected_fitting_window_flag";
    description = "if selected_fitting_window_flag is 1 or 2 then use surface_albedo_328, if "
        "selected_fitting_window_flag is 3 then use surface_albedo_376, else set to NaN";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* O3_column_number_density */
    description = "total ozone column";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_column_number_density", harp_type_float, 1,
                                                   dimension_type, NULL, description, "mol/m^2", NULL,
                                                   read_input_ozone_total_vertical_column);

    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/ozone_total_vertical_column[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* O3_column_number_density_uncertainty */
    description = "total ozone column random error";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_column_number_density_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description, "mol/m^2",
                                                   NULL, read_input_ozone_total_vertical_column_precision);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/ozone_total_vertical_column_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* tropopause_pressure */
    description = "tropopause pressure";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "tropopause_pressure",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, "Pa", NULL,
                                                                     read_so2cbr_tropopause_pressure);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/tm5_constant_a[], /PRODUCT/SUPPORT_DATA/INPUT_DATA/tm5_constant_b[], "
        "/PRODUCT/SUPPORT_DATA/INPUT_DATA/tm5_tropopause_layer_index[], "
        "/PRODUCT/SUPPORT_DATA/INPUT_DATA/surface_pressure[]";
    description = "pressure in Pa at tropause is derived from the upper bound of the layer with tropopause layer index "
        "k: exp((log(tm5_constant_a[k] + tm5_constant_b[k] * surface_pressure[]) + "
        "log(tm5_constant_a[k + 1] + tm5_constant_b[k + 1] * surface_pressure[]))/2.0)";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* SO2_column_number_density */
    description = "total vertical column of sulfur dioxide";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "SO2_column_number_density", harp_type_float, 1,
                                                   dimension_type, NULL, description, "mol/m^2", NULL,
                                                   read_so2cbr_sulfurdioxide_total_vertical_column);
    path = "/PRODUCT/sulfurdioxide_total_vertical_column[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column unset", NULL, path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfurdioxide_total_vertical_column_1km[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column=1km", NULL, path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfurdioxide_total_vertical_column_7km[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column=7km", NULL, path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfurdioxide_total_vertical_column_15km[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column=15km", NULL, path, NULL);

    /* SO2_column_number_density_uncertainty_random */
    description = "precision of the total vertical column of sulfur dioxide";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "SO2_column_number_density_uncertainty_random",
                                                   harp_type_float, 1, dimension_type, NULL, description, "mol/m^2",
                                                   NULL, read_so2cbr_sulfurdioxide_total_vertical_column_precision);

    path = "/PRODUCT/sulfurdioxide_total_vertical_column_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column unset", NULL, path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfurdioxide_total_vertical_column_1km_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column=1km", NULL, path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfurdioxide_total_vertical_column_7km_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column=7km", NULL, path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfurdioxide_total_vertical_column_15km_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column=15km", NULL, path, NULL);

    /* SO2_column_number_density_uncertainty_systematic */
    description = "systematic error of the total vertical column density of sulfur dioxide";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "SO2_column_number_density_uncertainty_systematic",
                                                   harp_type_float, 1, dimension_type, NULL, description, "mol/m^2",
                                                   NULL, read_so2cbr_sulfurdioxide_total_vertical_column_trueness);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfurdioxide_total_vertical_column_trueness[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column unset", NULL, path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfurdioxide_total_vertical_column_1km_trueness[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column=1km", NULL, path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfurdioxide_total_vertical_column_7km_trueness[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column=7km", NULL, path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfurdioxide_total_vertical_column_15km_trueness[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column=15km", NULL, path, NULL);

    /* SO2_column_number_density_validity */
    description = "continuous quality descriptor, varying between 0 (no data) and 100 (full quality data)";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "SO2_column_number_density_validity",
                                                   harp_type_int8, 1, dimension_type, NULL, description, NULL, NULL,
                                                   read_product_qa_value);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/PRODUCT/qa_value", NULL);

    /* SO2_column_number_density_amf */
    description = "total air mass factor";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "SO2_column_number_density_amf", harp_type_float,
                                                   1, dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_so2cbr_sulfurdioxide_total_air_mass_factor);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfurdioxide_total_air_mass_factor_polluted[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column unset", NULL, path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfurdioxide_total_air_mass_factor_1km[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column=1km", NULL, path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfurdioxide_total_air_mass_factor_7km[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column=7km", NULL, path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfurdioxide_total_air_mass_factor_15km[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column=15km", NULL, path, NULL);

    /* SO2_column_number_density_amf_uncertainty_random */
    description = "random error of the total air mass factor";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "SO2_column_number_density_amf_uncertainty_random", harp_type_float,
                                                   1, dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_so2cbr_sulfurdioxide_total_air_mass_factor_precision);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfurdioxide_total_air_mass_factor_polluted_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column unset", NULL, path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfurdioxide_total_air_mass_factor_1km_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column=1km", NULL, path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfurdioxide_total_air_mass_factor_7km_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column=7km", NULL, path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfurdioxide_total_air_mass_factor_15km_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column=15km", NULL, path, NULL);

    /* SO2_column_number_density_amf_uncertainty_systematic */
    description = "systematic error of the total air mass factor";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "SO2_column_number_density_amf_uncertainty_systematic",
                                                   harp_type_float, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_so2cbr_sulfurdioxide_total_air_mass_factor_trueness);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfurdioxide_total_air_mass_factor_polluted_trueness[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column unset", NULL, path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfurdioxide_total_air_mass_factor_1km_trueness[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column=1km", NULL, path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfurdioxide_total_air_mass_factor_7km_trueness[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column=7km", NULL, path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfurdioxide_total_air_mass_factor_15km_trueness[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column=15km", NULL, path, NULL);

    /* SO2_column_number_density_avk */
    description = "averaging kernel";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "SO2_column_number_density_avk", harp_type_float,
                                                   2, dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_so2cbr_averaging_kernel);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/averaging_kernel[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/averaging_kernel[], "
        "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfurdioxide_averaging_kernel_scaling_box_1km[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column=1km", NULL, path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/averaging_kernel[], "
        "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfurdioxide_averaging_kernel_scaling_box_7km[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column=7km", NULL, path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/averaging_kernel[], "
        "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfurdioxide_averaging_kernel_scaling_box_15km[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column=15km", NULL, path, NULL);

    /* SO2_volume_mixing_ratio_dry_air_apriori */
    description = "volume mixing ratio profile of sulfur dioxide";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "SO2_volume_mixing_ratio_dry_air_apriori",
                                                   harp_type_float, 2, dimension_type, NULL, description, "ppv",
                                                   include_so2cbr_apriori_profile,
                                                   read_so2cbr_sulfurdioxide_profile_apriori);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfurdioxide_profile_apriori[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column unset", NULL, path, NULL);

    /* SO2_slant_column_number_density */
    description = "background corrected sulfur dioxide slant column density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "SO2_slant_column_number_density",
                                                   harp_type_float, 1, dimension_type, NULL, description, "mol/m^2",
                                                   NULL, read_so2cbr_sulfurdioxide_slant_column_corrected);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfurdioxide_slant_column_corrected[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* SO2_type */
    description = "sulfur dioxide volcano activity flag";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "SO2_type", harp_type_int8, 1, dimension_type,
                                                   NULL, description, NULL, NULL,
                                                   read_so2cbr_sulfurdioxide_detection_flag);
    harp_variable_definition_set_enumeration_values(variable_definition, 5, so2cbr_type_values);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfurdioxide_detection_flag[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

static void register_tcwv_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    const char *path;
    const char *description;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[3] =
        { harp_dimension_time, harp_dimension_vertical, harp_dimension_independent };
    long dimension[3] = { -1, -1, 2 };

    module = harp_ingestion_register_module("S5P_PAL_L2_TCWV", "Sentinel-5P PAL", "S5P_PAL", "L2__TCWV__",
                                            "Sentinel-5P L2 Total Column Water Vapor product", ingestion_init,
                                            ingestion_done);

    product_definition = harp_ingestion_register_product(module, "S5P_PAL_L2_TCWV", NULL, read_dimensions);

    register_common_variables(product_definition, 1);

    /* pressure_bounds */
    description = "pressure_bounds";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "pressure_bounds", harp_type_float, 3,
                                                   dimension_type, dimension, description, "Pa", NULL,
                                                   read_tcwv_pressure_bounds);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/pressure_constant_a_top[], "
        "/PRODUCT/SUPPORT_DATA/INPUT_DATA/pressure_constant_a_bottom[], "
        "/PRODUCT/SUPPORT_DATA/INPUT_DATA/pressure_constant_b_top[], "
        "/PRODUCT/SUPPORT_DATA/INPUT_DATA/pressure_constant_b_bottom[], "
        "/PRODUCT/SUPPORT_DATA/INPUT_DATA/surface_pressure[]";
    description = "pressure in Pa at layer k is derived from surface pressure in Pa as: pressure_constant_a_top[k] + "
        "pressure_constant_b_top[k] * surface_pressure[] for the top and pressure_constant_a_bottom[k] + "
        "pressure_constant_b_bottom[k] * surface_pressure[] for the bottom of the layer";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* cloud_fraction */
    description = "Retrieved effective radiometric cloud fraction using the OCRA/ROCINN CRB model";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_fraction", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_input_cloud_fraction);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_fraction[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_pressure */
    description = "Retrieved atmospheric pressure at the level of cloud using the OCRA/ROCINN CRB model";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_pressure", harp_type_float, 1,
                                                   dimension_type, NULL, description, "Pa", NULL,
                                                   read_input_cloud_pressure);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_pressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_albedo */
    description = "cloud albedo";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_albedo", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_input_cloud_albedo);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_albedo";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* surface_pressure */
    description = "surface air pressure";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "surface_pressure", harp_type_float, 1,
                                                   dimension_type, NULL, description, "Pa", NULL,
                                                   read_input_surface_pressure);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/surface_pressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* surface_albedo */
    description = "surface albedo";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "surface_albedo", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_input_surface_albedo);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/surface_albedo[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* water_vapor_column_number_density */
    description = "total vertical column of water vapor";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "water_vapor_column_number_density",
                                                   harp_type_float, 1, dimension_type, NULL, description, "mol/m^2",
                                                   NULL, read_product_total_column_water_vapor);
    path = "/PRODUCT/total_column_water_vapor";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* water_vapor_column_number_density_uncertainty */
    description = "precision of the total vertical column of water vapor";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "water_vapor_column_number_density_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description, "mol/m^2",
                                                   NULL, read_product_total_column_water_vapor_precision);
    path = "/PRODUCT/total_column_water_vapor_precision";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* water_vapor_column_number_density_validity */
    description = "continuous quality descriptor, varying between 0 (no data) and 100 (full quality data)";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "water_vapor_column_number_density_validity",
                                                   harp_type_int8, 1, dimension_type, NULL, description, NULL, NULL,
                                                   read_product_qa_value);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/PRODUCT/qa_value", NULL);

    /* water_vapor_column_number_density_amf */
    description = "water vapor total column air mass factor";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "water_vapor_column_number_density_amf",
                                                   harp_type_float, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL, read_results_air_mass_factor_total);

    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/air_mass_factor_total[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* water_vapor_column_number_density_avk */
    description = "total column averaging kernel";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "water_vapor_column_number_density_avk",
                                                   harp_type_float, 2, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL, read_results_averaging_kernel);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/averaging_kernel[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* water_vapor_mass_mixing_ratio_apriori */
    description = "a-priori mass mixing ratio profile of water vapor";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "water_vapor_mass_mixing_ratio_apriori",
                                                   harp_type_float, 2, dimension_type, NULL, description, "kg/kg", NULL,
                                                   read_results_water_vapor_profile_apriori);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/water_vapour_profile_apriori[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

int harp_ingestion_module_pal_s5p_l2_init(void)
{
    register_aer_ot_product();
    register_bro_product();
    register_chocho_product();
    register_sif_product();
    register_so2cbr_product();
    register_tcwv_product();

    return 0;
}
