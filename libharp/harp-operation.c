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

#include "harp-operation.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static int compare_int32(const void *untyped_a, const void *untyped_b)
{
    int32_t a = *((int32_t *)untyped_a);
    int32_t b = *((int32_t *)untyped_b);


    if (a < b)
    {
        return -1;
    }

    if (a > b)
    {
        return 1;
    }

    return 0;
}

static int add_latitude_longitude_bounds_to_area_mask(harp_area_mask *area_mask, int num_vertices, double *latitude,
                                                      const char *latitude_unit, double *longitude,
                                                      const char *longitude_unit)
{
    harp_spherical_polygon *polygon;

    if (latitude_unit != NULL || longitude_unit != NULL)
    {
        double *coordinates;
        int i;

        coordinates = (double *)malloc(2 * num_vertices * sizeof(double));
        if (coordinates == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           2 * num_vertices * sizeof(double), __FILE__, __LINE__);
            return -1;
        }
        for (i = 0; i < num_vertices; i++)
        {
            coordinates[i] = latitude[i];
            coordinates[i + num_vertices] = longitude[i];
        }
        if (latitude_unit != NULL)
        {
            if (harp_unit_compare(latitude_unit, "degree_north") != 0)
            {
                if (harp_convert_unit(latitude_unit, "degree_north", num_vertices, coordinates) != 0)
                {
                    free(coordinates);
                    return -1;
                }
            }
        }
        if (longitude_unit != NULL)
        {
            if (harp_unit_compare(longitude_unit, "degree_east") != 0)
            {
                if (harp_convert_unit(longitude_unit, "degree_east", num_vertices, &coordinates[num_vertices]) != 0)
                {
                    free(coordinates);
                    return -1;
                }
            }
        }
        if (harp_spherical_polygon_from_latitude_longitude_bounds(0, num_vertices, coordinates,
                                                                  &coordinates[num_vertices], &polygon) != 0)
        {
            free(coordinates);
            return -1;
        }
        free(coordinates);
    }
    else
    {
        if (harp_spherical_polygon_from_latitude_longitude_bounds(0, num_vertices, latitude, longitude, &polygon) != 0)
        {
            return -1;
        }
    }
    if (harp_area_mask_add_polygon(area_mask, polygon) != 0)
    {
        harp_spherical_polygon_delete(polygon);
        return -1;
    }

    return 0;
}

static int eval_area_covers_area(harp_operation_area_covers_area_filter *operation, harp_spherical_polygon *polygon)
{
    return harp_area_mask_inside_area(operation->area_mask, polygon);
}

static int eval_area_covers_point(harp_operation_area_covers_point_filter *operation, harp_spherical_polygon *polygon)
{
    return harp_spherical_polygon_contains_point(polygon, &operation->point);
}

static int eval_area_inside_area(harp_operation_area_inside_area_filter *operation, harp_spherical_polygon *polygon)
{
    return harp_area_mask_covers_area(operation->area_mask, polygon);
}

static int eval_area_intersects_area(harp_operation_area_intersects_area_filter *operation,
                                     harp_spherical_polygon *polygon)
{
    if (operation->has_fraction)
    {
        return harp_area_mask_intersects_area_with_fraction(operation->area_mask, polygon, operation->min_fraction);
    }
    return harp_area_mask_intersects_area(operation->area_mask, polygon);
}

static int eval_bitmask(harp_operation_bit_mask_filter *operation, harp_data_type data_type, void *value)
{
    uint32_t bitmap_value;

    switch (data_type)
    {
        case harp_type_int8:
            bitmap_value = *((uint8_t *)value);
            break;
        case harp_type_int16:
            bitmap_value = *((uint16_t *)value);
            break;
        case harp_type_int32:
            bitmap_value = *((uint32_t *)value);
            break;
        default:
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "cannot perform bitmask filter for data type: %s",
                           harp_get_data_type_name(data_type));
            return -1;
    }
    switch (operation->operator_type)
    {
        case operator_bit_mask_any:
            return (bitmap_value & operation->bit_mask) != 0;
        case operator_bit_mask_none:
            return (bitmap_value & operation->bit_mask) == 0;
    }

    assert(0);
    exit(1);
}

static int eval_collocation(harp_operation_collocation_filter *operation, harp_data_type data_type, void *value)
{
    int32_t index;
    int low = 0;
    int high = operation->num_values - 1;

    /* this function is only used during the ingestion phase as a prefilter for the actual collocation filter */

    if (data_type != harp_type_int32)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "cannot perform index filter for data type: %s",
                       harp_get_data_type_name(data_type));
        return -1;
    }
    index = *((int32_t *)value);

    if (operation->num_values == 0)
    {
        return 0;
    }

    /* use bisection to find index in sorted list */
    while (high - low != 1)
    {
        long middle = (high + low) / 2;

        if (index >= operation->value[middle])
        {
            low = middle;
        }
        else
        {
            high = middle;
        }
    }

    return (index == operation->value[low]);
}

static int eval_comparison(harp_operation_comparison_filter *operation, harp_data_type data_type, void *value)
{
    double double_value;

    switch (data_type)
    {
        case harp_type_int8:
            double_value = (double)*((int8_t *)value);
            break;
        case harp_type_int16:
            double_value = (double)*((int16_t *)value);
            break;
        case harp_type_int32:
            double_value = (double)*((int32_t *)value);
            break;
        case harp_type_float:
            double_value = (double)*((float *)value);
            break;
        case harp_type_double:
            double_value = *((double *)value);
            break;
        default:
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "cannot perform numerical comparison filter for data type: %s",
                           harp_get_data_type_name(data_type));
            return -1;
    }

    if (operation->unit_converter != NULL)
    {
        double_value = harp_unit_converter_convert(operation->unit_converter, double_value);
    }

    switch (operation->operator_type)
    {
        case operator_eq:
            return double_value == operation->value;
        case operator_ne:
            return double_value != operation->value;
        case operator_lt:
            return double_value < operation->value;
        case operator_le:
            return double_value <= operation->value;
        case operator_gt:
            return double_value > operation->value;
        case operator_ge:
            return double_value >= operation->value;
    }

    assert(0);
    exit(1);
}

static int eval_longitude_range(harp_operation_longitude_range_filter *operation, harp_data_type data_type, void *value)
{
    double double_value;

    switch (data_type)
    {
        case harp_type_int8:
            double_value = (double)*((int8_t *)value);
            break;
        case harp_type_int16:
            double_value = (double)*((int16_t *)value);
            break;
        case harp_type_int32:
            double_value = (double)*((int32_t *)value);
            break;
        case harp_type_float:
            double_value = (double)*((float *)value);
            break;
        case harp_type_double:
            double_value = *((double *)value);
            break;
        default:
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "cannot perform longitude range filter for data type: %s",
                           harp_get_data_type_name(data_type));
            return -1;
    }

    if (operation->unit_converter != NULL)
    {
        double_value = harp_unit_converter_convert(operation->unit_converter, double_value);
    }

    /* map longitude to [min,min+360) */
    double_value = double_value - 360.0 * floor((double_value - operation->min) / 360.0);

    return (double_value <= operation->max);
}

static int eval_membership(harp_operation_membership_filter *operation, harp_data_type data_type, void *value)
{
    double double_value;
    int i;

    switch (data_type)
    {
        case harp_type_int8:
            double_value = (double)*((int8_t *)value);
            break;
        case harp_type_int16:
            double_value = (double)*((int16_t *)value);
            break;
        case harp_type_int32:
            double_value = (double)*((int32_t *)value);
            break;
        case harp_type_float:
            double_value = (double)*((float *)value);
            break;
        case harp_type_double:
            double_value = *((double *)value);
            break;
        default:
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "cannot perform numerical membership filter for data type: %s",
                           harp_get_data_type_name(data_type));
            return -1;
    }

    if (operation->unit_converter != NULL)
    {
        double_value = harp_unit_converter_convert(operation->unit_converter, double_value);
    }

    for (i = 0; i < operation->num_values; i++)
    {
        if (operation->value[i] == double_value)
        {
            return operation->operator_type == operator_in ? 1 : 0;
        }
    }

    return operation->operator_type == operator_in ? 0 : 1;
}

static int eval_point_distance(harp_operation_point_distance_filter *operation, harp_spherical_point *point)
{
    return (harp_spherical_point_distance_in_meters(&operation->point, point) <= operation->distance);
}

static int eval_point_in_area(harp_operation_point_in_area_filter *operation, harp_spherical_point *point)
{
    return harp_area_mask_covers_point(operation->area_mask, point);
}

static int eval_string_comparison(harp_operation_string_comparison_filter *operation, int num_enum_values,
                                  char **enum_name, harp_data_type data_type, void *value)
{
    const char *string_value;

    if (num_enum_values > 0)
    {
        int int_value;

        assert(enum_name != NULL);
        switch (data_type)
        {
            case harp_type_int8:
                int_value = (int)*((int8_t *)value);
                break;
            case harp_type_int16:
                int_value = (int)*((int16_t *)value);
                break;
            case harp_type_int32:
                int_value = (int)*((int32_t *)value);
                break;
            default:
                assert(0);
                exit(1);
        }
        if (int_value >= 0 && int_value < num_enum_values)
        {
            string_value = enum_name[int_value];
            assert(string_value != NULL);
        }
        else
        {
            string_value = "";
        }
    }
    else
    {
        if (data_type != harp_type_string)
        {
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "cannot perform string comparison filter for data type: %s",
                           harp_get_data_type_name(data_type));
            return -1;
        }
        string_value = *(char **)value;
    }

    switch (operation->operator_type)
    {
        case operator_eq:
            return (strcmp(operation->value, string_value) == 0);
        case operator_ne:
            return (strcmp(operation->value, string_value) != 0);
        case operator_lt:
            return (strcmp(operation->value, string_value) < 0);
        case operator_le:
            return (strcmp(operation->value, string_value) <= 0);
        case operator_gt:
            return (strcmp(operation->value, string_value) > 0);
        case operator_ge:
            return (strcmp(operation->value, string_value) >= 0);
    }

    assert(0);
    exit(1);
}

