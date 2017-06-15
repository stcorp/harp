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

#include "harp.h"

#include <stdlib.h>
#include <string.h>

typedef struct collocation_index_slist_struct
{
    long num_indices;
    int32_t *index;
} collocation_index_slist;

static int cmp_index(const void *untyped_a, const void *untyped_b)
{
    int32_t a = *(int32_t *)untyped_a;
    int32_t b = *(int32_t *)untyped_b;

    return (a < b ? -1 : (a > b ? 1 : 0));
}

static void collocation_index_slist_delete(collocation_index_slist *index_slist)
{
    if (index_slist != NULL)
    {
        if (index_slist->index != NULL)
        {
            free(index_slist->index);
        }

        free(index_slist);
    }
}

static int collocation_index_slist_new(long num_indices, const int32_t *index,
                                       collocation_index_slist **new_index_slist)
{
    collocation_index_slist *index_slist;

    index_slist = (collocation_index_slist *)malloc(sizeof(collocation_index_slist));
    if (index_slist == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(collocation_index_slist), __FILE__, __LINE__);
        return -1;
    }

    index_slist->num_indices = num_indices;
    index_slist->index = NULL;

    if (num_indices > 0)
    {
        index_slist->index = (int32_t *)malloc(num_indices * sizeof(int32_t));
        if (index_slist->index == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           num_indices * sizeof(int32_t), __FILE__, __LINE__);
            collocation_index_slist_delete(index_slist);
            return -1;
        }

        memcpy(index_slist->index, index, num_indices * sizeof(int32_t));
        qsort(index_slist->index, num_indices, sizeof(int32_t), cmp_index);
    }

    *new_index_slist = index_slist;
    return 0;
}

static int collocation_index_slist_has_index(collocation_index_slist *index_slist, int32_t index)
{
    long lower_index;
    long upper_index;

    /* Use binary search to check if the index passed in is contained in the (sorted) collocation index list. */
    lower_index = 0;
    upper_index = index_slist->num_indices - 1;

    while (upper_index >= lower_index)
    {
        /* Determine the index that splits the search space into two (approximately) equal halves. */
        long pivot_index = lower_index + ((upper_index - lower_index) / 2);

        /* If the pivot equals the index to be found, terminate early. */
        if (index_slist->index[pivot_index] == index)
        {
            return 1;
        }

        /* If the pivot is smaller than the index to be found, search the upper sub array, otherwise search the lower
         * sub array.
         */
        if (index_slist->index[pivot_index] < index)
        {
            lower_index = pivot_index + 1;
        }
        else
        {
            upper_index = pivot_index - 1;
        }
    }

    return 0;
}

static void mask_logical_and(long num_elements, const uint8_t *source_mask_a, const uint8_t *source_mask_b,
                             uint8_t *target_mask)
{
    long i;

    for (i = 0; i < num_elements; i++)
    {
        target_mask[i] = source_mask_a[i] && source_mask_b[i];
    }
}

static int update_mask_for_product(const harp_collocation_result *collocation_result,
                                   int is_dataset_a, const char *product_path, uint8_t *mask)
{
    harp_product *product;
    harp_variable *collocation_index;
    collocation_index_slist *index_slist;
    long i;

    if (harp_import(product_path, NULL, NULL, &product) != 0)
    {
        return -1;
    }

    if (product->source_product == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "%s: source product undefined", product_path);
        harp_product_delete(product);
        return -1;
    }

    if (harp_product_get_variable_by_name(product, "collocation_index", &collocation_index) != 0)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "%s: variable 'collocation_index' undefined", product_path);
        harp_product_delete(product);
        return -1;
    }

    if (collocation_index->data_type != harp_type_int32)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "%s: invalid data type for variable 'collocation_index' (expected"
                       " '%s')", product_path, harp_get_data_type_name(harp_type_int32));
        harp_product_delete(product);
        return -1;
    }

    if (collocation_index_slist_new(collocation_index->num_elements, collocation_index->data.int32_data, &index_slist)
        != 0)
    {
        harp_product_delete(product);
        return -1;
    }

    for (i = 0; i < collocation_result->num_pairs; i++)
    {
        const harp_collocation_pair *pair = collocation_result->pair[i];
        long index;

        if (is_dataset_a)
        {
            if (harp_dataset_get_index_from_source_product(collocation_result->dataset_a, product->source_product,
                                                           &index) != 0)
            {
                return -1;
            }
            if (pair->product_index_a == index)
            {
                mask[i] = collocation_index_slist_has_index(index_slist, pair->collocation_index);
            }
        }
        else
        {
            if (harp_dataset_get_index_from_source_product(collocation_result->dataset_b, product->source_product,
                                                           &index) != 0)
            {
                return -1;
            }
            if (pair->product_index_b == index)
            {
                mask[i] = collocation_index_slist_has_index(index_slist, pair->collocation_index);
            }
        }
    }

    collocation_index_slist_delete(index_slist);
    harp_product_delete(product);
    return 0;
}

