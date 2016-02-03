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

#include "harp-filter.h"

#define NUM_COMPARISON_OPERATORS 6
#define NUM_MEMBERSHIP_OPERATORS 2

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Type casting. */
typedef struct type_cast_args_struct
{
    harp_predicate *predicate;
} type_cast_args;

/* *INDENT-OFF* */
#define DEFINE_TYPE_CAST_FUNCTION(TYPE_NAME, TYPE) \
static uint8_t type_cast_##TYPE_NAME(void *untyped_args, const void *untyped_value) \
{ \
    type_cast_args *args = (type_cast_args *)untyped_args; \
    double value = (double)*((TYPE *)untyped_value); \
    return args->predicate->eval(args->predicate->args, &value); \
}

DEFINE_TYPE_CAST_FUNCTION(int8,int8_t)
DEFINE_TYPE_CAST_FUNCTION(int16,int16_t)
DEFINE_TYPE_CAST_FUNCTION(int32,int32_t)
DEFINE_TYPE_CAST_FUNCTION(float,float)
DEFINE_TYPE_CAST_FUNCTION(double,double)

static harp_predicate_eval_func *type_cast_func[HARP_NUM_DATA_TYPES] =
    {type_cast_int8, type_cast_int16, type_cast_int32, type_cast_float, type_cast_double, NULL};
/* *INDENT-ON* */

/* Unit conversion. */
typedef struct unit_conversion_args_struct
{
    harp_unit_converter *unit_converter;
    harp_predicate *predicate;
} unit_conversion_args;

/* *INDENT-OFF* */
#define DEFINE_UNIT_CONVERSION_FUNCTION(TYPE_NAME, TYPE) \
static uint8_t unit_convert_##TYPE_NAME(void *untyped_args, const void *untyped_value) \
{ \
    unit_conversion_args *args = (unit_conversion_args *)untyped_args; \
    double value = harp_unit_converter_convert(args->unit_converter, (double)*((TYPE *)untyped_value)); \
    return args->predicate->eval(args->predicate->args, &value); \
}

DEFINE_UNIT_CONVERSION_FUNCTION(int8,int8_t)
DEFINE_UNIT_CONVERSION_FUNCTION(int16,int16_t)
DEFINE_UNIT_CONVERSION_FUNCTION(int32,int32_t)
DEFINE_UNIT_CONVERSION_FUNCTION(float,float)
DEFINE_UNIT_CONVERSION_FUNCTION(double,double)

static harp_predicate_eval_func *unit_conversion_func[HARP_NUM_DATA_TYPES] =
    {unit_convert_int8, unit_convert_int16, unit_convert_int32, unit_convert_float, unit_convert_double, NULL};
/* *INDENT-ON* */

/* Comparison. */
typedef struct comparison_args_struct
{
    double value;
} comparison_args;

/* *INDENT-OFF* */
#define DEFINE_COMPARISON_FUNCTION(OPERATOR_NAME, OPERATOR, TYPE_NAME, TYPE) \
static uint8_t compare_##OPERATOR_NAME##_##TYPE_NAME(void *untyped_args, const void *untyped_value) \
{ \
    return ((double)*((TYPE *)untyped_value)) OPERATOR ((comparison_args *)untyped_args)->value; \
}

DEFINE_COMPARISON_FUNCTION(eq,==,int8,int8_t)
DEFINE_COMPARISON_FUNCTION(eq,==,int16,int16_t)
DEFINE_COMPARISON_FUNCTION(eq,==,int32,int32_t)
DEFINE_COMPARISON_FUNCTION(eq,==,float,float)
DEFINE_COMPARISON_FUNCTION(eq,==,double,double)

DEFINE_COMPARISON_FUNCTION(ne,!=,int8,int8_t)
DEFINE_COMPARISON_FUNCTION(ne,!=,int16,int16_t)
DEFINE_COMPARISON_FUNCTION(ne,!=,int32,int32_t)
DEFINE_COMPARISON_FUNCTION(ne,!=,float,float)
DEFINE_COMPARISON_FUNCTION(ne,!=,double,double)

DEFINE_COMPARISON_FUNCTION(lt,<,int8,int8_t)
DEFINE_COMPARISON_FUNCTION(lt,<,int16,int16_t)
DEFINE_COMPARISON_FUNCTION(lt,<,int32,int32_t)
DEFINE_COMPARISON_FUNCTION(lt,<,float,float)
DEFINE_COMPARISON_FUNCTION(lt,<,double,double)

DEFINE_COMPARISON_FUNCTION(le,<=,int8,int8_t)
DEFINE_COMPARISON_FUNCTION(le,<=,int16,int16_t)
DEFINE_COMPARISON_FUNCTION(le,<=,int32,int32_t)
DEFINE_COMPARISON_FUNCTION(le,<=,float,float)
DEFINE_COMPARISON_FUNCTION(le,<=,double,double)

DEFINE_COMPARISON_FUNCTION(gt,>,int8,int8_t)
DEFINE_COMPARISON_FUNCTION(gt,>,int16,int16_t)
DEFINE_COMPARISON_FUNCTION(gt,>,int32,int32_t)
DEFINE_COMPARISON_FUNCTION(gt,>,float,float)
DEFINE_COMPARISON_FUNCTION(gt,>,double,double)

DEFINE_COMPARISON_FUNCTION(ge,>=,int8,int8_t)
DEFINE_COMPARISON_FUNCTION(ge,>=,int16,int16_t)
DEFINE_COMPARISON_FUNCTION(ge,>=,int32,int32_t)
DEFINE_COMPARISON_FUNCTION(ge,>=,float,float)
DEFINE_COMPARISON_FUNCTION(ge,>=,double,double)

