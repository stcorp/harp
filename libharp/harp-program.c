/*
 * Copyright (C) 2015-2022 S[&]T, The Netherlands.
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

#include "harp-program.h"
#include "harp-filter-collocation.h"
#include "harp-filter.h"
#include "harp-filter.h"
#include "harp-filter-collocation.h"
#include "harp-vertical-profiles.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

int harp_program_new(harp_program **new_program)
{
    harp_program *program;

    program = (harp_program *)malloc(sizeof(harp_program));
    if (program == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_program), __FILE__, __LINE__);
        return -1;
    }

    program->num_operations = 0;
    program->operation = NULL;
    program->current_index = 0;

    program->option_enable_aux_afgl86 = harp_get_option_enable_aux_afgl86();
    program->option_enable_aux_usstd76 = harp_get_option_enable_aux_usstd76();
    program->option_regrid_out_of_bounds = harp_get_option_regrid_out_of_bounds();

    /* we only explicitly set the regrid_out_of_bounds option */
    harp_set_option_regrid_out_of_bounds(0);

    *new_program = program;
    return 0;
}

void harp_program_delete(harp_program *program)
{
    if (program != NULL)
    {
        /* reset global HARP options to initial values */
        harp_set_option_enable_aux_afgl86(program->option_enable_aux_afgl86);
        harp_set_option_enable_aux_usstd76(program->option_enable_aux_usstd76);
        harp_set_option_regrid_out_of_bounds(program->option_regrid_out_of_bounds);

        if (program->operation != NULL)
        {
            int i;

            for (i = 0; i < program->num_operations; i++)
            {
                harp_operation_delete(program->operation[i]);
            }

            free(program->operation);
        }

        free(program);
    }
}

int harp_program_add_operation(harp_program *program, harp_operation *operation)
{
    if (program->num_operations % BLOCK_SIZE == 0)
    {
        harp_operation **operation;

        operation = (harp_operation **)realloc(program->operation,
                                               (program->num_operations + BLOCK_SIZE) * sizeof(harp_operation *));
        if (operation == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           (program->num_operations + BLOCK_SIZE) * sizeof(harp_operation *), __FILE__, __LINE__);
            return -1;
        }

        program->operation = operation;
    }

    program->operation[program->num_operations] = operation;
    program->num_operations++;

    return 0;
}

