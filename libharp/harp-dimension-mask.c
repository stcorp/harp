/*
 * Copyright (C) 2015-2023 S[&]T, The Netherlands.
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

#include "harp-dimension-mask.h"
#include "harp-internal.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

int harp_dimension_mask_new(int num_dimensions, const long *dimension, harp_dimension_mask **new_dimension_mask)
{
    int i;
    harp_dimension_mask *dimension_mask;

    dimension_mask = (harp_dimension_mask *)malloc(sizeof(harp_dimension_mask));
    if (dimension_mask == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_dimension_mask), __FILE__, __LINE__);
        return -1;
    }

    dimension_mask->num_dimensions = num_dimensions;
    dimension_mask->mask = NULL;
    dimension_mask->num_elements = 1;
    dimension_mask->masked_dimension_length = (num_dimensions == 0 ? 1 : dimension[num_dimensions - 1]);
    for (i = 0; i < num_dimensions; i++)
    {
        dimension_mask->dimension[i] = dimension[i];
        dimension_mask->num_elements *= dimension[i];
    }

    dimension_mask->mask = malloc((size_t)dimension_mask->num_elements * sizeof(uint8_t));
    if (dimension_mask->mask == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       dimension_mask->num_elements * sizeof(uint8_t), __FILE__, __LINE__);
        harp_dimension_mask_delete(dimension_mask);
        return -1;
    }

    /* Initialize the mask to all 1's. */
    for (i = 0; i < dimension_mask->num_elements; i++)
    {
        dimension_mask->mask[i] = 1;
    }

    *new_dimension_mask = dimension_mask;
    return 0;
}

void harp_dimension_mask_delete(harp_dimension_mask *dimension_mask)
{
    if (dimension_mask != NULL)
    {
        if (dimension_mask->mask != NULL)
        {
            free(dimension_mask->mask);
        }

        free(dimension_mask);
    }
}

int harp_dimension_mask_copy(const harp_dimension_mask *other_dimension_mask, harp_dimension_mask **new_dimension_mask)
{
    harp_dimension_mask *dimension_mask;
    int i;

    assert(other_dimension_mask != NULL);
    assert(new_dimension_mask != NULL);

    dimension_mask = (harp_dimension_mask *)malloc(sizeof(harp_dimension_mask));
    if (dimension_mask == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_dimension_mask), __FILE__, __LINE__);
        return -1;
    }

    dimension_mask->num_dimensions = other_dimension_mask->num_dimensions;
    for (i = 0; i < dimension_mask->num_dimensions; i++)
    {
        dimension_mask->dimension[i] = other_dimension_mask->dimension[i];
    }
    dimension_mask->num_elements = other_dimension_mask->num_elements;
    dimension_mask->masked_dimension_length = other_dimension_mask->masked_dimension_length;
    dimension_mask->mask = NULL;

    dimension_mask->mask = (uint8_t *)malloc(dimension_mask->num_elements * sizeof(uint8_t));
    if (dimension_mask->mask == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       dimension_mask->num_elements * sizeof(uint8_t), __FILE__, __LINE__);
        return -1;
    }

    memcpy(dimension_mask->mask, other_dimension_mask->mask, dimension_mask->num_elements * sizeof(uint8_t));

    *new_dimension_mask = dimension_mask;
    return 0;
}

int harp_dimension_mask_set_new(harp_dimension_mask_set **new_dimension_mask_set)
{
    harp_dimension_mask_set *dimension_mask_set;
    int i;

    dimension_mask_set = (harp_dimension_mask_set *)malloc(HARP_NUM_DIM_TYPES * sizeof(harp_dimension_mask *));
    if (dimension_mask_set == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       HARP_NUM_DIM_TYPES * sizeof(harp_dimension_mask *), __FILE__, __LINE__);
        return -1;
    }

    for (i = 0; i < HARP_NUM_DIM_TYPES; i++)
    {
        dimension_mask_set[i] = NULL;
    }

    *new_dimension_mask_set = dimension_mask_set;
    return 0;
}

