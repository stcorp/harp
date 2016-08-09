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

#include "harp-filter-collocation.h"
#include "harp-dimension-mask.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define COLLOCATION_MASK_BLOCK_SIZE 1024

static int compare_by_index(const void *a, const void *b)
{
    harp_collocation_index_pair *pair_a = *(harp_collocation_index_pair **)a;
    harp_collocation_index_pair *pair_b = *(harp_collocation_index_pair **)b;

    if (pair_a->index < pair_b->index)
    {
        return -1;
    }

    if (pair_a->index > pair_b->index)
    {
        return 1;
    }

    return 0;
}

static int compare_by_collocation_index(const void *a, const void *b)
{
    harp_collocation_index_pair *pair_a = *(harp_collocation_index_pair **)a;
    harp_collocation_index_pair *pair_b = *(harp_collocation_index_pair **)b;

    if (pair_a->collocation_index < pair_b->collocation_index)
    {
        return -1;
    }

    if (pair_a->collocation_index > pair_b->collocation_index)
    {
        return 1;
    }

    return 0;
}

int harp_collocation_index_pair_new(long collocation_index, long index, harp_collocation_index_pair **new_index_pair)
{
    harp_collocation_index_pair *index_pair;

    index_pair = (harp_collocation_index_pair *)malloc(sizeof(harp_collocation_index_pair));
    if (index_pair == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_collocation_index_pair), __FILE__, __LINE__);
        return -1;
    }

    index_pair->collocation_index = collocation_index;
    index_pair->index = index;

    *new_index_pair = index_pair;
    return 0;
}

void harp_collocation_index_pair_delete(harp_collocation_index_pair *index_pair)
{
    if (index_pair != NULL)
    {
        free(index_pair);
    }
}

int harp_collocation_mask_new(harp_collocation_mask **new_mask)
{
    harp_collocation_mask *mask;

    mask = (harp_collocation_mask *)malloc(sizeof(harp_collocation_mask));
    if (mask == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_collocation_mask), __FILE__, __LINE__);
        return -1;
    }

    mask->num_index_pairs = 0;
    mask->index_pair = NULL;

    *new_mask = mask;
    return 0;
}

void harp_collocation_mask_delete(harp_collocation_mask *mask)
{
    if (mask != NULL)
    {
        if (mask->index_pair != NULL)
        {
            long i;

            for (i = 0; i < mask->num_index_pairs; i++)
            {
                harp_collocation_index_pair_delete(mask->index_pair[i]);
            }

            free(mask->index_pair);
        }

        free(mask);
    }
}

int harp_collocation_mask_add_index_pair(harp_collocation_mask *mask, harp_collocation_index_pair *index_pair)
{
    if (mask->num_index_pairs % COLLOCATION_MASK_BLOCK_SIZE == 0)
    {
        harp_collocation_index_pair **new_index_pair;

        new_index_pair = realloc(mask->index_pair, (mask->num_index_pairs + COLLOCATION_MASK_BLOCK_SIZE)
                                 * sizeof(harp_collocation_index_pair *));
        if (new_index_pair == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           (mask->num_index_pairs + COLLOCATION_MASK_BLOCK_SIZE)
                           * sizeof(harp_collocation_index_pair *), __FILE__, __LINE__);
            return -1;
        }

        mask->index_pair = new_index_pair;
    }

    mask->index_pair[mask->num_index_pairs] = index_pair;
    mask->num_index_pairs++;
    return 0;
}

void harp_collocation_mask_sort_by_index(harp_collocation_mask *mask)
{
    qsort(mask->index_pair, mask->num_index_pairs, sizeof(harp_collocation_index_pair *), compare_by_index);
}

void harp_collocation_mask_sort_by_collocation_index(harp_collocation_mask *mask)
{
    qsort(mask->index_pair, mask->num_index_pairs, sizeof(harp_collocation_index_pair *), compare_by_collocation_index);
}

