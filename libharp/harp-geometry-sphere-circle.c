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

#include "harp-geometry.h"

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
