/*
 * Copyright (C) 2015-2016 S[&]T, The Netherlands.
 *
 * This file is part of HARP.
 *
 * HARP is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * HARP is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with HARP; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "coda.h"
#include "harp-ingestion.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct ingest_info_struct
{
    coda_product *product;
    int product_version;
    long num_main;
    long *num_altitudes;
    long max_num_altitudes;
    int num_species;
    coda_cursor *scan_cursor;
    coda_cursor *geo_cursor;
    coda_cursor *pt_cursor;
    coda_cursor *h2o_cursor;
    coda_cursor *o3_cursor;
    coda_cursor *hno3_cursor;
    coda_cursor *ch4_cursor;
    coda_cursor *n2o_cursor;
    coda_cursor *no2_cursor;
    coda_cursor *f11_cursor;
    coda_cursor *clno_cursor;
    coda_cursor *n2o5_cursor;
    coda_cursor *f12_cursor;
    int h2o_id;
    int o3_id;
    int hno3_id;
    int ch4_id;
    int n2o_id;
    int no2_id;
    int f11_id;
    int clno_id;
    int n2o5_id;
    int f12_id;
    uint8_t *lrv;       /* logical retrieval vector; dim=[num_main, {pT, sp#1, sp#2, .., sp#n}, max_num_altitudes] */
} ingest_info;

