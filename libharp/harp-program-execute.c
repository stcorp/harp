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
        else
        {
            harp_predicate_delete(predicate);
        }

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
            case harp_operation_filter_point_distance:
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
            case harp_operation_filter_area_mask_covers_point:
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
        harp_variable *longitude;
        harp_variable *latitude;

        if (harp_product_get_variable_by_name(product, "longitude", &longitude) != 0)
        {
            harp_predicate_set_delete(predicate_set);
            harp_set_error(HARP_ERROR_OPERATION, OPERATION_FILTER_POINT_MISSING_LON);
            return -1;
        }

        if (harp_product_get_variable_by_name(product, "latitude", &latitude) != 0)
        {
            harp_predicate_set_delete(predicate_set);
            harp_set_error(HARP_ERROR_OPERATION, OPERATION_FILTER_POINT_MISSING_LAT);
            return -1;
        }

        /* we were promised 0D filters */
        assert(longitude->num_dimensions == 0 && latitude->num_dimensions == 0);

        if (harp_point_predicate_update_mask_0d(predicate_set->num_predicates, predicate_set->predicate, longitude,
                                                latitude, product_mask) != 0)
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
            case harp_operation_filter_point_distance:
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
            case harp_operation_filter_area_mask_covers_point:
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
        harp_variable *longitude;
        harp_variable *latitude;

        if (harp_product_get_variable_by_name(product, "longitude", &longitude) != 0)
        {
            harp_predicate_set_delete(predicate_set);
            harp_set_error(HARP_ERROR_OPERATION, OPERATION_FILTER_POINT_MISSING_LON);
            return -1;
        }

        if (harp_product_get_variable_by_name(product, "latitude", &latitude) != 0)
        {
            harp_predicate_set_delete(predicate_set);
            harp_set_error(HARP_ERROR_OPERATION, OPERATION_FILTER_POINT_MISSING_LAT);
            return -1;
        }

        /* We were promised 1D filters */
        assert(longitude->num_dimensions == 1 && latitude->num_dimensions == 1);

        if (dimension_mask_set[harp_dimension_time] == NULL)
        {
            long dimension = product->dimension[harp_dimension_time];

            if (harp_dimension_mask_new(1, &dimension, &dimension_mask_set[harp_dimension_time]) != 0)
            {
                harp_predicate_set_delete(predicate_set);
                return -1;
            }
        }

        if (harp_point_predicate_update_mask_1d(predicate_set->num_predicates, predicate_set->predicate, longitude,
                                                latitude, dimension_mask_set[harp_dimension_time]) != 0)
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
            case harp_operation_filter_area_mask_covers_area:
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
            case harp_operation_filter_area_mask_intersects_area:
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
        harp_variable *longitude_bounds;
        harp_variable *latitude_bounds;

        if (harp_product_get_variable_by_name(product, "longitude_bounds", &longitude_bounds) != 0)
        {
            harp_predicate_set_delete(predicate_set);
            harp_set_error(HARP_ERROR_OPERATION, OPERATION_FILTER_AREA_MISSING_LON_BOUNDS);
            return -1;
        }

        if (harp_product_get_variable_by_name(product, "latitude_bounds", &latitude_bounds) != 0)
        {
            harp_predicate_set_delete(predicate_set);
            harp_set_error(HARP_ERROR_OPERATION, OPERATION_FILTER_AREA_MISSING_LAT_BOUNDS);
            return -1;
        }

        assert(longitude_bounds->num_dimensions == 1 && latitude_bounds->num_dimensions == 1);
        if (!harp_variable_has_dimension_types(longitude_bounds, 1, dimension_type)
            || !harp_variable_has_dimension_types(longitude_bounds, 1, dimension_type))
        {
            harp_set_error(HARP_ERROR_OPERATION, OPERATION_FILTER_AREA_WRONG_DIMENSION_FORMAT, "{independent}");
            harp_predicate_set_delete(predicate_set);
            return -1;
        }

        if (harp_area_predicate_update_mask_0d(predicate_set->num_predicates, predicate_set->predicate,
                                               longitude_bounds, latitude_bounds, product_mask) != 0)
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
            case harp_operation_filter_area_mask_covers_area:
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
            case harp_operation_filter_area_mask_intersects_area:
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
        harp_variable *longitude_bounds;
        harp_variable *latitude_bounds;

        if (harp_product_get_variable_by_name(product, "longitude_bounds", &longitude_bounds) != 0)
        {
            harp_predicate_set_delete(predicate_set);
            harp_set_error(HARP_ERROR_OPERATION, OPERATION_FILTER_AREA_MISSING_LON_BOUNDS);
            return -1;
        }

        if (harp_product_get_variable_by_name(product, "latitude_bounds", &latitude_bounds) != 0)
        {
            harp_predicate_set_delete(predicate_set);
            harp_set_error(HARP_ERROR_OPERATION, OPERATION_FILTER_AREA_MISSING_LAT_BOUNDS);
            return -1;
        }

        /* We were promised 1D area filters, meaning lat/lon bounds are 2D */
        assert(longitude_bounds->num_dimensions == 2);
        assert(latitude_bounds->num_dimensions == 2);
        if (!harp_variable_has_dimension_types(longitude_bounds, 2, dimension_type) ||
            !harp_variable_has_dimension_types(latitude_bounds, 2, dimension_type))
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
                                               longitude_bounds, latitude_bounds,
                                               dimension_mask_set[harp_dimension_time]) != 0)
        {
            harp_predicate_set_delete(predicate_set);
            return -1;
        }
    }

    harp_predicate_set_delete(predicate_set);

    return 0;
}