static harp_predicate_eval_func *comparison_func[NUM_COMPARISON_OPERATORS][HARP_NUM_DATA_TYPES] =
    {{compare_eq_int8, compare_eq_int16, compare_eq_int32, compare_eq_float, compare_eq_double, NULL},
     {compare_ne_int8, compare_ne_int16, compare_ne_int32, compare_ne_float, compare_ne_double, NULL},
     {compare_lt_int8, compare_lt_int16, compare_lt_int32, compare_lt_float, compare_lt_double, NULL},
     {compare_le_int8, compare_le_int16, compare_le_int32, compare_le_float, compare_le_double, NULL},
     {compare_gt_int8, compare_gt_int16, compare_gt_int32, compare_gt_float, compare_gt_double, NULL},
     {compare_ge_int8, compare_ge_int16, compare_ge_int32, compare_ge_float, compare_ge_double, NULL}};
/* *INDENT-ON* */

/* String comparison. */
typedef struct string_comparison_args_struct
{
    char *value;
} string_comparision_args;

static uint8_t string_compare_eq(void *untyped_args, const void *untyped_value)
{
    return (strcmp(*((const char **)untyped_value), ((string_comparision_args *)untyped_args)->value) == 0);
}

static uint8_t string_compare_ne(void *untyped_args, const void *untyped_value)
{
    return (strcmp(*((const char **)untyped_value), ((string_comparision_args *)untyped_args)->value) != 0);
}

/* *INDENT-OFF* */
static harp_predicate_eval_func *string_comparison_func[NUM_COMPARISON_OPERATORS] =
    {string_compare_eq, string_compare_ne, NULL, NULL, NULL, NULL};
/* *INDENT-ON* */

/* Set membership. */
typedef struct membership_test_args_struct
{
    int num_values;
    double *value;
} membership_test_args;

/* *INDENT-OFF* */
#define DEFINE_MEMBERSHIP_TEST_IN_FUNCTION(TYPE_NAME, TYPE) \
static uint8_t test_membership_in_##TYPE_NAME(void *untyped_args, const void *untyped_value) \
{ \
    membership_test_args *args = (membership_test_args *)untyped_args; \
    double value = (double)*((TYPE *)untyped_value); \
    const double *first = args->value, *last = (args->value + args->num_values); \
    for (; first != last; ++first) \
    { \
        if (value == *first) \
        { \
            return 1; \
        } \
    } \
    return 0; \
}

DEFINE_MEMBERSHIP_TEST_IN_FUNCTION(int8, int8_t)
DEFINE_MEMBERSHIP_TEST_IN_FUNCTION(int16, int16_t)
DEFINE_MEMBERSHIP_TEST_IN_FUNCTION(int32, int32_t)
DEFINE_MEMBERSHIP_TEST_IN_FUNCTION(float, float)
DEFINE_MEMBERSHIP_TEST_IN_FUNCTION(double, double)

#define DEFINE_MEMBERSHIP_TEST_NOT_IN_FUNCTION(TYPE_NAME, TYPE) \
static uint8_t test_membership_not_in_##TYPE_NAME(void *untyped_args, const void *untyped_value) \
{ \
    return !test_membership_in_##TYPE_NAME(untyped_args, untyped_value); \
}

DEFINE_MEMBERSHIP_TEST_NOT_IN_FUNCTION(int8, int8_t)
DEFINE_MEMBERSHIP_TEST_NOT_IN_FUNCTION(int16, int16_t)
DEFINE_MEMBERSHIP_TEST_NOT_IN_FUNCTION(int32, int32_t)
DEFINE_MEMBERSHIP_TEST_NOT_IN_FUNCTION(float, float)
DEFINE_MEMBERSHIP_TEST_NOT_IN_FUNCTION(double, double)

static harp_predicate_eval_func *membership_test_func[NUM_MEMBERSHIP_OPERATORS][HARP_NUM_DATA_TYPES] =
    {{test_membership_in_int8, test_membership_in_int16, test_membership_in_int32, test_membership_in_float,
      test_membership_in_double, NULL},
     {test_membership_not_in_int8, test_membership_not_in_int16, test_membership_not_in_int32,
      test_membership_not_in_float, test_membership_not_in_double, NULL}};
/* *INDENT-ON* */

/* String set membership. */
typedef struct string_membership_test_args_struct
{
    int num_values;
    char **value;
} string_membership_test_args;

static uint8_t test_string_membership_in(void *untyped_args, const void *untyped_value)
{
    string_membership_test_args *args = (string_membership_test_args *)untyped_args;
    const char *value = *((const char **)untyped_value);

    char **first = args->value, **last = (args->value + args->num_values);

    for (; first != last; ++first)
    {
        if (strcmp(value, *first) == 0)
        {
            return 1;
        }
    }
    return 0;
}

static uint8_t test_string_membership_not_in(void *untyped_args, const void *untyped_value)
{
    return !test_string_membership_in(untyped_args, untyped_value);
}

/* *INDENT-OFF* */
static harp_predicate_eval_func *string_membership_test_func[NUM_MEMBERSHIP_OPERATORS] =
    {test_string_membership_in, test_string_membership_not_in};
/* *INDENT-ON* */

/* Valid range. */
typedef struct valid_range_test_args_struct
{
    harp_scalar valid_min;
    harp_scalar valid_max;
} valid_range_test_args;

