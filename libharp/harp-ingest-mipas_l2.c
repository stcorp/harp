/*
 * Copyright (C) 2015-2022 S[&]T, The Netherlands.
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
#include <string.h>

#define NUM_SPECIES_TYPES 15

typedef enum species_type_enum
{
    species_h2o,
    species_o3,
    species_hno3,
    species_ch4,
    species_n2o,
    species_no2,
    species_f11,
    species_clno,
    species_n2o5,
    species_f12,
    species_cof2,
    species_ccl4,
    species_hcn,
    species_f14,
    species_f22
} species_type;

const char *species_name[] = {
    "H2O", "O3", "HNO3", "CH4", "N2O", "NO2", "F11", "ClNO", "N2O5", "F12", "COF2", "CCL4", "HCN", "F14", "F22"
};

const char *species_mds_name[] = {
    "h2o_retrieval_mds",
    "o3_retrieval_mds",
    "hno3_retrieval_mds",
    "ch4_retrieval_mds",
    "n2o_retrieval_mds",
    "no2_retrieval_mds",
    "f11_retrieval_mds",
    "clno_retrieval_mds",
    "n2o5_retrieval_mds",
    "f12_retrieval_mds",
    "cof2_retrieval_mds",
    "ccl4_retrieval_mds",
    "hcn_retrieval_mds",
    "f14_retrieval_mds",
    "f22_retrieval_mds"
};

typedef struct ingest_info_struct
{
    coda_product *product;
    int product_version;
    long num_main;
    long *num_altitudes;
    long max_num_altitudes;
    int num_species;
    int selected_species;
    coda_cursor *scan_cursor;
    coda_cursor *geo_cursor;
    coda_cursor *pt_cursor;
    coda_cursor *mds_cursor[NUM_SPECIES_TYPES];
    int species_index[NUM_SPECIES_TYPES];       /* species index for lrv and scan information ads */
    uint8_t *lrv;       /* logical retrieval vector; dim=[num_main, {pT, sp#1, sp#2, .., sp#n}, max_num_altitudes] */
} ingest_info;

static void reverse_double_array(double *data, long num_elements)
{
    long bottom, top;

    for (bottom = 0, top = num_elements - 1; bottom < num_elements / 2; bottom++, top--)
    {
        double tmp = data[bottom];

        data[bottom] = data[top];
        data[top] = tmp;
    }
}

static void reverse_uint8_array(uint8_t *data, long num_elements)
{
    long bottom, top;

    for (bottom = 0, top = num_elements - 1; bottom < num_elements / 2; bottom++, top--)
    {
        uint8_t tmp = data[bottom];

        data[bottom] = data[top];
        data[top] = tmp;
    }
}

static int init_species_numbers(ingest_info *info)
{
    coda_cursor sph_cursor;
    char *order_of_species;
    char *head;
    char *tail;
    long length;
    int species_number;
    int i;

    if (coda_cursor_set_product(&sph_cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&sph_cursor, "sph") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&sph_cursor, "order_of_species") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_string_length(&sph_cursor, &length) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    order_of_species = (char *)malloc(length + 1);
    if (order_of_species == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       (long)length + 1, __FILE__, __LINE__);
        return -1;
    }
    if (coda_cursor_read_string(&sph_cursor, order_of_species, length + 1) != 0)
    {
        free(order_of_species);
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    while (length > 0 && order_of_species[length - 1] == ' ')
    {
        length--;
    }
    order_of_species[length] = '\0';

    species_number = 0;
    head = order_of_species;
    tail = order_of_species;
    while (*head != '\0')
    {
        while (*tail != '\0' && *tail != ',')
        {
            tail++;
        }
        if (*tail != '\0')
        {
            *tail = '\0';
            tail++;
        }

        if (strcasecmp(head, "H2O") == 0)
        {
            info->species_index[species_h2o] = species_number;
        }
        else if (strcasecmp(head, "O3") == 0)
        {
            info->species_index[species_o3] = species_number;
        }
        else if (strcasecmp(head, "HNO3") == 0)
        {
            info->species_index[species_hno3] = species_number;
        }
        else if (strcasecmp(head, "CH4") == 0)
        {
            info->species_index[species_ch4] = species_number;
        }
        else if (strcasecmp(head, "N2O") == 0)
        {
            info->species_index[species_n2o] = species_number;
        }
        else if (strcasecmp(head, "NO2") == 0)
        {
            info->species_index[species_no2] = species_number;
        }
        else if (strcasecmp(head, "F11") == 0)
        {
            info->species_index[species_f11] = species_number;
        }
        else if (strcasecmp(head, "CLNO") == 0)
        {
            info->species_index[species_clno] = species_number;
        }
        else if (strcasecmp(head, "N2O5") == 0)
        {
            info->species_index[species_n2o5] = species_number;
        }
        else if (strcasecmp(head, "F12") == 0)
        {
            info->species_index[species_f12] = species_number;
        }
        else if (strcasecmp(head, "COF2") == 0)
        {
            info->species_index[species_cof2] = species_number;
        }
        else if (strcasecmp(head, "CCL4") == 0)
        {
            info->species_index[species_ccl4] = species_number;
        }
        else if (strcasecmp(head, "HCN") == 0)
        {
            info->species_index[species_hcn] = species_number;
        }
        else if (strcasecmp(head, "F14") == 0)
        {
            info->species_index[species_f14] = species_number;
        }
        else if (strcasecmp(head, "F22") == 0)
        {
            info->species_index[species_f22] = species_number;
        }

        species_number++;
        head = tail;
    }
    free(order_of_species);
    if (species_number != info->num_species)
    {
        harp_set_error(HARP_ERROR_INGESTION, "number of species (%d) does not match expected number (%d)",
                       species_number, info->num_species);
        return -1;
    }
    for (i = species_h2o; i <= species_no2; i++)
    {
        if (info->species_index[i] < 0)
        {
            harp_set_error(HARP_ERROR_INGESTION, "missing %s in /sph/order_of_species", species_name[i]);
            return -1;
        }
    }
    if (info->product_version >= 3)
    {
        for (i = species_f11; i <= species_f12; i++)
        {
            if (info->species_index[i] < 0)
            {
                harp_set_error(HARP_ERROR_INGESTION, "missing %s in /sph/order_of_species", species_name[i]);
                return -1;
            }
        }
    }
    if (info->product_version >= 4)
    {
        for (i = species_cof2; i <= species_f22; i++)
        {
            if (info->species_index[i] < 0)
            {
                harp_set_error(HARP_ERROR_INGESTION, "missing %s in /sph/order_of_species", species_name[i]);
                return -1;
            }
        }
    }

    return 0;
}

