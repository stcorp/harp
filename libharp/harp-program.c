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

#include "harp-program.h"
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
