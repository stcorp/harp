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

#include "harp-action.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static void collocation_filter_args_delete(harp_collocation_filter_args *args)
{
    if (args != NULL)
    {
        if (args->filename != NULL)
        {
            free(args->filename);
        }

        free(args);
    }
}

static void comparison_filter_args_delete(harp_comparison_filter_args *args)
{
    if (args != NULL)
    {
        if (args->variable_name != NULL)
        {
            free(args->variable_name);
        }

        if (args->unit != NULL)
        {
            free(args->unit);
        }

        free(args);
    }
}

static void string_comparison_filter_args_delete(harp_string_comparison_filter_args *args)
{
    if (args != NULL)
    {
        if (args->variable_name != NULL)
        {
            free(args->variable_name);
        }

        if (args->value != NULL)
        {
            free(args->value);
        }

        free(args);
    }
}

static void bit_mask_filter_args_delete(harp_bit_mask_filter_args *args)
{
    if (args != NULL)
    {
        if (args->variable_name != NULL)
        {
            free(args->variable_name);
        }

        free(args);
    }
}

static void membership_filter_args_delete(harp_membership_filter_args *args)
{
    if (args != NULL)
    {
        if (args->variable_name != NULL)
        {
            free(args->variable_name);
        }

        if (args->value != NULL)
        {
            free(args->value);
        }

        if (args->unit != NULL)
        {
            free(args->unit);
        }

        free(args);
    }
}

static void string_membership_filter_args_delete(harp_string_membership_filter_args *args)
{
    if (args != NULL)
    {
        if (args->variable_name != NULL)
        {
            free(args->variable_name);
        }

        if (args->value != NULL)
        {
            int i;

            for (i = 0; i < args->num_values; i++)
            {
                if (args->value[i] != NULL)
                {
                    free(args->value[i]);
                }
            }

            free(args->value);
        }

        free(args);
    }
}

static void valid_range_filter_args_delete(harp_valid_range_filter_args *args)
{
    if (args != NULL)
    {
        if (args->variable_name != NULL)
        {
            free(args->variable_name);
        }

        free(args);
    }
}

static void longitude_range_filter_args_delete(harp_longitude_range_filter_args *args)
{
    if (args != NULL)
    {
        if (args->min_unit != NULL)
        {
            free(args->min_unit);
        }

        if (args->max_unit != NULL)
        {
            free(args->max_unit);
        }

        free(args);
    }
}

static void point_distance_filter_args_delete(harp_point_distance_filter_args *args)
{
    if (args != NULL)
    {
        if (args->longitude_unit != NULL)
        {
            free(args->longitude_unit);
        }

        if (args->latitude_unit != NULL)
        {
            free(args->latitude_unit);
        }

        if (args->distance_unit != NULL)
        {
            free(args->distance_unit);
        }

        free(args);
    }
}

static void area_mask_covers_point_filter_args_delete(harp_area_mask_covers_point_filter_args *args)
{
    if (args != NULL)
    {
        if (args->filename != NULL)
        {
            free(args->filename);
        }

        free(args);
    }
}

static void area_mask_covers_area_filter_args_delete(harp_area_mask_covers_area_filter_args *args)
{
    if (args != NULL)
    {
        if (args->filename != NULL)
        {
            free(args->filename);
        }

        free(args);
    }
}

static void area_mask_intersects_area_filter_args_delete(harp_area_mask_intersects_area_filter_args *args)
{
    if (args != NULL)
    {
        if (args->filename != NULL)
        {
            free(args->filename);
        }

        free(args);
    }
}

static void variable_derivation_args_delete(harp_variable_derivation_args *args)
{
    if (args != NULL)
    {
        if (args->variable_name != NULL)
        {
            free(args->variable_name);
        }

        if (args->unit != NULL)
        {
            free(args->unit);
        }

        free(args);
    }
}

static void variable_inclusion_args_delete(harp_variable_inclusion_args *args)
{
    if (args != NULL)
    {
        if (args->variable_name != NULL)
        {
            int i;

            for (i = 0; i < args->num_variables; i++)
            {
                if (args->variable_name[i] != NULL)
                {
                    free(args->variable_name[i]);
                }
            }

            free(args->variable_name);
        }

        free(args);
    }
}

static void variable_exclusion_args_delete(harp_variable_exclusion_args *args)
{
    if (args != NULL)
    {
        if (args->variable_name != NULL)
        {
            int i;

            for (i = 0; i < args->num_variables; i++)
            {
                if (args->variable_name[i] != NULL)
                {
                    free(args->variable_name[i]);
                }
            }

            free(args->variable_name);
        }

        free(args);
    }
}

