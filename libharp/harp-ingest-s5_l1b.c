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
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* Macro to determine the number of elements in a one dimensional C array. */
#define ARRAY_SIZE(X) (sizeof((X))/sizeof((X)[0]))

/* Maximum length of a path string in generated mapping descriptions. */
#define MAX_PATH_LENGTH 256

typedef enum s5_product_type_enum
{
    s5_type_uvr,
    s5_type_nir,
    s5_type_swr,
    s5_type_irr,
} s5_product_type;

#define S5_NUM_PRODUCT_TYPES (((int)s5_type_irr) + 1)


typedef enum s5_dimension_type_enum
{
    s5_dim_scanline,    /* original along-track dimension */
    s5_dim_pixel,       /* original across-track dimension */
    s5_dim_corner,      /* 4 polygon corners per ground pixel */
    s5_dim_spectral,    /* extra wavelengths (e.g. spectral_channel) */
} s5_dimension_type;

/* handy constant: last enum value + 1 */
#define S5_NUM_DIM_TYPES   ((int)s5_dim_spectral + 1)

static const char *s5_dimension_name[S5_NUM_PRODUCT_TYPES][S5_NUM_DIM_TYPES] = {
    {"scanline", "ground_pixel", "pixel_corners", "spectral_channel"},  /* UVR */
    {"scanline", "ground_pixel", "pixel_corners", "spectral_channel"},  /* NIR */
    {"scanline", "ground_pixel", "pixel_corners", "spectral_channel"},  /* SWR */
    {"scanline", "pixel", NULL, "spectral_channel"},    /* IRR */
};

typedef struct ingest_info_struct
{
    coda_product *product;

    coda_cursor product_cursor; /* /data/band... */
    coda_cursor geolocation_cursor;     /* /data/band.../geolocation_data */
    coda_cursor instrument_cursor;
    coda_cursor observation_cursor;

    coda_cursor observable_cursor;      /* points at radiance|irradiance */
    coda_cursor observable_error_cursor;        /* points at *error dataset */
    coda_cursor observable_noise_cursor;        /* points at *noise dataset */

    coda_cursor sensor_mode_cursor;
    coda_cursor geo_data_cursor;

    int use_band_option;
    int use_calibrated_coeff;   /* whether to use calibrated or nominal coefficients */

    s5_product_type product_type;
    long num_scanlines;
    long num_pixels;
    long num_corners;
    long num_spectral;

    int processor_version;
    int collection_number;

    harp_scalar observable_fill_value;
    harp_scalar observable_error_fill_value;
    harp_scalar observable_noise_fill_value;

    uint8_t *surface_layer_status;
} ingest_info;


/* The routines start here
 */

static const char *get_product_type_name(s5_product_type product_type)
{
    switch (product_type)
    {
        case s5_type_uvr:
            return "SN5_1B_UVR";
        case s5_type_nir:
            return "SN5_1B_NIR";
        case s5_type_swr:
            return "SN5_1B_SWR";
        case s5_type_irr:
            return "SN5_1B_IRR";
    }

    assert(0);
    exit(1);
}

/* Tiny helper for get_product_type() */
static void dash_to_underscore(char *s)
{
    /* use size_t for byte offsets into the char array */
    size_t i;

    /* Changing '-' to '_' */
    for (i = 0; s[i] != '\0'; ++i)
    {
        if (s[i] == '-')
        {
            s[i] = '_';
        }
    }
}

static void broadcast_array_int8(long num_scanlines, long num_pixels, int8_t *data)
{
    long i;

    /* Repeat the value for each scanline for all pixels in that scanline. Iterate in reverse to avoid overwriting
     * scanline values.
     */
    for (i = num_scanlines - 1; i >= 0; i--)
    {
        long j;

        for (j = 0; j < num_pixels; j++)
        {
            data[i * num_pixels + j] = data[i];
        }
    }
}

static void broadcast_array_int16(long num_scanlines, long num_pixels, int16_t *data)
{
    long i;

    /* Repeat the value for each scanline for all pixels in that scanline. Iterate in reverse to avoid overwriting
     * scanline values.
     */
    for (i = num_scanlines - 1; i >= 0; i--)
    {
        long j;

        for (j = 0; j < num_pixels; j++)
        {
            data[i * num_pixels + j] = data[i];
        }
    }
}

