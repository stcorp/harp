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

#include "harpcollocate.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif
#ifdef WIN32
#include "windows.h"
#endif

#define DETECTION_BLOCK_SIZE 12

const char *collocation_criterion_command_line_option_from_criterion_type(Collocation_criterion_type
                                                                          Collocation_criterion_type)
{
    switch (Collocation_criterion_type)
    {
        case collocation_criterion_type_time:
            return "-dt 'value [unit]'";
            break;

        case collocation_criterion_type_latitude:
            return "-dlat 'value [unit]'";
            break;

        case collocation_criterion_type_longitude:
            return "-dlon 'value [unit]'";
            break;

        case collocation_criterion_type_point_distance:
            return "-dp 'value [unit]'";
            break;

        case collocation_criterion_type_overlapping_percentage:
            return "-da 'value [unit]'";
            break;

        case collocation_criterion_type_overlapping:
            return "-overlap";
            break;

        case collocation_criterion_type_point_a_in_area_b:
            return "-painab";
            break;

        case collocation_criterion_type_point_b_in_area_a:
            return "-pbinaa";
            break;

        case collocation_criterion_type_sza:
            return "-dsza 'value [unit]'";
            break;

        case collocation_criterion_type_saa:
            return "-dsaa 'value [unit]'";
            break;

        case collocation_criterion_type_vza:
            return "-dvza 'value [unit]'";
            break;

        case collocation_criterion_type_vaa:
            return "-dvaa 'value [unit]'";
            break;

        case collocation_criterion_type_theta:
            return "-dtheta 'value [unit]'";
            break;
    }

    return "unknown";
}

const char *weighting_factor_command_line_option_from_difference_type(harp_collocation_difference_type difference_type)
{
    switch (difference_type)
    {
        case difference_type_absolute_difference_in_time:
            return "-wft 'value [unit]'";
            break;

        case difference_type_absolute_difference_in_latitude:
            return "-wflat 'value [unit]'";
            break;

        case difference_type_absolute_difference_in_longitude:
            return "-wflon 'value [unit]'";
            break;

        case difference_type_point_distance:
            return "-wfdp 'value [unit]'";
            break;

        case difference_type_overlapping_percentage:
            return "-wfa 'value [unit]'";
            break;

        case difference_type_absolute_difference_in_sza:
            return "-wfsza 'value [unit]'";
            break;

        case difference_type_absolute_difference_in_saa:
            return "-wfsaa 'value [unit]'";
            break;

        case difference_type_absolute_difference_in_vza:
            return "-wfvza 'value [unit]'";
            break;

        case difference_type_absolute_difference_in_vaa:
            return "-wfvaa 'value [unit]'";
            break;

        case difference_type_absolute_difference_in_theta:
            return "-wftheta 'value [unit]'";
            break;

        case difference_type_delta:
        case difference_type_unknown:
            return "unknown";
            break;
    }

    return "unknown";
}

static void swap_strings(char **a, char **b)
{
    char *tmp;

    tmp = *a;
    *a = *b;
    *b = tmp;
}

static void bubble_sort_string_array(char **string_array, int num_strings)
{
    int i, j;

    for (i = 0; i < num_strings; ++i)
    {
        for (j = i + 1; j < num_strings; ++j)
        {
            if (strcmp(string_array[i], string_array[j]) > 0)
            {
                swap_strings(&string_array[i], &string_array[j]);
            }
        }
    }
}

int dataset_new(Dataset **new_dataset)
{
    Dataset *dataset = NULL;

    dataset = (Dataset *)malloc(sizeof(Dataset));

    if (dataset == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(Dataset), __FILE__, __LINE__);
        return -1;
    }

    dataset->num_files = 0;
    dataset->filename = NULL;
    dataset->datetime_start = NULL;
    dataset->datetime_stop = NULL;

    *new_dataset = dataset;
    return 0;
}

void dataset_delete(Dataset *dataset)
{
    if (dataset == NULL)
    {
        return;
    }

    if (dataset->filename != NULL)
    {
        int i;

        for (i = 0; i < dataset->num_files; ++i)
        {
            free(dataset->filename[i]);
        }
        free(dataset->filename);
    }

    if (dataset->datetime_start != NULL)
    {
        free(dataset->datetime_start);
    }

    if (dataset->datetime_stop != NULL)
    {
        free(dataset->datetime_stop);
    }

    free(dataset);
}

int dataset_add_filename(Dataset *dataset, char *filename_in)
{
    if (dataset->num_files % DATASET_BLOCK_SIZE == 0)
    {
        char **filename = NULL;

        filename = realloc(dataset->filename, (dataset->num_files + DATASET_BLOCK_SIZE) * sizeof(char *));

        if (filename == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           (long)(dataset->num_files + DATASET_BLOCK_SIZE) * sizeof(char *), __FILE__, __LINE__);
            return -1;
        }

        dataset->filename = filename;
    }

    dataset->filename[dataset->num_files] = strdup(filename_in);
    dataset->num_files++;

    return 0;
}

/*----------------------
 * Collocation criteria
 *----------------------*/

static void collocation_criterion_delete(Collocation_criterion *collocation_criterion)
{
    if (collocation_criterion == NULL)
    {
        return;
    }
    else
    {
        if (collocation_criterion->original_unit != NULL)
        {
            free(collocation_criterion->original_unit);
        }

        if (collocation_criterion->collocation_unit != NULL)
        {
            free(collocation_criterion->collocation_unit);
        }

        free(collocation_criterion);
    }
}

static int collocation_criterion_new(Collocation_criterion_type type,
                                     double value, char *original_unit,
                                     char *collocation_unit, Collocation_criterion **new_criterion)
{
    Collocation_criterion *criterion = NULL;

    criterion = (Collocation_criterion *)malloc(sizeof(Collocation_criterion));
    if (criterion == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(Collocation_criterion), __FILE__, __LINE__);
        return -1;
    }

    criterion->type = type;
    criterion->value = value;

    criterion->original_unit = strdup(original_unit);

    if (criterion->original_unit == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        collocation_criterion_delete(criterion);
        return -1;
    }

    criterion->collocation_unit = strdup(collocation_unit);

    if (criterion->collocation_unit == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        collocation_criterion_delete(criterion);
        return -1;
    }

    *new_criterion = criterion;

    return 0;
}

int collocation_options_new(Collocation_options **new_collocation_options)
{
    int i, k;

    Collocation_options *collocation_options = NULL;

    collocation_options = (Collocation_options *)malloc(sizeof(Collocation_options));

    if (collocation_options == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(Collocation_options), __FILE__, __LINE__);
        return -1;
    }

    /* Resampling of existing collocation result file */
    collocation_options->skip_collocate = 0;
    collocation_options->filename_result_in = NULL;

    /* Input/output */
    collocation_options->dataset_a_in = NULL;
    collocation_options->dataset_b_in = NULL;
    collocation_options->filename_result = NULL;

    /* Collocation criteria */
    collocation_options->num_criteria = 0;
    for (i = 0; i < MAX_NUM_COLLOCATION_CRITERIA; i++)
    {
        collocation_options->criterion_is_set[i] = 0;
        collocation_options->criterion[i] = NULL;
    }

    /* Resampling options */
    collocation_options->resampling_method = resampling_method_none;
    collocation_options->num_weighting_factors = 0;
    for (k = 0; k < HARP_COLLOCATION_RESULT_MAX_NUM_DIFFERENCES; k++)
    {
        collocation_options->weighting_factor_is_set[k] = 0;
        collocation_options->weighting_factor[k] = NULL;
    }

    *new_collocation_options = collocation_options;

    return 0;
}

