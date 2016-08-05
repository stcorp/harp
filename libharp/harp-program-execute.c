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
#include "harp-action.h"
#include "harp-program-execute.h"
#include "harp-predicate.h"
#include "harp-filter.h"
#include "harp-filter-collocation.h"

static int find_variable(const harp_product *product, const char *name, int num_dimensions,
                         const harp_dimension_type *dimension_type, harp_variable **variable)
{
    harp_variable *candidate;

    if (harp_product_get_variable_by_name(product, name, &candidate) != 0)
    {
        return -1;
    }
    if (num_dimensions >= 0)
    {
        if (dimension_type == NULL && candidate->num_dimensions != num_dimensions)
        {
            return -1;
        }
        if (dimension_type != NULL && !harp_variable_has_dimension_types(candidate, num_dimensions, dimension_type))
        {
            return -1;
        }
    }
    if (variable != NULL)
    {
        *variable = candidate;
    }

    return 0;
}

static int evaluate_value_filters_0d(const harp_product *product, harp_program *program, uint8_t *product_mask)
{
    int i;

    i = 0;
    while (i < program->num_actions)
    {
        const harp_action *action;
        const char *variable_name;
        harp_variable *variable;
        harp_predicate *predicate;

        action = program->action[i];
        if (harp_action_get_variable_name(action, &variable_name) != 0)
        {
            /* Action is not a variable filter, skip it. */
            i++;
            continue;
        }

        if (harp_product_get_variable_by_name(product, variable_name, &variable) != 0)
        {
            return -1;
        }

        if (variable->num_dimensions != 0)
        {
            /* Variable is not 0-D, skip. */
            i++;
            continue;
        }

        if (harp_get_filter_predicate_for_action(action, variable->data_type, variable->unit, variable->valid_min,
                                                 variable->valid_max, &predicate) != 0)
        {
            return -1;
        }

        if (harp_predicate_update_mask_all_0d(predicate, variable, product_mask) != 0)
        {
            harp_predicate_delete(predicate);
            return -1;
        }
        else
        {
            harp_predicate_delete(predicate);
        }

        if (harp_program_remove_action_at_index(program, i) != 0)
        {
            return -1;
        }
    }

    return 0;
}

