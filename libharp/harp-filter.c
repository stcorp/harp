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

#include "harp-filter.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static void free_string_data(char **first, char **last)
{
    for (; first != last; first++)
    {
        if (*first != NULL)
        {
            free(*first);
            *first = NULL;
        }
    }
}

static void filter_array_int8(long num_source_elements, const uint8_t *mask, const int8_t *source,
                              long num_target_elements, int8_t *target)
{
    const int8_t *source_end;
    int8_t *target_end;

    target_end = target + num_target_elements;
    for (source_end = source + num_source_elements; source != source_end; source++, mask++)
    {
        if (*mask)
        {
            *target = *source;
            target++;
            num_target_elements--;
        }
    }

    for (; target != target_end; target++)
    {
        *target = 0;
    }
}

static void filter_array_int16(long num_source_elements, const uint8_t *mask, const int16_t *source,
                               long num_target_elements, int16_t *target)
{
    const int16_t *source_end;
    int16_t *target_end;

    target_end = target + num_target_elements;
    for (source_end = source + num_source_elements; source != source_end; source++, mask++)
    {
        if (*mask)
        {
            *target = *source;
            target++;
            num_target_elements--;
        }
    }

    for (; target != target_end; target++)
    {
        *target = 0;
    }
}

static void filter_array_int32(long num_source_elements, const uint8_t *mask, const int32_t *source,
                               long num_target_elements, int32_t *target)
{
    const int32_t *source_end;
    int32_t *target_end;

    target_end = target + num_target_elements;
    for (source_end = source + num_source_elements; source != source_end; source++, mask++)
    {
        if (*mask)
        {
            *target = *source;
            target++;
            num_target_elements--;
        }
    }

    for (; target != target_end; target++)
    {
        *target = 0;
    }
}

static void filter_array_float(long num_source_elements, const uint8_t *mask, const float *source,
                               long num_target_elements, float *target)
{
    const float *source_end;
    float *target_end;

    target_end = target + num_target_elements;
    for (source_end = source + num_source_elements; source != source_end; source++, mask++)
    {
        if (*mask)
        {
            *target = *source;
            target++;
        }
    }

    for (; target != target_end; target++)
    {
        *target = (float)harp_nan();
    }
}

static void filter_array_double(long num_source_elements, const uint8_t *mask, const double *source,
                                long num_target_elements, double *target)
{
    const double *source_end;
    double *target_end;

    target_end = target + num_target_elements;
    for (source_end = source + num_source_elements; source != source_end; source++, mask++)
    {
        if (*mask)
        {
            *target = *source;
            target++;
        }
    }

    for (; target != target_end; target++)
    {
        *target = harp_nan();
    }
}

static void filter_array_string(long num_source_elements, const uint8_t *mask, char **source,
                                long num_target_elements, char **target)
{
    char **source_end;
    char **target_end;

    target_end = target + num_target_elements;
    for (source_end = source + num_source_elements; source != source_end; source++, mask++)
    {
        if (*mask)
        {
            if (target != source)
            {
                if (*target != NULL)
                {
                    free(*target);
                }

                *target = *source;
                *source = NULL;
            }

            target++;
            num_target_elements--;
        }
    }

    free_string_data(target, target_end);
}

static void filter_array(harp_data_type data_type, long num_source_elements, const uint8_t *mask, harp_array source,
                         long num_target_elements, harp_array target)
{
    if (mask == NULL)
    {
        assert(num_source_elements == num_target_elements);

        if (target.ptr != source.ptr)
        {
            if (data_type == harp_type_string)
            {
                free_string_data(target.string_data, target.string_data + num_target_elements);
            }

            memcpy(target.ptr, source.ptr, num_target_elements * harp_get_size_for_type(data_type));

            if (data_type == harp_type_string)
            {
                memset(source.ptr, 0, num_source_elements * harp_get_size_for_type(data_type));
            }
        }
    }
    else
    {
        switch (data_type)
        {
            case harp_type_int8:
                filter_array_int8(num_source_elements, mask, source.int8_data, num_target_elements, target.int8_data);
                break;
            case harp_type_int16:
                filter_array_int16(num_source_elements, mask, source.int16_data, num_target_elements,
                                   target.int16_data);
                break;
            case harp_type_int32:
                filter_array_int32(num_source_elements, mask, source.int32_data, num_target_elements,
                                   target.int32_data);
                break;
            case harp_type_float:
                filter_array_float(num_source_elements, mask, source.float_data, num_target_elements,
                                   target.float_data);
                break;
            case harp_type_double:
                filter_array_double(num_source_elements, mask, source.double_data, num_target_elements,
                                    target.double_data);
                break;
            case harp_type_string:
                filter_array_string(num_source_elements, mask, source.string_data, num_target_elements,
                                    target.string_data);
                break;
            default:
                assert(0);
                exit(1);
        }
    }
}

