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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "harp.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

int resample_nearest_a(harp_collocation_result *collocation_result, int difference_index);
int resample_nearest_b(harp_collocation_result *collocation_result, int difference_index);

typedef struct collocation_criterium_struct
{
    char *variable_name;
    double value;
    char *unit;
    int use_modulo;
    double modulo_value;
} collocation_criterium;

typedef struct cache_variables_struct
{
    harp_variable *index;       /* reference */
    harp_variable *latitude;    /* copy */
    harp_variable *longitude;   /* copy */
    harp_variable *latitude_bounds;     /* copy */
    harp_variable *longitude_bounds;    /* copy */
    harp_variable **criterium;  /* references */
} cache_variables;

typedef struct collocation_info_struct
{
    /* options */
    int num_criteria;
    collocation_criterium **criterium;
    int datetime_index; /* datetime criterium index can only be -1 or 0 */
    double datetime_conversion_factor;
    int point_distance_index;
    double point_distance_conversion_factor;
    int filter_area_intersects;
    int filter_point_in_area_xy;
    int filter_point_in_area_yx;

    int perform_nearest_neighbour_x_first;
    char *nearest_neighbour_x_variable_name;
    int nearest_neighbour_x_criterium_index;
    char *nearest_neighbour_y_variable_name;
    int nearest_neighbour_y_criterium_index;

    /* result */
    harp_collocation_result *collocation_result;

    /* state */
    long *sorted_index_a;       /* indices of products sorted by datetime_start/datetime_stop */
    long *sorted_index_b;
    harp_product *product_a;    /* we only have one product of dataset A loaded at any moment */
    harp_product **product_b;   /* for dataset B we may have multiple products loaded */
    harp_dataset *dataset_a;
    harp_dataset *dataset_b;

    cache_variables variables_a;
    cache_variables variables_b;

    double *difference;
} collocation_info;

static void collocation_criterium_delete(collocation_criterium *criterium)
{
    if (criterium != NULL)
    {
        if (criterium->variable_name != NULL)
        {
            free(criterium->variable_name);
        }

        if (criterium->unit != NULL)
        {
            free(criterium->unit);
        }

        free(criterium);
    }
}

static int collocation_criterium_new(int variable_name_length, const char *variable_name, double value, int unit_length,
                                     const char *unit, collocation_criterium **new_criterium)
{
    collocation_criterium *criterium = NULL;

    assert(variable_name != NULL);
    if (value < 0)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "collocation criterium value cannot be negative");
        return -1;
    }

    criterium = (collocation_criterium *)malloc(sizeof(collocation_criterium));
    if (criterium == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(collocation_criterium), __FILE__, __LINE__);
        return -1;
    }

    criterium->variable_name = NULL;
    criterium->value = value;
    criterium->unit = NULL;
    criterium->use_modulo = 0;
    criterium->modulo_value = harp_plusinf();

    criterium->variable_name = malloc(variable_name_length + 1);
    if (criterium->variable_name == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        collocation_criterium_delete(criterium);
        return -1;
    }
    memcpy(criterium->variable_name, variable_name, variable_name_length);
    criterium->variable_name[variable_name_length] = '\0';

    if (unit_length == 0)
    {
        if (strcmp(criterium->variable_name, "datetime") == 0)
        {
            /* use the default unit for a datetime _distance_ */
            unit_length = strlen(HARP_UNIT_TIME);
            unit = HARP_UNIT_TIME;
        }
        else if (strcmp(criterium->variable_name, "point_distance") == 0)
        {
            unit_length = strlen(HARP_UNIT_LENGTH);
            unit = HARP_UNIT_LENGTH;
        }
    }

    if (unit_length != 0)
    {
        criterium->unit = malloc(unit_length + 1);
        if (criterium->unit == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                           __LINE__);
            collocation_criterium_delete(criterium);
            return -1;
        }
        memcpy(criterium->unit, unit, unit_length);
        criterium->unit[unit_length] = '\0';
    }

    /* determine whether we should use a module comparison */
    if (strcmp(criterium->variable_name, "longitude") == 0 || strcmp(criterium->variable_name, "wind_direction") == 0 ||
        strstr(criterium->variable_name, "azimuth_angle") != NULL)
    {
        criterium->use_modulo = 1;
        criterium->modulo_value = 360;
        if (criterium->unit != NULL)
        {
            if (harp_convert_unit(HARP_UNIT_ANGLE, criterium->unit, 1, &criterium->modulo_value) != 0)
            {
                return -1;
            }
        }
    }
    *new_criterium = criterium;

    return 0;
}

static int collocation_criterium_set_unit(collocation_criterium *criterium, const char *unit)
{
    criterium->unit = strdup(unit);
    if (criterium->unit == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        return -1;
    }
    if (criterium->use_modulo)
    {
        if (harp_convert_unit(HARP_UNIT_ANGLE, criterium->unit, 1, &criterium->modulo_value) != 0)
        {
            return -1;
        }
    }

    return 0;
}

