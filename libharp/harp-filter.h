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

#ifndef HARP_FILTER_H
#define HARP_FILTER_H

#include "harp-internal.h"
#include "harp-operation.h"
#include "harp-predicate.h"

int harp_predicate_update_mask_all_0d(const harp_predicate *predicate, const harp_variable *variable,
                                      uint8_t *product_mask);
int harp_predicate_update_mask_all_1d(const harp_predicate *predicate, const harp_variable *variable,
                                      harp_dimension_mask *dimension_mask);
int harp_predicate_update_mask_all_2d(const harp_predicate *predicate, const harp_variable *variable,
                                      harp_dimension_mask *primary_dimension_mask,
                                      harp_dimension_mask *secondary_dimension_mask);
int harp_predicate_update_mask_any(const harp_predicate *predicate, const harp_variable *variable,
                                   harp_dimension_mask *dimension_mask);
int harp_point_predicate_update_mask_all_0d(int num_predicates, harp_predicate **predicate,
                                            const harp_variable *longitude, const harp_variable *latitude,
                                            uint8_t *product_mask);
int harp_point_predicate_update_mask_all_1d(int num_predicates, harp_predicate **predicate,
                                            const harp_variable *longitude, const harp_variable *latitude,
                                            harp_dimension_mask *dimension_mask);
int harp_area_predicate_update_mask_all_0d(int num_predicates, harp_predicate **predicate,
                                           const harp_variable *latitude_bounds, const harp_variable *longitude_bounds,
                                           uint8_t *product_mask);
int harp_area_predicate_update_mask_all_1d(int num_predicates, harp_predicate **predicate,
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
