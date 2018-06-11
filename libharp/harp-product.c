/*
 * Copyright (C) 2015-2018 S[&]T, The Netherlands.
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
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/** \defgroup harp_product HARP Products
 * The HARP Products module contains everything related to HARP products.
 *
 * The representation of a HARP product in C is a structure containing:
 * - an array of variables
 * - an array of dimension lengths for each dimension type (unvailable dimensions have length -1)
 * - the `source_product` global attribute (can be NULL)
 * - the `history` global attribute (can be NULL)
 *
 * Note that the `Conventions` global attribute is not included as this is automatically handled by the import/export
 * functions of HARP. Similar, the `datetime_start` and `datetime_stop` attributes are handled by the export function.
 * They are set to the minimum and maximum values of the variables `datetime`, `datetime_start` and `datetime_stop`
 * (if available).
 *
 * For each variable in the HARP product the dimensions need to match the length of their type as defined in the
 * dimension array of the HARP product (for all dimension types except 'independent').
 */

static int get_arguments(int argc, char *argv[], char **new_arguments)
{
    char *arguments = NULL;
    size_t length = 0;
    int add_quotes = 0;
    int i;

    /* Determine the stringlength */
    for (i = 1; i < argc; i++)
    {
        length += strlen(argv[i]);

        /* Add quotes for arguments that contain a whitespace, a semi-colon, an expression or a [unit] */
        add_quotes = (strstr(argv[i], " ") != NULL || strstr(argv[i], ";") != NULL || strstr(argv[i], "[") != NULL ||
                      strstr(argv[i], "]") != NULL || strstr(argv[i], "<") != NULL || strstr(argv[i], "!") != NULL ||
                      strstr(argv[i], "=") != NULL || strstr(argv[i], ">") != NULL);

        if (add_quotes)
        {
            length += 2;
        }
        if (i < argc - 1)
        {
            /* Add an extra whitespace */
            length++;
        }
    }
    /* Add an extra string termination character */
    length++;

    /* Combine the arguments (while skipping argv[0]) */
    arguments = calloc(length, sizeof(char));
    if (arguments == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       length * sizeof(char), __FILE__, __LINE__);
        return -1;
    }

    for (i = 1; i < argc; i++)
    {
        /* Add quotes for arguments that contain a whitespace, a semi-colon, an expression or a [unit] */
        add_quotes = (strstr(argv[i], " ") != NULL || strstr(argv[i], ";") != NULL || strstr(argv[i], "[") != NULL ||
                      strstr(argv[i], "]") != NULL || strstr(argv[i], "<") != NULL || strstr(argv[i], "!") != NULL ||
                      strstr(argv[i], "=") != NULL || strstr(argv[i], ">") != NULL);

        if (add_quotes)
        {
            strcat(arguments, "'");
            strcat(arguments, argv[i]);
            strcat(arguments, "'");
        }
        else
        {
            strcat(arguments, argv[i]);
        }
        if (i < argc - 1)
        {
            strcat(arguments, " ");
        }
    }

    *new_arguments = arguments;
    return 0;
}

static harp_variable *comparison_variable;

static int compare_variable_elements(const void *a, const void *b)
{
    long index_a = *(long *)a;
    long index_b = *(long *)b;

    switch (comparison_variable->data_type)
    {
        case harp_type_int8:
            if (comparison_variable->data.int8_data[index_a] < comparison_variable->data.int8_data[index_b])
            {
                return -1;
            }
            else if (comparison_variable->data.int8_data[index_a] > comparison_variable->data.int8_data[index_b])
            {
                return 1;
            }
            return 0;
        case harp_type_int16:
            if (comparison_variable->data.int16_data[index_a] < comparison_variable->data.int16_data[index_b])
            {
                return -1;
            }
            else if (comparison_variable->data.int16_data[index_a] > comparison_variable->data.int16_data[index_b])
            {
                return 1;
            }
            return 0;
        case harp_type_int32:
            if (comparison_variable->data.int32_data[index_a] < comparison_variable->data.int32_data[index_b])
            {
                return -1;
            }
            else if (comparison_variable->data.int32_data[index_a] > comparison_variable->data.int32_data[index_b])
            {
                return 1;
            }
            return 0;
        case harp_type_float:
            if (comparison_variable->data.float_data[index_a] < comparison_variable->data.float_data[index_b])
            {
                return -1;
            }
            else if (comparison_variable->data.float_data[index_a] > comparison_variable->data.float_data[index_b])
            {
                return 1;
            }
            return 0;
        case harp_type_double:
            if (comparison_variable->data.double_data[index_a] < comparison_variable->data.double_data[index_b])
            {
                return -1;
            }
            else if (comparison_variable->data.double_data[index_a] > comparison_variable->data.double_data[index_b])
            {
                return 1;
            }
            return 0;
        case harp_type_string:
            return strcmp(comparison_variable->data.string_data[index_a],
                          comparison_variable->data.string_data[index_b]);
    }
    assert(0);
    exit(1);
}

static void sync_product_dimensions_on_variable_add(harp_product *product, const harp_variable *variable)
{
    int i;

    for (i = 0; i < variable->num_dimensions; ++i)
    {
        harp_dimension_type dimension_type = variable->dimension_type[i];

        if (dimension_type != harp_dimension_independent && product->dimension[dimension_type] == 0)
        {
            product->dimension[dimension_type] = variable->dimension[i];
        }
    }
}

