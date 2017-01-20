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
