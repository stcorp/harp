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

#include "harp-internal.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** \defgroup harp_variable HARP Variables
 * The HARP Variables module contains everything related to HARP variables.
 */

static void write_scalar(harp_scalar data, harp_data_type data_type, int (*print) (const char *, ...))
{
    switch (data_type)
    {
        case harp_type_int8:
            print("%d", (int)data.int8_data);
            break;
        case harp_type_int16:
            print("%d", (int)data.int16_data);
            break;
        case harp_type_int32:
            print("%ld", (long)data.int32_data);
            break;
        case harp_type_float:
            print("%.8g", (double)data.float_data);
            break;
        case harp_type_double:
            print("%.16g", (double)data.double_data);
            break;
        default:
            assert(0);
            exit(1);
    }
}

static void write_array(harp_array data, harp_data_type data_type, long num_elements, long block_size,
                        int (*print) (const char *, ...))
{
    long i, j, index;

    index = 0;
    for (i = 0; i < num_elements / block_size; i++)
    {
        if (block_size > 1)
        {
            print("\n  ");
        }
        for (j = 0; j < block_size; j++)
        {
            switch (data_type)
            {
                case harp_type_int8:
                    print("%d", (int)data.int8_data[index]);
                    break;
                case harp_type_int16:
                    print("%d", (int)data.int16_data[index]);
                    break;
                case harp_type_int32:
                    print("%ld", (long)data.int32_data[index]);
                    break;
                case harp_type_float:
                    print("%.8g", (double)data.float_data[index]);
                    break;
                case harp_type_double:
                    print("%.16g", (double)data.double_data[index]);
                    break;
                case harp_type_string:
                    print("\"%s\"", data.string_data[index]);
                    break;
            }
            if (index < num_elements - 1)
            {
                print(", ");
            }
            index++;
        }
    }
}

/* This function will construct a 'flag_values' attribute value based on the enumaration names.
 * The caller of this function is responsible of freeing the returned flag_values string. */
int harp_variable_get_flag_values_string(const harp_variable *variable, char **flag_values)
{
    int length;
    int pos;
    int i;

    if (variable->num_enum_values <= 0)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variable has no enumeration values (%s:%u)", __FILE__, __LINE__);
        return -1;
    }
    if (variable->data_type != harp_type_int8)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "flag_meanings attribute is only applicable for int8 data (%s:%u)",
                       __FILE__, __LINE__);
        return -1;
    }

    if (variable->num_enum_values > 100)
    {
        assert(variable->num_enum_values < 1000);       /* this should always be the case for int8 data */
        /* all numbers are three digits, except 0-99 (one digit less), except 0-9 (on additional digit less) */
        length = variable->num_enum_values * 3 - 100 - 10;
    }
    else if (variable->num_enum_values > 10)
    {
        /* all numbers are two digits, except 0-9 (on digit less) */
        length = variable->num_enum_values * 2 - 10;
    }
    else
    {
        /* all numbers are one digit */
        length = variable->num_enum_values;
    }
    /* add room for 'b' postfix, ', ' separators, and terminating zero */
    length += variable->num_enum_values + (variable->num_enum_values - 1) * 2 + 1;
    *flag_values = (char *)malloc(length * sizeof(char));
    if (*flag_values == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %u bytes) (%s:%u)",
                       length * sizeof(char), __FILE__, __LINE__);
        return -1;
    }
    pos = 0;
    for (i = 0; i < variable->num_enum_values; i++)
    {
        pos += sprintf(&(*flag_values)[pos], "%db", i);
        if (i < variable->num_enum_values - 1)
        {
            (*flag_values)[pos] = ',';
            pos++;
            (*flag_values)[pos] = ' ';
        }
        else
        {
            (*flag_values)[pos] = '\0';
        }
        pos++;
    }
    assert(pos == length);

    return 0;
}

/* This function will construct a 'flag_meanings' attribute value based on the enumaration names.
 * The caller of this function is responsible of freeing the returned flag_meanings string. */
int harp_variable_get_flag_meanings_string(const harp_variable *variable, char **flag_meanings)
{
    int length;
    int pos;
    int i;

    if (variable->num_enum_values <= 0)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variable has no enumeration values (%s:%u)", __FILE__, __LINE__);
        return -1;
    }
    if (variable->data_type != harp_type_int8)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "flag_meanings attribute is only applicable for int8 data (%s:%u)",
                       __FILE__, __LINE__);
        return -1;
    }

    length = 0;
    for (i = 0; i < variable->num_enum_values; i++)
    {
        if (variable->enum_name[i] == NULL || variable->enum_name[i][0] == '\0')
        {
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "name for enumeration value '%d' is not set (%s:%u)", i,
                           __FILE__, __LINE__);
            return -1;
        }
        length += strlen(variable->enum_name[i]);
    }
    /* add room for spaces + terminating zero */
    length += variable->num_enum_values;
    *flag_meanings = (char *)malloc(length * sizeof(char));
    if (*flag_meanings == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %u bytes) (%s:%u)",
                       length * sizeof(char), __FILE__, __LINE__);
        return -1;
    }
    pos = 0;
    for (i = 0; i < variable->num_enum_values; i++)
    {
        strcpy(&(*flag_meanings)[pos], variable->enum_name[i]);
        pos += strlen(variable->enum_name[i]);
        if (i < variable->num_enum_values - 1)
        {
            (*flag_meanings)[pos] = ' ';
        }
        else
        {
            (*flag_meanings)[pos] = '\0';
        }
        pos++;
    }
    assert(pos == length);

    return 0;
}

int harp_variable_set_enumeration_values_using_flag_meanings(harp_variable *variable, const char *flag_meanings)
{
    char *buffer;
    const char **enum_name;
    int num_enum_values;
    int i;
    char *p;

    buffer = strdup(flag_meanings);
    if (buffer == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        return -1;
    }

    num_enum_values = 0;
    p = buffer;
    while (*p != '\0')
    {
        while (*p == ' ')
        {
            p++;
        }
        if (*p != '\0')
        {
            /* start of a new label */
            num_enum_values++;
        }
        while (*p != ' ' && *p != '\0')
        {
            p++;
        }
    }

    enum_name = (const char **)malloc(num_enum_values * sizeof(char *));
    if (enum_name == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_enum_values * sizeof(char *), __FILE__, __LINE__);
        return -1;
    }

    p = buffer;
    i = 0;
    while (*p != '\0')
    {
        while (*p == ' ')
        {
            p++;
        }
        if (*p != '\0')
        {
            /* start of a new label */
            enum_name[i] = p;
            i++;
        }
        while (*p != ' ' && *p != '\0')
        {
            p++;
        }
        if (*p != '\0')
        {
            *p = '\0';
            p++;
        }
    }
    assert(i == num_enum_values);

    if (harp_variable_set_enumeration_values(variable, num_enum_values, enum_name) != 0)
    {
        free(enum_name);
        free(buffer);
        return -1;
    }

    free(enum_name);
    free(buffer);

    return 0;
}

