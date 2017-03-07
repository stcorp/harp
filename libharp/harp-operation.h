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

#ifndef HARP_OPERATION_H
#define HARP_OPERATION_H

#include "harp-internal.h"

typedef enum harp_operation_type_enum
{
    harp_operation_area_mask_covers_area_filter,
    harp_operation_area_mask_covers_point_filter,
    harp_operation_area_mask_intersects_area_filter,
    harp_operation_bit_mask_filter,
    harp_operation_collocation_filter,
    harp_operation_comparison_filter,
    harp_operation_derive_variable,
    harp_operation_derive_smoothed_column_collocated,
    harp_operation_exclude_variable,
    harp_operation_flatten,
    harp_operation_keep_variable,
    harp_operation_longitude_range_filter,
    harp_operation_membership_filter,
    harp_operation_point_distance_filter,
    harp_operation_point_in_area_filter,
    harp_operation_regrid,
    harp_operation_regrid_collocated,
    harp_operation_smooth_collocated,
    harp_operation_string_comparison_filter,
    harp_operation_string_membership_filter,
    harp_operation_valid_range_filter,
    harp_operation_wrap,
} harp_operation_type;

typedef struct harp_operation_struct
{
    harp_operation_type type;
    void *args;
} harp_operation;

typedef enum harp_collocation_filter_type_enum
{
    harp_collocation_left,
    harp_collocation_right
} harp_collocation_filter_type;

typedef enum harp_comparison_operator_type_enum
{
    harp_operator_eq,
    harp_operator_ne,
    harp_operator_lt,
    harp_operator_le,
    harp_operator_gt,
    harp_operator_ge
} harp_comparison_operator_type;

typedef enum harp_bit_mask_operator_type_enum
{
    harp_operator_bit_mask_any,
    harp_operator_bit_mask_none
} harp_bit_mask_operator_type;

typedef enum harp_membership_operator_type_enum
{
    harp_operator_in,
    harp_operator_not_in
} harp_membership_operator_type;

typedef struct harp_area_mask_covers_area_filter_args_struct
{
    char *filename;
} harp_area_mask_covers_area_filter_args;

typedef struct harp_area_mask_covers_point_filter_args_struct
{
    char *filename;
} harp_area_mask_covers_point_filter_args;

typedef struct harp_area_mask_intersects_area_filter_args_struct
{
    char *filename;
    double min_percentage;
} harp_area_mask_intersects_area_filter_args;

typedef struct harp_bit_mask_filter_args_struct
{
    char *variable_name;
    harp_bit_mask_operator_type operator_type;
    uint32_t bit_mask;
} harp_bit_mask_filter_args;

typedef struct harp_collocation_filter_args_struct
{
    char *filename;
    harp_collocation_filter_type filter_type;
} harp_collocation_filter_args;

typedef struct harp_comparison_filter_args_struct
{
    char *variable_name;
    harp_comparison_operator_type operator_type;
    double value;
    char *unit;
} harp_comparison_filter_args;

typedef struct harp_derive_variable_args_struct
{
    char *variable_name;
    int num_dimensions;
    harp_dimension_type dimension_type[HARP_MAX_NUM_DIMS];
    char *unit;
} harp_derive_variable_args;

typedef struct harp_derive_smoothed_column_collocated_args_struct
{
    char *variable_name;
    int num_dimensions;
    harp_dimension_type dimension_type[HARP_MAX_NUM_DIMS];
    char *unit;
    char *axis_variable_name;
    char *axis_unit;
    char *collocation_result;
    char target_dataset;
    char *dataset_dir;
} harp_derive_smoothed_column_collocated_args;

typedef struct harp_exclude_variable_args_struct
{
    int num_variables;
    char **variable_name;
} harp_exclude_variable_args;

typedef struct harp_flatten_args_struct
{
    harp_dimension_type dimension_type;
} harp_flatten_args;

typedef struct harp_keep_variable_args_struct
{
    int num_variables;
    char **variable_name;
} harp_keep_variable_args;

typedef struct harp_longitude_range_filter_args_struct
{
    double min;
    char *min_unit;
    double max;
    char *max_unit;
} harp_longitude_range_filter_args;

typedef struct harp_membership_filter_args_struct
{
    char *variable_name;
    harp_membership_operator_type operator_type;
    int num_values;
    double *value;
    char *unit;
} harp_membership_filter_args;

typedef struct harp_point_distance_filter_args_struct
{
    double latitude;
    char *latitude_unit;
    double longitude;
    char *longitude_unit;
    double distance;
    char *distance_unit;
} harp_point_distance_filter_args;

typedef struct harp_point_in_area_filter_args_struct
{
    double latitude;
    char *latitude_unit;
    double longitude;
    char *longitude_unit;
} harp_point_in_area_filter_args;

typedef struct harp_regrid_args_struct
{
    harp_variable *axis_variable;
} harp_regrid_args;