/* *INDENT-OFF* */
#define DEFINE_VALID_RANGE_TEST_INTEGER_FUNCTION(TYPE_NAME, TYPE) \
static uint8_t test_valid_range_##TYPE_NAME(void *untyped_args, const void *untyped_value) \
{ \
    valid_range_test_args *args = (valid_range_test_args *)untyped_args; \
    TYPE value = *((TYPE *)untyped_value); \
    return (value >= args->valid_min.TYPE_NAME##_data && value <= args->valid_max.TYPE_NAME##_data); \
}

#define DEFINE_VALID_RANGE_TEST_REAL_FUNCTION(TYPE_NAME, TYPE) \
static uint8_t test_valid_range_##TYPE_NAME(void *untyped_args, const void *untyped_value) \
{ \
    valid_range_test_args *args = (valid_range_test_args *)untyped_args; \
    TYPE value = *((TYPE *)untyped_value); \
    return (!harp_isnan(value) && value >= args->valid_min.TYPE_NAME##_data && value \
            <= args->valid_max.TYPE_NAME##_data); \
}

DEFINE_VALID_RANGE_TEST_INTEGER_FUNCTION(int8,int8_t)
DEFINE_VALID_RANGE_TEST_INTEGER_FUNCTION(int16,int16_t)
DEFINE_VALID_RANGE_TEST_INTEGER_FUNCTION(int32,int32_t)
DEFINE_VALID_RANGE_TEST_REAL_FUNCTION(float,float)
DEFINE_VALID_RANGE_TEST_REAL_FUNCTION(double,double)

static harp_predicate_eval_func *valid_range_test_func[HARP_NUM_DATA_TYPES] =
    {test_valid_range_int8, test_valid_range_int16, test_valid_range_int32, test_valid_range_float,
     test_valid_range_double, NULL};
/* *INDENT-ON* */

/* Longitude range. */
typedef struct longitude_range_test_args_struct
{
    double min;
    double max;
} longitude_range_test_args;

/**
 * Normalize an angle to the interval [\a lower_bound, \a lower_bound + 360] degrees.
 *
 * \param  angle       Angle to normalize (in degrees).
 * \param  lower_bound Lower bound of the interval to normalize to (in degrees).
 * \return             Angle normalized to the interval [\a lower_bound, \a lower_bound + 360] degrees.
 */
static double normalize_angle(double angle, double lower_bound)
{
    return angle - 360.0 * floor((angle - lower_bound) / 360.0);
}

static uint8_t test_longitude_range(void *untyped_args, const void *untyped_value)
{
    longitude_range_test_args *args = (longitude_range_test_args *)untyped_args;

    return (normalize_angle(*((double *)untyped_value), args->min) <= args->max);
}

/* Index filter. */
typedef struct index_test_args_struct
{
    long num_indices;
    int32_t *index;
} index_test_args;

static uint8_t test_index(void *untyped_args, const void *untyped_value)
{
    long lower_index;
    long upper_index;

    index_test_args *args = (index_test_args *)untyped_args;
    int32_t key = *((int32_t *)untyped_value);

    /* Use binary search to check if the index passed in (via the untyped_value argument) exists in the (sorted) list
     * of indices.
     */
    lower_index = 0;
    upper_index = args->num_indices - 1;

    while (upper_index >= lower_index)
    {
        /* Determine the index that splits the search space into two (approximately) equal halves. */
        long pivot_index = lower_index + ((upper_index - lower_index) / 2);

        /* If the pivot equals the key, terminate early. */
        if (args->index[pivot_index] == key)
        {
            return 1;
        }

        /* If the pivot is smaller than the key, search the upper sub array, otherwise search the lower sub array. */
        if (args->index[pivot_index] < key)
        {
            lower_index = pivot_index + 1;
        }
        else
        {
            upper_index = pivot_index - 1;
        }
    }

    return 0;
}

static int get_data_type_index(harp_data_type data_type)
{
    switch (data_type)
    {
        case harp_type_int8:
            return 0;
        case harp_type_int16:
            return 1;
        case harp_type_int32:
            return 2;
        case harp_type_float:
            return 3;
        case harp_type_double:
            return 4;
        case harp_type_string:
            return 5;
        default:
            assert(0);
            exit(1);
    }
}

static int get_comparision_operator_index(harp_comparison_operator_type operator_type)
{
    switch (operator_type)
    {
        case harp_operator_eq:
            return 0;
        case harp_operator_ne:
            return 1;
        case harp_operator_lt:
            return 2;
        case harp_operator_le:
            return 3;
        case harp_operator_gt:
            return 4;
        case harp_operator_ge:
            return 5;
        default:
            assert(0);
            exit(1);
    }
}

static int get_membership_operator_index(harp_membership_operator_type operator_type)
{
    switch (operator_type)
    {
        case harp_operator_in:
            return 0;
        case harp_operator_not_in:
            return 1;
        default:
            assert(0);
            exit(1);
    }
}

static harp_predicate_eval_func *get_type_cast_func(harp_data_type data_type)
{
    return type_cast_func[get_data_type_index(data_type)];
}

static harp_predicate_eval_func *get_unit_conversion_func(harp_data_type data_type)
{
    return unit_conversion_func[get_data_type_index(data_type)];
}

static harp_predicate_eval_func *get_comparison_func(harp_comparison_operator_type operator_type,
                                                     harp_data_type data_type)
{
    return comparison_func[get_comparision_operator_index(operator_type)][get_data_type_index(data_type)];
}

static harp_predicate_eval_func *get_string_comparison_func(harp_comparison_operator_type operator_type)
{
    return string_comparison_func[get_comparision_operator_index(operator_type)];
}

static harp_predicate_eval_func *get_membership_test_func(harp_membership_operator_type operator_type,
                                                          harp_data_type data_type)
{
    return membership_test_func[get_membership_operator_index(operator_type)][get_data_type_index(data_type)];
}

static harp_predicate_eval_func *get_string_membership_test_func(harp_membership_operator_type operator_type)
{
    return string_membership_test_func[get_membership_operator_index(operator_type)];
}

static harp_predicate_eval_func *get_valid_range_test_func(harp_data_type data_type)
{
    return valid_range_test_func[get_data_type_index(data_type)];
}

static void type_cast_args_delete(type_cast_args *args)
{
    if (args != NULL)
    {
        harp_predicate_delete(args->predicate);
        free(args);
    }
}

static void type_cast_args_delete_func(void *untyped_args)
{
    type_cast_args_delete((type_cast_args *)untyped_args);
}

static int type_cast_args_new(harp_predicate *predicate, type_cast_args **new_args)
{
    type_cast_args *args;

    args = (type_cast_args *)malloc(sizeof(type_cast_args));
    if (args == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(type_cast_args), __FILE__, __LINE__);
        return -1;
    }

    args->predicate = predicate;

    *new_args = args;
    return 0;
}

static void unit_conversion_args_delete(unit_conversion_args *args)
{
    if (args != NULL)
    {
        harp_unit_converter_delete(args->unit_converter);
        harp_predicate_delete(args->predicate);
        free(args);
    }
}

static void unit_conversion_args_delete_func(void *untyped_args)
{
    unit_conversion_args_delete((unit_conversion_args *)untyped_args);
}

static int unit_conversion_args_new(const char *source_unit, const char *target_unit, harp_predicate *predicate,
                                    unit_conversion_args **new_args)
{
    unit_conversion_args *args;

    args = (unit_conversion_args *)malloc(sizeof(unit_conversion_args));
    if (args == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(unit_conversion_args), __FILE__, __LINE__);
        return -1;
    }

    args->unit_converter = NULL;
    args->predicate = NULL;

    if (harp_unit_converter_new(source_unit, target_unit, &args->unit_converter) != 0)
    {
        unit_conversion_args_delete(args);
        return -1;
    }

    args->predicate = predicate;
    *new_args = args;
    return 0;
}

