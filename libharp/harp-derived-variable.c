/*
 * Copyright (C) 2015-2025 S[&]T, The Netherlands.
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

#include "hashtable.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct conversion_info_struct
{
    const harp_product *product;
    const harp_variable_conversion *conversion;
    char *dimsvar_name;
    const char *variable_name;
    int num_dimensions;
    harp_dimension_type dimension_type[HARP_MAX_NUM_DIMS];
    uint8_t *skip;      /* 1: variable cannot be created; 2: variable cannot be used because of cyclic dependency */
    int depth;
    int max_depth;
    harp_variable *variable;
} conversion_info;

static int find_and_execute_conversion(conversion_info *info);

static void set_variable_not_found_error(conversion_info *info)
{
    int i;

    harp_set_error(HARP_ERROR_VARIABLE_NOT_FOUND, "could not derive variable '%s {", info->variable_name);
    for (i = 0; i < info->num_dimensions; i++)
    {
        if (i > 0)
        {
            harp_add_error_message(",");
        }
        harp_add_error_message(harp_get_dimension_type_name(info->dimension_type[i]));
    }
    harp_add_error_message("}'");
}

static int has_dimension_types(const harp_variable *variable, int num_dimensions,
                               const harp_dimension_type *dimension_type, long independent_dimension_length)
{
    int i;

    if (variable->num_dimensions != num_dimensions)
    {
        return 0;
    }

    for (i = 0; i < num_dimensions; i++)
    {
        if (variable->dimension_type[i] != dimension_type[i])
        {
            return 0;
        }
        if (dimension_type[i] == harp_dimension_independent && independent_dimension_length >= 0 &&
            variable->dimension[i] != independent_dimension_length)
        {
            return 0;
        }
    }

    return 1;
}

static char *get_dimsvar_name(const char *variable_name, int num_dimensions, const harp_dimension_type *dimension_type)
{
    char *dimsvar_name;
    int i;

    assert(num_dimensions >= 0 && num_dimensions <= HARP_MAX_NUM_DIMS);

    /* see harp-internal.h for format definition of dimsvar_name */

    dimsvar_name = malloc(HARP_MAX_NUM_DIMS + strlen(variable_name) + 1);
    if (dimsvar_name == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory(could not allocate % lu bytes) (%s:%u)",
                       HARP_MAX_NUM_DIMS + strlen(variable_name) + 1, __FILE__, __LINE__);
        return NULL;
    }

    for (i = 0; i < num_dimensions; i++)
    {
        switch (dimension_type[i])
        {
            case harp_dimension_independent:
                dimsvar_name[i] = 'I';
                break;
            case harp_dimension_time:
                dimsvar_name[i] = 'T';
                break;
            case harp_dimension_latitude:
                dimsvar_name[i] = 'A';
                break;
            case harp_dimension_longitude:
                dimsvar_name[i] = 'O';
                break;
            case harp_dimension_vertical:
                dimsvar_name[i] = 'V';
                break;
            case harp_dimension_spectral:
                dimsvar_name[i] = 'S';
                break;
            default:
                assert(0);
                exit(1);
        }
    }
    for (i = num_dimensions; i < HARP_MAX_NUM_DIMS; i++)
    {
        dimsvar_name[i] = ' ';
    }
    strcpy(&dimsvar_name[HARP_MAX_NUM_DIMS], variable_name);

    return dimsvar_name;
}

static int conversion_info_init(conversion_info *info, const harp_product *product)
{
    info->product = product;
    info->conversion = NULL;
    info->dimsvar_name = NULL;
    info->variable_name = NULL;
    info->num_dimensions = 0;
    info->skip = NULL;
    info->depth = 0;
    info->max_depth = 8;
    info->variable = NULL;

    info->skip = malloc(harp_derived_variable_conversions->num_variables);
    if (info->skip == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate % lu bytes) (%s:%u)",
                       (long)harp_derived_variable_conversions->num_variables, __FILE__, __LINE__);
        return -1;
    }
    memset(info->skip, 0, harp_derived_variable_conversions->num_variables);

    return 0;
}

static int conversion_info_set_variable(conversion_info *info, const char *variable_name, int num_dimensions,
                                        const harp_dimension_type *dimension_type)
{
    int i;

    if (info->dimsvar_name != NULL)
    {
        free(info->dimsvar_name);
    }
    info->variable_name = NULL;

    info->num_dimensions = num_dimensions;
    for (i = 0; i < num_dimensions; i++)
    {
        info->dimension_type[i] = dimension_type[i];
    }

    info->dimsvar_name = get_dimsvar_name(variable_name, num_dimensions, info->dimension_type);
    if (info->dimsvar_name == NULL)
    {
        return -1;
    }
    info->variable_name = &info->dimsvar_name[HARP_MAX_NUM_DIMS];

    return 0;

}

