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
