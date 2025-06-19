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
#include "harp-ingestion.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>


static const char *snow_ice_type_values[] = { "snow_free_land", "sea_ice", "permanent_ice", "snow", "ocean" };


typedef enum s5_product_type_enum
{
    s5_type_aui,
    s5_type_ch4,
    s5_type_no2,
    s5_type_o3,
    s5_type_so2,
    s5_type_cld,
    s5_type_co,
} s5_product_type;


#define S5_NUM_PRODUCT_TYPES (((int)s5_type_co) + 1)


typedef enum s5_dimension_type_enum
{
    s5_dim_time = 0,    /* flattened scanline x pixel grid */
    s5_dim_scanline,    /* original along-track dimension */
    s5_dim_pixel,       /* original across-track dimension */
    s5_dim_corner,      /* 4 polygon corners per ground pixel */
    s5_dim_layer,       /* pressure / altitude layers */
    s5_dim_level,       /* layer +1 (bounds) */
    s5_dim_spectral,    /* extra wavelengths (e.g. reflectance pair) */
    s5_dim_profile      /* short profile axis (SO2 options, etc.) */
} s5_dimension_type;

/* handy constant: last enum value + 1 */
#define S5_NUM_DIM_TYPES   ((int)s5_dim_profile + 1)

static const char *s5_dimension_name[S5_NUM_PRODUCT_TYPES][S5_NUM_DIM_TYPES] = {
    {"time", "scanline", "ground_pixel", "corner", NULL, NULL, NULL, NULL},     /* AUI */
    {"time", "scanline", "ground_pixel", "corner", "layer", NULL, "sif_wavelengths", NULL},     /* CH4 */
    {"time", "scanline", "ground_pixel", "corner", "layer", NULL, NULL, NULL},  /* NO2 */
    {"time", "scanline", "ground_pixel", "corner", "layer", NULL, NULL, NULL},  /* O3_ */
    {"time", "scanline", "ground_pixel", "corner", "layer", NULL, NULL, "profile"},     /* SO2 */
    {"time", "scanline", "ground_pixel", "corner", NULL, NULL, NULL, NULL},     /* CLD */
    {"time", "scanline", "ground_pixel", "corner", "layer", NULL, NULL, NULL},  /* CO_ */
};

/* the array shape of delta_time variable for each data product */
static const int s5_delta_time_num_dims[S5_NUM_PRODUCT_TYPES] = { 1, 1, 1, 1, 1, 1 };

typedef struct ingest_info_struct
{
    coda_product *product;

    int use_co_corrected;
    int use_co_nd_avk;
    int use_ch4_band_options;   /* CH4: SWIR-1 (default), SWIR-3, or NIR-2 */
    int use_cld_band_options;   /* CLD: BAND3A (default), or BAND3C */
    int so2_column_type;        /* 0: PBL (anthropogenic), 1: 1km box profile, 2: 7km bp, 3: 15km bp, 4: layer height */

    s5_product_type product_type;
    long num_times;
    long num_scanlines;
    long num_pixels;
    long num_corners;
    long num_layers;
    long num_levels;
    long num_latitudes;
    long num_longitudes;
    long num_spectral;
    long num_profile;

    coda_cursor product_cursor;
    coda_cursor geolocation_cursor;
    coda_cursor detailed_results_cursor;
    coda_cursor input_data_cursor;

    /* CLD */
    coda_cursor b3a_product_cursor;
    coda_cursor b3a_geolocation_cursor;
    coda_cursor b3a_detailed_results_cursor;
    coda_cursor b3a_input_data_cursor;
    coda_cursor b3c_product_cursor;
    coda_cursor b3c_geolocation_cursor;
    coda_cursor b3c_detailed_results_cursor;
    coda_cursor b3c_input_data_cursor;

    int processor_version;
    int collection_number;
    int wavelength_ratio;
    int ch4_option;     /* CH4: physics (default) or precision */
    int no2_column_option;      /* NO2: total (default) or summed */
    int is_nrti;

    uint8_t *surface_layer_status;      /* used for O3; 0: use as-is, 1: remove */
} ingest_info;


/* The routines start here 
 */

static const char *get_product_type_name(s5_product_type product_type)
{
    switch (product_type)
    {
        case s5_type_aui:
            return "SN5_02_AUI";
        case s5_type_ch4:
            return "SN5_02_CH4";
        case s5_type_no2:
            return "SN5_02_NO2";
        case s5_type_o3:
            return "SN5_02_O3_";
        case s5_type_so2:
            return "SN5_02_SO2";
        case s5_type_cld:
            return "SN5_02_CLD";
        case s5_type_co:
            return "SN5_02_CO_";
    }

    assert(0);
    exit(1);
}

/* tiny helper for get_product_type() */
static void dash_to_underscore(char *s)
{
    /* Changing '-' to '_' */
    for (; *s; ++s)
        if (*s == '-')
            *s = '_';
}


static void broadcast_array_float(long num_scanlines, long num_pixels, float *data)
{
    long i;

    /* Repeat the value for each scanline for all pixels in that scanline. Iterate in reverse to avoid overwriting
     * scanline values.
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

static void broadcast_array_double(long num_scanlines, long num_pixels, double *data)
{
    long i;

    /* Repeat the value for each scanline for all pixels in that scanline. Iterate in reverse to avoid overwriting
     * scanline values.
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

static int get_product_type(coda_product *product, s5_product_type *product_type)
{
    coda_cursor cursor, child, *src = NULL;
    char buf[256];      /* plenty of room for long IDs   */
    long len;
    int i;

    /* 1. bind root */
    if (coda_cursor_set_product(&cursor, product) != 0)
        return harp_set_error(HARP_ERROR_CODA, NULL), -1;

    /* 2. first try the clean ProductShortName */
    if (coda_cursor_goto(&cursor, "/METADATA/GRANULE_DESCRIPTION@ProductShortName") == 0)
    {
        src = &cursor;
    }
    else if (coda_cursor_goto(&cursor, "/@product_name") == 0)
    {
        /* may be scalar or 1-D array */
        coda_type_class tc;

        if (coda_cursor_get_type_class(&cursor, &tc) != 0)
            return harp_set_error(HARP_ERROR_CODA, NULL), -1;

        if (tc == coda_array_class)
        {
            child = cursor;
            if (coda_cursor_goto_first_array_element(&child) != 0)
                return harp_set_error(HARP_ERROR_CODA, NULL), -1;
            src = &child;
        }
        else
        {
            src = &cursor;
        }
    }
    else
    {
        return harp_set_error(HARP_ERROR_INGESTION, "cannot find product identifier"), -1;
    }

    /* 3. read the string */
    if (coda_cursor_get_string_length(src, &len) != 0 ||
        len <= 0 || len >= (long)sizeof(buf) || coda_cursor_read_string(src, buf, sizeof(buf)) != 0)
        return harp_set_error(HARP_ERROR_CODA, NULL), -1;

    /* 4. normalise and show */
    dash_to_underscore(buf);

    /* 5. search for any known short code */
    for (i = 0; i < S5_NUM_PRODUCT_TYPES; i++)
    {
        const char *code = get_product_type_name((s5_product_type)i);   /* e.g. "SN5_02_CO_" */

        if (strstr(buf, code) != NULL)
        {
            *product_type = (s5_product_type)i;
            return 0;
        }
    }

    return harp_set_error(HARP_ERROR_INGESTION, "unsupported product type '%s'", buf), -1;
}


/* Recursively search for the named 1D dimension field within a CODA structure. */
static int find_dimension_length_recursive(coda_cursor *cursor, const char *name, long *length)
{
    coda_type_class type_class;

    if (coda_cursor_get_type_class(cursor, &type_class) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, "Failed to get type class");
        return -1;
    }

    if (type_class == coda_record_class)
    {
        coda_cursor sub_cursor = *cursor;

        /* Navigate to the first field */
        if (coda_cursor_goto_first_record_field(&sub_cursor) == 0)
        {
            do
            {
                /* Attempt to navigate to the field by name */
                coda_cursor test_cursor = *cursor;

                if (coda_cursor_goto_record_field_by_name(&test_cursor, name) == 0)
                {
                    long coda_dim[CODA_MAX_NUM_DIMS];
                    int num_dims;

                    if (coda_cursor_get_array_dim(&test_cursor, &num_dims, coda_dim) != 0)
                    {
                        harp_set_error(HARP_ERROR_CODA, "Failed to get array dimensions");
                        return -1;
                    }

                    if (num_dims != 1)
                    {
                        harp_set_error(HARP_ERROR_INGESTION, "Field '%s' is not a 1D array", name);
                        return -1;
                    }

                    *length = coda_dim[0];
                    return 0;
                }

                /* Recursively search in the substructure */
                if (find_dimension_length_recursive(&sub_cursor, name, length) == 0)
                {
                    return 0;
                }

            } while (coda_cursor_goto_next_record_field(&sub_cursor) == 0);
        }
    }
    else if (type_class == coda_array_class)
    {
        long num_elements;

        if (coda_cursor_get_num_elements(cursor, &num_elements) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, "Failed to get number of array elements");
            return -1;
        }

        if (num_elements > 0)
        {
            coda_cursor sub_cursor = *cursor;

            if (coda_cursor_goto_array_element_by_index(&sub_cursor, 0) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, "Failed to go to array element");
                return -1;
            }

            if (find_dimension_length_recursive(&sub_cursor, name, length) == 0)
            {
                return 0;
            }
        }
    }

    /* Not found in this branch */
    return -1;
}

/* Find dimension length by recursively searching under data/PRODUCT. */
static int get_dimension_length(ingest_info *info, const char *name, long *length)
{
    coda_cursor cursor = info->product_cursor;

    if (find_dimension_length_recursive(&cursor, name, length) != 0)
    {
        harp_set_error(HARP_ERROR_INGESTION, "Dimension '%s' not found in product structure", name);
        return -1;
    }

    return 0;
}


/* Init Routines */