static void sync_product_dimensions_on_variable_remove(harp_product *product, const harp_variable *variable)
{
    int inactive_dimension_mask[HARP_NUM_DIM_TYPES] = { 0 };
    int num_inactive_dimensions;
    int i;
    int j;

    /* Update product dimensions. Dimensions that only the variable to be removed depends upon are set to zero. Other
     * dimensions are left untouched.
     */
    num_inactive_dimensions = 0;
    for (i = 0; i < variable->num_dimensions; i++)
    {
        harp_dimension_type dimension_type = variable->dimension_type[i];

        if (dimension_type != harp_dimension_independent && !inactive_dimension_mask[dimension_type])
        {
            /* For each dimension the variable to be removed depends upon, assume it is the only variable that depends
             * on that dimension. Mark such dimension as inactive.
             */
            assert(product->dimension[dimension_type] > 0);
            inactive_dimension_mask[dimension_type] = 1;
            num_inactive_dimensions++;
        }
    }

    if (num_inactive_dimensions == 0)
    {
        /* Removing the variable will not affect product dimensions. */
        return;
    }

    for (i = 0; i < product->num_variables; i++)
    {
        const harp_variable *other_variable = product->variable[i];

        if (other_variable == variable)
        {
            continue;
        }

        for (j = 0; j < other_variable->num_dimensions; j++)
        {
            harp_dimension_type dimension_type = other_variable->dimension_type[j];

            if (dimension_type != harp_dimension_independent && inactive_dimension_mask[dimension_type])
            {
                /* If the product contains a variable (other than the variable to be removed) that depends on a
                 * dimension marked as inactive, it follows that this dimension is in fact active.
                 */
                assert(other_variable->dimension[j] > 0);
                inactive_dimension_mask[dimension_type] = 0;
                num_inactive_dimensions--;
            }
        }

        if (num_inactive_dimensions == 0)
        {
            /* For all dimension the variable to be removed depends upon, another variable has been found that depends
             * on this dimension as well. Removing the variable therefore will not affect product dimensions.
             */
            break;
        }
    }

    /* Set each product dimension to zero for which no variable (other than the variable to be removed) was found that
     * depends on this dimension.
     */
    if (num_inactive_dimensions > 0)
    {
        for (i = 0; i < HARP_NUM_DIM_TYPES; i++)
        {
            if (inactive_dimension_mask[i])
            {
                product->dimension[i] = 0;
            }
        }
    }
}

/** Add a time dimension to each variable in the product.
 * If a variable in the product does not have a time dimension as first dimension then this dimension is introduced
 * and the data of the variable is replicated for each time element.
 * If the product was not time dependent (i.e. none of the variables were time dependent) then the product will be
 * made time dependent with time dimension length 1.
 * \param product Product that should be made time dependent.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
int harp_product_make_time_dependent(harp_product *product)
{
    int i;

    if (product->dimension[harp_dimension_time] == 0)
    {
        product->dimension[harp_dimension_time] = 1;
    }

    for (i = 0; i < product->num_variables; i++)
    {
        harp_variable *variable = product->variable[i];

        if (variable->num_dimensions == 0 || variable->dimension_type[0] != harp_dimension_time)
        {
            if (harp_variable_add_dimension(variable, 0, harp_dimension_time, product->dimension[harp_dimension_time])
                != 0)
            {
                return -1;
            }
        }
    }

    return 0;
}

int harp_product_rearrange_dimension(harp_product *product, harp_dimension_type dimension_type, long num_dim_elements,
                                     const long *dim_element_ids)
{
    int i;

    if (dimension_type == harp_dimension_independent)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "cannot rearrange '%s' dimension (%s:%u)",
                       harp_get_dimension_type_name(harp_dimension_independent), __FILE__, __LINE__);
        return -1;
    }

    if (product->dimension[dimension_type] == 0)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "product does not depend on dimension '%s' (%s:%u)",
                       harp_get_dimension_type_name(dimension_type), __FILE__, __LINE__);
        return -1;
    }

    if (dim_element_ids == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "dim_element_ids is NULL (%s:%u)", __FILE__, __LINE__);
        return -1;
    }

    if (num_dim_elements == 0)
    {
        /* If the new length of the dimension to be rearranged is zero, return an empty product. */
        harp_product_remove_all_variables(product);
        return 0;
    }

    for (i = 0; i < product->num_variables; i++)
    {
        harp_variable *variable = product->variable[i];
        int j;

        for (j = 0; j < variable->num_dimensions; j++)
        {
            if (variable->dimension_type[j] != dimension_type)
            {
                continue;
            }

            if (harp_variable_rearrange_dimension(variable, j, num_dim_elements, dim_element_ids) != 0)
            {
                return -1;
            }
        }
    }

    product->dimension[dimension_type] = num_dim_elements;

    return 0;
}

/* sort/filter the time dimension of \a product such that the index_variable content equals \a index */
int harp_product_filter_by_index(harp_product *product, const char *index_variable, long num_elements, int32_t *index)
{
    harp_variable *variable;
    long *dim_element_ids;
    long i, j;

    if (harp_product_get_variable_by_name(product, index_variable, &variable) != 0)
    {
        return -1;
    }

    dim_element_ids = malloc(num_elements * sizeof(long));
    if (dim_element_ids == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_elements * sizeof(long), __FILE__, __LINE__);
        return -1;
    }

    for (i = 0; i < num_elements; i++)
    {
        for (j = 0; j < variable->num_elements; j++)
        {
            if (index[i] == variable->data.int32_data[j])
            {
                break;
            }
        }
        if (j == variable->num_elements)
        {
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "index %ld not found in %s variable", (long)index[i],
                           index_variable);
            free(dim_element_ids);
            return -1;
        }
        dim_element_ids[i] = j;
    }

    if (harp_product_rearrange_dimension(product, harp_dimension_time, num_elements, dim_element_ids) != 0)
    {
        free(dim_element_ids);
        return -1;
    }

    free(dim_element_ids);

    return 0;
}

int harp_product_resize_dimension(harp_product *product, harp_dimension_type dimension_type, long length)
{
    int i;

    for (i = 0; i < product->num_variables; i++)
    {
        harp_variable *variable = product->variable[i];
        int j;

        for (j = 0; j < variable->num_dimensions; j++)
        {
            if (variable->dimension_type[j] == dimension_type)
            {
                if (harp_variable_resize_dimension(variable, j, length) != 0)
                {
                    return -1;
                }
            }
        }
    }
    product->dimension[dimension_type] = length;

    return 0;
}

