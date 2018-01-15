/*
 * Copyright (C) 2015-2018 S[&]T, The Netherlands.
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
#include "harp-constants.h"
#include "harp-dimension-mask.h"
#include "harp-filter.h"
#include "harp-filter-collocation.h"
#include "harp-geometry.h"
#include "harp-operation.h"
#include "harp-program.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct read_buffer_struct
{
    harp_data_type data_type;
    long num_elements;
    size_t buffer_size;
    harp_array data;
} read_buffer;

typedef struct ingest_info_struct
{
    harp_ingestion_module *module;      /* ingestion module to use */
    harp_product_definition *product_definition;        /* definition of the product to ingest */
    coda_product *cproduct;     /* reference to coda product handle (in case CODA is used for ingestion) */

    void *user_data;    /* ingestion module specific information. */

    long dimension[HARP_NUM_DIM_TYPES]; /* length of each dimension (0 if not in use) */
    harp_dimension_mask_set *dimension_mask_set;        /* which indices along each dimension should be ingested */
    uint8_t product_mask;
    uint8_t *variable_mask;     /* indicates for each variable whether it should be included in the product */

    const char *basename;       /* product basename */
    harp_product *product;      /* resulting HARP product */

    read_buffer *block_buffer;  /* buffer used for storing results from 'read_all' and 'read_range' */
    int (*block_buffer_read_all) (void *user_data, harp_array data);    /* 'read_all' that was used to fill buffer */
    /* 'read_range' that was used to fill buffer */
    int (*block_buffer_read_range) (void *user_data, long index_offset, long index_length, harp_array data);
    long block_buffer_block_size;       /* byte size of each block */
    long block_buffer_index_offset;     /* index of first block in the buffer */
    long block_buffer_max_blocks;       /* total number of blocks for the variable */
    long block_buffer_num_blocks;       /* number of blocks that can fit in the buffer */
} ingest_info;

static void read_buffer_free_string_data(read_buffer *buffer)
{
    if (buffer->data_type == harp_type_string)
    {
        long i;

        for (i = 0; i < buffer->num_elements; i++)
        {
            if (buffer->data.string_data[i] != NULL)
            {
                free(buffer->data.string_data[i]);
            }

            buffer->data.string_data[i] = NULL;
        }
    }
}

static void read_buffer_delete(read_buffer *buffer)
{
    if (buffer != NULL)
    {
        if (buffer->data.ptr != NULL)
        {
            read_buffer_free_string_data(buffer);
            free(buffer->data.ptr);
        }

        free(buffer);
    }
}

static int read_buffer_new(harp_data_type data_type, long num_elements, read_buffer **new_buffer)
{
    read_buffer *buffer;

    buffer = (read_buffer *)malloc(sizeof(read_buffer));
    if (buffer == NULL)
    {
        return -1;
    }

    buffer->data_type = data_type;
    buffer->num_elements = num_elements;
    buffer->buffer_size = num_elements * harp_get_size_for_type(data_type);
    buffer->data.ptr = NULL;

    if (buffer->buffer_size > 0)
    {
        buffer->data.ptr = malloc(buffer->buffer_size);
        if (buffer->data.ptr == NULL)
        {
            read_buffer_delete(buffer);
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           buffer->buffer_size, __FILE__, __LINE__);
            return -1;
        }

        memset(buffer->data.ptr, 0, buffer->buffer_size);
    }

    *new_buffer = buffer;
    return 0;
}

static int read_buffer_resize(read_buffer *buffer, harp_data_type data_type, long num_elements)
{
    size_t new_buffer_size = num_elements * harp_get_size_for_type(data_type);

    if (new_buffer_size > buffer->buffer_size)
    {
        void *ptr;

        ptr = realloc(buffer->data.ptr, new_buffer_size);
        if (ptr == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           new_buffer_size, __FILE__, __LINE__);
            return -1;
        }
        buffer->data.ptr = ptr;
        buffer->buffer_size = new_buffer_size;
    }

    buffer->data_type = data_type;
    buffer->num_elements = num_elements;
    if (new_buffer_size > 0)
    {
        memset(buffer->data.ptr, 0, new_buffer_size);
    }

    return 0;
}

static void ingestion_done(ingest_info *info)
{
    if (info != NULL)
    {
        if (info->cproduct != NULL)
        {
            coda_close(info->cproduct);
        }

        if (info->user_data != NULL)
        {
            assert(info->module != NULL && info->module->ingestion_done != NULL);
            info->module->ingestion_done(info->user_data);
        }

        harp_dimension_mask_set_delete(info->dimension_mask_set);

        if (info->variable_mask != NULL)
        {
            free(info->variable_mask);
        }

        harp_product_delete(info->product);

        read_buffer_delete(info->block_buffer);

        free(info);
    }
}

static int ingestion_init(ingest_info **new_info)
{
    ingest_info *info;

    info = (ingest_info *)malloc(sizeof(ingest_info));
    if (info == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(ingest_info), __FILE__, __LINE__);
        return -1;
    }

    info->module = NULL;
    info->product_definition = NULL;
    info->cproduct = NULL;
    info->user_data = NULL;
    memset(info->dimension, 0, HARP_NUM_DIM_TYPES * sizeof(long));
    info->dimension_mask_set = NULL;
    info->product_mask = 1;
    info->variable_mask = NULL;
    info->basename = NULL;
    info->product = NULL;
    info->block_buffer = NULL;
    info->block_buffer_read_all = NULL;

    if (harp_dimension_mask_set_new(&info->dimension_mask_set) != 0)
    {
        ingestion_done(info);
        return -1;
    }

    *new_info = info;
    return 0;
}

static int read_all(ingest_info *info, const harp_variable_definition *variable_def, harp_array data)
{
    long dimension[HARP_MAX_NUM_DIMS];
    harp_array block;
    long block_stride;
    long num_elements;
    long index;
    int i;

    if (variable_def->read_all != NULL)
    {
        return variable_def->read_all(info->user_data, data);
    }

    for (i = 0; i < variable_def->num_dimensions; i++)
    {
        if (variable_def->dimension_type[i] == harp_dimension_independent)
        {
            dimension[i] = variable_def->dimension[i];
        }
        else
        {
            dimension[i] = info->dimension[variable_def->dimension_type[i]];
        }
    }
    num_elements = harp_get_num_elements(variable_def->num_dimensions, dimension);

    if (variable_def->read_range != NULL)
    {
        /* read_range() should have only been set for variables that have one or more dimensions */
        assert(variable_def->num_dimensions > 0);

        return variable_def->read_range(info->user_data, 0, dimension[0], data);
    }

    assert(variable_def->read_block != NULL);

    if (variable_def->num_dimensions == 0 || variable_def->dimension[0] == 1)
    {
        return variable_def->read_block(info->user_data, 0, data);
    }

    block = data;
    block_stride = harp_get_size_for_type(variable_def->data_type) * (num_elements / dimension[0]);

    for (index = 0; index < dimension[0]; index++)
    {
        if (variable_def->read_block(info->user_data, index, block) != 0)
        {
            return -1;
        }
        block.ptr = (void *)(((char *)block.ptr) + block_stride);
    }

    return 0;
}

