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

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define MAX_NAME_LENGTH 128

typedef enum binning_type_enum
{
    binning_skip,
    binning_remove,
    binning_average
} binning_type;


static binning_type get_binning_type(harp_variable *variable)
{
    int i;

    /* any variable with a time dimension that is not the first dimension gets removed */
    for (i = 1; i < variable->num_dimensions; i++)
    {
        if (variable->dimension[i] == harp_dimension_time)
        {
            return binning_remove;
        }
    }

    /* we only bin variables with a time dimension */
    if (variable->num_dimensions == 0 || variable->dimension_type[0] != harp_dimension_time)
    {
        return binning_skip;
    }

    /* we can't bin string values */
    if (variable->data_type == harp_type_string)
    {
        return binning_remove;
    }

    /* we can't bin values that have no unit */
    if (variable->unit == NULL)
    {
        return binning_remove;
    }

    /* uncertainty propagation needs to be handled differently (remove for now) */
    if (strstr(variable->name, "_uncertainty") != NULL)
    {
        return binning_remove;
    }

    /* we can't bin averaging kernels */
    if (strstr(variable->name, "_avk") != NULL)
    {
        return binning_remove;
    }

    /* we can't bin latitude/longitude bounds if they define an area */
    if (strcmp(variable->name, "latitude_bounds") == 0 || strcmp(variable->name, "longitude_bounds") == 0)
    {
        if (variable->num_dimensions > 0 &&
            variable->dimension_type[variable->num_dimensions - 1] == harp_dimension_independent &&
            variable->dimension[variable->num_dimensions - 1] > 2)
        {
            return binning_remove;
        }
    }

    /* use average by default */
    return binning_average;
}

static int filter_binable_variables(harp_product *product)
{
    int i;

    for (i = product->num_variables - 1; i >= 0; i--)
    {
        if (get_binning_type(product->variable[i]) == binning_remove)
        {
            if (harp_product_remove_variable(product, product->variable[i]) != 0)
            {
                return -1;
            }
        }
    }

    return 0;
}


/** \addtogroup harp_product
 * @{
 */

/** Bin the product's variables.
 * This will bin all variables in the time dimension. Each time sample will be put in the bin defined by bin_index.
 * All variables with a time dimension will then be resampled using these bins.
 * The resulting value for each variable will be the average of all values for the bin.
 * Variables with multiple dimensions will have all elements in the sub dimensions averaged on an element by element
 * basis.
 *
 * Variables that have a time dimension but no unit (or using a string data type) will be removed.
 *
 * All variables that are binned are converted to a double data type.
 * Bins that have no samples will end up with a NaN value.
 *
 * \param product Product to regrid.
 * \param num_bins Number of target bins.
 * \param num_elements Length of bin_index array (should equal the length of the time dimension)
 * \param bin_index Array of target bin index numbers (0 .. num_bins-1) for each sample in the time dimension.
 *
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_product_bin(harp_product *product, long num_bins, long num_elements, long *bin_index)
{
    long *index;
    long *count;
    long i, j, k;

    if (num_elements != product->dimension[harp_dimension_time])
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "num_elements (%ld) does not match time dimension length (%ld) "
                       "(%s:%u)", num_elements, product->dimension[harp_dimension_time], __FILE__, __LINE__);
        return -1;
    }

    for (i = 0; i < num_elements; i++)
    {
        if (bin_index[i] < 0 || bin_index[i] >= num_bins)
        {
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "bin_index[%ld] (%ld) should be in the range [0..%ld) (%s:%u)",
                           i, bin_index[i], num_bins, __FILE__, __LINE__);
            return -1;
        }
    }

    if (filter_binable_variables(product) != 0)
    {
        return -1;
    }

    index = malloc(num_bins * sizeof(long));
    if (index == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_bins * sizeof(long), __FILE__, __LINE__);
        return -1;
    }
    count = malloc(num_bins * sizeof(long));
    if (count == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_bins * sizeof(long), __FILE__, __LINE__);
        free(index);
        return -1;
    }

    /* for each bin, store the index of the first sample that contributes to the bin */
    for (i = 0; i < num_bins; i++)
    {
        index[i] = 0;
        count[i] = 0;
    }
    for (i = 0; i < num_elements; i++)
    {
        if (count[bin_index[i]] == 0)
        {
            index[bin_index[i]] = i;
        }
        count[bin_index[i]]++;
    }

    for (k = 0; k < product->num_variables; k++)
    {
        harp_variable *variable;
        long num_sub_elements;
        binning_type type;

        variable = product->variable[k];

        type = get_binning_type(variable);

        assert(type != binning_remove);
        if (type == binning_skip)
        {
            continue;
        }

        assert(variable->dimension[0] == num_elements);
        num_sub_elements = variable->num_elements / num_elements;

        if (harp_variable_convert_data_type(variable, harp_type_double) != 0)
        {
            free(index);
            free(count);
            return -1;
        }

        /* TODO: use special approach for index averaging (take median) */
        /* TODO: use special approach for datetime (start/stop) averaging (create start/stop bounds) */
        /* TODO: use special approach for lat/lon (bounds) averaging (use spatial averaging + bounds) */

        /* first sum up all values of a bin into the location of the first sample */
        for (i = 0; i < num_elements; i++)
        {
            long target_index = index[bin_index[i]];

            if (target_index != i)
            {
                for (j = 0; j < num_sub_elements; j++)
                {
                    variable->data.double_data[target_index * num_sub_elements + j] +=
                        variable->data.double_data[i * num_sub_elements + j];
                }
            }
        }

        /* then divide by the number of elements in the bin */
        for (i = 0; i < num_bins; i++)
        {
            long target_index = index[i];

            if (count[i] > 1)
            {
                for (j = 0; j < num_sub_elements; j++)
                {
                    variable->data.double_data[target_index * num_sub_elements + j] /= count[i];
                }
            }
        }

        /* resample the time dimension to the target bins */
        if (harp_variable_rearrange_dimension(variable, 0, num_bins, index) != 0)
        {
            free(index);
            free(count);
            return -1;
        }

        /* set all empty bins to NaN */
        for (i = 0; i < num_bins; i++)
        {
            if (count[i] == 0)
            {
                double nan_value = harp_nan();

                for (j = 0; j < num_sub_elements; j++)
                {
                    variable->data.double_data[i * num_sub_elements + j] = nan_value;
                }
            }
        }
    }

    product->dimension[harp_dimension_time] = num_bins;

    free(index);
    free(count);

    return 0;
}

