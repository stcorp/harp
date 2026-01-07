/*
 * Copyright (C) 2015-2026 S[&]T, The Netherlands.
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
    binning_average,
    binning_uncertainty,
    binning_weight,     /* only used for int32_t and float data */
    binning_angle,      /* will use averaging using 2D vectors */
    binning_time_min,
    binning_time_max,
    binning_time_average
} binning_type;


static binning_type get_binning_type(harp_variable *variable)
{
    long variable_name_length = (long)strlen(variable->name);
    int i;

    /* any variable with a time dimension that is not the first dimension gets removed */
    for (i = 1; i < variable->num_dimensions; i++)
    {
        if (variable->dimension[i] == harp_dimension_time)
        {
            return binning_remove;
        }
    }

    /* only keep valid count variables */
    if (variable_name_length >= 5 && strcmp(&variable->name[variable_name_length - 5], "count") == 0)
    {
        if (variable->num_dimensions < 1 || variable->dimension_type[0] != harp_dimension_time ||
            variable->data_type != harp_type_int32 || variable->unit != NULL)
        {
            return binning_remove;
        }
        if (variable_name_length == 5 && variable->num_dimensions != 1)
        {
            return binning_remove;
        }
        return binning_weight;
    }

    /* only keep valid weight variables */
    if (variable_name_length >= 6 && strcmp(&variable->name[variable_name_length - 6], "weight") == 0)
    {
        if (variable->num_dimensions < 1 || variable->dimension_type[0] != harp_dimension_time ||
            variable->data_type != harp_type_float || variable->unit != NULL)
        {
            return binning_remove;
        }
        return binning_weight;
    }

    /* we only bin variables with a time dimension */
    if (variable->num_dimensions == 0 || variable->dimension_type[0] != harp_dimension_time)
    {
        return binning_skip;
    }

    /* variables with enumeration values get removed */
    if (variable->num_enum_values > 0)
    {
        return binning_remove;
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

    if (strstr(variable->name, "_uncertainty") != NULL)
    {
        if (strstr(variable->name, "_uncertainty_systematic") != NULL)
        {
            /* always propagate uncertainty assuming full correlation for the systematic part */
            return binning_average;
        }
        if (strstr(variable->name, "_uncertainty_random") != NULL)
        {
            /* always propagate uncertainty assuming no correlation for the random part */
            return binning_uncertainty;
        }
        /* for the total uncertainty let it depend on the given parameter/option */
        if (harp_get_option_propagate_uncertainty() == 1)
        {
            /* propagate uncertainty assuming full correlation */
            return binning_average;
        }
        /* propagate uncertainty assuming no correlation */
        return binning_uncertainty;
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

    if (strstr(variable->name, "latitude") != NULL || strstr(variable->name, "longitude") != NULL ||
        strstr(variable->name, "angle") != NULL || strstr(variable->name, "direction") != NULL)
    {
        return binning_angle;
    }

    /* use minimum/maximum for datetime start/stop */
    if (variable->num_dimensions == 1)
    {
        if (strcmp(variable->name, "datetime_start") == 0)
        {
            return binning_time_min;
        }
        if (strcmp(variable->name, "datetime_stop") == 0)
        {
            return binning_time_max;
        }
    }

    /* use average by default */
    return binning_average;
}

/* find a <variable->name>_count variable.
 * If the variable exists but is invalid its entry in the bintype array will be set to binning_remove.
 */
static int get_count_variable_for_variable(harp_product *product, harp_variable *variable, binning_type *bintype,
                                           harp_variable **count_variable)
{
    char variable_name[MAX_NAME_LENGTH];
    int index;
    int i;

    *count_variable = NULL;

    snprintf(variable_name, MAX_NAME_LENGTH, "%s_count", variable->name);
    if (!harp_product_has_variable(product, variable_name))
    {
        return 0;
    }

    if (harp_product_get_variable_index_by_name(product, variable_name, &index) != 0)
    {
        return -1;
    }
    if (bintype[index] == binning_remove)
    {
        return 0;
    }

    /* make sure that the dimensions of the count variable match the dimensions of the given variable */
    if (product->variable[index]->num_dimensions != variable->num_dimensions)
    {
        bintype[index] = binning_remove;
        return 0;
    }
    for (i = 0; i < product->variable[index]->num_dimensions; i++)
    {
        if (product->variable[index]->dimension_type[i] != variable->dimension_type[i] ||
            product->variable[index]->dimension[i] != variable->dimension[i])
        {
            bintype[index] = binning_remove;
            return 0;
        }
    }

    *count_variable = product->variable[index];

    return 0;
}

/* find a <variable->name>_weight variable.
 * If the variable exists but is invalid its entry in the bintype array will be set to binning_remove.
 */
static int get_weight_variable_for_variable(harp_product *product, harp_variable *variable, binning_type *bintype,
                                            harp_variable **weight_variable)
{
    char variable_name[MAX_NAME_LENGTH];
    int index;
    int i;

    *weight_variable = NULL;

    snprintf(variable_name, MAX_NAME_LENGTH, "%s_weight", variable->name);
    if (!harp_product_has_variable(product, variable_name))
    {
        return 0;
    }

    if (harp_product_get_variable_index_by_name(product, variable_name, &index) != 0)
    {
        return -1;
    }
    if (bintype[index] == binning_remove)
    {
        return 0;
    }

    /* make sure that the dimensions of the weight variable match the dimensions of the given variable */
    if (product->variable[index]->num_dimensions != variable->num_dimensions)
    {
        bintype[index] = binning_remove;
        return 0;
    }
    for (i = 0; i < product->variable[index]->num_dimensions; i++)
    {
        if (product->variable[index]->dimension_type[i] != variable->dimension_type[i] ||
            product->variable[index]->dimension[i] != variable->dimension[i])
        {
            bintype[index] = binning_remove;
            return 0;
        }
    }

    *weight_variable = product->variable[index];

    return 0;
}

/* get count values for each element in the provided variable.
 * if a '<variable>_count' or 'count' variable exists then 'count' will be populated and the return value will be 1.
 * if no applicable count variable could be found then the return value will be 0.
 * the return value is -1 when an error is encountered.
 */
static int get_count_for_variable(harp_product *product, harp_variable *variable, binning_type *bintype, int32_t *count)
{
    harp_variable *count_variable = NULL;
    long i, j;

    if (variable->num_dimensions < 1 || variable->dimension_type[0] != harp_dimension_time)
    {
        return 0;
    }

    if (get_count_variable_for_variable(product, variable, bintype, &count_variable) != 0)
    {
        return -1;
    }
    if (count_variable == NULL)
    {
        int index;

        if (!harp_product_has_variable(product, "count"))
        {
            return 0;
        }
        if (harp_product_get_variable_index_by_name(product, "count", &index) != 0)
        {
            return -1;
        }
        if (bintype[index] == binning_remove)
        {
            return 0;
        }
        count_variable = product->variable[index];
    }

    /* store data into count parameter */
    if (variable->num_elements == count_variable->num_elements)
    {
        memcpy(count, count_variable->data.int32_data, count_variable->num_elements * sizeof(int32_t));
    }
    else
    {
        long num_sub_elements = variable->num_elements / count_variable->num_elements;

        assert(count_variable->num_elements < variable->num_elements);

        for (i = 0; i < count_variable->num_elements; i++)
        {
            for (j = 0; j < num_sub_elements; j++)
            {
                count[i * num_sub_elements + j] = count_variable->data.int32_data[i];
            }
        }
    }

    return 1;
}

/* get weight values for each element in the provided variable.
 * if a '<variable>_weight' or 'weight' variable exists then 'weight' will be populated and the return value will be 1.
 * if no applicable weight variable could be found then the return value will be 0.
 * the return value is -1 when an error is encountered.
 */
static int get_weight_for_variable(harp_product *product, harp_variable *variable, binning_type *bintype, float *weight)
{
    harp_variable *weight_variable = NULL;
    long i, j;

    if (variable->num_dimensions <= 1 || variable->dimension_type[0] != harp_dimension_time)
    {
        return 0;
    }

    if (get_weight_variable_for_variable(product, variable, bintype, &weight_variable) != 0)
    {
        return -1;
    }
    if (weight_variable == NULL)
    {
        int index;

        if (!harp_product_has_variable(product, "weight"))
        {
            return 0;
        }
        if (harp_product_get_variable_index_by_name(product, "weight", &index) != 0)
        {
            return -1;
        }
        if (bintype[index] == binning_remove)
        {
            return 0;
        }
        weight_variable = product->variable[index];

        /* initial dimensions should match */
        if (weight_variable->num_dimensions > variable->num_dimensions)
        {
            return 0;
        }
        for (i = 0; i < weight_variable->num_dimensions; i++)
        {
            if (weight_variable->dimension_type[i] != variable->dimension_type[i] ||
                weight_variable->dimension[i] != variable->dimension[i])
            {
                return 0;
            }
        }
    }

    /* store data into weight parameter */
    if (variable->num_elements == weight_variable->num_elements)
    {
        memcpy(weight, weight_variable->data.float_data, weight_variable->num_elements * sizeof(float));
    }
    else
    {
        long num_sub_elements = variable->num_elements / weight_variable->num_elements;

        for (i = 0; i < weight_variable->num_elements; i++)
        {
            for (j = 0; j < num_sub_elements; j++)
            {
                weight[i * num_sub_elements + j] = weight_variable->data.float_data[i];
            }
        }
    }

    return 1;
}

static int add_count_variable(harp_product *product, binning_type *bintype, binning_type target_bintype,
                              const char *variable_name, int num_dimensions, harp_dimension_type *dimension_type,
                              long *dimension, int32_t *count)
{
    char count_variable_name[MAX_NAME_LENGTH];
    harp_variable *variable;
    int index = -1;

    if (variable_name != NULL)
    {
        snprintf(count_variable_name, MAX_NAME_LENGTH, "%s_count", variable_name);
    }
    else
    {
        strcpy(count_variable_name, "count");
    }

    if (harp_product_has_variable(product, count_variable_name))
    {
        if (harp_product_get_variable_index_by_name(product, count_variable_name, &index) != 0)
        {
            return -1;
        }
    }

    if (index != -1 && bintype[index] != binning_remove)
    {
        /* if the count variable already exists and does not get removed then we assume it is correct/consistent
         * (i.e. existing count=0 <-> variable=NaN) */
        /* update bintype anyway */
        bintype[index] = target_bintype;
        return 0;
    }

    if (harp_variable_new(count_variable_name, harp_type_int32, num_dimensions, dimension_type, dimension,
                          &variable) != 0)
    {
        return -1;
    }
    memcpy(variable->data.int32_data, count, variable->num_elements * sizeof(int32_t));
    if (index == -1)
    {
        if (harp_product_add_variable(product, variable) != 0)
        {
            harp_variable_delete(variable);
            return -1;
        }
        index = product->num_variables - 1;
    }
    else
    {
        if (harp_product_replace_variable(product, variable) != 0)
        {
            harp_variable_delete(variable);
            return -1;
        }
    }
    bintype[index] = target_bintype;

    return 0;
}

static int add_weight_variable(harp_product *product, binning_type *bintype, binning_type target_bintype,
                               const char *variable_name, int num_dimensions, harp_dimension_type *dimension_type,
                               long *dimension, float *weight)
{
    char weight_variable_name[MAX_NAME_LENGTH];
    harp_variable *variable;
    int index = -1;

    if (variable_name != NULL)
    {
        snprintf(weight_variable_name, MAX_NAME_LENGTH, "%s_weight", variable_name);
    }
    else
    {
        strcpy(weight_variable_name, "weight");
    }

    if (harp_product_has_variable(product, weight_variable_name))
    {
        if (harp_product_get_variable_index_by_name(product, weight_variable_name, &index) != 0)
        {
            return -1;
        }
    }

    if (harp_variable_new(weight_variable_name, harp_type_float, num_dimensions, dimension_type, dimension,
                          &variable) != 0)
    {
        return -1;
    }
    memcpy(variable->data.float_data, weight, variable->num_elements * sizeof(float));
    if (index == -1)
    {
        if (harp_product_add_variable(product, variable) != 0)
        {
            harp_variable_delete(variable);
            return -1;
        }
        index = product->num_variables - 1;
    }
    else
    {
        if (harp_product_replace_variable(product, variable) != 0)
        {
            harp_variable_delete(variable);
            return -1;
        }
    }
    bintype[index] = target_bintype;

    return 0;
}

/** \addtogroup harp_product
 * @{
 */

/** Bin the product's variables.
 * This will bin all variables in the time dimension. Each time sample will be put in the bin defined by bin_index.
 * All variables with a time dimension will then be resampled using these bins.
 * The resulting value for each variable will be the average of all values for the bin (using existing count or weight
 * variables as weighting factors where available).
 * Variables with multiple dimensions will have all elements in the sub dimensions averaged on an element by element
 * basis.
 *
 * Variables that have a time dimension but no unit (or using a string data type) will be removed.
 * The exception are count and weight variables, which will be summed.
 *
 * All variables that are binned (except existing count/weight variables) are converted to a double data type.
 * Bins that have no samples will end up with a NaN value.
 *
 * If the product did not already have a 'count' variable then a 'count' variable will be added to the product that
 * will contain the number of samples per bin.
 *
 * Only non-NaN values will contribute to a bin. If there are NaN values and there is not already a variable-specific
 * count or weight variable for that variable, then a separate variable-specific count variable will be created that
 * will contain the number of non-NaN values that contributed to each bin. This count variable will have the same
 * dimensions as the variable it provides the count for.
 *
 * For angle variables a variable-specific weight variable will be created (if it did not yet exist) that contains
 * the magnitude of the sum of the unit vectors that was used to calculate the angle average.
 *
 * For uncertainty variables the first order propagation rules are used (assuming full correlation for systematic
 * uncertainty variables and using the harp_get_option_propagate_uncertainty() setting for total and random uncertainty
 * variables).
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
    harp_dimension_type dimension_type[HARP_MAX_NUM_DIMS];
    binning_type *bintype = NULL;
    double nan_value = harp_nan();
    long count_size = 0;
    int32_t *bin_count = NULL;
    int32_t *count = NULL;
    float *weight = NULL;
    long *index = NULL;
    long i, j, k;
    int result;

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

    /* make 'bintype' big enough to also store any count/weight variables that we may want to add (i.e. 1 + factor 2) */
    bintype = malloc((2 * product->num_variables + 1) * sizeof(binning_type));
    if (bintype == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       (2 * product->num_variables + 1) * sizeof(binning_type), __FILE__, __LINE__);
        goto error;
    }
    for (k = 0; k < product->num_variables; k++)
    {
        bintype[k] = get_binning_type(product->variable[k]);

        /* determine the maximum number of elements (as size for the 'count' and 'weight' arrays) */
        if (bintype[k] != binning_remove && bintype[k] != binning_skip)
        {
            long total_num_elements = product->variable[k]->num_elements;

            if (num_bins > num_elements)
            {
                /* use longest time dimension (before vs. after binning) */
                total_num_elements = num_bins * (total_num_elements / num_elements);
            }
            if (total_num_elements > count_size)
            {
                count_size = total_num_elements;
            }
        }
    }

    index = malloc(num_bins * sizeof(long));
    if (index == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_bins * sizeof(long), __FILE__, __LINE__);
        goto error;
    }
    bin_count = malloc(num_bins * sizeof(int32_t));
    if (bin_count == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_bins * sizeof(int32_t), __FILE__, __LINE__);
        goto error;
    }
    count = malloc(count_size * sizeof(int32_t));
    if (count == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       count_size * sizeof(int32_t), __FILE__, __LINE__);
        goto error;
    }
    weight = malloc(count_size * sizeof(float));
    if (weight == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       count_size * sizeof(float), __FILE__, __LINE__);
        goto error;
    }

    /* for each bin, store the index of the first sample that contributes to the bin */
    /* this is where we will aggregate all samples for that bin */
    for (i = 0; i < num_bins; i++)
    {
        index[i] = 0;   /* initialize with 0 so harp_variable_rearrange_dimension will get valid indices for all bins */
        bin_count[i] = 0;
    }
    for (i = 0; i < num_elements; i++)
    {
        if (bin_count[bin_index[i]] == 0)
        {
            index[bin_index[i]] = i;
        }
        bin_count[bin_index[i]]++;
    }

    /* pre-process all variables */
    for (k = 0; k < product->num_variables; k++)
    {
        harp_variable *variable;

        if (bintype[k] == binning_skip || bintype[k] == binning_remove)
        {
            continue;
        }

        variable = product->variable[k];

        /* convert variables to double */
        if (bintype[k] != binning_weight)
        {
            if (harp_variable_convert_data_type(variable, harp_type_double) != 0)
            {
                goto error;
            }
        }

        /* convert all angles to 2D vectors [cos(x),sin(x)] and pre-multiply by existing weights */
        if (bintype[k] == binning_angle)
        {
            harp_variable *weight_variable = NULL;

            if (get_weight_variable_for_variable(product, variable, bintype, &weight_variable) != 0)
            {
                goto error;
            }
            if (weight_variable == NULL)
            {
                /* create new weight variable using unit vector norms */
                for (i = 0; i < variable->num_elements; i++)
                {
                    weight[i] = 1.0;
                }
                if (add_weight_variable(product, bintype, binning_weight, variable->name, variable->num_dimensions,
                                        variable->dimension_type, variable->dimension, weight) != 0)
                {
                    goto error;
                }
                if (get_weight_variable_for_variable(product, variable, bintype, &weight_variable) != 0)
                {
                    goto error;
                }
                assert(weight_variable != NULL);
            }

            if (harp_convert_unit(variable->unit, "rad", variable->num_elements, variable->data.double_data) != 0)
            {
                goto error;
            }
            if (harp_variable_add_dimension(variable, variable->num_dimensions, harp_dimension_independent, 2) != 0)
            {
                goto error;
            }
            for (i = 0; i < variable->num_elements; i += 2)
            {
                double angle = variable->data.double_data[i];
                float norm = weight_variable->data.float_data[i / 2];

                if (norm == 0 || harp_isnan(angle))
                {
                    variable->data.double_data[i] = 0;
                    variable->data.double_data[i + 1] = 0;
                    weight_variable->data.float_data[i / 2] = 0;
                }
                else
                {
                    variable->data.double_data[i] = norm * cos(angle);
                    variable->data.double_data[i + 1] = norm * sin(angle);
                }
            }
        }

        /* pre-multiply variables by existing counts/weights (weights have preference) */
        if (bintype[k] == binning_average || bintype[k] == binning_uncertainty)
        {
            result = get_weight_for_variable(product, variable, bintype, weight);
            if (result < 0)
            {
                goto error;
            }
            if (result == 1)
            {
                /* multiply by the weight */
                for (i = 0; i < variable->num_elements; i++)
                {
                    variable->data.double_data[i] *= weight[i];
                }
            }
            else
            {
                result = get_count_for_variable(product, variable, bintype, count);
                if (result < 0)
                {
                    goto error;
                }
                if (result == 1)
                {
                    /* multiply by the count */
                    for (i = 0; i < variable->num_elements; i++)
                    {
                        variable->data.double_data[i] *= count[i];
                    }
                }
            }
        }

        /* square the pre-weighted uncertainties */
        if (bintype[k] == binning_uncertainty)
        {
            for (i = 0; i < variable->num_elements; i++)
            {
                variable->data.double_data[i] *= variable->data.double_data[i];
            }
        }
    }

    /* sum up all samples into bins (in place) and create count variables where needed */
    for (k = 0; k < product->num_variables; k++)
    {
        harp_variable *variable;
        long num_sub_elements;

        if (bintype[k] == binning_skip || bintype[k] == binning_remove || bintype[k] == binning_weight)
        {
            /* we handle the summable variables in a second iteration to prevent wrong use of weights/counts */
            continue;
        }

        variable = product->variable[k];
        assert(variable->dimension[0] == num_elements);
        num_sub_elements = variable->num_elements / num_elements;

        if (bintype[k] == binning_time_min)
        {
            /* take minimum of all values per bin */
            assert(num_sub_elements == 1);
            for (i = 0; i < num_elements; i++)
            {
                long target_index = index[bin_index[i]];

                if (variable->data.double_data[i] < variable->data.double_data[target_index] ||
                    (harp_isnan(variable->data.double_data[target_index]) &&
                     !harp_isnan(variable->data.double_data[i])))
                {
                    variable->data.double_data[target_index] = variable->data.double_data[i];
                }
            }
        }
        else if (bintype[k] == binning_time_max)
        {
            /* take maximum of all values per bin */
            assert(num_sub_elements == 1);
            for (i = 0; i < num_elements; i++)
            {
                long target_index = index[bin_index[i]];

                if (variable->data.double_data[i] > variable->data.double_data[target_index] ||
                    (harp_isnan(variable->data.double_data[target_index]) &&
                     !harp_isnan(variable->data.double_data[i])))
                {
                    variable->data.double_data[target_index] = variable->data.double_data[i];
                }
            }
        }
        else if (bintype[k] == binning_angle)
        {
            /* store the sum of the vectors of a bin into the location of the first sample */
            for (i = 0; i < num_elements; i++)
            {
                long target_index = index[bin_index[i]];

                if (target_index != i)
                {
                    for (j = 0; j < num_sub_elements; j += 2)
                    {
                        variable->data.double_data[target_index * num_sub_elements + j] +=
                            variable->data.double_data[i * num_sub_elements + j];
                        variable->data.double_data[target_index * num_sub_elements + j + 1] +=
                            variable->data.double_data[i * num_sub_elements + j + 1];
                    }
                }
            }
        }
        else
        {
            int use_weight_variable = 0;
            int store_count_variable = 0;
            int store_weight_variable = 0;

            assert(bintype[k] == binning_average || bintype[k] == binning_uncertainty);

            /* sum up all values of a bin into the location of the first sample */

            result = get_weight_for_variable(product, variable, bintype, weight);
            if (result < 0)
            {
                goto error;
            }
            if (result == 1)
            {
                use_weight_variable = 1;
            }
            else
            {
                result = get_count_for_variable(product, variable, bintype, count);
                if (result < 0)
                {
                    goto error;
                }
                if (result == 0)
                {
                    /* if there is no pre-existing weight or count variable then set all counts to 1 */
                    for (i = 0; i < variable->num_elements; i++)
                    {
                        count[i] = 1;
                    }
                }
            }

            for (i = 0; i < num_elements; i++)
            {
                long target_index = index[bin_index[i]];

                if (target_index != i)
                {
                    for (j = 0; j < num_sub_elements; j++)
                    {
                        if (harp_isnan(variable->data.double_data[i * num_sub_elements + j]))
                        {
                            if (use_weight_variable)
                            {
                                if (weight[i * num_sub_elements + j] != 0)
                                {
                                    weight[i * num_sub_elements + j] = 0;
                                    store_weight_variable = 1;
                                }
                            }
                            else if (count[i * num_sub_elements + j] != 0)
                            {
                                count[i * num_sub_elements + j] = 0;
                                store_count_variable = 1;
                            }
                        }
                        else
                        {
                            variable->data.double_data[target_index * num_sub_elements + j] +=
                                variable->data.double_data[i * num_sub_elements + j];
                        }
                    }
                }
                else
                {
                    for (j = 0; j < num_sub_elements; j++)
                    {
                        if (harp_isnan(variable->data.double_data[target_index * num_sub_elements + j]))
                        {
                            if (use_weight_variable)
                            {
                                if (weight[target_index * num_sub_elements + j] != 0)
                                {
                                    weight[target_index * num_sub_elements + j] = 0;
                                    store_weight_variable = 1;
                                }
                            }
                            else if (count[target_index * num_sub_elements + j] != 0)
                            {
                                count[target_index * num_sub_elements + j] = 0;
                                store_count_variable = 1;
                            }
                            variable->data.double_data[target_index * num_sub_elements + j] = 0;
                        }
                    }
                }
            }

            if (store_count_variable)
            {
                if (add_count_variable(product, bintype, binning_weight, variable->name, variable->num_dimensions,
                                       variable->dimension_type, variable->dimension, count) != 0)
                {
                    goto error;
                }
            }
            if (store_weight_variable)
            {
                if (add_weight_variable(product, bintype, binning_weight, variable->name, variable->num_dimensions,
                                        variable->dimension_type, variable->dimension, weight) != 0)
                {
                    goto error;
                }
            }
        }
    }
    /* do the same, but now only for the weight and count variables */
    for (k = 0; k < product->num_variables; k++)
    {
        harp_variable *variable;
        long num_sub_elements;

        if (bintype[k] != binning_weight)
        {
            continue;
        }

        variable = product->variable[k];
        assert(variable->dimension[0] == num_elements);
        num_sub_elements = variable->num_elements / num_elements;

        /* sum up all values of a bin into the location of the first sample */
        for (i = 0; i < num_elements; i++)
        {
            long target_index = index[bin_index[i]];

            if (target_index != i)
            {
                if (variable->data_type == harp_type_int32)
                {
                    for (j = 0; j < num_sub_elements; j++)
                    {
                        variable->data.int32_data[target_index * num_sub_elements + j] +=
                            variable->data.int32_data[i * num_sub_elements + j];
                    }
                }
                else
                {
                    assert(variable->data_type == harp_type_float);
                    for (j = 0; j < num_sub_elements; j++)
                    {
                        variable->data.float_data[target_index * num_sub_elements + j] +=
                            variable->data.float_data[i * num_sub_elements + j];
                    }
                }
            }
        }
    }

    /* resample variables */
    for (k = 0; k < product->num_variables; k++)
    {
        if (bintype[k] == binning_skip || bintype[k] == binning_remove)
        {
            continue;
        }

        /* resample the time dimension to the target bins */
        /* this uses the first sample for empty bins, but we invalidate these bins later */
        if (harp_variable_rearrange_dimension(product->variable[k], 0, num_bins, index) != 0)
        {
            goto error;
        }
    }

    /* set all empty bins to NaN (for double) or 0 (for int32/float count/weight) */
    for (k = 0; k < product->num_variables; k++)
    {
        harp_variable *variable;
        long num_sub_elements;

        if (bintype[k] == binning_skip || bintype[k] == binning_remove)
        {
            continue;
        }

        variable = product->variable[k];
        num_sub_elements = variable->num_elements / num_elements;
        for (i = 0; i < num_bins; i++)
        {
            if (bin_count[i] == 0)
            {
                if (variable->data_type == harp_type_int32)
                {
                    for (j = 0; j < num_sub_elements; j++)
                    {
                        variable->data.int32_data[i * num_sub_elements + j] = 0;
                    }
                }
                else if (variable->data_type == harp_type_float)
                {
                    for (j = 0; j < num_sub_elements; j++)
                    {
                        variable->data.float_data[i * num_sub_elements + j] = 0;
                    }
                }
                else
                {
                    for (j = 0; j < num_sub_elements; j++)
                    {
                        variable->data.double_data[i * num_sub_elements + j] = nan_value;
                    }
                }
            }
        }
    }

    /* update product dimensions */
    product->dimension[harp_dimension_time] = num_bins;

    /* add global count variable if it didn't exist yet */
    dimension_type[0] = harp_dimension_time;
    if (add_count_variable(product, bintype, binning_skip, NULL, 1, dimension_type, &num_bins, bin_count) != 0)
    {
        goto error;
    }

    /* post-process all variables */
    for (k = 0; k < product->num_variables; k++)
    {
        harp_variable *variable;

        if (bintype[k] == binning_skip || bintype[k] == binning_remove)
        {
            continue;
        }

        variable = product->variable[k];

        if (bintype[k] == binning_angle)
        {
            harp_variable *weight_variable;

            /* convert angle variables back from 2D vectors to angles */
            for (i = 0; i < variable->num_elements; i += 2)
            {
                double x = variable->data.double_data[i];
                double y = variable->data.double_data[i + 1];

                weight[i / 2] = sqrt(x * x + y * y);
                variable->data.double_data[i] = atan2(y, x);
            }
            if (harp_variable_remove_dimension(variable, variable->num_dimensions - 1, 0) != 0)
            {
                goto error;
            }
            /* convert all angles back to the original unit */
            if (harp_convert_unit("rad", variable->unit, variable->num_elements, variable->data.double_data) != 0)
            {
                goto error;
            }

            /* set values to NaN if weight==0, and update weight to be the norm of the averaged vector otherwise */
            if (get_weight_variable_for_variable(product, variable, bintype, &weight_variable) != 0)
            {
                goto error;
            }
            assert(weight_variable != NULL);
            for (i = 0; i < variable->num_elements; i++)
            {
                if (weight_variable->data.float_data[i] == 0)
                {
                    variable->data.double_data[i] = nan_value;
                }
                else
                {
                    weight_variable->data.float_data[i] = weight[i];
                }
            }
        }

        /* take square root of the sum before dividing by the sum of the counts/weights */
        if (bintype[k] == binning_uncertainty)
        {
            for (i = 0; i < variable->num_elements; i++)
            {
                variable->data.double_data[i] = sqrt(variable->data.double_data[i]);
            }
        }

        /* divide variables by the sample count/weight and/or set values to NaN if count/weight==0 */
        if (bintype[k] == binning_average || bintype[k] == binning_uncertainty)
        {
            result = get_weight_for_variable(product, variable, bintype, weight);
            if (result < 0)
            {
                goto error;
            }
            if (result == 1)
            {
                /* divide by the weight (or set value to NaN if weight==0) */
                for (i = 0; i < variable->num_elements; i++)
                {
                    if (weight[i] == 0)
                    {
                        variable->data.double_data[i] = nan_value;
                    }
                    else
                    {
                        variable->data.double_data[i] /= weight[i];
                    }
                }
            }
            else
            {
                result = get_count_for_variable(product, variable, bintype, count);
                if (result < 0)
                {
                    goto error;
                }
                if (result == 1)
                {
                    /* divide by the count (or set value to NaN if count==0) */
                    for (i = 0; i < variable->num_elements; i++)
                    {
                        if (count[i] == 0)
                        {
                            variable->data.double_data[i] = nan_value;
                        }
                        else
                        {
                            variable->data.double_data[i] /= count[i];
                        }
                    }
                }
            }
        }
    }

    /* remove all variables that need to be removed (in reverse order!) */
    for (k = product->num_variables - 1; k >= 0; k--)
    {
        if (bintype[k] == binning_remove)
        {
            if (harp_product_remove_variable(product, product->variable[k]) != 0)
            {
                goto error;
            }
        }
    }

    free(bintype);
    free(weight);
    free(count);
    free(bin_count);
    free(index);

    return 0;

  error:
    if (bintype != NULL)
    {
        free(bintype);
    }
    if (weight != NULL)
    {
        free(weight);
    }
    if (count != NULL)
    {
        free(count);
    }
    if (bin_count != NULL)
    {
        free(bin_count);
    }
    if (index != NULL)
    {
        free(index);
    }
    return -1;
}

