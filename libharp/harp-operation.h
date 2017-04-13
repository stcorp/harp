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
#include "harp-area-mask.h"
#include "harp-filter-collocation.h"
#include "harp-geometry.h"

typedef enum harp_operation_type_enum
{
    operation_area_mask_covers_area_filter,
    operation_area_mask_covers_point_filter,
    operation_area_mask_intersects_area_filter,
    operation_bit_mask_filter,
    operation_collocation_filter,
    operation_comparison_filter,
    operation_derive_variable,
    operation_derive_smoothed_column_collocated,
    operation_exclude_variable,
    operation_flatten,
    operation_keep_variable,
    operation_longitude_range_filter,
    operation_membership_filter,
    operation_point_distance_filter,
    operation_point_in_area_filter,
    operation_regrid,
    operation_regrid_collocated,
    operation_rename,
    operation_smooth_collocated,
    operation_string_comparison_filter,
    operation_string_membership_filter,
    operation_valid_range_filter,
    operation_wrap,
} harp_operation_type;

typedef enum harp_comparison_operator_type_enum
{
    operator_eq,
    operator_ne,
    operator_lt,
    operator_le,
    operator_gt,
    operator_ge
} harp_comparison_operator_type;

typedef enum harp_bit_mask_operator_type_enum
{
    operator_bit_mask_any,
    operator_bit_mask_none
} harp_bit_mask_operator_type;

typedef enum harp_membership_operator_type_enum
{
    operator_in,
    operator_not_in
} harp_membership_operator_type;

/* We use a class hierarchy of structs
 * Each struct can be cast to its 'base struct'
 *
 * - harp_operation
 *   |- harp_operation_value_filter
 *   |  |-  harp_operation_bit_mask_filter
 *   |  |-  harp_operation_collocation_filter
 *   |  |-  harp_operation_comparison_filter
 *   |  |-  harp_operation_longitude_range_filter
 *   |  |-  harp_operation_membership_filter
 *   |  |-  harp_operation_string_comparison_filter
 *   |  |-  harp_operation_string_membership_filter
 *   |  |-  harp_operation_valid_range_filter
 *   |- harp_operation_point_filter
 *   |  |-  harp_operation_area_mask_covers_point_filter
 *   |  |-  harp_operation_point_distance_filter
 *   |- harp_operation_polygon_filter
 *   |  |-  harp_operation_area_mask_covers_area_filter
 *   |  |-  harp_operation_area_mask_intersects_area_filter
 *   |  |-  harp_operation_point_in_area_filter
 *   |-  harp_operation_derive_variable
 *   |-  harp_operation_derive_smoothed_column_collocated
 *   |-  harp_operation_exclude_variable
 *   |-  harp_operation_flatten
 *   |-  harp_operation_keep_variable
 *   |-  harp_operation_regrid
 *   |-  harp_operation_regrid_collocated
 *   |-  harp_operation_rename
 *   |-  harp_operation_smooth_collocated
 *   |-  harp_operation_wrap
 */

typedef struct harp_operation_struct
{
    harp_operation_type type;
} harp_operation;

typedef struct harp_operation_value_filter_struct
{
    harp_operation_type type;
    int (*eval) (struct harp_operation_value_filter_struct *, harp_data_type, void *);
} harp_operation_value_filter;

typedef struct harp_operation_point_filter_struct
{
    harp_operation_type type;
    int (*eval) (struct harp_operation_point_filter_struct *, harp_spherical_point *);
} harp_operation_point_filter;

typedef struct harp_operation_polygon_filter_struct
{
    harp_operation_type type;
    int (*eval) (struct harp_operation_polygon_filter_struct *, harp_spherical_polygon *);
} harp_operation_polygon_filter;

typedef struct harp_operation_area_mask_covers_area_filter_struct
{
    harp_operation_type type;
    int (*eval) (struct harp_operation_area_mask_covers_area_filter_struct *, harp_spherical_polygon *);
    /* parameters */
    char *filename;
    /* extra */
    harp_area_mask *area_mask;
} harp_operation_area_mask_covers_area_filter;

typedef struct harp_operation_area_mask_covers_point_filter_struct
{
    harp_operation_type type;
    int (*eval) (struct harp_operation_area_mask_covers_point_filter_struct *, harp_spherical_point *);
    /* parameters */
    char *filename;
    /* extra */
    harp_area_mask *area_mask;
} harp_operation_area_mask_covers_point_filter;

typedef struct harp_operation_area_mask_intersects_area_filter_struct
{
    harp_operation_type type;
    int (*eval) (struct harp_operation_area_mask_intersects_area_filter_struct *, harp_spherical_polygon *);
    /* parameters */
    char *filename;
    double min_percentage;
    /* extra */
    harp_area_mask *area_mask;
} harp_operation_area_mask_intersects_area_filter;

typedef struct harp_operation_bit_mask_filter_struct
{
    harp_operation_type type;
    int (*eval) (struct harp_operation_bit_mask_filter_struct *, harp_data_type, void *);
    /* parameters */
    char *variable_name;
    harp_bit_mask_operator_type operator_type;
    uint32_t bit_mask;
} harp_operation_bit_mask_filter;