static int collocation_filter_args_new(const char *filename, harp_collocation_filter_type filter_type,
                                       harp_collocation_filter_args **new_args)
{
    harp_collocation_filter_args *args;

    args = (harp_collocation_filter_args *)malloc(sizeof(harp_collocation_filter_args));
    if (args == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_collocation_filter_args), __FILE__, __LINE__);
        return -1;
    }

    args->filename = NULL;
    args->filter_type = filter_type;

    args->filename = strdup(filename);
    if (args->filename == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        collocation_filter_args_delete(args);
        return -1;
    }

    *new_args = args;
    return 0;
}

static int comparison_filter_args_new(const char *variable_name, harp_comparison_operator_type operator_type,
                                      double value, const char *unit, harp_comparison_filter_args **new_args)
{
    harp_comparison_filter_args *args;

    assert(variable_name != NULL);

    args = (harp_comparison_filter_args *)malloc(sizeof(harp_comparison_filter_args));
    if (args == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_comparison_filter_args), __FILE__, __LINE__);
        return -1;
    }

    args->variable_name = NULL;
    args->operator_type = operator_type;
    args->value = value;
    args->unit = NULL;

    args->variable_name = strdup(variable_name);
    if (args->variable_name == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        comparison_filter_args_delete(args);
        return -1;
    }

    if (unit != NULL)
    {
        args->unit = strdup(unit);
        if (args->unit == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                           __LINE__);
            comparison_filter_args_delete(args);
            return -1;
        }
    }

    *new_args = args;
    return 0;
}

static int string_comparison_filter_args_new(const char *variable_name, harp_comparison_operator_type operator_type,
                                             const char *value, harp_string_comparison_filter_args **new_args)
{
    harp_string_comparison_filter_args *args;

    assert(variable_name != NULL);
    assert(value != NULL);
    assert(operator_type == harp_operator_eq || operator_type == harp_operator_ne);

    args = (harp_string_comparison_filter_args *)malloc(sizeof(harp_string_comparison_filter_args));
    if (args == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_string_comparison_filter_args), __FILE__, __LINE__);
        return -1;
    }

    args->variable_name = NULL;
    args->operator_type = operator_type;

    args->variable_name = strdup(variable_name);
    if (args->variable_name == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        string_comparison_filter_args_delete(args);
        return -1;
    }

    args->value = strdup(value);
    if (args->value == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        string_comparison_filter_args_delete(args);
        return -1;
    }

    *new_args = args;
    return 0;
}

static int bit_mask_filter_args_new(const char *variable_name, harp_bit_mask_operator_type operator_type,
                                    uint32_t bit_mask, harp_bit_mask_filter_args **new_args)
{
    harp_bit_mask_filter_args *args;

    assert(variable_name != NULL);

    args = (harp_bit_mask_filter_args *)malloc(sizeof(harp_bit_mask_filter_args));
    if (args == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_bit_mask_filter_args), __FILE__, __LINE__);
        return -1;
    }

    args->variable_name = NULL;
    args->operator_type = operator_type;
    args->bit_mask = bit_mask;

    args->variable_name = strdup(variable_name);
    if (args->variable_name == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        bit_mask_filter_args_delete(args);
        return -1;
    }

    *new_args = args;
    return 0;
}

static int membership_filter_args_new(const char *variable_name, harp_membership_operator_type operator_type,
                                      int num_values, const double *value, const char *unit,
                                      harp_membership_filter_args **new_args)
{
    harp_membership_filter_args *args;

    assert(variable_name != NULL);
    assert(num_values == 0 || value != NULL);

    args = (harp_membership_filter_args *)malloc(sizeof(harp_membership_filter_args));
    if (args == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_membership_filter_args), __FILE__, __LINE__);
        return -1;
    }

    args->variable_name = NULL;
    args->operator_type = operator_type;
    args->num_values = num_values;
    args->value = NULL;
    args->unit = NULL;

    args->variable_name = strdup(variable_name);
    if (args->variable_name == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        membership_filter_args_delete(args);
        return -1;
    }

    if (value != NULL)
    {
        args->value = (double *)malloc(num_values * sizeof(double));
        if (args->value == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           num_values * sizeof(double), __FILE__, __LINE__);
            membership_filter_args_delete(args);
            return -1;
        }

        memcpy(args->value, value, num_values * sizeof(double));
    }

    if (unit != NULL)
    {
        args->unit = strdup(unit);
        if (args->unit == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                           __LINE__);
            membership_filter_args_delete(args);
            return -1;
        }
    }

    *new_args = args;
    return 0;
}