/**
 * @}
 */

/** Bin the product's variables such that all samples end up in a single bin.
 *
 * \param product Product to regrid.
 *
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
int harp_product_bin_full(harp_product *product)
{
    long *bin_index;
    long num_elements;
    long i;

    num_elements = product->dimension[harp_dimension_time];
    if (num_elements == 0)
    {
        /* nothing to do */
        return 0;
    }

    bin_index = malloc(num_elements * sizeof(long));
    if (bin_index == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_elements * sizeof(long), __FILE__, __LINE__);
        return -1;
    }
    for (i = 0; i < num_elements; i++)
    {
        bin_index[i] = 0;
    }

    if (harp_product_bin(product, 1, num_elements, bin_index) != 0)
    {
        free(bin_index);
        return -1;
    }

    free(bin_index);
    return 0;
}

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

    if (harp_product_detach_variable(product, collocation_index) != 0)
    {
        harp_collocation_result_shallow_delete(filtered_collocation_result);
        free(bin_index);
        free(index);
        return -1;
    }

    if (harp_product_bin(product, num_bins, collocation_index->num_elements, bin_index) != 0)
    {
        harp_collocation_result_shallow_delete(filtered_collocation_result);
        harp_variable_delete(collocation_index);
        free(bin_index);
        free(index);
        return -1;
    }

    if (harp_variable_rearrange_dimension(collocation_index, 0, num_bins, index) != 0)
    {
        harp_collocation_result_shallow_delete(filtered_collocation_result);
        harp_variable_delete(collocation_index);
        free(bin_index);
        free(index);
        return -1;
    }

    harp_collocation_result_shallow_delete(filtered_collocation_result);
    free(bin_index);
    free(index);

    /* add filtered collocation_index back again */
    if (harp_product_add_variable(product, collocation_index) != 0)
    {
        harp_variable_delete(collocation_index);
        return -1;
    }

    return 0;
}