static int read_block(ingest_info *info, const harp_variable_definition *variable_def, long index, harp_array data)
{
    if (variable_def->read_block != NULL)
    {
        return variable_def->read_block(info->user_data, index, data);
    }
    if (variable_def->read_all != NULL)
    {
        if (variable_def->num_dimensions == 0 || variable_def->dimension[0] == 1)
        {
            /* there is only one block, so read directly into the target buffer */
            return variable_def->read_all(info->user_data, data);
        }

        /* we need to use an internal buffer, filled using the read_all() callback */
        if (info->block_buffer_read_all != variable_def->read_all)
        {
            long dimension[HARP_MAX_NUM_DIMS];
            long num_elements;
            int i;

            for (i = 0; i < variable_def->num_dimensions; i++)
            {
                if (variable_def->dimension_type[i] == harp_dimension_independent)
                {
                    dimension[i] = variable_def->dimension[i];
                }
                else
                {
                    dimension[i] = info->dimension[variable_def->dimension_type[i]];
                }
            }
            num_elements = harp_get_num_elements(variable_def->num_dimensions, dimension);

            if (info->block_buffer == NULL)
            {
                if (read_buffer_new(variable_def->data_type, num_elements, &info->block_buffer) != 0)
                {
                    return -1;
                }
            }
            else
            {
                if (read_buffer_resize(info->block_buffer, variable_def->data_type, num_elements) != 0)
                {
                    return -1;
                }
            }
            if (variable_def->read_all(info->user_data, info->block_buffer->data) != 0)
            {
                return -1;
            }
            info->block_buffer_read_all = variable_def->read_all;
            info->block_buffer_read_range = NULL;
            info->block_buffer_block_size =
                harp_get_size_for_type(variable_def->data_type) * num_elements / dimension[0];
        }
    }
    else
    {
        assert(variable_def->read_range != NULL);

        /* we need to use an internal buffer, filled using the read_range() callback */
        if (info->block_buffer_read_range != variable_def->read_range)
        {
            long dimension[HARP_MAX_NUM_DIMS];
            long num_block_elements;
            int i;

            /* read_range() should have only been set for variables that have one or more dimensions */
            assert(variable_def->num_dimensions > 0);

            for (i = 0; i < variable_def->num_dimensions; i++)
            {
                if (variable_def->dimension_type[i] == harp_dimension_independent)
                {
                    dimension[i] = variable_def->dimension[i];
                }
                else
                {
                    dimension[i] = info->dimension[variable_def->dimension_type[i]];
                }
            }
            info->block_buffer_max_blocks = dimension[0];
            num_block_elements = harp_get_num_elements(variable_def->num_dimensions, dimension) / dimension[0];
            info->block_buffer_num_blocks = variable_def->get_optimal_range_length(info->user_data);
            if (info->block_buffer_num_blocks > info->block_buffer_max_blocks)
            {
                info->block_buffer_num_blocks = info->block_buffer_max_blocks;
            }

            if (info->block_buffer == NULL)
            {
                if (read_buffer_new(variable_def->data_type, info->block_buffer_num_blocks * num_block_elements,
                                    &info->block_buffer) != 0)
                {
                    return -1;
                }
            }
            else
            {
                if (read_buffer_resize(info->block_buffer, variable_def->data_type,
                                       info->block_buffer_num_blocks * num_block_elements) != 0)
                {
                    return -1;
                }
            }
            info->block_buffer_read_range = variable_def->read_range;
            info->block_buffer_read_all = NULL;
            info->block_buffer_block_size = harp_get_size_for_type(variable_def->data_type) * num_block_elements;
            /* set index_offset to an invalid value, so a read will be triggered */
            info->block_buffer_index_offset = info->block_buffer_num_blocks;
        }

        if (index < info->block_buffer_index_offset ||
            index >= info->block_buffer_index_offset + info->block_buffer_num_blocks)
        {
            long block_index = index / info->block_buffer_num_blocks;
            long num_blocks;

            info->block_buffer_index_offset = block_index * info->block_buffer_num_blocks;
            num_blocks = info->block_buffer_num_blocks;
            if (info->block_buffer_index_offset + num_blocks > info->block_buffer_max_blocks)
            {
                num_blocks = info->block_buffer_max_blocks - info->block_buffer_index_offset;
            }
            if (variable_def->read_range(info->user_data, info->block_buffer_index_offset, num_blocks,
                                         info->block_buffer->data) != 0)
            {
                return -1;
            }
        }

        index -= info->block_buffer_index_offset;
    }

    memcpy(data.ptr, &info->block_buffer->data.int8_data[index * info->block_buffer_block_size],
           info->block_buffer_block_size);

    return 0;
}

