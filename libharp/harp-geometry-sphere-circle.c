/*
 * Copyright (C) 2015-2018 S[&]T, The Netherlands.
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

#include "harp-geometry.h"

#include <math.h>

/* Checks whether two circles are equal */
int harp_spherical_circle_equal(const harp_spherical_circle *circle1, const harp_spherical_circle *circle2)
{
    return (harp_spherical_point_equal(&circle1->center, &circle2->center) &&
            HARP_GEOMETRY_FPeq(circle1->radius, circle2->radius));
}

/* Checks whether circle contains point */
int harp_spherical_circle_contains_point(const harp_spherical_point *point, const harp_spherical_circle *circle)
{
    double distance = harp_spherical_point_distance(point, &circle->center);

    if (HARP_GEOMETRY_FPle(distance, circle->radius))
    {
        return 1;       /* True */
    }
    return 0;   /* False */
}

/* Transform a circle using an Euler transformation */
void harp_spherical_circle_apply_euler_transformation(harp_spherical_circle *circle_out,
                                                      const harp_spherical_circle *circle_in,
                                                      const harp_euler_transformation *transformation)
{
    harp_spherical_point_apply_euler_transformation(&circle_out->center, &circle_in->center, transformation);
    circle_out->radius = circle_in->radius;
    harp_spherical_point_check(&circle_out->center);
}