/* Initialize CODA cursors for main record groups with inline comments. */
static int init_cursors(ingest_info *info)
{
    coda_cursor cursor;

    /* Bind a cursor to the root of the CODA product */
    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    /* CLD product has to set of bands each containing its own product type */
    if (info->product_type == s5_type_cld)
    {
        if (coda_cursor_goto_record_field_by_name(&cursor, "PRODUCT_BAND3A") != 0)
        {
            /* Fallback to data/PRODUCT for simulated files */
            if (coda_cursor_goto_record_field_by_name(&cursor, "data") != 0 ||
                coda_cursor_goto_record_field_by_name(&cursor, "PRODUCT_BAND3A") != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
        }
        /* Save PRODUCT_BAND3A cursor; subsequent navigation is relative to this. */
        info->b3a_product_cursor = cursor;

        /* Enter SUPPORT_DATA under PRODUCT (same location for both layouts):
         * '/PRODUCT/SUPPORT_DATA' or '/data/PRODUCT/SUPPORT_DATA'
         */
        if (coda_cursor_goto_record_field_by_name(&cursor, "SUPPORT_DATA") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }

        /* Geolocation group (skip for O3-TCL): under SUPPORT_DATA
         * '/.../SUPPORT_DATA/GEOLOCATIONS' for both layouts.
         */
        if (coda_cursor_goto_record_field_by_name(&cursor, "GEOLOCATIONS") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        info->b3a_geolocation_cursor = cursor;

        /* Back to SUPPORT_DATA */
        coda_cursor_goto_parent(&cursor);

        /* Detailed results: '/.../SUPPORT_DATA/DETAILED_RESULTS' */
        if (coda_cursor_goto_record_field_by_name(&cursor, "DETAILED_RESULTS") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        info->b3a_detailed_results_cursor = cursor;

        /* Back to SUPPORT_DATA */
        coda_cursor_goto_parent(&cursor);

        /* Input data group (skip for O3-TCL): '/.../SUPPORT_DATA/INPUT_DATA' */
        if (coda_cursor_goto_record_field_by_name(&cursor, "INPUT_DATA") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        info->b3a_input_data_cursor = cursor;

        /* (Again) Bind a cursor to the root of the CODA product 
         * (to repeat the procedure above for BAND3B). */
        if (coda_cursor_set_product(&cursor, info->product) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }

        if (coda_cursor_goto_record_field_by_name(&cursor, "PRODUCT_BAND3C") != 0)
        {
            /* fallback to data/PRODUCT for simulated files */
            if (coda_cursor_goto_record_field_by_name(&cursor, "data") != 0 ||
                coda_cursor_goto_record_field_by_name(&cursor, "PRODUCT_BAND3C") != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
        }
        /* Save PRODUCT_BAND3A cursor; subsequent navigation is relative to this. */
        info->b3c_product_cursor = cursor;

        /* Enter SUPPORT_DATA under PRODUCT (same location for both layouts):
         * '/PRODUCT/SUPPORT_DATA' or '/data/PRODUCT/SUPPORT_DATA'
         */
        if (coda_cursor_goto_record_field_by_name(&cursor, "SUPPORT_DATA") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }

        /* Geolocation group (skip for O3-TCL): under SUPPORT_DATA
         * '/.../SUPPORT_DATA/GEOLOCATIONS' for both layouts.
         */
        if (coda_cursor_goto_record_field_by_name(&cursor, "GEOLOCATIONS") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        info->b3c_geolocation_cursor = cursor;

        /* Back to SUPPORT_DATA */
        coda_cursor_goto_parent(&cursor);

        /* Detailed results: '/.../SUPPORT_DATA/DETAILED_RESULTS' */
        if (coda_cursor_goto_record_field_by_name(&cursor, "DETAILED_RESULTS") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        info->b3c_detailed_results_cursor = cursor;
        /* Back to SUPPORT_DATA */
        coda_cursor_goto_parent(&cursor);

        /* Input data group (skip for O3-TCL): '/.../SUPPORT_DATA/INPUT_DATA' */
        if (coda_cursor_goto_record_field_by_name(&cursor, "INPUT_DATA") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        info->b3c_input_data_cursor = cursor;

        /* Make the cursors point to BAND3A by default */
        if (info->use_cld_band_options == 0)
        {
            info->product_cursor = info->b3a_product_cursor;
            info->geolocation_cursor = info->b3a_geolocation_cursor;
            info->detailed_results_cursor = info->b3a_detailed_results_cursor;
            info->input_data_cursor = info->b3a_input_data_cursor;
        }
        else
        {
            info->product_cursor = info->b3c_product_cursor;
            info->geolocation_cursor = info->b3c_geolocation_cursor;
            info->detailed_results_cursor = info->b3c_detailed_results_cursor;
            info->input_data_cursor = info->b3c_input_data_cursor;
        }
    }
    else
    {
        /* Navigate into the "PRODUCT" record:
         *    - Standard path: '/PRODUCT'
         *    - Simulated fallback: '/data/PRODUCT'
         */
        if (coda_cursor_goto_record_field_by_name(&cursor, "PRODUCT") != 0)
        {
            /* Fallback to data/PRODUCT for simulated files */
            if (coda_cursor_goto_record_field_by_name(&cursor, "data") != 0 ||
                coda_cursor_goto_record_field_by_name(&cursor, "PRODUCT") != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
        }

        /* Save PRODUCT cursor; subsequent navigation is relative to this. */
        info->product_cursor = cursor;

        /* Enter SUPPORT_DATA under PRODUCT (same location for both layouts):
         * '/PRODUCT/SUPPORT_DATA' or '/data/PRODUCT/SUPPORT_DATA'
         */
        if (coda_cursor_goto_record_field_by_name(&cursor, "SUPPORT_DATA") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }

        /* Geolocation group (skip for O3-TCL): under SUPPORT_DATA
         * '/.../SUPPORT_DATA/GEOLOCATIONS' for both layouts.
         */
        if (coda_cursor_goto_record_field_by_name(&cursor, "GEOLOCATIONS") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        info->geolocation_cursor = cursor;

        /* Back to SUPPORT_DATA */
        coda_cursor_goto_parent(&cursor);

        /* Detailed results: '/.../SUPPORT_DATA/DETAILED_RESULTS' */
        if (coda_cursor_goto_record_field_by_name(&cursor, "DETAILED_RESULTS") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        info->detailed_results_cursor = cursor;

        /* Back to SUPPORT_DATA */
        coda_cursor_goto_parent(&cursor);

        /* Input data group (skip for O3-TCL): '/.../SUPPORT_DATA/INPUT_DATA' */
        if (coda_cursor_goto_record_field_by_name(&cursor, "INPUT_DATA") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        info->input_data_cursor = cursor;
    }

    return 0;
}

/* Initialize record dimension lengths for the Sentinel-5 simulated L2 dataset */
static int init_dimensions(ingest_info *info)
{
    /* Get number of scanlines */
    if (s5_dimension_name[info->product_type][s5_dim_scanline] != NULL)
    {
        if (get_dimension_length(info, s5_dimension_name[info->product_type][s5_dim_scanline],
                                 &info->num_scanlines) != 0)
        {
            return -1;
        }
    }

    /* Get number of ground pixels */
    if (s5_dimension_name[info->product_type][s5_dim_pixel] != NULL)
    {
        if (get_dimension_length(info, s5_dimension_name[info->product_type][s5_dim_pixel], &info->num_pixels) != 0)
        {
            return -1;
        }
    }

    /* Get number of corners and validate */
    if (s5_dimension_name[info->product_type][s5_dim_corner] != NULL)
    {
        if (get_dimension_length(info, s5_dimension_name[info->product_type][s5_dim_corner], &info->num_corners) != 0)
        {
            return -1;
        }
        if (info->num_corners != 4)
        {
            harp_set_error(HARP_ERROR_INGESTION, "dimension '%s' has length %ld; expected 4",
                           s5_dimension_name[info->product_type][s5_dim_corner], info->num_corners);
            return -1;
        }
    }

    /* Get number of layers */
    if (s5_dimension_name[info->product_type][s5_dim_layer] != NULL)
    {
        if (get_dimension_length(info, s5_dimension_name[info->product_type][s5_dim_layer], &info->num_layers) != 0)
        {
            return -1;
        }
    }

    if (s5_dimension_name[info->product_type][s5_dim_level] != NULL)
    {
        if (get_dimension_length(info, s5_dimension_name[info->product_type][s5_dim_level], &info->num_levels) != 0)
        {
            return -1;
        }
    }

    /* Infer levels = layers + 1 */
    if (info->num_layers > 0 && info->num_levels > 0)
    {
        if (info->num_levels != info->num_layers + 1)
        {
            harp_set_error(HARP_ERROR_INGESTION, "dimension '%s' has length %ld; expected %ld",
                           s5_dimension_name[info->product_type][s5_dim_level], info->num_levels, info->num_layers + 1);
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
                           s5_dimension_name[info->product_type][s5_dim_level], info->num_levels);
            return -1;
        }

        info->num_layers = info->num_levels - 1;
    }

    return 0;
}


/* Extract Sentinel-5 L2 product collection and processor version
 * from the global "logical product name".
 */
static int init_versions(ingest_info *info)
{
    coda_cursor cursor;
    char product_name[84];

    /* Since earlier S5P L2 products did not always have a valid 'id' global attribute
     * we will keep the version numbers at -1 if we can't extract the right information.
     */
    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, "/@id") != 0)
    {
        /* no global 'id' attribute */
        return 0;
    }
    if (coda_cursor_read_string(&cursor, product_name, 84) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (strlen(product_name) != 83)
    {
        /* 'id' attribute does not contain a valid logical product name */
        return 0;
    }

    /* Populating the variables */
    info->collection_number = (int)strtol(&product_name[58], NULL, 10);
    info->processor_version = (int)strtol(&product_name[61], NULL, 10);

    return 0;
}

static void ingestion_done(void *user_data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->surface_layer_status != NULL)
    {
        free(info->surface_layer_status);
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
    info->num_times = 0;
    info->num_scanlines = 0;
    info->num_pixels = 0;
    info->num_corners = 0;
    info->num_layers = 0;
    info->num_levels = 0;

    info->num_spectral = 0;
    info->num_profile = 0;

    info->wavelength_ratio = 354;

    info->surface_layer_status = NULL;

    /* default */
    info->ch4_option = 0;
    info->use_ch4_band_options = 0;
    info->no2_column_option = 0;
    info->use_cld_band_options = 0;     /* CLD: BAND3A (default), or BAND3C */
    info->so2_column_type = 0;  /* 0=PBL (default)  1=1 km  2=7 km  3=15 km */



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

    *definition = *module->product_definition;

    if (harp_ingestion_options_has_option(options, "wavelength_ratio"))
    {
        if (harp_ingestion_options_get_option(options, "wavelength_ratio", &option_value) != 0)
        {
            ingestion_done(info);
            return -1;
        }
        if (strcmp(option_value, "335_367nm") == 0)
        {
            info->wavelength_ratio = 335;
        }
        else if (strcmp(option_value, "354_388nm") == 0)
        {
            info->wavelength_ratio = 354;
        }
        else
        {
            /* Option values are guaranteed to be legal if present. */
            assert(strcmp(option_value, "340_380nm") == 0);
            info->wavelength_ratio = 340;
        }
    }


    if (info->product_type == s5_type_ch4)
    {
        /* CH4: methane_dry_air_column_mixing_ratio_[physics|proxy] */
        if (harp_ingestion_options_has_option(options, "ch4"))
        {
            if (harp_ingestion_options_get_option(options, "ch4", &option_value) != 0)
            {
                ingestion_done(info);
                return -1;
            }
            if (strcmp(option_value, "proxy") == 0)
            {
                info->ch4_option = 1;
            }
            else
            {
                /* Physics is the default and first in the list */
                assert(strcmp(option_value, "physics") == 0);
                info->ch4_option = 0;
            }
        }
        /* CH4: surface_albedo_[swir_1|swir_3|nir_2] */
        if (harp_ingestion_options_has_option(options, "band"))
        {
            if (harp_ingestion_options_get_option(options, "band", &option_value) != 0)
            {
                ingestion_done(info);
                return -1;
            }
            if (strcmp(option_value, "SWIR-3") == 0)
            {
                info->use_ch4_band_options = 1;
            }
            else if (strcmp(option_value, "NIR-2") == 0)
            {
                info->use_ch4_band_options = 2;
            }
            else
            {
                /* Must be SWIR-1 */
                assert(strcmp(option_value, "SWIR-1") == 0);
                info->use_ch4_band_options = 0;
            }
        }
    }

    /* CLD: BAND3A (default), or BAND3C */
    if (info->product_type == s5_type_cld)
    {
        /* Only if option was provided, otherwise use the dafult value, provided above */
        if (harp_ingestion_options_has_option(options, "band"))
        {
            if (harp_ingestion_options_get_option(options, "band", &option_value) != 0)
            {
                ingestion_done(info);
                return -1;
            }
            if (strcmp(option_value, "band3c") == 0)
            {
                info->use_cld_band_options = 1;
            }
            else
            {
                /* Must be BAND3A */
                assert(strcmp(option_value, "band3a") == 0);
                info->use_cld_band_options = 0;
            }
        }
    }

    /* NO2: nitrogen_dioxide_[|summed]_total_column */
    if (harp_ingestion_options_has_option(options, "total_column"))
    {
        if (harp_ingestion_options_get_option(options, "total_column", &option_value) != 0)
        {
            ingestion_done(info);
            return -1;
        }
        if (strcmp(option_value, "summed") == 0)
        {
            info->no2_column_option = 1;
        }
        else
        {
            /* physics is the default and first in the list */
            assert(strcmp(option_value, "total") == 0);
            info->no2_column_option = 0;
        }
    }


    /* SO2 */
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


    if (init_dimensions(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }

    /* Adding spectral dimension to AUI product for reflectance */
    if (info->product_type == s5_type_aui)
    {
        info->num_spectral = 2; /*  (lower, upper) reflectances  */
    }
    else if (info->product_type == s5_type_ch4)
    {
        info->num_spectral = 4; /*  sif_wavelengths  */
    }

    if (info->product_type == s5_type_so2)
    {
        info->num_profile = 4;
    }

    *user_data = info;

    return 0;
}


/* Reading Routines */

/* Supply HARP with the lengths of the global axes for the
 * Sentinel-5 simulated products.  
 */
static int read_dimensions(void *user_data, long dimension[HARP_NUM_DIM_TYPES])
{
    ingest_info *info = (ingest_info *)user_data;

    /* From the online documentation: 
     *
     * time       : Temporal dimension; this is also the only appendable dimension.
     * vertical   : Vertical dimension, indicating height or depth.
     * spectral   : Spectral dimension, associated with wavelength, wavenumber, or frequency.
     * latitude   : Latitude dimension, only to be used for the latitude axis
     *              of a regular latitude x longitude grid.
     * longitude  : Longitude dimension, only to be used for the longitude axis
     *              of a regular latitude x longitude grid.
     * independent: Independent dimension, used to index other quantities, such
     *              as the corner coordinates of ground pixel polygons.
     *
     * [Note]: Within a HARP product, all dimensions of the same type should
     * have the same length, except independent dimensions. For example, it is
     * an error to have two variables within the same product that both have a
     * time dimension, yet of a different length.
     */


    dimension[harp_dimension_time] = info->num_scanlines * info->num_pixels;

    /* 2. vertical grid - only if available                                  */
    if (info->num_layers > 0)
        dimension[harp_dimension_vertical] = info->num_layers;

    switch (info->product_type)
    {
        case s5_type_aui:
            dimension[harp_dimension_spectral] = info->num_spectral;
            break;
        case s5_type_ch4:
            dimension[harp_dimension_spectral] = info->num_spectral;
            break;
        case s5_type_so2:
            dimension[harp_dimension_time] = info->num_scanlines * info->num_pixels;
            break;
            /* CLD, NO2, CO, ... need no extra axes */
        default:
            break;
    }

    return 0;
}

/* Copied from the s5p l2 module */
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


/* Read and convert the observation time array for Sentinel-5 simulated CO. */
static int read_datetime(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    harp_array time_reference_array;
    double time_reference;
    long i;

    /* 1) Read the single time reference value (seconds since 2010-01-01) */
    time_reference_array.ptr = &time_reference;
    if (read_dataset(info->product_cursor, "time", harp_type_double, 1, time_reference_array) != 0)
    {
        return -1;
    }

    /* 2) Read delta_time and optionally broadcast:
     *    - If standard layout (2D), read num_scanlines values then broadcast over pixels.
     *    - If simulated layout (1D), read num_scanlines values only.
     */
    if (s5_delta_time_num_dims[info->product_type] == 2)
    {
        /* Standard S5P: one delta_time per scanline, then repeat for each pixel */
        if (read_dataset(info->product_cursor, "delta_time", harp_type_double, info->num_scanlines, data) != 0)
        {
            return -1;
        }
        broadcast_array_double(info->num_scanlines, info->num_pixels, data.double_data);
    }
    else
    {
        /* Simulated: exactly one delta_time per scanline, no broadcast */
        if (read_dataset(info->product_cursor, "delta_time", harp_type_double, info->num_scanlines, data) != 0)
        {
            return -1;
        }
    }

    /* 3) Convert milliseconds to seconds and add to reference time */
    {
        long count = info->num_scanlines * (s5_delta_time_num_dims[info->product_type] == 2 ? info->num_pixels : 1);

        for (i = 0; i < count; i++)
        {
            data.double_data[i] = time_reference + data.double_data[i] / 1e3;
        }
    }

    return 0;
}

/* Read the absolute orbit number from the global attribute */
static int read_orbit_index(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    coda_cursor cursor;
    coda_native_type read_type;
    uint32_t uval;
    int32_t ival;

    /* 1) Bind a cursor to the root product */
    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    /* 2) Try /@orbit_start first, then /@orbit */
    if (coda_cursor_goto(&cursor, "/@orbit_start") != 0 && coda_cursor_goto(&cursor, "/@orbit") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    /* 3) If it's an array, move to its first element */
    {
        coda_type_class tc;

        if (coda_cursor_get_type_class(&cursor, &tc) != 0)
        {
            return -1;
        }
        if (tc == coda_array_class)
        {
            if (coda_cursor_goto_first_array_element(&cursor) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
        }
    }

    /* 4) Determine the native storage type and read appropriately */
    if (coda_cursor_get_read_type(&cursor, &read_type) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (read_type == coda_native_type_uint32)
    {
        /* Stored as an unsigned 32-bit */
        if (coda_cursor_read_uint32(&cursor, &uval) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        ival = (int32_t)uval;
    }
    else
    {
        /* Stored as a signed 32-bit (or other compatible) */
        if (coda_cursor_read_int32(&cursor, &ival) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
    }

    /* 5) Write back into the HARP buffer */
    data.int32_data[0] = ival;
    return 0;
}


/* Field: data/PRODUCT */

static int read_product_latitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->geolocation_cursor, "latitude", harp_type_float, info->num_scanlines * info->num_pixels,
                        data);
}

static int read_product_longitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->geolocation_cursor, "longitude", harp_type_float, info->num_scanlines * info->num_pixels,
                        data);
}

static int read_product_qa_value(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int result;

    /* we don't want the add_offset/scale_factor applied for the qa_value; we just want the raw 8bit value */
    coda_set_option_perform_conversions(0);
    result = read_dataset(info->product_cursor, "qa_value", harp_type_int8,
                          info->num_scanlines * info->num_pixels, data);
    coda_set_option_perform_conversions(1);

    return result;
}

static int read_product_carbon_monoxide_total_column(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->product_type == s5_type_co)
    {
        return read_dataset(info->product_cursor, "carbon_monoxide_total_column", harp_type_float,
                            info->num_scanlines * info->num_pixels, data);
    }
    else
    {
        return read_dataset(info->detailed_results_cursor, "carbon_monoxide_total_column", harp_type_float,
                            info->num_scanlines * info->num_pixels, data);
    }
}

static int read_product_carbon_monoxide_total_column_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "carbon_monoxide_total_column_precision", harp_type_float,
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
        case 335:
            variable_name = "aerosol_index_335_367";
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
        case 335:
            variable_name = "aerosol_index_335_367_precision";
            break;
        default:
            assert(0);
            exit(1);
    }

    return read_dataset(info->product_cursor, variable_name, harp_type_float, info->num_scanlines * info->num_pixels,
                        data);
}

static int read_product_methane_dry_air_column_mixing_ratio(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    const char *dataset_name = NULL;
    long count;

    /* Total number of elements = scanlines x ground pixels */
    count = info->num_scanlines * info->num_pixels;

    /* Pick the right HDF5 field based on the ch4 option */
    switch (info->ch4_option)
    {
        case 0:
            dataset_name = "methane_dry_air_column_mixing_ratio_physics";       /* physics-based */
            break;
        case 1:
            dataset_name = "methane_dry_air_column_mixing_ratio_proxy"; /* proxy-based */
            break;
        default:
            /* Should never happen if option parsing is correct */
            harp_set_error(HARP_ERROR_INGESTION, "invalid CH4 option %d", info->ch4_option);
            return -1;
    }

    /* Read the chosen dataset in one shot */
    if (read_dataset(info->product_cursor, dataset_name, harp_type_float, count, data) != 0)
    {
        return -1;
    }

    /* 1-D along time already ascending -> nothing more to do */
    return 0;
}


static int read_product_methane_dry_air_column_mixing_ratio_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    const char *dataset_name = NULL;
    long count;

    /* Total number of elements = scanlines x ground pixels */
    count = info->num_scanlines * info->num_pixels;

    /* Pick the right HDF5 field based on the ch4 option */
    switch (info->ch4_option)
    {
        case 0:
            dataset_name = "methane_dry_air_column_mixing_ratio_precision_physics";     /* physics-based */
            break;
        case 1:
            dataset_name = "methane_dry_air_column_mixing_ratio_precision_proxy";       /* proxy-based */
            break;
        default:
            /* Should never happen if option parsing is correct */
            harp_set_error(HARP_ERROR_INGESTION, "invalid CH4 option %d", info->ch4_option);
            return -1;
    }

    /* Read the chosen dataset in one shot */
    if (read_dataset(info->product_cursor, dataset_name, harp_type_float, count, data) != 0)
    {
        return -1;
    }

    /* 1-D along time already ascending -> nothing more to do */
    return 0;
}


static int read_product_nitrogen_dioxide_tropospheric_column(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "nitrogen_dioxide_tropospheric_column", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_product_nitrogen_dioxide_tropospheric_column_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "nitrogen_dioxide_tropospheric_column_uncertainty", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_product_nitrogen_dioxide_tropospheric_column_air_mass_factor(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "nitrogen_dioxide_tropospheric_column_air_mass_factor", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}


static int read_product_nitrogen_dioxide_total_column_air_mass_factor(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "nitrogen_dioxide_total_column_air_mass_factor", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_product_nitrogen_dioxide_total_column_avk(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long dimension[2];

    if (read_dataset(info->product_cursor, "nitrogen_dioxide_total_column_averaging_kernel", harp_type_float,
                     info->num_scanlines * info->num_pixels * info->num_layers, data) != 0)
    {
        return -1;
    }
    dimension[0] = info->num_scanlines * info->num_pixels;
    dimension[1] = info->num_layers;
    return harp_array_invert(harp_type_float, 1, 2, dimension, data);
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

static int read_product_effective_cloud_fraction(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "effective_cloud_fraction", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_product_effective_cloud_fraction_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "effective_cloud_fraction_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_product_cloud_pressure(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "cloud_pressure", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_product_cloud_pressure_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "cloud_pressure_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_product_cloud_height(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "cloud_height_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_product_cloud_height_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "cloud_height_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_product_sulfur_dioxide_layer_height(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "sulfur_dioxide_layer_height", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_product_sulfur_dioxide_layer_height_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "sulfur_dioxide_layer_height_uncertainty", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_product_sulfur_dioxide_layer_height_flag(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->product_cursor, "sulfur_dioxide_layer_height_flag", harp_type_int8,
                        info->num_scanlines * info->num_pixels, data);
}


/* Field: data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS */

/* Convert **processing-quality flags** from the file to the type/shape
 * expected by HARP.
 */
static int read_results_processing_quality_flags(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    coda_cursor cursor = info->product_cursor;
    long expected = info->num_scanlines * info->num_pixels;
    uint64_t *tmp = NULL;
    long i;

    /* inside PRODUCT, go to the variable */
    if (coda_cursor_goto_record_field_by_name(&cursor, "processing_quality_flags") != 0)
        return harp_set_error(HARP_ERROR_CODA, NULL), -1;

    /* sanity-check element count */
    long actual;

    if (coda_cursor_get_num_elements(&cursor, &actual) != 0 || actual != expected)
        return harp_set_error(HARP_ERROR_INGESTION,
                              "processing_quality_flags: expected %ld elements, got %ld", expected, actual), -1;

    /* read uint64 -> tmp */
    tmp = malloc(expected * sizeof(*tmp));
    if (!tmp)
        return harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "allocating %ld bytes", expected * sizeof(*tmp)), -1;

    if (coda_cursor_read_uint64_array(&cursor, tmp, coda_array_ordering_c) != 0)
        return free(tmp), harp_set_error(HARP_ERROR_CODA, NULL), -1;

    /* Cast to int32 in place */
    for (i = 0; i < expected; i++)
        data.int32_data[i] = (int32_t)tmp[i];

    free(tmp);
    return 0;
}


static int read_results_water_total_column(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "water_total_column", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_results_semiheavy_water_total_column(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "semiheavy_water_total_column", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_results_carbon_dioxide_total_column(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "carbon_dioxide_total_column", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}



/* Read the **measured TOA reflectances** that form the Aerosol-Index
 * wavelength pair and pack them into a 2-column HARP array.
 */
static int read_results_reflectance_measured(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    const char *variable_name_lower = NULL;
    const char *variable_name_upper = NULL;

    long num_elements = info->num_scanlines * info->num_pixels;

    /* Determine reflectance variable names based on wavelength_ratio */
    switch (info->wavelength_ratio)
    {
        case 354:
            variable_name_lower = "reflectance_354_measured";
            variable_name_upper = "reflectance_388_measured";
            break;
        case 340:
            variable_name_lower = "reflectance_340_measured";
            variable_name_upper = "reflectance_380_measured";
            break;
        case 335:
            variable_name_lower = "reflectance_335_measured";
            variable_name_upper = "reflectance_367_measured";
            break;
        default:
            fprintf(stderr, "[ERROR] Unknown wavelength_ratio: %d\n", info->wavelength_ratio);
            assert(0);
            exit(1);
    }

    /* Allocate temporary buffers */
    harp_array refl_lower;
    harp_array refl_upper;

    refl_lower.float_data = malloc(num_elements * sizeof(float));
    if (refl_lower.float_data == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "could not allocate memory for reflectance_lower array");
        return -1;
    }

    refl_upper.float_data = malloc(num_elements * sizeof(float));
    if (refl_upper.float_data == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "could not allocate memory for reflectance_upper array");
        free(refl_lower.float_data);
        return -1;
    }

    /* Check actual dataset sizes */
    {
        coda_cursor cursor = info->detailed_results_cursor;

        if (coda_cursor_goto_record_field_by_name(&cursor, variable_name_lower) == 0)
        {
            long actual_elements;

            coda_cursor_get_num_elements(&cursor, &actual_elements);
        }

        cursor = info->detailed_results_cursor;
        if (coda_cursor_goto_record_field_by_name(&cursor, variable_name_upper) == 0)
        {
            long actual_elements;

            coda_cursor_get_num_elements(&cursor, &actual_elements);
        }
    }

    /* Read the lower reflectance dataset */
    if (read_dataset(info->detailed_results_cursor, variable_name_lower, harp_type_float, num_elements, refl_lower) !=
        0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        free(refl_lower.float_data);
        free(refl_upper.float_data);
        return -1;
    }

    /* Read the upper reflectance dataset */
    if (read_dataset(info->detailed_results_cursor, variable_name_upper, harp_type_float, num_elements, refl_upper) !=
        0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        free(refl_lower.float_data);
        free(refl_upper.float_data);
        return -1;
    }

    /* Fill the final harp_array (2D: {time, spectral=2}) */
    for (long i = 0; i < num_elements; i++)
    {
        data.float_data[i] = refl_lower.float_data[i];  /* spectral index 0 */
        data.float_data[num_elements + i] = refl_upper.float_data[i];   /* spectral index 1 */
    }

    /* Clean up */
    free(refl_lower.float_data);
    free(refl_upper.float_data);

    return 0;
}


/* Read the **measured-reflectance precisions** for the two
 * wavelengths that form the Aerosol-Index pair.
 */
static int read_results_reflectance_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    const char *var_lo = NULL;
    const char *var_hi = NULL;

    long n = info->num_scanlines * info->num_pixels;

    /* 1) Map wavelength-ratio -> variable names */
    switch (info->wavelength_ratio)
    {
        case 354:
            var_lo = "reflectance_precision_354_measured";
            var_hi = "reflectance_precision_388_measured";
            break;
        case 340:
            var_lo = "reflectance_precision_340_measured";
            var_hi = "reflectance_precision_380_measured";
            break;
        case 335:
            var_lo = "reflectance_precision_335_measured";
            var_hi = "reflectance_precision_367_measured";
            break;
        default:
            harp_set_error(HARP_ERROR_INGESTION, "unknown wavelength_ratio %d", info->wavelength_ratio);
            return -1;
    }

    /* 2) Allocate temp buffers */
    harp_array prec_lo, prec_hi;

    prec_lo.float_data = malloc(n * sizeof(float));
    prec_hi.float_data = malloc(n * sizeof(float));
    if (!prec_lo.float_data || !prec_hi.float_data)
    {
        free(prec_lo.float_data);
        free(prec_hi.float_data);
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "allocating precision buffers (%ld elements)", n);
        return -1;
    }

    /* 3) Read datasets */
    if (read_dataset(info->detailed_results_cursor, var_lo, harp_type_float, n, prec_lo) != 0 ||
        read_dataset(info->detailed_results_cursor, var_hi, harp_type_float, n, prec_hi) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        free(prec_lo.float_data);
        free(prec_hi.float_data);
        return -1;      /* read_dataset() already set an error */
    }

    /* 4) Interleave into output {time, spectral=2} */
    for (long i = 0; i < n; i++)
    {
        data.float_data[i] = prec_lo.float_data[i];     /* lambda_low  */
        data.float_data[n + i] = prec_hi.float_data[i]; /* lambda_high */
    }

    free(prec_lo.float_data);
    free(prec_hi.float_data);
    return 0;
}


static int read_co_column_number_density_avk(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long dimension[2];

    if (read_dataset(info->detailed_results_cursor, "carbon_monoxide_total_column_averaging_kernel", harp_type_float,
                     info->num_scanlines * info->num_pixels * info->num_layers, data) != 0)
    {
        return -1;
    }
    dimension[0] = info->num_scanlines * info->num_pixels;
    dimension[1] = info->num_layers;

    return harp_array_invert(harp_type_float, 1, 2, dimension, data);
}

static int read_results_ch4_total_column_avk(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long dimension[2];

    if (read_dataset(info->detailed_results_cursor, "methane_total_column_averaging_kernel", harp_type_float,
                     info->num_scanlines * info->num_pixels * info->num_layers, data) != 0)
    {
        return -1;
    }
    dimension[0] = info->num_scanlines * info->num_pixels;
    dimension[1] = info->num_layers;

    return harp_array_invert(harp_type_float, 1, 2, dimension, data);
}

static int read_results_cloud_centre_height(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "cloud_centre_height", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_results_cloud_optical_depth(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "cloud_optical_depth", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_results_surface_albedo(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    const char *variable_name = NULL;

    if (info->product_type == s5_type_aui)
    {
        switch (info->wavelength_ratio)
        {
            case 354:
                variable_name = "scene_albedo_388";     /* for 354_388nm (default) */
                break;
            case 340:
                variable_name = "scene_albedo_380";     /* for 340_380nm */
                break;
            case 335:
                variable_name = "scene_albedo_367";     /* for 335_367nm */
                break;
            default:
                assert(0);
                exit(1);
        }

        return read_dataset(info->detailed_results_cursor, variable_name, harp_type_float,
                            info->num_scanlines * info->num_pixels, data);
    }
    else if (info->product_type == s5_type_ch4)
    {

        switch (info->use_ch4_band_options)
        {
            case 0:
                variable_name = "surface_albedo_swir_1";        /* default */
                break;
            case 1:
                variable_name = "surface_albedo_swir_3";
                break;
            case 2:
                variable_name = "surface_albedo_nir_2";
                break;
            default:
                assert(0);
                exit(1);
        }

        return read_dataset(info->detailed_results_cursor, variable_name, harp_type_float,
                            info->num_scanlines * info->num_pixels, data);

    }
    else if (info->product_type == s5_type_no2)
    {
        variable_name = "surface_albedo";
        return read_dataset(info->input_data_cursor, variable_name, harp_type_float,
                            info->num_scanlines * info->num_pixels, data);
    }
    else if (info->product_type == s5_type_o3)
    {
        variable_name = "surface_albedo_335";
        return read_dataset(info->input_data_cursor, variable_name, harp_type_float,
                            info->num_scanlines * info->num_pixels, data);
    }
    else if (info->product_type == s5_type_so2)
    {
        variable_name = "surface_albedo";
        return read_dataset(info->input_data_cursor, variable_name, harp_type_float,
                            info->num_scanlines * info->num_pixels, data);
    }
    else if (info->product_type == s5_type_co)
    {
        variable_name = "surface_albedo";
        return read_dataset(info->detailed_results_cursor, variable_name, harp_type_float,
                            info->num_scanlines * info->num_pixels, data);
    }

    harp_set_error(HARP_ERROR_CODA, NULL);
    return -1;

}


static int read_results_methane_total_column_prefit(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "methane_total_column_prefit", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}


static int read_results_methane_profile_apriori(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long dimension[2];

    if (read_dataset(info->detailed_results_cursor, "methane_profile_apriori", harp_type_float,
                     info->num_scanlines * info->num_pixels * info->num_layers, data) != 0)
    {
        return -1;
    }
    dimension[0] = info->num_scanlines * info->num_pixels;
    dimension[1] = info->num_layers;
    return harp_array_invert(harp_type_float, 1, 2, dimension, data);
}


static int read_results_carbon_monoxide_profile_apriori(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long dimension[2];

    if (read_dataset(info->detailed_results_cursor, "carbon_monoxide_profile_apriori", harp_type_float,
                     info->num_scanlines * info->num_pixels * info->num_layers, data) != 0)
    {
        return -1;
    }

    dimension[0] = info->num_scanlines * info->num_pixels;
    dimension[1] = info->num_layers;
    return harp_array_invert(harp_type_float, 1, 2, dimension, data);
}


static int read_results_carbon_dioxide_profile_apriori(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long dimension[2];

    if (read_dataset(info->detailed_results_cursor, "carbon_dioxide_profile_apriori", harp_type_float,
                     info->num_scanlines * info->num_pixels * info->num_layers, data) != 0)
    {
        return -1;
    }

    dimension[0] = info->num_scanlines * info->num_pixels;
    dimension[1] = info->num_layers;
    return harp_array_invert(harp_type_float, 1, 2, dimension, data);
}


static int read_results_oxygen_total_column_apriori(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "oxygen_total_column_apriori", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_results_water_total_column_apriori(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "water_total_column_apriori", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_results_dry_air_column(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "dry_air_column", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_results_solar_induced_fluorescence(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (read_dataset(info->detailed_results_cursor, "solar_induced_fluorescence", harp_type_float,
                     info->num_scanlines * info->num_pixels * info->num_spectral, data) != 0)
    {
        return -1;
    }

    return 0;
}

static int read_results_pressure(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long dimension[2];

    if (read_dataset(info->detailed_results_cursor, "pressure", harp_type_float,
                     info->num_scanlines * info->num_pixels * info->num_layers, data) != 0)
    {
        return -1;
    }
    dimension[0] = info->num_scanlines * info->num_pixels;
    dimension[1] = info->num_layers;
    return harp_array_invert(harp_type_float, 1, 2, dimension, data);
}

static int read_results_altitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long dimension[2];

    if (read_dataset(info->detailed_results_cursor, "altitude", harp_type_float,
                     info->num_scanlines * info->num_pixels * info->num_layers, data) != 0)
    {
        return -1;
    }
    dimension[0] = info->num_scanlines * info->num_pixels;
    dimension[1] = info->num_layers;
    return harp_array_invert(harp_type_float, 1, 2, dimension, data);
}


static int read_results_aerosol_size(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "aerosol_size", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}


static int read_results_aerosol_particle_column(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "aerosol_particle_column", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_results_aerosol_layer_height(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "aerosol_layer_height", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_results_nitrogen_dioxide_stratospheric_column_amf(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor,
                        "nitrogen_dioxide_stratospheric_column_air_mass_factor",
                        harp_type_float, info->num_scanlines * info->num_pixels, data);
}

static int read_results_cloud_radiance_fraction(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor,
                        "cloud_radiance_fraction", harp_type_float, info->num_scanlines * info->num_pixels, data);
}


static int read_results_nitrogen_dioxide_slant_column(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor,
                        "nitrogen_dioxide_slant_column", harp_type_float, info->num_scanlines * info->num_pixels, data);
}


static int read_results_nitrogen_dioxide_slant_column_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor,
                        "nitrogen_dioxide_slant_column_uncertainty",
                        harp_type_float, info->num_scanlines * info->num_pixels, data);
}


static int read_results_ozone_slant_column(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor,
                        "ozone_slant_column", harp_type_float, info->num_scanlines * info->num_pixels, data);
}


static int read_results_ozone_slant_column_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->product_type == s5_type_o3)
    {
        return read_dataset(info->detailed_results_cursor,
                            "ozone_slant_column_precision",
                            harp_type_float, info->num_scanlines * info->num_pixels, data);
    }
    else
    {
        return read_dataset(info->detailed_results_cursor,
                            "ozone_slant_column_uncertainty",
                            harp_type_float, info->num_scanlines * info->num_pixels, data);
    }

    harp_set_error(HARP_ERROR_CODA, NULL);
    return -1;
}

static int read_results_water_vapor_slant_column(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor,
                        "water_vapor_slant_column", harp_type_float, info->num_scanlines * info->num_pixels, data);
}

static int read_results_water_vapor_slant_column_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor,
                        "water_vapor_slant_column_uncertainty",
                        harp_type_float, info->num_scanlines * info->num_pixels, data);
}

static int read_results_water_liquid_slant_column(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor,
                        "water_liquid_slant_column", harp_type_float, info->num_scanlines * info->num_pixels, data);
}

static int read_results_nitrogen_dioxide_stratospheric_column(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor,
                        "nitrogen_dioxide_stratospheric_column",
                        harp_type_float, info->num_scanlines * info->num_pixels, data);
}


static int read_results_nitrogen_dioxide_stratospheric_column_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor,
                        "nitrogen_dioxide_stratospheric_column_uncertainty",
                        harp_type_float, info->num_scanlines * info->num_pixels, data);
}

static int read_results_nitrogen_dioxide_total_column(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    const char *variable_name = NULL;

    if (info->product_type == s5_type_no2)
    {
        switch (info->no2_column_option)
        {
            case 0:
                variable_name = "nitrogen_dioxide_total_column";
                break;
            case 1:
                variable_name = "nitrogen_dioxide_summed_total_column";
                break;
            default:
                assert(0);
                exit(1);
        }

        return read_dataset(info->detailed_results_cursor,
                            variable_name, harp_type_float, info->num_scanlines * info->num_pixels, data);
    }

    harp_set_error(HARP_ERROR_CODA, NULL);
    return -1;
}

static int read_results_nitrogen_dioxide_total_column_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    const char *variable_name = NULL;

    if (info->product_type == s5_type_no2)
    {
        switch (info->no2_column_option)
        {
            case 0:
                variable_name = "nitrogen_dioxide_total_column_uncertainty";
                break;
            case 1:
                variable_name = "nitrogen_dioxide_summed_total_column_uncertainty";
                break;
            default:
                assert(0);
                exit(1);
        }

        return read_dataset(info->detailed_results_cursor,
                            variable_name, harp_type_float, info->num_scanlines * info->num_pixels, data);
    }

    harp_set_error(HARP_ERROR_CODA, NULL);
    return -1;
}