static void broadcast_array_float(long num_scanlines, long num_pixels, float *data)
{
    long i;

    /* Repeat the value for each scanline for all pixels in that scanline. Iterate in reverse to avoid overwriting
     * scanline values.
     */
    for (i = num_scanlines - 1; i >= 0; i--)
    {
        long j;

        for (j = 0; j < num_pixels; j++)
        {
            data[i * num_pixels + j] = data[i];
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
    {
        return harp_set_error(HARP_ERROR_CODA, NULL), -1;
    }

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
        {
            return harp_set_error(HARP_ERROR_CODA, NULL), -1;
        }

        if (tc == coda_array_class)
        {
            child = cursor;
            if (coda_cursor_goto_first_array_element(&child) != 0)
            {
                return harp_set_error(HARP_ERROR_CODA, NULL), -1;
            }
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
    {
        return harp_set_error(HARP_ERROR_CODA, NULL), -1;
    }

    /* 4. normalise and show */
    dash_to_underscore(buf);

    /* 5. search for any known short code */
    for (i = 0; i < S5_NUM_PRODUCT_TYPES; i++)
    {
        const char *code = get_product_type_name((s5_product_type)i);   /* e.g. "SN5_1B_NIR" */

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
    char *curr_band;

    curr_band = NULL;

    /* Choosing the apropriate dataset based on the option chosen */
    if (info->product_type == s5_type_uvr)
    {
        if (info->use_band_option == 0)
        {
            curr_band = "band1a";
        }
        else if (info->use_band_option == 1)
        {
            curr_band = "band1b";
        }
        else if (info->use_band_option == 2)
        {
            curr_band = "band2";
        }
        else
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
    }
    else if (info->product_type == s5_type_nir)
    {
        if (info->use_band_option == 0)
        {
            curr_band = "band3a";
        }
        else if (info->use_band_option == 1)
        {
            curr_band = "band3b";
        }
        else if (info->use_band_option == 2)
        {
            curr_band = "band3c";
        }
        else
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
    }
    else if (info->product_type == s5_type_swr)
    {
        if (info->use_band_option == 0)
        {
            curr_band = "band4";
        }
        else if (info->use_band_option == 1)
        {
            curr_band = "band5";
        }
        else
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
    }
    else if (info->product_type == s5_type_irr)
    {
        if (info->use_band_option == 0)
        {
            curr_band = "band1a";
        }
        else if (info->use_band_option == 1)
        {
            curr_band = "band1b";
        }
        else if (info->use_band_option == 2)
        {
            curr_band = "band2";
        }
        else if (info->use_band_option == 3)
        {
            curr_band = "band3a";
        }
        else if (info->use_band_option == 4)
        {
            curr_band = "band3b";
        }
        else if (info->use_band_option == 5)
        {
            curr_band = "band3c";
        }
        else if (info->use_band_option == 6)
        {
            curr_band = "band4";
        }
        else if (info->use_band_option == 7)
        {
            curr_band = "band5";
        }
        else
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
    }


    /* Bind a cursor to the root of the CODA product */
    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    /* Products has to set of bands each containing its own product type */
    if (coda_cursor_goto_record_field_by_name(&cursor, curr_band) != 0)
    {
        /* Fallback to data/band* for simulated files */
        if (coda_cursor_goto_record_field_by_name(&cursor, "data") != 0 ||
            coda_cursor_goto_record_field_by_name(&cursor, curr_band) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
    }
    /* Save data/band* cursor; subsequent navigation is relative to this. */
    info->product_cursor = cursor;

    /* Geolocation group: under band*
     * '/data/band.../geolocation_data' for both layouts.
     */
    if (coda_cursor_goto_record_field_by_name(&cursor, "geolocation_data") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    info->geolocation_cursor = cursor;

    /* Back to data/band* */
    coda_cursor_goto_parent(&cursor);

    /* Instrument data: '/data/band.../instrument_data' */
    if (coda_cursor_goto_record_field_by_name(&cursor, "instrument_data") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    info->instrument_cursor = cursor;

    /* Back to data/band* */
    coda_cursor_goto_parent(&cursor);

    /* Observation data: '/data/band.../observation_data' */
    if (coda_cursor_goto_record_field_by_name(&cursor, "observation_data") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    info->observation_cursor = cursor;



    return 0;
}

/* Initialize record dimension lengths for the Sentinel-5 simulated L1b dataset */
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

    /* Get number of spectral channels and validate */
    if (s5_dimension_name[info->product_type][s5_dim_spectral] != NULL)
    {
        if (get_dimension_length(info, s5_dimension_name[info->product_type][s5_dim_spectral], &info->num_spectral) !=
            0)
        {
            return -1;
        }
    }

    return 0;
}

/* From S5P L1b module */
static int init_dataset(coda_cursor cursor, const char *name, long num_elements, coda_cursor *new_cursor,
                        harp_scalar *fill_value)
{
    long coda_num_elements;

    if (coda_cursor_goto_record_field_by_name(&cursor, name) != 0)
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
    if (coda_cursor_goto(&cursor, "@FillValue[0]") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    if (coda_cursor_read_float(&cursor, &fill_value->float_data) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    coda_cursor_goto_parent(&cursor);
    coda_cursor_goto_parent(&cursor);
    coda_cursor_goto_parent(&cursor);

    *new_cursor = cursor;

    return 0;
}


/* Extract Sentinel-5 L1b product collection and processor version
 * from the global "logical product name".
 */
static int init_versions(ingest_info *info)
{
    coda_cursor cursor;
    char product_name[84];

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
    info->surface_layer_status = NULL;

    /* Dimensions */
    info->num_scanlines = 0;
    info->num_pixels = 0;
    info->num_corners = 0;
    info->num_spectral = 0;


    /* Each product has its own bands, which we convert into options */
    info->use_band_option = 0;
    info->use_calibrated_coeff = 1;


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

    if (info->product_type == s5_type_uvr)
    {
        if (harp_ingestion_options_has_option(options, "band"))
        {
            if (harp_ingestion_options_get_option(options, "band", &option_value) != 0)
            {
                ingestion_done(info);
                return -1;
            }

            if (strcmp(option_value, "1b") == 0)
            {
                info->use_band_option = 1;
            }
            else if (strcmp(option_value, "2") == 0)
            {
                info->use_band_option = 2;
            }
            else
            {
                /* Option values are guaranteed to be legal if present. */
                assert(strcmp(option_value, "1a") == 0);
                info->use_band_option = 0;
            }
        }
    }
    else if (info->product_type == s5_type_nir)
    {
        if (harp_ingestion_options_has_option(options, "band"))
        {
            if (harp_ingestion_options_get_option(options, "band", &option_value) != 0)
            {
                ingestion_done(info);
                return -1;
            }

            if (strcmp(option_value, "3b") == 0)
            {
                info->use_band_option = 1;
            }
            else if (strcmp(option_value, "3c") == 0)
            {
                info->use_band_option = 2;
            }
            else
            {
                /* Option values are guaranteed to be legal if present. */
                assert(strcmp(option_value, "3a") == 0);
                info->use_band_option = 0;
            }
        }
    }
    else if (info->product_type == s5_type_swr)
    {
        if (harp_ingestion_options_has_option(options, "band"))
        {
            if (harp_ingestion_options_get_option(options, "band", &option_value) != 0)
            {
                ingestion_done(info);
                return -1;
            }

            if (strcmp(option_value, "5") == 0)
            {
                info->use_band_option = 1;
            }
            else
            {
                /* Option values are guaranteed to be legal if present. */
                assert(strcmp(option_value, "4") == 0);
                info->use_band_option = 0;
            }
        }
    }
    else if (info->product_type == s5_type_irr)
    {
        if (harp_ingestion_options_has_option(options, "band"))
        {
            if (harp_ingestion_options_get_option(options, "band", &option_value) != 0)
            {
                ingestion_done(info);
                return -1;
            }

            if (strcmp(option_value, "1b") == 0)
            {
                info->use_band_option = 1;
            }
            else if (strcmp(option_value, "2") == 0)
            {
                info->use_band_option = 2;
            }
            else if (strcmp(option_value, "3a") == 0)
            {
                info->use_band_option = 3;
            }
            else if (strcmp(option_value, "3b") == 0)
            {
                info->use_band_option = 4;
            }
            else if (strcmp(option_value, "3c") == 0)
            {
                info->use_band_option = 5;
            }
            else if (strcmp(option_value, "4") == 0)
            {
                info->use_band_option = 6;
            }
            else if (strcmp(option_value, "5") == 0)
            {
                info->use_band_option = 7;
            }
            else
            {
                /* Option values are guaranteed to be legal if present. */
                assert(strcmp(option_value, "1a") == 0);
                info->use_band_option = 0;
            }
        }
    }


    /* For calculating wavelengths from the wavelenght coefficients */
    if (harp_ingestion_options_has_option(options, "lambda"))
    {
        if (harp_ingestion_options_get_option(options, "lambda", &option_value) != 0)
        {
            ingestion_done(info);
            return -1;
        }

        if (strcmp(option_value, "nominal") == 0)
        {
            info->use_calibrated_coeff = 0;
        }
        else
        {
            /* Option values are guaranteed to be legal if present. */
            assert(strcmp(option_value, "calibrated") == 0);
            info->use_calibrated_coeff = 1;
        }
    }


    if (init_cursors(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }


    /* Getting input product dimensios */
    if (init_dimensions(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }

    /* to decode the uncertainties for radiance|irradiance */
    if (info->product_type == s5_type_irr)
    {
        if (init_dataset
            (info->observation_cursor, "irradiance", info->num_scanlines * info->num_pixels * info->num_spectral,
             &info->observable_cursor, &info->observable_fill_value) != 0)
        {
            ingestion_done(info);
            return -1;
        }
        if (init_dataset(info->observation_cursor, "irradiance_error",
                         info->num_scanlines * info->num_pixels * info->num_spectral, &info->observable_error_cursor,
                         &info->observable_error_fill_value) != 0)
        {
            ingestion_done(info);
            return -1;
        }
        if (init_dataset(info->observation_cursor, "irradiance_noise",
                         info->num_scanlines * info->num_pixels * info->num_spectral, &info->observable_noise_cursor,
                         &info->observable_noise_fill_value) != 0)
        {
            ingestion_done(info);
            return -1;
        }
    }
    else
    {
        if (init_dataset
            (info->observation_cursor, "radiance", info->num_scanlines * info->num_pixels * info->num_spectral,
             &info->observable_cursor, &info->observable_fill_value) != 0)
        {
            ingestion_done(info);
            return -1;
        }
        if (init_dataset(info->observation_cursor, "radiance_error",
                         info->num_scanlines * info->num_pixels * info->num_spectral, &info->observable_error_cursor,
                         &info->observable_error_fill_value) != 0)
        {
            ingestion_done(info);
            return -1;
        }
        if (init_dataset(info->observation_cursor, "radiance_noise",
                         info->num_scanlines * info->num_pixels * info->num_spectral, &info->observable_noise_cursor,
                         &info->observable_noise_fill_value) != 0)
        {
            ingestion_done(info);
            return -1;
        }
    }

    *user_data = info;

    return 0;
}


/* Reading Routines */

static int read_dimensions(void *user_data, long dimension[HARP_NUM_DIM_TYPES])
{
    ingest_info *info = (ingest_info *)user_data;

    dimension[harp_dimension_time] = info->num_scanlines * info->num_pixels;
    dimension[harp_dimension_spectral] = info->num_spectral;

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
        case harp_type_int16:
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
                if (read_type == coda_native_type_uint16)
                {
                    if (coda_cursor_read_uint16_array(&cursor, (uint16_t *)data.int16_data, coda_array_ordering_c) != 0)
                    {
                        harp_set_error(HARP_ERROR_CODA, NULL);
                        return -1;
                    }
                }
                else
                {
                    if (coda_cursor_read_int16_array(&cursor, data.int16_data, coda_array_ordering_c) != 0)
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
    harp_array time_reference_array;
    double time_reference;
    long i, j;

    time_reference_array.ptr = &time_reference;
    if (read_dataset(info->observation_cursor, "time", harp_type_double, 1, time_reference_array) != 0)
    {
        return -1;
    }

    if (read_dataset(info->observation_cursor, "delta_time", harp_type_double, info->num_scanlines, data) != 0)
    {
        return -1;
    }

    for (i = info->num_scanlines - 1; i >= 0; i--)
    {
        data.double_data[i] += time_reference * 24 * 60 * 60;
        /* replicate for each pixel in the scanline */
        for (j = 0; j <= info->num_pixels; j++)
        {
            data.double_data[i * info->num_pixels + j] = data.double_data[i];
        }
    }

    return 0;
}

static int read_datetime_length(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    coda_cursor cursor = info->observation_cursor;
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
    if (coda_cursor_goto_next_array_element(&cursor) != 0)
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

/* Field: data/band.../geolocation_data */

static int read_geolocation_latitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->geolocation_cursor, "latitude", harp_type_float, info->num_scanlines * info->num_pixels,
                        data);
}

static int read_geolocation_longitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->geolocation_cursor, "longitude", harp_type_float, info->num_scanlines * info->num_pixels,
                        data);
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

/* Observation variables */

static int read_observation_measurement_quality(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (read_dataset(info->observation_cursor, "measurement_quality", harp_type_int16, info->num_scanlines, data) != 0)
    {
        return -1;
    }

    broadcast_array_int16(info->num_scanlines, info->num_pixels, data.int16_data);

    return 0;
}

static int read_observation_radiance(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (read_dataset(info->observation_cursor, "radiance", harp_type_float,
                     info->num_scanlines * info->num_pixels * info->num_spectral, data) != 0)
    {
        return -1;
    }

    return 0;
}

static int decode_uncertainty(ingest_info *info, const char *error_var_name, const char *obs_var_name,
                              harp_array sigma_out)
{
    long n = (long)info->num_scanlines * info->num_pixels * info->num_spectral;

    /* scratch buffers – allocated on first use, kept for life of product */
    static int8_t *enc = NULL;
    static float *obs = NULL;
    static long buf_size = 0;

    harp_array enc_arr;
    harp_array obs_arr;

    int8_t fill_E;
    float fill_R;

    if (buf_size < n)
    {
        enc = realloc(enc, n * sizeof(int8_t)); /* encoded bytes */
        obs = realloc(obs, n * sizeof(float));  /* radiance|irradiance */
        if (enc == NULL || obs == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "unable to allocate decode buffers");
            return -1;
        }
        buf_size = n;
    }


    enc_arr.int8_data = enc;
    obs_arr.float_data = obs;

    /* reading the uncertainty */
    if (read_dataset(info->observation_cursor, error_var_name, harp_type_int8, n, enc_arr) != 0)
    {
        return -1;
    }

    /* reading the radiance|irradiance */
    if (read_dataset(info->observation_cursor, obs_var_name, harp_type_float, n, obs_arr) != 0)
    {
        return -1;
    }

    broadcast_array_int8(info->num_scanlines, info->num_pixels, enc);
    broadcast_array_float(info->num_scanlines, info->num_pixels, obs);

    if (strcmp(error_var_name, "radiance_error") == 0 || strcmp(error_var_name, "irradiance_error") == 0)
    {
        fill_E = info->observable_error_fill_value.int8_data;
    }
    else if (strcmp(error_var_name, "radiance_noise") == 0 || strcmp(error_var_name, "irradiance_noise") == 0)
    {
        fill_E = info->observable_noise_fill_value.int8_data;
    }

    fill_R = info->observable_fill_value.float_data;

    /* decode slice */
    for (long i = 0; i < n; i++)
    {
        int8_t E = enc[i];
        float R = obs[i];

        if (E == fill_E || R == fill_R)
        {
            sigma_out.float_data[i] = harp_nan();
        }
        else
        {
            sigma_out.float_data[i] = fabsf(R / expf((float)E / 20.0f));
        }
    }
    return 0;
}

static int read_observation_radiance_error(void *user_data, harp_array data)
{
    return decode_uncertainty((ingest_info *)user_data, "radiance_error", "radiance", data);
}

static int read_observation_radiance_noise(void *user_data, harp_array data)
{
    return decode_uncertainty((ingest_info *)user_data, "radiance_noise", "radiance", data);
}


static int read_observation_spectral_channel_quality(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (read_dataset(info->observation_cursor, "spectral_channel_quality", harp_type_int8,
                     info->num_scanlines * info->num_pixels * info->num_spectral, data) != 0)
    {
        return -1;
    }

    broadcast_array_int8(info->num_scanlines, info->num_pixels, data.int8_data);

    return 0;
}

static int read_observation_irradiance(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (read_dataset(info->observation_cursor, "irradiance", harp_type_float,
                     info->num_scanlines * info->num_pixels * info->num_spectral, data) != 0)
    {
        return -1;
    }

    return 0;
}

static int read_observation_irradiance_error(void *user_data, harp_array data)
{
    return decode_uncertainty((ingest_info *)user_data, "irradiance_error", "irradiance", data);
}

static int read_observation_irradiance_noise(void *user_data, harp_array data)
{
    return decode_uncertainty((ingest_info *)user_data, "irradiance_noise", "irradiance", data);
}

/* Instrument variables */

static int read_instrument_wavelength(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    float *lambda = data.float_data;    /* end wavelengths' array */
    const long L = info->num_spectral - 1;      /* end counter    */
    const double invL = 1.0 / L;        /* inverse of L */
    long s, p, k;       /* loop counters */
    const long coeff_count = info->num_scanlines * info->num_pixels * 4;
    harp_array coeff_array;
    float *cheb_coeff;
    const char *var_name;

    cheb_coeff = malloc(coeff_count * sizeof(float));
    if (cheb_coeff == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       coeff_count * sizeof(float), __FILE__, __LINE__);
        return -1;
    }

    coeff_array.float_data = cheb_coeff;

    if (info->use_calibrated_coeff == 1)
    {
        var_name = "calibrated_wavelength_coefficients";
    }
    else
    {
        var_name = "nominal_wavelength_coefficients";
    }

    if (read_dataset(info->instrument_cursor, var_name, harp_type_float, coeff_count, coeff_array) != 0)
    {
        free(cheb_coeff);
        return -1;
    }

    /* Evaluate lambda(scanline,pixel,k) */
    for (s = 0; s < info->num_scanlines; s++)
    {
        for (p = 0; p < info->num_pixels; p++)
        {
            const float *a = &cheb_coeff[(s * info->num_pixels + p) * 4];       /* a0..a3 */
            const long base = (s * info->num_pixels + p) * info->num_spectral;

            for (k = 0; k < info->num_spectral; k++)
            {
                const double xi = 2.0 * (double)k * invL - 1.0;

                /* Chebyshev basis (order-3) */
                const double T0 = 1.0;
                const double T1 = xi;
                const double T2 = 2.0 * xi * xi - 1.0;
                const double T3 = (4.0 * xi * xi - 3.0) * xi;

                lambda[base + k] = (float)(a[0] * T0 + a[1] * T1 + a[2] * T2 + a[3] * T3);
            }
        }
    }

    free(cheb_coeff);
    return 0;
}

static int read_instrument_wavelength_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    const long L = info->num_spectral - 1;
    const float invL = 1.0f / (float)L;
    long s, p, k;
    const char *var_name;
    harp_array tmp;
    const long count = info->num_scanlines * info->num_pixels * 4;
    float *sig_a;

    sig_a = malloc(count * sizeof(float));
    if (sig_a == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       count * sizeof(float), __FILE__, __LINE__);
        return -1;
    }

    tmp.float_data = sig_a;

    if (info->use_calibrated_coeff)
    {
        var_name = "calibrated_wavelength_coefficients_error";
    }
    else
    {
        var_name = "nominal_wavelength_coefficients_error";
    }


    if (read_dataset(info->instrument_cursor, var_name, harp_type_float, count, tmp) != 0)
    {
        free(sig_a);
        return -1;
    }

    /* sigma */
    float *sig_l = data.float_data;

    for (s = 0; s < info->num_scanlines; s++)
    {
        for (p = 0; p < info->num_pixels; p++)
        {
            const float *sa = &sig_a[(s * info->num_pixels + p) * 4];
            const long base = (s * info->num_pixels + p) * info->num_spectral;

            for (k = 0; k < info->num_spectral; k++)
            {
                const double xi = 2.0 * (double)k * invL - 1.0;

                const double T0 = 1.0;
                const double T1 = xi;
                const double T2 = 2.0 * xi * xi - 1.0;
                const double T3 = (4.0 * xi * xi - 3.0) * xi;

                /* variance */
                double var = T0 * T0 * sa[0] * sa[0] + T1 * T1 * sa[1] * sa[1] + T2 * T2 * sa[2] * sa[2] +
                    T3 * T3 * sa[3] * sa[3];

                sig_l[base + k] = (float)sqrt(var);
            }
        }
    }

    free(sig_a);
    return 0;
}

static int read_instrument_spectral_calibration_quality(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->instrument_cursor, "spectral_calibration_quality", harp_type_int16,
                        info->num_scanlines * info->num_pixels, data);
}

/*
 * Products' Registration Routines
 */

static void register_mapping_per_band(harp_variable_definition *variable_definition, const char *variable_name,
                                      const char *dataset_name, const char *bands_list[], const char *bands_list_map[],
                                      int num_bands, const char *description)
{
    int i;
    char path[MAX_PATH_LENGTH];


    for (i = 0; i < num_bands; i++)
    {
        if (strcmp(variable_name, "datetime[]") == 0)
        {
            snprintf(path, MAX_PATH_LENGTH, "/data/%s/%s/time, /data/%s/%s/delta_time[]", bands_list[i], dataset_name,
                     bands_list[i], dataset_name);
            harp_variable_definition_add_mapping(variable_definition, bands_list_map[i], NULL, path, description);
        }
        else
        {
            snprintf(path, MAX_PATH_LENGTH, "/data/%s/%s/%s", bands_list[i], dataset_name, variable_name);
            harp_variable_definition_add_mapping(variable_definition, bands_list_map[i], NULL, path, description);
        }
    }
}

static void register_geolocation_variables(harp_product_definition
                                           *product_definition, const char *bands_list[],
                                           const char *bands_list_map[], int num_bands)
{
    const char *var_name;
    const char *description;

    harp_variable_definition *variable_definition;

    harp_dimension_type dimension_type_1d[1] = { harp_dimension_time };
    harp_dimension_type dimension_type_2d[2] = { harp_dimension_time, harp_dimension_independent };
    long bounds_dimension[2] = { -1, 4 };

    /* latitude */
    description = "Latitude of the center of each ground pixel on the WGS84 reference ellipsoid.";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_float, 1,
                                                   dimension_type_1d, NULL, description, "degree_north", NULL,
                                                   read_geolocation_latitude);
    harp_variable_definition_set_valid_range_float(variable_definition, -90.0f, 90.0f);

    var_name = "latitude[]";
    description = NULL;
    register_mapping_per_band(variable_definition, var_name, "geolocation_data", bands_list, bands_list_map, num_bands,
                              description);

    /* longitude */
    description = "Longitude of the center of each ground pixel on the WGS84 reference ellipsoid.";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_float, 1,
                                                   dimension_type_1d, NULL, description, "degree_east", NULL,
                                                   read_geolocation_longitude);
    harp_variable_definition_set_valid_range_float(variable_definition, -180.0f, 180.0f);
    var_name = "longitude[]";
    description = NULL;
    register_mapping_per_band(variable_definition, var_name, "geolocation_data", bands_list, bands_list_map, num_bands,
                              description);


    /* latitude_bounds */
    description = "The four latitude boundaries of each ground pixel on the WGS84 reference ellipsoid.";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "latitude_bounds", harp_type_float, 2,
                                                   dimension_type_2d, bounds_dimension, description, "degree_north",
                                                   NULL, read_geolocation_latitude_bounds);
    harp_variable_definition_set_valid_range_float(variable_definition, -90.0f, 90.0f);
    var_name = "latitude_bounds[]";
    description = NULL;
    register_mapping_per_band(variable_definition, var_name, "geolocation_data", bands_list, bands_list_map, num_bands,
                              description);

    /* longitude_bounds */
    description = "The four longitude boundaries of each ground pixel on the WGS84 reference ellipsoid.";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "longitude_bounds", harp_type_float, 2,
                                                   dimension_type_2d, bounds_dimension, description, "degree_east",
                                                   NULL, read_geolocation_longitude_bounds);
    harp_variable_definition_set_valid_range_float(variable_definition, -180.0f, 180.0f);
    var_name = "longitude_bounds[]";
    description = NULL;
    register_mapping_per_band(variable_definition, var_name, "geolocation_data", bands_list, bands_list_map, num_bands,
                              description);

    /* sensor_altitude */
    description = "The altitude of the spacecraft relative to the WGS84 reference ellipsoid.";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "sensor_altitude", harp_type_float, 1,
                                                   dimension_type_1d, NULL, description,
                                                   "m", NULL, read_geolocation_satellite_altitude);

    var_name = "satellite_altitude[]";
    description = "the satellite altitude associated with a scanline is " "repeated for each pixel in the scanline";
    register_mapping_per_band(variable_definition, var_name, "geolocation_data", bands_list, bands_list_map, num_bands,
                              description);

    /* sensor_latitude */
    description = "Latitude of the spacecraft sub-satellite point on the WGS84 reference ellipsoid.";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "sensor_latitude", harp_type_float, 1,
                                                   dimension_type_1d, NULL, description, "degree_north", NULL,
                                                   read_geolocation_satellite_latitude);
    harp_variable_definition_set_valid_range_float(variable_definition, -90.0f, 90.0f);
    var_name = "satellite_latitude[]";
    description = "the satellite latitude associated with a scanline is repeated for each pixel in the scanline";
    register_mapping_per_band(variable_definition, var_name, "geolocation_data", bands_list, bands_list_map, num_bands,
                              description);

    /* sensor_longitude */
    description = "Longitude of the spacecraft sub-satellite point on the WGS84 reference ellipsoid.";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "sensor_longitude", harp_type_float, 1,
                                                   dimension_type_1d, NULL, description, "degree_east", NULL,
                                                   read_geolocation_satellite_longitude);
    harp_variable_definition_set_valid_range_float(variable_definition, -180.0f, 180.0f);
    var_name = "satellite_longitude[]";
    description = "the satellite longitude associated with a scanline is repeated for each pixel in the scanline";
    register_mapping_per_band(variable_definition, var_name, "geolocation_data", bands_list, bands_list_map, num_bands,
                              description);

    /* solar_zenith_angle */
    description = "Zenith angle of the sun at the ground pixel location on the WGS84 reference ellipsoid.";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "solar_zenith_angle", harp_type_float, 1,
                                                   dimension_type_1d, NULL, description, "degree", NULL,
                                                   read_geolocation_solar_zenith_angle);
    harp_variable_definition_set_valid_range_float(variable_definition, 0.0f, 180.0f);
    var_name = "solar_zenith_angle[]";
    description = NULL;
    register_mapping_per_band(variable_definition, var_name, "geolocation_data", bands_list, bands_list_map, num_bands,
                              description);

    /* solar_azimuth_angle */
    description = "Azimuth angle of the sun at the ground pixel location on the WGS84 ellipsoid.";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "solar_azimuth_angle", harp_type_float, 1,
                                                   dimension_type_1d, NULL, description, "degree", NULL,
                                                   read_geolocation_solar_azimuth_angle);
    harp_variable_definition_set_valid_range_float(variable_definition, -180.0f, 180.0f);

    var_name = "solar_azimuth_angle[]";
    description = NULL;
    register_mapping_per_band(variable_definition, var_name, "geolocation_data", bands_list, bands_list_map, num_bands,
                              description);

    /* viewing_zenith_angle */
    description = "Zenith angle of the spacecraft at the ground pixel location on the WGS84 reference ellipsoid.";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "sensor_zenith_angle", harp_type_float, 1,
                                                   dimension_type_1d, NULL, description, "degree", NULL,
                                                   read_geolocation_viewing_zenith_angle);
    harp_variable_definition_set_valid_range_float(variable_definition, 0.0f, 180.0f);

    var_name = "viewing_zenith_angle[]";
    description = NULL;
    register_mapping_per_band(variable_definition, var_name, "geolocation_data", bands_list, bands_list_map, num_bands,
                              description);

    /* viewing_azimuth_angle */
    description = "Azimuth angle of the spacecraft at the ground pixel location on the WGS84 reference ellipsoid.";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "sensor_azimuth_angle", harp_type_float, 1,
                                                   dimension_type_1d, NULL, description, "degree", NULL,
                                                   read_geolocation_viewing_azimuth_angle);
    harp_variable_definition_set_valid_range_float(variable_definition, -180.0f, 180.0f);

    var_name = "viewing_azimuth_angle[]";
    description = NULL;
    register_mapping_per_band(variable_definition, var_name, "geolocation_data", bands_list, bands_list_map, num_bands,
                              description);
}


