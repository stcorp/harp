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
#include "harp-geometry.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PATH_LENGTH 256

typedef struct ingest_info_struct
{
    coda_product *product;
    int format_version;
    long num_time;
    long *coadding_factor;      /* number of geo pixels per mdsr for each mdsr (only for nadir data) */
    long *num_vertical; /* (only for profile data) */

    int *n_stvec;       /* state vector in partial columns, usually 54 */
    int *n_1;   /* number of fitted main gas species, usually 2 */
    int has_extended_diag;      /* does the add_diag field have number density and AKM information */

    long max_num_vertical;      /* (only for profile data) */
    double *profile_buffer;     /* [max_num_vertical] (only for profile data) */
    double *integration_time;   /* integration time for each mdsr */
    long *geo_dsr_id;   /* id of geo dsr for each mdsr */
    coda_cursor *mds_cursor;
    coda_cursor *geo_cursor;
    coda_cursor *clouds_aerosol_cursor;
    int state_type;
} ingest_info;

static int init_cursor(ingest_info *info, const char *dsname, coda_cursor **mds_cursor, long *num_elements)
{
    coda_cursor cursor;
    long field_index;
    int available;
    long i;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_record_field_index_from_name(&cursor, dsname, &field_index) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_record_field_available_status(&cursor, field_index, &available) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (!available)
    {
        /* no data */
        *num_elements = 0;
        return 0;
    }
    if (coda_cursor_goto_record_field_by_name(&cursor, dsname) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, num_elements) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (*num_elements == 0)
    {
        /* no data */
        return 0;
    }

    *mds_cursor = malloc((*num_elements) * sizeof(coda_cursor));
    if (*mds_cursor == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       (*num_elements) * sizeof(coda_cursor), __FILE__, __LINE__);
        return -1;
    }

    if (coda_cursor_goto_first_array_element(&cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    for (i = 0; i < *num_elements; i++)
    {
        (*mds_cursor)[i] = cursor;

        if (i < (*num_elements) - 1)
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

static int init_nadir_cursors(ingest_info *info, const char *dsname)
{
    long num_geo_elements;
    long num_clouds_aerosol_elements;
    long i;

    if (init_cursor(info, dsname, &info->mds_cursor, &info->num_time) != 0)
    {
        return -1;
    }
    if (info->num_time == 0)
    {
        return 0;
    }
    if (init_cursor(info, "geolocation_nadir", &info->geo_cursor, &num_geo_elements) != 0)
    {
        return -1;
    }
    if (init_cursor(info, "clouds_aerosol", &info->clouds_aerosol_cursor, &num_clouds_aerosol_elements) != 0)
    {
        return -1;
    }
    if (num_clouds_aerosol_elements != num_geo_elements)
    {
        harp_set_error(HARP_ERROR_INGESTION, "size of datasets 'geolocation_nadir' (%ld) and 'clouds_aerosol' (%ld) "
                       "do not match", num_clouds_aerosol_elements, num_geo_elements);
        return -1;
    }

    info->coadding_factor = malloc(info->num_time * sizeof(long));
    if (info->coadding_factor == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       info->num_time * sizeof(long), __FILE__, __LINE__);
        return -1;
    }
    info->integration_time = malloc(info->num_time * sizeof(double));
    if (info->integration_time == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       info->num_time * sizeof(double), __FILE__, __LINE__);
        return -1;
    }
    info->geo_dsr_id = malloc(info->num_time * sizeof(long));
    if (info->geo_dsr_id == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       info->num_time * sizeof(long), __FILE__, __LINE__);
        return -1;
    }

    info->geo_dsr_id[0] = 0;
    for (i = 0; i < info->num_time; i++)
    {
        coda_cursor mds_cursor;
        coda_cursor geo_cursor;
        double mds_integration_time;
        double geo_integration_time;
        double mds_time;
        double geo_time;

        mds_cursor = info->mds_cursor[i];
        if (coda_cursor_goto_record_field_by_name(&mds_cursor, "dsr_time") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_read_double(&mds_cursor, &mds_time) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        coda_cursor_goto_parent(&mds_cursor);
        if (coda_cursor_goto_record_field_by_name(&mds_cursor, "integr_time") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_read_double(&mds_cursor, &mds_integration_time) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        info->integration_time[i] = mds_integration_time;

        geo_time = -1;
        while (geo_time < mds_time && info->geo_dsr_id[i] < num_geo_elements)
        {
            geo_cursor = info->geo_cursor[info->geo_dsr_id[i]];
            if (coda_cursor_goto_record_field_by_name(&geo_cursor, "dsr_time") != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            if (coda_cursor_read_double(&geo_cursor, &geo_time) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            coda_cursor_goto_parent(&geo_cursor);
            if (geo_time < mds_time)
            {
                info->geo_dsr_id[i]++;
            }
        }
        if (geo_time > mds_time || info->geo_dsr_id[i] >= num_geo_elements)
        {
            harp_set_error(HARP_ERROR_INGESTION, "product error detected (no geolocation DSR "
                           "with same DSR time for measurement DSR %ld)", i);
            return -1;
        }

        if (coda_cursor_goto_record_field_by_name(&geo_cursor, "integr_time") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_read_double(&geo_cursor, &geo_integration_time) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        info->coadding_factor[i] = (int)(mds_integration_time / geo_integration_time);

        if (i < info->num_time - 1)
        {
            info->geo_dsr_id[i + 1] = info->geo_dsr_id[i] + 1;
        }
    }

    return 0;
}

static int init_limb_cursors(ingest_info *info, const char *dsname)
{
    long num_geo_elements;
    long i;

    if (init_cursor(info, dsname, &info->mds_cursor, &info->num_time) != 0)
    {
        return -1;
    }
    if (info->num_time == 0)
    {
        return 0;
    }
    if (init_cursor(info, "geolocation_limb", &info->geo_cursor, &num_geo_elements) != 0)
    {
        return -1;
    }

    info->num_vertical = malloc(info->num_time * sizeof(long));
    if (info->num_vertical == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       info->num_time * sizeof(long), __FILE__, __LINE__);
        return -1;
    }
    info->n_stvec = malloc(info->num_time * sizeof(int));
    if (info->n_stvec == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       info->num_time * sizeof(int), __FILE__, __LINE__);
        return -1;
    }
    info->n_1 = malloc(info->num_time * sizeof(int));
    if (info->n_1 == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       info->num_time * sizeof(int), __FILE__, __LINE__);
        return -1;
    }


    info->integration_time = malloc(info->num_time * sizeof(double));
    if (info->integration_time == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       info->num_time * sizeof(double), __FILE__, __LINE__);
        return -1;
    }
    info->geo_dsr_id = malloc(info->num_time * sizeof(long));
    if (info->geo_dsr_id == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       info->num_time * sizeof(long), __FILE__, __LINE__);
        return -1;
    }

    info->geo_dsr_id[0] = 0;
    for (i = 0; i < info->num_time; i++)
    {
        coda_cursor mds_cursor;
        coda_cursor geo_cursor;
        double mds_integration_time;
        double mds_time;
        double geo_time;
        uint8_t n_meas;
        uint8_t n_main;
        uint16_t nstvec;
        uint16_t n1;

        mds_cursor = info->mds_cursor[i];
        if (coda_cursor_goto_record_field_by_name(&mds_cursor, "n_main") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_read_uint8(&mds_cursor, &n_main) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        coda_cursor_goto_parent(&mds_cursor);
        if (coda_cursor_goto_record_field_by_name(&mds_cursor, "n_state_vec") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_read_uint16(&mds_cursor, &nstvec) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        coda_cursor_goto_parent(&mds_cursor);
        if (coda_cursor_goto_record_field_by_name(&mds_cursor, "n1") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_read_uint16(&mds_cursor, &n1) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        coda_cursor_goto_parent(&mds_cursor);
        if (coda_cursor_goto_record_field_by_name(&mds_cursor, "n_meas") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_read_uint8(&mds_cursor, &n_meas) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        coda_cursor_goto_parent(&mds_cursor);

        info->num_vertical[i] = n_main;
        if (n_main > info->max_num_vertical)
        {
            info->max_num_vertical = n_main;
        }
        info->n_stvec[i] = nstvec;
        info->n_1[i] = n1;

        if (coda_cursor_goto_record_field_by_name(&mds_cursor, "measurement_grid") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_goto_array_element_by_index(&mds_cursor, n_meas / 2) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_goto_record_field_by_name(&mds_cursor, "dsr_time") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_read_double(&mds_cursor, &mds_time) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        coda_cursor_goto_parent(&mds_cursor);
        coda_cursor_goto_parent(&mds_cursor);
        coda_cursor_goto_parent(&mds_cursor);
        if (coda_cursor_goto_record_field_by_name(&mds_cursor, "integr_time") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_read_double(&mds_cursor, &mds_integration_time) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        info->integration_time[i] = mds_integration_time;

        geo_cursor = info->geo_cursor[info->geo_dsr_id[i]];
        if (coda_cursor_goto_record_field_by_name(&geo_cursor, "dsr_time") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_read_double(&geo_cursor, &geo_time) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        coda_cursor_goto_parent(&geo_cursor);
        while (geo_time > mds_time && info->geo_dsr_id[i] > 0)
        {
            info->geo_dsr_id[i]--;
            geo_cursor = info->geo_cursor[info->geo_dsr_id[i]];
            if (coda_cursor_goto_record_field_by_name(&geo_cursor, "dsr_time") != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            if (coda_cursor_read_double(&geo_cursor, &geo_time) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            coda_cursor_goto_parent(&geo_cursor);
        }
        while (geo_time < mds_time && info->geo_dsr_id[i] < num_geo_elements)
        {
            info->geo_dsr_id[i]++;
            geo_cursor = info->geo_cursor[info->geo_dsr_id[i]];
            if (coda_cursor_goto_record_field_by_name(&geo_cursor, "dsr_time") != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            if (coda_cursor_read_double(&geo_cursor, &geo_time) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            coda_cursor_goto_parent(&geo_cursor);
        }
        if (geo_time > mds_time || info->geo_dsr_id[i] >= num_geo_elements)
        {
            harp_set_error(HARP_ERROR_INGESTION, "product error detected (no geolocation DSR "
                           "with same DSR time for measurement DSR %ld)", i);
            return -1;
        }

        if (i < info->num_time - 1)
        {
            info->geo_dsr_id[i + 1] = info->geo_dsr_id[i] + 1;
        }
    }

    info->profile_buffer = malloc(info->max_num_vertical * sizeof(double));
    if (info->profile_buffer == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       info->max_num_vertical * sizeof(double), __FILE__, __LINE__);
        return -1;
    }

    return 0;
}

static int init_has_extended_diag(ingest_info *info)
{
    long add_diag_length;
    int stvec, nmain, n1;
    coda_cursor cursor;

    cursor = info->mds_cursor[0];
    if (coda_cursor_goto_record_field_by_name(&cursor, "add_diag") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &add_diag_length) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    stvec = info->n_stvec[0];
    nmain = info->num_vertical[0];
    n1 = info->n_1[0];
    if (add_diag_length >= 2 + stvec + 2 * nmain * n1 + 2 * nmain + n1 * nmain * nmain)
    {
        /* the add_diag field is long enough, so assume it contains the number densities and AKM */
        info->has_extended_diag = 1;
    }

    return 0;
}

static int read_dimensions(void *user_data, long dimension[HARP_NUM_DIM_TYPES])
{
    dimension[harp_dimension_time] = ((ingest_info *)user_data)->num_time;
    dimension[harp_dimension_vertical] = ((ingest_info *)user_data)->max_num_vertical;
    return 0;
}

static int get_data(void *user_data, long index, const char *fieldname, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    coda_cursor cursor;

    cursor = info->mds_cursor[index];
    if (coda_cursor_goto_record_field_by_name(&cursor, fieldname) != 0)
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

static int get_vcd_data(void *user_data, long index, const char *fieldname, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    coda_cursor cursor;

    cursor = info->mds_cursor[index];
    if (coda_cursor_goto_record_field_by_name(&cursor, fieldname) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_array_element_by_index(&cursor, 0) != 0)
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

static int get_profile_data(void *user_data, long index, const char *fieldname, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    coda_cursor cursor;
    int i;

    cursor = info->mds_cursor[index];
    if (coda_cursor_goto_record_field_by_name(&cursor, fieldname) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_double_array(&cursor, data.double_data, coda_array_ordering_c) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    /* store in reverse order (from bottom of atmosphere to top of atmosphere) */
    if (harp_array_invert(harp_type_double, 0, 1, &info->num_vertical[index], data) != 0)
    {
        return -1;
    }
    for (i = info->num_vertical[index]; i < info->max_num_vertical; i++)
    {
        data.double_data[i] = coda_NaN();
    }

    return 0;
}

static int get_profile_vmr_data(void *user_data, long index, const char *fieldname, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    coda_cursor cursor;
    long dim_index[2];
    int i;

    cursor = info->mds_cursor[index];
    if (coda_cursor_goto_record_field_by_name(&cursor, "main_species") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    for (i = 0; i < info->num_vertical[index]; i++)
    {
        dim_index[0] = i;
        dim_index[1] = 0;
        if (coda_cursor_goto_array_element(&cursor, 2, dim_index) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_goto_record_field_by_name(&cursor, fieldname) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        /* store in reverse order (from bottom of atmosphere to top of atmosphere) */
        if (coda_cursor_read_double(&cursor, &data.double_data[info->num_vertical[index] - 1 - i]) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        coda_cursor_goto_parent(&cursor);
        coda_cursor_goto_parent(&cursor);
    }
    for (i = info->num_vertical[index]; i < info->max_num_vertical; i++)
    {
        data.double_data[i] = coda_NaN();
    }

    return 0;
}

static int get_avk_from_add_diag(void *user_data, long index, harp_array data, int convert_to_nd)
{
    coda_cursor cursor;
    double *add_diag;
    long i, j, reversed_i, reversed_j, num_elements, conv_position;
    ingest_info *info = (ingest_info *)user_data;
    int avk_position = 2 + info->n_stvec[index] + 2 * info->num_vertical[index] * info->n_1[index] +
        2 * info->num_vertical[index];

    /* The AVK is given in the add_diag field in partial columns (AVK_pc).
     * To convert these into number density units (scaling), a transformation is done:
     *   AVK_nd = conv_nd_i/conv_nd_j * AVK_pc
     * or
     *   AVK_mix = conv_mix_i/conv_mix_j * AVK_pc
     *
     * where conv_nd_i are found in the add_diag field at position 2+stvec+2*n1*num_vertical+num_vertical and
     * conv_mix_i are found in the add_diag field at position 2+stvec+2*n1*num_vertical
     */

    if (convert_to_nd)
    {
        /* position of number density conversion factors */
        conv_position = 2 + info->n_stvec[index] + 2 * info->num_vertical[index] * info->n_1[index] +
            info->num_vertical[index];
    }
    else
    {
        /* position of vmr conversion factors */
        conv_position = 2 + info->n_stvec[index] + 2 * info->num_vertical[index] * info->n_1[index];
    }

    assert(info->has_extended_diag == 1);
    cursor = info->mds_cursor[index];

    if (coda_cursor_goto_record_field_by_name(&cursor, "add_diag") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    if (coda_cursor_get_num_elements(&cursor, &num_elements) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    if (num_elements < 2 + info->n_stvec[index] + 2 * info->num_vertical[index] * info->n_1[index] +
        2 * info->num_vertical[index] + info->n_1[index] * info->num_vertical[index] * info->num_vertical[index])
    {
        harp_set_error(HARP_ERROR_INGESTION, "size of add_diag array (%ld) is too small", num_elements);
        return -1;
    }
    add_diag = malloc(num_elements * sizeof(double));
    if (add_diag == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_elements * sizeof(double), __FILE__, __LINE__);
        return -1;
    }

    if (coda_cursor_read_double_array(&cursor, add_diag, coda_array_ordering_c) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        free(add_diag);
        return -1;
    }

    /* read AKM and store in reversed order */
    for (i = 0, reversed_i = info->max_num_vertical - 1; i < info->num_vertical[index]; i++, reversed_i--)
    {
        for (j = 0, reversed_j = info->max_num_vertical - 1; j < info->num_vertical[index]; j++, reversed_j--)
        {
            data.double_data[reversed_i * info->max_num_vertical + reversed_j] = add_diag[conv_position + i] /
                add_diag[conv_position + j] * add_diag[avk_position + i * info->num_vertical[index] + j];
        }
        for (j = info->num_vertical[index], reversed_j = info->max_num_vertical - info->num_vertical[index] - 1;
             j < info->max_num_vertical; j++, reversed_j--)
        {
            assert(reversed_j >= 0);
            data.double_data[reversed_i * info->max_num_vertical + reversed_j] = coda_NaN();
        }
    }
    free(add_diag);

    /* fill remaining missing values, */
    for (i = info->num_vertical[index], reversed_i = info->max_num_vertical - info->num_vertical[index] - 1;
         i < info->max_num_vertical; i++, reversed_i--)
    {
        assert(reversed_i >= 0);
        for (j = 0, reversed_j = info->max_num_vertical - 1; j < info->max_num_vertical; j++, reversed_j--)
        {
            assert(reversed_j >= 0);
            data.double_data[reversed_i * info->max_num_vertical + reversed_j] = coda_NaN();
        }
    }

    return 0;
}

static int get_latitude_sub(coda_cursor *cursor, double *latitude)
{
    if (coda_cursor_goto_first_record_field(cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_double(cursor, latitude) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    coda_cursor_goto_parent(cursor);
    return 0;
}

static int get_longitude_sub(coda_cursor *cursor, double *longitude)
{
    if (coda_cursor_goto_record_field_by_index(cursor, 1) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_double(cursor, longitude) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    coda_cursor_goto_parent(cursor);
    return 0;
}

static int get_latitude_and_longitude(coda_cursor *cursor, double *latitude, double *longitude)
{
    if (coda_cursor_goto_first_record_field(cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_double(cursor, latitude) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_next_record_field(cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_double(cursor, longitude) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    coda_cursor_goto_parent(cursor);
    return 0;
}

static int get_latitude_from_array(coda_cursor *cursor, long index, double *latitude)
{
    if (coda_cursor_goto_array_element_by_index(cursor, index) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (get_latitude_sub(cursor, latitude) != 0)
    {
        return -1;
    }
    coda_cursor_goto_parent(cursor);
    return 0;
}

static int get_longitude_from_array(coda_cursor *cursor, long index, double *longitude)
{
    if (coda_cursor_goto_array_element_by_index(cursor, index) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (get_longitude_sub(cursor, longitude) != 0)
    {
        return -1;
    }
    coda_cursor_goto_parent(cursor);
    return 0;
}

static int get_latitude_and_longitude_from_array(coda_cursor *cursor, long index, double *latitude, double *longitude)
{
    if (coda_cursor_goto_array_element_by_index(cursor, index) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (get_latitude_and_longitude(cursor, latitude, longitude) != 0)
    {
        return -1;
    }
    coda_cursor_goto_parent(cursor);
    return 0;
}

static int get_angle_data(void *user_data, long index, const char *field_name, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    coda_cursor cursor;
    int coadding_factor;

    coadding_factor = info->coadding_factor[index];

    if (coadding_factor == 1)
    {
        /* no co-adding of geolocation pixels needed */

        cursor = info->geo_cursor[info->geo_dsr_id[index]];
        if (coda_cursor_goto_record_field_by_name(&cursor, field_name) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        /* use middle one */
        if (coda_cursor_goto_array_element_by_index(&cursor, 1) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_read_double(&cursor, data.double_data) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
    }
    else if (info->integration_time[index] <= 1.0)
    {
        /* co-add geolocation pixels to calculate pixel coordinates for this measurement */

        /* use end position of N/2-th geolocation pixel */
        cursor = info->geo_cursor[info->geo_dsr_id[index] + coadding_factor / 2 - 1];
        if (coda_cursor_goto_record_field_by_name(&cursor, field_name) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        /* use last one */
        if (coda_cursor_goto_array_element_by_index(&cursor, 2) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_read_double(&cursor, data.double_data) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
    }
    else
    {
        double value;

        /* co-add the nadir high integration time pixel containing both forward and backward scans */

        /* read first center components from second pixel */
        cursor = info->geo_cursor[info->geo_dsr_id[index] + 2 - 1];
        if (coda_cursor_goto_record_field_by_name(&cursor, field_name) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        /* use last one */
        if (coda_cursor_goto_array_element_by_index(&cursor, 2) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_read_double(&cursor, data.double_data) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        /* goto last pixel of the last backward scan (pixel_id = coadding_factor - 1) */
        cursor = info->geo_cursor[info->geo_dsr_id[index] + coadding_factor - 1];
        if (coda_cursor_goto_record_field_by_name(&cursor, field_name) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        /* use middle one */
        if (coda_cursor_goto_array_element_by_index(&cursor, 1) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_read_double(&cursor, &value) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        /* take average */
        *data.double_data = (*data.double_data + value) / 2;
    }

    return 0;
}

static int get_geo_profile_data(void *user_data, long index, const char *field_name, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    coda_cursor cursor;

    cursor = info->geo_cursor[info->geo_dsr_id[index]];
    if (coda_cursor_goto_record_field_by_name(&cursor, field_name) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    /* use middle one */
    if (coda_cursor_goto_array_element_by_index(&cursor, 1) != 0)
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

static int read_datetime(void *user_data, long index, harp_array data)
{
    return get_data(user_data, index, "dsr_time", data);
}

static int read_datetime_profile(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    coda_cursor cursor;

    cursor = info->geo_cursor[info->geo_dsr_id[index]];
    if (coda_cursor_goto_record_field_by_name(&cursor, "dsr_time") != 0)
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

static int read_integration_time(void *user_data, long index, harp_array data)
{
    return get_data(user_data, index, "integr_time", data);
}

static int read_orbit_index(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    coda_cursor cursor;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, "/mph/abs_orbit") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_int32(&cursor, data.int32_data) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    return 0;
}

static int read_latitude(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    coda_cursor cursor;
    double latitude[2];
    double longitude[2];
    int coadding_factor;

    coadding_factor = info->coadding_factor[index];
    if (coadding_factor == 1)
    {
        /* no co-adding of geolocation pixels needed */
        cursor = info->geo_cursor[info->geo_dsr_id[index]];
        if (coda_cursor_goto_record_field_by_name(&cursor, "cen_coor_nad") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (get_latitude_sub(&cursor, latitude) != 0)
        {
            return -1;
        }
    }
    else if (info->integration_time[index] <= 1.0)
    {
        /* use end position of N/2-th geolocation pixel */
        cursor = info->geo_cursor[info->geo_dsr_id[index] + coadding_factor / 2 - 1];
        if (coda_cursor_goto_record_field_by_name(&cursor, "cor_coor_nad") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (get_latitude_and_longitude_from_array(&cursor, 2, latitude, longitude) != 0)
        {
            return -1;
        }
        if (get_latitude_and_longitude_from_array(&cursor, 3, &latitude[1], &longitude[1]) != 0)
        {
            return -1;
        }
        harp_geographic_average(latitude[0], longitude[0], latitude[1], longitude[1], latitude, longitude);
    }
    else
    {
        /* co-add the nadir high integration time pixel containing both forward and backward scans */

        /* determine first coordinate from end of second geolocation pixel */
        cursor = info->geo_cursor[info->geo_dsr_id[index] + 2 - 1];
        if (coda_cursor_goto_record_field_by_name(&cursor, "cor_coor_nad") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (get_latitude_and_longitude_from_array(&cursor, 2, latitude, longitude) != 0)
        {
            return -1;
        }
        if (get_latitude_and_longitude_from_array(&cursor, 3, &latitude[1], &longitude[1]) != 0)
        {
            return -1;
        }
        harp_geographic_average(latitude[0], longitude[0], latitude[1], longitude[1], latitude, longitude);

        /* read second coordinate from last pixel */
        cursor = info->geo_cursor[info->geo_dsr_id[index] + coadding_factor - 1];
        if (coda_cursor_goto_record_field_by_name(&cursor, "cen_coor_nad") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (get_latitude_and_longitude(&cursor, &latitude[1], &longitude[1]) != 0)
        {
            return -1;
        }
        harp_geographic_average(latitude[0], longitude[0], latitude[1], longitude[1], latitude, longitude);
    }

    *data.double_data = latitude[0];

    return 0;
}

static int read_longitude(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    coda_cursor cursor;
    double latitude[2];
    double longitude[2];
    int coadding_factor;

    coadding_factor = info->coadding_factor[index];
    if (coadding_factor == 1)
    {
        /* no co-adding of geolocation pixels needed */
        cursor = info->geo_cursor[info->geo_dsr_id[index]];
        if (coda_cursor_goto_record_field_by_name(&cursor, "cen_coor_nad") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (get_longitude_sub(&cursor, longitude) != 0)
        {
            return -1;
        }
    }
    else if (info->integration_time[index] <= 1.0)
    {
        /* use end position of N/2-th geolocation pixel */
        cursor = info->geo_cursor[info->geo_dsr_id[index] + coadding_factor / 2 - 1];
        if (coda_cursor_goto_record_field_by_name(&cursor, "cor_coor_nad") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (get_latitude_and_longitude_from_array(&cursor, 2, latitude, longitude) != 0)
        {
            return -1;
        }
        if (get_latitude_and_longitude_from_array(&cursor, 3, &latitude[1], &longitude[1]) != 0)
        {
            return -1;
        }
        harp_geographic_average(latitude[0], longitude[0], latitude[1], longitude[1], latitude, longitude);
    }
    else
    {
        /* co-add the nadir high integration time pixel containing both forward and backward scans */

        /* determine first coordinate from end of second geolocation pixel */
        cursor = info->geo_cursor[info->geo_dsr_id[index] + 2 - 1];
        if (coda_cursor_goto_record_field_by_name(&cursor, "cor_coor_nad") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (get_latitude_and_longitude_from_array(&cursor, 2, latitude, longitude) != 0)
        {
            return -1;
        }
        if (get_latitude_and_longitude_from_array(&cursor, 3, &latitude[1], &longitude[1]) != 0)
        {
            return -1;
        }
        harp_geographic_average(latitude[0], longitude[0], latitude[1], longitude[1], latitude, longitude);

        /* read second coordinate from last pixel */
        cursor = info->geo_cursor[info->geo_dsr_id[index] + coadding_factor - 1];
        if (coda_cursor_goto_record_field_by_name(&cursor, "cen_coor_nad") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (get_latitude_and_longitude(&cursor, &latitude[1], &longitude[1]) != 0)
        {
            return -1;
        }
        harp_geographic_average(latitude[0], longitude[0], latitude[1], longitude[1], latitude, longitude);
    }

    *data.double_data = longitude[0];

    return 0;
}

static int read_latitude_profile(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    coda_cursor cursor;

    cursor = info->geo_cursor[info->geo_dsr_id[index]];
    if (coda_cursor_goto_record_field_by_name(&cursor, "tangent_coord") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    /* use middle one */
    if (get_latitude_from_array(&cursor, 1, data.double_data) != 0)
    {
        return -1;
    }

    return 0;
}

static int read_longitude_profile(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    coda_cursor cursor;

    cursor = info->geo_cursor[info->geo_dsr_id[index]];
    if (coda_cursor_goto_record_field_by_name(&cursor, "tangent_coord") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    /* use middle one */
    if (get_longitude_from_array(&cursor, 1, data.double_data) != 0)
    {
        return -1;
    }

    return 0;
}

static int read_latitude_bounds(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    coda_cursor cursor;
    int coadding_factor;

    coadding_factor = info->coadding_factor[index];
    if (coadding_factor == 1)
    {
        /* no co-adding of geolocation pixels needed */

        cursor = info->geo_cursor[info->geo_dsr_id[index]];
        if (coda_cursor_goto_record_field_by_name(&cursor, "cor_coor_nad") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (get_latitude_from_array(&cursor, 0, &data.double_data[0]) != 0)
        {
            return -1;
        }
        if (get_latitude_from_array(&cursor, 1, &data.double_data[3]) != 0)
        {
            return -1;
        }
        if (get_latitude_from_array(&cursor, 2, &data.double_data[1]) != 0)
        {
            return -1;
        }
        if (get_latitude_from_array(&cursor, 3, &data.double_data[2]) != 0)
        {
            return -1;
        }
    }
    else if (info->integration_time[index] <= 1.0)
    {
        /* co-add geolocation pixels to calculate pixel coordinates for this measurement */

        /* read first geolocation pixel */
        cursor = info->geo_cursor[info->geo_dsr_id[index]];
        if (coda_cursor_goto_record_field_by_name(&cursor, "cor_coor_nad") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (get_latitude_from_array(&cursor, 0, &data.double_data[0]) != 0)
        {
            return -1;
        }
        if (get_latitude_from_array(&cursor, 1, &data.double_data[3]) != 0)
        {
            return -1;
        }

        /* read N-th geolocation pixel */
        cursor = info->geo_cursor[info->geo_dsr_id[index] + coadding_factor - 1];
        if (coda_cursor_goto_record_field_by_name(&cursor, "cor_coor_nad") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (get_latitude_from_array(&cursor, 2, &data.double_data[1]) != 0)
        {
            return -1;
        }
        if (get_latitude_from_array(&cursor, 3, &data.double_data[2]) != 0)
        {
            return -1;
        }
    }
    else
    {
        /* co-add the nadir high integration time pixel containing both forward and backward scans */

        /* read first corner coordinates from first geolocation pixel */
        cursor = info->geo_cursor[info->geo_dsr_id[index]];
        if (coda_cursor_goto_record_field_by_name(&cursor, "cor_coor_nad") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (get_latitude_from_array(&cursor, 0, &data.double_data[0]) != 0)
        {
            return -1;
        }

        /* read second corner coordinates from fourth pixel */
        cursor = info->geo_cursor[info->geo_dsr_id[index] + 4 - 1];
        if (coda_cursor_goto_record_field_by_name(&cursor, "cor_coor_nad") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (get_latitude_from_array(&cursor, 2, &data.double_data[1]) != 0)
        {
            return -1;
        }

        /* goto last pixel */
        cursor = info->geo_cursor[info->geo_dsr_id[index] + coadding_factor - 1];
        if (coda_cursor_goto_record_field_by_name(&cursor, "cor_coor_nad") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (get_latitude_from_array(&cursor, 1, &data.double_data[2]) != 0)
        {
            return -1;
        }
        if (get_latitude_from_array(&cursor, 3, &data.double_data[3]) != 0)
        {
            return -1;
        }
    }

    return 0;
}

static int read_longitude_bounds(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    coda_cursor cursor;
    int coadding_factor;

    coadding_factor = info->coadding_factor[index];
    if (coadding_factor == 1)
    {
        /* no co-adding of geolocation pixels needed */

        cursor = info->geo_cursor[info->geo_dsr_id[index]];
        if (coda_cursor_goto_record_field_by_name(&cursor, "cor_coor_nad") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (get_longitude_from_array(&cursor, 0, &data.double_data[0]) != 0)
        {
            return -1;
        }
        if (get_longitude_from_array(&cursor, 1, &data.double_data[3]) != 0)
        {
            return -1;
        }
        if (get_longitude_from_array(&cursor, 2, &data.double_data[1]) != 0)
        {
            return -1;
        }
        if (get_longitude_from_array(&cursor, 3, &data.double_data[2]) != 0)
        {
            return -1;
        }
    }
    else if (info->integration_time[index] <= 1.0)
    {
        /* co-add geolocation pixels to calculate pixel coordinates for this measurement */

        /* read first geolocation pixel */
        cursor = info->geo_cursor[info->geo_dsr_id[index]];
        if (coda_cursor_goto_record_field_by_name(&cursor, "cor_coor_nad") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (get_longitude_from_array(&cursor, 0, &data.double_data[0]) != 0)
        {
            return -1;
        }
        if (get_longitude_from_array(&cursor, 1, &data.double_data[3]) != 0)
        {
            return -1;
        }

        /* read N-th geolocation pixel */
        cursor = info->geo_cursor[info->geo_dsr_id[index] + coadding_factor - 1];
        if (coda_cursor_goto_record_field_by_name(&cursor, "cor_coor_nad") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (get_longitude_from_array(&cursor, 2, &data.double_data[1]) != 0)
        {
            return -1;
        }
        if (get_longitude_from_array(&cursor, 3, &data.double_data[2]) != 0)
        {
            return -1;
        }
    }
    else
    {
        /* co-add the nadir high integration time pixel containing both forward and backward scans */

        /* read first corner coordinates from first geolocation pixel */
        cursor = info->geo_cursor[info->geo_dsr_id[index]];
        if (coda_cursor_goto_record_field_by_name(&cursor, "cor_coor_nad") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (get_longitude_from_array(&cursor, 0, &data.double_data[0]) != 0)
        {
            return -1;
        }

        /* read second corner coordinates from fourth pixel */
        cursor = info->geo_cursor[info->geo_dsr_id[index] + 4 - 1];
        if (coda_cursor_goto_record_field_by_name(&cursor, "cor_coor_nad") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (get_longitude_from_array(&cursor, 2, &data.double_data[1]) != 0)
        {
            return -1;
        }

        /* goto last pixel */
        cursor = info->geo_cursor[info->geo_dsr_id[index] + coadding_factor - 1];
        if (coda_cursor_goto_record_field_by_name(&cursor, "cor_coor_nad") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (get_longitude_from_array(&cursor, 1, &data.double_data[2]) != 0)
        {
            return -1;
        }
        if (get_longitude_from_array(&cursor, 3, &data.double_data[3]) != 0)
        {
            return -1;
        }
    }

    return 0;
}

static int read_altitude_bounds(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    coda_cursor cursor;
    int i;

    cursor = info->mds_cursor[index];
    if (coda_cursor_goto_record_field_by_name(&cursor, "tangent_height") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_double_array(&cursor, data.double_data, coda_array_ordering_c) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    /* store in reverse order (from bottom of atmosphere to top of atmosphere) */
    if (harp_array_invert(harp_type_double, 0, 1, &info->num_vertical[index], data) != 0)
    {
        return -1;
    }
    /* turn lower layer altitudes into bounds */
    for (i = info->num_vertical[index] - 1; i >= 0; i--)
    {
        data.double_data[2 * i] = data.double_data[i];
        if (i == info->num_vertical[index] - 1)
        {
            /* use top-of-atmosphere altitude of 100km for upper boundary */
            data.double_data[2 * i + 1] = 100;
        }
        else
        {
            data.double_data[2 * i + 1] = data.double_data[i + 1];
        }
    }
    /* set remaining values to NaN */
    for (i = info->num_vertical[index]; i < info->max_num_vertical; i++)
    {
        data.double_data[2 * i] = coda_NaN();
        data.double_data[2 * i + 1] = coda_NaN();
    }

    return 0;
}

static int read_pressure_bounds(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    coda_cursor cursor;
    int i;

    cursor = info->mds_cursor[index];
    if (coda_cursor_goto_record_field_by_name(&cursor, "tangent_pressure") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_double_array(&cursor, data.double_data, coda_array_ordering_c) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    /* store in reverse order (from bottom of atmosphere to top of atmosphere) */
    if (harp_array_invert(harp_type_double, 0, 1, &info->num_vertical[index], data) != 0)
    {
        return -1;
    }
    /* turn lower layer pressures into bounds */
    for (i = info->num_vertical[index] - 1; i >= 0; i--)
    {
        data.double_data[2 * i] = data.double_data[i];
        if (i == info->num_vertical[index] - 1)
        {
            /* use top-of-atmosphere pressure of 3.2e-4 hPa for upper boundary */
            data.double_data[2 * i + 1] = 3.2E-4;
        }
        else
        {
            data.double_data[2 * i + 1] = data.double_data[i + 1];
        }
    }
    /* set remaining values to NaN */
    for (i = info->num_vertical[index]; i < info->max_num_vertical; i++)
    {
        data.double_data[2 * i] = coda_NaN();
        data.double_data[2 * i + 1] = coda_NaN();
    }

    return 0;
}

static int read_solar_zenith_angle(void *user_data, long index, harp_array data)
{
    return get_angle_data(user_data, index, "sol_zen_angle_toa", data);
}

static int read_solar_zenith_angle_profile(void *user_data, long index, harp_array data)
{
    return get_geo_profile_data(user_data, index, "sol_zen_angle_toa", data);
}

static int read_los_zenith_angle(void *user_data, long index, harp_array data)
{
    return get_angle_data(user_data, index, "los_zen_angle_toa", data);
}

static int read_los_zenith_angle_profile(void *user_data, long index, harp_array data)
{
    return get_geo_profile_data(user_data, index, "los_zen_angle_toa", data);
}

static int read_rel_azimuth_angle(void *user_data, long index, harp_array data)
{
    return get_angle_data(user_data, index, "rel_azi_angle_toa", data);
}

static int read_rel_azimuth_angle_profile(void *user_data, long index, harp_array data)
{
    return get_geo_profile_data(user_data, index, "rel_azi_angle_toa", data);
}

static int read_cloud_fraction(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    coda_cursor cursor;

    cursor = info->clouds_aerosol_cursor[info->geo_dsr_id[index]];
    if (coda_cursor_goto_record_field_by_name(&cursor, "cl_frac") != 0)
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

static int read_cloud_top_pressure(void *user_data, long index, harp_array data)
{
    return get_data(user_data, index, "cl_top_pres", data);
}

static int read_cloud_top_height(void *user_data, long index, harp_array data)
{
    return get_data(user_data, index, "cl_top_height", data);
}

static int read_absorbing_aerosol_index(void *user_data, long index, harp_array data)
{
    return get_data(user_data, index, "aero_abso_ind", data);
}

static int read_scan_direction_type(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->integration_time[index] > 1)
    {
        *data.int8_data = 2;
    }
    else
    {
        coda_cursor cursor;
        double latitude[4];
        double longitude[4];
        double px, py, qx, qy, rx, ry, z;

        cursor = info->geo_cursor[info->geo_dsr_id[index]];
        if (coda_cursor_goto_record_field_by_name(&cursor, "cor_coor_nad") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (get_latitude_and_longitude_from_array(&cursor, 0, &latitude[0], &longitude[0]) != 0)
        {
            return -1;
        }
        if (get_latitude_and_longitude_from_array(&cursor, 1, &latitude[1], &longitude[1]) != 0)
        {
            return -1;
        }
        if (get_latitude_and_longitude_from_array(&cursor, 2, &latitude[2], &longitude[2]) != 0)
        {
            return -1;
        }

        px = longitude[0] * CONST_DEG2RAD;
        py = latitude[0] * CONST_DEG2RAD;
        qx = longitude[1] * CONST_DEG2RAD;
        qy = latitude[1] * CONST_DEG2RAD;
        rx = longitude[2] * CONST_DEG2RAD;
        ry = latitude[2] * CONST_DEG2RAD;

        /* z = inprod(r, outprod(p, q)) */
        z = cos(qy) * (cos(ry) * sin(py) * sin(qx - rx) + cos(py) * sin(px - qx) * sin(ry)) -
            cos(py) * cos(ry) * sin(qy) * sin(px - rx);

        if (z < 0.0)
        {
            *data.int8_data = 1;
        }
        else
        {
            *data.int8_data = 0;
        }
    }

    return 0;
}

static int read_temperature(void *user_data, long index, harp_array data)
{
    return get_profile_data(user_data, index, "tangent_temp", data);
}

static int read_vcd(void *user_data, long index, harp_array data)
{
    return get_vcd_data(user_data, index, "vcd", data);
}

static int read_vcd_error(void *user_data, long index, harp_array data)
{
    harp_array vcd_data;
    double vcd_value;

    if (get_vcd_data(user_data, index, "vcd_err", data) != 0)
    {
        return -1;
    }
    if (((ingest_info *)user_data)->format_version < 2)
    {
        /* convert '%' to relative fraction */
        data.double_data[0] /= 100.0;
    }

    /* convert relative error to absolute error */
    vcd_data.double_data = &vcd_value;
    if (get_vcd_data(user_data, index, "vcd", vcd_data) != 0)
    {
        return -1;
    }
    data.double_data[0] *= vcd_value;

    return 0;
}

static int read_vcd_flag(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    coda_cursor cursor;

    cursor = info->mds_cursor[index];
    if (coda_cursor_goto_record_field_by_name(&cursor, "flag_vcd_flags") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_int32(&cursor, data.int32_data) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    return 0;
}

static int read_vmr(void *user_data, long index, harp_array data)
{
    return get_profile_vmr_data(user_data, index, "tang_vmr", data);
}

static int read_vmr_error(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    harp_array vmr_data;
    long i;

    if (get_profile_vmr_data(user_data, index, "err_tang_vmr", data) != 0)
    {
        return -1;
    }

    vmr_data.double_data = info->profile_buffer;
    if (read_vmr(user_data, index, vmr_data) != 0)
    {
        return -1;
    }

    for (i = 0; i < info->num_vertical[index]; i++)
    {
        data.double_data[i] = vmr_data.double_data[i] * data.double_data[i] / 100.;
    }

    return 0;
}

static int read_vmr_avk(void *user_data, long index, harp_array data)
{
    return get_avk_from_add_diag(user_data, index, data, 0);
}

static int read_nd(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    coda_cursor cursor;
    long i, reversed_i, num_elements;
    double *add_diag;
    int nd_position = 2 + info->n_stvec[index];

    /* nd is found in add_diag at position 2+nstvec (see e.g. ENV-TN-DLR-SCIA-0077 document) */

    assert(info->has_extended_diag == 1);
    cursor = info->mds_cursor[index];

    if (coda_cursor_goto_record_field_by_name(&cursor, "add_diag") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    if (coda_cursor_get_num_elements(&cursor, &num_elements) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    if (num_elements < 2 + info->n_stvec[index] + 2 * info->num_vertical[index] * info->n_1[index] +
        2 * info->num_vertical[index] + info->n_1[index] * info->num_vertical[index] * info->num_vertical[index])
    {
        harp_set_error(HARP_ERROR_INGESTION, "size of add_diag array (%ld) is too small", num_elements);
        return -1;
    }
    add_diag = malloc(num_elements * sizeof(double));
    if (add_diag == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_elements * sizeof(double), __FILE__, __LINE__);
        return -1;
    }

    if (coda_cursor_read_double_array(&cursor, add_diag, coda_array_ordering_c) != 0)
    {
        free(add_diag);
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    /* store in reversed order */
    for (i = 0, reversed_i = info->num_vertical[index] - 1; i < info->num_vertical[index]; i++, reversed_i--)
    {
        data.double_data[reversed_i] = add_diag[nd_position + i];
    }
    free(add_diag);

    /* fill remaining elements with NaNs */
    for (i = info->num_vertical[index], reversed_i = info->max_num_vertical - info->num_vertical[index] - 1;
         i < info->max_num_vertical; i++, reversed_i--)
    {
        data.double_data[reversed_i] = coda_NaN();
    }

    return 0;
}

static int read_nd_apriori(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    coda_cursor cursor;
    long i, reversed_i, num_elements;
    double *add_diag;
    int nd_ap_position = 2 + info->n_stvec[index] + info->num_vertical[index] * info->n_1[index];

    /* the initial number density profile is found in add_diag at position 2+nstvec+n1*numvert
     * (see e.g. ENV-TN-DLR-SCIA-0077 document)
     */

    assert(info->has_extended_diag == 1);
    cursor = info->mds_cursor[index];

    if (coda_cursor_goto_record_field_by_name(&cursor, "add_diag") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    if (coda_cursor_get_num_elements(&cursor, &num_elements) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    if (num_elements < 2 + info->n_stvec[index] + 2 * info->num_vertical[index] * info->n_1[index] +
        2 * info->num_vertical[index] + info->n_1[index] * info->num_vertical[index] * info->num_vertical[index])
    {
        harp_set_error(HARP_ERROR_INGESTION, "size of add_diag array (%ld) is too small", num_elements);
        return -1;
    }
    add_diag = malloc(num_elements * sizeof(double));
    if (add_diag == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_elements * sizeof(double), __FILE__, __LINE__);
        return -1;
    }

    if (coda_cursor_read_double_array(&cursor, add_diag, coda_array_ordering_c) != 0)
    {
        free(add_diag);
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    /* store in reversed order */
    for (i = 0, reversed_i = info->num_vertical[index] - 1; i < info->num_vertical[index]; i++, reversed_i--)
    {
        data.double_data[reversed_i] = add_diag[nd_ap_position + i];
    }
    free(add_diag);

    /* fill remaining elements with NaNs */
    for (i = info->num_vertical[index], reversed_i = info->max_num_vertical - info->num_vertical[index] - 1;
         i < info->max_num_vertical; i++, reversed_i--)
    {
        data.double_data[reversed_i] = coda_NaN();
    }

    return 0;
}

static int read_nd_error(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    harp_array nd_data;
    long i;

    /* we can use the vmr relative error as relative error for the number density */
    if (get_profile_vmr_data(user_data, index, "err_tang_vmr", data) != 0)
    {
        return -1;
    }

    nd_data.double_data = info->profile_buffer;
    if (read_nd(user_data, index, nd_data) != 0)
    {
        return -1;
    }

    for (i = 0; i < info->num_vertical[index]; i++)
    {
        data.double_data[i] = nd_data.double_data[i] * data.double_data[i] / 100.;
    }

    return 0;
}

static int read_nd_avk(void *user_data, long index, harp_array data)
{
    return get_avk_from_add_diag(user_data, index, data, 1);
}

static void ingestion_done(void *user_data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->coadding_factor != NULL)
    {
        free(info->coadding_factor);
    }
    if (info->num_vertical != NULL)
    {
        free(info->num_vertical);
    }
    if (info->n_stvec != NULL)
    {
        free(info->n_stvec);
    }
    if (info->n_1 != NULL)
    {
        free(info->n_1);
    }
    if (info->profile_buffer != NULL)
    {
        free(info->profile_buffer);
    }
    if (info->integration_time != NULL)
    {
        free(info->integration_time);
    }
    if (info->geo_dsr_id != NULL)
    {
        free(info->geo_dsr_id);
    }
    if (info->mds_cursor != NULL)
    {
        free(info->mds_cursor);
    }
    if (info->geo_cursor != NULL)
    {
        free(info->geo_cursor);
    }
    if (info->clouds_aerosol_cursor != NULL)
    {
        free(info->clouds_aerosol_cursor);
    }

    free(info);
}

static int ingestion_init(const harp_ingestion_module *module, coda_product *product,
                          const harp_ingestion_options *options, harp_product_definition **definition, void **user_data)
{
    const char *option_value;
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
    info->format_version = -1;
    info->num_time = 0;
    info->coadding_factor = NULL;
    info->num_vertical = NULL;
    info->n_stvec = NULL;
    info->n_1 = NULL;
    info->has_extended_diag = 0;
    info->max_num_vertical = -1;
    info->profile_buffer = NULL;
    info->integration_time = NULL;
    info->geo_dsr_id = NULL;
    info->mds_cursor = NULL;
    info->geo_cursor = NULL;
    info->clouds_aerosol_cursor = NULL;
    info->state_type = 0;

    if (coda_get_product_version(product, &info->format_version) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        ingestion_done(info);
        return -1;
    }

    if (harp_ingestion_options_has_option(options, "dataset"))
    {
        if (harp_ingestion_options_get_option(options, "dataset", &option_value) != 0)
        {
            ingestion_done(info);
            return -1;
        }
    }
    else
    {
        option_value = "nad_uv0_o3";
    }
    if (option_value[0] == 'n')
    {
        /* nadir */
        info->state_type = 0;
        if (option_value[4] == 'u')
        {
            /* uv bands */
            if (option_value[6] == '0')
            {
                *definition = module->product_definition[0];
            }
            else if (option_value[6] == '1')
            {
                *definition = module->product_definition[1];
            }
            else if (option_value[6] == '3')
            {
                *definition = module->product_definition[2];
            }
            else if (option_value[6] == '4')
            {
                *definition = module->product_definition[3];
            }
            else if (option_value[6] == '5')
            {
                *definition = module->product_definition[4];
            }
            else if (option_value[6] == '6')
            {
                *definition = module->product_definition[5];
            }
            else if (option_value[6] == '7')
            {
                *definition = module->product_definition[6];
            }
            else if (option_value[6] == '8')
            {
                *definition = module->product_definition[7];
            }
            else if (option_value[6] == '9')
            {
                *definition = module->product_definition[8];
            }
        }
        else
        {
            /* ir bands */
            if (option_value[6] == '0')
            {
                *definition = module->product_definition[9];
            }
            else if (option_value[6] == '1')
            {
                *definition = module->product_definition[10];
            }
            else if (option_value[6] == '2')
            {
                *definition = module->product_definition[11];
            }
            else if (option_value[6] == '3')
            {
                *definition = module->product_definition[12];
            }
            else if (option_value[6] == '4')
            {
                *definition = module->product_definition[13];
            }
        }
    }
    else if (option_value[0] == 'l')
    {
        /* limb */
        info->state_type = 1;
        if (option_value[6] == '0')
        {
            *definition = module->product_definition[14];
        }
        else if (option_value[6] == '1')
        {
            *definition = module->product_definition[15];
        }
        else if (option_value[6] == '3')
        {
            *definition = module->product_definition[16];
        }
    }
    else
    {
        /* cloud */
        info->state_type = 0;
        *definition = module->product_definition[17];
    }

    if (info->state_type == 0)
    {
        if (info->format_version < 4)
        {
            if (option_value[6] == '9')
            {
                /* dataset does not exist -> empty product */
                *user_data = info;
                return 0;
            }
            if (info->format_version < 3)
            {
                if (option_value[6] == '7' || option_value[6] == '8')
                {
                    /* dataset does not exist -> empty product */
                    *user_data = info;
                    return 0;
                }
            }
        }
        if (init_nadir_cursors(info, option_value) != 0)
        {
            ingestion_done(info);
            return -1;
        }
    }
    else
    {
        if (init_limb_cursors(info, option_value) != 0)
        {
            ingestion_done(info);
            return -1;
        }
        if (info->num_time > 0)
        {
            /* check if there is number density and an averaging kernel matrix (akm) available */
            if (init_has_extended_diag(info) != 0)
            {
                ingestion_done(info);
                return -1;
            }
        }
    }

    *user_data = info;

    return 0;
}

static int include_cloud_top_pressure(void *user_data)
{
    return (((ingest_info *)user_data)->format_version < 2);
}

static int include_cloud_top_height(void *user_data)
{
    return (((ingest_info *)user_data)->format_version >= 2);
}

static int include_add_diag(void *user_data)
{
    return ((ingest_info *)user_data)->has_extended_diag;
}

static void register_common_nadir_variables(harp_product_definition *product_definition, const char *dataset)
{
    const char *scan_direction_type_values[] = { "forward", "backward", "mixed" };
    const char *condition_no_coadding = "No co-adding needed";
    const char *condition_single_scan = "Co-adding needed and all N geolocations are within a single scan (N is not "
        "divisible by 5)";
    const char *condition_mixed = "Co-adding needed of both forward and backward scan pixels (N is divisible by 5)";
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2];
    long bounds_dimension[2] = { -1, 4 };
    const char *description;
    char path[MAX_PATH_LENGTH];

    dimension_type[0] = harp_dimension_time;
    dimension_type[1] = harp_dimension_independent;

    /* datetime_start */
    description = "measurement start time";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "datetime_start",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, "seconds since 2000-01-01", NULL,
                                                                      read_datetime);
    snprintf(path, MAX_PATH_LENGTH, "/%s[]/dsr_time", dataset);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* datetime_length */
    description = "measurement integration time";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "datetime_length",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, "s", NULL, read_integration_time);
    snprintf(path, MAX_PATH_LENGTH, "/%s[]/integr_time", dataset);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* orbit_index */
    description = "absolute orbit number";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "orbit_index", harp_type_int32, 0, NULL, NULL,
                                                   description, NULL, NULL, read_orbit_index);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/mph/abs_orbit", NULL);

    /* latitude */
    description = "center latitude for each nadir pixel";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "latitude", harp_type_double,
                                                                      1, dimension_type, NULL, description,
                                                                      "degree_north", NULL, read_latitude);
    snprintf(path, MAX_PATH_LENGTH, "/geolocation_nadir[]/cen_coor_nad/latitude");
    harp_variable_definition_add_mapping(variable_definition, NULL, condition_no_coadding, path, description);
    snprintf(path, MAX_PATH_LENGTH, "/geolocation_nadir[]/cor_coor_nad[]");
    description = "the latitude of the geographic average of cor_coor_nad[2] and cor_coor_nad[3] of the N/2-th pixel";
    harp_variable_definition_add_mapping(variable_definition, NULL, condition_single_scan, path, description);
    snprintf(path, MAX_PATH_LENGTH, "/geolocation_nadir[]/cen_coor_nad, /geolocation_nadir[]/cor_coor_nad[]");
    description = "the latitude of the geographic average of 1: the geographic average of cor_coor_nad[2] and "
        "cor_coor_nad[3] of the second pixel and 2: cen_coor_nad of the N-th pixel";
    harp_variable_definition_add_mapping(variable_definition, NULL, condition_mixed, path, description);

    /* longitude */
    description = "center longitude for each nadir pixel";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "longitude",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, "degree_east", NULL, read_longitude);
    snprintf(path, MAX_PATH_LENGTH, "/geolocation_nadir[]/cen_coor_nad/longitude");
    harp_variable_definition_add_mapping(variable_definition, NULL, condition_no_coadding, path, description);
    snprintf(path, MAX_PATH_LENGTH, "/geolocation_nadir[]/cor_coor_nad[]");
    description = "the longitude of the geographic average of cor_coor_nad[2] and cor_coor_nad[3] of the N/2-th pixel";
    harp_variable_definition_add_mapping(variable_definition, NULL, condition_single_scan, path, description);
    snprintf(path, MAX_PATH_LENGTH, "/geolocation_nadir[]/cen_coor_nad, /geolocation_nadir[]/cor_coor_nad[]");
    description = "the longitude of the geographic average of 1: the geographic average of cor_coor_nad[2] and "
        "cor_coor_nad[3] of the second pixel and 2: cen_coor_nad of the N-th pixel";
    harp_variable_definition_add_mapping(variable_definition, NULL, condition_mixed, path, description);

    /* latitude_bounds */
    description = "corner latitudes for each nadir pixel";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "latitude_bounds",
                                                                      harp_type_double, 2, dimension_type,
                                                                      bounds_dimension, description,
                                                                      "degree_north", NULL, read_latitude_bounds);
    snprintf(path, MAX_PATH_LENGTH, "/geolocation_nadir[]/cor_coor_nad[]/latitude");
    description = "corners are rearranged in the following way: 0,2,3,1";
    harp_variable_definition_add_mapping(variable_definition, NULL, condition_no_coadding, path, description);
    snprintf(path, MAX_PATH_LENGTH, "/geolocation_nadir[]/cor_coor_nad[]");
    description = "cor_coor_nad[0] and cor_coor_nad[1] are taken from the first pixel and cor_coor_nad[2] and "
        "cor_coor_nad[3] are taken from the N-th pixel; corners are rearranged in the following way: 0,2,3,1";
    harp_variable_definition_add_mapping(variable_definition, NULL, condition_single_scan, path, description);
    description = "cor_coor_nad[0] is taken from the first pixel, cor_coor_nad[2] is taken from the fourth pixel, "
        "cor_coor_nad[1] is taken from cor_coor_nad[3] of the N-th pixel, and cor_coor_nad[3] is taken from "
        "cor_coor_nad[1] from the N-th pixel; corners are rearranged in the following way: 0,2,3,1";
    harp_variable_definition_add_mapping(variable_definition, NULL, condition_mixed, path, description);

    /* longitude_bounds */
    description = "corner longitudes for each nadir pixel";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "longitude_bounds",
                                                                      harp_type_double, 2, dimension_type,
                                                                      bounds_dimension, description,
                                                                      "degree_east", NULL, read_longitude_bounds);
    snprintf(path, MAX_PATH_LENGTH, "/geolocation_nadir[]/cor_coor_nad[]/longitude");
    description = "corners are rearranged in the following way: 0,2,3,1";
    harp_variable_definition_add_mapping(variable_definition, NULL, condition_no_coadding, path, description);
    snprintf(path, MAX_PATH_LENGTH, "/geolocation_nadir[]/cor_coor_nad[]");
    description = "cor_coor_nad[0] and cor_coor_nad[1] are taken from the first pixel and cor_coor_nad[2] and "
        "cor_coor_nad[3] are taken from the N-th pixel; corners are rearranged in the following way: 0,2,3,1";
    harp_variable_definition_add_mapping(variable_definition, NULL, condition_single_scan, path, description);
    description = "cor_coor_nad[0] is taken from the first pixel, cor_coor_nad[2] is taken from the fourth pixel, "
        "cor_coor_nad[1] is taken from cor_coor_nad[3] of the N-th pixel, and cor_coor_nad[3] is taken from "
        "cor_coor_nad[1] from the N-th pixel; corners are rearranged in the following way: 0,2,3,1";
    harp_variable_definition_add_mapping(variable_definition, NULL, condition_mixed, path, description);

    /* solar_zenith_angle */
    description = "solar zenith angle at top of atmosphere";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "solar_zenith_angle",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, "degree", NULL,
                                                                      read_solar_zenith_angle);
    snprintf(path, MAX_PATH_LENGTH, "/geolocation_nadir[]/sol_zen_angle_toa[1]");
    harp_variable_definition_add_mapping(variable_definition, NULL, condition_no_coadding, path, NULL);
    snprintf(path, MAX_PATH_LENGTH, "/geolocation_nadir[]/sol_zen_angle_toa[2]");
    description = "the value at end of integration time of the N/2-th geolocation";
    harp_variable_definition_add_mapping(variable_definition, NULL, condition_single_scan, path, description);
    snprintf(path, MAX_PATH_LENGTH,
             "/geolocation_nadir[]/sol_zen_angle_toa[1], /geolocation_nadir[]/sol_zen_angle_toa[2]");
    description = "the average of the value at end of integration time of the second record and the value at middle of "
        "integration time of the N-th record";
    harp_variable_definition_add_mapping(variable_definition, NULL, condition_mixed, path, description);

    /* viewing_zenith_angle */
    description = "line of sight zenith angle at top of atmosphere";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "viewing_zenith_angle",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, "degree", NULL,
                                                                      read_los_zenith_angle);
    snprintf(path, MAX_PATH_LENGTH, "/geolocation_nadir[]/los_zen_angle_toa[1]");
    harp_variable_definition_add_mapping(variable_definition, NULL, condition_no_coadding, path, NULL);
    snprintf(path, MAX_PATH_LENGTH, "/geolocation_nadir[]/los_zen_angle_toa[2]");
    description = "the value at end of integration time of the N/2-th geolocation";
    harp_variable_definition_add_mapping(variable_definition, NULL, condition_single_scan, path, description);
    snprintf(path, MAX_PATH_LENGTH,
             "/geolocation_nadir[]/los_zen_angle_toa[1], /geolocation_nadir[]/los_zen_angle_toa[2]");
    description = "the average of the value at end of integration time of the second record and the value at middle of "
        "integration time of the N-th record";
    harp_variable_definition_add_mapping(variable_definition, NULL, condition_mixed, path, description);

    /* relative_azimuth_angle */
    description = "relative azimuth angle at top of atmosphere";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "relative_azimuth_angle",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, "degree", NULL,
                                                                      read_rel_azimuth_angle);
    snprintf(path, MAX_PATH_LENGTH, "/geolocation_nadir[]/rel_azi_angle_toa[1]");
    harp_variable_definition_add_mapping(variable_definition, NULL, condition_no_coadding, path, NULL);
    snprintf(path, MAX_PATH_LENGTH, "/geolocation_nadir[]/rel_azi_angle_toa[2]");
    description = "the value at end of integration time of the N/2-th geolocation";
    harp_variable_definition_add_mapping(variable_definition, NULL, condition_single_scan, path, description);
    snprintf(path, MAX_PATH_LENGTH,
             "/geolocation_nadir[]/rel_azi_angle_toa[1], /geolocation_nadir[]/rel_azi_angle_toa[2]");
    description = "the average of the value at end of integration time of the second record and the value at middle of "
        "integration time of the N-th record";
    harp_variable_definition_add_mapping(variable_definition, NULL, condition_mixed, path, description);

    /* scan_direction_type */
    description = "scan direction for each measurement";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "scan_direction_type",
                                                                      harp_type_int8, 1, dimension_type, NULL,
                                                                      description, NULL, NULL,
                                                                      read_scan_direction_type);
    harp_variable_definition_set_enumeration_values(variable_definition, 3, scan_direction_type_values);
    snprintf(path, MAX_PATH_LENGTH, "/geolocation_nadir[]/corner_coord[], /geolocation_nadir[]/dsr_time");
    description = "When the integration time is higher than 1s we are dealing with a mixed (2) pixel, otherwise the "
        "scan direction is based on the corner coordinates of the first ground pixel of the measurement. The first "
        "geolocation pixel is a backscan (1) pixel if the inproduct of the unit vector of the third corner with the "
        "outproduct of the unit vector of the first corner and the unit vector of the second corner is negative "
        "(otherwise it is part of a forward (0) scan)";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);
}

static void register_common_nadir_cloud_variables(harp_product_definition *product_definition)
{
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1];
    const char *description;
    const char *path;

    dimension_type[0] = harp_dimension_time;

    /* cloud_fraction */
    description = "average cloud fraction of footprint";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "cloud_fraction",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                                      read_cloud_fraction);
    path = "/clouds_aerosol[]/cl_frac";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

static void register_common_limb_variables(harp_product_definition *product_definition, const char *dataset)
{
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[3];
    long bounds_dimension[3] = { -1, -1, 2 };
    const char *description;
    const char *limb_mapping;
    char path[MAX_PATH_LENGTH];

    limb_mapping = "records in geolocation_limb do not have a one-to-one mapping with records in the limb/occultation "
        "measurement datasets; HARP uses a single measurement time and tangent location per profile which is taken "
        "from the middlemost measurement used for the retrieval (i.e. index = (n_meas - 1) / 2); the geolocation "
        "record for this measurement is retrieved by matching the measurement time "
        "measurement_grid[(n_meas - 1) / 2].dsr_time with the geolocation record time geolocation_limb[]/dsr_time";

    dimension_type[0] = harp_dimension_time;
    dimension_type[1] = harp_dimension_vertical;
    dimension_type[2] = harp_dimension_independent;

    /* datetime_start */
    description = "measurement start time for each profile";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "datetime_start",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, "seconds since 2000-01-01", NULL,
                                                                      read_datetime_profile);
    snprintf(path, MAX_PATH_LENGTH, "/geolocation_limb[]/dsr_time");
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, limb_mapping);

    /* datetime_length */
    description = "measurement integration time";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "datetime_length",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, "s", NULL, read_integration_time);
    snprintf(path, MAX_PATH_LENGTH, "/%s[]/integr_time", dataset);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* orbit_index */
    description = "absolute orbit number";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "orbit_index", harp_type_int32, 0, NULL, NULL,
                                                   description, NULL, NULL, read_orbit_index);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/mph/abs_orbit", NULL);

    /* altitude_bounds */
    description = "altitude bounds for each profile point";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "altitude_bounds",
                                                                      harp_type_double, 3, dimension_type,
                                                                      bounds_dimension, description, "km", NULL,
                                                                      read_altitude_bounds);
    description = "the tangent heights are the lower bound altitudes; for the top of the highest layer a TOA value "
        "of 100km is used";
    snprintf(path, MAX_PATH_LENGTH, "/%s[]/tangent_height[]", dataset);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* pressure_bounds */
    description = "pressure bounds for each profile point";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "pressure_bounds",
                                                                      harp_type_double, 3, dimension_type,
                                                                      bounds_dimension, description, "hPa", NULL,
                                                                      read_pressure_bounds);
    snprintf(path, MAX_PATH_LENGTH, "/%s[]/tangent_pressure[]", dataset);
    description = "the tangent pressures are the lower bound pressures; for the top of the highest layer a pressure "
        "value of 3.2e-4 hPa is used";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* latitude */
    description = "tangent latitude of the vertically mid profile point";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "latitude", harp_type_double,
                                                                      1, dimension_type, NULL, description,
                                                                      "degree_north", NULL, read_latitude_profile);
    snprintf(path, MAX_PATH_LENGTH, "/geolocation_limb[]/tangent_coord[1]/latitude");
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, limb_mapping);

    /* longitude */
    description = "tangent longitude of the vertically mid profile point";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "longitude",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, "degree_east", NULL,
                                                                      read_longitude_profile);
    snprintf(path, MAX_PATH_LENGTH, "/geolocation_limb[]/tangent_coord[1]/longitude");
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, limb_mapping);

    /* solar_zenith_angle */
    description = "solar zenith angle at top of atmosphere for the middle most profile point";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "solar_zenith_angle",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, "degree", NULL,
                                                                      read_solar_zenith_angle_profile);
    snprintf(path, MAX_PATH_LENGTH, "/geolocation_limb[]/sol_zen_angle_toa[1]");
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, limb_mapping);

    /* viewing_zenith_angle */
    description = "line of sight zenith angle at top of atmosphere for the middle most profile point";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "viewing_zenith_angle",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, "degree", NULL,
                                                                      read_los_zenith_angle_profile);
    snprintf(path, MAX_PATH_LENGTH, "/geolocation_limb[]/los_zen_angle_toa[1]");
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, limb_mapping);

    /* relative_azimuth_angle */
    description = "relative azimuth angle at top of atmosphere for the middle most profile point";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "relative_azimuth_angle",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, "degree", NULL,
                                                                      read_rel_azimuth_angle_profile);
    snprintf(path, MAX_PATH_LENGTH, "/geolocation_limb[]/rel_azi_angle_toa[1]");
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, limb_mapping);

    /* temperature */
    description = "temperature for each profile point";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "temperature",
                                                                      harp_type_double, 2, dimension_type,
                                                                      bounds_dimension, description, "K", NULL,
                                                                      read_temperature);
    snprintf(path, MAX_PATH_LENGTH, "/%s[]/tangent_temp[]", dataset);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

int harp_ingestion_module_sciamachy_l2_init(void)
{
    const char *dataset_options[] = {
        "nad_uv0_o3", "nad_uv1_no2", "nad_uv3_bro", "nad_uv4_h2co", "nad_uv5_so2", "nad_uv6_oclo", "nad_uv7_so2",
        "nad_uv8_h2o", "nad_uv9_chocho", "nad_ir0_h2o", "nad_ir1_ch4", "nad_ir2_n2o", "nad_ir3_co", "nad_ir4_co2",
        "lim_uv0_o3", "lim_uv1_no2", "lim_uv3_bro", "clouds_aerosol"
    };
    const char *condition_add_diag =
        "additional diagnostics vector in limb DSR is long enough to contain number density and AKM information";
    const char *condition_3k = "applicable format specification >= PO-RS-MDA-GS2009_15_3K";
    const char *condition_3j = "applicable format specification <= PO-RS-MDA-GS2009_15_3J";
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[3];
    const char *error_mapping;
    const char *vmr_avk_mapping;
    const char *nd_avk_mapping;
    const char *description;
    const char *path;

    dimension_type[0] = harp_dimension_time;
    dimension_type[1] = harp_dimension_vertical;
    dimension_type[2] = harp_dimension_vertical;

    error_mapping = "relative error is converted to absolute error by multiplying with measured value";
    vmr_avk_mapping = "the AVK for partial columns as given in the add_diag vector at position "
        "2+stvec+2*n1*num_altitudes+2*num_altitudes is converted to volume mixing ratio units by multiplying each "
        "element with conv_mix_i/conv_mix_j, where conv_mix is found in add_diag at position "
        "2+stvec+2*n1*num_altitudes; the vertical axis of the AVK are reversed";
    nd_avk_mapping = "the AVK for partial columns as given in the add_diag vector at position "
        "2+stvec+2*n1*num_altitudes+2*num_altitudes is converted to number density units by multiplying each element "
        "with conv_nd_i/conv_nd_j, where conv_nd is found in add_diag at position "
        "2+stvec+2*n1*num_altitudes+num_altitudes; the vertical axis of the AVK are reversed";

    description = "SCIAMACHY Off-Line Level-2";
    module = harp_ingestion_register_module("SCIAMACHY_L2", "SCIAMACHY", "ENVISAT_SCIAMACHY", "SCI_OL__2P",
                                                 description, ingestion_init, ingestion_done);

    harp_ingestion_register_option(module, "dataset", "the dataset of the L2 product to ingest; each dataset is a "
                                   "combination of nadir/limb choice, retrieval window, and main quantity; option "
                                   "values are 'nad_uv0_o3' (default), 'nad_uv1_no2', 'nad_uv3_bro',  'nad_uv4_h2co', "
                                   "'nad_uv5_so2', 'nad_uv6_oclo', 'nad_uv7_so2', 'nad_uv8_h2o', 'nad_uv9_chocho', "
                                   "'nad_ir0_h2o', 'nad_ir1_ch4', 'nad_ir2_n2o', 'nad_ir3_co', 'nad_ir4_co2', "
                                   "'lim_uv0_o3', 'lim_uv1_no2', 'lim_uv3_bro', 'clouds_aerosol'", 18, dataset_options);

    /*** nad_uv0_o3 ***/
    description = "total column data retrieved from UV window 0 (O3)";
    product_definition = harp_ingestion_register_product(module, "SCIAMACHY_L2_NADIR_UV0_O3", description,
                                                         read_dimensions);
    harp_product_definition_add_mapping(product_definition, NULL, "dataset=nad_uv0_o3 or dataset unset");

    register_common_nadir_variables(product_definition, "nad_uv0_o3");

    /* O3_column_number_density */
    description = "ozone vertical column density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "O3_column_number_density",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, "molec/cm^2", NULL, read_vcd);
    path = "/nad_uv0_o3[]/vcd[0]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* O3_column_number_density_uncertainty */
    description = "error on the ozone vertical column density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "O3_column_number_density_uncertainty",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, "molec/cm^2", NULL, read_vcd_error);
    path = "/nad_uv0_o3[]/vcd_err[0], /nad_uv0_o3[]/vcd[0]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, error_mapping);

    /* O3_column_number_density_validity */
    description = "flag describing the ozone vertical column density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "O3_column_number_density_validity",
                                                                      harp_type_int32, 1, dimension_type, NULL,
                                                                      description, NULL, NULL, read_vcd_flag);
    path = "/nad_uv0_o3[]/flag_vcd_flags";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    register_common_nadir_cloud_variables(product_definition);

    /*** nad_uv1_no2 ***/
    description = "total column data retrieved from UV window 1 (NO2)";
    product_definition = harp_ingestion_register_product(module, "SCIAMACHY_L2_NADIR_UV1_NO2", description,
                                                         read_dimensions);
    harp_product_definition_add_mapping(product_definition, NULL, "dataset=nad_uv1_no2");

    register_common_nadir_variables(product_definition, "nad_uv1_no2");

    /* NO2_column_number_density */
    description = "NO2 vertical column density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "NO2_column_number_density",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, "molec/cm^2", NULL, read_vcd);
    path = "/nad_uv1_no2[]/vcd[0]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* NO2_column_number_density_uncertainty */
    description = "error on the NO2 vertical column density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "NO2_column_number_density_uncertainty",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, "molec/cm^2", NULL, read_vcd_error);
    path = "/nad_uv1_no2[]/vcd_err[0], /nad_uv1_no2[]/vcd[0]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, error_mapping);

    /* NO2_column_number_density_validity */
    description = "flag describing the NO2 vertical column density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "NO2_column_number_density_validity",
                                                                      harp_type_int32, 1, dimension_type, NULL,
                                                                      description, NULL, NULL, read_vcd_flag);
    path = "/nad_uv1_no2[]/flag_vcd_flags";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    register_common_nadir_cloud_variables(product_definition);

    /*** nad_uv3_bro ***/
    description = "total column data retrieved from UV window 3 (BrO)";
    product_definition = harp_ingestion_register_product(module, "SCIAMACHY_L2_NADIR_UV3_BRO", description,
                                                         read_dimensions);
    harp_product_definition_add_mapping(product_definition, NULL, "dataset=nad_uv3_bro");

    register_common_nadir_variables(product_definition, "nad_uv3_bro");

    /* BrO_column_number_density */
    description = "BrO vertical column density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "BrO_column_number_density",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, "molec/cm^2", NULL, read_vcd);
    path = "/nad_uv3_bro[]/vcd[0]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* BrO_column_number_density_uncertainty */
    description = "error on the BrO vertical column density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "BrO_column_number_density_uncertainty",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, "molec/cm^2", NULL, read_vcd_error);
    path = "/nad_uv3_bro[]/vcd_err[0], /nad_uv3_bro[]/vcd[0]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, error_mapping);

    /* BrO_column_number_density_validity */
    description = "flag describing the BrO vertical column density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "BrO_column_number_density_validity",
                                                                      harp_type_int32, 1, dimension_type, NULL,
                                                                      description, NULL, NULL, read_vcd_flag);
    path = "/nad_uv3_bro[]/flag_vcd_flags";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    register_common_nadir_cloud_variables(product_definition);

    /*** nad_uv4_h2co ***/
    description = "total column data retrieved from UV window 4 (H2CO)";
    product_definition = harp_ingestion_register_product(module, "SCIAMACHY_L2_NADIR_UV4_H2CO", description,
                                                         read_dimensions);
    harp_product_definition_add_mapping(product_definition, NULL, "dataset=nad_uv4_h2co");

    register_common_nadir_variables(product_definition, "nad_uv4_h2co");

    /* HCHO_column_number_density */
    description = "HCHO vertical column density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "HCHO_column_number_density",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, "molec/cm^2", NULL, read_vcd);
    path = "/nad_uv4_h2co[]/vcd[0]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* HCHO_column_number_density_uncertainty */
    description = "error on the HCHO vertical column density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "HCHO_column_number_density_uncertainty",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, "molec/cm^2", NULL, read_vcd_error);
    path = "/nad_uv4_h2co[]/vcd_err[0], /nad_uv4_h2co[]/vcd[0]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, error_mapping);

    /* HCHO_column_number_density_validity */
    description = "flag describing the HCHO vertical column density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "HCHO_column_number_density_validity",
                                                                      harp_type_int32, 1, dimension_type, NULL,
                                                                      description, NULL, NULL, read_vcd_flag);
    path = "/nad_uv4_h2co[]/flag_vcd_flags";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    register_common_nadir_cloud_variables(product_definition);

    /*** nad_uv5_so2 ***/
    description = "total column data retrieved from UV window 5 (SO2)";
    product_definition = harp_ingestion_register_product(module, "SCIAMACHY_L2_NADIR_UV5_SO2", description,
                                                         read_dimensions);
    harp_product_definition_add_mapping(product_definition, NULL, "dataset=nad_uv5_so2");

    register_common_nadir_variables(product_definition, "nad_uv5_so2");

    /* SO2_column_number_density */
    description = "SO2 vertical column density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "SO2_column_number_density",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, "molec/cm^2", NULL, read_vcd);
    path = "/nad_uv5_so2[]/vcd[0]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* SO2_column_number_density_uncertainty */
    description = "error on the SO2 vertical column density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "SO2_column_number_density_uncertainty",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, "molec/cm^2", NULL, read_vcd_error);
    path = "/nad_uv5_so2[]/vcd_err[0], /nad_uv5_so2[]/vcd[0]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, error_mapping);

    /* SO2_column_number_density_validity */
    description = "flag describing the SO2 vertical column density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "SO2_column_number_density_validity",
                                                                      harp_type_int32, 1, dimension_type, NULL,
                                                                      description, NULL, NULL, read_vcd_flag);
    path = "/nad_uv5_so2[]/flag_vcd_flags";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    register_common_nadir_cloud_variables(product_definition);

    /*** nad_uv6_oclo ***/
    description = "total column data retrieved from UV window 6 (OClO)";
    product_definition = harp_ingestion_register_product(module, "SCIAMACHY_L2_NADIR_UV6_OCLO", description,
                                                         read_dimensions);
    harp_product_definition_add_mapping(product_definition, NULL, "dataset=nad_uv6_oclo");

    register_common_nadir_variables(product_definition, "nad_uv6_oclo");

    /* OClO_column_number_density */
    description = "OClO vertical column density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "OClO_column_number_density",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, "molec/cm^2", NULL, read_vcd);
    path = "/nad_uv6_oclo[]/vcd[0]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* OClO_column_number_density_uncertainty */
    description = "error on the OClO vertical column density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "OClO_column_number_density_uncertainty",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, "molec/cm^2", NULL, read_vcd_error);
    path = "/nad_uv6_oclo[]/vcd_err[0], /nad_uv6_oclo[]/vcd[0]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, error_mapping);

    /* OClO_column_number_density_validity */
    description = "flag describing the OClO vertical column density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "OClO_column_number_density_validity",
                                                                      harp_type_int32, 1, dimension_type, NULL,
                                                                      description, NULL, NULL, read_vcd_flag);
    path = "/nad_uv6_oclo[]/flag_vcd_flags";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    register_common_nadir_cloud_variables(product_definition);

    /*** nad_uv7_so2 ***/
    description = "total column data retrieved from UV window 7 (SO2)";
    product_definition = harp_ingestion_register_product(module, "SCIAMACHY_L2_NADIR_UV7_SO2", description,
                                                         read_dimensions);
    harp_product_definition_add_mapping(product_definition, NULL, "dataset=nad_uv7_so2");

    register_common_nadir_variables(product_definition, "nad_uv7_so2");

    /* SO2_column_number_density */
    description = "SO2 vertical column density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "SO2_column_number_density",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, "molec/cm^2", NULL, read_vcd);
    path = "/nad_uv7_so2[]/vcd[0]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* SO2_column_number_density_uncertainty */
    description = "error on the SO2 vertical column density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "SO2_column_number_density_uncertainty",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, "molec/cm^2", NULL, read_vcd_error);
    path = "/nad_uv7_so2[]/vcd_err[0], /nad_uv7_so2[]/vcd[0]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, error_mapping);

    /* SO2_column_number_density_validity */
    description = "flag describing the SO2 vertical column density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "SO2_column_number_density_validity",
                                                                      harp_type_int32, 1, dimension_type, NULL,
                                                                      description, NULL, NULL, read_vcd_flag);
    path = "/nad_uv7_so2[]/flag_vcd_flags";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    register_common_nadir_cloud_variables(product_definition);

    /*** nad_uv8_h2o ***/
    description = "total column data retrieved from UV window 8 (H2O)";
    product_definition = harp_ingestion_register_product(module, "SCIAMACHY_L2_NADIR_UV8_H2O", description,
                                                         read_dimensions);
    harp_product_definition_add_mapping(product_definition, NULL, "dataset=nad_uv8_h2o");

    register_common_nadir_variables(product_definition, "nad_uv8_h2o");

    /* H2O_column_number_density */
    description = "H2O vertical column density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "H2O_column_number_density",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, "molec/cm^2", NULL, read_vcd);
    path = "/nad_uv8_h2o[]/vcd[0]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* H2O_column_number_density_uncertainty */
    description = "error on the H2O vertical column density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "H2O_column_number_density_uncertainty",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, "molec/cm^2", NULL, read_vcd_error);
    path = "/nad_uv8_h2o[]/vcd_err[0], /nad_uv8_h2o[]/vcd[0]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, error_mapping);

    /* H2O_column_number_density_validity */
    description = "flag describing the H2O vertical column density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "H2O_column_number_density_validity",
                                                                      harp_type_int32, 1, dimension_type, NULL,
                                                                      description, NULL, NULL, read_vcd_flag);
    path = "/nad_uv8_h2o[]/flag_vcd_flags";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    register_common_nadir_cloud_variables(product_definition);

    /*** nad_uv9_chocho ***/
    description = "total column data retrieved from UV window 9 (CHOCHO)";
    product_definition = harp_ingestion_register_product(module, "SCIAMACHY_L2_NADIR_UV9_CHOCHO", description,
                                                         read_dimensions);
    harp_product_definition_add_mapping(product_definition, NULL, "dataset=nad_uv9_chocho");

    register_common_nadir_variables(product_definition, "nad_uv9_chocho");

    /* C2H2O2_column_number_density */
    description = "C2H2O2 vertical column density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "C2H2O2_column_number_density", harp_type_double,
                                                                      1, dimension_type, NULL, description,
                                                                      "molec/cm^2", NULL, read_vcd);
    path = "/nad_uv9_chocho[]/vcd[0]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* C2H2O2_column_number_density_uncertainty */
    description = "error on the C2H2O2 vertical column density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "C2H2O2_column_number_density_uncertainty",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, "molec/cm^2", NULL, read_vcd_error);
    path = "/nad_uv9_chocho[]/vcd_err[0], /nad_uv8_h2o[]/vcd[0]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, error_mapping);

    /* C2H2O2_column_number_density_validity */
    description = "flag describing the C2H2O2 vertical column density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "C2H2O2_column_number_density_validity",
                                                                      harp_type_int32, 1, dimension_type, NULL,
                                                                      description, NULL, NULL, read_vcd_flag);
    path = "/nad_uv9_chocho[]/flag_vcd_flags";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    register_common_nadir_cloud_variables(product_definition);

    /*** nad_ir0_h2o ***/
    description = "total column data retrieved from IR window 0 (H2O)";
    product_definition = harp_ingestion_register_product(module, "SCIAMACHY_L2_NADIR_IR0_H2O", description,
                                                         read_dimensions);
    harp_product_definition_add_mapping(product_definition, NULL, "dataset=nad_ir0_h2o");

    register_common_nadir_variables(product_definition, "nad_ir0_h2o");

    /* H2O_column_number_density */
    description = "H2O vertical column density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "H2O_column_number_density",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, "molec/cm^2", NULL, read_vcd);
    path = "/nad_ir0_h2o[]/vcd[0]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* H2O_column_number_density_uncertainty */
    description = "error on the H2O vertical column density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "H2O_column_number_density_uncertainty",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, "molec/cm^2", NULL, read_vcd_error);
    path = "/nad_ir0_h2o[]/vcd_err[0], /nad_ir0_h2o[]/vcd[0]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, error_mapping);

    /* H2O_column_number_density_validity */
    description = "flag describing the H2O vertical column density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "H2O_column_number_density_validity",
                                                                      harp_type_int32, 1, dimension_type, NULL,
                                                                      description, NULL, NULL, read_vcd_flag);
    path = "/nad_ir0_h2o[]/flag_vcd_flags";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    register_common_nadir_cloud_variables(product_definition);

    /*** nad_ir1_ch4 ***/
    description = "total column data retrieved from IR window 1 (CH4)";
    product_definition = harp_ingestion_register_product(module, "SCIAMACHY_L2_NADIR_IR1_CH4", description,
                                                         read_dimensions);
    harp_product_definition_add_mapping(product_definition, NULL, "dataset=nad_ir1_ch4");

    register_common_nadir_variables(product_definition, "nad_ir1_ch4");

    /* CH4_column_number_density */
    description = "CH4 vertical column density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "CH4_column_number_density",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, "molec/cm^2", NULL, read_vcd);
    path = "/nad_ir1_ch4[]/vcd[0]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* CH4_column_number_density_uncertainty */
    description = "error on the CH4 vertical column density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "CH4_column_number_density_uncertainty",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, "molec/cm^2", NULL, read_vcd_error);
    path = "/nad_ir1_ch4[]/vcd_err[0], /nad_ir1_ch4[]/vcd[0]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, error_mapping);

    /* CH4_column_number_density_validity */
    description = "flag describing the CH4 vertical column density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "CH4_column_number_density_validity",
                                                                      harp_type_int32, 1, dimension_type, NULL,
                                                                      description, NULL, NULL, read_vcd_flag);
    path = "/nad_ir1_ch4[]/flag_vcd_flags";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    register_common_nadir_cloud_variables(product_definition);

    /*** nad_ir2_n2o ***/
    description = "total column data retrieved from IR window 2 (N2O)";
    product_definition = harp_ingestion_register_product(module, "SCIAMACHY_L2_NADIR_IR2_N2O", description,
                                                         read_dimensions);
    harp_product_definition_add_mapping(product_definition, NULL, "dataset=nad_ir2_n2o");

    register_common_nadir_variables(product_definition, "nad_ir2_n2o");

    /* N2O_column_number_density */
    description = "N2O vertical column density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "N2O_column_number_density",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, "molec/cm^2", NULL, read_vcd);
    path = "/nad_ir2_n2o[]/vcd[0]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* N2O_column_number_density_uncertainty */
    description = "error on the N2O vertical column density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "N2O_column_number_density_uncertainty",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, "molec/cm^2", NULL, read_vcd_error);
    path = "/nad_ir2_n2o[]/vcd_err[0], /nad_ir2_n2o[]/vcd[0]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, error_mapping);

    /* N2O_column_number_density_validity */
    description = "flag describing the N2O vertical column density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "N2O_column_number_density_validity",
                                                                      harp_type_int32, 1, dimension_type, NULL,
                                                                      description, NULL, NULL, read_vcd_flag);
    path = "/nad_ir2_n2o[]/flag_vcd_flags";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    register_common_nadir_cloud_variables(product_definition);

    /*** nad_ir3_co ***/
    description = "total column data retrieved from IR window 3 (CO)";
    product_definition = harp_ingestion_register_product(module, "SCIAMACHY_L2_NADIR_IR3_CO", description,
                                                         read_dimensions);
    harp_product_definition_add_mapping(product_definition, NULL, "dataset=nad_ir3_co");

    register_common_nadir_variables(product_definition, "nad_ir3_co");

    /* CO_column_number_density */
    description = "CO vertical column density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "CO_column_number_density",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, "molec/cm^2", NULL, read_vcd);
    path = "/nad_ir3_co[]/vcd[0]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* CO_column_number_density_uncertainty */
    description = "error on the CO vertical column density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "CO_column_number_density_uncertainty",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, "molec/cm^2", NULL, read_vcd_error);
    path = "/nad_ir3_co[]/vcd_err[0], /nad_ir3_co[]/vcd[0]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, error_mapping);

    /* CO_column_number_density_validity */
    description = "flag describing the CO vertical column density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "CO_column_number_density_validity",
                                                                      harp_type_int32, 1, dimension_type, NULL,
                                                                      description, NULL, NULL, read_vcd_flag);
    path = "/nad_ir3_co[]/flag_vcd_flags";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    register_common_nadir_cloud_variables(product_definition);

    /*** nad_ir4_co2 ***/
    description = "total column data retrieved from IR window 4 (CO2)";
    product_definition = harp_ingestion_register_product(module, "SCIAMACHY_L2_NADIR_IR4_CO2", description,
                                                         read_dimensions);
    harp_product_definition_add_mapping(product_definition, NULL, "dataset=nad_ir4_co2");

    register_common_nadir_variables(product_definition, "nad_ir4_co2");

    /* CO2_column_number_density */
    description = "CO2 vertical column density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "CO2_column_number_density",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, "molec/cm^2", NULL, read_vcd);
    path = "/nad_ir4_co2[]/vcd[0]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* CO2_column_number_density_uncertainty */
    description = "error on the CO2 vertical column density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "CO2_column_number_density_uncertainty",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, "molec/cm^2", NULL, read_vcd_error);
    path = "/nad_ir4_co2[]/vcd_err[0], /nad_ir4_co2[]/vcd[0]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, error_mapping);

    /* CO2_column_number_density_validity */
    description = "flag describing the CO2 vertical column density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "CO2_column_number_density_validity",
                                                                      harp_type_int32, 1, dimension_type, NULL,
                                                                      description, NULL, NULL, read_vcd_flag);
    path = "/nad_ir4_co2[]/flag_vcd_flags";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    register_common_nadir_cloud_variables(product_definition);

    /*** lim_uv0_o3 ***/
    description = "limb profile data retrieved from UV window 0 (O3)";
    product_definition = harp_ingestion_register_product(module, "SCIAMACHY_L2_LIMB_UV0_O3", description,
                                                         read_dimensions);
    harp_product_definition_add_mapping(product_definition, NULL, "dataset=lim_uv0_o3");

    register_common_limb_variables(product_definition, "lim_uv0_o3");

    /* O3_volume_mixing_ratio */
    description = "ozone volume mixing ratio";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "O3_volume_mixing_ratio",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "ppv", NULL, read_vmr);
    path = "/lim_uv0_o3[]/main_species[,0]/tang_vmr";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* O3_volume_mixing_ratio_uncertainty */
    description = "error on the ozone volume mixing ratio";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "O3_volume_mixing_ratio_uncertainty",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "ppv", NULL, read_vmr_error);
    path = "/lim_uv0_o3[]/main_species[,0]/err_tang_vmr, /lim_uv0_o3[]/main_species[,0]/tang_vmr";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, error_mapping);

    /* O3_volume_mixing_ratio_avk */
    description = "averaging kernel on the ozone volume mixing ratio";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "O3_volume_mixing_ratio_avk",
                                                                      harp_type_double, 3, dimension_type, NULL,
                                                                      description, HARP_UNIT_DIMENSIONLESS,
                                                                      include_add_diag, read_vmr_avk);
    path = "/lim_uv0_o3[]/main_species[,0]/add_diag[0..n]";
    harp_variable_definition_add_mapping(variable_definition, NULL, condition_add_diag, path, vmr_avk_mapping);

    /* O3_number_density */
    description = "ozone number density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "O3_number_density",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "molec/cm^3",
                                                                      include_add_diag, read_nd);
    path = "/lim_uv0_o3[]/main_species[,0]/add_diag[0..n]";
    harp_variable_definition_add_mapping(variable_definition, NULL, condition_add_diag, path, NULL);

    /* O3_number_density_uncertainty */
    description = "error on the ozone number density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "O3_number_density_uncertainty",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "molec/cm^3",
                                                                      include_add_diag, read_nd_error);
    path = "/lim_uv0_o3[]/main_species[,0]/err_tang_vmr, /lim_uv0_o3[]/main_species[,0]/add_diag[0..n]";
    harp_variable_definition_add_mapping(variable_definition, NULL, condition_add_diag, path, error_mapping);

    /* O3_number_density_apriori */
    description = "a priori ozone number density profile";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "O3_number_density_apriori",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "molec/cm^3",
                                                                      include_add_diag, read_nd_apriori);
    path = "/lim_uv0_o3[]/main_species[,0]/add_diag[0..n]";
    harp_variable_definition_add_mapping(variable_definition, NULL, condition_add_diag, path, NULL);

    /* O3_number_density_avk */
    description = "averaging kernel on the ozone number density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "O3_number_density_avk",
                                                                      harp_type_double, 3, dimension_type, NULL,
                                                                      description, "(molec/cm^3)/(molec/cm^3)",
                                                                      include_add_diag, read_nd_avk);
    path = "/lim_uv0_o3[]/main_species[,0]/add_diag[0..n]";
    harp_variable_definition_add_mapping(variable_definition, NULL, condition_add_diag, path, nd_avk_mapping);

    /*** lim_uv1_no2 ***/
    description = "limb profile data retrieved from UV window 1 (NO2)";
    product_definition = harp_ingestion_register_product(module, "SCIAMACHY_L2_LIMB_UV1_NO2", description,
                                                         read_dimensions);
    harp_product_definition_add_mapping(product_definition, NULL, "dataset=lim_uv1_no2");

    register_common_limb_variables(product_definition, "lim_uv1_no2");

    /* NO2_volume_mixing_ratio */
    description = "NO2 volume mixing ratio";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "NO2_volume_mixing_ratio",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "ppv", NULL, read_vmr);
    path = "/lim_uv1_no2[]/main_species[,0]/tang_vmr";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* NO2_volume_mixing_ratio_uncertainty */
    description = "error on the NO2 volume mixing ratio";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "NO2_volume_mixing_ratio_uncertainty",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "ppv", NULL, read_vmr_error);
    path = "/lim_uv1_no2[]/main_species[,0]/err_tang_vmr, /lim_uv1_no2[]/main_species[,0]/tang_vmr";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, error_mapping);

    /* NO2_volume_mixing_ratio_avk */
    description = "averaging kernel on the NO2 volume mixing ratio";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "NO2_volume_mixing_ratio_avk",
                                                                      harp_type_double, 3, dimension_type, NULL,
                                                                      description, HARP_UNIT_DIMENSIONLESS,
                                                                      include_add_diag, read_vmr_avk);
    path = "/lim_uv1_no2[]/main_species[,0]/add_diag[0..n]";
    harp_variable_definition_add_mapping(variable_definition, NULL, condition_add_diag, path, vmr_avk_mapping);

    /* NO2_number_density */
    description = "NO2 number density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "NO2_number_density",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "molec/cm^3",
                                                                      include_add_diag, read_nd);
    path = "/lim_uv1_no2[]/main_species[,0]/add_diag[0..n]";
    harp_variable_definition_add_mapping(variable_definition, NULL, condition_add_diag, path, NULL);

    /* NO2_number_density_uncertainty */
    description = "error on the NO2 number density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "NO2_number_density_uncertainty",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "molec/cm^3",
                                                                      include_add_diag, read_nd_error);
    path = "/lim_uv1_no2[]/main_species[,0]/err_tang_vmr, /lim_uv1_no2[]/main_species[,0]/add_diag[0..n]";
    harp_variable_definition_add_mapping(variable_definition, NULL, condition_add_diag, path, error_mapping);

    /* NO2_number_density_apriori */
    description = "a priori NO2 number density profile";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "NO2_number_density_apriori",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "molec/cm^3",
                                                                      include_add_diag, read_nd_apriori);
    path = "/lim_uv1_no2[]/main_species[,0]/add_diag[0..n]";
    harp_variable_definition_add_mapping(variable_definition, NULL, condition_add_diag, path, NULL);

    /* NO2_number_density_avk */
    description = "averaging kernel on the NO2 number density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "NO2_number_density_avk",
                                                                      harp_type_double, 3, dimension_type, NULL,
                                                                      description, "(molec/cm^3)/(molec/cm^3)",
                                                                      include_add_diag, read_nd_avk);
    path = "/lim_uv1_no2[]/main_species[,0]/add_diag[0..n]";
    harp_variable_definition_add_mapping(variable_definition, NULL, condition_add_diag, path, nd_avk_mapping);

    /*** lim_uv3_bro ***/
    description = "limb profile data retrieved from UV window 3 (BrO)";
    product_definition = harp_ingestion_register_product(module, "SCIAMACHY_L2_LIMB_UV3_BRO", description,
                                                         read_dimensions);
    harp_product_definition_add_mapping(product_definition, NULL, "dataset=lim_uv3_bro");

    register_common_limb_variables(product_definition, "lim_uv3_bro");

    /* BrO_volume_mixing_ratio */
    description = "BrO volume mixing ratio";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "BrO_volume_mixing_ratio",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "ppv", NULL, read_vmr);
    path = "/lim_uv3_bro[]/main_species[,0]/tang_vmr";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* BrO_volume_mixing_ratio_uncertainty */
    description = "error on the BrO volume mixing ratio";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "BrO_volume_mixing_ratio_uncertainty",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "ppv", NULL, read_vmr_error);
    path = "/lim_uv3_bro[]/main_species[,0]/err_tang_vmr, /lim_uv3_bro[]/main_species[,0]/tang_vmr";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, error_mapping);

    /* BrO_volume_mixing_ratio_avk */
    description = "averaging kernel on the BrO volume mixing ratio";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "BrO_volume_mixing_ratio_avk",
                                                                      harp_type_double, 3, dimension_type, NULL,
                                                                      description, HARP_UNIT_DIMENSIONLESS,
                                                                      include_add_diag, read_vmr_avk);
    path = "/lim_uv3_bro[]/main_species[,0]/add_diag[0..n]";
    harp_variable_definition_add_mapping(variable_definition, NULL, condition_add_diag, path, vmr_avk_mapping);

    /* BrO_number_density */
    description = "BrO number density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "BrO_number_density",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "molec/cm^3",
                                                                      include_add_diag, read_nd);
    path = "/lim_uv3_bro[]/main_species[,0]/add_diag[0..n]";
    harp_variable_definition_add_mapping(variable_definition, NULL, condition_add_diag, path, NULL);

    /* BrO_number_density_uncertainty */
    description = "error on the BrO number density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "BrO_number_density_uncertainty",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "molec/cm^3",
                                                                      include_add_diag, read_nd_error);
    path = "/lim_uv3_bro[]/main_species[,0]/err_tang_vmr, /lim_uv3_bro[]/main_species[,0]/add_diag[0..n]";
    harp_variable_definition_add_mapping(variable_definition, NULL, condition_add_diag, path, error_mapping);

    /* BrO_number_density_apriori */
    description = "a priori BrO number density profile";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "BrO_number_density_apriori",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "molec/cm^3",
                                                                      include_add_diag, read_nd_apriori);
    path = "/lim_uv3_bro[]/main_species[,0]/add_diag[0..n]";
    harp_variable_definition_add_mapping(variable_definition, NULL, condition_add_diag, path, NULL);

    /* BrO_number_density_avk */
    description = "averaging kernel on the BrO number density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "BrO_number_density_avk",
                                                                      harp_type_double, 3, dimension_type, NULL,
                                                                      description, "(molec/cm^3)/(molec/cm^3)",
                                                                      include_add_diag, read_nd_avk);
    path = "/lim_uv3_bro[]/main_species[,0]/add_diag[0..n]";
    harp_variable_definition_add_mapping(variable_definition, NULL, condition_add_diag, path, nd_avk_mapping);

    /*** clouds_aerosol ***/
    description = "clouds and aerosol data";
    product_definition = harp_ingestion_register_product(module, "SCIAMACHY_L2_CLOUDS_AEROSOL", description,
                                                         read_dimensions);
    harp_product_definition_add_mapping(product_definition, NULL, "dataset=clouds_aerosol");

    register_common_nadir_variables(product_definition, "clouds_aerosol");

    register_common_nadir_cloud_variables(product_definition);

    /* cloud_top_pressure */
    description = "cloud top pressure";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "cloud_top_pressure",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, "hPa", include_cloud_top_pressure,
                                                                      read_cloud_top_pressure);
    path = "/clouds_aerosol[]/cl_top_pres";
    harp_variable_definition_add_mapping(variable_definition, NULL, condition_3j, path, NULL);

    /* cloud_top_height */
    description = "cloud top height";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "cloud_top_height",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, "km", include_cloud_top_height,
                                                                      read_cloud_top_height);
    path = "/clouds_aerosol[]/cl_top_height";
    harp_variable_definition_add_mapping(variable_definition, NULL, condition_3k, path, NULL);

    /* absorbing_aerosol_index */
    description = "absorbing aerosol index";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "absorbing_aerosol_index",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                                      read_absorbing_aerosol_index);
    path = "/clouds_aerosol[]/aero_abso_ind";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);


    return 0;
}