static int eval_string_membership(harp_operation_string_membership_filter *operation, int num_enum_values,
                                  char **enum_name, harp_data_type data_type, void *value)
{
    const char *string_value;
    int i;

    if (num_enum_values > 0)
    {
        int int_value;

        assert(enum_name != NULL);
        switch (data_type)
        {
            case harp_type_int8:
                int_value = (int)*((int8_t *)value);
                break;
            case harp_type_int16:
                int_value = (int)*((int16_t *)value);
                break;
            case harp_type_int32:
                int_value = (int)*((int32_t *)value);
                break;
            default:
                assert(0);
                exit(1);
        }
        if (int_value >= 0 && int_value < num_enum_values)
        {
            string_value = enum_name[int_value];
            assert(string_value != NULL);
        }
        else
        {
            string_value = "";
        }
    }
    else
    {
        if (data_type != harp_type_string)
        {
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "cannot perform string membership filter for data type: %s",
                           harp_get_data_type_name(data_type));
            return -1;
        }
        string_value = *(char **)value;
    }

    for (i = 0; i < operation->num_values; i++)
    {
        if (strcmp(operation->value[i], string_value) == 0)
        {
            return operation->operator_type == operator_in ? 1 : 0;
        }
    }

    return operation->operator_type == operator_in ? 0 : 1;
}

static int eval_valid_range(harp_operation_valid_range_filter *operation, harp_data_type data_type, void *value)
{
    double double_value;

    switch (data_type)
    {
        case harp_type_int8:
            double_value = (double)*((int8_t *)value);
            break;
        case harp_type_int16:
            double_value = (double)*((int16_t *)value);
            break;
        case harp_type_int32:
            double_value = (double)*((int32_t *)value);
            break;
        case harp_type_float:
            double_value = (double)*((float *)value);
            break;
        case harp_type_double:
            double_value = *((double *)value);
            break;
        default:
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "cannot perform valid range filter for data type: %s",
                           harp_get_data_type_name(data_type));
            return -1;
    }

    return (!harp_isnan(double_value) && double_value >= operation->valid_min && double_value <= operation->valid_max);
}

static void area_covers_area_filter_delete(harp_operation_area_covers_area_filter *operation)
{
    if (operation != NULL)
    {
        if (operation->filename != NULL)
        {
            free(operation->filename);
        }
        if (operation->area_mask != NULL)
        {
            harp_area_mask_delete(operation->area_mask);
        }

        free(operation);
    }
}

static void area_covers_point_filter_delete(harp_operation_area_covers_point_filter *operation)
{
    if (operation != NULL)
    {
        free(operation);
    }
}

static void area_inside_area_filter_delete(harp_operation_area_inside_area_filter *operation)
{
    if (operation != NULL)
    {
        if (operation->filename != NULL)
        {
            free(operation->filename);
        }
        if (operation->area_mask != NULL)
        {
            harp_area_mask_delete(operation->area_mask);
        }

        free(operation);
    }
}

static void area_intersects_area_filter_delete(harp_operation_area_intersects_area_filter *operation)
{
    if (operation != NULL)
    {
        if (operation->filename != NULL)
        {
            free(operation->filename);
        }
        if (operation->area_mask != NULL)
        {
            harp_area_mask_delete(operation->area_mask);
        }

        free(operation);
    }
}

static void bin_collocated_delete(harp_operation_bin_collocated *operation)
{
    if (operation != NULL)
    {
        if (operation->collocation_result != NULL)
        {
            free(operation->collocation_result);
        }

        free(operation);
    }
}

static void bin_with_variable_delete(harp_operation_bin_with_variable *operation)
{
    if (operation != NULL)
    {
        if (operation->variable_name != NULL)
        {
            free(operation->variable_name);
        }

        free(operation);
    }
}

static void bit_mask_filter_delete(harp_operation_bit_mask_filter *operation)
{
    if (operation != NULL)
    {
        if (operation->variable_name != NULL)
        {
            free(operation->variable_name);
        }

        free(operation);
    }
}

static void collocation_filter_delete(harp_operation_collocation_filter *operation)
{
    if (operation != NULL)
    {
        if (operation->filename != NULL)
        {
            free(operation->filename);
        }
        if (operation->collocation_mask != NULL)
        {
            harp_collocation_mask_delete(operation->collocation_mask);
        }
        if (operation->value != NULL)
        {
            free(operation->value);
        }

        free(operation);
    }
}

static void comparison_filter_delete(harp_operation_comparison_filter *operation)
{
    if (operation != NULL)
    {
        if (operation->variable_name != NULL)
        {
            free(operation->variable_name);
        }
        if (operation->unit != NULL)
        {
            free(operation->unit);
        }
        if (operation->unit_converter != NULL)
        {
            harp_unit_converter_delete(operation->unit_converter);
        }

        free(operation);
    }
}

static void derive_variable_delete(harp_operation_derive_variable *operation)
{
    if (operation != NULL)
    {
        if (operation->variable_name != NULL)
        {
            free(operation->variable_name);
        }
        if (operation->unit != NULL)
        {
            free(operation->unit);
        }

        free(operation);
    }
}

static void derive_smoothed_column_collocated_dataset_delete
    (harp_operation_derive_smoothed_column_collocated_dataset *operation)
{
    if (operation != NULL)
    {
        if (operation->variable_name != NULL)
        {
            free(operation->variable_name);
        }
        if (operation->unit != NULL)
        {
            free(operation->unit);
        }
        if (operation->axis_variable_name != NULL)
        {
            free(operation->axis_variable_name);
        }
        if (operation->axis_unit != NULL)
        {
            free(operation->axis_unit);
        }
        if (operation->collocation_result != NULL)
        {
            free(operation->collocation_result);
        }
        if (operation->dataset_dir != NULL)
        {
            free(operation->dataset_dir);
        }

        free(operation);
    }
}

static void derive_smoothed_column_collocated_product_delete
    (harp_operation_derive_smoothed_column_collocated_product *operation)
{
    if (operation != NULL)
    {
        if (operation->variable_name != NULL)
        {
            free(operation->variable_name);
        }
        if (operation->unit != NULL)
        {
            free(operation->unit);
        }
        if (operation->axis_variable_name != NULL)
        {
            free(operation->axis_variable_name);
        }
        if (operation->axis_unit != NULL)
        {
            free(operation->axis_unit);
        }
        if (operation->filename != NULL)
        {
            free(operation->filename);
        }

        free(operation);
    }
}

static void exclude_variable_delete(harp_operation_exclude_variable *operation)
{
    if (operation != NULL)
    {
        if (operation->variable_name != NULL)
        {
            int i;

            for (i = 0; i < operation->num_variables; i++)
            {
                if (operation->variable_name[i] != NULL)
                {
                    free(operation->variable_name[i]);
                }
            }

            free(operation->variable_name);
        }

        free(operation);
    }
}

static void flatten_delete(harp_operation_flatten *operation)
{
    if (operation != NULL)
    {
        free(operation);
    }
}

static void keep_variable_delete(harp_operation_keep_variable *operation)
{
    if (operation != NULL)
    {
        if (operation->variable_name != NULL)
        {
            int i;

            for (i = 0; i < operation->num_variables; i++)
            {
                if (operation->variable_name[i] != NULL)
                {
                    free(operation->variable_name[i]);
                }
            }

            free(operation->variable_name);
        }

        free(operation);
    }
}

static void longitude_range_filter_delete(harp_operation_longitude_range_filter *operation)
{
    if (operation != NULL)
    {
        if (operation->unit_converter != NULL)
        {
            harp_unit_converter_delete(operation->unit_converter);
        }

        free(operation);
    }
}

static void membership_filter_delete(harp_operation_membership_filter *operation)
{
    if (operation != NULL)
    {
        if (operation->variable_name != NULL)
        {
            free(operation->variable_name);
        }
        if (operation->value != NULL)
        {
            free(operation->value);
        }
        if (operation->unit != NULL)
        {
            free(operation->unit);
        }
        if (operation->unit_converter != NULL)
        {
            harp_unit_converter_delete(operation->unit_converter);
        }

        free(operation);
    }
}

static void point_distance_filter_delete(harp_operation_point_distance_filter *operation)
{
    if (operation != NULL)
    {
        free(operation);
    }
}

static void point_in_area_filter_delete(harp_operation_point_in_area_filter *operation)
{
    if (operation != NULL)
    {
        if (operation->filename != NULL)
        {
            free(operation->filename);
        }
        if (operation->area_mask != NULL)
        {
            harp_area_mask_delete(operation->area_mask);
        }

        free(operation);
    }
}

static void regrid_delete(harp_operation_regrid *operation)
{
    if (operation != NULL)
    {
        if (operation->axis_variable != NULL)
        {
            harp_variable_delete(operation->axis_variable);
        }

        free(operation);
    }
}

