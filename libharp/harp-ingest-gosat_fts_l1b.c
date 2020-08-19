/*
 * Copyright (C) 2015-2020 S[&]T, The Netherlands.
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

/* Macro to determine the number of elements in a one dimensional C array. */
#define ARRAY_SIZE(X) (sizeof((X))/sizeof((X)[0]))

const char *band_option_values[] = { "1p", "1s", "2p", "2s", "3p", "3s", "4" };
const long band_max_num_wavenumbers[] = { 6565, 6565, 8080, 8080, 6565, 6565, 7575 };

typedef struct ingest_info_struct
{
    coda_product *product;
    coda_cursor wavenumber_cursor;
    coda_cursor radiance_cursor;
    coda_cursor time_cursor;
    coda_cursor geometric_info_cursor;

    int band_id;        /* 0: band1p, 1: band1s, 2: band2p, 3: band2s, 4: band3p, 5: band3s, 6: band4 */

    long num_main;
    int wavenumber_filter_min;  /* id of first wavenumber that will be included */
    int wavenumber_filter_max;  /* id of last wavenumber that will be included */

    float *radiance;    /* radiance buffer */
} ingest_info;

static int init_cursors(ingest_info *info)
{
    const char *wavenumber_path;
    const char *radiance_path;

    if (coda_cursor_set_product(&info->wavenumber_cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    info->radiance_cursor = info->wavenumber_cursor;
    info->time_cursor = info->wavenumber_cursor;
    info->geometric_info_cursor = info->wavenumber_cursor;

    if (info->band_id < 6)
    {
        wavenumber_path = "exposureAttribute/pointAttribute/RadiometricCorrectionInfo/spectrumObsWavelengthRange_SWIR";
        if (coda_cursor_goto(&info->wavenumber_cursor, wavenumber_path) != 0)
        {
            if (coda_errno == CODA_ERROR_INVALID_NAME)
            {
                /* This is a night-time product (that only has a TIR */
                /* band) so return an empty product (see issue 79).  */
                return 1;
            }
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
    }
    else
    {
        wavenumber_path = "exposureAttribute/pointAttribute/RadiometricCorrectionInfo/spectrumObsWavelengthRange_TIR";
        if (coda_cursor_goto(&info->wavenumber_cursor, wavenumber_path) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
    }

    switch (info->band_id)
    {
        case 0:
        case 1:
            radiance_path = "Spectrum/SWIR/band1/obsWavelength";
            break;
        case 2:
        case 3:
            radiance_path = "Spectrum/SWIR/band2/obsWavelength";
            break;
        case 4:
        case 5:
            radiance_path = "Spectrum/SWIR/band3/obsWavelength";
            break;
        default:
            radiance_path = "Spectrum/TIR/band4/obsWavelength";
            break;
    }
    if (coda_cursor_goto(&info->radiance_cursor, radiance_path) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    if (coda_cursor_goto(&info->time_cursor, "exposureAttribute/pointAttribute/Time") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    if (coda_cursor_goto(&info->geometric_info_cursor, "exposureAttribute/pointAttribute/geometricInfo") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    return 0;
}

static int init_num_main(ingest_info *info)
{
    if (coda_cursor_get_num_elements(&info->time_cursor, &info->num_main) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    return 0;
}

static int read_dimensions(void *user_data, long dimension[HARP_NUM_DIM_TYPES])
{
    ingest_info *info = (ingest_info *)user_data;

    dimension[harp_dimension_time] = info->num_main;
    dimension[harp_dimension_spectral] = band_max_num_wavenumbers[info->band_id];

    return 0;
}

static int read_time(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    coda_cursor cursor = info->time_cursor;
    int32_t year, month, day, hour, minute, second, microsecond;
    double seconds;

    if (coda_cursor_goto_array_element_by_index(&cursor, index) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&cursor, "year") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_int32(&cursor, &year) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    coda_cursor_goto_parent(&cursor);
    if (coda_cursor_goto_record_field_by_name(&cursor, "month") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_int32(&cursor, &month) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    coda_cursor_goto_parent(&cursor);
    if (coda_cursor_goto_record_field_by_name(&cursor, "day") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_int32(&cursor, &day) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    coda_cursor_goto_parent(&cursor);
    if (coda_cursor_goto_record_field_by_name(&cursor, "hour") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_int32(&cursor, &hour) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    coda_cursor_goto_parent(&cursor);
    if (coda_cursor_goto_record_field_by_name(&cursor, "min") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_int32(&cursor, &minute) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    coda_cursor_goto_parent(&cursor);
    if (coda_cursor_goto_record_field_by_name(&cursor, "sec") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_double(&cursor, &seconds) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    coda_cursor_goto_parent(&cursor);
    second = (int32_t)seconds;
    microsecond = (int32_t)((seconds - second) * 1.0E6);
    if (coda_datetime_to_double(year, month, day, hour, minute, second, microsecond, data.double_data) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    return 0;
}

static int read_latitude(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    coda_cursor cursor = info->geometric_info_cursor;

    if (coda_cursor_goto_record_field_by_name(&cursor, "centerLat") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_array_element_by_index(&cursor, index) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_double(&cursor, data.double_data) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    return 0;
}

static int read_longitude(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    coda_cursor cursor = info->geometric_info_cursor;

    if (coda_cursor_goto_record_field_by_name(&cursor, "centerLon") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_array_element_by_index(&cursor, index) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_double(&cursor, data.double_data) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    return 0;
}

static double complex_norm(const float *complex_value)
{
    double re = (double)complex_value[0];
    double im = (double)complex_value[1];

    return sqrt(re * re + im * im);
}

static int read_radiance(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long offset = 0;
    long i;

    if (info->radiance == NULL)
    {
        coda_cursor cursor = info->radiance_cursor;
        long size;

        size = info->num_main * band_max_num_wavenumbers[info->band_id] * 2 * sizeof(float);
        if (info->band_id < 6)
        {
            size *= 2;
        }
        info->radiance = malloc(size);
        if (info->radiance == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)", size,
                           __FILE__, __LINE__);
            return -1;
        }
        if (coda_cursor_read_float_array(&cursor, info->radiance, coda_array_ordering_c) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
    }

    switch (info->band_id)
    {
        case 0:
        case 2:
        case 4:
            offset = index * 2 * band_max_num_wavenumbers[info->band_id] * 2;
            break;
        case 1:
        case 3:
        case 5:
            offset =
                index * 2 * band_max_num_wavenumbers[info->band_id] * 2 + band_max_num_wavenumbers[info->band_id] * 2;
            break;
        case 6:
            offset = index * band_max_num_wavenumbers[info->band_id] * 2;
            break;
    }

    for (i = 0; i < band_max_num_wavenumbers[info->band_id]; i++)
    {
        data.double_data[i] = complex_norm(&info->radiance[offset + i * 2]);
    }

    return 0;
}

static int read_wavenumber_param(ingest_info *info, long index, double *a, double *b)
{
    coda_cursor cursor = info->wavenumber_cursor;

    if (info->band_id < 6)
    {
        index = index * 6 + info->band_id;
    }
    index *= 2;

    if (coda_cursor_goto_array_element_by_index(&cursor, index) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_double(&cursor, a) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_next_array_element(&cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_double(&cursor, b) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    return 0;
}

static int read_wavenumber(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    double a;
    double b;
    long i;

    if (read_wavenumber_param(info, index, &a, &b) != 0)
    {
        return -1;
    }

    for (i = 0; i < band_max_num_wavenumbers[info->band_id]; i++)
    {
        data.double_data[i] = a * i + b;
    }

    return 0;
}

static int parse_option_band(ingest_info *info, const harp_ingestion_options *options)
{
    const char *value;

    if (harp_ingestion_options_get_option(options, "band", &value) == 0)
    {
        int i;

        for (i = 0; i < (int)(ARRAY_SIZE(band_option_values) - 1); i++)
        {
            if (strcmp(value, band_option_values[i]) == 0)
            {
                info->band_id = i;
                return 0;
            }
        }

        /* Option values are guaranteed to be legal if present. */
        assert(strcmp(value, band_option_values[ARRAY_SIZE(band_option_values) - 1]) == 0);
        info->band_id = ARRAY_SIZE(band_option_values) - 1;
    }

    return 0;
}

static void ingestion_done(void *user_data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->radiance != NULL)
    {
        free(info->radiance);
    }

    free(info);
}

static int ingestion_init(const harp_ingestion_module *module, coda_product *product,
                          const harp_ingestion_options *options, harp_product_definition **definition, void **user_data)
{
    ingest_info *info;
    int retval;

    info = malloc(sizeof(ingest_info));
    if (info == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(ingest_info), __FILE__, __LINE__);
        return -1;
    }
    info->product = product;
    info->band_id = 0;
    info->radiance = NULL;

    if (parse_option_band(info, options) != 0)
    {
        ingestion_done(info);
        return -1;
    }
    retval = init_cursors(info);
    if (retval < 0)
    {
        ingestion_done(info);
        return -1;
    }
    else if (retval == 0)
    {
        if (init_num_main(info) != 0)
        {
            ingestion_done(info);
            return -1;
        }
    }
    else
    {
        /* This is a night-time product and we have selected a band     */
        /* that does not exist, return an empty product (see issue 79). */
        info->num_main = 0;
    }

    *definition = module->product_definition[info->band_id];
    *user_data = info;

    return 0;
}

static harp_product_definition *register_radiance_product(harp_ingestion_module *module, int band_id)
{
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };
    harp_dimension_type profile_dimension_type[2] = { harp_dimension_time, harp_dimension_spectral };
    const char *product_name;
    const char *product_description;
    const char *description;
    const char *mapping_description;
    const char *path;
    const char *unit;

    /* radiance product */
    switch (band_id)
    {
        case 0:
            product_name = "GOSAT_FTS_L1b_band1p";
            product_description = "band1-p spectra";
            mapping_description = "band=1p or band unset";
            break;
        case 1:
            product_name = "GOSAT_FTS_L1b_band1s";
            product_description = "band1-s spectra";
            mapping_description = "band=1s";
            break;
        case 2:
            product_name = "GOSAT_FTS_L1b_band2p";
            product_description = "band2-p spectra";
            mapping_description = "band=2p";
            break;
        case 3:
            product_name = "GOSAT_FTS_L1b_band2s";
            product_description = "band2-s spectra";
            mapping_description = "band=2s";
            break;
        case 4:
            product_name = "GOSAT_FTS_L1b_band3p";
            product_description = "band3-p spectra";
            mapping_description = "band=3p";
            break;
        case 5:
            product_name = "GOSAT_FTS_L1b_band3s";
            product_description = "band3-s spectra";
            mapping_description = "band=3s";
            break;
        case 6:
            product_name = "GOSAT_FTS_L1b_band4";
            product_description = "band4 spectra";
            mapping_description = "band=4";
            break;
        default:
            assert(0);
            exit(1);
    }
    product_definition = harp_ingestion_register_product(module, product_name, product_description, read_dimensions);
    harp_product_definition_add_mapping(product_definition, NULL, mapping_description);

    /* datetime */
    description = "start time of the measurement";
    variable_definition =
        harp_ingestion_register_variable_block_read(product_definition, "datetime", harp_type_double, 1,
                                                    dimension_type, NULL, description, "seconds since 2000-01-01",
                                                    NULL, read_time);
    path = "/exposureAttribute/pointAttribute/Time[]";
    description = "the record with year/month/day/hour/min/sec values is converted to a double precision floating "
        "point value that represents the amount of seconds since 2000-01-01 00:00:00";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* longitude */
    description = "longitude of the measurement";
    variable_definition =
        harp_ingestion_register_variable_block_read(product_definition, "longitude", harp_type_double, 1,
                                                    dimension_type, NULL, description, "degree_east", NULL,
                                                    read_longitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0, 180.0);
    path = "/exposureAttribute/pointAttribute/geometricInfo/centerLon[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* latitude */
    description = "latitude of the measurement";
    variable_definition =
        harp_ingestion_register_variable_block_read(product_definition, "latitude", harp_type_double, 1,
                                                    dimension_type, NULL, description, "degree_north", NULL,
                                                    read_latitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    path = "/exposureAttribute/pointAttribute/geometricInfo/centerLat[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* radiance */
    description = "radiances derived by taking the norm of the fourier transform of measured wavelengths";
    unit = (band_id < 6 ? "V/cm^-1" : "W/(cm^2.sr.cm^-1)");
    variable_definition =
        harp_ingestion_register_variable_block_read(product_definition, "radiance", harp_type_double, 2,
                                                    profile_dimension_type, NULL, description, unit, NULL,
                                                    read_radiance);
    description =
        "the radiance returned is the complex norm of the complex value that is stored in the product; in "
        "other words, what is returned is sqrt(real * real + imag * imag)";
    switch (band_id)
    {
        case 0:
            path = "/Spectrum/SWIR/band1/obsWavelength[,0,,]";
            break;
        case 1:
            path = "/Spectrum/SWIR/band1/obsWavelength[,1,,]";
            break;
        case 2:
            path = "/Spectrum/SWIR/band2/obsWavelength[,0,,]";
            break;
        case 3:
            path = "/Spectrum/SWIR/band2/obsWavelength[,1,,]";
            break;
        case 4:
            path = "/Spectrum/SWIR/band3/obsWavelength[,0,,]";
            break;
        case 5:
            path = "/Spectrum/SWIR/band3/obsWavelength[,1,,]";
            break;
        case 6:
            path = "/Spectrum/TIR/band4/obsWavelength[]";
            break;
        default:
            assert(0);
            exit(1);
    }
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* wavenumber */
    description = "wavenumber for each point in the spectrum";
    variable_definition =
        harp_ingestion_register_variable_block_read(product_definition, "wavenumber", harp_type_double, 2,
                                                    profile_dimension_type, NULL, description, "cm^-1", NULL,
                                                    read_wavenumber);
    description =
        "the wavenumbers are calculated by evaluating the function a.x + b for x = 0 .. N-1 with a,b the "
        "wavelength range parameters in the product";
    if (band_id < 6)
    {
        path = "/exposureAttribute/pointAttribute/RadiometricCorrectionInfo/spectrumObsWavelengthRange_SWIR";
    }
    else
    {
        path = "/exposureAttribute/pointAttribute/RadiometricCorrectionInfo/spectrumObsWavelengthRange_TIR";
    }
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    return product_definition;
}

int harp_ingestion_module_gosat_fts_l1b_init(void)
{
    harp_ingestion_module *module;
    int i;

    module = harp_ingestion_register_module("GOSAT_FTS_L1b", "GOSAT FTS", "GOSAT", "L1B_FTS",
                                            "GOSAT FTS Level 1b radiance spectra", ingestion_init, ingestion_done);
    harp_ingestion_register_option(module, "band", "spectral band to ingest (default is 1p)",
                                   ARRAY_SIZE(band_option_values), band_option_values);

    for (i = 0; i < (int)ARRAY_SIZE(band_option_values); i++)
    {
        register_radiance_product(module, i);
    }

    return 0;
}
