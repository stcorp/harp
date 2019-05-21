/*
 * Copyright (C) 2015-2019 S[&]T, The Netherlands.
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

#include "harp-filter-collocation.h"
#include "harp-dimension-mask.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define COLLOCATION_MASK_BLOCK_SIZE 1024

static int compare_by_index(const void *a, const void *b)
{
    harp_collocation_index_pair *pair_a = (harp_collocation_index_pair *)a;
    harp_collocation_index_pair *pair_b = (harp_collocation_index_pair *)b;

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
    harp_collocation_index_pair *pair_a = (harp_collocation_index_pair *)a;
    harp_collocation_index_pair *pair_b = (harp_collocation_index_pair *)b;

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

static int collocation_mask_new(harp_collocation_mask **new_mask)
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
            free(mask->index_pair);
        }

        free(mask);
    }
}

static int collocation_mask_add_index_pair(harp_collocation_mask *mask, long collocation_index, long index)
{
    if (mask->num_index_pairs % COLLOCATION_MASK_BLOCK_SIZE == 0)
    {
        harp_collocation_index_pair *new_index_pair;

        new_index_pair = realloc(mask->index_pair, (mask->num_index_pairs + COLLOCATION_MASK_BLOCK_SIZE)
                                 * sizeof(harp_collocation_index_pair));
        if (new_index_pair == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           (mask->num_index_pairs + COLLOCATION_MASK_BLOCK_SIZE)
                           * sizeof(harp_collocation_index_pair), __FILE__, __LINE__);
            return -1;
        }

        mask->index_pair = new_index_pair;
    }

    mask->index_pair[mask->num_index_pairs].collocation_index = collocation_index;
    mask->index_pair[mask->num_index_pairs].index = index;
    mask->num_index_pairs++;

    return 0;
}

static void collocation_mask_sort_by_index(harp_collocation_mask *mask)
{
    qsort(mask->index_pair, mask->num_index_pairs, sizeof(harp_collocation_index_pair), compare_by_index);
}

static void collocation_mask_sort_by_collocation_index(harp_collocation_mask *mask)
{
    qsort(mask->index_pair, mask->num_index_pairs, sizeof(harp_collocation_index_pair), compare_by_collocation_index);
}

static int collocation_mask_from_result(const harp_collocation_result *collocation_result,
                                        harp_collocation_filter_type filter_type, const char *source_product,
                                        harp_collocation_mask **new_mask)
{
    long i;
    long product_index = -1;
    harp_collocation_mask *mask;

    if (collocation_result == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "collocation_result is NULL");
        return -1;
    }

    if (collocation_mask_new(&mask) != 0)
    {
        return -1;
    }

    /* get the dataset-index associated with the source_product */
    if (filter_type == harp_collocation_left)
    {
        if (harp_dataset_get_index_from_source_product(collocation_result->dataset_a, source_product,
                                                       &product_index) != 0)
        {
            /* source_product does not appear in the collocation result column */
            product_index = -1;
        }
    }
    else
    {
        if (harp_dataset_get_index_from_source_product(collocation_result->dataset_b, source_product,
                                                       &product_index) != 0)
        {
            /* source_product does not appear in the collocation result column */
            product_index = -1;
        }
    }

    /* if product_index is -1, no match will be found */
    for (i = 0; i < collocation_result->num_pairs && product_index >= 0; i++)
    {
        const harp_collocation_pair *pair;

        pair = collocation_result->pair[i];
        if (filter_type == harp_collocation_left)
        {
            if (pair->product_index_a != product_index)
            {
                continue;
            }

            if (collocation_mask_add_index_pair(mask, pair->collocation_index, pair->sample_index_a) != 0)
            {
                harp_collocation_mask_delete(mask);
                return -1;
            }
        }
        else
        {
            if (pair->product_index_b != product_index)
            {
                continue;
            }

            if (collocation_mask_add_index_pair(mask, pair->collocation_index, pair->sample_index_b) != 0)
            {
                harp_collocation_mask_delete(mask);
                return -1;
            }
        }

    }

    *new_mask = mask;
    return 0;
}