static int execute_value_filter(harp_product *product, harp_program *program)
{
    harp_dimension_mask_set *dimension_mask_set = NULL;
    harp_variable *variable;
    const char *variable_name;
    int num_operations = 1;
    int data_type_size;
    long i, j;
    int k;

    if (harp_operation_get_variable_name(program->operation[program->current_index], &variable_name) != 0)
    {
        return -1;
    }

    /* if the next operations are also value filters on the same variable then include them */
    while (program->current_index + num_operations < program->num_operations)
    {
        const char *next_variable_name;

        if (!harp_operation_is_value_filter(program->operation[program->current_index + num_operations]))
        {
            break;
        }
        if (harp_operation_get_variable_name(program->operation[program->current_index + num_operations],
                                             &next_variable_name) != 0)
        {
            return -1;
        }
        if (strcmp(variable_name, next_variable_name) != 0)
        {
            break;
        }
        num_operations++;
    }

    if (harp_product_get_variable_by_name(product, variable_name, &variable) != 0)
    {
        return -1;
    }
    data_type_size = harp_get_size_for_type(variable->data_type);

    for (k = 0; k < num_operations; k++)
    {
        if (harp_operation_set_valid_range(program->operation[program->current_index + k], variable->data_type,
                                           variable->valid_min, variable->valid_max) != 0)
        {
            return -1;
        }
        if (variable->unit != NULL)
        {
            if (harp_operation_set_value_unit(program->operation[program->current_index + k], variable->unit) != 0)
            {
                return -1;
            }
        }
    }

    if (variable->num_dimensions == 0)
    {
        for (k = 0; k < num_operations; k++)
        {
            int result;

            if (harp_operation_is_string_value_filter(program->operation[program->current_index + k]))
            {
                harp_operation_string_value_filter *operation;

                operation = (harp_operation_string_value_filter *)program->operation[program->current_index + k];
                result = operation->eval(operation, variable->num_enum_values, variable->enum_name,
                                         variable->data_type, variable->data.ptr);
            }
            else
            {
                harp_operation_numeric_value_filter *operation;

                operation = (harp_operation_numeric_value_filter *)program->operation[program->current_index + k];
                result = operation->eval(operation, variable->data_type, variable->data.ptr);
            }
            if (result < 0)
            {
                return -1;
            }
            if (result == 0)
            {
                /* the full product is masked out so remove all variables to make it empty */
                harp_product_remove_all_variables(product);
                return 0;
            }
        }
    }
    else if (variable->num_dimensions == 1 && variable->dimension_type[0] != harp_dimension_independent)
    {
        harp_dimension_mask *dimension_mask;

        if (harp_dimension_mask_set_new(&dimension_mask_set) != 0)
        {
            return -1;
        }

        if (harp_dimension_mask_new(variable->num_dimensions, variable->dimension, &dimension_mask) != 0)
        {
            harp_dimension_mask_set_delete(dimension_mask_set);
            return -1;
        }
        dimension_mask_set[variable->dimension_type[0]] = dimension_mask;

        for (i = 0; i < variable->num_elements; i++)
        {
            for (k = 0; k < num_operations; k++)
            {
                if (dimension_mask->mask[i])
                {
                    harp_operation *operation;
                    int result;

                    operation = program->operation[program->current_index + k];
                    if (harp_operation_is_string_value_filter(operation))
                    {
                        harp_operation_string_value_filter *string_operation;

                        string_operation = (harp_operation_string_value_filter *)operation;
                        result = string_operation->eval(string_operation, variable->num_enum_values,
                                                        variable->enum_name, variable->data_type,
                                                        &variable->data.int8_data[i * data_type_size]);
                    }
                    else
                    {
                        harp_operation_numeric_value_filter *numeric_operation;

                        numeric_operation = (harp_operation_numeric_value_filter *)operation;
                        result = numeric_operation->eval(numeric_operation, variable->data_type,
                                                         &variable->data.int8_data[i * data_type_size]);
                    }
                    if (result < 0)
                    {
                        harp_dimension_mask_set_delete(dimension_mask_set);
                        return -1;
                    }
                    dimension_mask->mask[i] = result;
                }
            }
            if (!dimension_mask->mask[i])
            {
                dimension_mask->masked_dimension_length--;
            }
        }

        if (harp_product_filter(product, dimension_mask_set) != 0)
        {
            harp_dimension_mask_set_delete(dimension_mask_set);
            return -1;
        }

        harp_dimension_mask_set_delete(dimension_mask_set);
    }
    else if (variable->num_dimensions == 2 && variable->dimension_type[0] == harp_dimension_time &&
             variable->dimension_type[1] != harp_dimension_independent &&
             variable->dimension_type[1] != harp_dimension_time)
    {
        harp_dimension_type dimension_type;
        harp_dimension_mask *time_mask;
        harp_dimension_mask *dimension_mask;
        long index = 0;

        dimension_type = variable->dimension_type[1];

        if (harp_dimension_mask_set_new(&dimension_mask_set) != 0)
        {
            return -1;
        }

        if (harp_dimension_mask_new(1, variable->dimension, &dimension_mask_set[harp_dimension_time]) != 0)
        {
            return -1;
        }
        time_mask = dimension_mask_set[harp_dimension_time];

        if (harp_dimension_mask_new(variable->num_dimensions, variable->dimension, &dimension_mask_set[dimension_type])
            != 0)
        {
            return -1;
        }
        dimension_mask = dimension_mask_set[dimension_type];

        dimension_mask->masked_dimension_length = 0;
        for (i = 0; i < variable->dimension[0]; i++)
        {
            long new_dimension_length = 0;

            for (j = 0; j < variable->dimension[1]; j++)
            {
                for (k = 0; k < num_operations; k++)
                {
                    if (dimension_mask->mask[index])
                    {
                        harp_operation *operation;
                        int result;

                        operation = program->operation[program->current_index + k];
                        if (harp_operation_is_string_value_filter(operation))
                        {
                            harp_operation_string_value_filter *string_operation;

                            string_operation = (harp_operation_string_value_filter *)operation;
                            result = string_operation->eval(string_operation, variable->num_enum_values,
                                                            variable->enum_name, variable->data_type,
                                                            &variable->data.int8_data[index * data_type_size]);
                        }
                        else
                        {
                            harp_operation_numeric_value_filter *numeric_operation;

                            numeric_operation = (harp_operation_numeric_value_filter *)operation;
                            result = numeric_operation->eval(numeric_operation, variable->data_type,
                                                             &variable->data.int8_data[index * data_type_size]);
                        }
                        if (result < 0)
                        {
                            harp_dimension_mask_set_delete(dimension_mask_set);
                            return -1;
                        }
                        dimension_mask->mask[index] = result;
                    }
                }
                if (dimension_mask->mask[index])
                {
                    new_dimension_length++;
                }
                index++;
            }
            if (new_dimension_length == 0)
            {
                time_mask->mask[i] = 0;
                time_mask->masked_dimension_length--;
            }
            else if (new_dimension_length > dimension_mask->masked_dimension_length)
            {
                dimension_mask->masked_dimension_length = new_dimension_length;
            }
        }

        if (harp_product_filter(product, dimension_mask_set) != 0)
        {
            harp_dimension_mask_set_delete(dimension_mask_set);
            return -1;
        }

        harp_dimension_mask_set_delete(dimension_mask_set);
    }
    else
    {
        harp_set_error(HARP_ERROR_OPERATION, "variable '%s' has invalid dimensions for filtering", variable_name);
        return -1;
    }

    /* jump to the last operation in the list that we performed */
    program->current_index += num_operations - 1;

    return 0;
}