static void regrid_collocated_dataset_delete(harp_operation_regrid_collocated_dataset *operation)
{
    if (operation != NULL)
    {
        if (operation->axis_variable_name != NULL)
        {
            free(operation->axis_variable_name);
        }
        if (operation->axis_unit != NULL)
        {
            free(operation->axis_unit);
        }
        if (operation->collocation_result != NULL)
        {
            free(operation->collocation_result);
        }
        if (operation->dataset_dir != NULL)
        {
            free(operation->dataset_dir);
        }

        free(operation);
    }
}

static void regrid_collocated_product_delete(harp_operation_regrid_collocated_product *operation)
{
    if (operation != NULL)
    {
        if (operation->axis_variable_name != NULL)
        {
            free(operation->axis_variable_name);
        }
        if (operation->axis_unit != NULL)
        {
            free(operation->axis_unit);
        }
        if (operation->filename != NULL)
        {
            free(operation->filename);
        }

        free(operation);
    }
}

static void rename_delete(harp_operation_rename *operation)
{
    if (operation != NULL)
    {
        if (operation->variable_name != NULL)
        {
            free(operation->variable_name);
        }
        if (operation->new_variable_name != NULL)
        {
            free(operation->new_variable_name);
        }

        free(operation);
    }
}

static void set_delete(harp_operation_set *operation)
{
    if (operation != NULL)
    {
        if (operation->option != NULL)
        {
            free(operation->option);
        }
        if (operation->value != NULL)
        {
            free(operation->value);
        }

        free(operation);
    }
}

static void smooth_collocated_dataset_delete(harp_operation_smooth_collocated_dataset *operation)
{
    if (operation != NULL)
    {
        if (operation->variable_name != NULL)
        {
            int i;

            for (i = 0; i < operation->num_variables; i++)
            {
                if (operation->variable_name[i] != NULL)
                {
                    free(operation->variable_name[i]);
                }
            }
            free(operation->variable_name);
        }
        if (operation->axis_variable_name != NULL)
        {
            free(operation->axis_variable_name);
        }
        if (operation->axis_unit != NULL)
        {
            free(operation->axis_unit);
        }
        if (operation->collocation_result != NULL)
        {
            free(operation->collocation_result);
        }
        if (operation->dataset_dir != NULL)
        {
            free(operation->dataset_dir);
        }

        free(operation);
    }
}

static void smooth_collocated_product_delete(harp_operation_smooth_collocated_product *operation)
{
    if (operation != NULL)
    {
        if (operation->variable_name != NULL)
        {
            int i;

            for (i = 0; i < operation->num_variables; i++)
            {
                if (operation->variable_name[i] != NULL)
                {
                    free(operation->variable_name[i]);
                }
            }
            free(operation->variable_name);
        }
        if (operation->axis_variable_name != NULL)
        {
            free(operation->axis_variable_name);
        }
        if (operation->axis_unit != NULL)
        {
            free(operation->axis_unit);
        }
        if (operation->filename != NULL)
        {
            free(operation->filename);
        }

        free(operation);
    }
}

static void sort_delete(harp_operation_sort *operation)
{
    if (operation != NULL)
    {
        if (operation->variable_name != NULL)
        {
            free(operation->variable_name);
        }

        free(operation);
    }
}

static void string_comparison_filter_delete(harp_operation_string_comparison_filter *operation)
{
    if (operation != NULL)
    {
        if (operation->variable_name != NULL)
        {
            free(operation->variable_name);
        }
        if (operation->value != NULL)
        {
            free(operation->value);
        }

        free(operation);
    }
}

static void string_membership_filter_delete(harp_operation_string_membership_filter *operation)
{
    if (operation != NULL)
    {
        if (operation->variable_name != NULL)
        {
            free(operation->variable_name);
        }

        if (operation->value != NULL)
        {
            int i;

            for (i = 0; i < operation->num_values; i++)
            {
                if (operation->value[i] != NULL)
                {
                    free(operation->value[i]);
                }
            }

            free(operation->value);
        }

        free(operation);
    }
}

static void valid_range_filter_delete(harp_operation_valid_range_filter *operation)
{
    if (operation != NULL)
    {
        if (operation->variable_name != NULL)
        {
            free(operation->variable_name);
        }

        free(operation);
    }
}

static void wrap_delete(harp_operation_wrap *operation)
{
    if (operation != NULL)
    {
        if (operation->variable_name != NULL)
        {
            free(operation->variable_name);
        }
        if (operation->unit != NULL)
        {
            free(operation->unit);
        }

        free(operation);
    }
}

void harp_operation_delete(harp_operation *operation)
{
    switch (operation->type)
    {
        case operation_area_covers_area_filter:
            area_covers_area_filter_delete((harp_operation_area_covers_area_filter *)operation);
            break;
        case operation_area_covers_point_filter:
            area_covers_point_filter_delete((harp_operation_area_covers_point_filter *)operation);
            break;
        case operation_area_inside_area_filter:
            area_inside_area_filter_delete((harp_operation_area_inside_area_filter *)operation);
            break;
        case operation_area_intersects_area_filter:
            area_intersects_area_filter_delete((harp_operation_area_intersects_area_filter *)operation);
            break;
        case operation_bin_collocated:
            bin_collocated_delete((harp_operation_bin_collocated *)operation);
            break;
        case operation_bin_with_variable:
            bin_with_variable_delete((harp_operation_bin_with_variable *)operation);
            break;
        case operation_bit_mask_filter:
            bit_mask_filter_delete((harp_operation_bit_mask_filter *)operation);
            break;
        case operation_collocation_filter:
            collocation_filter_delete((harp_operation_collocation_filter *)operation);
            break;
        case operation_comparison_filter:
            comparison_filter_delete((harp_operation_comparison_filter *)operation);
            break;
        case operation_derive_variable:
            derive_variable_delete((harp_operation_derive_variable *)operation);
            break;
        case operation_derive_smoothed_column_collocated_dataset:
            derive_smoothed_column_collocated_dataset_delete
                ((harp_operation_derive_smoothed_column_collocated_dataset *)operation);
            break;
        case operation_derive_smoothed_column_collocated_product:
            derive_smoothed_column_collocated_product_delete
                ((harp_operation_derive_smoothed_column_collocated_product *)operation);
            break;
        case operation_exclude_variable:
            exclude_variable_delete((harp_operation_exclude_variable *)operation);
            break;
        case operation_flatten:
            flatten_delete((harp_operation_flatten *)operation);
            break;
        case operation_keep_variable:
            keep_variable_delete((harp_operation_keep_variable *)operation);
            break;
        case operation_longitude_range_filter:
            longitude_range_filter_delete((harp_operation_longitude_range_filter *)operation);
            break;
        case operation_membership_filter:
            membership_filter_delete((harp_operation_membership_filter *)operation);
            break;
        case operation_point_distance_filter:
            point_distance_filter_delete((harp_operation_point_distance_filter *)operation);
            break;
        case operation_point_in_area_filter:
            point_in_area_filter_delete((harp_operation_point_in_area_filter *)operation);
            break;
        case operation_regrid:
            regrid_delete((harp_operation_regrid *)operation);
            break;
        case operation_regrid_collocated_dataset:
            regrid_collocated_dataset_delete((harp_operation_regrid_collocated_dataset *)operation);
            break;
        case operation_regrid_collocated_product:
            regrid_collocated_product_delete((harp_operation_regrid_collocated_product *)operation);
            break;
        case operation_rename:
            rename_delete((harp_operation_rename *)operation);
            break;
        case operation_set:
            set_delete((harp_operation_set *)operation);
            break;
        case operation_smooth_collocated_dataset:
            smooth_collocated_dataset_delete((harp_operation_smooth_collocated_dataset *)operation);
            break;
        case operation_smooth_collocated_product:
            smooth_collocated_product_delete((harp_operation_smooth_collocated_product *)operation);
            break;
        case operation_sort:
            sort_delete((harp_operation_sort *)operation);
            break;
        case operation_string_comparison_filter:
            string_comparison_filter_delete((harp_operation_string_comparison_filter *)operation);
            break;
        case operation_string_membership_filter:
            string_membership_filter_delete((harp_operation_string_membership_filter *)operation);
            break;
        case operation_valid_range_filter:
            valid_range_filter_delete((harp_operation_valid_range_filter *)operation);
            break;
        case operation_wrap:
            wrap_delete((harp_operation_wrap *)operation);
            break;
    }
}

int harp_operation_area_covers_area_filter_new(const char *filename, int num_latitudes, double *latitude,
                                               const char *latitude_unit, int num_longitudes, double *longitude,
                                               const char *longitude_unit, harp_operation **new_operation)
{
    harp_operation_area_covers_area_filter *operation;

    if (num_latitudes != num_longitudes)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "number of latitude and longitude points need to be the same");
        return -1;
    }
    if (filename != NULL && num_latitudes > 0)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "cannot provide both area mask file and individual area (%s:%u)",
                       __FILE__, __LINE__);
        return -1;
    }
    if (filename == NULL && num_latitudes == 0)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "neither area mask file nor individual area provided (%s:%u)",
                       __FILE__, __LINE__);
        return -1;
    }
    operation = (harp_operation_area_covers_area_filter *)malloc(sizeof(harp_operation_area_covers_area_filter));
    if (operation == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_operation_area_covers_area_filter), __FILE__, __LINE__);
        return -1;
    }
    operation->type = operation_area_covers_area_filter;
    operation->eval = eval_area_covers_area;
    operation->filename = NULL;
    operation->area_mask = NULL;

    if (filename != NULL)
    {
        operation->filename = strdup(filename);
        if (operation->filename == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                           __LINE__);
            area_covers_area_filter_delete(operation);
            return -1;
        }
        if (harp_area_mask_read(operation->filename, &operation->area_mask) != 0)
        {
            area_covers_area_filter_delete(operation);
            return -1;
        }
    }
    else
    {
        if (harp_area_mask_new(&operation->area_mask) != 0)
        {
            area_covers_area_filter_delete(operation);
            return -1;
        }
        if (add_latitude_longitude_bounds_to_area_mask(operation->area_mask, num_latitudes, latitude, latitude_unit,
                                                       longitude, longitude_unit) != 0)
        {
            area_covers_area_filter_delete(operation);
            return -1;
        }
    }

    *new_operation = (harp_operation *)operation;
    return 0;
}