/** Rearrange the data of a variable in one dimension.
 * This function allows data of a variable to be rearranged according to the order of the indices in dim_element_id.
 * The number of indices (num_dim_elements) in dim_element_id does not have to correspond to the number of
 * elements in the specified (dim_index) dimension. This means that the data block will grow/shrink when the amount of
 * elements in dim_element_id is larger/smaller (note that the amount of elements can only become larger if elements
 * are duplicated).
 *
 * \param variable Pointer to variable that should have its data rearranged.
 * \param dim_index The id of the dimension in which the rearrangement should take place.
 * \param num_dim_elements Number of elements in dim_element_id.
 * \param dim_element_ids An array containing the ids in dimension dim_index in the new arrangement (ids may occur more
 * than once and the number of ids may be smaller or larger than the length of dimension dim_index).
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
int harp_variable_rearrange_dimension(harp_variable *variable, int dim_index, long num_dim_elements,
                                      const long *dim_element_ids)
{
    char *buffer;
    long *move_to_id;
    int8_t *moved;
    long new_num_elements;
    long num_groups;
    long num_block_elements;
    long element_size;
    long filter_block_size;
    long i_increment;
    long i;
    int needs_shuffle;

    /* The multidimensional array is split in three parts:
     *   num_elements = num_groups * dim[dim_index] * num_block_elements
     *   new_num_elements = num_groups * num_dim_elements * num_block_elements */
    if (variable == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variable is NULL (%s:%u)", __FILE__, __LINE__);
        return -1;
    }
    if (num_dim_elements <= 0)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "num_dim_elements argument <= 0 (%s:%u)", __FILE__, __LINE__);
        return -1;
    }
    if (dim_element_ids == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "dim_element_ids argument is NULL (%s:%u)", __FILE__, __LINE__);
        return -1;
    }
    if (dim_index < 0 || dim_index >= variable->num_dimensions)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "dim_index argument (%d) is not in the range [0,%d) (%s:%u)",
                       dim_index, variable->num_dimensions, __FILE__, __LINE__);
        return -1;
    }
    if (variable->num_elements == 0)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "cannot reshape variable '%s' (variable has 0 elements) (%s:%u)",
                       variable->name, __FILE__, __LINE__);
        return -1;
    }

    needs_shuffle = num_dim_elements != variable->dimension[dim_index];
    for (i = 0; i < num_dim_elements; i++)
    {
        if (dim_element_ids[i] < 0 || dim_element_ids[i] >= variable->dimension[dim_index])
        {
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT,
                           "dim_element_ids[%d] argument (%d) is not in the range [0,%d) (%s:%u)",
                           i, dim_element_ids[i], variable->dimension[dim_index], __FILE__, __LINE__);
            return -1;
        }
        if (dim_element_ids[i] != i)
        {
            needs_shuffle = 1;
        }
    }
    if (!needs_shuffle)
    {
        /* all ellements are already in the right location, don't do anything */
        return 0;
    }

    /* Calculate the number of times we have to reshuffle the indices (i.e. the product of the higher dimensions). */
    num_groups = 1;
    for (i = 0; i < (long)dim_index; i++)
    {
        num_groups *= variable->dimension[i];
    }

    /* Calculate the number of elements per block - a block can be moved with one memcpy(). */
    num_block_elements = variable->num_elements / (num_groups * variable->dimension[dim_index]);

    /* Calculate the new total number of elements. */
    new_num_elements = num_groups * num_dim_elements * num_block_elements;

    /* Calculate the element size and block size */
    element_size = harp_get_size_for_type(variable->data_type);
    filter_block_size = num_block_elements * element_size;

    /* If num_dim_elements > dimension[dim_index] then increase the memory for variable->data. */
    if (num_dim_elements > variable->dimension[dim_index])
    {
        void *variable_data;

        variable_data = realloc(variable->data.ptr, (size_t)new_num_elements * element_size);
        if (variable_data == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %u bytes) (%s:%u)",
                           new_num_elements * element_size, __FILE__, __LINE__);
            return -1;
        }

        variable->data.ptr = variable_data;
    }

    /* Determine the positions where the old elements should end up.
     * In case an element will end up in more than one position we determine the first. */
    move_to_id = (long *)malloc((size_t)variable->dimension[dim_index] * sizeof(long));
    if (move_to_id == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       variable->dimension[dim_index] * sizeof(long), __FILE__, __LINE__);
        return -1;
    }
    for (i = 0; i < variable->dimension[dim_index]; i++)
    {
        move_to_id[i] = -1;     /* by default an element will be removed */
    }
    for (i = 0; i < num_dim_elements; i++)
    {
        if (move_to_id[dim_element_ids[i]] == -1)
        {
            move_to_id[dim_element_ids[i]] = i;
        }
    }

    /* Temporary storage room for a block */
    buffer = (char *)malloc((size_t)filter_block_size);
    if (buffer == NULL)
    {
        free(move_to_id);
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %u bytes) (%s:%u)",
                       filter_block_size, __FILE__, __LINE__);
        return -1;
    }

    /* Create temporary array that indicates whether a block has already been moved to its 'move_to_id' position */
    moved = (int8_t *)malloc((size_t)variable->dimension[dim_index] * sizeof(int8_t));
    if (moved == NULL)
    {
        free(move_to_id);
        free(buffer);
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       variable->dimension[dim_index] * sizeof(int8_t), __FILE__, __LINE__);
        return -1;
    }

    /* Iterate over all groups */
    /* we iterate backwards when the new data block (per group) is larger and iterate forwards otherwise */
    if (new_num_elements > variable->num_elements)
    {
        i_increment = -1;
        i = num_groups - 1;
    }
    else
    {
        i_increment = 1;
        i = 0;
    }

    while (i >= 0 && i < num_groups)
    {
        char *from_ptr;
        char *to_ptr;
        long to_id;

        from_ptr = (char *)variable->data.ptr + i * variable->dimension[dim_index] * filter_block_size;
        to_ptr = (char *)variable->data.ptr + i * num_dim_elements * filter_block_size;

        /* First move the old data to the new location */
        if (to_ptr != from_ptr)
        {
            memmove(to_ptr, from_ptr, (size_t)(variable->dimension[dim_index] * filter_block_size));
        }

        /* remove all strings for blocks that will be removed */
        if (variable->data_type == harp_type_string)
        {
            long j;

            for (j = 0; j < variable->dimension[dim_index]; j++)
            {
                if (move_to_id[j] == -1)
                {
                    char **string_data;
                    long k;

                    string_data = (char **)&to_ptr[j * filter_block_size];
                    for (k = 0; k < num_block_elements; k++)
                    {
                        if (string_data[k] != NULL)
                        {
                            free(string_data[k]);
                        }
                    }
                }
            }
        }

        /* Now do a reordering within the new space for the blocks */

        /* Reset the 'moved' array */
        memset(moved, 0, (size_t)variable->dimension[dim_index] * sizeof(int8_t));

        /* Assign data to each block */
        for (to_id = 0; to_id < num_dim_elements; to_id++)
        {
            long from_id = dim_element_ids[to_id];
            int is_cycle = 0;
            long k;

            if (move_to_id[from_id] == to_id)
            {
                /* We already filled this block because of an earlier shuffle */
                if (moved[from_id])
                {
                    continue;
                }
                /* The item is already in the right spot (and we don't have to duplicate strings) */
                if (to_id == from_id)
                {
                    moved[from_id] = 1;
                    continue;
                }
            }

            /* Check if the data at the current position needs to be moved away first */
            if (to_id < variable->dimension[dim_index] && move_to_id[to_id] != -1 && !moved[to_id])
            {
                /* We move the current data away recursively. However, we need to deal with possible cycles */
                k = move_to_id[to_id];
                while (k < variable->dimension[dim_index] && move_to_id[k] != -1 && !moved[k] && move_to_id[k] != to_id)
                {
                    k = move_to_id[k];
                }

                if (k < variable->dimension[dim_index] && move_to_id[k] == to_id)
                {
                    /* We have a cycle, so we use the temporary buffer */
                    assert(k == from_id);
                    memcpy(buffer, &to_ptr[from_id * filter_block_size], (size_t)filter_block_size);
                    is_cycle = 1;
                }

                while (k != to_id)
                {
                    memcpy(&to_ptr[k * filter_block_size], &to_ptr[dim_element_ids[k] * filter_block_size],
                           (size_t)filter_block_size);
                    k = dim_element_ids[k];
                    moved[k] = 1;
                }
            }

            /* Now copy the source data to the current position */
            if (is_cycle)
            {
                /* we need to use the data from the temporary buffer if there was a cylce */
                memcpy(&to_ptr[to_id * filter_block_size], buffer, (size_t)filter_block_size);
                moved[from_id] = 1;
            }
            else
            {
                if (moved[from_id])
                {
                    /* this is a duplicate of another block (that has already been moved) */
                    from_id = move_to_id[from_id];
                    memcpy(&to_ptr[to_id * filter_block_size], &to_ptr[from_id * filter_block_size],
                           (size_t)filter_block_size);

                    if (variable->data_type == harp_type_string)
                    {
                        char **string_data;

                        /* duplicate all strings in the block */
                        string_data = (char **)&to_ptr[to_id * filter_block_size];
                        for (k = 0; k < num_block_elements; k++)
                        {
                            if (string_data[k] != NULL)
                            {
                                string_data[k] = strdup(string_data[k]);
                                if (string_data[k] == NULL)
                                {
                                    free(moved);
                                    free(buffer);
                                    free(move_to_id);
                                    harp_set_error(HARP_ERROR_OUT_OF_MEMORY,
                                                   "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                                                   __LINE__);
                                    return -1;
                                }
                            }
                        }
                    }
                }
                else
                {
                    memcpy(&to_ptr[to_id * filter_block_size], &to_ptr[from_id * filter_block_size],
                           (size_t)filter_block_size);
                    moved[from_id] = 1;
                }
            }
        }

        i += i_increment;
    }

    free(moved);
    free(buffer);
    free(move_to_id);

    /* if num_dim_elements < dimension[dim_index] then make data block smaller */
    if (num_dim_elements < variable->dimension[dim_index])
    {
        void *variable_data;

        variable_data = realloc(variable->data.ptr, (size_t)new_num_elements * element_size);
        if (variable_data == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %u bytes) (%s:%u)",
                           new_num_elements * element_size, __FILE__, __LINE__);
            return -1;
        }
        variable->data.ptr = variable_data;
    }

    /* update variable properties */
    variable->num_elements = new_num_elements;
    variable->dimension[dim_index] = num_dim_elements;

    return 0;
}