static int conversion_info_init_with_variable(conversion_info *info, const harp_product *product,
                                              const char *variable_name, int num_dimensions,
                                              const harp_dimension_type *dimension_type)
{
    if (conversion_info_init(info, product) != 0)
    {
        return -1;
    }
    return conversion_info_set_variable(info, variable_name, num_dimensions, dimension_type);
}

static void conversion_info_done(conversion_info *info)
{
    if (info->dimsvar_name != NULL)
    {
        free(info->dimsvar_name);
    }
    if (info->skip != NULL)
    {
        free(info->skip);
    }
    if (info->variable != NULL)
    {
        harp_variable_delete(info->variable);
    }
}

static int create_variable(conversion_info *info)
{
    const harp_variable_conversion *conversion = info->conversion;
    long dimension[HARP_MAX_NUM_DIMS];
    harp_variable *variable = NULL;
    int i;

    for (i = 0; i < conversion->num_dimensions; i++)
    {
        if (conversion->dimension_type[i] == harp_dimension_independent)
        {
            dimension[i] = conversion->independent_dimension_length;
        }
        else
        {
            dimension[i] = info->product->dimension[conversion->dimension_type[i]];
            if (dimension[i] == 0 && conversion->dimension_type[i] == harp_dimension_time)
            {
                /* make product time dependent */
                dimension[i] = 1;
            }
        }
    }

    if (harp_variable_new(conversion->variable_name, conversion->data_type, conversion->num_dimensions,
                          conversion->dimension_type, dimension, &variable) != 0)
    {
        return -1;
    }

    /* Set unit */
    if (conversion->unit != NULL)
    {
        variable->unit = strdup(conversion->unit);
        if (variable->unit == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                           __LINE__);
            harp_variable_delete(variable);
            return -1;
        }
    }

    info->variable = variable;

    return 0;
}

static int get_source_variable(conversion_info *info, harp_data_type data_type, const char *unit, int *is_temp)
{
    *is_temp = 0;

    if (harp_product_get_variable_by_name(info->product, info->variable_name, &info->variable) == 0)
    {
        if (harp_variable_has_dimension_types(info->variable, info->num_dimensions, info->dimension_type))
        {
            /* variable already exists */
            if (unit != NULL && !harp_variable_has_unit(info->variable, unit))
            {
                /* create a copy if we need to perform unit conversion */
                if (harp_variable_copy(info->variable, &info->variable) != 0)
                {
                    info->variable = NULL;
                    return -1;
                }
                *is_temp = 1;
                if (harp_variable_convert_unit(info->variable, unit) != 0)
                {
                    return -1;
                }
            }
            if (info->variable->data_type != data_type)
            {
                if (*is_temp == 0)
                {
                    /* create a copy if we need to perform data type conversion */
                    if (harp_variable_copy(info->variable, &info->variable) != 0)
                    {
                        info->variable = NULL;
                        return -1;
                    }
                    *is_temp = 1;
                }
                if (harp_variable_convert_data_type(info->variable, data_type) != 0)
                {
                    return -1;
                }
            }
            return 0;
        }
        info->variable = NULL;
    }

    *is_temp = 1;

    if (find_and_execute_conversion(info) != 0)
    {
        return -1;
    }

    if (unit != NULL)
    {
        if (harp_variable_convert_unit(info->variable, unit) != 0)
        {
            return -1;
        }
    }

    return 0;
}

static int perform_conversion(conversion_info *info)
{
    harp_variable *source_variable[MAX_NUM_SOURCE_VARIABLES];
    int is_temp[MAX_NUM_SOURCE_VARIABLES];
    int result;
    int i, j;

    for (i = 0; i < info->conversion->num_source_variables; i++)
    {
        conversion_info source_info;
        harp_source_variable_definition *source_definition = &info->conversion->source_definition[i];

        if (conversion_info_init_with_variable(&source_info, info->product, source_definition->variable_name,
                                               source_definition->num_dimensions, source_definition->dimension_type) !=
            0)
        {
            return -1;
        }
        memcpy(source_info.skip, info->skip, harp_derived_variable_conversions->num_variables);
        source_info.depth = info->depth + 1;

        if (get_source_variable(&source_info, source_definition->data_type, source_definition->unit, &is_temp[i]) != 0)
        {
            for (j = 0; j < i; j++)
            {
                if (is_temp[j])
                {
                    harp_variable_delete(source_variable[j]);
                }
            }
            return -1;
        }
        source_variable[i] = source_info.variable;
        source_info.variable = NULL;
        conversion_info_done(&source_info);
    }

    result = create_variable(info);
    if (result == 0)
    {
        result = info->conversion->set_variable_data(info->variable, (const harp_variable **)source_variable);
        /* TODO: set description of variable based on the applied conversion
         * e.g. <target_var_name> from (<source_var_name> from ...), (<source_var_2_name> from ...)
         */
    }

    for (j = 0; j < info->conversion->num_source_variables; j++)
    {
        if (is_temp[j])
        {
            harp_variable_delete(source_variable[j]);
        }
    }

    return result;
}

