/*
 * Copyright (C) 2015-2026 S[&]T, The Netherlands.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "harp.h"

#include <stdlib.h>
#include <string.h>

/* this function requires collocation_result to be sorted by collocation_index */
static long get_index_for_collocation_index(const harp_collocation_result *collocation_result,
                                            int32_t collocation_index)
{
    long lower_index;
    long upper_index;

    /* Use binary search to check if the index passed in is contained in the (sorted) collocation index list. */
    lower_index = 0;
    upper_index = collocation_result->num_pairs - 1;

    while (upper_index >= lower_index)
    {
        /* Determine the index that splits the search space into two (approximately) equal halves. */
        long pivot_index = lower_index + ((upper_index - lower_index) / 2);

        /* If the pivot equals the index to be found, terminate early. */
        if (collocation_result->pair[pivot_index]->collocation_index == collocation_index)
        {
            return pivot_index;
        }

        /* If the pivot is smaller than the index to be found, search the upper sub array, otherwise search the lower
         * sub array.
         */
        if (collocation_result->pair[pivot_index]->collocation_index < collocation_index)
        {
            lower_index = pivot_index + 1;
        }
        else
        {
            upper_index = pivot_index - 1;
        }
    }

    return -1;
}

static int update_mask_for_product(const harp_collocation_result *collocation_result, const char *product_path,
                                   uint8_t *mask)
{
    harp_product *product;
    harp_variable *collocation_index;
    long i;

    if (harp_import(product_path, "keep(collocation_index);derive(collocation_index int32 {time})", NULL, &product) !=
        0)
    {
        return -1;
    }

    if (harp_product_get_variable_by_name(product, "collocation_index", &collocation_index) != 0)
    {
        harp_product_delete(product);
        return -1;
    }

    for (i = 0; i < collocation_index->num_elements; i++)
    {
        long index;

        index = get_index_for_collocation_index(collocation_result, collocation_index->data.int32_data[i]);
        if (index < 0)
        {
            harp_set_error(HARP_ERROR_INVALID_PRODUCT, "collocation result does contain collocation index %d",
                           collocation_index->data.int32_data[i]);
            harp_product_delete(product);
            return -1;
        }
        mask[index] = 1;
    }

    harp_product_delete(product);
    return 0;
}

static int update_collocation_result(harp_collocation_result *collocation_result, harp_dataset *dataset)
{
    uint8_t *mask = NULL;
    long i;

    mask = calloc(collocation_result->num_pairs, sizeof(uint8_t));
    if (mask == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       collocation_result->num_pairs * sizeof(uint8_t), __FILE__, __LINE__);
        return -1;
    }
    for (i = 0; i < collocation_result->num_pairs; i++)
    {
        mask[i] = 0;
    }

    for (i = 0; i < dataset->num_products; i++)
    {
        if (update_mask_for_product(collocation_result, dataset->metadata[i]->filename, mask) != 0)
        {
            free(mask);
            return -1;
        }
    }

    for (i = collocation_result->num_pairs - 1; i >= 0; i--)
    {
        if (!mask[i])
        {
            if (harp_collocation_result_remove_pair_at_index(collocation_result, i) != 0)
            {
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
    harp_dataset *dataset;
    const char *output;

    if (argc < 4 || argc > 5 || argv[2][0] == '-' || argv[3][0] == '-')
    {
        return 1;
    }
    if (argc == 5)
    {
        if (argv[4][0] == '-')
        {
            return 1;
        }
        output = argv[4];
    }
    else
    {
        output = argv[2];
    }

    if (harp_collocation_result_read(argv[2], &collocation_result) != 0)
    {
        return -1;
    }
    if (harp_collocation_result_sort_by_collocation_index(collocation_result) != 0)
    {
        harp_collocation_result_delete(collocation_result);
        return -1;
    }

    if (harp_dataset_new(&dataset) != 0)
    {
        harp_collocation_result_delete(collocation_result);
        return -1;
    }
    if (harp_dataset_import(dataset, argv[3], NULL) != 0)
    {
        harp_collocation_result_delete(collocation_result);
        harp_dataset_delete(dataset);
        return -1;
    }

    if (update_collocation_result(collocation_result, dataset) != 0)
    {
        harp_collocation_result_delete(collocation_result);
        harp_dataset_delete(dataset);
        return -1;
    }

    harp_dataset_delete(dataset);

    if (harp_collocation_result_write(output, collocation_result) != 0)
    {
        harp_collocation_result_delete(collocation_result);
        return -1;
    }
    harp_collocation_result_delete(collocation_result);

    return 0;
}
