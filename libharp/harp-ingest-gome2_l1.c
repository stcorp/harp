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

/* --------------------------- defines ------------------------------------ */

#ifndef FALSE
#define FALSE    0
#define TRUE     1
#endif

/* BAND_1A         0
   BAND_1B         1
   BAND_2A         2
   BAND_2B         3
   BAND_3          4
   BAND_4          5 */
#define MAX_NR_BANDS                  6
#define MAX_READOUTS_PER_MDR_RECORD  32
#define MAX_PIXELS                 4096

#define CHECKED_MALLOC(v, s) v = malloc(s); if (v == NULL) { harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)", s, __FILE__, __LINE__); return -1;}

/* -------------------------- typedefs ------------------------------------ */

typedef enum ingestion_data_type_enum
{
    DATA_RADIANCE,
    DATA_TRANSMISSION,
    DATA_SUN,
    DATA_MOON,
    DATA_SUN_REFERENCE
} ingestion_data_type;

typedef struct ingest_info_struct
{
    coda_product *product;
    int format_version;
    ingestion_data_type ingestion_data; /* RADIANCE, TRANSMISSION, SUN, MOON */
    char *lightsource;  /* Earthshine, Sun, Moon */

    /* Data about the bands */
    long num_pixels[MAX_NR_BANDS];
    long offset_of_band[MAX_NR_BANDS];
    long total_num_pixels_all_bands;
    int band_nr;        /* Which band (0-5) to ingest, -1 means all bands */

    /* Data about the MDR-records */
    long num_mdr_records;
    coda_cursor *mdr_lightsource_cursors;
    uint16_t *max_num_recs;
    int *band_nr_fastest_band;
    int *index_of_fastest_timer_in_list_of_timers;
    short *no_mixed_pixel;
    short *readout_offset;      /* First valid readout in MDR record, will always be 1 (in case the first readout is skipped) or 0 (default) */

    /* Data about the VIADR_SMR-records */
    long num_viadr_smr_records;
} ingest_info;

typedef enum spectral_variable_type_enum
{
    RADIANCE,
    WAVELENGTH,
    INTEGRATION_TIME
} spectral_variable_type;

/* ---- prototypes of functions that are called before they are defined --- */
static int get_smr_spectral_data(ingest_info *info, const char *fieldname, double *double_data_array);

/* ---------------------- global variables -------------------------------- */

const char *band_name_in_file[] = { "BAND_1A", "BAND_1B", "BAND_2A", "BAND_2B", "BAND_3", "BAND_4" };
const char *band_name_as_option[] = { "band-1a", "band-1b", "band-2a", "band-2b", "band-3", "band-4" };
const char *wavelength_name_in_file[] =
    { "WAVELENGTH_1A", "WAVELENGTH_1B", "WAVELENGTH_2A", "WAVELENGTH_2B", "WAVELENGTH_3", "WAVELENGTH_4" };

/* --------------------------- code --------------------------------------- */

static void ingestion_done(void *user_data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->lightsource != NULL)
    {
        free(info->lightsource);
    }
    if (info->mdr_lightsource_cursors != NULL)
    {
        free(info->mdr_lightsource_cursors);
    }
    if (info->max_num_recs != NULL)
    {
        free(info->max_num_recs);
    }
    if (info->band_nr_fastest_band != NULL)
    {
        free(info->band_nr_fastest_band);
    }
    if (info->index_of_fastest_timer_in_list_of_timers != NULL)
    {
        free(info->index_of_fastest_timer_in_list_of_timers);
    }
    if (info->no_mixed_pixel != NULL)
    {
        free(info->no_mixed_pixel);
    }
    if (info->readout_offset != NULL)
    {
        free(info->readout_offset);
    }
    free(info);
}

static int band_name_to_band_nr(const char *band_name)
{
    int i;

    for (i = 0; i < 10; i++)
    {
        if (strcmp(band_name, band_name_in_file[i]) == 0)
        {
            return i;
        }
        if (strcmp(band_name, band_name_as_option[i]) == 0)
        {
            return i;
        }
    }
    return -1;
}

/* Start of code for the ingestion of measurements */

static void copy_double_data_to_following_rows(long num_rows_with_data, long num_columns, double *start_of_data)
{
    uint16_t i;
    double *dest_data;

    dest_data = start_of_data + num_columns;
    for (i = (MAX_READOUTS_PER_MDR_RECORD / num_rows_with_data); i > 1; i--)
    {
        *dest_data = *start_of_data;
        dest_data += num_columns;
    }
}

