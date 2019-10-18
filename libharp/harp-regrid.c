/*
 * Copyright (C) 2015-2019 S[&]T, The Netherlands.
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

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define MAX_NAME_LENGTH 128

typedef enum resample_type_enum
{
    resample_skip,
    resample_remove,
    resample_linear,
    resample_log,       /* interpolate linear using coordinates [x,log(y)] */
    resample_loglog,    /* interpolate linear using coordinates [log(x),log(y)] */
    resample_interval
} resample_type;


static long get_unpadded_length(double *vector, long vector_length)
{
    long i;

    for (i = vector_length - 1; i >= 0; i--)
    {
        if (!harp_isnan(vector[i]))
        {
            return i + 1;
        }
    }

    return vector_length;
}

static resample_type get_resample_type(harp_variable *variable, harp_dimension_type dimension_type)
{
    int num_matching_dims;
    int i;

    if (dimension_type == harp_dimension_time)
    {
        /* also remove these variables if they are provided as scalars (without time dimension) */

        /* we can't interpolate these datetime boundary edge values */
        if (strcmp(variable->name, "datetime_start") == 0 || strcmp(variable->name, "datetime_stop") == 0)
        {
            return resample_remove;
        }
        /* datetime_length requires interval interpolation which is currently not supported for the time dimension */
        if (strcmp(variable->name, "datetime_length") == 0)
        {
            return resample_interval;
        }
    }

    /* ensure that there is only 1 dimension of the given type */
    for (i = 0, num_matching_dims = 0; i < variable->num_dimensions; i++)
    {
        if (variable->dimension_type[i] == dimension_type)
        {
            num_matching_dims++;
        }
    }

    if (num_matching_dims == 0)
    {
        /* if the variable has no matching dimension, we should always skip */
        return resample_skip;
    }

    /* we can't resample strings */
    if (variable->data_type == harp_type_string)
    {
        return resample_remove;
    }

    /* we can't resample data without a unit */
    if (variable->unit == NULL)
    {
        /* this also (intentionally) removes 'index' and 'count' variables when regridding in the time dimension */
        return resample_remove;
    }

    if (num_matching_dims != 1)
    {
        /* remove all variables with more than one matching dimension */
        /* TODO: how to resample 2D AVKs */
        return resample_remove;
    }

    /* uncertainty propagation needs to be handled differently (remove for now) */
    if (strstr(variable->name, "_uncertainty") != NULL)
    {
        return resample_remove;
    }

    /* boundary variables needs to be handled differently (remove for now) */
    if (strstr(variable->name, "_bounds") != NULL)
    {
        return resample_remove;
    }

    if (dimension_type == harp_dimension_vertical)
    {
        /* use interval interpolation for vertical regridding of 1D column AVKs */
        if (strstr(variable->name, "_avk") != NULL)
        {
            return resample_interval;
        }
        /* use interval interpolation for vertical regridding of partial column profiles */
        if (strstr(variable->name, "_column_") != NULL)
        {
            return resample_interval;
        }
    }

    if (dimension_type == harp_dimension_spectral)
    {
        if (strstr(variable->name, "aerosol_optical_depth") != NULL ||
            strstr(variable->name, "aerosol_extinction_coefficient") != NULL)
        {
            return resample_loglog;
        }
    }

    /* resample linearly by default */
    return resample_linear;
}

static int needs_interval_resample(harp_product *product, harp_dimension_type dimension_type)
{
    int i;

    for (i = 0; i < product->num_variables; i++)
    {
        if (get_resample_type(product->variable[i], dimension_type) == resample_interval)
        {
            return 1;
        }
    }

    return 0;
}

static int resize_dimension(harp_product *product, harp_dimension_type dimension_type, long num_elements)
{
    int i;

    for (i = 0; i < product->num_variables; i++)
    {
        harp_variable *var = product->variable[i];
        int j;

        for (j = 0; j < var->num_dimensions; j++)
        {
            if (var->dimension_type[j] == dimension_type)
            {
                if (harp_variable_resize_dimension(var, j, num_elements) != 0)
                {
                    return -1;
                }
            }
        }
    }

    product->dimension[dimension_type] = num_elements;

    return 0;
}


static int filter_resamplable_variables(harp_product *product, harp_dimension_type dimension_type)
{
    int i;

    for (i = product->num_variables - 1; i >= 0; i--)
    {
        if (get_resample_type(product->variable[i], dimension_type) == resample_remove)
        {
            if (harp_product_remove_variable(product, product->variable[i]) != 0)
            {
                return -1;
            }
        }
    }

    return 0;
}

