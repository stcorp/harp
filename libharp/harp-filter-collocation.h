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

#ifndef HARP_FILTER_COLLOCATION_H
#define HARP_FILTER_COLLOCATION_H

#include "harp-internal.h"
#include "harp-operation.h"

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
int harp_collocation_mask_from_result(const harp_collocation_result *collocation_result,
                                      harp_collocation_filter_type filter_type, const char *source_product,
                                      harp_collocation_mask **new_mask);
int harp_collocation_mask_import(const char *filename, harp_collocation_filter_type filter_type,
                                 const char *original_filename, harp_collocation_mask **new_mask);

int harp_filter_index(const harp_variable *index, harp_collocation_mask *collocation_mask,
                      harp_dimension_mask *dimension_mask);
int harp_filter_collocation_index(const harp_variable *collocation_index, harp_collocation_mask *collocation_mask,
                                  harp_dimension_mask *dimension_mask);
int harp_product_apply_collocation_mask(harp_collocation_mask *collocation_mask, harp_product *product);

#endif