static int get_variable(ingest_info *info, const harp_variable_definition *variable_def,
                        const harp_dimension_mask_set *dimension_mask_set, harp_variable **new_variable)
{
    harp_variable *variable;

    if (harp_variable_definition_exclude(variable_def, info->user_data))
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "cannot read variable '%s' (variable unavailable)",
                       variable_def->name);
        return -1;
    }

    if (variable_def->num_dimensions == 0)
    {
        /* special case for scalars */
        if (harp_variable_new(variable_def->name, variable_def->data_type, 0, NULL, NULL, &variable) != 0)
        {
            return -1;
        }

        if (read_all(info, variable_def, variable->data) != 0)
        {
            harp_variable_delete(variable);
            return -1;
        }
    }
    else
    {
        const harp_dimension_mask *dimension_mask[HARP_MAX_NUM_DIMS];
        harp_dimension_type dimension_type[HARP_MAX_NUM_DIMS];
        long dimension[HARP_MAX_NUM_DIMS];
        long masked_dimension[HARP_MAX_NUM_DIMS];
        int has_2D_masks = 0;
        int has_dimension_masks = 0;
        int has_secondary_masks = 0;
        long i, j;

        /* variable has one or more dimensions */

        /* determine the dimensions of the variable (with and without using the applicable dimension masks) */
        for (i = 0; i < variable_def->num_dimensions; i++)
        {
            dimension_type[i] = variable_def->dimension_type[i];
            if (dimension_type[i] == harp_dimension_independent)
            {
                dimension[i] = variable_def->dimension[i];
                dimension_mask[i] = NULL;
                masked_dimension[i] = variable_def->dimension[i];
            }
            else
            {
                dimension[i] = info->dimension[dimension_type[i]];
                if (dimension_mask_set == NULL || dimension_mask_set[dimension_type[i]] == NULL)
                {
                    dimension_mask[i] = NULL;
                    masked_dimension[i] = info->dimension[dimension_type[i]];
                }
                else
                {
                    dimension_mask[i] = dimension_mask_set[dimension_type[i]];
                    masked_dimension[i] = dimension_mask[i]->masked_dimension_length;
                    has_dimension_masks = 1;
                    if (i != 0)
                    {
                        has_secondary_masks = 1;
                    }
                    if (dimension_mask[i]->num_dimensions == 2)
                    {
                        has_2D_masks = 1;
                    }
                }
            }
        }

        /* to be able to apply 2D dimension masks to a variable that does not depend on the time dimension,
         * the variable is expanded by adding the time dimension */
        if (has_2D_masks && variable_def->dimension_type[0] != harp_dimension_time)
        {
            const uint8_t *mask[HARP_MAX_NUM_DIMS];
            long mask_stride[HARP_MAX_NUM_DIMS - 1];
            read_buffer *buffer;
            harp_array block;
            long num_buffer_elements;
            long block_stride;
            long num_dimensions;

            /* update dimension arrays to include the added time dimension */
            for (i = variable_def->num_dimensions; i >= 1; i--)
            {
                dimension_type[i] = dimension_type[i - 1];
                dimension[i] = dimension[i - 1];
                dimension_mask[i] = dimension_mask[i - 1];
                masked_dimension[i] = masked_dimension[i - 1];
            }
            dimension_type[0] = harp_dimension_time;
            dimension[0] = info->dimension[harp_dimension_time];
            if (dimension_mask_set == NULL || dimension_mask_set[harp_dimension_time] == NULL)
            {
                dimension_mask[0] = NULL;
                masked_dimension[0] = info->dimension[harp_dimension_time];
            }
            else
            {
                dimension_mask[0] = dimension_mask_set[harp_dimension_time];
                masked_dimension[0] = dimension_mask[0]->masked_dimension_length;
            }
            num_dimensions = variable_def->num_dimensions + 1;

            /* create variable */
            if (harp_variable_new(variable_def->name, variable_def->data_type, num_dimensions, dimension_type,
                                  masked_dimension, &variable) != 0)
            {
                return -1;
            }

            /* we read the whole non-time-dependent variable data once (in full) and then filter for each sample */
            num_buffer_elements = harp_get_num_elements(num_dimensions - 1, &dimension[1]);
            if (read_buffer_new(variable->data_type, num_buffer_elements, &buffer) != 0)
            {
                harp_variable_delete(variable);
                return -1;
            }
            if (read_all(info, variable_def, buffer->data) != 0)
            {
                read_buffer_delete(buffer);
                harp_variable_delete(variable);
                return -1;
            }

            for (i = 0; i < num_dimensions; i++)
            {
                if (dimension_mask[i] == NULL)
                {
                    mask[i] = NULL;
                }
                else
                {
                    mask[i] = dimension_mask[i]->mask;
                    if (dimension_mask[i]->num_dimensions == 2)
                    {
                        mask_stride[i] = dimension_mask[i]->dimension[1];
                    }
                    else
                    {
                        assert(dimension_mask[i]->num_dimensions == 1);
                        mask_stride[i] = 0;
                    }
                }
            }

            block = variable->data;
            block_stride = harp_get_size_for_type(variable->data_type) * (variable->num_elements /
                                                                          variable->dimension[0]);

            for (i = 0; i < dimension[0]; i++)
            {
                if (dimension_mask[0] == NULL || dimension_mask[0]->mask[i])
                {
                    harp_array_filter(variable->data_type, num_dimensions - 1, &dimension[1], &mask[1], buffer->data,
                                      &masked_dimension[1], block);

                    block.ptr = (void *)(((char *)block.ptr) + block_stride);
                }

                for (j = 1; j < variable->num_dimensions; j++)
                {
                    if (mask[j] != NULL)
                    {
                        mask[j] += mask_stride[j];
                    }
                }
            }

            read_buffer_delete(buffer);
        }
        else
        {
            /* create variable */
            if (harp_variable_new(variable_def->name, variable_def->data_type, variable_def->num_dimensions,
                                  variable_def->dimension_type, masked_dimension, &variable) != 0)
            {
                return -1;
            }

            if (has_dimension_masks)
            {
                harp_array block;
                long block_stride;

                block = variable->data;
                block_stride = harp_get_size_for_type(variable->data_type) * (variable->num_elements /
                                                                              variable->dimension[0]);

                if (has_secondary_masks)
                {
                    const uint8_t *mask[HARP_MAX_NUM_DIMS];
                    long mask_stride[HARP_MAX_NUM_DIMS - 1];
                    long num_buffer_elements;
                    read_buffer *buffer;

                    num_buffer_elements = harp_get_num_elements(variable_def->num_dimensions - 1, &dimension[1]);
                    if (read_buffer_new(variable->data_type, num_buffer_elements, &buffer) != 0)
                    {
                        harp_variable_delete(variable);
                        return -1;
                    }

                    for (i = 0; i < variable->num_dimensions; i++)
                    {
                        if (dimension_mask[i] == NULL)
                        {
                            mask[i] = NULL;
                        }
                        else
                        {
                            mask[i] = dimension_mask[i]->mask;
                            if (dimension_mask[i]->num_dimensions == 2)
                            {
                                assert(i != 0);
                                mask_stride[i] = dimension_mask[i]->dimension[1];
                            }
                            else
                            {
                                assert(dimension_mask[i]->num_dimensions == 1);
                                mask_stride[i] = 0;
                            }
                        }
                    }

                    for (i = 0; i < dimension[0]; i++)
                    {
                        if (mask[0] == NULL || mask[0][i])
                        {
                            if (read_block(info, variable_def, i, buffer->data) != 0)
                            {
                                read_buffer_delete(buffer);
                                harp_variable_delete(variable);
                                return -1;
                            }

                            harp_array_filter(variable->data_type, variable_def->num_dimensions - 1, &dimension[1],
                                              &mask[1], buffer->data, &masked_dimension[1], block);
                            read_buffer_free_string_data(buffer);

                            block.ptr = (void *)(((char *)block.ptr) + block_stride);
                        }

                        for (j = 1; j < variable->num_dimensions; j++)
                        {
                            if (mask[j] != NULL)
                            {
                                mask[j] += mask_stride[j];
                            }
                        }
                    }

                    read_buffer_delete(buffer);
                }
                else
                {
                    /* we can read directly into the variable */
                    assert(dimension_mask[0] != NULL);
                    for (i = 0; i < dimension[0]; i++)
                    {
                        if (!dimension_mask[0]->mask[i])
                        {
                            continue;
                        }
                        if (read_block(info, variable_def, i, block) != 0)
                        {
                            harp_variable_delete(variable);
                            return -1;
                        }
                        block.ptr = (void *)(((char *)block.ptr) + block_stride);
                    }
                }
            }
            else
            {
                if (read_all(info, variable_def, variable->data) != 0)
                {
                    harp_variable_delete(variable);
                    return -1;
                }
            }
        }
    }

    /* copy variable attributes */
    if (variable_def->description != NULL)
    {
        variable->description = strdup(variable_def->description);
        if (variable->description == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                           __LINE__);
            harp_variable_delete(variable);
            return -1;
        }
    }

    if (variable_def->unit != NULL)
    {
        variable->unit = strdup(variable_def->unit);
        if (variable->unit == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                           __LINE__);
            harp_variable_delete(variable);
            return -1;
        }
    }

    variable->valid_min = variable_def->valid_min;
    variable->valid_max = variable_def->valid_max;

    if (variable_def->num_enum_values > 0)
    {
        if (harp_variable_set_enumeration_values(variable, variable_def->num_enum_values,
                                                 (const char **)variable_def->enum_name) != 0)
        {
            harp_variable_delete(variable);
            return -1;
        }
    }

    *new_variable = variable;
    return 0;
}

static int find_variable_definition(ingest_info *info, const char *name, harp_variable_definition **variable_def)
{
    int index;

    index = harp_product_definition_get_variable_index(info->product_definition, name);
    if (index < 0)
    {
        return -1;
    }
    if (!info->variable_mask[index])
    {
        return -1;
    }

    *variable_def = info->product_definition->variable_definition[index];

    return 0;
}

static int init_product_dimensions(ingest_info *info)
{
    memset(info->dimension, 0, HARP_NUM_DIM_TYPES * sizeof(long));
    if (info->product_definition->read_dimensions(info->user_data, info->dimension) != 0)
    {
        return -1;
    }

    return 0;
}

static int init_variable_mask(ingest_info *info)
{
    int i;

    /* allocate the variable mask */
    info->variable_mask = (uint8_t *)malloc(info->product_definition->num_variable_definitions * sizeof(uint8_t));
    if (info->variable_mask == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       info->product_definition->num_variable_definitions * sizeof(uint8_t), __FILE__, __LINE__);
        return -1;
    }

    /* initialize variable mask according to the availability of each variable */
    for (i = 0; i < info->product_definition->num_variable_definitions; i++)
    {
        info->variable_mask[i] = !harp_variable_definition_exclude(info->product_definition->variable_definition[i],
                                                                   info->user_data);
    }

    return 0;
}

static int product_has_empty_dimensions(ingest_info *info)
{
    int i;

    for (i = 0; i < info->product_definition->num_variable_definitions; i++)
    {
        const harp_variable_definition *variable_def;
        int j;

        variable_def = info->product_definition->variable_definition[i];
        for (j = 0; j < variable_def->num_dimensions; j++)
        {
            harp_dimension_type dimension_type;

            dimension_type = variable_def->dimension_type[j];
            if (dimension_type != harp_dimension_independent && info->dimension[dimension_type] == 0)
            {
                return 1;
            }
        }
    }

    return 0;
}

static int product_has_variables(ingest_info *info)
{
    int i;

    for (i = 0; i < info->product_definition->num_variable_definitions; i++)
    {
        if (info->variable_mask[i])
        {
            return 1;
        }
    }

    return 0;
}