static int comparison_args_new(double value, comparison_args **new_args)
{
    comparison_args *args;

    args = (comparison_args *)malloc(sizeof(comparison_args));
    if (args == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(comparison_args), __FILE__, __LINE__);
        return -1;
    }

    args->value = value;

    *new_args = args;
    return 0;
}

static void string_comparision_args_delete(string_comparision_args *args)
{
    if (args != NULL)
    {
        if (args->value != NULL)
        {
            free(args->value);
        }

        free(args);
    }
}

static void string_comparision_args_delete_func(void *untyped_args)
{
    string_comparision_args_delete((string_comparision_args *)untyped_args);
}

static int string_comparision_args_new(const char *value, string_comparision_args **new_args)
{
    string_comparision_args *args;

    assert(value != NULL);

    args = (string_comparision_args *)malloc(sizeof(string_comparision_args));
    if (args == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(string_comparision_args), __FILE__, __LINE__);
        return -1;
    }

    args->value = strdup(value);
    if (args->value == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        return -1;
    }

    *new_args = args;
    return 0;
}

static void membership_test_args_delete(membership_test_args *args)
{
    if (args != NULL)
    {
        if (args->value != NULL)
        {
            free(args->value);
        }

        free(args);
    }
}

static void membership_test_args_delete_func(void *untyped_args)
{
    membership_test_args_delete((membership_test_args *)untyped_args);
}

static int membership_test_args_new(int num_values, double *value, membership_test_args **new_args)
{
    membership_test_args *args;

    args = (membership_test_args *)malloc(sizeof(membership_test_args));
    if (args == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(membership_test_args), __FILE__, __LINE__);
        return -1;
    }

    args->num_values = num_values;
    args->value = NULL;

    if (value != NULL)
    {
        args->value = (double *)malloc(num_values * sizeof(double));
        if (args->value == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           num_values * sizeof(double), __FILE__, __LINE__);
            membership_test_args_delete(args);
            return -1;
        }

        memcpy(args->value, value, num_values * sizeof(double));
    }

    *new_args = args;
    return 0;
}

static void string_membership_test_args_delete(string_membership_test_args *args)
{
    if (args != NULL)
    {
        if (args->value != NULL)
        {
            int i;

            for (i = 0; i < args->num_values; i++)
            {
                if (args->value[i] != NULL)
                {
                    free(args->value[i]);
                }
            }

            free(args->value);
        }

        free(args);
    }
}

static void string_membership_test_args_delete_func(void *untyped_args)
{
    string_membership_test_args_delete((string_membership_test_args *)untyped_args);
}

static int string_membership_test_args_new(int num_values, const char **value, string_membership_test_args **new_args)
{
    string_membership_test_args *args;

    args = (string_membership_test_args *)malloc(sizeof(string_membership_test_args));
    if (args == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(string_membership_test_args), __FILE__, __LINE__);
        return -1;
    }

    args->num_values = num_values;
    args->value = NULL;

    if (value != NULL)
    {
        int i;

        args->value = (char **)malloc(num_values * sizeof(char *));
        if (args->value == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           num_values * sizeof(char *), __FILE__, __LINE__);
            string_membership_test_args_delete(args);
            return -1;
        }

        for (i = 0; i < num_values; i++)
        {
            if (value[i] == NULL)
            {
                args->value[i] = NULL;
            }
            else
            {
                args->value[i] = strdup(value[i]);
                if (args->value[i] == NULL)
                {
                    harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)",
                                   __FILE__, __LINE__);
                    string_membership_test_args_delete(args);
                    return -1;
                }
            }
        }
    }

    *new_args = args;
    return 0;
}

static int valid_range_test_args_new(harp_scalar valid_min, harp_scalar valid_max, valid_range_test_args **new_args)
{
    valid_range_test_args *args;

    args = (valid_range_test_args *)malloc(sizeof(valid_range_test_args));
    if (args == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(valid_range_test_args), __FILE__, __LINE__);
        return -1;
    }

    args->valid_min = valid_min;
    args->valid_max = valid_max;

    *new_args = args;
    return 0;
}

static int longitude_range_test_args_new(double min, double max, longitude_range_test_args **new_args)
{
    longitude_range_test_args *args;

    args = (longitude_range_test_args *)malloc(sizeof(longitude_range_test_args));
    if (args == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(longitude_range_test_args), __FILE__, __LINE__);
        return -1;
    }

    args->min = min;
    args->max = max;

    *new_args = args;
    return 0;
}

static void index_test_args_delete(index_test_args *args)
{
    if (args != NULL)
    {
        if (args->index != NULL)
        {
            free(args->index);
        }

        free(args);
    }
}

static void index_test_args_delete_func(void *untyped_args)
{
    index_test_args_delete((index_test_args *)untyped_args);
}