static void print_source_variable(const harp_source_variable_definition *source_definition,
                                  int (*print)(const char *, ...), int indent);

/* return: 0: possible, 1: not possible (at all), 2: not possible (cycle), 3: not possible (out of budget) */
static int find_source_variables(conversion_info *info, harp_source_variable_definition *source_definition,
                                 double total_budget, double *best_cost)
{
    harp_variable *variable;
    harp_variable_conversion_list *conversion_list;
    harp_variable_conversion *best_conversion = NULL;
    int is_out_of_budget = 0;
    int has_cycle = 0;
    int index;
    int i;

    if (total_budget < 0)
    {
        return 3;
    }

    if (harp_product_has_variable(info->product, source_definition->variable_name))
    {
        if (harp_product_get_variable_by_name(info->product, source_definition->variable_name, &variable) == 0 &&
            has_dimension_types(variable, source_definition->num_dimensions, source_definition->dimension_type,
                                source_definition->independent_dimension_length))
        {
            /* variable is present in the product */
            *best_cost = 0;
            return 0;
        }
    }

    if (total_budget < 1)
    {
        return 3;
    }

    /* if we are at the maximum search depth then bail out */
    if (info->depth == info->max_depth)
    {
        /* treat this as an out-of-budget (since we want to allow further searches for this variable at lower depths) */
        return 3;
    }

    /* try to find a conversion for the variable */
    index = hashtable_get_index_from_name(harp_derived_variable_conversions->hash_data,
                                          source_definition->dimsvar_name);
    if (index < 0)
    {
        /* no conversion found */
        return 1;
    }
    if (info->skip[index])
    {
        if (info->skip[index] == 2)
        {
            /* this is a cycle */
            return 2;
        }
        return 1;
    }

    conversion_list = harp_derived_variable_conversions->conversions_for_variable[index];

    for (i = 0; i < conversion_list->num_conversions; i++)
    {
        harp_variable_conversion *conversion = conversion_list->conversion[i];
        double budget = total_budget - 1;
        double total_cost = 1;
        int j;

        if (conversion->enabled != NULL && !conversion->enabled())
        {
            continue;
        }
        for (j = 0; j < conversion->num_dimensions; j++)
        {
            if (conversion->dimension_type[j] == harp_dimension_independent &&
                source_definition->independent_dimension_length >= 0 &&
                conversion->independent_dimension_length != source_definition->independent_dimension_length)
            {
                break;
            }
        }
        if (j < conversion->num_dimensions)
        {
            continue;
        }

        info->skip[index] = 2;
        info->depth++;

        for (j = 0; j < conversion->num_source_variables; j++)
        {
            double cost;
            int result;

            /* recursively find the source variables for creating this variable */
            result = find_source_variables(info, &conversion->source_definition[j], budget, &cost);
            if (result != 0)
            {
                /* source not found */
                if (result == 3)
                {
                    is_out_of_budget = 1;
                }
                else if (result == 2)
                {
                    has_cycle = 1;
                }
                break;
            }
            budget -= cost;
            total_cost += cost;
        }

        info->depth--;
        info->skip[index] = 0;

        if (j == conversion->num_source_variables)
        {
            /* the conversion is possible */
            if (best_conversion == NULL || total_cost < *best_cost)
            {
                best_conversion = conversion;
                *best_cost = total_cost;
            }
        }
    }

    if (best_conversion != NULL)
    {
        return 0;
    }

    /* no conversion found */

    if (is_out_of_budget)
    {
        return 3;
    }

    if (has_cycle)
    {
        return 2;
    }

    /* permanently mark this variable as something that cannot be derived */
    info->skip[index] = 1;

    return 1;
}