static int dimension_mask_set_has_empty_masks(const harp_dimension_mask_set *dimension_mask_set)
{
    int i;

    for (i = 0; i < HARP_NUM_DIM_TYPES; i++)
    {
        if (dimension_mask_set[i] != NULL && dimension_mask_set[i]->masked_dimension_length == 0)
        {
            return 1;
        }
    }

    return 0;
}

static int execute_value_filter(ingest_info *info, harp_program *program)
{
    harp_variable_definition *variable_def;
    read_buffer *buffer;
    const char *variable_name;
    int num_operations = 1;
    int data_type_size;
    long i, j;
    int k;

    if (info->product_mask == 0)
    {
        return 0;
    }

    if (harp_operation_get_variable_name(program->operation[program->current_index], &variable_name) != 0)
    {
        return -1;
    }

    if (find_variable_definition(info, variable_name, &variable_def) != 0)
    {
        /* non existent variable is an error */
        harp_set_error(HARP_ERROR_OPERATION, "cannot filter on non-existent variable %s", variable_name);
        return -1;
    }
    data_type_size = harp_get_size_for_type(variable_def->data_type);

    /* if the next operations are also value filters on the same variable then include them */
    while (program->current_index + num_operations < program->num_operations)
    {
        const char *next_variable_name;

        if (!harp_operation_is_value_filter(program->operation[program->current_index + num_operations]))
        {
            break;
        }
        if (harp_operation_get_variable_name(program->operation[program->current_index + num_operations],
                                             &next_variable_name) != 0)
        {
            return -1;
        }
        if (strcmp(variable_name, next_variable_name) != 0)
        {
            break;
        }
        num_operations++;
    }

    if (variable_def->unit != NULL)
    {
        for (k = 0; k < num_operations; k++)
        {
            if (harp_operation_set_value_unit(program->operation[program->current_index + k], variable_def->unit) != 0)
            {
                return -1;
            }
        }
    }

    if (variable_def->num_dimensions == 0)
    {
        if (read_buffer_new(variable_def->data_type, 1, &buffer) != 0)
        {
            return -1;
        }

        if (read_block(info, variable_def, 0, buffer->data) != 0)
        {
            read_buffer_delete(buffer);
            return -1;
        }

        for (k = 0; k < num_operations; k++)
        {
            int result;

            if (harp_operation_is_string_value_filter(program->operation[program->current_index + k]))
            {
                harp_operation_string_value_filter *operation;

                operation = (harp_operation_string_value_filter *)program->operation[program->current_index + k];
                result = operation->eval(operation, variable_def->num_enum_values, variable_def->enum_name,
                                         variable_def->data_type, buffer->data.ptr);
            }
            else
            {
                harp_operation_numeric_value_filter *operation;

                operation = (harp_operation_numeric_value_filter *)program->operation[program->current_index + k];
                result = operation->eval(operation, variable_def->data_type, buffer->data.ptr);
            }
            if (result < 0)
            {
                read_buffer_delete(buffer);
                return -1;
            }
            info->product_mask = result;
        }

        read_buffer_delete(buffer);
    }
    else if (variable_def->num_dimensions == 1 && variable_def->dimension_type[0] != harp_dimension_independent)
    {
        harp_dimension_type dimension_type;
        harp_dimension_mask *dimension_mask;

        dimension_type = variable_def->dimension_type[0];

        if (info->dimension_mask_set[dimension_type] == NULL)
        {
            long dimension = info->dimension[dimension_type];

            if (harp_dimension_mask_new(1, &dimension, &info->dimension_mask_set[dimension_type]) != 0)
            {
                return -1;
            }
        }
        dimension_mask = info->dimension_mask_set[dimension_type];

        if (info->dimension_mask_set[dimension_type]->num_dimensions == 2)
        {
            /* create a reduced (1-D) temporary dimension mask from the 2-D dimension mask */
            if (harp_dimension_mask_reduce(info->dimension_mask_set[dimension_type], 1, &dimension_mask) != 0)
            {
                return -1;
            }
        }

        if (read_buffer_new(variable_def->data_type, 1, &buffer) != 0)
        {
            if (info->dimension_mask_set[dimension_type]->num_dimensions == 2)
            {
                harp_dimension_mask_delete(dimension_mask);
            }
            return -1;
        }

        for (i = 0; i < info->dimension[dimension_type]; i++)
        {
            if (dimension_mask->mask[i])
            {
                if (read_block(info, variable_def, i, buffer->data) != 0)
                {
                    if (info->dimension_mask_set[dimension_type]->num_dimensions == 2)
                    {
                        harp_dimension_mask_delete(dimension_mask);
                    }
                    read_buffer_delete(buffer);
                    return -1;
                }

                for (k = 0; k < num_operations; k++)
                {
                    if (dimension_mask->mask[i])
                    {
                        int result;

                        if (harp_operation_is_string_value_filter(program->operation[program->current_index + k]))
                        {
                            harp_operation_string_value_filter *operation;

                            operation =
                                (harp_operation_string_value_filter *)program->operation[program->current_index + k];
                            result = operation->eval(operation, variable_def->num_enum_values, variable_def->enum_name,
                                                     variable_def->data_type, buffer->data.int8_data);
                        }
                        else
                        {
                            harp_operation_numeric_value_filter *operation;

                            operation =
                                (harp_operation_numeric_value_filter *)program->operation[program->current_index + k];
                            result = operation->eval(operation, variable_def->data_type, buffer->data.int8_data);
                        }
                        if (result < 0)
                        {
                            if (info->dimension_mask_set[dimension_type]->num_dimensions == 2)
                            {
                                harp_dimension_mask_delete(dimension_mask);
                            }
                            read_buffer_delete(buffer);
                            return -1;
                        }
                        dimension_mask->mask[i] = result;
                    }
                }
                if (!dimension_mask->mask[i])
                {
                    dimension_mask->masked_dimension_length--;
                }
            }
        }

        read_buffer_delete(buffer);

        if (info->dimension_mask_set[dimension_type]->num_dimensions == 2)
        {
            /* propagate the reduced (1-D) temporary dimension mask to the 2-D dimension mask */
            if (harp_dimension_mask_merge(dimension_mask, 1, info->dimension_mask_set[dimension_type]) != 0)
            {
                harp_dimension_mask_delete(dimension_mask);
                return -1;
            }
            harp_dimension_mask_delete(dimension_mask);
        }
    }
    else if (variable_def->num_dimensions == 2 && variable_def->dimension_type[0] == harp_dimension_time &&
             variable_def->dimension_type[1] != harp_dimension_independent &&
             variable_def->dimension_type[1] != harp_dimension_time)
    {
        harp_dimension_type dimension_type;
        harp_dimension_mask *time_mask;
        harp_dimension_mask *dimension_mask;
        long index = 0;

        dimension_type = variable_def->dimension_type[1];

        if (info->dimension_mask_set[harp_dimension_time] == NULL)
        {
            long dimension = info->dimension[harp_dimension_time];

            if (harp_dimension_mask_new(1, &dimension, &info->dimension_mask_set[harp_dimension_time]) != 0)
            {
                return -1;
            }
        }
        time_mask = info->dimension_mask_set[harp_dimension_time];

        if (info->dimension_mask_set[dimension_type] == NULL)
        {
            long dimension[2];

            dimension[0] = info->dimension[harp_dimension_time];
            dimension[1] = info->dimension[dimension_type];
            if (harp_dimension_mask_new(2, dimension, &info->dimension_mask_set[dimension_type]) != 0)
            {
                return -1;
            }
        }
        else if (info->dimension_mask_set[dimension_type]->num_dimensions != 2)
        {
            /* extend the existing 1-D mask to 2-D by repeating it along the outer dimension */
            assert(info->dimension_mask_set[dimension_type]->num_dimensions == 1);
            if (harp_dimension_mask_prepend_dimension(info->dimension_mask_set[dimension_type],
                                                      info->dimension[harp_dimension_time]) != 0)
            {
                return -1;
            }
        }
        dimension_mask = info->dimension_mask_set[dimension_type];

        if (read_buffer_new(variable_def->data_type, info->dimension[dimension_type], &buffer) != 0)
        {
            return -1;
        }

        dimension_mask->masked_dimension_length = 0;
        for (i = 0; i < info->dimension[harp_dimension_time]; i++)
        {
            if (time_mask->mask[i])
            {
                long new_dimension_length = 0;

                if (read_block(info, variable_def, i, buffer->data) != 0)
                {
                    read_buffer_delete(buffer);
                    return -1;
                }

                for (j = 0; j < info->dimension[dimension_type]; j++)
                {
                    if (dimension_mask->mask[index])
                    {
                        for (k = 0; k < num_operations; k++)
                        {
                            if (dimension_mask->mask[index])
                            {
                                harp_operation *operation = program->operation[program->current_index + k];
                                int result;

                                if (harp_operation_is_string_value_filter(operation))
                                {
                                    harp_operation_string_value_filter *string_operation;

                                    string_operation = (harp_operation_string_value_filter *)operation;
                                    result = string_operation->eval(string_operation, variable_def->num_enum_values,
                                                                    variable_def->enum_name, variable_def->data_type,
                                                                    &buffer->data.int8_data[j * data_type_size]);
                                }
                                else
                                {
                                    harp_operation_numeric_value_filter *numeric_operation;

                                    numeric_operation = (harp_operation_numeric_value_filter *)operation;
                                    result = numeric_operation->eval(numeric_operation, variable_def->data_type,
                                                                     &buffer->data.int8_data[j * data_type_size]);
                                }
                                if (result < 0)
                                {
                                    read_buffer_delete(buffer);
                                    return -1;
                                }
                                dimension_mask->mask[index] = result;
                            }
                        }
                        if (dimension_mask->mask[index])
                        {
                            new_dimension_length++;
                        }
                    }
                    index++;
                }

                read_buffer_free_string_data(buffer);

                if (new_dimension_length == 0)
                {
                    time_mask->mask[i] = 0;
                    time_mask->masked_dimension_length--;
                }
                else if (new_dimension_length > dimension_mask->masked_dimension_length)
                {
                    dimension_mask->masked_dimension_length = new_dimension_length;
                }
            }
            else
            {
                index += info->dimension[dimension_type];
            }
        }

        read_buffer_delete(buffer);
    }
    else
    {
        harp_set_error(HARP_ERROR_OPERATION, "variable '%s' has invalid dimensions for filtering", variable_name);
        return -1;
    }

    if (dimension_mask_set_has_empty_masks(info->dimension_mask_set))
    {
        info->product_mask = 0;
    }

    /* jump to the last operation in the list that we performed */
    program->current_index += num_operations - 1;

    return 0;
}

