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
#include "harp-constants.h"
#include "harp-dimension-mask.h"
#include "harp-filter.h"
#include "harp-filter-collocation.h"
#include "harp-geometry.h"
#include "harp-action.h"

#include <assert.h>

typedef struct read_buffer_struct
{
    harp_data_type data_type;
    long num_elements;
    size_t buffer_size;
    harp_array data;
} read_buffer;

typedef struct ingest_info_struct
{
    harp_ingestion_module *module;      /* Ingestion module to use. */
    harp_product_definition *product_definition;        /* Definition of the product to ingest. */
    coda_product *cproduct;     /* Reference to coda product handle (in case CODA is used for ingestion) */

    void *user_data;    /* Ingestion module specific information. */

    harp_action_list *action_list;      /* List of actions to apply. */

    long dimension[HARP_NUM_DIM_TYPES]; /* Length of each dimension (0 if not in use). */
    harp_dimension_mask_set *dimension_mask_set;        /* which indices along each dimension should be ingested. */
    uint8_t product_mask;
    uint8_t *variable_mask;     /* indicates for each variable whether it should be included in the product. */

    const char *basename;       /* Product basename. */
    harp_product *product;      /* Resulting HARP product. */

    read_buffer *sample_buffer; /* buffer used for storing results from 'read_all' and 'read_range' */
    int (*sample_buffer_read_all) (void *user_data, harp_array data);   /* 'read_all' that was used to fill buffer */
    /* 'read_range' that was used to fill buffer */
    int (*sample_buffer_read_range) (void *user_data, long index_offset, long index_lenght, harp_array data);
    long sample_buffer_block_size;      /* byte size of block for each sample */
    long sample_buffer_index_offset;    /* index of first sample in the buffer */
    long sample_buffer_num_samples;     /* total number of samples for the variable */
    long sample_buffer_num_blocks;      /* number of samples that can fit in the buffer */
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

        read_buffer_delete(info->sample_buffer);

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
    info->action_list = NULL;
    memset(info->dimension, 0, HARP_NUM_DIM_TYPES * sizeof(long));
    info->dimension_mask_set = NULL;
    info->product_mask = 1;
    info->variable_mask = NULL;
    info->basename = NULL;
    info->product = NULL;
    info->sample_buffer = NULL;
    info->sample_buffer_read_all = NULL;

    if (harp_dimension_mask_set_new(&info->dimension_mask_set) != 0)
    {
        ingestion_done(info);
        return -1;
    }

    *new_info = info;
    return 0;
}

static int read_sample(ingest_info *info, const harp_variable_definition *variable_def, long index, harp_array data)
{
    if (variable_def->read_sample != NULL)
    {
        return variable_def->read_sample(info->user_data, index, data);
    }
    if (variable_def->read_all != NULL)
    {
        if (variable_def->num_dimensions == 0 || variable_def->dimension_type[0] != harp_dimension_time)
        {
            /* there is only one sample, so read directly into the target buffer */
            return variable_def->read_all(info->user_data, data);
        }

        /* we need to use an internal buffer, filled using the read_all() callback */
        if (info->sample_buffer_read_all != variable_def->read_all)
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

            if (info->sample_buffer == NULL)
            {
                if (read_buffer_new(variable_def->data_type, num_elements, &info->sample_buffer) != 0)
                {
                    return -1;
                }
            }
            else
            {
                if (read_buffer_resize(info->sample_buffer, variable_def->data_type, num_elements) != 0)
                {
                    return -1;
                }
            }
            if (variable_def->read_all(info->user_data, info->sample_buffer->data) != 0)
            {
                return -1;
            }
            info->sample_buffer_read_all = variable_def->read_all;
            info->sample_buffer_read_range = NULL;
            info->sample_buffer_block_size =
                harp_get_size_for_type(variable_def->data_type) * num_elements / dimension[0];
        }

        memcpy(data.ptr, &info->sample_buffer->data.int8_data[index * info->sample_buffer_block_size],
               info->sample_buffer_block_size);
    }
    else
    {
        assert(variable_def->read_range != NULL);

        /* we need to use an internal buffer, filled using the read_range() callback */
        if (info->sample_buffer_read_range != variable_def->read_range)
        {
            long dimension[HARP_MAX_NUM_DIMS];
            long num_elements;
            long num_blocks;
            int i;

            /* read_range() should have only been set for variables that have a time dimension as first dimension */
            assert(variable_def->num_dimensions > 0 && variable_def->dimension_type[0] == harp_dimension_time);

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
            info->sample_buffer_num_samples = dimension[0];
            num_elements = harp_get_num_elements(variable_def->num_dimensions, dimension);

            num_blocks = variable_def->get_max_range(info->user_data);

            if (info->sample_buffer == NULL)
            {
                if (read_buffer_new(variable_def->data_type, num_blocks, &info->sample_buffer) != 0)
                {
                    return -1;
                }
            }
            else
            {
                if (read_buffer_resize(info->sample_buffer, variable_def->data_type, num_blocks) != 0)
                {
                    return -1;
                }
            }
            info->sample_buffer_read_range = variable_def->read_range;
            info->sample_buffer_read_all = NULL;
            info->sample_buffer_block_size =
                harp_get_size_for_type(variable_def->data_type) * num_elements / dimension[0];
            info->sample_buffer_index_offset = num_blocks;      /* set to invalid offset, so a read will be triggered */
            info->sample_buffer_num_blocks = num_blocks;
        }

        if (index < info->sample_buffer_index_offset ||
            index >= info->sample_buffer_index_offset + info->sample_buffer_num_blocks)
        {
            long block_index = index / info->sample_buffer_num_blocks;
            long num_samples;

            info->sample_buffer_index_offset = block_index * info->sample_buffer_num_blocks;
            num_samples = info->sample_buffer_num_blocks;
            if (num_samples > info->sample_buffer_num_samples - info->sample_buffer_index_offset)
            {
                num_samples = info->sample_buffer_num_samples - info->sample_buffer_index_offset;
            }
            if (variable_def->read_range(info->user_data, info->sample_buffer_index_offset, num_samples,
                                         info->sample_buffer->data) != 0)
            {
                return -1;
            }
        }

        index -= info->sample_buffer_index_offset;

        memcpy(data.ptr, &info->sample_buffer->data.int8_data[index * info->sample_buffer_block_size],
               info->sample_buffer_block_size);
    }

    return 0;
}

static long update_mask_1d(int num_predicates, harp_predicate **predicate, long num_elements, long stride,
                           const void *data, uint8_t *mask)
{
    uint8_t *mask_end;
    long num_masked = 0;

    for (mask_end = mask + num_elements; mask != mask_end; ++mask)
    {
        if (*mask)
        {
            int i;

            for (i = 0; i < num_predicates; ++i)
            {
                if (!predicate[i]->eval(predicate[i]->args, data))
                {
                    *mask = 0;
                    break;
                }
            }

            if (i == num_predicates)
            {
                ++num_masked;
            }
        }

        data = (void *)(((char *)data) + stride);
    }

    return num_masked;
}

static int predicate_update_mask_0d(ingest_info *info, int num_predicates, harp_predicate **predicate,
                                    const harp_variable_definition *variable_def, uint8_t *product_mask)
{
    read_buffer *buffer;

    if (num_predicates == 0)
    {
        return 0;
    }
    if (predicate == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "predicate is NULL (%s:%lu)", __FILE__, __LINE__);
        return -1;

    }
    if (variable_def == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variable_def is NULL (%s:%lu)", __FILE__, __LINE__);
        return -1;

    }
    if (product_mask == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "product_mask is NULL (%s:%lu)", __FILE__, __LINE__);
        return -1;

    }
    if (variable_def->num_dimensions != 0)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variable '%s' has %d dimensions; expected 0", variable_def->name,
                       variable_def->num_dimensions);
        return -1;
    }
    if (!*product_mask)
    {
        /* Product mask is false. */
        return 0;
    }

    if (read_buffer_new(variable_def->data_type, 1, &buffer) != 0)
    {
        return -1;
    }

    if (read_sample(info, variable_def, 0, buffer->data) != 0)
    {
        read_buffer_delete(buffer);
        return -1;
    }

    update_mask_1d(num_predicates, predicate, 1, harp_get_size_for_type(variable_def->data_type), buffer->data.ptr,
                   product_mask);

    read_buffer_delete(buffer);

    return 0;
}