int harp_product_get_derived_bounds_for_grid(harp_product *product, harp_variable *grid, harp_variable **bounds)
{
    if (grid->num_dimensions == 1 && grid->dimension_type[0] == harp_dimension_time &&
        strcmp(grid->name, "datetime") == 0)
    {
        harp_variable *datetime_start = NULL;
        harp_variable *datetime_stop = NULL;
        harp_data_type data_type = harp_type_double;
        long i;

        /* try to derive datetime_start and datetime_stop and store these into the bounds variable */
        if (harp_product_get_derived_variable(product, "datetime_start", &data_type, grid->unit,
                                              grid->num_dimensions, grid->dimension_type, &datetime_start) != 0)
        {
            return -1;
        }
        /* extend datetime_start variable to add the stop times */
        if (harp_variable_add_dimension(datetime_start, datetime_start->num_dimensions, harp_dimension_independent, 2)
            != 0)
        {
            harp_variable_delete(datetime_start);
            return -1;
        }
        if (harp_product_get_derived_variable(product, "datetime_stop", &data_type, grid->unit,
                                              grid->num_dimensions, grid->dimension_type, &datetime_stop) != 0)
        {
            harp_variable_delete(datetime_start);
            return -1;
        }
        for (i = 0; i < grid->num_elements; i++)
        {
            datetime_start->data.double_data[2 * i + 1] = datetime_stop->data.double_data[i];
        }
        harp_variable_delete(datetime_stop);
        *bounds = datetime_start;
    }
    else
    {
        harp_dimension_type dim_type[HARP_MAX_NUM_DIMS];
        char *bounds_name = NULL;
        int i;

        assert(grid->num_dimensions < HARP_MAX_NUM_DIMS);
        for (i = 0; i < grid->num_dimensions; i++)
        {
            dim_type[i] = grid->dimension_type[i];
        }
        dim_type[grid->num_dimensions] = harp_dimension_independent;

        /* derive the name of the bounds variable for the axis */
        bounds_name = malloc(strlen(grid->name) + 7 + 1);
        if (!bounds_name)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string)"
                           " (%s:%u)", __FILE__, __LINE__);
            return -1;
        }
        strcpy(bounds_name, grid->name);
        strcat(bounds_name, "_bounds");

        if (harp_product_get_derived_variable(product, bounds_name, &grid->data_type, grid->unit, grid->num_dimensions + 1,
                                              dim_type, bounds) != 0)
        {
            free(bounds_name);
            return -1;
        }
        free(bounds_name);
    }

    return 0;
}

static int get_bounds_for_grid_from_variable(harp_variable *grid, harp_variable **bounds)
{
    harp_product *product = NULL;

    /* Create a dummy product to allow deriving the bounds for the target grid */
    if (harp_product_new(&product) != 0)
    {
        return -1;
    }
    if (harp_product_add_variable(product, grid) != 0)
    {
        harp_product_delete(product);
        return -1;
    }
    if (harp_product_get_derived_bounds_for_grid(product, grid, bounds) != 0)
    {
        if (harp_product_detach_variable(product, grid) == 0)
        {
            harp_product_delete(product);
        }
        return -1;
    }
    if (harp_product_detach_variable(product, grid) != 0)
    {
        /* we can't delete the product since it still contains the grid (which we can't delete) */
        return -1;
    }
    harp_product_delete(product);

    return 0;
}