static void register_observation_variables(harp_product_definition
                                           *product_definition, const char *bands_list[],
                                           const char *bands_list_map[], int num_bands)
{
    const char *var_name;
    const char *description;

    harp_variable_definition *variable_definition;

    harp_dimension_type dimension_type_1d[1] = { harp_dimension_time };
    harp_dimension_type dimension_type_2d_spec[2] = { harp_dimension_time, harp_dimension_spectral };

    /* validity */
    description = "Overall quality information for a measurement.";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "validity", harp_type_int16, 1,
                                                   dimension_type_1d, NULL, description, NULL, NULL,
                                                   read_observation_measurement_quality);

    var_name = "measurement_quality[]";
    description = "the measurement quality associated with a scanline is repeated for each pixel in the scanline";
    register_mapping_per_band(variable_definition, var_name, "observation_data", bands_list, bands_list_map, num_bands,
                              description);

    /* datetime */
    description = "time of the measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double, 1,
                                                   dimension_type_1d, NULL, description, "seconds since 2020-01-01",
                                                   NULL, read_datetime);

    var_name = "datetime[]";
    description = "time converted from days since 2020-01-01 to seconds since 2020-01-01 (using 86400 seconds per "
        "day) and delta_time added; the time associated with a scanline is repeated for each pixel in the scanline";
    register_mapping_per_band(variable_definition, var_name, "observation_data", bands_list, bands_list_map, num_bands,
                              description);

    /* datetime_length */
    description = "measurement duration";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime_length", harp_type_double, 0, NULL,
                                                   NULL, description, "s", NULL, read_datetime_length);
    register_mapping_per_band(variable_definition, var_name, "delta_time", bands_list, bands_list_map, num_bands,
                              "delta_time[1] - delta_time[0]");

    /* photon_radiance */
    description = "measured spectral photon radiance for each spectral channel";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "photon_radiance", harp_type_float, 2,
                                                   dimension_type_2d_spec, NULL, description, "mol/(s.m^2.nm.sr)", NULL,
                                                   read_observation_radiance);
    var_name = "radiance[]";
    register_mapping_per_band(variable_definition, var_name, "observation_data", bands_list, bands_list_map, num_bands,
                              NULL);

    /* photon_radiance_uncertainty_systematic */
    description = "spectral radiance systematic uncertainty";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "photon_radiance_uncertainty_systematic",
                                                   harp_type_float, 2, dimension_type_2d_spec, NULL, description,
                                                   "mol/(s.m^2.nm.sr)", NULL, read_observation_radiance_error);
    var_name = "radiance_error[]";
    description = "uncertainty = abs(radiance / exp(radiance_error / 20))";
    register_mapping_per_band(variable_definition, var_name, "observation_data", bands_list, bands_list_map, num_bands,
                              description);

    /* photon_radiance_uncertainty_random */
    description = "spectral radiance random uncertainty";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "photon_radiance_uncertainty_random",
                                                   harp_type_float, 2, dimension_type_2d_spec, NULL, description,
                                                   "mol/(s.m^2.nm.sr)", NULL, read_observation_radiance_noise);
    var_name = "radiance_noise[]";
    description = "uncertainty = abs(radiance / exp(radiance_noise / 20))";
    register_mapping_per_band(variable_definition, var_name, "observation_data", bands_list, bands_list_map, num_bands,
                              description);

    /* photon_radiance_validity */
    description = "Quality assessment information for each (spectral) channel.";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "photon_radiance_validity", harp_type_int8, 2,
                                                   dimension_type_2d_spec, NULL, description, NULL, NULL,
                                                   read_observation_spectral_channel_quality);
    var_name = "spectral_channel_quality[]";
    description = NULL;
    register_mapping_per_band(variable_definition, var_name, "observation_data", bands_list, bands_list_map, num_bands,
                              description);
}