static void collocation_info_delete(collocation_info *info)
{
    int i;

    if (info != NULL)
    {
        if (info->criterium != NULL)
        {
            for (i = 0; i < info->num_criteria; i++)
            {
                if (info->criterium[i] != NULL)
                {
                    collocation_criterium_delete(info->criterium[i]);
                }
            }
            free(info->criterium);
        }
        if (info->nearest_neighbour_x_variable_name != NULL)
        {
            free(info->nearest_neighbour_x_variable_name);
        }
        if (info->nearest_neighbour_y_variable_name != NULL)
        {
            free(info->nearest_neighbour_y_variable_name);
        }
        if (info->collocation_result != NULL)
        {
            harp_collocation_result_delete(info->collocation_result);
        }
        if (info->sorted_index_a != NULL)
        {
            free(info->sorted_index_a);
        }
        if (info->sorted_index_b != NULL)
        {
            free(info->sorted_index_b);
        }
        if (info->product_a != NULL)
        {
            harp_product_delete(info->product_a);
        }
        if (info->product_b != NULL)
        {
            assert(info->dataset_b != NULL);
            for (i = 0; i < info->dataset_b->num_products; i++)
            {
                if (info->product_b[i] != NULL)
                {
                    harp_product_delete(info->product_b[i]);
                }
            }
            free(info->product_b);
        }
        if (info->dataset_a != NULL)
        {
            harp_dataset_delete(info->dataset_a);
        }
        if (info->dataset_b != NULL)
        {
            harp_dataset_delete(info->dataset_b);
        }
        if (info->variables_a.latitude != NULL)
        {
            harp_variable_delete(info->variables_a.latitude);
        }
        if (info->variables_a.longitude != NULL)
        {
            harp_variable_delete(info->variables_a.longitude);
        }
        if (info->variables_a.latitude_bounds != NULL)
        {
            harp_variable_delete(info->variables_a.latitude_bounds);
        }
        if (info->variables_a.longitude_bounds != NULL)
        {
            harp_variable_delete(info->variables_a.longitude_bounds);
        }
        if (info->variables_a.criterium != NULL)
        {
            free(info->variables_a.criterium);
        }
        if (info->variables_b.latitude != NULL)
        {
            harp_variable_delete(info->variables_b.latitude);
        }
        if (info->variables_b.longitude != NULL)
        {
            harp_variable_delete(info->variables_b.longitude);
        }
        if (info->variables_b.latitude_bounds != NULL)
        {
            harp_variable_delete(info->variables_b.latitude_bounds);
        }
        if (info->variables_b.longitude_bounds != NULL)
        {
            harp_variable_delete(info->variables_b.longitude_bounds);
        }
        if (info->variables_b.criterium != NULL)
        {
            free(info->variables_b.criterium);
        }
        if (info->difference != NULL)
        {
            free(info->difference);
        }
        free(info);
    }
}

static int collocation_info_new(collocation_info **new_info)
{
    collocation_info *info = NULL;

    info = (collocation_info *)malloc(sizeof(collocation_info));
    if (info == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(collocation_info), __FILE__, __LINE__);
        return -1;
    }

    info->num_criteria = 0;
    info->criterium = NULL;
    info->datetime_index = -1;
    info->datetime_conversion_factor = 1;
    info->point_distance_index = -1;
    info->point_distance_conversion_factor = 1;
    info->filter_area_intersects = 0;
    info->filter_point_in_area_xy = 0;
    info->filter_point_in_area_yx = 0;
    info->perform_nearest_neighbour_x_first = 0;
    info->nearest_neighbour_x_variable_name = NULL;
    info->nearest_neighbour_x_criterium_index = -1;
    info->nearest_neighbour_y_variable_name = NULL;
    info->nearest_neighbour_y_criterium_index = -1;
    info->collocation_result = NULL;
    info->sorted_index_a = NULL;
    info->sorted_index_b = NULL;
    info->product_a = NULL;
    info->product_b = NULL;
    info->dataset_a = NULL;
    info->dataset_b = NULL;
    info->variables_a.index = NULL;
    info->variables_a.latitude = NULL;
    info->variables_a.longitude = NULL;
    info->variables_a.latitude_bounds = NULL;
    info->variables_a.longitude_bounds = NULL;
    info->variables_a.criterium = NULL;
    info->variables_b.index = NULL;
    info->variables_b.latitude = NULL;
    info->variables_b.longitude = NULL;
    info->variables_b.latitude_bounds = NULL;
    info->variables_b.longitude_bounds = NULL;
    info->variables_b.criterium = NULL;
    info->difference = NULL;

    if (harp_dataset_new(&info->dataset_a) != 0)
    {
        collocation_info_delete(info);
        return -1;
    }
    if (harp_dataset_new(&info->dataset_b) != 0)
    {
        collocation_info_delete(info);
        return -1;
    }
    if (harp_collocation_result_new(&info->collocation_result, 0, NULL, NULL) != 0)
    {
        collocation_info_delete(info);
        return -1;
    }

    *new_info = info;

    return 0;
}

static int collocation_info_add_criterium(collocation_info *info, int variable_name_length, const char *variable_name,
                                          double value, int unit_length, const char *unit)
{
    collocation_criterium **new_criterium;
    collocation_criterium *criterium;
    int i;

    new_criterium = realloc(info->criterium, (info->num_criteria + 1) * sizeof(collocation_criterium *));
    if (new_criterium == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       (info->num_criteria + 1) * sizeof(collocation_criterium *), __FILE__, __LINE__);
        return -1;
    }
    info->criterium = new_criterium;

    if (collocation_criterium_new(variable_name_length, variable_name, value, unit_length, unit, &criterium) != 0)
    {
        return -1;
    }

    for (i = 0; i < info->num_criteria; i++)
    {
        if (strcmp(info->criterium[i]->variable_name, criterium->variable_name) == 0)
        {
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "cannot provide more than one criterium for variable '%s'",
                           criterium->variable_name);
            collocation_criterium_delete(criterium);
            return -1;
        }
    }
    info->criterium[info->num_criteria] = criterium;
    info->num_criteria++;

    return 0;
}