static int execute_index_filter(harp_product *product, harp_program *program)
{
    harp_dimension_mask_set *dimension_mask_set = NULL;
    harp_operation_index_filter *operation;
    harp_dimension_mask *dimension_mask;
    long dimension;
    long i;

    if (harp_dimension_mask_set_new(&dimension_mask_set) != 0)
    {
        return -1;
    }

    operation = (harp_operation_index_filter *)program->operation[program->current_index];
    dimension = product->dimension[operation->dimension_type];
    if (dimension <= 0)
    {
        return 0;
    }

    if (harp_dimension_mask_set_new(&dimension_mask_set) != 0)
    {
        return -1;
    }
    if (harp_dimension_mask_new(1, &dimension, &dimension_mask) != 0)
    {
        harp_dimension_mask_set_delete(dimension_mask_set);
        return -1;
    }
    dimension_mask_set[operation->dimension_type] = dimension_mask;

    for (i = 0; i < dimension; i++)
    {
        int result;

        result = operation->eval(operation, i);
        if (result < 0)
        {
            harp_dimension_mask_set_delete(dimension_mask_set);
            return -1;
        }
        dimension_mask->mask[i] = result;
        if (!dimension_mask->mask[i])
        {
            dimension_mask->masked_dimension_length--;
        }
    }

    if (harp_product_filter(product, dimension_mask_set) != 0)
    {
        harp_dimension_mask_set_delete(dimension_mask_set);
        return -1;
    }

    harp_dimension_mask_set_delete(dimension_mask_set);

    return 0;
}

static int execute_point_filter(harp_product *product, harp_program *program)
{
    harp_dimension_type dimension_type = harp_dimension_time;
    harp_data_type data_type = harp_type_double;
    harp_variable *latitude;
    harp_variable *longitude;
    uint8_t *mask;
    int num_operations = 1;
    long num_points;
    long i;
    int k;

    if (harp_product_get_derived_variable(product, "latitude", &data_type, "degree_north", 1, &dimension_type,
                                          &latitude) != 0)
    {
        return -1;
    }
    if (harp_product_get_derived_variable(product, "longitude", &data_type, "degree_east", 1, &dimension_type,
                                          &longitude) != 0)
    {
        harp_variable_delete(latitude);
        return -1;
    }

    num_points = latitude->dimension[0];

    /* if the next operations are also point filters then include them */
    while (program->current_index + num_operations < program->num_operations)
    {
        if (!harp_operation_is_point_filter(program->operation[program->current_index + num_operations]))
        {
            break;
        }
        num_operations++;
    }

    mask = (uint8_t *)malloc(num_points * sizeof(uint8_t));
    if (mask == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_points * sizeof(uint8_t), __FILE__, __LINE__);
        return -1;
    }

    for (i = 0; i < num_points; i++)
    {
        harp_spherical_point point;

        point.lat = latitude->data.double_data[i];
        point.lon = longitude->data.double_data[i];
        harp_spherical_point_rad_from_deg(&point);
        harp_spherical_point_check(&point);

        mask[i] = 1;
        for (k = 0; k < num_operations; k++)
        {
            if (mask[i])
            {
                harp_operation_point_filter *operation;
                int result;

                operation = (harp_operation_point_filter *)program->operation[program->current_index + k];
                result = operation->eval(operation, &point);
                if (result < 0)
                {
                    harp_variable_delete(latitude);
                    harp_variable_delete(longitude);
                    free(mask);
                    return -1;
                }
                mask[i] = result;
            }
        }
    }

    harp_variable_delete(latitude);
    harp_variable_delete(longitude);

    if (harp_product_filter_dimension(product, harp_dimension_time, mask) != 0)
    {
        free(mask);
        return -1;
    }
    free(mask);

    /* jump to the last operation in the list that we performed */
    program->current_index += num_operations - 1;

    return 0;
}