/** Filter data of a variable in one dimension.
 * This function removes all elements in the given dimension where \a mask is set to 0.
 * The size of \a mask should correspond to the number of elements in the specified (dim_index) dimension.
 * The size of the given dimenion (and num_elements) will be reduced accordingly.
 * It is an error to provide a list of \a mask values that only contain zeros (i.e. filter out all elements).
 *
 * \param variable Pointer to variable that should have its data rearranged.
 * \param dim_index The id of the dimension in which the rearrangement should take place.
 * \param mask An array of length variable->dimension[dim_index] that contains true/false (1/0) values on whether
 *     to keep an element or not.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
int harp_variable_filter_dimension(harp_variable *variable, int dim_index, const uint8_t *mask)
{
    void *variable_data;
    long num_dim_elements;
    long new_num_elements;
    long num_groups;
    long num_block_elements;
    long element_size;
    long filter_block_size;
    long i;

    /* The multidimensional array is split in three parts:
     *   num_elements = num_groups * dim[dim_index] * num_block_elements
     *   new_num_elements = num_groups * num_dim_elements * num_block_elements */
    if (variable == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variable argument is NULL (%s:%u)", __FILE__, __LINE__);
        return -1;
    }
    if (mask == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "mask argument is NULL (%s:%u)", __FILE__, __LINE__);
        return -1;
    }
    if (dim_index < 0 || dim_index >= variable->num_dimensions)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "dim_index argument (%d) is not in the range [0,%d) (%s:%u)",
                       dim_index, variable->num_dimensions, __FILE__, __LINE__);
        return -1;
    }
    if (variable->num_elements == 0)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "cannot filter variable '%s' (variable has 0 elements) (%s:%u)",
                       variable->name, __FILE__, __LINE__);
        return -1;
    }

    num_dim_elements = 0;
    for (i = 0; i < variable->dimension[dim_index]; i++)
    {
        num_dim_elements += (mask[i] ? 1 : 0);
    }
    if (num_dim_elements == 0)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "cannot reshape variable '%s' (variable has 0 elements) (%s:%u)",
                       variable->name, __FILE__, __LINE__);
        return -1;
    }
    if (num_dim_elements == variable->dimension[dim_index])
    {
        /* everything is included -> no filtering needed */
        return 0;
    }

    /* Calculate the number of times we have to filter the indices (i.e. the product of the higher dimensions) */
    num_groups = 1;
    for (i = 0; i < (long)dim_index; i++)
    {
        num_groups *= variable->dimension[i];
    }

    /* Calculate the number of elements per block - a block can be moved with one memcpy */
    num_block_elements = variable->num_elements / (num_groups * variable->dimension[dim_index]);

    /* Calculate the new total number of elements */
    new_num_elements = num_groups * num_dim_elements * num_block_elements;

    /* Calculate the element size and block size */
    element_size = harp_get_size_for_type(variable->data_type);
    filter_block_size = num_block_elements * element_size;

    for (i = 0; i < num_groups; i++)
    {
        char *from_block_ptr = (char *)variable->data.ptr + i * variable->dimension[dim_index] * filter_block_size;
        char *to_block_ptr = (char *)variable->data.ptr + i * num_dim_elements * filter_block_size;
        long from_id;
        long to_id = 0;

        for (from_id = 0; from_id < variable->dimension[dim_index]; from_id++)
        {
            char *from_ptr = &from_block_ptr[from_id * filter_block_size];
            char *to_ptr = &to_block_ptr[to_id * filter_block_size];

            if (mask[from_id])
            {
                if (to_ptr != from_ptr)
                {
                    memmove(to_ptr, from_ptr, (size_t)(filter_block_size));
                }
                to_id++;
            }
            else
            {
                /* remove all strings for the items that get discarded */
                if (variable->data_type == harp_type_string)
                {
                    char **string_data = (char **)from_ptr;
                    long k;

                    for (k = 0; k < num_block_elements; k++)
                    {
                        if (string_data[k] != NULL)
                        {
                            free(string_data[k]);
                        }
                    }
                }
            }
        }
    }

    variable_data = realloc(variable->data.ptr, (size_t)new_num_elements * element_size);
    if (variable_data == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %u bytes) (%s:%u)",
                       new_num_elements * element_size, __FILE__, __LINE__);
        return -1;
    }
    variable->data.ptr = variable_data;

    /* update variable properties */
    variable->num_elements = new_num_elements;
    variable->dimension[dim_index] = num_dim_elements;

    return 0;
}

/** Resize the dimension of a variable.
 * If the new dimension is shorter, the dimension is truncated.
 * If the new dimension is larger, new items will be filled with NaN (floating point), 0 (integer), or NULL (string).
 * \param variable Pointer to variable that should have a dimension added.
 * \param dim_index The position in the list of dimensions that needs to be resized.
 * \param length The new length of the dimension
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
int harp_variable_resize_dimension(harp_variable *variable, int dim_index, long length)
{
    void *data;
    long element_size;
    long new_num_elements;
    long num_block_elements;
    long num_blocks;
    long i, j;

    if (variable == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variable argument is NULL (%s:%u)", __FILE__, __LINE__);
        return -1;
    }
    if (dim_index < 0 || dim_index >= variable->num_dimensions)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "dim_index argument (%d) is not in the range [0:%d) (%s:%u)",
                       dim_index, variable->num_dimensions, __FILE__, __LINE__);
        return -1;
    }
    if (length <= 0)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "invalid length (%ld) for new dimension (%s:%u)", length, __FILE__,
                       __LINE__);
        return -1;
    }
    if (length == variable->dimension[dim_index])
    {
        /* nothing to do */
        return 0;
    }

    element_size = harp_get_size_for_type(variable->data_type);
    num_blocks = 1;
    for (i = 0; i < dim_index; i++)
    {
        num_blocks *= variable->dimension[i];
    }
    num_block_elements = 1;
    for (i = dim_index + 1; i < variable->num_dimensions; i++)
    {
        num_block_elements *= variable->dimension[i];
    }
    new_num_elements = num_blocks * length * num_block_elements;

    if (length < variable->dimension[dim_index])
    {
        /* shrink data */
        for (i = 0; i < num_blocks; i++)
        {
            long from_offset = i * variable->dimension[dim_index] * num_block_elements;
            long to_offset = i * length * num_block_elements;

            if (variable->data_type == harp_type_string)
            {
                /* remove trailing strings */
                for (j = length * num_block_elements; j < variable->dimension[dim_index] * num_block_elements; j++)
                {
                    if (variable->data.string_data[from_offset + j] != NULL)
                    {
                        free(variable->data.string_data[from_offset + j]);
                    }
                }
            }

            if (i > 0)
            {
                memmove((char *)variable->data.ptr + to_offset * element_size,
                        (char *)variable->data.ptr + from_offset * element_size,
                        (size_t)(length * num_block_elements * element_size));
            }
        }
    }

    data = realloc(variable->data.ptr, (size_t)new_num_elements * element_size);
    if (data == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY,
                       "out of memory (could not allocate %lu bytes) (%s:%u)",
                       (size_t)new_num_elements * element_size, __FILE__, __LINE__);
        return -1;
    }
    variable->data.ptr = data;

    if (length > variable->dimension[dim_index])
    {
        /* expand data */
        for (i = num_blocks - 1; i >= 0; i--)
        {
            long from_offset = i * variable->dimension[dim_index] * num_block_elements;
            long to_offset = i * length * num_block_elements;

            if (i > 0)
            {
                memmove((char *)variable->data.ptr + to_offset * element_size,
                        (char *)variable->data.ptr + from_offset * element_size,
                        (size_t)(variable->dimension[dim_index] * num_block_elements * element_size));
            }

            for (j = variable->dimension[dim_index] * num_block_elements; j < length * num_block_elements; j++)
            {
                /* set trailing data to NaN/0/NULL */
                switch (variable->data_type)
                {
                    case harp_type_int8:
                        variable->data.int8_data[to_offset + j] = 0;
                        break;
                    case harp_type_int16:
                        variable->data.int16_data[to_offset + j] = 0;
                        break;
                    case harp_type_int32:
                        variable->data.int32_data[to_offset + j] = 0;
                        break;
                    case harp_type_float:
                        variable->data.float_data[to_offset + j] = (float)harp_nan();
                        break;
                    case harp_type_double:
                        variable->data.double_data[to_offset + j] = harp_nan();
                        break;
                    case harp_type_string:
                        variable->data.string_data[to_offset + j] = NULL;
                        break;
                }
            }
        }
    }
    variable->num_elements = new_num_elements;
    variable->dimension[dim_index] = length;

    return 0;
}

