/*
 * Copyright (C) 2015-2020 S[&]T, The Netherlands.
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
#include "harp-geometry.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define MAX_NAME_LENGTH 128
#define LATLON_BLOCK_SIZE 1024

typedef enum binning_type_enum
{
    binning_skip,
    binning_remove,
    binning_average,
    binning_uncertainty,
    binning_sum,        /* only used for int32_t and float data */
    binning_angle,      /* will use averaging using complex values */
    binning_time_min,
    binning_time_max,
    binning_time_average
} binning_type;


static binning_type get_binning_type(harp_variable *variable, int is_time_dimension)
{
    long variable_name_length = (long)strlen(variable->name);
    int i;

    /* variables with enumeration values get removed */
    if (variable->num_enum_values > 0)
    {
        return binning_remove;
    }

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
        return binning_sum;
    }

    /* only keep valid weight variables */
    if (variable_name_length >= 6 && strcmp(&variable->name[variable_name_length - 6], "weight") == 0)
    {
        if (variable->num_dimensions < 1 || variable->dimension_type[0] != harp_dimension_time ||
            variable->data_type != harp_type_float || variable->unit != NULL)
        {
            return binning_remove;
        }
        return binning_sum;
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

    if (strstr(variable->name, "_uncertainty") != NULL)
    {
        if (!is_time_dimension || strstr(variable->name, "_uncertainty_systematic") != NULL)
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

static binning_type get_spatial_binning_type(harp_variable *variable)
{
    binning_type type = get_binning_type(variable, 0);

    if (type != binning_remove && type != binning_skip)
    {
        /* remove all latitude/longitude variables */
        if (strstr(variable->name, "latitude") != NULL || strstr(variable->name, "longitude") != NULL)
        {
            return binning_remove;
        }

        /* existing count and weight variables are removed if we perform a spatial bin */
        if (type == binning_sum)
        {
            return binning_remove;
        }

        /* use a plain 'bin()' on datetime variables and check that they are one dimensional and dependent on time */
        if (strcmp(variable->name, "datetime") == 0 || strcmp(variable->name, "datetime_length") == 0)
        {
            if (variable->num_dimensions != 1 || variable->dimension_type[0] != harp_dimension_time)
            {
                return binning_remove;
            }
            return binning_time_average;
        }
    }

    return type;
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

/* map polygon to right longitude range, close at the poles (if needed),
 * replicate first point at the end, and calculate min/max lat/lon
 */
static void make_2d_polygon(long *num_elements, double *latitude, double *longitude, double reference_longitude,
                            double *latitude_min, double *latitude_max, double *longitude_min, double *longitude_max)
{
    double min_lat, max_lat;
    double min_lon, max_lon, lon;
    long i;

    if (longitude[0] < reference_longitude - 180)
    {
        longitude[0] += 360;
    }
    if (longitude[0] >= reference_longitude + 180)
    {
        longitude[0] -= 360;
    }

    min_lon = longitude[0];
    max_lon = min_lon;
    min_lat = latitude[0];
    max_lat = min_lat;

    for (i = 1; i < *num_elements; i++)
    {
        while (longitude[i] < longitude[i - 1] - 180)
        {
            longitude[i] += 360;
        }
        while (longitude[i] > longitude[i - 1] + 180)
        {
            longitude[i] -= 360;
        }

        if (latitude[i] < min_lat)
        {
            min_lat = latitude[i];
        }
        else if (latitude[i] > max_lat)
        {
            max_lat = latitude[i];
        }

        if (longitude[i] < min_lon)
        {
            min_lon = longitude[i];
        }
        else if (longitude[i] > max_lon)
        {
            max_lon = longitude[i];
        }
    }

    /* close the polygon (this could have a different longitude, due to the ref_lon mapping) */
    lon = longitude[0];
    while (lon < longitude[(*num_elements) - 1] - 180)
    {
        lon += 360;
    }
    while (lon > longitude[(*num_elements) - 1] + 180)
    {
        lon -= 360;
    }
    if (lon < min_lon)
    {
        min_lon = lon;
    }
    else if (lon > max_lon)
    {
        max_lon = lon;
    }
    /* we are covering a pole if our longitude range equals 360 degrees */
    if (fabs(max_lon - (min_lon + 360)) < 1e-4)
    {
        if (max_lat > 0)
        {
            if (min_lat < 0)
            {
                /* if we cross the equator then we don't know which pole is covered */
                /* skip polygon by setting num_elements to 0 */
                *num_elements = 0;
                return;
            }
            max_lat = 90;
            /* close the polygon via the North pole */
            longitude[*num_elements] = longitude[(*num_elements) - 1];
            latitude[*num_elements] = 90;
            (*num_elements)++;
            longitude[*num_elements] = longitude[0];
            latitude[*num_elements] = 90;
            (*num_elements)++;
        }
        else if (min_lat < 0)
        {
            min_lat = -90;
            /* close the polygon via the South pole */
            longitude[*num_elements] = longitude[(*num_elements) - 1];
            latitude[*num_elements] = -90;
            (*num_elements)++;
            longitude[*num_elements] = longitude[0];
            latitude[*num_elements] = -90;
            (*num_elements)++;
        }
    }

    /* wrap longitude range to [reference_longitude-180,reference_longitude+360] */
    if (min_lon < reference_longitude - 360)
    {
        min_lon += 360;
        max_lon += 360;
        for (i = 0; i < *num_elements; i++)
        {
            longitude[i] += 360;
        }
    }
    while (min_lon >= reference_longitude + 180)
    {
        min_lon -= 360;
        max_lon -= 360;
        for (i = 0; i < *num_elements; i++)
        {
            longitude[i] -= 360;
        }
    }

    *latitude_min = min_lat;
    *latitude_max = max_lat;
    *longitude_min = min_lon;
    *longitude_max = max_lon;

    /* repeat first point at the end to make iterating over it more easy */
    latitude[*num_elements] = latitude[0];
    longitude[*num_elements] = longitude[0];
    (*num_elements)++;
}

static int add_cell_index(long cell_index, long *cumsum_index, long **latlon_cell_index, double **latlon_weight)
{
    if ((*cumsum_index) % LATLON_BLOCK_SIZE == 0)
    {
        long *new_latlon_cell_index;
        double *new_latlon_weight;

        new_latlon_cell_index = realloc(*latlon_cell_index, ((*cumsum_index) + LATLON_BLOCK_SIZE) * sizeof(long));
        if (new_latlon_cell_index == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           ((*cumsum_index) + LATLON_BLOCK_SIZE) * sizeof(long), __FILE__, __LINE__);
            return -1;
        }
        *latlon_cell_index = new_latlon_cell_index;
        new_latlon_weight = realloc(*latlon_weight, ((*cumsum_index) + LATLON_BLOCK_SIZE) * sizeof(double));
        if (new_latlon_weight == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           ((*cumsum_index) + LATLON_BLOCK_SIZE) * sizeof(double), __FILE__, __LINE__);
            return -1;
        }
        *latlon_weight = new_latlon_weight;
    }
    (*latlon_cell_index)[(*cumsum_index)] = cell_index;
    (*latlon_weight)[(*cumsum_index)] = 1.0;    /* initialize with default weight */
    (*cumsum_index)++;

    return 0;
}

/* latitude_edges and longitude_edges should contain just 2 elements (bounds of the cell) */
static double find_weight_for_polygon_and_cell(long num_points, double *poly_latitude, double *poly_longitude,
                                               double *temp_latitude, double *temp_longitude,
                                               double *latitude_edges, double *longitude_edges)
{
    double latitude, longitude, next_latitude, next_longitude;
    double cell_area, poly_area;
    long offset = num_points;
    long num_temp = 0;
    long i;

    if (num_points < 3)
    {
        return 0.0;
    }

    /* we start with filling temp_latitude and temp_longitude at offset 'num_points' */
    /* this allows us to use the same temp buffers in-place for the longitude clamping in the second step */

    /* clamp to latitude range */
    for (i = 0; i < num_points - 1; i++)
    {
        latitude = poly_latitude[i];
        longitude = poly_longitude[i];
        next_latitude = poly_latitude[i + 1];
        next_longitude = poly_longitude[i + 1];

        if (latitude < latitude_edges[0])
        {
            if (next_latitude > latitude_edges[0])
            {
                longitude += (latitude_edges[0] - latitude) * (next_longitude - longitude) / (next_latitude - latitude);
                latitude = latitude_edges[0];
            }
        }
        else if (latitude > latitude_edges[1])
        {
            if (next_latitude < latitude_edges[1])
            {
                longitude += (latitude_edges[1] - latitude) * (next_longitude - longitude) / (next_latitude - latitude);
                latitude = latitude_edges[1];
            }
        }
        if (latitude >= latitude_edges[0] && latitude <= latitude_edges[1])
        {
            temp_latitude[offset + num_temp] = latitude;
            temp_longitude[offset + num_temp] = longitude;
            num_temp++;
            if (next_latitude < latitude_edges[0])
            {
                temp_longitude[offset + num_temp] = longitude + (latitude_edges[0] - latitude) *
                    (next_longitude - longitude) / (next_latitude - latitude);
                temp_latitude[offset + num_temp] = latitude_edges[0];
                num_temp++;
            }
            else if (next_latitude > latitude_edges[1])
            {
                temp_longitude[offset + num_temp] = longitude + (latitude_edges[1] - latitude) *
                    (next_longitude - longitude) / (next_latitude - latitude);
                temp_latitude[offset + num_temp] = latitude_edges[1];
                num_temp++;
            }
        }
    }

    if (num_temp < 3)
    {
        return 0.0;
    }

    if (temp_latitude[offset] != temp_latitude[offset + num_temp - 1] ||
        temp_longitude[offset] != temp_longitude[offset + num_temp - 1])
    {
        temp_latitude[offset + num_temp] = temp_latitude[offset];
        temp_longitude[offset + num_temp] = temp_longitude[offset];
        num_temp++;
    }

    /* clamp to longitude range */
    num_points = num_temp;
    num_temp = 0;
    for (i = 0; i < num_points - 1; i++)
    {
        latitude = temp_latitude[offset + i];
        longitude = temp_longitude[offset + i];
        next_latitude = temp_latitude[offset + i + 1];
        next_longitude = temp_longitude[offset + i + 1];

        if (longitude < longitude_edges[0])
        {
            if (next_longitude > longitude_edges[0])
            {
                latitude +=
                    (longitude_edges[0] - longitude) * (next_latitude - latitude) / (next_longitude - longitude);
                longitude = longitude_edges[0];
            }
        }
        else if (longitude > longitude_edges[1])
        {
            if (next_longitude < longitude_edges[1])
            {
                latitude +=
                    (longitude_edges[1] - longitude) * (next_latitude - latitude) / (next_longitude - longitude);
                longitude = longitude_edges[1];
            }
        }
        if (longitude >= longitude_edges[0] && longitude <= longitude_edges[1])
        {
            temp_latitude[num_temp] = latitude;
            temp_longitude[num_temp] = longitude;
            num_temp++;
            if (next_longitude < longitude_edges[0])
            {
                temp_latitude[num_temp] = latitude + (longitude_edges[0] - longitude) *
                    (next_latitude - latitude) / (next_longitude - longitude);
                temp_longitude[num_temp] = longitude_edges[0];
                num_temp++;
            }
            else if (next_longitude > longitude_edges[1])
            {
                temp_latitude[num_temp] = latitude + (longitude_edges[1] - longitude) *
                    (next_latitude - latitude) / (next_longitude - longitude);
                temp_longitude[num_temp] = longitude_edges[1];
                num_temp++;
            }
        }
    }

    if (num_temp < 3)
    {
        return 0.0;
    }

    if (temp_latitude[0] != temp_latitude[num_temp - 1] || temp_longitude[0] != temp_longitude[num_temp - 1])
    {
        temp_latitude[num_temp] = temp_latitude[0];
        temp_longitude[num_temp] = temp_longitude[0];
        num_temp++;
    }

    /* calculate polygon area */
    /* the area of a polygon is equal to 0.5 * sum(k=1..n-1, vec(0,k) X vec(0,k+1)) */
    poly_area = 0;
    for (i = 0; i < num_temp - 1; i++)
    {
        poly_area += (temp_longitude[i] + temp_longitude[i + 1]) * (temp_latitude[i] - temp_latitude[i + 1]);
    }
    poly_area /= 2.0;
    if (poly_area < 0)
    {
        poly_area = -poly_area;
    }
    cell_area = (latitude_edges[1] - latitude_edges[0]) * (longitude_edges[1] - longitude_edges[0]);

    return poly_area / cell_area;
}

static int find_matching_cells_and_weights_for_bounds(harp_variable *latitude_bounds, harp_variable *longitude_bounds,
                                                      long num_latitude_edges, double *latitude_edges,
                                                      long num_longitude_edges, double *longitude_edges,
                                                      long *num_latlon_index, long **latlon_cell_index,
                                                      double **latlon_weight)
{
    double *temp_poly_latitude = NULL;
    double *temp_poly_longitude = NULL;
    double *poly_latitude = NULL;
    double *poly_longitude = NULL;
    long num_latitude_cells = num_latitude_edges - 1;
    long num_longitude_cells = num_longitude_edges - 1;
    long *min_lat_id = NULL, *max_lat_id = NULL;        /* min/max grid latitude index for each longitude grid row */
    long *min_lon_id = NULL, *max_lon_id = NULL;        /* min/max grid longitude index for each latitude grid row */
    long cumsum_index = 0;
    long num_elements;
    long max_num_vertices;
    long i, j, k;

    num_elements = latitude_bounds->dimension[0];
    max_num_vertices = latitude_bounds->dimension[latitude_bounds->num_dimensions - 1];

    if (longitude_bounds->dimension[latitude_bounds->num_dimensions - 1] != max_num_vertices)
    {
        harp_set_error(HARP_ERROR_INVALID_VARIABLE, "latitude_bounds and longitude_bounds variables should have the "
                       "same length for the inpendent dimension");
        return -1;
    }

    /* add 1 point to allow closing the polygon (i.e. repeat first point at the end) */
    /* and allow room for 2 more points to close polygons that cover a pole */
    poly_latitude = malloc((max_num_vertices + 3) * sizeof(double));
    if (poly_latitude == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       (max_num_vertices + 3) * sizeof(double), __FILE__, __LINE__);
        goto error;
    }
    poly_longitude = malloc((max_num_vertices + 3) * sizeof(double));
    if (poly_longitude == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       (max_num_vertices + 3) * sizeof(double), __FILE__, __LINE__);
        goto error;
    }
    /* the temporary polygon is used for calculating the overlap fraction with a cell */
    /* it needs to be able to hold three times the amount of points as the input polygon */
    temp_poly_latitude = malloc(3 * (max_num_vertices + 3) * sizeof(double));
    if (temp_poly_latitude == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       3 * (max_num_vertices + 3) * sizeof(double), __FILE__, __LINE__);
        goto error;
    }
    temp_poly_longitude = malloc(3 * (max_num_vertices + 3) * sizeof(double));
    if (temp_poly_longitude == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       3 * (max_num_vertices + 3) * sizeof(double), __FILE__, __LINE__);
        goto error;
    }

    /* add two to the length to allow indexing just before and after the range */
    min_lat_id = malloc((num_longitude_cells + 2) * sizeof(long));
    if (min_lat_id == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       (num_longitude_cells + 2) * sizeof(long), __FILE__, __LINE__);
        goto error;
    }
    max_lat_id = malloc((num_longitude_cells + 2) * sizeof(long));
    if (max_lat_id == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       (num_longitude_cells + 2) * sizeof(long), __FILE__, __LINE__);
        goto error;
    }
    min_lon_id = malloc((num_latitude_cells + 2) * sizeof(long));
    if (min_lon_id == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       (num_latitude_cells + 2) * sizeof(long), __FILE__, __LINE__);
        goto error;
    }
    max_lon_id = malloc((num_latitude_cells + 2) * sizeof(long));
    if (max_lon_id == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       (num_latitude_cells + 2) * sizeof(long), __FILE__, __LINE__);
        goto error;
    }

    for (i = 0; i < num_elements; i++)
    {
        double lat_min, lat_max, lon_min, lon_max;
        long num_vertices = max_num_vertices;
        int loop;

        num_latlon_index[i] = 0;

        memcpy(poly_latitude, &latitude_bounds->data.double_data[i * max_num_vertices],
               max_num_vertices * sizeof(double));
        memcpy(poly_longitude, &longitude_bounds->data.double_data[i * max_num_vertices],
               max_num_vertices * sizeof(double));
        while (num_vertices > 0 && harp_isnan(poly_latitude[num_vertices - 1]))
        {
            num_vertices--;
        }
        if (num_vertices > 2 && poly_latitude[0] == poly_latitude[num_vertices - 1] &&
            poly_longitude[0] == poly_longitude[num_vertices - 1])
        {
            /* remove duplicate point (make_2d_polygon will introduce it again) */
            num_vertices--;
        }
        if (num_vertices == 2)
        {
            /* treat this as a bounding rect -> create a polygon with four points from the edge coordinates */
            poly_latitude[2] = poly_latitude[1];
            poly_longitude[2] = poly_longitude[1];
            poly_latitude[1] = poly_latitude[0];
            poly_latitude[3] = poly_latitude[2];
            poly_longitude[3] = poly_longitude[0];
            num_vertices = 4;
        }
        else if (num_vertices < 2)
        {
            /* skip polygon */
            continue;
        }

        /* TODO:
         * - reorder polygon such that it is turning counter-clockwise
         * - check that the polygon is convex
         *   this can be done by looking at the outer products of vec(0,k) X vec(0,k+1)
         *   this should be positive (>=0) for all 0<k<n-1
         */

        make_2d_polygon(&num_vertices, poly_latitude, poly_longitude, longitude_edges[0], &lat_min, &lat_max, &lon_min,
                        &lon_max);
        if (num_vertices == 0)
        {
            continue;
        }

        if (lat_max <= latitude_edges[0] || lat_min >= latitude_edges[num_latitude_edges - 1])
        {
            continue;
        }

        /* We loop twice to handle wrap-around situations. The second time we use longitudes + 360 */
        for (loop = 0; loop < 2; loop++)
        {
            long lat_id = -1, lon_id = -1;
            long next_lat_id, next_lon_id;
            long cumsum_offset = cumsum_index;

            if (loop == 1)
            {
                lon_min += 360;
                lon_max += 360;
                for (k = 0; k < num_vertices; k++)
                {
                    poly_longitude[k] += 360;
                }
            }

            if (lon_max <= longitude_edges[0] || lon_min >= longitude_edges[num_longitude_edges - 1])
            {
                continue;
            }

            for (j = 0; j < num_longitude_cells + 2; j++)
            {
                min_lat_id[j] = num_latitude_cells;
                max_lat_id[j] = -1;
            }
            for (j = 0; j < num_latitude_cells + 2; j++)
            {
                min_lon_id[j] = num_longitude_cells;
                max_lon_id[j] = -1;
            }

            /* iterate over all line segments and determine which grid cells are crossed */
            /* we initially add each crossing cell with weight 1 */
            harp_interpolate_find_index(num_latitude_edges, latitude_edges, poly_latitude[0], &lat_id);
            if (lat_id == num_latitude_edges)
            {
                lat_id = num_latitude_cells;
            }
            harp_interpolate_find_index(num_longitude_edges, longitude_edges, poly_longitude[0], &lon_id);
            if (lon_id == num_longitude_edges)
            {
                lon_id = num_longitude_cells;
            }
            next_lat_id = lat_id;
            next_lon_id = lon_id;
            /* add cell of starting point (if it falls within the grid) */
            if (lon_id >= 0 && lon_id < num_longitude_cells && lat_id >= 0 && lat_id < num_latitude_cells)
            {
                if (lon_id < min_lon_id[lat_id + 1] || lon_id > max_lon_id[lat_id + 1] ||
                    lat_id < min_lat_id[lon_id + 1] || lat_id > max_lat_id[lon_id + 1])
                {
                    num_latlon_index[i]++;
                    if (add_cell_index(lat_id * num_longitude_cells + lon_id, &cumsum_index, latlon_cell_index,
                                       latlon_weight) != 0)
                    {
                        goto error;
                    }
                }
            }
            if (lat_id < min_lat_id[lon_id + 1])
            {
                min_lat_id[lon_id + 1] = lat_id;
            }
            if (lat_id > max_lat_id[lon_id + 1])
            {
                max_lat_id[lon_id + 1] = lat_id;
            }
            if (lon_id < min_lon_id[lat_id + 1])
            {
                min_lon_id[lat_id + 1] = lon_id;
            }
            if (lon_id > max_lon_id[lat_id + 1])
            {
                max_lon_id[lat_id + 1] = lon_id;
            }
            for (j = 0; j < num_vertices - 1; j++)
            {
                double latitude = poly_latitude[j];
                double longitude = poly_longitude[j];
                double next_latitude = poly_latitude[j + 1];
                double next_longitude = poly_longitude[j + 1];

                /* determine grid location of end of line segment */
                harp_interpolate_find_index(num_latitude_edges, latitude_edges, poly_latitude[j + 1], &next_lat_id);
                if (next_lat_id == num_latitude_edges)
                {
                    next_lat_id = num_latitude_cells;
                }
                harp_interpolate_find_index(num_longitude_edges, longitude_edges, poly_longitude[j + 1], &next_lon_id);
                if (next_lon_id == num_longitude_edges)
                {
                    next_lon_id = num_longitude_cells;
                }
                while (lat_id != next_lat_id || lon_id != next_lon_id)
                {
                    /* determine intermediate cells that the line segment crosses */
                    if (next_lat_id > lat_id)
                    {
                        double slope = (next_longitude - longitude) / (next_latitude - latitude);

                        if (next_lon_id > lon_id &&
                            longitude + (latitude_edges[lat_id + 1] - latitude) * slope > longitude_edges[lon_id + 1])
                        {
                            /* move right */
                            latitude += (longitude_edges[lon_id + 1] - longitude) / slope;
                            longitude = longitude_edges[lon_id + 1];
                            lon_id++;
                        }
                        else if (next_lon_id < lon_id &&
                                 longitude + (latitude_edges[lat_id + 1] - latitude) * slope < longitude_edges[lon_id])
                        {
                            /* move left */
                            latitude += (longitude_edges[lon_id] - longitude) / slope;
                            longitude = longitude_edges[lon_id];
                            lon_id--;
                        }
                        else
                        {
                            /* move up */
                            longitude += (latitude_edges[lat_id + 1] - latitude) * slope;
                            latitude = latitude_edges[lat_id + 1];
                            lat_id++;
                        }
                    }
                    else if (next_lat_id < lat_id)
                    {
                        double slope = (next_longitude - longitude) / (next_latitude - latitude);

                        if (next_lon_id > lon_id &&
                            longitude + (latitude_edges[lat_id] - latitude) * slope > longitude_edges[lon_id + 1])
                        {
                            /* move right */
                            latitude += (longitude_edges[lon_id + 1] - longitude) / slope;
                            longitude = longitude_edges[lon_id + 1];
                            lon_id++;
                        }
                        else if (next_lon_id < lon_id &&
                                 longitude + (latitude_edges[lat_id] - latitude) * slope < longitude_edges[lon_id])
                        {
                            /* move left */
                            latitude += (longitude_edges[lon_id] - longitude) / slope;
                            longitude = longitude_edges[lon_id];
                            lon_id--;
                        }
                        else
                        {
                            /* move down */
                            longitude += (latitude_edges[lat_id] - latitude) * slope;
                            latitude = latitude_edges[lat_id];
                            lat_id--;
                        }
                    }
                    else
                    {
                        double slope = (next_latitude - latitude) / (next_longitude - longitude);

                        if (next_lon_id > lon_id)
                        {
                            /* move right */
                            latitude += (longitude_edges[lon_id + 1] - longitude) * slope;
                            longitude = longitude_edges[lon_id + 1];
                            lon_id++;
                        }
                        else
                        {
                            /* move left */
                            latitude += (longitude_edges[lon_id] - longitude) * slope;
                            longitude = longitude_edges[lon_id];
                            lon_id--;
                        }
                    }
                    /* add next cell (if it falls within the grid) */
                    if (lon_id >= 0 && lon_id < num_longitude_cells && lat_id >= 0 && lat_id < num_latitude_cells)
                    {
                        if (lon_id < min_lon_id[lat_id + 1] || lon_id > max_lon_id[lat_id + 1] ||
                            lat_id < min_lat_id[lon_id + 1] || lat_id > max_lat_id[lon_id + 1])
                        {
                            num_latlon_index[i]++;
                            if (add_cell_index(lat_id * num_longitude_cells + lon_id, &cumsum_index, latlon_cell_index,
                                               latlon_weight) != 0)
                            {
                                goto error;
                            }
                        }
                    }
                    if (lat_id < min_lat_id[lon_id + 1])
                    {
                        min_lat_id[lon_id + 1] = lat_id;
                    }
                    if (lat_id > max_lat_id[lon_id + 1])
                    {
                        max_lat_id[lon_id + 1] = lat_id;
                    }
                    if (lon_id < min_lon_id[lat_id + 1])
                    {
                        min_lon_id[lat_id + 1] = lon_id;
                    }
                    if (lon_id > max_lon_id[lat_id + 1])
                    {
                        max_lon_id[lat_id + 1] = lon_id;
                    }
                }
            }

            /* calculate actual weight (based on overlap fraction) for each cell we have added up to now */
            for (j = cumsum_offset; j < cumsum_index; j++)
            {
                lat_id = (*latlon_cell_index)[j] / num_longitude_cells;
                lon_id = (*latlon_cell_index)[j] - lat_id * num_longitude_cells;
                (*latlon_weight)[j] = find_weight_for_polygon_and_cell(num_vertices, poly_latitude, poly_longitude,
                                                                       temp_poly_latitude, temp_poly_longitude,
                                                                       &latitude_edges[lat_id],
                                                                       &longitude_edges[lon_id]);
            }

            /* add all grid cells that lie fully within the polygon */
            for (j = 0; j < num_latitude_cells; j++)
            {
                if (min_lon_id[j + 1] < max_lon_id[j + 1])
                {
                    for (k = min_lon_id[j + 1] + 1; k < max_lon_id[j + 1]; k++)
                    {
                        long cell_index = j * num_longitude_cells + k;

                        if (j > min_lat_id[k + 1] && j < max_lat_id[k + 1])
                        {
                            long l;

                            /* check if this cell wasn't already added due to a partial overlap */
                            for (l = cumsum_offset; l < cumsum_index; l++)
                            {
                                if (cell_index == (*latlon_cell_index)[l])
                                {
                                    break;
                                }
                            }
                            if (l == cumsum_index)
                            {
                                /* add cell with full weight */
                                num_latlon_index[i]++;
                                if (add_cell_index(cell_index, &cumsum_index, latlon_cell_index, latlon_weight) != 0)
                                {
                                    goto error;
                                }
                                (*latlon_weight)[cumsum_index - 1] =
                                    find_weight_for_polygon_and_cell(num_vertices, poly_latitude, poly_longitude,
                                                                     temp_poly_latitude, temp_poly_longitude,
                                                                     &latitude_edges[j], &longitude_edges[k]);
                            }
                        }
                    }
                }
            }
        }
    }

    free(poly_latitude);
    free(poly_longitude);
    free(temp_poly_latitude);
    free(temp_poly_longitude);
    free(min_lat_id);
    free(max_lat_id);
    free(min_lon_id);
    free(max_lon_id);

    return 0;

  error:
    if (poly_latitude != NULL)
    {
        free(poly_latitude);
    }
    if (poly_longitude != NULL)
    {
        free(poly_longitude);
    }
    if (temp_poly_latitude != NULL)
    {
        free(temp_poly_latitude);
    }
    if (temp_poly_longitude != NULL)
    {
        free(temp_poly_longitude);
    }
    if (min_lat_id != NULL)
    {
        free(min_lat_id);
    }
    if (max_lat_id != NULL)
    {
        free(max_lat_id);
    }
    if (min_lon_id != NULL)
    {
        free(min_lon_id);
    }
    if (max_lon_id != NULL)
    {
        free(max_lon_id);
    }

    return -1;
}