static void collocation_options_init(Collocation_options *collocation_options)
{
    /* Start with no resampling */
    collocation_options->resampling_method = resampling_method_none;
}

static int collocation_options_add_collocation_criterion(Collocation_options *collocation_options,
                                                         const Collocation_criterion *criterion_in,
                                                         Collocation_criterion_type criterion_type)
{
    Collocation_criterion *criterion = NULL;

    /* Enumaration defines ordering of criteria */
    int index = criterion_type;

    if (index < 0 || index >= MAX_NUM_COLLOCATION_CRITERIA)
    {
        harp_set_error(HARP_ERROR_INVALID_INDEX,
                       "Collocation criterion index (%ld) is not in the range [0,%ld) (%s:%u)",
                       MAX_NUM_COLLOCATION_CRITERIA, index, __FILE__, __LINE__);
        return -1;
    }

    /* If already in use, delete the old criterion first */
    criterion = collocation_options->criterion[index];
    if (criterion != NULL)
    {
        collocation_criterion_delete(criterion);
        collocation_options->num_criteria--;
    }

    collocation_criterion_new(criterion_in->type, criterion_in->value,
                              criterion_in->original_unit,
                              criterion_in->collocation_unit, &(collocation_options->criterion[index]));

    collocation_options->criterion_is_set[index] = 1;
    collocation_options->num_criteria++;

    return 0;
}

static int weighting_factor_new(harp_collocation_difference_type difference_type, double value, char *original_unit,
                                char *collocation_unit, Weighting_factor **weighting_factor_new)
{
    Weighting_factor *weighting_factor = NULL;

    weighting_factor = (Weighting_factor *)malloc(sizeof(Weighting_factor));

    if (weighting_factor == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(Weighting_factor), __FILE__, __LINE__);
        return -1;
    }

    weighting_factor->difference_type = difference_type;
    weighting_factor->value = value;
    weighting_factor->original_unit = strdup(original_unit);
    weighting_factor->collocation_unit = strdup(collocation_unit);

    *weighting_factor_new = weighting_factor;
    return 0;
}

static void weighting_factor_delete(Weighting_factor *weighting_factor)
{
    if (weighting_factor == NULL)
    {
        return;
    }
    else
    {
        if (weighting_factor->original_unit != NULL)
        {
            free(weighting_factor->original_unit);
        }

        if (weighting_factor->collocation_unit != NULL)
        {
            free(weighting_factor->collocation_unit);
        }

        free(weighting_factor);
    }
}

void collocation_options_delete(Collocation_options *collocation_options)
{
    if (collocation_options == NULL)
    {
        return;
    }
    else
    {
        int i, k;

        /* Delete input/output data */
        dataset_delete(collocation_options->dataset_a_in);
        dataset_delete(collocation_options->dataset_b_in);

        if (collocation_options->filename_result_in != NULL)
        {
            free(collocation_options->filename_result_in);
        }

        if (collocation_options->filename_result != NULL)
        {
            free(collocation_options->filename_result);
        }

        /* Delete collocation criteria */
        for (i = 0; i < MAX_NUM_COLLOCATION_CRITERIA; i++)
        {
            if (collocation_options->criterion[i] != NULL)
            {
                collocation_criterion_delete(collocation_options->criterion[i]);
            }
        }

        /* Delete resampling options */
        for (k = 0; k < HARP_COLLOCATION_RESULT_MAX_NUM_DIFFERENCES; k++)
        {
            if (collocation_options->weighting_factor[k] != NULL)
            {
                weighting_factor_delete(collocation_options->weighting_factor[k]);
            }
        }
    }

    free(collocation_options);
}

static int collocation_options_add_weighting_factor(Collocation_options *collocation_options,
                                                    Weighting_factor *weighting_factor_in,
                                                    harp_collocation_difference_type difference_type)
{
    Weighting_factor *weighting_factor = NULL;

    /* Enumaration of difference type defines the ordering of weighting factors */
    int index = difference_type;

    if (index < 0)
    {
        harp_set_error(HARP_ERROR_INVALID_INDEX,
                       "Difference type (%ld) is not in the range [0,%ld) (%s:%u)",
                       HARP_COLLOCATION_RESULT_MAX_NUM_DIFFERENCES, index, __FILE__, __LINE__);
        return -1;
    }

    /* If already in use, delete the old criterion first */
    weighting_factor = collocation_options->weighting_factor[index];

    if (weighting_factor != NULL)
    {
        weighting_factor_delete(weighting_factor);
        collocation_options->num_weighting_factors--;
    }

    if (weighting_factor_new(weighting_factor_in->difference_type, weighting_factor_in->value,
                             weighting_factor_in->original_unit,
                             weighting_factor_in->collocation_unit,
                             &(collocation_options->weighting_factor[index])) != 0)
    {
        return -1;
    }

    collocation_options->weighting_factor_is_set[index] = 1;
    collocation_options->num_weighting_factors++;

    return 0;
}

/* Map collocation criterion type to difference type (= weighting_factor type) */
void get_difference_type_from_collocation_criterion_type(Collocation_criterion_type criterion_type,
                                                         harp_collocation_difference_type *difference_type)
{
    switch (criterion_type)
    {
        case collocation_criterion_type_time:
            *difference_type = difference_type_absolute_difference_in_time;
            break;

        case collocation_criterion_type_latitude:
            *difference_type = difference_type_absolute_difference_in_latitude;
            break;

        case collocation_criterion_type_longitude:
            *difference_type = difference_type_absolute_difference_in_longitude;
            break;

        case collocation_criterion_type_point_distance:
            *difference_type = difference_type_point_distance;
            break;

        case collocation_criterion_type_point_a_in_area_b:
        case collocation_criterion_type_point_b_in_area_a:
        case collocation_criterion_type_overlapping:
            *difference_type = difference_type_unknown;
            break;

        case collocation_criterion_type_overlapping_percentage:
            *difference_type = difference_type_overlapping_percentage;
            break;

        case collocation_criterion_type_sza:
            *difference_type = difference_type_absolute_difference_in_sza;
            break;

        case collocation_criterion_type_saa:
            *difference_type = difference_type_absolute_difference_in_saa;
            break;

        case collocation_criterion_type_vza:
            *difference_type = difference_type_absolute_difference_in_vza;
            break;

        case collocation_criterion_type_vaa:
            *difference_type = difference_type_absolute_difference_in_vaa;
            break;

        case collocation_criterion_type_theta:
            *difference_type = difference_type_absolute_difference_in_theta;
            break;
    }
}