int harp_collocation_mask_from_result(const harp_collocation_result *collocation_result,
                                      harp_collocation_filter_type filter_type, const char *source_product,
                                      harp_collocation_mask **new_mask)
{
    long i;
    long product_index;
    harp_collocation_mask *mask;

    if (collocation_result == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "collocation_result is NULL");
        return -1;
    }

    if (harp_collocation_mask_new(&mask) != 0)
    {
        return -1;
    }

    for (i = 0; i < collocation_result->num_pairs; i++)
    {
        const harp_collocation_pair *pair;
        harp_collocation_index_pair *index_pair;

        pair = collocation_result->pair[i];
        if (filter_type == harp_collocation_left)
        {
            if (harp_dataset_get_index_from_source_product(collocation_result->dataset_a, source_product,
                                                           &product_index) != 0)
            {
                harp_collocation_mask_delete(mask);
                return -1;
            }
            if (pair->product_index_a != product_index)
            {
                continue;
            }

            if (harp_collocation_index_pair_new(pair->collocation_index, pair->sample_index_a, &index_pair) != 0)
            {
                harp_collocation_mask_delete(mask);
                return -1;
            }
        }
        else
        {
            if (harp_dataset_get_index_from_source_product(collocation_result->dataset_b, source_product,
                                                           &product_index) != 0)
            {
                harp_collocation_mask_delete(mask);
                return -1;
            }
            if (pair->product_index_b != product_index)
            {
                continue;
            }

            if (harp_collocation_index_pair_new(pair->collocation_index, pair->sample_index_b, &index_pair) != 0)
            {
                harp_collocation_mask_delete(mask);
                return -1;
            }
        }

        if (harp_collocation_mask_add_index_pair(mask, index_pair) != 0)
        {
            harp_collocation_index_pair_delete(index_pair);
            harp_collocation_mask_delete(mask);
            return -1;
        }
    }

    *new_mask = mask;
    return 0;
}

int harp_collocation_mask_import(const char *filename, harp_collocation_filter_type filter_type,
                                 const char *source_product, harp_collocation_mask **new_mask)
{
    harp_collocation_result *collocation_result;
    harp_collocation_mask *mask;

    if (filename == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "filename is NULL");
        return -1;
    }

    if (harp_collocation_result_read(filename, &collocation_result) != 0)
    {
        return -1;
    }

    if (harp_collocation_mask_from_result(collocation_result, filter_type, source_product, &mask) != 0)
    {
        harp_collocation_result_delete(collocation_result);
        return -1;
    }
    harp_collocation_result_delete(collocation_result);

    *new_mask = mask;
    return 0;
}

int harp_filter_index(const harp_variable *index, harp_collocation_mask *collocation_mask,
                      harp_dimension_mask *dimension_mask)
{
    long i;
    long j;
    long count;

    assert(index->num_dimensions == 1);
    assert(dimension_mask->num_dimensions == 1 && dimension_mask->num_elements == index->num_elements);

    harp_collocation_mask_sort_by_index(collocation_mask);

    i = 0;
    j = 0;
    count = 0;
    while (i < collocation_mask->num_index_pairs && j < index->num_elements)
    {
        if (collocation_mask->index_pair[i]->index < index->data.int32_data[j])
        {
            /* Measurement not present in product, ignore. */
            i++;
        }
        else if (collocation_mask->index_pair[i]->index > index->data.int32_data[j])
        {
            /* Measurement not selected, or duplicate index in product. */
            dimension_mask->mask[j] = 0;
            j++;
        }
        else
        {
            if (dimension_mask->mask[j])
            {
                count++;
            }

            while (i < collocation_mask->num_index_pairs
                   && collocation_mask->index_pair[i]->index == index->data.int32_data[j])
            {
                i++;
            }

            j++;
        }
    }

    while (j < index->num_elements)
    {
        dimension_mask->mask[j] = 0;
        j++;
    }

    dimension_mask->masked_dimension_length = count;
    return 0;
}

int harp_filter_collocation_index(const harp_variable *collocation_index, harp_collocation_mask *collocation_mask,
                                  harp_dimension_mask *dimension_mask)
{
    long i;
    long j;
    long count;

    assert(collocation_index->num_dimensions == 1);
    assert(dimension_mask->num_dimensions == 1 && dimension_mask->num_elements == collocation_index->num_elements);

    harp_collocation_mask_sort_by_collocation_index(collocation_mask);

    i = 0;
    j = 0;
    count = 0;
    while (i < collocation_mask->num_index_pairs && j < collocation_index->num_elements)
    {
        if (collocation_mask->index_pair[i]->collocation_index < collocation_index->data.int32_data[j])
        {
            /* Measurement not present in product, ignore. */
            i++;
        }
        else if (collocation_mask->index_pair[i]->collocation_index > collocation_index->data.int32_data[j])
        {
            /* Measurement not selected, or duplicate index in product. */
            dimension_mask->mask[j] = 0;
            j++;
        }
        else
        {
            if (dimension_mask->mask[j])
            {
                count++;
            }

            j++;
        }
    }

    while (j < collocation_index->num_elements)
    {
        if (dimension_mask->mask[j])
        {
            count++;
        }

        j++;
    }

    dimension_mask->masked_dimension_length = count;
    return 0;
}

