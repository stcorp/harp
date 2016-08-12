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

#include "harp-program.h"
#include "harp-operation-parse.h"
#include "harp-filter-collocation.h"
#include "harp-filter.h"

#include <assert.h>
#include <stdlib.h>

int harp_program_new(harp_program **new_program)
{
    harp_program *program;

    program = (harp_program *)malloc(sizeof(harp_program));
    if (program == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_program), __FILE__, __LINE__);
        return -1;
    }

    program->num_operations = 0;
    program->operation = NULL;

    *new_program = program;
    return 0;
}

void harp_program_delete(harp_program *program)
{
    if (program != NULL)
    {
        if (program->operation != NULL)
        {
            int i;

            for (i = 0; i < program->num_operations; i++)
            {
                harp_operation_delete(program->operation[i]);
            }

            free(program->operation);
        }

        free(program);
    }
}

int harp_program_copy(const harp_program *other_program, harp_program **new_program)
{
    harp_program *program;
    int i;

    if (harp_program_new(&program) != 0)
    {
        return -1;
    }

    for (i = 0; i < other_program->num_operations; i++)
    {
        harp_operation *operation;

        if (harp_operation_copy(other_program->operation[i], &operation) != 0)
        {
            harp_program_delete(program);
            return -1;
        }

        if (harp_program_add_operation(program, operation) != 0)
        {
            harp_program_delete(program);
            return -1;
        }
    }

    *new_program = program;
    return 0;
}

int harp_program_add_operation(harp_program *program, harp_operation *operation)
{
    if (program->num_operations % BLOCK_SIZE == 0)
    {
        harp_operation **operation;

        operation =
            (harp_operation **)realloc(program->operation,
                                       (program->num_operations + BLOCK_SIZE) * sizeof(harp_operation *));
        if (operation == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           (program->num_operations + BLOCK_SIZE) * sizeof(harp_operation *), __FILE__, __LINE__);
            return -1;
        }

        program->operation = operation;
    }

    program->operation[program->num_operations] = operation;
    program->num_operations++;

    return 0;
}

int harp_program_remove_operation_at_index(harp_program *program, int index)
{
    int i;

    assert(program != NULL);
    assert(index >= 0 && index < program->num_operations);

    harp_operation_delete(program->operation[index]);
    for (i = index + 1; i < program->num_operations; i++)
    {
        program->operation[i - 1] = program->operation[i];
    }
    program->num_operations--;

    return 0;
}

int harp_program_remove_operation(harp_program *program, harp_operation *operation)
{
    int i;

    for (i = 0; i < program->num_operations; i++)
    {
        if (program->operation[i] == operation)
        {
            harp_program_remove_operation_at_index(program, i);
        }
    }

    return 0;
}