/** Add dimension to a variable, replicating data for all subdimensions.
 * The dimension will be inserted at position of \a dim_index in the list of dimensions.
 * If \a dim_index equals \c num_dimensions, then the new dimension is appended.
 * \param variable Pointer to variable that should have a dimension added.
 * \param dim_index Position in the list of dimensions at which to insert the new dimension [0..num_dimensions].
 * \param dimension_type Type of the new dimension.
 * \param length Length of the new dimension.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
int harp_variable_add_dimension(harp_variable *variable, int dim_index, harp_dimension_type dimension_type, long length)
{
    void *data;
    long element_size;
    long new_num_elements;
    long num_block_elements;
    long num_blocks;
    long i, j, k;

    if (variable == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variable is NULL (%s:%u)", __FILE__, __LINE__);
        return -1;
    }
    if (dim_index < 0 || dim_index > variable->num_dimensions)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "dim_index argument (%d) is not in the range [0:%d] (%s:%u)",
                       dim_index, variable->num_dimensions, __FILE__, __LINE__);
        return -1;
    }
    if (length <= 0)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "invalid length (%ld) for new dimension (%s:%u)", length, __FILE__,
                       __LINE__);
        return -1;
    }
    if (variable->num_dimensions == HARP_MAX_NUM_DIMS)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "cannot add dimension to variable that already has the maximum "
                       "number of allowed dimensions (%s:%u)", __FILE__, __LINE__);
        return -1;
    }
    if (variable->num_elements == 0)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "cannot reshape variable (variable has 0 elements) (%s:%u)",
                       __FILE__, __LINE__);
        return -1;
    }
    if (dimension_type == harp_dimension_time)
    {
        if (dim_index != 0)
        {
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "dimensions of type '%s' can only be added at index 0 (%s:%u)",
                           harp_get_dimension_type_name(harp_dimension_time), __FILE__, __LINE__);
            return -1;
        }

        if (variable->num_dimensions >= 1 && variable->dimension_type[0] == harp_dimension_time)
        {
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "cannot add dimension of type '%s' because variable already has"
                           " a dimension of this type (%s:%u)", harp_get_dimension_type_name(harp_dimension_time),
                           __FILE__, __LINE__);
            return -1;
        }
    }
    if (dimension_type != harp_dimension_independent)
    {
        for (i = 0; i < variable->num_dimensions; ++i)
        {
            if (variable->dimension_type[i] == dimension_type && variable->dimension[i] != length)
            {
                harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "length (%ld) for new dimension of type '%s' is"
                               " inconsistent with length (%ld) of existing dimension of the same type (%s:%u)", length,
                               harp_get_dimension_type_name(dimension_type), variable->dimension[i], __FILE__,
                               __LINE__);
                return -1;
            }
        }
    }

    element_size = harp_get_size_for_type(variable->data_type);
    num_block_elements = 1;
    for (i = dim_index; i < variable->num_dimensions; i++)
    {
        num_block_elements *= variable->dimension[i];
    }
    num_blocks = variable->num_elements / num_block_elements;

    new_num_elements = num_blocks * length * num_block_elements;

    data = realloc(variable->data.ptr, (size_t)new_num_elements * element_size);
    if (data == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       (size_t)new_num_elements * element_size, __FILE__, __LINE__);
        return -1;
    }
    variable->data.ptr = data;

    for (i = num_blocks - 1; i >= 0; i--)
    {
        char *from_ptr;

        from_ptr = (char *)variable->data.ptr + i * num_block_elements * element_size;

        for (j = length - 1; j >= 0; j--)
        {
            char *to_ptr;

            to_ptr = (char *)variable->data.ptr + (i * length + j) * num_block_elements * element_size;

            memmove(to_ptr, from_ptr, (size_t)(num_block_elements * element_size));

            if (variable->data_type == harp_type_string && j != 0)
            {
                char **string_data = (char **)to_ptr;

                /* duplicate all strings in the block (except for the first block) */
                for (k = 0; k < num_block_elements; k++)
                {
                    if (string_data[k] != NULL)
                    {
                        string_data[k] = strdup(string_data[k]);
                        if (string_data[k] == NULL)
                        {
                            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string)"
                                           " (%s:%u)", __FILE__, __LINE__);
                            return -1;
                        }
                    }
                }
            }
        }
    }
    variable->num_elements = new_num_elements;
    variable->num_dimensions++;
    for (i = variable->num_dimensions - 1; i > dim_index; i--)
    {
        variable->dimension_type[i] = variable->dimension_type[i - 1];
        variable->dimension[i] = variable->dimension[i - 1];
    }
    variable->dimension_type[dim_index] = dimension_type;
    variable->dimension[dim_index] = length;

    return 0;
}

/** Remove dimension of a variable, keeping only the given index of that dimension in the result.
 * This function removes all elements in the given dimension except for the element at \a index and then collapses the
 * dimension (the number of dimensions will be reduced by 1).
 *
 * \param variable Pointer to variable that should have its data rearranged.
 * \param dim_index The id of the dimension in which the rearrangement should take place.
 * \param index The index of the element to keep within the to-be-removed dimension.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
int harp_variable_remove_dimension(harp_variable *variable, int dim_index, long index)
{
    int i;

    if (harp_variable_rearrange_dimension(variable, dim_index, 1, &index) != 0)
    {
        return -1;
    }

    for (i = dim_index; i < variable->num_dimensions - 1; i++)
    {
        variable->dimension[i] = variable->dimension[i + 1];
        variable->dimension_type[i] = variable->dimension_type[i + 1];
    }
    variable->num_dimensions--;

    return 0;
}

/** \addtogroup harp_variable
 * @{
 */

/** Create new variable.
 * \param name Name of the variable.
 * \param data_type Storage type of the variable data.
 * \param num_dimensions Number of array dimensions (use '0' for scalar data).
 * \param dimension_type Array with the dimension type for each of the dimensions.
 * \param dimension Array with length for each of the dimensions.
 * \param new_variable Pointer to the C variable where the new HARP variable will be stored.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_variable_new(const char *name, harp_data_type data_type, int num_dimensions,
                                  const harp_dimension_type *dimension_type, const long *dimension,
                                  harp_variable **new_variable)
{
    harp_variable *variable;
    int i;

    if (!harp_is_identifier(name))
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variable name '%s' is not a valid identifier (%s:%u)", name,
                       __FILE__, __LINE__);
        return -1;
    }

    /* Check dimension related arguments. */
    if (dimension_type != NULL)
    {
        long dimension_length[HARP_NUM_DIM_TYPES];

        for (i = 0; i < HARP_NUM_DIM_TYPES; ++i)
        {
            dimension_length[i] = -1;
        }

        for (i = 0; i < num_dimensions; ++i)
        {
            if (dimension_type[i] == harp_dimension_independent)
            {
                continue;
            }

            if (dimension_type[i] == harp_dimension_time && i != 0)
            {
                harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "dimensions of type '%s' only allowed at index 0 (%s:%u)",
                               harp_get_dimension_type_name(harp_dimension_time), __FILE__, __LINE__);
                return -1;
            }

            if (dimension_length[dimension_type[i]] == -1)
            {
                dimension_length[dimension_type[i]] = dimension[i];
            }
            else if (dimension_length[dimension_type[i]] != dimension[i])
            {
                harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "inconsistent lengths (%ld != %ld) encountered for"
                               " dimension of type '%s' (%s:%u)", dimension_length[dimension_type[i]], dimension[i],
                               harp_get_dimension_type_name(dimension_type[i]), __FILE__, __LINE__);
                return -1;
            }
        }
    }

    variable = (harp_variable *)malloc(sizeof(harp_variable));
    if (variable == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_variable), __FILE__, __LINE__);
        return -1;
    }
    variable->name = NULL;
    variable->data_type = data_type;
    variable->num_dimensions = num_dimensions;
    variable->data.ptr = NULL;
    variable->description = NULL;
    variable->unit = NULL;
    variable->num_enum_values = 0;
    variable->enum_name = NULL;

    variable->num_elements = 1;
    for (i = 0; i < num_dimensions; i++)
    {
        variable->dimension_type[i] = (dimension_type == NULL ? harp_dimension_independent : dimension_type[i]);
        variable->dimension[i] = dimension[i];
        variable->num_elements *= dimension[i];
    }

    variable->name = strdup(name);
    if (variable->name == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        harp_variable_delete(variable);
        return -1;
    }

    variable->data.ptr = malloc((size_t)variable->num_elements * harp_get_size_for_type(data_type));
    if (variable->data.ptr == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       variable->num_elements * harp_get_size_for_type(data_type), __FILE__, __LINE__);
        harp_variable_delete(variable);
        return -1;
    }
    memset(variable->data.ptr, 0, (size_t)variable->num_elements * harp_get_size_for_type(data_type));

    if (data_type != harp_type_string)
    {
        variable->valid_min = harp_get_valid_min_for_type(data_type);
        variable->valid_max = harp_get_valid_max_for_type(data_type);
    }

    *new_variable = variable;
    return 0;
}

/** Delete variable.
 * Remove variable and all attached attributes.
 * \param variable HARP variable
 */
LIBHARP_API void harp_variable_delete(harp_variable *variable)
{
    if (variable == NULL)
    {
        return;
    }
    if (variable->name != NULL)
    {
        free(variable->name);
    }
    if (variable->data.ptr != NULL)
    {
        if (variable->data_type == harp_type_string)
        {
            long i;

            for (i = 0; i < variable->num_elements; i++)
            {
                if (variable->data.string_data[i] != NULL)
                {
                    free(variable->data.string_data[i]);
                }
            }
        }
        free(variable->data.ptr);
    }
    if (variable->description != NULL)
    {
        free(variable->description);
    }
    if (variable->unit != NULL)
    {
        free(variable->unit);
    }
    if (variable->enum_name != NULL)
    {
        int i;

        for (i = 0; i < variable->num_enum_values; i++)
        {
            if (variable->enum_name[i] != NULL)
            {
                free(variable->enum_name[i]);
            }
        }
        free(variable->enum_name);
    }

    free(variable);
}