static int string_membership_filter_args_new(const char *variable_name, harp_membership_operator_type operator_type,
                                             int num_values, const char **value,
                                             harp_string_membership_filter_args **new_args)
{
    harp_string_membership_filter_args *args;

    assert(variable_name != NULL);
    assert(num_values == 0 || value != NULL);

    args = (harp_string_membership_filter_args *)malloc(sizeof(harp_string_membership_filter_args));
    if (args == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_string_membership_filter_args), __FILE__, __LINE__);
        return -1;
    }

    args->variable_name = NULL;
    args->operator_type = operator_type;
    args->num_values = num_values;
    args->value = NULL;

    args->variable_name = strdup(variable_name);
    if (args->variable_name == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        string_membership_filter_args_delete(args);
        return -1;
    }

    if (value != NULL)
    {
        int i;

        args->value = (char **)malloc(num_values * sizeof(char *));
        if (args->value == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           num_values * sizeof(char *), __FILE__, __LINE__);
            string_membership_filter_args_delete(args);
            return -1;
        }

        for (i = 0; i < num_values; i++)
        {
            if (value[i] == NULL)
            {
                args->value[i] = NULL;
            }
            else
            {
                args->value[i] = strdup(value[i]);
                if (args->value[i] == NULL)
                {
                    harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)",
                                   __FILE__, __LINE__);
                    string_membership_filter_args_delete(args);
                    return -1;
                }
            }
        }
    }

    *new_args = args;
    return 0;
}

static int valid_range_filter_args_new(const char *variable_name, harp_valid_range_filter_args **new_args)
{
    harp_valid_range_filter_args *args;

    args = (harp_valid_range_filter_args *)malloc(sizeof(harp_valid_range_filter_args));
    if (args == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_valid_range_filter_args), __FILE__, __LINE__);
        return -1;
    }

    args->variable_name = strdup(variable_name);
    if (args->variable_name == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        valid_range_filter_args_delete(args);
        return -1;
    }

    *new_args = args;
    return 0;
}

static int longitude_range_filter_args_new(double min, const char *min_unit, double max, const char *max_unit,
                                           harp_longitude_range_filter_args **new_args)
{
    harp_longitude_range_filter_args *args;

    args = (harp_longitude_range_filter_args *)malloc(sizeof(harp_longitude_range_filter_args));
    if (args == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_longitude_range_filter_args), __FILE__, __LINE__);
        return -1;
    }
    args->min = min;
    args->min_unit = NULL;
    args->max = max;
    args->max_unit = NULL;

    if (min_unit != NULL)
    {
        args->min_unit = strdup(min_unit);
        if (args->min_unit == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                           __LINE__);
            longitude_range_filter_args_delete(args);
            return -1;
        }
    }

    if (max_unit != NULL)
    {
        args->max_unit = strdup(max_unit);
        if (args->max_unit == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                           __LINE__);
            longitude_range_filter_args_delete(args);
            return -1;
        }
    }

    *new_args = args;
    return 0;
}

static int point_distance_filter_args_new(double longitude, const char *longitude_unit, double latitude,
                                          const char *latitude_unit, double distance, const char *distance_unit,
                                          harp_point_distance_filter_args **new_args)
{
    harp_point_distance_filter_args *args;

    args = (harp_point_distance_filter_args *)malloc(sizeof(harp_point_distance_filter_args));
    if (args == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_point_distance_filter_args), __FILE__, __LINE__);
        return -1;
    }
    args->longitude = longitude;
    args->longitude_unit = NULL;
    args->latitude = latitude;
    args->latitude_unit = NULL;
    args->distance = distance;
    args->distance_unit = NULL;

    if (longitude_unit != NULL)
    {
        args->longitude_unit = strdup(longitude_unit);
        if (args->longitude_unit == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                           __LINE__);
            point_distance_filter_args_delete(args);
            return -1;
        }
    }

    if (latitude_unit != NULL)
    {
        args->latitude_unit = strdup(latitude_unit);
        if (args->latitude_unit == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                           __LINE__);
            point_distance_filter_args_delete(args);
            return -1;
        }
    }

    if (distance_unit != NULL)
    {
        args->distance_unit = strdup(distance_unit);
        if (args->distance_unit == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                           __LINE__);
            point_distance_filter_args_delete(args);
            return -1;
        }
    }

    *new_args = args;
    return 0;
}

static int area_mask_covers_point_filter_args_new(const char *filename,
                                                  harp_area_mask_covers_point_filter_args **new_args)
{
    harp_area_mask_covers_point_filter_args *args;

    args = (harp_area_mask_covers_point_filter_args *)malloc(sizeof(harp_area_mask_covers_point_filter_args));
    if (args == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_area_mask_covers_point_filter_args), __FILE__, __LINE__);
        return -1;
    }

    args->filename = strdup(filename);
    if (args->filename == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        area_mask_covers_point_filter_args_delete(args);
        return -1;
    }

    *new_args = args;
    return 0;
}

static int area_mask_covers_area_filter_args_new(const char *filename,
                                                 harp_area_mask_covers_area_filter_args **new_args)
{
    harp_area_mask_covers_area_filter_args *args;

    args = (harp_area_mask_covers_area_filter_args *)malloc(sizeof(harp_area_mask_covers_area_filter_args));
    if (args == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_area_mask_covers_area_filter_args), __FILE__, __LINE__);
        return -1;
    }

    args->filename = strdup(filename);
    if (args->filename == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        area_mask_covers_area_filter_args_delete(args);
        return -1;
    }

    *new_args = args;
    return 0;
}