typedef struct harp_operation_collocation_filter_struct
{
    harp_operation_type type;
    int (*eval) (struct harp_operation_collocation_filter_struct *, harp_data_type, void *);
    /* parameters */
    char *filename;
    harp_collocation_filter_type filter_type;
    /* extra */
    harp_collocation_mask *collocation_mask;
    /* extra (for membership filter that is only used for the ingestion phase) */
    int num_values;
    int32_t *value;
} harp_operation_collocation_filter;

typedef struct harp_operation_comparison_filter_struct
{
    harp_operation_type type;
    int (*eval) (struct harp_operation_comparison_filter_struct *, harp_data_type, void *);
    /* parameters */
    char *variable_name;
    harp_comparison_operator_type operator_type;
    double value;
    char *unit;
    /* extra */
    harp_unit_converter *unit_converter;
} harp_operation_comparison_filter;

typedef struct harp_operation_derive_variable_struct
{
    harp_operation_type type;
    /* parameters */
    char *variable_name;
    int num_dimensions;
    harp_dimension_type dimension_type[HARP_MAX_NUM_DIMS];
    char *unit;
} harp_operation_derive_variable;

typedef struct harp_operation_derive_smoothed_column_collocated_struct
{
    harp_operation_type type;
    /* parameters */
    char *variable_name;
    int num_dimensions;
    harp_dimension_type dimension_type[HARP_MAX_NUM_DIMS];
    char *unit;
    char *axis_variable_name;
    char *axis_unit;
    char *collocation_result;
    char target_dataset;
    char *dataset_dir;
} harp_operation_derive_smoothed_column_collocated;

typedef struct harp_operation_exclude_variable_struct
{
    harp_operation_type type;
    /* parameters */
    int num_variables;
    char **variable_name;
} harp_operation_exclude_variable;

typedef struct harp_operation_flatten_struct
{
    harp_operation_type type;
    /* parameters */
    harp_dimension_type dimension_type;
} harp_operation_flatten;

typedef struct harp_operation_keep_variable_struct
{
    harp_operation_type type;
    /* parameters */
    int num_variables;
    char **variable_name;
} harp_operation_keep_variable;

typedef struct harp_operation_longitude_range_filter_struct
{
    harp_operation_type type;
    int (*eval) (struct harp_operation_longitude_range_filter_struct *, harp_data_type, void *);
    /* parameters */
    double min;
    double max;
    /* extra */
    harp_unit_converter *unit_converter;
} harp_operation_longitude_range_filter;

typedef struct harp_operation_membership_filter_struct
{
    harp_operation_type type;
    int (*eval) (struct harp_operation_membership_filter_struct *, harp_data_type, void *);
    /* parameters */
    char *variable_name;
    harp_membership_operator_type operator_type;
    int num_values;
    double *value;
    char *unit;
    /* extra */
    harp_unit_converter *unit_converter;
} harp_operation_membership_filter;

typedef struct harp_operation_point_distance_filter_struct
{
    harp_operation_type type;
    int (*eval) (struct harp_operation_point_distance_filter_struct *, harp_spherical_point *);
    /* parameters */
    harp_spherical_point point;
    double distance;
} harp_operation_point_distance_filter;

typedef struct harp_operation_point_in_area_filter_struct
{
    harp_operation_type type;
    int (*eval) (struct harp_operation_point_in_area_filter_struct *, harp_spherical_polygon *);
    /* parameters */
    harp_spherical_point point;
} harp_operation_point_in_area_filter;

typedef struct harp_operation_regrid_struct
{
    harp_operation_type type;
    /* parameters */
    harp_variable *axis_variable;
} harp_operation_regrid;

typedef struct harp_operation_regrid_collocated_struct
{
    harp_operation_type type;
    /* parameters */
    harp_dimension_type dimension_type;
    char *axis_variable_name;
    char *axis_unit;
    char *collocation_result;
    char target_dataset;
    char *dataset_dir;
} harp_operation_regrid_collocated;

typedef struct harp_operation_rename_struct
{
    harp_operation_type type;
    /* parameters */
    char *variable_name;
    char *new_variable_name;
} harp_operation_rename;

typedef struct harp_operation_smooth_collocated_struct
{
    harp_operation_type type;
    /* parameters */
    int num_variables;
    char **variable_name;
    harp_dimension_type dimension_type;
    char *axis_variable_name;
    char *axis_unit;
    char *collocation_result;
    char target_dataset;
    char *dataset_dir;
} harp_operation_smooth_collocated;

typedef struct harp_operation_string_comparison_filter_struct
{
    harp_operation_type type;
    int (*eval) (struct harp_operation_string_comparison_filter_struct *, harp_data_type, char **);
    /* parameters */
    char *variable_name;
    harp_comparison_operator_type operator_type;
    char *value;
} harp_operation_string_comparison_filter;