static int execute_point_filter(ingest_info *info, harp_program *program)
{
    harp_variable_definition *latitude_def;
    harp_variable_definition *longitude_def;
    harp_variable *latitude;
    harp_variable *longitude;
    uint8_t *mask;
    int num_operations = 1;
    long num_points;
    long i;
    int k;

    if (find_variable_definition(info, "latitude", &latitude_def) != 0)
    {
        harp_set_error(HARP_ERROR_OPERATION, "point filter expected variable latitude");
        return -1;
    }
    if (find_variable_definition(info, "longitude", &longitude_def) != 0)
    {
        harp_set_error(HARP_ERROR_OPERATION, "point filter expected variable longitude");
        return -1;
    }

    if (get_variable(info, latitude_def, NULL, &latitude) != 0)
    {
        return -1;
    }
    if (get_variable(info, longitude_def, NULL, &longitude) != 0)
    {
        harp_variable_delete(latitude);
        return -1;
    }

    if (harp_variable_convert_unit(latitude, "degree_north") != 0)
    {
        harp_variable_delete(latitude);
        harp_variable_delete(longitude);
        return -1;
    }
    if (harp_variable_convert_unit(longitude, "degree_east") != 0)
    {
        harp_variable_delete(latitude);
        harp_variable_delete(longitude);
        return -1;
    }

    if (latitude->num_dimensions == 0)
    {
        if (harp_variable_add_dimension(latitude, 0, harp_dimension_time, info->dimension[harp_dimension_time]) != 0)
        {
            harp_variable_delete(latitude);
            harp_variable_delete(longitude);
            return -1;
        }
    }
    if (longitude->num_dimensions == 0)
    {
        if (harp_variable_add_dimension(longitude, 0, harp_dimension_time, info->dimension[harp_dimension_time]) != 0)
        {
            harp_variable_delete(latitude);
            harp_variable_delete(longitude);
            return -1;
        }
    }

    if (latitude->num_dimensions != 1 || latitude->dimension_type[0] != harp_dimension_time)
    {
        harp_set_error(HARP_ERROR_OPERATION, "variable 'latitude' has invalid dimensions for filtering");
        harp_variable_delete(latitude);
        harp_variable_delete(longitude);
        return -1;
    }
    if (longitude->num_dimensions != 1 || longitude->dimension_type[0] != harp_dimension_time)
    {
        harp_set_error(HARP_ERROR_OPERATION, "variable 'longitude' has invalid dimensions for filtering");
        harp_variable_delete(latitude);
        harp_variable_delete(longitude);
        return -1;
    }

    num_points = latitude->dimension[0];

    /* if the next operations are also point filters then include them */
    while (program->current_index + num_operations < program->num_operations)
    {
        if (!harp_operation_is_point_filter(program->operation[program->current_index + num_operations]))
        {
            break;
        }
        num_operations++;
    }

    if (info->dimension_mask_set[harp_dimension_time] == NULL)
    {
        long dimension = num_points;

        if (harp_dimension_mask_new(1, &dimension, &info->dimension_mask_set[harp_dimension_time]) != 0)
        {
            harp_variable_delete(latitude);
            harp_variable_delete(longitude);
            return -1;
        }
    }

    mask = info->dimension_mask_set[harp_dimension_time]->mask;

    for (i = 0; i < num_points; i++)
    {
        if (mask[i])
        {
            harp_spherical_point point;

            point.lat = latitude->data.double_data[i];
            point.lon = longitude->data.double_data[i];
            harp_spherical_point_rad_from_deg(&point);
            harp_spherical_point_check(&point);

            for (k = 0; k < num_operations; k++)
            {
                if (mask[i])
                {
                    harp_operation_point_filter *operation;
                    int result;

                    operation = (harp_operation_point_filter *)program->operation[program->current_index + k];
                    result = operation->eval(operation, &point);
                    if (result < 0)
                    {
                        harp_variable_delete(latitude);
                        harp_variable_delete(longitude);
                        return -1;
                    }
                    mask[i] = result;
                }
            }
            if (!mask[i])
            {
                info->dimension_mask_set[harp_dimension_time]->masked_dimension_length--;
            }
        }
    }

    if (dimension_mask_set_has_empty_masks(info->dimension_mask_set))
    {
        info->product_mask = 0;
    }

    harp_variable_delete(latitude);
    harp_variable_delete(longitude);

    /* jump to the last operation in the list that we performed */
    program->current_index += num_operations - 1;

    return 0;
}

