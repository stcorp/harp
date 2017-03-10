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
#include <stdlib.h>
#include <string.h>

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

static void derive_variable_args_delete(harp_derive_variable_args *args)
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

static void derive_smoothed_column_collocated_args_delete(harp_derive_smoothed_column_collocated_args *args)
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
        if (args->axis_variable_name != NULL)
        {
            free(args->axis_variable_name);
        }
        if (args->axis_unit != NULL)
        {
            free(args->axis_unit);
        }
        if (args->collocation_result != NULL)
        {
            free(args->collocation_result);
        }
        if (args->dataset_dir != NULL)
        {
            free(args->dataset_dir);
        }

        free(args);
    }
}

static void exclude_variable_args_delete(harp_exclude_variable_args *args)
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

static void flatten_args_delete(harp_flatten_args *args)
{
    if (args != NULL)
    {
        free(args);
    }
}

static void keep_variable_args_delete(harp_keep_variable_args *args)
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

static void point_distance_filter_args_delete(harp_point_distance_filter_args *args)
{
    if (args != NULL)
    {
        if (args->latitude_unit != NULL)
        {
            free(args->latitude_unit);
        }

        if (args->longitude_unit != NULL)
        {
            free(args->longitude_unit);
        }

        if (args->distance_unit != NULL)
        {
            free(args->distance_unit);
        }

        free(args);
    }
}

static void point_in_area_filter_args_delete(harp_point_in_area_filter_args *args)
{
    if (args != NULL)
    {
        if (args->latitude_unit != NULL)
        {
            free(args->latitude_unit);
        }

        if (args->longitude_unit != NULL)
        {
            free(args->longitude_unit);
        }

        free(args);
    }
}

static void regrid_args_delete(harp_regrid_args *args)
{
    if (args != NULL)
    {
        if (args->axis_variable != NULL)
        {
            harp_variable_delete(args->axis_variable);
        }

        free(args);
    }
}

static void regrid_collocated_args_delete(harp_regrid_collocated_args *args)
{
    if (args != NULL)
    {
        if (args->axis_variable_name != NULL)
        {
            free(args->axis_variable_name);
        }
        if (args->axis_unit != NULL)
        {
            free(args->axis_unit);
        }
        if (args->collocation_result != NULL)
        {
            free(args->collocation_result);
        }
        if (args->dataset_dir != NULL)
        {
            free(args->dataset_dir);
        }

        free(args);
    }
}

static void rename_args_delete(harp_rename_args *args)
{
    if (args != NULL)
    {
        if (args->variable_name != NULL)
        {
            free(args->variable_name);
        }
        if (args->new_variable_name != NULL)
        {
            free(args->new_variable_name);
        }

        free(args);
    }
}


static void smooth_collocated_args_delete(harp_smooth_collocated_args *args)
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
        if (args->axis_variable_name != NULL)
        {
            free(args->axis_variable_name);
        }
        if (args->axis_unit != NULL)
        {
            free(args->axis_unit);
        }
        if (args->collocation_result != NULL)
        {
            free(args->collocation_result);
        }
        if (args->dataset_dir != NULL)
        {
            free(args->dataset_dir);
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

static void wrap_args_delete(harp_wrap_args *args)
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

static int derive_variable_args_new(const char *variable_name, int num_dimensions,
                                    const harp_dimension_type *dimension_type, const char *unit,
                                    harp_derive_variable_args **new_args)
{
    harp_derive_variable_args *args;
    int i;

    assert(variable_name != NULL);

    args = (harp_derive_variable_args *)malloc(sizeof(harp_derive_variable_args));
    if (args == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_derive_variable_args), __FILE__, __LINE__);
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
        derive_variable_args_delete(args);
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
            derive_variable_args_delete(args);
            return -1;
        }
    }

    *new_args = args;
    return 0;
}

static int derive_smoothed_column_collocated_args_new(const char *variable_name, int num_dimensions,
                                                      const harp_dimension_type *dimension_type, const char *unit,
                                                      const char *axis_variable_name, const char *axis_unit,
                                                      const char *collocation_result, const char target_dataset,
                                                      const char *dataset_dir,
                                                      harp_derive_smoothed_column_collocated_args **new_args)
{
    harp_derive_smoothed_column_collocated_args *args;
    int i;

    assert(variable_name != NULL);
    assert(axis_variable_name != NULL);
    assert(axis_unit != NULL);
    assert(collocation_result != NULL);
    assert(dataset_dir != NULL);

    args = (harp_derive_smoothed_column_collocated_args *)malloc(sizeof(harp_derive_smoothed_column_collocated_args));
    if (args == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_derive_smoothed_column_collocated_args), __FILE__, __LINE__);
        return -1;
    }
    args->variable_name = NULL;
    args->num_dimensions = num_dimensions;
    args->unit = NULL;
    args->axis_variable_name = strdup(axis_variable_name);
    args->axis_unit = strdup(axis_unit);
    args->collocation_result = strdup(collocation_result);
    args->dataset_dir = strdup(dataset_dir);
    args->target_dataset = target_dataset;

    args->variable_name = strdup(variable_name);
    if (args->variable_name == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        derive_smoothed_column_collocated_args_delete(args);
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
            derive_smoothed_column_collocated_args_delete(args);
            return -1;
        }
    }

    *new_args = args;
    return 0;
}

