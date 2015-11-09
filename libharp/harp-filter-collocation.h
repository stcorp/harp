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

#ifndef HARP_FILTER_COLLOCATION_H
#define HARP_FILTER_COLLOCATION_H

#include "harp-internal.h"

/* TODO: Move collocation_type from harp-operation.h to this file, optionally rename it, and reverse the dependency. */
#include "harp-action.h"
#include "harp-collocation.h"

typedef struct harp_collocation_index_pair_struct
{
    long collocation_index;
    long index;
} harp_collocation_index_pair;

typedef struct harp_collocation_mask_struct
{
    long num_index_pairs;
    harp_collocation_index_pair **index_pair;
} harp_collocation_mask;

int harp_collocation_index_pair_new(long collocation_index, long index, harp_collocation_index_pair **new_index_pair);
void harp_collocation_index_pair_delete(harp_collocation_index_pair *index_pair);

int harp_collocation_mask_new(harp_collocation_mask **new_mask);
void harp_collocation_mask_delete(harp_collocation_mask *mask);
int harp_collocation_mask_add_index_pair(harp_collocation_mask *mask, harp_collocation_index_pair *index_pair);
void harp_collocation_mask_sort_by_index(harp_collocation_mask *mask);
void harp_collocation_mask_sort_by_collocation_index(harp_collocation_mask *mask);
int harp_collocation_mask_import(const char *filename, harp_collocation_filter_type filter_type,
                                 const char *original_filename, harp_collocation_mask **new_mask);

int harp_filter_index(const harp_variable *index, harp_collocation_mask *collocation_mask,
                      harp_dimension_mask *dimension_mask);
int harp_filter_collocation_index(const harp_variable *collocation_index, harp_collocation_mask *collocation_mask,
                                  harp_dimension_mask *dimension_mask);
int harp_product_apply_collocation_mask(harp_collocation_mask *collocation_mask, harp_product *product);

#endif