static int invert_unit(const char *unit, char **unit_inverted)
{
    size_t stringlength;

    if (*unit_inverted != NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "Input argument '*unit_inverted' must be empty");
        return -1;
    }

    if (unit == NULL)
    {
        *unit_inverted = NULL;
        return 0;
    }

    stringlength = strlen("1/(") + strlen(unit) + strlen(")") + 1;
    *unit_inverted = calloc(stringlength, sizeof(char));
    if (*unit_inverted == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       stringlength * sizeof(char), __FILE__, __LINE__);
        return -1;
    }

    strcat(*unit_inverted, "1/(");
    strcat(*unit_inverted, unit);
    strcat(*unit_inverted, ")");
    return 0;
}

/* The default weightfactors units are the reciprocals of the collocation units collocation criteria.
 * The default weighting factor value is 1. */
static int get_default_weighting_factor_unit_and_value_from_difference_type(harp_collocation_difference_type
                                                                            difference_type, double *new_value,
                                                                            char **new_original_unit,
                                                                            char **new_collocation_unit)
{
    const char *collocation_unit = "";
    char *collocation_unit_inverted = NULL;

    switch (difference_type)
    {
        case difference_type_absolute_difference_in_time:
            collocation_unit = HARP_UNIT_TIME;
            break;

        case difference_type_absolute_difference_in_latitude:
            collocation_unit = HARP_UNIT_LATITUDE;
            break;

        case difference_type_absolute_difference_in_longitude:
            collocation_unit = HARP_UNIT_LONGITUDE;
            break;

        case difference_type_point_distance:
            collocation_unit = HARP_UNIT_LENGTH;
            break;

        case difference_type_overlapping_percentage:
            collocation_unit = HARP_UNIT_PERCENT;
            break;

        case difference_type_absolute_difference_in_sza:
        case difference_type_absolute_difference_in_saa:
        case difference_type_absolute_difference_in_vza:
        case difference_type_absolute_difference_in_vaa:
        case difference_type_absolute_difference_in_theta:
            collocation_unit = HARP_UNIT_ANGLE;
            break;

        case difference_type_unknown:
        case difference_type_delta:
            collocation_unit = "";
            break;
    }

    if (invert_unit(collocation_unit, &collocation_unit_inverted) != 0)
    {
        return -1;
    }

    *new_value = 1.0;
    *new_original_unit = strdup(collocation_unit_inverted);
    *new_collocation_unit = strdup(collocation_unit_inverted);

    free(collocation_unit_inverted);
    return 0;
}

static int collocation_options_add_missing_weighting_factors_with_default_values(Collocation_options
                                                                                 *collocation_options)
{
    int i;

    /* Add default weighting factors for collocation criteria that have been set */
    for (i = 0; i < HARP_COLLOCATION_RESULT_MAX_NUM_DIFFERENCES; i++)
    {
        harp_collocation_difference_type difference_type = i;

        /* If the weighting factor has not been set, set the default one */
        if (collocation_options->weighting_factor_is_set[difference_type] == 0)
        {
            Weighting_factor *weighting_factor = NULL;
            char *original_unit = NULL;
            char *collocation_unit = NULL;
            double value;

            /* Obtain the default value and unit */
            if (get_default_weighting_factor_unit_and_value_from_difference_type
                (difference_type, &value, &original_unit, &collocation_unit) != 0)
            {
                return -1;
            }

            /* Add the weighting factor to the collocation options */
            if (weighting_factor_new(difference_type, value, original_unit, collocation_unit, &weighting_factor) != 0)
            {
                free(original_unit);
                free(collocation_unit);
                return -1;
            }
            if (collocation_options_add_weighting_factor(collocation_options, weighting_factor, difference_type) != 0)
            {
                weighting_factor_delete(weighting_factor);
                free(original_unit);
                free(collocation_unit);
                return -1;
            }

            weighting_factor_delete(weighting_factor);
            free(original_unit);
            free(collocation_unit);
        }
    }

    return 0;
}

/* Put the filenames that are defined with a command line option into the dataset */
static int parse_command_line_option_with_names(int argc, char *argv[], int *argindex, Dataset **new_dataset)
{
    Dataset *dataset = NULL;

    /* Use a temporary index k to update argument index argindex */
    int k;

    /* Proceed to the next argument (which is not command line option such as "-ia") */
    k = *argindex;
    k++;

    if (dataset_new(&dataset) != 0)
    {
        return -1;
    }

    while (k < argc && argv[k][0] != '-')
    {
        /* Put the filename in a string and make some checks */
        char *string = NULL;

        string = strdup(argv[k]);

        /* Copy the filename to the dataset */
        if (dataset_add_filename(dataset, string) != 0)
        {
            free(string);
            dataset_delete(dataset);
            return -1;
        }

        free(string);

        k++;
    }

    /* Return the last index */
    *argindex = k - 1;

    *new_dataset = dataset;

    return 0;
}

static int grab_value_from_string(char *string, int *new_cursor_position, double *new_value)
{
    double value;
    char *value_string = NULL;
    char *cursor = NULL;
    size_t stringlength = 0;

    /* Start with first character of string */
    int original_cursor_position = *new_cursor_position;
    int cursor_position = *new_cursor_position;

    cursor = string + cursor_position;

    while (*cursor == ' ')
    {
        cursor++;
        cursor_position++;
    }
    original_cursor_position = cursor_position;

    /* Determine stringlength */
    stringlength = 0;
    while (*cursor != '[' && *cursor != ' ' && *cursor != ';' && *cursor != '\0')
    {
        cursor++;
        cursor_position++;
        stringlength++;
    }

    /* Rewind */
    cursor = string + original_cursor_position;
    value_string = calloc((stringlength + 1), sizeof(char));
    if (value_string == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)\n",
                       (stringlength + 1) * sizeof(char), __FILE__, __LINE__);
        return -1;
    }
    strncpy(value_string, cursor, stringlength);

    value = (double)atof(value_string);
    free(value_string);

    *new_value = value;
    *new_cursor_position = cursor_position;
    return 0;
}

static int grab_unit_from_string(char *string, int *new_cursor_position, char **new_unit)
{
    char *unit = NULL;
    char *cursor = NULL;
    size_t stringlength = 0;

    /* Start with first character of string */
    int original_cursor_position = *new_cursor_position;
    int cursor_position = *new_cursor_position;

    cursor = string + cursor_position;

    /* Skip leading white space */
    while (*cursor == ' ')
    {
        cursor++;
        cursor_position++;
    }
    original_cursor_position = cursor_position;

    if (*cursor == ';')
    {
        *new_cursor_position = cursor_position;
        *new_unit = NULL;
        return 0;
    }

    /* Search for square bracket */
    if (*cursor != '[')
    {
        /* No unit is found */
        *new_cursor_position = cursor_position;
        *new_unit = NULL;
        return 0;
    }

    if (*cursor == '[')
    {
        cursor++;
        cursor_position++;
        original_cursor_position = cursor_position;
    }

    /* Determine stringlength */
    stringlength = 0;
    while (*cursor != ']' && *cursor != ';' && *cursor != '\0')
    {
        cursor++;
        cursor_position++;
        stringlength++;
    }

    /* Rewind */
    cursor = string + original_cursor_position;
    unit = calloc((stringlength + 1), sizeof(char));
    if (unit == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)\n",
                       (stringlength + 1) * sizeof(char), __FILE__, __LINE__);
        return -1;
    }
    strncpy(unit, cursor, stringlength);

    /* Fast forward to the end */
    cursor = string + cursor_position;
    if (*cursor == ']')
    {
        cursor++;
        cursor_position++;
    }

    /* Done, return result */
    *new_cursor_position = cursor_position;
    *new_unit = unit;
    return 0;
}