static int read_results_effective_temperature(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor,
                        "effective_temperature", harp_type_float, info->num_scanlines * info->num_pixels, data);
}

static int read_results_effective_scene_amf(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor,
                        "effective_scene_air_mass_factor",
                        harp_type_float, info->num_scanlines * info->num_pixels, data);
}

static int read_results_effective_scene_albedo(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor,
                        "effective_scene_albedo", harp_type_float, info->num_scanlines * info->num_pixels, data);
}

static int read_results_ozone_total_column_avk(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long dimension[2];

    if (read_dataset(info->detailed_results_cursor,
                     "ozone_total_column_averaging_kernel", harp_type_float,
                     info->num_scanlines * info->num_pixels * info->num_layers, data) != 0)
    {
        return -1;
    }

    dimension[0] = info->num_scanlines * info->num_pixels;
    dimension[1] = info->num_layers;

    return harp_array_invert(harp_type_float, 1, 2, dimension, data);
}

static int read_results_ozone_profile_apriori(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long dimension[2];

    if (read_dataset(info->detailed_results_cursor,
                     "ozone_profile_apriori", harp_type_float,
                     info->num_scanlines * info->num_pixels * info->num_layers, data) != 0)
    {
        return -1;
    }

    dimension[0] = info->num_scanlines * info->num_pixels;
    dimension[1] = info->num_layers;

    return harp_array_invert(harp_type_float, 1, 2, dimension, data);
}

static int read_results_pressure_grid(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long dimension[2];

    if (read_dataset(info->detailed_results_cursor,
                     "pressure_grid", harp_type_float,
                     info->num_scanlines * info->num_pixels * info->num_layers, data) != 0)
    {
        return -1;
    }

    dimension[0] = info->num_scanlines * info->num_pixels;
    dimension[1] = info->num_layers;

    return harp_array_invert(harp_type_float, 1, 2, dimension, data);
}

static int read_results_scene_albedo(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "scene_albedo", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}


static int read_results_scene_albedo_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "scene_albedo_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_results_scene_pressure(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "scene_pressure", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}


static int read_results_scene_pressure_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "scene_pressure_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_results_scene_height(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "scene_height", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}


static int read_results_scene_height_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "scene_height_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_results_cloud_albedo(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "cloud_albedo", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}


static int read_results_cloud_albedo_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "cloud_albedo_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_results_sulfur_dioxide_slant_column(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "sulfur_dioxide_slant_column", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_results_sulfur_dioxide_slant_column_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "sulfur_dioxide_slant_column_precision", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_results_sulfur_dioxide_slant_column_trueness(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "sulfur_dioxide_slant_column_trueness", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_results_sulfur_dioxide_total_column_avk(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long dimension[2];

    if (read_dataset(info->detailed_results_cursor, "sulfur_dioxide_total_column_averaging_kernel", harp_type_float,
                     info->num_scanlines * info->num_pixels * info->num_layers, data) != 0)
    {
        return -1;
    }
    dimension[0] = info->num_scanlines * info->num_pixels;
    dimension[1] = info->num_layers;
    return harp_array_invert(harp_type_float, 1, 2, dimension, data);
}

static int read_results_sulfur_dioxide_layer_pressure(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "sulfur_dioxide_layer_pressure", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_results_sulfur_dioxide_layer_pressure_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->detailed_results_cursor, "sulfur_dioxide_layer_pressure_uncertainty", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

/* Field: data/PRODUCT/SUPPORT_DATA/GEOLOCATIONS */

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

static int read_geolocation_satellite_orbit_phase(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->geolocation_cursor, "satellite_orbit_phase", harp_type_double, info->num_scanlines, data);
}


static int read_geolocation_solar_zenith_angle(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->geolocation_cursor, "solar_zenith_angle", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_geolocation_solar_azimuth_angle(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->geolocation_cursor, "solar_azimuth_angle", harp_type_float,
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


/* Field: data/PRODUCT/SUPPORT_DATA/INPUT_DATA */
static int read_input_surface_altitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "surface_altitude", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_input_surface_altitude_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    const char *variable_name = NULL;

    if (info->product_type == s5_type_no2)
    {
        variable_name = "surface_altitude_uncertainty";
    }
    else
    {
        variable_name = "surface_altitude_precision";
    }
    return read_dataset(info->input_data_cursor, variable_name, harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_input_surface_pressure(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "surface_pressure", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_input_aerosol_index(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->product_type == s5_type_o3 || info->product_type == s5_type_so2)
    {
        return read_dataset(info->input_data_cursor, "aerosol_index_340_380", harp_type_float,
                            info->num_scanlines * info->num_pixels, data);
    }
    else
    {
        return read_dataset(info->input_data_cursor, "aerosol_index_354_388", harp_type_float,
                            info->num_scanlines * info->num_pixels, data);
    }
}

static int read_input_cloud_albedo(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "cloud_albedo", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_input_cloud_albedo_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "cloud_albedo_uncertainty", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_input_cloud_pressure(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "cloud_pressure", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_input_effective_cloud_fraction(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "effective_cloud_fraction", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_input_effective_cloud_fraction_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "effective_cloud_fraction_uncertainty", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_input_scene_albedo(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "scene_albedo", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_input_scene_albedo_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "scene_albedo_uncertainty", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_input_scene_pressure(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "scene_pressure", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_input_scene_pressure_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "scene_pressure_uncertainty", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_input_tropopause_pressure(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "tropopause_pressure", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_input_sulfur_dioxide_profile_apriori(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long dimension[2];

    if (read_dataset(info->input_data_cursor, "sulfur_dioxide_profile_apriori", harp_type_float,
                     info->num_scanlines * info->num_pixels * info->num_layers, data) != 0)
    {
        return -1;
    }

    dimension[0] = info->num_scanlines * info->num_pixels;
    dimension[1] = info->num_layers;
    return harp_array_invert(harp_type_float, 1, 2, dimension, data);
}

static int read_input_cloud_height(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "cloud_height", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

static int read_input_ozone_total_column(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "ozone_total_column", harp_type_float,
                        info->num_scanlines * info->num_pixels, data);
}

/* Helper function */
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

/* Helper function */
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

static int read_sea_ice_fraction(void *user_data, harp_array data)
{
    return read_sea_ice_fraction_from_flag(user_data, "snow_ice_flag", data);
}

/* Helper function */
static int read_no2_pressure_bounds(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    /* Dimensions */
    const long num_profiles = info->num_scanlines * info->num_pixels;   /* time dimension          */
    const long num_layers = info->num_layers;   /* 137 for S-5 simulated   */
    const long num_levels = num_layers + 1;     /* 138 level boundaries    */
    harp_array psurf;   /* surface pressure for every pixel */

    /* Temporary buffers for a, b and surface-pressure */
    harp_array coef_a;

    coef_a.ptr = malloc(num_levels * sizeof(double));
    if (coef_a.ptr == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "cannot allocate coef_a buffer");
        return -1;
    }

    harp_array coef_b;

    coef_b.ptr = malloc(num_levels * sizeof(double));
    if (coef_b.ptr == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "cannot allocate coef_b buffer");
        free(coef_a.ptr);
        return -1;
    }

    psurf.ptr = malloc(num_profiles * sizeof(double));
    if (psurf.ptr == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "cannot allocate surface-pressure buffer");
        free(coef_a.ptr);
        free(coef_b.ptr);
        return -1;
    }

    /* Read the three datasets */
    if (read_dataset(info->input_data_cursor, "pressure_coefficient_a",
                     harp_type_double, num_levels, coef_a) != 0 ||
        read_dataset(info->input_data_cursor, "pressure_coefficient_b",
                     harp_type_double, num_levels, coef_b) != 0 ||
        read_dataset(info->input_data_cursor, "surface_pressure", harp_type_double, num_profiles, psurf) != 0)
    {
        free(coef_a.ptr);
        free(coef_b.ptr);
        free(psurf.ptr);
        return -1;
    }

    /* Build the (layer, 2) pressure-bounds array */
    for (long p = 0; p < num_profiles; p++)
    {
        double *bounds = &data.double_data[p * num_layers * 2];
        double sp = psurf.double_data[p];

        for (long j = 0; j < num_layers; j++)
        {
            /* upper bound of layer j */
            bounds[j * 2] = coef_a.double_data[j] + coef_b.double_data[j] * sp;
            /* lower bound of layer j (= upper of j+1) */
            bounds[j * 2 + 1] = coef_a.double_data[j + 1] + coef_b.double_data[j + 1] * sp;
        }
        /* Clamp TOA pressure to >= 1 mPa */
        if (bounds[(num_layers - 1) * 2] < 1e-3)
            bounds[(num_layers - 1) * 2] = 1e-3;
    }

    free(coef_a.ptr);
    free(coef_b.ptr);
    free(psurf.ptr);
    return 0;
}

/* See: Table 4.3.4 - Surface classification flags in S5 L2 products and generated auxiliary data.
 * Source: EPS-SG S5 L2 PFS v4, 13-Jul-2023
 * Link: https://user.eumetsat.int/s3/eup-strapi-media/EPS_SG_Sentinel_5_Level_2_Product_Format_Specification_V4_1122b7fd75.pdf
 */
static int read_input_surface_classification(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->input_data_cursor, "surface_classification", harp_type_int8,      /* keep 0...255 codes */
                        info->num_scanlines * info->num_pixels, data);
}

/* 
 * Variables' Registration Routines 
 */

static void register_core_variables(harp_product_definition *product_definition, int delta_time_num_dims,
                                    int include_validity)
{
    const char *path;
    const char *description;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type_1d[1] = { harp_dimension_time };

    /* datetime_start */
    description = "Start time of the measurement.";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime_start", harp_type_double, 1,
                                                   dimension_type_1d, NULL, description, "seconds since 2010-01-01",
                                                   NULL, read_datetime);

    path = "/data/PRODUCT/time, /data/PRODUCT/delta_time[]";

    if (delta_time_num_dims == 2)
    {
        description = "time converted from milliseconds since a reference time"
            "(given as seconds since 2010-01-01) to " "seconds since"
            "2010-01-01 (using 86400 seconds per day); the time associated"
            "with a scanline is repeated " "for each pixel in the scanline";
    }
    else
    {
        description = "time converted from milliseconds since a reference time"
            "(given as seconds since 2010-01-01) to " "seconds since" "2010-01-01 (using 86400 seconds per day)";
    }

    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);


    /* orbit_index */
    description = "absolute orbit number";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "orbit_index", harp_type_int32, 0, NULL, NULL,
                                                   description, NULL, NULL, read_orbit_index);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/@orbit_start", NULL);


    if (include_validity)
    {
        /* validity */
        description = "processing quality flag";
        variable_definition =
            harp_ingestion_register_variable_full_read(product_definition, "validity", harp_type_int32, 1,
                                                       dimension_type_1d, NULL, description, NULL, NULL,
                                                       read_results_processing_quality_flags);
        path = "/data/PRODUCT/processing_quality_flags[]";
        description = "the uint64 data is cast to int32";
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);
    }
}

/* CLD product: core variables for BAND-3A / BAND-3C */
static void register_core_variables_cld(harp_product_definition *product_definition, int include_validity)
{
    const char *description;
    harp_variable_definition *var;
    harp_dimension_type dim_time[1] = { harp_dimension_time };

    /* datetime_start                                     */
    description = "Start time of the measurement.";
    var = harp_ingestion_register_variable_full_read(product_definition, "datetime_start", harp_type_double, 1, dim_time, NULL, description, "seconds since 2010-01-01", NULL,  /* no include-function */
                                                     read_datetime);

    /* two alternative paths, selected by the user option */
    harp_variable_definition_add_mapping(var, "band=band3a or band unset", NULL,
                                         "/data/PRODUCT_BAND3A/time, /data/PRODUCT_BAND3A/delta_time[]",
                                         "time converted from milliseconds since a reference time to "
                                         "seconds since 2010-01-01 (86400 s / day)");

    harp_variable_definition_add_mapping(var, "band=band3c", NULL,
                                         "/data/PRODUCT_BAND3C/time, /data/PRODUCT_BAND3C/delta_time[]",
                                         "as above but for BAND-3C");

    /* orbit_index                                        */
    description = "absolute orbit number";
    var = harp_ingestion_register_variable_full_read(product_definition,
                                                     "orbit_index", harp_type_int32,
                                                     0, NULL, NULL, description, NULL, NULL, read_orbit_index);
    harp_variable_definition_add_mapping(var, NULL, NULL, "/@orbit_start", NULL);

    /* validity */
    if (include_validity)
    {
        description = "processing quality flag";
        var = harp_ingestion_register_variable_full_read(product_definition,
                                                         "validity", harp_type_int32,
                                                         1, dim_time, NULL,
                                                         description, NULL, NULL,
                                                         read_results_processing_quality_flags);

        harp_variable_definition_add_mapping(var, "band=band3a or band unset", NULL,
                                             "/data/PRODUCT_BAND3A/processing_quality_flags[]",
                                             "the uint64 data is cast to int32");

        harp_variable_definition_add_mapping(var, "band=band3c", NULL,
                                             "/data/PRODUCT_BAND3C/processing_quality_flags[]",
                                             "the uint64 data is cast to int32");
    }
}



static void register_geolocation_variables(harp_product_definition *product_definition)
{
    const char *path;
    const char *description;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };

    /* latitude */
    description = "Latitude of the center of each ground pixel on the WGS84 reference ellipsoid.";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "latitude", harp_type_float, 1, dimension_type,
                                                   NULL, description, "degree_north", NULL, read_product_latitude);
    harp_variable_definition_set_valid_range_float(variable_definition, -90.0f, 90.0f);
    path = "/data/PRODUCT/SUPPORT_DATA/GEOLOCATIONS/latitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* longitude */
    description = "longitude of the ground pixel center (WGS84)";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "longitude", harp_type_float, 1, dimension_type,
                                                   NULL, description, "degree_east", NULL, read_product_longitude);
    harp_variable_definition_set_valid_range_float(variable_definition, -180.0f, 180.0f);
    path = "/data/PRODUCT/SUPPORT_DATA/GEOLOCATIONS/longitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