/** Create a copy of a variable.
 * The function will create a deep-copy of the given HARP variable, also creating copyies of all attributes.
 * \param other_variable Variable that should be copied.
 * \param new_variable Pointer to the C variable where the new HARP variable will be stored.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_variable_copy(const harp_variable *other_variable, harp_variable **new_variable)
{
    harp_variable *variable;
    long i;

    variable = (harp_variable *)malloc(sizeof(harp_variable));
    if (variable == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_variable), __FILE__, __LINE__);
        return -1;
    }
    variable->name = NULL;
    variable->data_type = other_variable->data_type;
    variable->num_dimensions = other_variable->num_dimensions;
    for (i = 0; i < variable->num_dimensions; i++)
    {
        variable->dimension_type[i] = other_variable->dimension_type[i];
        variable->dimension[i] = other_variable->dimension[i];
    }
    variable->num_elements = other_variable->num_elements;
    variable->data.ptr = NULL;
    variable->description = NULL;
    variable->unit = NULL;
    variable->valid_min = other_variable->valid_min;
    variable->valid_max = other_variable->valid_max;
    variable->num_enum_values = 0;
    variable->enum_name = NULL;

    variable->name = strdup(other_variable->name);
    if (variable->name == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        harp_variable_delete(variable);
        return -1;
    }

    if (other_variable->description != NULL)
    {
        variable->description = strdup(other_variable->description);
        if (variable->description == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                           __LINE__);
            harp_variable_delete(variable);
            return -1;
        }
    }

    if (other_variable->unit != NULL)
    {
        variable->unit = strdup(other_variable->unit);
        if (variable->unit == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                           __LINE__);
            harp_variable_delete(variable);
            return -1;
        }
    }

    if (other_variable->enum_name != NULL && other_variable->num_enum_values > 0)
    {
        variable->enum_name = malloc(other_variable->num_enum_values * sizeof(char *));
        if (variable->enum_name == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           other_variable->num_enum_values * sizeof(char *), __FILE__, __LINE__);
            harp_variable_delete(variable);
            return -1;
        }
        for (i = 0; i < other_variable->num_enum_values; i++)
        {
            variable->enum_name[i] = strdup(other_variable->enum_name[i]);
            if (variable->enum_name[i] == NULL)
            {
                harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)",
                               __FILE__, __LINE__);
                harp_variable_delete(variable);
                return -1;
            }
            variable->num_enum_values++;
        }
    }

    variable->data.ptr = malloc((size_t)variable->num_elements * harp_get_size_for_type(variable->data_type));
    if (variable->data.ptr == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       variable->num_elements * harp_get_size_for_type(variable->data_type), __FILE__, __LINE__);
        harp_variable_delete(variable);
        return -1;
    }
    if (variable->data_type == harp_type_string)
    {
        memset(variable->data.ptr, 0, (size_t)variable->num_elements * harp_get_size_for_type(harp_type_string));
        for (i = 0; i < variable->num_elements; i++)
        {
            variable->data.string_data[i] = strdup(other_variable->data.string_data[i]);
            if (variable->data.string_data[i] == NULL)
            {
                harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                               __LINE__);
                harp_variable_delete(variable);
                return -1;
            }
        }
    }
    else
    {
        memcpy(variable->data.ptr, other_variable->data.ptr,
               (size_t)variable->num_elements * harp_get_size_for_type(variable->data_type));
    }

    *new_variable = variable;
    return 0;
}

/** Append one variable to another.
 * Both variables need to have the 'time' dimension as first dimension.
 * And all non-time dimensions need to be the same for both variables.
 * \param variable Variable to which data should be appended.
 * \param other_variable Variable that should be appended.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_variable_append(harp_variable *variable, const harp_variable *other_variable)
{
    void *data;
    long element_size;
    long new_num_elements;
    long i;

    if (strcmp(variable->name, other_variable->name) != 0)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variables don't have the same name");
        return -1;
    }
    if (variable->data_type != other_variable->data_type)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variables don't have the same datatype (%s)", variable->name);
        return -1;
    }
    if (variable->num_dimensions != other_variable->num_dimensions)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variables don't have the same number of dimensions (%s)",
                       variable->name);
        return -1;
    }
    if (variable->num_enum_values != other_variable->num_enum_values)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variables don't have the same number of enumeration values (%s)",
                       variable->name);
        return -1;
    }
    if (variable->num_dimensions == 0 || variable->dimension_type[0] != harp_dimension_time ||
        other_variable->num_dimensions == 0 || other_variable->dimension_type[0] != harp_dimension_time)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variables need to be time dependent (%s)", variable->name);
        return -1;
    }

    for (i = 1; i < variable->num_dimensions; i++)
    {
        if (variable->dimension_type[i] != other_variable->dimension_type[i])
        {
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variables (%s) don't have the same type of dimensions",
                           variable->name);
            return -1;
        }
        if (variable->dimension[i] != other_variable->dimension[i])
        {
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variables (%s) don't have the same dimension lengths",
                           variable->name);
            return -1;
        }
    }

    element_size = harp_get_size_for_type(variable->data_type);
    new_num_elements = variable->num_elements + other_variable->num_elements;
    data = realloc(variable->data.ptr, (size_t)new_num_elements * element_size);
    if (data == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       (size_t)new_num_elements * element_size, __FILE__, __LINE__);
        return -1;
    }
    variable->data.ptr = data;

    if (variable->data_type == harp_type_string)
    {
        memset(&variable->data.string_data[variable->num_elements], 0,
               (size_t)other_variable->num_elements * element_size);
        for (i = 0; i < other_variable->num_elements; i++)
        {
            variable->data.string_data[variable->num_elements + i] = strdup(other_variable->data.string_data[i]);
            if (variable->data.string_data[variable->num_elements + i] == NULL)
            {
                harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                               __LINE__);
                return -1;
            }
        }
    }
    else
    {
        char *to_ptr = (char *)variable->data.ptr + variable->num_elements * element_size;

        memcpy(to_ptr, other_variable->data.ptr, (size_t)other_variable->num_elements * element_size);
    }
    variable->dimension[0] += other_variable->dimension[0];
    variable->num_elements += other_variable->num_elements;

    return 0;
}

/** Change the name of a variable.
 * \param variable The variable for which the name should be changed.
 * \param name The new name of the variable.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_variable_rename(harp_variable *variable, const char *name)
{
    if (variable == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variable is NULL (%s:%u)", __FILE__, __LINE__);
        return -1;
    }
    if (name == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "name is NULL (%s:%u)", __FILE__, __LINE__);
        return -1;
    }

    /* Set variable name */
    if (variable->name != NULL)
    {
        free(variable->name);
    }

    variable->name = strdup(name);

    if (variable->name == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        return -1;
    }

    return 0;
}

/** Set the unit attribute of the specified variable.
 * Store a copy of \a unit as the value of the unit attribute of the specified variable. The previous value (if any)
 * will be freed.
 * \param variable Variable for which to set the unit attribute.
 * \param unit New value for the unit attribute.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_variable_set_unit(harp_variable *variable, const char *unit)
{
    char *unit_copy;

    if (unit == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "unit is NULL (%s:%u)", __FILE__, __LINE__);
        return -1;
    }

    unit_copy = strdup(unit);

    if (unit_copy == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        return -1;
    }

    if (variable->unit != NULL)
    {
        free(variable->unit);
    }

    variable->unit = unit_copy;

    return 0;
}

/** Set the description attribute of the specified variable.
 * Store a copy of \a description as the value of the description attribute of the specified variable. The previous
 * value (if any) will be freed.
 * \param variable Variable for which to set the description attribute.
 * \param description New value for the description attribute.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_variable_set_description(harp_variable *variable, const char *description)
{
    char *description_copy;

    if (description == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "description is NULL (%s:%u)", __FILE__, __LINE__);
        return -1;
    }

    description_copy = strdup(description);

    if (description_copy == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        return -1;
    }

    if (variable->description != NULL)
    {
        free(variable->description);
    }

    variable->description = description_copy;

    return 0;
}

/** Set the enumeration values for a variable with integer values.
 * This will assign enumeration labels to each integer value from 0 to num_enum_values-1.
 * The enumeration label for a value is found by using the value as index in the enum_name array.
 * This function will also set valid_min to 0 and valid_max to num_enum_values - 1
 * (i.e. any variable value that lies outside the index range is considered invalid).
 * You can pass num_enum_values = 0 to clear any previous enumeration values that were set for the variable.
 * \param variable Variable for which to set the enumeration values.
 * \param num_enum_values Number of enumeration values
 * \param enum_name Array with the names for each of the enumeration values
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_variable_set_enumeration_values(harp_variable *variable, int num_enum_values,
                                                     const char **enum_name)
{
    int i;

    if (num_enum_values < 0)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "num_enum_values < 0 (%s:%u)", __FILE__, __LINE__);
        return -1;
    }
    if (num_enum_values > 0)
    {
        if (enum_name == NULL)
        {
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "enum_name is NULL (%s:%u)", __FILE__, __LINE__);
            return -1;
        }
        for (i = 0; i < num_enum_values; i++)
        {
            if (enum_name[i] == NULL)
            {
                harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "enum_name[%d] is NULL (%s:%u)", i, __FILE__, __LINE__);
                return -1;
            }
            if (!harp_is_identifier(enum_name[i]))
            {
                harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "enumeration value '%s' is not a valid identifier (%s:%u)",
                               enum_name[i], __FILE__, __LINE__);
                return -1;
            }
        }
    }
    if (variable->data_type != harp_type_int8 && variable->data_type != harp_type_int16 &&
        variable->data_type != harp_type_int32)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "enumeration values not allowed for variable of type %s (%s:%u)",
                       harp_get_data_type_name(variable->data_type), __FILE__, __LINE__);
        return -1;
    }

    /* first clear the existing enumeration values */
    if (variable->enum_name != NULL)
    {
        for (i = 0; i < variable->num_enum_values; i++)
        {
            if (variable->enum_name[i] != NULL)
            {
                free(variable->enum_name[i]);
            }
        }
        free(variable->enum_name);
        variable->enum_name = NULL;
    }
    variable->num_enum_values = 0;

    if (num_enum_values == 0)
    {
        /* nothing futher to do -> return */
        return 0;
    }

    variable->enum_name = malloc(num_enum_values * sizeof(char *));
    if (variable->enum_name == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_enum_values * sizeof(char *), __FILE__, __LINE__);
        return -1;
    }
    for (i = 0; i < num_enum_values; i++)
    {
        variable->enum_name[i] = strdup(enum_name[i]);
        if (variable->enum_name[i] == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)",
                           __FILE__, __LINE__);
            return -1;
        }
        variable->num_enum_values++;
    }

    switch (variable->data_type)
    {
        case harp_type_int8:
            variable->valid_min.int8_data = 0;
            variable->valid_max.int8_data = (int8_t)(num_enum_values - 1);
            break;
        case harp_type_int16:
            variable->valid_min.int16_data = 0;
            variable->valid_max.int16_data = (int16_t)(num_enum_values - 1);
            break;
        case harp_type_int32:
            variable->valid_min.int32_data = 0;
            variable->valid_max.int32_data = (int32_t)(num_enum_values - 1);
            break;
        default:
            assert(0);
            exit(1);
    }

    return 0;
}

