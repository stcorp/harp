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
#include "harp-ingestion.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>


typedef enum s4_product_type_enum
{
    s4_type_alh,
    s4_type_aui,
    s4_type_cld,
    s4_type_fdy,
    s4_type_gly,
    s4_type_no2,
    s4_type_o3,
    s4_type_o3_tsc,
    s4_type_so2,
} s4_product_type;

#define S4_NUM_PRODUCT_TYPES (((int)s4_type_so2) + 1)

static const char *snow_ice_type_values[2] = { "snow_free_land", "snow_ice" };

typedef enum s4_wavelength_ratio_enum
{
    s4_340_380nm,
    s4_354_388nm,
} s4_wavelength_ratio;


typedef struct ingest_info_struct
{
    coda_product *product;
    s4_wavelength_ratio wavelength_ratio;
    int use_alh_surface_albedo_770;
    int use_so2_surface_albedo_376;
    int use_nir;
    int use_summed_total_column;
    /* so2 = 0: PBL (anthropogenic), 1: 1km box profile, 2: 7km bp, 3: 15km bpl/polluted */
    int so2_column_type;
    /* cld_model = 0: CAL (default), 1: CRB, 2: CMA+OCA */
    int cld_model;

    s4_product_type product_type;
    long num_scanlines;
    long num_pixels;
    long num_layers;

    coda_cursor product_cursor;
    coda_cursor geolocation_cursor;
    coda_cursor detailed_results_cursor;
    coda_cursor input_data_cursor;
} ingest_info;


static const char *get_product_type_name(s4_product_type product_type)
{
    switch (product_type)
    {
        case s4_type_alh:
            return "UVN-2-ALH";
        case s4_type_aui:
            return "UVN-2-AUI";
        case s4_type_cld:
            return "UVN-2-CLD";
        case s4_type_fdy:
            return "UVN-2-FDY";
        case s4_type_gly:
            return "UVN-2-GLY";
        case s4_type_no2:
            return "UVN-2-NO2";
        case s4_type_o3:
            return "UVN-2-O3";
        case s4_type_o3_tsc:
            return "UVN-2-O3-TSC";
        case s4_type_so2:
            return "UVN-2-SO2";
    }

    assert(0);
    exit(1);
}