static int collocation_info_add_criterium_from_string(collocation_info *info, char *argument)
{
    char *cursor;
    char *variable_name;
    char *value_str;
    char *unit = NULL;
    double value;
    int variable_name_length;
    int unit_length = 0;

    cursor = argument;
    while (*cursor == ' ')
    {
        cursor++;
    }
    variable_name = cursor;
    while (*cursor != ' ' && *cursor != '\0')
    {
        cursor++;
    }
    variable_name_length = cursor - variable_name;
    if (variable_name_length == 0)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "invalid criterium '%s'", argument);
        return -1;
    }

    while (*cursor == ' ')
    {
        cursor++;
    }
    value_str = cursor;
    value = strtod(value_str, &cursor);
    if (cursor == value_str)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "missing value in criterium '%s'", argument);
        return -1;
    }

    while (*cursor == ' ')
    {
        cursor++;
    }
    if (*cursor != '\0')
    {
        if (*cursor != '[')
        {
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "invalid unit in criterium '%s' (expected '[')", argument);
            return -1;
        }
        cursor++;
        unit = cursor;
        while (*cursor != ']' && *cursor != '\0')
        {
            cursor++;
        }
        unit_length = cursor - unit;
        if (unit_length == 0)
        {
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "invalid unit in criterium '%s'", argument);
            return -1;
        }
        if (*cursor != ']')
        {
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "invalid unit in criterium '%s' (expected ']')", argument);
            return -1;
        }
        cursor++;
        while (*cursor == ' ')
        {
            cursor++;
        }
        if (*cursor != '\0')
        {
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "invalid criterium '%s'", argument);
            return -1;
        }
    }

    return collocation_info_add_criterium(info, variable_name_length, variable_name, value, unit_length, unit);
}