static int init_profile_info(ingest_info *info)
{
    coda_cursor cursor;
    long num_elements;
    long i;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&cursor, "scan_information_mds") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &num_elements) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    info->num_main = num_elements;
    if (info->num_main == 0)
    {
        return 0;
    }
    info->max_num_altitudes = 0;
    info->num_altitudes = malloc(info->num_main * sizeof(long));
    if (info->num_altitudes == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       info->num_main * sizeof(long), __FILE__, __LINE__);
        return -1;
    }
    if (coda_cursor_goto_first_array_element(&cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    for (i = 0; i < num_elements; i++)
    {
        if (coda_cursor_goto_record_field_by_name(&cursor, "zpd_crossing_time") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_get_num_elements(&cursor, &info->num_altitudes[i]) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (info->num_altitudes[i] > info->max_num_altitudes)
        {
            info->max_num_altitudes = info->num_altitudes[i];
        }
        coda_cursor_goto_parent(&cursor);
        if (i < num_elements - 1)
        {
            if (coda_cursor_goto_next_array_element(&cursor) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
        }
    }
    if (info->max_num_altitudes == 0)
    {
        /* if the vertical axis length is 0 then there is no use trying to ingest anything -> set num_main to 0 */
        info->num_main = 0;
    }

    return 0;
}

static int init_cursors_for_dataset(const char *dataset_name, ingest_info *info, coda_cursor **ds_cursor)
{
    coda_cursor cursor;
    long i;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&cursor, dataset_name) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    *ds_cursor = malloc(info->num_main * sizeof(coda_cursor));
    if (*ds_cursor == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       info->num_main * sizeof(coda_cursor), __FILE__, __LINE__);
        return -1;
    }
    if (coda_cursor_goto_first_array_element(&cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    for (i = 0; i < info->num_main; i++)
    {
        (*ds_cursor)[i] = cursor;
        if (i < info->num_main - 1)
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

static int init_cursors(ingest_info *info)
{
    if (info->num_main > 0)
    {
        int i;

        if (init_cursors_for_dataset("scan_information_mds", info, &info->scan_cursor) != 0)
        {
            return -1;
        }
        if (init_cursors_for_dataset("scan_geolocation_ads", info, &info->geo_cursor) != 0)
        {
            return -1;
        }
        if (init_cursors_for_dataset("pt_retrieval_mds", info, &info->pt_cursor) != 0)
        {
            return -1;
        }
        for (i = 0; i < NUM_SPECIES_TYPES; i++)
        {
            if (info->species_index[i] >= 0)
            {
                if (init_cursors_for_dataset(species_mds_name[i], info, &info->mds_cursor[i]) != 0)
                {
                    return -1;
                }
            }
        }
    }
    return 0;
}

static int init_logical_retrieval_vector(ingest_info *info)
{
    long i;
    long offset;

    if (info->num_main <= 0)
    {
        return 0;
    }

    info->lrv = malloc(info->num_main * (info->num_species + 1) * info->max_num_altitudes * sizeof(uint8_t));
    if (info->lrv == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       info->num_main * (info->num_species + 1) * info->max_num_altitudes * sizeof(uint8_t), __FILE__,
                       __LINE__);
        return -1;
    }
    memset(info->lrv, 0, info->num_main * (info->num_species + 1) * info->max_num_altitudes);

    offset = 0;
    for (i = 0; i < info->num_main; i++)
    {
        coda_cursor cursor;
        int j;

        cursor = info->scan_cursor[i];
        if (coda_cursor_goto_record_field_by_name(&cursor, "retrieval_p_t") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_goto_record_field_by_name(&cursor, "lrv_p_t_flag") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_read_uint8_array(&cursor, &info->lrv[offset], coda_array_ordering_c) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        reverse_uint8_array(&info->lrv[offset], info->num_altitudes[i]);
        offset += info->max_num_altitudes;
        coda_cursor_goto_parent(&cursor);
        coda_cursor_goto_parent(&cursor);
        if (coda_cursor_goto_record_field_by_name(&cursor, "retrieval_vmr") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_goto_first_array_element(&cursor) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        for (j = 0; j < info->num_species; j++)
        {
            if (coda_cursor_goto_record_field_by_name(&cursor, "lrv_vmr_flag") != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            if (coda_cursor_read_uint8_array(&cursor, &info->lrv[offset], coda_array_ordering_c) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            reverse_uint8_array(&info->lrv[offset], info->num_altitudes[i]);
            offset += info->max_num_altitudes;
            coda_cursor_goto_parent(&cursor);
            if (j < info->num_species - 1)
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

static void ingestion_done(void *user_data)
{
    ingest_info *info = (ingest_info *)user_data;
    int i;

    if (info->num_altitudes != NULL)
    {
        free(info->num_altitudes);
    }
    if (info->scan_cursor != NULL)
    {
        free(info->scan_cursor);
    }
    if (info->geo_cursor != NULL)
    {
        free(info->geo_cursor);
    }
    if (info->pt_cursor != NULL)
    {
        free(info->pt_cursor);
    }
    for (i = 0; i < NUM_SPECIES_TYPES; i++)
    {
        if (info->mds_cursor[i] != NULL)
        {
            free(info->mds_cursor[i]);
        }
    }
    if (info->lrv != NULL)
    {
        free(info->lrv);
    }

    free(info);
}

static int ingestion_init(const harp_ingestion_module *module, coda_product *product,
                          const harp_ingestion_options *options, harp_product_definition **definition, void **user_data)
{
    const char *option_value;
    ingest_info *info;
    int i;

    (void)options;

    info = malloc(sizeof(ingest_info));
    if (info == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(ingest_info), __FILE__, __LINE__);
        return -1;
    }
    info->product = product;
    info->product_version = -1;
    info->num_main = -1;
    info->num_altitudes = NULL;
    info->max_num_altitudes = 0;
    info->num_species = 0;
    info->selected_species = -1;
    info->scan_cursor = NULL;
    info->geo_cursor = NULL;
    info->pt_cursor = NULL;
    for (i = 0; i < NUM_SPECIES_TYPES; i++)
    {
        info->mds_cursor[i] = NULL;
        info->species_index[i] = -1;
    }
    info->lrv = NULL;

    if (harp_ingestion_options_get_option(options, "species", &option_value) == 0)
    {
        for (i = 0; i < NUM_SPECIES_TYPES; i++)
        {
            if (strcmp(option_value, species_name[i]) == 0)
            {
                info->selected_species = i;
                break;
            }
        }
    }

    if (coda_get_product_version(info->product, &info->product_version) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        ingestion_done(info);
        return -1;
    }
    if (info->product_version < 3)
    {
        info->num_species = 6;
    }
    else if (info->product_version < 4)
    {
        info->num_species = 10;
    }
    else
    {
        info->num_species = 15;
    }
    if (init_species_numbers(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }
    if (info->selected_species >= 0)
    {
        for (i = 0; i < NUM_SPECIES_TYPES; i++)
        {
            if (i != info->selected_species)
            {
                info->species_index[i] = -1;
            }
        }
    }
    if (init_profile_info(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }
    if (init_cursors(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }
    if (init_logical_retrieval_vector(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }

    *definition = *module->product_definition;
    *user_data = info;

    return 0;
}

static int get_akm_data(ingest_info *info, const coda_cursor *mds_cursor, long index, uint8_t *lrv, harp_array data)
{
    coda_cursor cursor;
    long num_elements;
    long num_i;
    long i, j;

    cursor = *mds_cursor;
    if (coda_cursor_goto_record_field_by_name(&cursor, "avg_kernel") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &num_elements) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    assert(num_elements <= (info->max_num_altitudes * info->max_num_altitudes));
    if (coda_cursor_read_double_array(&cursor, data.double_data, coda_array_ordering_c) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    reverse_double_array(data.double_data, num_elements);

    num_elements = (long)sqrt(num_elements);
    num_i = num_elements;
    for (i = info->max_num_altitudes - 1; i >= 0; i--)
    {
        /* we need to reverse the lrv index, because we already reversed 'data' with reverse_array() */
        if (lrv[i])
        {
            long num_j = num_elements;

            num_i--;
            assert(num_i >= 0);
            for (j = info->max_num_altitudes - 1; j >= 0; j--)
            {
                if (lrv[j])
                {
                    num_j--;
                    assert(num_j >= 0);
                    data.double_data[i * info->max_num_altitudes + j] = data.double_data[num_i * num_elements + num_j];
                }
                else
                {
                    data.double_data[i * info->max_num_altitudes + j] = harp_nan();
                }
            }
            assert(num_j == 0);
        }
        else
        {
            for (j = 0; j < info->max_num_altitudes; j++)
            {
                data.double_data[i * info->max_num_altitudes + j] = harp_nan();
            }
        }
    }
    assert(num_i == 0);

    if (info->selected_species != -1)
    {
        uint8_t *lrv_filter;

        lrv_filter = &info->lrv[(index * (info->num_species + 1) + info->selected_species + 1) *
                                info->max_num_altitudes];
        for (i = 0; i < info->max_num_altitudes; i++)
        {
            if (lrv_filter[i])
            {
                long num_j = 0;

                for (j = 0; j < info->max_num_altitudes; j++)
                {
                    if (lrv_filter[j])
                    {

                        data.double_data[num_i * info->max_num_altitudes + num_j] =
                            data.double_data[i * info->max_num_altitudes + j];
                        num_j++;
                    }
                }
                for (j = num_j; j < info->max_num_altitudes; j++)
                {
                    data.double_data[num_i * info->max_num_altitudes + j] = harp_nan();
                }
                num_i++;
            }
        }
        for (i = num_i; i < info->max_num_altitudes; i++)
        {
            for (j = 0; j < info->max_num_altitudes; j++)
            {
                data.double_data[i * info->max_num_altitudes + j] = harp_nan();
            }
        }
    }

    return 0;
}

static int get_profile_data(ingest_info *info, const coda_cursor *mds_cursor, const char *fieldname, long index,
                            uint8_t *lrv, harp_array data)
{
    coda_cursor cursor;
    long num_elements;
    long i;

    cursor = *mds_cursor;
    if (coda_cursor_goto_record_field_by_name(&cursor, fieldname) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &num_elements) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_double_array(&cursor, data.double_data, coda_array_ordering_c) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    reverse_double_array(data.double_data, num_elements);

    for (i = info->max_num_altitudes - 1; i >= 0; i--)
    {
        if (lrv[i])
        {
            assert(num_elements > 0);
            data.double_data[i] = data.double_data[num_elements - 1];
            num_elements--;
        }
        else
        {
            data.double_data[i] = harp_nan();
        }
    }
    assert(num_elements == 0);

    if (info->selected_species != -1)
    {
        uint8_t *lrv_filter;

        lrv_filter = &info->lrv[(index * (info->num_species + 1) + info->selected_species + 1) *
                                info->max_num_altitudes];
        for (i = 0; i < info->max_num_altitudes; i++)
        {
            if (lrv_filter[i])
            {
                data.double_data[num_elements] = data.double_data[i];
                num_elements++;
            }
        }
        for (i = num_elements; i < info->max_num_altitudes; i++)
        {
            data.double_data[i] = harp_nan();
        }
    }

    return 0;
}

static int get_profile_uncertainty_data(ingest_info *info, const coda_cursor *mds_cursor, const char *fieldname,
                                        long index, uint8_t *lrv, harp_array data)
{
    coda_cursor cursor;
    long num_elements;
    long num_pts;
    long i;

    cursor = *mds_cursor;
    if (coda_cursor_goto_record_field_by_name(&cursor, fieldname) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &num_elements) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    /* covar variable contains num_pts * (num_pts + 1) / 2 items */
    num_pts = (long)sqrt(2 * num_elements);     /* when rounded down this will thus give our number of num_pts */

    /* the first num_pts elements contain the variance data */
    if (coda_cursor_goto_first_array_element(&cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    for (i = 0; i < num_pts; i++)
    {
        if (coda_cursor_read_double(&cursor, &data.double_data[i]) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }

        /* uncertainty = sqrt(variance) */
        data.double_data[i] = sqrt(data.double_data[i]);

        if (i < num_pts - 1)
        {
            if (coda_cursor_goto_next_array_element(&cursor) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
        }
    }
    reverse_double_array(data.double_data, num_pts);

    for (i = info->max_num_altitudes - 1; i >= 0; i--)
    {
        /* we need to reverse the lrv index, because we already reversed 'data' with reverse_array() */
        if (lrv[i])
        {
            assert(num_pts > 0);
            data.double_data[i] = data.double_data[num_pts - 1];
            num_pts--;
        }
        else
        {
            data.double_data[i] = harp_nan();
        }
    }
    assert(num_pts == 0);

    if (info->selected_species != -1)
    {
        uint8_t *lrv_filter;

        lrv_filter = &info->lrv[(index * (info->num_species + 1) + info->selected_species + 1) *
                                info->max_num_altitudes];
        for (i = 0; i < info->max_num_altitudes; i++)
        {
            if (lrv_filter[i])
            {
                data.double_data[num_pts] = data.double_data[i];
                num_pts++;
            }
        }
        for (i = num_pts; i < info->max_num_altitudes; i++)
        {
            data.double_data[i] = harp_nan();
        }
    }

    return 0;
}

static int get_data(const coda_cursor *mds_cursor, const char *fieldname, const char *subfieldname, harp_array data)
{
    coda_cursor cursor;

    cursor = *mds_cursor;
    if (coda_cursor_goto_record_field_by_name(&cursor, fieldname) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (subfieldname != NULL)
    {
        if (coda_cursor_goto_record_field_by_name(&cursor, subfieldname) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
    }
    if (coda_cursor_read_double(&cursor, data.double_data) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    return 0;
}

static int read_dimensions(void *user_data, long dimension[HARP_NUM_DIM_TYPES])
{
    dimension[harp_dimension_time] = ((ingest_info *)user_data)->num_main;
    dimension[harp_dimension_vertical] = ((ingest_info *)user_data)->max_num_altitudes;
    return 0;
}

static int include_angles(void *user_data)
{
    return ((ingest_info *)user_data)->product_version >= 1;
}

static int include_h2o_akm(void *user_data)
{
    return ((ingest_info *)user_data)->product_version >= 2 &&
        ((ingest_info *)user_data)->species_index[species_h2o] >= 0;
}

static int include_o3_akm(void *user_data)
{
    return ((ingest_info *)user_data)->product_version >= 2 &&
        ((ingest_info *)user_data)->species_index[species_o3] >= 0;
}

static int include_hno3_akm(void *user_data)
{
    return ((ingest_info *)user_data)->product_version >= 2 &&
        ((ingest_info *)user_data)->species_index[species_hno3] >= 0;
}

static int include_ch4_akm(void *user_data)
{
    return ((ingest_info *)user_data)->product_version >= 2 &&
        ((ingest_info *)user_data)->species_index[species_ch4] >= 0;
}

static int include_n2o_akm(void *user_data)
{
    return ((ingest_info *)user_data)->product_version >= 2 &&
        ((ingest_info *)user_data)->species_index[species_n2o] >= 0;
}

static int include_no2_akm(void *user_data)
{
    return ((ingest_info *)user_data)->product_version >= 2 &&
        ((ingest_info *)user_data)->species_index[species_no2] >= 0;
}

static int include_f11_akm(void *user_data)
{
    return ((ingest_info *)user_data)->species_index[species_f11] >= 0;
}

static int include_clno_akm(void *user_data)
{
    return ((ingest_info *)user_data)->species_index[species_clno] >= 0;
}

static int include_n2o5_akm(void *user_data)
{
    return ((ingest_info *)user_data)->species_index[species_n2o5] >= 0;
}

static int include_f12_akm(void *user_data)
{
    return ((ingest_info *)user_data)->species_index[species_f12] >= 0;
}

static int include_cof2_akm(void *user_data)
{
    return ((ingest_info *)user_data)->species_index[species_cof2] >= 0;
}

static int include_ccl4_akm(void *user_data)
{
    return ((ingest_info *)user_data)->species_index[species_ccl4] >= 0;
}

static int include_hcn_akm(void *user_data)
{
    return ((ingest_info *)user_data)->species_index[species_hcn] >= 0;
}

static int include_f14_akm(void *user_data)
{
    return ((ingest_info *)user_data)->species_index[species_f14] >= 0;
}

static int include_f22_akm(void *user_data)
{
    return ((ingest_info *)user_data)->species_index[species_f22] >= 0;
}

static int include_h2o(void *user_data)
{
    return ((ingest_info *)user_data)->species_index[species_h2o] >= 0;
}

static int include_o3(void *user_data)
{
    return ((ingest_info *)user_data)->species_index[species_o3] >= 0;
}

static int include_hno3(void *user_data)
{
    return ((ingest_info *)user_data)->species_index[species_hno3] >= 0;
}

static int include_ch4(void *user_data)
{
    return ((ingest_info *)user_data)->species_index[species_ch4] >= 0;
}

static int include_n2o(void *user_data)
{
    return ((ingest_info *)user_data)->species_index[species_n2o] >= 0;
}

static int include_no2(void *user_data)
{
    return ((ingest_info *)user_data)->species_index[species_no2] >= 0;
}

static int include_f11(void *user_data)
{
    return ((ingest_info *)user_data)->species_index[species_f11] >= 0;
}

static int include_clno(void *user_data)
{
    return ((ingest_info *)user_data)->species_index[species_clno] >= 0;
}

static int include_n2o5(void *user_data)
{
    return ((ingest_info *)user_data)->species_index[species_n2o5] >= 0;
}

static int include_f12(void *user_data)
{
    return ((ingest_info *)user_data)->species_index[species_f12] >= 0;
}

static int include_cof2(void *user_data)
{
    return ((ingest_info *)user_data)->species_index[species_cof2] >= 0;
}

static int include_ccl4(void *user_data)
{
    return ((ingest_info *)user_data)->species_index[species_ccl4] >= 0;
}

static int include_hcn(void *user_data)
{
    return ((ingest_info *)user_data)->species_index[species_hcn] >= 0;
}

static int include_f14(void *user_data)
{
    return ((ingest_info *)user_data)->species_index[species_f14] >= 0;
}

static int include_f22(void *user_data)
{
    return ((ingest_info *)user_data)->species_index[species_f22] >= 0;
}

static int read_datetime(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (get_data(&info->geo_cursor[index], "dsr_time", NULL, data) != 0)
    {
        return -1;
    }
    /* some products have invalid time values (which are set to 0) -> set those values to NaN */
    if (*data.double_data == 0)
    {
        *data.double_data = harp_nan();
    }

    return 0;
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

static int read_altitude(void *user_data, long index, harp_array data)
{
    ingest_info *info;
    coda_cursor cursor;
    long num_altitudes;

    info = (ingest_info *)user_data;
    cursor = info->scan_cursor[index];
    num_altitudes = info->num_altitudes[index];
    if (coda_cursor_goto_record_field_by_name(&cursor, "tangent_altitude_los") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_double_array(&cursor, data.double_data, coda_array_ordering_c) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    reverse_double_array(data.double_data, num_altitudes);

    if (info->selected_species != -1)
    {
        uint8_t *lrv_filter;
        long num_elements = 0;
        long i;

        lrv_filter = &info->lrv[(index * (info->num_species + 1) + info->selected_species + 1) *
                                info->max_num_altitudes];
        for (i = 0; i < num_altitudes; i++)
        {
            if (lrv_filter[i])
            {
                data.double_data[num_elements] = data.double_data[i];
                num_elements++;
            }
        }
        for (i = num_elements; i < num_altitudes; i++)
        {
            data.double_data[i] = harp_nan();
        }
    }

    return 0;
}

static int read_latitude(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return get_data(&info->geo_cursor[index], "loc_mid", "latitude", data);
}

static int read_longitude(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return get_data(&info->geo_cursor[index], "loc_mid", "longitude", data);
}

static int read_solar_elevation_angle(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return get_data(&info->geo_cursor[index], "target_sun_elev", NULL, data);
}

static int read_solar_azimuth_angle(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return get_data(&info->geo_cursor[index], "target_sun_azi", NULL, data);
}

static int read_los_azimuth_angle(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return get_data(&info->geo_cursor[index], "sat_target_azi", NULL, data);
}

static int read_pressure(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return get_profile_data(info, &info->pt_cursor[index], "tan_press", index,
                            &info->lrv[index * (info->num_species + 1) * info->max_num_altitudes], data);
}

static int read_pressure_uncertainty(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return get_profile_uncertainty_data(info, &info->pt_cursor[index], "tan_press_var_cov", index,
                                        &info->lrv[index * (info->num_species + 1) * info->max_num_altitudes], data);
}

static int read_temperature(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return get_profile_data(info, &info->pt_cursor[index], "temp", index,
                            &info->lrv[index * (info->num_species + 1) * info->max_num_altitudes], data);
}

static int read_temperature_uncertainty(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return get_profile_uncertainty_data(info, &info->pt_cursor[index], "temp_var_cov", index,
                                        &info->lrv[index * (info->num_species + 1) * info->max_num_altitudes], data);
}

static int read_h2o(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_h2o];

    return get_profile_data(info, &info->mds_cursor[species_h2o][index], "conc_alt", index,
                            &info->lrv[(index * (info->num_species + 1) + species_index + 1) * info->max_num_altitudes],
                            data);
}

static int read_h2o_uncertainty(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_h2o];

    return get_profile_uncertainty_data(info, &info->mds_cursor[species_h2o][index], "conc_var_cov", index,
                                        &info->lrv[(index * (info->num_species + 1) + species_index + 1) *
                                                   info->max_num_altitudes], data);
}

static int read_o3(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_o3];

    return get_profile_data(info, &info->mds_cursor[species_o3][index], "conc_alt", index,
                            &info->lrv[(index * (info->num_species + 1) + species_index + 1) * info->max_num_altitudes],
                            data);
}

static int read_o3_uncertainty(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_o3];

    return get_profile_uncertainty_data(info, &info->mds_cursor[species_o3][index], "conc_var_cov", index,
                                        &info->lrv[(index * (info->num_species + 1) + species_index + 1) *
                                                   info->max_num_altitudes], data);
}

static int read_hno3(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_hno3];

    return get_profile_data(info, &info->mds_cursor[species_hno3][index], "conc_alt", index,
                            &info->lrv[(index * (info->num_species + 1) + species_index + 1) * info->max_num_altitudes],
                            data);
}

static int read_hno3_uncertainty(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_hno3];

    return get_profile_uncertainty_data(info, &info->mds_cursor[species_hno3][index], "conc_var_cov", index,
                                        &info->lrv[(index * (info->num_species + 1) + species_index + 1) *
                                                   info->max_num_altitudes], data);
}

static int read_ch4(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_ch4];

    return get_profile_data(info, &info->mds_cursor[species_ch4][index], "conc_alt", index,
                            &info->lrv[(index * (info->num_species + 1) + species_index + 1) * info->max_num_altitudes],
                            data);
}

static int read_ch4_uncertainty(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_ch4];

    return get_profile_uncertainty_data(info, &info->mds_cursor[species_ch4][index], "conc_var_cov", index,
                                        &info->lrv[(index * (info->num_species + 1) + species_index + 1) *
                                                   info->max_num_altitudes], data);
}

static int read_n2o(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_n2o];

    return get_profile_data(info, &info->mds_cursor[species_n2o][index], "conc_alt", index,
                            &info->lrv[(index * (info->num_species + 1) + species_index + 1) * info->max_num_altitudes],
                            data);
}

static int read_n2o_uncertainty(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_n2o];

    return get_profile_uncertainty_data(info, &info->mds_cursor[species_n2o][index], "conc_var_cov", index,
                                        &info->lrv[(index * (info->num_species + 1) + species_index + 1) *
                                                   info->max_num_altitudes], data);
}

static int read_no2(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_no2];

    return get_profile_data(info, &info->mds_cursor[species_no2][index], "conc_alt", index,
                            &info->lrv[(index * (info->num_species + 1) + species_index + 1) * info->max_num_altitudes],
                            data);
}

static int read_no2_uncertainty(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_no2];

    return get_profile_uncertainty_data(info, &info->mds_cursor[species_no2][index], "conc_var_cov", index,
                                        &info->lrv[(index * (info->num_species + 1) + species_index + 1) *
                                                   info->max_num_altitudes], data);
}

static int read_f11(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_f11];

    return get_profile_data(info, &info->mds_cursor[species_f11][index], "conc_alt", index,
                            &info->lrv[(index * (info->num_species + 1) + species_index + 1) * info->max_num_altitudes],
                            data);
}

static int read_f11_uncertainty(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_f11];

    return get_profile_uncertainty_data(info, &info->mds_cursor[species_f11][index], "conc_var_cov", index,
                                        &info->lrv[(index * (info->num_species + 1) + species_index + 1) *
                                                   info->max_num_altitudes], data);
}

static int read_clno(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_clno];

    return get_profile_data(info, &info->mds_cursor[species_clno][index], "conc_alt", index,
                            &info->lrv[(index * (info->num_species + 1) + species_index + 1) * info->max_num_altitudes],
                            data);
}

static int read_clno_uncertainty(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_clno];

    return get_profile_uncertainty_data(info, &info->mds_cursor[species_clno][index], "conc_var_cov", index,
                                        &info->lrv[(index * (info->num_species + 1) + species_index + 1) *
                                                   info->max_num_altitudes], data);
}

static int read_n2o5(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_n2o5];

    return get_profile_data(info, &info->mds_cursor[species_n2o5][index], "conc_alt", index,
                            &info->lrv[(index * (info->num_species + 1) + species_index + 1) * info->max_num_altitudes],
                            data);
}

static int read_n2o5_uncertainty(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_n2o5];

    return get_profile_uncertainty_data(info, &info->mds_cursor[species_n2o5][index], "conc_var_cov", index,
                                        &info->lrv[(index * (info->num_species + 1) + species_index + 1) *
                                                   info->max_num_altitudes], data);
}

static int read_f12(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_f12];

    return get_profile_data(info, &info->mds_cursor[species_f12][index], "conc_alt", index,
                            &info->lrv[(index * (info->num_species + 1) + species_index + 1) * info->max_num_altitudes],
                            data);
}

static int read_f12_uncertainty(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_f12];

    return get_profile_uncertainty_data(info, &info->mds_cursor[species_f12][index], "conc_var_cov", index,
                                        &info->lrv[(index * (info->num_species + 1) + species_index + 1) *
                                                   info->max_num_altitudes], data);
}

static int read_cof2(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_cof2];

    return get_profile_data(info, &info->mds_cursor[species_cof2][index], "conc_alt", index,
                            &info->lrv[(index * (info->num_species + 1) + species_index + 1) * info->max_num_altitudes],
                            data);
}

static int read_cof2_uncertainty(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_cof2];

    return get_profile_uncertainty_data(info, &info->mds_cursor[species_cof2][index], "conc_var_cov", index,
                                        &info->lrv[(index * (info->num_species + 1) + species_index + 1) *
                                                   info->max_num_altitudes], data);
}

static int read_ccl4(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_ccl4];

    return get_profile_data(info, &info->mds_cursor[species_ccl4][index], "conc_alt", index,
                            &info->lrv[(index * (info->num_species + 1) + species_index + 1) * info->max_num_altitudes],
                            data);
}