static int area_mask_intersects_area_filter_args_new(const char *filename, double min_percentage,
                                                     harp_area_mask_intersects_area_filter_args **new_args)
{
    harp_area_mask_intersects_area_filter_args *args;

    args = (harp_area_mask_intersects_area_filter_args *)malloc(sizeof(harp_area_mask_intersects_area_filter_args));
    if (args == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_area_mask_intersects_area_filter_args), __FILE__, __LINE__);
        return -1;
    }

    args->filename = NULL;
    args->min_percentage = min_percentage;

    args->filename = strdup(filename);
    if (args->filename == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        area_mask_intersects_area_filter_args_delete(args);
        return -1;
    }

    *new_args = args;
    return 0;
}

static int variable_derivation_args_new(const char *variable_name, int num_dimensions,
                                        const harp_dimension_type *dimension_type, const char *unit,
                                        harp_variable_derivation_args **new_args)
{
    harp_variable_derivation_args *args;
    int i;

    assert(variable_name != NULL);

    args = (harp_variable_derivation_args *)malloc(sizeof(harp_variable_derivation_args));
    if (args == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_variable_derivation_args), __FILE__, __LINE__);
        return -1;
    }
    args->variable_name = NULL;
    args->num_dimensions = num_dimensions;
    args->unit = NULL;

    args->variable_name = strdup(variable_name);
    if (args->variable_name == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        variable_derivation_args_delete(args);
        return -1;
    }

    for (i = 0; i < num_dimensions; i++)
    {
        args->dimension_type[i] = dimension_type[i];
    }

    if (unit != NULL)
    {
        args->unit = strdup(unit);
        if (args->unit == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                           __LINE__);
            variable_derivation_args_delete(args);
            return -1;
        }
    }

    *new_args = args;
    return 0;
}

static int variable_inclusion_args_new(int num_variables, const char **variable_name,
                                       harp_variable_inclusion_args **new_args)
{
    harp_variable_inclusion_args *args;

    assert(variable_name != NULL);

    args = (harp_variable_inclusion_args *)malloc(sizeof(harp_variable_inclusion_args));
    if (args == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_variable_inclusion_args), __FILE__, __LINE__);
        return -1;
    }
    args->num_variables = num_variables;
    args->variable_name = NULL;

    if (num_variables > 0)
    {
        int i;

        args->variable_name = (char **)malloc(num_variables * sizeof(char *));
        if (args->variable_name == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           num_variables * sizeof(char *), __FILE__, __LINE__);
            variable_inclusion_args_delete(args);
            return -1;
        }

        for (i = 0; i < num_variables; i++)
        {
            args->variable_name[i] = strdup(variable_name[i]);
            if (args->variable_name[i] == NULL)
            {
                harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                               __LINE__);
                variable_inclusion_args_delete(args);
                return -1;
            }
        }
    }

    *new_args = args;
    return 0;
}

static int variable_exclusion_args_new(int num_variables, const char **variable_name,
                                       harp_variable_exclusion_args **new_args)
{
    harp_variable_exclusion_args *args;

    assert(variable_name != NULL);

    args = (harp_variable_exclusion_args *)malloc(sizeof(harp_variable_exclusion_args));
    if (args == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_variable_exclusion_args), __FILE__, __LINE__);
        return -1;
    }
    args->num_variables = num_variables;
    args->variable_name = NULL;

    if (num_variables > 0)
    {
        int i;

        args->variable_name = (char **)malloc(num_variables * sizeof(char *));
        if (args->variable_name == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           num_variables * sizeof(char *), __FILE__, __LINE__);
            variable_exclusion_args_delete(args);
            return -1;
        }

        for (i = 0; i < num_variables; i++)
        {
            args->variable_name[i] = strdup(variable_name[i]);
            if (args->variable_name[i] == NULL)
            {
                harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                               __LINE__);
                variable_exclusion_args_delete(args);
                return -1;
            }
        }
    }

    *new_args = args;
    return 0;
}

static int collocation_filter_args_copy(const harp_collocation_filter_args *args,
                                        harp_collocation_filter_args **new_args)
{
    assert(args != NULL);
    return collocation_filter_args_new(args->filename, args->filter_type, new_args);
}

static int comparison_filter_args_copy(const harp_comparison_filter_args *args, harp_comparison_filter_args **new_args)
{
    assert(args != NULL);
    return comparison_filter_args_new(args->variable_name, args->operator_type, args->value, args->unit, new_args);
}