/* Filter data of a variable in one dimension.
 * This function removes for all variables all elements in the given dimension where \a mask is set to 0.
 * The size of \a mask should correspond to the length of the given dimension.
 * It is an error to provide a list of \a mask values that only contain zeros (i.e. filter out all elements).
 *
 * Input:
 *    product        Pointer to product for which the variables should have their data filtered.
 *    dimension_type Dimension to filter.
 *    mask           An array containing true/false (1/0) values on whether to keep an element or not.
 */
int harp_product_filter_dimension(harp_product *product, harp_dimension_type dimension_type, const uint8_t *mask)
{
    long masked_dimension_length;
    int i;

    if (dimension_type == harp_dimension_independent)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "cannot filter '%s' dimension (%s:%u)",
                       harp_get_dimension_type_name(harp_dimension_independent), __FILE__, __LINE__);
        return -1;
    }

    if (product->dimension[dimension_type] == 0)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "product does not depend on dimension '%s'",
                       harp_get_dimension_type_name(dimension_type));
        return -1;
    }

    if (mask == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "mask is NULL (%s:%u)", __FILE__, __LINE__);
        return -1;
    }

    masked_dimension_length = 0;
    for (i = 0; i < product->dimension[dimension_type]; i++)
    {
        if (mask[i])
        {
            masked_dimension_length++;
        }
    }

    if (masked_dimension_length == 0)
    {
        /* If the new length of the dimension to be filtered is zero, return an empty product. */
        harp_product_remove_all_variables(product);
        return 0;
    }

    for (i = 0; i < product->num_variables; i++)
    {
        harp_variable *variable = product->variable[i];
        int j;

        for (j = 0; j < variable->num_dimensions; j++)
        {
            if (variable->dimension_type[j] != dimension_type)
            {
                continue;
            }

            if (harp_variable_filter_dimension(variable, j, mask) != 0)
            {
                return -1;
            }
        }
    }

    product->dimension[dimension_type] = masked_dimension_length;

    return 0;
}

/* Remove the specified dimension from the product.
 * All variables that depend on the specified dimension will be removed from the product.
 *
 * Input:
 *    product        Product to operate on.
 *    dimension_type The dimension that should be removed.
 */
int harp_product_remove_dimension(harp_product *product, harp_dimension_type dimension_type)
{
    int i;

    if (dimension_type == harp_dimension_independent)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "cannot remove '%s' dimension (%s:%u)",
                       harp_get_dimension_type_name(harp_dimension_independent), __FILE__, __LINE__);
        return -1;
    }

    if (product->dimension[dimension_type] == 0)
    {
        /* Product does not depend on dimensions to be removed, so nothing has to be done. */
        return 0;
    }

    for (i = product->num_variables - 1; i >= 0; i--)
    {
        harp_variable *variable = product->variable[i];

        if (!harp_variable_has_dimension_type(variable, dimension_type))
        {
            continue;
        }

        if (harp_product_remove_variable(product, variable) != 0)
        {
            return -1;
        }
    }
    assert(product->dimension[dimension_type] == 0);

    return 0;
}

/** Remove all variables from a product.
 * \param product Product from which variables should be removed.
 */
void harp_product_remove_all_variables(harp_product *product)
{
    if (product->variable != NULL)
    {
        int i;

        for (i = 0; i < product->num_variables; i++)
        {
            harp_variable_delete(product->variable[i]);
        }

        free(product->variable);
    }

    memset(product->dimension, 0, HARP_NUM_DIM_TYPES * sizeof(long));
    product->num_variables = 0;
    product->variable = NULL;
}

/**
 * Determine the datetime range covered by the product. Start and stop datetimes are returned as the (fractional) number
 * of days since 2000.
 *
 * \param  product        Product to compute the datetime range of.
 * \param  datetime_start Pointer to the location where the start datetime of the product will be stored. If NULL, the
 *   start datetime will not be stored.
 * \param  datetime_stop  Pointer to the location where the stop datetime of the product will be stored. If NULL, the
 *   stop datetime will not be stored.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
int harp_product_get_datetime_range(const harp_product *product, double *datetime_start, double *datetime_stop)
{
    harp_dimension_type dimension_type[1] = { harp_dimension_time };
    harp_variable *mid_datetime = NULL;
    harp_variable *datetime;
    long i;

    if (datetime_start != NULL)
    {
        double start = harp_plusinf();

        if (harp_product_get_derived_variable(product, "datetime_start", NULL, "days since 2000-01-01", 1,
                                              dimension_type, &datetime) != 0)
        {
            if (harp_product_get_derived_variable(product, "datetime", NULL, "days since 2000-01-01", 1, dimension_type,
                                                  &datetime) != 0)
            {
                return -1;
            }
            mid_datetime = datetime;
        }
        if (harp_variable_convert_data_type(datetime, harp_type_double) != 0)
        {
            harp_variable_delete(datetime);
            return -1;
        }

        for (i = 0; i < datetime->num_elements; i++)
        {
            const double value = datetime->data.double_data[i];

            if (harp_isnan(value) || value < datetime->valid_min.double_data || value > datetime->valid_max.double_data)
            {
                continue;
            }

            if (value < start)
            {
                start = value;
            }
        }

        if (harp_isplusinf(start) || start < datetime->valid_min.double_data || start > datetime->valid_max.double_data)
        {
            harp_variable_delete(datetime);
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "cannot determine valid start value for datetime range");
            return -1;
        }

        *datetime_start = start;

        if (mid_datetime == NULL)
        {
            harp_variable_delete(datetime);
        }
    }

    if (datetime_stop != NULL)
    {
        double stop = harp_mininf();

        if (harp_product_get_derived_variable(product, "datetime_stop", NULL, "days since 2000-01-01", 1,
                                              dimension_type, &datetime) != 0)
        {
            if (mid_datetime != NULL)
            {
                datetime = mid_datetime;
            }
            else if (harp_product_get_derived_variable(product, "datetime", NULL, "days since 2000-01-01", 1,
                                                       dimension_type, &datetime) != 0)
            {
                return -1;
            }
        }
        if (harp_variable_convert_data_type(datetime, harp_type_double) != 0)
        {
            harp_variable_delete(datetime);
            return -1;
        }

        for (i = 0; i < datetime->num_elements; i++)
        {
            const double value = datetime->data.double_data[i];

            if (harp_isnan(value) || value < datetime->valid_min.double_data || value > datetime->valid_max.double_data)
            {
                continue;
            }

            if (value > stop)
            {
                stop = value;
            }
        }

        if (harp_ismininf(stop) || stop < datetime->valid_min.double_data || stop > datetime->valid_max.double_data)
        {
            harp_variable_delete(datetime);
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "cannot determine valid stop value for datetime range");
            return -1;
        }

        *datetime_stop = stop;

        if (mid_datetime == NULL)
        {
            harp_variable_delete(datetime);
        }
    }

    if (mid_datetime != NULL)
    {
        harp_variable_delete(mid_datetime);
    }

    return 0;
}

int harp_product_get_storage_size(const harp_product *product, int with_attributes, int64_t *size)
{
    int64_t total_size = 0;
    int i;

    if (with_attributes)
    {
        if (product->source_product != NULL)
        {
            total_size += strlen(product->source_product);
        }
        if (product->history != NULL)
        {
            total_size += strlen(product->history);
        }
    }

    for (i = 0; i < product->num_variables; i++)
    {
        harp_variable *variable = product->variable[i];

        total_size += variable->num_elements * harp_get_size_for_type(variable->data_type);
        if (with_attributes)
        {
            if (variable->description != 0)
            {
                total_size += strlen(variable->description);
            }
            if (variable->unit != 0)
            {
                total_size += strlen(variable->unit);
            }
        }
    }

    *size = total_size;
    return 0;
}

/** \addtogroup harp_product
 * @{
 */

