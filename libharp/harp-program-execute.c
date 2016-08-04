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

static int evaluate_value_filters_0d(const lazy_harp_product *product, harp_program *program,
                                     harp_product_mask *mask)
{
    int i;

    i = 0;
    while (i < program->num_actions)
    {
        const harp_action *action;
        const char *variable_name;
        lazy_harp_variable *variable;
        harp_predicate_set *predicate_set;
        int j;

        action = program->action[i];
        if (harp_action_get_variable_name(action, &variable_name) != 0)
        {
            /* Action is not a variable filter, skip it. */
            i++;
            continue;
        }

        if (product->get_variable_by_name(product, variable_name, &variable) != 0)
        {
            return -1;
        }

        if (variable->get_num_dimensions() != 0)
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

/** Given a program *consisting only of filter actions*, build a product_mask that,
 * when applied to the product that is associated with product_info, conforms to the application
 * of the filter actions.
 */
int harp_program_make_mask(harp_product_lazy_info *lazy_product_info, harp_program *filters,
                           harp_product_mask *mask)
{
    return -1;
}

static int execute_non_filter_action(harp_product *product, const harp_action *action)
{
    /* assert the pre-condition */
    assert(!harp_action_is_filter(action));

    switch (action->type)
    {
    case harp_action_derive_variable:
        /* TODO */
        break;
    default:
        /* assert post condition: the action has been performed */
        assert(0);
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
int harp_product_execute_program(harp_product *product, harp_program *program)
{
    long action_id = 0;

    /* while the program isn't done */
    while (action_id < program->num_actions)
    {

        harp_program *filters = NULL;
        harp_product_mask *mask = NULL;

        if (harp_program_new(&filters) != 0)
        {
            goto error;
        }

        /* collect the filter actions prefix for the next masking phase */
        while (harp_action_is_filter(program->action[action_id]))
        {
            if (harp_program_add_action(program, program->action[action_id]) != 0)
            {
                harp_program_delete(filters);
                goto error;
            }

            action_id++;
        }

        /* build the product mask */
        if (harp_program_make_mask(NULL, filters, &mask))
        {
            harp_program_delete(filters);
            goto error;
        }

        /* apply the mask */
        if (harp_product_apply_mask(product, mask))
        {
            harp_program_delete(filters);
            harp_product_mask_delete(mask);
            goto error;
        }

        /* run non-filter actions */
        while (!harp_action_is_filter(program->action[action_id]))
        {
            if (execute_non_filter_action(product, program->action[action_id]) != 0)
            {
                harp_program_delete(filters);
                harp_product_mask_delete(mask);
                goto error;
            }

            action_id++;
        }
    }

    /* we must be done with the program */
    assert(action_id == program->num_actions - 1);

error:
    return -1;
}

/**
 * }@
 */
