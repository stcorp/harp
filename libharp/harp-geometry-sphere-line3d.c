/*
 * Copyright (C) 2015-2022 S[&]T, The Netherlands.
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

void harp_spherical_line3d_normal(harp_vector3d *normalout, const harp_spherical_line3d *linein)
{
    harp_vector3d_crossproduct(normalout, &(linein->begin), &(linein->end));
}

int8_t harp_point_on_spherical_line3d(const harp_spherical_line3d *line, const harp_vector3d *point)
{
    double theta_begin_point, theta_end_point, theta_line;

    /* Get the angle theta of the line and points.
     * The dot product is defined by:
     *
     *   a . b = ||a|| * ||b|| * cos(theta)
     * 
     * if we re-arrange for theta we get
     *
     *   theta = acos((a . b) / (||a|| * ||b||))
     *
     * Since vectors are normalized we can simplify to:
     *
     *   theta = acos(a . b)
     */
    theta_begin_point = acos(harp_vector3d_dotproduct(&(line->begin), point));
    theta_end_point = acos(harp_vector3d_dotproduct(point, &(line->end)));
    theta_line = acos(harp_vector3d_dotproduct(&(line->begin), &(line->end)));

    /* If the angles from the start and end point of the line are equal to the
     * total angle of the line, then the point is on the line. */
    return HARP_GEOMETRY_FPeq(theta_begin_point + theta_end_point, theta_line);
}

int8_t harp_spherical_line3d_intersects(const harp_spherical_line3d *line1,
                                        const harp_spherical_line3d *line2)
{
    /* The idea is to get the two intersection points of the great circles of
     * the lines, i.e. intersect the planes of the lines. We perform this in 3D
     * space. Then check if one of the intersection points is within the
     * boundaries of both lines by comparing the angles. */
    harp_vector3d n1, n2, i1, i2;
    double norm;

    /* Compute normal of great circles, i.e. planes */
    harp_spherical_line3d_normal(&n1, line1);
    harp_spherical_line3d_normal(&n2, line2);

    /* Compute normal of normal of planes, this is equal to one intersection
     * point. */
    harp_vector3d_crossproduct(&i1, &n1, &n2);

    norm = harp_vector3d_norm(&i1);
    /* If the cross product is the zero vector then the lines are equal */
    if (norm == 0.0)
    {
        return 1;
    }
    else
    {
        /* Normalize intersection point */
        i1.x = i1.x / norm;
        i1.y = i1.y / norm;
        i1.z = i1.z / norm;

        /* The second intersection point is opposite of the first */
        i2.x = -i1.x;
        i2.y = -i1.y;
        i2.z = -i1.z;

        /* Return false, if an intersection point is equal to begin or end
         * point of a line */
        if (harp_vector3d_equal(&(line1->begin), &(line2->begin)) ||
            harp_vector3d_equal(&(line1->begin), &(line2->end)) ||
            harp_vector3d_equal(&(line1->end), &(line2->begin)) ||
            harp_vector3d_equal(&(line1->end), &(line2->end)))
        {
            return 0;
        }

        /* Check if the intersection points are within both original lines */
        return (harp_point_on_spherical_line3d(line1, &i1) && harp_point_on_spherical_line3d(line2, &i1)) ||
            (harp_point_on_spherical_line3d(line1, &i2) && harp_point_on_spherical_line3d(line2, &i2));
    }
}