/** Create new product.
 * The product will be intialized with 0 variables and 0 attributes.
 * \param new_product Pointer to the C variable where the new HARP product will be stored.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_product_new(harp_product **new_product)
{
    harp_product *product;

    product = (harp_product *)malloc(sizeof(harp_product));
    if (product == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_product), __FILE__, __LINE__);
        return -1;
    }

    memset(product->dimension, 0, HARP_NUM_DIM_TYPES * sizeof(long));
    product->num_variables = 0;
    product->variable = NULL;
    product->source_product = NULL;
    product->history = NULL;

    *new_product = product;
    return 0;
}

/** Delete product.
 * Remove product and all attached variables and attributes.
 * \param product HARP product.
 */
LIBHARP_API void harp_product_delete(harp_product *product)
{
    if (product != NULL)
    {
        if (product->variable != NULL)
        {
            int i;

            for (i = 0; i < product->num_variables; i++)
            {
                harp_variable_delete(product->variable[i]);
            }

            free(product->variable);
        }

        if (product->source_product != NULL)
        {
            free(product->source_product);
        }

        if (product->history != NULL)
        {
            free(product->history);
        }

        free(product);
    }
}

/** Create a copy of a product.
 * The function will create a deep-copy of the given product, also creating copyies of all attributes and variables.
 * \param other_product Product that should be copied.
 * \param new_product Pointer to the variable where the new HARP product will be stored.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_product_copy(const harp_product *other_product, harp_product **new_product)
{
    harp_product *product;
    int i;

    if (harp_product_new(&product) != 0)
    {
        return -1;
    }

    for (i = 0; i < other_product->num_variables; i++)
    {
        harp_variable *variable;

        if (harp_variable_copy(other_product->variable[i], &variable) != 0)
        {
            harp_product_delete(product);
            return -1;
        }

        if (harp_product_add_variable(product, variable) != 0)
        {
            harp_variable_delete(variable);
            harp_product_delete(product);
            return -1;
        }
    }

    if (other_product->source_product != NULL)
    {
        product->source_product = strdup(other_product->source_product);
        if (product->source_product == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                           __LINE__);
            harp_product_delete(product);
            return -1;
        }
    }

    if (other_product->history != NULL)
    {
        product->history = strdup(other_product->history);
        if (product->history == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                           __LINE__);
            harp_product_delete(product);
            return -1;
        }
    }

    *new_product = product;
    return 0;
}

/** Append one product to another.
 * The 'index' variable, if present, will be removed.
 * All variables in both products will have a 'time' dimension introduced as first dimension.
 * Both products will have all non-time dimensions extended to the maximum of either product.
 * Any 'source_product' attribute for the first product will be removed.
 *
 * If you pass NULL for 'other_product', then 'product' will be updated as if it was the result of a merge
 * (i.e. remove 'index', add 'time' dimension, and remove 'source_product' attribute).
 * \param product Product to which data should be appended.
 * \param other_product (optional) Product that should be appended.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_product_append(harp_product *product, harp_product *other_product)
{
    harp_variable *variable;
    harp_variable *other_variable;
    harp_dimension_type dimension_type;
    int i;

    if (harp_product_has_variable(product, "index"))
    {
        if (harp_product_remove_variable_by_name(product, "index") != 0)
        {
            return -1;
        }
    }
    if (harp_product_make_time_dependent(product) != 0)
    {
        return -1;
    }
    if (product->source_product != NULL)
    {
        free(product->source_product);
        product->source_product = NULL;
    }

    if (other_product == NULL)
    {
        /* just update 'product' as if it was a result from a merge and return */
        return 0;
    }

    if (harp_product_has_variable(other_product, "index"))
    {
        if (harp_product_remove_variable_by_name(other_product, "index") != 0)
        {
            return -1;
        }
    }

    /* now check if both products have the same variables */
    if (product->num_variables != other_product->num_variables)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "products don't have the same number of variables");
        return -1;
    }
    for (i = 0; i < product->num_variables; i++)
    {
        variable = product->variable[i];
        if (!harp_product_has_variable(other_product, variable->name))
        {
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "products don't both have variable '%s'", variable->name);
            return -1;
        }
    }

    if (harp_product_make_time_dependent(other_product) != 0)
    {
        return -1;
    }

    /* align size of all non-time dimensions */
    for (dimension_type = 0; dimension_type < HARP_NUM_DIM_TYPES; dimension_type++)
    {
        if (dimension_type != harp_dimension_time)
        {
            if (product->dimension[dimension_type] > other_product->dimension[dimension_type])
            {
                if (harp_product_resize_dimension(other_product, dimension_type, product->dimension[dimension_type]) !=
                    0)
                {
                    return -1;
                }
            }
            else if (product->dimension[dimension_type] < other_product->dimension[dimension_type])
            {
                if (harp_product_resize_dimension(product, dimension_type, other_product->dimension[dimension_type]) !=
                    0)
                {
                    return -1;
                }
            }
        }
    }

    /* append all variables */
    for (i = 0; i < product->num_variables; i++)
    {
        variable = product->variable[i];
        if (harp_product_get_variable_by_name(other_product, variable->name, &other_variable) != 0)
        {
            assert(0);
            exit(1);
        }
        if (harp_variable_append(variable, other_variable) != 0)
        {
            return -1;
        }
    }
    product->dimension[harp_dimension_time] += other_product->dimension[harp_dimension_time];

    return 0;
}

