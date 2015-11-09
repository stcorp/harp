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

#include "harpcollocate.h"

/* Check if the pairs are neighbours (i.e. have the same measurement index for the master dataset) */
static int pair_is_neighbour(const harp_collocation_pair *first_pair, const harp_collocation_pair *second_pair,
                             int master_a)
{
    if (master_a)
    {
        /* Use source_product_a and index_a. */
        if (strcmp(second_pair->source_product_a, first_pair->source_product_a) == 0
            && second_pair->index_a == first_pair->index_a)
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
        if (strcmp(second_pair->source_product_b, first_pair->source_product_b) == 0
            && second_pair->index_b == first_pair->index_b)
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
    for (i = target_id + 1; i < collocation_result->num_pairs; i++)
    {
        harp_collocation_pair_delete(collocation_result->pair[i]);
    }
    collocation_result->num_pairs = target_id + 1;

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