static int evaluate_value_filters_1d(const harp_product *product, harp_program *program,
                                     harp_dimension_mask_set *dimension_mask_set)
{
    int i;

    i = 0;
    while (i < program->num_actions)
    {
        const harp_action *action;
        const char *variable_name;
        harp_variable *variable;
        harp_dimension_type dimension_type;
        harp_predicate *predicate;

        action = program->action[i];
        if (harp_action_get_variable_name(action, &variable_name) != 0)
        {
            /* Action is not a variable filter, skip it. */
            i++;
            continue;
        }

        if (harp_product_get_variable_by_name(product, variable_name, &variable) != 0)
        {
            return -1;
        }

        if (variable->num_dimensions != 1)
        {
            /* Variable is not 1-D, skip. */
            i++;
            continue;
        }

        dimension_type = variable->dimension_type[0];
        if (dimension_type == harp_dimension_independent)
        {
            harp_set_error(HARP_ERROR_ACTION, "variable '%s' has independent outer dimension", variable->name);
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

        if (harp_get_filter_predicate_for_action(action, variable->data_type, variable->unit, variable->valid_min,
                                                 variable->valid_max, &predicate) != 0)
        {
            return -1;
        }

        if (harp_predicate_update_mask_all_1d(predicate, variable, dimension_mask_set[dimension_type]) != 0)
        {
            harp_predicate_delete(predicate);
            return -1;
        }
        else
        {
            harp_predicate_delete(predicate);
        }

        if (harp_program_remove_action_at_index(program, i) != 0)
        {
            return -1;
        }
    }

    return 0;
}

static int evaluate_value_filters_2d(const harp_product *product, harp_program *program,
                                     harp_dimension_mask_set *dimension_mask_set)
{
    int i;

    i = 0;
    while (i < program->num_actions)
    {
        harp_action *action;
        const char *variable_name;
        harp_variable *variable;
        harp_dimension_type dimension_type;
        harp_predicate *predicate;

        action = program->action[i];
        if (harp_action_get_variable_name(action, &variable_name) != 0)
        {
            /* Action is not a variable filter, skip it. */
            i++;
            continue;
        }

        if (harp_product_get_variable_by_name(product, variable_name, &variable) != 0)
        {
            return -1;
        }

        if (variable->num_dimensions != 2)
        {
            /* Variable is not 2-D, skip. */
            i++;
            continue;
        }

        if (variable->dimension_type[0] != harp_dimension_time)
        {
            harp_set_error(HARP_ERROR_ACTION, "outer dimension of variable '%s' is of type '%s'; expected '%s'",
                           variable->name, harp_get_dimension_type_name(variable->dimension_type[0]),
                           harp_get_dimension_type_name(harp_dimension_time));
            return -1;
        }

        dimension_type = variable->dimension_type[1];
        if (dimension_type == harp_dimension_independent)
        {
            harp_set_error(HARP_ERROR_ACTION, "variable '%s' has independent inner dimension", variable->name);
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

        if (harp_get_filter_predicate_for_action(action, variable->data_type, variable->unit, variable->valid_min,
                                                 variable->valid_max, &predicate) != 0)
        {
            return -1;
        }

        if (harp_predicate_update_mask_all_2d(predicate, variable, dimension_mask_set[harp_dimension_time],
                                              dimension_mask_set[dimension_type]) != 0)
        {
            harp_predicate_delete(predicate);
            return -1;
        }
        else
        {
            harp_predicate_delete(predicate);
        }

        if (harp_program_remove_action_at_index(program, i) != 0)
        {
            return -1;
        }
    }

    return 0;
}

static int evaluate_valid_range_filters(const harp_product *product, harp_program *program,
                                        harp_dimension_mask_set *dimension_mask_set)
{
    int i;

    i = 0;
    while (i < program->num_actions)
    {
        const harp_action *action;
        const harp_valid_range_filter_args *args;
        harp_variable *variable;
        harp_predicate *predicate;

        action = program->action[i];
        if (action->type != harp_action_filter_valid_range)
        {
            /* Action is not a valid value filter, skip it. */
            i++;
            continue;
        }

        args = (const harp_valid_range_filter_args *)action->args;
        if (harp_product_get_variable_by_name(product, args->variable_name, &variable) != 0)
        {
            return -1;
        }

        if (variable->num_dimensions < 1)
        {
            harp_set_error(HARP_ERROR_ACTION, "variable '%s' has %d dimensions; expected 1 or more", variable->name,
                           variable->num_dimensions);
            return -1;
        }

        if (variable->dimension_type[0] != harp_dimension_time)
        {
            harp_set_error(HARP_ERROR_ACTION, "outer dimension of variable '%s' is of type '%s'; expected '%s'",
                           variable->name, harp_get_dimension_type_name(variable->dimension_type[0]),
                           harp_get_dimension_type_name(harp_dimension_time));
            return -1;
        }

        if (dimension_mask_set[harp_dimension_time] == NULL)
        {
            long dimension = product->dimension[harp_dimension_time];

            /* Create dimension mask if necessary. */
            if (harp_dimension_mask_new(1, &dimension, &dimension_mask_set[harp_dimension_time]) != 0)
            {
                return -1;
            }
        }

        if (harp_valid_range_filter_predicate_new(variable->data_type, variable->valid_min, variable->valid_max,
                                                  &predicate) != 0)
        {
            return -1;
        }

        if (harp_predicate_update_mask_any(predicate, variable, dimension_mask_set[harp_dimension_time]) != 0)
        {
            harp_predicate_delete(predicate);
            return -1;
        }
        else
        {
            harp_predicate_delete(predicate);
        }

        if (harp_program_remove_action_at_index(program, i) != 0)
        {
            return -1;
        }
    }

    return 0;
}

static int evaluate_point_filters_0d(const harp_product *product, harp_program *program, uint8_t *product_mask)
{
    harp_predicate_set *predicate_set;
    int i;

    /* Create filter predicates for all point filters and collect them in a predicate set. The filter predicates
     * created here will be re-used for all points. Actions for which a predicate has been created are removed from
     * the list of actions to perform.
     */
    if (harp_predicate_set_new(&predicate_set) != 0)
    {
        return -1;
    }

    i = 0;
    while (i < program->num_actions)
    {
        const harp_action *action;
        harp_predicate *predicate;

        /* Create filter predicate and add it to the predicate set. Remove the action from the list of actions to
         * perform.
         */
        action = program->action[i];
        switch (action->type)
        {
            case harp_action_filter_point_distance:
                {
                    const harp_point_distance_filter_args *args;

                    args = (const harp_point_distance_filter_args *)action->args;
                    if (harp_point_distance_filter_predicate_new(args, &predicate) != 0)
                    {
                        harp_predicate_set_delete(predicate_set);
                        return -1;
                    }
                }
                break;
            case harp_action_filter_area_mask_covers_point:
                {
                    const harp_area_mask_covers_point_filter_args *args;

                    args = (const harp_area_mask_covers_point_filter_args *)action->args;
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

        if (harp_program_remove_action_at_index(program, i) != 0)
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
            return -1;
        }

        if (harp_product_get_variable_by_name(product, "latitude", &latitude) != 0)
        {
            harp_predicate_set_delete(predicate_set);
            return -1;
        }

        if (harp_point_predicate_update_mask_all_0d(predicate_set->num_predicates, predicate_set->predicate, longitude,
                                                    latitude, product_mask) != 0)
        {
            harp_predicate_set_delete(predicate_set);
            return -1;
        }
    }

    harp_predicate_set_delete(predicate_set);

    return 0;
}

static int evaluate_point_filters_1d(const harp_product *product, harp_program *program,
                                     harp_dimension_mask_set *dimension_mask_set)
{
    harp_predicate_set *predicate_set = NULL;
    int i;

    /* Create filter predicates for all point filters and collect them in a predicate set. The filter predicates
     * created here will be re-used for all points. Actions for which a predicate has been created are removed from
     * the list of actions to perform.
     */
    if (harp_predicate_set_new(&predicate_set) != 0)
    {
        return -1;
    }

    i = 0;
    while (i < program->num_actions)
    {
        const harp_action *action;
        harp_predicate *predicate;

        /* Create filter predicate and add it to the predicate set. Remove the action from the list of actions to
         * perform.
         */
        action = program->action[i];
        switch (action->type)
        {
            case harp_action_filter_point_distance:
                {
                    const harp_point_distance_filter_args *args;

                    args = (const harp_point_distance_filter_args *)action->args;
                    if (harp_point_distance_filter_predicate_new(args, &predicate) != 0)
                    {
                        harp_predicate_set_delete(predicate_set);
                        return -1;
                    }
                }
                break;
            case harp_action_filter_area_mask_covers_point:
                {
                    const harp_area_mask_covers_point_filter_args *args;

                    args = (const harp_area_mask_covers_point_filter_args *)action->args;
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

        if (harp_program_remove_action_at_index(program, i) != 0)
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
            return -1;
        }

        if (harp_product_get_variable_by_name(product, "latitude", &latitude) != 0)
        {
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

        if (harp_point_predicate_update_mask_all_1d(predicate_set->num_predicates, predicate_set->predicate,
                                                    longitude, latitude, dimension_mask_set[harp_dimension_time]) != 0)
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
    int i;

    /* Create filter predicates for all area filters and collect them in a predicate set. The filter predicates created
     * here will be re-used for all points. Actions for which a predicate has been created are removed from the list of
     * actions to perform.
     */
    if (harp_predicate_set_new(&predicate_set) != 0)
    {
        return -1;
    }

    i = 0;
    while (i < program->num_actions)
    {
        const harp_action *action;
        harp_predicate *predicate;

        /* Create filter predicate and add it to the predicate set. Remove the action from the list of actions to
         * perform.
         */
        action = program->action[i];
        switch (action->type)
        {
            case harp_action_filter_area_mask_covers_area:
                {
                    const harp_area_mask_covers_area_filter_args *args;

                    args = (const harp_area_mask_covers_area_filter_args *)action->args;
                    if (harp_area_mask_covers_area_filter_predicate_new(args, &predicate) != 0)
                    {
                        harp_predicate_set_delete(predicate_set);
                        return -1;
                    }
                }
                break;
            case harp_action_filter_area_mask_intersects_area:
                {
                    const harp_area_mask_intersects_area_filter_args *args;

                    args = (const harp_area_mask_intersects_area_filter_args *)action->args;
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

        if (harp_program_remove_action_at_index(program, i) != 0)
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
            return -1;
        }

        if (harp_product_get_variable_by_name(product, "latitude_bounds", &latitude_bounds) != 0)
        {
            harp_predicate_set_delete(predicate_set);
            return -1;
        }

        if (harp_area_predicate_update_mask_all_0d(predicate_set->num_predicates, predicate_set->predicate,
                                                   longitude_bounds, latitude_bounds, product_mask) != 0)
        {
            harp_predicate_set_delete(predicate_set);
            return -1;
        }
    }

    harp_predicate_set_delete(predicate_set);

    return 0;
}

static int evaluate_area_filters_1d(const harp_product *product, harp_program *program,
                                    harp_dimension_mask_set *dimension_mask_set)
{
    harp_predicate_set *predicate_set;
    int i;

    /* Create filter predicates for all area filters and collect them in a predicate set. The filter predicates created
     * here will be re-used for all points. Actions for which a predicate has been created are removed from the list of
     * actions to perform.
     */
    if (harp_predicate_set_new(&predicate_set) != 0)
    {
        return -1;
    }

    i = 0;
    while (i < program->num_actions)
    {
        const harp_action *action;
        harp_predicate *predicate;

        /* Create filter predicate and add it to the predicate set. Remove the action from the list of actions to
         * perform.
         */
        action = program->action[i];
        switch (action->type)
        {
            case harp_action_filter_area_mask_covers_area:
                {
                    const harp_area_mask_covers_area_filter_args *args;

                    args = (const harp_area_mask_covers_area_filter_args *)action->args;
                    if (harp_area_mask_covers_area_filter_predicate_new(args, &predicate) != 0)
                    {
                        harp_predicate_set_delete(predicate_set);
                        return -1;
                    }
                }
                break;
            case harp_action_filter_area_mask_intersects_area:
                {
                    const harp_area_mask_intersects_area_filter_args *args;

                    args = (const harp_area_mask_intersects_area_filter_args *)action->args;
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

        if (harp_program_remove_action_at_index(program, i) != 0)
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
            return -1;
        }

        if (harp_product_get_variable_by_name(product, "latitude_bounds", &latitude_bounds) != 0)
        {
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

        if (harp_area_predicate_update_mask_all_1d(predicate_set->num_predicates, predicate_set->predicate,
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
static int execute_variable_exclude_filter_action(harp_product *product, harp_program *program)
{
    const harp_variable_exclusion_args *ex_args;
    int variable_id;
    int j;
    harp_action *action;

    assert(program->num_actions != 0);
    action = program->action[0];
    if (action->type != harp_action_exclude_variable)
    {
        return 0;
    }

    /* unmark the variables to exclude */
    ex_args = (const harp_variable_exclusion_args *)action->args;
    for (j = 0; j < ex_args->num_variables; j++)
    {
        if (harp_product_get_variable_id_by_name(product, ex_args->variable_name[j], &variable_id) != 0)
        {
            /* already removed, not an error */
            continue;
        }

        /* execute the action: remove the variable */
        if (harp_product_remove_variable(product, product->variable[variable_id]) != 0)
        {
            return -1;
        }
    }

    /* remove the action that we executed */
    if (harp_program_remove_action_at_index(program, 0) != 0)
    {
        return -1;
    }

    return 0;
}

/* execute the variable include filter from the head of program */
static int execute_variable_include_filter_action(harp_product *product, harp_program *program)
{
    uint8_t *include_variable_mask;
    harp_action *action;
    const harp_variable_inclusion_args *in_args;
    int variable_id;
    int j;

    assert(program->num_actions != 0);
    action = program->action[0];
    if (action->type != harp_action_include_variable)
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
    in_args = (const harp_variable_inclusion_args *)action->args;
    for (j = 0; j < in_args->num_variables; j++)
    {
        if (harp_product_get_variable_id_by_name(product, in_args->variable_name[j], &variable_id) != 0)
        {
            harp_set_error(HARP_ERROR_ACTION, "cannot keep non-existant variable '%s'", in_args->variable_name[j]);
            goto error;
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
                goto error;
            }
        }
    }

    /* remove the action that we execute */
    if (harp_program_remove_action_at_index(program, 0) != 0)
    {
        goto error;
    }

    free(include_variable_mask);
    return 0;

  error:
    free(include_variable_mask);
    return -1;
}

/* run a collocation filter action at the head of program */
static int execute_collocation_filter(harp_product *product, harp_program *program)
{
    harp_action *action;
    const harp_collocation_filter_args *args;
    harp_collocation_mask *collocation_mask;

    assert(program->num_actions != 0);
    action = program->action[0];
    if (action->type != harp_action_filter_collocation)
    {
        /* Action is not a collocation filter action, skip it. */
        return 0;
    }

    if (product->source_product == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "product attribute 'source_product' is NULL");
        return -1;
    }

    args = (const harp_collocation_filter_args *)action->args;
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

    if (harp_program_remove_action_at_index(program, 0) != 0)
    {
        return -1;
    }

    return 0;
}

static int action_is_dimension_filter(const harp_action *action)
{
    switch (action->type)
    {
        case harp_action_filter_comparison:
        case harp_action_filter_string_comparison:
        case harp_action_filter_bit_mask:
        case harp_action_filter_membership:
        case harp_action_filter_string_membership:
        case harp_action_filter_valid_range:
        case harp_action_filter_longitude_range:
        case harp_action_filter_point_distance:
        case harp_action_filter_area_mask_covers_point:
        case harp_action_filter_area_mask_covers_area:
        case harp_action_filter_area_mask_intersects_area:
            return 1;
        default:
            return 0;
    }
}

/* execute the prefix of the program of 0..n dimension filter actions */
static int execute_filter_actions(harp_product *product, harp_program *program)
{
    uint8_t product_mask = 1;
    harp_dimension_type dimension_type[1] = { harp_dimension_independent };
    harp_dimension_mask_set *dimension_mask_set;
    harp_program *dimension_filters = NULL;
    int i;

    /* greedily exit when program is trivial */
    if (program->num_actions == 0)
    {
        return 0;
    }

    if (harp_program_new(&dimension_filters) != 0)
    {
        return -1;
    }

    /* pop the prefix of dimension-filters that we'll process into a subprogram */
    for (i = 0; i < program->num_actions; i++)
    {
        harp_action *copy = NULL;

        if (!action_is_dimension_filter(program->action[0]))
        {
            break;
        }
        if (harp_action_copy(program->action[0], &copy) != 0)
        {
            return -1;
        }
        if (harp_program_add_action(dimension_filters, copy) != 0)
        {
            return -1;
        }
        if (harp_program_remove_action_at_index(program, 0) != 0)
        {
            return -1;
        }
    }

    /* Now run these dimension filters in optimized order.
     * Each of the execution function below will go through the dimension_filters and execute
     * actions greedily if possible.
     */

    /* First filter pass (0-D variables). */
    if (evaluate_value_filters_0d(product, dimension_filters, &product_mask) != 0)
    {
        return -1;
    }
    /* TODO Shouldn't point filters just raise errors when longitude/latitude is missing? */
    if (find_variable(product, "longitude", 0, NULL, NULL) == 0
        && find_variable(product, "latitude", 0, NULL, NULL) == 0)
    {
        if (evaluate_point_filters_0d(product, dimension_filters, &product_mask) != 0)
        {
            return -1;
        }
    }
    if (find_variable(product, "longitude_bounds", 1, dimension_type, NULL) == 0
        && find_variable(product, "latitude_bounds", 1, dimension_type, NULL) == 0)
    {
        if (evaluate_area_filters_0d(product, dimension_filters, &product_mask) != 0)
        {
            return -1;
        }
    }

    if (product_mask == 0)
    {
        harp_product_remove_all_variables(product);
        return 0;
    }

    /* Second filter pass (1-D variables). */
    if (harp_dimension_mask_set_new(&dimension_mask_set) != 0)
    {
        return -1;
    }
    if (evaluate_value_filters_1d(product, dimension_filters, dimension_mask_set) != 0)
    {
        harp_dimension_mask_set_delete(dimension_mask_set);
        return -1;
    }
    if (evaluate_point_filters_1d(product, dimension_filters, dimension_mask_set) != 0)
    {
        harp_dimension_mask_set_delete(dimension_mask_set);
        return -1;
    }
    if (evaluate_area_filters_1d(product, dimension_filters, dimension_mask_set) != 0)
    {
        harp_dimension_mask_set_delete(dimension_mask_set);
        return -1;
    }

    /* Apply the dimension masks computed so far, to speed up subsequent filtering steps. */
    if (harp_product_filter(product, dimension_mask_set) != 0)
    {
        harp_dimension_mask_set_delete(dimension_mask_set);
        return -1;
    }
    harp_dimension_mask_set_delete(dimension_mask_set);

    if (harp_product_is_empty(product))
    {
        return 0;
    }

    /* Third filter pass (2-D variables). */
    if (harp_dimension_mask_set_new(&dimension_mask_set) != 0)
    {
        return -1;
    }
    if (evaluate_value_filters_2d(product, dimension_filters, dimension_mask_set) != 0)
    {
        harp_dimension_mask_set_delete(dimension_mask_set);
        return -1;
    }
    if (harp_dimension_mask_set_simplify(dimension_mask_set) != 0)
    {
        harp_dimension_mask_set_delete(dimension_mask_set);
        return -1;
    }

    /* Apply the dimension masks computed so far.
     * This is required because the valid range filter implementation does
     * not support secondary dimension masks.
     */
    if (harp_product_filter(product, dimension_mask_set) != 0)
    {
        harp_dimension_mask_set_delete(dimension_mask_set);
        return -1;
    }
    harp_dimension_mask_set_delete(dimension_mask_set);

    if (harp_product_is_empty(product))
    {
        return 0;
    }

    /* Valid range filters. */
    if (harp_dimension_mask_set_new(&dimension_mask_set) != 0)
    {
        return -1;
    }
    if (evaluate_valid_range_filters(product, dimension_filters, dimension_mask_set) != 0)
    {
        harp_dimension_mask_set_delete(dimension_mask_set);
        return -1;
    }

    /* Apply the dimension masks computed so far. */
    if (harp_product_filter(product, dimension_mask_set) != 0)
    {
        harp_dimension_mask_set_delete(dimension_mask_set);
        return -1;
    }
    harp_dimension_mask_set_delete(dimension_mask_set);

    if (harp_product_is_empty(product))
    {
        return 0;
    }

    /* Verify that all dimension filters have been executed */
    if (dimension_filters->num_actions != 0)
    {
        harp_set_error(HARP_ERROR_ACTION, "Could not execute all filter actions.");
        return -1;
    }

    return 0;
}

static int execute_derivation(harp_product *product, harp_program *program)
{
    harp_variable_derivation_args *args = NULL;
    harp_action *action = program->action[0];

    if (action->type != harp_action_derive_variable)
    {
        return 0;
    }

    /* get action arguments */
    args = (harp_variable_derivation_args *)action->args;

    /* execute the action */
    if (harp_product_add_derived_variable(product, args->variable_name, args->unit, args->num_dimensions,
                                          args->dimension_type) != 0)
    {
        return -1;
    }

    /* remove the action from the queue */
    if (harp_program_remove_action_at_index(program, 0) != 0)
    {
        return -1;
    }

    return 0;
}

static int execute_next_action(harp_product *product, harp_program *program)
{
    harp_action *action = NULL;

    if (program->num_actions == 0)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "Can't execute next action of empty program.");
        return -1;
    }

    /* determine type of next action */
    action = program->action[0];
    switch (action->type)
    {
        case harp_action_exclude_variable:
            if (execute_variable_exclude_filter_action(product, program))
            {
                return -1;
            }

            break;
        case harp_action_include_variable:
            if (execute_variable_include_filter_action(product, program))
            {
                return -1;
            }

            break;

        case harp_action_derive_variable:
            if (execute_derivation(product, program) != 0)
            {
                return -1;
            }

            break;

        case harp_action_filter_collocation:
            if (execute_collocation_filter(product, program) != 0)
            {
                return -1;
            }

            break;

        default:
            /* all that's left should be dimension filters */
            if (action_is_dimension_filter(action))
            {
                if (execute_filter_actions(product, program) != 0)
                {
                    return -1;
                }
            }
            else
            {
                /* Don't know how to run this action type */
                assert(0);
            }

            break;
    }

    return 0;
}

/** \addtogroup harp_product
 * @{
 */

/**
 * Execute a set of actions (the program) on a HARP product.
 *
 * \param  product Product that the actions should be executed on.
 * \param  program Program to execute
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
int harp_product_execute_program(harp_product *product, harp_program *program)
{
    harp_program *program_copy = NULL;

    if (harp_program_verify(program) != 0)
    {
        goto error;
    }

    if (harp_program_copy(program, &program_copy) != 0)
    {
        goto error;
    }

    /* while the program isn't done */
    while (program_copy->num_actions != 0)
    {
        if (execute_next_action(product, program_copy) != 0)
        {
            goto error;
        }
    }

    /* Assert post-condition; we must be done with the program */
    assert(program_copy->num_actions == 0);

    return 0;

  error:
    harp_program_delete(program_copy);

    return -1;
}

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
    harp_program *program;

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

    if (harp_program_from_string(actions, &program) != 0)
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
 * }@
 */