int harp_product_clamp_dimension(harp_product *product, harp_dimension_type dimension_type,
                                 const char *grid_variable_name, const char *unit, double lower_bound,
                                 double upper_bound)
{
    harp_variable *target_grid;
    harp_variable *target_bounds;
    harp_data_type data_type = harp_type_double;
    long num_time_elements;
    long max_local_dim_length = 0;
    long dim_length;
    long i, j;

    /* make sure lower_bound is the minimum and upper_bound is the maximum value */
    if (lower_bound > upper_bound)
    {
        double temp = lower_bound;

        lower_bound = upper_bound;
        upper_bound = temp;
    }

    if (harp_product_get_derived_variable(product, grid_variable_name, &data_type, unit, 1, &dimension_type,
                                          &target_grid) != 0)
    {
        harp_dimension_type grid_dim_type[2];

        if (dimension_type == harp_dimension_time)
        {
            return -1;
        }

        /* Failed to derive time independent. Try time dependent. */
        grid_dim_type[0] = harp_dimension_time;
        grid_dim_type[1] = dimension_type;
        if (harp_product_get_derived_variable(product, grid_variable_name, &data_type, unit, 2, grid_dim_type,
                                              &target_grid) != 0)
        {
            return -1;
        }
    }

    if (harp_product_get_derived_bounds_for_grid(product, target_grid, &target_bounds) != 0)
    {
        harp_variable_delete(target_grid);
        return -1;
    }

    if (target_grid->num_dimensions == 2)
    {
        num_time_elements = target_grid->dimension[0];
        dim_length = target_grid->dimension[1];
    }
    else
    {
        num_time_elements = 1;
        dim_length = target_grid->dimension[0];
    }

    /* adapt target_grid/target_bounds to match the clamp min/max */
    for (i = 0; i < num_time_elements; i++)
    {
        long offset = i * dim_length;
        long local_dim_length;
        long index;
        int ascend;

        local_dim_length = get_unpadded_length(&target_grid->data.double_data[offset], dim_length);
        if (local_dim_length == 0)
        {
            continue;
        }

        ascend = (target_bounds->data.double_data[(offset + local_dim_length) * 2 - 1] >=
                  target_bounds->data.double_data[offset * 2]);

        /* clamp lower boundary */
        if (harp_isfinite(ascend ? lower_bound : upper_bound))
        {
            index = -1;
            if (ascend)
            {
                if (target_bounds->data.double_data[offset * 2] < lower_bound ||
                    harp_isnan(target_bounds->data.double_data[offset * 2]))
                {
                    index = 0;
                    while (index < local_dim_length &&
                           (target_bounds->data.double_data[(offset + index) * 2 + 1] <= lower_bound ||
                            harp_isnan(target_bounds->data.double_data[(offset + index) * 2 + 1])))
                    {
                        index++;
                    }
                }
            }
            else
            {
                if (target_bounds->data.double_data[offset * 2] > upper_bound ||
                    harp_isnan(target_bounds->data.double_data[offset * 2]))
                {
                    index = 0;
                    while (index < local_dim_length &&
                           (target_bounds->data.double_data[(offset + index) * 2 + 1] >= upper_bound ||
                            harp_isnan(target_bounds->data.double_data[(offset + index) * 2 + 1])))
                    {
                        index++;
                    }
                }
            }

            /* index equals -1 if clamp value is before lower bound */
            /* index equals local_dim_length if clamp value is after upper bound */
            if (index >= 0)
            {
                if (index > 0)
                {
                    /* remove lower points */
                    local_dim_length -= index;
                    for (j = 0; j < local_dim_length; j++)
                    {
                        target_grid->data.double_data[offset + j] = target_grid->data.double_data[offset + j + index];
                        target_bounds->data.double_data[(offset + j) * 2] =
                            target_bounds->data.double_data[(offset + j + index) * 2];
                        target_bounds->data.double_data[(offset + j) * 2 + 1] =
                            target_bounds->data.double_data[(offset + j + index) * 2 + 1];
                    }
                    for (j = local_dim_length; j < dim_length; j++)
                    {
                        target_grid->data.double_data[offset + j] = harp_nan();
                        target_bounds->data.double_data[(offset + j) * 2] = harp_nan();
                        target_bounds->data.double_data[(offset + j) * 2 + 1] = harp_nan();
                    }
                    if (local_dim_length == 0)
                    {
                        continue;
                    }
                }
                /* clamp lower bound */
                target_bounds->data.double_data[offset * 2] = ascend ? lower_bound : upper_bound;
                /* set lowest axis coordinate value to mid-point of new lower bounds */
                if (dimension_type == harp_dimension_vertical && strcmp(grid_variable_name, "pressure") == 0)
                {
                    target_grid->data.double_data[offset] =
                        exp((log(target_bounds->data.double_data[offset * 2]) +
                             log(target_bounds->data.double_data[offset * 2 + 1])) / 2.0);
                }
                else
                {
                    target_grid->data.double_data[offset] =
                        (target_bounds->data.double_data[offset * 2] +
                         target_bounds->data.double_data[offset * 2 + 1]) / 2.0;
                }
            }
        }

        /* clamp upper boundary */
        if (harp_isfinite(ascend ? upper_bound : lower_bound))
        {
            index = local_dim_length;
            if (ascend)
            {
                if (target_bounds->data.double_data[(offset + local_dim_length) * 2 - 1] > upper_bound ||
                    harp_isnan(target_bounds->data.double_data[(offset + local_dim_length) * 2 - 1]))
                {
                    index = local_dim_length - 1;
                    while (index >= 0 &&
                           (target_bounds->data.double_data[(offset + index) * 2] >= upper_bound ||
                            harp_isnan(target_bounds->data.double_data[(offset + index) * 2])))
                    {
                        index--;
                    }
                }
            }
            else
            {
                if (target_bounds->data.double_data[(offset + local_dim_length) * 2 - 1] < lower_bound ||
                    harp_isnan(target_bounds->data.double_data[(offset + local_dim_length) * 2 - 1]))
                {
                    index = local_dim_length - 1;
                    while (index >= 0 &&
                           (target_bounds->data.double_data[(offset + index) * 2] <= lower_bound ||
                            harp_isnan(target_bounds->data.double_data[(offset + index) * 2])))
                    {
                        index--;
                    }
                }
            }

            /* index equals local_dim_length if clamp value is after upper bound */
            /* index equals -1 if clamp value is before lower bound */
            if (index < local_dim_length)
            {
                /* remove upper points */
                for (j = index + 1; j < local_dim_length; j++)
                {
                    target_grid->data.double_data[offset + j] = harp_nan();
                    target_bounds->data.double_data[(offset + j) * 2] = harp_nan();
                    target_bounds->data.double_data[(offset + j) * 2 + 1] = harp_nan();
                }
                local_dim_length = index + 1;
                if (local_dim_length == 0)
                {
                    continue;
                }
                /* clamp upper bound */
                target_bounds->data.double_data[(offset + index) * 2 + 1] = ascend ? upper_bound : lower_bound;
                /* set uppermost axis coordinate value to mid-point of new upper bounds */
                if (dimension_type == harp_dimension_vertical && strcmp(grid_variable_name, "pressure") == 0)
                {
                    target_grid->data.double_data[(offset + index)] =
                        exp((log(target_bounds->data.double_data[(offset + index) * 2]) +
                             log(target_bounds->data.double_data[(offset + index) * 2 + 1])) / 2.0);
                }
                else
                {
                    target_grid->data.double_data[(offset + index)] =
                        (target_bounds->data.double_data[(offset + index) * 2] +
                         target_bounds->data.double_data[(offset + index) * 2 + 1]) / 2.0;
                }
            }
        }

        if (max_local_dim_length < local_dim_length)
        {
            max_local_dim_length = local_dim_length;
        }
    }

    if (max_local_dim_length < dim_length)
    {
        if (harp_variable_resize_dimension(target_grid, target_grid->num_dimensions - 1, max_local_dim_length) != 0)
        {
            harp_variable_delete(target_grid);
            harp_variable_delete(target_bounds);
            return -1;
        }
        if (harp_variable_resize_dimension(target_bounds, target_bounds->num_dimensions - 2, max_local_dim_length) != 0)
        {
            harp_variable_delete(target_grid);
            harp_variable_delete(target_bounds);
            return -1;
        }
    }

    /* regrid product using the clamped axis variables */
    if (harp_product_regrid_with_axis_variable(product, target_grid, target_bounds) != 0)
    {
        harp_variable_delete(target_grid);
        harp_variable_delete(target_bounds);
        return -1;
    }

    harp_variable_delete(target_grid);
    harp_variable_delete(target_bounds);

    return 0;
}

/** \addtogroup harp_product
 * @{
 */