int harp_operation_area_covers_point_filter_new(double latitude, const char *latitude_unit, double longitude,
                                                const char *longitude_unit, harp_operation **new_operation)
{
    harp_operation_area_covers_point_filter *operation;

    operation = (harp_operation_area_covers_point_filter *)malloc(sizeof(harp_operation_point_in_area_filter));
    if (operation == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_operation_area_covers_point_filter), __FILE__, __LINE__);
        return -1;
    }
    operation->type = operation_area_covers_point_filter;
    operation->eval = eval_area_covers_point;
    operation->point.lat = latitude;
    operation->point.lon = longitude;

    /* convert parameters to internal units */
    if (latitude_unit != NULL)
    {
        if (harp_unit_compare(latitude_unit, "degree_north") != 0)
        {
            if (harp_convert_unit(latitude_unit, "degree_north", 1, &operation->point.lat) != 0)
            {
                return -1;
            }
        }
    }
    if (longitude_unit != NULL)
    {
        if (harp_unit_compare(longitude_unit, "degree_east") != 0)
        {
            if (harp_convert_unit(longitude_unit, "degree_east", 1, &operation->point.lon) != 0)
            {
                return -1;
            }
        }
    }

    harp_spherical_point_rad_from_deg(&operation->point);
    harp_spherical_point_check(&operation->point);

    *new_operation = (harp_operation *)operation;
    return 0;
}

int harp_operation_area_inside_area_filter_new(const char *filename, int num_latitudes, double *latitude,
                                               const char *latitude_unit, int num_longitudes, double *longitude,
                                               const char *longitude_unit, harp_operation **new_operation)
{
    harp_operation_area_inside_area_filter *operation;

    if (num_latitudes != num_longitudes)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "number of latitude and longitude points need to be the same");
        return -1;
    }
    if (filename != NULL && num_latitudes > 0)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "cannot provide both area mask file and individual area (%s:%u)",
                       __FILE__, __LINE__);
        return -1;
    }
    if (filename == NULL && num_latitudes == 0)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "neither area mask file nor individual area provided (%s:%u)",
                       __FILE__, __LINE__);
        return -1;
    }
    operation = (harp_operation_area_inside_area_filter *)malloc(sizeof(harp_operation_area_inside_area_filter));
    if (operation == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_operation_area_inside_area_filter), __FILE__, __LINE__);
        return -1;
    }
    operation->type = operation_area_inside_area_filter;
    operation->eval = eval_area_inside_area;
    operation->filename = NULL;
    operation->area_mask = NULL;

    if (filename != NULL)
    {
        operation->filename = strdup(filename);
        if (operation->filename == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                           __LINE__);
            area_inside_area_filter_delete(operation);
            return -1;
        }
        if (harp_area_mask_read(operation->filename, &operation->area_mask) != 0)
        {
            area_inside_area_filter_delete(operation);
            return -1;
        }
    }
    else
    {
        if (harp_area_mask_new(&operation->area_mask) != 0)
        {
            area_inside_area_filter_delete(operation);
            return -1;
        }
        if (add_latitude_longitude_bounds_to_area_mask(operation->area_mask, num_latitudes, latitude, latitude_unit,
                                                       longitude, longitude_unit) != 0)
        {
            area_inside_area_filter_delete(operation);
            return -1;
        }
    }

    *new_operation = (harp_operation *)operation;
    return 0;
}

int harp_operation_area_intersects_area_filter_new(const char *filename, int num_latitudes, double *latitude,
                                                   const char *latitude_unit, int num_longitudes, double *longitude,
                                                   const char *longitude_unit, double *min_fraction,
                                                   harp_operation **new_operation)
{
    harp_operation_area_intersects_area_filter *operation;

    if (num_latitudes != num_longitudes)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "number of latitude and longitude points need to be the same");
        return -1;
    }
    if (filename != NULL && num_latitudes > 0)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "cannot provide both area mask file and individual area (%s:%u)",
                       __FILE__, __LINE__);
        return -1;
    }
    if (filename == NULL && num_latitudes == 0)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "neither area mask file nor individual area provided (%s:%u)",
                       __FILE__, __LINE__);
        return -1;
    }
    operation =
        (harp_operation_area_intersects_area_filter *)malloc(sizeof(harp_operation_area_intersects_area_filter));
    if (operation == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_operation_area_intersects_area_filter), __FILE__, __LINE__);
        return -1;
    }
    operation->type = operation_area_intersects_area_filter;
    operation->eval = eval_area_intersects_area;
    operation->filename = NULL;
    if (min_fraction == NULL)
    {
        operation->has_fraction = 0;
        operation->min_fraction = 0;
    }
    else
    {
        operation->has_fraction = 1;
        operation->min_fraction = *min_fraction;
    }
    operation->area_mask = NULL;

    if (filename != NULL)
    {
        operation->filename = strdup(filename);
        if (operation->filename == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                           __LINE__);
            area_intersects_area_filter_delete(operation);
            return -1;
        }
        if (harp_area_mask_read(operation->filename, &operation->area_mask) != 0)
        {
            area_intersects_area_filter_delete(operation);
            return -1;
        }
    }
    else
    {
        if (harp_area_mask_new(&operation->area_mask) != 0)
        {
            area_intersects_area_filter_delete(operation);
            return -1;
        }
        if (add_latitude_longitude_bounds_to_area_mask(operation->area_mask, num_latitudes, latitude, latitude_unit,
                                                       longitude, longitude_unit) != 0)
        {
            area_intersects_area_filter_delete(operation);
            return -1;
        }
    }

    *new_operation = (harp_operation *)operation;
    return 0;
}

int harp_operation_bin_collocated_new(const char *collocation_result, const char target_dataset,
                                      harp_operation **new_operation)
{
    harp_operation_bin_collocated *operation;

    assert(collocation_result != NULL);

    operation = (harp_operation_bin_collocated *)malloc(sizeof(harp_operation_bin_collocated));
    if (operation == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_operation_bin_collocated), __FILE__, __LINE__);
        return -1;
    }
    operation->type = operation_bin_collocated;
    operation->collocation_result = NULL;
    operation->target_dataset = target_dataset;

    operation->collocation_result = strdup(collocation_result);
    if (operation->collocation_result == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        bin_collocated_delete(operation);
        return -1;
    }

    *new_operation = (harp_operation *)operation;
    return 0;
}

int harp_operation_bin_with_variable_new(const char *variable_name, harp_operation **new_operation)
{
    harp_operation_bin_with_variable *operation;

    assert(variable_name != NULL);

    operation = (harp_operation_bin_with_variable *)malloc(sizeof(harp_operation_bin_with_variable));
    if (operation == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_operation_bin_with_variable), __FILE__, __LINE__);
        return -1;
    }
    operation->type = operation_bin_with_variable;
    operation->variable_name = NULL;

    operation->variable_name = strdup(variable_name);
    if (operation->variable_name == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        bin_with_variable_delete(operation);
        return -1;
    }

    *new_operation = (harp_operation *)operation;
    return 0;
}

int harp_operation_bit_mask_filter_new(const char *variable_name, harp_bit_mask_operator_type operator_type,
                                       uint32_t bit_mask, harp_operation **new_operation)
{
    harp_operation_bit_mask_filter *operation;

    assert(variable_name != NULL);

    operation = (harp_operation_bit_mask_filter *)malloc(sizeof(harp_operation_bit_mask_filter));
    if (operation == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_operation_bit_mask_filter), __FILE__, __LINE__);
        return -1;
    }
    operation->type = operation_bit_mask_filter;
    operation->eval = eval_bitmask;
    operation->variable_name = NULL;
    operation->operator_type = operator_type;
    operation->bit_mask = bit_mask;

    operation->variable_name = strdup(variable_name);
    if (operation->variable_name == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        bit_mask_filter_delete(operation);
        return -1;
    }

    *new_operation = (harp_operation *)operation;
    return 0;
}

int harp_operation_collocation_filter_new(const char *filename, harp_collocation_filter_type filter_type,
                                          harp_operation **new_operation)
{
    harp_operation_collocation_filter *operation;

    operation = (harp_operation_collocation_filter *)malloc(sizeof(harp_operation_collocation_filter));
    if (operation == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_operation_collocation_filter), __FILE__, __LINE__);
        return -1;
    }
    operation->type = operation_collocation_filter;
    operation->eval = eval_collocation;
    operation->filename = NULL;
    operation->filter_type = filter_type;
    operation->collocation_mask = NULL;
    operation->num_values = 0;
    operation->value = NULL;

    operation->filename = strdup(filename);
    if (operation->filename == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        collocation_filter_delete(operation);
        return -1;
    }

    *new_operation = (harp_operation *)operation;
    return 0;
}