/**
 * @}
 */

/** Bin the product's variables (from dataset a in the collocation result) such that all pairs that have the same
 * item in dataset b are averaged together.
 *
 * \param product Product to regrid.
 * \param collocation_result The collocation result containing the list of matching pairs.
 *
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
int harp_product_bin_with_collocated_dataset(harp_product *product, harp_collocation_result *collocation_result)
{
    harp_collocation_result *filtered_collocation_result = NULL;
    harp_variable *collocation_index = NULL;
    long *index;        /* contains index of first sample for each bin */
    long *bin_index;
    long num_bins;
    long i, j;

    /* Get the source product's collocation index variable */
    if (harp_product_get_variable_by_name(product, "collocation_index", &collocation_index) != 0)
    {
        return -1;
    }

    /* copy the collocation result for filtering */
    if (harp_collocation_result_shallow_copy(collocation_result, &filtered_collocation_result) != 0)
    {
        return -1;
    }

    /* Reduce the collocation result to only pairs that include the source product */
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

    index = malloc(collocation_index->num_elements * sizeof(long));
    if (index == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       collocation_index->num_elements * sizeof(long), __FILE__, __LINE__);
        harp_collocation_result_shallow_delete(filtered_collocation_result);
        return -1;
    }
    bin_index = malloc(collocation_index->num_elements * sizeof(long));
    if (bin_index == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       collocation_index->num_elements * sizeof(long), __FILE__, __LINE__);
        harp_collocation_result_shallow_delete(filtered_collocation_result);
        free(index);
        return -1;
    }

    num_bins = 0;
    for (i = 0; i < collocation_index->num_elements; i++)
    {
        for (j = 0; j < num_bins; j++)
        {
            if (filtered_collocation_result->pair[index[j]]->product_index_b ==
                filtered_collocation_result->pair[i]->product_index_b &&
                filtered_collocation_result->pair[index[j]]->sample_index_b ==
                filtered_collocation_result->pair[i]->sample_index_b)
            {
                break;
            }
        }
        if (j == num_bins)
        {
            /* add new value to bin */
            index[num_bins] = i;
            num_bins++;
        }
        bin_index[i] = j;
    }

    free(index);

    if (harp_product_bin(product, num_bins, collocation_index->num_elements, bin_index) != 0)
    {
        harp_collocation_result_shallow_delete(filtered_collocation_result);
        free(bin_index);
        return -1;
    }

    /* cleanup */
    harp_collocation_result_shallow_delete(filtered_collocation_result);
    free(bin_index);

    return 0;
}