void harp_dimension_mask_set_delete(harp_dimension_mask_set *dimension_mask_set)
{
    if (dimension_mask_set != NULL)
    {
        int i;

        for (i = 0; i < HARP_NUM_DIM_TYPES; i++)
        {
            harp_dimension_mask_delete(dimension_mask_set[i]);
        }

        free(dimension_mask_set);
    }
}

int harp_dimension_mask_fill_true(harp_dimension_mask *dimension_mask)
{
    assert(dimension_mask != NULL && dimension_mask->num_elements > 0 && dimension_mask->mask != NULL);

    memset(dimension_mask->mask, 1, dimension_mask->num_elements);

    dimension_mask->masked_dimension_length = 1;
    if (dimension_mask->num_elements > 0)
    {
        dimension_mask->masked_dimension_length *= dimension_mask->dimension[dimension_mask->num_dimensions - 1];
    }

    return 0;
}

int harp_dimension_mask_fill_false(harp_dimension_mask *dimension_mask)
{
    assert(dimension_mask != NULL && dimension_mask->num_elements > 0 && dimension_mask->mask != NULL);

    memset(dimension_mask->mask, 0, dimension_mask->num_elements * sizeof(uint8_t));
    dimension_mask->masked_dimension_length = 0;

    return 0;
}

static long count(long num_elements, const uint8_t *mask)
{
    const uint8_t *mask_end;
    long count;

    count = 0;
    for (mask_end = mask + num_elements; mask != mask_end; mask++)
    {
        if (*mask)
        {
            count++;
        }
    }

    return count;
}

int harp_dimension_mask_update_masked_length(harp_dimension_mask *dimension_mask)
{
    long num_blocks;
    long num_block_elements;
    long max_masked_length;
    long i;

    assert(dimension_mask != NULL);
    assert(dimension_mask->num_elements == 0 || dimension_mask->mask != NULL);

    num_blocks = (dimension_mask->num_dimensions <= 1 ? 1 : dimension_mask->dimension[0]);
    num_block_elements = dimension_mask->num_elements / num_blocks;

    max_masked_length = 0;
    for (i = 0; i < num_blocks; i++)
    {
        long masked_length;

        masked_length = count(num_block_elements, &dimension_mask->mask[i * num_block_elements]);
        if (masked_length > max_masked_length)
        {
            max_masked_length = masked_length;
        }
    }

    dimension_mask->masked_dimension_length = max_masked_length;
    return 0;
}

int harp_dimension_mask_outer_product(const harp_dimension_mask *row_mask, const harp_dimension_mask *col_mask,
                                      harp_dimension_mask **new_dimension_mask)
{
    harp_dimension_mask *dimension_mask;
    long i;

    assert(row_mask != NULL && row_mask->num_dimensions == 1 && row_mask->mask != NULL);
    assert(col_mask != NULL && col_mask->num_dimensions == 1 && col_mask->mask != NULL);
    assert(new_dimension_mask != NULL);

    dimension_mask = (harp_dimension_mask *)malloc(sizeof(harp_dimension_mask));
    if (dimension_mask == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_dimension_mask), __FILE__, __LINE__);
        return -1;
    }

    dimension_mask->num_dimensions = 2;
    dimension_mask->dimension[0] = row_mask->num_elements;
    dimension_mask->dimension[1] = col_mask->num_elements;
    dimension_mask->num_elements = dimension_mask->dimension[0] * dimension_mask->dimension[1];

    if (row_mask->masked_dimension_length != 0)
    {
        dimension_mask->masked_dimension_length = col_mask->masked_dimension_length;
    }

    dimension_mask->mask = malloc(dimension_mask->num_elements * sizeof(uint8_t));
    if (dimension_mask->mask == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       dimension_mask->num_elements * sizeof(uint8_t), __FILE__, __LINE__);
        harp_dimension_mask_delete(dimension_mask);
        return -1;
    }

    for (i = 0; i < row_mask->num_elements; i++)
    {
        if (row_mask->mask[i])
        {
            memcpy(dimension_mask->mask + i * dimension_mask->dimension[1], col_mask->mask,
                   dimension_mask->dimension[1] * sizeof(uint8_t));
        }
        else
        {
            memset(dimension_mask->mask + i * dimension_mask->dimension[1], 0,
                   dimension_mask->dimension[1] * sizeof(uint8_t));
        }
    }

    *new_dimension_mask = dimension_mask;
    return 0;
}

