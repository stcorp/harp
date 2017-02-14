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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef enum main_data_variable_enum
{
    DATETIME,
    LONGITUDE,
    LATITUDE
} main_data_variable;

#define CHECKED_MALLOC(v, s) v = malloc(s); if (v == NULL) { harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)", s, __FILE__, __LINE__); return -1;}

#define SCANS_PER_SCANLINE     30
#define SPECTRA_PER_SCAN        4
#define SPECTRA_PER_SCANLINE   (SPECTRA_PER_SCAN * SCANS_PER_SCANLINE)

typedef struct ingest_info_struct
{
    coda_product *product;
    int format_version;
    long mdr_records;   // Total number of MDR records (valid scanlines and dummy records)
    long valid_scanlines;       // Each file is a series of scanlines, each scanline is a series of scans, each scan is a 2x2 matrix of spectral measurements.
    coda_cursor *mdr_cursors;
    long num_main;      // Number spectral measurements in the file (number scanlines * 30 * 4)
    long num_pixels;    // Number of pixels in 1 scan (will usually be 8700)
    int16_t nr_scale_factors;
    int16_t *scale_factors;
    int16_t *channel_first;
    int16_t *channel_last;
} ingest_info;

static int get_main_data(ingest_info *info, const char *fieldname, main_data_variable var_type,
                         double *double_data_array)
{
    coda_cursor cursor;
    double *double_data;
    double locations[SPECTRA_PER_SCANLINE * 2];
    long mdr_nr, i, offset;

    double_data = double_data_array;
    for (mdr_nr = 0; mdr_nr < info->valid_scanlines; mdr_nr++)
    {
        cursor = info->mdr_cursors[mdr_nr];
        if (coda_cursor_goto(&cursor, fieldname) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        switch (var_type)
        {
            case DATETIME:
                if (coda_cursor_read_double(&cursor, double_data) != 0)
                {
                    harp_set_error(HARP_ERROR_CODA, NULL);
                    return -1;
                }
                double_data++;
                break;
            case LONGITUDE:
            case LATITUDE:
                if (coda_cursor_read_double_array(&cursor, locations, coda_array_ordering_c) != 0)
                {
                    harp_set_error(HARP_ERROR_CODA, NULL);
                    return -1;
                }
                if (var_type == LONGITUDE)
                {
                    offset = 0;
                }
                else
                {
                    offset = 1;
                }
                for (i = 0; i < SPECTRA_PER_SCANLINE; i++)
                {
                    *double_data = locations[2 * i + offset];
                    double_data++;
                }
                break;
        }
    }
    if (coda_cursor_goto_root(&cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    return 0;
}

static int get_spectra_sample_data(ingest_info *info, long row, float *float_data_array)
{
    static int32_t first_channel;
    static int16_t measured_spectrum_data[SPECTRA_PER_SCANLINE * 8700];
    static int16_t *spectrum_data;
    coda_cursor cursor;
    float *float_data, *start_of_float_data_this_spectrum;
    int16_t scale_nr, channel_nr;
    int16_t *start_of_this_spectrum;

    float_data = float_data_array;
    cursor = info->mdr_cursors[row / SPECTRA_PER_SCANLINE];
    if (row % SPECTRA_PER_SCANLINE == 0L)
    {
        if (coda_cursor_goto_record_field_by_name(&cursor, "IDefNsfirst1b") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_read_int32(&cursor, &first_channel) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        coda_cursor_goto_parent(&cursor);

        /* GS1cSpect contains int16 and has the following dimensions: */
        /* dim[0] = SCANS_PER_SCANLINE (fixed at 30)                  */
        /* dim[1] = SPECTRA_PER_SCAN (fixed at 4)                     */
        /* dim[2] = pixels in one spectrum (fixed at 8700)            */
        if (coda_cursor_goto_record_field_by_name(&cursor, "GS1cSpect") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_read_int16_array(&cursor, measured_spectrum_data, coda_array_ordering_c) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        spectrum_data = measured_spectrum_data;
    }

    start_of_this_spectrum = spectrum_data;
    start_of_float_data_this_spectrum = float_data;
    for (scale_nr = 0; scale_nr < info->nr_scale_factors; scale_nr++)
    {
        spectrum_data = start_of_this_spectrum + info->channel_first[scale_nr] - first_channel;
        for (channel_nr = info->channel_first[scale_nr]; channel_nr <= info->channel_last[scale_nr]; channel_nr++)
        {
            /* Because this data has limited precision (it was stored in */
            /* an int16), we store the radiance in a float.              */
            *float_data = (float)(*spectrum_data) * pow(10.0, -(info->scale_factors[scale_nr]));
            float_data++;
            spectrum_data++;
        }
    }
    spectrum_data = start_of_this_spectrum + info->num_pixels;
    float_data = start_of_float_data_this_spectrum + info->num_pixels;

    return 0;
}

static int get_wavenumber_sample_data(ingest_info *info, long row, float *float_data_array)
{
    static double sample_width;
    static int32_t first_sample, last_sample, sample;
    coda_cursor cursor;
    float *float_data;
    long i;

    float_data = float_data_array;
    cursor = info->mdr_cursors[row / SPECTRA_PER_SCANLINE];
    if (row % SPECTRA_PER_SCANLINE == 0L)
    {
        if (coda_cursor_goto_record_field_by_name(&cursor, "IDefSpectDWn1b") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_read_double(&cursor, &sample_width) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        coda_cursor_goto_parent(&cursor);
        if (coda_cursor_goto_record_field_by_name(&cursor, "IDefNsfirst1b") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_read_int32(&cursor, &first_sample) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        coda_cursor_goto_parent(&cursor);
        if (coda_cursor_goto_record_field_by_name(&cursor, "IDefNslast1b") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_read_int32(&cursor, &last_sample) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (last_sample < first_sample)
        {
            harp_set_error(HARP_SUCCESS, "product error detected (IDefNslast1b < IDefNsfirst1b)");
            return -1;
        }
        if ((last_sample - first_sample + 1) > info->num_pixels)
        {
            harp_set_error(HARP_SUCCESS, "product error detected (IDefNslast1b - IDefNsfirst1b + 1 > 8700)");
            return -1;
        }
    }
    for (sample = first_sample, i = 0; sample <= last_sample; sample++, i++)
    {
        *float_data = (float)(sample_width * sample);
        float_data++;
    }
    for (; i < info->num_pixels; i++)
    {
        float_data++;
    }
    return 0;
}

static void ingestion_done(void *user_data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->scale_factors != NULL)
    {
        free(info->scale_factors);
    }
    if (info->channel_first != NULL)
    {
        free(info->channel_first);
    }
    if (info->channel_last != NULL)
    {
        free(info->channel_last);
    }

    free(info);
}

static int read_dimensions(void *user_data, long dimension[HARP_NUM_DIM_TYPES])
{
    dimension[harp_dimension_time] = ((ingest_info *)user_data)->num_main;
    dimension[harp_dimension_spectral] = ((ingest_info *)user_data)->num_pixels;
    return 0;
}

static int read_datetime(void *user_data, harp_array data)
{
    double *mdr_times, *double_data;
    double scantime;
    ingest_info *info = (ingest_info *)user_data;
    long i, j, k;
    int retval;

    CHECKED_MALLOC(mdr_times, sizeof(double) * info->valid_scanlines);
    retval = get_main_data(info, "RECORD_HEADER/RECORD_START_TIME", DATETIME, mdr_times);
    double_data = data.double_data;
    for (i = 0; i < info->valid_scanlines; i++)
    {
        for (j = 0; j < SCANS_PER_SCANLINE; j++)
        {
            /* A full scanline takes 8 seconds and consist of 37 scans */
            /* (30 scans with data and 7 for calibration etc.          */
            scantime = mdr_times[i] + (j * 8.0 / 37);
            for (k = 0; k < SPECTRA_PER_SCAN; k++)
            {
                *double_data = scantime;
                double_data++;
            }
        }
    }
    free(mdr_times);
    return retval;
}

static int read_latitude(void *user_data, harp_array data)
{
    return get_main_data((ingest_info *)user_data, "GGeoSondLoc", LATITUDE, data.double_data);
}

static int read_longitude(void *user_data, harp_array data)
{
    return get_main_data((ingest_info *)user_data, "GGeoSondLoc", LONGITUDE, data.double_data);
}

static int read_spectral_radiance_sample(void *user_data, long index, harp_array data)
{
    return get_spectra_sample_data((ingest_info *)user_data, index, data.float_data);
}

static int read_wavenumber_sample(void *user_data, long index, harp_array data)
{
    return get_wavenumber_sample_data((ingest_info *)user_data, index, data.float_data);
}

static int read_scan_subset_counter(void *user_data, harp_array data)
{
    int8_t *int8_data;
    long i;
    int8_t j;

    int8_data = data.int8_data;
    for (i = 0; i < ((ingest_info *)user_data)->valid_scanlines; i++)
    {
        for (j = 0; j < SPECTRA_PER_SCANLINE; j++)
        {
            *int8_data = j;
            int8_data++;
        }
    }
    return 0;
}

static int init_dimensions(ingest_info *info)
{
    coda_cursor cursor, saved_cursor;
    int num_dims;
    long dim[HARP_MAX_NUM_DIMS], i;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    /* Count the number of valid scanlines per product */
    if (coda_cursor_goto_record_field_by_name(&cursor, "MDR") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &info->mdr_records) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_first_array_element(&cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    info->valid_scanlines = 0;
    CHECKED_MALLOC(info->mdr_cursors, sizeof(coda_cursor) * info->mdr_records);
    for (i = 0; i < info->mdr_records; i++)
    {
        int is_mdr;

        saved_cursor = cursor;
        if (coda_cursor_goto_record_field_by_name(&cursor, "MDR") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if ((coda_cursor_get_record_field_available_status(&cursor, 0, &is_mdr) == 0) && is_mdr)
        {
            info->mdr_cursors[info->valid_scanlines] = cursor;
            info->valid_scanlines++;
        }
        coda_cursor_goto_parent(&cursor);
        if (i < (info->mdr_records - 1))
        {
            cursor = saved_cursor;
            if (coda_cursor_goto_next_array_element(&cursor) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
        }
    }
    if (info->valid_scanlines == 0)
    {
        harp_set_error(HARP_SUCCESS, "No MDR records with valid scanlines found");
        return -1;
    }
    info->num_main = info->valid_scanlines * SPECTRA_PER_SCANLINE;

    cursor = info->mdr_cursors[0];

    /* Count the number of pixels with spectra data per spectrum (this will usually be 8700) */
    if (coda_cursor_goto_record_field_by_name(&cursor, "GS1cSpect") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_array_dim(&cursor, &num_dims, dim) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    /* dim[0] = SCANS_PER_SCANLINE (fixed at 30) */
    /* dim[1] = SPECTRA_PER_SCAN (fixed at 4)    */
    /* dim[2] = pixels in one spectrum           */
    info->num_pixels = dim[2];

    coda_cursor_goto_root(&cursor);

    return 0;
}

static int read_GIADR_scalefactors(ingest_info *info)
{
    coda_cursor cursor;
    long max_scale_factors;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&cursor, "GIADR_ScaleFactors") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&cursor, "IDefScaleSondNbScale") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_int16(&cursor, &info->nr_scale_factors) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    coda_cursor_goto_parent(&cursor);
    if (coda_cursor_goto_record_field_by_name(&cursor, "IDefScaleSondScaleFactor") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &max_scale_factors) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    CHECKED_MALLOC(info->scale_factors, max_scale_factors * sizeof(int16_t));
    if (coda_cursor_read_int16_array(&cursor, info->scale_factors, coda_array_ordering_c) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    coda_cursor_goto_parent(&cursor);
    if (coda_cursor_goto_record_field_by_name(&cursor, "IDefScaleSondNsfirst") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    CHECKED_MALLOC(info->channel_first, max_scale_factors * sizeof(int16_t));
    if (coda_cursor_read_int16_array(&cursor, info->channel_first, coda_array_ordering_c) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    coda_cursor_goto_parent(&cursor);
    if (coda_cursor_goto_record_field_by_name(&cursor, "IDefScaleSondNslast") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    CHECKED_MALLOC(info->channel_last, max_scale_factors * sizeof(int16_t));
    if (coda_cursor_read_int16_array(&cursor, info->channel_last, coda_array_ordering_c) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    return 0;
}

static int ingestion_init(const harp_ingestion_module *module, coda_product *product,
                          const harp_ingestion_options *options, harp_product_definition **definition, void **user_data)
{
    int format_version;
    ingest_info *info;

    (void)options;

    if (coda_get_product_version(product, &format_version) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    CHECKED_MALLOC(info, sizeof(ingest_info));
    memset(info, '\0', sizeof(ingest_info));
    info->product = product;
    info->format_version = format_version;
    info->valid_scanlines = 0;

    if (init_dimensions(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }
    if (read_GIADR_scalefactors(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }

    *definition = *module->product_definition;
    *user_data = info;

    return 0;
}

int harp_ingestion_module_iasi_l1_init(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2];
    const char *description;
    const char *path;

    description = "IASI Level 1";
    module =
        harp_ingestion_register_module_coda("IASI_L1", "IASI", "EPS", "IASI_xxx_1C", description, ingestion_init,
                                            ingestion_done);

    description = "IASI Level 1 product";
    product_definition = harp_ingestion_register_product(module, "IASI_L1", description, read_dimensions);
    description = "IASI Level 1 products contain a number of scanlines, each scanline contains 30 scans, each scan "
        "contains 4 spectra and each spectrum contains 8700 measurements";
    harp_product_definition_add_mapping(product_definition, description, NULL);

    dimension_type[0] = harp_dimension_time;
    dimension_type[1] = harp_dimension_spectral;

    /* datetime */
    description = "time of the measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double, 1, dimension_type,
                                                   NULL, description, "seconds since 2000-01-01", NULL, read_datetime);
    path = "/MDR[]/MDR/RECORD_HEADER/RECORD_START_TIME";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, "");

    /* latitude */
    description = "center latitude of the measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_double, 1, dimension_type,
                                                   NULL, description, "degree_north", NULL, read_latitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    path = "/MDR[]/MDR/GGeoSondLoc[,,1]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* longitude */
    description = "center longitude of the measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_double, 1, dimension_type,
                                                   NULL, description, "degree_east", NULL, read_longitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0, 180.0);
    path = "/MDR[]/MDR/GGeoSondLoc[,,0]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* wavenumber_radiance */
    description = "measured radiances";
    variable_definition =
        harp_ingestion_register_variable_sample_read(product_definition, "wavenumber_radiance", harp_type_float, 2,
                                                     dimension_type, NULL, description, "W/m^2.sr.m^-1", NULL,
                                                     read_spectral_radiance_sample);
    path = "/MDR[]/MDR/GS1cSpect[], /MDR[]/MDR/IDefNsfirst1b, /GIADR_ScaleFactors/IDefScaleSondNbScale, "
        "/GIADR_ScaleFactors/IDefScaleSondScaleFactor[], /GIADR_ScaleFactors/IdefScaleSondNsfirst[], "
        "/GIADR_ScaleFactors/IDefScaleSondNslast[]";
    description = "spectral data is scaled using the information in GIADR_ScaleFactors:"
        "``for numScale = 0 to (IDefScaleSondNbScale - 1) do`` ``{`` ``SF = IDefScaleSondScaleFactor[numScale];``"
        " ``for chanNb = IdefScaleSondNsfirst[numScale] to IDefScaleSondNslast[numScale] do`` ``{``"
        " ``w = chanNb - IDefNsfirst1b + 1;`` ``pixel_readout[w] = GS1cSpect[..,..,w] * 10^(-SF)`` ``}`` ``}``";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* wavenumber */
    description = "nominal wavelength assignment for each of the detector pixels";
    variable_definition =
        harp_ingestion_register_variable_sample_read(product_definition, "wavenumber", harp_type_float, 2,
                                                     dimension_type, NULL, description, "m^-1", NULL,
                                                     read_wavenumber_sample);
    path = "/MDR[]/MDR/IDefSpectDWn1b, /MDR[]/MDR/IDefNsfirst1b, /MDR[]/MDR/IDefNslast1b";
    description = "wavenumber[i] = IDefSpectDWn1b * (i + IDefNsfirst1b - 1). ";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* scan_subset_counter */
    description = "relative index (0-119) of this measurement within an MDR";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "scan_subset_counter", harp_type_int8, 1,
                                                   dimension_type, NULL, description, NULL, NULL,
                                                   read_scan_subset_counter);

    return 0;
}