/* CLD product: geolocation (BAND-3A / BAND-3C) */
static void register_geolocation_variables_cld(harp_product_definition *product_definition)
{
    const char *description;
    harp_variable_definition *var;
    harp_dimension_type dim_time[1] = { harp_dimension_time };

    /* latitude */
    description = "Latitude of the ground-pixel centre (WGS-84).";
    var = harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_float, 1, dim_time, NULL, description, "degree_north", NULL,     /* include-function */
                                                     read_product_latitude);

    harp_variable_definition_set_valid_range_float(var, -90.0f, 90.0f);

    harp_variable_definition_add_mapping(var, "band=band3a or band unset", NULL,
                                         "/data/PRODUCT_BAND3A/SUPPORT_DATA/GEOLOCATIONS/latitude[]", NULL);

    harp_variable_definition_add_mapping(var, "band=band3c", NULL,
                                         "/data/PRODUCT_BAND3C/SUPPORT_DATA/GEOLOCATIONS/latitude[]", NULL);

    /* longitude */
    description = "Longitude of the ground-pixel centre (WGS-84).";
    var = harp_ingestion_register_variable_full_read(product_definition,
                                                     "longitude", harp_type_float,
                                                     1, dim_time, NULL,
                                                     description, "degree_east", NULL, read_product_longitude);

    harp_variable_definition_set_valid_range_float(var, -180.0f, 180.0f);

    harp_variable_definition_add_mapping(var, "band=band3a or band unset", NULL,
                                         "/data/PRODUCT_BAND3A/SUPPORT_DATA/GEOLOCATIONS/longitude[]", NULL);

    harp_variable_definition_add_mapping(var, "band=band3c", NULL,
                                         "/data/PRODUCT_BAND3C/SUPPORT_DATA/GEOLOCATIONS/longitude[]", NULL);
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
    description = "The four latitude boundaries of each ground pixel.";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "latitude_bounds", harp_type_float, 2,
                                                   bounds_dimension_type, bounds_dimension,
                                                   description, "degree_north", NULL, read_geolocation_latitude_bounds);
    harp_variable_definition_set_valid_range_float(variable_definition, -90.0f, 90.0f);
    path = "/data/PRODUCT/SUPPORT_DATA/GEOLOCATIONS/latitude_bounds[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* longitude_bounds */
    description = "The four longitude boundaries of each ground pixel.";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "longitude_bounds", harp_type_float, 2,
                                                   bounds_dimension_type, bounds_dimension,
                                                   description, "degree_east", NULL, read_geolocation_longitude_bounds);
    harp_variable_definition_set_valid_range_float(variable_definition, -180.0f, 180.0f);
    path = "/data/PRODUCT/SUPPORT_DATA/GEOLOCATIONS/longitude_bounds[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* sensor_latitude */
    description = "Latitude of the spacecraft sub-satellite point on the WGS84 reference ellipsoid.";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "sensor_latitude",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     description, "degree_north", NULL,
                                                                     read_geolocation_satellite_latitude);
    harp_variable_definition_set_valid_range_float(variable_definition, -90.0f, 90.0f);
    path = "/data/PRODUCT/SUPPORT_DATA/GEOLOCATIONS/satellite_latitude[]";
    description = "the satellite latitude associated with a scanline is repeated for each pixel in the scanline";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* sensor_longitude */
    description = "Longitude of the spacecraft sub-satellite point on the WGS84 reference ellipsoid.";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "sensor_longitude",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     description, "degree_east", NULL,
                                                                     read_geolocation_satellite_longitude);
    harp_variable_definition_set_valid_range_float(variable_definition, -180.0f, 180.0f);
    path = "/data/PRODUCT/SUPPORT_DATA/GEOLOCATIONS/satellite_longitude[]";
    description = "the satellite longitude associated with a scanline is repeated for each pixel in the scanline";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* sensor_altitude */
    description = "Altitude of the spacecraft relative to the WGS84 reference ellipsoid.";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "sensor_altitude",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     description, "m", NULL,
                                                                     read_geolocation_satellite_altitude);
    harp_variable_definition_set_valid_range_float(variable_definition, 700000.0f, 900000.0f);
    path = "/data/PRODUCT/SUPPORT_DATA/GEOLOCATIONS/satellite_altitude[]";
    description = "the satellite altitude associated with a scanline is repeated for each pixel in the scanline";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* sensor_orbit_phase */
    description = "Relative offset (0.0 ... 1.0) of the measurement in the orbit.";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "sensor_orbit_phase",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                                     read_geolocation_satellite_orbit_phase);
    path = "/data/PRODUCT/SUPPORT_DATA/GEOLOCATIONS/satellite_orbit_phase[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* solar_zenith_angle */
    description = "Zenith angle of the sun measured from the ground pixel location on the WGS84 reference ellipsoid.";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "solar_zenith_angle",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     description, "degree", NULL,
                                                                     read_geolocation_solar_zenith_angle);
    harp_variable_definition_set_valid_range_float(variable_definition, 0.0f, 180.0f);
    path = "/data/PRODUCT/SUPPORT_DATA/GEOLOCATIONS/solar_zenith_angle[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* solar_azimuth_angle */
    description = "Azimuth angle of the sun measured from the ground pixel location on the WGS84 ellipsoid.";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "solar_azimuth_angle",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     description, "degree", NULL,
                                                                     read_geolocation_solar_azimuth_angle);
    harp_variable_definition_set_valid_range_float(variable_definition, -180.0f, 180.0f);
    path = "/data/PRODUCT/SUPPORT_DATA/GEOLOCATIONS/solar_azimuth_angle[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* sensor_zenith_angle */
    description =
        "Zenith angle of the spacecraft measured from the ground pixel location on the WGS84 reference ellipsoid.";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "sensor_zenith_angle", harp_type_float, 1,
                                                   dimension_type, NULL, description, "degree", NULL,
                                                   read_geolocation_viewing_zenith_angle);
    harp_variable_definition_set_valid_range_float(variable_definition, 0.0f, 180.0f);
    path = "/data/PRODUCT/SUPPORT_DATA/GEOLOCATIONS/viewing_zenith_angle[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* sensor_azimuth_angle */
    description = "Azimuth angle of the spacecraft measured from the ground pixel WGS84 reference ellipsoid.";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "sensor_azimuth_angle",
                                                   harp_type_float, 1, dimension_type, NULL,
                                                   description, "degree", NULL, read_geolocation_viewing_azimuth_angle);
    harp_variable_definition_set_valid_range_float(variable_definition, -180.0f, 180.0f);
    path = "/data/PRODUCT/SUPPORT_DATA/GEOLOCATIONS/viewing_azimuth_angle[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

/*  CLD product: full geolocation set (BAND-3A / BAND-3C) */
static void register_additional_geolocation_variables_cld(harp_product_definition *pd)
{
    /* common helpers */
    harp_dimension_type t1[1] = { harp_dimension_time };
    harp_dimension_type t2[2] = { harp_dimension_time,
        harp_dimension_independent
    };
    long sz2[2] = { -1, 4 };    /* {time, corner=4} */

    const char *descr;
    harp_variable_definition *var;
    const char *path_a; /* PRODUCT_BAND3A ... */
    const char *path_c; /* PRODUCT_BAND3C ... */

    /* latitude_bounds (time, corner) */
    descr = "Four latitude boundaries of each ground pixel.";
    var = harp_ingestion_register_variable_full_read(pd, "latitude_bounds", harp_type_float,
                                                     2, t2, sz2, descr, "degree_north",
                                                     NULL, read_geolocation_latitude_bounds);

    harp_variable_definition_set_valid_range_float(var, -90.f, 90.f);

    path_a = "/data/PRODUCT_BAND3A/SUPPORT_DATA/GEOLOCATIONS/latitude_bounds[]";
    path_c = "/data/PRODUCT_BAND3C/SUPPORT_DATA/GEOLOCATIONS/latitude_bounds[]";

    harp_variable_definition_add_mapping(var, "band=band3a or band unset", NULL, path_a, NULL);
    harp_variable_definition_add_mapping(var, "band=band3c", NULL, path_c, NULL);

    /* longitude_bounds (time, corner) */
    descr = "Four longitude boundaries of each ground pixel.";
    var = harp_ingestion_register_variable_full_read(pd, "longitude_bounds", harp_type_float,
                                                     2, t2, sz2, descr, "degree_east",
                                                     NULL, read_geolocation_longitude_bounds);

    harp_variable_definition_set_valid_range_float(var, -180.f, 180.f);

    path_a = "/data/PRODUCT_BAND3A/SUPPORT_DATA/GEOLOCATIONS/longitude_bounds[]";
    path_c = "/data/PRODUCT_BAND3C/SUPPORT_DATA/GEOLOCATIONS/longitude_bounds[]";

    harp_variable_definition_add_mapping(var, "band=band3a or band unset", NULL, path_a, NULL);
    harp_variable_definition_add_mapping(var, "band=band3c", NULL, path_c, NULL);

    /* sensor_latitude (scalar) */
    descr = "Sub-satellite latitude.";
    var = harp_ingestion_register_variable_full_read(pd, "sensor_latitude", harp_type_float,
                                                     1, t1, NULL, descr, "degree_north", NULL,
                                                     read_geolocation_satellite_latitude);

    harp_variable_definition_set_valid_range_float(var, -90.f, 90.f);

    path_a = "/data/PRODUCT_BAND3A/SUPPORT_DATA/GEOLOCATIONS/satellite_latitude[]";
    path_c = "/data/PRODUCT_BAND3C/SUPPORT_DATA/GEOLOCATIONS/satellite_latitude[]";

    harp_variable_definition_add_mapping(var, "band=band3a or band unset",
                                         NULL, path_a, "value for each scanline is repeated for every pixel");
    harp_variable_definition_add_mapping(var, "band=band3c",
                                         NULL, path_c, "value for each scanline is repeated for every pixel");

    /* sensor_longitude (scalar) */
    descr = "Sub-satellite longitude.";
    var = harp_ingestion_register_variable_full_read(pd, "sensor_longitude", harp_type_float,
                                                     1, t1, NULL, descr, "degree_east", NULL,
                                                     read_geolocation_satellite_longitude);

    harp_variable_definition_set_valid_range_float(var, -180.f, 180.f);

    path_a = "/data/PRODUCT_BAND3A/SUPPORT_DATA/GEOLOCATIONS/satellite_longitude[]";
    path_c = "/data/PRODUCT_BAND3C/SUPPORT_DATA/GEOLOCATIONS/satellite_longitude[]";

    harp_variable_definition_add_mapping(var, "band=band3a or band unset",
                                         NULL, path_a, "value for each scanline is repeated for every pixel");
    harp_variable_definition_add_mapping(var, "band=band3c",
                                         NULL, path_c, "value for each scanline is repeated for every pixel");

    /* sensor_altitude (scalar) */
    descr = "Space-craft altitude (WGS-84).";
    var = harp_ingestion_register_variable_full_read(pd, "sensor_altitude", harp_type_float,
                                                     1, t1, NULL, descr, "m", NULL,
                                                     read_geolocation_satellite_altitude);

    harp_variable_definition_set_valid_range_float(var, 700000.f, 900000.f);

    path_a = "/data/PRODUCT_BAND3A/SUPPORT_DATA/GEOLOCATIONS/satellite_altitude[]";
    path_c = "/data/PRODUCT_BAND3C/SUPPORT_DATA/GEOLOCATIONS/satellite_altitude[]";

    harp_variable_definition_add_mapping(var, "band=band3a or band unset",
                                         NULL, path_a, "value for each scanline is repeated for every pixel");
    harp_variable_definition_add_mapping(var, "band=band3c",
                                         NULL, path_c, "value for each scanline is repeated for every pixel");

    /* sensor_orbit_phase (scalar, double) */
    descr = "Relative orbital phase (0 ... 1).";
    var = harp_ingestion_register_variable_full_read(pd, "sensor_orbit_phase", harp_type_double,
                                                     1, t1, NULL, descr, HARP_UNIT_DIMENSIONLESS, NULL,
                                                     read_geolocation_satellite_orbit_phase);

    path_a = "/data/PRODUCT_BAND3A/SUPPORT_DATA/GEOLOCATIONS/satellite_orbit_phase[]";
    path_c = "/data/PRODUCT_BAND3C/SUPPORT_DATA/GEOLOCATIONS/satellite_orbit_phase[]";

    harp_variable_definition_add_mapping(var, "band=band3a or band unset", NULL, path_a, NULL);
    harp_variable_definition_add_mapping(var, "band=band3c", NULL, path_c, NULL);

    /* solar_zenith_angle (scalar) */
    descr = "Solar zenith angle.";
    var = harp_ingestion_register_variable_full_read(pd, "solar_zenith_angle", harp_type_float,
                                                     1, t1, NULL, descr, "degree", NULL,
                                                     read_geolocation_solar_zenith_angle);

    harp_variable_definition_set_valid_range_float(var, 0.f, 180.f);

    path_a = "/data/PRODUCT_BAND3A/SUPPORT_DATA/GEOLOCATIONS/solar_zenith_angle[]";
    path_c = "/data/PRODUCT_BAND3C/SUPPORT_DATA/GEOLOCATIONS/solar_zenith_angle[]";

    harp_variable_definition_add_mapping(var, "band=band3a or band unset", NULL, path_a, NULL);
    harp_variable_definition_add_mapping(var, "band=band3c", NULL, path_c, NULL);

    /* solar_azimuth_angle (scalar) */
    descr = "Solar azimuth angle.";
    var = harp_ingestion_register_variable_full_read(pd, "solar_azimuth_angle", harp_type_float,
                                                     1, t1, NULL, descr, "degree", NULL,
                                                     read_geolocation_solar_azimuth_angle);

    harp_variable_definition_set_valid_range_float(var, -180.f, 180.f);

    path_a = "/data/PRODUCT_BAND3A/SUPPORT_DATA/GEOLOCATIONS/solar_azimuth_angle[]";
    path_c = "/data/PRODUCT_BAND3C/SUPPORT_DATA/GEOLOCATIONS/solar_azimuth_angle[]";

    harp_variable_definition_add_mapping(var, "band=band3a or band unset", NULL, path_a, NULL);
    harp_variable_definition_add_mapping(var, "band=band3c", NULL, path_c, NULL);

    /* sensor_zenith_angle (scalar) */
    descr = "Space-craft zenith angle.";
    var = harp_ingestion_register_variable_full_read(pd, "sensor_zenith_angle", harp_type_float,
                                                     1, t1, NULL, descr, "degree", NULL,
                                                     read_geolocation_viewing_zenith_angle);

    harp_variable_definition_set_valid_range_float(var, 0.f, 180.f);

    path_a = "/data/PRODUCT_BAND3A/SUPPORT_DATA/GEOLOCATIONS/viewing_zenith_angle[]";
    path_c = "/data/PRODUCT_BAND3C/SUPPORT_DATA/GEOLOCATIONS/viewing_zenith_angle[]";

    harp_variable_definition_add_mapping(var, "band=band3a or band unset", NULL, path_a, NULL);
    harp_variable_definition_add_mapping(var, "band=band3c", NULL, path_c, NULL);

    /* sensor_azimuth_angle (scalar) */
    descr = "Space-craft azimuth angle.";
    var = harp_ingestion_register_variable_full_read(pd, "sensor_azimuth_angle", harp_type_float,
                                                     1, t1, NULL, descr, "degree", NULL,
                                                     read_geolocation_viewing_azimuth_angle);

    harp_variable_definition_set_valid_range_float(var, -180.f, 180.f);

    path_a = "/data/PRODUCT_BAND3A/SUPPORT_DATA/GEOLOCATIONS/viewing_azimuth_angle[]";
    path_c = "/data/PRODUCT_BAND3C/SUPPORT_DATA/GEOLOCATIONS/viewing_azimuth_angle[]";

    harp_variable_definition_add_mapping(var, "band=band3a or band unset", NULL, path_a, NULL);
    harp_variable_definition_add_mapping(var, "band=band3c", NULL, path_c, NULL);
}