static int index_test_args_new(int num_indices, int32_t *index, index_test_args **new_args)
{
    index_test_args *args;

    args = (index_test_args *)malloc(sizeof(index_test_args));
    if (args == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(index_test_args), __FILE__, __LINE__);
        return -1;
    }

    assert(num_indices >= 0);
    args->num_indices = num_indices;
    args->index = NULL;

    if (num_indices > 0)
    {
        args->index = (int32_t *)malloc(num_indices * sizeof(int32_t));
        if (args->index == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           num_indices * sizeof(int32_t), __FILE__, __LINE__);
            index_test_args_delete(args);
            return -1;
        }

        assert(index != NULL);
        memcpy(args->index, index, num_indices * sizeof(int32_t));
    }

    *new_args = args;
    return 0;
}

static int type_cast_new(harp_data_type data_type, harp_predicate *wrapped_predicate, harp_predicate **new_predicate)
{
    harp_predicate *predicate;
    type_cast_args *args;

    if (type_cast_args_new(wrapped_predicate, &args) != 0)
    {
        return -1;
    }

    if (harp_predicate_new(get_type_cast_func(data_type), args, type_cast_args_delete_func, &predicate) != 0)
    {
        type_cast_args_delete(args);
        return -1;
    }

    *new_predicate = predicate;
    return 0;
}

static int unit_conversion_new(harp_data_type data_type, const char *source_unit, const char *target_unit,
                               harp_predicate *wrapped_predicate, harp_predicate **new_predicate)
{
    harp_predicate *predicate;
    unit_conversion_args *args;

    if (unit_conversion_args_new(source_unit, target_unit, wrapped_predicate, &args) != 0)
    {
        return -1;
    }

    if (harp_predicate_new(get_unit_conversion_func(data_type), args, unit_conversion_args_delete_func,
                           &predicate) != 0)
    {
        unit_conversion_args_delete(args);
        return -1;
    }

    *new_predicate = predicate;
    return 0;
}

int harp_comparison_filter_predicate_new(const harp_comparison_filter_args *args, harp_data_type data_type,
                                         const char *unit, harp_predicate **new_predicate)
{
    harp_predicate *predicate;
    harp_predicate_eval_func *eval_func;
    comparison_args *predicate_args;
    int unit_conversion_required;

    if (data_type == harp_type_string)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "predicate not defined for data type: %s",
                       harp_get_data_type_name(data_type));
        return -1;
    }

    if (args->unit != NULL && unit == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "conversion to unit '%s' impossible: source unit is unknown",
                       args->unit);
        return -1;
    }

    unit_conversion_required = (args->unit != NULL && strcmp(unit, args->unit) != 0);
    if (unit_conversion_required)
    {
        eval_func = get_comparison_func(args->operator_type, harp_type_double);
    }
    else
    {
        eval_func = get_comparison_func(args->operator_type, data_type);
    }

    if (comparison_args_new(args->value, &predicate_args) != 0)
    {
        return -1;
    }

    if (harp_predicate_new(eval_func, predicate_args, NULL, &predicate) != 0)
    {
        free(predicate_args);
        return -1;
    }

    if (unit_conversion_required)
    {
        /* Unit conversion required. */
        if (unit_conversion_new(data_type, unit, args->unit, predicate, &predicate) != 0)
        {
            harp_predicate_delete(predicate);
            return -1;
        }
    }

    *new_predicate = predicate;
    return 0;
}

int harp_string_comparison_filter_predicate_new(const harp_string_comparison_filter_args *args,
                                                harp_data_type data_type, harp_predicate **new_predicate)
{
    harp_predicate *predicate;
    string_comparision_args *predicate_args;

    if (data_type != harp_type_string || (args->operator_type != harp_operator_eq
                                          && args->operator_type != harp_operator_ne))
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "predicate not defined for data type: %s",
                       harp_get_data_type_name(data_type));
        return -1;
    }

    if (string_comparision_args_new(args->value, &predicate_args) != 0)
    {
        return -1;
    }

    if (harp_predicate_new(get_string_comparison_func(args->operator_type), predicate_args,
                           string_comparision_args_delete_func, &predicate) != 0)
    {
        string_comparision_args_delete(predicate_args);
        return -1;
    }

    *new_predicate = predicate;
    return 0;
}

int harp_membership_filter_predicate_new(const harp_membership_filter_args *args, harp_data_type data_type,
                                         const char *unit, harp_predicate **new_predicate)
{
    harp_predicate *predicate;
    harp_predicate_eval_func *eval_func;
    membership_test_args *predicate_args;
    int unit_conversion_required;

    if (data_type == harp_type_string)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "predicate not defined for data type: %s",
                       harp_get_data_type_name(data_type));
        return -1;
    }

    if (args->unit != NULL && unit == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "conversion to unit '%s' impossible: source unit is unknown",
                       args->unit);
        return -1;
    }

    unit_conversion_required = (args->unit != NULL && strcmp(unit, args->unit) != 0);
    if (unit_conversion_required)
    {
        /* Unit conversion required. */
        eval_func = get_membership_test_func(args->operator_type, harp_type_double);
    }
    else
    {
        eval_func = get_membership_test_func(args->operator_type, data_type);
    }

    if (membership_test_args_new(args->num_values, args->value, &predicate_args) != 0)
    {
        return -1;
    }

    if (harp_predicate_new(eval_func, predicate_args, membership_test_args_delete_func, &predicate) != 0)
    {
        membership_test_args_delete(predicate_args);
        return -1;
    }

    if (unit_conversion_required)
    {
        /* Unit conversion required. */
        if (unit_conversion_new(data_type, unit, args->unit, predicate, &predicate) != 0)
        {
            harp_predicate_delete(predicate);
            return -1;
        }
    }

    *new_predicate = predicate;
    return 0;
}