/** Set the source product attribute of the specified product.
 * Stores the base name of \a product_path as the value of the source product attribute of the specified product.
 * The previous value (if any) will be freed.
 * The base name of the product path is the filename of the product without any directory name components.
 * \param product Product for which to set the source product attribute.
 * \param product_path Relative or absolute path to the product or just the product filename.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_product_set_source_product(harp_product *product, const char *product_path)
{
    char *source_product;

    if (product_path == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "product_path is NULL (%s:%u)", __FILE__, __LINE__);
        return -1;
    }

    source_product = strdup(harp_basename(product_path));
    if (source_product == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        return -1;
    }

    if (product->source_product != NULL)
    {
        free(product->source_product);
    }

    product->source_product = source_product;

    return 0;
}

/** Set the history attribute of the specified product.
 * Store a copy of \a history as the value of the history attribute of the specified product. The previous value (if
 * any) will be freed.
 * \param product Product for which to set the history attribute.
 * \param history New value for the history attribute.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_product_set_history(harp_product *product, const char *history)
{
    char *history_copy;

    if (history == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "history is NULL (%s:%u)", __FILE__, __LINE__);
        return -1;
    }

    history_copy = strdup(history);
    if (history_copy == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        return -1;
    }

    if (product->history != NULL)
    {
        free(product->history);
    }

    product->history = history_copy;

    return 0;
}

/** Add a variable to a product.
 * \note The memory management of the variable will be handled via the product after you have added the variable.
 * \param product Product to which the variable should be added.
 * \param variable Variable that should be added to the product.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_product_add_variable(harp_product *product, harp_variable *variable)
{
    int i;

    if (variable == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variable is NULL (%s:%u)", __FILE__, __LINE__);
        return -1;
    }

    if (harp_product_has_variable(product, variable->name))
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variable '%s' exists (%s:%u)", variable->name, __FILE__, __LINE__);
        return -1;
    }

    /* Verify that variable and product dimensions are compatible. */
    for (i = 0; i < variable->num_dimensions; i++)
    {
        harp_dimension_type dimension_type = variable->dimension_type[i];

        if (dimension_type == harp_dimension_independent || product->dimension[dimension_type] == 0)
        {
            continue;
        }

        if (variable->dimension[i] != product->dimension[dimension_type])
        {
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "dimension %d (of type '%s') of variable '%s' is incompatible"
                           " with product; variable = %ld, product = %ld (%s:%u)", i,
                           harp_get_dimension_type_name(dimension_type), variable->name, variable->dimension[i],
                           product->dimension[dimension_type], __FILE__, __LINE__);
            return -1;
        }
    }

    /* Add the variable to the product. */
    if (product->num_variables % BLOCK_SIZE == 0)
    {
        harp_variable **variable;

        variable = realloc(product->variable, (product->num_variables + BLOCK_SIZE) * sizeof(harp_variable *));
        if (variable == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           (product->num_variables + BLOCK_SIZE) * sizeof(harp_variable *), __FILE__, __LINE__);
            return -1;
        }

        product->variable = variable;
    }
    product->variable[product->num_variables] = variable;
    product->num_variables++;

    /* Update product dimensions. */
    sync_product_dimensions_on_variable_add(product, variable);

    return 0;
}

/** Detach a variable from a product.
 * Removes a variable from a product without deleting the variable itself. After detaching, the caller of the function
 * will be responsible for the further memory management of the variable.
 * \param product Product from which the variable should be detached.
 * \param variable Variable that should be detached.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_product_detach_variable(harp_product *product, const harp_variable *variable)
{
    int i;
    int j;

    if (variable == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variable is NULL (%s:%u)", __FILE__, __LINE__);
        return -1;
    }

    for (i = 0; i < product->num_variables; i++)
    {
        if (product->variable[i] == variable)
        {
            /* Update product dimensions. */
            sync_product_dimensions_on_variable_remove(product, variable);

            /* Remove the variable from the product. */
            for (j = i + 1; j < product->num_variables; j++)
            {
                product->variable[j - 1] = product->variable[j];
            }
            product->num_variables--;

            return 0;
        }
    }

    harp_set_error(HARP_ERROR_VARIABLE_NOT_FOUND, "could not find variable '%s'", variable->name);
    return -1;
}

/** Remove a variable from a product.
 * This function removes the specified variable from the product and then deletes the variable itself.
 * \param product Product from which the variable should be removed.
 * \param variable Variable that should be removed.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_product_remove_variable(harp_product *product, harp_variable *variable)
{
    if (variable == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variable is NULL (%s:%u)", __FILE__, __LINE__);
        return -1;
    }

    /* NB. Product dimensions will be updated by harp_product_detach_variable(). */
    if (harp_product_detach_variable(product, variable) != 0)
    {
        return -1;
    }
    harp_variable_delete(variable);

    return 0;
}