static int execute_polygon_filter(ingest_info *info, harp_program *program)
{
    harp_variable_definition *latitude_bounds_def;
    harp_variable_definition *longitude_bounds_def;
    harp_variable *latitude_bounds;
    harp_variable *longitude_bounds;
    uint8_t *mask;
    int num_operations = 1;
    long num_areas;
    long num_points;
    long i;
    int k;

    if (find_variable_definition(info, "latitude_bounds", &latitude_bounds_def) != 0)
    {
        harp_set_error(HARP_ERROR_OPERATION, "area filter expected variable latitude_bounds");
        return -1;
    }
    if (find_variable_definition(info, "longitude_bounds", &longitude_bounds_def) != 0)
    {
        harp_set_error(HARP_ERROR_OPERATION, "area filter expected variable longitude_bounds");
        return -1;
    }

    if (get_variable(info, latitude_bounds_def, NULL, &latitude_bounds) != 0)
    {
        return -1;
    }
    if (get_variable(info, longitude_bounds_def, NULL, &longitude_bounds) != 0)
    {
        harp_variable_delete(latitude_bounds);
        return -1;
    }

    if (harp_variable_convert_unit(latitude_bounds, "degree_north") != 0)
    {
        harp_variable_delete(latitude_bounds);
        harp_variable_delete(longitude_bounds);
        return -1;
    }
    if (harp_variable_convert_unit(longitude_bounds, "degree_east") != 0)
    {
        harp_variable_delete(latitude_bounds);
        harp_variable_delete(longitude_bounds);
        return -1;
    }

    if (latitude_bounds->num_dimensions == 0 || latitude_bounds->dimension_type[0] != harp_dimension_time)
    {
        if (harp_variable_add_dimension(latitude_bounds, 0, harp_dimension_time, info->dimension[harp_dimension_time])
            != 0)
        {
            harp_variable_delete(latitude_bounds);
            harp_variable_delete(longitude_bounds);
            return -1;
        }
    }
    if (longitude_bounds->num_dimensions == 0 || longitude_bounds->dimension_type[0] != harp_dimension_time)
    {
        if (harp_variable_add_dimension(longitude_bounds, 0, harp_dimension_time, info->dimension[harp_dimension_time])
            != 0)
        {
            harp_variable_delete(latitude_bounds);
            harp_variable_delete(longitude_bounds);
            return -1;
        }
    }

    if (latitude_bounds->num_dimensions != 2 || latitude_bounds->dimension_type[0] != harp_dimension_time)
    {
        harp_set_error(HARP_ERROR_OPERATION, "variable 'latitude_bounds' has invalid dimensions for filtering");
        harp_variable_delete(latitude_bounds);
        harp_variable_delete(longitude_bounds);
        return -1;
    }
    if (longitude_bounds->num_dimensions != 2 || longitude_bounds->dimension_type[0] != harp_dimension_time)
    {
        harp_set_error(HARP_ERROR_OPERATION, "variable 'longitude_bounds' has invalid dimensions for filtering");
        harp_variable_delete(latitude_bounds);
        harp_variable_delete(longitude_bounds);
        return -1;
    }

    if (latitude_bounds->dimension[1] != longitude_bounds->dimension[1])
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "the length of the independent dimension of variable "
                       "'latitude_bounds' (%ld) does not match the length of the independent dimension of variable "
                       "'longitude_bounds' (%ld)", latitude_bounds->dimension[1], longitude_bounds->dimension[1]);
        harp_variable_delete(latitude_bounds);
        harp_variable_delete(longitude_bounds);
        return -1;
    }
    if (latitude_bounds->dimension[1] < 3)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "the length of the independent dimension of variables "
                       "'latitude_bounds' and 'longitude_bounds' should be 3 or more");
        harp_variable_delete(latitude_bounds);
        harp_variable_delete(longitude_bounds);
        return -1;
    }

    num_areas = latitude_bounds->dimension[0];
    num_points = latitude_bounds->dimension[1];

    /* if the next operations are also polygon filters then include them */
    while (program->current_index + num_operations < program->num_operations)
    {
        if (!harp_operation_is_polygon_filter(program->operation[program->current_index + num_operations]))
        {
            break;
        }
        num_operations++;
    }

    if (info->dimension_mask_set[harp_dimension_time] == NULL)
    {
        long dimension = num_areas;

        if (harp_dimension_mask_new(1, &dimension, &info->dimension_mask_set[harp_dimension_time]) != 0)
        {
            harp_variable_delete(latitude_bounds);
            harp_variable_delete(longitude_bounds);
            return -1;
        }
    }

    mask = info->dimension_mask_set[harp_dimension_time]->mask;

    for (i = 0; i < num_areas; i++)
    {
        if (mask[i])
        {
            harp_spherical_polygon *area;

            if (harp_spherical_polygon_from_latitude_longitude_bounds(0, num_points,
                                                                      &latitude_bounds->data.double_data[i *
                                                                                                         num_points],
                                                                      &longitude_bounds->data.double_data[i *
                                                                                                          num_points],
                                                                      &area) != 0)
            {
                harp_variable_delete(latitude_bounds);
                harp_variable_delete(longitude_bounds);
                return -1;
            }
            else
            {
                for (k = 0; k < num_operations; k++)
                {
                    if (mask[i])
                    {
                        harp_operation_polygon_filter *operation;
                        int result;

                        operation = (harp_operation_polygon_filter *)program->operation[program->current_index + k];
                        result = operation->eval(operation, area);
                        if (result < 0)
                        {
                            harp_variable_delete(latitude_bounds);
                            harp_variable_delete(longitude_bounds);
                            harp_spherical_polygon_delete(area);
                            return -1;
                        }
                        mask[i] = result;
                    }
                }
                if (!mask[i])
                {
                    info->dimension_mask_set[harp_dimension_time]->masked_dimension_length--;
                }
            }
            harp_spherical_polygon_delete(area);
        }
    }

    harp_variable_delete(latitude_bounds);
    harp_variable_delete(longitude_bounds);

    if (dimension_mask_set_has_empty_masks(info->dimension_mask_set))
    {
        info->product_mask = 0;
    }

    /* jump to the last operation in the list that we performed */
    program->current_index += num_operations - 1;

    return 0;
}

static int execute_exclude_variable(ingest_info *info, harp_operation_exclude_variable *operation)
{
    int index;
    int j;

    /* unmark the variables to exclude */
    for (j = 0; j < operation->num_variables; j++)
    {
        index = harp_product_definition_get_variable_index(info->product_definition, operation->variable_name[j]);
        if (index < 0)
        {
            /* non-existent variable, not an error */
            continue;
        }

        info->variable_mask[index] = 0;
    }

    return 0;
}

static int execute_keep_variable(ingest_info *info, harp_operation_keep_variable *operation)
{
    uint8_t *included;
    int index;
    int j;

    included = (uint8_t *)calloc(info->product_definition->num_variable_definitions, sizeof(uint8_t));
    if (included == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       info->product_definition->num_variable_definitions * sizeof(uint8_t), __FILE__, __LINE__);
        return -1;
    }

    /* assume all variables are excluded */
    for (j = 0; j < info->product_definition->num_variable_definitions; j++)
    {
        included[j] = 0;
    }

    /* set the 'keep' flags in the mask */
    for (j = 0; j < operation->num_variables; j++)
    {
        index = harp_product_definition_get_variable_index(info->product_definition, operation->variable_name[j]);
        if (index < 0 || info->variable_mask[index] == 0)
        {
            harp_set_error(HARP_ERROR_OPERATION, "cannot keep non-existent variable %s", operation->variable_name[j]);
            free(included);
            return -1;
        }

        included[index] = 1;
    }

    /* filter the variables using the mask */
    for (j = info->product_definition->num_variable_definitions - 1; j >= 0; j--)
    {
        info->variable_mask[j] = info->variable_mask[j] && included[j];
    }

    free(included);

    return 0;
}

/* Perform performance optimized execution of filtering operations during ingestion.
 * This only performs the filters/includes/excludes that can be executed during the ingest.
 */