static int execute_polygon_filter(harp_product *product, harp_program *program)
{
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_independent };
    harp_data_type data_type = harp_type_double;
    harp_variable *latitude_bounds;
    harp_variable *longitude_bounds;
    uint8_t *mask;
    int num_operations = 1;
    long num_areas;
    long num_points;
    long i;
    int k;

    if (harp_product_get_derived_variable(product, "latitude_bounds", &data_type, "degree_north", 2, dimension_type,
                                          &latitude_bounds) != 0)
    {
        return -1;
    }
    if (harp_product_get_derived_variable(product, "longitude_bounds", &data_type, "degree_east", 2, dimension_type,
                                          &longitude_bounds) != 0)
    {
        harp_variable_delete(latitude_bounds);
        return -1;
    }

    if (latitude_bounds->dimension[1] != longitude_bounds->dimension[1])
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "the length of the independent dimension of variable "
                       "'latitude_bounds' (%ld) does not match the length of the independent dimension of variable "
                       "'longitude_bounds' (%ld)", latitude_bounds->dimension[1], longitude_bounds->dimension[1]);
        harp_variable_delete(latitude_bounds);
        harp_variable_delete(longitude_bounds);
        return -1;
    }
    if (latitude_bounds->dimension[1] < 2)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "the length of the independent dimension of variables "
                       "'latitude_bounds' and 'longitude_bounds' should be 2 or higher");
        harp_variable_delete(latitude_bounds);
        harp_variable_delete(longitude_bounds);
        return -1;
    }

    num_areas = latitude_bounds->dimension[0];
    num_points = latitude_bounds->dimension[1];

    /* if the next operations are also polygon filters then include them */
    while (program->current_index + num_operations < program->num_operations)
    {
        if (!harp_operation_is_polygon_filter(program->operation[program->current_index + num_operations]))
        {
            break;
        }
        num_operations++;
    }

    mask = (uint8_t *)malloc(latitude_bounds->dimension[0] * sizeof(uint8_t));
    if (mask == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       latitude_bounds->dimension[0] * sizeof(uint8_t), __FILE__, __LINE__);
        return -1;
    }

    for (i = 0; i < num_areas; i++)
    {
        harp_spherical_polygon *area;

        if (harp_spherical_polygon_from_latitude_longitude_bounds(0, num_points,
                                                                  &latitude_bounds->data.double_data[i * num_points],
                                                                  &longitude_bounds->data.double_data[i * num_points],
                                                                  &area) != 0)
        {
            harp_variable_delete(latitude_bounds);
            harp_variable_delete(longitude_bounds);
            free(mask);
            return -1;
        }
        else
        {
            mask[i] = 1;
            for (k = 0; k < num_operations; k++)
            {
                if (mask[i])
                {
                    harp_operation_polygon_filter *operation;
                    int result;

                    operation = (harp_operation_polygon_filter *)program->operation[program->current_index + k];
                    result = operation->eval(operation, area);
                    if (result < 0)
                    {
                        harp_variable_delete(latitude_bounds);
                        harp_variable_delete(longitude_bounds);
                        harp_spherical_polygon_delete(area);
                        free(mask);
                        return -1;
                    }
                    mask[i] = result;
                }
            }
        }
        harp_spherical_polygon_delete(area);
    }

    harp_variable_delete(latitude_bounds);
    harp_variable_delete(longitude_bounds);

    if (harp_product_filter_dimension(product, harp_dimension_time, mask) != 0)
    {
        free(mask);
        return -1;
    }
    free(mask);

    /* jump to the last operation in the list that we performed */
    program->current_index += num_operations - 1;

    return 0;
}

static int execute_collocation_filter(harp_product *product, harp_operation_collocation_filter *operation)
{
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
        harp_dimension_type dimension_type = harp_dimension_time;

        if (harp_product_add_derived_variable(product, "index", NULL, NULL, 1, &dimension_type) != 0)
        {
            return -1;
        }
    }

    if (harp_operation_prepare_collocation_filter((harp_operation *)operation, product->source_product) != 0)
    {
        return -1;
    }

    return harp_product_apply_collocation_mask(product, operation->collocation_mask);
}

static int execute_bin_collocated(harp_product *product, harp_operation_bin_collocated *operation)
{
    harp_collocation_result *collocation_result = NULL;

    if (harp_collocation_result_read(operation->collocation_result, &collocation_result) != 0)
    {
        return -1;
    }

    if (operation->target_dataset == 'a')
    {
        harp_collocation_result_swap_datasets(collocation_result);
    }

    if (harp_product_bin_with_collocated_dataset(product, collocation_result) != 0)
    {
        harp_collocation_result_delete(collocation_result);
        return -1;
    }


    return 0;
}

static int execute_bin_spatial(harp_product *product, harp_operation_bin_spatial *operation)
{
    return harp_product_bin_spatial_full(product, operation->num_latitude_edges, operation->latitude_edges,
                                         operation->num_longitude_edges, operation->longitude_edges);
}

static int execute_bin_with_variables(harp_product *product, harp_operation_bin_with_variables *operation)
{
    return harp_product_bin_with_variable(product, operation->num_variables, (const char **)operation->variable_name);
}

static int execute_clamp(harp_product *product, harp_operation_clamp *operation)
{
    return harp_product_clamp_dimension(product, operation->dimension_type, operation->axis_variable_name,
                                        operation->axis_unit, operation->bounds[0], operation->bounds[1]);
}

