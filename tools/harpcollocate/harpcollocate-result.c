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

#include "harpcollocate.h"

static const char *collocation_unit_from_difference_type(const harp_collocation_difference_type difference_type)
{
    switch (difference_type)
    {
        case harp_collocation_difference_absolute_time:
            return HARP_UNIT_TIME;

        case harp_collocation_difference_absolute_latitude:
            return HARP_UNIT_LATITUDE;

        case harp_collocation_difference_absolute_longitude:
            return HARP_UNIT_LONGITUDE;

        case harp_collocation_difference_point_distance:
            return HARP_UNIT_LENGTH;

        case harp_collocation_difference_overlapping_percentage:
            return HARP_UNIT_PERCENT;

        case harp_collocation_difference_absolute_sza:
        case harp_collocation_difference_absolute_saa:
        case harp_collocation_difference_absolute_vza:
        case harp_collocation_difference_absolute_vaa:
        case harp_collocation_difference_absolute_theta:
            return HARP_UNIT_ANGLE;

        case harp_collocation_difference_unknown:
        case harp_collocation_difference_delta:
            break;
    }
    return "";
}

int collocation_result_convert_units(harp_collocation_result *collocation_result)
{
    int k;

    for (k = 0; k < HARP_COLLOCATION_RESULT_MAX_NUM_DIFFERENCES; k++)
    {
        /* For each column k that is used ... */
        if (collocation_result->difference_available[k])
        {
            long i;

            /* Put all values into a temporary array */
            double *difference_value = malloc((size_t)collocation_result->num_pairs * sizeof(double));

            for (i = 0; i < collocation_result->num_pairs; i++)
            {
                difference_value[i] = collocation_result->pair[i]->difference[k];
            }

            /* and convert them from the collocation unit to the original unit (set by the user) */
            if (harp_convert_unit(collocation_unit_from_difference_type(k), collocation_result->difference_unit[k],
                                  collocation_result->num_pairs, difference_value) != 0)
            {
                free(difference_value);
                return -1;
            }

            /* Update the column */
            for (i = 0; i < collocation_result->num_pairs; i++)
            {
                collocation_result->pair[i]->difference[k] = difference_value[i];
            }

            free(difference_value);
        }
    }

    return 0;
}