static void register_instrument_variables(harp_product_definition
                                          *product_definition, const char *bands_list[],
                                          const char *bands_list_map[], int num_bands)
{
    const char *description;
    const char *var_name;
    char path[MAX_PATH_LENGTH];
    char cond[MAX_PATH_LENGTH];

    harp_variable_definition *variable_definition;

    harp_dimension_type dimension_type_1d[1] = { harp_dimension_time };
    harp_dimension_type dimension_type_2d_spec[2] = { harp_dimension_time, harp_dimension_spectral };
    long bounds_dimension[2] = { -1, 4 };


    /* wavelength */
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "wavelength", harp_type_float, 2,
                                                   dimension_type_2d_spec, NULL, "wavelength", "nm", NULL,
                                                   read_instrument_wavelength);
    description = "evaluation of the 3rd-order Chebyshev polynomial coefficients using the spectral index";
    for (int i = 0; i < num_bands; i++)
    {
        /* calibrated (default / lambda unset) */
        snprintf(path, MAX_PATH_LENGTH, "/data/%s/instrument_data/calibrated_wavelength_coefficients[]", bands_list[i]);

        snprintf(cond, MAX_PATH_LENGTH, "%s,lambda=calibrated or lambda unset", bands_list_map[i]);

        harp_variable_definition_add_mapping(variable_definition, cond, NULL, path, description);

        /* nominal */
        snprintf(path, MAX_PATH_LENGTH, "/data/%s/instrument_data/nominal_wavelength_coefficients[]", bands_list[i]);

        snprintf(cond, MAX_PATH_LENGTH, "%s,lambda=nominal", bands_list_map[i]);

        harp_variable_definition_add_mapping(variable_definition, cond, NULL, path, description);
    }

    /* wavelength_uncertainty */
    description = "1-sigma uncertainty of the wavelength";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "wavelength_uncertainty", harp_type_float, 2,
                                                   dimension_type_2d_spec, NULL, description, "nm", NULL,
                                                   read_instrument_wavelength_error);

    /* dataset mappings: one per band x lambda option */
    description = "evaluation of the 3rd-order Chebyshev polynomial coefficients using the spectral index";
    for (int i = 0; i < num_bands; i++)
    {
        /* calibrated (default / lambda unset) */
        snprintf(path, MAX_PATH_LENGTH, "/data/%s/instrument_data/calibrated_wavelength_coefficients_error[]",
                 bands_list[i]);

        snprintf(cond, MAX_PATH_LENGTH, "%s,lambda=calibrated or lambda unset", bands_list_map[i]);

        harp_variable_definition_add_mapping(variable_definition, cond, NULL, path, description);

        /* nominal */
        snprintf(path, MAX_PATH_LENGTH, "/data/%s/instrument_data/nominal_wavelength_coefficients_error[]",
                 bands_list[i]);

        snprintf(cond, MAX_PATH_LENGTH, "%s,lambda=nominal", bands_list_map[i]);

        harp_variable_definition_add_mapping(variable_definition, cond, NULL, path, description);
    }

    /* wavelength_validity */
    description = "Spectral calibration quality assessment information for each pixel.";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "wavelength_validity", harp_type_int16, 1,
                                                   dimension_type_1d, bounds_dimension, description, NULL, NULL,
                                                   read_instrument_spectral_calibration_quality);
    var_name = "spectral_calibration_quality[]";
    description = NULL;
    register_mapping_per_band(variable_definition, var_name, "instrument_data", bands_list, bands_list_map, num_bands,
                              description);
}

