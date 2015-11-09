/*
 * Copyright (C) 2015 S[&]T, The Netherlands.
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

#include "harp-internal.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "udunits2.h"

static ut_system *unit_system = NULL;

struct harp_unit_converter_struct
{
    cv_converter *converter;
};

static void handle_udunits_error(void)
{
    switch (ut_get_status())
    {
        case UT_SUCCESS:
            break;
        case UT_BAD_ARG:
            harp_set_error(HARP_ERROR_UNIT_CONVERSION, "invalid argument");
            break;
        case UT_EXISTS:
            harp_set_error(HARP_ERROR_UNIT_CONVERSION, "unit, prefix, or identifier already exists");
            break;
        case UT_NO_UNIT:
            harp_set_error(HARP_ERROR_UNIT_CONVERSION, "unit does not exist");
            break;
        case UT_OS:
            harp_set_error(HARP_ERROR_UNIT_CONVERSION, strerror(errno));
            break;
        case UT_NOT_SAME_SYSTEM:
            harp_set_error(HARP_ERROR_UNIT_CONVERSION, "units belong to different unit-systems");
            break;
        case UT_MEANINGLESS:
            harp_set_error(HARP_ERROR_UNIT_CONVERSION, "operation on the unit(s) is meaningless");
            break;
        case UT_NO_SECOND:
            harp_set_error(HARP_ERROR_UNIT_CONVERSION, "unit-system doesn't have a unit named 'second'");
            break;
        case UT_VISIT_ERROR:
            harp_set_error(HARP_ERROR_UNIT_CONVERSION, "error occurred while visiting a unit");
            break;
        case UT_CANT_FORMAT:
            harp_set_error(HARP_ERROR_UNIT_CONVERSION, "unit can't be formatted in the desired manner");
            break;
        case UT_SYNTAX:
            harp_set_error(HARP_ERROR_UNIT_CONVERSION, "string unit representation contains syntax error");
            break;
        case UT_UNKNOWN:
            harp_set_error(HARP_ERROR_UNIT_CONVERSION, "string unit representation contains unknown word");
            break;
        case UT_OPEN_ARG:
            harp_set_error(HARP_ERROR_UNIT_CONVERSION, "cannot open argument-specified unit databast");
            break;
        case UT_OPEN_ENV:
            harp_set_error(HARP_ERROR_UNIT_CONVERSION, "cannot open environment-specified unit database");
            break;
        case UT_OPEN_DEFAULT:
            harp_set_error(HARP_ERROR_UNIT_CONVERSION, "cannot open installed, default, unit database");
            break;
        case UT_PARSE:
            harp_set_error(HARP_ERROR_UNIT_CONVERSION, "error parsing unit database");
            break;
    }
}

static int unit_system_init(void)
{
    if (unit_system == NULL)
    {
        ut_set_error_message_handler(ut_ignore);

        unit_system = ut_read_xml(NULL);
        if (unit_system == NULL)
        {
            handle_udunits_error();
            return -1;
        }
    }

    return 0;
}

static void unit_system_done(void)
{
    if (unit_system != NULL)
    {
        ut_free_system(unit_system);
        unit_system = NULL;
    }
}

static int parse_unit(const char *str, ut_unit **new_unit)
{
    ut_unit *unit;

    if (unit_system_init() != 0)
    {
        return -1;
    }

    unit = ut_parse(unit_system, (str == NULL ? "" : str), UT_ASCII);
    if (unit == NULL)
    {
        handle_udunits_error();
        return -1;
    }

    *new_unit = unit;
    return 0;
}

int harp_unit_is_valid(const char *str)
{
    ut_unit *unit;

    if (str == NULL)
    {
        return 0;
    }

    if (parse_unit(str, &unit) != 0)
    {
        return 0;
    }

    ut_free(unit);

    return 1;
}

void harp_unit_converter_delete(harp_unit_converter *unit_converter)
{
    if (unit_converter != NULL)
    {
        if (unit_converter->converter != NULL)
        {
            cv_free(unit_converter->converter);
        }

        free(unit_converter);
    }
}

int harp_unit_converter_new(const char *from_unit, const char *to_unit, harp_unit_converter **new_unit_converter)
{
    harp_unit_converter *unit_converter;
    ut_unit *from_udunit;
    ut_unit *to_udunit;

    if (parse_unit(from_unit, &from_udunit) != 0)
    {
        return -1;
    }

    if (parse_unit(to_unit, &to_udunit) != 0)
    {
        ut_free(from_udunit);
        return -1;
    }

    if (!ut_are_convertible(from_udunit, to_udunit))
    {
        harp_set_error(HARP_ERROR_UNIT_CONVERSION, "unit '%s' cannot be converted to unit '%s'",
                       (from_unit == NULL ? "\"\"" : from_unit), (to_unit == NULL ? "\"\"" : to_unit));
        ut_free(to_udunit);
        ut_free(from_udunit);
        return -1;
    }

    unit_converter = (harp_unit_converter *)malloc(sizeof(harp_unit_converter));
    if (unit_converter == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_unit_converter), __FILE__, __LINE__);
        ut_free(to_udunit);
        ut_free(from_udunit);
        return -1;
    }

    unit_converter->converter = ut_get_converter(from_udunit, to_udunit);
    if (unit_converter->converter == NULL)
    {
        handle_udunits_error();
        harp_unit_converter_delete(unit_converter);
        ut_free(to_udunit);
        ut_free(from_udunit);
        return -1;
    }

    ut_free(to_udunit);
    ut_free(from_udunit);

    *new_unit_converter = unit_converter;
    return 0;
}

double harp_unit_converter_convert(const harp_unit_converter *unit_converter, double value)
{
    return cv_convert_double(unit_converter->converter, value);
}

void harp_unit_converter_convert_array(const harp_unit_converter *unit_converter, long num_values, double *value)
{
    double *value_end;

    for (value_end = value + num_values; value != value_end; value++)
    {
        *value = cv_convert_double(unit_converter->converter, *value);
    }
}

/**
 * Compare the two specified units. Units can compare equal even if their string representations are not, e.g. consider
 * "W" (Watt) and "J/s" (Joule per second).
 * \return
 *   \arg \c <0, \a unit_a is considered less than \a unit_b.
 *   \arg \c 0, \a unit_a and \a unit_b are considered equal.
 *   \arg \c >0, \a unit_a is considered greater than \a unit_b.
 */