static int evaluate_ingestion_mask(ingest_info *info, harp_program *program)
{
    while (program->current_index < program->num_operations)
    {
        harp_operation *operation = program->operation[program->current_index];

        /* note that some consecutive filter operations can be executed together for optimization purposes */
        /* so the filter functions below may increase program->current_index itself */
        switch (operation->type)
        {
            case operation_bit_mask_filter:
            case operation_comparison_filter:
            case operation_longitude_range_filter:
            case operation_membership_filter:
            case operation_string_comparison_filter:
            case operation_string_membership_filter:
            case operation_valid_range_filter:
                if (execute_value_filter(info, program) != 0)
                {
                    return -1;
                }
                break;
            case operation_point_distance_filter:
            case operation_point_in_area_filter:
                if (execute_point_filter(info, program) != 0)
                {
                    return -1;
                }
                break;
            case operation_area_covers_area_filter:
            case operation_area_covers_point_filter:
            case operation_area_inside_area_filter:
            case operation_area_intersects_area_filter:
                if (execute_polygon_filter(info, program) != 0)
                {
                    return -1;
                }
                break;
            case operation_collocation_filter:
                /* read the collocation mask that will be used as a filter on the index variable */
                if (harp_operation_prepare_collocation_filter(operation, info->product->source_product) != 0)
                {
                    return -1;
                }
                /* perform a prefilter by filtering the index variable */
                if (execute_value_filter(info, program) != 0)
                {
                    return -1;
                }
                /* we only performed the prefilter phase, so don't increase the current_index and just stop here */
                /* the remaining program will be executed on the in-memory product */
                return 0;
            case operation_exclude_variable:
                if (execute_exclude_variable(info, (harp_operation_exclude_variable *)operation) != 0)
                {
                    return -1;
                }
                break;
            case operation_keep_variable:
                if (execute_keep_variable(info, (harp_operation_keep_variable *)operation) != 0)
                {
                    return -1;
                }
                break;
            case operation_bin_collocated:
            case operation_bin_full:
            case operation_bin_with_variable:
            case operation_derive_variable:
            case operation_derive_smoothed_column_collocated_dataset:
            case operation_derive_smoothed_column_collocated_product:
            case operation_flatten:
            case operation_regrid:
            case operation_regrid_collocated_dataset:
            case operation_regrid_collocated_product:
            case operation_rename:
            case operation_set:
            case operation_smooth_collocated_dataset:
            case operation_smooth_collocated_product:
            case operation_sort:
            case operation_wrap:
                /* these operations can only be performed on in-memory data */
                return 0;
        }

        program->current_index++;

        if (info->product_mask == 0)
        {
            return 0;
        }
    }

    return 0;
}

/* Ingest a product while taking into account filter operations at the head of program.
 */
static int get_product(ingest_info *info, harp_program *program)
{
    int i;

    if (harp_product_new(&info->product) != 0)
    {
        return -1;
    }

    info->product->source_product = strdup(info->basename);
    if (info->product->source_product == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        return -1;
    }

    if (init_product_dimensions(info) != 0)
    {
        return -1;
    }
    if (product_has_empty_dimensions(info))
    {
        /* empty product is not considered an error */
        info->product_mask = 0;
        return 0;
    }

    if (init_variable_mask(info) != 0)
    {
        return -1;
    }
    if (!product_has_variables(info))
    {
        /* empty product is not considered an error */
        info->product_mask = 0;
        return 0;
    }

    if (evaluate_ingestion_mask(info, program))
    {
        return -1;
    }

    if (info->product_mask == 0)
    {
        return 0;
    }

    /* read all variables, applying dimension masks on the fly */
    for (i = 0; i < info->product_definition->num_variable_definitions; i++)
    {
        harp_variable *variable;

        if (!info->variable_mask[i])
        {
            continue;
        }

        if (get_variable(info, info->product_definition->variable_definition[i], info->dimension_mask_set,
                         &variable) != 0)
        {
            return -1;
        }

        if (harp_product_add_variable(info->product, variable) != 0)
        {
            harp_variable_delete(variable);
            return -1;
        }
    }

    /* verify ingested product */
    if (harp_product_verify(info->product) != 0)
    {
        return -1;
    }

    /* perform remaining operations */
    if (harp_product_execute_program(info->product, program) != 0)
    {
        return -1;
    }

    return 0;
}

static int ingest(const char *filename, harp_program *program, const harp_ingestion_options *option_list,
                  harp_product **product)
{
    ingest_info *info;

    if (ingestion_init(&info) != 0)
    {
        return -1;
    }
    if (harp_ingestion_find_module(filename, &info->module, &info->cproduct) != 0)
    {
        ingestion_done(info);
        return -1;
    }
    if (harp_ingestion_module_validate_options(info->module, option_list) != 0)
    {
        ingestion_done(info);
        return -1;
    }
    if (info->cproduct != NULL && info->module->ingestion_init_coda != NULL)
    {
        if (info->module->ingestion_init_coda(info->module, info->cproduct, option_list, &info->product_definition,
                                              &info->user_data) != 0)
        {
            ingestion_done(info);
            return -1;
        }
    }
    else
    {
        assert(info->module->ingestion_init_custom != NULL);
        if (info->module->ingestion_init_custom(info->module, filename, option_list, &info->product_definition,
                                                &info->user_data) != 0)
        {
            ingestion_done(info);
            return -1;
        }
    }
    assert(info->product_definition != NULL);

    info->basename = harp_basename(filename);

    /* ingest the product */
    if (get_product(info, program) != 0)
    {
        ingestion_done(info);
        return -1;
    }

    *product = info->product;
    info->product = NULL;

    ingestion_done(info);

    return 0;
}

int harp_ingest(const char *filename, const char *operations, const char *options, harp_product **product)
{
    harp_program *program;
    harp_ingestion_options *option_list;
    int perform_conversions;
    int perform_boundary_checks;
    int status;

    if (filename == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "filename is NULL (%s:%u)", __FILE__, __LINE__);
        return -1;
    }

    if (product == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "product is NULL (%s:%u)", __FILE__, __LINE__);
        return -1;
    }

    if (harp_ingestion_init() != 0)
    {
        return -1;
    }

    if (operations == NULL)
    {
        if (harp_program_new(&program) != 0)
        {
            return -1;
        }
    }
    else
    {
        if (harp_program_from_string(operations, &program) != 0)
        {
            return -1;
        }
    }

    if (options == NULL)
    {
        if (harp_ingestion_options_new(&option_list) != 0)
        {
            harp_program_delete(program);
            return -1;
        }
    }
    else
    {
        if (harp_ingestion_options_from_string(options, &option_list) != 0)
        {
            harp_program_delete(program);
            return -1;
        }
    }

    /* all ingestion routines that use CODA are build on the assumption that 'perform conversions' is enabled, so we
     * explicitly enable it here just in case it was disabled somewhere else */
    perform_conversions = coda_get_option_perform_conversions();
    coda_set_option_perform_conversions(1);

    /* we also disable the boundary checks of libcoda for increased ingestion performance */
    perform_boundary_checks = coda_get_option_perform_boundary_checks();
    coda_set_option_perform_boundary_checks(0);

    status = ingest(filename, program, option_list, product);

    /* set the libcoda options back to their original values */
    coda_set_option_perform_boundary_checks(perform_boundary_checks);
    coda_set_option_perform_conversions(perform_conversions);

    harp_ingestion_options_delete(option_list);
    harp_program_delete(program);
    return status;
}