static void register_uvr_product(void)
{
    const char *description;

    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;

    const char *band_option_values[3] = { "1a", "1b", "2" };
    const char *lambda_option_values[2] = { "calibrated", "nominal" };

    const char *bands_list[3] = { "band1a", "band1b", "band2" };
    const char *bands_list_map[3] = { "band=1a or band unset", "band=1b", "band=2" };
    int num_bands = ARRAY_SIZE(bands_list);


    description = "Sentinel-5 L1b UVR radiance spectra";
    module = harp_ingestion_register_module("S5_L1B_UVR", "Sentinel-5", "EPS_SG", "SN5_1B_UVR",
                                            description, ingestion_init, ingestion_done);

    description = "Choose which UVR band values to ingest: `band1a` (default), `band1b`, or `band2`";
    harp_ingestion_register_option(module, "band", description, 3, band_option_values);


    description = "Choose which wavelength data to ingest: `calibrated` (default), or `nominal`";
    harp_ingestion_register_option(module, "lambda", description, 2, lambda_option_values);

    product_definition = harp_ingestion_register_product(module, "S5_L1B_UVR", NULL, read_dimensions);

    /* orbit_index */
    description = "absolute orbit number";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "orbit_index",
                                                                     harp_type_int32, 0, NULL, NULL,
                                                                     description, NULL, NULL, read_orbit_index);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/@orbit_start", NULL);


    register_geolocation_variables(product_definition, bands_list, bands_list_map, num_bands);
    register_observation_variables(product_definition, bands_list, bands_list_map, num_bands);
    register_instrument_variables(product_definition, bands_list, bands_list_map, num_bands);
}