/** Store a copy of \a str at \a index into the flattened array of strings associated with \a variable.
 * \warning No boundary checks are performed on \a index.
 * \param variable Variable of type string.
 * \param index Index into the flattened array of strings associated with the variable.
 * \param str The string of which a copy should be stored.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_variable_set_string_data_element(harp_variable *variable, long index, const char *str)
{
    if (variable == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variable is NULL (%s:%u)", __FILE__, __LINE__);
        return -1;
    }
    if (variable->data_type != harp_type_string)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variable is of type '%s'; expected '%s' (%s:%u)",
                       harp_get_data_type_name(variable->data_type), harp_get_data_type_name(harp_type_string),
                       __FILE__, __LINE__);
        return -1;
    }
    if (index < 0)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "index is negative (%s:%u)", __FILE__, __LINE__);
        return -1;
    }
    if (str == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "str is NULL (%s:%u)", __FILE__, __LINE__);
        return -1;
    }

    if (variable->data.string_data[index] != NULL)
    {
        free(variable->data.string_data[index]);
    }

    variable->data.string_data[index] = strdup(str);

    if (variable->data.string_data[index] == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        return -1;
    }

    return 0;
}

/** Convert the data for the variable such that it matches the given data type.
 * The memory for the block holding the data for the attribute will be resized to match the new data type if needed.
 * You cannot convert string data to numeric data or vice-versa. Conversion from floating point to integer data (or
 * vice versa) is allowed though.
 * \param variable Variable whose data should be aligned with the given data type.
 * \param target_data_type Data type to which the data for the variable should be converted.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_variable_convert_data_type(harp_variable *variable, harp_data_type target_data_type)
{
    harp_scalar valid_min;
    harp_scalar valid_max;
    harp_array data;
    int i;

    if (variable == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variable is NULL (%s:%u)", __FILE__, __LINE__);
        return -1;
    }

    if (variable->data_type == target_data_type)
    {
        /* no conversion */
        return 0;
    }
    if (variable->data_type == harp_type_string)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT,
                       "conversion from string to numerical value is not possible for variable '%s'", variable->name);
        return -1;
    }
    if (target_data_type == harp_type_string)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT,
                       "conversion from numerical value to string is not possible for variable '%s'", variable->name);
        return -1;
    }
    if (variable->num_elements == 0 || variable->data.ptr == NULL)
    {
        /* nothing to convert */
        variable->data_type = target_data_type;
        return 0;
    }

    valid_min = harp_get_valid_min_for_type(target_data_type);
    valid_max = harp_get_valid_max_for_type(target_data_type);

    switch (target_data_type)
    {
        case harp_type_int8:
            data.ptr = malloc((size_t)variable->num_elements * sizeof(int8_t));
            if (data.ptr == NULL)
            {
                harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                               variable->num_elements * sizeof(int8_t), __FILE__, __LINE__);
                return -1;
            }
            switch (variable->data_type)
            {
                case harp_type_int16:
                    for (i = 0; i < variable->num_elements; i++)
                    {
                        data.int8_data[i] = (int8_t)variable->data.int16_data[i];
                    }
                    if (variable->valid_min.int16_data < valid_min.int8_data)
                    {
                        variable->valid_min = valid_min;
                    }
                    else
                    {
                        variable->valid_min.int8_data = (int8_t)variable->valid_min.int16_data;
                    }
                    if (variable->valid_max.int16_data > valid_max.int8_data)
                    {
                        variable->valid_max = valid_max;
                    }
                    else
                    {
                        variable->valid_max.int8_data = (int8_t)variable->valid_max.int16_data;
                    }
                    break;
                case harp_type_int32:
                    for (i = 0; i < variable->num_elements; i++)
                    {
                        data.int8_data[i] = (int8_t)variable->data.int32_data[i];
                    }
                    if (variable->valid_min.int32_data < valid_min.int8_data)
                    {
                        variable->valid_min = valid_min;
                    }
                    else
                    {
                        variable->valid_min.int8_data = (int8_t)variable->valid_min.int32_data;
                    }
                    if (variable->valid_max.int32_data > valid_max.int8_data)
                    {
                        variable->valid_max = valid_max;
                    }
                    else
                    {
                        variable->valid_max.int8_data = (int8_t)variable->valid_max.int32_data;
                    }
                    break;
                case harp_type_float:
                    for (i = 0; i < variable->num_elements; i++)
                    {
                        data.int8_data[i] = (int8_t)variable->data.float_data[i];
                    }
                    if (variable->valid_min.float_data < valid_min.int8_data)
                    {
                        variable->valid_min = valid_min;
                    }
                    else
                    {
                        variable->valid_min.int8_data = (int8_t)variable->valid_min.float_data;
                    }
                    if (variable->valid_max.float_data > valid_max.int8_data)
                    {
                        variable->valid_max = valid_max;
                    }
                    else
                    {
                        variable->valid_max.int8_data = (int8_t)variable->valid_max.float_data;
                    }
                    break;
                case harp_type_double:
                    for (i = 0; i < variable->num_elements; i++)
                    {
                        data.int8_data[i] = (int8_t)variable->data.double_data[i];
                    }
                    if (variable->valid_min.double_data < valid_min.int8_data)
                    {
                        variable->valid_min = valid_min;
                    }
                    else
                    {
                        variable->valid_min.int8_data = (int8_t)variable->valid_min.double_data;
                    }
                    if (variable->valid_max.double_data > valid_max.int8_data)
                    {
                        variable->valid_max = valid_max;
                    }
                    else
                    {
                        variable->valid_max.int8_data = (int8_t)variable->valid_max.double_data;
                    }
                    break;
                default:
                    assert(0);
                    exit(1);
            }
            break;
        case harp_type_int16:
            data.ptr = malloc((size_t)variable->num_elements * sizeof(int16_t));
            if (data.ptr == NULL)
            {
                harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                               variable->num_elements * sizeof(int16_t), __FILE__, __LINE__);
                return -1;
            }
            switch (variable->data_type)
            {
                case harp_type_int8:
                    for (i = 0; i < variable->num_elements; i++)
                    {
                        data.int16_data[i] = (int16_t)variable->data.int8_data[i];
                    }
                    variable->valid_min.int16_data = (int16_t)variable->valid_min.int8_data;
                    variable->valid_max.int16_data = (int16_t)variable->valid_max.int8_data;
                    break;
                case harp_type_int32:
                    for (i = 0; i < variable->num_elements; i++)
                    {
                        data.int16_data[i] = (int16_t)variable->data.int32_data[i];
                    }
                    if (variable->valid_min.int32_data < valid_min.int16_data)
                    {
                        variable->valid_min = valid_min;
                    }
                    else
                    {
                        variable->valid_min.int16_data = (int16_t)variable->valid_min.int32_data;
                    }
                    if (variable->valid_max.int32_data > valid_max.int16_data)
                    {
                        variable->valid_max = valid_max;
                    }
                    else
                    {
                        variable->valid_max.int16_data = (int16_t)variable->valid_max.int32_data;
                    }
                    break;
                case harp_type_float:
                    for (i = 0; i < variable->num_elements; i++)
                    {
                        data.int16_data[i] = (int16_t)variable->data.float_data[i];
                    }
                    if (variable->valid_min.float_data < valid_min.int16_data)
                    {
                        variable->valid_min = valid_min;
                    }
                    else
                    {
                        variable->valid_min.int16_data = (int16_t)variable->valid_min.float_data;
                    }
                    if (variable->valid_max.float_data > valid_max.int16_data)
                    {
                        variable->valid_max = valid_max;
                    }
                    else
                    {
                        variable->valid_max.int16_data = (int16_t)variable->valid_max.float_data;
                    }
                    break;
                case harp_type_double:
                    for (i = 0; i < variable->num_elements; i++)
                    {
                        data.int16_data[i] = (int16_t)variable->data.double_data[i];
                    }
                    if (variable->valid_min.double_data < valid_min.int16_data)
                    {
                        variable->valid_min = valid_min;
                    }
                    else
                    {
                        variable->valid_min.int16_data = (int16_t)variable->valid_min.double_data;
                    }
                    if (variable->valid_max.double_data > valid_max.int16_data)
                    {
                        variable->valid_max = valid_max;
                    }
                    else
                    {
                        variable->valid_max.int16_data = (int16_t)variable->valid_max.double_data;
                    }
                    break;
                default:
                    assert(0);
                    exit(1);
            }
            break;
        case harp_type_int32:
            data.ptr = malloc((size_t)variable->num_elements * sizeof(int32_t));
            if (data.ptr == NULL)
            {
                harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                               variable->num_elements * sizeof(int32_t), __FILE__, __LINE__);
                return -1;
            }
            switch (variable->data_type)
            {
                case harp_type_int8:
                    for (i = 0; i < variable->num_elements; i++)
                    {
                        data.int32_data[i] = (int32_t)variable->data.int8_data[i];
                    }
                    variable->valid_min.int32_data = (int32_t)variable->valid_min.int8_data;
                    variable->valid_max.int32_data = (int32_t)variable->valid_max.int8_data;
                    break;
                case harp_type_int16:
                    for (i = 0; i < variable->num_elements; i++)
                    {
                        data.int32_data[i] = (int32_t)variable->data.int16_data[i];
                    }
                    variable->valid_min.int32_data = (int32_t)variable->valid_min.int16_data;
                    variable->valid_max.int32_data = (int32_t)variable->valid_max.int16_data;
                    break;
                case harp_type_float:
                    for (i = 0; i < variable->num_elements; i++)
                    {
                        data.int32_data[i] = (int32_t)variable->data.float_data[i];
                    }
                    if (variable->valid_min.float_data < valid_min.int32_data)
                    {
                        variable->valid_min = valid_min;
                    }
                    else
                    {
                        variable->valid_min.int32_data = (int32_t)variable->valid_min.float_data;
                    }
                    if (variable->valid_max.float_data > valid_max.int32_data)
                    {
                        variable->valid_max = valid_max;
                    }
                    else
                    {
                        variable->valid_max.int32_data = (int32_t)variable->valid_max.float_data;
                    }
                    break;
                case harp_type_double:
                    for (i = 0; i < variable->num_elements; i++)
                    {
                        data.int32_data[i] = (int32_t)variable->data.double_data[i];
                    }
                    if (variable->valid_min.double_data < valid_min.int32_data)
                    {
                        variable->valid_min = valid_min;
                    }
                    else
                    {
                        variable->valid_min.int32_data = (int32_t)variable->valid_min.double_data;
                    }
                    if (variable->valid_max.double_data > valid_max.int32_data)
                    {
                        variable->valid_max = valid_max;
                    }
                    else
                    {
                        variable->valid_max.int32_data = (int32_t)variable->valid_max.double_data;
                    }
                    break;
                default:
                    assert(0);
                    exit(1);
            }
            break;
        case harp_type_float:
            data.ptr = malloc((size_t)variable->num_elements * sizeof(float));
            if (data.ptr == NULL)
            {
                harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                               variable->num_elements * sizeof(float), __FILE__, __LINE__);
                return -1;
            }
            switch (variable->data_type)
            {
                case harp_type_int8:
                    for (i = 0; i < variable->num_elements; i++)
                    {
                        data.float_data[i] = (float)variable->data.int8_data[i];
                    }
                    variable->valid_min.float_data = (float)variable->valid_min.int8_data;
                    variable->valid_max.float_data = (float)variable->valid_max.int8_data;
                    break;
                case harp_type_int16:
                    for (i = 0; i < variable->num_elements; i++)
                    {
                        data.float_data[i] = (float)variable->data.int16_data[i];
                    }
                    variable->valid_min.float_data = (float)variable->valid_min.int16_data;
                    variable->valid_max.float_data = (float)variable->valid_max.int16_data;
                    break;
                case harp_type_int32:
                    for (i = 0; i < variable->num_elements; i++)
                    {
                        data.float_data[i] = (float)variable->data.int32_data[i];
                    }
                    variable->valid_min.float_data = (float)variable->valid_min.int32_data;
                    variable->valid_max.float_data = (float)variable->valid_max.int32_data;
                    break;
                case harp_type_double:
                    for (i = 0; i < variable->num_elements; i++)
                    {
                        data.float_data[i] = (float)variable->data.double_data[i];
                    }
                    if (variable->valid_min.double_data < valid_min.float_data)
                    {
                        variable->valid_min = valid_min;
                    }
                    else
                    {
                        variable->valid_min.float_data = (float)variable->valid_min.double_data;
                    }
                    if (variable->valid_max.double_data > valid_max.float_data)
                    {
                        variable->valid_max = valid_max;
                    }
                    else
                    {
                        variable->valid_max.float_data = (float)variable->valid_max.double_data;
                    }
                    break;
                default:
                    assert(0);
                    exit(1);
            }
            break;
        case harp_type_double:
            data.ptr = malloc((size_t)variable->num_elements * sizeof(double));
            if (data.ptr == NULL)
            {
                harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                               variable->num_elements * sizeof(double), __FILE__, __LINE__);
                return -1;
            }
            switch (variable->data_type)
            {
                case harp_type_int8:
                    for (i = 0; i < variable->num_elements; i++)
                    {
                        data.double_data[i] = (double)variable->data.int8_data[i];
                    }
                    variable->valid_min.double_data = (double)variable->valid_min.int8_data;
                    variable->valid_max.double_data = (double)variable->valid_max.int8_data;
                    break;
                case harp_type_int16:
                    for (i = 0; i < variable->num_elements; i++)
                    {
                        data.double_data[i] = (double)variable->data.int16_data[i];
                    }
                    variable->valid_min.double_data = (double)variable->valid_min.int16_data;
                    variable->valid_max.double_data = (double)variable->valid_max.int16_data;
                    break;
                case harp_type_int32:
                    for (i = 0; i < variable->num_elements; i++)
                    {
                        data.double_data[i] = (double)variable->data.int32_data[i];
                    }
                    variable->valid_min.double_data = (double)variable->valid_min.int32_data;
                    variable->valid_max.double_data = (double)variable->valid_max.int32_data;
                    break;
                case harp_type_float:
                    for (i = 0; i < variable->num_elements; i++)
                    {
                        data.double_data[i] = (double)variable->data.float_data[i];
                    }
                    variable->valid_min.double_data = (double)variable->valid_min.float_data;
                    variable->valid_max.double_data = (double)variable->valid_max.float_data;
                    break;
                default:
                    assert(0);
                    exit(1);
            }
            break;
        default:
            assert(0);
            exit(1);
    }

    free(variable->data.ptr);
    variable->data.ptr = data.ptr;
    variable->data_type = target_data_type;

    return 0;
}