int harp_string_membership_filter_predicate_new(const harp_string_membership_filter_args *args,
                                                harp_data_type data_type, harp_predicate **new_predicate)
{
    harp_predicate *predicate;
    string_membership_test_args *predicate_args;

    if (data_type != harp_type_string)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "predicate not defined for data type: %s",
                       harp_get_data_type_name(data_type));
        return -1;
    }

    if (string_membership_test_args_new(args->num_values, (const char **)args->value, &predicate_args) != 0)
    {
        return -1;
    }

    if (harp_predicate_new(get_string_membership_test_func(args->operator_type), predicate_args,
                           string_membership_test_args_delete_func, &predicate) != 0)
    {
        string_membership_test_args_delete(predicate_args);
        return -1;
    }

    *new_predicate = predicate;
    return 0;
}

int harp_valid_range_filter_predicate_new(harp_data_type data_type, harp_scalar valid_min, harp_scalar valid_max,
                                          harp_predicate **new_predicate)
{
    harp_predicate *predicate;
    valid_range_test_args *predicate_args;

    if (data_type == harp_type_string)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "predicate not defined for data type: %s",
                       harp_get_data_type_name(data_type));
        return -1;
    }

    if (valid_range_test_args_new(valid_min, valid_max, &predicate_args) != 0)
    {
        return -1;
    }

    if (harp_predicate_new(get_valid_range_test_func(data_type), predicate_args, NULL, &predicate) != 0)
    {
        free(predicate_args);
        return -1;
    }

    *new_predicate = predicate;
    return 0;
}

int harp_longitude_range_filter_predicate_new(const harp_longitude_range_filter_args *args, harp_data_type data_type,
                                              const char *unit, harp_predicate **new_predicate)
{
    harp_predicate *predicate;
    longitude_range_test_args *predicate_args;
    double min;
    double max;
    int unit_conversion_required;

    if (data_type == harp_type_string)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "predicate not defined for data type: %s",
                       harp_get_data_type_name(data_type));
        return -1;
    }

    if (unit == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "cannot convert longitude to unit 'degree_east'; unit of longitude"
                       " variable is undefined");
        return -1;
    }

    unit_conversion_required = (strcmp(unit, "degree_east") != 0);

    min = args->min;
    if (args->min_unit == NULL && unit_conversion_required)
    {
        if (harp_convert_unit(unit, "degree_east", 1, &min) != 0)
        {
            return -1;
        }
    }
    if (args->min_unit != NULL && strcmp(args->min_unit, "degree_east") != 0)
    {
        if (harp_convert_unit(args->min_unit, "degree_east", 1, &min) != 0)
        {
            return -1;
        }
    }

    max = args->max;
    if (args->max_unit == NULL && unit_conversion_required)
    {
        if (harp_convert_unit(unit, "degree_east", 1, &max) != 0)
        {
            return -1;
        }
    }
    if (args->max_unit != NULL && strcmp(args->max_unit, "degree_east") != 0)
    {
        if (harp_convert_unit(args->max_unit, "degree_east", 1, &max) != 0)
        {
            return -1;
        }
    }

    /* Normalize maximum longitude to the range [minimum longitude, minimum longitude + 360.0]. */
    max = normalize_angle(max, min);

    if (longitude_range_test_args_new(min, max, &predicate_args) != 0)
    {
        return -1;
    }

    if (harp_predicate_new(test_longitude_range, predicate_args, NULL, &predicate) != 0)
    {
        free(predicate_args);
        return -1;
    }

    if (unit_conversion_required)
    {
        /* Unit conversion required. */
        if (unit_conversion_new(data_type, unit, "degree_east", predicate, &predicate) != 0)
        {
            harp_predicate_delete(predicate);
            return -1;
        }
    }
    else if (data_type != harp_type_double)
    {
        /* Type conversion required. */
        if (type_cast_new(data_type, predicate, &predicate) != 0)
        {
            harp_predicate_delete(predicate);
            return -1;
        }
    }

    *new_predicate = predicate;
    return 0;
}

static int compare_index(const void *untyped_index_a, const void *untyped_index_b)
{
    int32_t index_a = *((int32_t *)untyped_index_a);
    int32_t index_b = *((int32_t *)untyped_index_b);

    if (index_a < index_b)
    {
        return -1;
    }

    if (index_a > index_b)
    {
        return 1;
    }

    return 0;
}

int harp_collocation_filter_predicate_new(const harp_collocation_result *collocation_result, const char *source_product,
                                          harp_collocation_filter_type filter_type, int use_collocation_index,
                                          harp_predicate **new_predicate)
{
    harp_predicate *predicate;
    index_test_args *predicate_args;

    if (collocation_result->num_pairs > 0)
    {
        int32_t *index;
        long num_indices;
        long num_unique_indices;
        long i;

        index = (int32_t *)malloc(collocation_result->num_pairs * sizeof(int32_t));
        if (index == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           collocation_result->num_pairs * sizeof(int32_t), __FILE__, __LINE__);
            return -1;
        }

        /* Extract indices to filter on from the collocation result based on the filter type (left or right), and the
         * index type (collocation_index or index).
         */
        num_indices = 0;
        for (i = 0; i < collocation_result->num_pairs; i++)
        {
            const harp_collocation_pair *pair;

            pair = collocation_result->pair[i];
            if (filter_type == harp_collocation_left && strcmp(pair->source_product_a, source_product) == 0)
            {
                index[num_indices] = (use_collocation_index ? pair->collocation_index : pair->index_a);
                num_indices++;
            }

            if (filter_type == harp_collocation_right && strcmp(pair->source_product_b, source_product) == 0)
            {
                index[num_indices] = (use_collocation_index ? pair->collocation_index : pair->index_b);
                num_indices++;
            }
        }

        /* Sort the list of indices. */
        qsort(index, num_indices, sizeof(int32_t), compare_index);

        /* Remove duplicate indices (should not occur when the index type equals collocation_index). */
        num_unique_indices = 0;
        for (i = 0; i < num_indices; i++)
        {
            if (i == 0 || index[i] != index[i - 1])
            {
                index[num_unique_indices] = index[i];
                num_unique_indices++;
            }
        }

        if (index_test_args_new(num_unique_indices, index, &predicate_args) != 0)
        {
            free(index);
            return -1;
        }

        free(index);
    }
    else
    {
        if (index_test_args_new(0, NULL, &predicate_args) != 0)
        {
            return -1;
        }
    }

    if (harp_predicate_new(test_index, predicate_args, index_test_args_delete_func, &predicate) != 0)
    {
        index_test_args_delete(predicate_args);
        return -1;
    }

    *new_predicate = predicate;
    return 0;
}