static int find_and_execute_conversion(conversion_info *info)
{
    int index;

    index = hashtable_get_index_from_name(harp_derived_variable_conversions->hash_data, info->dimsvar_name);
    if (index >= 0)
    {
        harp_variable_conversion_list *conversion_list =
            harp_derived_variable_conversions->conversions_for_variable[index];
        harp_variable_conversion *best_conversion = NULL;
        double best_cost;
        int i;

        for (i = 0; i < conversion_list->num_conversions; i++)
        {
            harp_variable_conversion *conversion = conversion_list->conversion[i];
            double budget = best_conversion == NULL ? harp_plusinf() : best_cost;
            double total_cost = 0;
            int j;

            if (conversion->enabled != NULL && !conversion->enabled())
            {
                continue;
            }
            if (info->skip[index])
            {
                continue;
            }

            info->skip[index] = 2;

            for (j = 0; j < conversion->num_source_variables; j++)
            {
                int result;
                double cost;

                result = find_source_variables(info, &conversion->source_definition[j], budget, &cost);
                if (result != 0)
                {
                    /* source not found */
                    break;
                }
                budget -= cost;
                total_cost += cost;
            }

            if (j == conversion->num_source_variables)
            {
                /* conversion should be possible */
                if (best_conversion == NULL || total_cost < best_cost)
                {
                    best_conversion = conversion;
                    best_cost = total_cost;
                }
            }

            info->skip[index] = 0;
        }

        if (best_conversion != NULL)
        {
            int result;

            info->conversion = best_conversion;
            info->skip[index] = 2;
            result = perform_conversion(info);
            info->skip[index] = 0;
            return result;
        }
    }

    set_variable_not_found_error(info);
    return -1;
}

static void print_conversion(conversion_info *info, int (*print)(const char *, ...));

static int find_and_print_conversion(conversion_info *info, int (*print)(const char *, ...))
{
    int index;

    index = hashtable_get_index_from_name(harp_derived_variable_conversions->hash_data, info->dimsvar_name);
    if (index >= 0)
    {
        harp_variable_conversion_list *conversion_list =
            harp_derived_variable_conversions->conversions_for_variable[index];
        harp_variable_conversion *best_conversion = NULL;
        double best_cost;
        int i;

        for (i = 0; i < conversion_list->num_conversions; i++)
        {
            harp_variable_conversion *conversion = conversion_list->conversion[i];
            double budget = best_conversion == NULL ? harp_plusinf() : best_cost;
            double total_cost = 0;
            int j;

            if (conversion->enabled != NULL && !conversion->enabled())
            {
                continue;
            }
            if (info->skip[index])
            {
                continue;
            }

            info->skip[index] = 2;

            for (j = 0; j < conversion->num_source_variables; j++)
            {
                int result;
                double cost;

                result = find_source_variables(info, &conversion->source_definition[j], budget, &cost);
                if (result != 0)
                {
                    /* source not found */
                    break;
                }
                budget -= cost;
                total_cost += cost;
            }

            if (j == conversion->num_source_variables)
            {
                /* all source variables were found, conversion should be possible */
                if (best_conversion == NULL || total_cost < best_cost)
                {
                    best_conversion = conversion;
                    best_cost = total_cost;
                }
            }

            info->skip[index] = 0;
        }

        if (best_conversion != NULL)
        {
            info->conversion = best_conversion;
            info->skip[index] = 2;
            print_conversion(info, print);
            info->skip[index] = 0;
            return 0;
        }
    }

    set_variable_not_found_error(info);
    return -1;
}

static int print_source_variable_conversion(conversion_info *info, int (*print)(const char *, ...))
{
    harp_variable *variable;

    if (harp_product_get_variable_by_name(info->product, info->variable_name, &variable) == 0)
    {
        if (harp_variable_has_dimension_types(variable, info->num_dimensions, info->dimension_type))
        {
            print("\n");
            return 0;
        }
    }
    return find_and_print_conversion(info, print);
}

static void print_conversion_variable(const harp_variable_conversion *conversion, int (*print)(const char *, ...))
{
    int i;

    print("%s", conversion->variable_name);
    if (conversion->num_dimensions > 0)
    {
        print(" {");
        for (i = 0; i < conversion->num_dimensions; i++)
        {
            print("%s", harp_get_dimension_type_name(conversion->dimension_type[i]));
            if (conversion->dimension_type[i] == harp_dimension_independent)
            {
                print("(%ld)", conversion->independent_dimension_length);
            }
            if (i < conversion->num_dimensions - 1)
            {
                print(",");
            }
        }
        print("}");
    }
    if (conversion->unit != NULL)
    {
        print(" [%s]", conversion->unit);
    }
    print(" (%s)", harp_get_data_type_name(conversion->data_type));
}