static int collocation_info_update(collocation_info *info)
{
    int i, j;

    /* add criteria for the nearest neighbour filters (if they were not there yet) */
    if (info->nearest_neighbour_x_variable_name != NULL)
    {
        for (i = 0; i < info->num_criteria; i++)
        {
            if (strcmp(info->criterium[i]->variable_name, info->nearest_neighbour_x_variable_name) == 0)
            {
                info->nearest_neighbour_x_criterium_index = i;
                break;
            }
        }
        if (i == info->num_criteria)
        {
            if (collocation_info_add_criterium(info, strlen(info->nearest_neighbour_x_variable_name),
                                               info->nearest_neighbour_x_variable_name, harp_plusinf(), 0, NULL) != 0)
            {
                return -1;
            }
            info->nearest_neighbour_x_criterium_index = info->num_criteria - 1;
        }
    }

    if (info->nearest_neighbour_y_variable_name != NULL)
    {
        for (i = 0; i < info->num_criteria; i++)
        {
            if (strcmp(info->criterium[i]->variable_name, info->nearest_neighbour_y_variable_name) == 0)
            {
                info->nearest_neighbour_y_criterium_index = i;
                break;
            }
        }
        if (i == info->num_criteria)
        {
            if (collocation_info_add_criterium(info, strlen(info->nearest_neighbour_y_variable_name),
                                               info->nearest_neighbour_y_variable_name, harp_plusinf(), 0, NULL) != 0)
            {
                return -1;
            }
            info->nearest_neighbour_y_criterium_index = info->num_criteria - 1;
        }
    }

    /* if no criteria are set then all data is kept and no need to collocate */
    if (info->num_criteria == 0 && !info->filter_area_intersects && !info->filter_point_in_area_xy &&
        !info->filter_point_in_area_yx)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "no collocation criteria are set");
        return -1;
    }

    /* properly order criteria and identify special treatment */
    for (i = 0; i < info->num_criteria; i++)
    {
        if (strcmp(info->criterium[i]->variable_name, "datetime") == 0)
        {
            /* make sure that datetime criterium is the first */
            if (i != 0)
            {
                collocation_criterium *criterium;
                int j;

                /* put datetime criterium in the first position */
                criterium = info->criterium[i];
                for (j = i; j > 0; j--)
                {
                    info->criterium[j] = info->criterium[j - 1];
                }
                info->criterium[0] = criterium;
            }
            info->datetime_index = 0;
            /* determine conversion factor between threshold unit and internal HARP unit */
            if (harp_convert_unit(HARP_UNIT_TIME, info->criterium[info->datetime_index]->unit, 1,
                                  &info->datetime_conversion_factor) != 0)
            {
                return -1;
            }
            break;
        }
    }
    for (i = 0; i < info->num_criteria; i++)
    {
        if (strcmp(info->criterium[i]->variable_name, "point_distance") == 0)
        {
            info->point_distance_index = i;
            /* determine conversion factor between threshold unit and internal HARP unit */
            if (harp_convert_unit(HARP_UNIT_LENGTH, info->criterium[info->point_distance_index]->unit, 1,
                                  &info->point_distance_conversion_factor) != 0)
            {
                return -1;
            }
            break;
        }
    }

    /* initialize sorted indices */
    info->sorted_index_a = malloc(info->dataset_a->num_products * sizeof(long));
    if (info->sorted_index_a == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       info->dataset_a->num_products * sizeof(long), __FILE__, __LINE__);
        return -1;
    }
    for (i = 0; i < info->dataset_a->num_products; i++)
    {
        /* start with source_product ordering */
        info->sorted_index_a[i] = info->dataset_a->sorted_index[i];

        /* bubble down to make it sorted on datetime start/stop */
        for (j = i; j > 0; j--)
        {
            harp_product_metadata *metadata = info->dataset_a->metadata[info->sorted_index_a[j]];
            harp_product_metadata *metadata_prev = info->dataset_a->metadata[info->sorted_index_a[j - 1]];

            if (metadata->datetime_start < metadata_prev->datetime_start ||
                (metadata->datetime_start == metadata_prev->datetime_start &&
                 metadata->datetime_stop < metadata_prev->datetime_stop))
            {
                long index = info->sorted_index_a[j - 1];

                info->sorted_index_a[j - 1] = info->sorted_index_a[j];
                info->sorted_index_a[j] = index;
            }
        }
    }

    info->sorted_index_b = malloc(info->dataset_b->num_products * sizeof(long));
    if (info->sorted_index_b == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       info->dataset_b->num_products * sizeof(long), __FILE__, __LINE__);
        return -1;
    }
    for (i = 0; i < info->dataset_b->num_products; i++)
    {
        info->sorted_index_b[i] = info->dataset_b->sorted_index[i];

        for (j = i; j > 0; j--)
        {
            harp_product_metadata *metadata = info->dataset_b->metadata[info->sorted_index_b[j]];
            harp_product_metadata *metadata_prev = info->dataset_b->metadata[info->sorted_index_b[j - 1]];

            if (metadata->datetime_start < metadata_prev->datetime_start ||
                (metadata->datetime_start == metadata_prev->datetime_start &&
                 metadata->datetime_stop < metadata_prev->datetime_stop))
            {
                long index = info->sorted_index_b[j - 1];

                info->sorted_index_b[j - 1] = info->sorted_index_b[j];
                info->sorted_index_b[j] = index;
            }
        }
    }

    /* initialized product_b array */
    info->product_b = malloc(info->dataset_b->num_products * sizeof(harp_product *));
    if (info->product_b == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       info->dataset_b->num_products * sizeof(harp_product *), __FILE__, __LINE__);
        return -1;
    }
    for (i = 0; i < info->dataset_b->num_products; i++)
    {
        info->product_b[i] = NULL;
    }

    /* set the differences for the collocation result */
    info->collocation_result->num_differences = info->num_criteria;
    info->collocation_result->difference_variable_name = malloc((info->num_criteria + 1) * sizeof(char *));
    if (info->collocation_result->difference_variable_name == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       (info->num_criteria + 1) * sizeof(char *), __FILE__, __LINE__);
        return -1;
    }
    info->collocation_result->difference_unit = malloc((info->num_criteria + 1) * sizeof(char *));
    if (info->collocation_result->difference_unit == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       (info->num_criteria + 1) * sizeof(char *), __FILE__, __LINE__);
        return -1;
    }
    for (i = 0; i < info->num_criteria; i++)
    {
        info->collocation_result->difference_variable_name[i] = NULL;
        info->collocation_result->difference_unit[i] = NULL;
    }
    for (i = 0; i < info->num_criteria; i++)
    {
        if (i == info->point_distance_index)
        {
            info->collocation_result->difference_variable_name[i] = strdup(info->criterium[i]->variable_name);
            if (info->collocation_result->difference_variable_name[i] == NULL)
            {
                harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                               __LINE__);
                return -1;
            }
        }
        else
        {
            info->collocation_result->difference_variable_name[i] =
                malloc(strlen(info->criterium[i]->variable_name) + 9);
            if (info->collocation_result->difference_variable_name == NULL)
            {
                harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                               (long)(strlen(info->criterium[i]->variable_name) + 9), __FILE__, __LINE__);
                return -1;
            }
            strcpy(info->collocation_result->difference_variable_name[i], info->criterium[i]->variable_name);
            strcat(info->collocation_result->difference_variable_name[i], "_absdiff");
        }
        /* we only populate the unit if we already have it, we will update this information during matchup */
        if (info->criterium[i]->unit != NULL)
        {
            info->collocation_result->difference_unit[i] = strdup(info->criterium[i]->unit);
            if (info->collocation_result->difference_unit[i] == NULL)
            {
                harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                               __LINE__);
                return -1;
            }
        }
    }

    /* initialize the arrays to hold the references to the variables for evaluating the criteria */
    info->variables_a.criterium = malloc(info->num_criteria * sizeof(harp_variable *));
    if (info->variables_a.criterium == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       (info->num_criteria) * sizeof(harp_variable *), __FILE__, __LINE__);
        return -1;
    }
    info->variables_b.criterium = malloc(info->num_criteria * sizeof(harp_variable *));
    if (info->variables_b.criterium == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       (info->num_criteria) * sizeof(harp_variable *), __FILE__, __LINE__);
        return -1;
    }
    for (i = 0; i < info->num_criteria; i++)
    {
        info->variables_a.criterium[i] = NULL;
        info->variables_b.criterium[i] = NULL;
    }

    /* initialize array in which the differences are stored */
    info->difference = malloc(info->num_criteria * sizeof(double));
    if (info->difference == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       (info->num_criteria) * sizeof(double), __FILE__, __LINE__);
        return -1;
    }

    return 0;
}