/**
 * Resample all variables in product against a specified grid.
 * The target grid variable should be an axis variable containing the target grid (as 'double' values).
 * It should be a one-dimensional variable (for a time independent grid or when regridding in the time dimension)
 * or a two-dimensional variable (for a time dependent grid when not regridding in the time dimension).
 * The dimension to use for regridding is based on the type of the last dimenion of the target grid variable.
 * This function cannot be used to regrid an independent dimension.
 *
 * If the target grid variable is two dimensional then its time dimension should match that of the product.
 *
 * For each variable in the product a dimension-specific rule based on the variable name will determine how to regrid
 * the variable (point/interval interpolation).
 * If interval interpolation is needed for one of the variables then target boundaries are needed.
 * These can be provided using the optional target_bounds parameter. If this parameter is not provided, the boundaries
 * will be calculated automatically from the target grid (by inter/extrapolating intervals from mid-points).
 *
 * The source grid (and bounds) are determined by performing a variable derivation on the product (using the variable
 * name of the target_grid variable).
 *
 * \param product Product to resample.
 * \param target_grid Target grid variable.
 * \param target_bounds Target grid boundaries variable (optional).
 *
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_product_regrid_with_axis_variable(harp_product *product, harp_variable *target_grid,
                                                       harp_variable *target_bounds)
{
    harp_dimension_type dimension_type;
    long source_max_dim_elements;       /* actual elems + NaN padding */
    long source_grid_max_dim_elements;
    long source_grid_num_dim_elements;
    long target_grid_max_dim_elements;
    long target_grid_num_dim_elements;
    long source_num_time_elements;
    int source_grid_num_dims = 1;
    int target_grid_num_dims;
    int out_of_bound_flag;
    harp_variable *variable;
    long i;

    /* owned memory */
    harp_variable *source_grid = NULL;
    harp_variable *source_bounds = NULL;
    harp_variable *local_target_grid = NULL;
    harp_variable *local_target_bounds = NULL;
    double *source_buffer = NULL;
    double *target_buffer = NULL;

    out_of_bound_flag = harp_get_option_regrid_out_of_bounds();

    if (target_grid->data_type != harp_type_double)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "invalid data type for axis variable");
        return -1;
    }
    target_grid_num_dims = target_grid->num_dimensions;
    if (target_grid_num_dims != 1 && target_grid_num_dims != 2)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "invalid dimensions for axis variable");
        return -1;
    }
    dimension_type = target_grid->dimension_type[target_grid->num_dimensions - 1];
    if (dimension_type == harp_dimension_independent)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "invalid dimensions for axis variable");
        return -1;
    }
    if (target_grid_num_dims == 2)
    {
        if (target_grid->dimension_type[0] != harp_dimension_time || dimension_type == harp_dimension_time)
        {
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "invalid dimensions for axis variable");
            return -1;
        }
        if (target_grid->dimension[0] != product->dimension[harp_dimension_time])
        {
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "time dimension of axis variable does not match product");
            return -1;
        }
    }
    target_grid_max_dim_elements = target_grid->dimension[target_grid_num_dims - 1];

    if (harp_variable_copy(target_grid, &local_target_grid) != 0)
    {
        goto error;
    }

    if (target_bounds != NULL)
    {
        if (target_bounds->data_type != harp_type_double)
        {
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "invalid data type for axis bounds variable");
            return -1;
        }
        if (target_bounds->num_dimensions != target_grid_num_dims + 1)
        {
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "inconsistent dimensions for axis bounds variable");
            return -1;
        }
        if ((target_bounds->dimension_type[0] != target_grid->dimension_type[0]) ||
            (target_bounds->dimension[0] != target_grid->dimension[0]))
        {
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "inconsistent dimensions for axis bounds variable");
            return -1;
        }
        if (target_grid_num_dims == 2)
        {
            if ((target_bounds->dimension_type[1] != target_grid->dimension_type[1]) ||
                (target_bounds->dimension[1] != target_grid->dimension[1]))
            {
                harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "inconsistent dimensions for axis bounds variable");
                return -1;
            }
        }
        if (target_bounds->dimension_type[target_grid_num_dims] != harp_dimension_independent ||
            target_bounds->dimension[target_grid_num_dims] != 2)
        {
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "invalid independent dimension for axis bounds variable");
            return -1;
        }

        if (harp_variable_copy(target_bounds, &local_target_bounds) != 0)
        {
            goto error;
        }
    }

    if (dimension_type == harp_dimension_time)
    {
        source_num_time_elements = 1;

        /* Derive the source grid */
        if (harp_product_get_derived_variable(product, target_grid->name, &target_grid->data_type, target_grid->unit, 1,
                                              target_grid->dimension_type, &source_grid) != 0)
        {
            goto error;
        }
        source_grid_max_dim_elements = source_grid->dimension[0];
        source_max_dim_elements = source_grid_max_dim_elements;
    }
    else
    {
        harp_dimension_type grid_dim_type[2];

        if (product->dimension[harp_dimension_time] == 0)
        {
            /* if the product did not have a time dimension then introduce one with length 1
             * all variables that will be regridded will have this dimension added as first dimension */
            product->dimension[harp_dimension_time] = 1;
        }
        source_num_time_elements = product->dimension[harp_dimension_time];

        grid_dim_type[0] = harp_dimension_time;
        grid_dim_type[1] = dimension_type;

        /* Derive the source grid */
        /* Try time independent */
        if (harp_product_get_derived_variable(product, target_grid->name, &target_grid->data_type, target_grid->unit, 1,
                                              &grid_dim_type[1], &source_grid) != 0)
        {
            /* Failed to derive time independent. Try time dependent. */
            if (harp_product_get_derived_variable(product, target_grid->name, &target_grid->data_type,
                                                  target_grid->unit, 2, grid_dim_type, &source_grid) != 0)
            {
                goto error;
            }
            source_grid_num_dims = 2;
        }
        source_grid_max_dim_elements = source_grid->dimension[source_grid->num_dimensions - 1];
        source_max_dim_elements = source_grid_max_dim_elements;
    }

    /* derive bounds variables if necessary for resampling */
    if (needs_interval_resample(product, dimension_type))
    {
        if (local_target_bounds == NULL)
        {
            if (get_bounds_for_grid_from_variable(local_target_grid, &local_target_bounds) != 0)
            {
                goto error;
            }
        }
        if (harp_product_get_derived_bounds_for_grid(product, source_grid, &source_bounds) != 0)
        {
            goto error;
        }
    }

    /* remove grid variables if they exists (since we don't want to interpolate them) */
    /* this won't affect the source_grid/source_bounds variables that we already derived */
    if (harp_product_has_variable(product, source_grid->name))
    {
        if (harp_product_remove_variable_by_name(product, source_grid->name) != 0)
        {
            goto error;
        }
    }
    if (source_bounds != NULL)
    {
        if (harp_product_has_variable(product, source_bounds->name))
        {
            if (harp_product_remove_variable_by_name(product, source_bounds->name) != 0)
            {
                goto error;
            }
        }
    }

    /* remove variables that can't be resampled */
    if (filter_resamplable_variables(product, dimension_type) != 0)
    {
        goto error;
    }

    /* Use loglin interpolation if vertical pressure grid */
    if (dimension_type == harp_dimension_vertical && strcmp(local_target_grid->name, "pressure") == 0)
    {
        for (i = 0; i < source_grid->num_elements; i++)
        {
            source_grid->data.double_data[i] = log(source_grid->data.double_data[i]);
        }
        for (i = 0; i < local_target_grid->num_elements; i++)
        {
            local_target_grid->data.double_data[i] = log(local_target_grid->data.double_data[i]);
        }
        if (source_bounds != NULL)
        {
            for (i = 0; i < source_bounds->num_elements; i++)
            {
                source_bounds->data.double_data[i] = log(source_bounds->data.double_data[i]);
            }
        }
        if (local_target_bounds != NULL)
        {
            for (i = 0; i < local_target_bounds->num_elements; i++)
            {
                local_target_bounds->data.double_data[i] = log(local_target_bounds->data.double_data[i]);
            }
        }
    }

    /* Resize the dimension in the target product to make room for the resampled data */
    if (target_grid_max_dim_elements > source_max_dim_elements)
    {
        if (resize_dimension(product, dimension_type, target_grid_max_dim_elements) != 0)
        {
            goto error;
        }
        source_max_dim_elements = target_grid_max_dim_elements;
    }

    /* allocate the buffers for the interpolation */
    source_buffer = (double *)malloc(source_max_dim_elements * (size_t)sizeof(double));
    if (source_buffer == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       source_max_dim_elements * sizeof(double), __FILE__, __LINE__);
        goto error;
    }
    target_buffer = (double *)malloc(target_grid_max_dim_elements * (size_t)sizeof(double));
    if (target_buffer == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       target_grid_max_dim_elements * sizeof(double), __FILE__, __LINE__);
        goto error;
    }

    /* regrid each variable */
    for (i = product->num_variables - 1; i >= 0; i--)
    {
        resample_type type;
        long source_time_index;
        long target_time_index;
        long num_blocks;
        long num_elements;
        long j;

        variable = product->variable[i];

        /* Check if we can resample this kind of variable */
        type = get_resample_type(variable, dimension_type);

        assert(type != resample_remove);
        if (type == resample_skip)
        {
            continue;
        }

        /* Ensure that the variable data consists of doubles */
        if (variable->data_type != harp_type_double && harp_variable_convert_data_type(variable, harp_type_double) != 0)
        {
            goto error;
        }

        /* Make time independent variables time dependent if source grid or target grid is 2D (i.e. time dependent) */
        if (source_grid_num_dims > 1 || target_grid_num_dims > 1)
        {
            if (variable->dimension_type[0] != harp_dimension_time)
            {
                if (harp_variable_add_dimension(variable, 0, harp_dimension_time, source_num_time_elements) != 0)
                {
                    return -1;
                }
            }
        }
        /* Als make variable time dependent if the grid dimension is time and the variable does not depend on time */
        if (dimension_type == harp_dimension_time &&
            (variable->num_dimensions == 0 || variable->dimension_type[0] != harp_dimension_time))
        {
            if (harp_variable_add_dimension(variable, 0, harp_dimension_time, source_grid_max_dim_elements) != 0)
            {
                return -1;
            }
        }

        /* treat variable as a [num_blocks, source_max_dim_elements, num_elements] array with indices [j,k,l] */
        num_blocks = 1;
        num_elements = 1;
        j = 0;
        assert(variable->num_dimensions > 0);
        while (variable->dimension_type[j] != dimension_type)
        {
            assert(j < variable->num_dimensions - 1);
            num_blocks *= variable->dimension[j];
            j++;
        }
        j++;    /* skip dimension that is going to be regridded */
        while (j < variable->num_dimensions)
        {
            num_elements *= variable->dimension[j];
            j++;
        }

        /* interpolate the data of the variable over the given dimension */
        /* keep track of time index separately since num_blocks can capture more than just the time dimension */
        source_time_index = 0;
        target_time_index = 0;
        source_grid_num_dim_elements = get_unpadded_length(source_grid->data.double_data, source_grid_max_dim_elements);
        target_grid_num_dim_elements = get_unpadded_length(target_grid->data.double_data, target_grid_max_dim_elements);
        for (j = 0; j < num_blocks; j++)
        {
            long k, l;

            /* keep track of time index for 2D grids */
            if (j % (num_blocks / source_num_time_elements) == 0)
            {
                if (source_grid_num_dims == 2 && j > 0)
                {
                    source_time_index++;
                    source_grid_num_dim_elements =
                        get_unpadded_length(&source_grid->data.double_data[source_time_index *
                                                                           source_grid_max_dim_elements],
                                            source_grid_max_dim_elements);
                }
                if (target_grid_num_dims == 2 && j > 0)
                {
                    target_time_index++;
                    target_grid_num_dim_elements =
                        get_unpadded_length(&target_grid->data.double_data[target_time_index *
                                                                           target_grid_max_dim_elements],
                                            target_grid_max_dim_elements);
                }
            }

            for (l = 0; l < num_elements; l++)
            {
                /* we need to regrid by taking a slice for each sub element 'l' */
                for (k = 0; k < source_grid_num_dim_elements; k++)
                {
                    source_buffer[k] = variable->data.double_data[(j * source_max_dim_elements + k) * num_elements + l];
                }
                if (type == resample_linear)
                {
                    harp_interpolate_array_linear
                        (source_grid_num_dim_elements,
                         &source_grid->data.double_data[source_time_index * source_grid_max_dim_elements],
                         source_buffer, target_grid_num_dim_elements,
                         &local_target_grid->data.double_data[target_time_index * target_grid_max_dim_elements],
                         out_of_bound_flag, target_buffer);
                }
                else if (type == resample_loglog)
                {
                    harp_interpolate_array_logloglinear
                        (source_grid_num_dim_elements,
                         &source_grid->data.double_data[source_time_index * source_grid_max_dim_elements],
                         source_buffer, target_grid_num_dim_elements,
                         &local_target_grid->data.double_data[target_time_index * target_grid_max_dim_elements],
                         out_of_bound_flag, target_buffer);
                }
                else if (type == resample_interval)
                {
                    harp_interval_interpolate_array_linear
                        (source_grid_num_dim_elements,
                         &source_bounds->data.double_data[source_time_index * source_grid_max_dim_elements * 2],
                         source_buffer, target_grid_num_dim_elements,
                         &local_target_bounds->data.double_data[target_time_index * target_grid_max_dim_elements * 2],
                         target_buffer);
                }
                else
                {
                    /* other resampling methods are not supported, but should also never be set */
                    assert(0);
                    exit(1);
                }

                for (k = 0; k < target_grid_num_dim_elements; k++)
                {
                    variable->data.double_data[(j * source_max_dim_elements + k) * num_elements + l] = target_buffer[k];
                }
                for (k = target_grid_num_dim_elements; k < target_grid_max_dim_elements; k++)
                {
                    variable->data.double_data[(j * source_max_dim_elements + k) * num_elements + l] = harp_nan();
                }
            }
        }
    }

    /* Resize the dimension in the target product to minimal size */
    if (target_grid_max_dim_elements < source_max_dim_elements)
    {
        if (resize_dimension(product, dimension_type, target_grid_max_dim_elements) != 0)
        {
            goto error;
        }
    }

    /* ensure consistent axis variables in product */
    if (harp_variable_copy(target_grid, &variable) != 0)
    {
        goto error;
    }
    if (harp_product_add_variable(product, variable) != 0)
    {
        harp_variable_delete(variable);
        goto error;
    }
    /* add axis bounds variable if either it was provided explicitly or if we derived it */
    if (dimension_type != harp_dimension_time)
    {
        if (target_bounds != NULL)
        {
            if (harp_variable_copy(target_bounds, &variable) != 0)
            {
                goto error;
            }
            if (harp_product_add_variable(product, variable) != 0)
            {
                harp_variable_delete(variable);
                goto error;
            }
        }
        else if (local_target_bounds != NULL)
        {
            if (dimension_type == harp_dimension_vertical && strcmp(local_target_grid->name, "pressure") == 0)
            {
                for (i = 0; i < local_target_bounds->num_elements; i++)
                {
                    local_target_bounds->data.double_data[i] = exp(local_target_bounds->data.double_data[i]);
                }
            }
            if (harp_variable_copy(local_target_bounds, &variable) != 0)
            {
                goto error;
            }
            if (harp_product_add_variable(product, variable) != 0)
            {
                harp_variable_delete(variable);
                goto error;
            }
        }
    }

    /* cleanup */
    harp_variable_delete(source_grid);
    harp_variable_delete(source_bounds);
    harp_variable_delete(local_target_grid);
    harp_variable_delete(local_target_bounds);
    free(source_buffer);
    free(target_buffer);

    return 0;

  error:
    harp_variable_delete(source_grid);
    harp_variable_delete(source_bounds);
    harp_variable_delete(local_target_grid);
    harp_variable_delete(local_target_bounds);
    free(source_buffer);
    free(target_buffer);

    return -1;
}