int harp_collocation_mask_import(const char *filename, harp_collocation_filter_type filter_type,
                                 long min_collocation_index, long max_collocation_index,
                                 const char *source_product, harp_collocation_mask **new_mask)
{
    harp_collocation_result *collocation_result;
    harp_collocation_mask *mask;
    const char *source_product_a = NULL;
    const char *source_product_b = NULL;

    if (filename == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "filename is NULL");
        return -1;
    }

    if (filter_type == harp_collocation_left)
    {
        source_product_a = source_product;
    }
    else
    {
        source_product_b = source_product;
    }
    if (harp_collocation_result_read_range(filename, min_collocation_index, max_collocation_index,
                                           source_product_a, source_product_b, &collocation_result) != 0)
    {
        return -1;
    }

    if (collocation_mask_from_result(collocation_result, filter_type, source_product, &mask) != 0)
    {
        harp_collocation_result_delete(collocation_result);
        return -1;
    }
    harp_collocation_result_delete(collocation_result);

    *new_mask = mask;
    return 0;
}

/* find value in sorted array of values (returns 1 if found, 0 if not found) */
static int find_collocation_pair_for_collocation_index(harp_collocation_mask *collocation_mask,
                                                       long collocation_index, long *index)
{
    long lower_index;
    long upper_index;

    lower_index = 0;
    upper_index = collocation_mask->num_index_pairs - 1;

    while (upper_index >= lower_index)
    {
        /* Determine the index that splits the search space into two (approximately) equal halves. */
        long pivot_index = lower_index + ((upper_index - lower_index) / 2);

        /* If the pivot equals the key, terminate early. */
        if (collocation_mask->index_pair[pivot_index].collocation_index == collocation_index)
        {
            *index = pivot_index;
            return 1;
        }

        /* If the pivot is smaller than the key, search the upper sub array, otherwise search the lower sub array. */
        if (collocation_mask->index_pair[pivot_index].collocation_index < collocation_index)
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

static int filter_collocation_index(const harp_variable *collocation_index, harp_collocation_mask *collocation_mask,
                                    harp_dimension_mask *dimension_mask)
{
    long i;

    assert(collocation_index->num_dimensions == 1);
    assert(dimension_mask->num_dimensions == 1 && dimension_mask->num_elements == collocation_index->num_elements);

    collocation_mask_sort_by_collocation_index(collocation_mask);

    for (i = 0; i < collocation_index->num_elements; i++)
    {
        if (dimension_mask->mask[i])
        {
            long index;

            if (!find_collocation_pair_for_collocation_index(collocation_mask, collocation_index->data.int32_data[i],
                                                             &index))
            {
                dimension_mask->mask[i] = 0;
                dimension_mask->masked_dimension_length--;
            }
        }
    }

    return 0;
}

int harp_product_apply_collocation_mask(harp_product *product, harp_collocation_mask *collocation_mask)
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

        if (collocation_index->data_type != harp_type_int32)
        {
            harp_set_error(HARP_ERROR_OPERATION, "variable '%s' has wrong data type", collocation_index->name);
            return -1;
        }
        if (collocation_index->num_dimensions != 1)
        {
            harp_set_error(HARP_ERROR_OPERATION, "variable '%s' has %d dimensions (expected 1)",
                           collocation_index->name, collocation_index->num_dimensions);
            return -1;
        }

        if (collocation_index->dimension_type[0] != harp_dimension_time)
        {
            harp_set_error(HARP_ERROR_OPERATION, "dimension 0 of variable '%s' is of type '%s' (expected '%s')",
                           collocation_index->name, harp_get_dimension_type_name(collocation_index->dimension_type[0]),
                           harp_get_dimension_type_name(harp_dimension_time));
            return -1;
        }

        dimension = product->dimension[harp_dimension_time];
        if (harp_dimension_mask_new(1, &dimension, &dimension_mask) != 0)
        {
            return -1;
        }

        if (filter_collocation_index(collocation_index, collocation_mask, dimension_mask) != 0)
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
        if (index->data_type != harp_type_int32)
        {
            harp_set_error(HARP_ERROR_OPERATION, "variable '%s' has wrong data type", index->name);
            return -1;
        }
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
        collocation_mask_sort_by_index(collocation_mask);

        /* Both the collocation mask and the 'index' variable should now be sorted (since the 'index' variable should
         * always be sorted).
         */
        i = 0;
        j = 0;
        num_elements = 0;
        while (i < collocation_mask->num_index_pairs && j < index->num_elements)
        {
            if (collocation_mask->index_pair[i].index < index->data.int32_data[j])
            {
                /* Measurement not present in product, ignore. */
                i++;
            }
            else if (collocation_mask->index_pair[i].index > index->data.int32_data[j])
            {
                /* Measurement not selected, or duplicate index in product, ignore. */
                j++;
            }
            else
            {
                assert(collocation_mask->index_pair[i].index == index->data.int32_data[j]);
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
            if (collocation_mask->index_pair[i].index < index->data.int32_data[j])
            {
                /* Measurement not present in product, ignore. */
                i++;
            }
            else if (collocation_mask->index_pair[i].index > index->data.int32_data[j])
            {
                /* Measurement not selected, or duplicate index in product, ignore. */
                j++;
            }
            else
            {
                assert(collocation_mask->index_pair[i].index == index->data.int32_data[j]);

                dimension_index[k] = j;
                collocation_index->data.int32_data[k] = collocation_mask->index_pair[i].collocation_index;

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

static int get_collocated_product(harp_collocation_result *collocation_result, const char *source_product_b,
                                  harp_product **product)
{
    harp_collocation_mask *mask;
    harp_product_metadata *product_metadata;
    harp_product *collocated_product;
    harp_collocation_pair *pair;

    if (harp_collocation_result_filter_for_source_product_b(collocation_result, source_product_b) != 0)
    {
        return -1;
    }
    if (collocation_result->num_pairs == 0)
    {
        *product = NULL;
        return 0;
    }
    /* use product b reference from first pair to find and import product */
    pair = collocation_result->pair[0];
    product_metadata = collocation_result->dataset_b->metadata[pair->product_index_b];
    if (product_metadata == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "missing product metadata for product %s",
                       collocation_result->dataset_b->source_product[pair->product_index_b]);
        return -1;
    }

    if (collocation_mask_from_result(collocation_result, harp_collocation_right, source_product_b, &mask) != 0)
    {
        return -1;
    }

    if (harp_import(product_metadata->filename, NULL, NULL, &collocated_product) != 0)
    {
        harp_set_error(HARP_ERROR_IMPORT, "could not import file %s", product_metadata->filename);
        harp_collocation_mask_delete(mask);
        return -1;
    }

    if (harp_product_apply_collocation_mask(collocated_product, mask) != 0)
    {
        harp_collocation_mask_delete(mask);
        return -1;
    }

    harp_collocation_mask_delete(mask);
    *product = collocated_product;

    return 0;
}

int harp_collocation_result_get_filtered_product_b(harp_collocation_result *collocation_result,
                                                   const char *source_product, harp_product **product)
{
    harp_collocation_result *result_copy;

    if (harp_collocation_result_shallow_copy(collocation_result, &result_copy) != 0)
    {
        return -1;
    }

    if (get_collocated_product(result_copy, source_product, product) != 0)
    {
        harp_collocation_result_shallow_delete(result_copy);
        return -1;
    }

    harp_collocation_result_shallow_delete(result_copy);

    return 0;
}
