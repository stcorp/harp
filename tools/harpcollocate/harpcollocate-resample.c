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

#include "harpcollocate.h"

/* Check if the pairs are neighbours (i.e. have the same measurement index for the master dataset) */
static int pair_is_neighbour(const harp_collocation_pair *first_pair, const harp_collocation_pair *second_pair,
                             int master_a)
{
    if (master_a)
    {
        /* Use source_product_a and index_a. */
        if (second_pair->product_index_a == first_pair->product_index_a
            && second_pair->sample_index_a == first_pair->sample_index_a)
        {
            /* Yes, we found a neighbour. */
            return 1;
        }
        else
        {
            return 0;
        }
    }
    else
    {
        /* Use source_product_b and index_b. */
        if (second_pair->product_index_b == first_pair->product_index_b
            && second_pair->sample_index_b == first_pair->sample_index_b)
        {
            /* Yes, we found a neighbour. */
            return 1;
        }
        else
        {
            return 0;
        }
    }
    return 0;
}

static int nearest_neighbour(const Collocation_options *collocation_options,
                             harp_collocation_result *collocation_result, int master_a)
{
    long target_id = 0;
    long i;

    if (master_a)
    {
        if (harp_collocation_result_sort_by_a(collocation_result) != 0)
        {
            return -1;
        }
    }
    else
    {
        if (harp_collocation_result_sort_by_b(collocation_result) != 0)
        {
            return -1;
        }
    }


    for (i = 1; i < collocation_result->num_pairs; i++)
    {
        if (pair_is_neighbour(collocation_result->pair[target_id], collocation_result->pair[i], master_a))
        {
            double delta_a, delta_b;

            if (calculate_delta(collocation_result, collocation_options, collocation_result->pair[target_id],
                                &delta_a) != 0)
            {
                return -1;
            }
            if (calculate_delta(collocation_result, collocation_options, collocation_result->pair[i], &delta_b) != 0)
            {
                return -1;
            }
            if (delta_b < delta_a)
            {
                /* swap the pairs */
                harp_collocation_pair *pair;

                pair = collocation_result->pair[i];
                collocation_result->pair[i] = collocation_result->pair[target_id];
                collocation_result->pair[target_id] = pair;
            }
        }
        else
        {
            target_id++;
            if (target_id != i)
            {
                /* swap the pairs */
                harp_collocation_pair *pair;

                pair = collocation_result->pair[i];
                collocation_result->pair[i] = collocation_result->pair[target_id];
                collocation_result->pair[target_id] = pair;
            }
        }
    }
    while (collocation_result->num_pairs - 1 > target_id)
    {
        if (harp_collocation_result_remove_pair_at_index(collocation_result, collocation_result->num_pairs - 1) != 0)
        {
            return -1;
        }
    }

    return 0;
}

int resample(const Collocation_options *collocation_options, harp_collocation_result *collocation_result)
{
    /* No resampling required */
    if (collocation_result->num_pairs == 0)
    {
        return 0;
    }

    switch (collocation_options->resampling_method)
    {
        case resampling_method_none:
            /* No resampling required */
            break;
        case resampling_method_nearest_neighbour_a:

            if (nearest_neighbour(collocation_options, collocation_result, 1) != 0)
            {
                return -1;
            }

            break;
        case resampling_method_nearest_neighbour_b:

            if (nearest_neighbour(collocation_options, collocation_result, 0) != 0)
            {
                return -1;
            }

            break;
        case resampling_method_nearest_neighbour_ab:

            if (nearest_neighbour(collocation_options, collocation_result, 1) != 0)
            {
                return -1;
            }

            if (nearest_neighbour(collocation_options, collocation_result, 0) != 0)
            {
                return -1;
            }

            break;
        case resampling_method_nearest_neighbour_ba:

            if (nearest_neighbour(collocation_options, collocation_result, 0) != 0)
            {
                return -1;
            }

            if (nearest_neighbour(collocation_options, collocation_result, 1) != 0)
            {
                return -1;
            }

            break;
    }

    return 0;
}