static int exclude_variable_args_new(int num_variables, const char **variable_name,
                                     harp_exclude_variable_args **new_args)
{
    harp_exclude_variable_args *args;

    assert(variable_name != NULL);

    args = (harp_exclude_variable_args *)malloc(sizeof(harp_exclude_variable_args));
    if (args == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_exclude_variable_args), __FILE__, __LINE__);
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
            exclude_variable_args_delete(args);
            return -1;
        }

        for (i = 0; i < num_variables; i++)
        {
            args->variable_name[i] = strdup(variable_name[i]);
            if (args->variable_name[i] == NULL)
            {
                harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                               __LINE__);
                exclude_variable_args_delete(args);
                return -1;
            }
        }
    }

    *new_args = args;
    return 0;
}

static int flatten_args_new(const harp_dimension_type dimension_type, harp_flatten_args **new_args)
{
    harp_flatten_args *args;

    args = (harp_flatten_args *)malloc(sizeof(harp_flatten_args));
    if (args == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_flatten_args), __FILE__, __LINE__);
        return -1;
    }

    args->dimension_type = dimension_type;

    *new_args = args;
    return 0;
}

static int keep_variable_args_new(int num_variables, const char **variable_name, harp_keep_variable_args **new_args)
{
    harp_keep_variable_args *args;

    assert(variable_name != NULL);

    args = (harp_keep_variable_args *)malloc(sizeof(harp_keep_variable_args));
    if (args == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_keep_variable_args), __FILE__, __LINE__);
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
            keep_variable_args_delete(args);
            return -1;
        }

        for (i = 0; i < num_variables; i++)
        {
            args->variable_name[i] = strdup(variable_name[i]);
            if (args->variable_name[i] == NULL)
            {
                harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                               __LINE__);
                keep_variable_args_delete(args);
                return -1;
            }
        }
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

static int point_distance_filter_args_new(double latitude, const char *latitude_unit, double longitude,
                                          const char *longitude_unit, double distance, const char *distance_unit,
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
    args->latitude = latitude;
    args->latitude_unit = NULL;
    args->longitude = longitude;
    args->longitude_unit = NULL;
    args->distance = distance;
    args->distance_unit = NULL;

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

static int point_in_area_filter_args_new(double latitude, const char *latitude_unit, double longitude,
                                         const char *longitude_unit, harp_point_in_area_filter_args **new_args)
{
    harp_point_in_area_filter_args *args;

    args = (harp_point_in_area_filter_args *)malloc(sizeof(harp_point_in_area_filter_args));
    if (args == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_point_distance_filter_args), __FILE__, __LINE__);
        return -1;
    }
    args->latitude = latitude;
    args->latitude_unit = NULL;
    args->longitude = longitude;
    args->longitude_unit = NULL;

    if (latitude_unit != NULL)
    {
        args->latitude_unit = strdup(latitude_unit);
        if (args->latitude_unit == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                           __LINE__);
            point_in_area_filter_args_delete(args);
            return -1;
        }
    }

    if (longitude_unit != NULL)
    {
        args->longitude_unit = strdup(longitude_unit);
        if (args->longitude_unit == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                           __LINE__);
            point_in_area_filter_args_delete(args);
            return -1;
        }
    }

    *new_args = args;
    return 0;
}

static int regrid_args_new(harp_dimension_type dimension_type, const char *axis_variable_name, const char *axis_unit,
                           long num_values, double *values, harp_regrid_args **new_args)
{
    harp_regrid_args *args;
    long i;

    assert(axis_variable_name != NULL);
    assert(axis_unit != NULL);

    args = (harp_regrid_args *)malloc(sizeof(harp_regrid_args));
    if (args == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_regrid_args), __FILE__, __LINE__);
        return -1;
    }
    args->axis_variable = NULL;

    if (harp_variable_new(axis_variable_name, harp_type_double, 1, &dimension_type, &num_values, &args->axis_variable)
        != 0)
    {
        regrid_args_delete(args);
        return -1;
    }
    if (harp_variable_set_unit(args->axis_variable, axis_unit) != 0)
    {
        regrid_args_delete(args);
        return -1;
    }
    for (i = 0; i < num_values; i++)
    {
        args->axis_variable->data.double_data[i] = values[i];
    }

    *new_args = args;
    return 0;
}

static int regrid_collocated_args_new(harp_dimension_type dimension_type, const char *axis_variable_name,
                                      const char *axis_unit, const char *collocation_result, const char target_dataset,
                                      const char *dataset_dir, harp_regrid_collocated_args **new_args)
{
    harp_regrid_collocated_args *args;

    assert(axis_variable_name != NULL);
    assert(axis_unit != NULL);
    assert(collocation_result != NULL);
    assert(dataset_dir != NULL);

    args = (harp_regrid_collocated_args *)malloc(sizeof(harp_regrid_collocated_args));
    if (args == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_regrid_collocated_args), __FILE__, __LINE__);
        return -1;
    }

    args->dimension_type = dimension_type;
    args->axis_variable_name = strdup(axis_variable_name);
    args->axis_unit = strdup(axis_unit);
    args->collocation_result = strdup(collocation_result);
    args->dataset_dir = strdup(dataset_dir);
    args->target_dataset = target_dataset;

    *new_args = args;
    return 0;
}