static int read_ccl4_uncertainty(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_ccl4];

    return get_profile_uncertainty_data(info, &info->mds_cursor[species_ccl4][index], "conc_var_cov", index,
                                        &info->lrv[(index * (info->num_species + 1) + species_index + 1) *
                                                   info->max_num_altitudes], data);
}

static int read_hcn(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_hcn];

    return get_profile_data(info, &info->mds_cursor[species_hcn][index], "conc_alt", index,
                            &info->lrv[(index * (info->num_species + 1) + species_index + 1) * info->max_num_altitudes],
                            data);
}

static int read_hcn_uncertainty(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_hcn];

    return get_profile_uncertainty_data(info, &info->mds_cursor[species_hcn][index], "conc_var_cov", index,
                                        &info->lrv[(index * (info->num_species + 1) + species_index + 1) *
                                                   info->max_num_altitudes], data);
}

static int read_f14(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_f14];

    return get_profile_data(info, &info->mds_cursor[species_f14][index], "conc_alt", index,
                            &info->lrv[(index * (info->num_species + 1) + species_index + 1) * info->max_num_altitudes],
                            data);
}

static int read_f14_uncertainty(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_f14];

    return get_profile_uncertainty_data(info, &info->mds_cursor[species_f14][index], "conc_var_cov", index,
                                        &info->lrv[(index * (info->num_species + 1) + species_index + 1) *
                                                   info->max_num_altitudes], data);
}

