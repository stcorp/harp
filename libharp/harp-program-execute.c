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

#include "harp-internal.h"
#include "harp-program.h"
#include "harp-operation.h"
#include "harp-program-execute.h"
#include "harp-predicate.h"
#include "harp-filter.h"
#include "harp-filter-collocation.h"
#include "harp-vertical-profiles.h"

static int evaluate_value_filters_0d(const harp_product *product, harp_program *ops_0d, uint8_t *product_mask)
{
    int i;

    i = 0;
    while (i < ops_0d->num_operations)
    {
        const harp_operation *operation;
        const char *variable_name;
        harp_variable *variable;
        harp_predicate *predicate;

        operation = ops_0d->operation[i];
        if (harp_operation_get_variable_name(operation, &variable_name) != 0)
        {
            /* Operation is not a variable filter, skip it. */
            i++;
            continue;
        }

        if (harp_product_get_variable_by_name(product, variable_name, &variable) != 0)
        {
            /* non existant variable is an error */
            harp_set_error(HARP_ERROR_OPERATION, OPERATION_FILTER_NON_EXISTANT_VARIABLE_FORMAT, variable_name);
            return -1;
        }

        /* We were promised 0D operations */
        assert(variable->num_dimensions == 0);

        if (harp_get_filter_predicate_for_operation(operation, variable->data_type, variable->unit, variable->valid_min,
                                                    variable->valid_max, &predicate) != 0)
        {
            return -1;
        }

        if (harp_predicate_update_mask_0d(predicate, variable, product_mask) != 0)
        {
            harp_predicate_delete(predicate);
            return -1;
        }
        harp_predicate_delete(predicate);

        if (harp_program_remove_operation_at_index(ops_0d, i) != 0)
        {
            return -1;
        }
    }

    return 0;
}

static int evaluate_value_filters_1d(const harp_product *product, harp_program *ops_1d,
                                     harp_dimension_mask_set *dimension_mask_set)
{
    int i;

    i = 0;
    while (i < ops_1d->num_operations)
    {
        const harp_operation *operation;
        const char *variable_name;
        harp_variable *variable;
        harp_dimension_type dimension_type;
        harp_predicate *predicate;

        operation = ops_1d->operation[i];
        if (harp_operation_get_variable_name(operation, &variable_name) != 0)
        {
            /* Operation is not a variable filter, skip it. */
            i++;
            continue;
        }

        if (harp_product_get_variable_by_name(product, variable_name, &variable) != 0)
        {
            return -1;
        }

        /* We were promised 1D operations */
        assert(variable->num_dimensions == 1);

        dimension_type = variable->dimension_type[0];
        if (dimension_type == harp_dimension_independent)
        {
            harp_set_error(HARP_ERROR_OPERATION, "variable '%s' has independent outer dimension", variable->name);
            return -1;
        }

        if (dimension_mask_set[dimension_type] == NULL)
        {
            long dimension = product->dimension[dimension_type];

            /* Create dimension mask if necessary. */
            if (harp_dimension_mask_new(1, &dimension, &dimension_mask_set[dimension_type]) != 0)
            {
                return -1;
            }
        }

        if (harp_get_filter_predicate_for_operation(operation, variable->data_type, variable->unit, variable->valid_min,
                                                    variable->valid_max, &predicate) != 0)
        {
            return -1;
        }

        if (harp_predicate_update_mask_1d(predicate, variable, dimension_mask_set[dimension_type]) != 0)
        {
            harp_predicate_delete(predicate);
            return -1;
        }
        else
        {
            harp_predicate_delete(predicate);
        }

        if (harp_program_remove_operation_at_index(ops_1d, i) != 0)
        {
            return -1;
        }
    }

    return 0;
}

static int evaluate_value_filters_2d(const harp_product *product, harp_program *ops_2d,
                                     harp_dimension_mask_set *dimension_mask_set)
{
    int i;

    i = 0;
    while (i < ops_2d->num_operations)
    {
        harp_operation *operation;
        const char *variable_name;
        harp_variable *variable;
        harp_dimension_type dimension_type;
        harp_predicate *predicate;

        operation = ops_2d->operation[i];
        if (harp_operation_get_variable_name(operation, &variable_name) != 0)
        {
            /* Operation is not a variable filter, skip it. */
            i++;
            continue;
        }

        if (harp_product_get_variable_by_name(product, variable_name, &variable) != 0)
        {
            return -1;
        }

        /* We were promised 2D operations */
        assert(variable->num_dimensions == 2);

        if (variable->dimension_type[0] != harp_dimension_time)
        {
            harp_set_error(HARP_ERROR_OPERATION, "outer dimension of variable '%s' is of type '%s'; expected '%s'",
                           variable->name, harp_get_dimension_type_name(variable->dimension_type[0]),
                           harp_get_dimension_type_name(harp_dimension_time));
            return -1;
        }

        dimension_type = variable->dimension_type[1];
        if (dimension_type == harp_dimension_independent)
        {
            harp_set_error(HARP_ERROR_OPERATION, "variable '%s' has independent inner dimension", variable->name);
            return -1;
        }

        if (dimension_mask_set[harp_dimension_time] == NULL)
        {
            long dimension = product->dimension[harp_dimension_time];

            if (harp_dimension_mask_new(1, &dimension, &dimension_mask_set[harp_dimension_time]) != 0)
            {
                return -1;
            }
        }

        dimension_type = variable->dimension_type[1];
        if (dimension_mask_set[dimension_type] == NULL)
        {
            long dimension[2] = { product->dimension[harp_dimension_time], product->dimension[dimension_type] };

            if (harp_dimension_mask_new(2, dimension, &dimension_mask_set[dimension_type]) != 0)
            {
                return -1;
            }
        }
        else if (dimension_mask_set[dimension_type]->num_dimensions != 2)
        {
            /* Extend the existing 1-D mask to 2-D by repeating it along the outer dimension. */
            assert(dimension_mask_set[dimension_type]->num_dimensions == 1);
            if (harp_dimension_mask_prepend_dimension(dimension_mask_set[dimension_type],
                                                      product->dimension[harp_dimension_time]) != 0)
            {
                return -1;
            }
        }

        if (harp_get_filter_predicate_for_operation(operation, variable->data_type, variable->unit, variable->valid_min,
                                                    variable->valid_max, &predicate) != 0)
        {
            return -1;
        }

        if (harp_predicate_update_mask_2d(predicate, variable, dimension_mask_set[harp_dimension_time],
                                          dimension_mask_set[dimension_type]) != 0)
        {
            harp_predicate_delete(predicate);
            return -1;
        }
        else
        {
            harp_predicate_delete(predicate);
        }

        if (harp_program_remove_operation_at_index(ops_2d, i) != 0)
        {
            return -1;
        }
    }

    return 0;
}