int harp_operation_comparison_filter_new(const char *variable_name, harp_comparison_operator_type operator_type,
                                         double value, const char *unit, harp_operation **new_operation)
{
    harp_operation_comparison_filter *operation;

    assert(variable_name != NULL);

    operation = (harp_operation_comparison_filter *)malloc(sizeof(harp_operation_comparison_filter));
    if (operation == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_operation_comparison_filter), __FILE__, __LINE__);
        return -1;
    }
    operation->type = operation_comparison_filter;
    operation->eval = eval_comparison;
    operation->variable_name = NULL;
    operation->operator_type = operator_type;
    operation->value = value;
    operation->unit = NULL;
    operation->unit_converter = NULL;

    operation->variable_name = strdup(variable_name);
    if (operation->variable_name == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        comparison_filter_delete(operation);
        return -1;
    }

    if (unit != NULL)
    {
        operation->unit = strdup(unit);
        if (operation->unit == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                           __LINE__);
            comparison_filter_delete(operation);
            return -1;
        }
    }

    *new_operation = (harp_operation *)operation;
    return 0;
}

int harp_operation_derive_variable_new(const char *variable_name, const harp_data_type *data_type, int num_dimensions,
                                       const harp_dimension_type *dimension_type, const char *unit,
                                       harp_operation **new_operation)
{
    harp_operation_derive_variable *operation;
    int i;

    assert(variable_name != NULL);

    operation = (harp_operation_derive_variable *)malloc(sizeof(harp_operation_derive_variable));
    if (operation == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_operation_derive_variable), __FILE__, __LINE__);
        return -1;
    }
    operation->type = operation_derive_variable;
    operation->variable_name = NULL;
    if (data_type != NULL)
    {
        operation->has_data_type = 1;
        operation->data_type = *data_type;
    }
    else
    {
        operation->has_data_type = 0;
        operation->data_type = harp_type_double;
    }
    if (num_dimensions < 0)
    {
        operation->has_dimensions = 0;
        operation->num_dimensions = 0;
    }
    else
    {
        operation->has_dimensions = 1;
        operation->num_dimensions = num_dimensions;
    }
    operation->unit = NULL;

    operation->variable_name = strdup(variable_name);
    if (operation->variable_name == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        derive_variable_delete(operation);
        return -1;
    }

    for (i = 0; i < num_dimensions; i++)
    {
        operation->dimension_type[i] = dimension_type[i];
    }

    if (unit != NULL)
    {
        operation->unit = strdup(unit);
        if (operation->unit == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                           __LINE__);
            derive_variable_delete(operation);
            return -1;
        }
    }

    *new_operation = (harp_operation *)operation;
    return 0;
}

int harp_operation_derive_smoothed_column_collocated_dataset_new(const char *variable_name, int num_dimensions,
                                                                 const harp_dimension_type *dimension_type,
                                                                 const char *unit, const char *axis_variable_name,
                                                                 const char *axis_unit, const char *collocation_result,
                                                                 const char target_dataset, const char *dataset_dir,
                                                                 harp_operation **new_operation)
{
    harp_operation_derive_smoothed_column_collocated_dataset *operation;
    int i;

    assert(variable_name != NULL);
    assert(axis_variable_name != NULL);
    assert(axis_unit != NULL);
    assert(collocation_result != NULL);
    assert(dataset_dir != NULL);

    operation =
        (harp_operation_derive_smoothed_column_collocated_dataset
         *)malloc(sizeof(harp_operation_derive_smoothed_column_collocated_dataset));
    if (operation == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_operation_derive_smoothed_column_collocated_dataset), __FILE__, __LINE__);
        return -1;
    }
    operation->type = operation_derive_smoothed_column_collocated_dataset;
    operation->variable_name = NULL;
    operation->num_dimensions = num_dimensions;
    operation->unit = NULL;
    operation->axis_variable_name = NULL;
    operation->axis_unit = NULL;
    operation->collocation_result = NULL;
    operation->target_dataset = target_dataset;
    operation->dataset_dir = NULL;

    operation->variable_name = strdup(variable_name);
    if (operation->variable_name == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        derive_smoothed_column_collocated_dataset_delete(operation);
        return -1;
    }
    operation->axis_variable_name = strdup(axis_variable_name);
    if (operation->axis_variable_name == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        derive_smoothed_column_collocated_dataset_delete(operation);
        return -1;
    }
    operation->axis_unit = strdup(axis_unit);
    if (operation->axis_unit == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        derive_smoothed_column_collocated_dataset_delete(operation);
        return -1;
    }
    operation->collocation_result = strdup(collocation_result);
    if (operation->collocation_result == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        derive_smoothed_column_collocated_dataset_delete(operation);
        return -1;
    }
    operation->dataset_dir = strdup(dataset_dir);
    if (operation->dataset_dir == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        derive_smoothed_column_collocated_dataset_delete(operation);
        return -1;
    }

    for (i = 0; i < num_dimensions; i++)
    {
        operation->dimension_type[i] = dimension_type[i];
    }

    if (unit != NULL)
    {
        operation->unit = strdup(unit);
        if (operation->unit == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                           __LINE__);
            derive_smoothed_column_collocated_dataset_delete(operation);
            return -1;
        }
    }

    *new_operation = (harp_operation *)operation;
    return 0;
}

int harp_operation_derive_smoothed_column_collocated_product_new(const char *variable_name, int num_dimensions,
                                                                 const harp_dimension_type *dimension_type,
                                                                 const char *unit, const char *axis_variable_name,
                                                                 const char *axis_unit, const char *filename,
                                                                 harp_operation **new_operation)
{
    harp_operation_derive_smoothed_column_collocated_product *operation;
    int i;

    assert(variable_name != NULL);
    assert(axis_variable_name != NULL);
    assert(axis_unit != NULL);
    assert(filename != NULL);

    operation =
        (harp_operation_derive_smoothed_column_collocated_product
         *)malloc(sizeof(harp_operation_derive_smoothed_column_collocated_product));
    if (operation == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_operation_derive_smoothed_column_collocated_product), __FILE__, __LINE__);
        return -1;
    }
    operation->type = operation_derive_smoothed_column_collocated_product;
    operation->variable_name = NULL;
    operation->num_dimensions = num_dimensions;
    operation->unit = NULL;
    operation->axis_variable_name = NULL;
    operation->axis_unit = NULL;
    operation->filename = NULL;

    operation->variable_name = strdup(variable_name);
    if (operation->variable_name == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        derive_smoothed_column_collocated_product_delete(operation);
        return -1;
    }
    operation->axis_variable_name = strdup(axis_variable_name);
    if (operation->axis_variable_name == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        derive_smoothed_column_collocated_product_delete(operation);
        return -1;
    }
    operation->axis_unit = strdup(axis_unit);
    if (operation->axis_unit == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        derive_smoothed_column_collocated_product_delete(operation);
        return -1;
    }
    operation->filename = strdup(filename);
    if (operation->filename == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        derive_smoothed_column_collocated_product_delete(operation);
        return -1;
    }

    for (i = 0; i < num_dimensions; i++)
    {
        operation->dimension_type[i] = dimension_type[i];
    }

    if (unit != NULL)
    {
        operation->unit = strdup(unit);
        if (operation->unit == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                           __LINE__);
            derive_smoothed_column_collocated_product_delete(operation);
            return -1;
        }
    }

    *new_operation = (harp_operation *)operation;
    return 0;
}

int harp_operation_exclude_variable_new(int num_variables, const char **variable_name, harp_operation **new_operation)
{
    harp_operation_exclude_variable *operation;

    assert(variable_name != NULL);

    operation = (harp_operation_exclude_variable *)malloc(sizeof(harp_operation_exclude_variable));
    if (operation == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_operation_exclude_variable), __FILE__, __LINE__);
        return -1;
    }
    operation->type = operation_exclude_variable;
    operation->num_variables = num_variables;
    operation->variable_name = NULL;

    if (num_variables > 0)
    {
        int i;

        operation->variable_name = (char **)malloc(num_variables * sizeof(char *));
        if (operation->variable_name == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           num_variables * sizeof(char *), __FILE__, __LINE__);
            exclude_variable_delete(operation);
            return -1;
        }

        for (i = 0; i < num_variables; i++)
        {
            operation->variable_name[i] = strdup(variable_name[i]);
            if (operation->variable_name[i] == NULL)
            {
                harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                               __LINE__);
                exclude_variable_delete(operation);
                return -1;
            }
        }
    }

    *new_operation = (harp_operation *)operation;
    return 0;
}

int harp_operation_flatten_new(const harp_dimension_type dimension_type, harp_operation **new_operation)
{
    harp_operation_flatten *operation;

    operation = (harp_operation_flatten *)malloc(sizeof(harp_operation_flatten));
    if (operation == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_operation_flatten), __FILE__, __LINE__);
        return -1;
    }
    operation->type = operation_flatten;
    operation->dimension_type = dimension_type;

    *new_operation = (harp_operation *)operation;
    return 0;
}

int harp_operation_keep_variable_new(int num_variables, const char **variable_name, harp_operation **new_operation)
{
    harp_operation_keep_variable *operation;

    assert(variable_name != NULL);

    operation = (harp_operation_keep_variable *)malloc(sizeof(harp_operation_keep_variable));
    if (operation == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_operation_keep_variable), __FILE__, __LINE__);
        return -1;
    }
    operation->type = operation_keep_variable;
    operation->num_variables = num_variables;
    operation->variable_name = NULL;

    if (num_variables > 0)
    {
        int i;

        operation->variable_name = (char **)malloc(num_variables * sizeof(char *));
        if (operation->variable_name == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           num_variables * sizeof(char *), __FILE__, __LINE__);
            keep_variable_delete(operation);
            return -1;
        }

        for (i = 0; i < num_variables; i++)
        {
            operation->variable_name[i] = strdup(variable_name[i]);
            if (operation->variable_name[i] == NULL)
            {
                harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                               __LINE__);
                keep_variable_delete(operation);
                return -1;
            }
        }
    }

    *new_operation = (harp_operation *)operation;
    return 0;
}