static int read_f22(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_f22];

    return get_profile_data(info, &info->mds_cursor[species_f22][index], "conc_alt", index,
                            &info->lrv[(index * (info->num_species + 1) + species_index + 1) * info->max_num_altitudes],
                            data);
}

static int read_f22_uncertainty(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_f22];

    return get_profile_uncertainty_data(info, &info->mds_cursor[species_f22][index], "conc_var_cov", index,
                                        &info->lrv[(index * (info->num_species + 1) + species_index + 1) *
                                                   info->max_num_altitudes], data);
}

static int read_h2o_vmr(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_h2o];

    return get_profile_data(info, &info->mds_cursor[species_h2o][index], "vmr", index,
                            &info->lrv[(index * (info->num_species + 1) + species_index + 1) * info->max_num_altitudes],
                            data);
}

static int read_h2o_vmr_uncertainty(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_h2o];

    return get_profile_uncertainty_data(info, &info->mds_cursor[species_h2o][index], "vmr_var_cov", index,
                                        &info->lrv[(index * (info->num_species + 1) + species_index + 1) *
                                                   info->max_num_altitudes], data);
}

static int read_o3_vmr(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_o3];

    return get_profile_data(info, &info->mds_cursor[species_o3][index], "vmr", index,
                            &info->lrv[(index * (info->num_species + 1) + species_index + 1) * info->max_num_altitudes],
                            data);
}