/* Get the value and unit that are defined with a
 * command line option (e.g. -dt '3.0 [h]') */
static int parse_command_line_option_with_value_and_unit(int argc, char *argv[], int *argindex,
                                                         double *value_parameter, char **unit_parameter)
{
    double value;
    char *unit = NULL;

    /* Store the current argument index */
    int i = *argindex;

    /* Grab the value and unit: Then go to the next argument. Note that the first character must be numeric digit */
    if (i + 1 < argc && argv[i + 1][0] != '-' && isalnum(argv[i + 1][0]) && !isalpha(argv[i + 1][0]))
    {
        int cursor_position = 0;

        if (grab_value_from_string(argv[i + 1], &cursor_position, &value) != 0)
        {
            return -1;
        }
        if (grab_unit_from_string(argv[i + 1], &cursor_position, &unit) != 0)
        {
            return -1;
        }

        i++;
    }
    else
    {
        char *argument = NULL;

        if (i + 1 < argc)
        {
            argument = argv[i + 1];
        }
        else
        {
            argument = argv[i];
        }

        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "Invalid value/unit in command line option ('%s' ?)", argument);

        return -1;
    }

    if (unit == NULL)
    {
        *unit_parameter = NULL;
    }
    else
    {
        *unit_parameter = strdup(unit);
        free(unit);
    }

    *value_parameter = value;

    /* Update the argument index */
    *argindex = i;

    return 0;
}

static int is_directory(const char *directoryname)
{
    struct stat statbuf;

    /* stat() the directory to be opened */
    if (stat(directoryname, &statbuf) != 0)
    {
        if (errno == ENOENT)
        {
            harp_set_error(HARP_ERROR_FILE_NOT_FOUND, "could not find %s", directoryname);
        }
        else
        {
            harp_set_error(HARP_ERROR_FILE_OPEN, "could not open %s (%s)", directoryname, strerror(errno));
        }
        return -1;
    }

    /* check that the file is a directory */
    if (statbuf.st_mode & S_IFDIR)
    {
        /* Return 'true' */
        return 1;
    }

    /* Return 'false' */
    return 0;
}

static int check_file(const char *filename)
{
    struct stat statbuf;

    /* stat() the file to be opened */
    if (stat(filename, &statbuf) != 0)
    {
        if (errno == ENOENT)
        {
            harp_set_error(HARP_ERROR_FILE_NOT_FOUND, "could not find %s", filename);
        }
        else
        {
            harp_set_error(HARP_ERROR_FILE_OPEN, "could not open %s (%s)", filename, strerror(errno));
        }
        return -1;
    }

    /* check that the file is a regular file */
    if ((statbuf.st_mode & S_IFREG) == 0)
    {
        harp_set_error(HARP_ERROR_FILE_OPEN, "could not open %s (not a regular file)", filename);
        return -1;
    }

    return 0;
}

static int expand_directory_name_into_file_names(const char *pathname, Dataset **dataset_dir)
{
    Dataset *dataset = NULL;

#ifdef WIN32
    WIN32_FIND_DATA FileData;
    HANDLE hSearch;
    BOOL fFinished;
    char *pattern;

    pattern = malloc(strlen(pathname) + 4 + 1);
    if (pattern == NULL)
    {
        coda_set_error(CODA_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       (long)strlen(pathname) + 4 + 1, __FILE__, __LINE__);
        return -1;
    }
    sprintf(pattern, "%s\\*.*", pathname);
    hSearch = FindFirstFile(pattern, &FileData);
    free(pattern);

    if (hSearch == INVALID_HANDLE_VALUE)
    {
        if (GetLastError() == ERROR_FILE_NOT_FOUND || GetLastError() == ERROR_NO_MORE_FILES)
        {
            /* no files found */
            continue;
        }
        coda_set_error(HARP_ERROR_INVALID_ARGUMENT, "could not access directory '%s'", pathname);
        return -1;
    }

    /* Walk through files in directory and add filenames to 'dataset_dir' */
    if (dataset_new(&dataset) != 0)
    {
        FindClose(hSearch);
        return -1;
    }

    fFinished = FALSE;
    while (!fFinished)
    {
        if (!(FileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        {
            char *filepath;

            filepath = malloc(strlen(pathname) + 1 + strlen(FileData.cFileName) + 1);
            if (filepath == NULL)
            {
                harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                               (long)strlen(pathname) + 1 + strlen(FileData.cFileName) + 1, __FILE__,__LINE__);
                dataset_delete(dataset);
                FindClose(hSearch);
                return -1;
            }
            sprintf(filepath, "%s\\%s", pathname, FileData.cFileName);
            if (check_file(filepath) == 0)
            {
                dataset_add_filename(dataset, filepath);
            }
            else
            {
                harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "'%s' is not a valid HARP file", filepath);
                free(filepath);
                dataset_delete(dataset);
                FindClose(hSearch);
                return -1;
            }
            free(filepath);
        }

        if (!FindNextFile(hSearch, &FileData))
        {
            if (GetLastError() == ERROR_NO_MORE_FILES)
            {
                fFinished = TRUE;
            }
            else
            {
                HARP_set_error(HARP_ERROR_INVALID_ARGUMENT, "could not retrieve directory entry");
                dataset_delete(dataset);
                FindClose(hSearch);
                return -1;
            }
        }
    }
    FindClose(hSearch);
#else
    DIR *dirp = NULL;
    struct dirent *dp = NULL;

    /* Open the directory */
    dirp = opendir(pathname);

    if (dirp == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "could not open directory %s", pathname);
        closedir(dirp);
        return -1;
    }

    /* Walk through files in directory and add filenames to 'dataset_dir' */
    if (dataset_new(&dataset) != 0)
    {
        closedir(dirp);
        return -1;
    }

    while ((dp = readdir(dirp)) != NULL)
    {
        char *filepath = NULL;

        /* Skip '.' and '..' */
        if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0)
        {
            continue;
        }

        /* Add path before filename */
        filepath = malloc(strlen(pathname) + 1 + strlen(dp->d_name) + 1);
        if (filepath == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           (long)strlen(pathname) + 1 + strlen(dp->d_name) + 1, __FILE__, __LINE__);
            closedir(dirp);
            dataset_delete(dataset);
            return -1;
        }
        sprintf(filepath, "%s/%s", pathname, dp->d_name);

        if (is_directory(filepath))
        {
            /* Skip subdirectories */
            free(filepath);
            continue;
        }

        if (check_file(filepath) == 0)
        {
            dataset_add_filename(dataset, filepath);
        }
        else
        {
            /* Exit, file type is not supported */
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "'%s' is not a valid HARP file", filepath);
            free(filepath);
            dataset_delete(dataset);
            closedir(dirp);
            return -1;
        }
        free(filepath);
    }

    closedir(dirp);
#endif

    *dataset_dir = dataset;
    return 0;
}