static void register_nir_product(void)
{
    const char *description;

    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;

    const char *band_option_values[3] = { "3a", "3b", "3c" };

    const char *bands_list[3] = { "band3a", "band3b", "band3c" };
    const char *bands_list_map[3] = { "band=3a or band unset", "band=3b", "band=3c" };
    int num_bands = ARRAY_SIZE(bands_list);


    description = "Sentinel-5 L1b NIR radiance spectra";
    module = harp_ingestion_register_module("S5_L1B_NIR", "Sentinel-5", "EPS_SG", "SN5_1B_NIR",
                                            description, ingestion_init, ingestion_done);

    description = "Choose which NIR band values to ingest: `band3a` (default), `band3b`, or `band3c`";
    harp_ingestion_register_option(module, "band", description, 3, band_option_values);

    product_definition = harp_ingestion_register_product(module, "S5_L1B_NIR", NULL, read_dimensions);

    /* orbit_index */
    description = "absolute orbit number";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "orbit_index",
                                                                     harp_type_int32, 0, NULL, NULL,
                                                                     description, NULL, NULL, read_orbit_index);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/@orbit_start", NULL);


    register_geolocation_variables(product_definition, bands_list, bands_list_map, num_bands);
    register_observation_variables(product_definition, bands_list, bands_list_map, num_bands);
    register_instrument_variables(product_definition, bands_list, bands_list_map, num_bands);
}