static int rename_args_new(const char *variable_name, const char *new_variable_name, harp_rename_args **new_args)
{
    harp_rename_args *args;

    assert(variable_name != NULL);
    assert(new_variable_name != NULL);

    args = (harp_rename_args *)malloc(sizeof(harp_rename_args));
    if (args == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_rename_args), __FILE__, __LINE__);
        return -1;
    }
    args->variable_name = strdup(variable_name);
    assert(args->variable_name != NULL);
    args->new_variable_name = strdup(new_variable_name);
    assert(args->new_variable_name != NULL);

    *new_args = args;
    return 0;
}

static int smooth_collocated_args_new(int num_variables, const char **variable_name, harp_dimension_type dimension_type,
                                      const char *axis_variable_name, const char *axis_unit,
                                      const char *collocation_result, const char target_dataset,
                                      const char *dataset_dir, harp_smooth_collocated_args **new_args)
{
    harp_smooth_collocated_args *args;

    assert(variable_name != NULL);
    assert(axis_variable_name != NULL);
    assert(axis_unit != NULL);
    assert(collocation_result != NULL);
    assert(dataset_dir != NULL);

    args = (harp_smooth_collocated_args *)malloc(sizeof(harp_smooth_collocated_args));
    if (args == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_smooth_collocated_args), __FILE__, __LINE__);
        return -1;
    }

    args->num_variables = 0;
    args->variable_name = NULL;
    args->dimension_type = dimension_type;
    args->axis_variable_name = strdup(axis_variable_name);
    args->axis_unit = strdup(axis_unit);
    args->collocation_result = strdup(collocation_result);
    args->dataset_dir = strdup(dataset_dir);
    args->target_dataset = target_dataset;

    if (num_variables > 0)
    {
        int i;

        args->variable_name = (char **)malloc(num_variables * sizeof(char *));
        if (args->variable_name == NULL)
        {
            smooth_collocated_args_delete(args);
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           num_variables * sizeof(char *), __FILE__, __LINE__);
            return -1;
        }
        for (i = 0; i < num_variables; i++)
        {
            args->variable_name[i] = NULL;
        }
        for (i = 0; i < num_variables; i++)
        {
            args->variable_name[i] = strdup(variable_name[i]);
            if (args->variable_name[i] == NULL)
            {
                smooth_collocated_args_delete(args);
                harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                               __LINE__);
                return -1;
            }
            args->num_variables++;
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

static int wrap_args_new(const char *variable_name, const char *unit, double min, double max, harp_wrap_args **new_args)
{
    harp_wrap_args *args;

    args = (harp_wrap_args *)malloc(sizeof(harp_wrap_args));
    if (args == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_wrap_args), __FILE__, __LINE__);
        return -1;
    }
    args->variable_name = NULL;
    args->unit = NULL;
    args->min = min;
    args->max = max;

    args->variable_name = strdup(variable_name);
    if (args->variable_name == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        wrap_args_delete(args);
        return -1;
    }
    if (unit != NULL)
    {
        args->unit = strdup(unit);
        if (args->unit == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                           __LINE__);
            wrap_args_delete(args);
            return -1;
        }
    }

    *new_args = args;
    return 0;
}

static int area_mask_covers_area_filter_args_copy(const harp_area_mask_covers_area_filter_args *args,
                                                  harp_area_mask_covers_area_filter_args **new_args)
{
    assert(args != NULL);
    return area_mask_covers_area_filter_args_new(args->filename, new_args);
}

static int area_mask_covers_point_filter_args_copy(const harp_area_mask_covers_point_filter_args *args,
                                                   harp_area_mask_covers_point_filter_args **new_args)
{
    assert(args != NULL);
    return area_mask_covers_point_filter_args_new(args->filename, new_args);
}

static int area_mask_intersects_area_filter_args_copy(const harp_area_mask_intersects_area_filter_args *args,
                                                      harp_area_mask_intersects_area_filter_args **new_args)
{
    assert(args != NULL);
    return area_mask_intersects_area_filter_args_new(args->filename, args->min_percentage, new_args);
}