static int execute_derive_variable(harp_product *product, harp_operation_derive_variable *operation)
{
    if (!operation->has_dimensions)
    {
        harp_variable *variable;

        /* we only perform unit and/or data type conversion; the variable should already be there */
        if (harp_product_get_variable_by_name(product, operation->variable_name, &variable) != 0)
        {
            return -1;
        }
        if (operation->unit != NULL)
        {
            if (harp_variable_has_unit(variable, operation->unit))
            {
                if (strcmp(variable->unit, operation->unit) != 0)
                {
                    /* make sure that the unit becomes syntactically the same */
                    if (harp_variable_set_unit(variable, operation->unit) != 0)
                    {
                        return -1;
                    }
                }
            }
            else
            {
                if (harp_variable_convert_unit(variable, operation->unit) != 0)
                {
                    return -1;
                }
            }
        }
        if (operation->has_data_type)
        {
            if (harp_variable_convert_data_type(variable, operation->data_type) != 0)
            {
                return -1;
            }
        }
        return 0;
    }
    if (operation->has_data_type)
    {
        return harp_product_add_derived_variable(product, operation->variable_name, &operation->data_type,
                                                 operation->unit, operation->num_dimensions, operation->dimension_type);
    }
    return harp_product_add_derived_variable(product, operation->variable_name, NULL, operation->unit,
                                             operation->num_dimensions, operation->dimension_type);
}

