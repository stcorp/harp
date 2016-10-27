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
#include <stdio.h>

#include "harp-operation-parser-state.h"
#include "harp-program.h"

int harp_parser_state_new(harp_parser_state **new_state)
{
    harp_parser_state *state;

    state = (harp_parser_state *)malloc(sizeof(harp_parser_state));
    if (state == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_parser_state), __FILE__, __LINE__);
        return -1;
    }

    state->hasError = 0;
    state->error = NULL;
    state->result = NULL;

    if (harp_program_new(&state->result) != 0)
    {
        harp_parser_state_delete(state);
        return -1;
    }

    *new_state = state;
    return 0;
}

void harp_parser_state_delete(harp_parser_state *state)
{
    if (state->result)
    {
        harp_program_delete(state->result);
    }
    if (state->error)
    {
        free(state->error);
    }

    free(state);
}

void harp_parser_state_set_error(harp_parser_state *state, const char *error)
{
    state->hasError = 1;
    state->error = strdup(error);
}

int harp_sized_array_new(harp_sized_array **new_array)
{
    harp_sized_array *l;
    l = (harp_sized_array *)malloc(sizeof(harp_sized_array));
    if (l == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
        sizeof(harp_sized_array), __FILE__, __LINE__);
        return -1;
    }

    l->num_elements = 0;
    l->array.ptr = NULL;

    *new_array = l;

    return 0;
}

void harp_sized_array_delete(harp_sized_array *l)
{
    free(l->array.ptr);
    free(l);
}

int harp_sized_array_add_string(harp_sized_array *harp_sized_array, const char *str)
{
    if (harp_sized_array->num_elements % BLOCK_SIZE == 0)
    {
        char **string_data;
        int new_num = (harp_sized_array->num_elements + BLOCK_SIZE);

        string_data = (char **)realloc(harp_sized_array->array.string_data, new_num * sizeof(char *));
        if (string_data == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
            new_num * sizeof(char *), __FILE__, __LINE__);
            return -1;
        }

        harp_sized_array->array.string_data = string_data;
    }

    harp_sized_array->array.string_data[harp_sized_array->num_elements] = strdup(str);
    harp_sized_array->num_elements++;

    return 0;
}

int harp_sized_array_add_double(harp_sized_array *harp_sized_array, const double d)
{
    if (harp_sized_array->num_elements % BLOCK_SIZE == 0)
    {
        double *double_data;
        int new_num = (harp_sized_array->num_elements + BLOCK_SIZE);

        double_data = (double *)realloc(harp_sized_array->array.double_data, new_num * sizeof(double));
        if (double_data == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
            new_num * sizeof(double), __FILE__, __LINE__);
            return -1;
        }

        harp_sized_array->array.double_data = double_data;
    }

    harp_sized_array->array.double_data[harp_sized_array->num_elements] = d;
    harp_sized_array->num_elements++;

    return 0;
}
