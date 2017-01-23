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

#ifndef HARP_FILTER_H
#define HARP_FILTER_H

#include "harp-internal.h"
#include "harp-operation.h"
#include "harp-predicate.h"

int harp_predicate_update_mask_0d(const harp_predicate *predicate, const harp_variable *variable,
                                  uint8_t *product_mask);
int harp_predicate_update_mask_1d(const harp_predicate *predicate, const harp_variable *variable,
                                  harp_dimension_mask *dimension_mask);
int harp_predicate_update_mask_2d(const harp_predicate *predicate, const harp_variable *variable,
                                  harp_dimension_mask *primary_dimension_mask,
                                  harp_dimension_mask *secondary_dimension_mask);
int harp_point_predicate_update_mask_0d(int num_predicates, harp_predicate **predicate, const harp_variable *latitude,
                                        const harp_variable *longitude, uint8_t *product_mask);
int harp_point_predicate_update_mask_1d(int num_predicates, harp_predicate **predicate, const harp_variable *latitude,
                                        const harp_variable *longitude, harp_dimension_mask *dimension_mask);
int harp_area_predicate_update_mask_0d(int num_predicates, harp_predicate **predicate,
                                       const harp_variable *latitude_bounds, const harp_variable *longitude_bounds,
                                       uint8_t *product_mask);
int harp_area_predicate_update_mask_1d(int num_predicates, harp_predicate **predicate,
                                       const harp_variable *latitude_bounds, const harp_variable *longitude_bounds,
                                       harp_dimension_mask *dimension_mask);

int harp_comparison_filter_predicate_new(const harp_comparison_filter_args *args, harp_data_type data_type,
                                         const char *unit, harp_predicate **new_predicate);
int harp_string_comparison_filter_predicate_new(const harp_string_comparison_filter_args *args,
                                                harp_data_type data_type, harp_predicate **new_predicate);
int harp_bit_mask_filter_predicate_new(const harp_bit_mask_filter_args *args, harp_data_type data_type,
                                       harp_predicate **new_predicate);
int harp_membership_filter_predicate_new(const harp_membership_filter_args *args, harp_data_type data_type,
                                         const char *unit, harp_predicate **new_predicate);
int harp_string_membership_filter_predicate_new(const harp_string_membership_filter_args *args,
                                                harp_data_type data_type, harp_predicate **new_predicate);
int harp_valid_range_filter_predicate_new(harp_data_type data_type, harp_scalar valid_min, harp_scalar valid_max,
                                          harp_predicate **new_predicate);
int harp_longitude_range_filter_predicate_new(const harp_longitude_range_filter_args *args, harp_data_type data_type,
                                              const char *unit, harp_predicate **new_predicate);
int harp_collocation_filter_predicate_new(const harp_collocation_result *collocation_result, const char *source_product,
                                          harp_collocation_filter_type filter_type, int use_collocation_index,
                                          harp_predicate **new_predicate);
int harp_get_filter_predicate_for_operation(const harp_operation *operation, harp_data_type data_type, const char *unit,
                                            harp_scalar valid_min, harp_scalar valid_max,
                                            harp_predicate **new_predicate);
int harp_point_distance_filter_predicate_new(const harp_point_distance_filter_args *args,
                                             harp_predicate **new_predicate);
int harp_point_in_area_filter_predicate_new(const harp_point_in_area_filter_args *args, harp_predicate **new_predicate);
int harp_area_mask_covers_point_filter_predicate_new(const harp_area_mask_covers_point_filter_args *args,
                                                     harp_predicate **new_predicate);
int harp_area_mask_covers_area_filter_predicate_new(const harp_area_mask_covers_area_filter_args *args,
                                                    harp_predicate **new_predicate);
int harp_area_mask_intersects_area_filter_predicate_new(const harp_area_mask_intersects_area_filter_args *args,
                                                        harp_predicate **new_predicate);

void harp_array_filter(harp_data_type data_type, int num_dimensions,
                       const long *source_dimension, const uint8_t **source_mask, harp_array source,
                       const long *target_dimension, harp_array target);

int harp_variable_filter(harp_variable *variable, const harp_dimension_mask_set *dimension_mask_set);

int harp_product_filter(harp_product *product, const harp_dimension_mask_set *dimension_mask_set);

#endif