static int perform_matchup_on_measurements(collocation_info *info, long index_a, long product_b_index, long index_b)
{
    double *longitude_bounds_a;
    double *latitude_bounds_a;
    double *longitude_bounds_b;
    double *latitude_bounds_b;
    double latitude_a;
    double longitude_a;
    double latitude_b;
    double longitude_b;
    long collocation_index;
    int num_vertices_a;
    int num_vertices_b;
    int i;

    for (i = 0; i < info->num_criteria; i++)
    {
        if (i == info->point_distance_index)
        {
            latitude_a = info->variables_a.latitude->data.double_data[index_a];
            longitude_a = info->variables_a.longitude->data.double_data[index_a];
            latitude_b = info->variables_b.latitude->data.double_data[index_b];
            longitude_b = info->variables_b.longitude->data.double_data[index_b];

            if (harp_geometry_get_point_distance(latitude_a, longitude_a, latitude_b, longitude_b, &info->difference[i])
                != 0)
            {
                return -1;
            }
            info->difference[i] *= info->point_distance_conversion_factor;
        }
        else
        {
            info->difference[i] = fabs(info->variables_a.criterium[i]->data.double_data[index_a] -
                                       info->variables_b.criterium[i]->data.double_data[index_b]);
            if (i == info->datetime_index)
            {
                info->difference[i] *= info->datetime_conversion_factor;
            }
        }
        if (info->criterium[i]->use_modulo)
        {
            while (info->difference[i] > info->criterium[i]->modulo_value)
            {
                info->difference[i] -= info->criterium[i]->modulo_value;
            }
            if (info->difference[i] > info->criterium[i]->modulo_value / 2)
            {
                info->difference[i] = info->criterium[i]->modulo_value - info->difference[i];
            }
        }
        /* we use !(x<=y) instead of x>y so a NaN value for the difference will also result in a mismatch */
        if (!(info->difference[i] <= info->criterium[i]->value))
        {
            return 0;
        }
    }

    if (info->filter_point_in_area_xy)
    {
        int in_area;

        latitude_a = info->variables_a.latitude->data.double_data[index_a];
        longitude_a = info->variables_a.longitude->data.double_data[index_a];
        num_vertices_b = info->variables_b.latitude_bounds->dimension[1];
        latitude_bounds_b = &info->variables_b.latitude_bounds->data.double_data[index_b * num_vertices_b];
        longitude_bounds_b = &info->variables_b.longitude_bounds->data.double_data[index_b * num_vertices_b];
        if (harp_geometry_has_point_in_area(latitude_a, longitude_a, num_vertices_b, latitude_bounds_b,
                                            longitude_bounds_b, &in_area) != 0)
        {
            return -1;
        }
        if (!in_area)
        {
            return 0;
        }
    }
    if (info->filter_point_in_area_yx)
    {
        int in_area;

        latitude_b = info->variables_b.latitude->data.double_data[index_b];
        longitude_b = info->variables_b.longitude->data.double_data[index_b];
        num_vertices_a = info->variables_a.latitude_bounds->dimension[1];
        latitude_bounds_a = &info->variables_a.latitude_bounds->data.double_data[index_a * num_vertices_a];
        longitude_bounds_a = &info->variables_a.longitude_bounds->data.double_data[index_a * num_vertices_a];
        if (harp_geometry_has_point_in_area(latitude_b, longitude_b, num_vertices_a, latitude_bounds_a,
                                            longitude_bounds_a, &in_area) != 0)
        {
            return -1;
        }
        if (!in_area)
        {
            return 0;
        }
    }
    if (info->filter_area_intersects)
    {
        int has_overlap;

        num_vertices_a = info->variables_a.latitude_bounds->dimension[1];
        latitude_bounds_a = &info->variables_a.latitude_bounds->data.double_data[index_a * num_vertices_a];
        longitude_bounds_a = &info->variables_a.longitude_bounds->data.double_data[index_a * num_vertices_a];
        num_vertices_b = info->variables_b.latitude_bounds->dimension[1];
        latitude_bounds_b = &info->variables_b.latitude_bounds->data.double_data[index_b * num_vertices_b];
        longitude_bounds_b = &info->variables_b.longitude_bounds->data.double_data[index_b * num_vertices_b];

        if (harp_geometry_has_area_overlap(num_vertices_a, latitude_bounds_a, longitude_bounds_a, num_vertices_b,
                                           latitude_bounds_b, longitude_bounds_b, &has_overlap, NULL) != 0)
        {
            return -1;
        }
        if (!has_overlap)
        {
            return 0;
        }
    }

    if (info->nearest_neighbour_x_criterium_index >= 0 || info->nearest_neighbour_y_criterium_index >= 0)
    {
        long product_index;
        long sample_index;

        /* replace any pair that is not closer for the first nearest neighbour criterium */
        /* since we apply a nearest filter there can only be at most one pair in the collocation result matching */
        /* the index we are looking for */
        if (info->perform_nearest_neighbour_x_first)
        {
            /* select nearest x */
            assert(info->nearest_neighbour_x_criterium_index >= 0);

            if (harp_dataset_has_product(info->collocation_result->dataset_a, info->product_a->source_product))
            {
                if (harp_dataset_get_index_from_source_product(info->collocation_result->dataset_a,
                                                               info->product_a->source_product, &product_index) != 0)
                {
                    return -1;
                }
                sample_index = info->variables_a.index->data.int32_data[index_a];

                for (i = 0; i < info->collocation_result->num_pairs; i++)
                {
                    harp_collocation_pair *pair = info->collocation_result->pair[i];

                    if (pair->product_index_a == product_index && pair->sample_index_a == sample_index)
                    {
                        if (pair->difference[info->nearest_neighbour_x_criterium_index] <=
                            info->difference[info->nearest_neighbour_x_criterium_index])
                        {
                            /* existing pair is closer -> ignore the new pair */
                            return 0;
                        }
                        /* new pair is closer, remove existing one */
                        if (harp_collocation_result_remove_pair_at_index(info->collocation_result, i) != 0)
                        {
                            return -1;
                        }
                        /* stop the search and immediately continue with adding the new pair */
                        break;
                    }
                }
            }
        }
        else
        {
            /* select nearest y */
            assert(info->nearest_neighbour_y_criterium_index >= 0);

            if (harp_dataset_has_product(info->collocation_result->dataset_b,
                                         info->product_b[product_b_index]->source_product))
            {
                if (harp_dataset_get_index_from_source_product(info->collocation_result->dataset_b,
                                                               info->product_b[product_b_index]->source_product,
                                                               &product_index) != 0)
                {
                    return -1;
                }
                sample_index = info->variables_b.index->data.int32_data[index_b];

                for (i = 0; i < info->collocation_result->num_pairs; i++)
                {
                    harp_collocation_pair *pair = info->collocation_result->pair[i];

                    if (pair->product_index_b == product_index && pair->sample_index_b == sample_index)
                    {
                        if (pair->difference[info->nearest_neighbour_y_criterium_index] <=
                            info->difference[info->nearest_neighbour_y_criterium_index])
                        {
                            /* existing pair is closer -> ignore the new pair */
                            return 0;
                        }
                        /* new pair is closer, remove existing one */
                        if (harp_collocation_result_remove_pair_at_index(info->collocation_result, i) != 0)
                        {
                            return -1;
                        }
                        /* stop the search and immediately continue with adding the new pair */
                        break;
                    }
                }
            }
        }
        /* the second nearest neighbour criterium, if it exists, can only be avaluated at the end of the collocation */
    }

    /* add new pair to result */
    if (info->collocation_result->num_pairs == 0)
    {
        collocation_index = 0;
    }
    else
    {
        collocation_index =
            info->collocation_result->pair[info->collocation_result->num_pairs - 1]->collocation_index + 1;
    }
    if (harp_collocation_result_add_pair(info->collocation_result, collocation_index, info->product_a->source_product,
                                         info->variables_a.index->data.int32_data[index_a],
                                         info->product_b[product_b_index]->source_product,
                                         info->variables_b.index->data.int32_data[index_b], info->num_criteria,
                                         info->difference) != 0)
    {
        return -1;
    }

    return 0;
}