static int get_product_type(coda_product *product, s4_product_type *product_type)
{
    const char *coda_product_type;
    int i;

    if (coda_get_product_type(product, &coda_product_type) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    for (i = 0; i < S4_NUM_PRODUCT_TYPES; i++)
    {
        if (strcmp(get_product_type_name((s4_product_type)i), coda_product_type) == 0)
        {
            *product_type = ((s4_product_type)i);
            return 0;
        }
    }

    harp_set_error(HARP_ERROR_INGESTION, "unsupported product type '%s'", coda_product_type);
    return -1;
}

static int get_dimension_length(ingest_info *info, const char *path, long *length)
{
    coda_cursor cursor = info->product_cursor;
    long coda_dim[CODA_MAX_NUM_DIMS];
    int num_dims;

    if (coda_cursor_goto(&cursor, path) != 0)
    {
        *length = 0;
        return 0;
    }

    if (coda_cursor_get_array_dim(&cursor, &num_dims, coda_dim) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    if (num_dims != 1)
    {
        harp_set_error(HARP_ERROR_INGESTION, "variable '/PRODUCT/%s' is not a 1D array", path);
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
    if (info->use_nir)
    {
        if (coda_cursor_goto_record_field_by_name(&cursor, "PRODUCT_NIR") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
    }
    else
    {
        if (coda_cursor_goto_record_field_by_name(&cursor, "PRODUCT") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
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
    if (get_dimension_length(info, "scanline", &info->num_scanlines) != 0)
    {
        return -1;
    }
    if (get_dimension_length(info, "ground_pixel", &info->num_pixels) != 0)
    {
        return -1;
    }
    if (info->product_type == s4_type_o3_tsc)
    {
        if (get_dimension_length(info, "subcolumn", &info->num_layers) != 0)
        {
            return -1;
        }
    }
    else
    {
        info->num_layers = 0;
    }

    return 0;
}

static void ingestion_done(void *user_data)
{
    ingest_info *info = (ingest_info *)user_data;

    free(info);
}

static int ingestion_init(const harp_ingestion_module *module, coda_product *product,
                          const harp_ingestion_options *options, harp_product_definition **definition, void **user_data)
{
    const char *option_value;
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
    info->wavelength_ratio = s4_354_388nm;
    info->use_alh_surface_albedo_770 = 0;
    info->use_so2_surface_albedo_376 = 0;
    info->use_nir = 0;
    info->use_summed_total_column = 0;
    info->so2_column_type = 0;
    info->cld_model = 0;
    info->num_scanlines = 0;
    info->num_pixels = 0;
    info->num_layers = 0;

    if (get_product_type(info->product, &info->product_type) != 0)
    {
        ingestion_done(info);
        return -1;
    }

    if (harp_ingestion_options_has_option(options, "surface_albedo"))
    {
        info->use_alh_surface_albedo_770 = 1;
        info->use_so2_surface_albedo_376 = 1;
    }
    if (harp_ingestion_options_has_option(options, "wavelength_ratio"))
    {
        if (harp_ingestion_options_get_option(options, "wavelength_ratio", &option_value) != 0)
        {
            ingestion_done(info);
            return -1;
        }
        if (strcmp(option_value, "354_388nm") == 0)
        {
            info->wavelength_ratio = s4_354_388nm;
        }
        else
        {
            assert(strcmp(option_value, "340_380nm") == 0);
            info->wavelength_ratio = s4_340_380nm;
        }
    }
    if (harp_ingestion_options_has_option(options, "band"))
    {
        info->use_nir = 1;
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
        else
        {
            assert(strcmp(option_value, "15km") == 0);
            info->so2_column_type = 3;
        }
    }
    if (harp_ingestion_options_has_option(options, "model"))
    {
        if (harp_ingestion_options_get_option(options, "model", &option_value) != 0)
        {
            ingestion_done(info);
            return -1;
        }
        if (strcmp(option_value, "CRB") == 0)
        {
            info->cld_model = 1;
        }
        else
        {
            assert(strcmp(option_value, "CMA+OCA") == 0);
            info->cld_model = 2;
        }
    }

    *definition = *module->product_definition;

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

    *user_data = info;

    return 0;
}

static int read_dimensions(void *user_data, long dimension[HARP_NUM_DIM_TYPES])
{
    ingest_info *info = (ingest_info *)user_data;

    dimension[harp_dimension_time] = info->num_scanlines * info->num_pixels;

    if (info->num_layers > 0)
    {
        dimension[harp_dimension_vertical] = info->num_layers;
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

static int read_datetime(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    coda_cursor cursor = info->product_cursor;
    double time_reference;
    long i;

    if (coda_cursor_goto(&cursor, "/@time_reference_days_since_1950[0]") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_double(&cursor, &time_reference) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    /* convert time reference from days since 1950-01-01 to seconds since 2000-01-01 */
    time_reference = (time_reference - 18262) * 24 * 60 * 60;

    if (read_dataset(info->product_cursor, "delta_time", harp_type_double, info->num_scanlines * info->num_pixels,
                     data) != 0)
    {
        return -1;
    }

    for (i = 0; i < info->num_scanlines * info->num_pixels; i++)
    {
        data.double_data[i] = data.double_data[i] + time_reference;
    }

    return 0;
}

static int read_datetime_length(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    coda_cursor cursor = info->product_cursor;
    double first, second;

    if (coda_cursor_goto_record_field_by_name(&cursor, "delta_time") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_first_array_element(&cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_double(&cursor, &first) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    coda_cursor_goto_parent(&cursor);
    if (coda_cursor_goto_array_element_by_index(&cursor, info->num_pixels) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_double(&cursor, &second) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    *data.double_data = second - first;

    return 0;
}

static int read_latitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "latitude", harp_type_float, info->num_scanlines * info->num_pixels,
                        data);
}

static int read_latitude_bounds(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->geolocation_cursor, "latitude_bounds", harp_type_float,
                        info->num_scanlines * info->num_pixels * 4, data);
}

static int read_longitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "longitude", harp_type_float, info->num_scanlines * info->num_pixels,
                        data);
}

static int read_longitude_bounds(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->geolocation_cursor, "longitude_bounds", harp_type_float,
                        info->num_scanlines * info->num_pixels * 4, data);
}

static int read_qa_value(void *user_data, harp_array data)
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

    if (info->cld_model == 2)
    {
        return read_dataset(info->input_data_cursor, "cma_cloud_fraction_psf", harp_type_float,
                            info->num_scanlines * info->num_pixels, data);
    }
    /* CAL and CRB share the same OCRA cloud fraction; CRB has no separate variant */
    return read_dataset(info->product_cursor, "cloud_fraction", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_product_cloud_fraction_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->cld_model == 1)
    {
        return read_dataset(info->detailed_results_cursor, "cloud_fraction_crb_precision", harp_type_float,
                            info->num_scanlines * info->num_pixels, data);
    }
    /* CMA+OCA has no precision/uncertainty for cma_cloud_fraction_psf; fall back to the CAL/default path */
    return read_dataset(info->product_cursor, "cloud_fraction_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_product_cloud_optical_thickness(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->cld_model == 2)
    {
        return read_dataset(info->input_data_cursor, "oca_total_cot_psf", harp_type_float,
                            info->num_scanlines * info->num_pixels, data);
    }
    /* CRB has no separate optical thickness variant; fall back to the CAL/default path */
    return read_dataset(info->product_cursor, "cloud_optical_thickness", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_product_cloud_optical_thickness_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "cloud_optical_thickness_precision", harp_type_float,
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

    if (info->cld_model == 1)
    {
        return read_dataset(info->detailed_results_cursor, "cloud_pressure_crb", harp_type_float,
                            info->num_scanlines * info->num_pixels, data);
    }
    if (info->cld_model == 2)
    {
        return read_dataset(info->input_data_cursor, "oca_total_ctp_psf", harp_type_float,
                            info->num_scanlines * info->num_pixels, data);
    }
    return read_dataset(info->product_cursor, "cloud_top_pressure", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_product_cloud_top_pressure_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->cld_model == 1)
    {
        return read_dataset(info->detailed_results_cursor, "cloud_pressure_crb_precision", harp_type_float,
                            info->num_scanlines * info->num_pixels, data);
    }
    /* CMA+OCA has no precision/uncertainty for oca_total_ctp_psf; fall back to the CAL/default path */
    return read_dataset(info->product_cursor, "cloud_top_pressure_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_surface_altitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "surface_altitude", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_surface_altitude_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "surface_altitude_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_surface_pressure(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "surface_pressure", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_snow_ice_type(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "snow_ice_flag", harp_type_int8,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_surface_temperature(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "surface_temperature", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_cld_surface_albedo(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "surface_albedo", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_cld_surface_albedo_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "surface_albedo_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_cloud_top_temperature(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "cloud_top_temperature", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_cloud_type(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "cloud_phase", harp_type_int8,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_solar_zenith_angle(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->geolocation_cursor, "solar_zenith_angle", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_solar_azimuth_angle(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->geolocation_cursor, "solar_azimuth_angle", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_viewing_zenith_angle(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->geolocation_cursor, "viewing_zenith_angle", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_viewing_azimuth_angle(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->geolocation_cursor, "viewing_azimuth_angle", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_cld_validity(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "processing_errors", harp_type_int8,
                        info->num_scanlines * info->num_pixels, data);
}


static int read_cloud_height_crb(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "cloud_height_crb", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_cloud_height_crb_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "cloud_height_crb_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_cloud_albedo_crb(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "cloud_albedo_crb", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_cloud_albedo_crb_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "cloud_albedo_crb_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_qa_value_crb(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int result;

    coda_set_option_perform_conversions(0);
    result = read_dataset(info->detailed_results_cursor, "qa_value_crb", harp_type_int8,
                          info->num_scanlines * info->num_pixels, data);
    coda_set_option_perform_conversions(1);

    return result;
}

static int read_surface_albedo_crb(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "surface_albedo_crb", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_surface_albedo_crb_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "surface_albedo_crb_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_processing_errors_crb(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "processing_errors_crb", harp_type_int8,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_liquid_particle_optical_depth(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "oca_liquid_cot_psf", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_liquid_particle_top_pressure(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "oca_liquid_ctp_psf", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_ice_particle_optical_depth(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "oca_ice_cot_psf", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_ice_particle_top_pressure(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "oca_ice_ctp_psf", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_dust_aerosol_optical_depth(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "oca_desert_dust_cot_psf", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_dust_aerosol_top_pressure(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "oca_desert_dust_ctp_psf", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_product_formaldehyde_tropospheric_column(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "formaldehyde_tropospheric_column", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_product_formaldehyde_tropospheric_column_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "formaldehyde_tropospheric_column_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_product_formaldehyde_tropospheric_column_trueness(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "formaldehyde_tropospheric_column_trueness", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_product_glyoxal_tropospheric_column(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "glyoxal_tropospheric_column", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_product_glyoxal_tropospheric_column_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "glyoxal_tropospheric_column_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_product_glyoxal_tropospheric_column_trueness(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "glyoxal_tropospheric_column_trueness", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_product_nitrogen_dioxide_doas_total_column_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "nitrogen_dioxide_doas_total_column_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_product_nitrogen_dioxide_stratospheric_column(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "nitrogen_dioxide_stratospheric_column", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_product_nitrogen_dioxide_tropospheric_column(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "nitrogen_dioxide_tropospheric_column", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_product_nitrogen_dioxide_tropospheric_column_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "nitrogen_dioxide_tropospheric_column_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
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

static int read_product_ozone_total_column_trueness(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "ozone_total_column_trueness", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_product_ozone_subcolumn(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "ozone_subcolumn", harp_type_float,
                        info->num_scanlines * info->num_pixels * info->num_layers, data);
}

static int read_product_ozone_subcolumn_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "ozone_subcolumn_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels * info->num_layers, data);
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

static int read_results_glyoxal_tropospheric_column_air_mass_factor(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "glyoxal_tropospheric_column_air_mass_factor", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_results_nitrogen_dioxide_stratospheric_air_mass_factor(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "nitrogen_dioxide_stratospheric_air_mass_factor",
                        harp_type_float, info->num_scanlines * info->num_pixels, data);
}

static int read_results_nitrogen_dioxide_tropospheric_air_mass_factor(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "nitrogen_dioxide_tropospheric_air_mass_factor", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_results_ozone_total_air_mass_factor(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "ozone_total_air_mass_factor", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_results_ozone_effective_temperature(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "ozone_effective_temperature", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_alh_surface_albedo(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    harp_array surface_albedo;
    int wavelength_index = info->use_alh_surface_albedo_770;
    long i;

    surface_albedo.ptr = malloc(info->num_scanlines * info->num_pixels * 2 * sizeof(float));
    if (surface_albedo.ptr == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       info->num_scanlines * info->num_pixels * 2 * sizeof(float), __FILE__, __LINE__);
        return -1;

    }
    if (read_dataset(info->detailed_results_cursor, "surface_albedo", harp_type_float,
                     info->num_scanlines * info->num_pixels * 2, surface_albedo) != 0)
    {
        free(surface_albedo.ptr);
        return -1;
    }

    for (i = 0; i < info->num_scanlines * info->num_pixels; i++)
    {
        data.float_data[i] = surface_albedo.float_data[i * 2 + wavelength_index];
    }

    free(surface_albedo.ptr);

    return 0;
}

static int read_so2_surface_albedo(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    const char *name = info->use_so2_surface_albedo_376 ? "surface_albedo_376" : "surface_albedo_328";

    return read_dataset(info->input_data_cursor, name, harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_aui_aerosol_index(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    const char *variable_name = NULL;

    switch (info->wavelength_ratio)
    {
        case s4_340_380nm:
            variable_name = "aerosol_index_340_380";
            break;
        case s4_354_388nm:
            variable_name = "aerosol_index_354_388";
            break;
        default:
            assert(0);
            exit(1);
    }

    return read_dataset(info->product_cursor, variable_name, harp_type_float, info->num_scanlines * info->num_pixels,
                        data);
}

static int read_aui_aerosol_index_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    const char *variable_name = NULL;

    switch (info->wavelength_ratio)
    {
        case s4_340_380nm:
            variable_name = "aerosol_index_340_380_precision";
            break;
        case s4_354_388nm:
            variable_name = "aerosol_index_354_388_precision";
            break;
        default:
            assert(0);
            exit(1);
    }

    return read_dataset(info->product_cursor, variable_name, harp_type_float, info->num_scanlines * info->num_pixels,
                        data);
}

static int read_aui_scene_albedo(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    const char *variable_name = NULL;

    switch (info->wavelength_ratio)
    {
        case s4_340_380nm:
            variable_name = "scene_albedo_380";
            break;
        case s4_354_388nm:
            variable_name = "scene_albedo_388";
            break;
        default:
            assert(0);
            exit(1);
    }

    return read_dataset(info->detailed_results_cursor, variable_name, harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_no2_nitrogen_dioxide_total_column(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    const char *variable_name = "nitrogen_dioxide_doas_total_column";

    if (info->use_summed_total_column)
    {
        variable_name = "nitrogen_dioxide_summed_total_column";
    }

    return read_dataset(info->product_cursor, variable_name, harp_type_float, info->num_scanlines * info->num_pixels,
                        data);
}

static int read_o3_tsc_subcolumn_bounds(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long dimension[2];

    if (read_dataset(info->product_cursor, "subcolumn_bounds", harp_type_float, 2 * info->num_layers, data) != 0)
    {
        return -1;
    }

    /* change {2,vertical} dimension ordering to {vertical,2} */
    dimension[0] = 2;
    dimension[1] = info->num_layers;
    return harp_array_transpose(harp_type_float, 2, dimension, NULL, data);
}

static int read_input_surface_albedo(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "surface_albedo", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_input_cloud_fraction(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "cloud_fraction", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_input_cloud_fraction_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "cloud_fraction_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_input_cloud_height(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "cloud_height", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_input_cloud_height_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "cloud_height_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_input_cloud_albedo(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "cloud_albedo", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_input_cloud_albedo_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "cloud_albedo_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_surface_type(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "surface_classification", harp_type_int32,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_input_cloud_pressure(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "cloud_pressure", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_input_cloud_pressure_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "cloud_pressure_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_o3_cloud_top_pressure(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "cloud_top_pressure", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_o3_cloud_top_pressure_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "cloud_top_pressure_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_o3_cloud_top_height(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "cloud_top_height", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_o3_cloud_top_height_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "cloud_top_height_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_o3_cloud_base_height(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "cloud_base_height", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_o3_cloud_base_height_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "cloud_base_height_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_o3_cloud_base_pressure(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "cloud_base_pressure", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_o3_cloud_base_pressure_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "cloud_base_pressure_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_so2_total_column(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    switch (info->so2_column_type)
    {
        case 0:
            return read_dataset(info->product_cursor, "sulfur_dioxide_total_column_polluted", harp_type_float,
                                info->num_scanlines * info->num_pixels, data);
        case 1:
            return read_dataset(info->detailed_results_cursor, "sulfur_dioxide_total_column_1km",
                                harp_type_float, info->num_scanlines * info->num_pixels, data);
        case 2:
            return read_dataset(info->detailed_results_cursor, "sulfur_dioxide_total_column_7km",
                                harp_type_float, info->num_scanlines * info->num_pixels, data);
        case 3:
            return read_dataset(info->detailed_results_cursor, "sulfur_dioxide_total_column_15km",
                                harp_type_float, info->num_scanlines * info->num_pixels, data);
    }

    assert(0);
    exit(1);
}

static int read_so2_total_column_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    switch (info->so2_column_type)
    {
        case 0:
            return read_dataset(info->product_cursor, "sulfur_dioxide_total_column_polluted_precision", harp_type_float,
                                info->num_scanlines * info->num_pixels, data);
        case 1:
            return read_dataset(info->detailed_results_cursor, "sulfur_dioxide_total_column_1km_precision",
                                harp_type_float, info->num_scanlines * info->num_pixels, data);
        case 2:
            return read_dataset(info->detailed_results_cursor, "sulfur_dioxide_total_column_7km_precision",
                                harp_type_float, info->num_scanlines * info->num_pixels, data);
        case 3:
            return read_dataset(info->detailed_results_cursor, "sulfur_dioxide_total_column_15km_precision",
                                harp_type_float, info->num_scanlines * info->num_pixels, data);
    }

    assert(0);
    exit(1);
}

static int read_so2_total_column_trueness(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    switch (info->so2_column_type)
    {
        case 0:
            return read_dataset(info->product_cursor, "sulfur_dioxide_total_column_polluted_trueness", harp_type_float,
                                info->num_scanlines * info->num_pixels, data);
        case 1:
            return read_dataset(info->detailed_results_cursor, "sulfur_dioxide_total_column_1km_trueness",
                                harp_type_float, info->num_scanlines * info->num_pixels, data);
        case 2:
            return read_dataset(info->detailed_results_cursor, "sulfur_dioxide_total_column_7km_trueness",
                                harp_type_float, info->num_scanlines * info->num_pixels, data);
        case 3:
            return read_dataset(info->detailed_results_cursor, "sulfur_dioxide_total_column_15km_trueness",
                                harp_type_float, info->num_scanlines * info->num_pixels, data);
    }

    assert(0);
    exit(1);
}

static int read_so2_total_air_mass_factor(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    switch (info->so2_column_type)
    {
        case 0:
            return read_dataset(info->detailed_results_cursor, "sulfur_dioxide_total_air_mass_factor_polluted",
                                harp_type_float, info->num_scanlines * info->num_pixels, data);
        case 1:
            return read_dataset(info->detailed_results_cursor, "sulfur_dioxide_total_air_mass_factor_1km",
                                harp_type_float, info->num_scanlines * info->num_pixels, data);
        case 2:
            return read_dataset(info->detailed_results_cursor, "sulfur_dioxide_total_air_mass_factor_7km",
                                harp_type_float, info->num_scanlines * info->num_pixels, data);
        case 3:
            return read_dataset(info->detailed_results_cursor, "sulfur_dioxide_total_air_mass_factor_15km",
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
            return read_dataset(info->detailed_results_cursor,
                                "sulfur_dioxide_total_air_mass_factor_polluted_precision", harp_type_float,
                                info->num_scanlines * info->num_pixels, data);
        case 1:
            return read_dataset(info->detailed_results_cursor, "sulfur_dioxide_total_air_mass_factor_1km_precision",
                                harp_type_float, info->num_scanlines * info->num_pixels, data);
        case 2:
            return read_dataset(info->detailed_results_cursor, "sulfur_dioxide_total_air_mass_factor_7km_precision",
                                harp_type_float, info->num_scanlines * info->num_pixels, data);
        case 3:
            return read_dataset(info->detailed_results_cursor, "sulfur_dioxide_total_air_mass_factor_15km_precision",
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
            return read_dataset(info->detailed_results_cursor, "sulfur_dioxide_total_air_mass_factor_polluted_trueness",
                                harp_type_float, info->num_scanlines * info->num_pixels, data);
        case 1:
            return read_dataset(info->detailed_results_cursor, "sulfur_dioxide_total_air_mass_factor_1km_trueness",
                                harp_type_float, info->num_scanlines * info->num_pixels, data);
        case 2:
            return read_dataset(info->detailed_results_cursor, "sulfur_dioxide_total_air_mass_factor_7km_trueness",
                                harp_type_float, info->num_scanlines * info->num_pixels, data);
        case 3:
            return read_dataset(info->detailed_results_cursor, "sulfur_dioxide_total_air_mass_factor_15km_trueness",
                                harp_type_float, info->num_scanlines * info->num_pixels, data);
    }

    assert(0);
    exit(1);
}

static int include_no2_total_column_precision(void *user_data)
{
    return !((ingest_info *)user_data)->use_summed_total_column;
}

static void register_core_variables(harp_product_definition *product_definition, int include_validity)
{
    const char *path;
    const char *description;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_independent };
    long bounds_dimension[2] = { -1, 4 };

    /* datetime */
    description = "time of the measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double, 1,
                                                   dimension_type, NULL, description, "seconds since 2000-01-01",
                                                   NULL, read_datetime);
    path = "/@time_reference_days_since_1950, /PRODUCT/delta_time[]";
    description = "time reference converted from days since 1950-01-01 to seconds since 2000-01-01 (using 86400 "
        "seconds per day) and delta_time added";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* datetime_length */
    description = "measurement duration";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime_length", harp_type_double, 0, NULL,
                                                   NULL, description, "s", NULL, read_datetime_length);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/PRODUCT/delta_time[]",
                                         "delta_time[num_ground_pixels] - delta_time[0]");

    /* latitude */
    description = "pixel center latitude";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_float, 1, dimension_type,
                                                   NULL, description, "degree_north", NULL, read_latitude);
    harp_variable_definition_set_valid_range_float(variable_definition, -90.0f, 90.0f);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/PRODUCT/latitude[]", NULL);

    /* longitude */
    description = "pixel center longitude";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_float, 1, dimension_type,
                                                   NULL, description, "degree_east", NULL, read_longitude);
    harp_variable_definition_set_valid_range_float(variable_definition, -180.0f, 180.0f);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/PRODUCT/longitude[]", NULL);

    /* latitude_bounds */
    description = "latitudes of pixel boundary";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "latitude_bounds", harp_type_float, 2,
                                                   dimension_type, bounds_dimension, description, "degree_north", NULL,
                                                   read_latitude_bounds);
    harp_variable_definition_set_valid_range_float(variable_definition, -90.0f, 90.0f);
    path = "/PRODUCT/SUPPORT_DATA/GEOLOCATIONS/latitude_bounds[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* longitude_bounds */
    description = "longitudes of pixel boundary";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "longitude_bounds", harp_type_float, 2,
                                                   dimension_type, bounds_dimension, description, "degree_east", NULL,
                                                   read_longitude_bounds);
    harp_variable_definition_set_valid_range_float(variable_definition, -180.0f, 180.0f);
    path = "/PRODUCT/SUPPORT_DATA/GEOLOCATIONS/longitude_bounds[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    if (include_validity)
    {
        /* validity */
        description = "continuous quality descriptor, varying between 0 (no data) and 100 (full quality data)";
        variable_definition =
            harp_ingestion_register_variable_full_read(product_definition, "validity", harp_type_int8, 1,
                                                       dimension_type, NULL, description, NULL, NULL, read_qa_value);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/PRODUCT/qa_value", NULL);
    }
}

static void register_additional_geolocation_variables(harp_product_definition *product_definition)
{
    const char *description;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };

    /* solar_zenith_angle */
    description = "zenith angle of the Sun at the ground pixel location; angle measured away from the vertical";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "solar_zenith_angle", harp_type_float, 1,
                                                   dimension_type, NULL, description, "degree", NULL,
                                                   read_solar_zenith_angle);
    harp_variable_definition_set_valid_range_float(variable_definition, 0.0f, 180.0f);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/PRODUCT/SUPPORT_DATA/GEOLOCATIONS/solar_zenith_angle[]", NULL);

    /* solar_azimuth_angle */
    description = "azimuth angle of the Sun at the ground pixel location; angle measured clockwise from North";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "solar_azimuth_angle", harp_type_float, 1,
                                                   dimension_type, NULL, description, "degree", NULL,
                                                   read_solar_azimuth_angle);
    harp_variable_definition_set_valid_range_float(variable_definition, -180.0f, 180.0f);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/PRODUCT/SUPPORT_DATA/GEOLOCATIONS/solar_azimuth_angle[]", NULL);

    /* viewing_zenith_angle */
    description = "zenith angle of the satellite at the ground pixel location; angle measured away from the vertical";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "viewing_zenith_angle", harp_type_float, 1,
                                                   dimension_type, NULL, description, "degree", NULL,
                                                   read_viewing_zenith_angle);
    harp_variable_definition_set_valid_range_float(variable_definition, 0.0f, 180.0f);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/PRODUCT/SUPPORT_DATA/GEOLOCATIONS/viewing_zenith_angle[]", NULL);

    /* viewing_azimuth_angle */
    description = "azimuth angle of the satellite at the ground pixel location; angle measured clockwise from North";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "viewing_azimuth_angle", harp_type_float, 1,
                                                   dimension_type, NULL, description, "degree", NULL,
                                                   read_viewing_azimuth_angle);
    harp_variable_definition_set_valid_range_float(variable_definition, -180.0f, 180.0f);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/PRODUCT/SUPPORT_DATA/GEOLOCATIONS/viewing_azimuth_angle[]", NULL);
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
                                                   read_surface_altitude);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/surface_altitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* surface_altitude_uncertainty */
    description = "standard error of surface altitude";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "surface_altitude_uncertainty", harp_type_float,
                                                   1, dimension_type, NULL, description, "m", NULL,
                                                   read_surface_altitude_uncertainty);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/surface_altitude_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* surface_pressure */
    description = "surface pressure";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "surface_pressure", harp_type_float, 1,
                                                   dimension_type, NULL, description, "Pa", NULL,
                                                   read_surface_pressure);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/surface_pressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* surface_type */
    description = "surface type classification";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "surface_type", harp_type_int32, 1,
                                                   dimension_type, NULL, description, NULL, NULL, read_surface_type);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/surface_classification[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

static void register_alh_product(void)
{
    const char *path;
    const char *description;
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };
    const char *surface_albedo_option_values[1] = { "770" };

    module = harp_ingestion_register_module("S4-L2-ALH", "Sentinel-4", "MTG", "UVN-2-ALH",
                                            "Sentinel-4 L2 Aerosol Layer Height", ingestion_init, ingestion_done);

    description = "whether to ingest the surface albedo at 758nm (default) or the surface alebedo at 770nm "
        "(surface_albedo=770)";
    harp_ingestion_register_option(module, "surface_albedo", description, 1, surface_albedo_option_values);

    product_definition = harp_ingestion_register_product(module, "S4-L2-ALH", NULL, read_dimensions);

    register_core_variables(product_definition, 1);
    register_additional_geolocation_variables(product_definition);
    register_surface_variables(product_definition);

    /* snow_ice_type */
    description = "surface snow/ice type";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "snow_ice_type", harp_type_int8, 1,
                                                   dimension_type, NULL, description, NULL, NULL, read_snow_ice_type);
    harp_variable_definition_set_enumeration_values(variable_definition, 2, snow_ice_type_values);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/snow_ice_flag[]";
    description = "0: snow_free_land, 1: snow_ice";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* cloud_fraction */
    description = "cloud fraction";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_fraction", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_input_cloud_fraction);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_fraction[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_fraction_uncertainty */
    description = "standard error of cloud fraction";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_fraction_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_input_cloud_fraction_precision);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_fraction_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_top_pressure */
    description = "cloud top pressure";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_top_pressure", harp_type_float, 1,
                                                   dimension_type, NULL, description, "Pa", NULL,
                                                   read_input_cloud_pressure);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_pressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_top_pressure_uncertainty */
    description = "standard error of cloud top pressure";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_top_pressure_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description, "Pa",
                                                   NULL, read_input_cloud_pressure_precision);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_pressure_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_height */
    description = "cloud height";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_height", harp_type_float, 1,
                                                   dimension_type, NULL, description, "m", NULL,
                                                   read_input_cloud_height);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_height[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_height_uncertainty */
    description = "standard error of cloud height";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_height_uncertainty", harp_type_float,
                                                   1, dimension_type, NULL, description, "m", NULL,
                                                   read_input_cloud_height_precision);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_height_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_albedo */
    description = "cloud albedo";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_albedo", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_input_cloud_albedo);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_albedo[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_albedo_uncertainty */
    description = "standard error of cloud albedo";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_albedo_uncertainty", harp_type_float,
                                                   1, dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS,
                                                   NULL, read_input_cloud_albedo_precision);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_albedo_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* aerosol_height */
    description = "height at center of aerosol layer";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "aerosol_height", harp_type_float, 1,
                                                   dimension_type, NULL, description, "m", NULL,
                                                   read_product_aerosol_mid_height);
    path = "/PRODUCT/aerosol_mid_height[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* aerosol_height_uncertainty */
    description = "standard error of height at center of aerosol layer";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "aerosol_height_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description, "m", NULL,
                                                   read_product_aerosol_mid_height_precision);
    path = "/PRODUCT/aerosol_mid_height_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* aerosol_pressure */
    description = "assumed layer pressure thickness";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "aerosol_pressure", harp_type_float, 1,
                                                   dimension_type, NULL, description, "Pa", NULL,
                                                   read_product_aerosol_mid_pressure);
    path = "/PRODUCT/aerosol_mid_pressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* aerosol_pressure_uncertainty */
    description = "standard error of assumed layer pressure thickness";
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
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_results_aerosol_optical_thickness);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/aerosol_optical_thickness[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* aerosol_optical_depth_uncertainty */
    description = "standard error of aerosol optical thickness";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "aerosol_optical_depth_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_results_aerosol_optical_thickness_precision);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/aerosol_optical_thickness_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* surface_albedo */
    description = "surface albedo";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "surface_albedo", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_alh_surface_albedo);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/surface_albedo[..,0]";
    harp_variable_definition_add_mapping(variable_definition, "surface_albedo unset", NULL, path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/surface_albedo[..,1]";
    harp_variable_definition_add_mapping(variable_definition, "surface_albedo=770", NULL, path, NULL);
}

static void register_aui_product(void)
{
    const char *path;
    const char *description;
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };
    const char *wavelength_ratio_option_values[2] = { "354_388nm", "340_380nm" };

    module = harp_ingestion_register_module("S4-L2-AUI", "Sentinel-4", "MTG", "UVN-2-AUI",
                                            "Sentinel-4 L2 UV Aerosol Index", ingestion_init, ingestion_done);

    description = "ingest aerosol index retrieved at wavelengths 354/388 nm (default) or 354/388 nm "
        "(wavelength_ratio=340_380nm)";
    harp_ingestion_register_option(module, "wavelength_ratio", description, 2, wavelength_ratio_option_values);

    product_definition = harp_ingestion_register_product(module, "S4-L2-AUI", NULL, read_dimensions);

    register_core_variables(product_definition, 1);
    register_additional_geolocation_variables(product_definition);
    register_surface_variables(product_definition);

    /* snow_ice_type */
    description = "surface snow/ice type";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "snow_ice_type", harp_type_int8, 1,
                                                   dimension_type, NULL, description, NULL, NULL, read_snow_ice_type);
    harp_variable_definition_set_enumeration_values(variable_definition, 2, snow_ice_type_values);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/snow_ice_flag[]";
    description = "0: snow_free_land, 1: snow_ice";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* absorbing_aerosol_index */
    description = "aerosol index";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "absorbing_aerosol_index", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_aui_aerosol_index);
    harp_variable_definition_add_mapping(variable_definition, "wavelength_ratio=354_388nm or wavelength_ratio unset",
                                         NULL, "/PRODUCT/aerosol_index_354_388", NULL);
    harp_variable_definition_add_mapping(variable_definition, "wavelength_ratio=340_380nm", NULL,
                                         "/PRODUCT/aerosol_index_340_380", NULL);

    /* absorbing_aerosol_index_uncertainty */
    description = "uncertainty of the aerosol index";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "absorbing_aerosol_index_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL, read_aui_aerosol_index_precision);
    harp_variable_definition_add_mapping(variable_definition, "wavelength_ratio=354_388nm or wavelength_ratio unset",
                                         NULL, "/PRODUCT/aerosol_index_354_388_precision", NULL);
    harp_variable_definition_add_mapping(variable_definition, "wavelength_ratio=340_380nm", NULL,
                                         "/PRODUCT/aerosol_index_340_380_precision", NULL);

    /* scene_albedo */
    description = "scene albedo";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "scene_albedo", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_aui_scene_albedo);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/scene_albedo_388[]";
    harp_variable_definition_add_mapping(variable_definition, "wavelength_ratio=354_388nm or wavelength_ratio unset",
                                         NULL, path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/scene_albedo_380[]";
    harp_variable_definition_add_mapping(variable_definition, "wavelength_ratio=340_380nm", NULL, path, NULL);
}