static int evaluate_point_filters_0d(const harp_product *product, harp_program *ops_0d, uint8_t *product_mask)
{
    harp_predicate_set *predicate_set;
    int i;

    /* Create filter predicates for all point filters and collect them in a predicate set. The filter predicates
     * created here will be re-used for all points. Operations for which a predicate has been created are removed from
     * the list of operations to perform.
     */
    if (harp_predicate_set_new(&predicate_set) != 0)
    {
        return -1;
    }

    i = 0;
    while (i < ops_0d->num_operations)
    {
        const harp_operation *operation;
        harp_predicate *predicate;

        /* Create filter predicate and add it to the predicate set. Remove the operation from the list of operations to
         * perform.
         */
        operation = ops_0d->operation[i];
        switch (operation->type)
        {
            case harp_operation_area_mask_covers_point_filter:
                {
                    const harp_area_mask_covers_point_filter_args *args;

                    args = (const harp_area_mask_covers_point_filter_args *)operation->args;
                    if (harp_area_mask_covers_point_filter_predicate_new(args, &predicate) != 0)
                    {
                        harp_predicate_set_delete(predicate_set);
                        return -1;
                    }
                }
                break;
            case harp_operation_point_distance_filter:
                {
                    const harp_point_distance_filter_args *args;

                    args = (const harp_point_distance_filter_args *)operation->args;
                    if (harp_point_distance_filter_predicate_new(args, &predicate) != 0)
                    {
                        harp_predicate_set_delete(predicate_set);
                        return -1;
                    }
                }
                break;
            default:
                /* Not a point filter, skip. */
                i++;
                continue;
        }

        if (harp_predicate_set_add_predicate(predicate_set, predicate) != 0)
        {
            harp_predicate_delete(predicate);
            harp_predicate_set_delete(predicate_set);
            return -1;
        }

        if (harp_program_remove_operation_at_index(ops_0d, i) != 0)
        {
            harp_predicate_set_delete(predicate_set);
            return -1;
        }
    }

    /* Update product mask. */
    if (predicate_set->num_predicates > 0)
    {
        harp_variable *latitude;
        harp_variable *longitude;

        if (harp_product_get_variable_by_name(product, "latitude", &latitude) != 0)
        {
            harp_predicate_set_delete(predicate_set);
            harp_set_error(HARP_ERROR_OPERATION, OPERATION_FILTER_POINT_MISSING_LAT);
            return -1;
        }

        if (harp_product_get_variable_by_name(product, "longitude", &longitude) != 0)
        {
            harp_predicate_set_delete(predicate_set);
            harp_set_error(HARP_ERROR_OPERATION, OPERATION_FILTER_POINT_MISSING_LON);
            return -1;
        }

        /* we were promised 0D filters */
        assert(latitude->num_dimensions == 0 && longitude->num_dimensions == 0);

        if (harp_point_predicate_update_mask_0d(predicate_set->num_predicates, predicate_set->predicate, latitude,
                                                longitude, product_mask) != 0)
        {
            harp_predicate_set_delete(predicate_set);
            return -1;
        }
    }

    harp_predicate_set_delete(predicate_set);

    return 0;
}