static int bit_mask_filter_args_copy(const harp_bit_mask_filter_args *args, harp_bit_mask_filter_args **new_args)
{
    assert(args != NULL);
    return bit_mask_filter_args_new(args->variable_name, args->operator_type, args->bit_mask, new_args);
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

static int derive_variable_args_copy(const harp_derive_variable_args *args, harp_derive_variable_args **new_args)
{
    assert(args != NULL);
    return derive_variable_args_new(args->variable_name, args->num_dimensions, args->dimension_type, args->unit,
                                    new_args);
}

static int derive_smoothed_column_collocated_args_copy(const harp_derive_smoothed_column_collocated_args *args,
                                                       harp_derive_smoothed_column_collocated_args **new_args)
{
    assert(args != NULL);
    return derive_smoothed_column_collocated_args_new(args->variable_name, args->num_dimensions, args->dimension_type,
                                                      args->unit, args->axis_variable_name, args->axis_unit,
                                                      args->collocation_result, args->target_dataset, args->dataset_dir,
                                                      new_args);
}

static int exclude_variable_args_copy(const harp_exclude_variable_args *args, harp_exclude_variable_args **new_args)
{
    assert(args != NULL);
    return exclude_variable_args_new(args->num_variables, (const char **)args->variable_name, new_args);
}

static int flatten_args_copy(const harp_flatten_args *args, harp_flatten_args **new_args)
{
    assert(args != NULL);
    return flatten_args_new(args->dimension_type, new_args);
}

static int keep_variable_args_copy(const harp_keep_variable_args *args, harp_keep_variable_args **new_args)
{
    assert(args != NULL);
    return keep_variable_args_new(args->num_variables, (const char **)args->variable_name, new_args);
}

static int longitude_range_filter_args_copy(const harp_longitude_range_filter_args *args,
                                            harp_longitude_range_filter_args **new_args)
{
    assert(args != NULL);
    return longitude_range_filter_args_new(args->min, args->min_unit, args->max, args->max_unit, new_args);
}

static int membership_filter_args_copy(const harp_membership_filter_args *args, harp_membership_filter_args **new_args)
{
    assert(args != NULL);
    return membership_filter_args_new(args->variable_name, args->operator_type, args->num_values, args->value,
                                      args->unit, new_args);
}

static int point_distance_filter_args_copy(const harp_point_distance_filter_args *args,
                                           harp_point_distance_filter_args **new_args)
{
    assert(args != NULL);
    return point_distance_filter_args_new(args->latitude, args->latitude_unit, args->longitude, args->longitude_unit,
                                          args->distance, args->distance_unit, new_args);
}

static int point_in_area_filter_args_copy(const harp_point_in_area_filter_args *args,
                                          harp_point_in_area_filter_args **new_args)
{
    assert(args != NULL);
    return point_in_area_filter_args_new(args->latitude, args->latitude_unit, args->longitude, args->longitude_unit,
                                         new_args);
}

static int regrid_args_copy(const harp_regrid_args *args, harp_regrid_args **new_args)
{
    assert(args != NULL);
    return regrid_args_new(args->axis_variable->dimension_type[0], args->axis_variable->name, args->axis_variable->unit,
                           args->axis_variable->dimension[0], args->axis_variable->data.double_data, new_args);
}

static int regrid_collocated_args_copy(const harp_regrid_collocated_args *args, harp_regrid_collocated_args **new_args)
{
    assert(args != NULL);
    return regrid_collocated_args_new(args->dimension_type, args->axis_variable_name, args->axis_unit,
                                      args->collocation_result, args->target_dataset, args->dataset_dir, new_args);
}

static int rename_args_copy(const harp_rename_args *args, harp_rename_args **new_args)
{
    assert(args != NULL);
    return rename_args_new(args->variable_name, args->new_variable_name, new_args);
}

static int smooth_collocated_args_copy(const harp_smooth_collocated_args *args, harp_smooth_collocated_args **new_args)
{
    assert(args != NULL);
    return smooth_collocated_args_new(args->num_variables, (const char **)args->variable_name, args->dimension_type,
                                      args->axis_variable_name, args->axis_unit, args->collocation_result,
                                      args->target_dataset, args->dataset_dir, new_args);
}

static int string_comparison_filter_args_copy(const harp_string_comparison_filter_args *args,
                                              harp_string_comparison_filter_args **new_args)
{
    assert(args != NULL);
    return string_comparison_filter_args_new(args->variable_name, args->operator_type, args->value, new_args);
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

static int wrap_args_copy(const harp_wrap_args *args, harp_wrap_args **new_args)
{
    assert(args != NULL);
    return wrap_args_new(args->variable_name, args->unit, args->min, args->max, new_args);
}

int harp_operation_new(harp_operation_type type, void *args, harp_operation **new_operation)
{
    harp_operation *operation;

    operation = (harp_operation *)malloc(sizeof(harp_operation));
    if (operation == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_operation), __FILE__, __LINE__);
        return -1;
    }

    operation->type = type;
    operation->args = args;

    *new_operation = operation;
    return 0;
}

static void args_delete(harp_operation_type operation_type, void *args)
{
    switch (operation_type)
    {
        case harp_operation_area_mask_covers_area_filter:
            area_mask_covers_area_filter_args_delete((harp_area_mask_covers_area_filter_args *)args);
            break;
        case harp_operation_area_mask_covers_point_filter:
            area_mask_covers_point_filter_args_delete((harp_area_mask_covers_point_filter_args *)args);
            break;
        case harp_operation_area_mask_intersects_area_filter:
            area_mask_intersects_area_filter_args_delete((harp_area_mask_intersects_area_filter_args *)args);
            break;
        case harp_operation_bit_mask_filter:
            bit_mask_filter_args_delete((harp_bit_mask_filter_args *)args);
            break;
        case harp_operation_collocation_filter:
            collocation_filter_args_delete((harp_collocation_filter_args *)args);
            break;
        case harp_operation_comparison_filter:
            comparison_filter_args_delete((harp_comparison_filter_args *)args);
            break;
        case harp_operation_derive_variable:
            derive_variable_args_delete((harp_derive_variable_args *)args);
            break;
        case harp_operation_derive_smoothed_column_collocated:
            derive_smoothed_column_collocated_args_delete((harp_derive_smoothed_column_collocated_args *)args);
            break;
        case harp_operation_exclude_variable:
            exclude_variable_args_delete((harp_exclude_variable_args *)args);
            break;
        case harp_operation_flatten:
            flatten_args_delete((harp_flatten_args *)args);
            break;
        case harp_operation_keep_variable:
            keep_variable_args_delete((harp_keep_variable_args *)args);
            break;
        case harp_operation_longitude_range_filter:
            longitude_range_filter_args_delete((harp_longitude_range_filter_args *)args);
            break;
        case harp_operation_membership_filter:
            membership_filter_args_delete((harp_membership_filter_args *)args);
            break;
        case harp_operation_point_distance_filter:
            point_distance_filter_args_delete((harp_point_distance_filter_args *)args);
            break;
        case harp_operation_point_in_area_filter:
            point_in_area_filter_args_delete((harp_point_in_area_filter_args *)args);
            break;
        case harp_operation_regrid:
            regrid_args_delete((harp_regrid_args *)args);
            break;
        case harp_operation_regrid_collocated:
            regrid_collocated_args_delete((harp_regrid_collocated_args *)args);
            break;
        case harp_operation_rename:
            rename_args_delete((harp_rename_args *)args);
            break;
        case harp_operation_smooth_collocated:
            smooth_collocated_args_delete((harp_smooth_collocated_args *)args);
            break;
        case harp_operation_string_comparison_filter:
            string_comparison_filter_args_delete((harp_string_comparison_filter_args *)args);
            break;
        case harp_operation_string_membership_filter:
            string_membership_filter_args_delete((harp_string_membership_filter_args *)args);
            break;
        case harp_operation_valid_range_filter:
            valid_range_filter_args_delete((harp_valid_range_filter_args *)args);
            break;
        case harp_operation_wrap:
            wrap_args_delete((harp_wrap_args *)args);
            break;
    }
}