static void print_source_variable(const harp_source_variable_definition *source_definition,
                                  int (*print)(const char *, ...), int indent)
{
    int k;

    for (k = 0; k < indent; k++)
    {
        print("  ");
    }
    print("%s", source_definition->variable_name);
    if (source_definition->num_dimensions > 0)
    {
        print(" {");
        for (k = 0; k < source_definition->num_dimensions; k++)
        {
            print("%s", harp_get_dimension_type_name(source_definition->dimension_type[k]));
            if (source_definition->dimension_type[k] == harp_dimension_independent &&
                source_definition->independent_dimension_length >= 0)
            {
                print("(%ld)", source_definition->independent_dimension_length);
            }
            if (k < source_definition->num_dimensions - 1)
            {
                print(",");
            }
        }
        print("}");
    }
    if (source_definition->unit != NULL)
    {
        print(" [%s]", source_definition->unit);
    }
    print(" (%s)", harp_get_data_type_name(source_definition->data_type));
}

static void print_conversion(conversion_info *info, int (*print)(const char *, ...))
{
    int i, k;

    if (info->conversion->num_source_variables == 0)
    {
        print("\n");
        for (k = 0; k < info->depth; k++)
        {
            print("  ");
        }
        print("derived without input variables\n");
    }
    else
    {
        print(" from\n");
        for (i = 0; i < info->conversion->num_source_variables; i++)
        {
            conversion_info source_info;
            harp_source_variable_definition *source_definition = &info->conversion->source_definition[i];

            print_source_variable(source_definition, print, info->depth);
            if (conversion_info_init_with_variable(&source_info, info->product, source_definition->variable_name,
                                                   source_definition->num_dimensions, source_definition->dimension_type)
                != 0)
            {
                print("ERROR: %s\n", harp_errno_to_string(harp_errno));
                return;
            }
            memcpy(source_info.skip, info->skip, harp_derived_variable_conversions->num_variables);
            source_info.depth = info->depth + 1;

            if (print_source_variable_conversion(&source_info, print) != 0)
            {
                for (k = 0; k < info->depth; k++)
                {
                    print("  ");
                }
                print("ERROR: %s\n", harp_errno_to_string(harp_errno));
            }
            conversion_info_done(&source_info);
        }
    }
    if (info->conversion->source_description != NULL)
    {
        for (k = 0; k < info->depth; k++)
        {
            print("  ");
        }
        print("note: %s\n", info->conversion->source_description);
    }
}

void harp_variable_conversion_print(const harp_variable_conversion *conversion, int (*print)(const char *, ...))
{
    int i;

    print_conversion_variable(conversion, print);
    if (conversion->num_source_variables > 0)
    {
        print(" from\n");
        for (i = 0; i < conversion->num_source_variables; i++)
        {
            print_source_variable(&conversion->source_definition[i], print, 1);
            print("\n");
        }
    }
    else
    {
        print("\n  derived without input variables\n");
    }
    if (conversion->source_description != NULL)
    {
        print("  note: %s\n", conversion->source_description);
    }
    print("\n");
}

void harp_variable_conversion_delete(harp_variable_conversion *conversion)
{
    if (conversion == NULL)
    {
        return;
    }

    if (conversion->dimsvar_name != NULL)
    {
        free(conversion->dimsvar_name);
    }
    /* we don't have to remove variable_name, since this is a pointer into dimsvar_name */
    if (conversion->unit != NULL)
    {
        free(conversion->unit);
    }

    if (conversion->source_definition != NULL)
    {
        int i;

        for (i = 0; i < conversion->num_source_variables; i++)
        {
            if (conversion->source_definition[i].dimsvar_name != NULL)
            {
                free(conversion->source_definition[i].dimsvar_name);
            }
            /* we don't have to remove source_definition[i].variable_name, since this is a pointer into dimsvar_name */
            if (conversion->source_definition[i].unit != NULL)
            {
                free(conversion->source_definition[i].unit);
            }
        }
        free(conversion->source_definition);
    }

    if (conversion->source_description != NULL)
    {
        free(conversion->source_description);
    }

    free(conversion);
}