static int string_comparison_filter_args_copy(const harp_string_comparison_filter_args *args,
                                              harp_string_comparison_filter_args **new_args)
{
    assert(args != NULL);
    return string_comparison_filter_args_new(args->variable_name, args->operator_type, args->value, new_args);
}

static int bit_mask_filter_args_copy(const harp_bit_mask_filter_args *args, harp_bit_mask_filter_args **new_args)
{
    assert(args != NULL);
    return bit_mask_filter_args_new(args->variable_name, args->operator_type, args->bit_mask, new_args);
}

static int membership_filter_args_copy(const harp_membership_filter_args *args, harp_membership_filter_args **new_args)
{
    assert(args != NULL);
    return membership_filter_args_new(args->variable_name, args->operator_type, args->num_values, args->value,
                                      args->unit, new_args);
}

static int string_membership_filter_args_copy(const harp_string_membership_filter_args *args,
                                              harp_string_membership_filter_args **new_args)
{
    assert(args != NULL);
    return string_membership_filter_args_new(args->variable_name, args->operator_type, args->num_values,
                                             (const char **)args->value, new_args);
}

static int valid_range_filter_args_copy(const harp_valid_range_filter_args *args,
                                        harp_valid_range_filter_args **new_args)
{
    assert(args != NULL);
    return valid_range_filter_args_new(args->variable_name, new_args);
}

static int longitude_range_filter_args_copy(const harp_longitude_range_filter_args *args,
                                            harp_longitude_range_filter_args **new_args)
{
    assert(args != NULL);
    return longitude_range_filter_args_new(args->min, args->min_unit, args->max, args->max_unit, new_args);
}

static int point_distance_filter_args_copy(const harp_point_distance_filter_args *args,
                                           harp_point_distance_filter_args **new_args)
{
    assert(args != NULL);
    return point_distance_filter_args_new(args->longitude, args->longitude_unit, args->latitude, args->latitude_unit,
                                          args->distance, args->distance_unit, new_args);
}

static int area_mask_covers_point_filter_args_copy(const harp_area_mask_covers_point_filter_args *args,
                                                   harp_area_mask_covers_point_filter_args **new_args)
{
    assert(args != NULL);
    return area_mask_covers_point_filter_args_new(args->filename, new_args);
}

static int area_mask_covers_area_filter_args_copy(const harp_area_mask_covers_area_filter_args *args,
                                                  harp_area_mask_covers_area_filter_args **new_args)
{
    assert(args != NULL);
    return area_mask_covers_area_filter_args_new(args->filename, new_args);
}

static int area_mask_intersects_area_filter_args_copy(const harp_area_mask_intersects_area_filter_args *args,
                                                      harp_area_mask_intersects_area_filter_args **new_args)
{
    assert(args != NULL);
    return area_mask_intersects_area_filter_args_new(args->filename, args->min_percentage, new_args);
}

static int variable_derivation_args_copy(const harp_variable_derivation_args *args,
                                         harp_variable_derivation_args **new_args)
{
    assert(args != NULL);
    return variable_derivation_args_new(args->variable_name, args->num_dimensions, args->dimension_type, args->unit,
                                        new_args);
}

static int variable_inclusion_args_copy(const harp_variable_inclusion_args *args,
                                        harp_variable_inclusion_args **new_args)
{
    assert(args != NULL);
    return variable_inclusion_args_new(args->num_variables, (const char **)args->variable_name, new_args);
}

static int variable_exclusion_args_copy(const harp_variable_exclusion_args *args,
                                        harp_variable_exclusion_args **new_args)
{
    assert(args != NULL);
    return variable_exclusion_args_new(args->num_variables, (const char **)args->variable_name, new_args);
}

int harp_action_new(harp_action_type type, void *args, harp_action **new_action)
{
    harp_action *action;

    action = (harp_action *)malloc(sizeof(harp_action));
    if (action == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_action), __FILE__, __LINE__);
        return -1;
    }

    action->type = type;
    action->args = args;

    *new_action = action;
    return 0;
}