/** Remove a variable from a product using the name of the variable.
 * This function removes the variable with the specified name from the product and then deletes the variable itself.
 * \param product Product from which the variable should be removed.
 * \param name Name of the variable that should be removed.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_product_remove_variable_by_name(harp_product *product, const char *name)
{
    harp_variable *variable;

    if (harp_product_get_variable_by_name(product, name, &variable) != 0)
    {
        return -1;
    }
    if (harp_product_detach_variable(product, variable) != 0)
    {
        return -1;
    }
    harp_variable_delete(variable);

    return 0;
}

/** Replaces an existing variable with the one provided.
 * The product should already contain a variable with the same name as \a variable. This function searches in the list
 * of variables in the product for one with the same name, removes this variable and then adds the given \a variable in
 * its place. Note that if you try to replace a variable with itself the function does nothing (and returns success).
 * \param product Product in which the variable should be replaced.
 * \param variable Variable that should be used to replace an existing variable.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_product_replace_variable(harp_product *product, harp_variable *variable)
{
    int index;
    int i;

    if (variable == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variable is NULL (%s:%u)", __FILE__, __LINE__);
        return -1;
    }

    if (harp_product_get_variable_index_by_name(product, variable->name, &index) != 0)
    {
        harp_set_error(HARP_ERROR_VARIABLE_NOT_FOUND, "variable '%s' does not exist (%s:%u)", variable->name, __FILE__,
                       __LINE__);
        return -1;
    }

    if (product->variable[index] == variable)
    {
        /* Attempt to replace variable by itself. */
        return 0;
    }

    /* Verify that variable and product dimensions are compatible. */
    for (i = 0; i < variable->num_dimensions; i++)
    {
        harp_dimension_type dimension_type = variable->dimension_type[i];

        if (dimension_type == harp_dimension_independent || product->dimension[dimension_type] == 0)
        {
            continue;
        }

        if (variable->dimension[i] != product->dimension[dimension_type])
        {
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "dimension %d (of type '%s') of variable '%s' is incompatible"
                           " with product; variable = %ld, product = %ld (%s:%u)", i,
                           harp_get_dimension_type_name(dimension_type), variable->name, variable->dimension[i],
                           product->dimension[dimension_type], __FILE__, __LINE__);
            return -1;
        }
    }

    /* Replace variable. */
    sync_product_dimensions_on_variable_remove(product, product->variable[index]);
    harp_variable_delete(product->variable[index]);

    product->variable[index] = variable;
    sync_product_dimensions_on_variable_add(product, product->variable[index]);

    return 0;
}

/** Test if product contains a variable with the specified name.
 * \param  product Product to search.
 * \param  name Name of the variable to search for.
 * \return
 *   \arg \c 0, Product does not contain a variable of the specified name.
 *   \arg \c 1, Product contains a variable of the specified name.
 */
LIBHARP_API int harp_product_has_variable(const harp_product *product, const char *name)
{
    int i;

    if (name == NULL)
    {
        return 0;
    }

    for (i = 0; i < product->num_variables; i++)
    {
        if (strcmp(product->variable[i]->name, name) == 0)
        {
            return 1;
        }
    }

    return 0;
}

/** Find variable with a given name for a product.
 * If no variable with the given name can be found an error is returned.
 * \param product Product in which to find the variable.
 * \param name Name of the variable.
 * \param variable Pointer to the C variable where the found HARP variable will be stored.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_product_get_variable_by_name(const harp_product *product, const char *name,
                                                  harp_variable **variable)
{
    int i;

    if (name == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "name is NULL (%s:%u)", __FILE__, __LINE__);
        return -1;
    }

    if (variable == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variable is NULL (%s:%u)", __FILE__, __LINE__);
        return -1;
    }

    for (i = 0; i < product->num_variables; i++)
    {
        if (strcmp(product->variable[i]->name, name) == 0)
        {
            *variable = product->variable[i];
            return 0;
        }
    }

    harp_set_error(HARP_ERROR_VARIABLE_NOT_FOUND, "variable '%s' does not exist", name);
    return -1;
}

/** Find index of variable with a given name for a product.
 * If no variable with the given name can be found an error is returned.
 * \param product Product in which the find the variable.
 * \param name Name of the variable.
 * \param index Pointer to the C variable where the index in the HARP variables list for the product is returned.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_product_get_variable_index_by_name(const harp_product *product, const char *name, int *index)
{
    int i;

    if (name == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "name is NULL (%s:%u)", __FILE__, __LINE__);
        return -1;
    }

    if (index == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "index is NULL (%s:%u)", __FILE__, __LINE__);
        return -1;
    }

    for (i = 0; i < product->num_variables; i++)
    {
        if (strcmp(product->variable[i]->name, name) == 0)
        {
            *index = i;
            return 0;
        }
    }

    harp_set_error(HARP_ERROR_VARIABLE_NOT_FOUND, "variable '%s' does not exist", name);
    return -1;
}

/** Determine whether all variables in a product have at least one element.
 * If at least one variable has 0 elements or if the product has 0 variables the function returns 1, and 0 otherwise.
 * \param product Product to check for empty data.
 * \return
 *   \arg \c 0, The product does not contain empty data.
 *   \arg \c 1, The product contains 0 variables or at least one variable has 0 elements.
 */
LIBHARP_API int harp_product_is_empty(const harp_product *product)
{
    int i;

    for (i = 0; i < product->num_variables; i++)
    {
        if (product->variable[i]->num_elements == 0)
        {
            /* If at least one variable has no data, return true. */
            return 1;
        }
    }

    /* Do we have any variables at all? */
    return (product->num_variables == 0);
}