static int predicate_update_mask_1d(ingest_info *info, int num_predicates, harp_predicate **predicate,
                                    const harp_variable_definition *variable_def, harp_dimension_mask *dimension_mask)
{
    read_buffer *buffer;
    harp_dimension_type dimension_type;
    long dimension;

    if (num_predicates == 0)
    {
        return 0;
    }
    if (predicate == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "predicate is NULL (%s:%lu)", __FILE__, __LINE__);
        return -1;

    }
    if (variable_def == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variable is NULL (%s:%lu)", __FILE__, __LINE__);
        return -1;

    }
    if (dimension_mask == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "dimension_mask is NULL (%s:%lu)", __FILE__, __LINE__);
        return -1;

    }

    if (variable_def->num_dimensions != 1)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variable '%s' has %d dimensions; expected 1", variable_def->name,
                       variable_def->num_dimensions);
        return -1;
    }
    dimension_type = variable_def->dimension_type[0];
    if (dimension_type == harp_dimension_independent)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variable '%s' has independent outer dimension",
                       variable_def->name);
        return -1;
    }

    dimension = info->dimension[dimension_type];
    if (dimension_mask->num_dimensions != 1)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "dimension mask has %d dimensions; expected 1",
                       dimension_mask->num_dimensions);
        return -1;
    }
    if (dimension_mask->num_elements != dimension)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "dimension mask has %ld elements, expected %ld",
                       dimension_mask->num_elements, dimension);
        return -1;
    }
    if (dimension_mask->masked_dimension_length == 0)
    {
        /* Dimension mask is false. */
        return 0;
    }
    assert(dimension_mask->mask != NULL);

    if (dimension_type == harp_dimension_time)
    {
        long num_masked;
        long i;

        if (read_buffer_new(variable_def->data_type, 1, &buffer) != 0)
        {
            return -1;
        }

        num_masked = 0;
        for (i = 0; i < dimension; i++)
        {
            if (dimension_mask->mask[i])
            {
                if (read_sample(info, variable_def, i, buffer->data) != 0)
                {
                    read_buffer_delete(buffer);
                    return -1;
                }

                num_masked += update_mask_1d(num_predicates, predicate, buffer->num_elements,
                                             harp_get_size_for_type(variable_def->data_type), buffer->data.ptr,
                                             &dimension_mask->mask[i]);
                read_buffer_free_string_data(buffer);
            }
        }

        dimension_mask->masked_dimension_length = num_masked;
        read_buffer_delete(buffer);
    }
    else
    {
        if (read_buffer_new(variable_def->data_type, dimension, &buffer) != 0)
        {
            return -1;
        }

        if (read_sample(info, variable_def, 0, buffer->data) != 0)
        {
            read_buffer_delete(buffer);
            return -1;
        }

        dimension_mask->masked_dimension_length = update_mask_1d(num_predicates, predicate, buffer->num_elements,
                                                                 harp_get_size_for_type(variable_def->data_type),
                                                                 buffer->data.ptr, dimension_mask->mask);
        read_buffer_delete(buffer);
    }

    return 0;
}