static int detect_dir(const char *pathname, int *is_dir)
{
    struct stat sb;

    if (stat(pathname, &sb) != 0)
    {
        if (errno == ENOENT || errno == ENOTDIR)
        {
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "%s: No such file or directory", pathname);
            return -1;
        }
    }

    if (sb.st_mode & S_IFDIR)
    {
        /* We are dealing with a directory name */
        *is_dir = 1;
    }
    else if (sb.st_mode & S_IFREG)
    {
        /* We are dealing with a file */
        *is_dir = 0;
    }

    return 0;
}

static int sort_file_names(Dataset *input_dataset, Dataset **new_dataset)
{
    int i;
    Dataset *dataset = NULL;
    char **string_array = NULL;
    int num_strings;

    num_strings = input_dataset->num_files;

    /* Create a new dataset */
    if (dataset_new(&dataset) != 0)
    {
        return -1;
    }

    /* Store the filenames in string_array */
    string_array = calloc((size_t)num_strings, sizeof(char *));
    for (i = 0; i < num_strings; i++)
    {
        string_array[i] = strdup(input_dataset->filename[i]);
    }

    /* Sort the filenames */
    bubble_sort_string_array(string_array, num_strings);

    /* Copy the sorted filenames into a new dataset */
    for (i = 0; i < num_strings; i++)
    {
        if (dataset_add_filename(dataset, string_array[i]) != 0)
        {
            int j;

            for (j = 0; j < num_strings; j++)
            {
                free(string_array[j]);
            }
            free(string_array);

            dataset_delete(dataset);

            return -1;
        }
    }

    for (i = 0; i < num_strings; i++)
    {
        free(string_array[i]);
    }
    free(string_array);

    *new_dataset = dataset;

    return 0;
}

static int turn_directory_names_into_separate_file_names(const Dataset *input_dataset, Dataset **new_dataset)
{
    int i;
    int is_dir = 0;

    Dataset *output_dataset = NULL;

    if (dataset_new(&output_dataset) != 0)
    {
        return -1;
    }

    for (i = 0; i < input_dataset->num_files; i++)
    {
        if (detect_dir(input_dataset->filename[i], &is_dir) != 0)
        {
            dataset_delete(output_dataset);
            return -1;
        }

        if (is_dir)
        {
            int j;

            Dataset *dataset_dir = NULL;
            Dataset *dataset_dir_sorted = NULL;

            /* Get the file names of the valid files in the directory and put them in 'dataset_dir' */
            if (expand_directory_name_into_file_names(input_dataset->filename[i], &dataset_dir) != 0)
            {
                dataset_delete(output_dataset);
                return -1;
            }

            /* Make sure that the directory contains files */
            if (dataset_dir->num_files < 1)
            {
                harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "%s: Empty directory", input_dataset->filename[i]);
                dataset_delete(output_dataset);
                return -1;
            }

            /* Sort the filenames in 'dataset_dir' */
            if (sort_file_names(dataset_dir, &dataset_dir_sorted) != 0)
            {
                dataset_delete(output_dataset);
                dataset_delete(dataset_dir);
                return -1;
            }

            /* Add the sorted files to 'dataset' */
            for (j = 0; j < dataset_dir_sorted->num_files; j++)
            {
                if (check_file(dataset_dir_sorted->filename[j]) != 0)
                {
                    dataset_delete(output_dataset);
                    dataset_delete(dataset_dir);
                    dataset_delete(dataset_dir_sorted);
                    return -1;
                }

                if (dataset_add_filename(output_dataset, dataset_dir_sorted->filename[j]) != 0)
                {
                    dataset_delete(output_dataset);
                    dataset_delete(dataset_dir);
                    dataset_delete(dataset_dir_sorted);
                    return -1;
                }
            }

            dataset_delete(dataset_dir);
            dataset_delete(dataset_dir_sorted);
        }
        else
        {
            if (check_file(input_dataset->filename[i]) != 0)
            {
                dataset_delete(output_dataset);
                return -1;
            }

            if (dataset_add_filename(output_dataset, input_dataset->filename[i]) != 0)
            {
                dataset_delete(output_dataset);
                return -1;
            }
        }
    }

    *new_dataset = output_dataset;

    return 0;
}

static int parse_command_line_option_with_filenames(int argc, char *argv[], int *i, Dataset **new_dataset)
{
    Dataset *dataset_original = NULL;
    Dataset *dataset_expanded = NULL;

    /* Grab the directory names and filenames, and update the index i */
    if (parse_command_line_option_with_names(argc, argv, i, &dataset_original) != 0)
    {
        return -1;
    }

    /* Turn all directory names (if any) into filenames */
    if (turn_directory_names_into_separate_file_names(dataset_original, &dataset_expanded) != 0)
    {
        dataset_delete(dataset_original);
        return -1;
    }

    dataset_delete(dataset_original);

    *new_dataset = dataset_expanded;

    return 0;
}

static void collocation_unit_from_criterion_type(Collocation_criterion_type type, char **collocation_unit)
{
    switch (type)
    {
        case collocation_criterion_type_time:
            *collocation_unit = strdup(HARP_UNIT_TIME);
            break;

        case collocation_criterion_type_latitude:
            *collocation_unit = strdup(HARP_UNIT_LATITUDE);
            break;

        case collocation_criterion_type_longitude:
            *collocation_unit = strdup(HARP_UNIT_LONGITUDE);
            break;

        case collocation_criterion_type_point_distance:
            *collocation_unit = strdup(HARP_UNIT_LENGTH);
            break;

        case collocation_criterion_type_overlapping_percentage:
            *collocation_unit = strdup(HARP_UNIT_PERCENT);
            break;

        case collocation_criterion_type_sza:
        case collocation_criterion_type_saa:
        case collocation_criterion_type_vza:
        case collocation_criterion_type_vaa:
        case collocation_criterion_type_theta:
            *collocation_unit = strdup(HARP_UNIT_ANGLE);
            break;

        case collocation_criterion_type_point_a_in_area_b:
        case collocation_criterion_type_point_b_in_area_a:
        case collocation_criterion_type_overlapping:
            *collocation_unit = strdup(HARP_UNIT_DIMENSIONLESS);
            break;
    }
}

static int grab_collocation_criterion(Collocation_options *collocation_options,
                                      int *argindex, Collocation_criterion_type criterion_type, int argc, char *argv[])
{
    int i = *argindex;

    double value;
    long num_values = 1;
    double values[1];
    char *original_unit = NULL;
    char *collocation_unit = NULL;
    Collocation_criterion *criterion = NULL;

    /* Grab the value and unit, and update the index i */
    if (parse_command_line_option_with_value_and_unit(argc, argv, &i, &value, &original_unit) != 0)
    {
        return -1;
    }

    *argindex = i;

    /* Get the collocation unit */
    collocation_unit_from_criterion_type(criterion_type, &collocation_unit);

    if (original_unit == NULL)
    {
        original_unit = strdup(collocation_unit);
        if (original_unit == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                           __LINE__);
            if (collocation_unit != NULL)
            {
                free(collocation_unit);
            }
            return -1;
        }
    }

    /* Convert the value to the unit that is used for collocation */
    values[0] = value;

    if (strcmp(original_unit, collocation_unit) != 0)
    {
        if (harp_convert_unit(original_unit, collocation_unit, num_values, values) != 0)
        {
            harp_set_error(HARP_ERROR_INVALID_NAME, "Invalid unit '%s' for command line option '%s'", original_unit,
                           collocation_criterion_command_line_option_from_criterion_type(criterion_type));

            if (original_unit != NULL)
            {
                free(original_unit);
            }

            if (collocation_unit != NULL)
            {
                free(collocation_unit);
            }

            return -1;
        }
    }

    value = values[0];

    /* Add the criterion to the collection of collocation criteria */
    collocation_criterion_new(criterion_type, value, original_unit, collocation_unit, &criterion);
    collocation_options_add_collocation_criterion(collocation_options, criterion, criterion_type);
    collocation_criterion_delete(criterion);

    if (original_unit != NULL)
    {
        free(original_unit);
    }

    if (collocation_unit != NULL)
    {
        free(collocation_unit);
    }

    return 0;
}