/* this function also adds the conversion to the global derived variable conversion list */
int harp_variable_conversion_new(const char *variable_name, harp_data_type data_type, const char *unit,
                                 int num_dimensions, harp_dimension_type *dimension_type,
                                 long independent_dimension_length, harp_conversion_function set_variable_data,
                                 harp_variable_conversion **new_conversion)
{
    harp_variable_conversion *conversion = NULL;
    int i;

    conversion = (harp_variable_conversion *)malloc(sizeof(harp_variable_conversion));
    if (conversion == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate % lu bytes) (%s:%u)",
                       sizeof(harp_variable_conversion), __FILE__, __LINE__);
        return -1;
    }
    conversion->variable_name = NULL;
    conversion->dimsvar_name = NULL;
    conversion->data_type = data_type;
    conversion->unit = NULL;
    conversion->num_dimensions = num_dimensions;
    for (i = 0; i < num_dimensions; i++)
    {
        conversion->dimension_type[i] = dimension_type[i];
    }
    conversion->independent_dimension_length = independent_dimension_length;
    conversion->num_source_variables = 0;
    conversion->source_definition = NULL;
    conversion->source_description = NULL;
    conversion->set_variable_data = set_variable_data;
    conversion->enabled = NULL;

    conversion->dimsvar_name = get_dimsvar_name(variable_name, num_dimensions, dimension_type);
    if (conversion->dimsvar_name == NULL)
    {
        harp_variable_conversion_delete(conversion);
        return -1;
    }
    conversion->variable_name = &conversion->dimsvar_name[HARP_MAX_NUM_DIMS];

    if (unit != NULL)
    {
        conversion->unit = strdup(unit);
        if (conversion->unit == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                           __LINE__);
            harp_variable_conversion_delete(conversion);
            return -1;
        }
    }

    if (harp_derived_variable_list_add_conversion(conversion) != 0)
    {
        harp_variable_conversion_delete(conversion);
        return -1;
    }

    *new_conversion = conversion;

    return 0;
}

int harp_variable_conversion_add_source(harp_variable_conversion *conversion, const char *variable_name,
                                        harp_data_type data_type, const char *unit, int num_dimensions,
                                        harp_dimension_type *dimension_type, long independent_dimension_length)
{
    harp_source_variable_definition *source_definition;
    int i;

    assert(conversion->num_source_variables < MAX_NUM_SOURCE_VARIABLES);

    source_definition = realloc(conversion->source_definition,
                                (conversion->num_source_variables + 1) * sizeof(harp_source_variable_definition));
    if (source_definition == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate % lu bytes) (%s:%u)",
                       (conversion->num_source_variables + 1) * sizeof(harp_source_variable_definition),
                       __FILE__, __LINE__);
        return -1;
    }
    conversion->source_definition = source_definition;

    source_definition = &conversion->source_definition[conversion->num_source_variables];
    conversion->num_source_variables++;

    source_definition->variable_name = NULL;
    source_definition->data_type = data_type;
    source_definition->unit = NULL;
    source_definition->num_dimensions = num_dimensions;
    source_definition->independent_dimension_length = independent_dimension_length;
    for (i = 0; i < num_dimensions; i++)
    {
        source_definition->dimension_type[i] = dimension_type[i];
    }

    source_definition->dimsvar_name = get_dimsvar_name(variable_name, num_dimensions, dimension_type);
    if (source_definition->dimsvar_name == NULL)
    {
        return -1;
    }
    source_definition->variable_name = &source_definition->dimsvar_name[HARP_MAX_NUM_DIMS];

    if (unit != NULL)
    {
        source_definition->unit = strdup(unit);
        if (source_definition->unit == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                           __LINE__);
            return -1;
        }
    }

    return 0;
}

int harp_variable_conversion_set_enabled_function(harp_variable_conversion *conversion,
                                                  harp_conversion_enabled_function enabled)
{
    assert(conversion->enabled == NULL);

    conversion->enabled = enabled;

    return 0;
}

int harp_variable_conversion_set_source_description(harp_variable_conversion *conversion, const char *description)
{
    assert(conversion->source_description == NULL);

    conversion->source_description = strdup(description);
    if (conversion->source_description == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        return -1;
    }

    return 0;
}

