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

#ifndef HARP_ACTION_H
#define HARP_ACTION_H

#include "harp-internal.h"

typedef enum harp_action_type_enum
{
    harp_action_filter_collocation,
    harp_action_filter_comparison,
    harp_action_filter_string_comparison,
    harp_action_filter_membership,
    harp_action_filter_string_membership,
    harp_action_filter_valid_range,
    harp_action_filter_longitude_range,
    harp_action_filter_point_distance,
    harp_action_filter_area_mask_covers_point,
    harp_action_filter_area_mask_covers_area,
    harp_action_filter_area_mask_intersects_area,
    harp_action_derive_variable,
    harp_action_include_variable,
    harp_action_exclude_variable
} harp_action_type;

typedef struct harp_action_struct
{
    harp_action_type type;
    void *args;
} harp_action;

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

typedef enum harp_membership_operator_type_enum
{
    harp_operator_in,
    harp_operator_not_in
} harp_membership_operator_type;

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

typedef struct harp_string_comparison_filter_args_struct
{
    char *variable_name;
    harp_comparison_operator_type operator_type;
    char *value;
} harp_string_comparison_filter_args;

typedef struct harp_membership_filter_args_struct
{
    char *variable_name;
    harp_membership_operator_type operator_type;
    int num_values;
    double *value;
    char *unit;
} harp_membership_filter_args;

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

typedef struct harp_longitude_range_filter_args_struct
{
    double min;
    char *min_unit;
    double max;
    char *max_unit;
} harp_longitude_range_filter_args;

typedef struct harp_point_distance_filter_args_struct
{
    double longitude;
    char *longitude_unit;
    double latitude;
    char *latitude_unit;
    double distance;
    char *distance_unit;
} harp_point_distance_filter_args;

typedef struct harp_area_mask_covers_point_filter_args_struct
{
    char *filename;
} harp_area_mask_covers_point_filter_args;

typedef struct harp_area_mask_covers_area_filter_args_struct
{
    char *filename;
} harp_area_mask_covers_area_filter_args;

typedef struct harp_area_mask_intersects_area_filter_args_struct
{
    char *filename;
    double min_percentage;
} harp_area_mask_intersects_area_filter_args;

typedef struct harp_variable_derivation_args_struct
{
    char *variable_name;
    int num_dimensions;
    harp_dimension_type dimension_type[HARP_MAX_NUM_DIMS];
    char *unit;
} harp_variable_derivation_args;

typedef struct harp_variable_inclusion_args_struct
{
    int num_variables;
    char **variable_name;
} harp_variable_inclusion_args;

typedef struct harp_variable_exclusion_args_struct
{
    int num_variables;
    char **variable_name;
} harp_variable_exclusion_args;

typedef struct harp_action_list_struct
{
    int num_actions;
    harp_action **action;
} harp_action_list;

/* Generic action */
int harp_action_new(harp_action_type type, void *args, harp_action **new_action);
void harp_action_delete(harp_action *action);
int harp_action_copy(const harp_action *other_action, harp_action **new_action);

/* Specific actions */
int harp_collocation_filter_new(const char *filename, harp_collocation_filter_type filter_type,
                                harp_action **new_action);

int harp_comparison_filter_new(const char *variable_name, harp_comparison_operator_type operator_type, double value,
                               const char *unit, harp_action **new_action);

int harp_string_comparison_filter_new(const char *variable_name, harp_comparison_operator_type operator_type,
                                      const char *value, harp_action **new_action);

int harp_membership_filter_new(const char *variable_name, harp_membership_operator_type operator_type, int num_values,
                               const double *value, const char *unit, harp_action **new_action);

int harp_string_membership_filter_new(const char *variable_name, harp_membership_operator_type operator_type,
                                      int num_values, const char **value, harp_action **new_action);

int harp_valid_range_filter_new(const char *variable_name, harp_action **new_action);

int harp_longitude_range_filter_new(double min, const char *min_unit, double max, const char *max_unit,
                                    harp_action **new_action);

int harp_point_distance_filter_new(double longitude, const char *longitude_unit, double latitude,
                                   const char *latitude_unit, double distance, const char *distance_unit,
                                   harp_action **action);

int harp_area_mask_covers_point_filter_new(const char *filename, harp_action **new_action);

int harp_area_mask_covers_area_filter_new(const char *filename, harp_action **new_action);

int harp_area_mask_intersects_area_filter_new(const char *filename, double min_percentage, harp_action **new_action);

int harp_variable_derivation_new(const char *variable_name, int num_dimensions,
                                 const harp_dimension_type *dimension_type, const char *unit, harp_action **new_action);

int harp_variable_inclusion_new(int num_variables, const char **variable_name, harp_action **new_action);

int harp_variable_exclusion_new(int num_variables, const char **variable_name, harp_action **new_action);

int harp_action_get_variable_name(const harp_action *action, const char **variable_name);

/* List of actions */
int harp_action_list_new(harp_action_list **new_action_list);
void harp_action_list_delete(harp_action_list *action_list);
int harp_action_list_add_action(harp_action_list *action_list, harp_action *action);
int harp_action_list_remove_action_at_index(harp_action_list *action_list, int index);
int harp_action_list_remove_action(harp_action_list *action_list, harp_action *action);
int harp_action_list_verify(const harp_action_list *action_list);

int harp_product_execute_action_list(harp_product *product, harp_action_list *action_list);

/* Parse functions */
int harp_action_list_from_string(const char *str, harp_action_list **new_action_list);

#endif