static int predicate_update_mask_2d(ingest_info *info, int num_predicates, harp_predicate **predicate,
                                    const harp_variable_definition *variable_def,
                                    harp_dimension_mask *primary_dimension_mask,
                                    harp_dimension_mask *secondary_dimension_mask)
{
    read_buffer *buffer;
    harp_dimension_type primary_dimension_type;
    harp_dimension_type secondary_dimension_type;
    long primary_dimension;
    long secondary_dimension;
    long primary_num_masked = 0;
    long secondary_max_masked = 0;
    long i;

    if (predicate == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "predicate is NULL (%s:%lu)", __FILE__, __LINE__);
        return -1;

    }
    if (variable_def == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variable_def is NULL (%s:%lu)", __FILE__, __LINE__);
        return -1;

    }
    if (primary_dimension_mask == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "primary_dimension_mask is NULL (%s:%lu)", __FILE__, __LINE__);
        return -1;

    }
    if (secondary_dimension_mask == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "secondary_dimension_mask is NULL (%s:%lu)", __FILE__, __LINE__);
        return -1;
    }

    if (variable_def->num_dimensions != 2)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variable '%s' has %d dimensions; expected 2", variable_def->name,
                       variable_def->num_dimensions);
        return -1;
    }
    primary_dimension_type = variable_def->dimension_type[0];
    if (primary_dimension_type != harp_dimension_time)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "outer dimension of variable '%s' is of type '%s'; expected '%s'",
                       variable_def->name, harp_get_dimension_type_name(primary_dimension_type),
                       harp_get_dimension_type_name(harp_dimension_time));
        return -1;
    }
    secondary_dimension_type = variable_def->dimension_type[1];
    if (secondary_dimension_type == harp_dimension_independent)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variable '%s' has independent inner dimension",
                       variable_def->name);
        return -1;
    }

    primary_dimension = info->dimension[primary_dimension_type];
    if (primary_dimension_mask->num_dimensions != 1)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "dimension mask has %d dimensions; expected 1",
                       primary_dimension_mask->num_dimensions);
        return -1;
    }
    if (primary_dimension_mask->num_elements != primary_dimension)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "dimension mask has %ld elements, expected %ld",
                       primary_dimension_mask->num_elements, primary_dimension);
        return -1;
    }
    if (primary_dimension_mask->masked_dimension_length == 0)
    {
        /* Primary dimension mask is false. */
        return 0;
    }
    assert(primary_dimension_mask->mask != NULL);

    secondary_dimension = info->dimension[secondary_dimension_type];
    if (secondary_dimension_mask->num_dimensions != 2)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "dimension mask has %d dimensions; expected 2",
                       secondary_dimension_mask->num_dimensions);
        return -1;
    }
    if (secondary_dimension_mask->num_elements != primary_dimension * secondary_dimension)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "dimension mask has %ld elements, expected %ld",
                       secondary_dimension_mask->num_elements, primary_dimension * secondary_dimension);
        return -1;
    }
    if (secondary_dimension_mask->masked_dimension_length == 0)
    {
        /* Secondary dimension mask is false. */
        return 0;
    }
    assert(secondary_dimension_mask->mask != NULL);

    if (read_buffer_new(variable_def->data_type, secondary_dimension, &buffer) != 0)
    {
        return -1;
    }

    for (i = 0; i < primary_dimension; i++)
    {
        if (primary_dimension_mask->mask[i])
        {
            long secondary_num_masked;

            if (read_sample(info, variable_def, i, buffer->data) != 0)
            {
                read_buffer_delete(buffer);
                return -1;
            }

            secondary_num_masked = update_mask_1d(num_predicates, predicate, buffer->num_elements,
                                                  harp_get_size_for_type(variable_def->data_type), buffer->data.ptr,
                                                  &secondary_dimension_mask->mask[i * secondary_dimension]);
            read_buffer_free_string_data(buffer);

            if (secondary_num_masked > secondary_max_masked)
            {
                secondary_max_masked = secondary_num_masked;
            }

            if (secondary_num_masked == 0)
            {
                primary_dimension_mask->mask[i] = 0;
            }
            else
            {
                ++primary_num_masked;
            }
        }
        else
        {
            memset(&secondary_dimension_mask->mask[i * secondary_dimension], 0, secondary_dimension * sizeof(uint8_t));
        }
    }

    read_buffer_delete(buffer);

    primary_dimension_mask->masked_dimension_length = primary_num_masked;
    secondary_dimension_mask->masked_dimension_length = secondary_max_masked;

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
        /* Special case for scalars. */
        if (harp_variable_new(variable_def->name, variable_def->data_type, 0, NULL, NULL, &variable) != 0)
        {
            return -1;
        }

        if (read_sample(info, variable_def, 0, variable->data) != 0)
        {
            harp_variable_delete(variable);
            return -1;
        }
    }
    else
    {
        const harp_dimension_mask *primary_dimension_mask;
        const harp_dimension_mask *secondary_dimension_mask[HARP_MAX_NUM_DIMS];
        long dimension[HARP_MAX_NUM_DIMS];
        long masked_dimension[HARP_MAX_NUM_DIMS];
        long primary_dimension;
        int has_primary_dimension;
        int has_secondary_masks;
        int has_2D_secondary_masks;
        int i;
        int j;

        /* Variable has one or more dimensions. */
        has_primary_dimension = (variable_def->dimension_type[0] == harp_dimension_time);
        primary_dimension = info->dimension[harp_dimension_time];
        primary_dimension_mask = (dimension_mask_set == NULL ? NULL : dimension_mask_set[harp_dimension_time]);

        /* Determine the dimensions of the variable, both with and without taking into account the applicable dimension
         * masks.
         */
        for (i = 0; i < variable_def->num_dimensions; i++)
        {
            harp_dimension_type dimension_type;

            dimension_type = variable_def->dimension_type[i];
            if (dimension_type == harp_dimension_independent)
            {
                dimension[i] = variable_def->dimension[i];
                masked_dimension[i] = variable_def->dimension[i];
            }
            else
            {
                dimension[i] = info->dimension[dimension_type];

                if (dimension_mask_set == NULL || dimension_mask_set[dimension_type] == NULL)
                {
                    masked_dimension[i] = info->dimension[dimension_type];
                }
                else
                {
                    masked_dimension[i] = dimension_mask_set[dimension_type]->masked_dimension_length;
                }
            }
        }

        /* Gather information about secondary dimension masks. The secondary_dimension_mask array holds pointers to
         * dimension masks for secondary dimensions only. Therefore, depending on whether or not the variable depends on
         * the primary (time) dimension, the loop below starts at index 0 (variable does not depend on the primary
         * dimension), or 1 (variable does depend on the primary dimension).
         */
        has_secondary_masks = 0;
        has_2D_secondary_masks = 0;
        for (i = (has_primary_dimension ? 1 : 0), j = 0; i < variable_def->num_dimensions; i++, j++)
        {
            harp_dimension_type dimension_type;

            dimension_type = variable_def->dimension_type[i];
            if (dimension_type == harp_dimension_independent)
            {
                secondary_dimension_mask[j] = NULL;
            }
            else
            {
                secondary_dimension_mask[j] = (dimension_mask_set == NULL ? NULL : dimension_mask_set[dimension_type]);
                if (secondary_dimension_mask[j] != NULL)
                {
                    has_secondary_masks = 1;
                    if (secondary_dimension_mask[j]->num_dimensions == 2)
                    {
                        has_2D_secondary_masks = 1;
                    }
                }
            }
        }

        /* Create variable. */
        if (harp_variable_new(variable_def->name, variable_def->data_type, variable_def->num_dimensions,
                              variable_def->dimension_type, masked_dimension, &variable) != 0)
        {
            return -1;
        }

        /* To be able to apply 2-D secondary dimension masks to a variable that does not depend on the primary (time)
         * dimension, the variable is expanded by adding the primary dimension.
         */
        if (has_2D_secondary_masks && !has_primary_dimension)
        {
            long length;

            if (dimension_mask_set == NULL || dimension_mask_set[harp_dimension_time] == NULL)
            {
                length = info->dimension[harp_dimension_time];
            }
            else
            {
                length = dimension_mask_set[harp_dimension_time]->masked_dimension_length;
            }

            if (harp_variable_add_dimension(variable, 0, harp_dimension_time, length) != 0)
            {
                harp_variable_delete(variable);
                return -1;
            }

            /* Update dimension arrays to include the added dimension. */
            for (i = variable_def->num_dimensions; i >= 1; i--)
            {
                dimension[i] = dimension[i - 1];
                masked_dimension[i] = masked_dimension[i - 1];
            }
            dimension[0] = info->dimension[harp_dimension_time];
            masked_dimension[0] = length;

            /* Variable now depends on the primary dimension. */
            has_primary_dimension = 1;
        }

        if (!has_primary_dimension)
        {
            /* Variable does not depend on the primary dimension. */
            if (!has_secondary_masks)
            {
                /* No mask defined for any secondary dimension. */
                if (read_sample(info, variable_def, 0, variable->data) != 0)
                {
                    harp_variable_delete(variable);
                    return -1;
                }
            }
            else
            {
                /* At least one mask defined for a secondary dimension. */
                const uint8_t *mask[HARP_MAX_NUM_DIMS];
                read_buffer *buffer;
                long num_buffer_elements;

                for (i = 0; i < variable->num_dimensions; ++i)
                {
                    if (secondary_dimension_mask[i] == NULL)
                    {
                        mask[i] = NULL;
                    }
                    else
                    {
                        /* A variable that does not depend on the primary dimension is expanded by adding the primary
                         * dimension if any of the dimension mask defined for any of the secondary dimensions of the
                         * variable are 2-D. Since the expanded variable does depend on the primary dimension, it is
                         * not handled here and therefore all secondary dimension masks encountered here should be 1-D.
                         */
                        assert(secondary_dimension_mask[i]->num_dimensions == 1);
                        mask[i] = secondary_dimension_mask[i]->mask;
                    }
                }

                num_buffer_elements = harp_get_num_elements(variable->num_dimensions, dimension);
                if (read_buffer_new(variable->data_type, num_buffer_elements, &buffer) != 0)
                {
                    harp_variable_delete(variable);
                    return -1;
                }

                if (read_sample(info, variable_def, 0, buffer->data) != 0)
                {
                    read_buffer_delete(buffer);
                    harp_variable_delete(variable);
                    return -1;
                }

                harp_array_filter(variable->data_type, variable->num_dimensions, dimension, mask, buffer->data,
                                  masked_dimension, variable->data);

                read_buffer_delete(buffer);
            }
        }
        else
        {
            /* Variable depends on the primary dimension. */
            harp_array block;
            long block_stride;
            long k;

            block = variable->data;
            block_stride = harp_get_size_for_type(variable->data_type) * (variable->num_elements
                                                                          / variable->dimension[0]);

            if (!has_secondary_masks)
            {
                /* No mask defined for any secondary dimension. */
                if (primary_dimension_mask == NULL)
                {
                    /* No mask defined for the primary dimension. */
                    for (k = 0; k < primary_dimension; k++)
                    {
                        if (read_sample(info, variable_def, k, block) != 0)
                        {
                            harp_variable_delete(variable);
                            return -1;
                        }

                        block.ptr = (void *)(((char *)block.ptr) + block_stride);
                    }
                }
                else
                {
                    /* Mask defined for the primary dimension. */
                    const uint8_t *primary_mask = primary_dimension_mask->mask;

                    for (k = 0; k < primary_dimension; k++)
                    {
                        if (primary_mask[k])
                        {
                            if (read_sample(info, variable_def, k, block) != 0)
                            {
                                harp_variable_delete(variable);
                                return -1;
                            }

                            block.ptr = (void *)(((char *)block.ptr) + block_stride);
                        }
                    }
                }
            }
            else
            {
                /* At least one mask defined for a secondary dimension. */
                const uint8_t *mask[HARP_MAX_NUM_DIMS - 1];
                long mask_stride[HARP_MAX_NUM_DIMS - 1];
                read_buffer *buffer;
                long num_buffer_elements;

                for (i = 0; i < variable->num_dimensions - 1; i++)
                {
                    if (secondary_dimension_mask[i] == NULL)
                    {
                        mask[i] = NULL;
                    }
                    else
                    {
                        mask[i] = secondary_dimension_mask[i]->mask;
                        if (secondary_dimension_mask[i]->num_dimensions == 2)
                        {
                            mask_stride[i] = secondary_dimension_mask[i]->dimension[1];
                        }
                        else
                        {
                            assert(secondary_dimension_mask[i]->num_dimensions == 1);
                            mask_stride[i] = 0;
                        }
                    }
                }

                num_buffer_elements = harp_get_num_elements(variable->num_dimensions - 1, &dimension[1]);
                if (read_buffer_new(variable->data_type, num_buffer_elements, &buffer) != 0)
                {
                    harp_variable_delete(variable);
                    return -1;
                }

                if (primary_dimension_mask == NULL)
                {
                    /* No mask defined for the primary dimension. */
                    for (k = 0; k < primary_dimension; k++)
                    {
                        if (read_sample(info, variable_def, k, buffer->data) != 0)
                        {
                            read_buffer_delete(buffer);
                            harp_variable_delete(variable);
                            return -1;
                        }

                        harp_array_filter(variable->data_type, variable->num_dimensions - 1, &dimension[1], mask,
                                          buffer->data, &masked_dimension[1], block);
                        read_buffer_free_string_data(buffer);

                        block.ptr = (void *)(((char *)block.ptr) + block_stride);

                        for (i = 0; i < variable->num_dimensions - 1; i++)
                        {
                            if (mask[i] != NULL)
                            {
                                mask[i] += mask_stride[i];
                            }
                        }
                    }
                }
                else
                {
                    /* Mask defined for the primary dimension. */
                    const uint8_t *primary_mask = primary_dimension_mask->mask;

                    for (k = 0; k < primary_dimension; k++)
                    {
                        if (primary_mask[k])
                        {
                            if (read_sample(info, variable_def, k, buffer->data) != 0)
                            {
                                read_buffer_delete(buffer);
                                harp_variable_delete(variable);
                                return -1;
                            }

                            harp_array_filter(variable->data_type, variable->num_dimensions - 1, &dimension[1], mask,
                                              buffer->data, &masked_dimension[1], block);
                            read_buffer_free_string_data(buffer);

                            block.ptr = (void *)(((char *)block.ptr) + block_stride);
                        }

                        for (i = 0; i < variable->num_dimensions - 1; i++)
                        {
                            if (mask[i] != NULL)
                            {
                                mask[i] += mask_stride[i];
                            }
                        }
                    }
                }

                read_buffer_delete(buffer);
            }
        }
    }

    /* Copy variable attributes. */
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

    *new_variable = variable;
    return 0;
}