int harp_get_filter_predicate_for_action(const harp_action *action, harp_data_type data_type, const char *unit,
                                         harp_scalar valid_min, harp_scalar valid_max, harp_predicate **new_predicate)
{
    switch (action->type)
    {
        case harp_action_filter_comparison:
            return harp_comparison_filter_predicate_new((harp_comparison_filter_args *)action->args, data_type, unit,
                                                        new_predicate);
        case harp_action_filter_string_comparison:
            return harp_string_comparison_filter_predicate_new((harp_string_comparison_filter_args *)action->args,
                                                               data_type, new_predicate);
        case harp_action_filter_membership:
            return harp_membership_filter_predicate_new((harp_membership_filter_args *)action->args, data_type, unit,
                                                        new_predicate);
        case harp_action_filter_string_membership:
            return harp_string_membership_filter_predicate_new((harp_string_membership_filter_args *)action->args,
                                                               data_type, new_predicate);
        case harp_action_filter_valid_range:
            return harp_valid_range_filter_predicate_new(data_type, valid_min, valid_max, new_predicate);
        case harp_action_filter_longitude_range:
            return harp_longitude_range_filter_predicate_new((harp_longitude_range_filter_args *)action->args,
                                                             data_type, unit, new_predicate);
        default:
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "no predicate defined for action");
            return -1;
    }
}

static long update_mask_1d(const harp_predicate *predicate, long num_elements, long stride, const void *data,
                           uint8_t *mask)
{
    uint8_t *mask_end;
    long num_masked = 0;

    for (mask_end = mask + num_elements; mask != mask_end; ++mask)
    {
        if (*mask)
        {
            if (!predicate->eval(predicate->args, data))
            {
                *mask = 0;
            }
            else
            {
                ++num_masked;
            }
        }

        data = (void *)(((char *)data) + stride);
    }

    return num_masked;
}

static void update_mask_2d(const harp_predicate *predicate, long num_primary, long num_secondary, long stride,
                           const void *data, uint8_t *primary_mask, uint8_t *secondary_mask,
                           long *primary_masked_length, long *secondary_masked_length)
{
    uint8_t *primary_mask_end;
    long primary_num_masked = 0;
    long max_secondary_num_masked = 0;

    for (primary_mask_end = primary_mask + num_primary; primary_mask != primary_mask_end; ++primary_mask)
    {
        if (*primary_mask)
        {
            uint8_t *secondary_mask_end;
            long secondary_num_masked = 0;

            for (secondary_mask_end = secondary_mask + num_secondary; secondary_mask != secondary_mask_end;
                 ++secondary_mask)
            {
                if (*secondary_mask)
                {
                    if (!predicate->eval(predicate->args, data))
                    {
                        *secondary_mask = 0;
                    }
                    else
                    {
                        ++secondary_num_masked;
                    }
                }

                data = (void *)(((char *)data) + stride);
            }

            if (secondary_num_masked > max_secondary_num_masked)
            {
                max_secondary_num_masked = secondary_num_masked;
            }

            if (secondary_num_masked == 0)
            {
                *primary_mask = 0;
            }
            else
            {
                ++primary_num_masked;
            }
        }
        else
        {
            memset(secondary_mask, 0, num_secondary * sizeof(uint8_t));
            secondary_mask += num_secondary;
            data = (void *)(((char *)data) + num_secondary * stride);
        }
    }

    *primary_masked_length = primary_num_masked;
    *secondary_masked_length = max_secondary_num_masked;
}

static long update_mask_any(const harp_predicate *predicate, long num_primary, long num_secondary, long stride,
                            const void *data, uint8_t *mask)
{
    uint8_t *mask_end;
    long num_masked = 0;

    for (mask_end = mask + num_primary; mask != mask_end; ++mask)
    {
        if (*mask)
        {
            void *data_end = (void *)(((char *)data) + num_secondary * stride);

            while (data != data_end)
            {
                if (predicate->eval(predicate->args, data))
                {
                    break;
                }

                data = (void *)(((char *)data) + stride);
            }

            if (data == data_end)
            {
                *mask = 0;
            }
            else
            {
                ++num_masked;
                data = data_end;
            }
        }
        else
        {
            data = (void *)(((char *)data) + num_secondary * stride);
        }
    }

    return num_masked;
}

int harp_predicate_update_mask_all_0d(const harp_predicate *predicate, const harp_variable *variable,
                                      uint8_t *product_mask)
{
    if (predicate == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "predicate is NULL (%s:%lu)", __FILE__, __LINE__);
        return -1;

    }
    if (variable == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variable is NULL (%s:%lu)", __FILE__, __LINE__);
        return -1;

    }
    if (product_mask == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "product_mask is NULL (%s:%lu)", __FILE__, __LINE__);
        return -1;

    }
    if (variable->num_dimensions != 0)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variable '%s' has %d dimensions; expected 0", variable->name,
                       variable->num_dimensions);
        return -1;
    }
    if (!*product_mask)
    {
        /* Product mask is false. */
        return 0;
    }

    update_mask_1d(predicate, 1, harp_get_size_for_type(variable->data_type), variable->data.ptr, product_mask);

    return 0;
}