static int evaluate_point_filters_1d(const harp_product *product, harp_program *ops_1d,
                                     harp_dimension_mask_set *dimension_mask_set)
{
    harp_predicate_set *predicate_set = NULL;
    int i;

    /* Create filter predicates for all point filters and collect them in a predicate set. The filter predicates
     * created here will be re-used for all points. Operations for which a predicate has been created are removed from
     * the list of operations to perform.
     */
    if (harp_predicate_set_new(&predicate_set) != 0)
    {
        return -1;
    }

    i = 0;
    while (i < ops_1d->num_operations)
    {
        const harp_operation *operation;
        harp_predicate *predicate;

        /* Create filter predicate and add it to the predicate set. Remove the operation from the list of operations to
         * perform.
         */
        operation = ops_1d->operation[i];
        switch (operation->type)
        {
            case harp_operation_area_mask_covers_point_filter:
                {
                    const harp_area_mask_covers_point_filter_args *args;

                    args = (const harp_area_mask_covers_point_filter_args *)operation->args;
                    if (harp_area_mask_covers_point_filter_predicate_new(args, &predicate) != 0)
                    {
                        harp_predicate_set_delete(predicate_set);
                        return -1;
                    }
                }
                break;
            case harp_operation_point_distance_filter:
                {
                    const harp_point_distance_filter_args *args;

                    args = (const harp_point_distance_filter_args *)operation->args;
                    if (harp_point_distance_filter_predicate_new(args, &predicate) != 0)
                    {
                        harp_predicate_set_delete(predicate_set);
                        return -1;
                    }
                }
                break;
            default:
                /* Not a point filter, skip. */
                i++;
                continue;
        }

        if (harp_predicate_set_add_predicate(predicate_set, predicate) != 0)
        {
            harp_predicate_delete(predicate);
            harp_predicate_set_delete(predicate_set);
            return -1;
        }

        if (harp_program_remove_operation_at_index(ops_1d, i) != 0)
        {
            harp_predicate_set_delete(predicate_set);
            return -1;
        }
    }

    /* Update dimension mask. */
    if (predicate_set->num_predicates > 0)
    {
        harp_variable *latitude;
        harp_variable *longitude;

        if (harp_product_get_variable_by_name(product, "latitude", &latitude) != 0)
        {
            harp_predicate_set_delete(predicate_set);
            harp_set_error(HARP_ERROR_OPERATION, OPERATION_FILTER_POINT_MISSING_LAT);
            return -1;
        }

        if (harp_product_get_variable_by_name(product, "longitude", &longitude) != 0)
        {
            harp_predicate_set_delete(predicate_set);
            harp_set_error(HARP_ERROR_OPERATION, OPERATION_FILTER_POINT_MISSING_LON);
            return -1;
        }

        /* We were promised 1D filters */
        assert(latitude->num_dimensions == 1 && longitude->num_dimensions == 1);

        if (dimension_mask_set[harp_dimension_time] == NULL)
        {
            long dimension = product->dimension[harp_dimension_time];

            if (harp_dimension_mask_new(1, &dimension, &dimension_mask_set[harp_dimension_time]) != 0)
            {
                harp_predicate_set_delete(predicate_set);
                return -1;
            }
        }

        if (harp_point_predicate_update_mask_1d(predicate_set->num_predicates, predicate_set->predicate, latitude,
                                                longitude, dimension_mask_set[harp_dimension_time]) != 0)
        {
            harp_predicate_set_delete(predicate_set);
            return -1;
        }
    }

    harp_predicate_set_delete(predicate_set);

    return 0;
}

static int evaluate_area_filters_0d(const harp_product *product, harp_program *program, uint8_t *product_mask)
{
    harp_predicate_set *predicate_set;
    harp_dimension_type dimension_type[1] = { harp_dimension_independent };
    int i;

    /* Create filter predicates for all area filters and collect them in a predicate set. The filter predicates created
     * here will be re-used for all points. Operations for which a predicate has been created are removed from the list of
     * operations to perform.
     */
    if (harp_predicate_set_new(&predicate_set) != 0)
    {
        return -1;
    }

    i = 0;
    while (i < program->num_operations)
    {
        const harp_operation *operation;
        harp_predicate *predicate;

        /* Create filter predicate and add it to the predicate set. Remove the operation from the list of operations to
         * perform.
         */
        operation = program->operation[i];
        switch (operation->type)
        {
            case harp_operation_area_mask_covers_area_filter:
                {
                    const harp_area_mask_covers_area_filter_args *args;

                    args = (const harp_area_mask_covers_area_filter_args *)operation->args;
                    if (harp_area_mask_covers_area_filter_predicate_new(args, &predicate) != 0)
                    {
                        harp_predicate_set_delete(predicate_set);
                        return -1;
                    }
                }
                break;
            case harp_operation_area_mask_intersects_area_filter:
                {
                    const harp_area_mask_intersects_area_filter_args *args;

                    args = (const harp_area_mask_intersects_area_filter_args *)operation->args;
                    if (harp_area_mask_intersects_area_filter_predicate_new(args, &predicate) != 0)
                    {
                        harp_predicate_set_delete(predicate_set);
                        return -1;
                    }
                }
                break;
            case harp_operation_point_in_area_filter:
                {
                    const harp_point_in_area_filter_args *args;

                    args = (const harp_point_in_area_filter_args *)operation->args;
                    if (harp_point_in_area_filter_predicate_new(args, &predicate) != 0)
                    {
                        harp_predicate_set_delete(predicate_set);
                        return -1;
                    }
                }
                break;
            default:
                /* Not an area filter, skip. */
                i++;
                continue;
        }

        if (harp_predicate_set_add_predicate(predicate_set, predicate) != 0)
        {
            harp_predicate_delete(predicate);
            harp_predicate_set_delete(predicate_set);
            return -1;
        }

        if (harp_program_remove_operation_at_index(program, i) != 0)
        {
            harp_predicate_set_delete(predicate_set);
            return -1;
        }
    }

    if (predicate_set->num_predicates > 0)
    {
        harp_variable *latitude_bounds;
        harp_variable *longitude_bounds;

        if (harp_product_get_variable_by_name(product, "latitude_bounds", &latitude_bounds) != 0)
        {
            harp_predicate_set_delete(predicate_set);
            harp_set_error(HARP_ERROR_OPERATION, OPERATION_FILTER_AREA_MISSING_LAT_BOUNDS);
            return -1;
        }

        if (harp_product_get_variable_by_name(product, "longitude_bounds", &longitude_bounds) != 0)
        {
            harp_predicate_set_delete(predicate_set);
            harp_set_error(HARP_ERROR_OPERATION, OPERATION_FILTER_AREA_MISSING_LON_BOUNDS);
            return -1;
        }

        assert(latitude_bounds->num_dimensions == 1 && longitude_bounds->num_dimensions == 1);
        if (!harp_variable_has_dimension_types(longitude_bounds, 1, dimension_type)
            || !harp_variable_has_dimension_types(longitude_bounds, 1, dimension_type))
        {
            harp_set_error(HARP_ERROR_OPERATION, OPERATION_FILTER_AREA_WRONG_DIMENSION_FORMAT, "{independent}");
            harp_predicate_set_delete(predicate_set);
            return -1;
        }

        if (harp_area_predicate_update_mask_0d(predicate_set->num_predicates, predicate_set->predicate,
                                               latitude_bounds, longitude_bounds, product_mask) != 0)
        {
            harp_predicate_set_delete(predicate_set);
            return -1;
        }
    }

    harp_predicate_set_delete(predicate_set);

    return 0;
}