/**
 * Test if variable contains at least one dimension equal to the specified dimension type.
 * \param variable Variable to check.
 * \param dimension_type Requested dimension type
 * \return
 *   \arg \c 0, Variable does not contain a dimension of the given dimension type
 *   \arg \c 1, Variable contains at least one dimension of the given dimension type
 */
LIBHARP_API int harp_variable_has_dimension_type(const harp_variable *variable, harp_dimension_type dimension_type)
{
    int i;

    for (i = 0; i < variable->num_dimensions; i++)
    {
        if (variable->dimension_type[i] == dimension_type)
        {
            return 1;
        }
    }

    return 0;
}

/**
 * Test if variable has dimensions equal to the specified list of dimension types.
 * \param variable Variable to check.
 * \param num_dimensions Number of dimensions the variable should have.
 * \param dimension_type Requested dimension type for each of the dimensions of the variable
 * \return
 *   \arg \c 0, Variable does not match the dimension types
 *   \arg \c 1, Variable matches number of dimensions and specified type for each dimension.
 */
LIBHARP_API int harp_variable_has_dimension_types(const harp_variable *variable, int num_dimensions,
                                                  const harp_dimension_type *dimension_type)
{
    int i;

    if (variable->num_dimensions != num_dimensions)
    {
        return 0;
    }

    for (i = 0; i < num_dimensions; i++)
    {
        if (variable->dimension_type[i] != dimension_type[i])
        {
            return 0;
        }
    }

    return 1;
}

/**
 * Test if variable has a unit equal to the specified unit.
 * \param variable Variable to check.
 * \param unit Number of dimensions the variable should have.
 * \return
 *   \arg \c 0, The unit of the variable does not equal the given unit
 *   \arg \c 1, The unit of the variable equals the given unit
 */
LIBHARP_API int harp_variable_has_unit(const harp_variable *variable, const char *unit)
{
    return (harp_unit_compare(variable->unit, unit) == 0);
}