static int ingest_metadata(const char *filename, const harp_ingestion_options *option_list, double *datetime_start,
                           double *datetime_stop, long dimension[])
{
    ingest_info *info;
    int i;

    if (ingestion_init(&info) != 0)
    {
        return -1;
    }
    if (harp_ingestion_find_module(filename, &info->module, &info->cproduct) != 0)
    {
        ingestion_done(info);
        return -1;
    }
    if (harp_ingestion_module_validate_options(info->module, option_list) != 0)
    {
        ingestion_done(info);
        return -1;
    }
    if (info->cproduct != NULL && info->module->ingestion_init_coda != NULL)
    {
        if (info->module->ingestion_init_coda(info->module, info->cproduct, option_list, &info->product_definition,
                                              &info->user_data) != 0)
        {
            ingestion_done(info);
            return -1;
        }
    }
    else
    {
        assert(info->module->ingestion_init_custom != NULL);
        if (info->module->ingestion_init_custom(info->module, filename, option_list, &info->product_definition,
                                                &info->user_data) != 0)
        {
            ingestion_done(info);
            return -1;
        }
    }
    assert(info->product_definition != NULL);

    info->basename = harp_basename(filename);

    if (harp_product_new(&info->product) != 0)
    {
        ingestion_done(info);
        return -1;
    }

    if (init_product_dimensions(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }
    if (dimension != NULL)
    {
        for (i = 0; i < HARP_NUM_DIM_TYPES; i++)
        {
            dimension[i] = info->dimension[i];
        }
    }

    if (product_has_empty_dimensions(info))
    {
        /* empty product is not considered an error */
        *datetime_start = harp_mininf();
        *datetime_stop = harp_plusinf();
        ingestion_done(info);
        return 0;
    }

    /* read all variables whose name starts with 'datetime' */
    for (i = 0; i < info->product_definition->num_variable_definitions; i++)
    {
        harp_variable_definition *variable_def;
        harp_variable *variable;

        variable_def = info->product_definition->variable_definition[i];
        if (strncmp(variable_def->name, "datetime", 8) != 0)
        {
            continue;
        }

        if (get_variable(info, variable_def, info->dimension_mask_set, &variable) != 0)
        {
            ingestion_done(info);
            return -1;
        }

        if (harp_product_add_variable(info->product, variable) != 0)
        {
            harp_variable_delete(variable);
            ingestion_done(info);
            return -1;
        }
    }

    if (harp_product_get_datetime_range(info->product, datetime_start, datetime_stop) != 0)
    {
        ingestion_done(info);
        return -1;
    }

    ingestion_done(info);

    return 0;
}

int harp_ingest_global_attributes(const char *filename, const char *options, double *datetime_start,
                                  double *datetime_stop, long dimension[], char **source_product)
{
    harp_ingestion_options *option_list;
    int perform_conversions;
    int perform_boundary_checks;
    int status;

    if (filename == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "filename is NULL (%s:%u)", __FILE__, __LINE__);
        return -1;
    }

    if (harp_ingestion_init() != 0)
    {
        return -1;
    }

    if (options == NULL)
    {
        if (harp_ingestion_options_new(&option_list) != 0)
        {
            return -1;
        }
    }
    else
    {
        if (harp_ingestion_options_from_string(options, &option_list) != 0)
        {
            return -1;
        }
    }

    /* all ingestion routines that use CODA are build on the assumption that 'perform conversions' is enabled, so we
     * explicitly enable it here just in case it was disabled somewhere else */
    perform_conversions = coda_get_option_perform_conversions();
    coda_set_option_perform_conversions(1);

    /* we also disable the boundary checks of libcoda for increased ingestion performance */
    perform_boundary_checks = coda_get_option_perform_boundary_checks();
    coda_set_option_perform_boundary_checks(0);

    status = ingest_metadata(filename, option_list, datetime_start, datetime_stop, dimension);

    /* set the libcoda options back to their original values */
    coda_set_option_perform_boundary_checks(perform_boundary_checks);
    coda_set_option_perform_conversions(perform_conversions);

    harp_ingestion_options_delete(option_list);

    if (status != 0)
    {
        return -1;
    }

    if (source_product != NULL)
    {
        /* the source_product always equals the filename for ingestions */
        *source_product = strdup(harp_basename(filename));
        if (*source_product == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                           __LINE__);
            return -1;
        }
    }

    return 0;
}

int harp_ingest_test(const char *filename, int (*print) (const char *, ...))
{
    coda_product *product = NULL;
    ingest_info *info;
    harp_program *program;
    harp_ingestion_options *option_list;
    harp_ingestion_module *module;
    int perform_conversions;
    int perform_boundary_checks;
    int num_options = 0;
    int *option_choice = NULL;
    int status;
    int depth, i;

    if (filename == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "filename is NULL (%s:%u)", __FILE__, __LINE__);
        return -1;
    }

    if (harp_ingestion_init() != 0)
    {
        return -1;
    }

    if (harp_program_new(&program) != 0)
    {
        return -1;
    }

    if (harp_ingestion_options_new(&option_list) != 0)
    {
        harp_program_delete(program);
        return -1;
    }

    /* all ingestion routines that use CODA are build on the assumption that 'perform conversions' is enabled, so we
     * explicitly enable it here just in case it was disabled somewhere else */
    perform_conversions = coda_get_option_perform_conversions();
    coda_set_option_perform_conversions(1);

    /* we also disable the boundary checks of libcoda for increased ingestion performance */
    perform_boundary_checks = coda_get_option_perform_boundary_checks();
    coda_set_option_perform_boundary_checks(0);

    status = harp_ingestion_find_module(filename, &module, &product);
    if (status == 0)
    {
        num_options = module->num_option_definitions;
        option_choice = malloc(num_options * sizeof(int));
        if (option_choice == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           num_options * sizeof(int), __FILE__, __LINE__);
            status = -1;
        }
        else
        {
            for (i = 0; i < num_options; i++)
            {
                option_choice[i] = -1;  /* -1 means that the option is not provided */
            }
        }
    }

    depth = num_options;
    while (status == 0 && depth >= 0)
    {
        if (depth == num_options)
        {
            status = ingestion_init(&info);
            if (status != 0)
            {
                /* no sense to try other options */
                break;
            }
            info->cproduct = product;
            info->basename = harp_basename(filename);
            info->module = module;

            print("ingestion:");
            for (i = 0; i < num_options; i++)
            {
                if (i > 0)
                {
                    print(",");
                }
                print(" %s ", module->option_definition[i]->name);
                if (option_choice[i] >= 0)
                {
                    print("= %s", module->option_definition[i]->allowed_value[option_choice[i]]);
                }
                else
                {
                    print("unset");
                }
            }
            fflush(stdout);

            if (info->cproduct != NULL && info->module->ingestion_init_coda != NULL)
            {
                status = info->module->ingestion_init_coda(info->module, info->cproduct, option_list,
                                                           &info->product_definition, &info->user_data);
            }
            else
            {
                assert(info->module->ingestion_init_custom != NULL);
                status = info->module->ingestion_init_custom(info->module, filename, option_list,
                                                             &info->product_definition, &info->user_data);
            }
            if (status == 0)
            {
                assert(info->product_definition != NULL);
                if (num_options > 0)
                {
                    print(" =>");
                }
                print(" %s", info->product_definition->name);
                fflush(stdout);

                status = get_product(info, program);
            }
            if (status == 0)
            {
                print(" [OK]\n");
            }
            else
            {
                print(" [FAIL]\n");
                print("ERROR: %s\n", harp_errno_to_string(harp_errno));
            }

            info->cproduct = NULL;
            ingestion_done(info);
            status = 0;

            depth--;
        }
        if (depth >= 0)
        {
            if (option_choice[depth] < module->option_definition[depth]->num_allowed_values - 1)
            {
                const char *value;

                option_choice[depth]++;
                value = module->option_definition[depth]->allowed_value[option_choice[depth]];
                harp_ingestion_options_set_option(option_list, module->option_definition[depth]->name, value);
                depth = num_options;
            }
            else
            {
                option_choice[depth] = -1;
                harp_ingestion_options_remove_option(option_list, module->option_definition[depth]->name);
                depth--;
            }
        }
    }

    if (option_choice != NULL)
    {
        free(option_choice);
    }
    if (product != NULL)
    {
        coda_close(product);
    }

    /* set the libcoda options back to their original values */
    coda_set_option_perform_boundary_checks(perform_boundary_checks);
    coda_set_option_perform_conversions(perform_conversions);

    harp_ingestion_options_delete(option_list);
    harp_program_delete(program);

    return status;
}