typedef struct harp_operation_string_membership_filter_struct
{
    harp_operation_type type;
    int (*eval) (struct harp_operation_string_membership_filter_struct *, harp_data_type, char **);
    /* parameters */
    char *variable_name;
    harp_membership_operator_type operator_type;
    int num_values;
    char **value;
} harp_operation_string_membership_filter;

typedef struct harp_operation_valid_range_filter_struct
{
    harp_operation_type type;
    int (*eval) (struct harp_operation_valid_range_filter_struct *, harp_data_type, void *);
    /* parameters */
    char *variable_name;
    /* extra */
    double valid_min;
    double valid_max;
} harp_operation_valid_range_filter;

typedef struct harp_operation_wrap_struct
{
    harp_operation_type type;
    /* parameters */
    char *variable_name;
    char *unit;
    double min;
    double max;
} harp_operation_wrap;

/* Generic operation */
void harp_operation_delete(harp_operation *operation);
int harp_operation_get_variable_name(const harp_operation *operation, const char **variable_name);
int harp_operation_prepare_collocation_filter(harp_operation *operation, const char *source_product);
int harp_operation_is_point_filter(const harp_operation *operation);
int harp_operation_is_polygon_filter(const harp_operation *operation);
int harp_operation_is_value_filter(const harp_operation *operation);
int harp_operation_set_value_unit(harp_operation *operation, const char *unit);

/* Specific operations */
int harp_operation_area_mask_covers_area_filter_new(const char *filename, harp_operation **new_operation);
int harp_operation_area_mask_covers_point_filter_new(const char *filename, harp_operation **new_operation);
int harp_operation_area_mask_intersects_area_filter_new(const char *filename, double min_percentage,
                                                        harp_operation **new_operation);
int harp_operation_bit_mask_filter_new(const char *variable_name, harp_bit_mask_operator_type operator_type,
                                       uint32_t bit_mask, harp_operation **new_operation);
int harp_operation_collocation_filter_new(const char *filename, harp_collocation_filter_type filter_type,
                                          harp_operation **new_operation);
int harp_operation_comparison_filter_new(const char *variable_name, harp_comparison_operator_type operator_type,
                                         double value, const char *unit, harp_operation **new_operation);
int harp_operation_derive_variable_new(const char *variable_name, int num_dimensions,
                                       const harp_dimension_type *dimension_type, const char *unit,
                                       harp_operation **new_operation);
int harp_operation_derive_smoothed_column_collocated_new(const char *variable_name, int num_dimensions,
                                                         const harp_dimension_type *dimension_type, const char *unit,
                                                         const char *axis_variable_name, const char *axis_unit,
                                                         const char *collocation_result, const char target_dataset,
                                                         const char *dataset_dir, harp_operation **new_operation);
int harp_operation_exclude_variable_new(int num_variables, const char **variable_name, harp_operation **new_operation);
int harp_operation_flatten_new(const harp_dimension_type dimension_type, harp_operation **new_operation);
int harp_operation_keep_variable_new(int num_variables, const char **variable_name, harp_operation **new_operation);
int harp_operation_longitude_range_filter_new(double min, const char *min_unit, double max, const char *max_unit,
                                              harp_operation **new_operation);
int harp_operation_membership_filter_new(const char *variable_name, harp_membership_operator_type operator_type,
                                         int num_values, const double *value, const char *unit,
                                         harp_operation **new_operation);
int harp_operation_point_distance_filter_new(double latitude, const char *latitude_unit, double longitude,
                                             const char *longitude_unit, double distance, const char *distance_unit,
                                             harp_operation **operation);
int harp_operation_point_in_area_filter_new(double latitude, const char *latitude_unit, double longitude,
                                            const char *longitude_unit, harp_operation **operation);
int harp_operation_regrid_new(harp_dimension_type dimension_type, const char *axis_variable_name, const char *axis_unit,
                              long num_values, double *values, harp_operation **new_operation);
int harp_operation_regrid_collocated_new(harp_dimension_type dimension_type, const char *axis_variable_name,
                                         const char *axis_unit, const char *collocation_result,
                                         const char target_dataset, const char *dataset_dir,
                                         harp_operation **new_operation);
int harp_operation_rename_new(const char *variable_name, const char *new_variable_name, harp_operation **new_operation);
int harp_operation_smooth_collocated_new(int num_variables, const char **variable_name,
                                         harp_dimension_type dimension_type, const char *axis_variable_name,
                                         const char *axis_unit, const char *collocation_result,
                                         const char target_dataset, const char *dataset_dir,
                                         harp_operation **new_operation);
int harp_operation_string_comparison_filter_new(const char *variable_name, harp_comparison_operator_type operator_type,
                                                const char *value, harp_operation **new_operation);
int harp_operation_string_membership_filter_new(const char *variable_name, harp_membership_operator_type operator_type,
                                                int num_values, const char **value, harp_operation **new_operation);
int harp_operation_valid_range_filter_new(const char *variable_name, harp_operation **new_operation);
int harp_operation_wrap_new(const char *variable_name, const char *unit, double min, double max,
                            harp_operation **new_operation);

#endif