/** Verify that a variable is internally consistent and complies with conventions.
 * \param variable Variable to verify.
 * \return
 *   \arg \c 0, Variable verified successfully.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_variable_verify(const harp_variable *variable)
{
    long dimension[HARP_MAX_NUM_DIMS] = { 0 };
    int dimension_index[HARP_MAX_NUM_DIMS] = { 0 };
    long num_elements;
    int i;

    if (variable == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variable is NULL");
        return -1;
    }

    if (variable->name == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_VARIABLE, "name undefined");
        return -1;
    }
    if (!harp_is_identifier(variable->name))
    {
        harp_set_error(HARP_ERROR_INVALID_VARIABLE, "variable name '%s' is not a valid identifier", variable->name);
        return -1;
    }

    switch (variable->data_type)
    {
        case harp_type_int8:
        case harp_type_int16:
        case harp_type_int32:
        case harp_type_float:
        case harp_type_double:
        case harp_type_string:
            break;
        default:
            harp_set_error(HARP_ERROR_INVALID_VARIABLE, "invalid data type");
            return -1;
    }

    if (variable->num_dimensions < 0 || variable->num_dimensions > HARP_MAX_NUM_DIMS)
    {
        harp_set_error(HARP_ERROR_INVALID_VARIABLE, "invalid number of dimensions %d", variable->num_dimensions);
        return -1;
    }

    for (i = 0; i < variable->num_dimensions; i++)
    {
        harp_dimension_type dimension_type = variable->dimension_type[i];

        switch (dimension_type)
        {
            case harp_dimension_independent:
            case harp_dimension_time:
            case harp_dimension_latitude:
            case harp_dimension_longitude:
            case harp_dimension_vertical:
            case harp_dimension_spectral:
                break;
            default:
                harp_set_error(HARP_ERROR_INVALID_VARIABLE, "dimension at index %d has invalid type", i);
                return -1;
        }

        if (dimension_type == harp_dimension_time && variable->dimension_type[0] != harp_dimension_time)
        {
            harp_set_error(HARP_ERROR_INVALID_VARIABLE, "inner dimension of type '%s' at index %d not allowed unless "
                           "outermost dimension (index 0) also of type '%s'",
                           harp_get_dimension_type_name(harp_dimension_time), i,
                           harp_get_dimension_type_name(harp_dimension_time));
            return -1;
        }

        if (variable->dimension[i] <= 0)
        {
            harp_set_error(HARP_ERROR_INVALID_VARIABLE, "dimension at index %d has invalid length %ld", i,
                           variable->dimension[i]);
            return -1;
        }

        if (dimension_type != harp_dimension_independent)
        {
            if (dimension[dimension_type] == 0)
            {
                dimension[dimension_type] = variable->dimension[i];
                dimension_index[dimension_type] = i;
            }
            else if (variable->dimension[i] != dimension[dimension_type])
            {
                harp_set_error(HARP_ERROR_INVALID_VARIABLE, "length %ld of dimension of type '%s' at index %d does not "
                               "match length %ld of dimension at index %d of the same type", variable->dimension[i],
                               harp_get_dimension_type_name(dimension_type), i, dimension[dimension_type],
                               dimension_index[dimension_type]);
                return -1;
            }
        }
    }

    if (variable->num_elements < 0)
    {
        harp_set_error(HARP_ERROR_INVALID_VARIABLE, "invalid number of elements %ld", variable->num_elements);
        return -1;
    }

    num_elements = harp_get_num_elements(variable->num_dimensions, variable->dimension);
    if (variable->num_elements != num_elements)
    {
        harp_set_error(HARP_ERROR_INVALID_VARIABLE, "number of elements %ld does not match product of dimension "
                       "lengths %ld", variable->num_elements, num_elements);
        return -1;

    }

    if (variable->num_elements > 0 && variable->data.ptr == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_VARIABLE, "number of elements is > 0, but variable contains no data");
        return -1;
    }

    if (variable->unit != NULL && !harp_unit_is_valid(variable->unit))
    {
        harp_set_error(HARP_ERROR_INVALID_VARIABLE, "invalid unit '%s'", variable->unit);
        return -1;
    }

    if ((variable->data_type == harp_type_float && harp_isnan(variable->valid_min.float_data)) ||
        (variable->data_type == harp_type_double && harp_isnan(variable->valid_min.double_data)))
    {
        harp_set_error(HARP_ERROR_INVALID_VARIABLE, "valid_min is NaN");
        return -1;
    }

    if ((variable->data_type == harp_type_float && harp_isnan(variable->valid_max.float_data)) ||
        (variable->data_type == harp_type_double && harp_isnan(variable->valid_max.double_data)))
    {
        harp_set_error(HARP_ERROR_INVALID_VARIABLE, "valid_max is NaN");
        return -1;
    }

    if (variable->num_enum_values > 0)
    {
        if (variable->enum_name == NULL)
        {
            harp_set_error(HARP_ERROR_INVALID_VARIABLE, "no enumeration names set");
            return -1;
        }
        for (i = 0; i < variable->num_enum_values; i++)
        {
            if (variable->enum_name[i] == NULL)
            {
                harp_set_error(HARP_ERROR_INVALID_VARIABLE, "empty enumeration value (%d)", i);
                return -1;
            }
            if (!harp_is_identifier(variable->enum_name[i]))
            {
                harp_set_error(HARP_ERROR_INVALID_VARIABLE, "enumeration value '%s' is not a valid identifier",
                               variable->enum_name[i]);
                return -1;
            }
        }
        if (variable->data_type != harp_type_int8 && variable->data_type != harp_type_int16 &&
            variable->data_type != harp_type_int32)
        {
            harp_set_error(HARP_ERROR_INVALID_VARIABLE, "enumeration values not allowed for variable of type %s",
                           harp_get_data_type_name(variable->data_type));
            return -1;
        }
    }

    return 0;
}

/** Print a harp_variable struct using the specified print function.
 * \param variable Variable to print.
 * \param show_attributes Whether or not to print the attributes.
 * \param print Print function to use
 */
LIBHARP_API void harp_variable_print(harp_variable *variable, int show_attributes, int (*print) (const char *, ...))
{
    int i;

    print("    ");
    if (variable == NULL)
    {
        print("NULL\n");
        return;
    }
    switch (variable->data_type)
    {
        case harp_type_int8:
            print("int8");
            break;
        case harp_type_int16:
            print("int16");
            break;
        case harp_type_int32:
            print("int32");
            break;
        case harp_type_float:
            print("float");
            break;
        case harp_type_double:
            print("double");
            break;
        case harp_type_string:
            print("string");
            break;
    }
    print(" %s", variable->name);
    if (variable->num_dimensions > 0)
    {
        print(" {");
        for (i = 0; i < variable->num_dimensions; i++)
        {
            if (variable->dimension_type[i] != harp_dimension_independent)
            {
                print("%s = ", harp_get_dimension_type_name(variable->dimension_type[i]));
            }
            print("%ld", variable->dimension[i]);
            if (i < variable->num_dimensions - 1)
            {
                print(", ");
            }
        }
        print("}");
    }
    if (variable->unit != NULL)
    {
        print(" [%s]", variable->unit);
    }
    print("\n");

    if (!show_attributes)
    {
        return;
    }

    if (variable->description != NULL)
    {
        print("        description = \"%s\"\n", variable->description);
    }
    if (variable->data_type != harp_type_string)
    {
        if (!harp_is_valid_min_for_type(variable->data_type, variable->valid_min))
        {
            print("        valid min = ");
            write_scalar(variable->valid_min, variable->data_type, print);
            print("\n");
        }
        if (!harp_is_valid_max_for_type(variable->data_type, variable->valid_max))
        {
            print("        valid max = ");
            write_scalar(variable->valid_max, variable->data_type, print);
            print("\n");
        }
    }
    if (variable->num_enum_values > 0)
    {
        print("        enum = ");
        if (variable->data_type == harp_type_int8)
        {
            /* print all values */
            for (i = 0; i < variable->num_enum_values; i++)
            {
                if (i > 0)
                {
                    print(", ");
                }
                print("\"%s\" (%d)", variable->enum_name[i], i);
            }
        }
        else
        {
            /* print only first and last value with ... */
            print("\"%s\" (0)", variable->enum_name[0]);
            if (variable->num_enum_values > 1)
            {
                if (variable->num_enum_values > 2)
                {
                    print(", ...");
                }
                print(", \"%s\" (%d)", variable->enum_name[variable->num_enum_values - 1],
                      variable->num_enum_values - 1);
            }
        }
        print("\n");

    }
    print("\n");
}

/** Print the data of a harp_variable using the specified print function.
 * \param variable Variable whose data to print.
 * \param print Print function to use
 */
LIBHARP_API void harp_variable_print_data(harp_variable *variable, int (*print) (const char *, ...))
{
    print("%s", variable->name);
    print(" = ");
    if (variable->num_dimensions <= 1)
    {
        write_array(variable->data, variable->data_type, variable->num_elements, 1, print);
    }
    else
    {
        write_array(variable->data, variable->data_type, variable->num_elements,
                    variable->dimension[variable->num_dimensions - 1], print);
    }
    print("\n\n");
}

/** @} */
