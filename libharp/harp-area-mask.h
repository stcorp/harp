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

#ifndef HARP_AREA_MASK_H
#define HARP_AREA_MASK_H

#include "harp-geometry.h"

typedef struct harp_area_mask_struct
{
    long num_polygons;
    harp_spherical_polygon **polygon;
} harp_area_mask;

int harp_area_mask_new(harp_area_mask **new_area_mask);
void harp_area_mask_delete(harp_area_mask *area_mask);
int harp_area_mask_add_polygon(harp_area_mask *area_mask, harp_spherical_polygon *polygon);

int harp_area_mask_covers_point(const harp_area_mask *area_mask, const harp_spherical_point *point);
int harp_area_mask_covers_area(const harp_area_mask *area_mask, const harp_spherical_polygon *area);
int harp_area_mask_inside_area(const harp_area_mask *area_mask, const harp_spherical_polygon *area);
int harp_area_mask_intersects_area(const harp_area_mask *area_mask, const harp_spherical_polygon *area);
int harp_area_mask_intersects_area_with_fraction(const harp_area_mask *area_mask, const harp_spherical_polygon *area,
                                                 double min_fraction);

int harp_area_mask_read(const char *path, harp_area_mask **new_area_mask);
#endif