static int read_o3_vmr_uncertainty(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_o3];

    return get_profile_uncertainty_data(info, &info->mds_cursor[species_o3][index], "vmr_var_cov", index,
                                        &info->lrv[(index * (info->num_species + 1) + species_index + 1) *
                                                   info->max_num_altitudes], data);
}

static int read_hno3_vmr(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_hno3];

    return get_profile_data(info, &info->mds_cursor[species_hno3][index], "vmr", index,
                            &info->lrv[(index * (info->num_species + 1) + species_index + 1) *
                                       info->max_num_altitudes], data);
}

static int read_hno3_vmr_uncertainty(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_hno3];

    return get_profile_uncertainty_data(info, &info->mds_cursor[species_hno3][index], "vmr_var_cov", index,
                                        &info->lrv[(index * (info->num_species + 1) + species_index + 1) *
                                                   info->max_num_altitudes], data);
}

static int read_ch4_vmr(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_ch4];

    return get_profile_data(info, &info->mds_cursor[species_ch4][index], "vmr", index,
                            &info->lrv[(index * (info->num_species + 1) + species_index + 1) * info->max_num_altitudes],
                            data);
}

static int read_ch4_vmr_uncertainty(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_ch4];

    return get_profile_uncertainty_data(info, &info->mds_cursor[species_ch4][index], "vmr_var_cov", index,
                                        &info->lrv[(index * (info->num_species + 1) + species_index + 1) *
                                                   info->max_num_altitudes], data);
}

static int read_n2o_vmr(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_n2o];

    return get_profile_data(info, &info->mds_cursor[species_n2o][index], "vmr", index,
                            &info->lrv[(index * (info->num_species + 1) + species_index + 1) *
                                       info->max_num_altitudes], data);
}

static int read_n2o_vmr_uncertainty(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_n2o];

    return get_profile_uncertainty_data(info, &info->mds_cursor[species_n2o][index], "vmr_var_cov", index,
                                        &info->lrv[(index * (info->num_species + 1) + species_index + 1) *
                                                   info->max_num_altitudes], data);
}

static int read_no2_vmr(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_no2];

    return get_profile_data(info, &info->mds_cursor[species_no2][index], "vmr", index,
                            &info->lrv[(index * (info->num_species + 1) + species_index + 1) * info->max_num_altitudes],
                            data);
}

static int read_no2_vmr_uncertainty(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_no2];

    return get_profile_uncertainty_data(info, &info->mds_cursor[species_no2][index], "vmr_var_cov", index,
                                        &info->lrv[(index * (info->num_species + 1) + species_index + 1) *
                                                   info->max_num_altitudes], data);
}

static int read_f11_vmr(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_f11];

    return get_profile_data(info, &info->mds_cursor[species_f11][index], "vmr", index,
                            &info->lrv[(index * (info->num_species + 1) + species_index + 1) * info->max_num_altitudes],
                            data);
}

static int read_f11_vmr_uncertainty(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_f11];

    return get_profile_uncertainty_data(info, &info->mds_cursor[species_f11][index], "vmr_var_cov", index,
                                        &info->lrv[(index * (info->num_species + 1) + species_index + 1) *
                                                   info->max_num_altitudes], data);
}

static int read_clno_vmr(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_clno];

    return get_profile_data(info, &info->mds_cursor[species_clno][index], "vmr", index,
                            &info->lrv[(index * (info->num_species + 1) + species_index + 1) * info->max_num_altitudes],
                            data);
}

static int read_clno_vmr_uncertainty(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_clno];

    return get_profile_uncertainty_data(info, &info->mds_cursor[species_clno][index], "vmr_var_cov", index,
                                        &info->lrv[(index * (info->num_species + 1) + species_index + 1) *
                                                   info->max_num_altitudes], data);
}

static int read_n2o5_vmr(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_n2o5];

    return get_profile_data(info, &info->mds_cursor[species_n2o5][index], "vmr", index,
                            &info->lrv[(index * (info->num_species + 1) + species_index + 1) * info->max_num_altitudes],
                            data);
}

static int read_n2o5_vmr_uncertainty(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_n2o5];

    return get_profile_uncertainty_data(info, &info->mds_cursor[species_n2o5][index], "vmr_var_cov", index,
                                        &info->lrv[(index * (info->num_species + 1) + species_index + 1) *
                                                   info->max_num_altitudes], data);
}

static int read_f12_vmr(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_f12];

    return get_profile_data(info, &info->mds_cursor[species_f12][index], "vmr", index,
                            &info->lrv[(index * (info->num_species + 1) + species_index + 1) * info->max_num_altitudes],
                            data);
}

static int read_f12_vmr_uncertainty(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_f12];

    return get_profile_uncertainty_data(info, &info->mds_cursor[species_f12][index], "vmr_var_cov", index,
                                        &info->lrv[(index * (info->num_species + 1) + species_index + 1) *
                                                   info->max_num_altitudes], data);
}

static int read_cof2_vmr(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_cof2];

    return get_profile_data(info, &info->mds_cursor[species_cof2][index], "vmr", index,
                            &info->lrv[(index * (info->num_species + 1) + species_index + 1) * info->max_num_altitudes],
                            data);
}

static int read_cof2_vmr_uncertainty(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_cof2];

    return get_profile_uncertainty_data(info, &info->mds_cursor[species_cof2][index], "vmr_var_cov", index,
                                        &info->lrv[(index * (info->num_species + 1) + species_index + 1) *
                                                   info->max_num_altitudes], data);
}

static int read_ccl4_vmr(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_ccl4];

    return get_profile_data(info, &info->mds_cursor[species_ccl4][index], "vmr", index,
                            &info->lrv[(index * (info->num_species + 1) + species_index + 1) * info->max_num_altitudes],
                            data);
}

static int read_ccl4_vmr_uncertainty(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_ccl4];

    return get_profile_uncertainty_data(info, &info->mds_cursor[species_ccl4][index], "vmr_var_cov", index,
                                        &info->lrv[(index * (info->num_species + 1) + species_index + 1) *
                                                   info->max_num_altitudes], data);
}

static int read_hcn_vmr(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_hcn];

    return get_profile_data(info, &info->mds_cursor[species_hcn][index], "vmr", index,
                            &info->lrv[(index * (info->num_species + 1) + species_index + 1) * info->max_num_altitudes],
                            data);
}

static int read_hcn_vmr_uncertainty(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_hcn];

    return get_profile_uncertainty_data(info, &info->mds_cursor[species_hcn][index], "vmr_var_cov", index,
                                        &info->lrv[(index * (info->num_species + 1) + species_index + 1) *
                                                   info->max_num_altitudes], data);
}

