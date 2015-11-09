/*
 * Copyright (C) 2015 S[&]T, The Netherlands.
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

#include "harp-internal.h"
#include "harp-predicate.h"

#include <assert.h>
#include <stdlib.h>

int harp_predicate_new(harp_predicate_eval_func * eval_func, void *args, harp_predicate_delete_func * delete_func,
                       harp_predicate **new_predicate)
{
    harp_predicate *predicate;

    assert(eval_func != NULL);

    predicate = (harp_predicate *)malloc(sizeof(harp_predicate));
    if (predicate == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_predicate), __FILE__, __LINE__);
        return -1;
    }

    predicate->eval = eval_func;
    predicate->args = args;
    predicate->delete = delete_func;

    *new_predicate = predicate;
    return 0;
}

void harp_predicate_delete(harp_predicate *predicate)
{
    if (predicate != NULL)
    {
        if (predicate->args != NULL)
        {
            if (predicate->delete != NULL)
            {
                predicate->delete(predicate->args);
            }
            else
            {
                free(predicate->args);
            }
        }

        free(predicate);
    }
}

void harp_predicate_set_delete(harp_predicate_set *predicate_set)
{
    if (predicate_set != NULL)
    {
        if (predicate_set->predicate != NULL)
        {
            int i;

            for (i = 0; i < predicate_set->num_predicates; i++)
            {
                harp_predicate_delete(predicate_set->predicate[i]);
            }

            free(predicate_set->predicate);
        }

        free(predicate_set);
    }
}

int harp_predicate_set_new(harp_predicate_set **new_predicate_set)
{
    harp_predicate_set *predicate_set;

    predicate_set = (harp_predicate_set *)malloc(sizeof(harp_predicate_set));
    if (predicate_set == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_predicate_set), __FILE__, __LINE__);
        return -1;
    }

    predicate_set->num_predicates = 0;
    predicate_set->predicate = NULL;

    *new_predicate_set = predicate_set;
    return 0;
}

int harp_predicate_set_add_predicate(harp_predicate_set *predicate_set, harp_predicate *predicate)
{
    if (predicate_set->num_predicates % BLOCK_SIZE == 0)
    {
        int i;
        harp_predicate **predicate;

        predicate = realloc(predicate_set->predicate, (predicate_set->num_predicates + BLOCK_SIZE)
                            * sizeof(harp_predicate *));
        if (predicate == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           (long)(predicate_set->num_predicates + BLOCK_SIZE) * sizeof(harp_predicate *), __FILE__,
                           __LINE__);
            return -1;
        }

        predicate_set->predicate = predicate;
        for (i = predicate_set->num_predicates; i < predicate_set->num_predicates + BLOCK_SIZE; i++)
        {
            predicate_set->predicate[i] = NULL;
        }
    }

    predicate_set->predicate[predicate_set->num_predicates] = predicate;
    predicate_set->num_predicates++;
    return 0;
}
