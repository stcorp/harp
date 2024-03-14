/*
 * Copyright (C) 2015-2024 S[&]T, The Netherlands.
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

#define CHECKED_MALLOC(v, s) v = malloc(s); if (v == NULL) { harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)", s, __FILE__, __LINE__); return -1; }

#define MAX_PIXELS        8192

/* -------------------------- typedefs ------------------------------------ */

typedef enum variable_type_enum
{
    IS_NO_ARRAY,
    USE_ARRAY_INDEX_0,
    USE_ARRAY_INDEX_1,
    USE_ARRAY_INDEX_0_3
} variable_type;

typedef enum ingestion_data_type_enum
{
    DATA_NADIR,
    DATA_LIMB,
    DATA_OCCULTATION,
    DATA_SUN_REFERENCE
} ingestion_data_type;

typedef struct ingest_info_struct
{
    coda_product *product;
    int format_version;
    ingestion_data_type ingestion_data; /* NADIR, LIMB, OCCULTATION, SUN_REFERENCE */
    char *datasource;   /* "nadir", "limb", "occultation", "sun_reference" */
    uint8_t mds_type;

    /* Data about the whole ingested file */
    long total_num_observations;
    uint16_t total_num_wavelengths;
    long num_states_current_datasource;
    int8_t *cluster_filter;

    /* Data about each state */
    uint16_t *num_clusters_per_state;
    uint16_t *max_num_obs_per_state;
    double *min_integr_time_per_state;
    coda_cursor *datasource_cursors_with_max_obs_per_state;

    /* Data about each sun_reference spectrum */
    coda_cursor first_sun_reference_D_spectra_cursor;

    /* Buffers that are used during the ingestion */
    double *wavelengths;
} ingest_info;

/* ---------------------- global variables -------------------------------- */

/* --------------------------- code --------------------------------------- */

static void ingestion_done(void *user_data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->datasource != NULL)
    {
        free(info->datasource);
    }
    if (info->num_clusters_per_state != NULL)
    {
        free(info->num_clusters_per_state);
    }
    if (info->cluster_filter != NULL)
    {
        free(info->cluster_filter);
    }
    if (info->max_num_obs_per_state != NULL)
    {
        free(info->max_num_obs_per_state);
    }
    if (info->min_integr_time_per_state != NULL)
    {
        free(info->min_integr_time_per_state);
    }
    if (info->datasource_cursors_with_max_obs_per_state)
    {
        free(info->datasource_cursors_with_max_obs_per_state);
    }
    if (info->wavelengths != NULL)
    {
        free(info->wavelengths);
    }
    free(info);
}

/* Start of code for the ingestion of a nadir/limb/occultation spectrum */

