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
