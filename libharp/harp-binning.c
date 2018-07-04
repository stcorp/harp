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
    binning_sum,        /* only used for int32_t data */
    binning_angle,      /* will use averaging using complex values */
    binning_time_min,
    binning_time_max,
    binning_time_average,
    binning_time_sum
} binning_type;


static binning_type get_binning_type(harp_variable *variable)
{
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
        long variable_name_length = strlen(variable->name);

        /* '...count' variables are just summed up, but only if they are unitless and use an int32 data type */
        if (variable->data_type == harp_type_int32 && variable_name_length >= 5 &&
            strcmp(&variable->name[variable_name_length - 5], "count") == 0)
        {
            if (strstr(variable->name, "latitude") != NULL || strstr(variable->name, "longitude") != NULL ||
                strstr(variable->name, "angle") != NULL || strstr(variable->name, "direction") != NULL)
            {
                /* we can't propagate average of angles (since we would also need the 'magnitudes' for this) */
                /* so just remove any counts for angles */
                return binning_remove;
            }
            return binning_sum;
        }

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

static binning_type get_spatial_binning_type(harp_variable *variable, int area_binning)
{
    binning_type type = get_binning_type(variable);

    if (type != binning_remove && type != binning_skip)
    {
        /* remove all latitude/longitude variables */
        if (strstr(variable->name, "latitude") != NULL || strstr(variable->name, "longitude") != NULL)
        {
            return binning_remove;
        }
        if (type == binning_sum)
        {
            /* count variables */
            if (strcmp(variable->name, "datetime_count") == 0)
            {
                if (variable->num_dimensions != 1)
                {
                    return binning_remove;
                }
                return binning_time_sum;
            }
            if (area_binning)
            {
                return binning_remove;
            }
        }
        if (strcmp(variable->name, "datetime") == 0 || strcmp(variable->name, "datetime_length") == 0)
        {
            if (variable->num_dimensions != 1)
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
    if (bintype[index] != binning_sum)
    {
        return 0;
    }
    if (product->variable[index]->data_type != harp_type_int32 ||
        product->variable[index]->num_dimensions != variable->num_dimensions)
    {
        bintype[index] = binning_remove;
        return 0;
    }

    /* make sure that the dimensions of the count variable match the dimensions of the given variable */
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

/* get count values for each element in the provided variable.
 * if a '<variable>_count' or 'count' variable exists then 'count' will be populated and the return value will be 1.
 * if no applicable count variable could be found then the return value will be 0.
 * the return value is -1 when an error is encountered.
 */
static int get_count_for_variable(harp_product *product, harp_variable *variable, binning_type *bintype, int32_t *count)
{
    harp_variable *count_variable = NULL;
    long i, j;

    if (get_count_variable_for_variable(product, variable, bintype, &count_variable) != 0)
    {
        return -1;
    }

    if (count_variable == NULL && harp_product_has_variable(product, "count"))
    {
        int index;

        if (harp_product_get_variable_index_by_name(product, "count", &index) != 0)
        {
            return -1;
        }
        if (bintype[index] == binning_sum)
        {
            if (product->variable[index]->data_type != harp_type_int32)
            {
                bintype[index] = binning_remove;
            }
            else
            {
                count_variable = product->variable[index];

                if (product->variable[index]->num_dimensions > variable->num_dimensions)
                {
                    /* make sure that the first dimensions of the count variable match the dimensions of the variable */
                    for (i = 0; i < variable->num_dimensions; i++)
                    {
                        if (count_variable->dimension_type[i] != variable->dimension_type[i] ||
                            count_variable->dimension[i] != variable->dimension[i])
                        {
                            bintype[index] = binning_remove;
                            return 0;
                        }
                    }
                }
                else
                {
                    /* make sure that the dimensions of the count variable match the first dimensions of the variable */
                    for (i = 0; i < count_variable->num_dimensions; i++)
                    {
                        if (count_variable->dimension_type[i] != variable->dimension_type[i] ||
                            count_variable->dimension[i] != variable->dimension[i])
                        {
                            bintype[index] = binning_remove;
                            return 0;
                        }
                    }
                }
            }
        }

    }

    if (count_variable == NULL)
    {
        return 0;
    }

    /* store data into count parameter */
    if (variable->num_elements == count_variable->num_elements)
    {
        memcpy(count, count_variable->data.int32_data, count_variable->num_elements * sizeof(int32_t));
    }
    else if (count_variable->num_elements > variable->num_elements)
    {
        long num_sub_elements = count_variable->num_elements / variable->num_elements;

        for (i = 0; i < variable->num_elements; i++)
        {
            count[i] = 0;
            for (j = 0; j < num_sub_elements; j++)
            {
                count[i] = count_variable->data.int32_data[i * num_sub_elements + j];
            }
        }
    }
    else
    {
        long num_sub_elements = variable->num_elements / count_variable->num_elements;

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

static int add_count_variable(harp_product *product, binning_type *bintype, binning_type target_bintype,
                              const char *variable_name, int num_dimensions, harp_dimension_type *dimension_type,
                              long *dimension, int32_t *count)
{
    char count_variable_name[MAX_NAME_LENGTH];
    harp_variable *variable;
    int index;

    if (variable_name != NULL)
    {
        snprintf(count_variable_name, MAX_NAME_LENGTH, "%s_count", variable_name);
    }
    else
    {
        strcpy(count_variable_name, "count");
    }

    if (!harp_product_has_variable(product, count_variable_name))
    {
        if (harp_variable_new(count_variable_name, harp_type_int32, num_dimensions, dimension_type, dimension,
                              &variable) != 0)
        {
            return -1;
        }
        memcpy(variable->data.int32_data, count, variable->num_elements * sizeof(int32_t));
        if (harp_product_add_variable(product, variable) != 0)
        {
            harp_variable_delete(variable);
            return -1;
        }
        bintype[product->num_variables - 1] = target_bintype;
    }
    else
    {
        if (harp_product_get_variable_index_by_name(product, count_variable_name, &index) != 0)
        {
            return -1;
        }
        if (bintype[index] == binning_remove)
        {
            /* if the existing count variable was scheduled for removal than replace it with a new one */
            if (harp_variable_new(count_variable_name, harp_type_int32, num_dimensions, dimension_type, dimension,
                                  &variable) != 0)
            {
                return -1;
            }
            memcpy(variable->data.int32_data, count, variable->num_elements * sizeof(int32_t));
            if (harp_product_replace_variable(product, variable) != 0)
            {
                harp_variable_delete(variable);
                return -1;
            }
            bintype[index] = target_bintype;
        }
        assert(bintype[index] = target_bintype);
        /* if the count variable already exists and does not get removed then we assume it is correct/consistent
         * (i.e. existing count=0 <-> variable=NaN) */
    }

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

/* latitude_edges and longitude_edges and should contain just 2 times (bounds of the cell) */
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
    /* it needs to be able to hold three times the amout of poinst as the input polygon */
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
        if (num_vertices < 3)
        {
            /* skip polygon */
            continue;
        }

        /* - reorder polygon such that it is turning counter-clockwise
         * - check that the polygon is convex
         * this can be done by looking at the outer products of vec(0,k) X vec(0,k+1)
         *   this should be positive (>=0) for all 0<k<n-1
         * n should be > 2 (i.e. bounds needs to be at least a triangle)
         */
        /* the area of a polygon is equal to 0.5 * sum(k=1..n-1, vec(0,k) X vec(0,k+1)) */

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

            /* iterate over all line segements and determine which grid cells are crossed */
            /* we initially add each crossing cell with weight 0 */
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
            for (j = cumsum_index - num_latlon_index[i]; j < cumsum_index; j++)
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
                            /* add cell with full weight */
                            num_latlon_index[i]++;
                            if (add_cell_index(cell_index, &cumsum_index, latlon_cell_index, latlon_weight) != 0)
                            {
                                goto error;
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
 * The resulting value for each variable will be the average of all values for the bin (using existing count variables
 * as weighting factors where available).
 * Variables with multiple dimensions will have all elements in the sub dimensions averaged on an element by element
 * basis.
 *
 * Variables that have a time dimension but no unit (or using a string data type) will be removed.
 *
 * All variables that are binned (except existing 'count' variables) are converted to a double data type.
 * Bins that have no samples will end up with a NaN value.
 *
 * If the product did not already have a 'count' variable then a 'count' variable will be added to the product that
 * will contain the number of samples per bin.
 *
 * Only non-NaN values will contribute to a bin. If there are NaN values then a separate variable-specific count
 * variable will be created that will contain the number of non-NaN values that contributed to each bin. This
 * count variable will have the same dimensions as the variable it provides the count for.
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
    long filtered_count_size = 0;
    int32_t *filtered_count = NULL;
    int32_t *count = NULL;
    long *index = NULL;
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

    /* make 'bintype' big enough to also store any count variables that we may want to add (i.e. 1 + factor 2) */
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

        /* determine the maximum number of elements (as size for the 'filtered_count' array) */
        if (bintype[k] != binning_remove && bintype[k] != binning_skip)
        {
            long total_num_elements = product->variable[k]->num_elements;

            if (num_bins > num_elements)
            {
                /* use longest time dimension (before vs. after binning) */
                total_num_elements = num_bins * (total_num_elements / num_elements);
            }
            if (total_num_elements > filtered_count_size)
            {
                filtered_count_size = total_num_elements;
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
    count = malloc(num_bins * sizeof(int32_t));
    if (count == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_bins * sizeof(int32_t), __FILE__, __LINE__);
        goto error;
    }
    filtered_count = malloc(filtered_count_size * sizeof(int32_t));
    if (filtered_count == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       filtered_count_size * sizeof(int32_t), __FILE__, __LINE__);
        goto error;
    }

    /* for each bin, store the index of the first sample that contributes to the bin */
    for (i = 0; i < num_bins; i++)
    {
        index[i] = 0;   /* initialize with 0 so harp_variable_rearrange_dimension will get valid indices for all bins */
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

    /* pre-process all variables */
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

        /* convert variables to double */
        if (bintype[k] != binning_sum)
        {
            if (harp_variable_convert_data_type(variable, harp_type_double) != 0)
            {
                goto error;
            }
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

        /* pre-multiply variables by existing counts */
        if (bintype[k] == binning_average)
        {
            int result;

            result = get_count_for_variable(product, variable, bintype, filtered_count);
            if (result < 0)
            {
                goto error;
            }
            if (result == 1)
            {
                /* multiply by the count */
                for (i = 0; i < variable->num_elements; i++)
                {
                    variable->data.double_data[i] *= filtered_count[i];
                }
            }
        }
    }

    /* sum up all samples into bins (in place) and create count variables where needed */
    for (k = 0; k < product->num_variables; k++)
    {
        harp_variable *variable;
        long num_sub_elements;

        if (bintype[k] == binning_skip || bintype[k] == binning_remove)
        {
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
        else
        {
            int store_count_variable = 0;

            /* sum up all values of a bin into the location of the first sample */

            if (bintype[k] == binning_angle)
            {
                /* for angle variables we use one filtered_count element per complex pair (and don't pre-multiply) */
                for (i = 0; i < variable->num_elements / 2; i++)
                {
                    filtered_count[i] = 1;
                }
            }
            else if (variable->data_type != harp_type_int32)
            {
                int result = 0;

                result = get_count_for_variable(product, variable, bintype, filtered_count);
                if (result < 0)
                {
                    goto error;
                }
                if (result == 0)
                {
                    /* if there is no pre-existing count then set all counts to 1 */
                    for (i = 0; i < variable->num_elements; i++)
                    {
                        filtered_count[i] = 1;
                    }
                }
            }

            for (i = 0; i < num_elements; i++)
            {
                long target_index = index[bin_index[i]];

                if (target_index != i)
                {
                    if (bintype[k] == binning_angle)
                    {
                        /* for angle variables we use one filtered_count element per complex pair */
                        for (j = 0; j < num_sub_elements; j += 2)
                        {
                            if (harp_isnan(variable->data.double_data[i * num_sub_elements + j]))
                            {
                                filtered_count[(i * num_sub_elements + j) / 2] = 0;
                                store_count_variable = 1;
                            }
                            else
                            {
                                variable->data.double_data[target_index * num_sub_elements + j] +=
                                    variable->data.double_data[i * num_sub_elements + j];
                                variable->data.double_data[target_index * num_sub_elements + j + 1] +=
                                    variable->data.double_data[i * num_sub_elements + j + 1];
                            }
                        }
                    }
                    else if (variable->data_type == harp_type_int32)
                    {
                        for (j = 0; j < num_sub_elements; j++)
                        {
                            variable->data.int32_data[target_index * num_sub_elements + j] +=
                                variable->data.int32_data[i * num_sub_elements + j];
                        }
                    }
                    else
                    {
                        for (j = 0; j < num_sub_elements; j++)
                        {
                            if (harp_isnan(variable->data.double_data[i * num_sub_elements + j]))
                            {
                                filtered_count[i * num_sub_elements + j] = 0;
                                store_count_variable = 1;
                            }
                            else
                            {
                                variable->data.double_data[target_index * num_sub_elements + j] +=
                                    variable->data.double_data[i * num_sub_elements + j];
                            }
                        }
                    }
                }
                else if (variable->data_type != harp_type_int32)
                {
                    if (bintype[k] == binning_angle)
                    {
                        for (j = 0; j < num_sub_elements; j += 2)
                        {
                            if (harp_isnan(variable->data.double_data[target_index * num_sub_elements + j]))
                            {
                                filtered_count[(target_index * num_sub_elements + j) / 2] = 0;
                                variable->data.double_data[target_index * num_sub_elements + j] = 0;
                                variable->data.double_data[target_index * num_sub_elements + j + 1] = 0;
                                store_count_variable = 1;
                            }
                        }
                    }
                    else
                    {
                        for (j = 0; j < num_sub_elements; j++)
                        {
                            if (harp_isnan(variable->data.double_data[target_index * num_sub_elements + j]))
                            {
                                filtered_count[target_index * num_sub_elements + j] = 0;
                                variable->data.double_data[target_index * num_sub_elements + j] = 0;
                                store_count_variable = 1;
                            }
                        }
                    }
                }
            }
            if (store_count_variable)
            {
                if (bintype[k] == binning_angle)
                {
                    /* we store the count for angles (temporarily) to be able to set the count=0 values to NaN */
                    /* don't include the 'complex' dimension for the count variable for angles */
                    if (add_count_variable(product, bintype, binning_sum, variable->name, variable->num_dimensions - 1,
                                           variable->dimension_type, variable->dimension, filtered_count) != 0)
                    {
                        goto error;
                    }
                }
                else
                {
                    if (add_count_variable(product, bintype, binning_sum, variable->name, variable->num_dimensions,
                                           variable->dimension_type, variable->dimension, filtered_count) != 0)
                    {
                        goto error;
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
        if (harp_variable_rearrange_dimension(product->variable[k], 0, num_bins, index) != 0)
        {
            goto error;
        }
    }

    /* update product dimensions */
    product->dimension[harp_dimension_time] = num_bins;

    /* add global count variable if it didn't exist yet */
    dimension_type[0] = harp_dimension_time;
    if (add_count_variable(product, bintype, binning_sum, NULL, 1, dimension_type, &num_bins, count) != 0)
    {
        goto error;
    }

    /* post-process all variables */
    for (k = 0; k < product->num_variables; k++)
    {
        harp_variable *variable;
        long num_sub_elements;
        int count_applied = 0;

        if (bintype[k] == binning_skip || bintype[k] == binning_remove)
        {
            continue;
        }

        variable = product->variable[k];
        num_sub_elements = variable->num_elements / num_elements;

        if (bintype[k] == binning_angle)
        {
            harp_variable *count_variable;

            /* convert angle variables back from complex values to angles */
            for (i = 0; i < variable->num_elements; i += 2)
            {
                variable->data.double_data[i] = atan2(variable->data.double_data[i + 1], variable->data.double_data[i]);
            }
            if (harp_variable_remove_dimension(variable, variable->num_dimensions - 1, 0) != 0)
            {
                goto error;
            }
            num_sub_elements /= 2;
            /* convert all angles back to the original unit */
            if (harp_convert_unit("rad", variable->unit, variable->num_elements, variable->data.double_data) != 0)
            {
                goto error;
            }

            /* set values to NaN if count==0 */
            if (get_count_variable_for_variable(product, variable, bintype, &count_variable) != 0)
            {
                goto error;
            }
            if (count_variable != NULL)
            {
                double nan_value = harp_nan();
                int count_index;

                for (i = 0; i < variable->num_elements; i++)
                {
                    if (count_variable->data.int32_data[i] == 0)
                    {
                        variable->data.double_data[i] = nan_value;
                    }
                }
                count_applied = 1;

                /* remove the count variable for angles, since it is meaningless for further propagation of averages */
                if (harp_product_get_variable_index_by_name(product, count_variable->name, &count_index) != 0)
                {
                    goto error;
                }
                bintype[count_index] = binning_remove;
            }
        }

        /* divide variables by the sample count and/or set values to NaN if count==0 */
        if (bintype[k] == binning_average)
        {
            double nan_value = harp_nan();
            int result;

            result = get_count_for_variable(product, variable, bintype, filtered_count);
            if (result < 0)
            {
                goto error;
            }
            if (result == 1)
            {
                /* divide by the count (or set value to NaN if count==0) */
                for (i = 0; i < variable->num_elements; i++)
                {
                    if (filtered_count[i] == 0)
                    {
                        variable->data.double_data[i] = nan_value;
                    }
                    else if (filtered_count[i] > 1)
                    {
                        variable->data.double_data[i] /= filtered_count[i];
                    }
                }
                count_applied = 1;
            }
        }

        /* set all empty bins to NaN (for double) or 0 (for int32) */
        if (!count_applied)
        {
            for (i = 0; i < num_bins; i++)
            {
                if (count[i] == 0)
                {
                    double nan_value = harp_nan();

                    if (variable->data_type == harp_type_int32)
                    {
                        for (j = 0; j < num_sub_elements; j++)
                        {
                            variable->data.int32_data[i * num_sub_elements + j] = 0;
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
    free(filtered_count);
    free(count);
    free(index);

    return 0;

  error:
    if (bintype != NULL)
    {
        free(bintype);
    }
    if (filtered_count != NULL)
    {
        free(filtered_count);
    }
    if (count != NULL)
    {
        free(count);
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
 * considered inclusive and the upper edge exclusive (except for the last cell (when there is now wrap-around)).
 *
 * The resulting value for each time/lat/lon cell will be the average of all values for that cell.
 * This will be a weighted average in case an area binning is performed and a straight average for point binning.
 * Variables with multiple dimensions will have all elements in its sub dimensions averaged on an element by element
 * basis (i.e. sub dimensions will be retained).
 *
 * Variables that have a time dimension but no unit (or using a string data type) will be removed.
 *
 * All variables that are binned (except existing 'count' variables) are converted to a double data type.
 * Cells that have no samples will end up with a NaN value.
 *
 * In case of point binning, if the product did not already have a 'count' variable then a 'count' variable will be
 * added to the product that will contain the number of samples per cell. In case of area binning, any existing 'count'
 * variables will be removed.
 *
 * Axis variables for the time dimension such as datetime, datetime_length, datetime_start, and datetime_stop will only
 * be binned in the time dimension (but will not gain a latitude or longitude dimension)
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
    harp_dimension_type dimension_type[HARP_MAX_NUM_DIMS];
    long dimension[HARP_MAX_NUM_DIMS];
    harp_variable *latitude = NULL;
    harp_variable *longitude = NULL;
    binning_type *bintype = NULL;
    long *num_latlon_index = NULL;      /* number of matching latlon cells for each sample [num_time_elements] */
    long *latlon_cell_index = NULL;     /* flat latlon cell index for each matching cell for each sample [sum(num_latlon_index)] */
    double *latlon_weight = NULL;       /* weight for each matching cell for each sample [sum(num_latlon_index)] */
    long *time_index = NULL;    /* index of first contributing sample for each bin */
    int32_t *time_count = NULL; /* number of samples per time bin */
    long filtered_count_size = 0;
    int32_t *filtered_count = NULL;
    double *filtered_weight = NULL;
    int32_t *count = NULL;      /* number of samples per latlon cell for each time bin [num_time_bins, num_latitude_edges-1, num_longitude_edges-1] */
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
    if (harp_product_get_derived_variable(product, "latitude_bounds", NULL, "degree_north", 2, dimension_type,
                                          &latitude) == 0)
    {
        if (harp_product_get_derived_variable(product, "longitude_bounds", NULL, "degree_east", 2, dimension_type,
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
        if (harp_product_get_derived_variable(product, "latitude", NULL, "degree_north", 1, dimension_type,
                                              &latitude) != 0)
        {
            goto error;
        }
        if (harp_product_get_derived_variable(product, "longitude", NULL, "degree_east", 1, dimension_type,
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
        bintype[k] = get_spatial_binning_type(product->variable[k], area_binning);

        /* determine the maximum number of elements (as size for the 'filtered_count' array) */
        if (bintype[k] != binning_remove && bintype[k] != binning_skip)
        {
            long total_num_elements = product->variable[k]->num_elements;

            /* is the resulting [time,latitude,longitude,...] larger than the input [time,...] ? */
            if (num_time_bins * spatial_block_length > num_time_elements)
            {
                /* use largest size (before vs. after binning) */
                total_num_elements = num_time_bins * spatial_block_length * (total_num_elements / num_time_elements);
            }
            if (total_num_elements > filtered_count_size)
            {
                filtered_count_size = total_num_elements;
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
    time_count = malloc(num_time_bins * sizeof(long));
    if (time_count == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_time_bins * sizeof(long), __FILE__, __LINE__);
        goto error;
    }
    count = malloc(num_time_bins * spatial_block_length * sizeof(int32_t));
    if (count == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_time_bins * spatial_block_length * sizeof(int32_t), __FILE__, __LINE__);
        goto error;
    }
    memset(count, 0, num_time_bins * spatial_block_length * sizeof(int32_t));
    filtered_count = malloc(filtered_count_size * sizeof(int32_t));
    if (filtered_count == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       filtered_count_size * sizeof(int32_t), __FILE__, __LINE__);
        goto error;
    }
    if (area_binning)
    {
        filtered_weight = malloc(filtered_count_size * sizeof(double));
        if (filtered_weight == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           filtered_count_size * sizeof(double), __FILE__, __LINE__);
            goto error;
        }
    }

    /* for each time bin, store the index of the first sample that contributes to the bin */
    for (i = 0; i < num_time_bins; i++)
    {
        time_index[i] = 0;
        time_count[i] = 0;
    }
    for (i = 0; i < num_time_elements; i++)
    {
        if (time_count[time_bin_index[i]] == 0)
        {
            time_index[time_bin_index[i]] = i;
        }
        time_count[time_bin_index[i]]++;
    }

    /* determine number of samples (unfiltered for NaN) that contribute to each lat/lon cell */
    cumsum_index = 0;
    for (i = 0; i < num_time_elements; i++)
    {
        long index_offset = time_bin_index[i] * spatial_block_length;

        for (l = 0; l < num_latlon_index[i]; l++)
        {
            count[index_offset + latlon_cell_index[cumsum_index]] += 1;
            cumsum_index++;
        }
    }

    /* pre-process all variables */
    for (k = 0; k < product->num_variables; k++)
    {
        harp_variable *variable;
        long num_sub_elements;

        if (bintype[k] == binning_skip || bintype[k] == binning_remove)
        {
            continue;
        }

        variable = product->variable[k];
        num_sub_elements = variable->num_elements / num_time_elements;

        /* convert variables to double */
        if (bintype[k] != binning_sum && bintype[k] != binning_time_sum)
        {
            if (harp_variable_convert_data_type(variable, harp_type_double) != 0)
            {
                goto error;
            }
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

        /* pre-multiply variables by existing counts */
        if (bintype[k] == binning_time_average || (bintype[k] == binning_average && !area_binning))
        {
            int result;

            result = get_count_for_variable(product, variable, bintype, filtered_count);
            if (result < 0)
            {
                goto error;
            }
            if (result == 1)
            {
                /* multiply by the count */
                for (i = 0; i < variable->num_elements; i++)
                {
                    variable->data.double_data[i] *= filtered_count[i];
                }
            }
        }
    }

    /* sum up all samples into spatial bins (replacing variables) and create count variables where needed */
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

        if (bintype[k] == binning_time_max || bintype[k] == binning_time_min || bintype[k] == binning_time_sum ||
            bintype[k] == binning_time_average)
        {
            /* datetime variables are only binned temporally, not spatially */
            if (harp_variable_convert_data_type(variable, harp_type_double) != 0)
            {
                goto error;
            }
            if (bintype[k] == binning_time_min)
            {
                /* take minimum of all values per bin */
                for (i = 0; i < num_time_elements; i++)
                {
                    long target_index = time_index[time_bin_index[i]];

                    if (variable->data.double_data[i] < variable->data.double_data[target_index])
                    {
                        variable->data.double_data[target_index] = variable->data.double_data[i];
                    }
                }
            }
            else if (bintype[k] == binning_time_max)
            {
                /* take maximum of all values per bin */
                for (i = 0; i < num_time_elements; i++)
                {
                    long target_index = time_index[time_bin_index[i]];

                    if (variable->data.double_data[i] > variable->data.double_data[target_index])
                    {
                        variable->data.double_data[target_index] = variable->data.double_data[i];
                    }
                }
            }
            else if (bintype[k] == binning_time_sum)
            {
                /* sum up all values of a bin into the location of the first sample */
                for (i = 0; i < num_time_elements; i++)
                {
                    long target_index = time_index[time_bin_index[i]];

                    if (i != target_index)
                    {
                        variable->data.int32_data[target_index] += variable->data.int32_data[i];
                    }
                }
            }
            else
            {
                int store_count_variable = 0;
                int result = 0;

                result = get_count_for_variable(product, variable, bintype, filtered_count);
                if (result < 0)
                {
                    goto error;
                }
                if (result == 0)
                {
                    /* if there is no pre-existing count then set all counts to 1 */
                    for (i = 0; i < variable->num_elements; i++)
                    {
                        filtered_count[i] = 1;
                    }
                }

                /* sum up all values of a bin into the location of the first sample */
                for (i = 0; i < num_time_elements; i++)
                {
                    long target_index = time_index[time_bin_index[i]];

                    if (target_index == i)
                    {
                        if (harp_isnan(variable->data.double_data[target_index]))
                        {
                            filtered_count[target_index] = 0;
                            variable->data.double_data[target_index] = 0;
                            store_count_variable = 1;
                        }
                    }
                    else
                    {
                        if (harp_isnan(variable->data.double_data[target_index]))
                        {
                            filtered_count[target_index] = 0;
                            store_count_variable = 1;
                        }
                        else
                        {
                            variable->data.double_data[target_index] += variable->data.double_data[i];
                        }
                    }
                }
                if (store_count_variable)
                {
                    if (add_count_variable(product, bintype, binning_time_sum, variable->name, variable->num_dimensions,
                                           variable->dimension_type, variable->dimension, filtered_count) != 0)
                    {
                        goto error;
                    }
                }
            }
            if (harp_variable_rearrange_dimension(variable, 0, num_time_bins, time_index) != 0)
            {
                goto error;
            }
        }
        else
        {
            harp_variable *new_variable = NULL;
            int store_count_variable = 0;

            assert(bintype[k] == binning_average || bintype[k] == binning_sum || bintype[k] == binning_angle);

            if (area_binning)
            {
                memset(filtered_weight, 0, filtered_count_size * sizeof(double));
            }
            else
            {
                if (bintype[k] == binning_angle)
                {
                    /* for angle variables we use one filtered_count element per complex pair (and don't pre-multiply) */
                    for (i = 0; i < variable->num_elements / 2; i++)
                    {
                        filtered_count[i] = 1;
                    }
                }
                else if (variable->data_type != harp_type_int32)
                {
                    int result = 0;

                    result = get_count_for_variable(product, variable, bintype, filtered_count);
                    if (result < 0)
                    {
                        goto error;
                    }
                    if (result == 0)
                    {
                        /* if there is no pre-existing count then set all counts to 1 */
                        for (i = 0; i < variable->num_elements; i++)
                        {
                            filtered_count[i] = 1;
                        }
                    }
                }
            }

            /* we need to create a new variable that includes the lat/lon dimensions and uses the binned time dimension */
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
                goto error;
            }

            /* sum up all values per cell */
            cumsum_index = 0;
            for (i = 0; i < num_time_elements; i++)
            {
                long index_offset = time_bin_index[i] * spatial_block_length;

                for (l = 0; l < num_latlon_index[i]; l++)
                {
                    long target_index = index_offset + latlon_cell_index[cumsum_index];

                    if (area_binning)
                    {
                        double weight = latlon_weight[cumsum_index];

                        assert(variable->data_type == harp_type_double);
                        if (bintype[k] == binning_angle)
                        {
                            /* for angle variables we use one filtered_weight element per complex pair */
                            for (j = 0; j < num_sub_elements; j += 2)
                            {
                                if (!harp_isnan(variable->data.double_data[i * num_sub_elements + j]))
                                {
                                    filtered_weight[(target_index * num_sub_elements + j) / 2] += weight;
                                    new_variable->data.double_data[target_index * num_sub_elements + j] +=
                                        weight * variable->data.double_data[i * num_sub_elements + j];
                                    new_variable->data.double_data[target_index * num_sub_elements + j + 1] +=
                                        weight * variable->data.double_data[i * num_sub_elements + j + 1];
                                }
                            }
                        }
                        else
                        {
                            for (j = 0; j < num_sub_elements; j++)
                            {
                                if (!harp_isnan(variable->data.double_data[i * num_sub_elements + j]))
                                {
                                    filtered_weight[target_index * num_sub_elements + j] += weight;
                                    new_variable->data.double_data[target_index * num_sub_elements + j] +=
                                        weight * variable->data.double_data[i * num_sub_elements + j];
                                }
                            }
                        }
                    }
                    else
                    {
                        if (bintype[k] == binning_angle)
                        {
                            /* for angle variables we use one filtered_count element per complex pair */
                            for (j = 0; j < num_sub_elements; j += 2)
                            {
                                if (harp_isnan(variable->data.double_data[i * num_sub_elements + j]))
                                {
                                    filtered_count[(i * num_sub_elements + j) / 2] = 0;
                                    store_count_variable = 1;
                                }
                                else
                                {
                                    new_variable->data.double_data[target_index * num_sub_elements + j] +=
                                        variable->data.double_data[i * num_sub_elements + j];
                                    new_variable->data.double_data[target_index * num_sub_elements + j + 1] +=
                                        variable->data.double_data[i * num_sub_elements + j + 1];
                                }
                            }
                        }
                        else if (variable->data_type == harp_type_int32)
                        {
                            for (j = 0; j < num_sub_elements; j++)
                            {
                                new_variable->data.int32_data[target_index * num_sub_elements + j] +=
                                    variable->data.int32_data[i * num_sub_elements + j];
                            }
                        }
                        else
                        {
                            for (j = 0; j < num_sub_elements; j++)
                            {
                                if (harp_isnan(variable->data.double_data[i * num_sub_elements + j]))
                                {
                                    filtered_count[i * num_sub_elements + j] = 0;
                                    store_count_variable = 1;
                                }
                                else
                                {
                                    new_variable->data.double_data[target_index * num_sub_elements + j] +=
                                        variable->data.double_data[i * num_sub_elements + j];
                                }
                            }
                        }
                    }
                    cumsum_index++;
                }
            }
            if (area_binning)
            {
                double nan_value = harp_nan();

                /* divide variable by the weights and/or set values to NaN if weights==0 */
                if (bintype[k] == binning_angle)
                {
                    for (i = 0; i < new_variable->num_elements; i += 2)
                    {
                        if (filtered_weight[i / 2] == 0)
                        {
                            new_variable->data.double_data[i] = nan_value;
                            new_variable->data.double_data[i + 1] = nan_value;
                        }
                    }
                }
                else
                {
                    for (i = 0; i < new_variable->num_elements; i++)
                    {
                        if (filtered_weight[i] == 0)
                        {
                            new_variable->data.double_data[i] = nan_value;
                        }
                        else if (bintype[k] == binning_average)
                        {
                            /* divide by the sum of the weights */
                            new_variable->data.double_data[i] /= filtered_weight[i];
                        }
                    }
                }
            }
            else
            {
                if (store_count_variable)
                {
                    if (bintype[k] == binning_angle)
                    {
                        /* we store the count for angles (temporarily) to be able to set the count=0 values to NaN */
                        /* don't include the 'complex' dimension for the count variable for angles */
                        if (add_count_variable(product, bintype, binning_sum, variable->name,
                                               variable->num_dimensions - 1, variable->dimension_type,
                                               variable->dimension, filtered_count) != 0)
                        {
                            goto error;
                        }
                    }
                    else
                    {
                        if (add_count_variable(product, bintype, binning_sum, variable->name, variable->num_dimensions,
                                               variable->dimension_type, variable->dimension, filtered_count) != 0)
                        {
                            goto error;
                        }
                    }
                }
            }

            /* replace variable in product with new variable */
            product->variable[k] = new_variable;
            harp_variable_delete(variable);
        }
    }

    product->dimension[harp_dimension_time] = num_time_bins;
    product->dimension[harp_dimension_latitude] = num_latitude_edges - 1;
    product->dimension[harp_dimension_longitude] = num_longitude_edges - 1;

    /* add global count variable if it didn't exist yet */
    if (area_binning)
    {
        /* we only store the total number of samples per temporal bin */
        dimension_type[0] = harp_dimension_time;
        dimension[0] = num_time_bins;
        if (add_count_variable(product, bintype, binning_sum, NULL, 1, dimension_type, dimension, time_count) != 0)
        {
            goto error;
        }
    }
    else
    {
        /* store counts per time x latitude x longitude */
        dimension_type[0] = harp_dimension_time;
        dimension[0] = num_time_bins;
        dimension_type[1] = harp_dimension_latitude;
        dimension[1] = num_latitude_edges - 1;
        dimension_type[2] = harp_dimension_longitude;
        dimension[2] = num_longitude_edges - 1;
        if (add_count_variable(product, bintype, binning_sum, NULL, 3, dimension_type, dimension, count) != 0)
        {
            goto error;
        }
    }

    /* post-process all variables */
    for (k = 0; k < product->num_variables; k++)
    {
        double nan_value = harp_nan();
        harp_variable *variable;

        if (bintype[k] == binning_skip || bintype[k] == binning_remove)
        {
            continue;
        }

        variable = product->variable[k];

        if (bintype[k] == binning_time_min || bintype[k] == binning_time_max)
        {
            /* set all empty bins to NaN */
            for (i = 0; i < variable->num_elements; i++)
            {
                if (time_count[i] == 0)
                {
                    variable->data.double_data[i] = nan_value;
                }
            }
        }
        else if (bintype[k] == binning_time_sum)
        {
            /* set all empty bins to 0 */
            for (i = 0; i < variable->num_elements; i++)
            {
                if (time_count[i] == 0)
                {
                    variable->data.int32_data[i] = 0;
                }
            }
        }
        else if (bintype[k] == binning_time_average)
        {
            harp_variable *count_variable;

            /* divide variable by the sample count and/or set values to NaN if count==0 */
            if (get_count_variable_for_variable(product, variable, bintype, &count_variable) != 0)
            {
                goto error;
            }
            if (count_variable != NULL)
            {
                for (i = 0; i < variable->num_elements; i++)
                {
                    if (count_variable->data.int32_data[i] == 0)
                    {
                        variable->data.double_data[i] = nan_value;
                    }
                    else if (count_variable->data.int32_data[i] > 1)
                    {
                        variable->data.double_data[i] /= count_variable->data.int32_data[i];
                    }
                }
            }
            else
            {
                for (i = 0; i < variable->num_elements; i++)
                {
                    if (time_count[i] == 0)
                    {
                        variable->data.double_data[i] = nan_value;
                    }
                    else if (time_count[i] > 1)
                    {
                        variable->data.double_data[i] /= time_count[i];
                    }
                }
            }
        }
        else
        {
            long num_sub_elements;
            int count_applied = 0;

            num_sub_elements = variable->num_elements / (num_time_bins * spatial_block_length);

            if (bintype[k] == binning_angle)
            {
                /* convert angle variables back from complex values to angles */
                for (i = 0; i < variable->num_elements; i += 2)
                {
                    variable->data.double_data[i] = atan2(variable->data.double_data[i + 1],
                                                          variable->data.double_data[i]);
                }
                if (harp_variable_remove_dimension(variable, variable->num_dimensions - 1, 0) != 0)
                {
                    goto error;
                }
                num_sub_elements /= 2;
                /* convert all angles back to the original unit */
                if (harp_convert_unit("rad", variable->unit, variable->num_elements, variable->data.double_data) != 0)
                {
                    goto error;
                }

                if (!area_binning)
                {
                    harp_variable *count_variable;

                    /* set values to NaN if count==0 */
                    if (get_count_variable_for_variable(product, variable, bintype, &count_variable) != 0)
                    {
                        goto error;
                    }
                    if (count_variable != NULL)
                    {
                        int count_index;

                        for (i = 0; i < variable->num_elements; i++)
                        {
                            if (count_variable->data.int32_data[i] == 0)
                            {
                                variable->data.double_data[i] = nan_value;
                            }
                        }
                        count_applied = 1;

                        /* remove the count variable for angles, since it is meaningless for further propagation of averages */
                        if (harp_product_get_variable_index_by_name(product, count_variable->name, &count_index) != 0)
                        {
                            goto error;
                        }
                        bintype[count_index] = binning_remove;
                    }
                }
            }

            if (!area_binning && !count_applied)
            {
                double nan_value = harp_nan();

                /* divide variables by the sample count and/or set values to NaN if count==0 */
                if (bintype[k] == binning_average)
                {
                    int result;

                    result = get_count_for_variable(product, variable, bintype, filtered_count);
                    if (result < 0)
                    {
                        goto error;
                    }
                    if (result == 1)
                    {
                        /* divide by the count (or set value to NaN if count==0) */
                        for (i = 0; i < variable->num_elements; i++)
                        {
                            if (filtered_count[i] == 0)
                            {
                                variable->data.double_data[i] = nan_value;
                            }
                            else if (filtered_count[i] > 1)
                            {
                                variable->data.double_data[i] /= filtered_count[i];
                            }
                        }
                    }
                    else
                    {
                        for (i = 0; i < num_time_bins * spatial_block_length; i++)
                        {
                            if (count[i] == 0)
                            {
                                for (j = 0; j < num_sub_elements; j++)
                                {
                                    variable->data.double_data[i * num_sub_elements + j] = nan_value;
                                }
                            }
                            else
                            {
                                for (j = 0; j < num_sub_elements; j++)
                                {
                                    variable->data.double_data[i * num_sub_elements + j] /= count[i];
                                }
                            }
                        }
                    }
                }
                else
                {
                    /* set all empty bins to NaN (for double) or 0 (for int32) */
                    for (i = 0; i < num_time_bins * spatial_block_length; i++)
                    {
                        if (count[i] == 0)
                        {
                            if (variable->data_type == harp_type_int32)
                            {
                                for (j = 0; j < num_sub_elements; j++)
                                {
                                    variable->data.int32_data[i * num_sub_elements + j] = 0;
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
    free(filtered_count);
    if (area_binning)
    {
        free(filtered_weight);
    }
    free(time_index);
    free(time_count);
    free(count);
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
    if (time_count != NULL)
    {
        free(time_count);
    }
    if (count != NULL)
    {
        free(count);
    }
    if (filtered_count != NULL)
    {
        free(filtered_count);
    }
    if (filtered_weight != NULL)
    {
        free(filtered_weight);
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

    if (get_binning_type(variable) == binning_remove)
    {
        harp_variable *original_variable = variable;

        /* we always want to keep the variable that we bin on */
        if (harp_variable_copy(original_variable, &variable) != 0)
        {
            free(bin_index);
            free(index);
            return -1;
        }
        if (harp_variable_rearrange_dimension(variable, 0, num_bins, index) != 0)
        {
            harp_variable_delete(variable);
            free(bin_index);
            free(index);
            return -1;
        }
    }
    else
    {
        variable = NULL;
    }

    free(index);

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