/** Print the full listing of available variable conversions.
 * \ingroup harp_documentation
 * If product is NULL then all possible conversions will be printed. If a product is provided then only conversions
 * that can be made using the content of that product will be shown.
 * The \a print function parameter should be a function that resembles printf().
 * The most common case use is to just use printf() itself. For example:
 * \code{.c}
 * harp_doc_list_conversions(product, printf);
 * \endcode
 * \param product Pointer to a HARP product (can be NULL).
 * \param variable_name Name of the target variable for which to show conversions (can be NULL).
 * \param print Reference to a printf compatible function.
 * \return
 *   \arg \c  0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_doc_list_conversions(const harp_product *product, const char *variable_name,
                                          int (*print)(const char *, ...))
{
    conversion_info info;
    int i, j;

    if (harp_derived_variable_conversions == NULL)
    {
        if (harp_derived_variable_list_init() != 0)
        {
            return -1;
        }
    }

    if (product == NULL)
    {
        /* just print all conversions */
        for (i = 0; i < harp_derived_variable_conversions->num_variables; i++)
        {
            harp_variable_conversion_list *conversion_list =
                harp_derived_variable_conversions->conversions_for_variable[i];
            int first = 1;

            for (j = 0; j < conversion_list->num_conversions; j++)
            {
                harp_variable_conversion *conversion = conversion_list->conversion[j];

                if (variable_name != NULL && strcmp(conversion->variable_name, variable_name) != 0)
                {
                    continue;
                }

                if (first)
                {
                    print("============================================================\n");
                    first = 0;
                }

                if (conversion->enabled != NULL && !conversion->enabled())
                {
                    continue;
                }
                harp_variable_conversion_print(conversion, print);
            }
        }
        return 0;
    }

    if (conversion_info_init(&info, product) != 0)
    {
        return -1;
    }

    /* Show possible conversions */
    for (i = 0; i < harp_derived_variable_conversions->num_variables; i++)
    {
        harp_variable_conversion_list *conversion_list = harp_derived_variable_conversions->conversions_for_variable[i];
        harp_variable_conversion *best_conversion = NULL;
        harp_variable_conversion *first_conversion;
        harp_variable *variable;
        double best_cost;

        assert(conversion_list->num_conversions > 0);

        first_conversion = conversion_list->conversion[0];

        if (variable_name != NULL && strcmp(first_conversion->variable_name, variable_name) != 0)
        {
            continue;
        }

        if (harp_product_has_variable(product, first_conversion->variable_name))
        {
            if (harp_product_get_variable_by_name(product, first_conversion->variable_name, &variable) == 0 &&
                harp_variable_has_dimension_types(variable, first_conversion->num_dimensions,
                                                  first_conversion->dimension_type))
            {
                /* variable with same dimensions already exists -> skip conversions for this variable */
                continue;
            }
        }

        /* initialize based on first conversion in the list */
        if (conversion_info_set_variable(&info, first_conversion->variable_name, first_conversion->num_dimensions,
                                         first_conversion->dimension_type) != 0)
        {
            return -1;
        }

        info.skip[i] = 2;

        for (j = 0; j < conversion_list->num_conversions; j++)
        {
            harp_variable_conversion *conversion = conversion_list->conversion[j];
            double budget = best_conversion == NULL ? harp_plusinf() : best_cost;
            double total_cost = 0;
            int k;

            if (conversion->enabled != NULL && !conversion->enabled())
            {
                continue;
            }

            for (k = 0; k < conversion->num_source_variables; k++)
            {
                double cost;
                int result;

                result = find_source_variables(&info, &conversion->source_definition[k], budget, &cost);
                if (result != 0)
                {
                    /* source not found */
                    break;
                }
                budget -= cost;
                total_cost += cost;
            }

            if (k == conversion->num_source_variables)
            {
                /* all sources are found, conversion should be possible */
                if (best_conversion == NULL || total_cost < best_cost)
                {
                    best_conversion = conversion;
                    best_cost = total_cost;
                }
            }
        }

        if (best_conversion != NULL)
        {
            info.conversion = best_conversion;
            print_conversion_variable(best_conversion, print);
            info.depth++;
            print_conversion(&info, print);
            info.depth--;
            print("\n");
            info.skip[i] = 0;
        }
        else
        {
            info.skip[i] = 1;
        }
    }

    conversion_info_done(&info);

    return 0;
}