int harp_operation_longitude_range_filter_new(double min, const char *min_unit, double max, const char *max_unit,
                                              harp_operation **new_operation)
{
    harp_operation_longitude_range_filter *operation;

    operation = (harp_operation_longitude_range_filter *)malloc(sizeof(harp_operation_longitude_range_filter));
    if (operation == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_operation_longitude_range_filter), __FILE__, __LINE__);
        return -1;
    }
    operation->type = operation_longitude_range_filter;
    operation->eval = eval_longitude_range;
    operation->min = min;
    operation->max = max;
    operation->unit_converter = NULL;

    if (min_unit != NULL)
    {
        if (harp_unit_compare(min_unit, "degree_east") != 0)
        {
            if (harp_convert_unit(min_unit, "degree_east", 1, &operation->min) != 0)
            {
                return -1;
            }
        }
    }
    if (max_unit != NULL)
    {
        if (harp_unit_compare(max_unit, "degree_east") != 0)
        {
            if (harp_convert_unit(max_unit, "degree_east", 1, &operation->max) != 0)
            {
                return -1;
            }
        }
    }

    *new_operation = (harp_operation *)operation;
    return 0;
}

int harp_operation_membership_filter_new(const char *variable_name, harp_membership_operator_type operator_type,
                                         int num_values, const double *value, const char *unit,
                                         harp_operation **new_operation)
{
    harp_operation_membership_filter *operation;

    assert(variable_name != NULL);
    assert(num_values == 0 || value != NULL);

    operation = (harp_operation_membership_filter *)malloc(sizeof(harp_operation_membership_filter));
    if (operation == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_operation_membership_filter), __FILE__, __LINE__);
        return -1;
    }
    operation->type = operation_membership_filter;
    operation->eval = eval_membership;
    operation->variable_name = NULL;
    operation->operator_type = operator_type;
    operation->num_values = num_values;
    operation->value = NULL;
    operation->unit = NULL;
    operation->unit_converter = NULL;

    operation->variable_name = strdup(variable_name);
    if (operation->variable_name == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        membership_filter_delete(operation);
        return -1;
    }

    if (value != NULL)
    {
        operation->value = (double *)malloc(num_values * sizeof(double));
        if (operation->value == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           num_values * sizeof(double), __FILE__, __LINE__);
            membership_filter_delete(operation);
            return -1;
        }

        memcpy(operation->value, value, num_values * sizeof(double));
    }

    if (unit != NULL)
    {
        operation->unit = strdup(unit);
        if (operation->unit == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                           __LINE__);
            membership_filter_delete(operation);
            return -1;
        }
    }

    *new_operation = (harp_operation *)operation;
    return 0;
}

int harp_operation_point_distance_filter_new(double latitude, const char *latitude_unit, double longitude,
                                             const char *longitude_unit, double distance, const char *distance_unit,
                                             harp_operation **new_operation)
{
    harp_operation_point_distance_filter *operation;

    operation = (harp_operation_point_distance_filter *)malloc(sizeof(harp_operation_point_distance_filter));
    if (operation == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_operation_point_distance_filter), __FILE__, __LINE__);
        return -1;
    }
    operation->type = operation_point_distance_filter;
    operation->eval = eval_point_distance;
    operation->point.lat = latitude;
    operation->point.lon = longitude;
    operation->distance = distance;

    /* convert parameters to internal units */
    if (latitude_unit != NULL)
    {
        if (harp_unit_compare(latitude_unit, "degree_north") != 0)
        {
            if (harp_convert_unit(latitude_unit, "degree_north", 1, &operation->point.lat) != 0)
            {
                return -1;
            }
        }
    }
    if (longitude_unit != NULL)
    {
        if (harp_unit_compare(longitude_unit, "degree_east") != 0)
        {
            if (harp_convert_unit(longitude_unit, "degree_east", 1, &operation->point.lon) != 0)
            {
                return -1;
            }
        }
    }
    if (distance_unit != NULL)
    {
        if (harp_unit_compare(distance_unit, "m") != 0)
        {
            if (harp_convert_unit(distance_unit, "m", 1, &operation->distance) != 0)
            {
                return -1;
            }
        }
    }

    harp_spherical_point_rad_from_deg(&operation->point);
    harp_spherical_point_check(&operation->point);

    *new_operation = (harp_operation *)operation;
    return 0;
}

int harp_operation_point_in_area_filter_new(const char *filename, int num_latitudes, double *latitude,
                                            const char *latitude_unit, int num_longitudes, double *longitude,
                                            const char *longitude_unit, harp_operation **new_operation)
{
    harp_operation_point_in_area_filter *operation;

    if (num_latitudes != num_longitudes)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "number of latitude and longitude points need to be the same");
        return -1;
    }
    if (filename != NULL && num_latitudes > 0)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "cannot provide both area mask file and individual area (%s:%u)",
                       __FILE__, __LINE__);
        return -1;
    }
    if (filename == NULL && num_latitudes == 0)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "neither area mask file nor individual area provided (%s:%u)",
                       __FILE__, __LINE__);
        return -1;
    }
    operation = (harp_operation_point_in_area_filter *)malloc(sizeof(harp_operation_point_in_area_filter));
    if (operation == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_operation_point_in_area_filter), __FILE__, __LINE__);
        return -1;
    }
    operation->type = operation_point_in_area_filter;
    operation->eval = eval_point_in_area;
    operation->filename = NULL;
    operation->area_mask = NULL;

    if (filename != NULL)
    {
        operation->filename = strdup(filename);
        if (operation->filename == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                           __LINE__);
            point_in_area_filter_delete(operation);
            return -1;
        }
        if (harp_area_mask_read(operation->filename, &operation->area_mask) != 0)
        {
            point_in_area_filter_delete(operation);
            return -1;
        }
    }
    else
    {
        if (harp_area_mask_new(&operation->area_mask) != 0)
        {
            point_in_area_filter_delete(operation);
            return -1;
        }
        if (add_latitude_longitude_bounds_to_area_mask(operation->area_mask, num_latitudes, latitude, latitude_unit,
                                                       longitude, longitude_unit) != 0)
        {
            point_in_area_filter_delete(operation);
            return -1;
        }
    }

    *new_operation = (harp_operation *)operation;
    return 0;
}

int harp_operation_regrid_new(harp_dimension_type dimension_type, const char *axis_variable_name, const char *axis_unit,
                              long num_values, double *values, harp_operation **new_operation)
{
    harp_operation_regrid *operation;
    long i;

    assert(axis_variable_name != NULL);
    assert(axis_unit != NULL);

    operation = (harp_operation_regrid *)malloc(sizeof(harp_operation_regrid));
    if (operation == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_operation_regrid), __FILE__, __LINE__);
        return -1;
    }
    operation->type = operation_regrid;
    operation->axis_variable = NULL;

    if (harp_variable_new(axis_variable_name, harp_type_double, 1, &dimension_type, &num_values,
                          &operation->axis_variable) != 0)
    {
        regrid_delete(operation);
        return -1;
    }
    if (harp_variable_set_unit(operation->axis_variable, axis_unit) != 0)
    {
        regrid_delete(operation);
        return -1;
    }
    for (i = 0; i < num_values; i++)
    {
        operation->axis_variable->data.double_data[i] = values[i];
    }

    *new_operation = (harp_operation *)operation;
    return 0;
}

int harp_operation_regrid_collocated_dataset_new(harp_dimension_type dimension_type, const char *axis_variable_name,
                                                 const char *axis_unit, const char *collocation_result,
                                                 const char target_dataset, const char *dataset_dir,
                                                 harp_operation **new_operation)
{
    harp_operation_regrid_collocated_dataset *operation;

    assert(axis_variable_name != NULL);
    assert(axis_unit != NULL);
    assert(collocation_result != NULL);
    assert(dataset_dir != NULL);

    operation = (harp_operation_regrid_collocated_dataset *)malloc(sizeof(harp_operation_regrid_collocated_dataset));
    if (operation == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_operation_regrid_collocated_dataset), __FILE__, __LINE__);
        return -1;
    }
    operation->type = operation_regrid_collocated_dataset;
    operation->dimension_type = dimension_type;
    operation->axis_variable_name = NULL;
    operation->axis_unit = NULL;
    operation->collocation_result = NULL;
    operation->target_dataset = target_dataset;
    operation->dataset_dir = NULL;

    operation->axis_variable_name = strdup(axis_variable_name);
    if (operation->axis_variable_name == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        regrid_collocated_dataset_delete(operation);
        return -1;
    }
    operation->axis_unit = strdup(axis_unit);
    if (operation->axis_unit == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        regrid_collocated_dataset_delete(operation);
        return -1;
    }
    operation->collocation_result = strdup(collocation_result);
    if (operation->collocation_result == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        regrid_collocated_dataset_delete(operation);
        return -1;
    }
    operation->dataset_dir = strdup(dataset_dir);
    if (operation->dataset_dir == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        regrid_collocated_dataset_delete(operation);
        return -1;
    }

    *new_operation = (harp_operation *)operation;
    return 0;
}