void harp_operation_delete(harp_operation *operation)
{
    if (operation != NULL)
    {
        if (operation->args != NULL)
        {
            args_delete(operation->type, operation->args);
        }

        free(operation);
    }
}

static int args_copy(harp_operation_type operation_type, const void *args, void **new_args)
{
    switch (operation_type)
    {
        case harp_operation_area_mask_covers_area_filter:
            return area_mask_covers_area_filter_args_copy((const harp_area_mask_covers_area_filter_args *)args,
                                                          (harp_area_mask_covers_area_filter_args **)new_args);
        case harp_operation_area_mask_covers_point_filter:
            return area_mask_covers_point_filter_args_copy((const harp_area_mask_covers_point_filter_args *)args,
                                                           (harp_area_mask_covers_point_filter_args **)new_args);
        case harp_operation_area_mask_intersects_area_filter:
            return area_mask_intersects_area_filter_args_copy((const harp_area_mask_intersects_area_filter_args *)args,
                                                              (harp_area_mask_intersects_area_filter_args **)new_args);
        case harp_operation_bit_mask_filter:
            return bit_mask_filter_args_copy((const harp_bit_mask_filter_args *)args,
                                             (harp_bit_mask_filter_args **)new_args);
        case harp_operation_collocation_filter:
            return collocation_filter_args_copy((const harp_collocation_filter_args *)args,
                                                (harp_collocation_filter_args **)new_args);
        case harp_operation_comparison_filter:
            return comparison_filter_args_copy((const harp_comparison_filter_args *)args,
                                               (harp_comparison_filter_args **)new_args);
        case harp_operation_derive_variable:
            return derive_variable_args_copy((const harp_derive_variable_args *)args,
                                             (harp_derive_variable_args **)new_args);
        case harp_operation_derive_smoothed_column_collocated:
            return derive_smoothed_column_collocated_args_copy
                ((const harp_derive_smoothed_column_collocated_args *)args,
                 (harp_derive_smoothed_column_collocated_args **)new_args);
        case harp_operation_exclude_variable:
            return exclude_variable_args_copy((harp_exclude_variable_args *)args,
                                              (harp_exclude_variable_args **)new_args);
        case harp_operation_flatten:
            return flatten_args_copy((harp_flatten_args *)args, (harp_flatten_args **)new_args);
        case harp_operation_keep_variable:
            return keep_variable_args_copy((harp_keep_variable_args *)args, (harp_keep_variable_args **)new_args);
        case harp_operation_longitude_range_filter:
            return longitude_range_filter_args_copy((const harp_longitude_range_filter_args *)args,
                                                    (harp_longitude_range_filter_args **)new_args);
        case harp_operation_membership_filter:
            return membership_filter_args_copy((const harp_membership_filter_args *)args,
                                               (harp_membership_filter_args **)new_args);
        case harp_operation_point_distance_filter:
            return point_distance_filter_args_copy((const harp_point_distance_filter_args *)args,
                                                   (harp_point_distance_filter_args **)new_args);
        case harp_operation_point_in_area_filter:
            return point_in_area_filter_args_copy((const harp_point_in_area_filter_args *)args,
                                                  (harp_point_in_area_filter_args **)new_args);
        case harp_operation_regrid:
            return regrid_args_copy((harp_regrid_args *)args, (harp_regrid_args **)new_args);
        case harp_operation_regrid_collocated:
            return regrid_collocated_args_copy((harp_regrid_collocated_args *)args,
                                               (harp_regrid_collocated_args **)new_args);
        case harp_operation_rename:
            return rename_args_copy((harp_rename_args *)args, (harp_rename_args **)new_args);
        case harp_operation_smooth_collocated:
            return smooth_collocated_args_copy((harp_smooth_collocated_args *)args,
                                               (harp_smooth_collocated_args **)new_args);
        case harp_operation_string_comparison_filter:
            return string_comparison_filter_args_copy((const harp_string_comparison_filter_args *)args,
                                                      (harp_string_comparison_filter_args **)new_args);
        case harp_operation_string_membership_filter:
            return string_membership_filter_args_copy((const harp_string_membership_filter_args *)args,
                                                      (harp_string_membership_filter_args **)new_args);
        case harp_operation_valid_range_filter:
            return valid_range_filter_args_copy((const harp_valid_range_filter_args *)args,
                                                (harp_valid_range_filter_args **)new_args);
        case harp_operation_wrap:
            return wrap_args_copy((const harp_wrap_args *)args, (harp_wrap_args **)new_args);
    }

    return -1;
}

