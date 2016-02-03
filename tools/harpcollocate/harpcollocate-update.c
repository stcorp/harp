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

#include "harpcollocate.h"

typedef enum dataset_selection_enum
{
    dataset_a,
    dataset_b
} dataset_selection;

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

static const char *select_source_product_a(const harp_collocation_pair *pair)
{
    return pair->source_product_a;
}

static const char *select_source_product_b(const harp_collocation_pair *pair)
{
    return pair->source_product_b;
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
                                   const char *(*key_func) (const harp_collocation_pair *pair),
                                   const char *product_path, uint8_t *mask)
{
    harp_product *product;
    harp_variable *collocation_index;
    collocation_index_slist *index_slist;
    long i;

    if (harp_import(product_path, &product) != 0)
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

        if (strcmp(key_func(pair), product->source_product) == 0)
        {
            mask[i] = collocation_index_slist_has_index(index_slist, pair->collocation_index);
        }
    }

    collocation_index_slist_delete(index_slist);
    harp_product_delete(product);
    return 0;
}

static int get_mask(const harp_collocation_result *collocation_result, dataset_selection selection,
                    const Dataset *dataset, uint8_t **new_mask)
{
    const char *(*key_func) (const harp_collocation_pair *pair);
    uint8_t *mask;
    int i;

    key_func = (selection == dataset_a ? select_source_product_a : select_source_product_b);

    mask = calloc(collocation_result->num_pairs, sizeof(uint8_t));
    if (mask == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       collocation_result->num_pairs * sizeof(uint8_t), __FILE__, __LINE__);
        return -1;
    }

    for (i = 0; i < dataset->num_files; i++)
    {
        if (update_mask_for_product(collocation_result, key_func, dataset->filename[i], mask) != 0)
        {
            free(mask);
            return -1;
        }
    }

    *new_mask = mask;
    return 0;
}

static void update_collocation_result(harp_collocation_result *collocation_result, const uint8_t *mask)
{
    long new_num_pairs;
    long i;
    long j;

    if (mask == NULL)
    {
        return;
    }

    new_num_pairs = 0;
    for (i = 0; i < collocation_result->num_pairs; i++)
    {
        if (mask[i])
        {
            new_num_pairs++;
        }
    }

    for (i = 0, j = 0; i < collocation_result->num_pairs; i++)
    {
        if (mask[i])
        {
            collocation_result->pair[j] = collocation_result->pair[i];
            j++;
        }
        else
        {
            harp_collocation_pair_delete(collocation_result->pair[i]);
        }
    }

    collocation_result->num_pairs = new_num_pairs;
}

int update(const Collocation_options *collocation_options, harp_collocation_result *collocation_result)
{
    uint8_t *mask_a = NULL;
    uint8_t *mask_b = NULL;
    const uint8_t *mask = NULL;

    /* Validate input arguments */
    if (collocation_options == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "collocation_options is NULL (%s:%u)", __FILE__, __LINE__);
        return -1;
    }

    /* Compute row masks for each dataset. */
    if (collocation_options->dataset_a_in != NULL)
    {
        if (get_mask(collocation_result, dataset_a, collocation_options->dataset_a_in, &mask_a) != 0)
        {
            return -1;
        }
    }

    if (collocation_options->dataset_b_in != NULL)
    {
        if (get_mask(collocation_result, dataset_b, collocation_options->dataset_b_in, &mask_b) != 0)
        {
            if (mask_a != NULL)
            {
                free(mask_a);
            }

            return -1;
        }
    }

    /* Determine the combined row mask. */
    if (mask_a == NULL)
    {
        mask = mask_b;
    }
    else if (mask_b == NULL)
    {
        mask = mask_a;
    }
    else
    {
        mask_logical_and(collocation_result->num_pairs, mask_a, mask_b, mask_a);
        mask = mask_a;
    }

    /* Update the collocation result (using the combined row mask). */
    update_collocation_result(collocation_result, mask);

    if (mask_a != NULL)
    {
        free(mask_a);
    }

    if (mask_b != NULL)
    {
        free(mask_b);
    }

    return 0;
}