int harp_product_apply_collocation_mask(harp_collocation_mask *collocation_mask, harp_product *product)
{
    harp_variable *collocation_index = NULL;

    if (collocation_mask == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "collocation_mask is NULL");
        return -1;
    }

    if (product == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "product is NULL");
        return -1;
    }

    if (product->dimension[harp_dimension_time] == 0)
    {
        return 0;
    }

    if (harp_product_get_variable_by_name(product, "collocation_index", &collocation_index) == 0)
    {
        harp_dimension_mask *dimension_mask;
        long dimension;

        /* TODO: Check number of dimensions, dimension type, and data type. */
        dimension = product->dimension[harp_dimension_time];
        if (harp_dimension_mask_new(1, &dimension, &dimension_mask) != 0)
        {
            return -1;
        }

        if (harp_filter_collocation_index(collocation_index, collocation_mask, dimension_mask) != 0)
        {
            harp_dimension_mask_delete(dimension_mask);
            return -1;
        }

        if (harp_product_filter_dimension(product, harp_dimension_time, dimension_mask->mask) != 0)
        {
            harp_dimension_mask_delete(dimension_mask);
            return -1;
        }
    }
    else
    {
        harp_dimension_type dimension_type = harp_dimension_time;
        harp_variable *index = NULL;
        long *dimension_index = NULL;
        long num_elements;
        long i;
        long j;
        long k;

        if (harp_product_get_variable_by_name(product, "index", &index) != 0)
        {
            return -1;
        }
        assert(index->data_type == harp_type_int32);

        if (index->num_dimensions != 1)
        {
            harp_set_error(HARP_ERROR_OPERATION, "variable '%s' has %d dimensions (expected 1)", index->name,
                           index->num_dimensions);
            return -1;
        }

        if (index->dimension_type[0] != harp_dimension_time)
        {
            harp_set_error(HARP_ERROR_OPERATION, "dimension 0 of variable '%s' is of type '%s' (expected '%s')",
                           index->name, harp_get_dimension_type_name(index->dimension_type[0]),
                           harp_get_dimension_type_name(harp_dimension_time));
            return -1;
        }

        /* Sort the collocation mask by index. */
        harp_collocation_mask_sort_by_index(collocation_mask);

        /* Both the collocation mask and the 'index' variable should now be sorted (since the 'index' variable should
         * always be sorted).
         */
        i = 0;
        j = 0;
        num_elements = 0;
        while (i < collocation_mask->num_index_pairs && j < index->num_elements)
        {
            if (collocation_mask->index_pair[i]->index < index->data.int32_data[j])
            {
                /* Measurement not present in product, ignore. */
                i++;
            }
            else if (collocation_mask->index_pair[i]->index > index->data.int32_data[j])
            {
                /* Measurement not selected, or duplicate index in product, ignore. */
                j++;
            }
            else
            {
                assert(collocation_mask->index_pair[i]->index == index->data.int32_data[j]);
                i++;
                num_elements++;
            }
        }

        if (num_elements == 0)
        {
            /* If the new length of the time dimension is zero, return an empty product. */
            harp_product_remove_all_variables(product);
            return 0;
        }

        if (harp_variable_new("collocation_index", harp_type_int32, 1, &dimension_type, &num_elements,
                              &collocation_index) != 0)
        {
            return -1;
        }

        dimension_index = (long *)malloc(num_elements * sizeof(long));
        if (dimension_index == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           num_elements * sizeof(long), __FILE__, __LINE__);
            harp_variable_delete(collocation_index);
            return -1;
        }

        i = 0;
        j = 0;
        k = 0;
        while (i < collocation_mask->num_index_pairs && j < index->num_elements)
        {
            if (collocation_mask->index_pair[i]->index < index->data.int32_data[j])
            {
                /* Measurement not present in product, ignore. */
                i++;
            }
            else if (collocation_mask->index_pair[i]->index > index->data.int32_data[j])
            {
                /* Measurement not selected, or duplicate index in product, ignore. */
                j++;
            }
            else
            {
                assert(collocation_mask->index_pair[i]->index == index->data.int32_data[j]);

                dimension_index[k] = j;
                collocation_index->data.int32_data[k] = collocation_mask->index_pair[i]->collocation_index;

                i++;
                k++;
            }
        }

        if (harp_product_rearrange_dimension(product, harp_dimension_time, num_elements, dimension_index) != 0)
        {
            free(dimension_index);
            harp_variable_delete(collocation_index);
            return -1;
        }

        if (harp_product_add_variable(product, collocation_index) != 0)
        {
            free(dimension_index);
            harp_variable_delete(collocation_index);
            return -1;
        }
        free(dimension_index);
    }

    return 0;
}