/**
 * Filter the source array by copying elements to the target array for which the corresponding entry in the source mask
 * evaluates to true. The length of the source array is allowed to be larger than the length of the target array, as
 * long as the total number of elements that will be copied is smaller than or equal to the length of the target array.
 * \param data_type           Data type of source and target arrays
 * \param num_dimensions      Number of dimensions of source and target arrays
 * \param source_dimension    Dimension length for each source dimension
 * \param source_mask         Source mask; If NULL, all elements from the source array will be copied. Otherwise, the
 *     mask should have the same length as the source array.
 * \param source              Source array.
 * \param target_dimension    Resulting dimension length for each target dimension
 * \param target              Target array.
 */
void harp_array_filter(harp_data_type data_type, int num_dimensions, const long *source_dimension,
                       const uint8_t **source_mask, harp_array source, const long *target_dimension, harp_array target)
{
    long data_type_size;
    long source_stride[HARP_MAX_NUM_DIMS];
    long target_stride[HARP_MAX_NUM_DIMS];
    long source_index[HARP_MAX_NUM_DIMS] = { 0 };
    long target_index[HARP_MAX_NUM_DIMS] = { 0 };
    int dimension_index;
    int i;

    /* Special case for scalars. */
    if (num_dimensions == 0)
    {
        filter_array(data_type, 1, NULL, source, 1, target);
        return;
    }

    if (num_dimensions == 1)
    {
        /* Special case for 1-D arrays. */
        filter_array(data_type, *source_dimension, *source_mask, source, *target_dimension, target);
        return;
    }

    /* TODO: Consecutive dimensions for which the corresponding masks are NULL can be folded. */
    data_type_size = harp_get_size_for_type(data_type);
    source_stride[num_dimensions - 1] = target_stride[num_dimensions - 1] = data_type_size;
    for (i = num_dimensions - 1; i > 0; i--)
    {
        source_stride[i - 1] = source_stride[i] * source_dimension[i];
        target_stride[i - 1] = target_stride[i] * target_dimension[i];
    }

    dimension_index = 0;
    while (dimension_index >= 0)
    {
        while (dimension_index >= 0 && dimension_index < num_dimensions - 1)
        {
            if (source_mask[dimension_index] != NULL)
            {
                /* Skip indices on the current dimension that should be discarded according to the mask. */
                while (source_index[dimension_index] < source_dimension[dimension_index]
                       && !source_mask[dimension_index][source_index[dimension_index]])
                {
                    source_index[dimension_index]++;
                    source.ptr = (void *)(((char *)source.ptr) + source_stride[dimension_index]);
                }
            }

            if (source_index[dimension_index] < source_dimension[dimension_index])
            {
                /* This index on the current dimension should be kept. Move to the next dimension. */
                dimension_index++;
            }
            else
            {
                /* Set any remaining blocks on the current dimension of the target array to null. */
                long num_blocks;

                num_blocks = target_dimension[dimension_index] - target_index[dimension_index];
                assert(num_blocks >= 0);

                if (num_blocks > 0)
                {
                    harp_array_null(data_type, num_blocks * target_stride[dimension_index] / data_type_size, target);
                    target.ptr = (void *)(((char *)target.ptr) + num_blocks * target_stride[dimension_index]);
                }

                /* Reached the end of the current dimension. Set the index on the current dimension to zero and increase
                 * the previous dimension (unless we reached the end of the outermost dimension).
                 */
                source_index[dimension_index] = 0;
                target_index[dimension_index] = 0;
                dimension_index--;

                if (dimension_index >= 0)
                {
                    source_index[dimension_index]++;
                    target_index[dimension_index]++;
                }
            }
        }

        if (dimension_index > 0)
        {
            /* Filter the fastest running dimension. */
            filter_array(data_type, source_dimension[dimension_index], source_mask[dimension_index], source,
                         target_dimension[dimension_index], target);

            /* Move to the next index on the previous dimension. */
            source_index[dimension_index] = 0;
            target_index[dimension_index] = 0;
            dimension_index--;

            source_index[dimension_index]++;
            target_index[dimension_index]++;
            source.ptr = (void *)(((char *)source.ptr) + source_stride[dimension_index]);
            target.ptr = (void *)(((char *)target.ptr) + target_stride[dimension_index]);
        }
    }
}