/** Bin the product's variables such that all samples that have the same combination of values from the given variables
 * are averaged together.
 *
 * \param product Product to regrid.
 * \param num_variables Number of variables
 * \param variable_name List of names of variables that define the bins (based on equal value combination).
 *
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
int harp_product_bin_with_variable(harp_product *product, int num_variables, const char **variable_name)
{
    harp_variable **variable = NULL;
    harp_variable **variable_copy = NULL;
    int *check_nan = NULL;
    long *index = NULL; /* contains index of first sample for each bin */
    long *bin_index = NULL;
    long num_elements;
    long num_bins;
    long i, j, k;

    if (num_variables < 1)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "binning requires at least one variable");
        return -1;
    }

    variable = malloc(num_variables * sizeof(harp_variable *));
    if (variable == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_variables * sizeof(harp_variable *), __FILE__, __LINE__);
        goto error;
    }
    variable_copy = malloc(num_variables * sizeof(harp_variable *));
    if (variable_copy == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_variables * sizeof(harp_variable *), __FILE__, __LINE__);
        goto error;
    }
    for (k = 0; k < num_variables; k++)
    {
        variable[k] = NULL;
        variable_copy[k] = NULL;
    }

    for (k = 0; k < num_variables; k++)
    {
        if (harp_product_get_variable_by_name(product, variable_name[k], &variable[k]) != 0)
        {
            goto error;
        }
        if (variable[k]->num_dimensions != 1 || variable[k]->dimension_type[0] != harp_dimension_time)
        {
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variable '%s' should be one dimensional and depend on time to "
                           "be used for binning", variable_name[k]);
            goto error;
        }
    }

    num_elements = variable[0]->num_elements;

    index = malloc(num_elements * sizeof(long));
    if (index == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_elements * sizeof(long), __FILE__, __LINE__);
        goto error;
    }
    bin_index = malloc(num_elements * sizeof(long));
    if (bin_index == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_elements * sizeof(long), __FILE__, __LINE__);
        goto error;
    }

    check_nan = malloc(num_variables * sizeof(int));
    if (check_nan == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_variables * sizeof(int), __FILE__, __LINE__);
        goto error;
    }

    num_bins = 0;
    for (i = 0; i < num_elements; i++)
    {
        for (k = 0; k < num_variables; k++)
        {
            check_nan[k] = 0;
            if (variable[k]->data_type == harp_type_float)
            {
                check_nan[k] = harp_isnan(variable[k]->data.float_data[i]);
            }
            else if (variable[k]->data_type == harp_type_double)
            {
                check_nan[k] = harp_isnan(variable[k]->data.double_data[i]);
            }
        }
        for (j = 0; j < num_bins; j++)
        {
            for (k = 0; k < num_variables; k++)
            {
                int equal = 1;

                switch (variable[k]->data_type)
                {
                    case harp_type_int8:
                        equal = variable[k]->data.int8_data[index[j]] == variable[k]->data.int8_data[i];
                        break;
                    case harp_type_int16:
                        equal = variable[k]->data.int16_data[index[j]] == variable[k]->data.int16_data[i];
                        break;
                    case harp_type_int32:
                        equal = variable[k]->data.int32_data[index[j]] == variable[k]->data.int32_data[i];
                        break;
                    case harp_type_float:
                        if (check_nan)
                        {
                            equal = harp_isnan(variable[k]->data.float_data[index[j]]);
                        }
                        else
                        {
                            equal = variable[k]->data.float_data[index[j]] == variable[k]->data.float_data[i];
                        }
                        break;
                    case harp_type_double:
                        if (check_nan)
                        {
                            equal = harp_isnan(variable[k]->data.double_data[index[j]]);
                        }
                        else
                        {
                            equal = variable[k]->data.double_data[index[j]] == variable[k]->data.double_data[i];
                        }
                        break;
                    case harp_type_string:
                        if (variable[k]->data.string_data[i] == NULL)
                        {
                            equal = variable[k]->data.string_data[index[j]] == NULL;
                        }
                        else if (variable[k]->data.string_data[index[j]] == NULL)
                        {
                            equal = 0;
                        }
                        else
                        {
                            equal = strcmp(variable[k]->data.string_data[index[j]],
                                           variable[k]->data.string_data[i]) == 0;
                        }
                        break;
                }
                if (!equal)
                {
                    break;
                }
            }
            if (k == num_variables)
            {
                /* equal bin */
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

    free(check_nan);
    check_nan = NULL;

    for (k = 0; k < num_variables; k++)
    {
        if (get_binning_type(variable[k]) == binning_remove)
        {
            /* we always want to keep the variable that we bin on */
            if (harp_variable_copy(variable[k], &variable_copy[k]) != 0)
            {
                goto error;
            }
            if (harp_variable_rearrange_dimension(variable_copy[k], 0, num_bins, index) != 0)
            {
                goto error;
            }
        }
    }

    free(index);
    index = NULL;

    if (harp_product_bin(product, num_bins, num_elements, bin_index) != 0)
    {
        goto error;
    }

    for (k = 0; k < num_variables; k++)
    {
        if (variable_copy[k] != NULL)
        {
            if (harp_product_add_variable(product, variable_copy[k]) != 0)
            {
                goto error;
            }
            variable_copy[k] = NULL;
        }
    }

    /* cleanup */
    free(bin_index);
    free(variable);
    free(variable_copy);
    return 0;

  error:
    if (check_nan != NULL)
    {
        free(check_nan);
    }
    if (index != NULL)
    {
        free(index);
    }
    if (bin_index != NULL)
    {
        free(bin_index);
    }
    if (variable != NULL)
    {
        free(variable);
    }
    if (variable_copy != NULL)
    {
        for (k = 0; k < num_variables; k++)
        {
            if (variable_copy[k] != NULL)
            {
                harp_variable_delete(variable_copy[k]);
            }
        }
        free(variable_copy);
    }

    return -1;
}