int harp_predicate_update_mask_all_1d(const harp_predicate *predicate, const harp_variable *variable,
                                      harp_dimension_mask *dimension_mask)
{
    if (predicate == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "predicate is NULL (%s:%lu)", __FILE__, __LINE__);
        return -1;

    }
    if (variable == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variable is NULL (%s:%lu)", __FILE__, __LINE__);
        return -1;

    }
    if (dimension_mask == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "dimension_mask is NULL (%s:%lu)", __FILE__, __LINE__);
        return -1;

    }
    if (variable->num_dimensions != 1)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variable '%s' has %d dimensions; expected 1", variable->name,
                       variable->num_dimensions);
        return -1;
    }
    if (variable->dimension_type[0] == harp_dimension_independent)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variable '%s' has independent outer dimension", variable->name);
        return -1;
    }
    if (dimension_mask->num_dimensions != 1)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "dimension mask has %d dimensions; expected 1",
                       dimension_mask->num_dimensions);
        return -1;
    }
    if (dimension_mask->num_elements != variable->num_elements)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "dimension mask has %ld elements, expected %ld",
                       dimension_mask->num_elements, variable->num_elements);
        return -1;
    }
    if (dimension_mask->masked_dimension_length == 0)
    {
        /* Dimension mask is false. */
        return 0;
    }
    assert(dimension_mask->mask != NULL);

    dimension_mask->masked_dimension_length = update_mask_1d(predicate, variable->num_elements,
                                                             harp_get_size_for_type(variable->data_type),
                                                             variable->data.ptr, dimension_mask->mask);

    return 0;
}

int harp_predicate_update_mask_all_2d(const harp_predicate *predicate, const harp_variable *variable,
                                      harp_dimension_mask *primary_dimension_mask,
                                      harp_dimension_mask *secondary_dimension_mask)
{
    if (predicate == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "predicate is NULL (%s:%lu)", __FILE__, __LINE__);
        return -1;

    }
    if (variable == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variable is NULL (%s:%lu)", __FILE__, __LINE__);
        return -1;

    }
    if (primary_dimension_mask == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "primary_dimension_mask is NULL (%s:%lu)", __FILE__, __LINE__);
        return -1;

    }
    if (secondary_dimension_mask == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "secondary_dimension_mask is NULL (%s:%lu)", __FILE__, __LINE__);
        return -1;

    }
    if (variable->num_dimensions != 2)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variable '%s' has %d dimensions; expected 2", variable->name,
                       variable->num_dimensions);
        return -1;
    }
    if (variable->dimension_type[0] != harp_dimension_time)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "outer dimension of variable '%s' is of type '%s'; expected '%s'",
                       variable->name, harp_get_dimension_type_name(variable->dimension_type[0]),
                       harp_get_dimension_type_name(harp_dimension_time));
        return -1;
    }
    if (variable->dimension_type[1] == harp_dimension_independent)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variable '%s' has independent inner dimension", variable->name);
        return -1;
    }
    if (primary_dimension_mask->num_dimensions != 1)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "dimension mask has %d dimensions; expected 1",
                       primary_dimension_mask->num_dimensions);
        return -1;
    }
    if (primary_dimension_mask->num_elements != variable->dimension[0])
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "dimension mask has %ld elements, expected %ld",
                       primary_dimension_mask->num_elements, variable->dimension[0]);
        return -1;
    }
    if (primary_dimension_mask->masked_dimension_length == 0)
    {
        /* Primary dimension mask is false. */
        return 0;
    }
    if (secondary_dimension_mask->num_dimensions != 2)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "dimension mask has %d dimensions; expected 2",
                       secondary_dimension_mask->num_dimensions);
        return -1;
    }
    if (secondary_dimension_mask->num_elements != variable->num_elements)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "dimension mask has %ld elements, expected %ld",
                       secondary_dimension_mask->num_elements, variable->num_elements);
        return -1;
    }
    if (secondary_dimension_mask->masked_dimension_length == 0)
    {
        /* Secondary dimension mask is false. */
        return 0;
    }
    assert(primary_dimension_mask->mask != NULL);
    assert(secondary_dimension_mask->mask != NULL);

    update_mask_2d(predicate, variable->dimension[0], variable->dimension[1],
                   harp_get_size_for_type(variable->data_type), variable->data.ptr,
                   primary_dimension_mask->mask, secondary_dimension_mask->mask,
                   &primary_dimension_mask->masked_dimension_length,
                   &secondary_dimension_mask->masked_dimension_length);

    return 0;
}

int harp_predicate_update_mask_any(const harp_predicate *predicate, const harp_variable *variable,
                                   harp_dimension_mask *dimension_mask)
{
    if (predicate == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "predicate is NULL (%s:%lu)", __FILE__, __LINE__);
        return -1;

    }
    if (variable == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variable is NULL (%s:%lu)", __FILE__, __LINE__);
        return -1;

    }
    if (dimension_mask == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "dimension_mask is NULL (%s:%lu)", __FILE__, __LINE__);
        return -1;

    }
    if (variable->num_dimensions < 1)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variable '%s' has %d dimensions; expected 1 or more",
                       variable->name, variable->num_dimensions);
        return -1;
    }
    if (variable->dimension_type[0] != harp_dimension_time)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "outer dimension of variable '%s' is of type '%s'; expected '%s'",
                       variable->name, harp_get_dimension_type_name(variable->dimension_type[0]),
                       harp_get_dimension_type_name(harp_dimension_time));
        return -1;
    }
    if (dimension_mask->num_dimensions != 1)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "dimension mask has %d dimensions; expected 1",
                       dimension_mask->num_dimensions);
        return -1;
    }
    if (dimension_mask->num_elements != variable->dimension[0])
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "dimension mask has %ld elements, expected %ld",
                       dimension_mask->num_elements, variable->dimension[0]);
        return -1;
    }
    if (dimension_mask->masked_dimension_length == 0)
    {
        /* Dimension mask is false. */
        return 0;
    }
    assert(dimension_mask->mask != NULL);

    dimension_mask->masked_dimension_length = update_mask_any(predicate, dimension_mask->num_elements,
                                                              variable->num_elements / dimension_mask->num_elements,
                                                              harp_get_size_for_type(variable->data_type),
                                                              variable->data.ptr, dimension_mask->mask);

    return 0;
}
