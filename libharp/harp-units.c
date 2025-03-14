/*
 * Copyright (C) 2015-2025 S[&]T, The Netherlands.
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
#include <errno.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>

#include "udunits2.h"

static char *harp_udunits2_xml_path = NULL;

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
            harp_set_error(HARP_ERROR_UNIT_CONVERSION, "cannot open unit database");
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

/** Set the location of the udunits2 unit conversion xml configuration file.
 * \ingroup harp_general
 * This function should be called before harp_init() is called.
 *
 * The HARP C library uses the udunits2 library to perform unit conversions. The xml configuration files for udunits2
 * are included with a HARP installation and a default absolute path to these xml files is built into the library.
 *
 * If the HARP installation ends up in a different location on disk compared to what was provided at build time then
 * you will either need to set the UDUNITS2_XML_PATH environment variable or call one of the functions
 * harp_set_udunits2_xml_path() or harp_set_udunits2_xml_path_conditional() to set the path programmatically.
 *
 * The path should be an absolute path to the udunits2.xml file that was included with the HARP installation.
 *
 * Specifying a path using this function will prevent HARP from using the UDUNITS2_XML_PATH environment variable.
 * If you still want HARP to acknowledge the UDUNITS2_XML_PATH environment variable then use something like this in
 * your code:
 * \code{.c}
 * if (getenv("UDUNITS2_XML_PATH") == NULL)
 * {
 *     harp_set_udunits2_xml_path("<your path>");
 * }
 * \endcode
 *
 *  \param path Absolute path to the udunits2.xml file
 *  \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_set_udunits2_xml_path(const char *path)
{
    if (harp_udunits2_xml_path != NULL)
    {
        free(harp_udunits2_xml_path);
        harp_udunits2_xml_path = NULL;
    }
    if (path == NULL)
    {
        return 0;
    }
    harp_udunits2_xml_path = strdup(path);
    if (harp_udunits2_xml_path == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        return -1;
    }

    return 0;
}

/** Set the location of the udunits2 xml configuration file based on the location of another file.
 * \ingroup harp_general
 * This function should be called before harp_init() is called.
 *
 * The HARP C library uses the udunits2 library to perform unit conversions. The xml configuration files for udunits2
 * are included with a HARP installation and a default absolute path to the main xml file is built into the library.
 *
 * If the HARP installation ends up in a different location on disk compared to what was provided at build time then
 * you will either need to set the UDUNITS2_XML_PATH environment variable or call one of the functions
 * harp_set_udunits2_xml_path() or harp_set_udunits2_xml_path_conditional() to set the path programmatically.
 *
 * This function will try to find the file with filename \a file in the provided searchpath \a searchpath.
 * The first directory in the searchpath where the file \a file exists will be appended with the relative location
 * \a relative_location to determine the udunits2 xml path.
 * If the file to search for could not be found in the searchpath then the HARP udunits2 xml path will not be set.
 *
 * If the UDUNITS2_XML_PATH environment variable was set then this function will not perform a search or set the
 * udunits2 xml path (i.e. the udunits2 xml path will be taken from the UDUNITS2_XML_PATH variable).
 *
 * If you provide NULL for \a searchpath then the PATH environment variable will be used as searchpath.
 * For instance, you can use harp_set_udunits2_xml_path_conditional(argv[0], NULL, "../somedir/udunits2.xml") to set
 * the udunits2 xml path to a location relative to the location of your executable.
 *
 * The searchpath, if provided, should have a similar format as the PATH environment variable of your system. Path
 * components should be separated by ';' on Windows and by ':' on other systems.
 *
 * The \a relative_location parameter should point to the udunits2.xml file itself (and not the directory that the file
 * is in).
 *
 * Note that this function differs from harp_set_udunits2_xml_path() in that it will not modify the udunits2 xml path
 * if the UDUNITS2_XML_PATH variable was set.
 *
 * \param file Filename of the file to search for
 * \param searchpath Search path where to look for the file \a file (can be NULL)
 * \param relative_location Filepath relative to the directory from \a searchpath where \a file was found that provides
 * the location of the udunits2.xml file.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_set_udunits2_xml_path_conditional(const char *file, const char *searchpath,
                                                       const char *relative_location)
{
    char *location;

    if (getenv("UDUNITS2_XML_PATH") != NULL)
    {
        return 0;
    }

    if (searchpath == NULL)
    {
        if (harp_path_for_program(file, &location) != 0)
        {
            return -1;
        }
    }
    else
    {
        if (harp_path_find_file(searchpath, file, &location) != 0)
        {
            return -1;
        }
    }
    if (location != NULL)
    {
        char *path;

        if (harp_path_from_path(location, 1, relative_location, &path) != 0)
        {
            free(location);
            return -1;
        }
        free(location);
        if (harp_set_udunits2_xml_path(path) != 0)
        {
            free(path);
            return -1;
        }
        free(path);
    }

    return 0;
}

static int unit_system_init(void)
{
    if (unit_system == NULL)
    {
        char *oldlocale = NULL;

        ut_set_error_message_handler(ut_ignore);

        /* udunits2 uses strtod() for the xml parsing which is locale aware.
         * We need to make sure that it uses a locale that has the correct decimal separator
         */
        oldlocale = strdup(setlocale(LC_NUMERIC, NULL));
        if (oldlocale == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                           __LINE__);
            return -1;
        }
        setlocale(LC_NUMERIC, "C");

        unit_system = ut_read_xml(harp_udunits2_xml_path);
        if (unit_system == NULL)
        {
            handle_udunits_error();
            harp_add_error_message(" (%s)", harp_udunits2_xml_path);
            free(oldlocale);
            return -1;
        }

        setlocale(LC_NUMERIC, oldlocale);
        free(oldlocale);
    }

    return 0;
}