static int perform_matchup_on_products(collocation_info *info, long product_b_index)
{
    long i, j;

    for (i = 0; i < info->product_a->dimension[harp_dimension_time]; i++)
    {
        for (j = 0; j < info->product_b[product_b_index]->dimension[harp_dimension_time]; j++)
        {
            if (perform_matchup_on_measurements(info, i, product_b_index, j) != 0)
            {
                return -1;
            }
        }
    }

    return 0;
}

static int remove_unused_variables(collocation_info *info, harp_product *product, int include_latlon,
                                   int include_latlon_bounds)
{
    uint8_t *included;
    int index;
    int i;

    included = (uint8_t *)malloc(product->num_variables);
    if (included == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       product->num_variables * sizeof(uint8_t), __FILE__, __LINE__);
        return -1;
    }

    /* assume all variables are excluded */
    for (i = 0; i < product->num_variables; i++)
    {
        included[i] = 0;
    }

    /* always keep 'index' variable */
    if (harp_product_get_variable_index_by_name(product, "index", &index) != 0)
    {
        free(included);
        return -1;
    }
    included[index] = 1;

    /* set the 'keep' flags in the mask */
    for (i = 0; i < info->num_criteria; i++)
    {
        if (i == info->point_distance_index)
        {
            continue;
        }
        if (harp_product_get_variable_index_by_name(product, info->criterium[i]->variable_name, &index) != 0)
        {
            free(included);
            return -1;
        }
        included[index] = 1;
    }

    if (include_latlon)
    {
        if (harp_product_get_variable_index_by_name(product, "latitude", &index) != 0)
        {
            free(included);
            return -1;
        }
        included[index] = 1;
        if (harp_product_get_variable_index_by_name(product, "longitude", &index) != 0)
        {
            free(included);
            return -1;
        }
        included[index] = 1;
    }
    if (include_latlon_bounds)
    {
        if (harp_product_get_variable_index_by_name(product, "latitude_bounds", &index) != 0)
        {
            free(included);
            return -1;
        }
        included[index] = 1;
        if (harp_product_get_variable_index_by_name(product, "longitude_bounds", &index) != 0)
        {
            free(included);
            return -1;
        }
        included[index] = 1;
    }

    /* filter the variables using the mask */
    for (i = product->num_variables - 1; i >= 0; i--)
    {
        if (!included[i])
        {
            if (harp_product_remove_variable(product, product->variable[i]) != 0)
            {
                free(included);
                return -1;
            }
        }
    }

    free(included);

    return 0;
}