static void args_delete(harp_action_type action_type, void *args)
{
    switch (action_type)
    {
        case harp_action_filter_collocation:
            collocation_filter_args_delete((harp_collocation_filter_args *)args);
            break;
        case harp_action_filter_comparison:
            comparison_filter_args_delete((harp_comparison_filter_args *)args);
            break;
        case harp_action_filter_string_comparison:
            string_comparison_filter_args_delete((harp_string_comparison_filter_args *)args);
            break;
        case harp_action_filter_bit_mask:
            bit_mask_filter_args_delete((harp_bit_mask_filter_args *)args);
            break;
        case harp_action_filter_membership:
            membership_filter_args_delete((harp_membership_filter_args *)args);
            break;
        case harp_action_filter_string_membership:
            string_membership_filter_args_delete((harp_string_membership_filter_args *)args);
            break;
        case harp_action_filter_valid_range:
            valid_range_filter_args_delete((harp_valid_range_filter_args *)args);
            break;
        case harp_action_filter_longitude_range:
            longitude_range_filter_args_delete((harp_longitude_range_filter_args *)args);
            break;
        case harp_action_filter_point_distance:
            point_distance_filter_args_delete((harp_point_distance_filter_args *)args);
            break;
        case harp_action_filter_area_mask_covers_point:
            area_mask_covers_point_filter_args_delete((harp_area_mask_covers_point_filter_args *)args);
            break;
        case harp_action_filter_area_mask_covers_area:
            area_mask_covers_area_filter_args_delete((harp_area_mask_covers_area_filter_args *)args);
            break;
        case harp_action_filter_area_mask_intersects_area:
            area_mask_intersects_area_filter_args_delete((harp_area_mask_intersects_area_filter_args *)args);
            break;
        case harp_action_derive_variable:
            variable_derivation_args_delete((harp_variable_derivation_args *)args);
            break;
        case harp_action_include_variable:
            variable_inclusion_args_delete((harp_variable_inclusion_args *)args);
            break;
        case harp_action_exclude_variable:
            variable_exclusion_args_delete((harp_variable_exclusion_args *)args);
            break;
        default:
            assert(0);
            exit(1);
    }
}

void harp_action_delete(harp_action *action)
{
    if (action != NULL)
    {
        if (action->args != NULL)
        {
            args_delete(action->type, action->args);
        }

        free(action);
    }
}

static int args_copy(harp_action_type action_type, const void *args, void **new_args)
{
    switch (action_type)
    {
        case harp_action_filter_collocation:
            return collocation_filter_args_copy((const harp_collocation_filter_args *)args,
                                                (harp_collocation_filter_args **)new_args);
        case harp_action_filter_comparison:
            return comparison_filter_args_copy((const harp_comparison_filter_args *)args,
                                               (harp_comparison_filter_args **)new_args);
        case harp_action_filter_string_comparison:
            return string_comparison_filter_args_copy((const harp_string_comparison_filter_args *)args,
                                                      (harp_string_comparison_filter_args **)new_args);
        case harp_action_filter_bit_mask:
            return bit_mask_filter_args_copy((const harp_bit_mask_filter_args *)args,
                                             (harp_bit_mask_filter_args **)new_args);
        case harp_action_filter_membership:
            return membership_filter_args_copy((const harp_membership_filter_args *)args,
                                               (harp_membership_filter_args **)new_args);
        case harp_action_filter_string_membership:
            return string_membership_filter_args_copy((const harp_string_membership_filter_args *)args,
                                                      (harp_string_membership_filter_args **)new_args);
        case harp_action_filter_valid_range:
            return valid_range_filter_args_copy((const harp_valid_range_filter_args *)args,
                                                (harp_valid_range_filter_args **)new_args);
        case harp_action_filter_longitude_range:
            return longitude_range_filter_args_copy((const harp_longitude_range_filter_args *)args,
                                                    (harp_longitude_range_filter_args **)new_args);
        case harp_action_filter_point_distance:
            return point_distance_filter_args_copy((const harp_point_distance_filter_args *)args,
                                                   (harp_point_distance_filter_args **)new_args);
        case harp_action_filter_area_mask_covers_point:
            return area_mask_covers_point_filter_args_copy((const harp_area_mask_covers_point_filter_args *)args,
                                                           (harp_area_mask_covers_point_filter_args **)new_args);
        case harp_action_filter_area_mask_covers_area:
            return area_mask_covers_area_filter_args_copy((const harp_area_mask_covers_area_filter_args *)args,
                                                          (harp_area_mask_covers_area_filter_args **)new_args);
        case harp_action_filter_area_mask_intersects_area:
            return area_mask_intersects_area_filter_args_copy((const harp_area_mask_intersects_area_filter_args *)args,
                                                              (harp_area_mask_intersects_area_filter_args **)new_args);
        case harp_action_derive_variable:
            return variable_derivation_args_copy((const harp_variable_derivation_args *)args,
                                                 (harp_variable_derivation_args **)new_args);
        case harp_action_include_variable:
            return variable_inclusion_args_copy((harp_variable_inclusion_args *)args,
                                                (harp_variable_inclusion_args **)new_args);
        case harp_action_exclude_variable:
            return variable_exclusion_args_copy((harp_variable_exclusion_args *)args,
                                                (harp_variable_exclusion_args **)new_args);
        default:
            assert(0);
            exit(1);
    }

    return -1;
}

int harp_action_copy(const harp_action *other_action, harp_action **new_action)
{
    harp_action *action;

    assert(other_action != NULL);

    action = (harp_action *)malloc(sizeof(harp_action));
    if (action == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_action), __FILE__, __LINE__);
        return -1;
    }
    action->type = other_action->type;
    action->args = NULL;

    if (other_action->args != NULL)
    {
        if (args_copy(other_action->type, other_action->args, &action->args) != 0)
        {
            harp_action_delete(action);
            return -1;
        }
    }

    *new_action = action;
    return 0;
}