static int evaluate_area_filters_1d(const harp_product *product, harp_program *ops_1d,
                                    harp_dimension_mask_set *dimension_mask_set)
{
    harp_predicate_set *predicate_set;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_independent };
    int i;

    /* Create filter predicates for all area filters and collect them in a predicate set. The filter predicates created
     * here will be re-used for all points. Operations for which a predicate has been created are removed from the list of
     * operations to perform.
     */
    if (harp_predicate_set_new(&predicate_set) != 0)
    {
        return -1;
    }

    i = 0;
    while (i < ops_1d->num_operations)
    {
        const harp_operation *operation;
        harp_predicate *predicate;

        /* Create filter predicate and add it to the predicate set. Remove the operation from the list of operations to
         * perform.
         */
        operation = ops_1d->operation[i];
        switch (operation->type)
        {
            case harp_operation_area_mask_covers_area_filter:
                {
                    const harp_area_mask_covers_area_filter_args *args;

                    args = (const harp_area_mask_covers_area_filter_args *)operation->args;
                    if (harp_area_mask_covers_area_filter_predicate_new(args, &predicate) != 0)
                    {
                        harp_predicate_set_delete(predicate_set);
                        return -1;
                    }
                }
                break;
            case harp_operation_area_mask_intersects_area_filter:
                {
                    const harp_area_mask_intersects_area_filter_args *args;

                    args = (const harp_area_mask_intersects_area_filter_args *)operation->args;
                    if (harp_area_mask_intersects_area_filter_predicate_new(args, &predicate) != 0)
                    {
                        harp_predicate_set_delete(predicate_set);
                        return -1;
                    }
                }
                break;
            case harp_operation_point_in_area_filter:
                {
                    const harp_point_in_area_filter_args *args;

                    args = (const harp_point_in_area_filter_args *)operation->args;
                    if (harp_point_in_area_filter_predicate_new(args, &predicate) != 0)
                    {
                        harp_predicate_set_delete(predicate_set);
                        return -1;
                    }
                }
                break;
            default:
                /* Not an area filter, skip. */
                i++;
                continue;
        }

        if (harp_predicate_set_add_predicate(predicate_set, predicate) != 0)
        {
            harp_predicate_delete(predicate);
            harp_predicate_set_delete(predicate_set);
            return -1;
        }

        if (harp_program_remove_operation_at_index(ops_1d, i) != 0)
        {
            harp_predicate_set_delete(predicate_set);
            return -1;
        }
    }

    if (predicate_set->num_predicates > 0)
    {
        harp_variable *latitude_bounds;
        harp_variable *longitude_bounds;

        if (harp_product_get_variable_by_name(product, "latitude_bounds", &latitude_bounds) != 0)
        {
            harp_predicate_set_delete(predicate_set);
            harp_set_error(HARP_ERROR_OPERATION, OPERATION_FILTER_AREA_MISSING_LAT_BOUNDS);
            return -1;
        }

        if (harp_product_get_variable_by_name(product, "longitude_bounds", &longitude_bounds) != 0)
        {
            harp_predicate_set_delete(predicate_set);
            harp_set_error(HARP_ERROR_OPERATION, OPERATION_FILTER_AREA_MISSING_LON_BOUNDS);
            return -1;
        }

        /* We were promised 1D area filters, meaning lat/lon bounds are 2D */
        assert(latitude_bounds->num_dimensions == 2);
        assert(longitude_bounds->num_dimensions == 2);
        if (!harp_variable_has_dimension_types(latitude_bounds, 2, dimension_type) ||
            !harp_variable_has_dimension_types(longitude_bounds, 2, dimension_type))
        {
            harp_set_error(HARP_ERROR_OPERATION, OPERATION_FILTER_AREA_WRONG_DIMENSION_FORMAT, "{time, independent}");
            harp_predicate_set_delete(predicate_set);
            return -1;
        }

        if (dimension_mask_set[harp_dimension_time] == NULL)
        {
            long dimension = product->dimension[harp_dimension_time];

            if (harp_dimension_mask_new(1, &dimension, &dimension_mask_set[harp_dimension_time]) != 0)
            {
                harp_predicate_set_delete(predicate_set);
                return -1;
            }
        }

        if (harp_area_predicate_update_mask_1d(predicate_set->num_predicates, predicate_set->predicate,
                                               latitude_bounds, longitude_bounds,
                                               dimension_mask_set[harp_dimension_time]) != 0)
        {
            harp_predicate_set_delete(predicate_set);
            return -1;
        }
    }

    harp_predicate_set_delete(predicate_set);

    return 0;
}

/* Compute 'dimensionality' for filter operations; sets num_dimensions to either 0, 1 or 2.
 */