typedef struct harp_regrid_collocated_args_struct
{
    harp_dimension_type dimension_type;
    char *axis_variable_name;
    char *axis_unit;
    char *collocation_result;
    char target_dataset;
    char *dataset_dir;
} harp_regrid_collocated_args;

typedef struct harp_smooth_collocated_args_struct
{
    int num_variables;
    char **variable_name;
    harp_dimension_type dimension_type;
    char *axis_variable_name;
    char *axis_unit;
    char *collocation_result;
    char target_dataset;
    char *dataset_dir;
} harp_smooth_collocated_args;

typedef struct harp_string_comparison_filter_args_struct
{
    char *variable_name;
    harp_comparison_operator_type operator_type;
    char *value;
} harp_string_comparison_filter_args;

typedef struct harp_string_membership_filter_args_struct
{
    char *variable_name;
    harp_membership_operator_type operator_type;
    int num_values;
    char **value;
} harp_string_membership_filter_args;

typedef struct harp_valid_range_filter_args_struct
{
    char *variable_name;
} harp_valid_range_filter_args;

typedef struct harp_wrap_args_struct
{
    char *variable_name;
    char *unit;
    double min;
    double max;
} harp_wrap_args;

/* Generic operation */
int harp_operation_new(harp_operation_type type, void *args, harp_operation **new_operation);
void harp_operation_delete(harp_operation *operation);
int harp_operation_copy(const harp_operation *other_operation, harp_operation **new_operation);
int harp_operation_get_variable_name(const harp_operation *operation, const char **variable_name);
int harp_operation_is_dimension_filter(const harp_operation *operation);

/* Specific operations */
int harp_area_mask_covers_area_filter_new(const char *filename, harp_operation **new_operation);
int harp_area_mask_covers_point_filter_new(const char *filename, harp_operation **new_operation);
int harp_area_mask_intersects_area_filter_new(const char *filename, double min_percentage,
                                              harp_operation **new_operation);
int harp_bit_mask_filter_new(const char *variable_name, harp_bit_mask_operator_type operator_type, uint32_t bit_mask,
                             harp_operation **new_operation);
int harp_collocation_filter_new(const char *filename, harp_collocation_filter_type filter_type,
                                harp_operation **new_operation);
int harp_comparison_filter_new(const char *variable_name, harp_comparison_operator_type operator_type, double value,
                               const char *unit, harp_operation **new_operation);
int harp_derive_variable_new(const char *variable_name, int num_dimensions, const harp_dimension_type *dimension_type,
                             const char *unit, harp_operation **new_operation);
int harp_derive_smoothed_column_collocated_new(const char *variable_name, int num_dimensions,
                                               const harp_dimension_type *dimension_type, const char *unit,
                                               const char *axis_variable_name, const char *axis_unit,
                                               const char *collocation_result, const char target_dataset,
                                               const char *dataset_dir, harp_operation **new_operation);
int harp_exclude_variable_new(int num_variables, const char **variable_name, harp_operation **new_operation);
int harp_flatten_new(const harp_dimension_type dimension_type, harp_operation **new_operation);
int harp_keep_variable_new(int num_variables, const char **variable_name, harp_operation **new_operation);
int harp_longitude_range_filter_new(double min, const char *min_unit, double max, const char *max_unit,
                                    harp_operation **new_operation);
int harp_membership_filter_new(const char *variable_name, harp_membership_operator_type operator_type, int num_values,
                               const double *value, const char *unit, harp_operation **new_operation);
int harp_point_distance_filter_new(double latitude, const char *latitude_unit, double longitude,
                                   const char *longitude_unit, double distance, const char *distance_unit,
                                   harp_operation **operation);
int harp_point_in_area_filter_new(double latitude, const char *latitude_unit, double longitude,
                                  const char *longitude_unit, harp_operation **operation);
int harp_regrid_new(harp_dimension_type dimension_type, const char *axis_variable_name, const char *axis_unit,
                    long num_values, double *values, harp_operation **new_operation);
int harp_regrid_collocated_new(harp_dimension_type dimension_type, const char *axis_variable_name,
                               const char *axis_unit, const char *collocation_result, const char target_dataset,
                               const char *dataset_dir, harp_operation **new_operation);
int harp_smooth_collocated_new(int num_variables, const char **variable_name, harp_dimension_type dimension_type,
                               const char *axis_variable_name, const char *axis_unit, const char *collocation_result,
                               const char target_dataset, const char *dataset_dir, harp_operation **new_operation);
int harp_string_comparison_filter_new(const char *variable_name, harp_comparison_operator_type operator_type,
                                      const char *value, harp_operation **new_operation);
int harp_string_membership_filter_new(const char *variable_name, harp_membership_operator_type operator_type,
                                      int num_values, const char **value, harp_operation **new_operation);
int harp_valid_range_filter_new(const char *variable_name, harp_operation **new_operation);
int harp_wrap_new(const char *variable_name, const char *unit, double min, double max, harp_operation **new_operation);

#endif