static int get_main_datetime_data(ingest_info *info, double *double_data_array)
{
    coda_cursor cursor;
    double *double_data, time_from_record;
    long i, j;

    double_data = double_data_array;
    for (i = 0; i < info->num_mdr_records; i++)
    {
        cursor = info->mdr_lightsource_cursors[i];

        if (coda_cursor_goto(&cursor, "RECORD_HEADER") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_goto_record_field_by_name(&cursor, "RECORD_START_TIME") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_read_double(&cursor, &time_from_record) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        for (j = info->readout_offset[i]; j < MAX_READOUTS_PER_MDR_RECORD; j++)
        {
            time_from_record += 0.1875; // 6 seconds per scan / 32 readouts
            *double_data = time_from_record;
            double_data++;
        }
    }
    return 0;
}

/*
 * Read GEO_EARTH_ACTUAL data from a datafile with product version 12 or
 * higher. The data is retrieved from the path:
 * GEO_EARTH_ACTUAL_<index fastest timer>[readout_nr]/dataset_name[start_data_index..end_data_index]/fieldname
 */
static int get_main_geo_earth_actual_data_new_version(ingest_info *info, long mdr_record, const char *dataset_name,
                                                      const char *fieldname, long start_data_index, long end_data_index,
                                                      long data_dim_size, double *double_data_one_mdr_record)
{
    coda_cursor cursor;
    double *double_data;
    long j, k;
    char geo_earth_actual_name[20];

    cursor = info->mdr_lightsource_cursors[mdr_record];
    sprintf(geo_earth_actual_name, "GEO_EARTH_ACTUAL_%d", info->index_of_fastest_timer_in_list_of_timers[mdr_record]);
    if (coda_cursor_goto_record_field_by_name(&cursor, geo_earth_actual_name) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_first_array_element(&cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    double_data = double_data_one_mdr_record;
    for (j = 0; j < info->max_num_recs[mdr_record]; j++)
    {
        if (coda_cursor_goto_record_field_by_name(&cursor, dataset_name) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (data_dim_size > 1)  /* The dataset-field is an array */
        {
            if (coda_cursor_goto_array_element_by_index(&cursor, start_data_index) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            for (k = start_data_index; k <= end_data_index; k++)
            {
                if (j >= info->readout_offset[mdr_record])
                {
                    if (fieldname != NULL)
                    {
                        if (coda_cursor_goto_record_field_by_name(&cursor, fieldname) != 0)
                        {
                            harp_set_error(HARP_ERROR_CODA, NULL);
                            return -1;
                        }
                    }
                    if (coda_cursor_read_double(&cursor, double_data) != 0)
                    {
                        harp_set_error(HARP_ERROR_CODA, NULL);
                        return -1;
                    }
                    if (fieldname != NULL)
                    {
                        coda_cursor_goto_parent(&cursor);
                    }
                    copy_double_data_to_following_rows(info->max_num_recs[mdr_record],
                                                       end_data_index - start_data_index + 1, double_data);
                    double_data++;
                }
                else
                {
                    /* Skip row, this row remains filled with NaN */
                }
                if (k < end_data_index)
                {
                    if (coda_cursor_goto_next_array_element(&cursor) != 0)
                    {
                        harp_set_error(HARP_ERROR_CODA, NULL);
                        return -1;
                    }
                }
            }
            double_data +=
                ((MAX_READOUTS_PER_MDR_RECORD / info->max_num_recs[mdr_record]) - 1) * (end_data_index -
                                                                                        start_data_index + 1);
            coda_cursor_goto_parent(&cursor);
            coda_cursor_goto_parent(&cursor);
        }
        else    /* The dataset-field is not an array */
        {
            if (j >= info->readout_offset[mdr_record])
            {
                if (coda_cursor_goto_record_field_by_name(&cursor, fieldname) != 0)
                {
                    harp_set_error(HARP_ERROR_CODA, NULL);
                    return -1;
                }
                if (coda_cursor_read_double(&cursor, double_data) != 0)
                {
                    harp_set_error(HARP_ERROR_CODA, NULL);
                    return -1;
                }
                coda_cursor_goto_parent(&cursor);
                copy_double_data_to_following_rows(info->max_num_recs[mdr_record], 1, double_data);
                double_data += (MAX_READOUTS_PER_MDR_RECORD / info->max_num_recs[mdr_record]);
            }
            else
            {
                /* Skip row, this row remains filled with NaN */
                double_data += (MAX_READOUTS_PER_MDR_RECORD / info->max_num_recs[mdr_record]) - 1;
            }
            coda_cursor_goto_parent(&cursor);
        }
        if (j < (info->max_num_recs[mdr_record] - 1))
        {
            if (coda_cursor_goto_next_array_element(&cursor) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
        }
    }
    return 0;
}

/*
 * Read GEO_EARTH_ACTUAL data from a datafile with product version 11 or
 * lower. The data is retrieved from the path:
 * GEO_EARTH_ACTUAL/datasetname[band_nr][start_data_index..end_data_index][readout_nr]/fieldname
 */
static int get_main_geo_earth_actual_data_old_version(ingest_info *info, long mdr_record, const char *dataset_name,
                                                      const char *fieldname, long start_data_index, long end_data_index,
                                                      long data_dim_size, double *double_data_one_mdr_record)
{
    coda_cursor cursor;
    double *double_data;
    long i, j;

    cursor = info->mdr_lightsource_cursors[mdr_record];
    if (coda_cursor_goto_record_field_by_name(&cursor, "GEO_EARTH_ACTUAL") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&cursor, dataset_name) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    for (i = start_data_index; i <= end_data_index; i++)
    {
        if (coda_cursor_goto_array_element_by_index
            (&cursor,
             (info->band_nr_fastest_band[mdr_record] * data_dim_size * MAX_READOUTS_PER_MDR_RECORD) +
             (i * MAX_READOUTS_PER_MDR_RECORD)) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        double_data = double_data_one_mdr_record + i - start_data_index;
        for (j = 0; j < info->max_num_recs[mdr_record]; j++)
        {
            if (fieldname != NULL)
            {
                if (coda_cursor_goto_record_field_by_name(&cursor, fieldname) != 0)
                {
                    harp_set_error(HARP_ERROR_CODA, NULL);
                    return -1;
                }
            }
            if (coda_cursor_read_double(&cursor, double_data) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            if (fieldname != NULL)
            {
                coda_cursor_goto_parent(&cursor);
            }
            if (j >= info->readout_offset[mdr_record])
            {
                copy_double_data_to_following_rows(info->max_num_recs[mdr_record],
                                                   end_data_index - start_data_index + 1, double_data);
                double_data +=
                    ((MAX_READOUTS_PER_MDR_RECORD / info->max_num_recs[mdr_record]) *
                     (end_data_index - start_data_index + 1));
            }
            else
            {
                /* Skip rows, these rows remain filled with NaN */
                double_data +=
                    (((MAX_READOUTS_PER_MDR_RECORD / info->max_num_recs[mdr_record]) - 1) * (end_data_index -
                                                                                             start_data_index + 1));
            }

            if (j < (info->max_num_recs[mdr_record] - 1))
            {
                if (coda_cursor_goto_next_array_element(&cursor) != 0)
                {
                    harp_set_error(HARP_ERROR_CODA, NULL);
                    return -1;
                }
            }
        }
        coda_cursor_goto_parent(&cursor);
    }
    return 0;
}

static int get_main_geo_earth_actual_data(ingest_info *info, const char *dataset_name, const char *fieldname,
                                          long start_data_index, long end_data_index, long data_dim_size,
                                          double *double_data_array)
{
    double *double_data, *save_double_data, nan;
    long i, j, k;

    double_data = double_data_array;
    for (i = 0; i < info->num_mdr_records; i++)
    {
        save_double_data = double_data;
        /* set all values to NaN */
        nan = coda_NaN();
        for (j = start_data_index; j <= end_data_index; j++)
        {
            for (k = info->readout_offset[i]; k < MAX_READOUTS_PER_MDR_RECORD; k++)
            {
                *double_data = nan;
                double_data++;
            }
        }
        double_data = save_double_data;

        if (info->format_version >= 12)
        {
            if (get_main_geo_earth_actual_data_new_version
                (info, i, dataset_name, fieldname, start_data_index, end_data_index, data_dim_size, double_data) != 0)
            {
                return -1;
            }
        }
        else    /* info->format_version < 12 */
        {
            if (get_main_geo_earth_actual_data_old_version
                (info, i, dataset_name, fieldname, start_data_index, end_data_index, data_dim_size, double_data) != 0)
            {
                return -1;
            }
        }
        double_data +=
            ((end_data_index - start_data_index + 1L) * (MAX_READOUTS_PER_MDR_RECORD - info->readout_offset[i]));
    }
    return 0;
}

static int get_main_cloud_data(ingest_info *info, long mdr_record, int fit_number, double *fit_data)
{
    coda_cursor cursor;
    double invalid_value_boundary = (fit_number == 1 ? -2147483 : -2147);
    const char *fieldname = (fit_number == 1 ? "FIT_1" : "FIT_2");
    long i;
    uint8_t fit_mode[MAX_READOUTS_PER_MDR_RECORD];

    cursor = info->mdr_lightsource_cursors[mdr_record];
    if (coda_cursor_goto_record_field_by_name(&cursor, "CLOUD") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    /* If FIT_MODE != 0, the cloud data will be NaN */
    if (coda_cursor_goto_record_field_by_name(&cursor, "FIT_MODE") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_uint8_array(&cursor, fit_mode, coda_array_ordering_c) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    coda_cursor_goto_parent(&cursor);

    if (coda_cursor_goto_record_field_by_name(&cursor, fieldname) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_double_array(&cursor, fit_data, coda_array_ordering_c) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    for (i = 0; i < info->max_num_recs[mdr_record]; i++)
    {
        if (fit_mode[i] != 0)
        {
            return 1;
        }
        if (fit_data[i] < invalid_value_boundary)       /* Check on fill value */
        {
            return 1;
        }
    }
    return 0;
}

static int get_spectral_data_per_band(coda_cursor cursor_start_of_band, ingest_info *info, const char *fieldname,
                                      spectral_variable_type var_type, long mdr_record, int band_nr,
                                      double *data_startposition)
{
    const double undefined_int32_vsf_value = -2147483648.0E128;
    coda_cursor cursor, start_of_band_array_element;
    double *double_data, *dest_data, rad, nan, integration_time_this_band;
    long i, j, k, double_data_row;
    uint16_t num_recs_of_band;

    cursor = cursor_start_of_band;
    switch (var_type)
    {
        case RADIANCE:
            nan = coda_NaN();
            if (coda_cursor_goto_record_field_by_name(&cursor, "NUM_RECS") != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            if (coda_cursor_goto_array_element_by_index(&cursor, band_nr) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            if (coda_cursor_read_uint16(&cursor, &num_recs_of_band) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            cursor = cursor_start_of_band;
            if (coda_cursor_goto_record_field_by_name(&cursor, band_name_in_file[band_nr]) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            if (coda_cursor_goto_first_array_element(&cursor) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            double_data_row = 0;
            for (i = 0; i < num_recs_of_band; i++)
            {
                double_data = data_startposition + (info->total_num_pixels_all_bands * double_data_row);
                for (j = 0; j < info->num_pixels[band_nr]; j++)
                {
                    start_of_band_array_element = cursor;
                    if (coda_cursor_goto_record_field_by_name(&cursor, fieldname) != 0)
                    {
                        harp_set_error(HARP_ERROR_CODA, NULL);
                        return -1;
                    }
                    if (i >= info->readout_offset[mdr_record])
                    {
                        if (coda_cursor_read_double(&cursor, &rad) != 0)
                        {
                            harp_set_error(HARP_ERROR_CODA, NULL);
                            return -1;
                        }
                        /* We do a compare on the difference because absolute
                         * comparison of rad against undefined_int32_vsf_value
                         * may sometimes incorrectly return false due to
                         * rounding issues.
                         */
                        if (fabs(rad - undefined_int32_vsf_value) > fabs(undefined_int32_vsf_value * 1E-12))
                        {
                            *double_data = rad;
                        }
                        else
                        {
                            *double_data = nan;
                        }
                        copy_double_data_to_following_rows(num_recs_of_band, info->total_num_pixels_all_bands,
                                                           double_data);
                        double_data++;
                    }
                    cursor = start_of_band_array_element;
                    if ((j < (info->num_pixels[band_nr] - 1)) || (i < (num_recs_of_band - 1)))
                    {
                        if (coda_cursor_goto_next_array_element(&cursor) != 0)
                        {
                            harp_set_error(HARP_ERROR_CODA, NULL);
                            return -1;
                        }
                    }
                }
                if (i >= info->readout_offset[mdr_record])
                {
                    double_data_row += (MAX_READOUTS_PER_MDR_RECORD / num_recs_of_band);
                }
                else
                {
                    double_data_row += (MAX_READOUTS_PER_MDR_RECORD / num_recs_of_band) - 1;
                }
            }
            break;

        case WAVELENGTH:
            if (coda_cursor_goto_record_field_by_name(&cursor, wavelength_name_in_file[band_nr]) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            if (coda_cursor_goto_first_array_element(&cursor) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            double_data = data_startposition;
            for (j = 0; j < info->num_pixels[band_nr]; j++)
            {
                if (coda_cursor_read_double(&cursor, double_data) != 0)
                {
                    harp_set_error(HARP_ERROR_CODA, NULL);
                    return -1;
                }
                /* Copy the wavelength value to the rows below it */
                dest_data = double_data + info->total_num_pixels_all_bands;
                for (k = 1; k <= (31 - info->readout_offset[mdr_record]); k++)
                {
                    *dest_data = *double_data;
                    dest_data += info->total_num_pixels_all_bands;
                }
                double_data++;
                if (j < (info->num_pixels[band_nr] - 1))
                {
                    if (coda_cursor_goto_next_array_element(&cursor) != 0)
                    {
                        harp_set_error(HARP_ERROR_CODA, NULL);
                        return -1;
                    }
                }
            }
            break;

        case INTEGRATION_TIME:
            if (coda_cursor_goto_record_field_by_name(&cursor, "INTEGRATION_TIMES") != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            if (coda_cursor_goto_array_element_by_index(&cursor, band_nr) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            double_data = data_startposition;
            if (coda_cursor_read_double(&cursor, &integration_time_this_band) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            for (j = 0; j < info->num_pixels[band_nr]; j++)
            {
                dest_data = double_data;
                for (k = 0; k <= (31 - info->readout_offset[mdr_record]); k++)
                {
                    *dest_data = integration_time_this_band;
                    dest_data += info->total_num_pixels_all_bands;
                }
                double_data++;
            }
            break;
    }

    return 0;
}

static int get_spectral_data(ingest_info *info, const char *fieldname, spectral_variable_type var_type,
                             double *double_data_array)
{
    coda_cursor cursor;
    double *double_data, nan;
    long i, j;
    int band_nr;

    /* set all values to NaN */
    nan = coda_NaN();
    double_data = double_data_array;
    for (i = 0; i < info->num_mdr_records; i++)
    {
        for (j = (info->readout_offset[i] * info->total_num_pixels_all_bands);
             j < (MAX_READOUTS_PER_MDR_RECORD * info->total_num_pixels_all_bands); j++)
        {
            *double_data = nan;
            double_data++;
        }
    }

    double_data = double_data_array;
    for (i = 0; i < info->num_mdr_records; i++)
    {
        cursor = info->mdr_lightsource_cursors[i];
        if (info->band_nr < 0)
        {
            /* Ingest all bands */
            for (band_nr = 0; band_nr < MAX_NR_BANDS; band_nr++)
            {
                if (get_spectral_data_per_band
                    (cursor, info, fieldname, var_type, i, band_nr, double_data + info->offset_of_band[band_nr]) != 0)
                {
                    return -1;
                }
            }
        }
        else
        {
            /* Ingest only this band */
            if (get_spectral_data_per_band(cursor, info, fieldname, var_type, i, info->band_nr, double_data) != 0)
            {
                return -1;
            }
        }
        double_data += (MAX_READOUTS_PER_MDR_RECORD - info->readout_offset[i]) * info->total_num_pixels_all_bands;
    }
    return 0;
}

static int read_datetime(void *user_data, harp_array data)
{
    return get_main_datetime_data((ingest_info *)user_data, data.double_data);
}

static int read_latitude(void *user_data, harp_array data)
{
    return get_main_geo_earth_actual_data((ingest_info *)user_data, "CENTRE_ACTUAL", "latitude", 0, 0, 1,
                                          data.double_data);
}

static int read_longitude(void *user_data, harp_array data)
{
    return get_main_geo_earth_actual_data((ingest_info *)user_data, "CENTRE_ACTUAL", "longitude", 0, 0, 1,
                                          data.double_data);
}

static int read_latitude_bounds(void *user_data, harp_array data)
{
    return get_main_geo_earth_actual_data((ingest_info *)user_data, "CORNER_ACTUAL", "latitude", 0, 3, 4,
                                          data.double_data);
}

static int read_longitude_bounds(void *user_data, harp_array data)
{
    return get_main_geo_earth_actual_data((ingest_info *)user_data, "CORNER_ACTUAL", "longitude", 0, 3, 4,
                                          data.double_data);
}

static int read_wavelength_photon_radiance(void *user_data, harp_array data)
{
    return get_spectral_data((ingest_info *)user_data, "RAD", RADIANCE, data.double_data);
}

static int read_transmittance(void *user_data, harp_array data)
{
    return get_spectral_data((ingest_info *)user_data, "RAD", RADIANCE, data.double_data);
}

static int read_sun_wavelength_photon_irradiance(void *user_data, harp_array data)
{
    return get_spectral_data((ingest_info *)user_data, "RAD", RADIANCE, data.double_data);
}

static int read_moon_wavelength_photon_irradiance(void *user_data, harp_array data)
{
    return get_spectral_data((ingest_info *)user_data, "RAD", RADIANCE, data.double_data);
}

static int read_wavelength(void *user_data, harp_array data)
{
    return get_spectral_data((ingest_info *)user_data, NULL, WAVELENGTH, data.double_data);
}

static int read_datetime_length(void *user_data, harp_array data)
{
    return get_spectral_data((ingest_info *)user_data, NULL, INTEGRATION_TIME, data.double_data);
}

static int read_scan_subset_counter(void *user_data, harp_array data)
{
    ingest_info *info;
    int8_t *int_data;
    long i, j;

    info = (ingest_info *)user_data;
    int_data = data.int8_data;
    for (i = 0; i < info->num_mdr_records; i++)
    {
        if (info->readout_offset[i] == 0)
        {
            *int_data = (MAX_READOUTS_PER_MDR_RECORD - 1) / 2;
            int_data++;
        }
        for (j = 0; j < (MAX_READOUTS_PER_MDR_RECORD - 1); j++)
        {
            *int_data = (uint8_t)(j / 2);
            int_data++;
        }
    }
    return 0;
}

static int read_scan_direction(void *user_data, long index, harp_array data)
{
    ingest_info *info;
    long subset_counter;
    static long index_plus_readout_offset;

    info = (ingest_info *)user_data;
    if ((index % 32 == 0) && (info->readout_offset[index / 32] != 0))
    {
        index_plus_readout_offset += info->readout_offset[index / 32];
    }
    if (index_plus_readout_offset == 0L)
    {
        /* First readout is from previous scan so subset counter = 15 */
        subset_counter = 15;
    }
    else
    {
        subset_counter = (((index_plus_readout_offset - 1) % 32) / 2);
    }
    if (info->no_mixed_pixel[index_plus_readout_offset / 32])
    {
        if (subset_counter < 12)
        {
            *(data.string_data) = strdup("forward");
        }
        else
        {
            *(data.string_data) = strdup("backward");
        }
    }
    else
    {
        *(data.string_data) = strdup("mixed");
    }
    index_plus_readout_offset++;

    return 0;
}

static int read_cloud_top_pressure(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    double *double_data, *start_mdr_record;
    double fit_data[MAX_READOUTS_PER_MDR_RECORD], total;
    long i, j, k, combined_rows;
    int retval;

    double_data = data.double_data;
    for (i = 0; i < info->num_mdr_records; i++)
    {
        retval = get_main_cloud_data(info, i, 1, fit_data);
        if (retval < 0)
        {
            /* A real error during the reading of data, stop this program */
            return -1;
        }
        else if (retval > 0)
        {
            /* The cloud data for this MDR record is invalid */
            for (j = info->readout_offset[i]; j < MAX_READOUTS_PER_MDR_RECORD; j++)
            {
                *double_data = coda_NaN();
                double_data++;
            }
            continue;
        }

        if (info->max_num_recs[i] < MAX_READOUTS_PER_MDR_RECORD)
        {
            combined_rows = MAX_READOUTS_PER_MDR_RECORD / info->max_num_recs[i];
            start_mdr_record = double_data;
            for (j = info->readout_offset[i]; j < info->max_num_recs[i]; j++)
            {
                /* We use logarithmic averaging:
                 *     10^( (log10(x1) + log10(x2) + ... log10(xn)) / n ) =>
                 *     10^( log10(x1 * x2 * ... * xn) / n ) =>
                 *     pow(x1 * x2 * ... * xn, 1/n)
                 */
                total = 1.0;
                for (k = 0; k < combined_rows; k++)
                {
                    total *= fit_data[j * combined_rows + k];
                }
                *double_data = pow(total, 1.0 / combined_rows);
                for (k = 1; k < combined_rows; k++)
                {
                    *(double_data + k) = *double_data;
                }
                double_data += combined_rows;
            }
            for (; double_data < (start_mdr_record + MAX_READOUTS_PER_MDR_RECORD - info->readout_offset[i]);
                 double_data++)
            {
                *double_data = coda_NaN();
            }
        }
        else
        {
            for (j = info->readout_offset[i]; j < MAX_READOUTS_PER_MDR_RECORD; j++)
            {
                *double_data = fit_data[j];
                double_data++;
            }
        }
    }
    return 0;
}

static int read_cloud_fraction(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    double *double_data, *start_mdr_record;
    double fit_data[MAX_READOUTS_PER_MDR_RECORD], total;
    long i, j, k, combined_rows;
    int retval;

    double_data = data.double_data;
    for (i = 0; i < info->num_mdr_records; i++)
    {
        retval = get_main_cloud_data(info, i, 2, fit_data);
        if (retval < 0)
        {
            /* A real error during the reading of data, stop this program */
            return -1;
        }
        else if (retval > 0)
        {
            /* The cloud data for this MDR record is invalid */
            for (j = info->readout_offset[i]; j < MAX_READOUTS_PER_MDR_RECORD; j++)
            {
                *double_data = coda_NaN();
                double_data++;
            }
            continue;
        }

        if (info->max_num_recs[i] < MAX_READOUTS_PER_MDR_RECORD)
        {
            combined_rows = MAX_READOUTS_PER_MDR_RECORD / info->max_num_recs[i];
            start_mdr_record = double_data;
            for (j = info->readout_offset[i]; j < info->max_num_recs[i]; j++)
            {
                /* We average the values */
                total = 0.0;
                for (k = 0; k < combined_rows; k++)
                {
                    total += fit_data[j * combined_rows + k];
                }
                *double_data = total / combined_rows;
                for (k = 1; k < combined_rows; k++)
                {
                    *(double_data + k) = *double_data;
                }
                double_data += combined_rows;
            }
            for (; double_data < (start_mdr_record + MAX_READOUTS_PER_MDR_RECORD - info->readout_offset[i]);
                 double_data++)
            {
                *double_data = coda_NaN();
            }
        }
        else
        {
            for (j = info->readout_offset[i]; j < MAX_READOUTS_PER_MDR_RECORD; j++)
            {
                *double_data = fit_data[j];
                double_data++;
            }
        }
    }
    return 0;
}

static int read_solar_zenith_angle(void *user_data, harp_array data)
{
    return get_main_geo_earth_actual_data((ingest_info *)user_data, "SOLAR_ZENITH_ACTUAL", NULL, 1, 1, 3,
                                          data.double_data);
}

static int read_solar_azimuth_angle(void *user_data, harp_array data)
{
    return get_main_geo_earth_actual_data((ingest_info *)user_data, "SOLAR_AZIMUTH_ACTUAL", NULL, 1, 1, 3,
                                          data.double_data);
}

static int read_viewing_zenith_angle(void *user_data, harp_array data)
{
    return get_main_geo_earth_actual_data((ingest_info *)user_data, "SAT_ZENITH_ACTUAL", NULL, 1, 1, 3,
                                          data.double_data);
}

static int read_viewing_azimuth_angle(void *user_data, harp_array data)
{
    return get_main_geo_earth_actual_data((ingest_info *)user_data, "SAT_AZIMUTH_ACTUAL", NULL, 1, 1, 3,
                                          data.double_data);
}

static int mdr_record_is_valid(ingest_info *info, coda_cursor *cursor)
{
    coda_type_class type_class;
    uint8_t output_selection;

    if (coda_cursor_goto_record_field_by_name(cursor, info->lightsource) != 0)
    {
        return FALSE;
    }
    if (coda_cursor_get_type_class(cursor, &type_class) != 0)
    {
        return FALSE;
    }
    if (type_class != coda_record_class)
    {
        return FALSE;
    }

    if (info->ingestion_data == DATA_RADIANCE || info->ingestion_data == DATA_TRANSMISSION)
    {
        if (coda_cursor_goto_record_field_by_name(cursor, "OUTPUT_SELECTION") != 0)
        {
            return FALSE;
        }
        /* Possible values: 0 = measured radiance, 1 = sun normalised radiance (i.e. transmittance) */
        if (coda_cursor_read_uint8(cursor, &output_selection) != 0)
        {
            coda_cursor_goto_parent(cursor);
            return FALSE;
        }
        coda_cursor_goto_parent(cursor);
        if ((output_selection == 1 && info->ingestion_data != DATA_TRANSMISSION) ||
            (output_selection == 0 && info->ingestion_data == DATA_TRANSMISSION))
        {
            /* The output selection of this MDR record does not match */
            /* the ingestion parameters, skip this MDR record.        */
            return FALSE;
        }
    }
    return TRUE;
}

static int determine_fastest_band(ingest_info *info, coda_cursor start_cursor, long valid_mdr_record)
{
    static uint16_t previous_num_recs_of_band[MAX_NR_BANDS] = { 0 };
    coda_cursor cursor;
    double min_integration_time;
    uint16_t num_recs_of_band;
    int i;

    cursor = start_cursor;
    /* Determine the band with the most detailed data     */
    /* (i.e. the band with the fastest integration time). */
    if (coda_cursor_goto_record_field_by_name(&cursor, "NUM_RECS") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_first_array_element(&cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    min_integration_time = 1000.0;
    for (i = 0; i < MAX_NR_BANDS; i++)
    {
        if (coda_cursor_read_uint16(&cursor, &num_recs_of_band) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (num_recs_of_band != previous_num_recs_of_band[i])
        {
            previous_num_recs_of_band[i] = num_recs_of_band;
            info->readout_offset[valid_mdr_record] = 1;
        }
        if (num_recs_of_band > info->max_num_recs[valid_mdr_record])
        {
            info->max_num_recs[valid_mdr_record] = num_recs_of_band;
            info->band_nr_fastest_band[valid_mdr_record] = i;
            min_integration_time = 6.0 / num_recs_of_band;
        }
        if (i < (MAX_NR_BANDS - 1))
        {
            if (coda_cursor_goto_next_array_element(&cursor) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
        }
    }
    if (min_integration_time <= 1.5)
    {
        /* There is at least one band with an integration time */
        /* equal or smaller than 1.5 seconds so there are no   */
        /* mixed pixels.                                       */
        info->no_mixed_pixel[valid_mdr_record] = TRUE;
    }

    if (info->format_version >= 12)
    {
        double unique_int[10];

        cursor = start_cursor;
        if (coda_cursor_goto_record_field_by_name(&cursor, "UNIQUE_INT") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_read_double_array(&cursor, unique_int, coda_array_ordering_c) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        for (i = 1; i <= 10; i++)
        {
            /* Check if we have found the min_integration_time, use a */
            /* margin of 0.01 to prevent rounding problems.           */
            if (fabs(unique_int[i - 1] - min_integration_time) < 0.01)
            {
                info->index_of_fastest_timer_in_list_of_timers[valid_mdr_record] = i;
                break;
            }
        }
        if (i > 10)
        {
            harp_set_error(HARP_ERROR_INGESTION,
                           "Can't find minimum integration time %lf in array of integration times",
                           min_integration_time);
            return -1;
        }
    }
    return 0;
}

static int init_measurements_dimensions(ingest_info *info)
{
    coda_cursor cursor, save_cursor_mdr, save_cursor_lightsource;
    double time_of_mdr_record, time_of_prev_mdr_record;
    long num_all_mdr_records, mdr_record, valid_mdr_record;
    long band_nr, offset, dim[CODA_MAX_NUM_DIMS];
    int num_dims, prev_mdr_record_was_valid;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    /* Count the number of valid MDR records */
    if (coda_cursor_goto_record_field_by_name(&cursor, "MDR") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &num_all_mdr_records) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_first_array_element(&cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    for (mdr_record = 0, valid_mdr_record = 0; mdr_record < num_all_mdr_records; mdr_record++)
    {
        save_cursor_mdr = cursor;
        if (mdr_record_is_valid(info, &cursor))
        {
            valid_mdr_record++;
        }
        cursor = save_cursor_mdr;
        if (mdr_record < (num_all_mdr_records - 1))
        {
            if (coda_cursor_goto_next_array_element(&cursor) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
        }
    }
    coda_cursor_goto_parent(&cursor);

    CHECKED_MALLOC(info->mdr_lightsource_cursors, valid_mdr_record * sizeof(coda_cursor));
    CHECKED_MALLOC(info->max_num_recs, valid_mdr_record * sizeof(uint16_t));
    CHECKED_MALLOC(info->band_nr_fastest_band, valid_mdr_record * sizeof(int));
    CHECKED_MALLOC(info->index_of_fastest_timer_in_list_of_timers, valid_mdr_record * sizeof(int));
    CHECKED_MALLOC(info->no_mixed_pixel, valid_mdr_record * sizeof(short));
    CHECKED_MALLOC(info->readout_offset, valid_mdr_record * sizeof(short));

    if (coda_cursor_goto_first_array_element(&cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    prev_mdr_record_was_valid = FALSE;
    time_of_prev_mdr_record = 0.0;
    for (mdr_record = 0, valid_mdr_record = 0; mdr_record < num_all_mdr_records; mdr_record++)
    {
        save_cursor_mdr = cursor;

        if (mdr_record_is_valid(info, &cursor))
        {
            info->mdr_lightsource_cursors[valid_mdr_record] = cursor;
            info->max_num_recs[valid_mdr_record] = 0;
            info->band_nr_fastest_band[valid_mdr_record] = 0;
            info->index_of_fastest_timer_in_list_of_timers[valid_mdr_record] = 1;
            info->no_mixed_pixel[valid_mdr_record] = FALSE;
            info->readout_offset[valid_mdr_record] = 0;
            if (!prev_mdr_record_was_valid)
            {
                info->readout_offset[valid_mdr_record] = 1;
            }

            save_cursor_lightsource = cursor;
            if (coda_cursor_goto(&cursor, "RECORD_HEADER") != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            if (coda_cursor_goto_record_field_by_name(&cursor, "RECORD_START_TIME") != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            if (coda_cursor_read_double(&cursor, &time_of_mdr_record) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            cursor = save_cursor_lightsource;
            if (fabs((time_of_mdr_record - time_of_prev_mdr_record) - 6.0) > 0.1)
            {
                info->readout_offset[valid_mdr_record] = 1;
            }

            if (determine_fastest_band(info, cursor, valid_mdr_record) != 0)
            {
                return -1;
            }
            valid_mdr_record++;

            /* Count the number of spectra per band */
            for (band_nr = 0; band_nr < MAX_NR_BANDS; band_nr++)
            {
                save_cursor_lightsource = cursor;
                if (coda_cursor_goto_record_field_by_name(&cursor, band_name_in_file[band_nr]) != 0)
                {
                    harp_set_error(HARP_ERROR_CODA, NULL);
                    return -1;
                }
                if (coda_cursor_get_array_dim(&cursor, &num_dims, dim) != 0)
                {
                    harp_set_error(HARP_ERROR_CODA, NULL);
                    return -1;
                }
                /* dim[0] = number of measurements for this band during a     */
                /*          (6 second) scan.                                  */
                /* dim[1] = number of pixels in one measurement for this band */
                if (info->num_pixels[band_nr] == 0)
                {
                    info->num_pixels[band_nr] = dim[1];
                }
                else if (info->num_pixels[band_nr] != dim[1])
                {
                    harp_set_error(HARP_ERROR_INGESTION, "Number of pixels for band %s is changed from %ld to %ld",
                                   band_name_in_file[band_nr], info->num_pixels[band_nr], dim[1]);
                    info->num_pixels[band_nr] = dim[1];
                }
                cursor = save_cursor_lightsource;
            }
            prev_mdr_record_was_valid = TRUE;
            time_of_prev_mdr_record = time_of_mdr_record;
        }
        else
        {
            prev_mdr_record_was_valid = FALSE;
        }

        cursor = save_cursor_mdr;
        if (mdr_record < (num_all_mdr_records - 1))
        {
            if (coda_cursor_goto_next_array_element(&cursor) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
        }
    }
    info->num_mdr_records = valid_mdr_record;

    coda_cursor_goto_root(&cursor);

    offset = 0L;
    for (band_nr = 0; band_nr < MAX_NR_BANDS; band_nr++)
    {
        info->offset_of_band[band_nr] = offset;
        offset += info->num_pixels[band_nr];
    }
    if (info->band_nr >= 0)
    {
        info->total_num_pixels_all_bands = info->num_pixels[info->band_nr];
    }
    else
    {
        info->total_num_pixels_all_bands = MAX_PIXELS;
    }
    return 0;
}

static int exclude_when_not_moon(void *user_data)
{
    ingest_info *info = (ingest_info *)user_data;

    return (info->ingestion_data != DATA_MOON);
}

static int exclude_when_not_sun(void *user_data)
{
    ingest_info *info = (ingest_info *)user_data;

    return (info->ingestion_data != DATA_SUN);
}

static int exclude_when_not_radiance_or_transmission(void *user_data)
{
    ingest_info *info = (ingest_info *)user_data;

    return ((info->ingestion_data != DATA_RADIANCE) && (info->ingestion_data != DATA_TRANSMISSION));
}

static int exclude_when_not_transmission(void *user_data)
{
    ingest_info *info = (ingest_info *)user_data;

    return (info->ingestion_data != DATA_TRANSMISSION);
}

static int exclude_when_not_radiance(void *user_data)
{
    ingest_info *info = (ingest_info *)user_data;

    return (info->ingestion_data != DATA_RADIANCE);
}

static void register_variables_measurement_fields(harp_product_definition *product_definition)
{
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2];
    harp_dimension_type bounds_dimension_type[2];
    long bounds_dimension[2] = { -1, 4 };
    const char *description;
    const char *path;

    dimension_type[0] = harp_dimension_time;
    dimension_type[1] = harp_dimension_spectral;

    bounds_dimension_type[0] = harp_dimension_time;
    bounds_dimension_type[1] = harp_dimension_independent;

    /* time_of_the_measurement */
    description = "time of the measurement at the end of the integration time";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double, 1, dimension_type,
                                                   NULL, description, "seconds since 2000-01-01", NULL, read_datetime);
    path = "/MDR[]/Earthshine/RECORD_HEADER/RECORD_START_TIME";
    description =
        "The record start time is the start time of the scan and thus the start time of the second readout in the MDR. The start time for readout i (0..31) is thus RECORD_START_TIME + (i - 1) * 0.1875 and the time at end of integration time (which is the time that is returned) is RECORD_START_TIME + i * 0.1875";
    harp_variable_definition_add_mapping(variable_definition, "data=radiance", NULL, path, description);
    harp_variable_definition_add_mapping(variable_definition, "data=transmission", NULL, path, description);
    path = "/MDR[]/Sun/RECORD_HEADER/RECORD_START_TIME";
    harp_variable_definition_add_mapping(variable_definition, "data=sun", NULL, path, description);
    path = "/MDR[]/Moon/RECORD_HEADER/RECORD_START_TIME";
    harp_variable_definition_add_mapping(variable_definition, "data=moon", NULL, path, description);

    /* latitude_of_the_measurement */
    description = "center latitude of the measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_double, 1, dimension_type,
                                                   NULL, description, "degree_north",
                                                   exclude_when_not_radiance_or_transmission, read_latitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    path = "/MDR[]/Earthshine/GEO_EARTH_ACTUAL/CENTRE_ACTUAL[INT_INDEX[band_id],]/latitude";
    description =
        "The integration time index INT_INDEX[band_id] is the index of the band with the minimum integration time (limited to those bands that are ingested).";
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA version < 12", path, description);
    path = "/MDR[]/Earthshine/GEO_EARTH_ACTUAL_INT_INDEX[timer_id][]/CENTRE_ACTUAL/latitude";
    description =
        "The integration time index INT_INDEX[timer_id] is the index (starting with 1) of the timer with the minimum integration time (limited to the timers of those bands that are ingested).";
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA version >= 12", path, description);

    /* longitude_of_the_measurement */
    description = "center longitude of the measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_double, 1, dimension_type,
                                                   NULL, description, "degree_east",
                                                   exclude_when_not_radiance_or_transmission, read_longitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0, 180.0);
    path = "/MDR[]/Earthshine/GEO_EARTH_ACTUAL/CENTRE_ACTUAL[INT_INDEX[band_id],]/longitude";
    description =
        "The integration time index INT_INDEX[band_id] is the index of the band with the minimum integration time (limited to those bands that are ingested).";
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA version < 12", path, description);
    path = "/MDR[]/Earthshine/GEO_EARTH_ACTUAL_INT_INDEX[timer_id][]/CENTRE_ACTUAL/longitude";
    description =
        "The integration time index INT_INDEX[timer_id] is the index (starting with 1) of the timer with the minimum integration time (limited to the timers of those bands that are ingested).";
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA version >= 12", path, description);

    /* latitude_bounds */
    description = "corner latitudes for the ground pixel of the measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "latitude_bounds", harp_type_double, 2,
                                                   bounds_dimension_type, bounds_dimension, description, "degree_north",
                                                   exclude_when_not_radiance_or_transmission, read_latitude_bounds);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    path = "/MDR[]/Earthshine/GEO_EARTH_ACTUAL/CORNER_ACTUAL[INT_INDEX[band_id],,]/latitude";
    description =
        "The integration time index INT_INDEX[band_id] is the index of the band with the minimum integration time (limited to those bands that are ingested). The corners ABCD are reordered as BDCA.";
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA version < 12", path, description);
    path = "/MDR[]/Earthshine/GEO_EARTH_ACTUAL_INT_INDEX[timer_id][]/CORNER_ACTUAL[]/latitude";
    description =
        "The integration time index INT_INDEX[timer_id] is the index (starting with 1) of the timer with the minimum integration time (limited to the timers of those bands that are ingested). The corners ABCD are reordered as BDCA.";
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA version >= 12", path, description);

    /* longitude_bounds */
    description = "corner longitudes for the ground pixel of the measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "longitude_bounds", harp_type_double, 2,
                                                   bounds_dimension_type, bounds_dimension, description, "degree_east",
                                                   exclude_when_not_radiance_or_transmission, read_longitude_bounds);
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0, 180.0);
    path = "/MDR[]/Earthshine/GEO_EARTH_ACTUAL/CORNER_ACTUAL[INT_INDEX[band_id],,]/longitude";
    description =
        "The integration time index INT_INDEX[band_id] is the index of the band with the minimum integration time (limited to those bands that are ingested). The corners ABCD are reordered as BDCA.";
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA version < 12", path, description);
    path = "/MDR[]/Earthshine/GEO_EARTH_ACTUAL_INT_INDEX[timer_id][]/CORNER_ACTUAL[]/longitude";
    description =
        "The integration time index INT_INDEX[timer_id] is the index (starting with 1) of the timer with the minimum integration time (limited to the timers of those bands that are ingested). The corners ABCD are reordered as BDCA.";
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA version >= 12", path, description);

    /* wavelength_photon_radiance */
    description = "measured radiances";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "wavelength_photon_radiance", harp_type_double,
                                                   2, dimension_type, NULL, description, "count/s/cm2/sr/nm",
                                                   exclude_when_not_radiance, read_wavelength_photon_radiance);
    path =
        "/MDR[]/Earthshine/BAND_1A[,]/RAD, /MDR[]/Earthshine/BAND_1B[,]/RAD, /MDR[]/Earthshine/BAND_2A[,]/RAD, /MDR[]/Earthshine/BAND_2B[,]/RAD, /MDR[]/Earthshine/BAND_3[,]/RAD, /MDR[]/Earthshine/BAND_4[,]/RAD";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* transmittance */
    description = "transmittance";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "transmittance", harp_type_double,
                                                   2, dimension_type, NULL, description, NULL,
                                                   exclude_when_not_transmission, read_transmittance);
    path =
        "/MDR[]/Earthshine/BAND_1A[,]/RAD, /MDR[]/Earthshine/BAND_1B[,]/RAD, /MDR[]/Earthshine/BAND_2A[,]/RAD, /MDR[]/Earthshine/BAND_2B[,]/RAD, /MDR[]/Earthshine/BAND_3[,]/RAD, /MDR[]/Earthshine/BAND_4[,]/RAD";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* wavelength_photon_irradiance of the sun */
    description = "measured sun irradiances";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "wavelength_photon_irradiance_sun",
                                                   harp_type_double, 2, dimension_type, NULL, description,
                                                   "count/s/cm2/nm", exclude_when_not_sun,
                                                   read_sun_wavelength_photon_irradiance);
    path =
        "/MDR[]/Sun/BAND_1A[,]/RAD, /MDR[]/Sun/BAND_1B[,]/RAD, /MDR[]/Sun/BAND_2A[,]/RAD, /MDR[]/Sun/BAND_2B[,]/RAD, /MDR[]/Sun/BAND_3[,]/RAD, /MDR[]/Sun/BAND_4[,]/RAD";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* wavelength_photon_irradiance of the moon */
    description = "measured moon irradiances";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "wavelength_photon_irradiance_moon",
                                                   harp_type_double, 2, dimension_type, NULL, description,
                                                   "count/s/cm2/nm", exclude_when_not_moon,
                                                   read_moon_wavelength_photon_irradiance);
    path =
        "/MDR[]/Moon/BAND_1A[,]/RAD, /MDR[]/Moon/BAND_1B[,]/RAD, /MDR[]/Moon/BAND_2A[,]/RAD, /MDR[]/Moon/BAND_2B[,]/RAD, /MDR[]/Moon/BAND_3[,]/RAD, /MDR[]/Moon/BAND_4[,]/RAD";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* wavelength */
    description = "nominal wavelength assignment for each of the detector pixels";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "wavelength", harp_type_double, 2,
                                                   dimension_type, NULL, description, "nm", NULL, read_wavelength);
    path =
        "/MDR[]/Earthshine/WAVELENGTH_1A[], /MDR[]/Earthshine/WAVELENGTH_1B[], /MDR[]/Earthshine/WAVELENGTH_2A[], /MDR/Earthshine[]/WAVELENGTH_2B[], /MDR[]/Earthshine/WAVELENGTH_3[], /MDR[]/Earthshine/WAVELENGTH_4[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "data=radiance or data=transmission", path, NULL);
    path =
        "/MDR[]/Sun/WAVELENGTH_1A[], /MDR[]/Sun/WAVELENGTH_1B[], /MDR[]/Sun/WAVELENGTH_2A[], /MDR[]/Sun/WAVELENGTH_2B[], /MDR[]/Sun/WAVELENGTH_3[], /MDR[]/Sun/WAVELENGTH_4[]";
    harp_variable_definition_add_mapping(variable_definition, "data=sun", NULL, path, NULL);
    path =
        "/MDR[]/Moon/WAVELENGTH_1A[], /MDR[]/Moon/WAVELENGTH_1B[], /MDR[]/Moon/WAVELENGTH_2A[], /MDR[]/Moon/WAVELENGTH_2B[], /MDR[]/Moon/WAVELENGTH_3[], /MDR[]/Moon/WAVELENGTH_4[]";
    harp_variable_definition_add_mapping(variable_definition, "data=moon", NULL, path, NULL);

    /* datetime_length */
    description = "integration time for each pixel";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime_length", harp_type_double, 2,
                                                   dimension_type, NULL, description, "s", NULL, read_datetime_length);
    path = "/MDR[]/Earthshine/INTEGRATION_TIMES[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "data=radiance or data=transmission", path, NULL);
    path = "/MDR[]/Sun/INTEGRATION_TIMES[]";
    harp_variable_definition_add_mapping(variable_definition, "data=sun", NULL, path, NULL);
    path = "/MDR[]/Moon/INTEGRATION_TIMES[]";
    harp_variable_definition_add_mapping(variable_definition, "data=moon", NULL, path, NULL);

    /* scan_subset_counter */
    description = "relative index (0-3) of this measurement within a scan (forward+backward)";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "scan_subset_counter", harp_type_int8, 1,
                                                   dimension_type, NULL, description, NULL,
                                                   exclude_when_not_radiance_or_transmission, read_scan_subset_counter);
    harp_variable_definition_set_valid_range_int8(variable_definition, 0, 3);

    /* scan_direction */
    description =
        "scan direction for each measurement: 'forward', 'backward' or 'mixed' (for a measurement that consisted of both a forward and backward scan)";
    variable_definition =
        harp_ingestion_register_variable_sample_read(product_definition, "scan_direction", harp_type_string, 1,
                                                     dimension_type, NULL, description, NULL,
                                                     exclude_when_not_radiance_or_transmission, read_scan_direction);
    path = "/MDR[]/Earthshine/INTEGRATION_TIMES[]";
    description =
        "when the integration time is higher than 1.5 s we are dealing with a mixed pixel, otherwise the scan direction is based on the subset counter of the measurement (0-11 forward, 12-15 = backward)";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* cloud_top_pressure */
    description = "cloud top pressure";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_top_pressure", harp_type_double, 1,
                                                   dimension_type, NULL, description, NULL,
                                                   exclude_when_not_radiance_or_transmission, read_cloud_top_pressure);
    path = "/MDR[]/Earthshine/CLOUD/FIT_1[]";
    description =
        "If the minimum ingested integration time > 187.5ms then the corresponding cloud top pressures will be combined using logarithmic averaging. The cloud top pressure will be set to NaN if FIT_MODE in the CLOUD structure is not equal to 0 or if FIT_1 is set to a fill value (even when this holds for only one of the averaged items)";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* cloud_fraction */
    description = "cloud fraction";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_fraction", harp_type_double, 1,
                                                   dimension_type, NULL, description, NULL,
                                                   exclude_when_not_radiance_or_transmission, read_cloud_fraction);
    path = "/MDR[]/Earthshine/CLOUD/FIT_2[]";
    description =
        "If the minimum ingested integration time > 187.5ms then the corresponding cloud fractions will be combined using averaging. The cloud fraction will be set to NaN if FIT_MODE in the CLOUD structure is not equal to 0 or if FIT_2 is set to a fill value (even when this holds for only one of the averaged items)";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* solar_zenith_angle */
    description = "solar zenith angle at top of atmosphere";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "solar_zenith_angle_toa", harp_type_double, 1,
                                                   dimension_type, NULL, description, "degree",
                                                   exclude_when_not_radiance_or_transmission, read_solar_zenith_angle);
    path = "/MDR[]/Earthshine/GEO_EARTH_ACTUAL/SOLAR_ZENITH_ACTUAL[INT_INDEX[band_id],1,]";
    description =
        "The integration time index INT_INDEX[band_id] is the index of the band with the minimum integration time (limited to those bands that are ingested).";
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA version < 12", path, description);
    path = "/MDR[]/Earthshine/GEO_EARTH_ACTUAL_INT_INDEX[timer_id][]/SOLAR_ZENITH_ACTUAL[1]";
    description =
        "The integration time index INT_INDEX[timer_id] is the index (starting with 1) of the timer with the minimum integration time (limited to the timers of those bands that are ingested).";
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA version >= 12", path, description);

    /* solar_azimuth_angle */
    description = "solar azimuth angle at top of atmosphere";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "solar_azimuth_angle_toa", harp_type_double, 1,
                                                   dimension_type, NULL, description, "degree",
                                                   exclude_when_not_radiance_or_transmission, read_solar_azimuth_angle);
    path = "/MDR[]/Earthshine/GEO_EARTH_ACTUAL/SOLAR_AZIMUTH_ACTUAL[INT_INDEX[band_id],1,]";
    description =
        "The integration time index INT_INDEX[band_id] is the index of the band with the minimum integration time (limited to those bands that are ingested).";
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA version < 12", path, description);
    path = "/MDR[]/Earthshine/GEO_EARTH_ACTUAL_INT_INDEX[timer_id][]/SOLAR_AZIMUTH_ACTUAL[1]";
    description =
        "The integration time index INT_INDEX[timer_id] is the index (starting with 1) of the timer with the minimum integration time (limited to the timers of those bands that are ingested).";
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA version >= 12", path, description);

    /* viewing_zenith_angle */
    description = "viewing zenith angle at top of atmosphere";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "viewing_zenith_angle_toa", harp_type_double, 1,
                                                   dimension_type, NULL, description, "degree",
                                                   exclude_when_not_radiance_or_transmission,
                                                   read_viewing_zenith_angle);
    path = "/MDR[]/Earthshine/GEO_EARTH_ACTUAL/SAT_ZENITH_ACTUAL[INT_INDEX[band_id],1,]";
    description =
        "The integration time index INT_INDEX[band_id] is the index of the band with the minimum integration time (limited to those bands that are ingested).";
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA version < 12", path, description);
    path = "/MDR[]/Earthshine/GEO_EARTH_ACTUAL_INT_INDEX[timer_id][]/SAT_ZENITH_ACTUAL[1]";
    description =
        "The integration time index INT_INDEX[timer_id] is the index (starting with 1) of the timer with the minimum integration time (limited to the timers of those bands that are ingested).";
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA version >= 12", path, description);

    /* viewing_azimuth_angle */
    description = "viewing azimuth angle at top of atmosphere";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "viewing_azimuth_angle_toa", harp_type_double, 1,
                                                   dimension_type, NULL, description, "degree",
                                                   exclude_when_not_radiance_or_transmission,
                                                   read_viewing_azimuth_angle);
    path = "/MDR[]/Earthshine/GEO_EARTH_ACTUAL/SAT_AZIMUTH_ACTUAL[INT_INDEX[band_id],1,]";
    description =
        "The integration time index INT_INDEX[band_id] is the index of the band with the minimum integration time (limited to those bands that are ingested).";
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA version < 12", path, description);
    path = "/MDR[]/Earthshine/GEO_EARTH_ACTUAL_INT_INDEX[timer_id][]/SAT_AZIMUTH_ACTUAL[1]";
    description =
        "The integration time index INT_INDEX[timer_id] is the index (starting with 1) of the timer with the minimum integration time (limited to the timers of those bands that are ingested).";
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA version >= 12", path, description);
}

static int read_dimensions_measurements_fields(void *user_data, long dimension[HARP_NUM_DIM_TYPES])
{
    ingest_info *info = (ingest_info *)user_data;
    long i, total_skipped_readouts;

    total_skipped_readouts = 0;
    for (i = 0; i < info->num_mdr_records; i++)
    {
        total_skipped_readouts += info->readout_offset[i];
    }

    dimension[harp_dimension_time] = (info->num_mdr_records * MAX_READOUTS_PER_MDR_RECORD) - total_skipped_readouts;
    dimension[harp_dimension_spectral] = info->total_num_pixels_all_bands;
    return 0;
}

/* Start of code for the ingestion of a reference spectrum */

static int get_smr_datetime(ingest_info *info, double *double_data_array, const char *fieldname)
{
    coda_cursor cursor;
    double *double_data;
    long i;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&cursor, "VIADR_SMR") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_first_array_element(&cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    double_data = double_data_array;
    for (i = 0; i < info->num_viadr_smr_records; i++)
    {
        if (coda_cursor_goto_record_field_by_name(&cursor, fieldname) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_read_double(&cursor, double_data) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        coda_cursor_goto_parent(&cursor);
        double_data++;
        if (i < (info->num_viadr_smr_records - 1))
        {
            if (coda_cursor_goto_next_array_element(&cursor) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
        }
    }
    return 0;
}

static int get_smr_spectral_data(ingest_info *info, const char *fieldname, double *double_data_array)
{
    coda_cursor cursor;
    double *double_data;
    long i, offset;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&cursor, "VIADR_SMR") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_first_array_element(&cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    double_data = double_data_array;
    for (i = 0; i < info->num_viadr_smr_records; i++)
    {
        if (coda_cursor_goto_record_field_by_name(&cursor, fieldname) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (info->band_nr >= 0)
        {
            offset = info->offset_of_band[info->band_nr];
        }
        else
        {
            offset = 0L;
        }
        if (coda_cursor_read_double_partial_array(&cursor, offset, info->total_num_pixels_all_bands, double_data) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        coda_cursor_goto_parent(&cursor);
        double_data += info->total_num_pixels_all_bands;
        if (i < (info->num_viadr_smr_records - 1))
        {
            if (coda_cursor_goto_next_array_element(&cursor) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
        }
    }
    return 0;
}

static int read_smr_datetime_start(void *user_data, harp_array data)
{
    return get_smr_datetime((ingest_info *)user_data, data.double_data, "START_UTC_SUN");
}

static int read_smr_datetime_end(void *user_data, harp_array data)
{
    return get_smr_datetime((ingest_info *)user_data, data.double_data, "END_UTC_SUN");
}

static int read_smr_irradiance(void *user_data, harp_array data)
{
    return get_smr_spectral_data((ingest_info *)user_data, "SMR", data.double_data);
}

static int read_smr_wavelength(void *user_data, harp_array data)
{
    return get_smr_spectral_data((ingest_info *)user_data, "LAMBDA_SMR", data.double_data);
}

static int init_sun_reference_dimensions(ingest_info *info)
{
    coda_cursor cursor;
    coda_type_class type_class;
    long offset;
    int band_nr;
    uint16_t num_pixels;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    /* Count the number of VIADR_SMR records */
    if (coda_cursor_goto_record_field_by_name(&cursor, "VIADR_SMR") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &info->num_viadr_smr_records) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    coda_cursor_goto_root(&cursor);
    if (coda_cursor_goto_record_field_by_name(&cursor, "GIADR_Bands") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    /* Depending on the version, GIADR_Bands can be an array */
    if (coda_cursor_get_type_class(&cursor, &type_class) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (type_class == coda_array_class)
    {
        if (coda_cursor_goto_first_array_element(&cursor) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
    }
    if (coda_cursor_goto_record_field_by_name(&cursor, "NUMBER_OF_PIXELS") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    offset = 0L;
    for (band_nr = 0; band_nr < MAX_NR_BANDS; band_nr++)
    {
        if (coda_cursor_goto_array_element_by_index(&cursor, band_nr) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_read_uint16(&cursor, &num_pixels) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        coda_cursor_goto_parent(&cursor);
        info->offset_of_band[band_nr] = offset;
        info->num_pixels[band_nr] = num_pixels;
        offset += info->num_pixels[band_nr];
    }
    if (info->band_nr >= 0)
    {
        info->total_num_pixels_all_bands = info->num_pixels[info->band_nr];
    }
    else
    {
        info->total_num_pixels_all_bands = MAX_PIXELS;
    }
    return 0;
}

void register_variables_reference_spectrum_fields(harp_product_definition *product_definition)
{
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2];
    const char *description;
    const char *path;

    dimension_type[0] = harp_dimension_time;
    dimension_type[1] = harp_dimension_spectral;

    /* start time */
    description = "start UTC date/time of Sun calibration mode measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime_start", harp_type_double, 1,
                                                   dimension_type, NULL, description, "seconds since 2000-01-01",
                                                   NULL, read_smr_datetime_start);
    path = "/VIADR_SMR[]/START_UTC_SUN";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* end time */
    description = "end UTC date/time of Sun calibration mode measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime_end", harp_type_double, 1,
                                                   dimension_type, NULL, description, "seconds since 2000-01-01",
                                                   NULL, read_smr_datetime_end);
    path = "/VIADR_SMR[]/START_UTC_SUN";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* wavelength_photon_irradiance */
    description = "solar mean reference spectrum";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "wavelength_photon_irradiance",
                                                   harp_type_double, 2, dimension_type, NULL, description,
                                                   "count/s/cm2/nm", NULL, read_smr_irradiance);
    path = "/VIADR_SMR[]/SMR[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* wavelength */
    description = "nominal wavelength assignment for each of the detector pixels";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "wavelength", harp_type_double, 2,
                                                   dimension_type, NULL, description, "nm", NULL, read_smr_wavelength);
    path = "/VIADR_SDR[]/LAMBDA_SMR[,]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

static int read_dimensions_reference_spectrum_fields(void *user_data, long dimension[HARP_NUM_DIM_TYPES])
{
    ingest_info *info = (ingest_info *)user_data;

    dimension[harp_dimension_time] = info->num_viadr_smr_records;
    dimension[harp_dimension_spectral] = info->total_num_pixels_all_bands;
    return 0;
}

/* Main code */

static int ingestion_init(const harp_ingestion_module *module, coda_product *product,
                          const harp_ingestion_options *options, harp_product_definition **definition, void **user_data)
{
    int format_version;
    ingest_info *info;
    const char *cp;

    if (coda_get_product_version(product, &format_version) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (format_version < 5)
    {
        harp_set_error(HARP_ERROR_INGESTION,
                       "This GOM_xxx_1B file is stored using a too old format and is not supported by HARP.");
        return -1;
    }

    CHECKED_MALLOC(info, sizeof(ingest_info));
    memset(info, '\0', sizeof(ingest_info));
    info->product = product;
    info->format_version = format_version;

    info->band_nr = -1;
    if (harp_ingestion_options_has_option(options, "band"))
    {
        if (harp_ingestion_options_get_option(options, "band", &cp) == 0)
        {
            info->band_nr = band_name_to_band_nr(cp);
        }
    }
    info->ingestion_data = DATA_RADIANCE;
    info->lightsource = strdup("Earthshine");
    if (harp_ingestion_options_has_option(options, "data"))
    {
        if (harp_ingestion_options_get_option(options, "data", &cp) == 0)
        {
            if (strcmp(cp, "radiance") == 0)
            {
                info->ingestion_data = DATA_RADIANCE;
                free(info->lightsource);
                info->lightsource = strdup("Earthshine");
            }
            else if (strcmp(cp, "transmission") == 0)
            {
                info->ingestion_data = DATA_TRANSMISSION;
                free(info->lightsource);
                info->lightsource = strdup("Earthshine");
            }
            else if (strcmp(cp, "sun") == 0)
            {
                info->ingestion_data = DATA_SUN;
                free(info->lightsource);
                info->lightsource = strdup("Sun");
            }
            else if (strcmp(cp, "moon") == 0)
            {
                info->ingestion_data = DATA_MOON;
                free(info->lightsource);
                info->lightsource = strdup("Moon");
            }
            else if (strcmp(cp, "sun_reference") == 0)
            {
                info->ingestion_data = DATA_SUN_REFERENCE;
                free(info->lightsource);
                info->lightsource = NULL;
            }
        }
    }

    if (info->ingestion_data == DATA_SUN_REFERENCE)
    {
        if (init_sun_reference_dimensions(info) != 0)
        {
            ingestion_done(info);
            return -1;
        }
        *definition = module->product_definition[1];
    }
    else
    {
        if (init_measurements_dimensions(info) != 0)
        {
            ingestion_done(info);
            return -1;
        }
        *definition = module->product_definition[0];
    }

    *user_data = info;

    return 0;
}

int harp_ingestion_module_gome2_l1_init(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition, *product_definition_sun_reference;
    const char *data_options[] = { "radiance", "transmission", "sun", "moon", "sun_reference" };
    const char *description;

    description = "GOME2 Level 1 data";
    module = harp_ingestion_register_module_coda("GOME2_L1", "GOME-2", "EPS", "GOME_xxx_1B", description,
                                                 ingestion_init, ingestion_done);
    harp_ingestion_register_option(module, "band", "only include data from the specified band ('band-1a', 'band-1b', "
                                   "'band-2a', 'band-2b', 'band-3', 'band-4'); by default data from all bands is "
                                   "retrieved", 6, band_name_as_option);
    harp_ingestion_register_option(module, "data", "retrieve the measured radiances, the transmission spectra, the sun "
                                   "measurement spectra, the moon measurement spectra or the sun reference spectrum; "
                                   "by default the measured radiances are retrieved", 5, data_options);

    description = "GOME2 Level 1b product";
    product_definition = harp_ingestion_register_product(module, "GOME2_L1", description,
                                                         read_dimensions_measurements_fields);
    description = "The GOME2 spectral data in the GOME2 L1b product is stored inside MDRs. There are separate MDRs for "
        "Earthshine, Calibration, Sun, and Moon measurements. In addition there are also 'Dummy Records' (DMDR) that "
        "can be present when there is lost data in the product. With HARP only Earthshine, Sun, and Moon "
        "measurements can be ingested.\n\n";
    harp_product_definition_add_mapping(product_definition, description, NULL);
    description = "Each MDR roughly contains a single scan. However, an MDR does not exactly correspond 1-to-1 with a "
        "GOME-2 scan. This is an important fact to be aware of. The real situation is as follows:\n\n";
    harp_product_definition_add_mapping(product_definition, description, NULL);
    description = "Within a single scan (a scan takes 6 seconds) there are 16 Instrument Source Packets "
        "(covering 375ms each) coming from the satellite. Each ISP contains at most two readouts (there are two if "
        "the integration time for a band is 187.5ms (or 93.75ms)). The problem is that the two readouts of the first "
        "ISP of a scan contain the last measurement of the previous scan and the first measurement of the new scan. "
        "The second ISP contains data for measurements #2 and #3, the third for #4 and #5, etc. The last measurement "
        "of a scan will again be found in the first ISP of the next scan. Instead of shifting the data and grouping "
        "all data of a single scan together in a single MDR the Level 1a and Level 1b processors just place the MDR "
        "boundary at the start of the first ISP of a scan and terminate the MDR at the end of ISP 16. This means that "
        "in Level 1b (but also 1a) products the first measurement in an MDR will always be the last measurement of "
        "the previous scan.\n\n";
    harp_product_definition_add_mapping(product_definition, description, NULL);
    description = "Nearly all meta-data for a readout (time, geolocation, viewing/solar angles, etc.) in an MDR are "
        "filled taking into account this same shift. This means that for retrieving the geolocation of the first "
        "readout of an MDR from the GEO_EARTH_ACTUAL record, one will in fact get the geolocation information of the "
        "last backscan pixel of the previous scan. However, the integration time meta-data for the first readout in "
        "an MDR is not shifted this way (the GEO_EARTH information is also not shifted, by the way, and just contains "
        "the 32 geolocation pixels for the scan). As long as the integration time does not change from one scan to "
        "another this won't impact anything, but the L1 products will contain invalid metadata for the first MDR "
        "readout if there is a change of integration time between two consecutive scans. In that case the calculated "
        "geolocation, angles, etc. of the first readout are values based on the integration time of the _new_ scan "
        "instead of the _old_ scan (e.g. the ground pixel will thus either be too large or too small). "
        "The (relative) good news to this is that, if a change in integration time occurs, the last pixel readout of "
        "the final scan with the 'old' integration time will never be valid and will have undefined values in the "
        "product (this is because the instrument prematurely terminates the final readout if a scan configuration "
        "change occurs). This means that the readout that has the 'invalid' meta-data will never be a valid "
        "measurement anyway.\n\n";
    harp_product_definition_add_mapping(product_definition, description, NULL);
    description = "Because of all this, HARP will exercise the following rules during ingestion:\n"
        "1) the first readout of the first MDR will always be ignored (and you will never see the last readout of the "
        "last scan, because it won't be in the product)\n"
        "2) the first readout after a change in measurement mode (i.e. earthshine vs. calibration vs. sun vs. moon) "
        "will be ignored\n"
        "3) if a change in integration time occurs (for any of the bands) then the first readout (for all bands) of "
        "the next MDR will be ignored\n"
        "4) if two MDRs are not continuous (i.e. there is a time gap) then the first readout of the second MDR will "
        "be ignored\n\n";
    harp_product_definition_add_mapping(product_definition, description, NULL);
    description = "GOME-2 uses 6 bands for the main spectra (1A, 1B, 2A, 2B, 3, and 4). Within a scan each band "
        "can have its own integration time. There will be at most 32 readouts per scan (corresponding with an "
        "integration time of 187.5ms). If the integration time is 375ms, 750ms, 1.5s, 3s or 6s there will be 16, 8, "
        "4, 2, or 1 measurement(s) respectively for this band in a scan. Some readouts may even cover multiple scans "
        "if the integration time is larger than 6s. HARP will combine the data for all bands into a single "
        "two-dimensional pixel_readout array. Because of the differences in integration time this means that for some "
        "bands there will be gaps in the pixel_readout array. These gaps will be filled with NaN values. HARP will "
        "always use the minimum integration time of all ingested bands to determine the time resolution for the HARP "
        "variables. For instance, if the minimum integration time for a scan is 1.5s you will find 4 entries in the "
        "HARP variables for this scan. All meta-data, such as geolocation, angles, etc. will also be ingested "
        "for this minimum integration time (i.e. you will see co-added meta-data if the integration time is > "
        "187.5ms). The minimum integration time is calculated based on those bands from which actual data is ingested. "
        "This means that the minimum integration time can change depending on the wavelength filter that was applied"
        "\n\n";
    harp_product_definition_add_mapping(product_definition, description, NULL);
    description = "The filtering on time and geolocation will always be performed using the 187.5ms resolution. "
        "A measurement with a higher integration time will only be included if each of its 187.5ms sub-parts have not "
        "been filtered out (this also holds for measurements with an integration time > 6s). If spectra from multiple "
        "bands with different integration times are ingested then the measurements with a high integration time will "
        "only be ingested if all subpixels of the measurements with the minimum integration time are also ingested. "
        "The measurement with a high integration time will be put in the same 'row' as the first corresponding minimum "
        "integration time measurement (i.e. measurements of different bands are aligned according to start time of the "
        "measurement).\n\n";
    harp_product_definition_add_mapping(product_definition, description, NULL);
    description = "If the band configuration changes somewhere during the orbit and a band filter is given, then "
        "we only include detector pixels that are inside the requested band for the duration of the whole orbit. i.e. "
        "detector pixels that change band during the orbit will always be excluded when a band filter is given.";
    harp_product_definition_add_mapping(product_definition, description, NULL);
    register_variables_measurement_fields(product_definition);

    description = "GOME2 Level 1b sun reference product";
    product_definition_sun_reference = harp_ingestion_register_product(module, "GOME2_L1_sun_reference", description,
                                                                       read_dimensions_reference_spectrum_fields);
    description = "GOME2 Level 1b sun reference data";
    harp_product_definition_add_mapping(product_definition_sun_reference, description, NULL);
    register_variables_reference_spectrum_fields(product_definition_sun_reference);

    return 0;
}