static int get_operation_dimensionality(harp_product *product, harp_operation *operation, long *num_dimensions)
{
    const char *variable_name = NULL;

    /* collocation filters */
    if (operation->type == harp_operation_collocation_filter)
    {
        *num_dimensions = 1L;
    }

    /* value filters */
    if (harp_operation_get_variable_name(operation, &variable_name) == 0)
    {
        harp_variable *variable = NULL;

        if (harp_product_get_variable_by_name(product, variable_name, &variable) != 0)
        {
            /* non existant variable is an error */
            harp_set_error(HARP_ERROR_OPERATION, OPERATION_FILTER_NON_EXISTANT_VARIABLE_FORMAT, variable_name);
            return -1;
        }
        if (variable->num_dimensions > 2)
        {
            harp_set_error(HARP_ERROR_OPERATION, OPERATION_FILTER_TOO_GREAT_DIMENSION_FORMAT, variable_name);
            return -1;
        }

        *num_dimensions = variable->num_dimensions;
    }
    /* point filters */
    else if (operation->type == harp_operation_area_mask_covers_point_filter ||
             operation->type == harp_operation_point_distance_filter)
    {
        harp_variable *latitude_def = NULL;
        harp_variable *longitude_def = NULL;

        if (harp_product_get_variable_by_name(product, "latitude", &latitude_def) != 0)
        {
            harp_set_error(HARP_ERROR_OPERATION, OPERATION_FILTER_POINT_MISSING_LAT);
            return -1;
        }
        if (harp_product_get_variable_by_name(product, "longitude", &longitude_def) != 0)
        {
            harp_set_error(HARP_ERROR_OPERATION, OPERATION_FILTER_POINT_MISSING_LON);
            return -1;
        }
        /* point filters must be 0D or 1D */
        if (latitude_def->num_dimensions > 1 || longitude_def->num_dimensions > 1)
        {
            harp_set_error(HARP_ERROR_OPERATION, OPERATION_FILTER_POINT_WRONG_DIMENSION_FORMAT, "{time}");
            return -1;
        }

        /* dimensionality is the max of the lat/lon variables */
        *num_dimensions =
            longitude_def->num_dimensions >
            latitude_def->num_dimensions ? longitude_def->num_dimensions : latitude_def->num_dimensions;
    }
    else if (operation->type == harp_operation_area_mask_covers_area_filter ||
             operation->type == harp_operation_area_mask_intersects_area_filter ||
             operation->type == harp_operation_point_in_area_filter)
    {
        harp_variable *latitude_bounds;
        harp_variable *longitude_bounds;

        if (harp_product_get_variable_by_name(product, "latitude_bounds", &latitude_bounds) != 0)
        {
            harp_set_error(HARP_ERROR_OPERATION, OPERATION_FILTER_AREA_MISSING_LAT_BOUNDS);
            return -1;
        }
        if (harp_product_get_variable_by_name(product, "longitude_bounds", &longitude_bounds) != 0)
        {
            harp_set_error(HARP_ERROR_OPERATION, OPERATION_FILTER_AREA_MISSING_LON_BOUNDS);
            return -1;
        }
        /* area filters must be 0D or 1D, which means that the bounds are 1D or 2D resp. */
        if (latitude_bounds->num_dimensions > 2 || longitude_bounds->num_dimensions > 2 ||
            latitude_bounds->num_dimensions < 1 || longitude_bounds->num_dimensions < 1)
        {
            harp_set_error(HARP_ERROR_OPERATION, OPERATION_FILTER_POINT_WRONG_DIMENSION_FORMAT, "{time}");
            return -1;
        }

        *num_dimensions =
            longitude_bounds->num_dimensions >
            latitude_bounds->num_dimensions ? longitude_bounds->num_dimensions : latitude_bounds->num_dimensions;
        /* don't count the independent dimension */
        *num_dimensions -= 1;
    }
    else
    {
        harp_set_error(HARP_ERROR_OPERATION, "encountered unsupported filter during ingestion");
        return -1;
    }

    return 0;
}

/** Execute the prefix of the program of 0..n dimension filter operations.
 *
 * It only considers dimension-filters, because these can be executed out-of-order.
 * This allows us to optimize filtering without the constraints of maintaining the
 * semantics of the program.
 *
 * The most important optimization is that filters are executed in order of their
 * 'dimensionality', i.e. based on the dimension of the masks that they update.
 * This increases performance, because setting a 0 on a N-dim mask, allows us to skip an entire
 * row in an N+1-dim mask.
 */
