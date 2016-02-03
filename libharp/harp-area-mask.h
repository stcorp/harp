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
int harp_area_mask_intersects_area(const harp_area_mask *area_mask, const harp_spherical_polygon *area,
                                   double min_percentage);

int harp_area_mask_read(const char *path, harp_area_mask **new_area_mask);
#endif