/* execute the variable exclude filter from the head of program */
static int execute_variable_exclude_filter_operation(harp_product *product, harp_program *program)
{
    const harp_variable_exclusion_args *ex_args;
    int variable_id;
    int j;
    harp_operation *operation;

    assert(program->num_operations != 0);
    operation = program->operation[0];
    if (operation->type != harp_operation_exclude_variable)
    {
        return 0;
    }

    /* unmark the variables to exclude */
    ex_args = (const harp_variable_exclusion_args *)operation->args;
    for (j = 0; j < ex_args->num_variables; j++)
    {
        if (harp_product_get_variable_id_by_name(product, ex_args->variable_name[j], &variable_id) != 0)
        {
            /* already removed, not an error */
            continue;
        }

        /* execute the operation: remove the variable */
        if (harp_product_remove_variable(product, product->variable[variable_id]) != 0)
        {
            return -1;
        }
    }

    /* remove the operation that we executed */
    if (harp_program_remove_operation_at_index(program, 0) != 0)
    {
        return -1;
    }

    return 0;
}

/* execute the variable include filter from the head of program */
static int execute_variable_keep_filter_operation(harp_product *product, harp_program *program)
{
    harp_operation *operation;
    const harp_variable_inclusion_args *in_args;
    int variable_id;
    int j;

    /* owned mem */
    uint8_t *include_variable_mask;

    assert(program->num_operations != 0);
    operation = program->operation[0];
    if (operation->type != harp_operation_keep_variable)
    {
        return 0;
    }

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
    in_args = (const harp_variable_inclusion_args *)operation->args;
    for (j = 0; j < in_args->num_variables; j++)
    {
        if (harp_product_get_variable_id_by_name(product, in_args->variable_name[j], &variable_id) != 0)
        {
            harp_set_error(HARP_ERROR_OPERATION, OPERATION_KEEP_NON_EXISTANT_VARIABLE_FORMAT,
                           in_args->variable_name[j]);
            free(include_variable_mask);
            return -1;
        }

        include_variable_mask[variable_id] = 1;
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

/* run a collocation filter operation at the head of program */
static int execute_collocation_filter(harp_product *product, harp_program *program)
{
    harp_operation *operation;
    const harp_collocation_filter_args *args;
    harp_collocation_mask *collocation_mask;

    assert(program->num_operations != 0);
    operation = program->operation[0];
    if (operation->type != harp_operation_filter_collocation)
    {
        /* Operation is not a collocation filter operation, skip it. */
        return 0;
    }

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
        /* Neither the "collocation_index" nor the "index" variable exists in the product,
         * which means collocation
         * filters cannot be applied. */
        harp_set_error(HARP_ERROR_OPERATION, OPERATION_FILTER_COLLOCATION_MISSING_INDEX);
        return -1;
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

/* run a collocation filter operation at the head of program */
static int execute_regrid(harp_product *product, harp_program *program)
{
    harp_operation *operation;
    const harp_regrid_args *args;
    harp_variable *target_grid = NULL;

    assert(program->num_operations != 0);
    operation = program->operation[0];
    if (operation->type != harp_operation_regrid)
    {
        /* Operation is not a regrid operation, skip it. */
        return 0;
    }

    args = (const harp_regrid_args *)operation->args;

    if (harp_profile_import_grid(args->grid_filename, &target_grid) != 0)
    {
        return -1;
    }

    if (harp_product_regrid_vertical_with_axis_variable(product, target_grid) != 0)
    {
        harp_variable_delete(target_grid);
        return -1;
    }

    harp_variable_delete(target_grid);

    if (harp_program_remove_operation_at_index(program, 0) != 0)
    {
        return -1;
    }

    return 0;
}

/* Compute 'dimensionality' for filter operations; sets num_dimensions to either 0, 1 or 2.
 */
static int get_operation_dimensionality(harp_product *product, harp_operation *operation, long *num_dimensions)
{
    const char *variable_name = NULL;

    /* collocation filters */
    if (operation->type == harp_operation_filter_collocation)
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
    else if (operation->type == harp_operation_filter_point_distance ||
             operation->type == harp_operation_filter_area_mask_covers_point)
    {
        harp_variable *longitude_def, *latitude_def = NULL;

        if (harp_product_get_variable_by_name(product, "longitude", &longitude_def) != 0)
        {
            harp_set_error(HARP_ERROR_OPERATION, OPERATION_FILTER_POINT_MISSING_LON);
            return -1;
        }
        if (harp_product_get_variable_by_name(product, "latitude", &latitude_def) != 0)
        {
            harp_set_error(HARP_ERROR_OPERATION, OPERATION_FILTER_POINT_MISSING_LAT);
            return -1;
        }
        /* point filters must be 0D or 1D */
        if (longitude_def->num_dimensions > 1 || latitude_def->num_dimensions > 1)
        {
            harp_set_error(HARP_ERROR_OPERATION, OPERATION_FILTER_POINT_WRONG_DIMENSION_FORMAT, "{time}");
            return -1;
        }

        /* dimensionality is the max of the lat/lon variables */
        *num_dimensions =
            longitude_def->num_dimensions >
            latitude_def->num_dimensions ? longitude_def->num_dimensions : latitude_def->num_dimensions;
    }
    else if (operation->type == harp_operation_filter_area_mask_covers_area ||
             operation->type == harp_operation_filter_area_mask_intersects_area)
    {
        harp_variable *longitude_bounds;
        harp_variable *latitude_bounds;

        if (harp_product_get_variable_by_name(product, "longitude_bounds", &longitude_bounds) != 0)
        {
            harp_set_error(HARP_ERROR_OPERATION, OPERATION_FILTER_AREA_MISSING_LON_BOUNDS);
            return -1;
        }
        if (harp_product_get_variable_by_name(product, "latitude_bounds", &latitude_bounds) != 0)
        {
            harp_set_error(HARP_ERROR_OPERATION, OPERATION_FILTER_AREA_MISSING_LAT_BOUNDS);
            return -1;
        }
        /* area filters must be 0D or 1D, which means that the bounds are 1D or 2D resp. */
        if (longitude_bounds->num_dimensions > 2 || latitude_bounds->num_dimensions > 2
            || longitude_bounds->num_dimensions < 1 || latitude_bounds->num_dimensions < 1)
        {
            harp_set_error(HARP_ERROR_OPERATION, OPERATION_FILTER_POINT_WRONG_DIMENSION_FORMAT, "{time}");
            return -1;
        }

        *num_dimensions =
            longitude_bounds->num_dimensions >
            latitude_bounds->num_dimensions ? longitude_bounds->num_dimensions : latitude_bounds->num_dimensions;
    }
    else
    {
        harp_set_error(HARP_ERROR_OPERATION, "Encountered unsupported filter during ingestion.");
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

    /* Pop the prefix of dimension-filters that we'll process into subprograms
     * concerned with 0D, 1D and 2D filter operations respectively.
     */
    for (i = program->num_operations - 1; i >= 0; i--)
    {
        harp_operation *operation = NULL;
        long dim = -1;

        if (!harp_operation_is_dimension_filter(program->operation[0]))
        {
            /* done with this phase */
            break;
        }
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
        if (harp_program_remove_operation_at_index(program, i) != 0)
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

    /* post condition */
    if (program->num_operations != 0)
    {
        harp_set_error(HARP_ERROR_OPERATION, "Could not execute all filter operations.");
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

static int execute_derivation(harp_product *product, harp_program *program)
{
    harp_variable_derivation_args *args = NULL;
    harp_operation *operation = program->operation[0];

    if (operation->type != harp_operation_derive_variable)
    {
        return 0;
    }

    /* get operation arguments */
    args = (harp_variable_derivation_args *)operation->args;

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
        case harp_operation_exclude_variable:
            if (execute_variable_exclude_filter_operation(product, program) != 0)
            {
                return -1;
            }
            break;
        case harp_operation_keep_variable:
            if (execute_variable_keep_filter_operation(product, program) != 0)
            {
                return -1;
            }
            break;
        case harp_operation_derive_variable:
            if (execute_derivation(product, program) != 0)
            {
                return -1;
            }
            break;
        case harp_operation_filter_collocation:
            if (execute_collocation_filter(product, program) != 0)
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
        default:
            /* all that's left should be dimension filters */
            if (!harp_operation_is_dimension_filter(operation))
            {
                /* Don't know how to run this operation type */
                assert(0);
                exit(1);
            }
            if (execute_filter_operations(product, program) != 0)
            {
                return -1;
            }
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
    }

    /* Assert post-condition; we must be done with the program */
    assert(program_copy->num_operations == 0);

    harp_program_delete(program_copy);
    return 0;
}

/**
 * Execute one or more operations on a product.
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