static void register_cld_product(void)
{
    const char *path;
    const char *description;
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };
    const char *band_option_values[1] = { "NIR" };
    const char *model_option_values[2] = { "CRB", "CMA+OCA" };
    const char *cloud_type_values[3] = { "cloud_free", "water_cloud", "ice_cloud" };

    module = harp_ingestion_register_module("S4-L2-CLD", "Sentinel-4", "MTG", "UVN-2-CLD",
                                            "Sentinel-4 L2 Cloud", ingestion_init, ingestion_done);

    description = "ingest cloud properties in the UV/VIS (default) or NIR (band=NIR)";
    harp_ingestion_register_option(module, "band", description, 1, band_option_values);

    description = "cloud retrieval model to use: CAL (default), CRB (model=CRB), or CMA+OCA (model=CMA+OCA)";
    harp_ingestion_register_option(module, "model", description, 2, model_option_values);

    product_definition = harp_ingestion_register_product(module, "S4-L2-CLD", NULL, read_dimensions);

    register_core_variables(product_definition, 0);
    register_additional_geolocation_variables(product_definition);

    /* cloud_base_height */
    description = "cloud base height";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_base_height", harp_type_float, 1,
                                                   dimension_type, NULL, description, "m", NULL,
                                                   read_product_cloud_base_height);
    path = "/PRODUCT/cloud_base_height[]";
    harp_variable_definition_add_mapping(variable_definition, "band unset", NULL, path, NULL);
    path = "/PRODUCT_NIR/cloud_base_height[]";
    harp_variable_definition_add_mapping(variable_definition, "band=NIR", NULL, path, NULL);

    /* cloud_base_height_precision */
    description = "standard error of cloud base height";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_base_height_uncertainty", harp_type_float,
                                                   1, dimension_type, NULL, description, "m", NULL,
                                                   read_product_cloud_base_height_precision);
    path = "/PRODUCT/cloud_base_height_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "band unset", NULL, path, NULL);
    path = "/PRODUCT_NIR/cloud_base_height_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "band=NIR", NULL, path, NULL);

    /* cloud_base_pressure */
    description = "cloud base pressure";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_base_pressure", harp_type_float, 1,
                                                   dimension_type, NULL, description, "Pa", NULL,
                                                   read_product_cloud_base_pressure);
    path = "/PRODUCT/cloud_base_pressure[]";
    harp_variable_definition_add_mapping(variable_definition, "band unset", NULL, path, NULL);
    path = "/PRODUCT_NIR/cloud_base_pressure[]";
    harp_variable_definition_add_mapping(variable_definition, "band=NIR", NULL, path, NULL);

    /* cloud_base_pressure_precision */
    description = "standard error of cloud base pressure";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_base_pressure_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description, "Pa", NULL,
                                                   read_product_cloud_base_pressure_precision);
    path = "/PRODUCT/cloud_base_pressure_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "band unset", NULL, path, NULL);
    path = "/PRODUCT_NIR/cloud_base_pressure_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "band=NIR", NULL, path, NULL);

    /* cloud_fraction */
    description = "cloud fraction";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_fraction", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_product_cloud_fraction);
    path = "/PRODUCT/cloud_fraction[]";
    harp_variable_definition_add_mapping(variable_definition, "band unset", "model unset or model=CRB", path, NULL);
    path = "/PRODUCT_NIR/cloud_fraction[]";
    harp_variable_definition_add_mapping(variable_definition, "band=NIR", "model unset or model=CRB", path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cma_cloud_fraction_psf[]";
    harp_variable_definition_add_mapping(variable_definition, "band unset", "model=CMA+OCA", path, NULL);
    path = "/PRODUCT_NIR/SUPPORT_DATA/INPUT_DATA/cma_cloud_fraction_psf[]";
    harp_variable_definition_add_mapping(variable_definition, "band=NIR", "model=CMA+OCA", path, NULL);

    /* cloud_fraction_uncertainty */
    description = "standard error of cloud fraction; not available for model=CMA+OCA (falls back to the CAL value)";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_fraction_uncertainty", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_product_cloud_fraction_precision);
    path = "/PRODUCT/cloud_fraction_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "band unset", "model unset", path, NULL);
    path = "/PRODUCT_NIR/cloud_fraction_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "band=NIR", "model unset", path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/cloud_fraction_crb_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "band unset", "model=CRB", path, NULL);
    path = "/PRODUCT_NIR/SUPPORT_DATA/DETAILED_RESULTS/cloud_fraction_crb_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "band=NIR", "model=CRB", path, NULL);

    /* cloud_optical_depth */
    description = "cloud optical thickness";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_optical_depth", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_product_cloud_optical_thickness);
    path = "/PRODUCT/cloud_optical_thickness[]";
    harp_variable_definition_add_mapping(variable_definition, "band unset", "model unset or model=CRB", path, NULL);
    path = "/PRODUCT_NIR/cloud_optical_thickness[]";
    harp_variable_definition_add_mapping(variable_definition, "band=NIR", "model unset or model=CRB", path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/oca_total_cot_psf[]";
    harp_variable_definition_add_mapping(variable_definition, "band unset", "model=CMA+OCA", path, NULL);
    path = "/PRODUCT_NIR/SUPPORT_DATA/INPUT_DATA/oca_total_cot_psf[]";
    harp_variable_definition_add_mapping(variable_definition, "band=NIR", "model=CMA+OCA", path, NULL);

    /* cloud_optical_depth_uncertainty */
    description = "standard error of cloud optical thickness";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_optical_depth_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_product_cloud_optical_thickness_precision);
    path = "/PRODUCT/cloud_optical_thickness_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "band unset", NULL, path, NULL);
    path = "/PRODUCT_NIR/cloud_optical_thickness_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "band=NIR", NULL, path, NULL);

    /* cloud_top_height */
    description = "cloud top height";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_top_height", harp_type_float, 1,
                                                   dimension_type, NULL, description, "m", NULL,
                                                   read_product_cloud_top_height);
    path = "/PRODUCT/cloud_top_height[]";
    harp_variable_definition_add_mapping(variable_definition, "band unset", NULL, path, NULL);
    path = "/PRODUCT_NIR/cloud_top_height[]";
    harp_variable_definition_add_mapping(variable_definition, "band=NIR", NULL, path, NULL);

    /* cloud_top_height_precision */
    description = "standard error of cloud top height";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_top_height_uncertainty", harp_type_float,
                                                   1, dimension_type, NULL, description, "m", NULL,
                                                   read_product_cloud_top_height_precision);
    path = "/PRODUCT/cloud_top_height_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "band unset", NULL, path, NULL);
    path = "/PRODUCT_NIR/cloud_top_height_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "band=NIR", NULL, path, NULL);

    /* cloud_top_pressure */
    description = "cloud top pressure";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_top_pressure", harp_type_float, 1,
                                                   dimension_type, NULL, description, "Pa", NULL,
                                                   read_product_cloud_top_pressure);
    path = "/PRODUCT/cloud_top_pressure[]";
    harp_variable_definition_add_mapping(variable_definition, "band unset", "model unset", path, NULL);
    path = "/PRODUCT_NIR/cloud_top_pressure[]";
    harp_variable_definition_add_mapping(variable_definition, "band=NIR", "model unset", path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/cloud_pressure_crb[]";
    harp_variable_definition_add_mapping(variable_definition, "band unset", "model=CRB", path, NULL);
    path = "/PRODUCT_NIR/SUPPORT_DATA/DETAILED_RESULTS/cloud_pressure_crb[]";
    harp_variable_definition_add_mapping(variable_definition, "band=NIR", "model=CRB", path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/oca_total_ctp_psf[]";
    harp_variable_definition_add_mapping(variable_definition, "band unset", "model=CMA+OCA", path, NULL);
    path = "/PRODUCT_NIR/SUPPORT_DATA/INPUT_DATA/oca_total_ctp_psf[]";
    harp_variable_definition_add_mapping(variable_definition, "band=NIR", "model=CMA+OCA", path, NULL);

    /* cloud_top_pressure_precision */
    description = "standard error of cloud top pressure";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_top_pressure_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description, "Pa", NULL,
                                                   read_product_cloud_top_pressure_precision);
    path = "/PRODUCT/cloud_top_pressure_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "band unset", "model unset", path, NULL);
    path = "/PRODUCT_NIR/cloud_top_pressure_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "band=NIR", "model unset", path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/cloud_pressure_crb_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "band unset", "model=CRB", path, NULL);
    path = "/PRODUCT_NIR/SUPPORT_DATA/DETAILED_RESULTS/cloud_pressure_crb_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "band=NIR", "model=CRB", path, NULL);

    /* surface_altitude */
    description = "surface altitude";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "surface_altitude", harp_type_float, 1,
                                                   dimension_type, NULL, description, "m", NULL,
                                                   read_surface_altitude);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/surface_altitude[]";
    harp_variable_definition_add_mapping(variable_definition, "band unset", NULL, path, NULL);
    path = "/PRODUCT_NIR/SUPPORT_DATA/INPUT_DATA/surface_altitude[]";
    harp_variable_definition_add_mapping(variable_definition, "band=NIR", NULL, path, NULL);

    /* surface_altitude_uncertainty */
    description = "standard error of surface altitude";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "surface_altitude_uncertainty", harp_type_float,
                                                   1, dimension_type, NULL, description, "m", NULL,
                                                   read_surface_altitude_uncertainty);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/surface_altitude_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "band unset", NULL, path, NULL);
    path = "/PRODUCT_NIR/SUPPORT_DATA/INPUT_DATA/surface_altitude_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "band=NIR", NULL, path, NULL);

    /* surface_pressure */
    description = "surface pressure";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "surface_pressure", harp_type_float, 1,
                                                   dimension_type, NULL, description, "Pa", NULL,
                                                   read_surface_pressure);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/surface_pressure[]";
    harp_variable_definition_add_mapping(variable_definition, "band unset", NULL, path, NULL);
    path = "/PRODUCT_NIR/SUPPORT_DATA/INPUT_DATA/surface_pressure[]";
    harp_variable_definition_add_mapping(variable_definition, "band=NIR", NULL, path, NULL);

    /* surface_type */
    description = "surface type classification";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "surface_type", harp_type_int32, 1,
                                                   dimension_type, NULL, description, NULL, NULL, read_surface_type);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/surface_classification[]";
    harp_variable_definition_add_mapping(variable_definition, "band unset", NULL, path, NULL);
    path = "/PRODUCT_NIR/SUPPORT_DATA/INPUT_DATA/surface_classification[]";
    harp_variable_definition_add_mapping(variable_definition, "band=NIR", NULL, path, NULL);

    /* snow_ice_type */
    description = "surface snow/ice type";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "snow_ice_type", harp_type_int8, 1,
                                                   dimension_type, NULL, description, NULL, NULL, read_snow_ice_type);
    harp_variable_definition_set_enumeration_values(variable_definition, 2, snow_ice_type_values);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/snow_ice_flag[]";
    description = "0: snow_free_land, 1: snow_ice";
    harp_variable_definition_add_mapping(variable_definition, "band unset", NULL, path, description);
    path = "/PRODUCT_NIR/SUPPORT_DATA/INPUT_DATA/snow_ice_flag[]";
    harp_variable_definition_add_mapping(variable_definition, "band=NIR", NULL, path, description);

    /* surface_temperature */
    description = "surface temperature";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "surface_temperature", harp_type_float, 1,
                                                   dimension_type, NULL, description, "K", NULL,
                                                   read_surface_temperature);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/surface_temperature[]";
    harp_variable_definition_add_mapping(variable_definition, "band unset", NULL, path, NULL);
    path = "/PRODUCT_NIR/SUPPORT_DATA/INPUT_DATA/surface_temperature[]";
    harp_variable_definition_add_mapping(variable_definition, "band=NIR", NULL, path, NULL);

    /* surface_albedo */
    description = "surface albedo fitted from the CAL model";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "surface_albedo", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_cld_surface_albedo);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/surface_albedo[]";
    harp_variable_definition_add_mapping(variable_definition, "band unset", NULL, path, NULL);
    path = "/PRODUCT_NIR/SUPPORT_DATA/DETAILED_RESULTS/surface_albedo[]";
    harp_variable_definition_add_mapping(variable_definition, "band=NIR", NULL, path, NULL);

    /* surface_albedo_uncertainty */
    description = "standard error of the surface albedo fitted from the CAL model";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "surface_albedo_uncertainty", harp_type_float,
                                                   1, dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_cld_surface_albedo_uncertainty);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/surface_albedo_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "band unset", NULL, path, NULL);
    path = "/PRODUCT_NIR/SUPPORT_DATA/DETAILED_RESULTS/surface_albedo_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "band=NIR", NULL, path, NULL);

    /* cloud_top_temperature */
    description = "cloud top temperature";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_top_temperature", harp_type_float, 1,
                                                   dimension_type, NULL, description, "K", NULL,
                                                   read_cloud_top_temperature);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/cloud_top_temperature[]";
    harp_variable_definition_add_mapping(variable_definition, "band unset", NULL, path, NULL);
    path = "/PRODUCT_NIR/SUPPORT_DATA/DETAILED_RESULTS/cloud_top_temperature[]";
    harp_variable_definition_add_mapping(variable_definition, "band=NIR", NULL, path, NULL);

    /* cloud_type */
    description = "cloud phase";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_type", harp_type_int8, 1,
                                                   dimension_type, NULL, description, NULL, NULL, read_cloud_type);
    harp_variable_definition_set_enumeration_values(variable_definition, 3, cloud_type_values);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/cloud_phase[]";
    description = "0: cloud_free, 1: water_cloud, 2: ice_cloud";
    harp_variable_definition_add_mapping(variable_definition, "band unset", NULL, path, description);
    path = "/PRODUCT_NIR/SUPPORT_DATA/DETAILED_RESULTS/cloud_phase[]";
    harp_variable_definition_add_mapping(variable_definition, "band=NIR", NULL, path, description);

    /* cloud_fraction_validity */
    description = "continuous quality descriptor, varying between 0 (no data) and 100 (full quality data)";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_fraction_validity", harp_type_int8, 1,
                                                   dimension_type, NULL, description, NULL, NULL, read_qa_value);
    path = "/PRODUCT/qa_value[]";
    harp_variable_definition_add_mapping(variable_definition, "band unset", NULL, path, NULL);
    path = "/PRODUCT_NIR/qa_value[]";
    harp_variable_definition_add_mapping(variable_definition, "band=NIR", NULL, path, NULL);

    /* validity */
    description = "processing error flag";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "validity", harp_type_int8, 1, dimension_type,
                                                   NULL, description, NULL, NULL, read_cld_validity);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/processing_errors[]";
    harp_variable_definition_add_mapping(variable_definition, "band unset", NULL, path, NULL);
    path = "/PRODUCT_NIR/SUPPORT_DATA/DETAILED_RESULTS/processing_errors[]";
    harp_variable_definition_add_mapping(variable_definition, "band=NIR", NULL, path, NULL);

    /* cloud_height_crb */
    description = "atmospheric height at the level of cloud using the ROCINN CRB model";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_height_crb", harp_type_float, 1,
                                                   dimension_type, NULL, description, "m", NULL,
                                                   read_cloud_height_crb);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/cloud_height_crb[]";
    harp_variable_definition_add_mapping(variable_definition, "band unset", NULL, path, NULL);
    path = "/PRODUCT_NIR/SUPPORT_DATA/DETAILED_RESULTS/cloud_height_crb[]";
    harp_variable_definition_add_mapping(variable_definition, "band=NIR", NULL, path, NULL);

    /* cloud_height_crb_uncertainty */
    description = "standard error of the atmospheric height at the level of cloud using the ROCINN CRB model";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_height_crb_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description, "m", NULL,
                                                   read_cloud_height_crb_precision);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/cloud_height_crb_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "band unset", NULL, path, NULL);
    path = "/PRODUCT_NIR/SUPPORT_DATA/DETAILED_RESULTS/cloud_height_crb_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "band=NIR", NULL, path, NULL);

    /* cloud_albedo_crb */
    description = "albedo of cloud using the ROCINN CRB model";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_albedo_crb", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_cloud_albedo_crb);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/cloud_albedo_crb[]";
    harp_variable_definition_add_mapping(variable_definition, "band unset", NULL, path, NULL);
    path = "/PRODUCT_NIR/SUPPORT_DATA/DETAILED_RESULTS/cloud_albedo_crb[]";
    harp_variable_definition_add_mapping(variable_definition, "band=NIR", NULL, path, NULL);

    /* cloud_albedo_crb_uncertainty */
    description = "standard error of the albedo of cloud using the ROCINN CRB model";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_albedo_crb_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL, read_cloud_albedo_crb_precision);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/cloud_albedo_crb_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "band unset", NULL, path, NULL);
    path = "/PRODUCT_NIR/SUPPORT_DATA/DETAILED_RESULTS/cloud_albedo_crb_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "band=NIR", NULL, path, NULL);

    /* cloud_fraction_validity_crb */
    description = "continuous quality descriptor for the CRB model, varying between 0 (no data) and 100 (full "
        "quality data)";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_fraction_validity_crb", harp_type_int8,
                                                   1, dimension_type, NULL, description, NULL, NULL,
                                                   read_qa_value_crb);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/qa_value_crb[]";
    harp_variable_definition_add_mapping(variable_definition, "band unset", NULL, path, NULL);
    path = "/PRODUCT_NIR/SUPPORT_DATA/DETAILED_RESULTS/qa_value_crb[]";
    harp_variable_definition_add_mapping(variable_definition, "band=NIR", NULL, path, NULL);

    /* surface_albedo_crb */
    description = "surface albedo fitted using the ROCINN CRB model";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "surface_albedo_crb", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_surface_albedo_crb);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/surface_albedo_crb[]";
    harp_variable_definition_add_mapping(variable_definition, "band unset", NULL, path, NULL);
    path = "/PRODUCT_NIR/SUPPORT_DATA/DETAILED_RESULTS/surface_albedo_crb[]";
    harp_variable_definition_add_mapping(variable_definition, "band=NIR", NULL, path, NULL);

    /* surface_albedo_crb_uncertainty */
    description = "standard error of the surface albedo fitted using the ROCINN CRB model";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "surface_albedo_crb_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL, read_surface_albedo_crb_precision);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/surface_albedo_crb_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "band unset", NULL, path, NULL);
    path = "/PRODUCT_NIR/SUPPORT_DATA/DETAILED_RESULTS/surface_albedo_crb_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "band=NIR", NULL, path, NULL);

    /* validity_crb */
    description = "processing error flag for the CRB model";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "validity_crb", harp_type_int8, 1,
                                                   dimension_type, NULL, description, NULL, NULL,
                                                   read_processing_errors_crb);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/processing_errors_crb[]";
    harp_variable_definition_add_mapping(variable_definition, "band unset", NULL, path, NULL);
    path = "/PRODUCT_NIR/SUPPORT_DATA/DETAILED_RESULTS/processing_errors_crb[]";
    harp_variable_definition_add_mapping(variable_definition, "band=NIR", NULL, path, NULL);

    /* liquid_particle_optical_depth */
    description = "liquid cloud optical thickness";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "liquid_particle_optical_depth",
                                                   harp_type_float, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL, read_liquid_particle_optical_depth);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/oca_liquid_cot_psf[]";
    harp_variable_definition_add_mapping(variable_definition, "band unset", NULL, path, NULL);
    path = "/PRODUCT_NIR/SUPPORT_DATA/INPUT_DATA/oca_liquid_cot_psf[]";
    harp_variable_definition_add_mapping(variable_definition, "band=NIR", NULL, path, NULL);

    /* liquid_particle_top_pressure */
    description = "liquid cloud top pressure";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "liquid_particle_top_pressure",
                                                   harp_type_float, 1, dimension_type, NULL, description, "Pa", NULL,
                                                   read_liquid_particle_top_pressure);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/oca_liquid_ctp_psf[]";
    harp_variable_definition_add_mapping(variable_definition, "band unset", NULL, path, NULL);
    path = "/PRODUCT_NIR/SUPPORT_DATA/INPUT_DATA/oca_liquid_ctp_psf[]";
    harp_variable_definition_add_mapping(variable_definition, "band=NIR", NULL, path, NULL);

    /* ice_particle_optical_depth */
    description = "ice cloud optical thickness";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "ice_particle_optical_depth", harp_type_float,
                                                   1, dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS,
                                                   NULL, read_ice_particle_optical_depth);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/oca_ice_cot_psf[]";
    harp_variable_definition_add_mapping(variable_definition, "band unset", NULL, path, NULL);
    path = "/PRODUCT_NIR/SUPPORT_DATA/INPUT_DATA/oca_ice_cot_psf[]";
    harp_variable_definition_add_mapping(variable_definition, "band=NIR", NULL, path, NULL);

    /* ice_particle_top_pressure */
    description = "ice cloud top pressure";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "ice_particle_top_pressure", harp_type_float,
                                                   1, dimension_type, NULL, description, "Pa", NULL,
                                                   read_ice_particle_top_pressure);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/oca_ice_ctp_psf[]";
    harp_variable_definition_add_mapping(variable_definition, "band unset", NULL, path, NULL);
    path = "/PRODUCT_NIR/SUPPORT_DATA/INPUT_DATA/oca_ice_ctp_psf[]";
    harp_variable_definition_add_mapping(variable_definition, "band=NIR", NULL, path, NULL);

    /* dust_aerosol_optical_depth */
    description = "desert dust optical thickness";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "dust_aerosol_optical_depth",
                                                   harp_type_float, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL, read_dust_aerosol_optical_depth);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/oca_desert_dust_cot_psf[]";
    harp_variable_definition_add_mapping(variable_definition, "band unset", NULL, path, NULL);
    path = "/PRODUCT_NIR/SUPPORT_DATA/INPUT_DATA/oca_desert_dust_cot_psf[]";
    harp_variable_definition_add_mapping(variable_definition, "band=NIR", NULL, path, NULL);

    /* dust_aerosol_top_pressure */
    description = "desert dust top pressure";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "dust_aerosol_top_pressure", harp_type_float,
                                                   1, dimension_type, NULL, description, "Pa", NULL,
                                                   read_dust_aerosol_top_pressure);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/oca_desert_dust_ctp_psf[]";
    harp_variable_definition_add_mapping(variable_definition, "band unset", NULL, path, NULL);
    path = "/PRODUCT_NIR/SUPPORT_DATA/INPUT_DATA/oca_desert_dust_ctp_psf[]";
    harp_variable_definition_add_mapping(variable_definition, "band=NIR", NULL, path, NULL);
}