static int find_matching_cells_for_points(harp_variable *latitude, harp_variable *longitude, long num_latitude_edges,
                                          double *latitude_edges, long num_longitude_edges, double *longitude_edges,
                                          long *num_latlon_index, long **latlon_cell_index)
{
    long latitude_index = -1;
    long longitude_index = -1;
    long cumsum_index = 0;
    long num_elements;
    long i;

    num_elements = latitude->dimension[0];
    for (i = 0; i < num_elements; i++)
    {
        double wrapped_longitude;

        harp_interpolate_find_index(num_latitude_edges, latitude_edges, latitude->data.double_data[i], &latitude_index);
        if (latitude_index < 0 || latitude_index >= num_latitude_edges - 1)
        {
            num_latlon_index[i] = 0;
            continue;
        }
        wrapped_longitude = harp_wrap(longitude->data.double_data[i], longitude_edges[0], longitude_edges[0] + 360);
        harp_interpolate_find_index(num_longitude_edges, longitude_edges, wrapped_longitude, &longitude_index);
        if (longitude_index < 0 || longitude_index >= num_longitude_edges - 1)
        {
            num_latlon_index[i] = 0;
            continue;
        }
        num_latlon_index[i] = 1;
        if (cumsum_index % LATLON_BLOCK_SIZE == 0)
        {
            long *new_latlon_cell_index;

            new_latlon_cell_index = realloc(*latlon_cell_index, (cumsum_index + LATLON_BLOCK_SIZE) * sizeof(long));
            if (new_latlon_cell_index == NULL)
            {
                harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                               (cumsum_index + LATLON_BLOCK_SIZE) * sizeof(long), __FILE__, __LINE__);
                return -1;
            }
            *latlon_cell_index = new_latlon_cell_index;
        }
        (*latlon_cell_index)[cumsum_index] = latitude_index * (num_longitude_edges - 1) + longitude_index;
        cumsum_index++;
    }

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
 * For uncertainty variables the first order propagation rules are used (assuming no correlation for total and random uncertainty variables
 * and full correlation for systematic uncertainty variables).
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
        bintype[k] = get_binning_type(product->variable[k], 1);

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
        if (bintype[k] != binning_sum)
        {
            if (harp_variable_convert_data_type(variable, harp_type_double) != 0)
            {
                goto error;
            }
        }

        /* convert all angles to complex values [cos(x),sin(x)] and pre-multiply by existing weights */
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
                if (add_weight_variable(product, bintype, binning_sum, variable->name, variable->num_dimensions,
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

        if (bintype[k] == binning_skip || bintype[k] == binning_remove || bintype[k] == binning_sum)
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

                if (variable->data.double_data[i] < variable->data.double_data[target_index])
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

                if (variable->data.double_data[i] > variable->data.double_data[target_index])
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
                if (add_count_variable(product, bintype, binning_sum, variable->name, variable->num_dimensions,
                                       variable->dimension_type, variable->dimension, count) != 0)
                {
                    goto error;
                }
            }
            if (store_weight_variable)
            {
                if (add_weight_variable(product, bintype, binning_sum, variable->name, variable->num_dimensions,
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

        if (bintype[k] != binning_sum)
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

            /* convert angle variables back from complex values to angles */
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

/** Bin the product's variables into a spatial grid.
 * This will bin all variables with a time dimension into a three dimensional time x latitude x longitude grid.
 * Each time sample will first be allocated to a time bin defined by time_bin_index (similar to \a harp_product_bin).
 * Then within that time bin the sample will be allocated to the appropriate cell(s) in the latitude/longitude grid as
 * defined by the latitude_edges and longitude_edges variables.
 *
 * The lat/lon grid will be a fixed time-independent grid and will have 'num_latitude_edges-1' latitudes and
 * 'num_longitude_edges-1' longitudes.
 * The latitude_edges and longitude_edges arrays provide the boundaries of the grid cells in degrees and need to be
 * provided in a strict ascending order. The latitude edge values need to be between -90 and 90 and for the longitude
 * edge values the constraint is that the difference between the last and first edge should be <= 360.
 *
 * If the product has latitude_bounds {time,independent} and longitude_bounds {time,independent} variables then an area
 * binning is performed. This means that each sample will be allocated to each lat/lon grid cell based on the amount of
 * overlap. This overlap calculation will treat lines between points as straight lines within the carthesian plane
 * (i.e. using a Plate Carree projection, and not using great circle arcs between points on a sphere).
 *
 * If the product doesn't have lat/lon bounds per sample, it should have latitude {time} and longitude {time} variables.
 * The binning onto the lat/lon grid will then be a point binning. This means that each sample is allocated to only one
 * grid cell based on its lat/lon coordinate. To achieve a unique assignment, for each cell the lower edge will be
 * considered inclusive and the upper edge exclusive (except for the last cell (when there is no wrap-around)).
 *
 * The resulting value for each time/lat/lon cell will be the average of all values for that cell.
 * This will be a weighted average in case an area binning is performed and a straight average for point binning.
 * Variables with multiple dimensions will have all elements in its sub dimensions averaged on an element by element
 * basis (i.e. sub dimensions will be retained).
 *
 * Variables that have a time dimension but no unit (or using a string data type) will be removed.
 * Any existing count or weight variables will also be removed.
 *
 * For uncertainty variables the first order propagation rules are used (assuming full correlation).
 *
 * All variables that are binned are converted to a double data type. Cells that have no samples will end up with a NaN
 * value.
 *
 * A 'count' variable will be added to the product that will contain the number of samples per time bin.
 * In addition, a 'weight' variable will be added that will contain the sum of weights for the contribution to each
 * cell. If a variable contained NaN values then a variable specific weight variable will be created with only the sum
 * of weights for the non-NaN entries.
 *
 * Axis variables for the time dimension such as datetime, datetime_length, datetime_start, and datetime_stop will only
 * be binned in the time dimension (and will not gain a latitude or longitude dimension).
 *
 * \param product Product to regrid.
 * \param num_time_bins Number of target bins in the time dimension.
 * \param num_time_elements Length of bin_index array (should equal the length of the time dimension)
 * \param time_bin_index Array of target time bin index numbers (0 .. num_bins-1) for each sample in the time dimension.
 * \param num_latitude_edges Number of edges for the latitude grid (number of latitude rows = num_latitude_edges - 1)
 * \param latitude_edges latitude grid edge vales
 * \param num_longitude_edges Number of edges for the longitude grid
 *        (number of longitude columns = num_longitude_edges - 1)
 * \param longitude_edges longitude grid edge vales
 *
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_product_bin_spatial(harp_product *product, long num_time_bins, long num_time_elements,
                                         long *time_bin_index, long num_latitude_edges, double *latitude_edges,
                                         long num_longitude_edges, double *longitude_edges)
{
    long spatial_block_length = (num_latitude_edges - 1) * (num_longitude_edges - 1);
    harp_data_type data_type = harp_type_double;
    harp_dimension_type dimension_type[HARP_MAX_NUM_DIMS];
    long dimension[HARP_MAX_NUM_DIMS];
    harp_variable *latitude = NULL;
    harp_variable *longitude = NULL;
    binning_type *bintype = NULL;
    double nan_value = harp_nan();
    long *num_latlon_index = NULL;      /* number of matching latlon cells for each sample [num_time_elements] */
    long *latlon_cell_index = NULL;     /* flat latlon cell index for each matching cell for each sample [sum(num_latlon_index)] */
    double *latlon_weight = NULL;       /* weight for each matching cell for each sample [sum(num_latlon_index)] */
    long *time_index = NULL;    /* index of first contributing sample for each bin */
    long weight_size = 0;
    int32_t *bin_count = NULL;  /* number of contributing samples for each time bin [num_time_bins] */
    float *weight = NULL;       /* sum of weights per latlon cell and time [num_time_bins, num_latitude_edges-1, num_longitude_edges-1] */
    long cumsum_index;  /* index into latlon_cell_index and latlon_weight */
    int area_binning = 0;
    long i, j, k, l;

    if (product->dimension[harp_dimension_latitude] > 0 || product->dimension[harp_dimension_longitude] > 0)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "spatial binning cannot be performed on products that already "
                       "have a latitude and/or longitude dimension");
        return -1;
    }

    if (num_time_elements != product->dimension[harp_dimension_time])
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "num_time_elements (%ld) does not match time dimension length "
                       "(%ld) (%s:%u)", num_time_elements, product->dimension[harp_dimension_time], __FILE__, __LINE__);
        return -1;
    }

    for (i = 0; i < num_time_elements; i++)
    {
        if (time_bin_index[i] < 0 || time_bin_index[i] >= num_time_bins)
        {
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "time_bin_index[%ld] (%ld) should be in the range [0..%ld) "
                           "(%s:%u)", i, time_bin_index[i], num_time_bins, __FILE__, __LINE__);
            return -1;
        }
    }

    if (num_latitude_edges < 2)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "need at least 2 latitude edges to perform spatial binning");
        return -1;
    }
    if (num_longitude_edges < 2)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "need at least 2 longitude edges to perform spatial binning");
        return -1;
    }
    for (i = 0; i < num_latitude_edges; i++)
    {
        if (latitude_edges[i] < -90.0 || latitude_edges[i] > 90.0)
        {
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "latitude edge value (%lf) needs to be in the range [-90,90] "
                           "for spatial binning", latitude_edges[i]);
            return -1;
        }
    }
    for (i = 1; i < num_latitude_edges; i++)
    {
        if (latitude_edges[i] <= latitude_edges[i - 1])
        {
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT,
                           "latitude edge values need to be in strict ascending order for spatial binning");
            return -1;
        }
    }
    for (i = 1; i < num_longitude_edges; i++)
    {
        if (longitude_edges[i] <= longitude_edges[i - 1])
        {
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT,
                           "longitude edge values need to be in strict ascending order for spatial binning");
            return -1;
        }
    }
    if (longitude_edges[num_longitude_edges - 1] - longitude_edges[0] > 360)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "longitude edge range (%lf .. %lf) cannot exceed 360 degrees",
                       latitude_edges[0], longitude_edges[num_longitude_edges - 1]);
        return -1;
    }


    num_latlon_index = malloc(num_time_elements * sizeof(long));
    if (num_latlon_index == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_time_elements * sizeof(long), __FILE__, __LINE__);
        goto error;
    }

    dimension_type[0] = harp_dimension_time;
    dimension_type[1] = harp_dimension_independent;
    if (harp_product_get_derived_variable(product, "latitude_bounds", &data_type, "degree_north", 2, dimension_type,
                                          &latitude) == 0)
    {
        if (harp_product_get_derived_variable(product, "longitude_bounds", &data_type, "degree_east", 2, dimension_type,
                                              &longitude) == 0)
        {
            area_binning = 1;
            /* determine matching cells and weighting factors */
            if (find_matching_cells_and_weights_for_bounds(latitude, longitude, num_latitude_edges, latitude_edges,
                                                           num_longitude_edges, longitude_edges, num_latlon_index,
                                                           &latlon_cell_index, &latlon_weight) != 0)
            {
                harp_variable_delete(latitude);
                harp_variable_delete(longitude);
                goto error;
            }
            harp_variable_delete(longitude);
        }
        harp_variable_delete(latitude);
    }
    if (!area_binning)
    {
        if (harp_product_get_derived_variable(product, "latitude", &data_type, "degree_north", 1, dimension_type,
                                              &latitude) != 0)
        {
            goto error;
        }
        if (harp_product_get_derived_variable(product, "longitude", &data_type, "degree_east", 1, dimension_type,
                                              &longitude) != 0)
        {
            goto error;
        }
        if (find_matching_cells_for_points(latitude, longitude, num_latitude_edges, latitude_edges, num_longitude_edges,
                                           longitude_edges, num_latlon_index, &latlon_cell_index) != 0)
        {
            harp_variable_delete(latitude);
            harp_variable_delete(longitude);
            goto error;
        }
        harp_variable_delete(latitude);
        harp_variable_delete(longitude);
    }

    /* make 'bintype' big enough to also store any count/weight variables that we may want to add (i.e. 2 + factor 2) */
    bintype = malloc((2 * product->num_variables + 2) * sizeof(binning_type));
    if (bintype == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       (2 * product->num_variables + 2) * sizeof(binning_type), __FILE__, __LINE__);
        goto error;
    }
    for (k = 0; k < product->num_variables; k++)
    {
        bintype[k] = get_spatial_binning_type(product->variable[k]);

        assert(bintype[k] != binning_uncertainty);

        /* determine the maximum number of elements (as size for the 'weight' array) */
        if (bintype[k] != binning_remove && bintype[k] != binning_skip)
        {
            long total_num_elements = product->variable[k]->num_elements;

            /* is the resulting [time,latitude,longitude,...] larger than the input [time,...] ? */
            if (num_time_bins * spatial_block_length > num_time_elements)
            {
                /* use largest size (before vs. after binning) */
                total_num_elements = num_time_bins * spatial_block_length * (total_num_elements / num_time_elements);
            }
            if (total_num_elements > weight_size)
            {
                weight_size = total_num_elements;
            }
        }
    }
    time_index = malloc(num_time_bins * sizeof(long));
    if (time_index == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_time_bins * sizeof(long), __FILE__, __LINE__);
        goto error;
    }
    bin_count = malloc(num_time_bins * sizeof(int32_t));
    if (bin_count == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_time_bins * sizeof(int32_t), __FILE__, __LINE__);
        goto error;
    }
    memset(bin_count, 0, num_time_bins * sizeof(int32_t));
    weight = malloc(weight_size * sizeof(float));
    if (weight == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       weight_size * sizeof(float), __FILE__, __LINE__);
        goto error;
    }

    /* for each time bin, store the index of the first sample that contributes to the bin */
    for (i = 0; i < num_time_bins; i++)
    {
        time_index[i] = 0;      /* initialize with 0 so harp_variable_rearrange_dimension will get valid indices */
        bin_count[i] = 0;
    }
    for (i = 0; i < num_time_elements; i++)
    {
        /* only include samples that contribute to at least one grid cell */
        if (num_latlon_index[i] > 0)
        {
            if (bin_count[time_bin_index[i]] == 0)
            {
                time_index[time_bin_index[i]] = i;
            }
            bin_count[time_bin_index[i]]++;
        }
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
        if (harp_variable_convert_data_type(variable, harp_type_double) != 0)
        {
            goto error;
        }

        if (bintype[k] == binning_angle)
        {
            /* convert all angles to complex values [cos(x),sin(x)] */
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
                variable->data.double_data[i] = cos(variable->data.double_data[i]);
                variable->data.double_data[i + 1] = sin(variable->data.double_data[i + 1]);
            }
        }
    }

    /* resample data */
    product->dimension[harp_dimension_time] = num_time_bins;
    product->dimension[harp_dimension_latitude] = num_latitude_edges - 1;
    product->dimension[harp_dimension_longitude] = num_longitude_edges - 1;

    /* create global count variable */
    dimension_type[0] = harp_dimension_time;
    dimension[0] = num_time_bins;
    if (add_count_variable(product, bintype, binning_skip, NULL, 1, dimension_type, dimension, bin_count) != 0)
    {
        goto error;
    }

    /* create global weight variable */
    memset(weight, 0, num_time_bins * spatial_block_length * sizeof(float));
    cumsum_index = 0;
    for (i = 0; i < num_time_elements; i++)
    {
        long index_offset = time_bin_index[i] * spatial_block_length;

        for (l = 0; l < num_latlon_index[i]; l++)
        {
            weight[index_offset + latlon_cell_index[cumsum_index]] += area_binning ? latlon_weight[cumsum_index] : 1;
            cumsum_index++;
        }
    }
    dimension_type[0] = harp_dimension_time;
    dimension[0] = num_time_bins;
    dimension_type[1] = harp_dimension_latitude;
    dimension[1] = num_latitude_edges - 1;
    dimension_type[2] = harp_dimension_longitude;
    dimension[2] = num_longitude_edges - 1;
    if (add_weight_variable(product, bintype, binning_skip, NULL, 3, dimension_type, dimension, weight) != 0)
    {
        goto error;
    }

    /* sum up all samples into spatial bins (replacing variables) and create weight variables where needed */
    for (k = 0; k < product->num_variables; k++)
    {
        harp_variable *variable;
        long num_sub_elements;

        if (bintype[k] == binning_skip || bintype[k] == binning_remove)
        {
            continue;
        }

        variable = product->variable[k];
        assert(variable->dimension[0] == num_time_elements);
        num_sub_elements = variable->num_elements / num_time_elements;

        if (bintype[k] == binning_time_max || bintype[k] == binning_time_min || bintype[k] == binning_time_average)
        {
            /* datetime variables are only binned temporally, not spatially */
            if (bintype[k] == binning_time_min)
            {
                /* take minimum of all values per bin */
                for (i = 0; i < num_time_elements; i++)
                {
                    if (num_latlon_index[i] > 0)
                    {
                        long target_index = time_index[time_bin_index[i]];

                        if (variable->data.double_data[i] < variable->data.double_data[target_index])
                        {
                            variable->data.double_data[target_index] = variable->data.double_data[i];
                        }
                    }
                }
            }
            else if (bintype[k] == binning_time_max)
            {
                /* take maximum of all values per bin */
                for (i = 0; i < num_time_elements; i++)
                {
                    if (num_latlon_index[i] > 0)
                    {
                        long target_index = time_index[time_bin_index[i]];

                        if (variable->data.double_data[i] > variable->data.double_data[target_index])
                        {
                            variable->data.double_data[target_index] = variable->data.double_data[i];
                        }
                    }
                }
            }
            else
            {
                /* sum up all values of a bin into the location of the first sample */
                /* we don't perform NaN filtering for datetime values (these should not be NaN) */
                for (i = 0; i < num_time_elements; i++)
                {
                    if (num_latlon_index[i] > 0)
                    {
                        long target_index = time_index[time_bin_index[i]];

                        if (target_index != i)
                        {
                            variable->data.double_data[target_index] += variable->data.double_data[i];
                        }
                    }
                }
            }
            /* resample the time dimension to the target bins */
            /* this uses the first sample for empty bins, which we set to NaN as a next step */
            if (harp_variable_rearrange_dimension(variable, 0, num_time_bins, time_index) != 0)
            {
                goto error;
            }
            /* set all empty bins to NaN and divide by sample count for the average */
            for (i = 0; i < variable->num_elements; i++)
            {
                if (bin_count[i] == 0)
                {
                    variable->data.double_data[i] = nan_value;
                }
                else if (bintype[k] == binning_time_average)
                {
                    variable->data.double_data[i] /= bin_count[i];
                }
            }
        }
        else
        {
            harp_variable *new_variable = NULL;
            int store_weight_variable = 0;

            assert(bintype[k] == binning_average || bintype[k] == binning_angle);

            /* we need to create a variable that includes the lat/lon dimensions and uses the binned time dimension */
            if (variable->num_dimensions + 2 >= HARP_MAX_NUM_DIMS)
            {
                harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "too many dimensions (%d) for variables %s to perform "
                               "spatial binning", variable->num_dimensions, variable->name);
                goto error;
            }
            dimension_type[0] = harp_dimension_time;
            dimension[0] = num_time_bins;
            dimension_type[1] = harp_dimension_latitude;
            dimension[1] = num_latitude_edges - 1;
            dimension_type[2] = harp_dimension_longitude;
            dimension[2] = num_longitude_edges - 1;
            for (i = 1; i < variable->num_dimensions; i++)
            {
                dimension_type[i + 2] = variable->dimension_type[i];
                dimension[i + 2] = variable->dimension[i];
            }
            if (harp_variable_new(variable->name, variable->data_type, variable->num_dimensions + 2, dimension_type,
                                  dimension, &new_variable) != 0)
            {
                goto error;
            }
            if (harp_variable_copy_attributes(variable, new_variable) != 0)
            {
                harp_variable_delete(new_variable);
                goto error;
            }

            /* sum up all values per cell */
            memset(weight, 0, weight_size * sizeof(float));
            cumsum_index = 0;
            for (i = 0; i < num_time_elements; i++)
            {
                long index_offset = time_bin_index[i] * spatial_block_length;

                for (l = 0; l < num_latlon_index[i]; l++)
                {
                    long target_index = index_offset + latlon_cell_index[cumsum_index];
                    double sample_weight = area_binning ? latlon_weight[cumsum_index] : 1;

                    if (bintype[k] == binning_angle)
                    {
                        /* for angle variables we use one weight element per complex pair */
                        for (j = 0; j < num_sub_elements; j += 2)
                        {
                            if (!harp_isnan(variable->data.double_data[i * num_sub_elements + j]))
                            {
                                weight[(target_index * num_sub_elements + j) / 2] += sample_weight;
                                new_variable->data.double_data[target_index * num_sub_elements + j] +=
                                    sample_weight * variable->data.double_data[i * num_sub_elements + j];
                                new_variable->data.double_data[target_index * num_sub_elements + j + 1] +=
                                    sample_weight * variable->data.double_data[i * num_sub_elements + j + 1];
                            }
                        }
                    }
                    else
                    {
                        for (j = 0; j < num_sub_elements; j++)
                        {
                            if (!harp_isnan(variable->data.double_data[i * num_sub_elements + j]))
                            {
                                weight[target_index * num_sub_elements + j] += sample_weight;
                                new_variable->data.double_data[target_index * num_sub_elements + j] +=
                                    sample_weight * variable->data.double_data[i * num_sub_elements + j];
                            }
                            else
                            {
                                store_weight_variable = 1;
                            }
                        }
                    }
                    cumsum_index++;
                }
            }

            /* replace variable in product with new variable */
            product->variable[k] = new_variable;
            harp_variable_delete(variable);
            variable = new_variable;

            /* post-process variable */
            if (bintype[k] == binning_angle)
            {
                /* convert angle variables back from complex values to angles */
                for (i = 0; i < variable->num_elements; i += 2)
                {
                    if (weight[i / 2] == 0)
                    {
                        variable->data.double_data[i] = nan_value;
                    }
                    else
                    {
                        double x = variable->data.double_data[i];
                        double y = variable->data.double_data[i + 1];

                        weight[i / 2] = sqrt(x * x + y * y);
                        variable->data.double_data[i] = atan2(y, x);
                    }
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
                store_weight_variable = 1;
            }
            else
            {
                for (i = 0; i < variable->num_elements; i++)
                {
                    if (weight[i] == 0)
                    {
                        variable->data.double_data[i] = nan_value;
                    }
                    else if (bintype[k] == binning_average)
                    {
                        /* divide by the sum of the weights */
                        variable->data.double_data[i] /= weight[i];
                    }
                }
            }

            if (store_weight_variable)
            {
                if (add_weight_variable(product, bintype, binning_skip, variable->name, variable->num_dimensions,
                                        variable->dimension_type, variable->dimension, weight) != 0)
                {
                    goto error;
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
    free(time_index);
    free(bin_count);
    free(num_latlon_index);
    if (latlon_cell_index != NULL)
    {
        free(latlon_cell_index);
    }
    if (latlon_weight != NULL)
    {
        free(latlon_weight);
    }

    /* add latitude_bounds and longitude_bounds variables */
    dimension_type[0] = harp_dimension_latitude;
    dimension[0] = num_latitude_edges - 1;
    dimension_type[1] = harp_dimension_independent;
    dimension[1] = 2;
    if (harp_variable_new("latitude_bounds", harp_type_double, 2, dimension_type, dimension, &latitude) != 0)
    {
        return -1;
    }
    for (i = 0; i < dimension[0]; i++)
    {
        latitude->data.double_data[2 * i] = latitude_edges[i];
        latitude->data.double_data[2 * i + 1] = latitude_edges[i + 1];
    }
    if (harp_product_add_variable(product, latitude) != 0)
    {
        harp_variable_delete(latitude);
        return -1;
    }
    if (harp_variable_set_unit(latitude, HARP_UNIT_LATITUDE) != 0)
    {
        return -1;
    }

    dimension_type[0] = harp_dimension_longitude;
    dimension[0] = num_longitude_edges - 1;
    if (harp_variable_new("longitude_bounds", harp_type_double, 2, dimension_type, dimension, &longitude) != 0)
    {
        return -1;
    }
    for (i = 0; i < dimension[0]; i++)
    {
        longitude->data.double_data[2 * i] = longitude_edges[i];
        longitude->data.double_data[2 * i + 1] = longitude_edges[i + 1];
    }
    if (harp_product_add_variable(product, longitude) != 0)
    {
        harp_variable_delete(longitude);
        return -1;
    }
    if (harp_variable_set_unit(longitude, HARP_UNIT_LONGITUDE) != 0)
    {
        return -1;
    }

    return 0;

  error:
    if (bintype != NULL)
    {
        free(bintype);
    }
    if (time_index != NULL)
    {
        free(time_index);
    }
    if (bin_count != NULL)
    {
        free(bin_count);
    }
    if (weight != NULL)
    {
        free(weight);
    }
    if (num_latlon_index != NULL)
    {
        free(num_latlon_index);
    }
    if (latlon_cell_index != NULL)
    {
        free(latlon_cell_index);
    }
    if (latlon_weight != NULL)
    {
        free(latlon_weight);
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
        if (get_binning_type(variable[k], 1) == binning_remove)
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

/** Perform a spatial binning such that all samples end up in a single time bin.
 *
 * \param product Product to regrid.
 * \param num_latitude_edges Number of edges for the latitude grid (number of latitude rows = num_latitude_edges - 1)
 * \param latitude_edges latitude grid edge vales
 * \param num_longitude_edges Number of edges for the longitude grid
 *        (number of longitude columns = num_longitude_edges - 1)
 * \param longitude_edges longitude grid edge vales
 *
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
int harp_product_bin_spatial_full(harp_product *product, long num_latitude_edges, double *latitude_edges,
                                  long num_longitude_edges, double *longitude_edges)
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

    if (harp_product_bin_spatial(product, 1, num_elements, bin_index, num_latitude_edges, latitude_edges,
                                 num_longitude_edges, longitude_edges) != 0)
    {
        free(bin_index);
        return -1;
    }

    free(bin_index);
    return 0;
}