static void register_surface_variables(harp_product_definition *product_definition, const char *product_type)
{

    const char *path;
    const char *description;

    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type_1d[1] = { harp_dimension_time };

    /* surface_altitude */
    description = "Height of the surface above WGS84 ellipsoid averaged over the S5 pixel.";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "surface_altitude", harp_type_float, 1, dimension_type_1d,
                                                   NULL, description, "m", NULL, read_input_surface_altitude);
    path = "/data/PRODUCT/SUPPORT_DATA/INPUT_DATA/surface_altitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);


    /* surface_altitude_uncertainty */
    /* [Note]: O3 does not contain this record */
    if (strcmp(product_type, "SN5_02_O3") != 0)
    {
        description = "Standard deviation of the height of the surface above WGS84 "
            "ellipsoid averaged over the S5 pixel";
        variable_definition =
            harp_ingestion_register_variable_full_read(product_definition, "surface_altitude_uncertainty",
                                                       harp_type_float, 1, dimension_type_1d, NULL, description,
                                                       "m", NULL, read_input_surface_altitude_precision);
    }

    if (strcmp(product_type, "SN5_02_NO2") == 0)
    {
        path = "data/PRODUCT/SUPPORT_DATA/INPUT_DATA/surface_altitude_uncertainty[]";
    }
    else
    {
        path = "data/PRODUCT/SUPPORT_DATA/INPUT_DATA/surface_altitude_precision[]";
    }

    if (strcmp(product_type, "SN5_02_O3") != 0)
    {
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
    }

    /* surface_pressure */
    description = "Surface pressure. From ECMWF and adjusted for surface elevation.";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "surface_pressure",
                                                   harp_type_float, 1, dimension_type_1d, NULL, description, "Pa", NULL,
                                                   read_input_surface_pressure);
    path = "data/PRODUCT/SUPPORT_DATA/INPUT_DATA/surface_pressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* surface_classification */
    description = "Surface classification.";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "surface_type",        /* HARP long-name */
                                                                     harp_type_int32,   /* store as int32 */
                                                                     1, dimension_type_1d, NULL,
                                                                     description, NULL, NULL,
                                                                     read_input_surface_classification);

    path = "/data/PRODUCT/SUPPORT_DATA/INPUT_DATA/surface_classification[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

/* CLD product - surface variables (BAND-3A / BAND-3C) */
static void register_surface_variables_cld(harp_product_definition *pd)
{
    harp_dimension_type t1[1] = { harp_dimension_time };
    const char *descr;
    harp_variable_definition *var;

    /* surface_altitude */
    descr = "Height of the surface above the WGS-84 ellipsoid averaged " "over the Sentinel-5 ground pixel.";
    var = harp_ingestion_register_variable_full_read(pd, "surface_altitude", harp_type_float,
                                                     1, t1, NULL, descr, "m", NULL, read_input_surface_altitude);

    /* BAND-3A (default / option unset) */
    harp_variable_definition_add_mapping(var, "band=band3a or band unset", NULL,
                                         "/data/PRODUCT_BAND3A/SUPPORT_DATA/INPUT_DATA/surface_altitude[]", NULL);

    /* BAND-3C */
    harp_variable_definition_add_mapping(var, "band=band3c", NULL,
                                         "/data/PRODUCT_BAND3C/SUPPORT_DATA/INPUT_DATA/surface_altitude[]", NULL);

    /* surface_altitude_uncertainty  (file name: surface_altitude_precision) */
    descr = "1-sigma uncertainty of the surface altitude.";
    var = harp_ingestion_register_variable_full_read(pd, "surface_altitude_uncertainty", harp_type_float,
                                                     1, t1, NULL, descr, "m", NULL,
                                                     read_input_surface_altitude_precision);

    harp_variable_definition_add_mapping(var, "band=band3a or band unset", NULL,
                                         "/data/PRODUCT_BAND3A/SUPPORT_DATA/INPUT_DATA/"
                                         "surface_altitude_precision[]", NULL);

    harp_variable_definition_add_mapping(var, "band=band3c", NULL,
                                         "/data/PRODUCT_BAND3C/SUPPORT_DATA/INPUT_DATA/"
                                         "surface_altitude_precision[]", NULL);

    /* surface_pressure */
    descr = "Surface pressure from ECMWF, adjusted for surface elevation.";
    var = harp_ingestion_register_variable_full_read(pd, "surface_pressure", harp_type_float,
                                                     1, t1, NULL, descr, "Pa", NULL, read_input_surface_pressure);

    harp_variable_definition_add_mapping(var, "band=band3a or band unset", NULL,
                                         "/data/PRODUCT_BAND3A/SUPPORT_DATA/INPUT_DATA/surface_pressure[]", NULL);

    harp_variable_definition_add_mapping(var, "band=band3c", NULL,
                                         "/data/PRODUCT_BAND3C/SUPPORT_DATA/INPUT_DATA/surface_pressure[]", NULL);
}

static void register_snow_ice_flag_variables(harp_product_definition *product_definition, const char *product_type)
{
    const char *path;
    const char *description;
    const char *mapping_condition = NULL;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };
    int (*read_snow_ice_type_function)(void *, harp_array);
    int (*read_sea_ice_fraction_function)(void *, harp_array);
    int (*condition_function)(void *) = NULL;

    read_snow_ice_type_function = read_snow_ice_type;
    read_sea_ice_fraction_function = read_sea_ice_fraction;

    if (strcmp(product_type, "SN5_02_CLD") != 0)
    {
        /* snow_ice_type */
        description = "Surface condition (snow/ice).";
        variable_definition =
            harp_ingestion_register_variable_full_read(product_definition, "snow_ice_type", harp_type_int32, 1,
                                                       dimension_type, NULL, description, NULL, condition_function,
                                                       read_snow_ice_type_function);
        harp_variable_definition_set_enumeration_values(variable_definition, 5, snow_ice_type_values);
        description = "0: snow_free_land (0), 1-100: sea_ice (1), 101: permanent_ice (2), "
            "103: snow (3), 255: ocean (4), other values map to -1";

        /* BAND-3A (default / option unset) */
        harp_variable_definition_add_mapping(variable_definition, "band=band3a or band unset", mapping_condition,
                                             "/data/PRODUCT_BAND3A/SUPPORT_DATA/INPUT_DATA/snow_ice_flag[]",
                                             description);

        /* BAND-3C */
        harp_variable_definition_add_mapping(variable_definition, "band=band3c", mapping_condition,
                                             "/data/PRODUCT_BAND3C/SUPPORT_DATA/INPUT_DATA/snow_ice_flag[]",
                                             description);

        /* sea_ice_fraction */
        description = "Sea-ice concentration (as a fraction)";
        variable_definition =
            harp_ingestion_register_variable_full_read(product_definition, "sea_ice_fraction", harp_type_float, 1,
                                                       dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS,
                                                       condition_function, read_sea_ice_fraction_function);
        description = "if 1 <= snow_ice_flag <= 100 then snow_ice_flag/100.0 else 0.0";

        /* BAND-3A (default / option unset) */
        harp_variable_definition_add_mapping(variable_definition, "band=band3a or band unset", mapping_condition,
                                             "/data/PRODUCT_BAND3A/SUPPORT_DATA/INPUT_DATA/snow_ice_flag[]",
                                             description);

        /* BAND-3C */
        harp_variable_definition_add_mapping(variable_definition, "band=band3c", mapping_condition,
                                             "/data/PRODUCT_BAND3C/SUPPORT_DATA/INPUT_DATA/snow_ice_flag[]",
                                             description);
    }
    else
    {
        path = "data/PRODUCT/SUPPORT_DATA/INPUT_DATA/snow_ice_flag[]";
        read_snow_ice_type_function = read_snow_ice_type;
        read_sea_ice_fraction_function = read_sea_ice_fraction;

        /* snow_ice_type */
        description = "Surface condition (snow/ice).";
        variable_definition =
            harp_ingestion_register_variable_full_read(product_definition, "snow_ice_type", harp_type_int32, 1,
                                                       dimension_type, NULL, description, NULL, condition_function,
                                                       read_snow_ice_type_function);
        harp_variable_definition_set_enumeration_values(variable_definition, 5, snow_ice_type_values);
        description =
            "0: snow_free_land (0), 1-100: sea_ice (1), 101: permanent_ice (2), 103: snow (3), 255: ocean (4), "
            "other values map to -1";
        harp_variable_definition_add_mapping(variable_definition, NULL, mapping_condition, path, description);

        /* sea_ice_fraction */
        description = "Sea-ice concentration (as a fraction)";
        variable_definition =
            harp_ingestion_register_variable_full_read(product_definition, "sea_ice_fraction", harp_type_float, 1,
                                                       dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS,
                                                       condition_function, read_sea_ice_fraction_function);
        description = "if 1 <= snow_ice_flag <= 100 then snow_ice_flag/100.0 else 0.0";
        harp_variable_definition_add_mapping(variable_definition, NULL, mapping_condition, path, description);
    }
}


/* 
 * Product Registration Routines 
 */


/* Aerosol */
static void register_aui_product(void)
{
    const char *path;
    const char *description;

    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;

    harp_dimension_type dimension_type[1] = { harp_dimension_time };
    /* 2-D: {time, spectral=2} */
    harp_dimension_type dimension_type_2d[2] = { harp_dimension_time, harp_dimension_spectral };
    const char *wavelength_ratio_option_values[3] = { "354_388nm", "340_380nm", "335_367nm" };

    const char *surface_albedo_option_values[3] = { "388nm", "380nm", "367nm" };

    /* Product Registration Phase */
    module = harp_ingestion_register_module("S5_L2_AUI",
                                            "Sentinel-5",
                                            "EPS_SG",
                                            "SN5_02_AUI",
                                            "Sentinel-5 L2 AUI total column", ingestion_init, ingestion_done);

    /* wavelength_ratio */
    description = "Ingest aerosol index retrieved at wavelengths 354/388 nm " "(default), 340/380 nm, or 335/367 nm";
    harp_ingestion_register_option(module, "wavelength_ratio", description, 3, wavelength_ratio_option_values);

    /* surface_albedo */
    description = "whether to ingest the surface albedo at 388 (default), "
        "surface albedo at 367 (surface_albedo=367nm), or the surface " "alebedo at 380 (surface_albedo=380nm)";
    harp_ingestion_register_option(module, "surface_albedo", description, 3, surface_albedo_option_values);

    product_definition = harp_ingestion_register_product(module, "SN5_02_AUI", NULL, read_dimensions);

    /* Variables' Registration Phase */

    register_core_variables(product_definition, s5_delta_time_num_dims[s5_type_aui], 1);
    register_geolocation_variables(product_definition);
    register_additional_geolocation_variables(product_definition);
    register_surface_variables(product_definition, "SN5_02_AUI");
    register_snow_ice_flag_variables(product_definition, "SN5_02_AUI");

    /* absorbing_aerosol_index */
    description = "aerosol index";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "absorbing_aerosol_index", harp_type_float, 1,
                                                   dimension_type, NULL,
                                                   description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL, read_product_aerosol_index);
    harp_variable_definition_add_mapping(variable_definition,
                                         "wavelength_ratio=354_388nm or wavelength_ratio unset",
                                         NULL, "/data/PRODUCT/aerosol_index_354_388", NULL);
    harp_variable_definition_add_mapping(variable_definition,
                                         "wavelength_ratio=340_380nm",
                                         NULL, "/data/PRODUCT/aerosol_index_340_380", NULL);
    harp_variable_definition_add_mapping(variable_definition, "wavelength_ratio=335_367nm", NULL,
                                         // "processor version >= 02.04.00", 
                                         "/data/PRODUCT/aerosol_index_335_367", NULL);

    /* absorbing_aerosol_index_uncertainty */
    description = "uncertainty of the aerosol index";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "absorbing_aerosol_index_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL, read_product_aerosol_index_precision);
    harp_variable_definition_add_mapping(variable_definition, "wavelength_ratio=354_388nm (default)", NULL,
                                         "data/PRODUCT/aerosol_index_354_388_precision", NULL);
    harp_variable_definition_add_mapping(variable_definition, "wavelength_ratio=340_380nm", NULL,
                                         "data/PRODUCT/aerosol_index_340_380_precision", NULL);
    harp_variable_definition_add_mapping(variable_definition, "wavelength_ratio=335_367nm", NULL,
                                         "data/PRODUCT/aerosol_index_335_367_precision", NULL);

    /* absorbing_aerosol_index_validity */
    description = "continuous quality descriptor, varying between 0 (no data) and 100 (full quality data)";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "absorbing_aerosol_index_validity",
                                                   harp_type_int32, 1, dimension_type, NULL, description,
                                                   NULL, NULL, read_product_qa_value);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "data/PRODUCT/qa_value", NULL);


    /* reflectance */
    description = "Measured reflectance pair (lower, upper) for selected wavelength ratio";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "reflectance", harp_type_float, 2,     /* Number of dimensions */
                                                                     dimension_type_2d, /* Dimension types */
                                                                     NULL,
                                                                     description,
                                                                     HARP_UNIT_DIMENSIONLESS,
                                                                     NULL, read_results_reflectance_measured);

    /* Add mappings for the variable 
     * (not strictly needed if read routine does
     * all the work, but it's good practice) 
     */
    harp_variable_definition_add_mapping(variable_definition,
                                         "wavelength_ratio=354_388nm or wavelength_ratio unset", NULL,
                                         "/data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/reflectance_354_measured[], /data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/reflectance_388_measured[]",
                                         NULL);
    harp_variable_definition_add_mapping(variable_definition, "wavelength_ratio=340_380nm", NULL,
                                         "/data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/reflectance_340_measured[], /data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/reflectance_380_measured[]",
                                         NULL);
    harp_variable_definition_add_mapping(variable_definition, "wavelength_ratio=335_367nm", NULL,
                                         "/data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/reflectance_335_measured[], /data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/reflectance_367_measured[]",
                                         NULL);


    /* reflectance_uncertainty */
    description = "Measured reflectance uncertainty";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition,
                                                                     "reflectance_uncertainty",
                                                                     harp_type_float,
                                                                     2,
                                                                     dimension_type_2d,
                                                                     NULL,
                                                                     description,
                                                                     HARP_UNIT_DIMENSIONLESS,
                                                                     NULL, read_results_reflectance_precision);

    /* mappings (optional but nice for clarity) */
    harp_variable_definition_add_mapping(variable_definition,
                                         "wavelength_ratio=354_388nm or wavelength_ratio unset", NULL,
                                         "/data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/reflectance_precision_354_measured[], "
                                         "/data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/reflectance_precision_388_measured[]",
                                         NULL);

    harp_variable_definition_add_mapping(variable_definition,
                                         "wavelength_ratio=340_380nm", NULL,
                                         "/data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/reflectance_precision_340_measured[], "
                                         "/data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/reflectance_precision_380_measured[]",
                                         NULL);

    harp_variable_definition_add_mapping(variable_definition,
                                         "wavelength_ratio=335_367nm", NULL,
                                         "/data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/reflectance_precision_335_measured[], "
                                         "/data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/reflectance_precision_367_measured[]",
                                         NULL);

    /* surface_albedo */
    description = "Scene albedo at 388nm (default), 380nm, or 367 nm based on the wavelenght_ratio option";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "surface_albedo", harp_type_float, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS,
                                                   NULL, read_results_surface_albedo);
    path = "data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/scene_albedo_388[]";
    harp_variable_definition_add_mapping(variable_definition, "surface_albedo=388nm (default)", NULL, path, NULL);
    path = "data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/scene_albedo_380[]";
    harp_variable_definition_add_mapping(variable_definition, "surface_albedo=380nm", NULL, path, NULL);
    path = "data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/scene_albedo_367[]";
    harp_variable_definition_add_mapping(variable_definition, "surface_albedo=367nm", NULL, path, NULL);

}