/** Retrieve a new variable based on the set of automatic conversions that are supported by HARP.
 * \ingroup harp_product
 * If the product already contained a variable with the given name, you will get a copy of that variable (and converted
 * to the specified data type and unit). Otherwise the function will try to create a new variable based on the data
 * found in the product or on available auxiliary data (e.g. built-in climatology).
 * The caller of this function will be responsible for the memory management of the returned variable.
 * \note setting unit to NULL returns a variable in the original unit
 * \note pointers to axis variables are passed through unmodified.
 * \param product Product from which to derive the new variable.
 * \param name Name of the variable that should be created.
 * \param data_type Data type (optional) of the variable that should be created.
 * \param unit Unit (optional) of the variable that should be created.
 * \param num_dimensions Number of dimensions of the variable that should be created.
 * \param dimension_type Type of dimension for each of the dimensions of the variable that should be created.
 * \param variable Pointer to the C variable where the derived HARP variable will be stored.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_product_get_derived_variable(const harp_product *product, const char *name,
                                                  const harp_data_type *data_type, const char *unit, int num_dimensions,
                                                  const harp_dimension_type *dimension_type, harp_variable **variable)
{
    conversion_info info;

    if (name == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "name of variable to be derived is empty (%s:%u)", __FILE__,
                       __LINE__);
        return -1;
    }

    if (harp_product_get_variable_by_name(product, name, &info.variable) == 0)
    {
        if (harp_variable_has_dimension_types(info.variable, num_dimensions, dimension_type))
        {
            /* variable already exists -> create a copy */
            if (harp_variable_copy(info.variable, &info.variable) != 0)
            {
                return -1;
            }

            if (unit != NULL)
            {
                if (harp_variable_has_unit(info.variable, unit))
                {
                    if (strcmp(info.variable->unit, unit) != 0)
                    {
                        /* make sure that the unit becomes syntactically the same */
                        if (harp_variable_set_unit(info.variable, unit) != 0)
                        {
                            return -1;
                        }
                    }
                }
                else
                {
                    if (harp_variable_convert_unit(info.variable, unit) != 0)
                    {
                        return -1;
                    }
                }
            }
            if ((data_type != NULL) && (info.variable->data_type != *data_type))
            {
                if (harp_variable_convert_data_type(info.variable, *data_type) != 0)
                {
                    return -1;
                }
            }
            *variable = info.variable;
            return 0;
        }
    }

    if (harp_derived_variable_conversions == NULL)
    {
        if (harp_derived_variable_list_init() != 0)
        {
            return -1;
        }
    }

    if (conversion_info_init_with_variable(&info, product, name, num_dimensions, dimension_type) != 0)
    {
        return -1;
    }

    if (find_and_execute_conversion(&info) != 0)
    {
        conversion_info_done(&info);
        return -1;
    }

    if (unit != NULL)
    {
        if (harp_variable_convert_unit(info.variable, unit) != 0)
        {
            conversion_info_done(&info);
            return -1;
        }
    }
    if (data_type != NULL)
    {
        if (info.variable->data_type != *data_type)
        {
            if (harp_variable_convert_data_type(info.variable, *data_type) != 0)
            {
                conversion_info_done(&info);
                return -1;
            }
        }
    }

    *variable = info.variable;
    info.variable = NULL;

    conversion_info_done(&info);

    return 0;
}

/** Create a derived variable and add it to the product.
 * \ingroup harp_product
 * If a similar named variable with the right dimensions was already in the product then that variable
 * will be modified to match the given unit
 * (and in case \a unit is NULL, then the function will just leave the product unmodified).
 * Otherwise the function will call harp_product_get_derived_variable() and add the new variable using
 * harp_product_add_variable() (removing any existing variable with the same name, but different dimensions)
 * \param product Product from which to derive the new variable and into which the derived variable should be placed.
 * \param name Name of the variable that should be added.
 * \param data_type Data type (optional) of the variable that should be added.
 * \param unit Unit (optional) of the variable that should be added.
 * \param num_dimensions Number of dimensions of the variable that should be created.
 * \param dimension_type Type of dimension for each of the dimensions of the variable that should be created.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_product_add_derived_variable(harp_product *product, const char *name,
                                                  const harp_data_type *data_type, const char *unit, int num_dimensions,
                                                  const harp_dimension_type *dimension_type)
{
    harp_variable *new_variable;
    harp_variable *variable = NULL;

    if (harp_product_get_variable_by_name(product, name, &variable) == 0)
    {
        if (harp_variable_has_dimension_types(variable, num_dimensions, dimension_type))
        {
            /* variable already exists */
            if (unit != NULL)
            {
                if (harp_variable_has_unit(variable, unit))
                {
                    if (strcmp(variable->unit, unit) != 0)
                    {
                        /* make sure that the unit becomes syntactically the same */
                        if (harp_variable_set_unit(variable, unit) != 0)
                        {
                            return -1;
                        }
                    }
                }
                else
                {
                    if (harp_variable_convert_unit(variable, unit) != 0)
                    {
                        return -1;
                    }
                }
            }
            if ((data_type != NULL) && (variable->data_type != *data_type))
            {
                if (harp_variable_convert_data_type(variable, *data_type) != 0)
                {
                    return -1;
                }
            }
            return 0;
        }
    }

    if (harp_derived_variable_conversions == NULL)
    {
        if (harp_derived_variable_list_init() != 0)
        {
            return -1;
        }
    }

    /* variable with right dimensions does not yet exist -> create and add it */
    if (harp_product_get_derived_variable(product, name, data_type, unit, num_dimensions, dimension_type, &new_variable)
        != 0)
    {
        return -1;
    }
    if (variable != NULL)
    {
        /* remove existing variable with same name (but different dimension) */
        if (harp_product_remove_variable(product, variable) != 0)
        {
            harp_variable_delete(new_variable);
            return -1;
        }
    }
    if (harp_product_add_variable(product, new_variable) != 0)
    {
        harp_variable_delete(new_variable);
        return -1;
    }

    return 0;
}