int harp_dimension_mask_prepend_dimension(harp_dimension_mask *dimension_mask, long length)
{
    uint8_t *mask;
    long new_num_elements;
    long i;

    assert(dimension_mask != NULL);
    assert(length > 0);
    assert(dimension_mask->num_dimensions < 2);
    assert(dimension_mask->num_elements > 0);

    new_num_elements = dimension_mask->num_elements * length;
    mask = (uint8_t *)realloc((void *)dimension_mask->mask, new_num_elements * sizeof(uint8_t));
    if (mask == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       new_num_elements * sizeof(uint8_t), __FILE__, __LINE__);
        return -1;
    }
    dimension_mask->mask = mask;

    for (i = 1; i < length; i++)
    {
        memcpy(dimension_mask->mask + i * dimension_mask->num_elements, dimension_mask->mask,
               dimension_mask->num_elements * sizeof(uint8_t));
    }

    dimension_mask->num_elements = new_num_elements;
    dimension_mask->num_dimensions++;
    for (i = dimension_mask->num_dimensions - 1; i > 0; i--)
    {
        dimension_mask->dimension[i] = dimension_mask->dimension[i - 1];
    }
    dimension_mask->dimension[0] = length;

    /* The masked dimension length is not affected by prepending a dimension. */
    return 0;
}

int harp_dimension_mask_append_dimension(harp_dimension_mask *dimension_mask, long length)
{
    uint8_t *mask;
    long new_num_elements;
    long i;

    assert(dimension_mask != NULL);
    assert(length > 0);
    assert(dimension_mask->num_dimensions < 2);
    assert(dimension_mask->num_elements > 0);
    assert(dimension_mask->mask != NULL);

    new_num_elements = dimension_mask->num_elements * length;

    mask = (uint8_t *)realloc((void *)dimension_mask->mask, new_num_elements * sizeof(uint8_t));
    if (mask == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       new_num_elements * sizeof(uint8_t), __FILE__, __LINE__);
        return -1;
    }
    dimension_mask->mask = mask;

    for (i = dimension_mask->num_elements - 1; i >= 0; i--)
    {
        long j;

        for (j = 0; j < length; j++)
        {
            dimension_mask->mask[i * length + j] = dimension_mask->mask[i];
        }
    }

    dimension_mask->num_elements = new_num_elements;
    dimension_mask->dimension[dimension_mask->num_dimensions] = length;
    dimension_mask->num_dimensions++;

    /* Update the masked dimension length. If the original mask is zero everywhere, then the new mask will also be zero
     * everywhere and thus the masked dimension length equals zero for both the original and the new mask. Otherwise,
     * the masked dimension length of the new mask will be equal to the length of the appended dimension (independent of
     * the masked dimension length of the original mask). This is because any non-zero entry in the original mask will
     * be repeated along the appended dimension.
     */
    if (dimension_mask->masked_dimension_length != 0)
    {
        dimension_mask->masked_dimension_length = length;
    }

    return 0;
}