static void register_swr_product(void)
{
    const char *description;

    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;


    const char *band_option_values[2] = { "4", "5" };

    const char *bands_list[2] = { "band=4 or band unset", "band=5" };
    const char *bands_list_map[2] = { "band4", "band=5" };
    int num_bands = ARRAY_SIZE(bands_list);


    description = "Sentinel-5 L1b SWR radiance spectra";
    module = harp_ingestion_register_module("S5_L1B_SWR", "Sentinel-5", "EPS_SG", "SN5_1B_SWR",
                                            description, ingestion_init, ingestion_done);

    description = "Choose which SWR band values to ingest: `band4` (default), or `band5`";
    harp_ingestion_register_option(module, "band", description, 2, band_option_values);

    product_definition = harp_ingestion_register_product(module, "S5_L1B_SWR", NULL, read_dimensions);

    /* orbit_index */
    description = "absolute orbit number";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "orbit_index",
                                                                     harp_type_int32, 0, NULL, NULL,
                                                                     description, NULL, NULL, read_orbit_index);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/@orbit_start", NULL);


    register_geolocation_variables(product_definition, bands_list, bands_list_map, num_bands);
    register_observation_variables(product_definition, bands_list, bands_list_map, num_bands);
    register_instrument_variables(product_definition, bands_list, bands_list_map, num_bands);

}