int harp_operation_copy(const harp_operation *other_operation, harp_operation **new_operation)
{
    harp_operation *operation;

    assert(other_operation != NULL);

    operation = (harp_operation *)malloc(sizeof(harp_operation));
    if (operation == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_operation), __FILE__, __LINE__);
        return -1;
    }
    operation->type = other_operation->type;
    operation->args = NULL;

    if (other_operation->args != NULL)
    {
        if (args_copy(other_operation->type, other_operation->args, &operation->args) != 0)
        {
            harp_operation_delete(operation);
            return -1;
        }
    }

    *new_operation = operation;
    return 0;
}

int harp_area_mask_covers_area_filter_new(const char *filename, harp_operation **new_operation)
{
    harp_area_mask_covers_area_filter_args *args;
    harp_operation *operation;

    if (area_mask_covers_area_filter_args_new(filename, &args) != 0)
    {
        return -1;
    }

    if (harp_operation_new(harp_operation_area_mask_covers_area_filter, args, &operation) != 0)
    {
        area_mask_covers_area_filter_args_delete(args);
        return -1;
    }

    *new_operation = operation;
    return 0;
}

int harp_area_mask_covers_point_filter_new(const char *filename, harp_operation **new_operation)
{
    harp_area_mask_covers_point_filter_args *args;
    harp_operation *operation;

    if (area_mask_covers_point_filter_args_new(filename, &args) != 0)
    {
        return -1;
    }

    if (harp_operation_new(harp_operation_area_mask_covers_point_filter, args, &operation) != 0)
    {
        area_mask_covers_point_filter_args_delete(args);
        return -1;
    }

    *new_operation = operation;
    return 0;
}

int harp_area_mask_intersects_area_filter_new(const char *filename, double min_percentage,
                                              harp_operation **new_operation)
{
    harp_area_mask_intersects_area_filter_args *args;
    harp_operation *operation;

    if (area_mask_intersects_area_filter_args_new(filename, min_percentage, &args) != 0)
    {
        return -1;
    }

    if (harp_operation_new(harp_operation_area_mask_intersects_area_filter, args, &operation) != 0)
    {
        area_mask_intersects_area_filter_args_delete(args);
        return -1;
    }

    *new_operation = operation;
    return 0;
}

int harp_bit_mask_filter_new(const char *variable_name, harp_bit_mask_operator_type operator_type, uint32_t bit_mask,
                             harp_operation **new_operation)
{
    harp_bit_mask_filter_args *args;
    harp_operation *operation;

    if (bit_mask_filter_args_new(variable_name, operator_type, bit_mask, &args) != 0)
    {
        return -1;
    }

    if (harp_operation_new(harp_operation_bit_mask_filter, args, &operation) != 0)
    {
        bit_mask_filter_args_delete(args);
        return -1;
    }

    *new_operation = operation;
    return 0;
}

int harp_collocation_filter_new(const char *filename, harp_collocation_filter_type filter_type,
                                harp_operation **new_operation)
{
    harp_collocation_filter_args *args;
    harp_operation *operation;

    if (collocation_filter_args_new(filename, filter_type, &args) != 0)
    {
        return -1;
    }

    if (harp_operation_new(harp_operation_collocation_filter, args, &operation) != 0)
    {
        collocation_filter_args_delete(args);
        return -1;
    }

    *new_operation = operation;
    return 0;
}

int harp_comparison_filter_new(const char *variable_name, harp_comparison_operator_type operator_type, double value,
                               const char *unit, harp_operation **new_operation)
{
    harp_comparison_filter_args *args;
    harp_operation *operation;

    if (comparison_filter_args_new(variable_name, operator_type, value, unit, &args) != 0)
    {
        return -1;
    }

    if (harp_operation_new(harp_operation_comparison_filter, args, &operation) != 0)
    {
        comparison_filter_args_delete(args);
        return -1;
    }

    *new_operation = operation;
    return 0;
}

int harp_derive_variable_new(const char *variable_name, int num_dimensions, const harp_dimension_type *dimension_type,
                             const char *unit, harp_operation **new_operation)
{
    harp_derive_variable_args *args;
    harp_operation *operation;

    if (derive_variable_args_new(variable_name, num_dimensions, dimension_type, unit, &args) != 0)
    {
        return -1;
    }

    if (harp_operation_new(harp_operation_derive_variable, args, &operation) != 0)
    {
        derive_variable_args_delete(args);
        return -1;
    }

    *new_operation = operation;
    return 0;
}