static void get_weighting_factor_collocation_unit_from_difference_type(harp_collocation_difference_type difference_type,
                                                                       char **collocation_unit)
{
    switch (difference_type)
    {
        case difference_type_absolute_difference_in_time:
            *collocation_unit = strdup("1/s");
            break;

        case difference_type_absolute_difference_in_latitude:
            *collocation_unit = strdup("1/degree_north");
            break;

        case difference_type_absolute_difference_in_longitude:
            *collocation_unit = strdup("1/degree_east");
            break;

        case difference_type_point_distance:
            *collocation_unit = strdup("1/m");
            break;

        case difference_type_overlapping_percentage:
            *collocation_unit = strdup("1/percent");
            break;

        case difference_type_absolute_difference_in_sza:
        case difference_type_absolute_difference_in_saa:
        case difference_type_absolute_difference_in_vza:
        case difference_type_absolute_difference_in_vaa:
        case difference_type_absolute_difference_in_theta:
            *collocation_unit = strdup("1/degree");
            break;

        case difference_type_unknown:
        case difference_type_delta:
            *collocation_unit = strdup("");
            break;
    }
}

static int grab_weighting_factor(Collocation_options *collocation_options, int *argindex,
                                 harp_collocation_difference_type difference_type, int argc, char *argv[])
{
    int i = *argindex;

    double value = 0.0;
    long num_values = 1;
    double values[1];
    char *original_unit = NULL;
    char *collocation_unit = NULL;
    Weighting_factor *weighting_factor = NULL;

    /* Grab the value and unit, and update the index i */
    if (parse_command_line_option_with_value_and_unit(argc, argv, &i, &value, &original_unit) != 0)
    {
        return -1;
    }

    *argindex = i;

    /* Get the variable name and collocation unit */
    get_weighting_factor_collocation_unit_from_difference_type(difference_type, &collocation_unit);

    if (original_unit == NULL)
    {
        original_unit = strdup(collocation_unit);
    }

    /* Convert the value to the unit that is used for collocation */
    values[0] = value;

    if (strcmp(original_unit, collocation_unit) != 0)
    {
        if (harp_convert_unit(original_unit, collocation_unit, num_values, values) != 0)
        {
            harp_set_error(HARP_ERROR_INVALID_NAME, "Invalid unit '%s' for command line option '%s'", original_unit,
                           weighting_factor_command_line_option_from_difference_type(difference_type));
            if (original_unit != NULL)
            {
                free(original_unit);
            }
            if (collocation_unit != NULL)
            {
                free(collocation_unit);
            }
            return -1;
        }
    }

    value = values[0];

    /* Add the weighting factor to the resampling options */
    if (weighting_factor_new(difference_type, value, original_unit, collocation_unit, &weighting_factor) != 0)
    {
        if (original_unit != NULL)
        {
            free(original_unit);
        }
        if (collocation_unit != NULL)
        {
            free(collocation_unit);
        }
        return -1;
    }

    if (collocation_options_add_weighting_factor(collocation_options, weighting_factor, difference_type) != 0)
    {
        if (original_unit != NULL)
        {
            free(original_unit);
        }
        if (collocation_unit != NULL)
        {
            free(collocation_unit);
        }
        weighting_factor_delete(weighting_factor);
        return -1;
    }

    weighting_factor_delete(weighting_factor);

    if (original_unit != NULL)
    {
        free(original_unit);
    }

    if (collocation_unit != NULL)
    {
        free(collocation_unit);
    }

    return 0;
}