/** Regrid the product's variables to the target grid of the collocated product.
 *
 * This function cannot be used to regrid the time dimension (or an independent dimension).
 *
 * Both the product and the collocated product need to have `collocation_index` variables.
 * These collocation indices will be used to determine the matching pairs.
 * For each `collocation_index` value in \a product there needs to be a matching value in the `collocation_index`
 * variable of \a collocated_product (but the reverse does not have to be true).
 *
 * \param product Product to regrid.
 * \param dimension_type Type of dimension that should be regridded.
 * \param axis_name The name of the variable to use as target grid.
 * \param axis_unit The unit in which the vertical_axis will be brought for the regridding.
 * \param collocated_product The product containing the collocated measurements and the target grid for the regridding.
 *
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_product_regrid_with_collocated_product(harp_product *product, harp_dimension_type dimension_type,
                                                            const char *axis_name, const char *axis_unit,
                                                            const harp_product *collocated_product)
{
    harp_dimension_type local_dimension_type[HARP_NUM_DIM_TYPES];
    harp_data_type data_type;
    harp_product *temp_product = NULL;
    char bounds_name[MAX_NAME_LENGTH];
    harp_variable *collocation_index = NULL;
    harp_variable *target_grid = NULL;
    harp_variable *target_bounds = NULL;
    harp_variable *variable;

    if (dimension_type == harp_dimension_independent || dimension_type == harp_dimension_time)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "can not regrid %s dimension",
                       harp_get_dimension_type_name(dimension_type));

    }
    if (product->dimension[dimension_type] == 0)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "product has no %s dimension",
                       harp_get_dimension_type_name(dimension_type));
        return -1;
    }

    snprintf(bounds_name, MAX_NAME_LENGTH, "%s_bounds", axis_name);

    if (harp_product_new(&temp_product) != 0)
    {
        return -1;
    }

    data_type = harp_type_int32;
    local_dimension_type[0] = harp_dimension_time;
    if (harp_product_get_derived_variable(collocated_product, "collocation_index", &data_type, NULL, 1,
                                          local_dimension_type, &variable) != 0)
    {
        harp_product_delete(temp_product);
        return -1;
    }
    if (harp_product_add_variable(temp_product, variable) != 0)
    {
        harp_variable_delete(variable);
        harp_product_delete(temp_product);
        return -1;
    }

    data_type = harp_type_double;
    if (collocated_product->dimension[dimension_type] == 0)
    {
        /* product does not depend on the regridding dimension
         * if the axis variable is still there (as 'axis_name {time}') then extend it
         * with the given dimension type and treat the length of the dimension as 1
         */
        if (harp_product_get_derived_variable(collocated_product, axis_name, &data_type, axis_unit, 1,
                                              local_dimension_type, &variable) != 0)
        {
            harp_product_delete(temp_product);
            return -1;
        }
        if (harp_variable_add_dimension(variable, 1, dimension_type, 1) != 0)
        {
            harp_variable_delete(variable);
            harp_product_delete(temp_product);
            return -1;
        }
        if (harp_product_add_variable(temp_product, variable) != 0)
        {
            harp_variable_delete(variable);
            harp_product_delete(temp_product);
            return -1;
        }
        /* in this case we don't have a target_bounds variable */
    }
    else
    {
        local_dimension_type[0] = harp_dimension_time;
        local_dimension_type[1] = dimension_type;
        local_dimension_type[2] = harp_dimension_independent;

        /* target grid */
        if (harp_product_get_derived_variable(collocated_product, axis_name, &data_type, axis_unit, 2,
                                              local_dimension_type, &variable) != 0)
        {
            harp_product_delete(temp_product);
            return -1;
        }
        if (harp_product_add_variable(temp_product, variable) != 0)
        {
            harp_variable_delete(variable);
            harp_product_delete(temp_product);
            return -1;
        }

        /* target grid bounds */
        if (harp_product_get_derived_variable(collocated_product, bounds_name, &data_type, axis_unit, 3,
                                              local_dimension_type, &variable) == 0)
        {
            if (harp_product_add_variable(temp_product, variable) != 0)
            {
                harp_variable_delete(variable);
                harp_product_delete(temp_product);
                return -1;
            }
        }
    }

    /* get the source product's collocation index variable */
    if (harp_product_get_variable_by_name(product, "collocation_index", &collocation_index) != 0)
    {
        return -1;
    }

    /* sort/filter the reduced collocated product so the samples are in the same order as in 'product' */
    if (harp_product_filter_by_index(temp_product, "collocation_index", collocation_index->num_elements,
                                     collocation_index->data.int32_data) != 0)
    {
        harp_product_delete(temp_product);
        return -1;
    }

    harp_product_get_variable_by_name(temp_product, axis_name, &target_grid);
    if (harp_product_has_variable(temp_product, bounds_name))
    {
        harp_product_get_variable_by_name(temp_product, bounds_name, &target_bounds);
    }
    if (harp_product_regrid_with_axis_variable(product, target_grid, target_bounds) != 0)
    {
        harp_product_delete(temp_product);
        return -1;
    }

    harp_product_delete(temp_product);

    return 0;
}