int harp_comparison_filter_new(const char *variable_name, harp_comparison_operator_type operator_type, double value,
                               const char *unit, harp_action **new_action)
{
    harp_comparison_filter_args *args;
    harp_action *action;

    if (comparison_filter_args_new(variable_name, operator_type, value, unit, &args) != 0)
    {
        return -1;
    }

    if (harp_action_new(harp_action_filter_comparison, args, &action) != 0)
    {
        comparison_filter_args_delete(args);
        return -1;
    }

    *new_action = action;
    return 0;
}

int harp_string_comparison_filter_new(const char *variable_name, harp_comparison_operator_type operator_type,
                                      const char *value, harp_action **new_action)
{
    harp_string_comparison_filter_args *args;
    harp_action *action;

    if (string_comparison_filter_args_new(variable_name, operator_type, value, &args) != 0)
    {
        return -1;
    }

    if (harp_action_new(harp_action_filter_string_comparison, args, &action) != 0)
    {
        string_comparison_filter_args_delete(args);
        return -1;
    }

    *new_action = action;
    return 0;
}

int harp_bit_mask_filter_new(const char *variable_name, harp_bit_mask_operator_type operator_type, uint32_t bit_mask,
                         harp_action **new_action)
{
    harp_bit_mask_filter_args *args;
    harp_action *action;

    if (bit_mask_filter_args_new(variable_name, operator_type, bit_mask, &args) != 0)
    {
        return -1;
    }

    if (harp_action_new(harp_action_filter_bit_mask, args, &action) != 0)
    {
        bit_mask_filter_args_delete(args);
        return -1;
    }

    *new_action = action;
    return 0;
}

int harp_membership_filter_new(const char *variable_name, harp_membership_operator_type operator_type, int num_values,
                               const double *value, const char *unit, harp_action **new_action)
{
    harp_membership_filter_args *args;
    harp_action *action;

    if (membership_filter_args_new(variable_name, operator_type, num_values, value, unit, &args) != 0)
    {
        return -1;
    }

    if (harp_action_new(harp_action_filter_membership, args, &action) != 0)
    {
        membership_filter_args_delete(args);
        return -1;
    }

    *new_action = action;
    return 0;
}

int harp_string_membership_filter_new(const char *variable_name, harp_membership_operator_type operator_type,
                                      int num_values, const char **value, harp_action **new_action)
{
    harp_string_membership_filter_args *args;
    harp_action *action;

    if (string_membership_filter_args_new(variable_name, operator_type, num_values, value, &args) != 0)
    {
        return -1;
    }

    if (harp_action_new(harp_action_filter_string_membership, args, &action) != 0)
    {
        string_membership_filter_args_delete(args);
        return -1;
    }

    *new_action = action;
    return 0;
}

int harp_collocation_filter_new(const char *filename, harp_collocation_filter_type filter_type,
                                harp_action **new_action)
{
    harp_collocation_filter_args *args;
    harp_action *action;

    if (collocation_filter_args_new(filename, filter_type, &args) != 0)
    {
        return -1;
    }

    if (harp_action_new(harp_action_filter_collocation, args, &action) != 0)
    {
        collocation_filter_args_delete(args);
        return -1;
    }

    *new_action = action;
    return 0;
}

int harp_valid_range_filter_new(const char *variable_name, harp_action **new_action)
{
    harp_valid_range_filter_args *args;
    harp_action *action;

    if (valid_range_filter_args_new(variable_name, &args) != 0)
    {
        return -1;
    }

    if (harp_action_new(harp_action_filter_valid_range, args, &action) != 0)
    {
        valid_range_filter_args_delete(args);
        return -1;
    }

    *new_action = action;
    return 0;
}

int harp_longitude_range_filter_new(double min, const char *min_unit, double max, const char *max_unit,
                                    harp_action **new_action)
{
    harp_longitude_range_filter_args *args;
    harp_action *action;

    if (longitude_range_filter_args_new(min, min_unit, max, max_unit, &args) != 0)
    {
        return -1;
    }

    if (harp_action_new(harp_action_filter_longitude_range, args, &action) != 0)
    {
        longitude_range_filter_args_delete(args);
        return -1;
    }

    *new_action = action;
    return 0;
}

int harp_point_distance_filter_new(double longitude, const char *longitude_unit, double latitude,
                                   const char *latitude_unit, double distance, const char *distance_unit,
                                   harp_action **new_action)
{
    harp_point_distance_filter_args *args;
    harp_action *action;

    if (point_distance_filter_args_new(longitude, longitude_unit, latitude, latitude_unit, distance, distance_unit,
                                       &args) != 0)
    {
        return -1;
    }

    if (harp_action_new(harp_action_filter_point_distance, args, &action) != 0)
    {
        point_distance_filter_args_delete(args);
        return -1;
    }

    *new_action = action;
    return 0;
}