int harp_variable_filter(harp_variable *variable, const harp_dimension_mask_set *dimension_mask_set)
{
    const uint8_t *mask[HARP_MAX_NUM_DIMS] = { 0 };
    long new_dimension[HARP_MAX_NUM_DIMS];
    long new_num_elements;
    int has_masks = 0;
    int has_2D_masks = 0;
    int i;

    if (dimension_mask_set == NULL)
    {
        return 0;
    }

    if (variable->num_dimensions == 0)
    {
        /* Scalars do not depend on any dimension, and will therefore not be affected by dimension masks. */
        return 0;
    }

    /* Determine the dimensions of the variable after filtering. */
    for (i = 0; i < variable->num_dimensions; i++)
    {
        harp_dimension_type dimension_type = variable->dimension_type[i];

        if (dimension_type == harp_dimension_independent)
        {
            new_dimension[i] = variable->dimension[i];
        }
        else
        {
            const harp_dimension_mask *dimension_mask = dimension_mask_set[dimension_type];

            if (dimension_mask == NULL)
            {
                new_dimension[i] = variable->dimension[i];
            }
            else
            {
                new_dimension[i] = dimension_mask->masked_dimension_length;
            }
        }
    }

    /* Determine the number of elements remaining after filtering. */
    new_num_elements = harp_get_num_elements(variable->num_dimensions, new_dimension);

    /* Get information about the applicable dimension masks. */
    for (i = 0; i < variable->num_dimensions; i++)
    {
        harp_dimension_type dimension_type = variable->dimension_type[i];

        if (dimension_type != harp_dimension_independent)
        {
            const harp_dimension_mask *dimension_mask = dimension_mask_set[dimension_type];

            if (dimension_mask == NULL)
            {
                continue;
            }

            assert(dimension_mask->mask != NULL);

            has_masks = 1;
            mask[i] = dimension_mask->mask;
            if (dimension_mask->num_dimensions == 2)
            {
                assert(i > 0 && variable->dimension_type[0] == harp_dimension_time);
                assert(dimension_type != harp_dimension_time);
                has_2D_masks = 1;
            }
        }
    }

    if (!has_masks)
    {
        /* No applicable dimension masks, hence no filtering required. */
        return 0;
    }

    if (!has_2D_masks)
    {
        harp_array_filter(variable->data_type, variable->num_dimensions, variable->dimension, mask, variable->data,
                          new_dimension, variable->data);
    }
    else
    {
        harp_array source = variable->data;
        harp_array target = variable->data;
        long source_stride;
        long target_stride;
        long mask_stride[HARP_MAX_NUM_DIMS] = { 0 };
        long j;

        /* Since the mask for time dimension is 1-D per definition, the fact that there are 2-D masks implies that there
         * is at least one mask for a secondary dimension.
         */
        assert(variable->dimension_type[0] == harp_dimension_time);

        /* Determine strides for iterating the (outer) time dimension. */
        source_stride = (variable->num_elements / variable->dimension[0]) * harp_get_size_for_type(variable->data_type);
        target_stride = (new_num_elements / new_dimension[0]) * harp_get_size_for_type(variable->data_type);

        for (i = 0; i < variable->num_dimensions; i++)
        {
            harp_dimension_type dimension_type = variable->dimension_type[i];

            if (dimension_type != harp_dimension_independent)
            {
                const harp_dimension_mask *dimension_mask = dimension_mask_set[dimension_type];

                if (dimension_mask == NULL)
                {
                    continue;
                }

                if (i == 0)
                {
                    assert(dimension_mask->num_dimensions == 1);
                    mask_stride[i] = 1;
                }
                else if (dimension_mask->num_dimensions == 2)
                {
                    assert(dimension_type != harp_dimension_time);
                    mask_stride[i] = dimension_mask->dimension[1];
                }
            }
        }

        for (j = 0; j < variable->dimension[0]; j++)
        {
            if (mask[0] == NULL || *mask[0])
            {
                harp_array_filter(variable->data_type, variable->num_dimensions - 1, &variable->dimension[1], &mask[1],
                                  source, &new_dimension[1], target);

                target.ptr = (void *)(((char *)target.ptr) + target_stride);
            }

            for (i = 0; i < variable->num_dimensions; i++)
            {
                if (mask[i] != NULL)
                {
                    mask[i] += mask_stride[i];
                }
            }

            source.ptr = (void *)(((char *)source.ptr) + source_stride);
        }
    }

    /* Free any remaining string data. */
    if (variable->data_type == harp_type_string)
    {
        free_string_data(variable->data.string_data + new_num_elements,
                         variable->data.string_data + variable->num_elements);
    }

    /* Adjust the size of the variable. */
    if (new_num_elements < variable->num_elements)
    {
        void *new_data;

        new_data = realloc(variable->data.ptr, new_num_elements * harp_get_size_for_type(variable->data_type));
        if (new_data == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %u bytes) (%s:%u)",
                           new_num_elements * harp_get_size_for_type(variable->data_type), __FILE__, __LINE__);
            return -1;
        }
        variable->data.ptr = new_data;
    }

    /* Update variable attributes. */
    variable->num_elements = new_num_elements;
    memcpy(variable->dimension, new_dimension, variable->num_dimensions * sizeof(long));

    return 0;
}