int harp_unit_compare(const char *unit_a, const char *unit_b)
{
    ut_unit *udunit_a;
    ut_unit *udunit_b;
    int result;

    if (parse_unit(unit_a, &udunit_a) != 0)
    {
        return -1;
    }

    if (parse_unit(unit_b, &udunit_b) != 0)
    {
        ut_free(udunit_a);
        return -1;
    }

    result = ut_compare(udunit_a, udunit_b);

    ut_free(udunit_b);
    ut_free(udunit_a);
    return result;
}

/** Perform unit conversion on data
 * \ingroup harp_general
 * Apply unit conversion on a range of floating point values. Conversion will be performed in-place.
 * If there is no conversion available from the current unit to the new unit then an error will be raised.
 * \param from_unit Existing unit of the data that should be converted (use udunits2 compliant units).
 * \param to_unit Unit to which the data should be converted (use udunits2 compliant units).
 * \param num_values Number of floating point values in \a value.
 * \param value Array of floating point values that should be converted.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_convert_unit(const char *from_unit, const char *to_unit, long num_values, double *value)
{
    harp_unit_converter *unit_converter;

    if (harp_unit_converter_new(from_unit, to_unit, &unit_converter) != 0)
    {
        return -1;
    }

    /* Perform unit conversion. */
    harp_unit_converter_convert_array(unit_converter, num_values, value);

    harp_unit_converter_delete(unit_converter);
    return 0;
}

/** Perform unit conversion on a variable
 * \ingroup harp_variable
 * Apply an automatic conversion on the variable to arrive at the new given unit.
 * If there is no conversion available from the current unit to the new unit then an error will be raised.
 * The data type of the variable will be changed to 'double' as part of the conversion.
 * \param variable Variable on which the apply unit conversion.
 * \param target_unit Unit to which the variable should be converted (use udunits2 compliant units).
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_variable_convert_unit(harp_variable *variable, const char *target_unit)
{
    harp_unit_converter *unit_converter;

    if (harp_unit_converter_new(variable->unit, target_unit, &unit_converter) != 0)
    {
        harp_add_error_message(" (in unit conversion of variable '%s')", variable->name);
        return -1;
    }

    /* Convert to double */
    if (harp_variable_convert_data_type(variable, harp_type_double) != 0)
    {
        harp_unit_converter_delete(unit_converter);
        return -1;
    }

    /* Scale the data */
    harp_unit_converter_convert_array(unit_converter, variable->num_elements, variable->data.double_data);

    /* Update valid_min */
    variable->valid_min.double_data = harp_unit_converter_convert(unit_converter, variable->valid_min.double_data);

    /* Update valid_max */
    variable->valid_max.double_data = harp_unit_converter_convert(unit_converter, variable->valid_max.double_data);

    if (variable->unit != NULL)
    {
        free(variable->unit);
        variable->unit = NULL;
    }

    variable->unit = strdup((target_unit == NULL ? "" : target_unit));
    if (variable->unit == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        harp_unit_converter_delete(unit_converter);
        return -1;
    }

    harp_unit_converter_delete(unit_converter);
    return 0;
}

void harp_unit_done()
{
    unit_system_done();
}