static int find_variable_definition(ingest_info *info, const char *name, int num_dimensions,
                                    const harp_dimension_type *dimension_type, harp_variable_definition **variable_def)
{
    int index;
    harp_variable_definition *candidate_def;

    index = harp_product_definition_get_variable_index(info->product_definition, name);
    if (index < 0)
    {
        return -1;
    }
    if (!info->variable_mask[index])
    {
        return -1;
    }

    candidate_def = info->product_definition->variable_definition[index];
    if (num_dimensions >= 0)
    {
        if (dimension_type == NULL)
        {
            if (candidate_def->num_dimensions != num_dimensions)
            {
                return -1;
            }
        }
        else
        {
            if (!harp_variable_definition_has_dimension_types(candidate_def, num_dimensions, dimension_type))
            {
                return -1;
            }
        }
    }

    if (variable_def != NULL)
    {
        *variable_def = candidate_def;
    }

    return 0;
}

static int evaluate_value_filters_0d(ingest_info *info)
{
    int i;

    i = 0;
    while (i < info->action_list->num_actions)
    {
        const harp_action *action;
        const char *variable_name;
        harp_variable_definition *variable_def;
        harp_predicate_set *predicate_set;
        int j;

        action = info->action_list->action[i];
        if (harp_action_get_variable_name(action, &variable_name) != 0)
        {
            /* Action is not a variable filter, skip it. */
            i++;
            continue;
        }

        if (find_variable_definition(info, variable_name, 0, NULL, &variable_def) != 0)
        {
            /* It may still be possible to apply the filter if the variable can be derived. Derived variables can only
             * be computed once the product has been ingested. Therefore, the filter is skipped here and another attempt
             * to apply it will be made after ingestion. */
            i++;
            continue;
        }

        /* Create filter predicates for all filters defined for this variable and collect them in a predicate set. The
         * variable is read, and filters are applied, per block (index on the outer dimension). The filter predicates
         * created here will be re-used for all blocks. Actions for which a predicate has been created are removed from
         * the list of actions to perform.
         */
        if (harp_predicate_set_new(&predicate_set) != 0)
        {
            return -1;
        }

        j = i;
        while (j < info->action_list->num_actions)
        {
            harp_predicate *predicate;

            action = info->action_list->action[j];
            if (harp_action_get_variable_name(action, &variable_name) != 0)
            {
                /* Action is not a variable filter, skip it. */
                j++;
                continue;
            }

            if (strcmp(variable_name, variable_def->name) != 0)
            {
                /* Filter applies to a different variable, skip it. */
                j++;
                continue;
            }

            /* Create filter predicate and add it to the predicate set. Remove the action from the list of actions
             * to perform.
             */
            if (harp_get_filter_predicate_for_action(action, variable_def->data_type, variable_def->unit,
                                                     variable_def->valid_min, variable_def->valid_max, &predicate) != 0)
            {
                harp_predicate_set_delete(predicate_set);
                return -1;
            }

            if (harp_predicate_set_add_predicate(predicate_set, predicate) != 0)
            {
                harp_predicate_delete(predicate);
                harp_predicate_set_delete(predicate_set);
                return -1;
            }

            if (harp_action_list_remove_action_at_index(info->action_list, j) != 0)
            {
                harp_predicate_set_delete(predicate_set);
                return -1;
            }
        }

        if (predicate_update_mask_0d(info, predicate_set->num_predicates, predicate_set->predicate, variable_def,
                                     &info->product_mask) != 0)
        {
            harp_predicate_set_delete(predicate_set);
            return -1;
        }
        else
        {
            harp_predicate_set_delete(predicate_set);
        }
    }

    return 0;
}

static int evaluate_value_filters_1d(ingest_info *info)
{
    int i;

    i = 0;
    while (i < info->action_list->num_actions)
    {
        const harp_action *action;
        const char *variable_name;
        harp_variable_definition *variable_def;
        harp_predicate_set *predicate_set;
        harp_dimension_type dimension_type;
        int j;

        action = info->action_list->action[i];
        if (harp_action_get_variable_name(action, &variable_name) != 0)
        {
            /* Action is not a variable filter, skip it. */
            i++;
            continue;
        }

        if (find_variable_definition(info, variable_name, 1, NULL, &variable_def) != 0)
        {
            /* It may still be possible to apply the filter if the variable can be derived. Derived variables can only
             * be computed once the product has been ingested. Therefore, the filter is skipped here and another attempt
             * to apply it will be made after ingestion. */
            i++;
            continue;
        }

        dimension_type = variable_def->dimension_type[0];
        if (dimension_type == harp_dimension_independent)
        {
            harp_set_error(HARP_ERROR_SCRIPT, "variable '%s' has independent outer dimension", variable_def->name);
            return -1;
        }

        /* Create filter predicates for all filters defined for this variable and collect them in a predicate set. The
         * variable is read, and filters are applied, per block (index on the outer dimension). The filter predicates
         * created here will be re-used for all blocks. Actions for which a predicate has been created are removed from
         * the list of actions to perform.
         */
        if (harp_predicate_set_new(&predicate_set) != 0)
        {
            return -1;
        }

        j = i;
        while (j < info->action_list->num_actions)
        {
            harp_predicate *predicate;

            action = info->action_list->action[j];
            if (harp_action_get_variable_name(action, &variable_name) != 0)
            {
                /* Action is not a variable filter, skip it. */
                j++;
                continue;
            }

            if (strcmp(variable_name, variable_def->name) != 0)
            {
                /* Filter applies to a different variable, skip it. */
                j++;
                continue;
            }

            /* Create filter predicate and add it to the predicate set. Remove the action from the list of actions
             * to perform.
             */
            if (harp_get_filter_predicate_for_action(action, variable_def->data_type, variable_def->unit,
                                                     variable_def->valid_min, variable_def->valid_max, &predicate) != 0)
            {
                harp_predicate_set_delete(predicate_set);
                return -1;
            }

            if (harp_predicate_set_add_predicate(predicate_set, predicate) != 0)
            {
                harp_predicate_delete(predicate);
                harp_predicate_set_delete(predicate_set);
                return -1;
            }

            if (harp_action_list_remove_action_at_index(info->action_list, j) != 0)
            {
                harp_predicate_set_delete(predicate_set);
                return -1;
            }
        }

        if (info->dimension_mask_set[dimension_type] == NULL)
        {
            long dimension = info->dimension[dimension_type];

            /* Create dimension mask if necessary. */
            if (harp_dimension_mask_new(1, &dimension, &info->dimension_mask_set[dimension_type]) != 0)
            {
                return -1;
            }
        }

        if (predicate_update_mask_1d(info, predicate_set->num_predicates, predicate_set->predicate, variable_def,
                                     info->dimension_mask_set[dimension_type]) != 0)
        {
            harp_predicate_set_delete(predicate_set);
            return -1;
        }
        else
        {
            harp_predicate_set_delete(predicate_set);
        }
    }

    return 0;
}