static int execute_filter_operations(harp_product *product, harp_program *program)
{
    uint8_t product_mask = 1;
    int i;
    int status = -1;    /* assume error */
    int first_non_filter;

    /* owned memory */
    harp_dimension_mask_set *dimension_mask_set = NULL;
    harp_program *ops_0d, *ops_1d, *ops_2d;

    ops_0d = ops_1d = ops_2d = NULL;

    /* greedily exit when program is trivial */
    if (program->num_operations == 0)
    {
        status = 0;
        goto cleanup;
    }

    if (harp_program_new(&ops_0d) != 0 || harp_program_new(&ops_1d) != 0 || harp_program_new(&ops_2d) != 0)
    {
        goto cleanup;
    }

    /* find first non-filter in the program */
    for (first_non_filter = 0; first_non_filter < program->num_operations; first_non_filter++)
    {
        if (!harp_operation_is_dimension_filter(program->operation[first_non_filter]))
        {
            /* done with this phase */
            break;
        }
    }

    /* Pop the prefix of dimension-filters that we'll process into subprograms
     * concerned with 0D, 1D and 2D filter operations respectively.
     */
    for (i = 0; i < first_non_filter; i++)
    {
        harp_operation *operation = NULL;
        long dim = -1;

        if (harp_operation_copy(program->operation[0], &operation) != 0)
        {
            goto cleanup;
        }
        if (get_operation_dimensionality(product, operation, &dim) != 0)
        {
            goto cleanup;
        }
        switch (dim)
        {
            case 0:
                harp_program_add_operation(ops_0d, operation);
                break;
            case 1:
                harp_program_add_operation(ops_1d, operation);
                break;
            case 2:
                harp_program_add_operation(ops_2d, operation);
                break;
            default:
                assert(0);
        }
        if (harp_program_remove_operation_at_index(program, 0) != 0)
        {
            goto cleanup;
        }
    }

    /* Now run these dimension filters in optimized order.
     * Each of the execution function below will go through the dimension_filters and execute
     * operations greedily if possible.
     */

    /* First filter pass (0-D variables). */
    if (evaluate_value_filters_0d(product, ops_0d, &product_mask) != 0)
    {
        goto cleanup;
    }
    if (evaluate_point_filters_0d(product, ops_0d, &product_mask) != 0)
    {
        goto cleanup;
    }
    if (evaluate_area_filters_0d(product, ops_0d, &product_mask) != 0)
    {
        goto cleanup;
    }

    if (product_mask == 0)
    {
        /* the full product is masked out so remove all variables to make it empty */
        harp_product_remove_all_variables(product);
        status = 0;
        goto cleanup;
    }

    /* Second filter pass (1-D variables). */
    if (harp_dimension_mask_set_new(&dimension_mask_set) != 0)
    {
        goto cleanup;
    }
    if (evaluate_value_filters_1d(product, ops_1d, dimension_mask_set) != 0)
    {
        goto cleanup;
    }
    if (evaluate_point_filters_1d(product, ops_1d, dimension_mask_set) != 0)
    {
        goto cleanup;
    }
    if (evaluate_area_filters_1d(product, ops_1d, dimension_mask_set) != 0)
    {
        goto cleanup;
    }

    /* Apply the dimension masks computed so far, to speed up subsequent filtering steps. */
    if (harp_product_filter(product, dimension_mask_set) != 0)
    {
        harp_dimension_mask_set_delete(dimension_mask_set);
        goto cleanup;
    }

    /* bail early */
    if (harp_product_is_empty(product))
    {
        status = 0;
        goto cleanup;
    }

    /* Third filter pass (2-D variables). */
    harp_dimension_mask_set_delete(dimension_mask_set);
    if (harp_dimension_mask_set_new(&dimension_mask_set) != 0)
    {
        goto cleanup;
    }
    if (evaluate_value_filters_2d(product, ops_2d, dimension_mask_set) != 0)
    {
        goto cleanup;
    }
    if (harp_dimension_mask_set_simplify(dimension_mask_set) != 0)
    {
        goto cleanup;
    }

    /* Apply the dimension masks computed so far. */
    if (harp_product_filter(product, dimension_mask_set) != 0)
    {
        goto cleanup;
    }

    if (harp_product_is_empty(product))
    {
        status = 0;
        goto cleanup;
    }

    assert(ops_0d->num_operations == 0);
    assert(ops_1d->num_operations == 0);
    assert(ops_2d->num_operations == 0);

    status = 0;

  cleanup:
    harp_dimension_mask_set_delete(dimension_mask_set);
    harp_program_delete(ops_0d);
    harp_program_delete(ops_1d);
    harp_program_delete(ops_2d);

    return status;
}

/* run a collocation filter operation at the head of program */
static int execute_collocation_filter(harp_product *product, harp_program *program)
{
    harp_operation *operation;
    const harp_collocation_filter_args *args;
    harp_collocation_mask *collocation_mask;

    assert(program->num_operations != 0);
    operation = program->operation[0];
    assert(operation->type == harp_operation_collocation_filter);

    if (product->source_product == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "product attribute 'source_product' is NULL");
        return -1;
    }

    /* Check for the presence of the 'collocation_index' or 'index' variable.
     * Either variable should be 1-D and should depend on the time dimension.
     * Even though subsequent functions will also verify this, this is important for the consistency of
     * error messages with ingestion.
     */
    if (!harp_product_has_variable(product, "collocation_index") && !harp_product_has_variable(product, "index"))
    {
        int dimension_type = harp_dimension_time;

        if (harp_product_add_derived_variable(product, "index", NULL, 1, &dimension_type) != 0)
        {
            return -1;
        }
    }

    args = (const harp_collocation_filter_args *)operation->args;
    if (harp_collocation_mask_import(args->filename, args->filter_type, product->source_product,
                                     &collocation_mask) != 0)
    {
        return -1;
    }

    if (harp_product_apply_collocation_mask(collocation_mask, product) != 0)
    {
        harp_collocation_mask_delete(collocation_mask);
        return -1;
    }
    else
    {
        harp_collocation_mask_delete(collocation_mask);
    }

    if (harp_program_remove_operation_at_index(program, 0) != 0)
    {
        return -1;
    }

    return 0;
}

/* execute the  derived variable operation from the head of program */
static int execute_derive_variable(harp_product *product, harp_program *program)
{
    harp_derive_variable_args *args = NULL;
    harp_operation *operation;

    assert(program->num_operations != 0);
    operation = program->operation[0];
    assert(operation->type == harp_operation_derive_variable);

    /* get operation arguments */
    args = (harp_derive_variable_args *)operation->args;

    /* execute the operation */
    if (harp_product_add_derived_variable(product, args->variable_name, args->unit, args->num_dimensions,
                                          args->dimension_type) != 0)
    {
        return -1;
    }

    /* remove the operation from the queue */
    if (harp_program_remove_operation_at_index(program, 0) != 0)
    {
        return -1;
    }

    return 0;
}

/* execute the calculation of a total column using a column averaging kernel and a-priori */
static int execute_derive_smoothed_column_collocated(harp_product *product, harp_program *program)
{
    harp_derive_smoothed_column_collocated_args *args = NULL;
    harp_operation *operation;
    harp_collocation_result *collocation_result = NULL;
    harp_variable *variable;

    assert(program->num_operations != 0);
    operation = program->operation[0];
    assert(operation->type == harp_operation_derive_smoothed_column_collocated);

    /* get operation arguments */
    args = (harp_derive_smoothed_column_collocated_args *)operation->args;

    if (harp_collocation_result_read(args->collocation_result, &collocation_result) != 0)
    {
        return -1;
    }

    if (args->target_dataset == 'a')
    {
        harp_collocation_result_swap_datasets(collocation_result);
    }
    if (harp_dataset_import(collocation_result->dataset_b, args->dataset_dir) != 0)
    {
        harp_collocation_result_delete(collocation_result);
        return -1;
    }

    /* execute the operation */
    if (harp_product_get_smoothed_column_using_collocated_dataset(product, args->variable_name, args->unit,
                                                                  args->num_dimensions, args->dimension_type,
                                                                  args->axis_variable_name, args->axis_unit,
                                                                  collocation_result, &variable) != 0)
    {
        harp_collocation_result_delete(collocation_result);
        return -1;
    }
    harp_collocation_result_delete(collocation_result);

    if (harp_product_has_variable(product, variable->name))
    {
        if (harp_product_replace_variable(product, variable) != 0)
        {
            harp_variable_delete(variable);
            return -1;
        }
    }
    else
    {
        if (harp_product_add_variable(product, variable) != 0)
        {
            harp_variable_delete(variable);
            return -1;
        }
    }

    /* remove the operation from the queue */
    if (harp_program_remove_operation_at_index(program, 0) != 0)
    {
        return -1;
    }

    return 0;
}