static int get_datetime_start_data(ingest_info *info, double *double_data_array)
{
    coda_cursor cursor;
    double *double_data, dsr_time;
    long i, num_states, nr_state_current_datasource;
    uint16_t num_rep_geo, j;
    uint8_t mds_type;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&cursor, "states") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &num_states) != 0)
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
    nr_state_current_datasource = 0;
    for (i = 0; i < num_states; i++)
    {
        if (coda_cursor_goto_record_field_by_name(&cursor, "mds_type") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_read_uint8(&cursor, &mds_type) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        coda_cursor_goto_parent(&cursor);
        if (mds_type == info->mds_type)
        {
            /* Time in HARP = dsr_time + readout_nr * minimum_integration_time */
            if (coda_cursor_goto_record_field_by_name(&cursor, "dsr_time") != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            if (coda_cursor_read_double(&cursor, &dsr_time) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            coda_cursor_goto_parent(&cursor);
            if (coda_cursor_goto_record_field_by_name(&cursor, "num_rep_geo") != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            if (coda_cursor_read_uint16(&cursor, &num_rep_geo) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            coda_cursor_goto_parent(&cursor);
            for (j = 0; j < num_rep_geo; j++)
            {
                *double_data = dsr_time + j * info->min_integr_time_per_state[nr_state_current_datasource];
                double_data++;
            }
            nr_state_current_datasource++;
        }
        if (i < (num_states - 1))
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

static int get_datetime_length_data(ingest_info *info, double *double_data_array)
{
    coda_cursor cursor;
    double *double_data;
    long i, num_states, nr_state_current_datasource;
    uint16_t num_rep_geo, j;
    uint8_t mds_type;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&cursor, "states") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &num_states) != 0)
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
    nr_state_current_datasource = 0;
    for (i = 0; i < num_states; i++)
    {
        if (coda_cursor_goto_record_field_by_name(&cursor, "mds_type") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_read_uint8(&cursor, &mds_type) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        coda_cursor_goto_parent(&cursor);
        if (mds_type == info->mds_type)
        {
            /* datetime length = minimum_integration_time for readout */
            if (coda_cursor_goto_record_field_by_name(&cursor, "num_rep_geo") != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            if (coda_cursor_read_uint16(&cursor, &num_rep_geo) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            coda_cursor_goto_parent(&cursor);
            for (j = 0; j < num_rep_geo; j++)
            {
                *double_data = info->min_integr_time_per_state[nr_state_current_datasource];
                double_data++;
            }
            nr_state_current_datasource++;
        }
        if (i < (num_states - 1))
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

static int get_main_data(ingest_info *info, const char *datasetname, variable_type dataset_dim, const char *fieldname,
                         variable_type field_dim, double *double_data_array)
{
    coda_cursor cursor, save_geo_cursor, save_dataset_cursor;
    double *double_data;
    long i, j, k, l;
    long num_geo_records;
    long dataset_start_index, dataset_end_index;
    long field_start_index, field_end_index;

    double_data = double_data_array;
    /* This loop walks through those records in the datasource array that have the maximum number of observations for a state. */
    for (i = 0; i < info->num_states_current_datasource; i++)
    {
        cursor = info->datasource_cursors_with_max_obs_per_state[i];
        if (coda_cursor_goto_record_field_by_name(&cursor, "geo") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_get_num_elements(&cursor, &num_geo_records) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_goto_first_array_element(&cursor) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        /* This loop walks through the geo array */
        for (j = 0; j < num_geo_records; j++)
        {
            save_geo_cursor = cursor;
            if (datasetname != NULL)
            {
                if (coda_cursor_goto_record_field_by_name(&cursor, datasetname) != 0)
                {
                    harp_set_error(HARP_ERROR_CODA, NULL);
                    return -1;
                }
            }
            dataset_start_index = dataset_end_index = 0;
            switch (dataset_dim)
            {
                case IS_NO_ARRAY:
                case USE_ARRAY_INDEX_0:
                    break;

                case USE_ARRAY_INDEX_1:
                    dataset_start_index = dataset_end_index = 1;
                    break;

                case USE_ARRAY_INDEX_0_3:
                    dataset_start_index = 0;
                    dataset_end_index = 3;
                    break;
            }
            /* This loop walks through the dataset array (like nadir[]/geo[]/corner_coord[]) */
            for (k = dataset_start_index; k <= dataset_end_index; k++)
            {
                save_dataset_cursor = cursor;
                if (dataset_dim != IS_NO_ARRAY)
                {
                    if (coda_cursor_goto_array_element_by_index(&cursor, k) != 0)
                    {
                        harp_set_error(HARP_ERROR_CODA, NULL);
                        return -1;
                    }
                }
                if (coda_cursor_goto_record_field_by_name(&cursor, fieldname) != 0)
                {
                    harp_set_error(HARP_ERROR_CODA, NULL);
                    return -1;
                }
                field_start_index = field_end_index = 0;
                switch (field_dim)
                {
                    case IS_NO_ARRAY:
                    case USE_ARRAY_INDEX_0:
                        break;

                    case USE_ARRAY_INDEX_1:
                        field_start_index = field_end_index = 1;
                        break;

                    case USE_ARRAY_INDEX_0_3:
                        field_start_index = 0;
                        field_end_index = 3;
                        break;
                }
                for (l = field_start_index; l <= field_end_index; l++)
                {
                    if (field_dim != IS_NO_ARRAY)
                    {
                        if (coda_cursor_goto_array_element_by_index(&cursor, l) != 0)
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
                    double_data++;
                }
                cursor = save_dataset_cursor;
            }
            cursor = save_geo_cursor;
            if (j < (num_geo_records - 1))
            {
                if (coda_cursor_goto_next_array_element(&cursor) != 0)
                {
                    harp_set_error(HARP_ERROR_CODA, NULL);
                    return -1;
                }
            }
        }
    }
    return 0;
}

static int get_spectral_data(ingest_info *info, double *double_data_array)
{
    coda_cursor cursor;
    double *double_data, *double_data_start_state, *double_data_start_cluster;
    long i, j, k, l, num_wavelengths, num_obs, num_copies, dim[CODA_MAX_NUM_DIMS];
    int num_dims;
    int8_t rad_units_flag;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&cursor, info->datasource) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_first_array_element(&cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    double_data_start_state = double_data_array;
    for (i = 0; i < info->num_states_current_datasource; i++)
    {
        double_data_start_cluster = double_data_start_state;
        for (j = 0; j < info->num_clusters_per_state[i]; j++)
        {
            if (info->cluster_filter[j] != -1)
            {
                continue;
            }
            if (coda_cursor_goto_record_field_by_name(&cursor, "rad_units_flag") != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            if (coda_cursor_read_int8(&cursor, &rad_units_flag) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            coda_cursor_goto_parent(&cursor);
            if (rad_units_flag != -1)
            {
                harp_set_error(HARP_ERROR_INGESTION,
                               "product contains both data in radiance units and data in binary units");
                return -1;
            }

            if (coda_cursor_goto_record_field_by_name(&cursor, "observations") != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            if (coda_cursor_get_array_dim(&cursor, &num_dims, dim) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            num_wavelengths = dim[1];
            num_obs = dim[0];
            num_copies = info->max_num_obs_per_state[i] / num_obs;
            double_data = double_data_start_cluster;
            for (k = 0; k < num_obs; k++)
            {
                if (coda_cursor_read_double_partial_array(&cursor, k * num_wavelengths, num_wavelengths, double_data) !=
                    0)
                {
                    harp_set_error(HARP_ERROR_CODA, NULL);
                    return -1;
                }
                for (l = 1; l < num_copies; l++)
                {
                    memcpy(double_data + (l * info->total_num_wavelengths), double_data,
                           num_wavelengths * sizeof(double));
                }
                double_data += info->total_num_wavelengths * num_copies;
            }
            coda_cursor_goto_parent(&cursor);
            double_data_start_cluster += num_wavelengths;

            if ((j < (info->num_clusters_per_state[i] - 1)) || (i < (info->num_states_current_datasource)))
            {
                if (coda_cursor_goto_next_array_element(&cursor) != 0)
                {
                    harp_set_error(HARP_ERROR_CODA, NULL);
                    return -1;
                }
            }
        }
        double_data_start_state += (info->total_num_wavelengths * info->max_num_obs_per_state[i]);
    }
    return 0;
}

static int get_wavelength_data(ingest_info *info, double *double_data_array)
{
    coda_cursor cursor;
    double *wavelengths, *double_data;
    long i, j, num_wavelengths;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&cursor, info->datasource) != 0)
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
    for (i = 0; i < info->num_states_current_datasource; i++)
    {
        wavelengths = info->wavelengths;
        for (j = 0; j < info->num_clusters_per_state[i]; j++)
        {
            if (info->cluster_filter[j] != -1)
            {
                continue;
            }
            if (coda_cursor_goto_record_field_by_name(&cursor, "pixel_wavelength") != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            if (coda_cursor_get_num_elements(&cursor, &num_wavelengths) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            if (coda_cursor_read_double_array(&cursor, wavelengths, coda_array_ordering_c) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            coda_cursor_goto_parent(&cursor);
            wavelengths += num_wavelengths;

            if ((j < (info->num_clusters_per_state[i] - 1)) || (i < (info->num_states_current_datasource)))
            {
                if (coda_cursor_goto_next_array_element(&cursor) != 0)
                {
                    harp_set_error(HARP_ERROR_CODA, NULL);
                    return -1;
                }
            }
        }
        for (j = 0; j < info->max_num_obs_per_state[i]; j++)
        {
            memcpy(double_data, info->wavelengths, info->total_num_wavelengths * sizeof(double));
            double_data += info->total_num_wavelengths;
        }
    }
    return 0;
}

static int get_integration_time(ingest_info *info, double *double_data_array)
{
    coda_cursor cursor, state_cursor;
    double *double_data, *double_data_start_state, *double_data_start_cluster;
    double intgr_time;
    long num_states, num_states_current_datasource, i, j, k, l;
    uint16_t clus_len;
    uint8_t mds_type;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&cursor, "states") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &num_states) != 0)
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
    num_states_current_datasource = 0;
    for (i = 0; i < num_states; i++)
    {
        state_cursor = cursor;
        if (coda_cursor_goto_record_field_by_name(&cursor, "mds_type") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_read_uint8(&cursor, &mds_type) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        coda_cursor_goto_parent(&cursor);
        if (mds_type == info->mds_type)
        {
            if (coda_cursor_goto_record_field_by_name(&cursor, "clus_config") != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            if (coda_cursor_goto_first_array_element(&cursor) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            double_data_start_state = double_data;
            for (j = 0; j < info->num_clusters_per_state[num_states_current_datasource]; j++)
            {
                if (info->cluster_filter[j] == -1)
                {
                    if (coda_cursor_goto_record_field_by_name(&cursor, "clus_len") != 0)
                    {
                        harp_set_error(HARP_ERROR_CODA, NULL);
                        return -1;
                    }
                    if (coda_cursor_read_uint16(&cursor, &clus_len) != 0)
                    {
                        harp_set_error(HARP_ERROR_CODA, NULL);
                        return -1;
                    }
                    coda_cursor_goto_parent(&cursor);
                    if (coda_cursor_goto_record_field_by_name(&cursor, "intgr_time") != 0)
                    {
                        harp_set_error(HARP_ERROR_CODA, NULL);
                        return -1;
                    }
                    if (coda_cursor_read_double(&cursor, &intgr_time) != 0)
                    {
                        harp_set_error(HARP_ERROR_CODA, NULL);
                        return -1;
                    }
                    coda_cursor_goto_parent(&cursor);

                    double_data_start_cluster = double_data;
                    for (k = 0; k < info->max_num_obs_per_state[num_states_current_datasource]; k++)
                    {
                        double_data = double_data_start_cluster + k * info->total_num_wavelengths;
                        for (l = 0; l < clus_len; l++)
                        {
                            *double_data = intgr_time;
                            double_data++;
                        }
                    }
                    double_data = double_data_start_cluster + clus_len;
                }
                if (j < (info->num_clusters_per_state[num_states_current_datasource] - 1))
                {
                    if (coda_cursor_goto_next_array_element(&cursor) != 0)
                    {
                        harp_set_error(HARP_ERROR_CODA, NULL);
                        return -1;
                    }
                }
            }
            double_data =
                double_data_start_state +
                info->max_num_obs_per_state[num_states_current_datasource] * info->total_num_wavelengths;
            num_states_current_datasource++;
        }
        cursor = state_cursor;
        if (i < (num_states - 1))
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

static int read_datetime_start(void *user_data, harp_array data)
{
    return get_datetime_start_data((ingest_info *)user_data, data.double_data);
}

static int read_datetime_length(void *user_data, harp_array data)
{
    return get_datetime_length_data((ingest_info *)user_data, data.double_data);
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

static int read_altitude(void *user_data, harp_array data)
{
    return get_main_data((ingest_info *)user_data, NULL, IS_NO_ARRAY, "tan_h", USE_ARRAY_INDEX_0, data.double_data);
}

static int read_latitude(void *user_data, harp_array data)
{
    if (((ingest_info *)user_data)->ingestion_data == DATA_NADIR)
    {
        return get_main_data((ingest_info *)user_data, "center_coord", IS_NO_ARRAY, "latitude", IS_NO_ARRAY,
                             data.double_data);
    }
    else
    {
        return get_main_data((ingest_info *)user_data, "tang_ground_point", USE_ARRAY_INDEX_1, "latitude", IS_NO_ARRAY,
                             data.double_data);
    }
}

static int read_longitude(void *user_data, harp_array data)
{
    if (((ingest_info *)user_data)->ingestion_data == DATA_NADIR)
    {
        return get_main_data((ingest_info *)user_data, "center_coord", IS_NO_ARRAY, "longitude", IS_NO_ARRAY,
                             data.double_data);
    }
    else
    {
        return get_main_data((ingest_info *)user_data, "tang_ground_point", USE_ARRAY_INDEX_1, "longitude", IS_NO_ARRAY,
                             data.double_data);
    }
}

static int read_latitude_bounds(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long i;
    double *double_data, save_one;

    if (get_main_data(info, "corner_coord", USE_ARRAY_INDEX_0_3, "latitude", IS_NO_ARRAY, data.double_data) != 0)
    {
        return -1;
    }

    /* Rearrange the corners 0, 1, 2, 3 as 0, 2, 3, 1 */
    double_data = data.double_data;
    for (i = 0; i < info->total_num_observations; i++)
    {
        save_one = *(double_data + 1);
        *(double_data + 1) = *(double_data + 2);
        *(double_data + 2) = *(double_data + 3);
        *(double_data + 3) = save_one;
        double_data += 4;
    }
    return 0;
}

static int read_longitude_bounds(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long i;
    double *double_data, save_one;

    if (get_main_data(info, "corner_coord", USE_ARRAY_INDEX_0_3, "longitude", IS_NO_ARRAY, data.double_data) != 0)
    {
        return -1;
    }

    /* Rearrange the corners 0, 1, 2, 3 as 0, 2, 3, 1 */
    double_data = data.double_data;
    for (i = 0; i < info->total_num_observations; i++)
    {
        save_one = *(double_data + 1);
        *(double_data + 1) = *(double_data + 2);
        *(double_data + 2) = *(double_data + 3);
        *(double_data + 3) = save_one;
        double_data += 4;
    }
    return 0;
}

static int read_sensor_altitude(void *user_data, harp_array data)
{
    return get_main_data((ingest_info *)user_data, NULL, IS_NO_ARRAY, "sat_h", IS_NO_ARRAY, data.double_data);
}

static int read_sensor_latitude(void *user_data, harp_array data)
{
    return get_main_data((ingest_info *)user_data, "sub_sat_point", IS_NO_ARRAY, "latitude", IS_NO_ARRAY,
                         data.double_data);
}

static int read_sensor_longitude(void *user_data, harp_array data)
{
    return get_main_data((ingest_info *)user_data, "sub_sat_point", IS_NO_ARRAY, "longitude", IS_NO_ARRAY,
                         data.double_data);
}

static int read_wavelength_photon_radiance(void *user_data, harp_array data)
{
    return get_spectral_data((ingest_info *)user_data, data.double_data);
}

static int read_wavelength(void *user_data, harp_array data)
{
    return get_wavelength_data((ingest_info *)user_data, data.double_data);
}

static int read_integration_time(void *user_data, harp_array data)
{
    return get_integration_time((ingest_info *)user_data, data.double_data);
}

static int read_solar_zenith_angle(void *user_data, harp_array data)
{
    return get_main_data((ingest_info *)user_data, NULL, IS_NO_ARRAY, "sol_zen_ang", USE_ARRAY_INDEX_1,
                         data.double_data);
}

static int read_solar_azimuth_angle(void *user_data, harp_array data)
{
    return get_main_data((ingest_info *)user_data, NULL, IS_NO_ARRAY, "sol_azi_ang", USE_ARRAY_INDEX_1,
                         data.double_data);
}

static int read_viewing_zenith_angle(void *user_data, harp_array data)
{
    return get_main_data((ingest_info *)user_data, NULL, IS_NO_ARRAY, "los_zen_ang", USE_ARRAY_INDEX_1,
                         data.double_data);
}

static int read_viewing_azimuth_angle(void *user_data, harp_array data)
{
    return get_main_data((ingest_info *)user_data, NULL, IS_NO_ARRAY, "los_azi_ang", USE_ARRAY_INDEX_1,
                         data.double_data);
}

static int read_scan_direction_type(void *user_data, long index, harp_array data)
{
    const double pi = 3.14159265358979;
    coda_cursor cursor;
    ingest_info *info = (ingest_info *)user_data;
    long state_nr, obs_nr, total_observations, i;
    double px, py, qx, qy, rx, ry, z;
    double latitude[3], longitude[3];

    /* Determine the state and the observation within the state for this scan_direction_type */
    total_observations = 0;
    for (state_nr = 0; state_nr < info->num_states_current_datasource; state_nr++)
    {
        if ((total_observations <= index) && ((total_observations + info->max_num_obs_per_state[state_nr]) > index))
        {
            obs_nr = (index - total_observations);
            break;
        }
        total_observations += info->max_num_obs_per_state[state_nr];
    }
    if (state_nr >= info->num_states_current_datasource)
    {
        harp_set_error(HARP_ERROR_INGESTION, "state index too large");
        return -1;
    }

    /* If minimum integration time for this state is > 1 second then all pixels for this     */
    /* state are mixed pixels. We keep a margin of 0.01 second to prevent rounding problems. */
    if (info->min_integr_time_per_state[state_nr] > 1.01)
    {
        *data.int8_data = 2;
        return 0;
    }

    cursor = info->datasource_cursors_with_max_obs_per_state[state_nr];
    if (coda_cursor_goto_record_field_by_name(&cursor, "geo") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_array_element_by_index(&cursor, obs_nr) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&cursor, "corner_coord") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_first_array_element(&cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    for (i = 0; i <= 2; i++)
    {
        if (coda_cursor_goto_record_field_by_name(&cursor, "latitude") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_read_double(&cursor, &latitude[i]) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        coda_cursor_goto_parent(&cursor);
        if (coda_cursor_goto_record_field_by_name(&cursor, "longitude") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_read_double(&cursor, &longitude[i]) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        coda_cursor_goto_parent(&cursor);
        if (coda_cursor_goto_next_array_element(&cursor) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
    }

    px = longitude[0] * pi / 180;
    py = latitude[0] * pi / 180;
    qx = longitude[1] * pi / 180;
    qy = latitude[1] * pi / 180;
    rx = longitude[2] * pi / 180;
    ry = latitude[2] * pi / 180;

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

    return 0;
}

static int include_nadir(void *user_data)
{
    return (((ingest_info *)user_data)->ingestion_data == DATA_NADIR);
}

static int include_limb_or_occultation(void *user_data)
{
    return ((((ingest_info *)user_data)->ingestion_data == DATA_LIMB) ||
            (((ingest_info *)user_data)->ingestion_data == DATA_OCCULTATION));
}

static int read_dimensions(void *user_data, long dimension[HARP_NUM_DIM_TYPES])
{
    dimension[harp_dimension_time] = ((ingest_info *)user_data)->total_num_observations;
    dimension[harp_dimension_spectral] = ((ingest_info *)user_data)->total_num_wavelengths;
    return 0;
}

static int init_nadir_limb_occultation_dimensions(ingest_info *info)
{
    coda_cursor cursor, product_cursor;
    double intgr_time;
    long datasource_index, num_states_all_datasources, i, j;
    int available;
    uint16_t num_clus, num_obs, max_num_obs, num_pixels, total_num_wavelengths;
    uint8_t mds_type;
    int8_t rad_units_flag;
    char *cluster_flag_name;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    product_cursor = cursor;

    /* Check if the datasource (nadir, limb, occultation) array is available */
    if (coda_cursor_get_record_field_index_from_name(&cursor, info->datasource, &datasource_index) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_record_field_available_status(&cursor, datasource_index, &available) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (!available)
    {
        harp_set_error(HARP_ERROR_INGESTION, "file does not contain %s data", info->datasource);
        return -1;
    }

    /* Check if the datasource contains radiance units */
    if (coda_cursor_goto_record_field_by_name(&cursor, info->datasource) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_first_array_element(&cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&cursor, "rad_units_flag") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_int8(&cursor, &rad_units_flag) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (rad_units_flag != -1)
    {
        harp_set_error(HARP_ERROR_INGESTION,
                       "product contains data in binary units, this ingestion is not supported in HARP");
        return -1;
    }
    cursor = product_cursor;

    /* Read the cluster filter array */
    CHECKED_MALLOC(info->cluster_filter, 64 * sizeof(int8_t));
    if (coda_cursor_goto_record_field_by_name(&cursor, "cal_options") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_first_array_element(&cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    switch (info->ingestion_data)
    {
        case DATA_NADIR:
            cluster_flag_name = "nadir_cluster_flag";
            break;

        case DATA_LIMB:
            cluster_flag_name = "limb_cluster_flag";
            break;

        case DATA_OCCULTATION:
            cluster_flag_name = "occ_cluster_flag";
            break;

        default:
            cluster_flag_name = "";
            break;
    }
    if (coda_cursor_goto_record_field_by_name(&cursor, cluster_flag_name) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_int8_array(&cursor, info->cluster_filter, coda_array_ordering_c) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    coda_cursor_goto_parent(&cursor);
    cursor = product_cursor;

    if (coda_cursor_goto_record_field_by_name(&cursor, "states") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &num_states_all_datasources) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    /* These allocations are too large but by using some extra memory here we prevent having */
    /* having to go through the states again just to count the num_states_all_datasources.   */
    CHECKED_MALLOC(info->num_clusters_per_state, num_states_all_datasources * sizeof(uint16_t));
    CHECKED_MALLOC(info->min_integr_time_per_state, num_states_all_datasources * sizeof(double));
    if (coda_cursor_goto_first_array_element(&cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    info->num_states_current_datasource = 0;
    for (i = 0; i < num_states_all_datasources; i++)
    {
        if (coda_cursor_goto_record_field_by_name(&cursor, "mds_type") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_read_uint8(&cursor, &mds_type) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        coda_cursor_goto_parent(&cursor);
        if (mds_type == info->mds_type)
        {
            /* Determine number of clusters of each state */
            if (coda_cursor_goto_record_field_by_name(&cursor, "num_clus") != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            if (coda_cursor_read_uint16(&cursor, &num_clus) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            coda_cursor_goto_parent(&cursor);
            info->num_clusters_per_state[info->num_states_current_datasource] = num_clus;

            /* Determine minimum integration time of each state */
            if (coda_cursor_goto_record_field_by_name(&cursor, "clus_config") != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            if (coda_cursor_goto_first_array_element(&cursor) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            info->min_integr_time_per_state[info->num_states_current_datasource] = 100.0;
            for (j = 0; j < num_clus; j++)
            {
                if (info->cluster_filter[j] == -1)
                {
                    if (coda_cursor_goto_record_field_by_name(&cursor, "intgr_time") != 0)
                    {
                        harp_set_error(HARP_ERROR_CODA, NULL);
                        return -1;
                    }
                    if (coda_cursor_read_double(&cursor, &intgr_time) != 0)
                    {
                        harp_set_error(HARP_ERROR_CODA, NULL);
                        return -1;
                    }
                    coda_cursor_goto_parent(&cursor);
                    if (intgr_time < info->min_integr_time_per_state[info->num_states_current_datasource])
                    {
                        info->min_integr_time_per_state[info->num_states_current_datasource] = intgr_time;
                    }
                }
                if (j < (num_clus - 1))
                {
                    if (coda_cursor_goto_next_array_element(&cursor) != 0)
                    {
                        harp_set_error(HARP_ERROR_CODA, NULL);
                        return -1;
                    }
                }
            }
            coda_cursor_goto_parent(&cursor);
            coda_cursor_goto_parent(&cursor);

            info->num_states_current_datasource++;
        }
        if (i < (num_states_all_datasources - 1))
        {
            if (coda_cursor_goto_next_array_element(&cursor) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
        }
    }
    CHECKED_MALLOC(info->datasource_cursors_with_max_obs_per_state,
                   info->num_states_current_datasource * sizeof(coda_cursor));
    CHECKED_MALLOC(info->max_num_obs_per_state, info->num_states_current_datasource * sizeof(uint16_t));
    cursor = product_cursor;
    if (coda_cursor_goto_record_field_by_name(&cursor, info->datasource) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_first_array_element(&cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    info->total_num_observations = 0;
    info->total_num_wavelengths = 0;
    for (i = 0; i < info->num_states_current_datasource; i++)
    {
        max_num_obs = 0;
        total_num_wavelengths = 0;
        for (j = 0; j < info->num_clusters_per_state[i]; j++)
        {
            if (info->cluster_filter[j] != -1)
            {
                continue;
            }
            /* Determine the maximum number of observations for this state   */
            /* and the datasource-record where that maximum number is found. */
            if (coda_cursor_goto_record_field_by_name(&cursor, "num_obs") != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            if (coda_cursor_read_uint16(&cursor, &num_obs) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            coda_cursor_goto_parent(&cursor);
            if (num_obs > max_num_obs)
            {
                info->datasource_cursors_with_max_obs_per_state[i] = cursor;
                max_num_obs = num_obs;
            }

            /* Determine the total number of pixels for this state */
            if (coda_cursor_goto_record_field_by_name(&cursor, "num_pixels") != 0)
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
            total_num_wavelengths += num_pixels;

            if ((j < (info->num_clusters_per_state[i] - 1)) || (i < (info->num_states_current_datasource)))
            {
                if (coda_cursor_goto_next_array_element(&cursor) != 0)
                {
                    harp_set_error(HARP_ERROR_CODA, NULL);
                    return -1;
                }
            }
        }
        info->max_num_obs_per_state[i] = max_num_obs;
        info->total_num_observations += max_num_obs;
        if (total_num_wavelengths > info->total_num_wavelengths)
        {
            info->total_num_wavelengths = total_num_wavelengths;
        }
    }
    CHECKED_MALLOC(info->wavelengths, info->total_num_wavelengths * sizeof(double));
    return 0;
}

static void register_nadir_limb_occultation_product(harp_ingestion_module *module)
{
    const char *scan_direction_type_values[] = { "forward", "backward", "mixed" };
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2];
    harp_dimension_type bounds_dimension_type[2];
    long bounds_dimension[2] = { -1, 4 };
    const char *description;
    const char *path;

    product_definition = harp_ingestion_register_product(module, "SCIAMACHY_L1c", "SCIAMACHY Level 1c",
                                                         read_dimensions);
    harp_product_definition_add_mapping(product_definition, NULL, "data=nadir or data=limb or data=occultation");

    dimension_type[0] = harp_dimension_time;
    dimension_type[1] = harp_dimension_spectral;

    bounds_dimension_type[0] = harp_dimension_time;
    bounds_dimension_type[1] = harp_dimension_independent;

    /* datetime_start */
    description = "start time of the measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime_start", harp_type_double, 1,
                                                   dimension_type, NULL, description, "seconds since 2000-01-01", NULL,
                                                   read_datetime_start);
    path = "/states/dsr_time";
    description =
        "the dsr_time is increased by the number of the applicable readout multiplied by the minimum integration time";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* datetime_length */
    description = "shortest integration time of all measurements at this time";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime_length", harp_type_double, 1,
                                                   dimension_type, NULL, description, "s", NULL, read_datetime_length);
    path = "/states[]/clus_config[]/intgr_time";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* orbit_index */
    description = "absolute orbit number";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "orbit_index", harp_type_int32, 0, NULL, NULL,
                                                   description, NULL, NULL, read_orbit_index);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/mph/abs_orbit", NULL);

    /* altitude */
    description = "tangent altitude for each measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "altitude", harp_type_double, 1, dimension_type,
                                                   NULL, description, "km", include_limb_or_occultation, read_altitude);
    description = "dsr is the dsr for the cluster with an integration time equal to the minimal integration time";
    path = "/limb[]/geo[]/tan_h[1]";
    harp_variable_definition_add_mapping(variable_definition, "data=limb", NULL, path, description);
    path = "/occultation[]/geo[]/tan_h[1]";
    harp_variable_definition_add_mapping(variable_definition, "data=occultation", NULL, path, description);

    /* latitude */
    description = "center latitude for each pixel";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_double, 1, dimension_type,
                                                   NULL, description, "degree_north", NULL, read_latitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    description = "dsr is the dsr for the cluster with an integration time equal to the minimal integration time of all"
        " ingested clusters for that state";
    path = "/nadir[]/geo[]/center_coord/latitude";
    harp_variable_definition_add_mapping(variable_definition, "data=nadir or data unset", NULL, path, description);
    path = "/limb[]/geo[]/tang_ground_point[1]/latitude";
    harp_variable_definition_add_mapping(variable_definition, "data=limb", NULL, path, description);
    path = "/occultation[]/geo[]/tang_ground_point[1]/latitude";
    harp_variable_definition_add_mapping(variable_definition, "data=occultation", NULL, path, description);

    /* longitude */
    description = "center longitude for each pixel";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_double, 1, dimension_type,
                                                   NULL, description, "degree_east", NULL, read_longitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    description = "dsr is the dsr for the cluster with an integration time equal to the minimal integration time of all"
        " ingested clusters for that state";
    path = "/nadir[]/geo[]/center_coord/longitude";
    harp_variable_definition_add_mapping(variable_definition, "data=nadir or data unset", NULL, path, description);
    path = "/limb[]/geo[]/tang_ground_point[1]/longitude";
    harp_variable_definition_add_mapping(variable_definition, "data=limb", NULL, path, description);
    path = "/occultation[]/geo[]/tang_ground_point[1]/longitude";
    harp_variable_definition_add_mapping(variable_definition, "data=occultation", NULL, path, description);

    /* latitude_bounds */
    description = "corner latitudes for each nadir pixel";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "latitude_bounds", harp_type_double, 2,
                                                   bounds_dimension_type, bounds_dimension, description, "degree_north",
                                                   include_nadir, read_latitude_bounds);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    path = "/nadir[]/geo[]/corner_coord[]/latitude";
    description = "dsr is the dsr for the cluster with an integration time equal to the minimal integration time of all"
        " ingested clusters for that state. The corners are rearranged in the following way: 0,2,3,1";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* longitude_bounds */
    description = "corner longitudes for each nadir pixel";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "longitude_bounds", harp_type_double, 2,
                                                   bounds_dimension_type, bounds_dimension, description, "degree_east",
                                                   include_nadir, read_longitude_bounds);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    path = "/nadir[]/geo[]/corner_coord[]/longitude";
    description = "dsr is the dsr for the cluster with an integration time equal to the minimal integration time of all"
        " ingested clusters for that state. The corners are rearranged in the following way: 0,2,3,1";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* sensor_altitude */
    description = "satellite altitude for each pixel";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "sensor_altitude", harp_type_double, 1,
                                                   dimension_type, NULL, description, "km", NULL, read_sensor_altitude);
    description = "dsr is the dsr for the cluster with an integration time equal to the minimal integration time of all"
        " ingested clusters for that state";
    path = "/nadir[]/geo[]/sat_h";
    harp_variable_definition_add_mapping(variable_definition, "data=nadir or data unset", NULL, path, description);
    path = "/limb[]/geo[]/sat_h";
    harp_variable_definition_add_mapping(variable_definition, "data=limb", NULL, path, description);
    path = "/occultation[]/geo[]/sat_h";
    harp_variable_definition_add_mapping(variable_definition, "data=occultation", NULL, path, description);

    /* sensor_latitude */
    description = "satellite latitude for each pixel";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "sensor_latitude", harp_type_double, 1,
                                                   dimension_type, NULL, description, "degree_north",
                                                   NULL, read_sensor_latitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    description = "dsr is the dsr for the cluster with an integration time equal to the minimal integration time of all"
        " ingested clusters for that state";
    path = "/nadir[]/geo[]/sub_sat_point/latitude";
    harp_variable_definition_add_mapping(variable_definition, "data=nadir or data unset", NULL, path, description);
    path = "/limb[]/geo[]/sub_sat_point/latitude";
    harp_variable_definition_add_mapping(variable_definition, "data=limb", NULL, path, description);
    path = "/occultation[]/geo[]/sub_sat_point/latitude";
    harp_variable_definition_add_mapping(variable_definition, "data=occultation", NULL, path, description);

    /* sensor_longitude */
    description = "satellite longitude for each pixel";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "sensor_longitude", harp_type_double, 1,
                                                   dimension_type, NULL, description, "degree_east",
                                                   NULL, read_sensor_longitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    description = "dsr is the dsr for the cluster with an integration time equal to the minimal integration time of all"
        " ingested clusters for that state";
    path = "/nadir[]/geo[]/sub_sat_point/longitude";
    harp_variable_definition_add_mapping(variable_definition, "data=nadir or data unset", NULL, path, description);
    path = "/limb[]/geo[]/sub_sat_point/longitude";
    harp_variable_definition_add_mapping(variable_definition, "data=limb", NULL, path, description);
    path = "/occultation[]/geo[]/sub_sat_point/longitude";
    harp_variable_definition_add_mapping(variable_definition, "data=occultation", NULL, path, description);

    /* wavelength_photon_radiance */
    description = "wavelength photon radiance of each spectrum measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "wavelength_photon_radiance",
                                                   harp_type_double, 2, dimension_type, NULL, description,
                                                   "(count/s/cm2/sr/nm)", NULL, read_wavelength_photon_radiance);
    path = "/nadir[]/observations[]";
    harp_variable_definition_add_mapping(variable_definition, "data=nadir or data unset", NULL, path, NULL);
    path = "/limb[]/observations[]";
    harp_variable_definition_add_mapping(variable_definition, "data=limb", NULL, path, NULL);
    path = "/occultation[]/observations[]";
    harp_variable_definition_add_mapping(variable_definition, "data=occultation", NULL, path, NULL);

    /* wavelength_of_each_spectrum_measurement */
    description = "nominal wavelength assignment for each of the detector pixels";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "wavelength", harp_type_double, 2,
                                                   dimension_type, NULL, description, "nm", NULL, read_wavelength);
    path = "nadir[]/pixel_wavelength[]";
    harp_variable_definition_add_mapping(variable_definition, "data=nadir or data unset", NULL, path, NULL);
    path = "limb[]/pixel_wavelength[]";
    harp_variable_definition_add_mapping(variable_definition, "data=limb", NULL, path, NULL);
    path = "occultation[]/pixel_wavelength[]";
    harp_variable_definition_add_mapping(variable_definition, "data=occultation", NULL, path, NULL);

    /* integration_time */
    description = "integration time for a readout";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "integration_time", harp_type_double, 2,
                                                   dimension_type, NULL, description, "s", NULL, read_integration_time);
    path = "/states[]/clus_config[]/intgr_time";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* solar_zenith_angle */
    description = "solar zenith angle for each measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "solar_zenith_angle", harp_type_double, 1,
                                                   dimension_type, NULL, description, "degree",
                                                   NULL, read_solar_zenith_angle);
    path = "/nadir[]/geo[]/sol_zen_ang[1]";
    harp_variable_definition_add_mapping(variable_definition, "data=nadir or data unset", NULL, path, NULL);
    path = "/limb[]/geo[]/sol_zen_ang[1]";
    harp_variable_definition_add_mapping(variable_definition, "data=limb", NULL, path, NULL);
    path = "/occultation[]/geo[]/sol_zen_ang[1]";
    harp_variable_definition_add_mapping(variable_definition, "data=occultation", NULL, path, NULL);

    /* solar_azimuth_angle */
    description = "solar azimuth angle for each measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "solar_azimuth_angle", harp_type_double, 1,
                                                   dimension_type, NULL, description, "degree",
                                                   NULL, read_solar_azimuth_angle);
    path = "/nadir[]/geo[]/sol_azi_ang[1]";
    harp_variable_definition_add_mapping(variable_definition, "data=nadir or data unset", NULL, path, NULL);
    path = "/limb[]/geo[]/sol_azi_ang[1]";
    harp_variable_definition_add_mapping(variable_definition, "data=limb", NULL, path, NULL);
    path = "/occultation[]/geo[]/sol_azi_ang[1]";
    harp_variable_definition_add_mapping(variable_definition, "data=occultation", NULL, path, NULL);

    /* viewing_zenith_angle */
    description = "viewing zenith angle for each measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "viewing_zenith_angle", harp_type_double, 1,
                                                   dimension_type, NULL, description, "degree",
                                                   NULL, read_viewing_zenith_angle);
    path = "/nadir[]/geo[]/los_zen_ang[1]";
    harp_variable_definition_add_mapping(variable_definition, "data=nadir or data unset", NULL, path, NULL);
    path = "/limb[]/geo[]/los_zen_ang[1]";
    harp_variable_definition_add_mapping(variable_definition, "data=limb", NULL, path, NULL);
    path = "/occultation[]/geo[]/los_zen_ang[1]";
    harp_variable_definition_add_mapping(variable_definition, "data=occultation", NULL, path, NULL);

    /* viewing_azimuth_angle */
    description = "viewing azimuth angle for each measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "viewing_azimuth_angle", harp_type_double, 1,
                                                   dimension_type, NULL, description, "degree",
                                                   NULL, read_viewing_azimuth_angle);
    path = "/nadir[]/geo[]/los_azi_ang[1]";
    harp_variable_definition_add_mapping(variable_definition, "data=nadir or data unset", NULL, path, NULL);
    path = "/limb[]/geo[]/los_azi_ang[1]";
    harp_variable_definition_add_mapping(variable_definition, "data=limb", NULL, path, NULL);
    path = "/occultation[]/geo[]/los_azi_ang[1]";
    harp_variable_definition_add_mapping(variable_definition, "data=occultation", NULL, path, NULL);

    /* scan_direction_type */
    description = "scan direction for each measurement";
    variable_definition =
        harp_ingestion_register_variable_block_read(product_definition, "scan_direction_type", harp_type_int8, 1,
                                                    dimension_type, NULL, description, NULL,
                                                    include_nadir, read_scan_direction_type);
    harp_variable_definition_set_enumeration_values(variable_definition, 3, scan_direction_type_values);
    path = "/nadir[]/geo[]/corner_coord[], /states[]/intg_times[]";
    description =
        "when the minimum integration time of a state is higher than 1 second we are dealing with a mixed (2) pixel"
        "otherwise the scan direction is based on the corner coordinates of the first ground pixel of the measurement. "
        "The first geolocation pixel is a backscan (1) pixel if the inproduct of the unit vector of the third "
        "corner with the outproduct of the unit vector of the first corner and the unit vector of the second "
        "corner is negative (otherwise it is part of a forward (0) scan).";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);
}

/* Start of code for the ingestion of a sun_reference spectrum */

static int get_sun_reference_pixel_data(ingest_info *info, char *fieldname, double *double_data_array)
{
    coda_cursor cursor;

    cursor = info->first_sun_reference_D_spectra_cursor;
    if (coda_cursor_goto_record_field_by_name(&cursor, fieldname) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_double_array(&cursor, double_data_array, coda_array_ordering_c) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    return 0;
}

static int read_wavelength_photon_irradiance(void *user_data, harp_array data)
{
    return get_sun_reference_pixel_data((ingest_info *)user_data, "mean_ref_spec", data.double_data);
}

static int read_wavelength_photon_irradiance_uncertainty(void *user_data, harp_array data)
{
    double irradiance_values[MAX_PIXELS], uncertainty_percentages[MAX_PIXELS];
    double *double_data, *irradiance_value, *uncertainty_percentage;
    int i, retval;

    retval = get_sun_reference_pixel_data((ingest_info *)user_data, "mean_ref_spec", irradiance_values);
    if (retval != 0)
    {
        return -1;
    }
    retval =
        get_sun_reference_pixel_data((ingest_info *)user_data, "rad_pre_mean_sun_ref_spec", uncertainty_percentages);
    if (retval != 0)
    {
        return -1;
    }
    /* Calculate the uncertainty in absolute values */
    double_data = data.double_data;
    irradiance_value = &irradiance_values[0];
    uncertainty_percentage = &uncertainty_percentages[0];
    for (i = 0; i < MAX_PIXELS; i++)
    {
        *double_data = *irradiance_value * *uncertainty_percentage;
        double_data++;
        irradiance_value++;
        uncertainty_percentage++;
    }
    return 0;
}

static int read_sun_reference_wavelength(void *user_data, harp_array data)
{
    return get_sun_reference_pixel_data((ingest_info *)user_data, "wvlen_sun_meas", data.double_data);
}

static int init_sun_reference_dimensions(ingest_info *info)
{
    coda_cursor cursor;
    long datasource_index, num_sun_reference_records, i;
    int available;
    char sun_spect_id[3];

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    /* Check if the sun_reference array is available */
    if (coda_cursor_get_record_field_index_from_name(&cursor, "sun_reference", &datasource_index) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_record_field_available_status(&cursor, datasource_index, &available) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (!available)
    {
        harp_set_error(HARP_ERROR_INGESTION, "file does not contain %s data", info->datasource);
        return -1;
    }

    /* Cluster filtering is not used for the sun_reference */
    CHECKED_MALLOC(info->cluster_filter, 64 * sizeof(int8_t));
    memset(info->cluster_filter, -1, 64 * sizeof(int8_t));

    if (coda_cursor_goto_record_field_by_name(&cursor, "sun_reference") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &num_sun_reference_records) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_first_array_element(&cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    info->total_num_observations = 0;
    for (i = 0; i < num_sun_reference_records; i++)
    {
        if (coda_cursor_goto_record_field_by_name(&cursor, "sun_spect_id") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_read_string(&cursor, sun_spect_id, sizeof(sun_spect_id)) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        coda_cursor_goto_parent(&cursor);
        /* Check if this is a calibrated diffuser sun spectrum */
        if (sun_spect_id[0] == 'D')
        {
            /* We ingest only the first spectrum */
            info->first_sun_reference_D_spectra_cursor = cursor;
            info->total_num_observations = 1;
            break;
        }
        if (i < (num_sun_reference_records - 1))
        {
            if (coda_cursor_goto_next_array_element(&cursor) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
        }
    }

    info->total_num_wavelengths = MAX_PIXELS;
    return 0;
}

static void register_sun_reference_product(harp_ingestion_module *module)
{
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2];
    const char *description;
    const char *path;

    product_definition = harp_ingestion_register_product(module, "SCIAMACHY_L1c_sun_reference",
                                                         "SCIAMACHY Level 1c sun reference", read_dimensions);
    harp_product_definition_add_mapping(product_definition, NULL, "data=sun_reference");

    dimension_type[0] = harp_dimension_time;
    dimension_type[1] = harp_dimension_spectral;

    /* wavelength_photon_irradiance */
    description = "wavelength photon irradiance of each spectrum measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "wavelength_photon_irradiance",
                                                   harp_type_double, 2, dimension_type, NULL, description,
                                                   "(count/s/cm2/nm)", NULL, read_wavelength_photon_irradiance);
    path = "/sun_reference[]/mean_ref_spec[], /sun_reference[]/sun_spect_id";
    description =
        "only the first calibrated diffuser sun spectrum (i.e. whose sun_spect_id starts with 'D') is ingested";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* wavelength_photon_irradiance_uncertainty */
    description = "error in the wavelength photon radiance of each spectrum measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "wavelength_photon_irradiance_uncertainty",
                                                   harp_type_double, 2, dimension_type, NULL, description,
                                                   "(count/s/cm2/nm)", NULL,
                                                   read_wavelength_photon_irradiance_uncertainty);
    path = "/sun_reference[]/rad_pre_mean_sun_ref_spec[], /sun_reference[]/sun_spect_id";
    description =
        "only the first calibrated diffuser sun spectrum (i.e. whose sun_spect_id starts with 'D') is ingested";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* wavelength_of_each_spectrum_measurement */
    description = "nominal wavelength assignment for each of the detector pixels";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "wavelength", harp_type_double, 2,
                                                   dimension_type, NULL, description, "nm", NULL,
                                                   read_sun_reference_wavelength);
    path = "/sun_reference[]/wvlen_sun_meas[], /sun_reference[]/sun_spect_id";
    description =
        "only the first calibrated diffuser sun spectrum (i.e. whose sun_spect_id starts with 'D') is ingested";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);
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

    CHECKED_MALLOC(info, sizeof(ingest_info));
    memset(info, '\0', sizeof(ingest_info));
    info->product = product;
    info->format_version = format_version;

    info->ingestion_data = DATA_NADIR;
    info->datasource = strdup("nadir");
    info->mds_type = 1;
    if (harp_ingestion_options_has_option(options, "data"))
    {
        if (harp_ingestion_options_get_option(options, "data", &cp) == 0)
        {
            if (strcmp(cp, "nadir") == 0)
            {
                info->ingestion_data = DATA_NADIR;
                free(info->datasource);
                info->datasource = strdup("nadir");
                info->mds_type = 1;
            }
            else if (strcmp(cp, "limb") == 0)
            {
                info->ingestion_data = DATA_LIMB;
                free(info->datasource);
                info->datasource = strdup("limb");
                info->mds_type = 2;
            }
            else if (strcmp(cp, "occultation") == 0)
            {
                info->ingestion_data = DATA_OCCULTATION;
                free(info->datasource);
                info->datasource = strdup("occultation");
                info->mds_type = 3;
            }
            else if (strcmp(cp, "sun_reference") == 0)
            {
                info->ingestion_data = DATA_SUN_REFERENCE;
                free(info->datasource);
                info->datasource = strdup("sun_reference");
                info->mds_type = 0;
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
        if (init_nadir_limb_occultation_dimensions(info) != 0)
        {
            ingestion_done(info);
            return -1;
        }
        *definition = module->product_definition[0];
    }
    *user_data = info;

    return 0;
}

int harp_ingestion_module_sciamachy_l1_init(void)
{
    harp_ingestion_module *module;
    const char *description;
    const char *data_options[] = { "nadir", "limb", "occultation", "sun_reference" };

    description = "SCIAMACHY Level 1c";
    module = harp_ingestion_register_module("SCIAMACHY_L1c", "SCIAMACHY", "ENVISAT_SCIAMACHY", "SCI_NLC_1P",
                                            description, ingestion_init, ingestion_done);

    harp_ingestion_register_option(module, "data",
                                   "retrieve the nadir, limb, occultation or sun_reference spectra; by default "
                                   "the nadir spectra are retrieved", 4, data_options);

    register_nadir_limb_occultation_product(module);
    register_sun_reference_product(module);

    return 0;
}