static void unit_system_done(void)
{
    if (unit_system != NULL)
    {
        ut_free_system(unit_system);
        unit_system = NULL;
        if (harp_udunits2_xml_path != NULL)
        {
            free(harp_udunits2_xml_path);
            harp_udunits2_xml_path = NULL;
        }
    }
}

static int parse_unit(const char *str, ut_unit **new_unit)
{
    ut_unit *unit;

    if (str == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "unit is NULL (%s:%lu)", __FILE__, __LINE__);
        return -1;
    }

    if (unit_system_init() != 0)
    {
        return -1;
    }

    unit = ut_parse(unit_system, str, UT_ASCII);
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
        harp_set_error(HARP_ERROR_UNIT_CONVERSION, "unit '%s' cannot be converted to unit '%s'", from_unit, to_unit);
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

float harp_unit_converter_convert_float(const harp_unit_converter *unit_converter, float value)
{
    return cv_convert_float(unit_converter->converter, value);
}

double harp_unit_converter_convert_double(const harp_unit_converter *unit_converter, double value)
{
    return cv_convert_double(unit_converter->converter, value);
}

void harp_unit_converter_convert_array_float(const harp_unit_converter *unit_converter, long num_values, float *value)
{
    float *value_end;

    for (value_end = value + num_values; value != value_end; value++)
    {
        *value = cv_convert_float(unit_converter->converter, *value);
    }
}

void harp_unit_converter_convert_array_double(const harp_unit_converter *unit_converter, long num_values, double *value)
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

/* this function is deprecated */
LIBHARP_API int harp_convert_unit(const char *from_unit, const char *to_unit, long num_values, double *value)
{
    return harp_convert_unit_double(from_unit, to_unit, num_values, value);
}

/** Perform unit conversion on single precision floating point data
 * \ingroup harp_general
 * Apply unit conversion on a range of single precision floating point values. Conversion will be performed in-place.
 * If there is no conversion available from the current unit to the new unit then an error will be raised.
 * \param from_unit Existing unit of the data that should be converted (use udunits2 compliant units).
 * \param to_unit Unit to which the data should be converted (use udunits2 compliant units).
 * \param num_values Number of floating point values in \a value.
 * \param value Array of single precision floating point values that should be converted.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_convert_unit_float(const char *from_unit, const char *to_unit, long num_values, float *value)
{
    harp_unit_converter *unit_converter;

    if (harp_unit_converter_new(from_unit, to_unit, &unit_converter) != 0)
    {
        return -1;
    }

    /* Perform unit conversion. */
    harp_unit_converter_convert_array_float(unit_converter, num_values, value);

    harp_unit_converter_delete(unit_converter);
    return 0;
}

/** Perform unit conversion on double precision floating point data
 * \ingroup harp_general
 * Apply unit conversion on a range of double precision floating point values. Conversion will be performed in-place.
 * If there is no conversion available from the current unit to the new unit then an error will be raised.
 * \param from_unit Existing unit of the data that should be converted (use udunits2 compliant units).
 * \param to_unit Unit to which the data should be converted (use udunits2 compliant units).
 * \param num_values Number of floating point values in \a value.
 * \param value Array of double precision floating point values that should be converted.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_convert_unit_double(const char *from_unit, const char *to_unit, long num_values, double *value)
{
    harp_unit_converter *unit_converter;

    if (harp_unit_converter_new(from_unit, to_unit, &unit_converter) != 0)
    {
        return -1;
    }

    /* Perform unit conversion. */
    harp_unit_converter_convert_array_double(unit_converter, num_values, value);

    harp_unit_converter_delete(unit_converter);
    return 0;
}


/** Perform unit conversion on a variable
 * \ingroup harp_variable
 * Apply an automatic conversion on the variable to arrive at the new given unit.
 * If there is no conversion available from the current unit to the new unit then an error will be raised.
 * The data type of the variable will be changed to 'double' as part of the conversion if it is not already using a
 * floating point data type.
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

    if (variable->data_type == harp_type_float)
    {
        /* Scale the data */
        harp_unit_converter_convert_array_float(unit_converter, variable->num_elements, variable->data.float_data);

        /* Update valid_min */
        variable->valid_min.float_data = harp_unit_converter_convert_float(unit_converter,
                                                                           variable->valid_min.float_data);

        /* Update valid_max */
        variable->valid_max.float_data = harp_unit_converter_convert_float(unit_converter,
                                                                           variable->valid_max.float_data);
    }
    else
    {
        /* Convert to double */
        if (harp_variable_convert_data_type(variable, harp_type_double) != 0)
        {
            harp_unit_converter_delete(unit_converter);
            return -1;
        }

        /* Scale the data */
        harp_unit_converter_convert_array_double(unit_converter, variable->num_elements, variable->data.double_data);

        /* Update valid_min */
        variable->valid_min.double_data = harp_unit_converter_convert_double(unit_converter,
                                                                             variable->valid_min.double_data);

        /* Update valid_max */
        variable->valid_max.double_data = harp_unit_converter_convert_double(unit_converter,
                                                                             variable->valid_max.double_data);
    }

    free(variable->unit);

    variable->unit = strdup(target_unit);
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

void harp_unit_done(void)
{
    unit_system_done();
}