static int execute_derive_smoothed_column_collocated_dataset
    (harp_product *product, harp_operation_derive_smoothed_column_collocated_dataset *operation)
{
    harp_collocation_result *collocation_result = NULL;
    harp_variable *variable;

    if (harp_collocation_result_read(operation->collocation_result, &collocation_result) != 0)
    {
        return -1;
    }

    if (operation->target_dataset == 'a')
    {
        harp_collocation_result_swap_datasets(collocation_result);
    }
    if (harp_dataset_import(collocation_result->dataset_b, operation->dataset_dir, NULL) != 0)
    {
        harp_collocation_result_delete(collocation_result);
        return -1;
    }

    /* execute the operation */
    if (harp_product_get_smoothed_column_using_collocated_dataset(product, operation->variable_name, operation->unit,
                                                                  operation->num_dimensions, operation->dimension_type,
                                                                  operation->axis_variable_name, operation->axis_unit,
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

    return 0;
}

static int execute_derive_smoothed_column_collocated_product
    (harp_product *product, harp_operation_derive_smoothed_column_collocated_product *operation)
{
    harp_product *collocated_product = NULL;
    harp_variable *variable;

    if (harp_import(operation->filename, NULL, NULL, &collocated_product) != 0)
    {
        return -1;
    }

    /* execute the operation */
    if (harp_product_get_smoothed_column_using_collocated_product(product, operation->variable_name, operation->unit,
                                                                  operation->num_dimensions, operation->dimension_type,
                                                                  operation->axis_variable_name, operation->axis_unit,
                                                                  collocated_product, &variable) != 0)
    {
        harp_product_delete(collocated_product);
        return -1;
    }
    harp_product_delete(collocated_product);

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

    return 0;
}

static int execute_exclude_variable(harp_product *product, harp_operation_exclude_variable *operation)
{
    int i, j;

    for (i = product->num_variables - 1; i >= 0; i--)
    {
        for (j = 0; j < operation->num_variables; j++)
        {
            if (harp_match_wildcard(operation->variable_name[j], product->variable[i]->name))
            {
                if (harp_product_remove_variable(product, product->variable[i]) != 0)
                {
                    return -1;
                }
                break;
            }
        }
    }

    return 0;
}

static int execute_flatten(harp_product *product, harp_operation_flatten *operation)
{
    return harp_product_flatten_dimension(product, operation->dimension_type);
}

static int execute_keep_variable(harp_product *product, harp_operation_keep_variable *operation)
{
    int i, j;

    for (i = product->num_variables - 1; i >= 0; i--)
    {
        int included = 0;

        for (j = 0; j < operation->num_variables; j++)
        {
            if (harp_match_wildcard(operation->variable_name[j], product->variable[i]->name))
            {
                included = 1;
                break;
            }
        }

        if (!included)
        {
            if (harp_product_remove_variable(product, product->variable[i]) != 0)
            {
                return -1;
            }
        }
    }

    for (j = 0; j < operation->num_variables; j++)
    {
        if (strchr(operation->variable_name[j], '*') == NULL && strchr(operation->variable_name[j], '?') == NULL)
        {
            if (harp_product_get_variable_index_by_name(product, operation->variable_name[j], &i) != 0)
            {
                harp_set_error(HARP_ERROR_OPERATION, "cannot keep non-existent variable %s",
                               operation->variable_name[j]);
                return -1;
            }
        }
    }

    return 0;
}

static int execute_rebin(harp_product *product, harp_operation_rebin *operation)
{
    if (operation->axis_bounds_variable->dimension_type[0] == harp_dimension_independent)
    {
        harp_set_error(HARP_ERROR_OPERATION, "regridding of '%s' dimension not supported",
                       harp_get_dimension_type_name(operation->axis_bounds_variable->dimension_type[0]));
        return -1;
    }

    if (harp_product_rebin_with_axis_bounds_variable(product, operation->axis_bounds_variable) != 0)
    {
        return -1;
    }
    return 0;
}

static int execute_regrid(harp_product *product, harp_operation_regrid *operation)
{
    if (operation->axis_variable->dimension_type[0] == harp_dimension_independent)
    {
        harp_set_error(HARP_ERROR_OPERATION, "regridding of '%s' dimension not supported",
                       harp_get_dimension_type_name(operation->axis_variable->dimension_type[0]));
        return -1;
    }

    if (harp_product_regrid_with_axis_variable(product, operation->axis_variable, operation->axis_bounds_variable) != 0)
    {
        return -1;
    }
    return 0;
}

static int execute_regrid_collocated_dataset(harp_product *product, harp_operation_regrid_collocated_dataset *operation)
{
    harp_collocation_result *collocation_result = NULL;

    if (harp_collocation_result_read(operation->collocation_result, &collocation_result) != 0)
    {
        return -1;
    }

    if (operation->target_dataset == 'a')
    {
        harp_collocation_result_swap_datasets(collocation_result);
    }
    if (harp_dataset_import(collocation_result->dataset_b, operation->dataset_dir, NULL) != 0)
    {
        harp_collocation_result_delete(collocation_result);
        return -1;
    }

    if (harp_product_regrid_with_collocated_dataset(product, operation->dimension_type, operation->axis_variable_name,
                                                    operation->axis_unit, collocation_result) != 0)
    {
        harp_collocation_result_delete(collocation_result);
        return -1;
    }

    harp_collocation_result_delete(collocation_result);
    return 0;
}

static int execute_regrid_collocated_product(harp_product *product, harp_operation_regrid_collocated_product *operation)
{
    harp_product *collocated_product = NULL;

    if (harp_import(operation->filename, NULL, NULL, &collocated_product) != 0)
    {
        return -1;
    }

    if (harp_product_regrid_with_collocated_product(product, operation->dimension_type, operation->axis_variable_name,
                                                    operation->axis_unit, collocated_product) != 0)
    {
        harp_product_delete(collocated_product);
        return -1;
    }

    harp_product_delete(collocated_product);
    return 0;
}

static int execute_rename(harp_product *product, harp_operation_rename *operation)
{
    harp_variable *variable = NULL;
    char *new_name;

    if (!harp_product_has_variable(product, operation->variable_name) &&
        harp_product_has_variable(product, operation->new_variable_name))
    {
        /* if the source variable does not exists but a variable with the target name does
         * then we already have the required target state
         * we then just return success without doing anything
         */
        return 0;
    }

    if (harp_product_get_variable_by_name(product, operation->variable_name, &variable) != 0)
    {
        return -1;
    }

    new_name = strdup(operation->new_variable_name);
    if (new_name == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        return -1;
    }

    if (harp_product_detach_variable(product, variable) != 0)
    {
        free(new_name);
        return -1;
    }
    free(variable->name);
    variable->name = new_name;

    if (harp_product_add_variable(product, variable) != 0)
    {
        harp_variable_delete(variable);
        return -1;
    }

    return 0;
}

static int execute_set(harp_product *product, harp_operation_set *operation)
{
    (void)product;

    if (strcmp(operation->option, "afgl86") == 0)
    {
        if (strcmp(operation->value, "enabled") == 0)
        {
            harp_set_option_enable_aux_afgl86(1);
            harp_set_option_enable_aux_usstd76(0);
        }
        else if (strcmp(operation->value, "disabled") == 0)
        {
            harp_set_option_enable_aux_afgl86(0);
            harp_set_option_enable_aux_usstd76(0);
        }
        else if (strcmp(operation->value, "usstd76") == 0)
        {
            harp_set_option_enable_aux_afgl86(0);
            harp_set_option_enable_aux_usstd76(1);
        }
        else
        {
            harp_set_error(HARP_ERROR_OPERATION, "invalid value '%s' for option '%s'", operation->value,
                           operation->option);
            return -1;
        }
    }
    else if (strcmp(operation->option, "collocation_datetime") == 0)
    {
        if (strcmp(operation->value, "enabled") == 0)
        {
            harp_set_option_create_collocation_datetime(1);
        }
        else if (strcmp(operation->value, "disabled") == 0)
        {
            harp_set_option_create_collocation_datetime(0);
        }
        else
        {
            harp_set_error(HARP_ERROR_OPERATION, "invalid value '%s' for option '%s'", operation->value,
                           operation->option);
            return -1;
        }
    }
    else if (strcmp(operation->option, "propagate_uncertainty") == 0)
    {
        if (strcmp(operation->value, "uncorrelated") == 0)
        {
            harp_set_option_propagate_uncertainty(0);
        }
        else if (strcmp(operation->value, "correlated") == 0)
        {
            harp_set_option_propagate_uncertainty(1);
        }
        else
        {
            harp_set_error(HARP_ERROR_OPERATION, "invalid value '%s' for option '%s'", operation->value,
                           operation->option);
            return -1;
        }
    }
    else if (strcmp(operation->option, "regrid_out_of_bounds") == 0)
    {
        if (strcmp(operation->value, "nan") == 0)
        {
            harp_set_option_regrid_out_of_bounds(0);
        }
        else if (strcmp(operation->value, "edge") == 0)
        {
            harp_set_option_regrid_out_of_bounds(1);
        }
        else if (strcmp(operation->value, "extrapolate") == 0)
        {
            harp_set_option_regrid_out_of_bounds(2);
        }
        else
        {
            harp_set_error(HARP_ERROR_OPERATION, "invalid value '%s' for option '%s'", operation->value,
                           operation->option);
            return -1;
        }
    }
    else
    {
        harp_set_error(HARP_ERROR_OPERATION, "invalid option '%s'", operation->option);
        return -1;
    }

    return 0;
}

static int execute_smooth_collocated_dataset(harp_product *product, harp_operation_smooth_collocated_dataset *operation)
{
    harp_collocation_result *collocation_result = NULL;

    if (operation->dimension_type != harp_dimension_vertical)
    {
        harp_set_error(HARP_ERROR_OPERATION, "regridding of '%s' dimension not supported",
                       harp_get_dimension_type_name(operation->dimension_type));
        return -1;
    }

    if (harp_collocation_result_read(operation->collocation_result, &collocation_result) != 0)
    {
        return -1;
    }

    if (operation->target_dataset == 'a')
    {
        harp_collocation_result_swap_datasets(collocation_result);
    }
    if (harp_dataset_import(collocation_result->dataset_b, operation->dataset_dir, NULL) != 0)
    {
        harp_collocation_result_delete(collocation_result);
        return -1;
    }

    if (harp_product_smooth_vertical_with_collocated_dataset(product, operation->num_variables,
                                                             (const char **)operation->variable_name,
                                                             operation->axis_variable_name, operation->axis_unit,
                                                             collocation_result) != 0)
    {
        harp_collocation_result_delete(collocation_result);
        return -1;
    }

    harp_collocation_result_delete(collocation_result);
    return 0;
}

static int execute_smooth_collocated_product(harp_product *product, harp_operation_smooth_collocated_product *operation)
{
    harp_product *collocated_product = NULL;

    if (operation->dimension_type != harp_dimension_vertical)
    {
        harp_set_error(HARP_ERROR_OPERATION, "regridding of '%s' dimension not supported",
                       harp_get_dimension_type_name(operation->dimension_type));
        return -1;
    }

    if (harp_import(operation->filename, NULL, NULL, &collocated_product) != 0)
    {
        return -1;
    }

    if (harp_product_smooth_vertical_with_collocated_product(product, operation->num_variables,
                                                             (const char **)operation->variable_name,
                                                             operation->axis_variable_name, operation->axis_unit,
                                                             collocated_product) != 0)
    {
        harp_product_delete(collocated_product);
        return -1;
    }

    harp_product_delete(collocated_product);
    return 0;
}

static int execute_sort(harp_product *product, harp_operation_sort *operation)
{
    return harp_product_sort(product, operation->num_variables, (const char **)operation->variable_name);
}

static int execute_squash(harp_product *product, harp_operation_squash *operation)
{
    int i, k;

    for (i = 0; i < operation->num_variables; i++)
    {
        harp_variable *variable;

        if (harp_product_get_variable_by_name(product, operation->variable_name[i], &variable) != 0)
        {
            return -1;
        }

        for (k = variable->num_dimensions - 1; k >= 0; k--)
        {
            if (variable->dimension_type[k] == operation->dimension_type)
            {
                if (harp_variable_squash_dimension(variable, k) != 0)
                {
                    return -1;
                }
            }
        }
    }

    return 0;
}

static int execute_wrap(harp_product *product, harp_operation_wrap *operation)
{
    harp_variable *variable;
    long i;

    if (harp_product_get_variable_by_name(product, operation->variable_name, &variable) != 0)
    {
        return -1;
    }
    if (operation->unit != NULL)
    {
        if (harp_variable_convert_unit(variable, operation->unit) != 0)
        {
            return -1;
        }
    }
    else
    {
        if (harp_variable_convert_data_type(variable, harp_type_double) != 0)
        {
            return -1;
        }
    }

    for (i = 0; i < variable->num_elements; i++)
    {
        variable->data.double_data[i] = harp_wrap(variable->data.double_data[i], operation->min, operation->max);
    }

    variable->valid_min.double_data = operation->min;
    variable->valid_max.double_data = operation->max;

    return 0;
}

/* this will start with the operation at program->current_index */
int harp_product_execute_program(harp_product *product, harp_program *program)
{
    while (program->current_index < program->num_operations)
    {
        harp_operation *operation = program->operation[program->current_index];

        /* note that some consecutive filter operations can be executed together for optimization purposes */
        /* so the filter functions below may increase program->current_index itself */
        switch (operation->type)
        {
            case operation_bit_mask_filter:
            case operation_comparison_filter:
            case operation_longitude_range_filter:
            case operation_membership_filter:
            case operation_string_comparison_filter:
            case operation_string_membership_filter:
            case operation_valid_range_filter:
                if (execute_value_filter(product, program) != 0)
                {
                    return -1;
                }
                break;
            case operation_index_comparison_filter:
            case operation_index_membership_filter:
                if (execute_index_filter(product, program) != 0)
                {
                    return -1;
                }
                break;
            case operation_point_distance_filter:
            case operation_point_in_area_filter:
                if (execute_point_filter(product, program) != 0)
                {
                    return -1;
                }
                break;
            case operation_area_covers_area_filter:
            case operation_area_covers_point_filter:
            case operation_area_inside_area_filter:
            case operation_area_intersects_area_filter:
                if (execute_polygon_filter(product, program) != 0)
                {
                    return -1;
                }
                break;
            case operation_collocation_filter:
                if (execute_collocation_filter(product, (harp_operation_collocation_filter *)operation) != 0)
                {
                    return -1;
                }
                break;
            case operation_bin_collocated:
                if (execute_bin_collocated(product, (harp_operation_bin_collocated *)operation) != 0)
                {
                    return -1;
                }
                break;
            case operation_bin_full:
                if (harp_product_bin_full(product) != 0)
                {
                    return -1;
                }
                break;
            case operation_bin_spatial:
                if (execute_bin_spatial(product, (harp_operation_bin_spatial *)operation) != 0)
                {
                    return -1;
                }
                break;
            case operation_bin_with_variables:
                if (execute_bin_with_variables(product, (harp_operation_bin_with_variables *)operation) != 0)
                {
                    return -1;
                }
                break;
            case operation_clamp:
                if (execute_clamp(product, (harp_operation_clamp *)operation) != 0)
                {
                    return -1;
                }
                break;
            case operation_derive_variable:
                if (execute_derive_variable(product, (harp_operation_derive_variable *)operation) != 0)
                {
                    return -1;
                }
                break;
            case operation_derive_smoothed_column_collocated_dataset:
                if (execute_derive_smoothed_column_collocated_dataset
                    (product, (harp_operation_derive_smoothed_column_collocated_dataset *)operation) != 0)
                {
                    return -1;
                }
                break;
            case operation_derive_smoothed_column_collocated_product:
                if (execute_derive_smoothed_column_collocated_product
                    (product, (harp_operation_derive_smoothed_column_collocated_product *)operation) != 0)
                {
                    return -1;
                }
                break;
            case operation_exclude_variable:
                if (execute_exclude_variable(product, (harp_operation_exclude_variable *)operation) != 0)
                {
                    return -1;
                }
                break;
            case operation_flatten:
                if (execute_flatten(product, (harp_operation_flatten *)operation) != 0)
                {
                    return -1;
                }
                break;
            case operation_keep_variable:
                if (execute_keep_variable(product, (harp_operation_keep_variable *)operation) != 0)
                {
                    return -1;
                }
                break;
            case operation_rebin:
                if (execute_rebin(product, (harp_operation_rebin *)operation) != 0)
                {
                    return -1;
                }
                break;
            case operation_regrid:
                if (execute_regrid(product, (harp_operation_regrid *)operation) != 0)
                {
                    return -1;
                }
                break;
            case operation_regrid_collocated_dataset:
                if (execute_regrid_collocated_dataset(product, (harp_operation_regrid_collocated_dataset *)operation) !=
                    0)
                {
                    return -1;
                }
                break;
            case operation_regrid_collocated_product:
                if (execute_regrid_collocated_product(product, (harp_operation_regrid_collocated_product *)operation) !=
                    0)
                {
                    return -1;
                }
                break;
            case operation_rename:
                if (execute_rename(product, (harp_operation_rename *)operation) != 0)
                {
                    return -1;
                }
                break;
            case operation_set:
                if (execute_set(product, (harp_operation_set *)operation) != 0)
                {
                    return -1;
                }
                break;
            case operation_smooth_collocated_dataset:
                if (execute_smooth_collocated_dataset(product, (harp_operation_smooth_collocated_dataset *)operation) !=
                    0)
                {
                    return -1;
                }
                break;
            case operation_smooth_collocated_product:
                if (execute_smooth_collocated_product(product, (harp_operation_smooth_collocated_product *)operation) !=
                    0)
                {
                    return -1;
                }
                break;
            case operation_sort:
                if (execute_sort(product, (harp_operation_sort *)operation) != 0)
                {
                    return -1;
                }
                break;
            case operation_squash:
                if (execute_squash(product, (harp_operation_squash *)operation) != 0)
                {
                    return -1;
                }
                break;
            case operation_wrap:
                if (execute_wrap(product, (harp_operation_wrap *)operation) != 0)
                {
                    return -1;
                }
                break;
        }

        if (harp_product_is_empty(product))
        {
            /* don't perform any of the remaining actions; just return the empty product */
            return 0;
        }
        program->current_index++;
    }

    return 0;
}

/** \addtogroup harp_product
 * @{
 */

/**
 * Execute one or more operations on a product.
 *
 * if one of the operations results in an empty product then the function will immediately return with
 * the empty product (and return code 0) and will not execute any of the remaining actions anymore.
 * \param product Product that the operations should be executed on.
 * \param operations Operations to execute; should be specified as a semi-colon separated string of operations.
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