static int get_mask(const harp_collocation_result *collocation_result, int is_dataset_a,
                    const harp_dataset *dataset, uint8_t **new_mask)
{
    uint8_t *mask;
    int i;

    mask = calloc(collocation_result->num_pairs, sizeof(uint8_t));
    if (mask == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       collocation_result->num_pairs * sizeof(uint8_t), __FILE__, __LINE__);
        return -1;
    }

    for (i = 0; i < dataset->num_products; i++)
    {
        if (update_mask_for_product(collocation_result, is_dataset_a, dataset->metadata[i]->filename, mask) != 0)
        {
            free(mask);
            return -1;
        }
    }

    *new_mask = mask;
    return 0;
}

static int update_collocation_result(harp_collocation_result *collocation_result, harp_dataset *dataset_a,
                                     harp_dataset *dataset_b)
{
    uint8_t *mask_a = NULL;
    uint8_t *mask_b = NULL;
    uint8_t *mask = NULL;
    long i;

    if (get_mask(collocation_result, 0, dataset_a, &mask_a) != 0)
    {
        return -1;
    }
    if (get_mask(collocation_result, 0, dataset_b, &mask_b) != 0)
    {
        free(mask_a);
        return -1;
    }

    mask_logical_and(collocation_result->num_pairs, mask_a, mask_b, mask);
    free(mask_a);
    free(mask_b);

    for (i = collocation_result->num_pairs - 1; i >= 0; i--)
    {
        if (!mask[i])
        {
            if (harp_collocation_result_remove_pair_at_index(collocation_result, i) != 0)
            {
                harp_collocation_result_delete(collocation_result);
                free(mask);
                return -1;
            }
        }
    }

    free(mask);

    return 0;
}

int update(int argc, char *argv[])
{
    harp_collocation_result *collocation_result;
    harp_dataset *dataset_a;
    harp_dataset *dataset_b;
    const char *output;

    if (argc < 5 || argc > 6 || argv[2][0] == '-' || argv[3][0] == '-' || argv[4][0] == '-')
    {
        return 1;
    }
    if (argc == 6)
    {
        if (argv[5][0] == '-')
        {
            return 1;
        }
        output = argv[5];
    }
    else
    {
        output = argv[2];
    }

    if (harp_collocation_result_read(argv[2], &collocation_result) != 0)
    {
        return -1;
    }

    if (harp_dataset_new(&dataset_a) != 0)
    {
        harp_collocation_result_delete(collocation_result);
        return -1;
    }
    if (harp_dataset_import(dataset_a, argv[3]) != 0)
    {
        harp_collocation_result_delete(collocation_result);
        harp_dataset_delete(dataset_a);
        return -1;
    }
    if (harp_dataset_new(&dataset_b) != 0)
    {
        harp_collocation_result_delete(collocation_result);
        harp_dataset_delete(dataset_a);
        return -1;
    }
    if (harp_dataset_import(dataset_b, argv[4]) != 0)
    {
        harp_collocation_result_delete(collocation_result);
        harp_dataset_delete(dataset_a);
        harp_dataset_delete(dataset_b);
        return -1;
    }

    if (update_collocation_result(collocation_result, dataset_a, dataset_b) != 0)
    {
        harp_collocation_result_delete(collocation_result);
        harp_dataset_delete(dataset_a);
        harp_dataset_delete(dataset_b);
        return -1;
    }
    harp_dataset_delete(dataset_a);
    harp_dataset_delete(dataset_b);

    if (harp_collocation_result_write(output, collocation_result) != 0)
    {
        harp_collocation_result_delete(collocation_result);
        return -1;
    }
    harp_collocation_result_delete(collocation_result);

    return 0;
}