int harp_derive_smoothed_column_collocated_new(const char *variable_name, int num_dimensions,
                                               const harp_dimension_type *dimension_type, const char *unit,
                                               const char *axis_variable_name, const char *axis_unit,
                                               const char *collocation_result, const char target_dataset,
                                               const char *dataset_dir, harp_operation **new_operation)
{
    harp_derive_smoothed_column_collocated_args *args;
    harp_operation *operation;

    if (derive_smoothed_column_collocated_args_new(variable_name, num_dimensions, dimension_type, unit,
                                                   axis_variable_name, axis_unit, collocation_result, target_dataset,
                                                   dataset_dir, &args) != 0)
    {
        return -1;
    }

    if (harp_operation_new(harp_operation_derive_smoothed_column_collocated, args, &operation) != 0)
    {
        derive_smoothed_column_collocated_args_delete(args);
        return -1;
    }

    *new_operation = operation;
    return 0;
}

int harp_exclude_variable_new(int num_variables, const char **variable_name, harp_operation **new_operation)
{
    harp_exclude_variable_args *args;
    harp_operation *operation;

    if (exclude_variable_args_new(num_variables, variable_name, &args) != 0)
    {
        return -1;
    }

    if (harp_operation_new(harp_operation_exclude_variable, args, &operation) != 0)
    {
        exclude_variable_args_delete(args);
        return -1;
    }

    *new_operation = operation;
    return 0;
}

int harp_flatten_new(const harp_dimension_type dimension_type, harp_operation **new_operation)
{
    harp_flatten_args *args;
    harp_operation *operation;

    if (flatten_args_new(dimension_type, &args) != 0)
    {
        return -1;
    }

    if (harp_operation_new(harp_operation_flatten, args, &operation) != 0)
    {
        flatten_args_delete(args);
        return -1;
    }

    *new_operation = operation;
    return 0;
}

int harp_keep_variable_new(int num_variables, const char **variable_name, harp_operation **new_operation)
{
    harp_keep_variable_args *args;
    harp_operation *operation;

    if (keep_variable_args_new(num_variables, variable_name, &args) != 0)
    {
        return -1;
    }

    if (harp_operation_new(harp_operation_keep_variable, args, &operation) != 0)
    {
        keep_variable_args_delete(args);
        return -1;
    }

    *new_operation = operation;
    return 0;
}

int harp_longitude_range_filter_new(double min, const char *min_unit, double max, const char *max_unit,
                                    harp_operation **new_operation)
{
    harp_longitude_range_filter_args *args;
    harp_operation *operation;

    if (longitude_range_filter_args_new(min, min_unit, max, max_unit, &args) != 0)
    {
        return -1;
    }

    if (harp_operation_new(harp_operation_longitude_range_filter, args, &operation) != 0)
    {
        longitude_range_filter_args_delete(args);
        return -1;
    }

    *new_operation = operation;
    return 0;
}

int harp_membership_filter_new(const char *variable_name, harp_membership_operator_type operator_type, int num_values,
                               const double *value, const char *unit, harp_operation **new_operation)
{
    harp_membership_filter_args *args;
    harp_operation *operation;

    if (membership_filter_args_new(variable_name, operator_type, num_values, value, unit, &args) != 0)
    {
        return -1;
    }

    if (harp_operation_new(harp_operation_membership_filter, args, &operation) != 0)
    {
        membership_filter_args_delete(args);
        return -1;
    }

    *new_operation = operation;
    return 0;
}

int harp_point_distance_filter_new(double latitude, const char *latitude_unit, double longitude,
                                   const char *longitude_unit, double distance, const char *distance_unit,
                                   harp_operation **new_operation)
{
    harp_point_distance_filter_args *args;
    harp_operation *operation;

    if (point_distance_filter_args_new(latitude, latitude_unit, longitude, longitude_unit, distance, distance_unit,
                                       &args) != 0)
    {
        return -1;
    }

    if (harp_operation_new(harp_operation_point_distance_filter, args, &operation) != 0)
    {
        point_distance_filter_args_delete(args);
        return -1;
    }

    *new_operation = operation;
    return 0;
}

int harp_point_in_area_filter_new(double latitude, const char *latitude_unit, double longitude,
                                  const char *longitude_unit, harp_operation **new_operation)
{
    harp_point_in_area_filter_args *args;
    harp_operation *operation;

    if (point_in_area_filter_args_new(latitude, latitude_unit, longitude, longitude_unit, &args) != 0)
    {
        return -1;
    }

    if (harp_operation_new(harp_operation_point_in_area_filter, args, &operation) != 0)
    {
        point_in_area_filter_args_delete(args);
        return -1;
    }

    *new_operation = operation;
    return 0;
}

int harp_regrid_new(harp_dimension_type dimension_type, const char *axis_variable_name, const char *axis_unit,
                    long num_values, double *values, harp_operation **new_operation)
{
    harp_regrid_args *args;
    harp_operation *operation;

    if (regrid_args_new(dimension_type, axis_variable_name, axis_unit, num_values, values, &args) != 0)
    {
        return -1;
    }

    if (harp_operation_new(harp_operation_regrid, args, &operation) != 0)
    {
        regrid_args_delete(args);
        return -1;
    }

    *new_operation = operation;
    return 0;
}

int harp_regrid_collocated_new(harp_dimension_type dimension_type, const char *axis_variable_name,
                               const char *axis_unit, const char *collocation_result, const char target_dataset,
                               const char *dataset_dir, harp_operation **new_operation)
{
    harp_regrid_collocated_args *args;
    harp_operation *operation;

    if (regrid_collocated_args_new(dimension_type, axis_variable_name, axis_unit, collocation_result, target_dataset,
                                   dataset_dir, &args) != 0)
    {
        return -1;
    }

    if (harp_operation_new(harp_operation_regrid_collocated, args, &operation) != 0)
    {
        regrid_collocated_args_delete(args);
        return -1;
    }

    *new_operation = operation;
    return 0;
}