static void register_fdy_product(void)
{
    const char *path;
    const char *description;
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };

    module = harp_ingestion_register_module("S4-L2-HCH", "Sentinel-4", "MTG", "UVN-2-FDY",
                                            "Sentinel-4 Formaldehyde", ingestion_init, ingestion_done);

    product_definition = harp_ingestion_register_product(module, "S4-L2-HCH", NULL, read_dimensions);

    register_core_variables(product_definition, 1);
    register_additional_geolocation_variables(product_definition);
    register_surface_variables(product_definition);

    /* snow_ice_type */
    description = "surface snow/ice type";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "snow_ice_type", harp_type_int8, 1,
                                                   dimension_type, NULL, description, NULL, NULL, read_snow_ice_type);
    harp_variable_definition_set_enumeration_values(variable_definition, 2, snow_ice_type_values);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/snow_ice_flag[]";
    description = "0: snow_free_land, 1: snow_ice";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* cloud_fraction */
    description = "cloud fraction";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_fraction", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_input_cloud_fraction);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_fraction[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_fraction_uncertainty */
    description = "standard error of cloud fraction";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_fraction_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_input_cloud_fraction_precision);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_fraction_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_top_pressure */
    description = "cloud top pressure";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_top_pressure", harp_type_float, 1,
                                                   dimension_type, NULL, description, "Pa", NULL,
                                                   read_input_cloud_pressure);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_pressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_top_pressure_uncertainty */
    description = "standard error of cloud top pressure";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_top_pressure_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description, "Pa",
                                                   NULL, read_input_cloud_pressure_precision);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_pressure_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_height */
    description = "cloud height";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_height", harp_type_float, 1,
                                                   dimension_type, NULL, description, "m", NULL,
                                                   read_input_cloud_height);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_height[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_height_uncertainty */
    description = "standard error of cloud height";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_height_uncertainty", harp_type_float,
                                                   1, dimension_type, NULL, description, "m", NULL,
                                                   read_input_cloud_height_precision);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_height_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_albedo */
    description = "cloud albedo";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_albedo", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_input_cloud_albedo);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_albedo[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_albedo_uncertainty */
    description = "standard error of cloud albedo";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_albedo_uncertainty", harp_type_float,
                                                   1, dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS,
                                                   NULL, read_input_cloud_albedo_precision);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_albedo_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* surface_albedo */
    description = "surface albedo at 342 nm";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "surface_albedo", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_input_surface_albedo);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/surface_albedo[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* tropospheric_HCHO_column_number_density */
    description = "HCHO tropospheric column density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "tropospheric_HCHO_column_number_density",
                                                   harp_type_float, 1, dimension_type, NULL, description, "mol/m2",
                                                   NULL, read_product_formaldehyde_tropospheric_column);
    path = "/PRODUCT/formaldehyde_tropospheric_column[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* tropospheric_HCHO_column_number_density_uncertainty_random */
    description = "random error of HCHO tropospheric column density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "tropospheric_HCHO_column_number_density_uncertainty_random",
                                                   harp_type_float, 1, dimension_type, NULL, description, "mol/m2",
                                                   NULL, read_product_formaldehyde_tropospheric_column_precision);
    path = "/PRODUCT/formaldehyde_tropospheric_column_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* tropospheric_HCHO_column_number_density_uncertainty_systematic */
    description = "systematic error of HCHO tropospheric column density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "tropospheric_HCHO_column_number_density_uncertainty_systematic",
                                                   harp_type_float, 1, dimension_type, NULL, description, "mol/m2",
                                                   NULL, read_product_formaldehyde_tropospheric_column_trueness);
    path = "/PRODUCT/formaldehyde_tropospheric_column_trueness[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* tropospheric_HCHO_column_number_density_amf */
    description = "tropospheric air mass factor";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "tropospheric_HCHO_column_number_density_amf",
                                                   harp_type_float, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_results_formaldehyde_tropospheric_air_mass_factor);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/formaldehyde_tropospheric_air_mass_factor[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* formaldehyde_tropospheric_air_mass_factor_uncertainty_random */
    description = "random error of HCHO tropospheric column density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "tropospheric_HCHO_column_number_density_amf_uncertainty_random",
                                                   harp_type_float, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_results_formaldehyde_tropospheric_air_mass_factor_precision);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/formaldehyde_tropospheric_air_mass_factor_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* formaldehyde_tropospheric_air_mass_factor_uncertainty_systematic */
    description = "systematic error of HCHO tropospheric column density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "tropospheric_HCHO_column_number_density_amf_uncertainty_systematic",
                                                   harp_type_float, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_results_formaldehyde_tropospheric_air_mass_factor_trueness);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/formaldehyde_tropospheric_air_mass_factor_trueness[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

static void register_gly_product(void)
{
    const char *path;
    const char *description;
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };

    module = harp_ingestion_register_module("S4-L2-CHO", "Sentinel-4", "MTG", "UVN-2-GLY",
                                            "Sentinel-4 Tropospheric Glyoxal", ingestion_init, ingestion_done);

    product_definition = harp_ingestion_register_product(module, "S4-L2-CHO", NULL, read_dimensions);

    register_core_variables(product_definition, 1);
    register_additional_geolocation_variables(product_definition);
    register_surface_variables(product_definition);

    /* snow_ice_type */
    description = "surface snow/ice type";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "snow_ice_type", harp_type_int8, 1,
                                                   dimension_type, NULL, description, NULL, NULL, read_snow_ice_type);
    harp_variable_definition_set_enumeration_values(variable_definition, 2, snow_ice_type_values);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/snow_ice_flag[]";
    description = "0: snow_free_land, 1: snow_ice";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* cloud_fraction */
    description = "cloud fraction";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_fraction", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_input_cloud_fraction);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_fraction[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_fraction_uncertainty */
    description = "standard error of cloud fraction";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_fraction_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_input_cloud_fraction_precision);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_fraction_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_top_pressure */
    description = "cloud top pressure";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_top_pressure", harp_type_float, 1,
                                                   dimension_type, NULL, description, "Pa", NULL,
                                                   read_input_cloud_pressure);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_pressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_top_pressure_uncertainty */
    description = "standard error of cloud top pressure";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_top_pressure_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description, "Pa",
                                                   NULL, read_input_cloud_pressure_precision);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_pressure_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_height */
    description = "cloud height";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_height", harp_type_float, 1,
                                                   dimension_type, NULL, description, "m", NULL,
                                                   read_input_cloud_height);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_height[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_height_uncertainty */
    description = "standard error of cloud height";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_height_uncertainty", harp_type_float,
                                                   1, dimension_type, NULL, description, "m", NULL,
                                                   read_input_cloud_height_precision);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_height_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_albedo */
    description = "cloud albedo";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_albedo", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_input_cloud_albedo);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_albedo[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_albedo_uncertainty */
    description = "standard error of cloud albedo";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_albedo_uncertainty", harp_type_float,
                                                   1, dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS,
                                                   NULL, read_input_cloud_albedo_precision);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_albedo_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* tropospheric_C2H2O2_column_number_density */
    description = "troposphere mole content of glyoxal";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "tropospheric_C2H2O2_column_number_density",
                                                   harp_type_float, 1, dimension_type, NULL, description, "mol/m2",
                                                   NULL, read_product_glyoxal_tropospheric_column);
    path = "/PRODUCT/glyoxal_tropospheric_column[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* tropospheric_C2H2O2_column_number_density_uncertainty_random */
    description = "random error of troposphere mole content of glyoxal";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "tropospheric_C2H2O2_column_number_density_uncertainty_random",
                                                   harp_type_float, 1, dimension_type, NULL, description, "mol/m2",
                                                   NULL, read_product_glyoxal_tropospheric_column_precision);
    path = "/PRODUCT/glyoxal_tropospheric_column_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* tropospheric_C2H2O2_column_number_density_uncertainty_systematic */
    description = "systematic error of troposphere mole content of glyoxal";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "tropospheric_C2H2O2_column_number_density_uncertainty_systematic",
                                                   harp_type_float, 1, dimension_type, NULL, description, "mol/m2",
                                                   NULL, read_product_glyoxal_tropospheric_column_trueness);
    path = "/PRODUCT/glyoxal_tropospheric_column_trueness[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* tropospheric_C2H2O2_column_number_density_amf */
    description = "tropospheric air mass factor";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "tropospheric_C2H2O2_column_number_density_amf",
                                                   harp_type_float, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_results_glyoxal_tropospheric_column_air_mass_factor);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/glyoxal_tropospheric_column_air_mass_factor[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

static void register_no2_product(void)
{
    const char *path;
    const char *description;
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };
    const char *total_column_options[] = { "summed", "total" };

    module = harp_ingestion_register_module("S4-L2-NO2", "Sentinel-4", "MTG", "UVN-2-NO2",
                                            "Sentinel-4 Nitrogen Dioxide", ingestion_init, ingestion_done);

    harp_ingestion_register_option(module, "total_column", "whether to use nitrogen_dioxide_doas_total_column (which "
                                   "is derived from the total slant column divided by the total amf) or "
                                   "nitrogen_dioxide_summed_total_column (which is the sum of the retrieved "
                                   "tropospheric and stratospheric columns); option values are 'summed' (default) and "
                                   "'total'", 2, total_column_options);

    product_definition = harp_ingestion_register_product(module, "S4-L2-NO2", NULL, read_dimensions);

    register_core_variables(product_definition, 1);
    register_additional_geolocation_variables(product_definition);
    register_surface_variables(product_definition);

    /* snow_ice_type */
    description = "surface snow/ice type";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "snow_ice_type", harp_type_int8, 1,
                                                   dimension_type, NULL, description, NULL, NULL, read_snow_ice_type);
    harp_variable_definition_set_enumeration_values(variable_definition, 2, snow_ice_type_values);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/snow_ice_flag[]";
    description = "0: snow_free_land, 1: snow_ice";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* cloud_fraction */
    description = "cloud fraction";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_fraction", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_input_cloud_fraction);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_fraction[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_fraction_uncertainty */
    description = "standard error of cloud fraction";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_fraction_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_input_cloud_fraction_precision);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_fraction_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_top_pressure */
    description = "cloud top pressure";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_top_pressure", harp_type_float, 1,
                                                   dimension_type, NULL, description, "Pa", NULL,
                                                   read_input_cloud_pressure);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_pressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_top_pressure_uncertainty */
    description = "standard error of cloud top pressure";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_top_pressure_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description, "Pa",
                                                   NULL, read_input_cloud_pressure_precision);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_pressure_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_height */
    description = "cloud height";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_height", harp_type_float, 1,
                                                   dimension_type, NULL, description, "m", NULL,
                                                   read_input_cloud_height);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_height[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_height_uncertainty */
    description = "standard error of cloud height";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_height_uncertainty", harp_type_float,
                                                   1, dimension_type, NULL, description, "m", NULL,
                                                   read_input_cloud_height_precision);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_height_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_albedo */
    description = "cloud albedo";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_albedo", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_input_cloud_albedo);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_albedo[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_albedo_uncertainty */
    description = "standard error of cloud albedo";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_albedo_uncertainty", harp_type_float,
                                                   1, dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS,
                                                   NULL, read_input_cloud_albedo_precision);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_albedo_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* NO2_column_number_density */
    description = "mole content of nitrogen dioxide";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "NO2_column_number_density",
                                                   harp_type_float, 1, dimension_type, NULL, description, "mol/m2",
                                                   NULL, read_no2_nitrogen_dioxide_total_column);
    path = "/PRODUCT/nitrogen_dioxide_summed_total_column[]";
    harp_variable_definition_add_mapping(variable_definition, "total_column=summed or total_column unset", NULL, path,
                                         NULL);
    path = "/PRODUCT/nitrogen_dioxide_doas_total_column[]";
    harp_variable_definition_add_mapping(variable_definition, "total_column=total", NULL, path, NULL);

    /* NO2_column_number_density_uncertainty */
    description = "standard error of  mole content of nitrogen dioxide";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "NO2_column_number_density_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description, "mol/m2",
                                                   include_no2_total_column_precision,
                                                   read_product_nitrogen_dioxide_doas_total_column_precision);
    path = "/PRODUCT/nitrogen_dioxide_doas_total_column_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "total_column=total", NULL, path, NULL);

    /* stratospheric_NO2_column_number_density */
    description = "troposphere mole content of nitrogen dioxide";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "stratospheric_NO2_column_number_density",
                                                   harp_type_float, 1, dimension_type, NULL, description, "mol/m2",
                                                   NULL, read_product_nitrogen_dioxide_stratospheric_column);
    path = "/PRODUCT/nitrogen_dioxide_stratospheric_column[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* stratospheric_NO2_column_number_density_amf */
    description = "stratospheric air mass factor";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "stratospheric_NO2_column_number_density_amf",
                                                   harp_type_float, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_results_nitrogen_dioxide_stratospheric_air_mass_factor);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/nitrogen_dioxide_stratospheric_air_mass_factor[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* tropospheric_NO2_column_number_density */
    description = "troposphere mole content of nitrogen dioxide";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "tropospheric_NO2_column_number_density",
                                                   harp_type_float, 1, dimension_type, NULL, description, "mol/m2",
                                                   NULL, read_product_nitrogen_dioxide_tropospheric_column);
    path = "/PRODUCT/nitrogen_dioxide_tropospheric_column[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* tropospheric_NO2_column_number_density_uncertainty */
    description = "standard error of troposphere mole content of nitrogen dioxide";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "tropospheric_NO2_column_number_density_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description, "mol/m2",
                                                   NULL, read_product_nitrogen_dioxide_tropospheric_column_precision);
    path = "/PRODUCT/nitrogen_dioxide_tropospheric_column_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* tropospheric_NO2_column_number_density_amf */
    description = "tropospheric air mass factor";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "tropospheric_NO2_column_number_density_amf",
                                                   harp_type_float, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_results_nitrogen_dioxide_tropospheric_air_mass_factor);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/nitrogen_dioxide_tropospheric_air_mass_factor[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

static void register_o3_product(void)
{
    const char *path;
    const char *description;
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };

    module = harp_ingestion_register_module("S4-L2-OTO", "Sentinel-4", "MTG", "UVN-2-O3",
                                            "Sentinel-4 Ozone Total Column", ingestion_init, ingestion_done);

    product_definition = harp_ingestion_register_product(module, "S4-L2-OTO", NULL, read_dimensions);

    register_core_variables(product_definition, 1);
    register_additional_geolocation_variables(product_definition);
    register_surface_variables(product_definition);

    /* snow_ice_type */
    description = "surface snow/ice type";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "snow_ice_type", harp_type_int8, 1,
                                                   dimension_type, NULL, description, NULL, NULL, read_snow_ice_type);
    harp_variable_definition_set_enumeration_values(variable_definition, 2, snow_ice_type_values);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/snow_ice_flag[]";
    description = "0: snow_free_land, 1: snow_ice";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* cloud_fraction */
    description = "cloud fraction";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_fraction", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_input_cloud_fraction);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_fraction[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_fraction_uncertainty */
    description = "standard error of cloud fraction";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_fraction_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_input_cloud_fraction_precision);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_fraction_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_top_pressure */
    description = "cloud top pressure";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_top_pressure", harp_type_float, 1,
                                                   dimension_type, NULL, description, "Pa", NULL,
                                                   read_o3_cloud_top_pressure);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_top_pressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_top_pressure_uncertainty */
    description = "standard error of cloud top pressure";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_top_pressure_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description, "Pa",
                                                   NULL, read_o3_cloud_top_pressure_precision);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_top_pressure_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_top_height */
    description = "cloud top height";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_top_height", harp_type_float, 1,
                                                   dimension_type, NULL, description, "m", NULL,
                                                   read_o3_cloud_top_height);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_top_height[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_top_height_uncertainty */
    description = "standard error of cloud top height";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_top_height_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description, "m", NULL,
                                                   read_o3_cloud_top_height_precision);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_top_height_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_base_height */
    description = "cloud base height";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_base_height", harp_type_float, 1,
                                                   dimension_type, NULL, description, "m", NULL,
                                                   read_o3_cloud_base_height);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_base_height[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_base_height_uncertainty */
    description = "standard error of cloud base height";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_base_height_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description, "m", NULL,
                                                   read_o3_cloud_base_height_precision);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_base_height_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_base_pressure */
    description = "cloud base pressure";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_base_pressure", harp_type_float, 1,
                                                   dimension_type, NULL, description, "Pa", NULL,
                                                   read_o3_cloud_base_pressure);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_base_pressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_base_pressure_uncertainty */
    description = "standard error of cloud base pressure";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_base_pressure_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description, "Pa", NULL,
                                                   read_o3_cloud_base_pressure_precision);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_base_pressure_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* surface_albedo */
    description = "surface albedo at 328 nm";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "surface_albedo", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_input_surface_albedo);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/surface_albedo[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* O3_column_number_density */
    description = "mole content of ozone";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_column_number_density",
                                                   harp_type_float, 1, dimension_type, NULL, description, "mol/m2",
                                                   NULL, read_product_ozone_total_column);
    path = "/PRODUCT/ozone_total_column[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* O3_column_number_density_uncertainty_random */
    description = "random error of mole content of ozone";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "O3_column_number_density_uncertainty_random",
                                                   harp_type_float, 1, dimension_type, NULL, description, "mol/m2",
                                                   NULL, read_product_ozone_total_column_precision);
    path = "/PRODUCT/ozone_total_column_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* O3_column_number_density_uncertainty_systematic */
    description = "systematic error of mole content of ozone";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "O3_column_number_density_uncertainty_systematic",
                                                   harp_type_float, 1, dimension_type, NULL, description, "mol/m2",
                                                   NULL, read_product_ozone_total_column_trueness);
    path = "/PRODUCT/ozone_total_column_trueness[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* O3_column_number_density_amf */
    description = "total column air mass factor";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_column_number_density_amf",
                                                   harp_type_float, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_results_ozone_total_air_mass_factor);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/ozone_total_air_mass_factor[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* O3_effective_temperature */
    description = "ozone cross section effective temperature";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_effective_temperature",
                                                   harp_type_float, 1, dimension_type, NULL, description, "K", NULL,
                                                   read_results_ozone_effective_temperature);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/ozone_effective_temperature[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

static void register_o3_tsc_product(void)
{
    const char *path;
    const char *description;
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[3] =
        { harp_dimension_time, harp_dimension_vertical, harp_dimension_independent };
    long bounds_dimension[2] = { -1, 2 };

    module = harp_ingestion_register_module("S4-L2-OTR", "Sentinel-4", "MTG", "UVN-2-O3-TSC",
                                            "Sentinel-4 Tropospheric Ozone", ingestion_init, ingestion_done);

    product_definition = harp_ingestion_register_product(module, "S4-L2-OTR", NULL, read_dimensions);

    register_core_variables(product_definition, 1);
    register_additional_geolocation_variables(product_definition);
    register_surface_variables(product_definition);

    /* snow_ice_type */
    description = "surface snow/ice type";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "snow_ice_type", harp_type_int8, 1,
                                                   dimension_type, NULL, description, NULL, NULL, read_snow_ice_type);
    harp_variable_definition_set_enumeration_values(variable_definition, 2, snow_ice_type_values);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/snow_ice_flag[]";
    description = "0: snow_free_land, 1: snow_ice";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* cloud_fraction */
    description = "cloud fraction";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_fraction", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_input_cloud_fraction);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_fraction[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_top_pressure */
    description = "cloud top pressure";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_top_pressure", harp_type_float, 1,
                                                   dimension_type, NULL, description, "Pa", NULL,
                                                   read_o3_cloud_top_pressure);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_top_pressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* surface_albedo */
    description = "assumed white sky surface albedo at 342 nm";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "surface_albedo", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_input_surface_albedo);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/surface_albedo[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* pressure_bounds */
    description = "pressure boundaries";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "pressure_bounds",
                                                   harp_type_float, 2, &dimension_type[1], bounds_dimension,
                                                   description, "Pa", NULL, read_o3_tsc_subcolumn_bounds);
    path = "/PRODUCT/subcolumn_bounds[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* O3_volume_mixing_ratio */
    description = "sub-column average volume mixing ratio of ozone";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_volume_mixing_ratio",
                                                   harp_type_float, 2, dimension_type, NULL, description, "ppmv",
                                                   NULL, read_product_ozone_subcolumn);
    path = "/PRODUCT/ozone_subcolumn[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* O3_volume_mixing_ratio_uncertainty */
    description = "standard error of the sub-column average volume mixing ratio of ozone";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_volume_mixing_ratio_uncertainty",
                                                   harp_type_float, 2, dimension_type, NULL, description, "ppmv",
                                                   NULL, read_product_ozone_subcolumn_precision);
    path = "/PRODUCT/ozone_subcolumn_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

