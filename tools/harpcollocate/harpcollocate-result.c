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