int harp_operation_regrid_collocated_product_new(harp_dimension_type dimension_type, const char *axis_variable_name,
                                                 const char *axis_unit, const char *filename,
                                                 harp_operation **new_operation)
{
    harp_operation_regrid_collocated_product *operation;

    assert(axis_variable_name != NULL);
    assert(axis_unit != NULL);
    assert(filename != NULL);

    operation = (harp_operation_regrid_collocated_product *)malloc(sizeof(harp_operation_regrid_collocated_product));
    if (operation == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_operation_regrid_collocated_product), __FILE__, __LINE__);
        return -1;
    }
    operation->type = operation_regrid_collocated_product;
    operation->dimension_type = dimension_type;
    operation->axis_variable_name = NULL;
    operation->axis_unit = NULL;
    operation->filename = NULL;

    operation->axis_variable_name = strdup(axis_variable_name);
    if (operation->axis_variable_name == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        regrid_collocated_product_delete(operation);
        return -1;
    }
    operation->axis_unit = strdup(axis_unit);
    if (operation->axis_unit == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        regrid_collocated_product_delete(operation);
        return -1;
    }
    operation->filename = strdup(filename);
    if (operation->filename == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        regrid_collocated_product_delete(operation);
        return -1;
    }

    *new_operation = (harp_operation *)operation;
    return 0;
}

int harp_operation_rename_new(const char *variable_name, const char *new_variable_name, harp_operation **new_operation)
{
    harp_operation_rename *operation;

    assert(variable_name != NULL);
    assert(new_variable_name != NULL);

    operation = (harp_operation_rename *)malloc(sizeof(harp_operation_rename));
    if (operation == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_operation_rename), __FILE__, __LINE__);
        return -1;
    }
    operation->type = operation_rename;
    operation->variable_name = NULL;
    operation->new_variable_name = NULL;

    operation->variable_name = strdup(variable_name);
    if (operation->variable_name == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        rename_delete(operation);
        return -1;
    }
    operation->new_variable_name = strdup(new_variable_name);
    if (operation->new_variable_name == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        rename_delete(operation);
        return -1;
    }

    *new_operation = (harp_operation *)operation;
    return 0;
}

int harp_operation_set_new(const char *option, const char *value, harp_operation **new_operation)
{
    harp_operation_set *operation;

    assert(option != NULL);
    assert(value != NULL);

    operation = (harp_operation_set *)malloc(sizeof(harp_operation_set));
    if (operation == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_operation_set), __FILE__, __LINE__);
        return -1;
    }
    operation->type = operation_set;
    operation->option = NULL;
    operation->value = NULL;

    operation->option = strdup(option);
    if (operation->option == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        set_delete(operation);
        return -1;
    }
    operation->value = strdup(value);
    if (operation->value == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        set_delete(operation);
        return -1;
    }

    *new_operation = (harp_operation *)operation;
    return 0;
}

int harp_operation_smooth_collocated_dataset_new(int num_variables, const char **variable_name,
                                                 harp_dimension_type dimension_type, const char *axis_variable_name,
                                                 const char *axis_unit, const char *collocation_result,
                                                 const char target_dataset, const char *dataset_dir,
                                                 harp_operation **new_operation)
{
    harp_operation_smooth_collocated_dataset *operation;

    assert(variable_name != NULL);
    assert(axis_variable_name != NULL);
    assert(axis_unit != NULL);
    assert(collocation_result != NULL);
    assert(dataset_dir != NULL);

    operation = (harp_operation_smooth_collocated_dataset *)malloc(sizeof(harp_operation_smooth_collocated_dataset));
    if (operation == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_operation_smooth_collocated_dataset), __FILE__, __LINE__);
        return -1;
    }
    operation->type = operation_smooth_collocated_dataset;
    operation->num_variables = 0;
    operation->variable_name = NULL;
    operation->dimension_type = dimension_type;
    operation->axis_variable_name = NULL;
    operation->axis_unit = NULL;
    operation->collocation_result = NULL;
    operation->target_dataset = target_dataset;
    operation->dataset_dir = NULL;

    operation->axis_variable_name = strdup(axis_variable_name);
    if (operation->axis_variable_name == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        smooth_collocated_dataset_delete(operation);
        return -1;
    }
    operation->axis_unit = strdup(axis_unit);
    if (operation->axis_unit == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        smooth_collocated_dataset_delete(operation);
        return -1;
    }
    operation->collocation_result = strdup(collocation_result);
    if (operation->collocation_result == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        smooth_collocated_dataset_delete(operation);
        return -1;
    }
    operation->dataset_dir = strdup(dataset_dir);
    if (operation->dataset_dir == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        smooth_collocated_dataset_delete(operation);
        return -1;
    }

    if (num_variables > 0)
    {
        int i;

        operation->variable_name = (char **)malloc(num_variables * sizeof(char *));
        if (operation->variable_name == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           num_variables * sizeof(char *), __FILE__, __LINE__);
            smooth_collocated_dataset_delete(operation);
            return -1;
        }
        for (i = 0; i < num_variables; i++)
        {
            operation->variable_name[i] = NULL;
        }
        for (i = 0; i < num_variables; i++)
        {
            operation->variable_name[i] = strdup(variable_name[i]);
            if (operation->variable_name[i] == NULL)
            {
                harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                               __LINE__);
                smooth_collocated_dataset_delete(operation);
                return -1;
            }
            operation->num_variables++;
        }
    }

    *new_operation = (harp_operation *)operation;
    return 0;
}

int harp_operation_smooth_collocated_product_new(int num_variables, const char **variable_name,
                                                 harp_dimension_type dimension_type, const char *axis_variable_name,
                                                 const char *axis_unit, const char *filename,
                                                 harp_operation **new_operation)
{
    harp_operation_smooth_collocated_product *operation;

    assert(variable_name != NULL);
    assert(axis_variable_name != NULL);
    assert(axis_unit != NULL);
    assert(filename != NULL);

    operation = (harp_operation_smooth_collocated_product *)malloc(sizeof(harp_operation_smooth_collocated_product));
    if (operation == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_operation_smooth_collocated_product), __FILE__, __LINE__);
        return -1;
    }
    operation->type = operation_smooth_collocated_product;
    operation->num_variables = 0;
    operation->variable_name = NULL;
    operation->dimension_type = dimension_type;
    operation->axis_variable_name = NULL;
    operation->axis_unit = NULL;
    operation->filename = NULL;

    operation->axis_variable_name = strdup(axis_variable_name);
    if (operation->axis_variable_name == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        smooth_collocated_product_delete(operation);
        return -1;
    }
    operation->axis_unit = strdup(axis_unit);
    if (operation->axis_unit == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        smooth_collocated_product_delete(operation);
        return -1;
    }
    operation->filename = strdup(filename);
    if (operation->filename == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        smooth_collocated_product_delete(operation);
        return -1;
    }

    if (num_variables > 0)
    {
        int i;

        operation->variable_name = (char **)malloc(num_variables * sizeof(char *));
        if (operation->variable_name == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           num_variables * sizeof(char *), __FILE__, __LINE__);
            smooth_collocated_product_delete(operation);
            return -1;
        }
        for (i = 0; i < num_variables; i++)
        {
            operation->variable_name[i] = NULL;
        }
        for (i = 0; i < num_variables; i++)
        {
            operation->variable_name[i] = strdup(variable_name[i]);
            if (operation->variable_name[i] == NULL)
            {
                harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                               __LINE__);
                smooth_collocated_product_delete(operation);
                return -1;
            }
            operation->num_variables++;
        }
    }

    *new_operation = (harp_operation *)operation;
    return 0;
}

int harp_operation_sort_new(const char *variable_name, harp_operation **new_operation)
{
    harp_operation_sort *operation;

    assert(variable_name != NULL);

    operation = (harp_operation_sort *)malloc(sizeof(harp_operation_sort));
    if (operation == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_operation_sort), __FILE__, __LINE__);
        return -1;
    }
    operation->type = operation_sort;
    operation->variable_name = NULL;

    operation->variable_name = strdup(variable_name);
    if (operation->variable_name == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        sort_delete(operation);
        return -1;
    }

    *new_operation = (harp_operation *)operation;
    return 0;
}

int harp_operation_string_comparison_filter_new(const char *variable_name, harp_comparison_operator_type operator_type,
                                                const char *value, harp_operation **new_operation)
{
    harp_operation_string_comparison_filter *operation;

    assert(variable_name != NULL);
    assert(value != NULL);

    operation = (harp_operation_string_comparison_filter *)malloc(sizeof(harp_operation_string_comparison_filter));
    if (operation == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_operation_string_comparison_filter), __FILE__, __LINE__);
        return -1;
    }
    operation->type = operation_string_comparison_filter;
    operation->eval = eval_string_comparison;
    operation->variable_name = NULL;
    operation->operator_type = operator_type;

    operation->variable_name = strdup(variable_name);
    if (operation->variable_name == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        string_comparison_filter_delete(operation);
        return -1;
    }

    operation->value = strdup(value);
    if (operation->value == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        string_comparison_filter_delete(operation);
        return -1;
    }

    *new_operation = (harp_operation *)operation;
    return 0;
}