static void register_so2_product(void)
{
    const char *path;
    const char *description;
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };
    const char *so2_column_options[] = { "1km", "7km", "15km" };
    const char *surface_albedo_option_values[1] = { "376" };

    module = harp_ingestion_register_module("S4-L2-SO2", "Sentinel-4", "MTG", "UVN-2-SO2",
                                            "Sentinel-4 Sulphur Dioxide", ingestion_init, ingestion_done);

    harp_ingestion_register_option(module, "so2_column", "whether to ingest the anothropogenic SO2 column at the PBL "
                                   "(default), the SO2 column from the 1km box profile (so2_column=1km), from the 7km "
                                   "box profile (so2_column=7km), or from the 15km box profile (so2_column=15km)", 3,
                                   so2_column_options);

    harp_ingestion_register_option(module, "surface_albedo", "whether to ingest the surface albedo at 328nm "
                                   "(default) or the surface albedo at 376nm (surface_albedo=376)", 1,
                                   surface_albedo_option_values);

    product_definition = harp_ingestion_register_product(module, "S4-L2-SO2", NULL, read_dimensions);

    register_core_variables(product_definition, 1);
    register_additional_geolocation_variables(product_definition);
    register_surface_variables(product_definition);

    /* snow_ice_type */
    description = "surface snow/ice type";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "snow_ice_type", harp_type_int8, 1,
                                                   dimension_type, NULL, description, NULL, NULL, read_snow_ice_type);
    harp_variable_definition_set_enumeration_values(variable_definition, 2, snow_ice_type_values);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/snow_ice_flag[]";
    description = "0: snow_free_land, 1: snow_ice";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* cloud_fraction */
    description = "cloud fraction";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_fraction", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_input_cloud_fraction);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_fraction[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_fraction_uncertainty */
    description = "standard error of cloud fraction";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_fraction_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_input_cloud_fraction_precision);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_fraction_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_top_pressure */
    description = "cloud top pressure";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_top_pressure", harp_type_float, 1,
                                                   dimension_type, NULL, description, "Pa", NULL,
                                                   read_input_cloud_pressure);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_pressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_top_pressure_uncertainty */
    description = "standard error of cloud top pressure";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_top_pressure_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description, "Pa",
                                                   NULL, read_input_cloud_pressure_precision);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_pressure_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_height */
    description = "cloud height";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_height", harp_type_float, 1,
                                                   dimension_type, NULL, description, "m", NULL,
                                                   read_input_cloud_height);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_height[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_height_uncertainty */
    description = "standard error of cloud height";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_height_uncertainty", harp_type_float,
                                                   1, dimension_type, NULL, description, "m", NULL,
                                                   read_input_cloud_height_precision);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_height_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_albedo */
    description = "cloud albedo";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_albedo", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_input_cloud_albedo);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_albedo[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_albedo_uncertainty */
    description = "standard error of cloud albedo";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_albedo_uncertainty", harp_type_float,
                                                   1, dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS,
                                                   NULL, read_input_cloud_albedo_precision);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_albedo_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* surface_albedo */
    description = "surface albedo";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "surface_albedo", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_so2_surface_albedo);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/surface_albedo_328[]";
    harp_variable_definition_add_mapping(variable_definition, "surface_albedo unset", NULL, path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/INPUT_DATA/surface_albedo_376[]";
    harp_variable_definition_add_mapping(variable_definition, "surface_albedo=376", NULL, path, NULL);

    /* SO2_column_number_density */
    description = "sulphur dioxide column density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "SO2_column_number_density", harp_type_float, 1,
                                                   dimension_type, NULL, description, "mol/m^2", NULL,
                                                   read_so2_total_column);
    path = "/PRODUCT/sulfur_dioxide_total_column_polluted[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column unset", NULL, path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfur_dioxide_total_column_1km[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column=1km", NULL, path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfur_dioxide_total_column_7km[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column=7km", NULL, path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfur_dioxide_total_column_15km[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column=15km", NULL, path, NULL);

    /* SO2_column_number_density_uncertainty_random */
    description = "random error of sulphur dioxide column density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "SO2_column_number_density_uncertainty_random",
                                                   harp_type_float, 1, dimension_type, NULL, description, "mol/m^2",
                                                   NULL, read_so2_total_column_precision);
    path = "/PRODUCT/sulfur_dioxide_total_column_polluted_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column unset", NULL, path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfur_dioxide_total_column_1km_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column=1km", NULL, path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfur_dioxide_total_column_7km_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column=7km", NULL, path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfur_dioxide_total_column_15km_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column=15km", NULL, path, NULL);

    /* SO2_column_number_density_uncertainty_systematic */
    description = "systematic error of sulphur dioxide column density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "SO2_column_number_density_uncertainty_systematic", harp_type_float,
                                                   1, dimension_type, NULL, description, "mol/m^2", NULL,
                                                   read_so2_total_column_trueness);
    path = "/PRODUCT/sulfur_dioxide_total_column_polluted_trueness[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column unset", NULL, path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfur_dioxide_total_column_1km_trueness[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column=1km", NULL, path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfur_dioxide_total_column_7km_trueness[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column=7km", NULL, path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfur_dioxide_total_column_15km_trueness[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column=15km", NULL, path, NULL);

    /* SO2_column_number_density_amf */
    description = "total air mass factor";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "SO2_column_number_density_amf", harp_type_float,
                                                   1, dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_so2_total_air_mass_factor);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfur_dioxide_total_air_mass_factor_polluted[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column unset", NULL, path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfur_dioxide_total_air_mass_factor_1km[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column=1km", NULL, path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfur_dioxide_total_air_mass_factor_7km[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column=7km", NULL, path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfur_dioxide_total_air_mass_factor_15km[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column=15km", NULL, path, NULL);

    /* SO2_column_number_density_amf_uncertainty_random */
    description = "random error of total air mass factor";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "SO2_column_number_density_amf_uncertainty_random", harp_type_float,
                                                   1, dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_so2_total_air_mass_factor_precision);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfur_dioxide_total_air_mass_factor_polluted_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column unset", NULL, path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfur_dioxide_total_air_mass_factor_1km_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column=1km", NULL, path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfur_dioxide_total_air_mass_factor_7km_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column=7km", NULL, path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfur_dioxide_total_air_mass_factor_15km_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column=15km", NULL, path, NULL);

    /* SO2_column_number_density_amf_uncertainty_systematic */
    description = "systematic error of total air mass factor";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "SO2_column_number_density_amf_uncertainty_systematic",
                                                   harp_type_float, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_so2_total_air_mass_factor_trueness);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfur_dioxide_total_air_mass_factor_polluted_trueness[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column unset", NULL, path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfur_dioxide_total_air_mass_factor_1km_trueness[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column=1km", NULL, path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfur_dioxide_total_air_mass_factor_7km_trueness[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column=7km", NULL, path, NULL);
    path = "/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfur_dioxide_total_air_mass_factor_15km_trueness[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column=15km", NULL, path, NULL);
}

int harp_ingestion_module_s4_l2_init(void)
{
    register_alh_product();
    register_aui_product();
    register_cld_product();
    register_fdy_product();
    register_gly_product();
    register_no2_product();
    register_o3_product();
    register_o3_tsc_product();
    register_so2_product();

    return 0;
}