/** Update the history attribute in the product based on the command line parameters.
 * This function will extend the existing product history metadata element with a line containing the current UTC time,
 * the HARP version, and the call that was used to run this program.
 * The command line execution call is constructed based on the \a argc and \a argv arguments.
 * The format of the added line is: YYYY-MM-DDThh:mm:ssZ [harp-x.y] args ....
 * \param product Product for which the history metada should be extended.
 * \param executable Name of the command line executable (this value is used instead of argv[0]).
 * \param argc Variable as passed by main().
 * \param argv Variable as passed by main().
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_product_update_history(harp_product *product, const char *executable, int argc, char *argv[])
{
    time_t now;
    struct tm *tmnow;
    char *arguments = NULL;
    char *buffer = NULL;
    size_t length;

    if (executable == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "executable is NULL");
        return -1;
    }

    /* Derive the arguments as a string */
    if (get_arguments(argc, argv, &arguments) != 0)
    {
        return -1;
    }

    /* get current UTC time */
    now = time(NULL);
    tmnow = gmtime(&now);
    if (tmnow == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_DATETIME, "could not get current time (%s)", strerror(errno));
        return -1;
    }

    /* Update the history attribute */
    /* Reserve length for 'YYYY-MM-DDThh:mm:ssZ [harp-<version>] ' and string termination character */
    length = 30 + strlen(HARP_VERSION);
    /* Add the length of the executable, a whitespace, and the arguments */
    length += strlen(executable) + 1 + strlen(arguments);
    if (product->history != NULL)
    {
        /* also add newline character */
        length += strlen(product->history) + 1;
    }

    /* Create the new history string */
    buffer = malloc(length * sizeof(char));
    if (buffer == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       length * sizeof(char), __FILE__, __LINE__);
        free(arguments);
        return -1;
    }
    buffer[0] = '\0';
    if (product->history != NULL)
    {
        strcat(buffer, product->history);
        strcat(buffer, "\n");
        free(product->history);
        product->history = NULL;
    }
    sprintf(&buffer[strlen(buffer)], "%04d-%02d-%02dT%02d:%02d:%02dZ [harp-%s] ", tmnow->tm_year + 1900,
            tmnow->tm_mon + 1, tmnow->tm_mday, tmnow->tm_hour, tmnow->tm_min, tmnow->tm_sec, HARP_VERSION);
    strcat(buffer, executable);
    strcat(buffer, " ");
    strcat(buffer, arguments);

    product->history = buffer;

    free(arguments);

    return 0;
}

/** Verify that a product is internally consistent and complies with conventions.
 * \param product Product to verify.
 * \return
 *   \arg \c 0, Product verified successfully.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_product_verify(const harp_product *product)
{
    hashtable *variable_names;
    int i;

    if (product == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "product is NULL");
        return -1;
    }

    for (i = 0; i < HARP_NUM_DIM_TYPES; i++)
    {
        if (product->dimension[i] < 0)
        {
            harp_set_error(HARP_ERROR_INVALID_PRODUCT, "dimension of type '%s' has invalid length %ld",
                           harp_get_dimension_type_name((harp_dimension_type)i), product->dimension[i]);
            return -1;
        }
    }

    if (product->num_variables < 0)
    {
        harp_set_error(HARP_ERROR_INVALID_PRODUCT, "invalid number of variables %d", product->num_variables);
        return -1;
    }

    if (product->num_variables > 0 && product->variable == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_PRODUCT, "number of variables is > 0, but product contains no variables");
        return -1;
    }

    /* Make sure that units module gets initialized so we report on initialization errors early and separately */
    if (!harp_unit_is_valid(""))
    {
        return -1;
    }

    /* Check variables. */
    for (i = 0; i < product->num_variables; i++)
    {
        const harp_variable *variable = product->variable[i];

        if (variable == NULL)
        {
            harp_set_error(HARP_ERROR_INVALID_PRODUCT, "variable at index %d undefined", i);
            return -1;
        }

        if (harp_variable_verify(variable) != 0)
        {
            if (variable->name == NULL)
            {
                harp_add_error_message(" (variable at index %d)", i);
            }
            else
            {
                harp_add_error_message(" (variable '%s')", variable->name);
            }

            return -1;
        }
    }

    /* Check consistency of dimensions between product and variables. */
    for (i = 0; i < product->num_variables; i++)
    {
        const harp_variable *variable = product->variable[i];
        int j;

        for (j = 0; j < variable->num_dimensions; j++)
        {
            if (variable->dimension_type[j] == harp_dimension_independent)
            {
                continue;
            }

            if (variable->dimension[j] != product->dimension[variable->dimension_type[j]])
            {
                harp_set_error(HARP_ERROR_INVALID_PRODUCT, "length %ld of dimension of type '%s' at index %d of "
                               "variable '%s' does not match length %ld of product dimension of type '%s'",
                               variable->dimension[j], harp_get_dimension_type_name(variable->dimension_type[j]), j,
                               variable->name, product->dimension[variable->dimension_type[j]],
                               harp_get_dimension_type_name(variable->dimension_type[j]));
                return -1;
            }
        }
    }

    variable_names = hashtable_new(1);
    if (variable_names == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate hashtable) (%s:%u)", __FILE__,
                       __LINE__);
        return -1;
    }

    for (i = 0; i < product->num_variables; i++)
    {
        const harp_variable *variable = product->variable[i];

        if (hashtable_add_name(variable_names, variable->name) != 0)
        {
            harp_set_error(HARP_ERROR_INVALID_PRODUCT, "variable name '%s' is not unique", variable->name);
            hashtable_delete(variable_names);
            return -1;
        }
    }

    hashtable_delete(variable_names);

    return 0;
}

/** Print a harp_product struct using the specified print function.
 * \param product Product to print.
 * \param show_attributes Whether or not to print the attributes of variables.
 * \param show_data Whether or not to print the data arrays of the variables
 * \param print Print function to use
 */
LIBHARP_API void harp_product_print(const harp_product *product, int show_attributes, int show_data,
                                    int (*print) (const char *, ...))
{
    int i;

    if (product == NULL)
    {
        print("NULL\n");
        return;
    }
    print("dimensions:\n");
    for (i = 0; i < HARP_NUM_DIM_TYPES; i++)
    {
        if (product->dimension[i] > 0)
        {
            print("    %s = %ld\n", harp_get_dimension_type_name(i), product->dimension[i]);
        }
    }
    print("\n");

    print("attributes:\n");
    if (product->source_product != NULL)
    {
        print("    source_product = \"%s\"\n", product->source_product);
    }
    if (product->history != NULL)
    {
        print("    history = \"%s\"\n", product->history);
    }
    print("\n");

    print("variables:\n");
    for (i = 0; i < product->num_variables; i++)
    {
        harp_variable_print(product->variable[i], show_attributes, print);
    }
    print("\n");

    if (show_data)
    {
        print("data:\n");
        for (i = 0; i < product->num_variables; i++)
        {
            harp_variable_print_data(product->variable[i], print);
        }
    }
}