/* execute the variable exclude filter from the head of program */
static int execute_exclude_variable(harp_product *product, harp_program *program)
{
    const harp_exclude_variable_args *ex_args;
    int j;
    harp_operation *operation;

    assert(program->num_operations != 0);
    operation = program->operation[0];
    assert(operation->type == harp_operation_exclude_variable);

    /* unmark the variables to exclude */
    ex_args = (const harp_exclude_variable_args *)operation->args;
    for (j = 0; j < ex_args->num_variables; j++)
    {
        if (harp_product_has_variable(product, ex_args->variable_name[j]))
        {
            /* execute the operation: remove the variable */
            if (harp_product_remove_variable_by_name(product, ex_args->variable_name[j]) != 0)
            {
                return -1;
            }
        }
    }

    /* remove the operation that we executed */
    if (harp_program_remove_operation_at_index(program, 0) != 0)
    {
        return -1;
    }

    return 0;
}

/* run a collocation filter operation at the head of program */
static int execute_flatten(harp_product *product, harp_program *program)
{
    harp_operation *operation;
    const harp_flatten_args *args;

    assert(program->num_operations != 0);
    operation = program->operation[0];
    assert(operation->type == harp_operation_flatten);

    args = (const harp_flatten_args *)operation->args;

    if (harp_product_flatten_dimension(product, args->dimension_type) != 0)
    {
        return -1;
    }

    if (harp_program_remove_operation_at_index(program, 0) != 0)
    {
        return -1;
    }

    return 0;

    return 0;
}

/* execute the variable include filter from the head of program */
static int execute_keep_variable(harp_product *product, harp_program *program)
{
    harp_operation *operation;
    const harp_keep_variable_args *in_args;
    int index;
    int j;

    /* owned mem */
    uint8_t *include_variable_mask;

    assert(program->num_operations != 0);
    operation = program->operation[0];
    assert(operation->type == harp_operation_keep_variable);

    include_variable_mask = (uint8_t *)calloc(product->num_variables, sizeof(uint8_t));
    if (include_variable_mask == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       product->num_variables * sizeof(uint8_t), __FILE__, __LINE__);
        return -1;
    }

    /* assume all variables are excluded */
    for (j = 0; j < product->num_variables; j++)
    {
        include_variable_mask[j] = 0;
    }

    /* set the 'keep' flags in the mask */
    in_args = (const harp_keep_variable_args *)operation->args;
    for (j = 0; j < in_args->num_variables; j++)
    {
        if (harp_product_get_variable_index_by_name(product, in_args->variable_name[j], &index) != 0)
        {
            harp_set_error(HARP_ERROR_OPERATION, OPERATION_KEEP_NON_EXISTANT_VARIABLE_FORMAT,
                           in_args->variable_name[j]);
            free(include_variable_mask);
            return -1;
        }

        include_variable_mask[index] = 1;
    }

    /* filter the variables using the mask */
    for (j = product->num_variables - 1; j >= 0; j--)
    {
        if (!include_variable_mask[j])
        {
            if (harp_product_remove_variable(product, product->variable[j]) != 0)
            {
                free(include_variable_mask);
                return -1;
            }
        }
    }

    free(include_variable_mask);

    /* remove the operation that we execute */
    if (harp_program_remove_operation_at_index(program, 0) != 0)
    {
        return -1;
    }

    return 0;
}

/* run a regridding operation at the head of program */
static int execute_regrid(harp_product *product, harp_program *program)
{
    harp_operation *operation;
    const harp_regrid_args *args;
    harp_variable *target_grid = NULL;

    assert(program->num_operations != 0);
    operation = program->operation[0];
    assert(operation->type == harp_operation_regrid);

    args = (const harp_regrid_args *)operation->args;

    if (args->axis_variable->dimension_type[0] == harp_dimension_time ||
        args->axis_variable->dimension_type[0] == harp_dimension_independent)
    {
        harp_set_error(HARP_ERROR_OPERATION, "regridding of '%s' dimension not supported",
                       harp_get_dimension_type_name(args->axis_variable->dimension_type[0]));
        return -1;
    }

    if (harp_product_regrid_with_axis_variable(product, args->axis_variable, NULL) != 0)
    {
        harp_variable_delete(target_grid);
        return -1;
    }

    if (harp_program_remove_operation_at_index(program, 0) != 0)
    {
        return -1;
    }

    return 0;
}