int harp_operation_string_membership_filter_new(const char *variable_name, harp_membership_operator_type operator_type,
                                                int num_values, const char **value, harp_operation **new_operation)
{
    harp_operation_string_membership_filter *operation;

    assert(variable_name != NULL);
    assert(num_values == 0 || value != NULL);

    operation = (harp_operation_string_membership_filter *)malloc(sizeof(harp_operation_string_membership_filter));
    if (operation == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_operation_string_membership_filter), __FILE__, __LINE__);
        return -1;
    }
    operation->type = operation_string_membership_filter;
    operation->eval = eval_string_membership;
    operation->variable_name = NULL;
    operation->operator_type = operator_type;
    operation->num_values = num_values;
    operation->value = NULL;

    operation->variable_name = strdup(variable_name);
    if (operation->variable_name == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        string_membership_filter_delete(operation);
        return -1;
    }

    if (value != NULL)
    {
        int i;

        operation->value = (char **)malloc(num_values * sizeof(char *));
        if (operation->value == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           num_values * sizeof(char *), __FILE__, __LINE__);
            string_membership_filter_delete(operation);
            return -1;
        }

        for (i = 0; i < num_values; i++)
        {
            if (value[i] == NULL)
            {
                operation->value[i] = NULL;
            }
            else
            {
                operation->value[i] = strdup(value[i]);
                if (operation->value[i] == NULL)
                {
                    harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)",
                                   __FILE__, __LINE__);
                    string_membership_filter_delete(operation);
                    return -1;
                }
            }
        }
    }

    *new_operation = (harp_operation *)operation;
    return 0;
}

int harp_operation_valid_range_filter_new(const char *variable_name, harp_operation **new_operation)
{
    harp_operation_valid_range_filter *operation;

    operation = (harp_operation_valid_range_filter *)malloc(sizeof(harp_operation_valid_range_filter));
    if (operation == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_operation_valid_range_filter), __FILE__, __LINE__);
        return -1;
    }
    operation->type = operation_valid_range_filter;
    operation->eval = eval_valid_range;
    operation->variable_name = NULL;
    operation->valid_min = harp_mininf();
    operation->valid_max = harp_plusinf();

    operation->variable_name = strdup(variable_name);
    if (operation->variable_name == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        valid_range_filter_delete(operation);
        return -1;
    }

    *new_operation = (harp_operation *)operation;
    return 0;
}

int harp_operation_wrap_new(const char *variable_name, const char *unit, double min, double max,
                            harp_operation **new_operation)
{
    harp_operation_wrap *operation;

    operation = (harp_operation_wrap *)malloc(sizeof(harp_operation_wrap));
    if (operation == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_operation_wrap), __FILE__, __LINE__);
        return -1;
    }
    operation->type = operation_wrap;
    operation->variable_name = NULL;
    operation->unit = NULL;
    operation->min = min;
    operation->max = max;

    operation->variable_name = strdup(variable_name);
    if (operation->variable_name == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        wrap_delete(operation);
        return -1;
    }
    if (unit != NULL)
    {
        operation->unit = strdup(unit);
        if (operation->unit == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                           __LINE__);
            wrap_delete(operation);
            return -1;
        }
    }

    *new_operation = (harp_operation *)operation;
    return 0;
}

int harp_operation_get_variable_name(const harp_operation *operation, const char **variable_name)
{
    switch (operation->type)
    {
        case operation_bit_mask_filter:
            *variable_name = ((harp_operation_bit_mask_filter *)operation)->variable_name;
            break;
        case operation_collocation_filter:
            *variable_name = "index";
            break;
        case operation_comparison_filter:
            *variable_name = ((harp_operation_comparison_filter *)operation)->variable_name;
            break;
        case operation_longitude_range_filter:
            *variable_name = "longitude";
            break;
        case operation_membership_filter:
            *variable_name = ((harp_operation_membership_filter *)operation)->variable_name;
            break;
        case operation_string_comparison_filter:
            *variable_name = ((harp_operation_string_comparison_filter *)operation)->variable_name;
            break;
        case operation_string_membership_filter:
            *variable_name = ((harp_operation_string_membership_filter *)operation)->variable_name;
            break;
        case operation_valid_range_filter:
            *variable_name = ((harp_operation_valid_range_filter *)operation)->variable_name;
            break;
        default:
            harp_set_error(HARP_ERROR_OPERATION, "operation has no variable name");
            return -1;
    }

    return 0;
}

int harp_operation_is_point_filter(const harp_operation *operation)
{
    switch (operation->type)
    {
        case operation_point_distance_filter:
        case operation_point_in_area_filter:
            return 1;
        default:
            return 0;
    }
}

int harp_operation_is_polygon_filter(const harp_operation *operation)
{
    switch (operation->type)
    {
        case operation_area_covers_area_filter:
        case operation_area_covers_point_filter:
        case operation_area_inside_area_filter:
        case operation_area_intersects_area_filter:
            return 1;
        default:
            return 0;
    }
}

int harp_operation_is_string_value_filter(const harp_operation *operation)
{
    switch (operation->type)
    {
        case operation_string_comparison_filter:
        case operation_string_membership_filter:
            return 1;
        default:
            return 0;
    }
}

int harp_operation_is_value_filter(const harp_operation *operation)
{
    switch (operation->type)
    {
        case operation_bit_mask_filter:
        case operation_comparison_filter:
        case operation_longitude_range_filter:
        case operation_membership_filter:
        case operation_string_comparison_filter:
        case operation_string_membership_filter:
        case operation_valid_range_filter:
            return 1;
        default:
            return 0;
    }
}

int harp_operation_prepare_collocation_filter(harp_operation *operation, const char *source_product)
{
    harp_operation_collocation_filter *collocation_operation = (harp_operation_collocation_filter *)operation;
    harp_collocation_mask *collocation_mask;
    int i;

    /* make sure we start with a clean state */
    if (collocation_operation->collocation_mask != NULL)
    {
        harp_collocation_mask_delete(collocation_operation->collocation_mask);
    }
    if (collocation_operation->value != NULL)
    {
        free(collocation_operation->value);
    }
    collocation_operation->collocation_mask = NULL;
    collocation_operation->num_values = 0;
    collocation_operation->value = NULL;

    if (harp_collocation_mask_import(collocation_operation->filename, collocation_operation->filter_type,
                                     source_product, &collocation_mask) != 0)
    {
        return -1;
    }
    collocation_operation->collocation_mask = collocation_mask;

    collocation_operation->value = (int32_t *)malloc(collocation_mask->num_index_pairs * sizeof(int32_t));
    if (collocation_operation->value == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       collocation_mask->num_index_pairs * sizeof(int32_t), __FILE__, __LINE__);
        return -1;
    }
    for (i = 0; i < collocation_mask->num_index_pairs; i++)
    {
        collocation_operation->value[i] = collocation_mask->index_pair[i].index;
    }

    /* Sort the list of indices */
    qsort(collocation_operation->value, collocation_mask->num_index_pairs, sizeof(int32_t), compare_int32);

    /* Remove duplicate indices */
    for (i = 0; i < collocation_mask->num_index_pairs; i++)
    {
        if (i == 0 || collocation_operation->value[i] != collocation_operation->value[i - 1])
        {
            collocation_operation->value[collocation_operation->num_values] = collocation_operation->value[i];
            collocation_operation->num_values++;
        }
    }

    return 0;
}

int harp_operation_set_valid_range(harp_operation *operation, harp_data_type data_type, harp_scalar valid_min,
                                   harp_scalar valid_max)
{
    double double_min, double_max;

    switch (data_type)
    {
        case harp_type_int8:
            double_min = (double)valid_min.int8_data;
            double_max = (double)valid_max.int8_data;
            break;
        case harp_type_int16:
            double_min = (double)valid_min.int16_data;
            double_max = (double)valid_max.int16_data;
            break;
        case harp_type_int32:
            double_min = (double)valid_min.int32_data;
            double_max = (double)valid_max.int32_data;
            break;
        case harp_type_float:
            double_min = (double)valid_min.float_data;
            double_max = (double)valid_max.float_data;
            break;
        case harp_type_double:
            double_min = valid_min.double_data;
            double_max = valid_max.double_data;
            break;
        default:
            assert(0);
            exit(1);
    }

    if (operation->type != operation_valid_range_filter)
    {
        harp_set_error(HARP_ERROR_OPERATION, "operation has no valid range");
        return -1;
    }

    ((harp_operation_valid_range_filter *)operation)->valid_min = double_min;
    ((harp_operation_valid_range_filter *)operation)->valid_max = double_max;

    return 0;
}

int harp_operation_set_value_unit(harp_operation *operation, const char *unit)
{
    const char *target_unit;
    harp_unit_converter **unit_converter;

    switch (operation->type)
    {
        case operation_comparison_filter:
            target_unit = ((harp_operation_comparison_filter *)operation)->unit;
            unit_converter = &((harp_operation_comparison_filter *)operation)->unit_converter;
            break;
        case operation_longitude_range_filter:
            target_unit = "degree_east";
            unit_converter = &((harp_operation_longitude_range_filter *)operation)->unit_converter;
            break;
        case operation_membership_filter:
            target_unit = ((harp_operation_membership_filter *)operation)->unit;
            unit_converter = &((harp_operation_membership_filter *)operation)->unit_converter;
            break;
        default:
            /* no need to perform unit conversion */
            return 0;
    }

    /* remove previous unit converter if there was one */
    if (*unit_converter != NULL)
    {
        harp_unit_converter_delete(*unit_converter);
        *unit_converter = NULL;
    }

    /* if the operation did not have a unit then we don't have to perform a unit conversion */
    if (target_unit == NULL)
    {
        return 0;
    }

    if (harp_unit_compare(unit, target_unit) == 0)
    {
        /* no need to perform unit conversion */
        return 0;
    }

    return harp_unit_converter_new(unit, target_unit, unit_converter);
}