static int filter_product(collocation_info *info, harp_product *product, int is_dataset_a)
{
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_independent };
    int include_latlon_bounds;
    int include_latlon;
    long i;

    if (is_dataset_a)
    {
        include_latlon = info->point_distance_index >= 0 || info->filter_point_in_area_xy;
        include_latlon_bounds = info->filter_area_intersects || info->filter_point_in_area_yx;
    }
    else
    {
        include_latlon = info->point_distance_index >= 0 || info->filter_point_in_area_yx;
        include_latlon_bounds = info->filter_area_intersects || info->filter_point_in_area_xy;
    }

    /* make sure that we have an 'index' variable */
    if (harp_product_add_derived_variable(product, "index", NULL, NULL, 1, dimension_type) != 0)
    {
        return -1;
    }

    for (i = 0; i < info->num_criteria; i++)
    {
        const char *unit = NULL;

        if (i == info->point_distance_index)
        {
            continue;
        }

        if (info->criterium[i]->unit == NULL)
        {
            harp_variable *variable;

            /* determine the unit automatically from the variable in the product and assign it to the criterium */
            /* also update the difference information in the collocation result */
            if (!harp_product_has_variable(product, info->criterium[i]->variable_name))
            {
                harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "could not determine unit for '%s' collocation criterium "
                               "(no such variable in product)", info->criterium[i]->variable_name);
                return -1;
            }
            if (harp_product_get_variable_by_name(product, info->criterium[i]->variable_name, &variable) != 0)
            {
                return -1;
            }
            if (variable->unit == NULL)
            {
                harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "could not determine unit for '%s' collocation criterium "
                               "(variable has no unit)", info->criterium[i]->variable_name);
                return -1;
            }
            if (collocation_criterium_set_unit(info->criterium[i], variable->unit) != 0)
            {
                return -1;
            }
            assert(info->collocation_result->difference_unit[i] == NULL);
            info->collocation_result->difference_unit[i] = strdup(variable->unit);
            if (info->collocation_result->difference_unit[i] == NULL)
            {
                harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                               __LINE__);
                return -1;
            }
        }
        if (i == info->datetime_index)
        {
            unit = HARP_UNIT_DATETIME;
        }
        else
        {
            unit = info->criterium[i]->unit;
        }

        if (harp_product_add_derived_variable(product, info->criterium[i]->variable_name, NULL, unit, 1, dimension_type)
            != 0)
        {
            return -1;
        }
    }

    /* just include lat/lon with default unit (since they may also be used as collocation criteria with a user specified
     * unit). We change the unit during the comparison if needed.
     */
    if (include_latlon)
    {
        if (harp_product_add_derived_variable(product, "latitude", NULL, NULL, 1, dimension_type) != 0)
        {
            return -1;
        }
        if (harp_product_add_derived_variable(product, "longitude", NULL, NULL, 1, dimension_type) != 0)
        {
            return -1;
        }
    }
    if (include_latlon_bounds)
    {
        if (harp_product_add_derived_variable(product, "latitude_bounds", NULL, NULL, 2, dimension_type) != 0)
        {
            return -1;
        }
        if (harp_product_add_derived_variable(product, "longitude_bounds", NULL, NULL, 2, dimension_type) != 0)
        {
            return -1;
        }
    }

    /* remove all variables that are not needed */
    if (remove_unused_variables(info, product, include_latlon, include_latlon_bounds) != 0)
    {
        return -1;
    }

    return 0;
}

static int assign_variables(collocation_info *info, cache_variables *cache, harp_product *product)
{
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_independent };
    long i;

    if (harp_product_get_variable_by_name(product, "index", &cache->index) != 0)
    {
        return -1;
    }

    if (harp_product_has_variable(product, "latitude"))
    {
        if (cache->latitude != NULL)
        {
            harp_variable_delete(cache->latitude);
        }
        if (cache->longitude != NULL)
        {
            harp_variable_delete(cache->longitude);
        }
        if (harp_product_get_derived_variable(product, "latitude", NULL, HARP_UNIT_LATITUDE, 1, dimension_type,
                                              &cache->latitude) != 0)
        {
            return -1;
        }
        if (harp_product_get_derived_variable(product, "longitude", NULL, HARP_UNIT_LONGITUDE, 1, dimension_type,
                                              &cache->longitude) != 0)
        {
            return -1;
        }
    }
    if (harp_product_has_variable(product, "latitude_bounds"))
    {
        if (cache->latitude_bounds != NULL)
        {
            harp_variable_delete(cache->latitude_bounds);
        }
        if (cache->longitude_bounds != NULL)
        {
            harp_variable_delete(cache->longitude_bounds);
        }
        if (harp_product_get_derived_variable(product, "latitude_bounds", NULL, HARP_UNIT_LATITUDE, 2, dimension_type,
                                              &cache->latitude_bounds) != 0)
        {
            return -1;
        }
        if (harp_product_get_derived_variable(product, "longitude_bounds", NULL, HARP_UNIT_LONGITUDE, 2, dimension_type,
                                              &cache->longitude_bounds) != 0)
        {
            return -1;
        }
    }

    for (i = 0; i < info->num_criteria; i++)
    {
        if (i == info->point_distance_index)
        {
            continue;
        }
        if (harp_product_get_variable_by_name(product, info->criterium[i]->variable_name, &cache->criterium[i]) != 0)
        {
            return -1;
        }
    }

    return 0;
}