static int evaluate_value_filters_2d(ingest_info *info)
{
    int i;

    i = 0;
    while (i < info->action_list->num_actions)
    {
        const harp_action *action;
        const char *variable_name;
        harp_variable_definition *variable_def;
        harp_dimension_type dimension_type;
        harp_predicate_set *predicate_set;
        int j;

        /* If not a variable filter, skip. */
        action = info->action_list->action[i];
        if (harp_action_get_variable_name(action, &variable_name) != 0)
        {
            /* Action is not a variable filter, skip it. */
            i++;
            continue;
        }

        if (find_variable_definition(info, variable_name, 2, NULL, &variable_def) != 0)
        {
            /* It may still be possible to apply the filter if the variable can be derived. Derived variables can only
             * be computed once the product has been ingested. Therefore, the filter is skipped here and another attempt
             * to apply it will be made after ingestion. */
            i++;
            continue;
        }

        if (variable_def->dimension_type[0] != harp_dimension_time)
        {
            harp_set_error(HARP_ERROR_SCRIPT, "outer dimension of variable '%s' is of type '%s'; expected '%s'",
                           variable_def->name, harp_get_dimension_type_name(variable_def->dimension_type[0]),
                           harp_get_dimension_type_name(harp_dimension_time));
            return -1;
        }

        dimension_type = variable_def->dimension_type[1];
        if (dimension_type == harp_dimension_independent)
        {
            harp_set_error(HARP_ERROR_SCRIPT, "variable '%s' has independent inner dimension", variable_def->name);
            return -1;
        }

        /* Create filter predicates for all filters defined for this variable and collect them in a predicate set. The
         * variable is read, and filters are applied, per block (index on the outer dimension). The filter predicates
         * created here will be re-used for all blocks. Actions for which a predicate has been created are removed from
         * the list of actions to perform.
         */
        if (harp_predicate_set_new(&predicate_set) != 0)
        {
            return -1;
        }

        j = i;
        while (j < info->action_list->num_actions)
        {
            harp_predicate *predicate;

            action = info->action_list->action[j];
            if (harp_action_get_variable_name(action, &variable_name) != 0)
            {
                /* Action is not a variable filter, skip it. */
                j++;
                continue;
            }

            if (strcmp(variable_name, variable_def->name) != 0)
            {
                /* Filter applies to a different variable, skip it. */
                j++;
                continue;
            }

            /* Create filter predicate and add it to the predicate set. Remove the action from the list of actions
             * to perform.
             */
            if (harp_get_filter_predicate_for_action(action, variable_def->data_type, variable_def->unit,
                                                     variable_def->valid_min, variable_def->valid_max, &predicate) != 0)
            {
                harp_predicate_set_delete(predicate_set);
                return -1;
            }

            if (harp_predicate_set_add_predicate(predicate_set, predicate) != 0)
            {
                harp_predicate_delete(predicate);
                harp_predicate_set_delete(predicate_set);
                return -1;
            }

            if (harp_action_list_remove_action_at_index(info->action_list, j) != 0)
            {
                harp_predicate_set_delete(predicate_set);
                return -1;
            }
        }

        if (info->dimension_mask_set[harp_dimension_time] == NULL)
        {
            long dimension = info->dimension[harp_dimension_time];

            if (harp_dimension_mask_new(1, &dimension, &info->dimension_mask_set[harp_dimension_time]) != 0)
            {
                return -1;
            }
        }

        dimension_type = variable_def->dimension_type[1];
        if (info->dimension_mask_set[dimension_type] == NULL)
        {
            long dimension[2] = { info->dimension[harp_dimension_time], info->dimension[dimension_type] };

            if (harp_dimension_mask_new(2, dimension, &info->dimension_mask_set[dimension_type]) != 0)
            {
                return -1;
            }
        }
        else if (info->dimension_mask_set[dimension_type]->num_dimensions != 2)
        {
            /* Extend the existing 1-D mask to 2-D by repeating it along the outer dimension. */
            assert(info->dimension_mask_set[dimension_type]->num_dimensions == 1);
            if (harp_dimension_mask_prepend_dimension(info->dimension_mask_set[dimension_type],
                                                      info->dimension[harp_dimension_time]) != 0)
            {
                return -1;
            }
        }

        if (predicate_update_mask_2d(info, predicate_set->num_predicates, predicate_set->predicate, variable_def,
                                     info->dimension_mask_set[harp_dimension_time],
                                     info->dimension_mask_set[dimension_type]) != 0)
        {
            harp_predicate_set_delete(predicate_set);
            return -1;
        }
        else
        {
            harp_predicate_set_delete(predicate_set);
        }
    }

    return 0;
}

static int evaluate_collocation_filter(ingest_info *info)
{
    harp_variable_definition *variable_def;
    const harp_collocation_filter_args *args;
    harp_collocation_result *collocation_result;
    harp_predicate *predicate = NULL;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };
    int use_collocation_index;
    int i;

    for (i = 0; i < info->action_list->num_actions; i++)
    {
        if (info->action_list->action[i]->type == harp_action_filter_collocation)
        {
            break;
        }
    }

    if (i == info->action_list->num_actions)
    {
        /* No collocation filter action present in action list. */
        return 0;
    }

    /* Check for the presence of the 'collocation_index' or 'index' variable. Either variable should be 1-D and should
     * depend on the time dimension.
     */
    if (find_variable_definition(info, "collocation_index", 1, dimension_type, &variable_def) == 0)
    {
        use_collocation_index = 1;
    }
    else if (find_variable_definition(info, "index", 1, dimension_type, &variable_def) == 0)
    {
        use_collocation_index = 0;
    }
    else
    {
        /* Neither the "collocation_index" nor the "index" variable exists in the product, which means collocation
         * filters cannot be applied. Another attempt to apply these filters will be made after ingestion.
         */
        return 0;
    }

    if (variable_def->data_type != harp_type_int32)
    {
        harp_set_error(HARP_ERROR_SCRIPT, "variable '%s' has data type '%s'; expected '%s'", variable_def->name,
                       harp_get_data_type_name(variable_def->data_type), harp_get_data_type_name(harp_type_int32));
        return -1;
    }

    /* Create filter predicate. */
    args = (const harp_collocation_filter_args *)info->action_list->action[i]->args;
    if (harp_collocation_result_read(args->filename, &collocation_result) != 0)
    {
        return -1;
    }
    if (harp_collocation_filter_predicate_new(collocation_result, info->product->source_product, args->filter_type,
                                              use_collocation_index, &predicate) != 0)
    {
        harp_collocation_result_delete(collocation_result);
        return -1;
    }
    else
    {
        harp_collocation_result_delete(collocation_result);
    }

    if (info->dimension_mask_set[harp_dimension_time] == NULL)
    {
        long dimension = info->dimension[harp_dimension_time];

        /* Create dimension mask if necessary. */
        if (harp_dimension_mask_new(1, &dimension, &info->dimension_mask_set[harp_dimension_time]) != 0)
        {
            harp_predicate_delete(predicate);
            return -1;
        }
    }

    if (predicate_update_mask_1d(info, 1, &predicate, variable_def, info->dimension_mask_set[harp_dimension_time]) != 0)
    {
        harp_predicate_delete(predicate);
        return -1;
    }
    else
    {
        harp_predicate_delete(predicate);
    }

    return 0;
}