int harp_area_mask_covers_point_filter_new(const char *filename, harp_action **new_action)
{
    harp_area_mask_covers_point_filter_args *args;
    harp_action *action;

    if (area_mask_covers_point_filter_args_new(filename, &args) != 0)
    {
        return -1;
    }

    if (harp_action_new(harp_action_filter_area_mask_covers_point, args, &action) != 0)
    {
        area_mask_covers_point_filter_args_delete(args);
        return -1;
    }

    *new_action = action;
    return 0;
}

int harp_area_mask_covers_area_filter_new(const char *filename, harp_action **new_action)
{
    harp_area_mask_covers_area_filter_args *args;
    harp_action *action;

    if (area_mask_covers_area_filter_args_new(filename, &args) != 0)
    {
        return -1;
    }

    if (harp_action_new(harp_action_filter_area_mask_covers_area, args, &action) != 0)
    {
        area_mask_covers_area_filter_args_delete(args);
        return -1;
    }

    *new_action = action;
    return 0;
}

int harp_area_mask_intersects_area_filter_new(const char *filename, double min_percentage, harp_action **new_action)
{
    harp_area_mask_intersects_area_filter_args *args;
    harp_action *action;

    if (area_mask_intersects_area_filter_args_new(filename, min_percentage, &args) != 0)
    {
        return -1;
    }

    if (harp_action_new(harp_action_filter_area_mask_intersects_area, args, &action) != 0)
    {
        area_mask_intersects_area_filter_args_delete(args);
        return -1;
    }

    *new_action = action;
    return 0;
}

int harp_variable_derivation_new(const char *variable_name, int num_dimensions,
                                 const harp_dimension_type *dimension_type, const char *unit, harp_action **new_action)
{
    harp_variable_derivation_args *args;
    harp_action *action;

    if (variable_derivation_args_new(variable_name, num_dimensions, dimension_type, unit, &args) != 0)
    {
        return -1;
    }

    if (harp_action_new(harp_action_derive_variable, args, &action) != 0)
    {
        variable_derivation_args_delete(args);
        return -1;
    }

    *new_action = action;
    return 0;
}

int harp_variable_inclusion_new(int num_variables, const char **variable_name, harp_action **new_action)
{
    harp_variable_inclusion_args *args;
    harp_action *action;

    if (variable_inclusion_args_new(num_variables, variable_name, &args) != 0)
    {
        return -1;
    }

    if (harp_action_new(harp_action_include_variable, args, &action) != 0)
    {
        variable_inclusion_args_delete(args);
        return -1;
    }

    *new_action = action;
    return 0;
}

int harp_variable_exclusion_new(int num_variables, const char **variable_name, harp_action **new_action)
{
    harp_variable_exclusion_args *args;
    harp_action *action;

    if (variable_exclusion_args_new(num_variables, variable_name, &args) != 0)
    {
        return -1;
    }

    if (harp_action_new(harp_action_exclude_variable, args, &action) != 0)
    {
        variable_exclusion_args_delete(args);
        return -1;
    }

    *new_action = action;
    return 0;
}

int harp_action_get_variable_name(const harp_action *action, const char **variable_name)
{
    switch (action->type)
    {
        case harp_action_filter_comparison:
            *variable_name = ((harp_comparison_filter_args *)action->args)->variable_name;
            break;
        case harp_action_filter_string_comparison:
            *variable_name = ((harp_string_comparison_filter_args *)action->args)->variable_name;
            break;
        case harp_action_filter_membership:
            *variable_name = ((harp_membership_filter_args *)action->args)->variable_name;
            break;
        case harp_action_filter_string_membership:
            *variable_name = ((harp_string_membership_filter_args *)action->args)->variable_name;
            break;
        case harp_action_filter_bit_mask:
            *variable_name = ((harp_bit_mask_filter_args *)action->args)->variable_name;
            break;
        case harp_action_filter_longitude_range:
            *variable_name = "longitude";
            break;
        default:
            return -1;
    }

    return 0;
}

/** \addtogroup harp_product
 * @{
 */

/**
 * Execute one or more actions on a product.
 * \param  product Product that the actions should be executed on.
 * \param  actions Actions to execute; should be specified as a semi-colon
 *                 separated string of actions.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_product_execute_actions(harp_product *product, const char *actions)
{
    harp_action_list *action_list;

    if (product == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "product is NULL");
        return -1;
    }

    if (actions == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "actions is NULL");
        return -1;
    }

    if (harp_action_list_from_string(actions, &action_list) != 0)
    {
        return -1;
    }

    if (harp_product_execute_action_list(product, action_list) != 0)
    {
        harp_action_list_delete(action_list);
        return -1;
    }

    harp_action_list_delete(action_list);

    return 0;
}

/** @} */