static void register_irr_product(void)
{
    const char *var_name;
    const char *description;

    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;

    harp_dimension_type dimension_type_1d[1] = { harp_dimension_time };
    harp_dimension_type dimension_type_2d_spec[2] = { harp_dimension_time, harp_dimension_spectral };

    const char *band_option_values[8] = { "1a", "1b", "2", "3a", "3b", "3c", "4", "5" };

    const char *bands_list[8] = { "band1a", "band1b", "band2", "band3a", "band3b", "band3c", "band4", "band5" };
    const char *bands_list_map[8] =
        { "band=1a or band unset", "band=1b", "band=2", "band=3a", "band=3b", "band=3c", "band=4", "band=5" };
    int num_bands = ARRAY_SIZE(bands_list);

    description = "Sentinel-5 L1b irradiance spectra";
    module = harp_ingestion_register_module("S5_L1B_IRR", "Sentinel-5", "EPS_SG", "SN5_1B_IRR",
                                            description, ingestion_init, ingestion_done);

    description = "Choose which IRR band values to ingest: `band1a` (default), `band1b`, `band2`, `band3a`, "
        "`band3b`, `band3c`, `band4`, or `band5`";
    harp_ingestion_register_option(module, "band", description, 8, band_option_values);

    product_definition = harp_ingestion_register_product(module, "S5_L1B_IRR", NULL, read_dimensions);

    /* orbit_index */
    description = "absolute orbit number";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "orbit_index",
                                                                     harp_type_int32, 0, NULL, NULL,
                                                                     description, NULL, NULL, read_orbit_index);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/@orbit_start", NULL);


    /* Geolocation Data */

    /* sensor_altitude */
    description = "The altitude of the spacecraft relative to the WGS84 reference ellipsoid.";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "sensor_altitude", harp_type_float, 1,
                                                   dimension_type_1d, NULL, description,
                                                   "m", NULL, read_geolocation_satellite_altitude);

    var_name = "satellite_altitude[]";
    description = "the satellite altitude associated with a scanline is repeated for each pixel in the scanline";
    register_mapping_per_band(variable_definition, var_name, "geolocation_data", bands_list, bands_list_map, num_bands,
                              description);

    /* sensor_latitude */
    description = "Latitude of the spacecraft sub-satellite point on the WGS84 reference ellipsoid.";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "sensor_latitude", harp_type_float, 1,
                                                   dimension_type_1d, NULL, description, "degree_north", NULL,
                                                   read_geolocation_satellite_latitude);
    harp_variable_definition_set_valid_range_float(variable_definition, -90.0f, 90.0f);
    var_name = "satellite_latitude[]";
    description = "the satellite latitude associated with a scanline is repeated for each pixel in the scanline";
    register_mapping_per_band(variable_definition, var_name, "geolocation_data", bands_list, bands_list_map, num_bands,
                              description);

    /* sensor_longitude */
    description = "Longitude of the spacecraft sub-satellite point on the WGS84 reference ellipsoid.";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "sensor_longitude", harp_type_float, 1,
                                                   dimension_type_1d, NULL, description, "degree_east", NULL,
                                                   read_geolocation_satellite_longitude);
    harp_variable_definition_set_valid_range_float(variable_definition, -180.0f, 180.0f);
    var_name = "satellite_longitude[]";
    description = "the satellite longitude associated with a scanline is repeated for each pixel in the scanline";
    register_mapping_per_band(variable_definition, var_name, "geolocation_data", bands_list, bands_list_map, num_bands,
                              description);


    /* Observation Data */

    /* validity */
    description = "Overall quality information for a measurement.";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "validity", harp_type_int16, 1,
                                                   dimension_type_1d, NULL, description, NULL, NULL,
                                                   read_observation_measurement_quality);
    var_name = "measurement_quality[]";

    description = "the measurement quality associated with a scanline is repeated for each pixel in the scanline";
    register_mapping_per_band(variable_definition, var_name, "observation_data", bands_list, bands_list_map, num_bands,
                              description);

    /* datetime */
    description = "time of the measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double, 1,
                                                   dimension_type_1d, NULL, description, "seconds since 2020-01-01",
                                                   NULL, read_datetime);

    var_name = "datetime[]";
    description = "time converted from days since 20200-01-01 to seconds since 2020-01-01 (using 86400 seconds per "
        "day) and delta_time added; the time associated with a scanline is repeated for each pixel in the scanline";
    register_mapping_per_band(variable_definition, var_name, "observation_data", bands_list, bands_list_map, num_bands,
                              description);

    /* datetime_length */
    description = "measurement duration";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime_length", harp_type_double, 0, NULL,
                                                   NULL, description, "s", NULL, read_datetime_length);
    register_mapping_per_band(variable_definition, var_name, "delta_time", bands_list, bands_list_map, num_bands,
                              "delta_time[1] - delta_time[0]");

    /* photon_irradiance */
    description = "Measured spectral photon irradiance for each spectral channel and cross track position.";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "photon_irradiance", harp_type_float, 2,
                                                   dimension_type_2d_spec, NULL, description,
                                                   "mol/(s.m^2.nm)", NULL, read_observation_irradiance);
    var_name = "irradiance[]";
    description = NULL;
    register_mapping_per_band(variable_definition, var_name, "observation_data", bands_list, bands_list_map, num_bands,
                              description);


    /* photon_irradiance_uncertainty_systematic */
    description = "spectral irradiance systematic uncertainty";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "photon_irradiance_uncertainty_systematic",
                                                   harp_type_float, 2, dimension_type_2d_spec, NULL, description,
                                                   "mol/(s.m^2.nm)", NULL, read_observation_irradiance_error);
    var_name = "irradiance_error[]";
    description = "uncertainty = abs(irradiance / exp(irradiance_error / 20))";
    register_mapping_per_band(variable_definition, var_name, "observation_data", bands_list, bands_list_map, num_bands,
                              description);

    /* photon_irradiance_uncertainty_random */
    description = "spectral irradiance random uncertainty";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "photon_irradiance_uncertainty_random",
                                                   harp_type_float, 2, dimension_type_2d_spec, NULL, description,
                                                   "mol/(s.m^2.nm.sr)", NULL, read_observation_irradiance_noise);
    var_name = "irradiance_noise[]";
    description = "uncertainty = abs(irradiance / exp(irradiance_noise / 20))";
    register_mapping_per_band(variable_definition, var_name, "observation_data", bands_list, bands_list_map, num_bands,
                              description);

    /* photon_irradiance_validity */
    description = "Quality assessment information for each (spectral) channel.";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "photon_irradiance_validity", harp_type_int8, 2,
                                                   dimension_type_2d_spec, NULL, description, NULL, NULL,
                                                   read_observation_spectral_channel_quality);
    var_name = "spectral_channel_quality[]";
    description = NULL;
    register_mapping_per_band(variable_definition, var_name, "observation_data", bands_list, bands_list_map, num_bands,
                              description);

    /* Instrument Variables */
    register_instrument_variables(product_definition, bands_list, bands_list_map, num_bands);
}

int harp_ingestion_module_s5_l1b_init(void)
{
    register_uvr_product();
    register_nir_product();
    register_swr_product();
    register_irr_product();

    return 0;
}