static int read_f14_vmr(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_f14];

    return get_profile_data(info, &info->mds_cursor[species_f14][index], "vmr", index,
                            &info->lrv[(index * (info->num_species + 1) + species_index + 1) * info->max_num_altitudes],
                            data);
}

static int read_f14_vmr_uncertainty(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_f14];

    return get_profile_uncertainty_data(info, &info->mds_cursor[species_f14][index], "vmr_var_cov", index,
                                        &info->lrv[(index * (info->num_species + 1) + species_index + 1) *
                                                   info->max_num_altitudes], data);
}

static int read_f22_vmr(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_f22];

    return get_profile_data(info, &info->mds_cursor[species_f22][index], "vmr", index,
                            &info->lrv[(index * (info->num_species + 1) + species_index + 1) * info->max_num_altitudes],
                            data);
}

static int read_f22_vmr_uncertainty(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_f22];

    return get_profile_uncertainty_data(info, &info->mds_cursor[species_f22][index], "vmr_var_cov", index,
                                        &info->lrv[(index * (info->num_species + 1) + species_index + 1) *
                                                   info->max_num_altitudes], data);
}

static int read_h2o_akm_vmr(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_h2o];

    return get_akm_data(info, &info->mds_cursor[species_h2o][index], index,
                        &info->lrv[(index * (info->num_species + 1) + species_index + 1) * info->max_num_altitudes],
                        data);
}

static int read_o3_akm_vmr(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_o3];

    return get_akm_data(info, &info->mds_cursor[species_o3][index], index,
                        &info->lrv[(index * (info->num_species + 1) + species_index + 1) * info->max_num_altitudes],
                        data);
}

static int read_hno3_akm_vmr(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_hno3];

    return get_akm_data(info, &info->mds_cursor[species_hno3][index], index,
                        &info->lrv[(index * (info->num_species + 1) + species_index + 1) * info->max_num_altitudes],
                        data);
}

static int read_ch4_akm_vmr(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_ch4];

    return get_akm_data(info, &info->mds_cursor[species_ch4][index], index,
                        &info->lrv[(index * (info->num_species + 1) + species_index + 1) * info->max_num_altitudes],
                        data);
}

static int read_n2o_akm_vmr(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_n2o];

    return get_akm_data(info, &info->mds_cursor[species_n2o][index], index,
                        &info->lrv[(index * (info->num_species + 1) + species_index + 1) * info->max_num_altitudes],
                        data);
}

static int read_no2_akm_vmr(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_no2];

    return get_akm_data(info, &info->mds_cursor[species_no2][index], index,
                        &info->lrv[(index * (info->num_species + 1) + species_index + 1) * info->max_num_altitudes],
                        data);
}

static int read_f11_akm_vmr(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_f11];

    return get_akm_data(info, &info->mds_cursor[species_f11][index], index,
                        &info->lrv[(index * (info->num_species + 1) + species_index + 1) * info->max_num_altitudes],
                        data);
}

static int read_clno_akm_vmr(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_clno];

    return get_akm_data(info, &info->mds_cursor[species_clno][index], index,
                        &info->lrv[(index * (info->num_species + 1) + species_index + 1) * info->max_num_altitudes],
                        data);
}

static int read_n2o5_akm_vmr(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_n2o5];

    return get_akm_data(info, &info->mds_cursor[species_n2o5][index], index,
                        &info->lrv[(index * (info->num_species + 1) + species_index + 1) * info->max_num_altitudes],
                        data);
}

static int read_f12_akm_vmr(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_f12];

    return get_akm_data(info, &info->mds_cursor[species_f12][index], index,
                        &info->lrv[(index * (info->num_species + 1) + species_index + 1) * info->max_num_altitudes],
                        data);
}

static int read_cof2_akm_vmr(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_cof2];

    return get_akm_data(info, &info->mds_cursor[species_cof2][index], index,
                        &info->lrv[(index * (info->num_species + 1) + species_index + 1) * info->max_num_altitudes],
                        data);
}

static int read_ccl4_akm_vmr(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_ccl4];

    return get_akm_data(info, &info->mds_cursor[species_ccl4][index], index,
                        &info->lrv[(index * (info->num_species + 1) + species_index + 1) * info->max_num_altitudes],
                        data);
}

static int read_hcn_akm_vmr(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_hcn];

    return get_akm_data(info, &info->mds_cursor[species_hcn][index], index,
                        &info->lrv[(index * (info->num_species + 1) + species_index + 1) * info->max_num_altitudes],
                        data);
}

static int read_f14_akm_vmr(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_f14];

    return get_akm_data(info, &info->mds_cursor[species_f14][index], index,
                        &info->lrv[(index * (info->num_species + 1) + species_index + 1) * info->max_num_altitudes],
                        data);
}

static int read_f22_akm_vmr(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int species_index = info->species_index[species_f22];

    return get_akm_data(info, &info->mds_cursor[species_f22][index], index,
                        &info->lrv[(index * (info->num_species + 1) + species_index + 1) * info->max_num_altitudes],
                        data);
}