int harp_dimension_mask_reduce(const harp_dimension_mask *dimension_mask, int dim_index,
                               harp_dimension_mask **new_dimension_mask)
{
    harp_dimension_mask *reduced_dimension_mask;
    long num_groups;
    long num_blocks;
    long num_block_elements;
    long i;

    assert(dimension_mask != NULL && dimension_mask->num_elements != 0 && dimension_mask->mask != NULL);
    assert(dim_index >= 0 && dim_index < dimension_mask->num_dimensions);

    /* The mask is split into three parts:
     *     num_elements = num_groups * num_blocks * num_block_elements.
     *
     * Here, num_groups is the product of dimensions [0, dim_index), num_blocks is dimension[dim_index],
     * and num_block_elements is the product of dimensions (dim_index, num_dimensions).
     */

    /* Calculate the number of times we have to filter the indices (i.e. the product of the higher dimensions) */
    num_groups = 1;
    for (i = 0; i < dim_index; i++)
    {
        num_groups *= dimension_mask->dimension[i];
    }

    /* Calculate the number of blocks. */
    num_blocks = dimension_mask->dimension[dim_index];

    /* Calculate the number of elements per block. */
    num_block_elements = dimension_mask->num_elements / (num_groups * num_blocks);

    /* Allocate the reduced mask. */
    if (harp_dimension_mask_new(1, &num_blocks, &reduced_dimension_mask) != 0)
    {
        return -1;
    }

    /* Initialize the mask to false (harp_dimension_mask_new() initializes to true). */
    if (harp_dimension_mask_fill_false(reduced_dimension_mask) != 0)
    {
        harp_dimension_mask_delete(reduced_dimension_mask);
        return -1;
    }

    /* Reduce the mask along the specified dimension. For each index on the specified dimension, if any value in the
     * sub mask corresponding to this index is set to true, set the corresponding value in the reduced mask to true and
     * move to the next index.
     */
    for (i = 0; i < num_blocks; i++)
    {
        long j;

        for (j = 0; j < num_groups; j++)
        {
            const uint8_t *block_ptr;
            const uint8_t *block_end_ptr;

            block_ptr = dimension_mask->mask + (j * num_blocks + i) * num_block_elements;
            block_end_ptr = block_ptr + num_block_elements;

            for (; block_ptr != block_end_ptr; block_ptr++)
            {
                if (*block_ptr)
                {
                    break;
                }
            }

            if (block_ptr != block_end_ptr)
            {
                /* If any value in the block is set to true, the loop above will exit early, and therefore block_ptr
                 * will not equal block_end_ptr. In this case, the corresponding value in the reduced mask can be set
                 * to true and no additional blocks related to this index need to be examined.
                 */
                reduced_dimension_mask->mask[i] = 1;
                reduced_dimension_mask->masked_dimension_length++;
                break;
            }
        }
    }

    assert(count(reduced_dimension_mask->num_elements, reduced_dimension_mask->mask)
           == reduced_dimension_mask->masked_dimension_length);

    *new_dimension_mask = reduced_dimension_mask;
    return 0;
}

/**
 * Merge two dimension masks in place.
 *
 * Compute the intersection (logical and) of \p dimension_mask and \p merged_dimension_mask, storing the result in
 * \p merged_dimension_mask. The masks should either have the same number of dimensions, in which case \p dim_index will
 * be ignored, or \p dimension_mask should be one-dimensional, and \p merged_dimension_mask should have more two or more
 * dimensions. In the latter case, \p dim_index determines the dimension of \p merged_dimension_mask along which
 * \p dimension_mask is to be applied.
 *
 * \param  dimension_mask        Dimension mask to be merged into \p merged_dimension_mask.
 * \param  dim_index             Dimension along which \p dimension_mask should be applied; ignored if \p dimension_mask
 *                               and \p merged_dimension_mask have the same number of dimensions.
 * \param  merged_dimension_mask Dimension mask into which \p dimension_mask will be merged; this masked will be updated
 *                               in place.
 * \return
 *     \arg \c 0, Success.
 *     \arg \c -1, Error occurred (check #harp_errno).
 */