int parse_arguments(int argc, char *argv[], Collocation_mode *new_collocation_mode,
                    Collocation_options **new_collocation_options)
{
    Collocation_mode collocation_mode;
    Collocation_options *collocation_options = NULL;
    int option_ia = 0;  /* Matchup mode and update mode */
    int option_ib = 0;  /* Matchup mode and update mode */
    int option_ir = 0;  /* Resampling mode and update mode */
    int option_or = 0;  /* Matchup mode, resampling mode and update mode */
    int i;

    if (argc < 3)
    {
        fprintf(stderr, "ERROR: invalid arguments\n");
        print_help();
        exit(1);
    }

    /* Make sure that first argument is not a '-foo' option */
    if (argv[1][0] == '-')
    {
        fprintf(stderr, "ERROR: invalid arguments\n");
        print_help();
        exit(1);
    }

    /* Determine the collocation mode */
    if (strcmp(argv[1], "matchup") == 0)
    {
        collocation_mode = collocation_mode_matchup;
    }
    else if (strcmp(argv[1], "resample") == 0)
    {
        collocation_mode = collocation_mode_resample;
    }
    else if (strcmp(argv[1], "update") == 0)
    {
        collocation_mode = collocation_mode_update;
    }
    else
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "Incorrect collocation mode '%s'", argv[1]);
        return -1;
    }

    /* Print help */
    if (argc == 3 || (strcmp(argv[2], "-h") == 0 || strcmp(argv[2], "--help") == 0))
    {
        switch (collocation_mode)
        {
            case collocation_mode_matchup:
                print_help_matchup();
                break;

            case collocation_mode_resample:
                print_help_resample();
                break;

            case collocation_mode_update:
                print_help_update();
                break;
        }
        exit(0);
    }

    /* Create new collocation options data structure */
    if (collocation_options_new(&collocation_options) != 0)
    {
        return -1;
    }

    /* Start with default collocation options */
    collocation_options_init(collocation_options);

    /* Parse the arguments */
    for (i = 2; i < argc; i++)
    {
        /*--------------------------------------------
         * Arguments for resampling existing filename
         *--------------------------------------------*/

        if ((strcmp(argv[i], "-ir") == 0 || strcmp(argv[i], "--input-result") == 0) && i + 1 < argc &&
            argv[i + 1][0] != '-')
        {
            option_ir = 1;

            /* Grab the name of the collocation result file */
            if (collocation_options->filename_result_in != NULL)
            {
                harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "Filename of input collocation result must be empty");
                collocation_options_delete(collocation_options);
                return -1;
            }
            collocation_options->filename_result_in = strdup(argv[i + 1]);
            if (collocation_options->filename_result_in == NULL)
            {
                harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                               __LINE__);
                collocation_options_delete(collocation_options);
                return -1;
            }
            collocation_options->skip_collocate = 1;

            i++;
        }

        /*-------------------------------
         * Gather input/output filenames
         *-------------------------------*/

        else if ((strcmp(argv[i], "-ia") == 0 || strcmp(argv[i], "--input-a") == 0) && i + 1 < argc &&
                 argv[i + 1][0] != '-')
        {
            Dataset *dataset = NULL;

            /* Get the original filenames, and update the index i */
            if (parse_command_line_option_with_filenames(argc, argv, &i, &dataset) != 0)
            {
                collocation_options_delete(collocation_options);
                return -1;
            }

            option_ia = 1;

            collocation_options->dataset_a_in = dataset;
        }
        else if ((strcmp(argv[i], "-ib") == 0 || strcmp(argv[i], "--input-b") == 0) && i + 1 < argc &&
                 argv[i + 1][0] != '-')
        {
            Dataset *dataset = NULL;

            /* Get the original filenames, and update the index i */
            if (parse_command_line_option_with_filenames(argc, argv, &i, &dataset) != 0)
            {
                collocation_options_delete(collocation_options);
                return -1;
            }

            option_ib = 1;

            collocation_options->dataset_b_in = dataset;
        }
        else if ((strcmp(argv[i], "-or") == 0 || strcmp(argv[i], "--output-result") == 0) && i + 1 < argc &&
                 argv[i + 1][0] != '-')
        {
            option_or = 1;

            /* Grab the name of the collocation result file */
            if (collocation_options->filename_result != NULL)
            {
                harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "Filename of collocation result must be empty");
                collocation_options_delete(collocation_options);
                return -1;
            }
            collocation_options->filename_result = strdup(argv[i + 1]);

            i++;
        }

         /*-----------------------------
         * Gather collocation criteria
         *-----------------------------*/

        /* Check for maximum allowed difference in time */
        else if (strcmp(argv[i], "-dt") == 0 && i + 1 < argc && argv[i + 1][0] != '-')
        {
            if (grab_collocation_criterion(collocation_options, &i, collocation_criterion_type_time, argc, argv) != 0)
            {
                collocation_options_delete(collocation_options);
                return -1;
            }
        }

        /* Check for maximum allowed difference in latitude */
        else if (strcmp(argv[i], "-dlat") == 0 && i + 1 < argc && argv[i + 1][0] != '-')
        {
            if (grab_collocation_criterion
                (collocation_options, &i, collocation_criterion_type_latitude, argc, argv) != 0)
            {
                collocation_options_delete(collocation_options);
                return -1;
            }
        }

        /* Check for maximum allowed difference in longitude */
        else if (strcmp(argv[i], "-dlon") == 0 && i + 1 < argc && argv[i + 1][0] != '-')
        {
            if (grab_collocation_criterion
                (collocation_options, &i, collocation_criterion_type_longitude, argc, argv) != 0)
            {
                collocation_options_delete(collocation_options);
                return -1;
            }
        }

        /* Check for maximum allowed point distance */
        else if (strcmp(argv[i], "-dp") == 0 && i + 1 < argc && argv[i + 1][0] != '-')
        {
            if (grab_collocation_criterion
                (collocation_options, &i, collocation_criterion_type_point_distance, argc, argv) != 0)
            {
                collocation_options_delete(collocation_options);
                return -1;
            }
        }

        /* Points of dataset A must fall in polygon areas of B */
        else if (strcmp(argv[i], "-painab") == 0 && i + 1 < argc && argv[i + 1][0] != '-')
        {
            if (grab_collocation_criterion
                (collocation_options, &i, collocation_criterion_type_point_a_in_area_b, argc, argv) != 0)
            {
                collocation_options_delete(collocation_options);
                return -1;
            }
        }

        /* Points of dataset B must fall in polygon areas of A */
        else if (strcmp(argv[i], "-pbinaa") == 0 && i + 1 < argc && argv[i + 1][0] != '-')
        {
            if (grab_collocation_criterion
                (collocation_options, &i, collocation_criterion_type_point_b_in_area_a, argc, argv) != 0)
            {
                collocation_options_delete(collocation_options);
                return -1;
            }
        }

        /* Check for minimum allowed overlapping percentage */
        else if (strcmp(argv[i], "-da") == 0 && i + 1 < argc && argv[i + 1][0] != '-')
        {
            if (grab_collocation_criterion
                (collocation_options, &i, collocation_criterion_type_overlapping_percentage, argc, argv) != 0)
            {
                collocation_options_delete(collocation_options);
                return -1;
            }
        }

        /* Areas must overlap (without need to specify the overlapping percentage) */
        else if (strcmp(argv[i], "-overlap") == 0 && i + 1 < argc && argv[i + 1][0] != '-')
        {
            if (grab_collocation_criterion
                (collocation_options, &i, collocation_criterion_type_overlapping, argc, argv) != 0)
            {
                collocation_options_delete(collocation_options);
                return -1;
            }
        }

        /* Check for maximum allowed difference in SZA */
        else if (strcmp(argv[i], "-dsza") == 0 && i + 1 < argc && argv[i + 1][0] != '-')
        {
            if (grab_collocation_criterion(collocation_options, &i, collocation_criterion_type_sza, argc, argv) != 0)
            {
                collocation_options_delete(collocation_options);
                return -1;
            }
        }

        /* Check for maximum allowed difference in SAA */
        else if (strcmp(argv[i], "-dsaa") == 0 && i + 1 < argc && argv[i + 1][0] != '-')
        {
            if (grab_collocation_criterion(collocation_options, &i, collocation_criterion_type_saa, argc, argv) != 0)
            {
                collocation_options_delete(collocation_options);
                return -1;
            }
        }

        /* Check for maximum allowed difference in VZA */
        else if (strcmp(argv[i], "-dvza") == 0 && i + 1 < argc && argv[i + 1][0] != '-')
        {
            if (grab_collocation_criterion(collocation_options, &i, collocation_criterion_type_vza, argc, argv) != 0)
            {
                collocation_options_delete(collocation_options);
                return -1;
            }
        }

        /* Check for maximum allowed difference in VAA */
        else if (strcmp(argv[i], "-dvaa") == 0 && i + 1 < argc && argv[i + 1][0] != '-')
        {
            if (grab_collocation_criterion(collocation_options, &i, collocation_criterion_type_vaa, argc, argv) != 0)
            {
                collocation_options_delete(collocation_options);
                return -1;
            }
        }

        /* Check for maximum allowed difference in scattering angle */
        else if (strcmp(argv[i], "-dtheta") == 0 && i + 1 < argc && argv[i + 1][0] != '-')
        {
            if (grab_collocation_criterion(collocation_options, &i, collocation_criterion_type_theta, argc, argv) != 0)
            {
                collocation_options_delete(collocation_options);
                return -1;
            }
        }

        /*-----------------------------
         * Gather resampling options
         *-----------------------------*/

        /* Grab resampling method */
        else if (strcmp(argv[i], "-Rnna") == 0 || strcmp(argv[i], "--nearest-neighbour-a") == 0)
        {
            switch (collocation_options->resampling_method)
            {
                    /* Only -Rnna on command line */
                case resampling_method_none:
                    collocation_options->resampling_method = resampling_method_nearest_neighbour_a;
                    break;

                    /* Both -Rnnb -Rnna */
                case resampling_method_nearest_neighbour_b:
                    collocation_options->resampling_method = resampling_method_nearest_neighbour_ba;
                    break;

                case resampling_method_nearest_neighbour_a:
                case resampling_method_nearest_neighbour_ab:
                case resampling_method_nearest_neighbour_ba:
                    collocation_options_delete(collocation_options);
                    harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "Incorrect arguments [%d] '%s'", i, argv[i]);
                    return -1;
                    break;
            }
        }
        else if (strcmp(argv[i], "-Rnnb") == 0 || strcmp(argv[i], "--nearest-neighbour-b") == 0)
        {
            switch (collocation_options->resampling_method)
            {
                    /* Only -Rnnb on command line */
                case resampling_method_none:
                    collocation_options->resampling_method = resampling_method_nearest_neighbour_b;
                    break;

                    /* Both -Rnna -Rnnb */
                case resampling_method_nearest_neighbour_a:
                    collocation_options->resampling_method = resampling_method_nearest_neighbour_ab;
                    break;

                case resampling_method_nearest_neighbour_b:
                case resampling_method_nearest_neighbour_ab:
                case resampling_method_nearest_neighbour_ba:
                    collocation_options_delete(collocation_options);
                    harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "Incorrect arguments [%d] '%s'", i, argv[i]);
                    return -1;
                    break;
            }
        }

        /* Check for override of default value of time weight factor */
        else if (strcmp(argv[i], "-wft") == 0 && i + 1 < argc && argv[i + 1][0] != '-')
        {
            if (grab_weighting_factor
                (collocation_options, &i, difference_type_absolute_difference_in_time, argc, argv) != 0)
            {
                collocation_options_delete(collocation_options);
                return -1;
            }
        }

        /* Check for override of default value of point distance weight factor */
        else if (strcmp(argv[i], "-wfdp") == 0 && i + 1 < argc && argv[i + 1][0] != '-')
        {
            if (grab_weighting_factor(collocation_options, &i, difference_type_point_distance, argc, argv) != 0)
            {
                collocation_options_delete(collocation_options);
                return -1;
            }
        }

        /* Check for override of default value of overlapping percentage weight factor */
        else if (strcmp(argv[i], "-wfa") == 0 && i + 1 < argc && argv[i + 1][0] != '-')
        {
            if (grab_weighting_factor(collocation_options, &i, difference_type_overlapping_percentage, argc, argv) != 0)
            {
                collocation_options_delete(collocation_options);
                return -1;
            }
        }

        /* Check for override of default value of SZA weight factor */
        else if (strcmp(argv[i], "-wfsza") == 0 && i + 1 < argc && argv[i + 1][0] != '-')
        {
            if (grab_weighting_factor
                (collocation_options, &i, difference_type_absolute_difference_in_sza, argc, argv) != 0)
            {
                collocation_options_delete(collocation_options);
                return -1;
            }
        }

        /* Check for override of default value of SAA weight factor */
        else if (strcmp(argv[i], "-wfsaa") == 0 && i + 1 < argc && argv[i + 1][0] != '-')
        {
            if (grab_weighting_factor
                (collocation_options, &i, difference_type_absolute_difference_in_saa, argc, argv) != 0)
            {
                collocation_options_delete(collocation_options);
                return -1;
            }
        }

        /* Check for override of default value of VZA weight factor */
        else if (strcmp(argv[i], "-wfvza") == 0 && i + 1 < argc && argv[i + 1][0] != '-')
        {
            if (grab_weighting_factor
                (collocation_options, &i, difference_type_absolute_difference_in_vza, argc, argv) != 0)
            {
                collocation_options_delete(collocation_options);
                return -1;
            }
        }

        /* Check for override of default value of VAA weight factor */
        else if (strcmp(argv[i], "-wfvaa") == 0 && i + 1 < argc && argv[i + 1][0] != '-')
        {
            if (grab_weighting_factor
                (collocation_options, &i, difference_type_absolute_difference_in_vaa, argc, argv) != 0)
            {
                collocation_options_delete(collocation_options);
                return -1;
            }
        }

        /* Check for override of default value of scattering angle weight factor */
        else if (strcmp(argv[i], "-wftheta") == 0 && i + 1 < argc && argv[i + 1][0] != '-')
        {
            if (grab_weighting_factor
                (collocation_options, &i, difference_type_absolute_difference_in_theta, argc, argv) != 0)
            {
                collocation_options_delete(collocation_options);
                return -1;
            }
        }

        /*----------------------------------
         * Other arguments are not accepted
         *----------------------------------*/

        else
        {
            collocation_options_delete(collocation_options);
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "Incorrect arguments [%d] '%s'", i, argv[i]);
            return -1;
        }

    }

    /* Validate the arguments */
    switch (collocation_mode)
    {
        case collocation_mode_matchup:

            /* Standard collocation mode */
            /* Make sure that input filenames of dataset A and B are set */
            if (option_ia == 0 || option_ib == 0)
            {
                collocation_options_delete(collocation_options);
                harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "Obligatory parameters -ia and -ib parameter are not set");
                return -1;
            }

            /* When not set, a default name is adopted for the collocation result */
            if (option_or == 0)
            {
                if (collocation_options->filename_result != NULL)
                {
                    collocation_options_delete(collocation_options);
                    harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "Filename of collocation result must be empty");
                    return -1;
                }
                collocation_options->filename_result = strdup("collocation_result.csv");
            }

            break;

        case collocation_mode_resample:

            /* Resample existing collocation mode (skip collocation, go directly to resampling result file) */
            if (option_ir == 1)
            {
                if (option_ia == 1 || option_ib == 1)
                {
                    collocation_options_delete(collocation_options);
                    harp_set_error(HARP_ERROR_INVALID_ARGUMENT,
                                   "Incorrect arguments [%d] %i, not allowed to set both -ia/-ib and -ir", i, argv[i]);
                    return -1;
                }

                /* By default, overwrite the input collocation result */
                if (option_or == 0)
                {
                    if (collocation_options->filename_result != NULL)
                    {
                        collocation_options_delete(collocation_options);
                        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "Filename of collocation result must be empty");
                        return -1;
                    }

                    collocation_options->filename_result = strdup(collocation_options->filename_result_in);
                }
            }
            break;

        case collocation_mode_update:

            /* Update mode */
            /* Make sure that input filenames of at least dataset A or B are set */
            if (option_ia == 0 && option_ib == 0)
            {
                collocation_options_delete(collocation_options);
                harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "Obligatory parameter -ia or -ib is not set");
                return -1;
            }

            /* When not set, a default name is adopted for the collocation result */
            if (option_or == 0)
            {
                if (collocation_options->filename_result != NULL)
                {
                    collocation_options_delete(collocation_options);
                    harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "Filename of collocation result must be empty");
                    return -1;
                }
                collocation_options->filename_result = strdup("collocation_result.csv");
            }

            break;
    }

    /* Add default weighting factors for the ones that have not been set by user */
    if (collocation_options_add_missing_weighting_factors_with_default_values(collocation_options) != 0)
    {
        collocation_options_delete(collocation_options);
        return -1;
    }

    *new_collocation_mode = collocation_mode;
    *new_collocation_options = collocation_options;
    return 0;
}