/** Bin the product's variables such that all samples that have the same value in the given variable are averaged
 * together.
 *
 * \param product Product to regrid.
 * \param variable_name Name of the variable that defines the bins (based on equal value).
 *
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
int harp_product_bin_with_variable(harp_product *product, const char *variable_name)
{
    harp_variable *variable;
    long *index;        /* contains index of first sample for each bin */
    long *bin_index;
    long num_elements;
    long num_bins;
    long i, j;

    if (harp_product_get_variable_by_name(product, variable_name, &variable) != 0)
    {
        return -1;
    }
    if (variable->num_dimensions != 1 || variable->dimension_type[0] != harp_dimension_time)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variable '%s' should be one dimensional and depend on time to be "
                       "used for binning", variable_name);
        return -1;
    }

    num_elements = variable->num_elements;

    index = malloc(num_elements * sizeof(long));
    if (index == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_elements * sizeof(long), __FILE__, __LINE__);
        return -1;
    }
    bin_index = malloc(num_elements * sizeof(long));
    if (bin_index == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_elements * sizeof(long), __FILE__, __LINE__);
        free(index);
        return -1;
    }

    num_bins = 0;
    for (i = 0; i < num_elements; i++)
    {
        int check_nan = 0;

        if (variable->data_type == harp_type_float)
        {
            check_nan = harp_isnan(variable->data.float_data[i]);
        }
        else if (variable->data_type == harp_type_double)
        {
            check_nan = harp_isnan(variable->data.double_data[i]);
        }
        for (j = 0; j < num_bins; j++)
        {
            int equal = 0;

            switch (variable->data_type)
            {
                case harp_type_int8:
                    equal = variable->data.int8_data[index[j]] == variable->data.int8_data[i];
                    break;
                case harp_type_int16:
                    equal = variable->data.int16_data[index[j]] == variable->data.int16_data[i];
                    break;
                case harp_type_int32:
                    equal = variable->data.int32_data[index[j]] == variable->data.int32_data[i];
                    break;
                case harp_type_float:
                    if (check_nan)
                    {
                        equal = harp_isnan(variable->data.float_data[index[j]]);
                    }
                    else
                    {
                        equal = variable->data.float_data[index[j]] == variable->data.float_data[i];
                    }
                    break;
                case harp_type_double:
                    if (check_nan)
                    {
                        equal = harp_isnan(variable->data.double_data[index[j]]);
                    }
                    else
                    {
                        equal = variable->data.double_data[index[j]] == variable->data.double_data[i];
                    }
                    break;
                case harp_type_string:
                    if (variable->data.string_data[i] == NULL)
                    {
                        equal = variable->data.string_data[index[j]] == NULL;
                    }
                    else if (variable->data.string_data[index[j]] == NULL)
                    {
                        equal = 0;
                    }
                    else
                    {
                        equal = strcmp(variable->data.string_data[index[j]], variable->data.string_data[i]) == 0;
                    }
                    break;
            }
            if (equal)
            {
                break;
            }
        }
        if (j == num_bins)
        {
            /* add new value to bin */
            index[num_bins] = i;
            num_bins++;
        }
        bin_index[i] = j;
    }

    free(index);

    if (get_binning_type(variable) == binning_remove)
    {
        harp_variable *original_variable = variable;

        /* we always want to keep the variable that we bin on */
        if (harp_variable_copy(original_variable, &variable) != 0)
        {
            free(bin_index);
            return -1;
        }
        if (harp_variable_rearrange_dimension(variable, 0, num_bins, bin_index) != 0)
        {
            harp_variable_delete(variable);
            free(bin_index);
            return -1;
        }
    }
    else
    {
        variable = NULL;
    }

    if (harp_product_bin(product, num_bins, num_elements, bin_index) != 0)
    {
        if (variable != NULL)
        {
            harp_variable_delete(variable);
        }
        free(bin_index);
        return -1;
    }

    if (variable != NULL)
    {
        if (harp_product_add_variable(product, variable) != 0)
        {
            harp_variable_delete(variable);
            free(bin_index);
            return -1;
        }
    }

    /* cleanup */
    free(bin_index);

    return 0;
}
