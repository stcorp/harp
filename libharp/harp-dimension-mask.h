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

#include "harp-internal.h"

#ifndef HARP_DIMENSION_MASK_H
#define HARP_DIMENSION_MASK_H

/** Maximum number of dimensions of a dimension mask. */
#define HARP_MAX_MASK_NUM_DIMS  2

typedef struct harp_dimension_mask_struct
{
    int num_dimensions;
    long dimension[HARP_MAX_MASK_NUM_DIMS];
    long num_elements;
    long masked_dimension_length;
    uint8_t *mask;
} harp_dimension_mask;

typedef harp_dimension_mask *harp_dimension_mask_set;

int harp_dimension_mask_new(int num_dimensions, const long *dimension, harp_dimension_mask **new_dimension_mask);
void harp_dimension_mask_delete(harp_dimension_mask *dimension_mask);
int harp_dimension_mask_copy(const harp_dimension_mask *other_dimension_mask, harp_dimension_mask **new_dimension_mask);

int harp_dimension_mask_set_new(harp_dimension_mask_set **new_dimension_mask_set);
void harp_dimension_mask_set_delete(harp_dimension_mask_set *dimension_mask_set);

int harp_dimension_mask_fill_true(harp_dimension_mask *dimension_mask);
int harp_dimension_mask_fill_false(harp_dimension_mask *dimension_mask);

int harp_dimension_mask_update_masked_length(harp_dimension_mask *dimension_mask);
int harp_dimension_mask_outer_product(const harp_dimension_mask *row_mask, const harp_dimension_mask *col_mask,
                                      harp_dimension_mask **new_dimension_mask);
int harp_dimension_mask_prepend_dimension(harp_dimension_mask *dimension_mask, long length);
int harp_dimension_mask_append_dimension(harp_dimension_mask *dimension_mask, long length);
int harp_dimension_mask_reduce(const harp_dimension_mask *dimension_mask, int dim_index,
                               harp_dimension_mask **new_dimension_mask);
int harp_dimension_mask_merge(const harp_dimension_mask *dimension_mask, int dim_index,
                              harp_dimension_mask *merged_dimension_mask);
int harp_dimension_mask_set_simplify(harp_dimension_mask_set *dimension_mask_set);

#endif