int harp_product_filter(harp_product *product, const harp_dimension_mask_set *dimension_mask_set)
{
    int i;

    if (dimension_mask_set == NULL)
    {
        return 0;
    }

    /* If the new length of any dimension is zero, return an empty product. This is not considered an error. */
    for (i = 0; i < HARP_NUM_DIM_TYPES; i++)
    {
        const harp_dimension_mask *dimension_mask = dimension_mask_set[i];

        if (dimension_mask != NULL && dimension_mask->masked_dimension_length == 0)
        {
            harp_product_remove_all_variables(product);
            return 0;
        }
    }

    /* Filter all variables in the product. */
    for (i = 0; i < product->num_variables; i++)
    {
        harp_variable *variable = product->variable[i];

        /* if we have a 2D dim filter then make sure that the variable has a time dimension */
        if (variable->num_dimensions > 0 && variable->dimension_type[0] != harp_dimension_time)
        {
            int j;

            for (j = 0; j < variable->num_dimensions; j++)
            {
                harp_dimension_type dimension_type = variable->dimension_type[j];

                if (dimension_type == harp_dimension_independent || dimension_mask_set[dimension_type] == NULL)
                {
                    continue;
                }

                if (dimension_mask_set[dimension_type]->num_dimensions == 2)
                {
                    break;
                }
            }

            if (j != variable->num_dimensions)
            {
                assert(product->dimension[harp_dimension_time] > 0);

                if (harp_variable_add_dimension(variable, 0, harp_dimension_time,
                                                product->dimension[harp_dimension_time]) != 0)
                {
                    return -1;
                }
            }
        }

        if (harp_variable_filter(variable, dimension_mask_set) != 0)
        {
            return -1;
        }
    }

    /* Update product dimensions. */
    for (i = 0; i < HARP_NUM_DIM_TYPES; i++)
    {
        const harp_dimension_mask *dimension_mask = dimension_mask_set[i];

        if (dimension_mask != NULL)
        {
            product->dimension[i] = dimension_mask->masked_dimension_length;
        }
    }

    return 0;
}