int harp_rename_new(const char *variable_name, const char *new_variable_name, harp_operation **new_operation)
{
    harp_rename_args *args;
    harp_operation *operation;

    if (rename_args_new(variable_name, new_variable_name, &args) != 0)
    {
        return -1;
    }

    if (harp_operation_new(harp_operation_rename, args, &operation) != 0)
    {
        rename_args_delete(args);
        return -1;
    }

    *new_operation = operation;
    return 0;
}

int harp_smooth_collocated_new(int num_variables, const char **variable_name, harp_dimension_type dimension_type,
                               const char *axis_variable_name, const char *axis_unit, const char *collocation_result,
                               const char target_dataset, const char *dataset_dir, harp_operation **new_operation)
{
    harp_smooth_collocated_args *args;
    harp_operation *operation;

    if (smooth_collocated_args_new(num_variables, variable_name, dimension_type, axis_variable_name, axis_unit,
                                   collocation_result, target_dataset, dataset_dir, &args) != 0)
    {
        return -1;
    }

    if (harp_operation_new(harp_operation_smooth_collocated, args, &operation) != 0)
    {
        smooth_collocated_args_delete(args);
        return -1;
    }

    *new_operation = operation;
    return 0;
}

int harp_string_comparison_filter_new(const char *variable_name, harp_comparison_operator_type operator_type,
                                      const char *value, harp_operation **new_operation)
{
    harp_string_comparison_filter_args *args;
    harp_operation *operation;

    if (string_comparison_filter_args_new(variable_name, operator_type, value, &args) != 0)
    {
        return -1;
    }

    if (harp_operation_new(harp_operation_string_comparison_filter, args, &operation) != 0)
    {
        string_comparison_filter_args_delete(args);
        return -1;
    }

    *new_operation = operation;
    return 0;
}

int harp_string_membership_filter_new(const char *variable_name, harp_membership_operator_type operator_type,
                                      int num_values, const char **value, harp_operation **new_operation)
{
    harp_string_membership_filter_args *args;
    harp_operation *operation;

    if (string_membership_filter_args_new(variable_name, operator_type, num_values, value, &args) != 0)
    {
        return -1;
    }

    if (harp_operation_new(harp_operation_string_membership_filter, args, &operation) != 0)
    {
        string_membership_filter_args_delete(args);
        return -1;
    }

    *new_operation = operation;
    return 0;
}

int harp_valid_range_filter_new(const char *variable_name, harp_operation **new_operation)
{
    harp_valid_range_filter_args *args;
    harp_operation *operation;

    if (valid_range_filter_args_new(variable_name, &args) != 0)
    {
        return -1;
    }

    if (harp_operation_new(harp_operation_valid_range_filter, args, &operation) != 0)
    {
        valid_range_filter_args_delete(args);
        return -1;
    }

    *new_operation = operation;
    return 0;
}

int harp_wrap_new(const char *variable_name, const char *unit, double min, double max, harp_operation **new_operation)
{
    harp_wrap_args *args;
    harp_operation *operation;

    if (wrap_args_new(variable_name, unit, min, max, &args) != 0)
    {
        return -1;
    }

    if (harp_operation_new(harp_operation_wrap, args, &operation) != 0)
    {
        wrap_args_delete(args);
        return -1;
    }

    *new_operation = operation;
    return 0;
}

int harp_operation_get_variable_name(const harp_operation *operation, const char **variable_name)
{
    switch (operation->type)
    {
        case harp_operation_bit_mask_filter:
            *variable_name = ((harp_bit_mask_filter_args *)operation->args)->variable_name;
            break;
        case harp_operation_comparison_filter:
            *variable_name = ((harp_comparison_filter_args *)operation->args)->variable_name;
            break;
        case harp_operation_longitude_range_filter:
            *variable_name = "longitude";
            break;
        case harp_operation_membership_filter:
            *variable_name = ((harp_membership_filter_args *)operation->args)->variable_name;
            break;
        case harp_operation_string_comparison_filter:
            *variable_name = ((harp_string_comparison_filter_args *)operation->args)->variable_name;
            break;
        case harp_operation_string_membership_filter:
            *variable_name = ((harp_string_membership_filter_args *)operation->args)->variable_name;
            break;
        case harp_operation_valid_range_filter:
            *variable_name = ((harp_valid_range_filter_args *)operation->args)->variable_name;
            break;
        default:
            harp_set_error(HARP_ERROR_OPERATION, "operation has no variable name");
            return -1;
    }

    return 0;
}

int harp_operation_is_dimension_filter(const harp_operation *operation)
{
    switch (operation->type)
    {
        case harp_operation_area_mask_covers_area_filter:
        case harp_operation_area_mask_covers_point_filter:
        case harp_operation_area_mask_intersects_area_filter:
        case harp_operation_bit_mask_filter:
        case harp_operation_comparison_filter:
        case harp_operation_longitude_range_filter:
        case harp_operation_membership_filter:
        case harp_operation_point_distance_filter:
        case harp_operation_point_in_area_filter:
        case harp_operation_string_comparison_filter:
        case harp_operation_string_membership_filter:
        case harp_operation_valid_range_filter:
            return 1;
        default:
            return 0;
    }
}