/** Regrid the product's variables (from dataset a in the collocation result) to the target grid of collocated products
 * in dataset b.
 *
 * This function cannot be used to regrid the time dimension (or an independent dimension).
 *
 * \param product Product to regrid.
 * \param dimension_type Type of dimension that should be regridded.
 * \param axis_name The name of the variable to use as target grid.
 * \param axis_unit The unit in which the vertical_axis will be brought for the regridding.
 * \param collocation_result The collocation result used to find matching variables.
 *   The collocation result is assumed to have the appropriate metadata available for all matches (dataset b).
 *
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_product_regrid_with_collocated_dataset(harp_product *product, harp_dimension_type dimension_type,
                                                            const char *axis_name, const char *axis_unit,
                                                            harp_collocation_result *collocation_result)
{
    harp_collocation_result *filtered_collocation_result = NULL;
    harp_data_type data_type = harp_type_double;
    harp_product *merged_product = NULL;
    char bounds_name[MAX_NAME_LENGTH];
    harp_variable *collocation_index = NULL;
    harp_variable *target_grid = NULL;
    harp_variable *target_bounds = NULL;
    long i;

    if (dimension_type == harp_dimension_independent || dimension_type == harp_dimension_time)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "can not regrid %s dimension",
                       harp_get_dimension_type_name(dimension_type));

    }
    if (product->dimension[dimension_type] == 0)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "product has no %s dimension",
                       harp_get_dimension_type_name(dimension_type));
        return -1;
    }

    /* get the source product's collocation index variable */
    if (harp_product_get_variable_by_name(product, "collocation_index", &collocation_index) != 0)
    {
        return -1;
    }

    /* copy the collocation result for filtering */
    if (harp_collocation_result_shallow_copy(collocation_result, &filtered_collocation_result) != 0)
    {
        return -1;
    }

    /* reduce the collocation result to only pairs that include the source product */
    if (harp_collocation_result_filter_for_collocation_indices(filtered_collocation_result,
                                                               collocation_index->num_elements,
                                                               collocation_index->data.int32_data) != 0)
    {
        harp_collocation_result_shallow_delete(filtered_collocation_result);
        return -1;
    }
    if (filtered_collocation_result->num_pairs != collocation_index->num_elements)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "product and collocation result are inconsistent");
        harp_collocation_result_shallow_delete(filtered_collocation_result);
        return -1;
    }

    snprintf(bounds_name, MAX_NAME_LENGTH, "%s_bounds", axis_name);

    for (i = 0; i < filtered_collocation_result->dataset_b->num_products; i++)
    {
        harp_dimension_type local_dimension_type[HARP_NUM_DIM_TYPES];
        harp_product *collocated_product;
        long j;

        if (harp_collocation_result_get_filtered_product_b(filtered_collocation_result,
                                                           filtered_collocation_result->dataset_b->source_product[i],
                                                           &collocated_product) != 0)
        {
            harp_product_delete(merged_product);
            harp_collocation_result_shallow_delete(filtered_collocation_result);
            return -1;
        }

        if (collocated_product == NULL || harp_product_is_empty(collocated_product))
        {
            continue;
        }
        if (collocated_product->dimension[dimension_type] == 0)
        {
            /* product does not depend on the regridding dimension
             * if the axis variable is still there (as 'axis_name {time}') then extend it
             * with the given dimension type and treat the length of the dimension as 1
             */
            local_dimension_type[0] = harp_dimension_time;
            if (harp_product_add_derived_variable(collocated_product, axis_name, &data_type, axis_unit, 1,
                                                  local_dimension_type) != 0)
            {
                harp_add_error_message(" for collocated dataset");
                harp_product_delete(collocated_product);
                harp_product_delete(merged_product);
                harp_collocation_result_shallow_delete(filtered_collocation_result);
                return -1;
            }
            if (harp_product_get_variable_by_name(collocated_product, axis_name, &target_grid) != 0)
            {
                harp_product_delete(collocated_product);
                harp_product_delete(merged_product);
                harp_collocation_result_shallow_delete(filtered_collocation_result);
                return -1;
            }
            if (harp_variable_add_dimension(target_grid, 1, dimension_type, 1) != 0)
            {
                harp_product_delete(collocated_product);
                harp_product_delete(merged_product);
                harp_collocation_result_shallow_delete(filtered_collocation_result);
                return -1;
            }
            collocated_product->dimension[dimension_type] = 1;
        }
        local_dimension_type[0] = harp_dimension_time;
        local_dimension_type[1] = dimension_type;
        local_dimension_type[2] = harp_dimension_independent;

        /* target grid */
        if (harp_product_add_derived_variable(collocated_product, axis_name, &data_type, axis_unit, 2,
                                              local_dimension_type) != 0)
        {
            harp_add_error_message(" for collocated dataset");
            harp_product_delete(collocated_product);
            harp_product_delete(merged_product);
            harp_collocation_result_shallow_delete(filtered_collocation_result);
            return -1;
        }

        /* target grid bounds */
        harp_product_add_derived_variable(collocated_product, bounds_name, &data_type, axis_unit, 3,
                                          local_dimension_type);
        /* it is Ok if the target boundaries cannot be derived (we ignore the return value of the function) */

        /* strip collocated product to just the variables that we need */
        for (j = collocated_product->num_variables - 1; j >= 0; j--)
        {
            const char *name = collocated_product->variable[j]->name;

            if (strcmp(name, "collocation_index") != 0 && strcmp(name, axis_name) != 0 &&
                strcmp(name, bounds_name) != 0)
            {
                if (harp_product_remove_variable(collocated_product, collocated_product->variable[j]) != 0)
                {
                    harp_product_delete(collocated_product);
                    harp_product_delete(merged_product);
                    harp_collocation_result_shallow_delete(filtered_collocation_result);
                    return -1;
                }
            }
        }

        if (merged_product == NULL)
        {
            merged_product = collocated_product;
        }
        else
        {
            if (harp_product_append(merged_product, collocated_product) != 0)
            {
                harp_add_error_message(" for collocated dataset");
                harp_product_delete(collocated_product);
                harp_product_delete(merged_product);
                harp_collocation_result_shallow_delete(filtered_collocation_result);
                return -1;
            }
            harp_product_delete(collocated_product);
        }
    }

    if (merged_product == NULL)
    {
        harp_collocation_result_shallow_delete(filtered_collocation_result);
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "collocated dataset does not contain any matching pairs");
        return -1;
    }

    /* sort/filter the merged product so the samples are in the same order as in 'product' */
    if (harp_product_filter_by_index(merged_product, "collocation_index", collocation_index->num_elements,
                                     collocation_index->data.int32_data) != 0)
    {
        harp_add_error_message(" for collocated dataset");
        harp_product_delete(merged_product);
        harp_collocation_result_shallow_delete(filtered_collocation_result);
        return -1;
    }

    harp_product_get_variable_by_name(merged_product, axis_name, &target_grid);
    if (harp_product_has_variable(merged_product, bounds_name))
    {
        harp_product_get_variable_by_name(merged_product, bounds_name, &target_bounds);
    }
    if (harp_product_regrid_with_axis_variable(product, target_grid, target_bounds) != 0)
    {
        harp_product_delete(merged_product);
        harp_collocation_result_shallow_delete(filtered_collocation_result);
        return -1;
    }

    /* cleanup */
    harp_product_delete(merged_product);
    harp_collocation_result_shallow_delete(filtered_collocation_result);

    return 0;
}

/**
 * @}
 */
