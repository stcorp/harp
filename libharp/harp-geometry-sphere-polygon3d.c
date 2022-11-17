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

#include <stddef.h>
#include <stdlib.h>

int harp_spherical_polygon3d_new(int32_t numberofpoints, harp_spherical_polygon3d **polygon)
{
    size_t size = offsetof(harp_spherical_polygon3d, point) + sizeof(harp_vector3d) * numberofpoints;

    *polygon = (harp_spherical_polygon3d *)malloc(size);
    if (*polygon == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)\n", size,
                       __FILE__, __LINE__);
        return -1;
    }
    (*polygon)->size = (int)size;
    (*polygon)->numberofpoints = numberofpoints;

    return 0;
}

void harp_spherical_polygon3d_delete(harp_spherical_polygon3d *polygon)
{
    free(polygon);
}

int harp_spherical_polygon3d_from_spherical_polygon(harp_spherical_polygon3d **polygonout, const harp_spherical_polygon *polygonin)
{
    int32_t i;

    if (harp_spherical_polygon3d_new(polygonin->numberofpoints, polygonout) != 0)
    {
        return -1;
    }

    for (i = 0; i < polygonin->numberofpoints; i++) {
        harp_vector3d_from_spherical_point(&((*polygonout)->point[i]), &(polygonin->point[i]));
    }

    return 0;
}

int harp_spherical_polygon3d_segment(harp_spherical_line3d *line, const harp_spherical_polygon3d *polygon, int32_t i)
{
    /* First, make sure that the index is valid */
    if (i >= 0 && i < polygon->numberofpoints)
    {
        if (i == (polygon->numberofpoints - 1))
        {
            /* We are dealing with index of last point; derive the line segment
             * connecting last point with first point of polygon */
            line->begin = polygon->point[i];
            line->end = polygon->point[0];
        }
        else
        {
            /* Derive the line segment connecting the current point with the nex point */
            line->begin = polygon->point[i];
            line->end = polygon->point[i + 1];
        }

        return 0;
    }
    else
    {
        /* The index is outside the valid range */
        return -1;
    }
}