static int evaluate_point_filters_0d(ingest_info *info)
{
    harp_variable_definition *longitude_def;
    harp_variable_definition *latitude_def;
    harp_predicate_set *predicate_set;
    int i;

    /* It may still be possible to apply the filter if the variables can be derived. Derived variables can only be
     * computed once the product has been ingested. Therefore, the filter is skipped here and another attempt to apply
     * it will be made after ingestion.
     */
    if (find_variable_definition(info, "longitude", 0, NULL, &longitude_def) != 0)
    {
        return 0;
    }
    if (find_variable_definition(info, "latitude", 0, NULL, &latitude_def) != 0)
    {
        return 0;
    }

    /* Create filter predicates for all point filters and collect them in a predicate set. The filter predicates
     * created here will be re-used for all points. Actions for which a predicate has been created are removed from
     * the list of actions to perform.
     */
    if (harp_predicate_set_new(&predicate_set) != 0)
    {
        return -1;
    }

    i = 0;
    while (i < info->action_list->num_actions)
    {
        const harp_action *action;
        harp_predicate *predicate;

        /* Create filter predicate and add it to the predicate set. Remove the action from the list of actions to
         * perform.
         */
        action = info->action_list->action[i];
        switch (action->type)
        {
            case harp_action_filter_point_distance:
                {
                    const harp_point_distance_filter_args *args;

                    args = (const harp_point_distance_filter_args *)action->args;
                    if (harp_point_distance_filter_predicate_new(args, &predicate) != 0)
                    {
                        harp_predicate_set_delete(predicate_set);
                        return -1;
                    }
                }
                break;
            case harp_action_filter_area_mask_covers_point:
                {
                    const harp_area_mask_covers_point_filter_args *args;

                    args = (const harp_area_mask_covers_point_filter_args *)action->args;
                    if (harp_area_mask_covers_point_filter_predicate_new(args, &predicate) != 0)
                    {
                        harp_predicate_set_delete(predicate_set);
                        return -1;
                    }
                }
                break;
            default:
                /* Not a point filter, skip. */
                i++;
                continue;
        }

        if (harp_predicate_set_add_predicate(predicate_set, predicate) != 0)
        {
            harp_predicate_delete(predicate);
            harp_predicate_set_delete(predicate_set);
            return -1;
        }

        if (harp_action_list_remove_action_at_index(info->action_list, i) != 0)
        {
            harp_predicate_set_delete(predicate_set);
            return -1;
        }
    }

    /* Update dimension mask. */
    if (predicate_set->num_predicates > 0)
    {
        harp_variable *longitude;
        harp_variable *latitude;

        if (get_variable(info, longitude_def, NULL, &longitude) != 0)
        {
            harp_predicate_set_delete(predicate_set);
            return -1;
        }
        if (get_variable(info, latitude_def, NULL, &latitude) != 0)
        {
            harp_variable_delete(longitude);
            harp_predicate_set_delete(predicate_set);
            return -1;
        }

        if (harp_point_predicate_update_mask_all_0d(predicate_set->num_predicates, predicate_set->predicate,
                                                    longitude, latitude, &info->product_mask) != 0)
        {
            harp_variable_delete(latitude);
            harp_variable_delete(longitude);
            harp_predicate_set_delete(predicate_set);
            return -1;
        }
        else
        {
            harp_variable_delete(latitude);
            harp_variable_delete(longitude);
        }
    }

    harp_predicate_set_delete(predicate_set);

    return 0;
}

static int evaluate_point_filters_1d(ingest_info *info)
{
    harp_variable_definition *longitude_def;
    harp_variable_definition *latitude_def;
    harp_predicate_set *predicate_set;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };
    int i;

    /* It may still be possible to apply the filter if the variables can be derived. Derived variables can only be
     * computed once the product has been ingested. Therefore, the filter is skipped here and another attempt to apply
     * it will be made after ingestion.
     */
    if (find_variable_definition(info, "longitude", 1, dimension_type, &longitude_def) != 0)
    {
        return 0;
    }
    if (find_variable_definition(info, "latitude", 1, dimension_type, &latitude_def) != 0)
    {
        return 0;
    }

    /* Create filter predicates for all point filters and collect them in a predicate set. The filter predicates
     * created here will be re-used for all points. Actions for which a predicate has been created are removed from
     * the list of actions to perform.
     */
    if (harp_predicate_set_new(&predicate_set) != 0)
    {
        return -1;
    }

    i = 0;
    while (i < info->action_list->num_actions)
    {
        const harp_action *action;
        harp_predicate *predicate;

        /* Create filter predicate and add it to the predicate set. Remove the action from the list of actions to
         * perform.
         */
        action = info->action_list->action[i];
        switch (action->type)
        {
            case harp_action_filter_point_distance:
                {
                    const harp_point_distance_filter_args *args;

                    args = (const harp_point_distance_filter_args *)action->args;
                    if (harp_point_distance_filter_predicate_new(args, &predicate) != 0)
                    {
                        harp_predicate_set_delete(predicate_set);
                        return -1;
                    }
                }
                break;
            case harp_action_filter_area_mask_covers_point:
                {
                    const harp_area_mask_covers_point_filter_args *args;

                    args = (const harp_area_mask_covers_point_filter_args *)action->args;
                    if (harp_area_mask_covers_point_filter_predicate_new(args, &predicate) != 0)
                    {
                        harp_predicate_set_delete(predicate_set);
                        return -1;
                    }
                }
                break;
            default:
                /* Not a point filter, skip. */
                i++;
                continue;
        }

        if (harp_predicate_set_add_predicate(predicate_set, predicate) != 0)
        {
            harp_predicate_delete(predicate);
            harp_predicate_set_delete(predicate_set);
            return -1;
        }

        if (harp_action_list_remove_action_at_index(info->action_list, i) != 0)
        {
            harp_predicate_set_delete(predicate_set);
            return -1;
        }
    }

    /* Update dimension mask. */
    if (predicate_set->num_predicates > 0)
    {
        harp_variable *longitude;
        harp_variable *latitude;

        if (get_variable(info, longitude_def, NULL, &longitude) != 0)
        {
            harp_predicate_set_delete(predicate_set);
            return -1;
        }
        if (get_variable(info, latitude_def, NULL, &latitude) != 0)
        {
            harp_variable_delete(longitude);
            harp_predicate_set_delete(predicate_set);
            return -1;
        }

        if (info->dimension_mask_set[harp_dimension_time] == NULL)
        {
            long dimension = info->dimension[harp_dimension_time];

            if (harp_dimension_mask_new(1, &dimension, &info->dimension_mask_set[harp_dimension_time]) != 0)
            {
                harp_variable_delete(latitude);
                harp_variable_delete(longitude);
                harp_predicate_set_delete(predicate_set);
                return -1;
            }
        }

        if (harp_point_predicate_update_mask_all_1d(predicate_set->num_predicates, predicate_set->predicate, longitude,
                                                    latitude, info->dimension_mask_set[harp_dimension_time]) != 0)
        {
            harp_variable_delete(latitude);
            harp_variable_delete(longitude);
            harp_predicate_set_delete(predicate_set);
            return -1;
        }
        else
        {
            harp_variable_delete(latitude);
            harp_variable_delete(longitude);
        }
    }

    harp_predicate_set_delete(predicate_set);

    return 0;
}