/* CH4  */
static void register_ch4_product(void)
{
    const char *path;
    const char *description;

    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;

    int include_validity = 1;

    harp_dimension_type dimension_type_1d[1] = { harp_dimension_time };
    /* 2-D: {time, spectral=2} */
    harp_dimension_type dimension_type_2d_spec[2] = { harp_dimension_time, harp_dimension_spectral };
    harp_dimension_type dimension_type_2d_vert[2] = { harp_dimension_time, harp_dimension_vertical };

    const char *ch4_option_values[2] = { "physics", "proxy" };
    const char *ch4_band_option_values[3] = { "SWIR-1", "SWIR-3", "NIR-2" };


    /* Product Registration Phase */
    module = harp_ingestion_register_module("S5_L2_CH4",
                                            "Sentinel-5",
                                            "EPS_SG",
                                            "SN5_02_CH4",
                                            "Sentinel-5 L2 CH4 total column", ingestion_init, ingestion_done);

    description = "Choose which CH4 column to ingest: "
        "'physics' (default physics-based column) or 'proxy' (alternate proxy column)";
    harp_ingestion_register_option(module, "ch4",       /* option name */
                                   description, 2,      /* number of values */
                                   ch4_option_values);  /* allowed values */


    description = "Choose which surface albedo to ingest: " "SWIR-1 (default), SWIR-3, or NIR-2";
    harp_ingestion_register_option(module, "band", description, 3, ch4_band_option_values);

    product_definition = harp_ingestion_register_product(module, "SN5_02_CH4", NULL, read_dimensions);

    /* Variables' Registration Phase */
    register_core_variables(product_definition, s5_delta_time_num_dims[s5_type_ch4], include_validity);
    register_geolocation_variables(product_definition);
    register_additional_geolocation_variables(product_definition);
    register_surface_variables(product_definition, "SN5_02_CH4");
    register_snow_ice_flag_variables(product_definition, "SN5_02_CH4");

    /* methane_dry_air_column_mixing_ratio */
    description = "Physics CH4 dry air column mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "CH4_column_volume_mixing_ratio_dry_air",
                                                   harp_type_float, 1, dimension_type_1d, NULL, description, "ppbv",
                                                   NULL, read_product_methane_dry_air_column_mixing_ratio);

    path = "data/PRODUCT/methane_dry_air_column_mixing_ratio_physics[]";
    harp_variable_definition_add_mapping(variable_definition, "ch4=physics", NULL, path, NULL);
    path = "data/PRODUCT/methane_dry_air_column_mixing_ratio_proxy[]";
    harp_variable_definition_add_mapping(variable_definition, "ch4=proxy", NULL, path, NULL);


    /* methane_dry_air_column_mixing_ratio_precision */
    description = "Physics CH4 dry air column mixing ratio noise estimate";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "CH4_column_volume_mixing_ratio_dry_air_uncertainty",
                                                   harp_type_float, 1, dimension_type_1d, NULL, description, "ppbv",
                                                   NULL, read_product_methane_dry_air_column_mixing_ratio_precision);

    path = "data/PRODUCT/methane_dry_air_column_mixing_ratio_precision_physics[]";
    harp_variable_definition_add_mapping(variable_definition, "ch4=physics", NULL, path, NULL);
    path = "data/PRODUCT/methane_dry_air_column_mixing_ratio_precision_proxy[]";
    harp_variable_definition_add_mapping(variable_definition, "ch4=proxy", NULL, path, NULL);


    /* qa_value */
    description = "Quality assurance value describing the quality of the product";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "CH4_column_volume_mixing_ratio_dry_air_validity",
                                                   harp_type_int32, 1,
                                                   dimension_type_1d, NULL, description, NULL, NULL,
                                                   read_product_qa_value);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "data/PRODUCT/qa_value", NULL);


    /* pressure */
    description = "Pressure grid";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "pressure",
                                                   harp_type_float, 2, dimension_type_2d_vert, NULL, description, "Pa",
                                                   NULL, read_results_pressure);
    description = "the vertical grid is inverted to make it ascending";
    path = "data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/pressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);


    /* altitude */
    description = "Altitude grid";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "altitude",
                                                   harp_type_float, 2, dimension_type_2d_vert, NULL, description, "m",
                                                   NULL, read_results_altitude);
    description = "the vertical grid is inverted to make it ascending";
    path = "data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/altitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);


    /* dry_air_column */
    description = "Column number density profile of dry air";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "dry_air_column_number_density", harp_type_float, 1,
                                                   dimension_type_1d, NULL, description, "mol/m2", NULL,
                                                   read_results_dry_air_column);
    path = "data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/dry_air_column[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);


    /* methane_profile_apriori */
    description = "A-priori CH4 profile";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "CH4_column_number_density_apriori", harp_type_float,
                                                   2, dimension_type_2d_vert, NULL, description, "mol/m2", NULL,
                                                   read_results_methane_profile_apriori);
    path = "data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/methane_profile_apriori[]";
    description = "the vertical grid is inverted to make it ascending";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);


    /* carbon_dioxide_profile_apriori */
    description = "A-priori CO2 profile";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "CO2_column_number_density_apriori", harp_type_float, 2,
                                                   dimension_type_2d_vert, NULL, description, "mol/m2",
                                                   NULL, read_results_carbon_dioxide_profile_apriori);
    description = "the vertical grid is inverted to make it ascending";
    path = "data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/carbon_dioxide_profile_apriori[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);


    /* oxygen_total_column_apriori */
    description = "A-priori O2 column";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "O2_column_number_density_apriori", harp_type_float, 1,
                                                   dimension_type_1d, NULL, description, "mol/m2", NULL,
                                                   read_results_oxygen_total_column_apriori);
    path = "data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/oxygen_total_column_apriori[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);


    /* water_total_column_apriori */
    description = "A-priori H2O column";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "H2O_column_number_density_apriori", harp_type_float, 1,
                                                   dimension_type_1d, NULL, description, "mol/m2", NULL,
                                                   read_results_water_total_column_apriori);
    path = "data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/water_total_column_apriori[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);


    /* solar_induced_fluorescence */
    description = "Solar induced fluorescence";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "solar_induced_fluorescence", harp_type_float, 2,
                                                   dimension_type_2d_spec, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS,
                                                   NULL, read_results_solar_induced_fluorescence);
    description = "the spectral grid is inverted to make it ascending";
    path = "data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/solar_induced_fluorescence[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);


    /* methane_total_column_averaging_kernel */
    description = "Physics CH4 column averaging kernel";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "CH4_column_number_density_avk", harp_type_float, 2,
                                                   dimension_type_2d_vert, NULL, description, HARP_UNIT_DIMENSIONLESS,
                                                   //include_co_nd_avk, 
                                                   NULL, read_results_ch4_total_column_avk);
    description = "the vertical grid is inverted to make it ascending";
    path = "data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/methane_total_column_averaging_kernel[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);


    /* water_total_column */
    description = "H2O column";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "H2O_column_number_density", harp_type_float, 1,
                                                   dimension_type_1d, NULL, description, "mol/m^2", NULL,
                                                   read_results_water_total_column);
    path = "data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/water_total_column[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);


    /* carbon_dioxide_total_column */
    description = "CO2 column";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "CO2_column_number_density", harp_type_float, 1,
                                                   dimension_type_1d, NULL, description, "mol/m^2", NULL,
                                                   read_results_carbon_dioxide_total_column);
    path = "data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/carbon_dioxide_total_column[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);


    /* carbon_monoxide_total_column */
    description = "CO column";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "CO_column_number_density", harp_type_float, 1,
                                                   dimension_type_1d, NULL, description, "mol/m^2", NULL,
                                                   read_product_carbon_monoxide_total_column);
    path = "data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/carbon_monoxide_total_column[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);


    /* aerosol_size */
    description = "Aerosol particle size";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "aerosol_effective_radius",
                                                   harp_type_float, 1,
                                                   dimension_type_1d, NULL, description, "m", NULL,
                                                   read_results_aerosol_size);
    path = "data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/aerosol_size[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);


    /* aerosol_particle_column */
    description = "Aerosol particle column";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "aerosol_column_number_density",
                                                   harp_type_float, 1,
                                                   dimension_type_1d, NULL, description, "mol/m^2", NULL,
                                                   read_results_aerosol_particle_column);
    path = "data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/aerosol_particle_column[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);


    /* aerosol_layer_height */
    description = "Aerosol layer height above the surface";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "aerosol_height", harp_type_float, 1,
                                                   dimension_type_1d, NULL, description, "m", NULL,
                                                   read_results_aerosol_layer_height);
    path = "data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/aerosol_layer_height[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* surface_albedo */
    description = "Surface albedo in the selected band";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "surface_albedo", harp_type_float, 1, dimension_type_1d,       /* {time} */
                                                                     NULL,
                                                                     description,
                                                                     HARP_UNIT_DIMENSIONLESS,
                                                                     NULL, read_results_surface_albedo);

    /* three mappings, each gated on band=... */
    path = "data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/surface_albedo_swir_1[]";
    harp_variable_definition_add_mapping(variable_definition, "band=SWIR-1", NULL, path, NULL);

    path = "data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/surface_albedo_swir_3[]";
    harp_variable_definition_add_mapping(variable_definition, "band=SWIR-3", NULL, path, NULL);

    path = "data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/surface_albedo_nir_2[]";
    harp_variable_definition_add_mapping(variable_definition, "band=NIR-2", NULL, path, NULL);
}


/* NO2 */
static void register_no2_product(void)
{
    const char *path;
    const char *description;

    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;

    int include_validity = 1;

    harp_dimension_type dimension_type_1d[1] = { harp_dimension_time };
    /* 2-D: {time, spectral=2} */
    //harp_dimension_type dimension_type_2d_spec[2] = { harp_dimension_time, harp_dimension_spectral };
    harp_dimension_type dimension_type_2d_vert[2] = { harp_dimension_time, harp_dimension_vertical };

    harp_dimension_type pressure_bounds_dimension_type[3] =
        { harp_dimension_time, harp_dimension_vertical, harp_dimension_independent };
    long pressure_bounds_dimension[3] = { -1, -1, 2 };

    const char *no2_column_option_values[2] = { "total", "summed" };

    /* Product Registration Phase */

    /* 
     * harp_ingestion_register_module( 
     *          <unique_registry_key>, 
     *          <product_class>,
     *          <product_source / platform>, 
     *          <product_type>, <= ProductShortName
     *          <description for CLI listing>, 
     *          <pointer to init callback>, 
     *          <pointer to clean-up callback>
     *  );
     *
     * [Note]: <product type> is defined in get_product_type_name() at the top of this file 
     */
    module = harp_ingestion_register_module("S5_L2_NO2",
                                            "Sentinel-5",
                                            "EPS_SG",
                                            "SN5_02_NO2",
                                            "Sentinel-5 L2 NO2 total column", ingestion_init, ingestion_done);

    description = "Choose which NO2 column to ingest: 'total' (default) or 'summed'";
    harp_ingestion_register_option(module, "total_column",      /* option name */
                                   description, 2,      /* number of values */
                                   no2_column_option_values);   /* allowed values */

    /* 
     * harp_ingestion_register_product(module ptr, "ProductShortName", options table (NULL), dimension-callback ) */
    product_definition = harp_ingestion_register_product(module, "SN5_02_NO2", NULL, read_dimensions);

    /* Variables' Registration Phase */

    register_core_variables(product_definition, s5_delta_time_num_dims[s5_type_no2], include_validity);
    register_geolocation_variables(product_definition);
    register_additional_geolocation_variables(product_definition);
    register_surface_variables(product_definition, "SN5_02_NO2");
    register_snow_ice_flag_variables(product_definition, "SN5_02_NO2");


    /* nitrogen_dioxide_tropospheric_column */
    description = "Tropospheric NO2 vertical column density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "tropospheric_NO2_column_number_density",
                                                   harp_type_float, 1, dimension_type_1d, NULL, description,
                                                   "mol/m^2", NULL, read_product_nitrogen_dioxide_tropospheric_column);
    path = "data/PRODUCT/nitrogen_dioxide_tropospheric_column[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* nitrogen_dioxide_tropospheric_column_uncertainty */
    description = "Tropospheric NO2 vertical column density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "tropospheric_NO2_column_number_density_uncertainty",
                                                   harp_type_float, 1, dimension_type_1d, NULL, description,
                                                   "mol/m^2", NULL,
                                                   read_product_nitrogen_dioxide_tropospheric_column_uncertainty);
    path = "data/PRODUCT/nitrogen_dioxide_tropospheric_column_uncertainty[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);


    /* nitrogen_dioxide_tropospheric_column_air_mass_factor */
    description = "Tropospheric air mass factor";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "tropospheric_NO2_column_number_density_amf",
                                                   harp_type_float, 1, dimension_type_1d, NULL, description,
                                                   "mol/m^2", NULL,
                                                   read_product_nitrogen_dioxide_tropospheric_column_air_mass_factor);
    path = "data/PRODUCT/nitrogen_dioxide_tropospheric_column_air_mass_factor[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);


    /* nitrogen_dioxide_total_column_air_mass_factor */
    description = "Total air mass factor";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "NO2_column_number_density_amf",
                                                   harp_type_float, 1, dimension_type_1d, NULL, description,
                                                   "mol/m^2", NULL,
                                                   read_product_nitrogen_dioxide_total_column_air_mass_factor);
    path = "data/PRODUCT/nitrogen_dioxide_total_column_air_mass_factor[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* nitrogen_dioxide_total_column_averaging_kernel */
    description = "Averaging kernel";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "NO2_column_number_density_avk", harp_type_float, 2,
                                                   dimension_type_2d_vert, NULL, description, HARP_UNIT_DIMENSIONLESS,
                                                   NULL, read_product_nitrogen_dioxide_total_column_avk);
    path = "data/PRODUCT/nitrogen_dioxide_total_column_averaging_kernel[]";
    description = "the vertical grid is inverted to make it ascending";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);


    /* qa_value */
    description = "Quality assurance value describing the quality of the product";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "tropospheric_NO2_column_number_density_validity",
                                                   harp_type_int32, 1, dimension_type_1d, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL, read_product_qa_value);
    path = "data/PRODUCT/qa_value[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* nitrogen_dioxide_stratospheric_column_air_mass_factor */
    description = "Stratospheric air mass factor";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "stratospheric_NO2_column_number_density_amf",
                                                   harp_type_float, 1, dimension_type_1d, NULL, description,
                                                   "mol/m^2", NULL,
                                                   read_results_nitrogen_dioxide_stratospheric_column_amf);
    path = "data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/nitrogen_dioxide_stratospheric_column_air_mass_factor[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_radiance_fraction */
    description = "Cloud radiance fraction";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "cloud_fraction",
                                                   harp_type_float, 1, dimension_type_1d, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL, read_results_cloud_radiance_fraction);
    path = "data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/cloud_radiance_fraction[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);


    /* nitrogen_dioxide_slant_column */
    description = "Total NO2 slant column density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "NO2_slant_column_number_density",
                                                   harp_type_float, 1, dimension_type_1d, NULL, description,
                                                   "mol/m^2", NULL, read_results_nitrogen_dioxide_slant_column);
    path = "data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/nitrogen_dioxide_slant_column[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* nitrogen_dioxide_slant_column_uncertainty */
    description = "Total NO2 slant column density uncertainty";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "NO2_slant_column_number_density_uncertainty",
                                                   harp_type_float, 1, dimension_type_1d, NULL, description,
                                                   "mol/m^2", NULL,
                                                   read_results_nitrogen_dioxide_slant_column_uncertainty);
    path = "data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/nitrogen_dioxide_slant_column_uncertainty[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* ozone_slant_column */
    description = "O3 slant column density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "O3_slant_column_number_density",
                                                   harp_type_float, 1, dimension_type_1d, NULL, description,
                                                   "mol/m^2", NULL, read_results_ozone_slant_column);
    path = "data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/ozone_slant_column[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* ozone_slant_column_uncertainty */
    description = "O3 slant column density uncertainty";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "O3_slant_column_number_density_uncertainty",
                                                   harp_type_float, 1, dimension_type_1d, NULL, description,
                                                   "mol/m^2", NULL, read_results_ozone_slant_column_uncertainty);
    path = "data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/ozone_slant_column_uncertainty[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* water_vapor_slant_column */
    description = "H2O vapor slant column density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "H2O_vapor_slant_column_number_density",
                                                   harp_type_float, 1, dimension_type_1d, NULL, description,
                                                   "mol/m^2", NULL, read_results_water_vapor_slant_column);
    path = "data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/water_vapor_slant_column[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* water_vapor_slant_column_uncertainty */
    description = "H2O vapor slant column density uncertainty";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "H2O_vapor_slant_column_number_density_uncertainty",
                                                   harp_type_float, 1, dimension_type_1d, NULL, description,
                                                   "mol/m^2", NULL, read_results_water_vapor_slant_column_uncertainty);
    path = "data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/water_vapor_slant_column_uncertainty[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* water_liquid_slant_column */
    description = "H2O liquid coefficient";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "liquid_H2O_slant_column_number_density",
                                                   harp_type_float, 1, dimension_type_1d, NULL, description,
                                                   "mol/m^2", NULL, read_results_water_liquid_slant_column);
    path = "data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/water_liquid_slant_column[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* water_liquid_slant_column_uncertainty */
    description = "H2O liquid coefficient uncertainty";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "liquid_H2O_slant_column_number_density_uncertainty",
                                                   harp_type_float, 1, dimension_type_1d, NULL, description,
                                                   "mol/m^2", NULL, read_results_water_liquid_slant_column);
    path = "data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/water_liquid_slant_column_uncertainty[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);


    /* nitrogen_dioxide_stratospheric_column */
    description = "Stratospheric NO2 vertical column density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "stratospheric_NO2_column_number_density",
                                                   harp_type_float, 1, dimension_type_1d, NULL, description,
                                                   "mol/m^2", NULL, read_results_nitrogen_dioxide_stratospheric_column);
    path = "data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/nitrogen_dioxide_stratospheric_column[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* nitrogen_dioxide_stratospheric_column_uncertainty */
    description = "Stratospheric NO2 vertical column density uncertainty";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "stratospheric_NO2_column_number_density_uncertainty",
                                                   harp_type_float, 1, dimension_type_1d, NULL, description,
                                                   "mol/m^2", NULL,
                                                   read_results_nitrogen_dioxide_stratospheric_column_uncertainty);
    path = "data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/nitrogen_dioxide_stratospheric_column_uncertainty[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* nitrogen_dioxide_[|summed]_total_column */
    description = "NO2 column number density values in the selected column option";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "NO2_column_number_density", harp_type_float, 1, dimension_type_1d,    /* {time} */
                                                                     NULL,
                                                                     description,
                                                                     "mol/m^2",
                                                                     NULL, read_results_nitrogen_dioxide_total_column);

    /* two mappings */
    path = "data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/nitrogen_dioxide_total_column[]";
    description = "Total NO2 vertical column density";
    harp_variable_definition_add_mapping(variable_definition, "total_column=total", NULL, path, description);

    path = "data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/nitrogen_dioxide_summed_total_column[]";
    description = "Sum of partial NO2 columns";
    harp_variable_definition_add_mapping(variable_definition, "total_column=summed", NULL, path, description);

    /* nitrogen_dioxide_total_column_uncertainty */
    description = "NO2 column number density uncertainty values in the selected column option";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "NO2_column_number_density_uncertainty", harp_type_float, 1, dimension_type_1d,        /* {time} */
                                                                     NULL,
                                                                     description,
                                                                     "mol/m^2",
                                                                     NULL,
                                                                     read_results_nitrogen_dioxide_total_column_uncertainty);

    /* two mappings */
    path = "data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/nitrogen_dioxide_total_column_uncertainty[]";
    description = "Total NO2 vertical column density uncertainty";
    harp_variable_definition_add_mapping(variable_definition, "total_column=total", NULL, path, description);

    path = "data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/nitrogen_dioxide_summed_total_column_uncertainty[]";
    description = "Sum of partial NO2 vertical column density uncertainty";
    harp_variable_definition_add_mapping(variable_definition, "total_column=summed", NULL, path, description);

    /* surface_albedo */
    description = "Surface albedo";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "surface_albedo", harp_type_float, 1, dimension_type_1d,
                                                   NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_results_surface_albedo);
    path = "data/PRODUCT/SUPPORT_DATA/INPUT_DATA/surface_albedo[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);


    /* pressure_bounds */
    description = "pressure boundaries";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "pressure_bounds", harp_type_double, 3,
                                                   pressure_bounds_dimension_type, pressure_bounds_dimension,
                                                   description, "Pa", NULL, read_no2_pressure_bounds);
    path =
        "data/PRODUCT/SUPPORT_DATA/INPUT_DATA/pressure_coefficient_a[], data/PRODUCT/SUPPORT_DATA/INPUT_DATA/pressure_coefficient_b[], data/PRODUCT/SUPPORT_DATA/INPUT_DATA/surface_pressure[]";
    description =
        "pressure in Pa at level k is derived from surface pressure in Pa as: pressure_coefficient_a[k] + "
        "pressure_coefficient_b[k] * surface_pressure[]; the top of atmosphere pressure is clamped to 1e-3 Pa";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* aerosol_index_354_388 */
    description = "Aerosol absorbing index 354/388 pair";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "aerosol_index", harp_type_float, 1, dimension_type_1d,
                                                   NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_input_aerosol_index);
    path = "data/PRODUCT/SUPPORT_DATA/INPUT_DATA/aerosol_index_354_388[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_albedo */
    description = "Cloud albedo";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "cloud_albedo", harp_type_float, 1, dimension_type_1d,
                                                   NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_input_cloud_albedo);
    path = "data/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_albedo[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_albedo_uncertainty */
    description = "Cloud albedo uncertainty";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "cloud_albedo_uncertainty", harp_type_float, 1, dimension_type_1d,
                                                   NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_input_cloud_albedo_uncertainty);
    path = "data/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_albedo_uncertainty[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_pressure */
    description = "Cloud pressure";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "cloud_pressure", harp_type_float, 1, dimension_type_1d,
                                                   NULL, description, "Pa", NULL, read_input_cloud_pressure);
    path = "data/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_pressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_pressure_uncertainty */
    description = "Cloud pressure uncertainty";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "cloud_pressure_uncertainty", harp_type_float, 1, dimension_type_1d,
                                                   NULL, description, "Pa", NULL, read_input_cloud_pressure);
    path = "data/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_pressure_uncertainty[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* effective_cloud_fraction */

    /* Encountering the following error: 
     * harp-ingestion-module.c:472: product_definition_add_variable: Assertion
     * `!harp_product_definition_has_variable(product_definition,
     * variable->name)' failed.
     * fish: Job 1, 'harpdump -l $HOME/D...' terminated by signal SIGABRT (Abort)

     * which implies that cloud_fraction variable has been already registered (and indeed it was). 
     * so either need to change the name of this variable or add it to the one above (*_radiance)
     */
    description = "Cloud fraction";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "effective_cloud_fraction", harp_type_float, 1, dimension_type_1d,
                                                   NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_input_effective_cloud_fraction);
    path = "data/PRODUCT/SUPPORT_DATA/INPUT_DATA/effective_cloud_fraction[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* effective_cloud_fraction_uncertainty */
    description = "Cloud fraction uncertainty";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "effective_cloud_fraction_uncertainty", harp_type_float, 1,
                                                   dimension_type_1d, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_input_effective_cloud_fraction_uncertainty);
    path = "data/PRODUCT/SUPPORT_DATA/INPUT_DATA/effective_cloud_fraction_uncertainty[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* scene_albedo */
    description = "Scene albedo";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "scene_albedo", harp_type_float, 1, dimension_type_1d,
                                                   NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_input_scene_albedo);
    path = "data/PRODUCT/SUPPORT_DATA/INPUT_DATA/scene_albedo[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* scene_albedo_uncertainty */
    description = "Scene albedo uncertainty";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "scene_albedo_uncertainty", harp_type_float, 1, dimension_type_1d,
                                                   NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_input_scene_albedo_uncertainty);
    path = "data/PRODUCT/SUPPORT_DATA/INPUT_DATA/scene_albedo_uncertainty[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* scene_pressure */
    description = "Scene pressure";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "scene_pressure", harp_type_float, 1, dimension_type_1d,
                                                   NULL, description, "Pa", NULL, read_input_scene_pressure);
    path = "data/PRODUCT/SUPPORT_DATA/INPUT_DATA/scene_pressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* scene_pressure_uncertainty */
    description = "Scene pressure uncertainty";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "scene_pressure_uncertainty", harp_type_float, 1, dimension_type_1d,
                                                   NULL, description, "Pa", NULL,
                                                   read_input_scene_pressure_uncertainty);
    path = "data/PRODUCT/SUPPORT_DATA/INPUT_DATA/scene_pressure_uncertainty[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* tropopause_pressure */
    description = "Tropopause pressure (CAMS)";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "tropopause_pressure", harp_type_float, 1, dimension_type_1d,
                                                   NULL, description, "Pa", NULL, read_input_tropopause_pressure);
    path = "data/PRODUCT/SUPPORT_DATA/INPUT_DATA/tropopause_pressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