/** Collapse a given dimension into the time dimension
 *
 * Flattening a product for a certain dimension collapses the dimension into the time dimension (i.e. the time
 * dimension and the provided dimension are flattened together).
 * For instance, if a product contains a variable with [num_time,num_longitude,num_latitudes,num_vertical] as
 * dimensions, then flattening for the vertical dimension will result in a variable with
 * [num_time*num_vertical,num_longitudes,num_latitudes] as dimensions.
 *
 * The end result of this function is the time dimension will have grown by a factor equal to the length of the given
 * dimension type and that none of the variables in the product will depend on the given dimension type anymore.
 *
 * Any variables that depend more than once on the given dimension type will be removed from the product.
 * If the length of the flattend dimensions does not equal 1 then the index and collocation_index variables will be
 * removed if present.
 * Variables that had the given dimension type but were time independent are first made time dependent before
 * flattening the dimension.
 *
 * Independent dimensions cannot be flattened.
 * \param product HARP product.
 * \param dimension_type Dimension to use for the flattening.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_product_flatten_dimension(harp_product *product, harp_dimension_type dimension_type)
{
    harp_variable *var;
    long dim_length = product->dimension[dimension_type];
    int i;

    if (dimension_type == harp_dimension_independent)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "cannot flatten independent dimension");
        return -1;
    }
    if (dim_length == 0 || dimension_type == harp_dimension_time)
    {
        return 0;
    }

    if (dim_length != 1)
    {
        /* remove index and collocation_index variables if they exist */
        if (harp_product_has_variable(product, "index"))
        {
            if (harp_product_remove_variable_by_name(product, "index") != 0)
            {
                return -1;
            }
        }
        if (harp_product_has_variable(product, "collocation_index"))
        {
            if (harp_product_remove_variable_by_name(product, "collocation_index") != 0)
            {
                return -1;
            }
        }
    }

    for (i = product->num_variables - 1; i >= 0; i--)
    {
        int order[HARP_MAX_NUM_DIMS];
        int dim_index = -1;
        int count = 0;
        int j;

        var = product->variable[i];

        for (j = 0; j < var->num_dimensions; j++)
        {
            if (var->dimension_type[j] == dimension_type)
            {
                count++;
                dim_index = j;
            }
        }

        if (count == 0)
        {
            if (var->num_dimensions > 0 && var->dimension_type[0] == harp_dimension_time)
            {
                /* add the dimension to be flattened in the right place; this effectively extends time appropriately */
                if (harp_variable_add_dimension(var, 1, dimension_type, dim_length) != 0)
                {
                    return -1;
                }
                dim_index = 1;
                count = 1;
            }
            else
            {
                /* skip variables that don't depend on the relevant dim AND not on time */
                continue;
            }
        }
        else if (count >= 2)
        {
            /* remove variables that depend more than once on the specified dimension */
            harp_product_remove_variable(product, var);
            continue;
        }

        /* the variable must be time-dependend */
        if (var->dimension_type[0] != harp_dimension_time)
        {
            if (product->dimension[harp_dimension_time] == 0)
            {
                product->dimension[harp_dimension_time] = 1;
            }
            if (harp_variable_add_dimension(var, 0, harp_dimension_time, product->dimension[harp_dimension_time]))
            {
                return -1;
            }

            dim_index++;
        }

        /* derive the new order of dimensions, splicing in dim_index at position 1 *if necessary* */
        if (dim_index != 1)
        {
            order[0] = 0;
            order[1] = dim_index;
            for (j = 2; j < var->num_dimensions; j++)
            {
                if (j <= dim_index)
                {
                    order[j] = j - 1;
                }
                else
                {
                    order[j] = j;
                }
            }

            /* reorder dimensions */
            if (harp_array_transpose(var->data_type, var->num_dimensions, var->dimension, order, var->data) != 0)
            {
                return -1;
            }
        }

        /* update the dimension info */
        var->dimension[harp_dimension_time] *= var->dimension[dim_index];
        for (j = dim_index; j < var->num_dimensions - 1; j++)
        {
            var->dimension[j] = var->dimension[j + 1];
            var->dimension_type[j] = var->dimension_type[j + 1];
        }

        var->num_dimensions--;
    }

    /* update the dimension info of the product */
    product->dimension[harp_dimension_time] *= dim_length;
    product->dimension[dimension_type] = 0;

    return 0;
}

/** Reorder a dimension for all variables in a product such that the variable with the given name ends up sorted.
 *
 * A variable for the provided variable_name should exist in the product and this variable should be a one dimensional
 * variable. The dimension that will be reordered is this single dimension of the referenced variable.
 *
 * \param product HARP product
 * \param variable_name Name of the variable to should end up sorted
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_product_sort(harp_product *product, const char *variable_name)
{
    long num_elements;
    long *dim_element_ids;
    long i;

    if (harp_product_get_variable_by_name(product, variable_name, &comparison_variable) != 0)
    {
        return -1;
    }
    if (comparison_variable->num_dimensions != 1)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variable for sorting should be a one dimensional array");
        return -1;
    }
    if (comparison_variable->dimension_type[0] == harp_dimension_independent)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "cannot sort independent dimension");
        return -1;
    }
    num_elements = comparison_variable->num_elements;

    dim_element_ids = malloc(num_elements * sizeof(long));
    if (dim_element_ids == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_elements * sizeof(long), __FILE__, __LINE__);
        return -1;
    }
    for (i = 0; i < num_elements; i++)
    {
        dim_element_ids[i] = i;
    }

    qsort(dim_element_ids, num_elements, sizeof(long), compare_variable_elements);

    if (harp_product_rearrange_dimension(product, comparison_variable->dimension_type[0], num_elements, dim_element_ids)
        != 0)
    {
        free(dim_element_ids);
        return -1;
    }

    free(dim_element_ids);

    return 0;
}

/** @} */