static int evaluate_area_filters_0d(ingest_info *info)
{
    harp_variable_definition *longitude_bounds_def;
    harp_variable_definition *latitude_bounds_def;
    harp_dimension_type dimension_type[1] = { harp_dimension_independent };
    harp_predicate_set *predicate_set;
    int i;

    /* It may still be possible to apply the filter if the variables can be derived. Derived variables can only be
     * computed once the product has been ingested. Therefore, the filter is skipped here and another attempt to apply
     * it will be made after ingestion.
     */
    if (find_variable_definition(info, "longitude_bounds", 1, dimension_type, &longitude_bounds_def) != 0)
    {
        return 0;
    }
    if (find_variable_definition(info, "latitude_bounds", 1, dimension_type, &latitude_bounds_def) != 0)
    {
        return 0;
    }

    /* Create filter predicates for all area filters and collect them in a predicate set. The filter predicates created
     * here will be re-used for all points. Actions for which a predicate has been created are removed from the list of
     * actions to perform.
     */
    if (harp_predicate_set_new(&predicate_set) != 0)
    {
        return -1;
    }

    i = 0;
    while (i < info->action_list->num_actions)
    {
        const harp_action *action;
        harp_predicate *predicate;

        /* Create filter predicate and add it to the predicate set. Remove the action from the list of actions to
         * perform.
         */
        action = info->action_list->action[i];
        switch (action->type)
        {
            case harp_action_filter_area_mask_covers_area:
                {
                    const harp_area_mask_covers_area_filter_args *args;

                    args = (const harp_area_mask_covers_area_filter_args *)action->args;
                    if (harp_area_mask_covers_area_filter_predicate_new(args, &predicate) != 0)
                    {
                        harp_predicate_set_delete(predicate_set);
                        return -1;
                    }
                }
                break;
            case harp_action_filter_area_mask_intersects_area:
                {
                    const harp_area_mask_intersects_area_filter_args *args;

                    args = (const harp_area_mask_intersects_area_filter_args *)action->args;
                    if (harp_area_mask_intersects_area_filter_predicate_new(args, &predicate) != 0)
                    {
                        harp_predicate_set_delete(predicate_set);
                        return -1;
                    }
                }
                break;
            default:
                /* Not an area filter, skip. */
                i++;
                continue;
        }

        if (harp_predicate_set_add_predicate(predicate_set, predicate) != 0)
        {
            harp_predicate_delete(predicate);
            harp_predicate_set_delete(predicate_set);
            return -1;
        }

        if (harp_action_list_remove_action_at_index(info->action_list, i) != 0)
        {
            harp_predicate_set_delete(predicate_set);
            return -1;
        }
    }

    if (predicate_set->num_predicates > 0)
    {
        harp_variable *longitude_bounds;
        harp_variable *latitude_bounds;

        if (get_variable(info, longitude_bounds_def, NULL, &longitude_bounds) != 0)
        {
            harp_predicate_set_delete(predicate_set);
            return -1;
        }
        if (get_variable(info, latitude_bounds_def, NULL, &latitude_bounds) != 0)
        {
            harp_variable_delete(longitude_bounds);
            harp_predicate_set_delete(predicate_set);
            return -1;
        }

        if (harp_area_predicate_update_mask_all_0d(predicate_set->num_predicates, predicate_set->predicate,
                                                   longitude_bounds, latitude_bounds, &info->product_mask) != 0)
        {
            harp_variable_delete(latitude_bounds);
            harp_variable_delete(longitude_bounds);
            harp_predicate_set_delete(predicate_set);
            return -1;
        }
        else
        {
            harp_variable_delete(latitude_bounds);
            harp_variable_delete(longitude_bounds);
        }
    }

    harp_predicate_set_delete(predicate_set);

    return 0;
}

static int evaluate_area_filters_1d(ingest_info *info)
{
    harp_variable_definition *longitude_bounds_def;
    harp_variable_definition *latitude_bounds_def;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_independent };
    harp_predicate_set *predicate_set;
    int i;

    /* It may still be possible to apply the filter if the variables can be derived. Derived variables can only be
     * computed once the product has been ingested. Therefore, the filter is skipped here and another attempt to apply
     * it will be made after ingestion.
     */
    if (find_variable_definition(info, "longitude_bounds", 2, dimension_type, &longitude_bounds_def) != 0)
    {
        return 0;
    }
    if (find_variable_definition(info, "latitude_bounds", 2, dimension_type, &latitude_bounds_def) != 0)
    {
        return 0;
    }

    /* Create filter predicates for all area filters and collect them in a predicate set. The filter predicates created
     * here will be re-used for all points. Actions for which a predicate has been created are removed from the list of
     * actions to perform.
     */
    if (harp_predicate_set_new(&predicate_set) != 0)
    {
        return -1;
    }

    i = 0;
    while (i < info->action_list->num_actions)
    {
        const harp_action *action;
        harp_predicate *predicate;

        /* Create filter predicate and add it to the predicate set. Remove the action from the list of actions to
         * perform.
         */
        action = info->action_list->action[i];
        switch (action->type)
        {
            case harp_action_filter_area_mask_covers_area:
                {
                    const harp_area_mask_covers_area_filter_args *args;

                    args = (const harp_area_mask_covers_area_filter_args *)action->args;
                    if (harp_area_mask_covers_area_filter_predicate_new(args, &predicate) != 0)
                    {
                        harp_predicate_set_delete(predicate_set);
                        return -1;
                    }
                }
                break;
            case harp_action_filter_area_mask_intersects_area:
                {
                    const harp_area_mask_intersects_area_filter_args *args;

                    args = (const harp_area_mask_intersects_area_filter_args *)action->args;
                    if (harp_area_mask_intersects_area_filter_predicate_new(args, &predicate) != 0)
                    {
                        harp_predicate_set_delete(predicate_set);
                        return -1;
                    }
                }
                break;
            default:
                /* Not an area filter, skip. */
                i++;
                continue;
        }

        if (harp_predicate_set_add_predicate(predicate_set, predicate) != 0)
        {
            harp_predicate_delete(predicate);
            harp_predicate_set_delete(predicate_set);
            return -1;
        }

        if (harp_action_list_remove_action_at_index(info->action_list, i) != 0)
        {
            harp_predicate_set_delete(predicate_set);
            return -1;
        }
    }

    if (predicate_set->num_predicates > 0)
    {
        harp_variable *longitude_bounds;
        harp_variable *latitude_bounds;

        if (get_variable(info, longitude_bounds_def, NULL, &longitude_bounds) != 0)
        {
            harp_predicate_set_delete(predicate_set);
            return -1;
        }
        if (get_variable(info, latitude_bounds_def, NULL, &latitude_bounds) != 0)
        {
            harp_variable_delete(longitude_bounds);
            harp_predicate_set_delete(predicate_set);
            return -1;
        }

        if (info->dimension_mask_set[harp_dimension_time] == NULL)
        {
            long dimension = info->dimension[harp_dimension_time];

            if (harp_dimension_mask_new(1, &dimension, &info->dimension_mask_set[harp_dimension_time]) != 0)
            {
                harp_variable_delete(latitude_bounds);
                harp_variable_delete(longitude_bounds);
                harp_predicate_set_delete(predicate_set);
                return -1;
            }
        }

        if (harp_area_predicate_update_mask_all_1d(predicate_set->num_predicates, predicate_set->predicate,
                                                   longitude_bounds, latitude_bounds,
                                                   info->dimension_mask_set[harp_dimension_time]) != 0)
        {
            harp_variable_delete(latitude_bounds);
            harp_variable_delete(longitude_bounds);
            harp_predicate_set_delete(predicate_set);
            return -1;
        }
        else
        {
            harp_variable_delete(latitude_bounds);
            harp_variable_delete(longitude_bounds);
        }
    }

    harp_predicate_set_delete(predicate_set);

    return 0;
}