static void reverse_array(double *data, long num_elements)
{
    long bottom, top;

    for (bottom = 0, top = num_elements - 1; bottom < num_elements / 2; bottom++, top--)
    {
        double tmp = data[bottom];

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
            info->h2o_id = species_number;
        }
        else if (strcasecmp(head, "O3") == 0)
        {
            info->o3_id = species_number;
        }
        else if (strcasecmp(head, "HNO3") == 0)
        {
            info->hno3_id = species_number;
        }
        else if (strcasecmp(head, "CH4") == 0)
        {
            info->ch4_id = species_number;
        }
        else if (strcasecmp(head, "N2O") == 0)
        {
            info->n2o_id = species_number;
        }
        else if (strcasecmp(head, "NO2") == 0)
        {
            info->no2_id = species_number;
        }
        else if (strcasecmp(head, "F11") == 0)
        {
            info->f11_id = species_number;
        }
        else if (strcasecmp(head, "CLNO") == 0)
        {
            info->clno_id = species_number;
        }
        else if (strcasecmp(head, "N2O5") == 0)
        {
            info->n2o5_id = species_number;
        }
        else if (strcasecmp(head, "F12") == 0)
        {
            info->f12_id = species_number;
        }

        species_number++;
        head = tail;
    }
    free(order_of_species);

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
        if (info->h2o_id != -1)
        {
            if (init_cursors_for_dataset("h2o_retrieval_mds", info, &info->h2o_cursor) != 0)
            {
                return -1;
            }
        }
        if (info->o3_id != -1)
        {
            if (init_cursors_for_dataset("o3_retrieval_mds", info, &info->o3_cursor) != 0)
            {
                return -1;
            }
        }
        if (info->hno3_id != -1)
        {
            if (init_cursors_for_dataset("hno3_retrieval_mds", info, &info->hno3_cursor) != 0)
            {
                return -1;
            }
        }
        if (info->ch4_id != -1)
        {
            if (init_cursors_for_dataset("ch4_retrieval_mds", info, &info->ch4_cursor) != 0)
            {
                return -1;
            }
        }
        if (info->n2o_id != -1)
        {
            if (init_cursors_for_dataset("n2o_retrieval_mds", info, &info->n2o_cursor) != 0)
            {
                return -1;
            }
        }
        if (info->no2_id != -1)
        {
            if (init_cursors_for_dataset("no2_retrieval_mds", info, &info->no2_cursor) != 0)
            {
                return -1;
            }
        }
        if (info->f11_id != -1)
        {
            if (init_cursors_for_dataset("f11_retrieval_mds", info, &info->f11_cursor) != 0)
            {
                return -1;
            }
        }
        if (info->clno_id != -1)
        {
            if (init_cursors_for_dataset("clno_retrieval_mds", info, &info->clno_cursor) != 0)
            {
                return -1;
            }
        }
        if (info->n2o5_id != -1)
        {
            if (init_cursors_for_dataset("n2o5_retrieval_mds", info, &info->n2o5_cursor) != 0)
            {
                return -1;
            }
        }
        if (info->f12_id != -1)
        {
            if (init_cursors_for_dataset("f12_retrieval_mds", info, &info->f12_cursor) != 0)
            {
                return -1;
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
    if (info->h2o_cursor != NULL)
    {
        free(info->h2o_cursor);
    }
    if (info->o3_cursor != NULL)
    {
        free(info->o3_cursor);
    }
    if (info->hno3_cursor != NULL)
    {
        free(info->hno3_cursor);
    }
    if (info->ch4_cursor != NULL)
    {
        free(info->ch4_cursor);
    }
    if (info->n2o_cursor != NULL)
    {
        free(info->n2o_cursor);
    }
    if (info->no2_cursor != NULL)
    {
        free(info->no2_cursor);
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
    info->product_version = -1;
    info->num_main = -1;
    info->max_num_altitudes = 0;
    info->num_altitudes = NULL;
    info->scan_cursor = NULL;
    info->geo_cursor = NULL;
    info->pt_cursor = NULL;
    info->h2o_cursor = NULL;
    info->o3_cursor = NULL;
    info->hno3_cursor = NULL;
    info->ch4_cursor = NULL;
    info->n2o_cursor = NULL;
    info->no2_cursor = NULL;
    info->f11_cursor = NULL;
    info->clno_cursor = NULL;
    info->n2o5_cursor = NULL;
    info->f12_cursor = NULL;
    info->h2o_id = -1;
    info->o3_id = -1;
    info->hno3_id = -1;
    info->ch4_id = -1;
    info->n2o_id = -1;
    info->no2_id = -1;
    info->f11_id = -1;
    info->clno_id = -1;
    info->n2o5_id = -1;
    info->f12_id = -1;
    info->lrv = NULL;

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
    else
    {
        info->num_species = 10;
    }
    if (init_species_numbers(info) != 0)
    {
        ingestion_done(info);
        return -1;
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
    long num_altitudes;
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
    reverse_array(data.double_data, num_elements);

    num_altitudes = info->num_altitudes[index];
    num_elements = sqrt(num_elements);
    num_i = num_elements;
    for (i = num_altitudes - 1; i >= 0; i--)
    {
        if (lrv[num_altitudes - i - 1])
        {
            long num_j = num_elements;

            num_i--;
            assert(num_i >= 0);
            for (j = num_altitudes - 1; j >= 0; j--)
            {
                if (lrv[num_altitudes - j - 1])
                {
                    num_j--;
                    assert(num_j >= 0);
                    data.double_data[i * num_altitudes + j] = data.double_data[num_i * num_elements + num_j];
                }
                else
                {
                    data.double_data[i * num_altitudes + j] = harp_nan();
                }
            }
            assert(num_j == 0);
        }
        else
        {
            for (j = 0; j < num_altitudes; j++)
            {
                data.double_data[i * num_altitudes + j] = harp_nan();
            }
        }
    }
    assert(num_i == 0);

    return 0;
}

static int get_profile_data(ingest_info *info, const coda_cursor *mds_cursor, const char *fieldname, long index,
                            uint8_t *lrv, harp_array data)
{
    coda_cursor cursor;
    long num_altitudes;
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
    reverse_array(data.double_data, num_elements);

    num_altitudes = info->num_altitudes[index];
    for (i = num_altitudes - 1; i >= 0; i--)
    {
        if (lrv[num_altitudes - i - 1])
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

    return 0;
}

static int get_profile_stdev_data(ingest_info *info, const coda_cursor *mds_cursor, const char *fieldname, long index,
                                  uint8_t *lrv, harp_array data)
{
    coda_cursor cursor;
    long num_altitudes;
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
    num_pts = sqrt(2 * num_elements);   /* when rounded down this will thus give our number of num_pts */

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

        /* stdev = sqrt(variance) */
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
    reverse_array(data.double_data, num_pts);

    num_altitudes = info->num_altitudes[index];
    for (i = num_altitudes - 1; i >= 0; i--)
    {
        if (lrv[num_altitudes - i - 1])
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

static int exclude_angles(void *user_data)
{
    return ((ingest_info *)user_data)->product_version < 1;
}

static int exclude_akm(void *user_data)
{
    return ((ingest_info *)user_data)->product_version < 2;
}

static int exclude_v3_species(void *user_data)
{
    return ((ingest_info *)user_data)->product_version < 3;
}

static int read_datetime(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (get_data(&info->geo_cursor[index], "dsr_time", NULL, data) != 0)
    {
        return -1;
    }
    *data.double_data = *data.double_data / 86400;      /* Convert seconds to days */
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
    reverse_array(data.double_data, num_altitudes);

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

static int read_pressure_stdev(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return get_profile_stdev_data(info, &info->pt_cursor[index], "tan_press_var_cov", index,
                                  &info->lrv[index * (info->num_species + 1) * info->max_num_altitudes], data);
}

static int read_temperature(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return get_profile_data(info, &info->pt_cursor[index], "temp", index,
                            &info->lrv[index * (info->num_species + 1) * info->max_num_altitudes], data);
}

static int read_temperature_stdev(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return get_profile_stdev_data(info, &info->pt_cursor[index], "temp_var_cov", index,
                                  &info->lrv[index * (info->num_species + 1) * info->max_num_altitudes], data);
}

static int read_h2o(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return get_profile_data(info, &info->h2o_cursor[index], "conc_alt", index,
                            &info->lrv[(index * (info->num_species + 1) + info->h2o_id + 1) * info->max_num_altitudes],
                            data);
}

static int read_h2o_stdev(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return get_profile_stdev_data(info, &info->h2o_cursor[index], "conc_var_cov", index,
                                  &info->lrv[(index * (info->num_species + 1) + info->h2o_id + 1) *
                                             info->max_num_altitudes], data);
}

static int read_o3(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return get_profile_data(info, &info->o3_cursor[index], "conc_alt", index,
                            &info->lrv[(index * (info->num_species + 1) + info->o3_id + 1) * info->max_num_altitudes],
                            data);
}

static int read_o3_stdev(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return get_profile_stdev_data(info, &info->o3_cursor[index], "conc_var_cov", index,
                                  &info->lrv[(index * (info->num_species + 1) + info->o3_id + 1) *
                                             info->max_num_altitudes], data);
}

static int read_hno3(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return get_profile_data(info, &info->hno3_cursor[index], "conc_alt", index,
                            &info->lrv[(index * (info->num_species + 1) + info->hno3_id + 1) * info->max_num_altitudes],
                            data);
}

static int read_hno3_stdev(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return get_profile_stdev_data(info, &info->hno3_cursor[index], "conc_var_cov", index,
                                  &info->lrv[(index * (info->num_species + 1) + info->hno3_id + 1) *
                                             info->max_num_altitudes], data);
}

static int read_ch4(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return get_profile_data(info, &info->ch4_cursor[index], "conc_alt", index,
                            &info->lrv[(index * (info->num_species + 1) + info->ch4_id + 1) * info->max_num_altitudes],
                            data);
}

static int read_ch4_stdev(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return get_profile_stdev_data(info, &info->ch4_cursor[index], "conc_var_cov", index,
                                  &info->lrv[(index * (info->num_species + 1) + info->ch4_id + 1) *
                                             info->max_num_altitudes], data);
}

static int read_n2o(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return get_profile_data(info, &info->n2o_cursor[index], "conc_alt", index,
                            &info->lrv[(index * (info->num_species + 1) + info->n2o_id + 1) * info->max_num_altitudes],
                            data);
}

static int read_n2o_stdev(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return get_profile_stdev_data(info, &info->n2o_cursor[index], "conc_var_cov", index,
                                  &info->lrv[(index * (info->num_species + 1) + info->n2o_id + 1) *
                                             info->max_num_altitudes], data);
}

static int read_no2(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return get_profile_data(info, &info->no2_cursor[index], "conc_alt", index,
                            &info->lrv[(index * (info->num_species + 1) + info->no2_id + 1) * info->max_num_altitudes],
                            data);
}

static int read_no2_stdev(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return get_profile_stdev_data(info, &info->no2_cursor[index], "conc_var_cov", index,
                                  &info->lrv[(index * (info->num_species + 1) + info->no2_id + 1) *
                                             info->max_num_altitudes], data);
}

static int read_f11(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return get_profile_data(info, &info->f11_cursor[index], "conc_alt", index,
                            &info->lrv[(index * (info->num_species + 1) + info->f11_id + 1) * info->max_num_altitudes],
                            data);
}

static int read_f11_stdev(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return get_profile_stdev_data(info, &info->f11_cursor[index], "conc_var_cov", index,
                                  &info->lrv[(index * (info->num_species + 1) + info->f11_id + 1) *
                                             info->max_num_altitudes], data);
}

static int read_clno(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return get_profile_data(info, &info->clno_cursor[index], "conc_alt", index,
                            &info->lrv[(index * (info->num_species + 1) + info->clno_id + 1) * info->max_num_altitudes],
                            data);
}

static int read_clno_stdev(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return get_profile_stdev_data(info, &info->clno_cursor[index], "conc_var_cov", index,
                                  &info->lrv[(index * (info->num_species + 1) + info->clno_id + 1) *
                                             info->max_num_altitudes], data);
}

static int read_n2o5(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return get_profile_data(info, &info->n2o5_cursor[index], "conc_alt", index,
                            &info->lrv[(index * (info->num_species + 1) + info->n2o5_id + 1) * info->max_num_altitudes],
                            data);
}

static int read_n2o5_stdev(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return get_profile_stdev_data(info, &info->n2o5_cursor[index], "conc_var_cov", index,
                                  &info->lrv[(index * (info->num_species + 1) + info->n2o5_id + 1) *
                                             info->max_num_altitudes], data);
}

static int read_f12(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return get_profile_data(info, &info->f12_cursor[index], "conc_alt", index,
                            &info->lrv[(index * (info->num_species + 1) + info->f12_id + 1) * info->max_num_altitudes],
                            data);
}

static int read_f12_stdev(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return get_profile_stdev_data(info, &info->f12_cursor[index], "conc_var_cov", index,
                                  &info->lrv[(index * (info->num_species + 1) + info->f12_id + 1) *
                                             info->max_num_altitudes], data);
}

static int read_h2o_vmr(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return get_profile_data(info, &info->h2o_cursor[index], "vmr", index,
                            &info->lrv[(index * (info->num_species + 1) + info->h2o_id + 1) * info->max_num_altitudes],
                            data);
}

static int read_h2o_vmr_stdev(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return get_profile_stdev_data(info, &info->h2o_cursor[index], "vmr_var_cov", index,
                                  &info->lrv[(index * (info->num_species + 1) + info->h2o_id + 1) *
                                             info->max_num_altitudes], data);
}

static int read_o3_vmr(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return get_profile_data(info, &info->o3_cursor[index], "vmr", index,
                            &info->lrv[(index * (info->num_species + 1) + info->o3_id + 1) * info->max_num_altitudes],
                            data);
}

static int read_o3_vmr_stdev(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return get_profile_stdev_data(info, &info->o3_cursor[index], "vmr_var_cov", index,
                                  &info->lrv[(index * (info->num_species + 1) + info->o3_id + 1) *
                                             info->max_num_altitudes], data);
}

static int read_hno3_vmr(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return get_profile_data(info, &info->hno3_cursor[index], "vmr", index,
                            &info->lrv[(index * (info->num_species + 1) + info->hno3_id + 1) *
                                       info->max_num_altitudes], data);
}

static int read_hno3_vmr_stdev(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return get_profile_stdev_data(info, &info->hno3_cursor[index], "vmr_var_cov", index,
                                  &info->lrv[(index * (info->num_species + 1) + info->hno3_id + 1) *
                                             info->max_num_altitudes], data);
}

static int read_ch4_vmr(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return get_profile_data(info, &info->ch4_cursor[index], "vmr", index,
                            &info->lrv[(index * (info->num_species + 1) + info->ch4_id + 1) * info->max_num_altitudes],
                            data);
}

static int read_ch4_vmr_stdev(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return get_profile_stdev_data(info, &info->ch4_cursor[index], "vmr_var_cov", index,
                                  &info->lrv[(index * (info->num_species + 1) + info->ch4_id + 1) *
                                             info->max_num_altitudes], data);
}

static int read_n2o_vmr(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return get_profile_data(info, &info->n2o_cursor[index], "vmr", index,
                            &info->lrv[(index * (info->num_species + 1) + info->n2o_id + 1) *
                                       info->max_num_altitudes], data);
}

static int read_n2o_vmr_stdev(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return get_profile_stdev_data(info, &info->n2o_cursor[index], "vmr_var_cov", index,
                                  &info->lrv[(index * (info->num_species + 1) + info->n2o_id + 1) *
                                             info->max_num_altitudes], data);
}

static int read_no2_vmr(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return get_profile_data(info, &info->no2_cursor[index], "vmr", index,
                            &info->lrv[(index * (info->num_species + 1) + info->no2_id + 1) * info->max_num_altitudes],
                            data);
}

static int read_no2_vmr_stdev(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return get_profile_stdev_data(info, &info->no2_cursor[index], "vmr_var_cov", index,
                                  &info->lrv[(index * (info->num_species + 1) + info->no2_id + 1) *
                                             info->max_num_altitudes], data);
}

static int read_f11_vmr(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return get_profile_data(info, &info->f11_cursor[index], "vmr", index,
                            &info->lrv[(index * (info->num_species + 1) + info->f11_id + 1) * info->max_num_altitudes],
                            data);
}

static int read_f11_vmr_stdev(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return get_profile_stdev_data(info, &info->f11_cursor[index], "vmr_var_cov", index,
                                  &info->lrv[(index * (info->num_species + 1) + info->f11_id + 1) *
                                             info->max_num_altitudes], data);
}

static int read_clno_vmr(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return get_profile_data(info, &info->clno_cursor[index], "vmr", index,
                            &info->lrv[(index * (info->num_species + 1) + info->clno_id + 1) * info->max_num_altitudes],
                            data);
}

static int read_clno_vmr_stdev(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return get_profile_stdev_data(info, &info->clno_cursor[index], "vmr_var_cov", index,
                                  &info->lrv[(index * (info->num_species + 1) + info->clno_id + 1) *
                                             info->max_num_altitudes], data);
}

static int read_n2o5_vmr(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return get_profile_data(info, &info->n2o5_cursor[index], "vmr", index,
                            &info->lrv[(index * (info->num_species + 1) + info->n2o5_id + 1) * info->max_num_altitudes],
                            data);
}

static int read_n2o5_vmr_stdev(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return get_profile_stdev_data(info, &info->n2o5_cursor[index], "vmr_var_cov", index,
                                  &info->lrv[(index * (info->num_species + 1) + info->n2o5_id + 1) *
                                             info->max_num_altitudes], data);
}

static int read_f12_vmr(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return get_profile_data(info, &info->f12_cursor[index], "vmr", index,
                            &info->lrv[(index * (info->num_species + 1) + info->f12_id + 1) * info->max_num_altitudes],
                            data);
}

static int read_f12_vmr_stdev(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return get_profile_stdev_data(info, &info->f12_cursor[index], "vmr_var_cov", index,
                                  &info->lrv[(index * (info->num_species + 1) + info->f12_id + 1) *
                                             info->max_num_altitudes], data);
}

static int read_h2o_akm_vmr(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return get_akm_data(info, &info->h2o_cursor[index], index,
                        &info->lrv[(index * (info->num_species + 1) + info->h2o_id + 1) * info->max_num_altitudes],
                        data);
}

static int read_o3_akm_vmr(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return get_akm_data(info, &info->o3_cursor[index], index,
                        &info->lrv[(index * (info->num_species + 1) + info->o3_id + 1) * info->max_num_altitudes],
                        data);
}

static int read_hno3_akm_vmr(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return get_akm_data(info, &info->hno3_cursor[index], index,
                        &info->lrv[(index * (info->num_species + 1) + info->hno3_id + 1) * info->max_num_altitudes],
                        data);
}

static int read_ch4_akm_vmr(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return get_akm_data(info, &info->ch4_cursor[index], index,
                        &info->lrv[(index * (info->num_species + 1) + info->ch4_id + 1) * info->max_num_altitudes],
                        data);
}

static int read_n2o_akm_vmr(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return get_akm_data(info, &info->n2o_cursor[index], index,
                        &info->lrv[(index * (info->num_species + 1) + info->n2o_id + 1) * info->max_num_altitudes],
                        data);
}

static int read_no2_akm_vmr(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return get_akm_data(info, &info->no2_cursor[index], index,
                        &info->lrv[(index * (info->num_species + 1) + info->no2_id + 1) * info->max_num_altitudes],
                        data);
}

static int read_f11_akm_vmr(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return get_akm_data(info, &info->f11_cursor[index], index,
                        &info->lrv[(index * (info->num_species + 1) + info->f11_id + 1) * info->max_num_altitudes],
                        data);
}

static int read_clno_akm_vmr(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return get_akm_data(info, &info->clno_cursor[index], index,
                        &info->lrv[(index * (info->num_species + 1) + info->clno_id + 1) * info->max_num_altitudes],
                        data);
}

static int read_n2o5_akm_vmr(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return get_akm_data(info, &info->n2o5_cursor[index], index,
                        &info->lrv[(index * (info->num_species + 1) + info->n2o5_id + 1) * info->max_num_altitudes],
                        data);
}

static int read_f12_akm_vmr(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return get_akm_data(info, &info->f12_cursor[index], index,
                        &info->lrv[(index * (info->num_species + 1) + info->f12_id + 1) * info->max_num_altitudes],
                        data);
}

int harp_ingestion_module_mip_nl__2p_init(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[3];
    const char *description;
    const char *path;

    description = "MIPAS Temperature, Pressure, and Atmospheric Constituents Profiles";
    module = harp_ingestion_register_module_coda("MIP_NL__2P", "ENVISAT_MIPAS", "MIP_NL__2P", description, NULL,
                                                 ingestion_init, ingestion_done);

    description = "profile data";
    product_definition = harp_ingestion_register_product(module, "MIPAS_NL_L2", description, read_dimensions);

    dimension_type[0] = harp_dimension_time;
    dimension_type[1] = harp_dimension_vertical;
    dimension_type[2] = harp_dimension_vertical;

    description = "start time of the measurement";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition, "datetime", harp_type_double,
                                                                       1, dimension_type, NULL, description,
                                                                       "days since 2000-01-01", NULL, read_datetime);
    path = "/scan_geolocation_ads[]/dsr_time[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "altitude";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition, "altitude", harp_type_double,
                                                                       2, dimension_type, NULL, description, "km", NULL,
                                                                       read_altitude);
    path = "/scan_information_mds[]/tangent_altitude_los[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "latitude";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition, "latitude", harp_type_double,
                                                                       1, dimension_type, NULL, description,
                                                                       "degree_north", NULL, read_latitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    path = "/scan_geolocation_ads[]/loc_mid[]/latitude";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "longitude";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition, "longitude",
                                                                       harp_type_double, 1, dimension_type, NULL,
                                                                       description, "degree_east", NULL,
                                                                       read_longitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0, 180.0);
    path = "/scan_geolocation_ads[]/loc_mid[]/longitude";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* Angles. */
    description = "solar elevation angle (target to sun)";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition, "solar_elevation_angle",
                                                                       harp_type_double, 1, dimension_type, NULL,
                                                                       description, "degree", exclude_angles,
                                                                       read_solar_elevation_angle);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    path = "/scan_geolocation_ads[]/target_sun_elev";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "solar azimuth angle (target to sun)";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition, "solar_azimuth_angle",
                                                                       harp_type_double, 1, dimension_type, NULL,
                                                                       description, "degree", exclude_angles,
                                                                       read_solar_azimuth_angle);
    harp_variable_definition_set_valid_range_double(variable_definition, 0.0, 360.0);
    path = "/scan_geolocation_ads[]/target_sun_azi";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "line-of-sight azimuth angle (satellite to target)";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition, "viewing_azimuth_angle",
                                                                       harp_type_double, 1, dimension_type, NULL,
                                                                       description, "degree", exclude_angles,
                                                                       read_los_azimuth_angle);
    harp_variable_definition_set_valid_range_double(variable_definition, 0.0, 360.0);
    path = "/scan_geolocation_ads[]/sat_target_azi";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* Pressure and temperature profiles. */
    description = "pressure";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition, "pressure", harp_type_double,
                                                                       2, dimension_type, NULL, description, "hPa",
                                                                       NULL, read_pressure);
    path = "/pt_retrieval_mds[]/pressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "pressure standard deviation";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition, "pressure_stdev",
                                                                       harp_type_double, 2, dimension_type, NULL,
                                                                       description, "hPa", NULL, read_pressure_stdev);
    path = "/pt_retrieval_mds[]/tan_press_var_cov[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "temperature";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition, "temperature",
                                                                       harp_type_double, 2, dimension_type, NULL,
                                                                       description, "K", NULL, read_temperature);
    path = "/pt_retrieval_mds[]/temp[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "temperature standard deviation";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition, "temperature_stdev",
                                                                       harp_type_double, 2, dimension_type, NULL,
                                                                       description, "K", NULL, read_temperature_stdev);
    path = "/pt_retrieval_mds[]/temp_var_cov[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* Number density profiles. */
    description = "H2O number density";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition, "H2O_number_density",
                                                                       harp_type_double, 2, dimension_type, NULL,
                                                                       description, "molec/cm^3", NULL, read_h2o);
    path = "/h2o_retrieval_mds[]/conc_alt[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "standard deviation for the H2O number density";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition, "H2O_number_density_stdev",
                                                                       harp_type_double, 2, dimension_type, NULL,
                                                                       description, "molec/cm^3", NULL, read_h2o_stdev);
    path = "/h2o_retrieval_mds[]/conc_var_cov[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "O3 number density";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition, "O3_number_density",
                                                                       harp_type_double, 2, dimension_type, NULL,
                                                                       description, "molec/cm^3", NULL, read_o3);
    path = "/o3_retrieval_mds[]/conc_alt[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "standard deviation for the O3 number density";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition, "O3_number_density_stdev",
                                                                       harp_type_double, 2, dimension_type, NULL,
                                                                       description, "molec/cm^3", NULL, read_o3_stdev);
    path = "/o3_retrieval_mds[]/conc_var_cov[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "HNO3 number density";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition, "HNO3_number_density",
                                                                       harp_type_double, 2, dimension_type, NULL,
                                                                       description, "molec/cm^3", NULL, read_hno3);
    path = "/hno3_retrieval_mds[]/conc_alt[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "standard deviation for the HNO3 number density";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition, "HNO3_number_density_stdev",
                                                                       harp_type_double, 2, dimension_type, NULL,
                                                                       description, "molec/cm^3", NULL,
                                                                       read_hno3_stdev);
    path = "/hno3_retrieval_mds[]/conc_var_cov[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "CH4 number density";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition, "CH4_number_density",
                                                                       harp_type_double, 2, dimension_type, NULL,
                                                                       description, "molec/cm^3", NULL, read_ch4);
    path = "/ch4_retrieval_mds[]/conc_alt[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "standard deviation for the CH4 number density";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition, "CH4_number_density_stdev",
                                                                       harp_type_double, 2, dimension_type, NULL,
                                                                       description, "molec/cm^3", NULL, read_ch4_stdev);
    path = "/ch4_retrieval_mds[]/conc_var_cov[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "N2O number density";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition, "N2O_number_density",
                                                                       harp_type_double, 2, dimension_type, NULL,
                                                                       description, "molec/cm^3", NULL, read_n2o);
    path = "/n2o_retrieval_mds[]/conc_alt[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "standard deviation for the N2O number density";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition, "N2O_number_density_stdev",
                                                                       harp_type_double, 2, dimension_type, NULL,
                                                                       description, "molec/cm^3", NULL, read_n2o_stdev);
    path = "/n2o_retrieval_mds[]/conc_var_cov[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "NO2 number density";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition, "NO2_number_density",
                                                                       harp_type_double, 2, dimension_type, NULL,
                                                                       description, "molec/cm^3", NULL, read_no2);
    path = "/no2_retrieval_mds[]/conc_alt[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "standard deviation for the NO2 number density";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition, "NO2_number_density_stdev",
                                                                       harp_type_double, 2, dimension_type, NULL,
                                                                       description, "molec/cm^3", NULL, read_no2_stdev);
    path = "/no2_retrieval_mds[]/conc_var_cov[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "F11 number density";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition, "F11_number_density",
                                                                       harp_type_double, 2, dimension_type, NULL,
                                                                       description, "molec/cm^3", exclude_v3_species,
                                                                       read_f11);
    path = "/f11_retrieval_mds[]/conc_alt[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "standard deviation for the F11 number density";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition, "F11_number_density_stdev",
                                                                       harp_type_double, 2, dimension_type, NULL,
                                                                       description, "molec/cm^3", exclude_v3_species,
                                                                       read_f11_stdev);
    path = "/f11_retrieval_mds[]/conc_var_cov[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "ClNO number density";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition, "ClNO_number_density",
                                                                       harp_type_double, 2, dimension_type, NULL,
                                                                       description, "molec/cm^3", exclude_v3_species,
                                                                       read_clno);
    path = "/clno_retrieval_mds[]/conc_alt[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "standard deviation for the ClNO number density";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition, "ClNO_number_density_stdev",
                                                                       harp_type_double, 2, dimension_type, NULL,
                                                                       description, "molec/cm^3", exclude_v3_species,
                                                                       read_clno_stdev);
    path = "/clno_retrieval_mds[]/conc_var_cov[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "N2O5 number density";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition, "N2O5_number_density",
                                                                       harp_type_double, 2, dimension_type, NULL,
                                                                       description, "molec/cm^3", exclude_v3_species,
                                                                       read_n2o5);
    path = "/n2o5_retrieval_mds[]/conc_alt[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "standard deviation for the N2O5 number density";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition, "N2O5_number_density_stdev",
                                                                       harp_type_double, 2, dimension_type, NULL,
                                                                       description, "molec/cm^3", exclude_v3_species,
                                                                       read_n2o5_stdev);
    path = "/n2o5_retrieval_mds[]/conc_var_cov[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "F12 number density";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition, "F12_number_density",
                                                                       harp_type_double, 2, dimension_type, NULL,
                                                                       description, "molec/cm^3", exclude_v3_species,
                                                                       read_f12);
    path = "/f12_retrieval_mds[]/conc_alt[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "standard deviation for the F12 number density";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition, "F12_number_density_stdev",
                                                                       harp_type_double, 2, dimension_type, NULL,
                                                                       description, "molec/cm^3", exclude_v3_species,
                                                                       read_f12_stdev);
    path = "/f12_retrieval_mds[]/conc_var_cov[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* Volume mixing ratio profiles. */
    description = "H2O volume mixing ratio";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition, "H2O_volume_mixing_ratio",
                                                                       harp_type_double, 2, dimension_type, NULL,
                                                                       description, "ppmv", NULL, read_h2o_vmr);
    path = "/h2o_retrieval_mds[]/vmr[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "standard deviation for the H2O volume mixing ratio";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition,
                                                                       "H2O_volume_mixing_ratio_stdev",
                                                                       harp_type_double, 2, dimension_type, NULL,
                                                                       description, "ppmv", NULL, read_h2o_vmr_stdev);
    path = "/h2o_retrieval_mds[]/vmr_var_cov[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "O3 volume mixing ratio";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition, "O3_volume_mixing_ratio",
                                                                       harp_type_double, 2, dimension_type, NULL,
                                                                       description, "ppmv", NULL, read_o3_vmr);
    path = "/o3_retrieval_mds[]/vmr[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "standard deviation for the O3 volume mixing ratio";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition,
                                                                       "O3_volume_mixing_ratio_stdev", harp_type_double,
                                                                       2, dimension_type, NULL, description, "ppmv",
                                                                       NULL, read_o3_vmr_stdev);
    path = "/o3_retrieval_mds[]/vmr_var_cov[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "HNO3 volume mixing ratio";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition, "HNO3_volume_mixing_ratio",
                                                                       harp_type_double, 2, dimension_type, NULL,
                                                                       description, "ppmv", NULL, read_hno3_vmr);
    path = "/hno3_retrieval_mds[]/vmr[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "standard deviation for the HNO3 volume mixing ratio";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition,
                                                                       "HNO3_volume_mixing_ratio_stdev",
                                                                       harp_type_double, 2, dimension_type, NULL,
                                                                       description, "ppmv", NULL, read_hno3_vmr_stdev);
    path = "/hno3_retrieval_mds[]/vmr_var_cov[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "CH4 volume mixing ratio";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition, "CH4_volume_mixing_ratio",
                                                                       harp_type_double, 2, dimension_type, NULL,
                                                                       description, "ppmv", NULL, read_ch4_vmr);
    path = "/ch4_retrieval_mds[]/vmr[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "standard deviation for the CH4 volume mixing ratio";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition,
                                                                       "CH4_volume_mixing_ratio_stdev",
                                                                       harp_type_double, 2, dimension_type, NULL,
                                                                       description, "ppmv", NULL, read_ch4_vmr_stdev);
    path = "/ch4_retrieval_mds[]/vmr_var_cov[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "N2O volume mixing ratio";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition, "N2O_volume_mixing_ratio",
                                                                       harp_type_double, 2, dimension_type, NULL,
                                                                       description, "ppmv", NULL, read_n2o_vmr);
    path = "/n2o_retrieval_mds[]/vmr[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "standard deviation for the N2O volume mixing ratio";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition,
                                                                       "N2O_volume_mixing_ratio_stdev",
                                                                       harp_type_double, 2, dimension_type, NULL,
                                                                       description, "ppmv", NULL, read_n2o_vmr_stdev);
    path = "/n2o_retrieval_mds[]/vmr_var_cov[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "NO2 volume mixing ratio";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition, "NO2_volume_mixing_ratio",
                                                                       harp_type_double, 2, dimension_type, NULL,
                                                                       description, "ppmv", NULL, read_no2_vmr);
    path = "/no2_retrieval_mds[]/vmr[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "standard deviation for the NO2 volume mixing ratio";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition,
                                                                       "NO2_volume_mixing_ratio_stdev",
                                                                       harp_type_double, 2, dimension_type, NULL,
                                                                       description, "ppmv", NULL, read_no2_vmr_stdev);
    path = "/no2_retrieval_mds[]/vmr_var_cov[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "F11 volume mixing ratio";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition, "F11_volume_mixing_ratio",
                                                                       harp_type_double, 2, dimension_type, NULL,
                                                                       description, "ppmv", exclude_v3_species,
                                                                       read_f11_vmr);
    path = "/f11_retrieval_mds[]/vmr[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "standard deviation for the F11 volume mixing ratio";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition,
                                                                       "F11_volume_mixing_ratio_stdev",
                                                                       harp_type_double, 2, dimension_type, NULL,
                                                                       description, "ppmv", exclude_v3_species,
                                                                       read_f11_vmr_stdev);
    path = "/f11_retrieval_mds[]/vmr_var_cov[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "ClNO volume mixing ratio";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition, "ClNO_volume_mixing_ratio",
                                                                       harp_type_double, 2, dimension_type, NULL,
                                                                       description, "ppmv", exclude_v3_species,
                                                                       read_clno_vmr);
    path = "/clno_retrieval_mds[]/vmr[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "standard deviation for the ClNO volume mixing ratio";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition,
                                                                       "ClNO_volume_mixing_ratio_stdev",
                                                                       harp_type_double, 2, dimension_type, NULL,
                                                                       description, "ppmv", exclude_v3_species,
                                                                       read_clno_vmr_stdev);
    path = "/clno_retrieval_mds[]/vmr_var_cov[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "N2O5 volume mixing ratio";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition, "N2O5_volume_mixing_ratio",
                                                                       harp_type_double, 2, dimension_type, NULL,
                                                                       description, "ppmv", exclude_v3_species,
                                                                       read_n2o5_vmr);
    path = "/n2o5_retrieval_mds[]/vmr[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "standard deviation for the N2O5 volume mixing ratio";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition,
                                                                       "N2O5_volume_mixing_ratio_stdev",
                                                                       harp_type_double, 2, dimension_type, NULL,
                                                                       description, "ppmv", exclude_v3_species,
                                                                       read_n2o5_vmr_stdev);
    path = "/n2o5_retrieval_mds[]/vmr_var_cov[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "F12 volume mixing ratio";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition, "F12_volume_mixing_ratio",
                                                                       harp_type_double, 2, dimension_type, NULL,
                                                                       description, "ppmv", exclude_v3_species,
                                                                       read_f12_vmr);
    path = "/f12_retrieval_mds[]/vmr[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "standard deviation for the F12 volume mixing ratio";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition,
                                                                       "F12_volume_mixing_ratio_stdev",
                                                                       harp_type_double, 2, dimension_type, NULL,
                                                                       description, "ppmv", exclude_v3_species,
                                                                       read_f12_vmr_stdev);
    path = "/f12_retrieval_mds[]/vmr_var_cov[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* Volume mixing ratio profile averaging kernel matrices. */
    description = "averaging kernel matrix";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition,
                                                                       "H2O_volume_mixing_ratio_avk",
                                                                       harp_type_double, 3, dimension_type, NULL,
                                                                       description, "ppmv/ppmv", exclude_akm,
                                                                       read_h2o_akm_vmr);
    path = "/h2o_retrieval_mds[]/avg_kernel[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    variable_definition = harp_ingestion_register_variable_sample_read(product_definition, "O3_volume_mixing_ratio_avk",
                                                                       harp_type_double, 3, dimension_type, NULL,
                                                                       description, "ppmv/ppmv", exclude_akm,
                                                                       read_o3_akm_vmr);
    path = "/o3_retrieval_mds[]/avg_kernel[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    variable_definition = harp_ingestion_register_variable_sample_read(product_definition,
                                                                       "HNO3_volume_mixing_ratio_avk",
                                                                       harp_type_double, 3, dimension_type, NULL,
                                                                       description, "ppmv/ppmv", exclude_akm,
                                                                       read_hno3_akm_vmr);
    path = "/hno3_retrieval_mds[]/avg_kernel[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    variable_definition = harp_ingestion_register_variable_sample_read(product_definition,
                                                                       "CH4_volume_mixing_ratio_avk", harp_type_double,
                                                                       3, dimension_type, NULL, description,
                                                                       "ppmv/ppmv", exclude_akm, read_ch4_akm_vmr);
    path = "/ch4_retrieval_mds[]/avg_kernel[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    variable_definition = harp_ingestion_register_variable_sample_read(product_definition,
                                                                       "N2O_volume_mixing_ratio_avk", harp_type_double,
                                                                       3, dimension_type, NULL, description,
                                                                       "ppmv/ppmv", exclude_akm, read_n2o_akm_vmr);
    path = "/n2o_retrieval_mds[]/avg_kernel[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    variable_definition = harp_ingestion_register_variable_sample_read(product_definition,
                                                                       "NO2_volume_mixing_ratio_avk",
                                                                       harp_type_double, 3, dimension_type, NULL,
                                                                       description, "ppmv/ppmv", exclude_akm,
                                                                       read_no2_akm_vmr);
    path = "/no2_retrieval_mds[]/avg_kernel[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    variable_definition = harp_ingestion_register_variable_sample_read(product_definition,
                                                                       "F11_volume_mixing_ratio_avk", harp_type_double,
                                                                       3, dimension_type, NULL, description,
                                                                       "ppmv/ppmv", exclude_v3_species,
                                                                       read_f11_akm_vmr);
    path = "/f11_retrieval_mds[]/avg_kernel[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    variable_definition = harp_ingestion_register_variable_sample_read(product_definition,
                                                                       "ClNO_volume_mixing_ratio_avk", harp_type_double,
                                                                       3, dimension_type, NULL, description,
                                                                       "ppmv/ppmv", exclude_v3_species,
                                                                       read_clno_akm_vmr);
    path = "/clno_retrieval_mds[]/avg_kernel[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    variable_definition = harp_ingestion_register_variable_sample_read(product_definition,
                                                                       "N2O5_volume_mixing_ratio_avk", harp_type_double,
                                                                       3, dimension_type, NULL, description,
                                                                       "ppmv/ppmv", exclude_v3_species,
                                                                       read_n2o5_akm_vmr);
    path = "/n2o5_retrieval_mds[]/avg_kernel[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    variable_definition = harp_ingestion_register_variable_sample_read(product_definition,
                                                                       "F12_volume_mixing_ratio_avk", harp_type_double,
                                                                       3, dimension_type, NULL, description,
                                                                       "ppmv/ppmv", exclude_v3_species,
                                                                       read_f12_akm_vmr);
    path = "/f12_retrieval_mds[]/avg_kernel[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    return 0;
}