int harp_dimension_mask_merge(const harp_dimension_mask *dimension_mask, int dim_index,
                              harp_dimension_mask *merged_dimension_mask)
{
    long i;

    assert(dimension_mask != NULL);
    assert(dimension_mask->num_elements == 0 || dimension_mask->mask != NULL);
    assert(merged_dimension_mask != NULL);
    assert(merged_dimension_mask->num_elements == 0 || merged_dimension_mask->mask != NULL);

    if (dimension_mask->num_dimensions == merged_dimension_mask->num_dimensions)
    {
        assert(dimension_mask->num_elements == merged_dimension_mask->num_elements);

        for (i = 0; i < merged_dimension_mask->num_elements; i++)
        {
            merged_dimension_mask->mask[i] = dimension_mask->mask[i] && merged_dimension_mask->mask[i];
        }
    }
    else
    {
        long num_groups;
        long num_blocks;
        long num_block_elements;

        assert(dimension_mask->num_dimensions == 1);
        assert(merged_dimension_mask->num_dimensions > 1);
        assert(dim_index >= 0 && dim_index < merged_dimension_mask->num_dimensions);
        assert(merged_dimension_mask->dimension[dim_index] == dimension_mask->num_elements);

        /* The mask is split into three parts:
         *     num_elements = num_groups * num_blocks * num_block_elements.
         *
         * Here, num_groups is the product of dimensions [0, dim_index), num_blocks is dimension[dim_index],
         * and num_block_elements is the product of dimensions (dim_index, num_dimensions).
         */

        /* Calculate the number of times we have to filter the indices (i.e. the product of the higher dimensions) */
        num_groups = 1;
        for (i = 0; i < dim_index; i++)
        {
            num_groups *= merged_dimension_mask->dimension[i];
        }

        /* Calculate the number of blocks. */
        num_blocks = merged_dimension_mask->dimension[dim_index];

        /* Calculate the number of elements per block. */
        num_block_elements = merged_dimension_mask->num_elements / (num_groups * num_blocks);

        /* Reduce the mask along the specified dimension. For each index on the specified dimension, if any value in the
         * sub mask corresponding to this index is set to true, set the corresponding value in the reduced mask to true and
         * move to the next index.
         */
        for (i = 0; i < num_blocks; i++)
        {
            long j;

            if (dimension_mask->mask[i])
            {
                continue;
            }

            for (j = 0; j < num_groups; j++)
            {
                memset(merged_dimension_mask->mask + (j * num_blocks + i) * num_block_elements, 0,
                       num_block_elements * sizeof(uint8_t));
            }
        }
    }

    if (harp_dimension_mask_update_masked_length(merged_dimension_mask) != 0)
    {
        return -1;
    }

    return 0;
}

int harp_dimension_mask_set_simplify(harp_dimension_mask_set *dimension_mask_set)
{
    int i;

    /* Update the primary dimension mask such that it is consistent with all 2-D secondary dimension masks. */
    for (i = 0; i < HARP_NUM_DIM_TYPES; i++)
    {
        harp_dimension_mask *dimension_mask = dimension_mask_set[i];
        harp_dimension_mask *reduced_dimension_mask;

        if (dimension_mask == NULL || dimension_mask->num_dimensions <= 1)
        {
            continue;
        }
        assert(dimension_mask->num_dimensions == 2);

        if (dimension_mask_set[harp_dimension_time] == NULL)
        {
            /* Create dimension mask for the primary dimension if necessary. */
            if (harp_dimension_mask_new(1, dimension_mask->dimension, &dimension_mask_set[harp_dimension_time]) != 0)
            {
                return -1;
            }
        }

        /* Update mask for the primary dimension based on information from the (2-D) mask of the secondary dimension. */
        if (harp_dimension_mask_reduce(dimension_mask, 0, &reduced_dimension_mask) != 0)
        {
            return -1;
        }

        if (harp_dimension_mask_merge(reduced_dimension_mask, -1, dimension_mask_set[harp_dimension_time]) != 0)
        {
            harp_dimension_mask_delete(reduced_dimension_mask);
            return -1;
        }
        harp_dimension_mask_delete(reduced_dimension_mask);
    }

    /* Update all 2-D secondary dimension masks such that they are consistent with the primary dimension mask. */
    for (i = 0; i < HARP_NUM_DIM_TYPES; i++)
    {
        harp_dimension_mask *dimension_mask = dimension_mask_set[i];

        if (dimension_mask == NULL || dimension_mask->num_dimensions <= 1)
        {
            continue;
        }
        assert(dimension_mask->num_dimensions == 2);
        assert(dimension_mask_set[harp_dimension_time] != NULL);

        if (harp_dimension_mask_merge(dimension_mask_set[harp_dimension_time], 0, dimension_mask) != 0)
        {
            return -1;
        }
    }

    /* Remove dimensions masks that are always true. */
    for (i = 0; i < HARP_NUM_DIM_TYPES; i++)
    {
        harp_dimension_mask *dimension_mask = dimension_mask_set[i];

        if (dimension_mask == NULL)
        {
            continue;
        }
        assert(dimension_mask->mask != NULL || dimension_mask->num_elements == 0);

        if (count(dimension_mask->num_elements, dimension_mask->mask) == dimension_mask->num_elements)
        {
            harp_dimension_mask_delete(dimension_mask);
            dimension_mask_set[i] = NULL;
        }
    }

    return 0;
}