static int evaluate_variable_filters(ingest_info *info)
{
    uint8_t *variable_mask = NULL;
    int i;

    /* Process include actions. */
    i = 0;
    while (i < info->action_list->num_actions)
    {
        const harp_action *action;
        const harp_variable_inclusion_args *args;
        int j;

        action = info->action_list->action[i];
        if (action->type != harp_action_include_variable)
        {
            i++;
            continue;
        }

        if (variable_mask == NULL)
        {
            /* Allocate the variable mask and initialize to zero. */
            variable_mask = (uint8_t *)calloc(info->product_definition->num_variable_definitions, sizeof(uint8_t));
            if (variable_mask == NULL)
            {
                harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                               info->product_definition->num_variable_definitions * sizeof(uint8_t), __FILE__,
                               __LINE__);
                return -1;
            }
        }

        args = (const harp_variable_inclusion_args *)action->args;
        for (j = 0; j < args->num_variables; j++)
        {
            int index;

            index = harp_product_definition_get_variable_index(info->product_definition, args->variable_name[j]);
            if (index < 0)
            {
                harp_set_error(HARP_ERROR_SCRIPT, "cannot include variable '%s' (variable does not exist)",
                               args->variable_name[j]);
                free(variable_mask);
                return -1;
            }

            if (!info->variable_mask[index])
            {
                harp_set_error(HARP_ERROR_SCRIPT, "cannot include variable '%s' (variable unavailable)",
                               args->variable_name[j]);
                free(variable_mask);
                return -1;
            }

            variable_mask[index] = 1;
        }

        if (harp_action_list_remove_action_at_index(info->action_list, i) != 0)
        {
            free(variable_mask);
            return -1;
        }
    }

    /* If variables were explicitly marked for inclusion, exclude all variables that were not marked for inclusion. */
    if (variable_mask != NULL)
    {
        for (i = 0; i < info->product_definition->num_variable_definitions; i++)
        {
            if (!variable_mask[i])
            {
                info->variable_mask[i] = 0;
            }
        }

        free(variable_mask);
    }

    /* Process exclude actions. */
    i = 0;
    while (i < info->action_list->num_actions)
    {
        const harp_action *action;
        const harp_variable_exclusion_args *args;
        int j;

        action = info->action_list->action[i];
        if (action->type != harp_action_exclude_variable)
        {
            i++;
            continue;
        }

        args = (const harp_variable_exclusion_args *)action->args;
        for (j = 0; j < args->num_variables; j++)
        {
            int index;

            index = harp_product_definition_get_variable_index(info->product_definition, args->variable_name[j]);
            if (index < 0)
            {
                harp_set_error(HARP_ERROR_SCRIPT, "cannot exclude variable '%s' (variable does not exist)",
                               args->variable_name[j]);
                return -1;
            }

            info->variable_mask[index] = 0;
        }

        if (harp_action_list_remove_action_at_index(info->action_list, i) != 0)
        {
            return -1;
        }
    }

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

    /* Allocate the variable mask. */
    info->variable_mask = (uint8_t *)malloc(info->product_definition->num_variable_definitions * sizeof(uint8_t));
    if (info->variable_mask == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       info->product_definition->num_variable_definitions * sizeof(uint8_t), __FILE__, __LINE__);
        return -1;
    }

    /* Initialize variable mask according to the availability of each variable. */
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

static int get_product(ingest_info *info)
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
        /* Empty product is not considered an error. */
        return 0;
    }

    if (init_variable_mask(info) != 0)
    {
        return -1;
    }
    if (!product_has_variables(info))
    {
        /* Empty product is not considered an error. */
        return 0;
    }

    /* Perform all actions that can be performed as part of product ingestion. */
    if (harp_action_list_verify(info->action_list) != 0)
    {
        return -1;
    }

    /* Variable selection filters. */
    if (evaluate_variable_filters(info) != 0)
    {
        return -1;
    }
    if (!product_has_variables(info))
    {
        /* Empty product is not considered an error. */
        return 0;
    }

    /* First filter pass (0-D variables). */
    if (evaluate_value_filters_0d(info) != 0)
    {
        return -1;
    }
    if (evaluate_point_filters_0d(info) != 0)
    {
        return -1;
    }
    if (evaluate_area_filters_0d(info) != 0)
    {
        return -1;
    }
    if (info->product_mask == 0)
    {
        /* Empty product is not considered an error. */
        return 0;
    }

    /* Second filter pass (1-D variables). */
    if (evaluate_collocation_filter(info) != 0)
    {
        return -1;
    }
    if (evaluate_value_filters_1d(info) != 0)
    {
        return -1;
    }
    if (evaluate_point_filters_1d(info) != 0)
    {
        return -1;
    }
    if (evaluate_area_filters_1d(info) != 0)
    {
        return -1;
    }
    if (dimension_mask_set_has_empty_masks(info->dimension_mask_set))
    {
        /* Empty product is not considered an error. */
        return 0;
    }

    /* Third filter pass (2-D variables). */
    if (evaluate_value_filters_2d(info) != 0)
    {
        return -1;
    }
    /* If any 2-D masks are defined, mask each index on the primary dimension for which all mask values in the
     * corresponding row of (any of) the 2-D mask(s) are all equal to false. Remove dimension masks that are always
     * true.
     */
    if (harp_dimension_mask_set_simplify(info->dimension_mask_set) != 0)
    {
        return -1;
    }
    if (dimension_mask_set_has_empty_masks(info->dimension_mask_set))
    {
        /* Empty product is not considered an error. */
        return 0;
    }

    /* Read all variables, applying dimension masks on the fly. */
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

    /* Verify ingested product. */
    if (harp_product_verify(info->product) != 0)
    {
        return -1;
    }

    /* Apply remaining actions. */
    if (harp_product_execute_action_list(info->product, info->action_list) != 0)
    {
        return -1;
    }

    return 0;
}

static int ingest(const char *filename, harp_action_list *action_list, const harp_ingestion_options *option_list,
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
    info->action_list = action_list;

    /* Ingest the product. */
    if (get_product(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }

    *product = info->product;
    info->product = NULL;

    ingestion_done(info);

    return 0;
}

/** Ingest a product.
 * \param[in] filename Filename of the product to ingest.
 * \param[in] actions Script defining the actions to be performed as part of ingestion (for example, filtering).
 * \param[in] options Options to be passed to the ingestion module.
 * \param[out] product Pointer to a location where a pointer to the ingested product will be stored.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
int harp_ingest(const char *filename, const char *actions, const char *options, harp_product **product)
{
    harp_action_list *action_list;
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

    if (actions == NULL)
    {
        if (harp_action_list_new(&action_list) != 0)
        {
            return -1;
        }
    }
    else
    {
        if (harp_action_list_from_string(actions, &action_list) != 0)
        {
            return -1;
        }
    }

    if (options == NULL)
    {
        if (harp_ingestion_options_new(&option_list) != 0)
        {
            harp_action_list_delete(action_list);
            return -1;
        }
    }
    else
    {
        if (harp_ingestion_options_from_string(options, &option_list) != 0)
        {
            harp_action_list_delete(action_list);
            return -1;
        }
    }

    /* All ingestion routines that use CODA are build on the assumption that 'perform conversions' is enabled, so we
     * explicitly enable it here just in case it was disabled somewhere else.
     */
    perform_conversions = coda_get_option_perform_conversions();
    coda_set_option_perform_conversions(1);

    /* We also disable the boundary checks of libcoda for increased ingestion performance. */
    perform_boundary_checks = coda_get_option_perform_boundary_checks();
    coda_set_option_perform_boundary_checks(0);

    status = ingest(filename, action_list, option_list, product);

    /* Set the libcoda options back to their original values. */
    coda_set_option_perform_boundary_checks(perform_boundary_checks);
    coda_set_option_perform_conversions(perform_conversions);

    harp_ingestion_options_delete(option_list);
    harp_action_list_delete(action_list);
    return status;
}

/** Test ingestion of a product using all possible ingestion option values.
 * Results are printed to stdout.
 * \param[in] filename Filename of the product to ingest.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
int harp_ingest_test(const char *filename)
{
    coda_product *product = NULL;
    ingest_info *info;
    harp_action_list *action_list;
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

    if (harp_action_list_new(&action_list) != 0)
    {
        return -1;
    }

    if (harp_ingestion_options_new(&option_list) != 0)
    {
        harp_action_list_delete(action_list);
        return -1;
    }

    /* All ingestion routines that use CODA are build on the assumption that 'perform conversions' is enabled, so we
     * explicitly enable it here just in case it was disabled somewhere else.
     */
    perform_conversions = coda_get_option_perform_conversions();
    coda_set_option_perform_conversions(1);

    /* We also disable the boundary checks of libcoda for increased ingestion performance. */
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

    printf("product: %s\n", filename);

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
            info->action_list = action_list;

            printf("ingestion:");
            for (i = 0; i < num_options; i++)
            {
                if (i > 0)
                {
                    printf(",");
                }
                printf(" %s ", module->option_definition[i]->name);
                if (option_choice[i] >= 0)
                {
                    printf("= %s", module->option_definition[i]->allowed_value[option_choice[i]]);
                }
                else
                {
                    printf("unset");
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
                    printf(" =>");
                }
                printf(" %s", info->product_definition->name);
                fflush(stdout);

                status = get_product(info);
            }
            if (status == 0)
            {
                printf(" [OK]\n");
            }
            else
            {
                printf(" [FAIL]\n");
                printf("ERROR: %s\n", harp_errno_to_string(harp_errno));
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

    /* Set the libcoda options back to their original values. */
    coda_set_option_perform_boundary_checks(perform_boundary_checks);
    coda_set_option_perform_conversions(perform_conversions);

    harp_ingestion_options_delete(option_list);
    harp_action_list_delete(action_list);

    return status;
}