/* Collocate two datasets */
static int perform_matchup(collocation_info *info)
{
    long i, j;
    double delta_time;  /* time criterium to efficiently filter for products that could have matching pairs */

    if (info->datetime_index >= 0)
    {
        delta_time = info->criterium[info->datetime_index]->value;

        /* the datetime start/stop in the metadata is provided in days since 2000-01-01 */
        if (harp_convert_unit(info->criterium[info->datetime_index]->unit, "days", 1, &delta_time) != 0)
        {
            return -1;
        }
    }
    else
    {
        /* set delta_time to infinite, so we match everything */
        delta_time = harp_plusinf();
    }

    /* loop over products in dataset A */
    for (i = 0; i < info->dataset_a->num_products; i++)
    {
        long index_a = info->sorted_index_a[i];
        double datetime_start_a = info->dataset_a->metadata[index_a]->datetime_start;
        double datetime_stop_a = info->dataset_a->metadata[index_a]->datetime_stop;

        /* import product of dataset A */
        if (harp_import(info->dataset_a->metadata[index_a]->filename, NULL, NULL, &info->product_a) != 0)
        {
            return -1;
        }
        if (harp_product_is_empty(info->product_a))
        {
            harp_product_delete(info->product_a);
            info->product_a = NULL;
            continue;
        }
        if (filter_product(info, info->product_a, 1) != 0)
        {
            return -1;
        }
        if (assign_variables(info, &info->variables_a, info->product_a) != 0)
        {
            return -1;
        }

        for (j = 0; j < info->dataset_b->num_products; j++)
        {
            long index_b = info->sorted_index_b[j];
            double datetime_start_b = info->dataset_b->metadata[index_b]->datetime_start;
            double datetime_stop_b = info->dataset_b->metadata[index_b]->datetime_stop;

            if (datetime_start_a <= datetime_stop_b + delta_time && datetime_start_b - delta_time <= datetime_stop_a)
            {
                /* overlap */
                if (info->product_b[index_b] == NULL)
                {
                    if (harp_import(info->dataset_b->metadata[index_b]->filename, NULL, NULL, &info->product_b[index_b])
                        != 0)
                    {
                        return -1;
                    }
                    if (harp_product_is_empty(info->product_b[index_b]))
                    {
                        continue;
                    }
                    if (filter_product(info, info->product_b[index_b], 0) != 0)
                    {
                        return -1;
                    }
                }
                else if (harp_product_is_empty(info->product_b[index_b]))
                {
                    continue;
                }

                if (assign_variables(info, &info->variables_b, info->product_b[index_b]) != 0)
                {
                    return -1;
                }

                if (perform_matchup_on_products(info, index_b) != 0)
                {
                    return -1;
                }
            }
            else if (info->product_b[index_b] != NULL)
            {
                harp_product_delete(info->product_b[index_b]);
                info->product_b[index_b] = NULL;
            }
        }
        harp_product_delete(info->product_a);
        info->product_a = NULL;
    }

    return 0;
}

int matchup(int argc, char *argv[])
{
    collocation_info *info = NULL;
    int i;

    if (collocation_info_new(&info) != 0)
    {
        return -1;
    }
    for (i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-d") == 0 && i + 1 < argc && argv[i + 1][0] != '-')
        {
            if (collocation_info_add_criterium_from_string(info, argv[i + 1]) != 0)
            {
                collocation_info_delete(info);

                return -1;
            }
            i++;
        }
        else if (strcmp(argv[i], "--area-intersects") == 0)
        {
            info->filter_area_intersects = 1;
        }
        else if (strcmp(argv[i], "--point-in-area-xy") == 0)
        {
            info->filter_point_in_area_xy = 1;
        }
        else if (strcmp(argv[i], "--point-in-area-yx") == 0)
        {
            info->filter_point_in_area_yx = 1;
        }
        else if (strcmp(argv[i], "-nx") == 0 && i + 1 < argc && argv[i + 1][0] != '-')
        {
            if (info->nearest_neighbour_x_variable_name != NULL)
            {
                collocation_info_delete(info);
                return 1;
            }
            info->nearest_neighbour_x_variable_name = strdup(argv[i + 1]);
            if (info->nearest_neighbour_x_variable_name == NULL)
            {
                harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                               __LINE__);
                collocation_info_delete(info);
                return -1;
            }
            if (info->nearest_neighbour_y_variable_name == NULL)
            {
                info->perform_nearest_neighbour_x_first = 1;
            }
            i++;
        }
        else if (strcmp(argv[i], "-ny") == 0 && i + 1 < argc && argv[i + 1][0] != '-')
        {
            if (info->nearest_neighbour_y_variable_name != NULL)
            {
                collocation_info_delete(info);
                return 1;
            }
            info->nearest_neighbour_y_variable_name = strdup(argv[i + 1]);
            if (info->nearest_neighbour_y_variable_name == NULL)
            {
                harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                               __LINE__);
                collocation_info_delete(info);
                return -1;
            }
            i++;
        }
        else
        {
            if (argv[i][0] == '-' || i != argc - 3)
            {
                collocation_info_delete(info);
                return 1;
            }
            break;
        }
    }

    if (harp_dataset_import(info->dataset_a, argv[argc - 3], NULL) != 0)
    {
        collocation_info_delete(info);
        return -1;
    }
    if (harp_dataset_import(info->dataset_b, argv[argc - 2], NULL) != 0)
    {
        collocation_info_delete(info);
        return -1;
    }

    if (info->dataset_a->num_products > 0 && info->dataset_b->num_products > 0)
    {
        if (collocation_info_update(info) != 0)
        {
            collocation_info_delete(info);
            return -1;
        }

        if (perform_matchup(info) != 0)
        {
            collocation_info_delete(info);
            return -1;
        }
    }

    if (info->nearest_neighbour_x_criterium_index >= 0 && info->nearest_neighbour_x_criterium_index >= 0)
    {
        /* perform the second nearest neighbour filtering using a filter on the collocation results */
        if (info->perform_nearest_neighbour_x_first)
        {
            resample_nearest_b(info->collocation_result, info->nearest_neighbour_y_criterium_index);
        }
        else
        {
            resample_nearest_a(info->collocation_result, info->nearest_neighbour_x_criterium_index);
        }
    }

    if (harp_collocation_result_write(argv[argc - 1], info->collocation_result) != 0)
    {
        collocation_info_delete(info);
        return -1;
    }

    collocation_info_delete(info);

    return 0;
}