/* run a regridding operation at the head of program */
static int execute_regrid_collocated(harp_product *product, harp_program *program)
{
    harp_operation *operation;
    const harp_regrid_collocated_args *args;
    harp_collocation_result *collocation_result = NULL;

    assert(program->num_operations != 0);
    operation = program->operation[0];
    assert(operation->type == harp_operation_regrid_collocated);

    args = (const harp_regrid_collocated_args *)operation->args;

    if (harp_collocation_result_read(args->collocation_result, &collocation_result) != 0)
    {
        return -1;
    }

    if (args->target_dataset == 'a')
    {
        harp_collocation_result_swap_datasets(collocation_result);
    }
    if (harp_dataset_import(collocation_result->dataset_b, args->dataset_dir) != 0)
    {
        harp_collocation_result_delete(collocation_result);
        return -1;
    }

    if (harp_product_regrid_with_collocated_dataset(product, args->dimension_type, args->axis_variable_name,
                                                    args->axis_unit, collocation_result) != 0)
    {
        harp_collocation_result_delete(collocation_result);
        return -1;
    }

    if (harp_program_remove_operation_at_index(program, 0) != 0)
    {
        harp_collocation_result_delete(collocation_result);
        return -1;
    }

    return 0;
}

/* run a smoothing operation at the head of program */
static int execute_smooth_collocated(harp_product *product, harp_program *program)
{
    harp_operation *operation;
    const harp_smooth_collocated_args *args;
    harp_collocation_result *collocation_result = NULL;

    assert(program->num_operations != 0);
    operation = program->operation[0];
    assert(operation->type == harp_operation_smooth_collocated);

    args = (const harp_smooth_collocated_args *)operation->args;

    if (args->dimension_type != harp_dimension_vertical)
    {
        harp_set_error(HARP_ERROR_OPERATION, "regridding of '%s' dimension not supported",
                       harp_get_dimension_type_name(args->dimension_type));
        return -1;
    }

    if (harp_collocation_result_read(args->collocation_result, &collocation_result) != 0)
    {
        return -1;
    }

    if (args->target_dataset == 'a')
    {
        harp_collocation_result_swap_datasets(collocation_result);
    }
    if (harp_dataset_import(collocation_result->dataset_b, args->dataset_dir) != 0)
    {
        harp_collocation_result_delete(collocation_result);
        return -1;
    }

    if (harp_product_smooth_vertical_with_collocated_dataset(product, args->num_variables,
                                                             (const char **)args->variable_name,
                                                             args->axis_variable_name, args->axis_unit,
                                                             collocation_result) != 0)
    {
        harp_collocation_result_delete(collocation_result);
        return -1;
    }

    if (harp_program_remove_operation_at_index(program, 0) != 0)
    {
        harp_collocation_result_delete(collocation_result);
        return -1;
    }

    return 0;
}

/* Execute the next operation in the specified program.
 * Consecutive dimension-filters are treated as a single 'operation' for optimization purposes.
 */
static int execute_next_operation(harp_product *product, harp_program *program)
{
    harp_operation *operation = NULL;

    assert(program->num_operations != 0);

    /* determine type of next operation */
    operation = program->operation[0];
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
            if (execute_filter_operations(product, program) != 0)
            {
                return -1;
            }
            break;
        case harp_operation_collocation_filter:
            if (execute_collocation_filter(product, program) != 0)
            {
                return -1;
            }
            break;
        case harp_operation_derive_variable:
            if (execute_derive_variable(product, program) != 0)
            {
                return -1;
            }
            break;
        case harp_operation_derive_smoothed_column_collocated:
            if (execute_derive_smoothed_column_collocated(product, program) != 0)
            {
                return -1;
            }
            break;
        case harp_operation_exclude_variable:
            if (execute_exclude_variable(product, program) != 0)
            {
                return -1;
            }
            break;
        case harp_operation_flatten:
            if (execute_flatten(product, program) != 0)
            {
                return -1;
            }
            break;
        case harp_operation_keep_variable:
            if (execute_keep_variable(product, program) != 0)
            {
                return -1;
            }
            break;
        case harp_operation_regrid:
            if (execute_regrid(product, program) != 0)
            {
                return -1;
            }
            break;
        case harp_operation_regrid_collocated:
            if (execute_regrid_collocated(product, program) != 0)
            {
                return -1;
            }
            break;
        case harp_operation_smooth_collocated:
            if (execute_smooth_collocated(product, program) != 0)
            {
                return -1;
            }
            break;
    }

    return 0;
}

/** \addtogroup harp_product
 * @{
 */

/**
 * Execute a set of operations (the program) on a HARP product.
 *
 * \param  product Product that the operations should be executed on.
 * \param  program Program to execute
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
int harp_product_execute_program(harp_product *product, harp_program *program)
{
    harp_program *program_copy = NULL;

    if (harp_program_copy(program, &program_copy) != 0)
    {
        return -1;
    }

    /* while the program isn't done */
    while (program_copy->num_operations != 0)
    {
        if (execute_next_operation(product, program_copy) != 0)
        {
            harp_program_delete(program_copy);
            return -1;
        }
        if (harp_product_is_empty(product))
        {
            /* don't perform any of the remaining actions; just return the empty product */
            harp_program_delete(program_copy);
            return 0;
        }
    }

    /* Assert post-condition; we must be done with the program */
    assert(program_copy->num_operations == 0);

    harp_program_delete(program_copy);
    return 0;
}

/**
 * Execute one or more operations on a product.
 *
 * if one of the operations results in an empty product then the function will immediately return with
 * the empty product (and return code 0) and will not execute any of the remaining actions anymore.
 * \param  product Product that the operations should be executed on.
 * \param  operations Operations to execute; should be specified as a semi-colon
 *                 separated string of operations.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_product_execute_operations(harp_product *product, const char *operations)
{
    harp_program *program;

    if (product == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "product is NULL");
        return -1;
    }

    if (operations == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "operations is NULL");
        return -1;
    }

    if (harp_program_from_string(operations, &program) != 0)
    {
        return -1;
    }

    if (harp_product_execute_program(product, program) != 0)
    {
        harp_program_delete(program);
        return -1;
    }

    harp_program_delete(program);

    return 0;
}

/**
 * @}
 */