/* O3 */
static void register_o3_product(void)
{
    const char *path;
    const char *description;

    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;

    int include_validity = 1;

    harp_dimension_type dimension_type_1d[1] = { harp_dimension_time };
    harp_dimension_type dimension_type_2d[2] = { harp_dimension_time, harp_dimension_vertical };


    /* Product Registration Phase */

    /* 
     * harp_ingestion_register_module( 
     *          <unique_registry_key>, 
     *          <product_class>,
     *          <product_source / platform>, 
     *          <product_type>, <= ProductShortName
     *          <description for CLI listing>, 
     *          <pointer to init callback>, 
     *          <pointer to clean-up callback>
     *  );
     *
     * [Note]: <product type> is defined in get_product_type_name() at the top of this file 
     */
    module = harp_ingestion_register_module("S5_L2_O3",
                                            "Sentinel-5",
                                            "EPS_SG",
                                            "SN5_02_O3_",
                                            "Sentinel-5 L2 O3 total column", ingestion_init, ingestion_done);

    /* harp_ingestion_register_product( module ptr, "ProductShortName", options table (NULL), dimension-callback ) */
    product_definition = harp_ingestion_register_product(module, "SN5_02_O3_", NULL, read_dimensions);


    /* Variables' Registration Phase */

    register_core_variables(product_definition, s5_delta_time_num_dims[s5_type_o3], include_validity);
    register_geolocation_variables(product_definition);
    register_additional_geolocation_variables(product_definition);
    register_surface_variables(product_definition, "SN5_02_O3");
    register_snow_ice_flag_variables(product_definition, "SN5_02_O3");

    /* ozone_total_column */
    description = "O3 VCD";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "O3_column_number_density", harp_type_float, 1,
                                                   dimension_type_1d, NULL, description, "mol/m^2", NULL,
                                                   read_product_ozone_total_column);
    path = "data/PRODUCT/ozone_total_column";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* ozone_total_column_precision */
    description = "O3 VCD random error";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "O3_column_number_density_uncertainty_random", harp_type_float, 1,
                                                   dimension_type_1d, NULL, description, "mol/m^2", NULL,
                                                   read_product_ozone_total_column_precision);
    path = "data/PRODUCT/ozone_total_column_precision";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* ozone_total_column_precision */
    description = "O3 VCD systematic error";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "O3_column_number_density_uncertainty_systematic", harp_type_float,
                                                   1, dimension_type_1d, NULL, description, "mol/m^2", NULL,
                                                   read_product_ozone_total_column_trueness);
    path = "data/PRODUCT/ozone_total_column_trueness";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* qa_value */
    description = "Quality assurance value describing the quality of the product";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "O3_column_number_density_validity", harp_type_int32, 1,
                                                   dimension_type_1d, NULL, description, NULL, NULL,
                                                   read_product_qa_value);
    path = "data/PRODUCT/qa_value[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* ozone_slant_column */
    description = "O3 SCD";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "O3_slant_column_number_density",
                                                   harp_type_float, 1, dimension_type_1d, NULL, description,
                                                   "mol/m^2", NULL, read_results_ozone_slant_column);
    path = "data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/ozone_slant_column[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* ozone_slant_column_uncertainty */
    description = "O3 SCD random error";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "O3_slant_column_number_density_uncertainty",
                                                   harp_type_float, 1, dimension_type_1d, NULL, description,
                                                   "mol/m^2", NULL, read_results_ozone_slant_column_uncertainty);
    path = "data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/ozone_slant_column_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* effective_temperature */
    description = "Effective temperature";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "O3_effective_temperature",
                                                   harp_type_float, 1, dimension_type_1d, NULL, description,
                                                   "K", NULL, read_results_effective_temperature);
    path = "data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/effective_temperature[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* effective_scene_air_mass_factor */
    description = "Effective scene AMF";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "O3_column_number_density_amf",
                                                   harp_type_float, 1, dimension_type_1d, NULL, description,
                                                   "mol/m^2", NULL, read_results_effective_scene_amf);
    path = "data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/effective_scene_air_mass_factor[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);


    /* effective_scene_albedo */
    description = "Effective scene albedo";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "scene_albedo",
                                                   harp_type_float, 1, dimension_type_1d, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL, read_results_effective_scene_albedo);
    path = "data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/effective_scene_albedo[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* ozone_total_column_avk */
    description = "Averaging kernels of ozone total column";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "O3_column_number_density_avk",
                                                   harp_type_float, 2, dimension_type_2d, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL, read_results_ozone_total_column_avk);
    path = "data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/ozone_total_column_averaging_kernel[]";
    description = "the vertical grid is inverted to make it ascending";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* ozone_profile_apriori */
    description = "O3 Profile";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "O3_column_number_density_apriori",
                                                   harp_type_float, 2, dimension_type_2d, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL, read_results_ozone_profile_apriori);
    path = "data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/ozone_profile_apriori[]";
    description = "the vertical grid is inverted to make it ascending";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* pressure_grid */
    description = "Pressure grid";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "pressure",
                                                   harp_type_float, 2, dimension_type_2d, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL, read_results_pressure_grid);
    path = "data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/pressure_grid[]";
    description = "the vertical grid is inverted to make it ascending";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* surface_albedo */
    description = "Surface albedo";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "surface_albedo", harp_type_float, 1, dimension_type_1d,
                                                   NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_results_surface_albedo);
    path = "data/PRODUCT/SUPPORT_DATA/INPUT_DATA/surface_albedo_335[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* effective_cloud_fraction */
    description = "Cloud fraction";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "cloud_fraction", harp_type_float, 1, dimension_type_1d,
                                                   NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_input_effective_cloud_fraction);
    path = "data/PRODUCT/SUPPORT_DATA/INPUT_DATA/effective_cloud_fraction[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_pressure */
    description = "Cloud pressure";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "cloud_pressure", harp_type_float, 1, dimension_type_1d,
                                                   NULL, description, "Pa", NULL, read_input_cloud_pressure);
    path = "data/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_pressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_albedo */
    description = "Cloud top albedo";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "cloud_albedo", harp_type_float, 1, dimension_type_1d,
                                                   NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_input_cloud_albedo);
    path = "data/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_albedo[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* scene_pressure */
    description = "Scene pressure";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "scene_pressure", harp_type_float, 1, dimension_type_1d,
                                                   NULL, description, "Pa", NULL, read_input_scene_pressure);
    path = "data/PRODUCT/SUPPORT_DATA/INPUT_DATA/scene_pressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* aerosol_index_340_380 */
    description = "Aerosol absorbing index 340/380 pair";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "aerosol_index", harp_type_float, 1, dimension_type_1d,
                                                   NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_input_aerosol_index);
    path = "data/PRODUCT/SUPPORT_DATA/INPUT_DATA/aerosol_index_340_380[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* tropopause_pressure */
    description = "Tropopause pressure (CAMS)";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "tropopause_pressure", harp_type_float, 1, dimension_type_1d,
                                                   NULL, description, "Pa", NULL, read_input_tropopause_pressure);
    path = "data/PRODUCT/SUPPORT_DATA/INPUT_DATA/tropopause_pressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}


/* Read a SO2 scalar field with an extra 'profile' dimension           */
/* and collapse that dimension according to info->so2_column_type.    */
static int read_so2_scalar(void *user_data, const char *dataset_name,   /* e.g. "sulfur_dioxide_total_column" */
                           harp_array data)     /* output: {time} = scanline x pixel   */
{
    ingest_info *info = (ingest_info *)user_data;

    /* total elements in the 3-D variable on file */
    long num_elements = info->num_scanlines * info->num_pixels * info->num_profile;     /* 4 profiles */

    /* temporary buffer for the full 3-D variable */
    harp_array buffer;

    buffer.ptr = malloc(num_elements * sizeof(float));
    if (buffer.ptr == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "cannot allocate %ld-element SO2 buffer", num_elements);
        return -1;
    }

    /* We first try under /data/PRODUCT/... */
    int status = read_dataset(info->product_cursor,
                              dataset_name, harp_type_float,
                              num_elements, buffer);

    /* If that failed, fall back to /data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/... */
    if (status != 0)
    {
        status = read_dataset(info->detailed_results_cursor, dataset_name, harp_type_float, num_elements, buffer);
    }

    if (status != 0)
    {
        free(buffer.ptr);
        return -1;      /* read_dataset set a HARP error for us */
    }

    /* copy the requested profile (0...3) into the 1-D HARP array */
    long stride = info->num_profile;    /* profile dimension length   */
    long sel_idx = info->so2_column_type;       /* 0=PBL,1=1 km,2=7 km,3=15 km */
    long out_idx = 0;

    for (long i = sel_idx; i < num_elements; i += stride)
        data.float_data[out_idx++] = buffer.float_data[i];

    free(buffer.ptr);
    return 0;
}


static int read_so2_total_column(void *u, harp_array d)
{
    return read_so2_scalar(u, "sulfur_dioxide_total_column", d);
}

static int read_so2_total_column_precision(void *u, harp_array d)
{
    return read_so2_scalar(u, "sulfur_dioxide_total_column_precision", d);
}

static int read_so2_total_column_trueness(void *u, harp_array d)
{
    return read_so2_scalar(u, "sulfur_dioxide_total_column_trueness", d);
}

static int read_so2_total_amf(void *u, harp_array d)
{
    return read_so2_scalar(u, "sulfur_dioxide_total_column_air_mass_factor", d);
}

static int read_so2_total_amf_precision(void *u, harp_array d)
{
    return read_so2_scalar(u, "sulfur_dioxide_total_column_air_mass_factor_precision", d);
}

static int read_so2_total_amf_trueness(void *u, harp_array d)
{
    return read_so2_scalar(u, "sulfur_dioxide_total_column_air_mass_factor_trueness", d);
}



/* SO2 */
static void register_so2_product(void)
{
    const char *path;
    const char *description;

    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;

    int include_validity = 1;

    harp_dimension_type dimension_type_1d[1] = { harp_dimension_time };
    harp_dimension_type dimension_type_2d[2] = { harp_dimension_time, harp_dimension_vertical };

    harp_dimension_type pressure_bounds_dimension_type[3] =
        { harp_dimension_time, harp_dimension_vertical, harp_dimension_independent };
    long pressure_bounds_dimension[3] = { -1, -1, 2 };

    const char *so2_column_options[] = { "1km", "7km", "15km" };

    /* Product Registration Phase */

    /* 
     * harp_ingestion_register_module( 
     *          <unique_registry_key>, 
     *          <product_class>,
     *          <product_source / platform>, 
     *          <product_type>, <= ProductShortName
     *          <description for CLI listing>, 
     *          <pointer to init callback>, 
     *          <pointer to clean-up callback>
     *  );
     *
     * [Note]: <product type> is defined in get_product_type_name() at the top of this file 
     */
    module = harp_ingestion_register_module("S5_L2_SO2",
                                            "Sentinel-5",
                                            "EPS_SG",
                                            "SN5_02_SO2",
                                            "Sentinel-5 L2 SO2 total column", ingestion_init, ingestion_done);

    harp_ingestion_register_option(module,
                                   "so2_column",
                                   "select the SO2 column from the 1 km, 7 km, or 15 km box profile; "
                                   "if the option is omitted the polluted-boundary-layer column (PBL) "
                                   "is ingested", 3, so2_column_options);

    /* harp_ingestion_register_product( module ptr, "ProductShortName", options table (NULL), dimension-callback ) */
    product_definition = harp_ingestion_register_product(module, "SN5_02_SO2", NULL, read_dimensions);

    /* Variables' Registration Phase */

    register_core_variables(product_definition, s5_delta_time_num_dims[s5_type_so2], include_validity);
    register_geolocation_variables(product_definition);
    register_additional_geolocation_variables(product_definition);
    register_surface_variables(product_definition, "SN5_02_SO2");
    register_snow_ice_flag_variables(product_definition, "SN5_02_SO2");

    /* SO2_column_number_density */
    description = "SO2 vertical column density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "SO2_column_number_density", harp_type_float,
                                                   1, dimension_type_1d, NULL, description, "mol/m^2", NULL,
                                                   read_so2_total_column);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/data/PRODUCT/sulfur_dioxide_total_column[]",
                                         "profile dimension sliced according to so2_column option");

    /* SO2_column_number_density_uncertainty_random */
    description = "Random uncertainty of SO2 column density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "SO2_column_number_density_uncertainty_random",
                                                   harp_type_float, 1, dimension_type_1d, NULL, description, "mol/m^2",
                                                   NULL, read_so2_total_column_precision);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/data/PRODUCT/sulfur_dioxide_total_column_precision[]",
                                         "profile dimension sliced according to so2_column option");

    /* SO2_column_number_density_uncertainty_systematic */
    description = "Systematic uncertainty of SO2 column density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "SO2_column_number_density_uncertainty_systematic", harp_type_float,
                                                   1, dimension_type_1d, NULL, description, "mol/m^2", NULL,
                                                   read_so2_total_column_trueness);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/data/PRODUCT/sulfur_dioxide_total_column_trueness[]",
                                         "profile dimension sliced according to so2_column option");


    /* sulfur_dioxide_layer_height */
    description = "Retrieved layer height of SO2 above sea level";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "SO2_layer_height",    /* HARP name              */
                                                                     harp_type_float, 1,        /* rank / type            */
                                                                     dimension_type_1d, NULL,   /* {time}                 */
                                                                     description, "m",  /* unit                   */
                                                                     NULL,      /* include-callback       */
                                                                     read_product_sulfur_dioxide_layer_height); /* read-callback          */

    path = "data/PRODUCT/sulfur_dioxide_layer_height[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* sulfur_dioxide_layer_height_uncertainty */
    description = "Uncertainty of the retrieved SO2 layer height";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "SO2_layer_height_uncertainty",
                                                   harp_type_float, 1, dimension_type_1d, NULL,
                                                   description, "m", NULL,
                                                   read_product_sulfur_dioxide_layer_height_uncertainty);

    path = "data/PRODUCT/sulfur_dioxide_layer_height_uncertainty[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* sulfur_dioxide_layer_height_flag */
    description = "Flag associated with SO2 layer-height retrieval quality";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "SO2_layer_height_validity",
                                                   harp_type_int8, 1, dimension_type_1d, NULL,
                                                   description, NULL, NULL,
                                                   read_product_sulfur_dioxide_layer_height_flag);

    path = "data/PRODUCT/sulfur_dioxide_layer_height_flag[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* qa_value */
    description = "Quality-assurance value describing the quality of the product";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "SO2_column_number_density_validity",
                                                   harp_type_int8, 1, dimension_type_1d, NULL,
                                                   description, NULL, NULL, read_product_qa_value);

    path = "data/PRODUCT/qa_value[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);


    /* SO2_column_number_density_amf */
    description = "Total air-mass factor of the SO2 column";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "SO2_column_number_density_amf", harp_type_float,
                                                   1, dimension_type_1d, NULL, description, HARP_UNIT_DIMENSIONLESS,
                                                   NULL, read_so2_total_amf);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfur_dioxide_total_column_air_mass_factor[]",
                                         "profile dimension sliced according to so2_column option");

    /* SO2_column_number_density_amf_uncertainty_random */
    description = "Random uncertainty of SO2 air-mass factor";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "SO2_column_number_density_amf_uncertainty_random", harp_type_float,
                                                   1, dimension_type_1d, NULL, description, HARP_UNIT_DIMENSIONLESS,
                                                   NULL, read_so2_total_amf_precision);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfur_dioxide_total_column_air_mass_factor_precision[]",
                                         "profile dimension sliced according to so2_column option");

    /* SO2_column_number_density_amf_uncertainty_systematic */
    description = "Systematic uncertainty of SO2 air-mass factor";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "SO2_column_number_density_amf_uncertainty_systematic",
                                                   harp_type_float, 1, dimension_type_1d, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL, read_so2_total_amf_trueness);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfur_dioxide_total_column_air_mass_factor_trueness[]",
                                         "profile dimension sliced according to so2_column option");

    /* sulfur_dioxide_slant_column */
    description = "SO2 slant column density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "SO2_slant_column_number_density",
                                                   harp_type_float, 1, dimension_type_1d, NULL,
                                                   description, "mol/m^2", NULL,
                                                   read_results_sulfur_dioxide_slant_column);

    path = "data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfur_dioxide_slant_column_corrected[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* sulfur_dioxide_slant_column_precision */
    description = "Random component of the uncertainty of the SO2 slant column density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "SO2_slant_column_number_density_uncertainty_random",
                                                   harp_type_float, 1, dimension_type_1d, NULL,
                                                   description, "mol/m^2", NULL,
                                                   read_results_sulfur_dioxide_slant_column_precision);

    path = "data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfur_dioxide_slant_column_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* sulfur_dioxide_slant_column_trueness */
    description = "Systematic component of the uncertainty of the SO2 slant column density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "SO2_slant_column_number_density_uncertainty_systematic",
                                                   harp_type_float, 1, dimension_type_1d, NULL,
                                                   description, "mol/m^2", NULL,
                                                   read_results_sulfur_dioxide_slant_column_trueness);

    path = "data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfur_dioxide_slant_column_trueness[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_radiance_fraction */
    description = "Cloud radiance fraction";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "cloud_fraction",
                                                   harp_type_float, 1, dimension_type_1d, NULL,
                                                   description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_results_cloud_radiance_fraction);

    path = "data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/cloud_radiance_fraction[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* sulfur_dioxide_total_column_averaging_kernel */
    description = "Averaging kernel for the SO2 total column";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "SO2_column_number_density_avk",
                                                   harp_type_float, 2, dimension_type_2d, NULL,
                                                   description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_results_sulfur_dioxide_total_column_avk);

    path = "data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfur_dioxide_total_column_averaging_kernel[]";
    description = "the vertical grid is inverted to make it ascending";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* sulfur_dioxide_layer_pressure */
    description = "Retrieved layer pressure of SO2";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "SO2_layer_pressure",
                                                   harp_type_float, 1, dimension_type_1d, NULL,
                                                   description, "Pa", NULL, read_results_sulfur_dioxide_layer_pressure);

    path = "data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfur_dioxide_layer_pressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* sulfur_dioxide_layer_pressure_uncertainty */
    description = "Total error on retrieved layer pressure of SO2";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "SO2_layer_pressure_uncertainty",
                                                   harp_type_float, 1, dimension_type_1d, NULL,
                                                   description, "Pa", NULL,
                                                   read_results_sulfur_dioxide_layer_pressure_uncertainty);

    path = "data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/sulfur_dioxide_layer_pressure_uncertainty[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* surface_albedo */
    description = "Surface albedo at 340 nm";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "surface_albedo", harp_type_float, 1, dimension_type_1d,
                                                   NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_results_surface_albedo);
    path = "data/PRODUCT/SUPPORT_DATA/INPUT_DATA/surface_albedo[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);


    /* sulfur_dioxide_profile_apriori */
    description = "A priori SO2 profile (CAMS)";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "SO2_column_number_density_apriori", harp_type_float, 2,
                                                   dimension_type_2d, NULL, description, "mol/m^2", NULL,
                                                   read_input_sulfur_dioxide_profile_apriori);
    path = "data/PRODUCT/SUPPORT_DATA/INPUT_DATA/sulfur_dioxide_profile_apriori[]";
    description = "the vertical grid is inverted to make it ascending";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* pressure_bounds */
    description = "pressure boundaries";
    /* Note: reusing logic from NO2 */
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "pressure_bounds", harp_type_double, 3,
                                                   pressure_bounds_dimension_type, pressure_bounds_dimension,
                                                   description, "Pa", NULL, read_no2_pressure_bounds);
    path =
        "data/PRODUCT/SUPPORT_DATA/INPUT_DATA/pressure_coefficient_a[], data/PRODUCT/SUPPORT_DATA/INPUT_DATA/pressure_coefficient_b[], data/PRODUCT/SUPPORT_DATA/INPUT_DATA/surface_pressure[]";
    description =
        "pressure in Pa at level k is derived from surface pressure in Pa as: pressure_coefficient_a[k] + "
        "pressure_coefficient_b[k] * surface_pressure[]; the top of atmosphere pressure is clamped to 1e-3 Pa";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* effective_cloud_fraction */
    description = "Cloud fraction";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "effective_cloud_fraction", harp_type_float, 1, dimension_type_1d,
                                                   NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_input_effective_cloud_fraction);
    path = "data/PRODUCT/SUPPORT_DATA/INPUT_DATA/effective_cloud_fraction[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_pressure */
    description = "Cloud top pressure";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "cloud_pressure", harp_type_float, 1, dimension_type_1d,
                                                   NULL, description, "Pa", NULL, read_input_cloud_pressure);
    path = "data/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_pressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_height */
    description = "Cloud centre height above the surface";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_height", harp_type_float, 1,
                                                   dimension_type_1d, NULL, description, "m", NULL,
                                                   read_input_cloud_height);
    path = "data/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_height[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_albedo */
    description = "Cloud albedo";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "cloud_albedo", harp_type_float, 1, dimension_type_1d,
                                                   NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_input_cloud_albedo);
    path = "data/PRODUCT/SUPPORT_DATA/INPUT_DATA/cloud_albedo[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* aerosol_index_340_380 */
    description = "Aerosol absorbing index 340/380 pair";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "absorbing_aerosol_index", harp_type_float, 1, dimension_type_1d,
                                                   NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_input_aerosol_index);
    path = "data/PRODUCT/SUPPORT_DATA/INPUT_DATA/aerosol_index_340_380[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* ozone_total_column */
    description = "O3 total column";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "O3_column_number_density", harp_type_float, 1,
                                                   dimension_type_1d, NULL, description, "mol/m^2", NULL,
                                                   read_input_ozone_total_column);
    path = "data/PRODUCT/SUPPORT_DATA/INPUT_DATA/ozone_total_column";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* scene_albedo */
    description = "Effective scene albedo at 340 nm";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "scene_albedo", harp_type_float, 1, dimension_type_1d,
                                                   NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_input_scene_albedo);
    path = "data/PRODUCT/SUPPORT_DATA/INPUT_DATA/scene_albedo[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* scene_pressure */
    description = "Effective scene pressure at 340 nm";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "scene_pressure", harp_type_float, 1, dimension_type_1d,
                                                   NULL, description, "Pa", NULL, read_input_scene_pressure);
    path = "data/PRODUCT/SUPPORT_DATA/INPUT_DATA/scene_pressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

