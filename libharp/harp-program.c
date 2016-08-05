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
#include "harp-action-parse.h"
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

    program->num_actions = 0;
    program->action = NULL;

    *new_program = program;
    return 0;
}

void harp_program_delete(harp_program *program)
{
    if (program != NULL)
    {
        if (program->action != NULL)
        {
            int i;

            for (i = 0; i < program->num_actions; i++)
            {
                harp_action_delete(program->action[i]);
            }

            free(program->action);
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

    for (i = 0; i < other_program->num_actions; i++)
    {
        harp_action *action;

        if (harp_action_copy(other_program->action[i], &action) != 0)
        {
            harp_program_delete(program);
            return -1;
        }

        if (harp_program_add_action(program, action) != 0)
        {
            harp_program_delete(program);
            return -1;
        }
    }

    *new_program = program;
    return 0;
}

int harp_program_add_action(harp_program *program, harp_action *action)
{
    if (program->num_actions % BLOCK_SIZE == 0)
    {
        harp_action **action;

        action = (harp_action **)realloc(program->action, (program->num_actions + BLOCK_SIZE) * sizeof(harp_action *));
        if (action == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           (program->num_actions + BLOCK_SIZE) * sizeof(harp_action *), __FILE__, __LINE__);
            return -1;
        }

        program->action = action;
    }

    program->action[program->num_actions] = action;
    program->num_actions++;

    return 0;
}

int harp_program_remove_action_at_index(harp_program *program, int index)
{
    int i;

    assert(program != NULL);
    assert(index >= 0 && index < program->num_actions);

    harp_action_delete(program->action[index]);
    for (i = index + 1; i < program->num_actions; i++)
    {
        program->action[i - 1] = program->action[i];
    }
    program->num_actions--;

    return 0;
}

int harp_program_remove_action(harp_program *program, harp_action *action)
{
    int i;

    for (i = 0; i < program->num_actions; i++)
    {
        if (program->action[i] == action)
        {
            harp_program_remove_action_at_index(program, i);
        }
    }

    return 0;
}

int harp_program_verify(const harp_program *program)
{
    int i;
    int count;

    count = 0;
    for (i = 0; i < program->num_actions; i++)
    {
        const harp_action *action = program->action[i];

        if (action->type != harp_action_filter_collocation)
        {
            continue;
        }

        if (count > 0)
        {
            harp_set_error(HARP_ERROR_ACTION, "program should not contain more than one collocation filter");
            return -1;
        }

        count++;
    }

    return 0;
}