int harp_ingestion_module_mipas_l2_init(void)
{
    const char *species_options[] = {
        "H2O", "O3", "HNO3", "CH4", "N2O", "NO2", "F11", "ClNO", "N2O5", "F12", "COF2", "CCL4", "HCN", "F14", "F22"
    };
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[3];
    const char *description;
    const char *path;

    description = "MIPAS Temperature, Pressure, and Atmospheric Constituents Profiles";
    module = harp_ingestion_register_module("MIPAS_L2", "MIPAS", "ENVISAT_MIPAS", "MIP_NL__2P", description,
                                            ingestion_init, ingestion_done);

    harp_ingestion_register_option(module, "species", "if the option is provided then ingest only the specified "
                                   "species (together with p and T) and remove all vertical levels for which the "
                                   "logical retrieval vector (lrv) for the specified species is false",
                                   NUM_SPECIES_TYPES, species_options);

    description = "profile data";
    product_definition = harp_ingestion_register_product(module, "MIPAS_L2", description, read_dimensions);

    dimension_type[0] = harp_dimension_time;
    dimension_type[1] = harp_dimension_vertical;
    dimension_type[2] = harp_dimension_vertical;

    description = "start time of the measurement";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "datetime", harp_type_double,
                                                                      1, dimension_type, NULL, description,
                                                                      "seconds since 2000-01-01", NULL, read_datetime);
    path = "/scan_geolocation_ads[]/dsr_time[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "absolute orbit number";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "orbit_index", harp_type_int32, 0, NULL, NULL,
                                                   description, NULL, NULL, read_orbit_index);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/mph/abs_orbit", NULL);

    description = "altitude";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "altitude", harp_type_double,
                                                                      2, dimension_type, NULL, description, "km", NULL,
                                                                      read_altitude);
    path = "/scan_information_mds[]/tangent_altitude_los[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "latitude";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "latitude", harp_type_double,
                                                                      1, dimension_type, NULL, description,
                                                                      "degree_north", NULL, read_latitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    path = "/scan_geolocation_ads[]/loc_mid[]/latitude";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "longitude";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "longitude",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, "degree_east", NULL, read_longitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0, 180.0);
    path = "/scan_geolocation_ads[]/loc_mid[]/longitude";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* Angles. */
    description = "solar elevation angle (target to sun)";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "solar_elevation_angle",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, "degree", include_angles,
                                                                      read_solar_elevation_angle);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    path = "/scan_geolocation_ads[]/target_sun_elev";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "solar azimuth angle (target to sun)";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "solar_azimuth_angle",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, "degree", include_angles,
                                                                      read_solar_azimuth_angle);
    harp_variable_definition_set_valid_range_double(variable_definition, 0.0, 360.0);
    path = "/scan_geolocation_ads[]/target_sun_azi";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "line-of-sight azimuth angle (satellite to target)";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "viewing_azimuth_angle",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, "degree", include_angles,
                                                                      read_los_azimuth_angle);
    harp_variable_definition_set_valid_range_double(variable_definition, 0.0, 360.0);
    path = "/scan_geolocation_ads[]/sat_target_azi";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* Pressure and temperature profiles. */
    description = "pressure";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "pressure", harp_type_double,
                                                                      2, dimension_type, NULL, description, "hPa",
                                                                      NULL, read_pressure);
    path = "/pt_retrieval_mds[]/tan_press[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "pressure standard deviation";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "pressure_uncertainty",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "hPa", NULL,
                                                                      read_pressure_uncertainty);
    path = "/pt_retrieval_mds[]/tan_press_var_cov[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "temperature";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "temperature",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "K", NULL, read_temperature);
    path = "/pt_retrieval_mds[]/temp[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "temperature standard deviation";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "temperature_uncertainty",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "K", NULL,
                                                                      read_temperature_uncertainty);
    path = "/pt_retrieval_mds[]/temp_var_cov[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* Number density profiles. */
    description = "H2O number density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "H2O_number_density",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "molec/cm^3", include_h2o, read_h2o);
    path = "/h2o_retrieval_mds[]/conc_alt[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "standard deviation for the H2O number density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "H2O_number_density_uncertainty",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "molec/cm^3", include_h2o,
                                                                      read_h2o_uncertainty);
    path = "/h2o_retrieval_mds[]/conc_var_cov[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "O3 number density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "O3_number_density",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "molec/cm^3", include_o3, read_o3);
    path = "/o3_retrieval_mds[]/conc_alt[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "standard deviation for the O3 number density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "O3_number_density_uncertainty",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "molec/cm^3", include_o3,
                                                                      read_o3_uncertainty);
    path = "/o3_retrieval_mds[]/conc_var_cov[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "HNO3 number density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "HNO3_number_density",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "molec/cm^3", include_hno3,
                                                                      read_hno3);
    path = "/hno3_retrieval_mds[]/conc_alt[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "standard deviation for the HNO3 number density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "HNO3_number_density_uncertainty",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "molec/cm^3", include_hno3,
                                                                      read_hno3_uncertainty);
    path = "/hno3_retrieval_mds[]/conc_var_cov[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "CH4 number density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "CH4_number_density",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "molec/cm^3", include_ch4, read_ch4);
    path = "/ch4_retrieval_mds[]/conc_alt[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "standard deviation for the CH4 number density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "CH4_number_density_uncertainty",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "molec/cm^3", include_ch4,
                                                                      read_ch4_uncertainty);
    path = "/ch4_retrieval_mds[]/conc_var_cov[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "N2O number density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "N2O_number_density",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "molec/cm^3", include_n2o, read_n2o);
    path = "/n2o_retrieval_mds[]/conc_alt[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "standard deviation for the N2O number density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "N2O_number_density_uncertainty",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "molec/cm^3", include_n2o,
                                                                      read_n2o_uncertainty);
    path = "/n2o_retrieval_mds[]/conc_var_cov[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "NO2 number density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "NO2_number_density",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "molec/cm^3", include_no2, read_no2);
    path = "/no2_retrieval_mds[]/conc_alt[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "standard deviation for the NO2 number density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "NO2_number_density_uncertainty",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "molec/cm^3", include_no2,
                                                                      read_no2_uncertainty);
    path = "/no2_retrieval_mds[]/conc_var_cov[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "F11 number density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "CCl3F_number_density",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "molec/cm^3", include_f11, read_f11);
    path = "/f11_retrieval_mds[]/conc_alt[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "standard deviation for the F11 number density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "CCl3F_number_density_uncertainty",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "molec/cm^3", include_f11,
                                                                      read_f11_uncertainty);
    path = "/f11_retrieval_mds[]/conc_var_cov[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "NOCl number density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "NOCl_number_density",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "molec/cm^3", include_clno,
                                                                      read_clno);
    path = "/clno_retrieval_mds[]/conc_alt[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "standard deviation for the NOCl number density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "NOCl_number_density_uncertainty",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "molec/cm^3", include_clno,
                                                                      read_clno_uncertainty);
    path = "/clno_retrieval_mds[]/conc_var_cov[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "N2O5 number density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "N2O5_number_density",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "molec/cm^3", include_n2o5,
                                                                      read_n2o5);
    path = "/n2o5_retrieval_mds[]/conc_alt[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "standard deviation for the N2O5 number density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "N2O5_number_density_uncertainty",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "molec/cm^3", include_n2o5,
                                                                      read_n2o5_uncertainty);
    path = "/n2o5_retrieval_mds[]/conc_var_cov[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "F12 number density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "CCl2F2_number_density",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "molec/cm^3", include_f12, read_f12);
    path = "/f12_retrieval_mds[]/conc_alt[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "standard deviation for the F12 number density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "CCl2F2_number_density_uncertainty",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "molec/cm^3", include_f12,
                                                                      read_f12_uncertainty);
    path = "/f12_retrieval_mds[]/conc_var_cov[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "COF2 number density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "COF2_number_density",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "molec/cm^3", include_cof2,
                                                                      read_cof2);
    path = "/cof2_retrieval_mds[]/conc_alt[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "standard deviation for the COF2 number density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "COF2_number_density_uncertainty",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "molec/cm^3", include_cof2,
                                                                      read_cof2_uncertainty);
    path = "/cof2_retrieval_mds[]/conc_var_cov[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "CCL4 number density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "CCl4_number_density",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "molec/cm^3", include_ccl4,
                                                                      read_ccl4);
    path = "/ccl4_retrieval_mds[]/conc_alt[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "standard deviation for the CCL4 number density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "CCl4_number_density_uncertainty",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "molec/cm^3", include_ccl4,
                                                                      read_ccl4_uncertainty);
    path = "/ccl4_retrieval_mds[]/conc_var_cov[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "HCN number density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "HCN_number_density",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "molec/cm^3", include_hcn, read_hcn);
    path = "/hcn_retrieval_mds[]/conc_alt[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "standard deviation for the HCN number density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "HCN_number_density_uncertainty",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "molec/cm^3", include_hcn,
                                                                      read_hcn_uncertainty);
    path = "/hcn_retrieval_mds[]/conc_var_cov[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "F14 number density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "CF4_number_density",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "molec/cm^3", include_f14, read_f14);
    path = "/f14_retrieval_mds[]/conc_alt[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "standard deviation for the F14 number density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "CF4_number_density_uncertainty",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "molec/cm^3", include_f14,
                                                                      read_f14_uncertainty);
    path = "/f14_retrieval_mds[]/conc_var_cov[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "F22 number density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "CHClF2_number_density",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "molec/cm^3", include_f22, read_f22);
    path = "/f22_retrieval_mds[]/conc_alt[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "standard deviation for the F22 number density";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "CHClF2_number_density_uncertainty",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "molec/cm^3", include_f22,
                                                                      read_f22_uncertainty);
    path = "/f22_retrieval_mds[]/conc_var_cov[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* Volume mixing ratio profiles. */
    description = "H2O volume mixing ratio";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "H2O_volume_mixing_ratio",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "ppmv", include_h2o, read_h2o_vmr);
    path = "/h2o_retrieval_mds[]/vmr[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "standard deviation for the H2O volume mixing ratio";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "H2O_volume_mixing_ratio_uncertainty",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "ppmv", include_h2o,
                                                                      read_h2o_vmr_uncertainty);
    path = "/h2o_retrieval_mds[]/vmr_var_cov[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "O3 volume mixing ratio";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "O3_volume_mixing_ratio",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "ppmv", include_o3, read_o3_vmr);
    path = "/o3_retrieval_mds[]/vmr[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "standard deviation for the O3 volume mixing ratio";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "O3_volume_mixing_ratio_uncertainty",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "ppmv", include_o3,
                                                                      read_o3_vmr_uncertainty);
    path = "/o3_retrieval_mds[]/vmr_var_cov[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "HNO3 volume mixing ratio";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "HNO3_volume_mixing_ratio",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "ppmv", include_hno3, read_hno3_vmr);
    path = "/hno3_retrieval_mds[]/vmr[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "standard deviation for the HNO3 volume mixing ratio";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "HNO3_volume_mixing_ratio_uncertainty",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "ppmv", include_hno3,
                                                                      read_hno3_vmr_uncertainty);
    path = "/hno3_retrieval_mds[]/vmr_var_cov[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "CH4 volume mixing ratio";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "CH4_volume_mixing_ratio",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "ppmv", include_ch4, read_ch4_vmr);
    path = "/ch4_retrieval_mds[]/vmr[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "standard deviation for the CH4 volume mixing ratio";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "CH4_volume_mixing_ratio_uncertainty",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "ppmv", include_ch4,
                                                                      read_ch4_vmr_uncertainty);
    path = "/ch4_retrieval_mds[]/vmr_var_cov[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "N2O volume mixing ratio";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "N2O_volume_mixing_ratio",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "ppmv", include_n2o, read_n2o_vmr);
    path = "/n2o_retrieval_mds[]/vmr[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "standard deviation for the N2O volume mixing ratio";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "N2O_volume_mixing_ratio_uncertainty",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "ppmv", include_n2o,
                                                                      read_n2o_vmr_uncertainty);
    path = "/n2o_retrieval_mds[]/vmr_var_cov[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "NO2 volume mixing ratio";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "NO2_volume_mixing_ratio",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "ppmv", include_no2, read_no2_vmr);
    path = "/no2_retrieval_mds[]/vmr[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "standard deviation for the NO2 volume mixing ratio";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "NO2_volume_mixing_ratio_uncertainty",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "ppmv", include_no2,
                                                                      read_no2_vmr_uncertainty);
    path = "/no2_retrieval_mds[]/vmr_var_cov[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "F11 volume mixing ratio";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "CCl3F_volume_mixing_ratio",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "ppmv", include_f11, read_f11_vmr);
    path = "/f11_retrieval_mds[]/vmr[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "standard deviation for the F11 volume mixing ratio";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "CCl3F_volume_mixing_ratio_uncertainty",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "ppmv", include_f11,
                                                                      read_f11_vmr_uncertainty);
    path = "/f11_retrieval_mds[]/vmr_var_cov[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "NOCl volume mixing ratio";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "NOCl_volume_mixing_ratio",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "ppmv", include_clno, read_clno_vmr);
    path = "/clno_retrieval_mds[]/vmr[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "standard deviation for the NOCl volume mixing ratio";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "NOCl_volume_mixing_ratio_uncertainty",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "ppmv", include_clno,
                                                                      read_clno_vmr_uncertainty);
    path = "/clno_retrieval_mds[]/vmr_var_cov[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "N2O5 volume mixing ratio";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "N2O5_volume_mixing_ratio",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "ppmv", include_n2o5, read_n2o5_vmr);
    path = "/n2o5_retrieval_mds[]/vmr[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "standard deviation for the N2O5 volume mixing ratio";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "N2O5_volume_mixing_ratio_uncertainty",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "ppmv", include_n2o5,
                                                                      read_n2o5_vmr_uncertainty);
    path = "/n2o5_retrieval_mds[]/vmr_var_cov[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "F12 volume mixing ratio";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "CCl2F2_volume_mixing_ratio",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "ppmv", include_f12, read_f12_vmr);
    path = "/f12_retrieval_mds[]/vmr[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "standard deviation for the F12 volume mixing ratio";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "CCl2F2_volume_mixing_ratio_uncertainty",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "ppmv", include_f12,
                                                                      read_f12_vmr_uncertainty);
    path = "/f12_retrieval_mds[]/vmr_var_cov[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "COF2 volume mixing ratio";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "COF2_volume_mixing_ratio",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "ppmv", include_cof2, read_cof2_vmr);
    path = "/cof2_retrieval_mds[]/vmr[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "standard deviation for the COF2 volume mixing ratio";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "COF2_volume_mixing_ratio_uncertainty",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "ppmv", include_cof2,
                                                                      read_cof2_vmr_uncertainty);
    path = "/cof2_retrieval_mds[]/vmr_var_cov[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "CCL4 volume mixing ratio";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "CCl4_volume_mixing_ratio",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "ppmv", include_ccl4, read_ccl4_vmr);
    path = "/ccl4_retrieval_mds[]/vmr[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "standard deviation for the CCL4 volume mixing ratio";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "CCl4_volume_mixing_ratio_uncertainty",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "ppmv", include_ccl4,
                                                                      read_ccl4_vmr_uncertainty);
    path = "/ccl4_retrieval_mds[]/vmr_var_cov[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "HCN volume mixing ratio";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "HCN_volume_mixing_ratio",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "ppmv", include_hcn, read_hcn_vmr);
    path = "/hcn_retrieval_mds[]/vmr[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "standard deviation for the HCN volume mixing ratio";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "HCN_volume_mixing_ratio_uncertainty",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "ppmv", include_hcn,
                                                                      read_hcn_vmr_uncertainty);
    path = "/hcn_retrieval_mds[]/vmr_var_cov[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "F14 volume mixing ratio";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "CF4_volume_mixing_ratio",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "ppmv", include_f14, read_f14_vmr);
    path = "/f14_retrieval_mds[]/vmr[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "standard deviation for the F14 volume mixing ratio";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "CF4_volume_mixing_ratio_uncertainty",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "ppmv", include_f14,
                                                                      read_f14_vmr_uncertainty);
    path = "/f14_retrieval_mds[]/vmr_var_cov[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "F22 volume mixing ratio";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "CHClF2_volume_mixing_ratio",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "ppmv", include_f22, read_f22_vmr);
    path = "/f22_retrieval_mds[]/vmr[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "standard deviation for the F22 volume mixing ratio";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "CHClF2_volume_mixing_ratio_uncertainty",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "ppmv", include_f22,
                                                                      read_f22_vmr_uncertainty);
    path = "/f22_retrieval_mds[]/vmr_var_cov[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* Volume mixing ratio profile averaging kernel matrices. */
    description = "averaging kernel matrix";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "H2O_volume_mixing_ratio_avk",
                                                                      harp_type_double, 3, dimension_type, NULL,
                                                                      description, "ppmv/ppmv", include_h2o_akm,
                                                                      read_h2o_akm_vmr);
    path = "/h2o_retrieval_mds[]/avg_kernel[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "O3_volume_mixing_ratio_avk",
                                                                      harp_type_double, 3, dimension_type, NULL,
                                                                      description, "ppmv/ppmv", include_o3_akm,
                                                                      read_o3_akm_vmr);
    path = "/o3_retrieval_mds[]/avg_kernel[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "HNO3_volume_mixing_ratio_avk",
                                                                      harp_type_double, 3, dimension_type, NULL,
                                                                      description, "ppmv/ppmv", include_hno3_akm,
                                                                      read_hno3_akm_vmr);
    path = "/hno3_retrieval_mds[]/avg_kernel[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "CH4_volume_mixing_ratio_avk", harp_type_double,
                                                                      3, dimension_type, NULL, description,
                                                                      "ppmv/ppmv", include_ch4_akm, read_ch4_akm_vmr);
    path = "/ch4_retrieval_mds[]/avg_kernel[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "N2O_volume_mixing_ratio_avk", harp_type_double,
                                                                      3, dimension_type, NULL, description,
                                                                      "ppmv/ppmv", include_n2o_akm, read_n2o_akm_vmr);
    path = "/n2o_retrieval_mds[]/avg_kernel[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "NO2_volume_mixing_ratio_avk",
                                                                      harp_type_double, 3, dimension_type, NULL,
                                                                      description, "ppmv/ppmv", include_no2_akm,
                                                                      read_no2_akm_vmr);
    path = "/no2_retrieval_mds[]/avg_kernel[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "CCl3F_volume_mixing_ratio_avk",
                                                                      harp_type_double, 3, dimension_type, NULL,
                                                                      description, "ppmv/ppmv", include_f11_akm,
                                                                      read_f11_akm_vmr);
    path = "/f11_retrieval_mds[]/avg_kernel[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "NOCl_volume_mixing_ratio_avk", harp_type_double,
                                                                      3, dimension_type, NULL, description,
                                                                      "ppmv/ppmv", include_clno_akm, read_clno_akm_vmr);
    path = "/clno_retrieval_mds[]/avg_kernel[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "N2O5_volume_mixing_ratio_avk", harp_type_double,
                                                                      3, dimension_type, NULL, description,
                                                                      "ppmv/ppmv", include_n2o5_akm, read_n2o5_akm_vmr);
    path = "/n2o5_retrieval_mds[]/avg_kernel[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "CCl2F2_volume_mixing_ratio_avk",
                                                                      harp_type_double, 3, dimension_type, NULL,
                                                                      description, "ppmv/ppmv", include_f12_akm,
                                                                      read_f12_akm_vmr);
    path = "/f12_retrieval_mds[]/avg_kernel[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "COF2_volume_mixing_ratio_avk",
                                                                      harp_type_double, 3, dimension_type, NULL,
                                                                      description, "ppmv/ppmv", include_cof2_akm,
                                                                      read_cof2_akm_vmr);
    path = "/cof2_retrieval_mds[]/avg_kernel[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "CCl4_volume_mixing_ratio_avk",
                                                                      harp_type_double, 3, dimension_type, NULL,
                                                                      description, "ppmv/ppmv", include_ccl4_akm,
                                                                      read_ccl4_akm_vmr);
    path = "/ccl4_retrieval_mds[]/avg_kernel[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "HCN_volume_mixing_ratio_avk",
                                                                      harp_type_double, 3, dimension_type, NULL,
                                                                      description, "ppmv/ppmv", include_hcn_akm,
                                                                      read_hcn_akm_vmr);
    path = "/hcn_retrieval_mds[]/avg_kernel[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "CF4_volume_mixing_ratio_avk",
                                                                      harp_type_double, 3, dimension_type, NULL,
                                                                      description, "ppmv/ppmv", include_f14_akm,
                                                                      read_f14_akm_vmr);
    path = "/f14_retrieval_mds[]/avg_kernel[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "CHClF2_volume_mixing_ratio_avk",
                                                                      harp_type_double, 3, dimension_type, NULL,
                                                                      description, "ppmv/ppmv", include_f22_akm,
                                                                      read_f22_akm_vmr);
    path = "/f22_retrieval_mds[]/avg_kernel[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    return 0;
}