/* CLD */
static void register_cld_product(void)
{
    const char *path;
    const char *description;

    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;

    int include_validity = 1;

    harp_dimension_type dimension_type_1d[1] = { harp_dimension_time };

    const char *cld_band_option_values[3] = { "band3a", "band3c" };


    /* Product Registration Phase */

    /* 
     * harp_ingestion_register_module( 
     *          <unique_registry_key>, 
     *          <product_class>,
     *          <product_source / platform>, 
     *          <product_type>, <= ProductShortName
     *          <description for CLI listing>, 
     *          <pointer to init callback>, 
     *          <pointer to clean-up callback>
     *  );
     *
     * [Note]: <product type> is defined in get_product_type_name() at the top of this file 
     */
    module = harp_ingestion_register_module("S5_L2_CLD",
                                            "Sentinel-5",
                                            "EPS_SG",
                                            "SN5_02_CLD",
                                            "Sentinel-5 L2 CLD total column", ingestion_init, ingestion_done);

    description = "Choose which CLD band values to ingest: `band3a` (default) or `band3c`";
    harp_ingestion_register_option(module, "band",      /* option name */
                                   description, 2,      /* number of values */
                                   cld_band_option_values);     /* allowed values */

    /* harp_ingestion_register_product( module ptr, "ProductShortName", options table (NULL), dimension-callback ) */
    product_definition = harp_ingestion_register_product(module, "SN5_02_CLD", NULL, read_dimensions);

    register_core_variables_cld(product_definition, include_validity);
    register_geolocation_variables_cld(product_definition);
    register_additional_geolocation_variables_cld(product_definition);
    register_surface_variables_cld(product_definition);
    register_snow_ice_flag_variables(product_definition, "SN5_02_CLD");


    /* effective_cloud_fraction */
    description = "Effective cloud fraction";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "effective_cloud_fraction", harp_type_float,
                                                   1, dimension_type_1d, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_product_effective_cloud_fraction);

    /* default (BAND-3A) */
    path = "/data/PRODUCT_BAND3A/effective_cloud_fraction[]";
    harp_variable_definition_add_mapping(variable_definition, "band=band3a or band unset", NULL, path, NULL);

    /* alternative (BAND-3C) */
    path = "/data/PRODUCT_BAND3C/effective_cloud_fraction[]";
    harp_variable_definition_add_mapping(variable_definition, "band=band3c", NULL, path, NULL);

    description = "Effective cloud fraction precision";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "effective_cloud_fraction_uncertainty", harp_type_float,
                                                   1, dimension_type_1d, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_product_effective_cloud_fraction_uncertainty);

    path = "/data/PRODUCT_BAND3A/effective_cloud_fraction_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "band=band3a or band unset", NULL, path, NULL);

    path = "/data/PRODUCT_BAND3C/effective_cloud_fraction_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "band=band3c", NULL, path, NULL);

    description = "Cloud pressure";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "cloud_pressure", harp_type_float,
                                                   1, dimension_type_1d, NULL, description,
                                                   "Pa", NULL, read_product_cloud_pressure);

    path = "/data/PRODUCT_BAND3A/cloud_pressure[]";
    harp_variable_definition_add_mapping(variable_definition, "band=band3a or band unset", NULL, path, NULL);

    path = "/data/PRODUCT_BAND3C/cloud_pressure[]";
    harp_variable_definition_add_mapping(variable_definition, "band=band3c", NULL, path, NULL);

    /* cloud_pressure_precision */
    description = "Cloud pressure precision";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "cloud_pressure_precision", harp_type_float,
                                                   1, dimension_type_1d, NULL, description,
                                                   "Pa", NULL, read_product_cloud_pressure_uncertainty);

    path = "/data/PRODUCT_BAND3A/cloud_pressure_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "band=band3a or band unset", NULL, path, NULL);

    path = "/data/PRODUCT_BAND3C/cloud_pressure_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "band=band3c", NULL, path, NULL);

    description = "Cloud height above sea-level";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "cloud_height", harp_type_float,
                                                   1, dimension_type_1d, NULL, description,
                                                   "m", NULL, read_product_cloud_height);

    path = "/data/PRODUCT_BAND3A/cloud_height[]";
    harp_variable_definition_add_mapping(variable_definition, "band=band3a or band unset", NULL, path, NULL);

    path = "/data/PRODUCT_BAND3C/cloud_height[]";
    harp_variable_definition_add_mapping(variable_definition, "band=band3c", NULL, path, NULL);

    /* cloud_height_precision */
    description = "Cloud height above sea-level precision";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "cloud_height_precision", harp_type_float,
                                                   1, dimension_type_1d, NULL, description,
                                                   "m", NULL, read_product_cloud_height_uncertainty);

    path = "/data/PRODUCT_BAND3A/cloud_height_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "band=band3a or band unset", NULL, path, NULL);

    path = "/data/PRODUCT_BAND3C/cloud_height_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "band=band3c", NULL, path, NULL);

    /* processing_quality_flags */
    description = "Quality assurance value describing the quality of the product";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "cloud_fraction_validity", harp_type_int32,
                                                   1, dimension_type_1d, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL, read_product_qa_value);

    path = "/data/PRODUCT_BAND3A/qa_value[]";
    harp_variable_definition_add_mapping(variable_definition, "band=band3a or band unset", NULL, path, NULL);

    path = "/data/PRODUCT_BAND3C/qa_value[]";
    harp_variable_definition_add_mapping(variable_definition, "band=band3c", NULL, path, NULL);


    description = "Scene albedo";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "scene_albedo", harp_type_float,
                                                   1, dimension_type_1d, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL, read_results_scene_albedo);

    path = "data/PRODUCT_BAND3A/SUPPORT_DATA/DETAILED_RESULTS/scene_albedo[]";
    harp_variable_definition_add_mapping(variable_definition, "band=band3a or band unset", NULL, path, NULL);

    path = "/data/PRODUCT_BAND3C/SUPPORT_DATA/DETAILED_RESULTS/scene_albedo[]";
    harp_variable_definition_add_mapping(variable_definition, "band=band3c", NULL, path, NULL);

    /* scene_albedo_precision */
    description = "Scene albedo precision";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "scene_albedo_uncertainty", harp_type_float,
                                                   1, dimension_type_1d, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_results_scene_albedo_uncertainty);

    path = "/data/PRODUCT_BAND3A/SUPPORT_DATA/DETAILED_RESULTS/scene_albedo_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "band=band3a or band unset", NULL, path, NULL);

    path = "/data/PRODUCT_BAND3C/SUPPORT_DATA/DETAILED_RESULTS/scene_albedo_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "band=band3c", NULL, path, NULL);

    description = "Scene pressure";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "scene_pressure", harp_type_float,
                                                   1, dimension_type_1d, NULL, description,
                                                   "Pa", NULL, read_results_scene_pressure);

    path = "/data/PRODUCT_BAND3A/SUPPORT_DATA/DETAILED_RESULTS/scene_pressure[]";
    harp_variable_definition_add_mapping(variable_definition, "band=band3a or band unset", NULL, path, NULL);

    path = "/data/PRODUCT_BAND3C/SUPPORT_DATA/DETAILED_RESULTS/scene_pressure[]";
    harp_variable_definition_add_mapping(variable_definition, "band=band3c", NULL, path, NULL);

    /* scene_pressure_precision */
    description = "Scene pressure precision";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "scene_pressure_uncertainty", harp_type_float,
                                                   1, dimension_type_1d, NULL, description,
                                                   "Pa", NULL, read_results_scene_pressure_uncertainty);

    path = "/data/PRODUCT_BAND3A/SUPPORT_DATA/DETAILED_RESULTS/scene_pressure_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "band=band3a or band unset", NULL, path, NULL);

    path = "/data/PRODUCT_BAND3C/SUPPORT_DATA/DETAILED_RESULTS/scene_pressure_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "band=band3c", NULL, path, NULL);

    description = "Scene height above sea-level";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "scene_height", harp_type_float,
                                                   1, dimension_type_1d, NULL, description,
                                                   "m", NULL, read_results_scene_height);

    path = "/data/PRODUCT_BAND3A/SUPPORT_DATA/DETAILED_RESULTS/scene_height[]";
    harp_variable_definition_add_mapping(variable_definition, "band=band3a or band unset", NULL, path, NULL);

    path = "/data/PRODUCT_BAND3C/sSUPPORT_DATA/DETAILED_RESULTS/cene_height[]";
    harp_variable_definition_add_mapping(variable_definition, "band=band3c", NULL, path, NULL);

    /* scene_height_precision */
    description = "Scene height above sea-level precision";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "scene_height_uncertainty", harp_type_float,
                                                   1, dimension_type_1d, NULL, description,
                                                   "m", NULL, read_results_scene_height_uncertainty);

    path = "/data/PRODUCT_BAND3A/SUPPORT_DATA/DETAILED_RESULTS/scene_height_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "band=band3a or band unset", NULL, path, NULL);

    path = "/data/PRODUCT_BAND3C/SUPPORT_DATA/DETAILED_RESULTS/scene_height_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "band=band3c", NULL, path, NULL);

    description = "Cloud albedo";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "cloud_albedo", harp_type_float,
                                                   1, dimension_type_1d, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL, read_results_cloud_albedo);

    path = "/data/PRODUCT_BAND3A/SUPPORT_DATA/DETAILED_RESULTS/cloud_albedo[]";
    harp_variable_definition_add_mapping(variable_definition, "band=band3a or band unset", NULL, path, NULL);

    path = "/data/PRODUCT_BAND3C/SUPPORT_DATA/DETAILED_RESULTS/cloud_albedo[]";
    harp_variable_definition_add_mapping(variable_definition, "band=band3c", NULL, path, NULL);

    /* cloud_albedo_precision */
    description = "Cloud albedo precision";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "cloud_albedo_uncertainty", harp_type_float,
                                                   1, dimension_type_1d, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_results_cloud_albedo_uncertainty);

    path = "/data/PRODUCT_BAND3A/SUPPORT_DATA/DETAILED_RESULTS/cloud_albedo_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "band=band3a or band unset", NULL, path, NULL);

    path = "/data/PRODUCT_BAND3C/SUPPORT_DATA/DETAILED_RESULTS/cloud_albedo_precision[]";
    harp_variable_definition_add_mapping(variable_definition, "band=band3c", NULL, path, NULL);
}




/* CO */
static void register_co_product(void)
{
    const char *path;
    const char *description;

    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;

    int include_validity = 1;

    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_vertical };

    /* Product Registration Phase */

    /* 
     * harp_ingestion_register_module( 
     *          <unique_registry_key>, 
     *          <product_class>,
     *          <product_source / platform>, 
     *          <product_type>, <= ProductShortName
     *          <description for CLI listing>, 
     *          <pointer to init callback>, 
     *          <pointer to clean-up callback>
     *  );
     *
     * [Note]: <product type> is defined in get_product_type_name() at the top of this file 
     */
    module = harp_ingestion_register_module("S5_L2_CO",
                                            "Sentinel-5",
                                            "EPS_SG",
                                            "SN5_02_CO_",
                                            "Sentinel-5 L2 CO total column", ingestion_init, ingestion_done);

    /* harp_ingestion_register_product( module ptr, "ProductShortName", options table (NULL), dimension-callback ) */
    product_definition = harp_ingestion_register_product(module, "SN5_02_CO_", NULL, read_dimensions);

    /* Variables' Registration Phase */

    register_core_variables(product_definition, s5_delta_time_num_dims[s5_type_co], include_validity);
    register_geolocation_variables(product_definition);
    register_additional_geolocation_variables(product_definition);
    register_surface_variables(product_definition, "SN5_02_CO_");
    register_snow_ice_flag_variables(product_definition, "SN5_02_CO_");

    /* CO_column_number_density */
    description = "Vertically integrated CO column density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "CO_column_number_density", harp_type_float, 1,
                                                   dimension_type, NULL, description, "mol/m^2", NULL,
                                                   read_product_carbon_monoxide_total_column);
    path = "data/PRODUCT/carbon_monoxide_total_column";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* CO_column_number_density_uncertainty */
    description = "Uncertainty of the vertically integrated CO column density (standard error)";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "CO_column_number_density_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, description,
                                                   "mol/m^2",
                                                   NULL, read_product_carbon_monoxide_total_column_precision);
    path = "data/PRODUCT/carbon_monoxide_total_column_precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* CO_column_number_density_validity */
    description = "Continuous quality descriptor, varying between 0 (no data) and 100 (full quality data)";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "CO_column_number_density_validity", harp_type_int32, 1,
                                                   dimension_type, NULL, description, NULL, NULL,
                                                   read_product_qa_value);
    path = "data/PRODUCT/qa_value[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* CO_column_number_density_avk */
    description = "CO total column averaging kernel";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "CO_column_number_density_avk", harp_type_float, 2,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS,
                                                   NULL, read_co_column_number_density_avk);
    path = "data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/carbon_monoxide_total_column_averaging_kernel[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* H2O_column_number_density */
    description = "H2O total column density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "H2O_column_number_density", harp_type_float, 1,
                                                   dimension_type, NULL, description, "mol/m^2", NULL,
                                                   read_results_water_total_column);
    path = "data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/water_total_column[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* H2O_162_column_number_density */
    description = "HDO total column";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "H2O_162_column_number_density", harp_type_float, 1,
                                                   dimension_type, NULL, description, "mol/m^2", NULL,
                                                   read_results_semiheavy_water_total_column);
    path = "data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/semiheavy_water_total_column[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* CH4_column_number_density */
    description = "Non scatering CH4 total column.";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "CH4_column_number_density", harp_type_float, 1,
                                                   dimension_type, NULL, description, "mol/m2", NULL,
                                                   read_results_methane_total_column_prefit);
    path = "data/PRODUCT/SUPPORT_DATA/INPUT_DATA/methane_total_column_prefit[]";
    description = "the vertical grid is inverted to make it ascending";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* cloud_height */
    description = "Cloud centre height above the surface";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_height", harp_type_float, 1,
                                                   dimension_type, NULL, description, "m", NULL,
                                                   read_results_cloud_centre_height);
    path = "data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/cloud_centre_height[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_optical_depth */
    description = "Cloud optical depth at 2330 nm";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "cloud_optical_depth", harp_type_float, 1,
                                                   dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL, read_results_cloud_optical_depth);
    path = "data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/cloud_optical_depth[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* surface_albedo */
    description = "Surface albedo";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "surface_albedo", harp_type_float, 1, dimension_type,
                                                   NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_results_surface_albedo);
    path = "data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/surface_albedo[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* CO_column_number_density_apriori */
    description = "A-priori CO profile";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "CO_column_number_density_apriori", harp_type_float, 2,
                                                   dimension_type, NULL, description, "mol/m2",
                                                   NULL, read_results_carbon_monoxide_profile_apriori);
    description = "the vertical grid is inverted to make it ascending";
    path = "data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/carbon_monoxide_profile_apriori[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* CH4_column_number_density_apriori */
    description = "A-priori CH4 profile";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "CH4_column_number_density_apriori", harp_type_float,
                                                   2, dimension_type, NULL, description, "mol/m2", NULL,
                                                   read_results_methane_profile_apriori);
    path = "data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/methane_profile_apriori[]";
    description = "the vertical grid is inverted to make it ascending";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* dry_air_column_number_density */
    description = "Column number density profile of dry air";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "dry_air_column_number_density", harp_type_float, 1,
                                                   dimension_type, NULL, description, "mol/m2", NULL,
                                                   read_results_dry_air_column);
    path = "data/PRODUCT/SUPPORT_DATA/DETAILED_RESULTS/dry_air_column[]";
    description = "the vertical grid is inverted to make it ascending";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);
}


int harp_ingestion_module_s5_l2_init(void)
{

    register_aui_product();
    register_ch4_product();
    register_no2_product();
    register_o3_product();
    register_so2_product();
    register_cld_product();
    register_co_product();

    return 0;
}
