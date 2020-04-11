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
#include "ipow.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

int harp_is_identifier(const char *name)
{
    int i;

    if (name == NULL)
    {
        return 0;
    }
    if (!isalpha(*name))
    {
        return 0;
    }
    i = 1;
    while (name[i] != '\0')
    {
        if (!(isalnum(name[i]) || name[i] == '_'))
        {
            return 0;
        }
        i++;
    }
    return 1;
}

static void clean_path(char *path)
{
    int from;
    int to;

    if (path == NULL || path[0] == '\0')
    {
        return;
    }

    from = 0;
    to = 0;
    while (path[from] == '.' && path[from + 1] == '/')
    {
        from += 2;
    }
    while (path[from] != '\0')
    {
        if (path[from] == '/' || path[from] == '\\')
        {
            if (path[from + 1] == '/' || path[from + 1] == '\\')
            {
                from++;
                continue;
            }
            if (path[from + 1] == '.')
            {
                if (path[from + 2] == '\0' || path[from + 2] == '/' || path[from + 2] == '\\')
                {
                    from += 2;
                    continue;
                }
                if (path[from + 2] == '.' &&
                    (path[from + 3] == '\0' || path[from + 3] == '/' || path[from + 3] == '\\'))
                {
                    if (!(to >= 2 && path[to - 1] == '.' && path[to - 2] == '.' &&
                          (to == 2 || path[to - 3] == '/' || path[to - 3] == '\\')))
                    {
                        int prev = to - 1;

                        /* find previous / or \ */
                        while (prev >= 0 && path[prev] != '/' && path[prev] != '\\')
                        {
                            prev--;
                        }
                        if (prev >= 0)
                        {
                            to = prev;
                            from += 3;
                            continue;
                        }
                    }
                }
            }
        }
        path[to] = path[from];
        from++;
        to++;
    }

    /* an empty path is a relative path to the current directory -> use '.' */
    if (to == 0)
    {
        path[to] = '.';
        to++;
    }

    path[to] = '\0';
}

int harp_path_find_file(const char *searchpath, const char *filename, char **location)
{
#ifdef WIN32
    const char path_separator_char = ';';
#else
    const char path_separator_char = ':';
#endif
    char *path;
    char *path_component;
    char *filepath = NULL;
    int filepath_length = 0;
    int filename_length = (int)strlen(filename);

    if (searchpath == NULL || searchpath[0] == '\0')
    {
        *location = NULL;
        return 0;
    }

    path = strdup(searchpath);
    if (path == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        return -1;
    }
    path_component = path;
    while (*path_component != '\0')
    {
        struct stat sb;
        char *p;
        int path_component_length;

        p = path_component;
        while (*p != '\0' && *p != path_separator_char)
        {
            p++;
        }
        if (*p != '\0')
        {
            *p = '\0';
            p++;
        }

        path_component_length = (int)strlen(path_component);
        if (filepath_length < path_component_length + filename_length + 1)
        {
            char *new_filepath;

            new_filepath = realloc(filepath, path_component_length + filename_length + 2);
            if (new_filepath == NULL)
            {
                harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                               __LINE__);
                if (filepath != NULL)
                {
                    free(filepath);
                }
                return -1;
            }
            filepath = new_filepath;
            filepath_length = path_component_length + filename_length + 1;
        }
        sprintf(filepath, "%s/%s", path_component, filename);

        if (stat(filepath, &sb) == 0)
        {
            if (sb.st_mode & S_IFREG)
            {
                /* we found the file */
                *location = filepath;
                free(path);
                return 0;
            }
        }

        path_component = p;
    }

    if (filepath != NULL)
    {
        free(filepath);
    }
    free(path);

    /* the file was not found */
    *location = NULL;
    return 0;
}

int harp_path_from_path(const char *initialpath, int is_filepath, const char *appendpath, char **resultpath)
{
    char *path;
    int initialpath_length;
    int appendpath_length;

    initialpath_length = (int)strlen(initialpath);
    appendpath_length = (appendpath == NULL ? 0 : (int)strlen(appendpath));

    if (is_filepath && initialpath_length > 0)
    {
        /* remove trailing parth */
        while (initialpath_length > 0 && initialpath[initialpath_length - 1] != '/' &&
               initialpath[initialpath_length - 1] != '\\')
        {
            initialpath_length--;
        }
    }

    *resultpath = malloc(initialpath_length + 1 + appendpath_length + 1);
    if (*resultpath == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        return -1;
    }
    path = *resultpath;
    if (initialpath_length > 0)
    {
        memcpy(path, initialpath, initialpath_length);
        path += initialpath_length;
        if (appendpath_length > 0)
        {
            *path = '/';
            path++;
        }
    }
    if (appendpath_length > 0)
    {
        memcpy(path, appendpath, appendpath_length);
        path += appendpath_length;
    }
    *path = '\0';

    clean_path(*resultpath);

    return 0;
}

int harp_path_for_program(const char *argv0, char **location)
{
    const char *p;
    int is_path = 0;

    /* default (i.e. not found) is NULL */
    *location = NULL;

    if (argv0 == NULL)
    {
        return 0;
    }

    p = argv0;
    while (*p != '\0')
    {
        if (*p == '/' || *p == '\\')
        {
            is_path = 1;
            break;
        }
        p++;
    }

    if (is_path)
    {
        *location = strdup(argv0);
        if (*location == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                           __LINE__);
            return -1;
        }
    }
    else
    {
        /* use PATH */
#ifdef WIN32
        int argv0_length = (int)strlen(argv0);

        if (harp_path_find_file(".", argv0, location) != 0)
        {
            return -1;
        }
        if (*location == NULL && getenv("PATH") != NULL)
        {
            if (harp_path_find_file(getenv("PATH"), argv0, location) != 0)
            {
                return -1;
            }
        }
        if (*location == NULL && (argv0_length <= 4 || strcmp(&argv0[argv0_length - 4], ".exe") != 0))
        {
            char *filepath;

            filepath = malloc(argv0_length + 5);
            if (filepath == NULL)
            {
                harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                               __LINE__);
                return -1;
            }
            strcpy(filepath, argv0);
            strcpy(&filepath[argv0_length], ".exe");
            if (harp_path_find_file(".", filepath, location) != 0)
            {
                free(filepath);
                return -1;
            }
            if (*location == NULL && getenv("PATH") != NULL)
            {
                if (harp_path_find_file(getenv("PATH"), filepath, location) != 0)
                {
                    free(filepath);
                    return -1;
                }
            }
            free(filepath);
        }
#else
        if (getenv("PATH") != NULL)
        {
            if (harp_path_find_file(getenv("PATH"), argv0, location) != 0)
            {
                return -1;
            }
        }
        else
        {
            *location = NULL;
        }
#endif
    }

    if (*location != NULL && (*location)[0] != '/' && (*location)[0] != '\\' &&
        !(isalpha((*location)[0]) && (*location)[1] == ':'))
    {
        char cwd[HARP_MAX_PATH_LENGTH + 1];
        char *relative_location;

        /* change relative path into absolute path */

        if (getcwd(cwd, HARP_MAX_PATH_LENGTH) == NULL)
        {
            /* there is a problem with the current working directory -> return 'not found' */
            return 0;
        }
        cwd[HARP_MAX_PATH_LENGTH] = '\0';

        relative_location = *location;
        if (harp_path_from_path(cwd, 0, relative_location, location) != 0)
        {
            free(relative_location);
            return -1;
        }
        free(relative_location);
    }

    return 0;
}

/** \addtogroup harp_general
 * @{
 */

/** Remove everything but the last pathname component from \a path.
 * \param path Path to compute the basename of.
 * \return Pointer to the last pathname component of \a path, i.e. everything from the end of \a path up to the first
 *   pathname component separation character ('\\' or '/' on Windows, '/' otherwise).
 */
LIBHARP_API const char *harp_basename(const char *path)
{
    if (path == NULL)
    {
        return NULL;
    }
    else
    {
        const char *separator = NULL;

#ifdef WIN32
        const char *cursor = path;

        while (*cursor != '\0')
        {
            if (*cursor == '\\' || *cursor == '/')
            {
                separator = cursor;
            }

            cursor++;
        }
#else
        separator = strrchr(path, '/');
#endif

        return (separator == NULL ? path : separator + 1);
    }
}

/** Returns the name of a data type.
 * \param data_type HARP basic data type
 * \return if the data type is known a string containing the name of the type, otherwise the string "unknown".
 */
LIBHARP_API const char *harp_get_data_type_name(harp_data_type data_type)
{
    switch (data_type)
    {
        case harp_type_int8:
            return "int8";
        case harp_type_int16:
            return "int16";
        case harp_type_int32:
            return "int32";
        case harp_type_float:
            return "float";
        case harp_type_double:
            return "double";
        case harp_type_string:
            return "string";
        default:
            assert(0);
            exit(1);
    }
}

/** Retrieve the byte size for a HARP data type.
 * \param data_type Data type for which to retrieve the size.
 * \return The size of the data type in bytes.
 */
LIBHARP_API long harp_get_size_for_type(harp_data_type data_type)
{
    switch (data_type)
    {
        case harp_type_int8:
            return (long)sizeof(int8_t);
        case harp_type_int16:
            return (long)sizeof(int16_t);
        case harp_type_int32:
            return (long)sizeof(int32_t);
        case harp_type_float:
            return (long)sizeof(float);
        case harp_type_double:
            return (long)sizeof(double);
        case harp_type_string:
            return (long)sizeof(char *);
        default:
            assert(0);
            exit(1);
    }
}

/** Retrieve the fill value for a HARP data type.
 * \param data_type Data type for which to retrieve the fill value.
 * \return The fill value for the data type.
 */
LIBHARP_API harp_scalar harp_get_fill_value_for_type(harp_data_type data_type)
{
    harp_scalar fill_value;

    switch (data_type)
    {
        case harp_type_int8:
            fill_value.int8_data = 0;
            break;
        case harp_type_int16:
            fill_value.int16_data = 0;
            break;
        case harp_type_int32:
            fill_value.int32_data = 0;
            break;
        case harp_type_float:
            fill_value.float_data = (float)harp_nan();
            break;
        case harp_type_double:
            fill_value.double_data = harp_nan();
            break;
        default:
            assert(0);
            exit(1);
    }

    return fill_value;
}

/** Retrieve the minimum valid value for a HARP data type.
 * \param data_type Data type for which to retrieve the minimum valid value.
 * \return The minimum valid value of the data type.
 */
LIBHARP_API harp_scalar harp_get_valid_min_for_type(harp_data_type data_type)
{
    harp_scalar valid_min;

    switch (data_type)
    {
        case harp_type_int8:
            valid_min.int8_data = -128;
            break;
        case harp_type_int16:
            valid_min.int16_data = -32768;
            break;
        case harp_type_int32:
            valid_min.int32_data = -2147483647 - 1;
            break;
        case harp_type_float:
            valid_min.float_data = (float)harp_mininf();
            break;
        case harp_type_double:
            valid_min.double_data = harp_mininf();
            break;
        default:
            assert(0);
            exit(1);
    }

    return valid_min;
}

/** Retrieve the maximum valid value for a HARP data type.
 * \param data_type Data type for which to retrieve the maximum valid value.
 * \return The maximum valid value of the data type.
 */
LIBHARP_API harp_scalar harp_get_valid_max_for_type(harp_data_type data_type)
{
    harp_scalar valid_max;

    switch (data_type)
    {
        case harp_type_int8:
            valid_max.int8_data = 127;
            break;
        case harp_type_int16:
            valid_max.int16_data = 32767;
            break;
        case harp_type_int32:
            valid_max.int32_data = 2147483647;
            break;
        case harp_type_float:
            valid_max.float_data = (float)harp_plusinf();
            break;
        case harp_type_double:
            valid_max.double_data = harp_plusinf();
            break;
        default:
            assert(0);
            exit(1);
    }

    return valid_max;
}

/** Test if \a value equals the fill value for the specified data type.
 * \param  data_type Data type corresponding to the value of \a value.
 * \param  value Value to test.
 * \return
 *   \arg \c 0, Value is not equal to the fill value.
 *   \arg \c 1, Value equals the fill value.
 */
LIBHARP_API int harp_is_fill_value_for_type(harp_data_type data_type, harp_scalar value)
{
    switch (data_type)
    {
        case harp_type_int8:
            return value.int8_data == 0;
        case harp_type_int16:
            return value.int16_data == 0;
        case harp_type_int32:
            return value.int32_data == 0;
        case harp_type_float:
            return harp_isnan(value.float_data);
        case harp_type_double:
            return harp_isnan(value.double_data);
        default:
            assert(0);
            exit(1);
    }
}

/** Test if \a value equals the minimum valid value for the specified data type.
 * \param  data_type Data type corresponding to the value of \a value.
 * \param  value Value to test.
 * \return
 *   \arg \c 0, Value is not equal to the minimum valid value.
 *   \arg \c 1, Value equals the minimum valid value.
 */
LIBHARP_API int harp_is_valid_min_for_type(harp_data_type data_type, harp_scalar value)
{
    switch (data_type)
    {
        case harp_type_int8:
            return value.int8_data == -128;
        case harp_type_int16:
            return value.int16_data == -32768;
        case harp_type_int32:
            return value.int32_data == -2147483647 - 1;
        case harp_type_float:
            return harp_ismininf(value.float_data);
        case harp_type_double:
            return harp_ismininf(value.double_data);
        default:
            assert(0);
            exit(1);
    }
}

/** Test if \a value equals the maximum valid value for the specified data type.
 * \param  data_type Data type corresponding to the value of \a value.
 * \param  value Value to test.
 * \return
 *   \arg \c 0, Value is not equal to the maximum valid value.
 *   \arg \c 1, Value equals the maximum valid value.
 */
LIBHARP_API int harp_is_valid_max_for_type(harp_data_type data_type, harp_scalar value)
{
    switch (data_type)
    {
        case harp_type_int8:
            return value.int8_data == 127;
        case harp_type_int16:
            return value.int16_data == 32767;
        case harp_type_int32:
            return value.int32_data == 2147483647;
        case harp_type_float:
            return harp_isplusinf(value.float_data);
        case harp_type_double:
            return harp_isplusinf(value.double_data);
        default:
            assert(0);
            exit(1);
    }
}

/** Find out whether a double value is a finite number (i.e. not NaN and not infinite).
 * \param x  A double value.
 * \return
 *   \arg \c 1, The double value is a finite number.
 *   \arg \c 0, The double value is not a finite number.
 */
LIBHARP_API int harp_isfinite(double x)
{
    return (!harp_isnan(x) && !harp_isinf(x));
}

/** Find out whether a double value equals NaN (Not a Number).
 * \param x  A double value.
 * \return
 *   \arg \c 1, The double value equals NaN.
 *   \arg \c 0, The double value does not equal NaN.
 */
LIBHARP_API int harp_isnan(double x)
{
    uint64_t e_mask, f_mask;

    union
    {
        uint64_t as_int;
        double as_double;
    } mkNaN;

    mkNaN.as_double = x;

    e_mask = 0x7ff0;
    e_mask <<= 48;

    if ((mkNaN.as_int & e_mask) != e_mask)
        return 0;       /* e != 2047 */

    f_mask = 1;
    f_mask <<= 52;
    f_mask--;

    /* number is NaN if f does not equal zero. */
    return (mkNaN.as_int & f_mask) != 0;
}

/** Retrieve a double value that respresents NaN (Not a Number).
 * \return The double value 'NaN'.
 */
LIBHARP_API double harp_nan(void)
{
    union
    {
        uint64_t as_int;
        double as_double;
    } mkNaN;

    mkNaN.as_int = 0x7ff8;
    mkNaN.as_int <<= 48;

    return mkNaN.as_double;
}

/** Find out whether a double value equals inf (either positive or negative infinity).
 * \param x  A double value.
 * \return
 *   \arg \c 1, The double value equals inf.
 *   \arg \c 0, The double value does not equal inf.
 */
LIBHARP_API int harp_isinf(double x)
{
    return harp_isplusinf(x) || harp_ismininf(x);
}

/** Find out whether a double value equals +inf (positive infinity).
 * \param x  A double value.
 * \return
 *   \arg \c 1, The double value equals +inf.
 *   \arg \c 0, The double value does not equal +inf.
 */
LIBHARP_API int harp_isplusinf(double x)
{
    uint64_t plusinf;

    union
    {
        uint64_t as_int;
        double as_double;
    } mkInf;

    mkInf.as_double = x;

    plusinf = 0x7ff0;
    plusinf <<= 48;

    return mkInf.as_int == plusinf;
}

/** Find out whether a double value equals -inf (negative infinity).
 * \param x  A double value.
 * \return
 *   \arg \c 1, The double value equals -inf.
 *   \arg \c 0, The double value does not equal -inf.
 */
LIBHARP_API int harp_ismininf(double x)
{
    uint64_t mininf;

    union
    {
        uint64_t as_int;
        double as_double;
    } mkInf;

    mkInf.as_double = x;

    mininf = 0xfff0;
    mininf <<= 48;

    return mkInf.as_int == mininf;
}

/** Retrieve a double value that respresents +inf (positive infinity).
 * \return The double value '+inf'.
 */
LIBHARP_API double harp_plusinf(void)
{
    union
    {
        uint64_t as_int;
        double as_double;
    } mkInf;

    mkInf.as_int = 0x7ff0;
    mkInf.as_int <<= 48;

    return mkInf.as_double;
}

/** Retrieve a double value that respresents -inf (negative infinity).
 * \return The double value '-inf'.
 */
LIBHARP_API double harp_mininf(void)
{
    union
    {
        uint64_t as_int;
        double as_double;
    } mkInf;

    mkInf.as_int = 0xfff0;
    mkInf.as_int <<= 48;

    return mkInf.as_double;
}

/**
 * @}
 */

long harp_parse_double(const char *buffer, long buffer_length, double *dst, int ignore_trailing_bytes)
{
    long length;
    int value_length;
    int exponent_length;
    int has_sign;
    double value;
    long exponent;
    int negative = 0;

    length = buffer_length;

    while (length > 0 && *buffer == ' ')
    {
        buffer++;
        length--;
    }

    has_sign = 0;
    if (length > 0)
    {
        if (*buffer == '+' || *buffer == '-')
        {
            negative = (*buffer == '-');
            has_sign = 1;
            buffer++;
            length--;
        }
    }

    /* check for NaN/Inf */
    if (length >= 3)
    {
        if ((buffer[0] == 'N' || buffer[0] == 'n') && (buffer[1] == 'A' || buffer[1] == 'a') &&
            (buffer[2] == 'N' || buffer[2] == 'n') && !has_sign)
        {
            length -= 3;
            if (!ignore_trailing_bytes && length != 0)
            {
                harp_set_error(HARP_ERROR_INVALID_FORMAT, "invalid format for ascii floating point value");
                return -1;
            }
            *dst = harp_nan();
            return buffer_length - length;
        }
        else if ((buffer[0] == 'I' || buffer[0] == 'i') && (buffer[1] == 'N' || buffer[1] == 'n') &&
                 (buffer[2] == 'F' || buffer[2] == 'f'))
        {
            length -= 3;
            if (!ignore_trailing_bytes && length != 0)
            {
                harp_set_error(HARP_ERROR_INVALID_FORMAT, "invalid format for ascii floating point value");
                return -1;
            }
            *dst = negative ? harp_mininf() : harp_plusinf();
            return buffer_length - length;
        }
    }

    value = 0;
    exponent = 0;
    value_length = 0;
    /* read mantissa part before the digit */
    while (length > 0)
    {
        if (*buffer < '0' || *buffer > '9')
        {
            break;
        }
        value = 10 * value + (*buffer - '0');
        value_length++;
        buffer++;
        length--;
    }
    /* read digit and mantissa part after the digit */
    if (length > 0)
    {
        if (*buffer == '.')
        {
            buffer++;
            length--;
            while (length > 0)
            {
                if (*buffer < '0' || *buffer > '9')
                {
                    break;
                }
                value = 10 * value + (*buffer - '0');
                exponent--;
                value_length++;
                buffer++;
                length--;
            }
        }
    }
    if (value_length == 0)
    {
        harp_set_error(HARP_ERROR_INVALID_FORMAT, "invalid format for ascii floating point value (no digits)");
        return -1;
    }

    if (negative)
    {
        value = -value;
    }
    /* read exponent part */
    if (length > 0 && (*buffer == 'd' || *buffer == 'D' || *buffer == 'e' || *buffer == 'E'))
    {
        long exponent_value;

        buffer++;
        length--;
        negative = 0;
        if (length > 0)
        {
            if (*buffer == '+' || *buffer == '-')
            {
                negative = (*buffer == '-');
                buffer++;
                length--;
            }
        }
        exponent_value = 0;
        exponent_length = 0;
        while (length > 0)
        {
            if (*buffer < '0' || *buffer > '9')
            {
                break;
            }
            exponent_value = 10 * exponent_value + (*buffer - '0');
            exponent_length++;
            buffer++;
            length--;
        }
        if (exponent_length == 0)
        {
            harp_set_error(HARP_ERROR_INVALID_FORMAT,
                           "invalid format for ascii floating point value (empty exponent value)");
            return -1;
        }
        if (negative)
        {
            exponent_value = -exponent_value;
        }
        exponent += exponent_value;
    }

    if (!ignore_trailing_bytes && length != 0)
    {
        while (length > 0 && *buffer == ' ')
        {
            buffer++;
            length--;
        }
        if (length != 0)
        {
            harp_set_error(HARP_ERROR_INVALID_FORMAT, "invalid format for ascii floating point value");
            return -1;
        }
    }

    if (exponent != 0)
    {
        value *= ipow(10, exponent);
    }

    *dst = value;

    return buffer_length - length;
}

/** Compute the number of elements from a list of dimension lengths.
 * \param num_dimensions Number of dimensions.
 * \param dimension Dimension lengths.
 * \return Number of elements (i.e. the product of the specified dimension lengths, or \c 1 if \a num_dimensions equals
 *   \c 0).
 */
long harp_get_num_elements(int num_dimensions, const long *dimension)
{
    long num_elements;
    int i;

    num_elements = 1;
    for (i = 0; i < num_dimensions; i++)
    {
        num_elements *= dimension[i];
    }

    return num_elements;
}

/**
 * Return the length of the longest string.
 * \param num_strings Number of strings in the array.
 * \param string_data Array of strings to operate on.
 * \return Length of the longest string.
 */
long harp_get_max_string_length(long num_strings, char **string_data)
{
    long max_length = 0;
    long i;

    for (i = 0; i < num_strings; i++)
    {
        if (string_data[i] != NULL)
        {
            long length = (long)strlen(string_data[i]);

            if (length > max_length)
            {
                max_length = length;
            }
        }
    }

    return max_length;
}

/**
 * Convert an array of variable length strings to a character array of fixed length strings. The size of the character
 * array is \a num_strings times \a min_string_length or the length of the longest string in \a string_data (whichever
 * is larger). Shorter strings will be padded with NUL (termination) characters. The caller is responsible for further
 * memory management of the character array.
 * \param[in] num_strings Number of strings in the array.
 * \param[in] string_data Array of strings to operate on.
 * \param[in] min_string_length Minimal fixed string length.
 * \param[out] string_length Pointer to the location where the length of the longest string will be stored. If NULL, the
 *   length will not be stored.
 * \param[out] char_data Pointer to the location where the pointer to the character array will be stored.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
int harp_get_char_array_from_string_array(long num_strings, char **string_data, long min_string_length,
                                          long *string_length, char **char_data)
{
    char *buffer;
    long length;
    long i;

    /* Determine fixed string length to use. */
    length = harp_get_max_string_length(num_strings, string_data);
    if (length < min_string_length)
    {
        length = min_string_length;
    }

    /* Allocate character array. */
    buffer = malloc((size_t)num_strings * length * sizeof(char));
    if (buffer == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_strings * length * sizeof(char), __FILE__, __LINE__);
        return -1;
    }

    /* Fill char array with NUL ('\0') characters. */
    memset(buffer, '\0', (size_t)(num_strings * length) * sizeof(char));

    /* Copy strings. */
    for (i = 0; i < num_strings; i++)
    {
        if (string_data[i] != NULL)
        {
            memcpy(&buffer[i * length], string_data[i], strlen(string_data[i]));
        }
    }

    if (string_length != NULL)
    {
        *string_length = length;
    }

    *char_data = buffer;

    return 0;
}

static void fill_int8(long num_elements, int8_t *data, int8_t value)
{
    int8_t *last;

    last = data + num_elements;
    while (data != last)
    {
        *data = value;
        data++;
    }
}

static void fill_int16(long num_elements, int16_t *data, int16_t value)
{
    int16_t *last;

    last = data + num_elements;
    while (data != last)
    {
        *data = value;
        data++;
    }
}

static void fill_int32(long num_elements, int32_t *data, int32_t value)
{
    int32_t *last;

    last = data + num_elements;
    while (data != last)
    {
        *data = value;
        data++;
    }
}

static void fill_float(long num_elements, float *data, float value)
{
    float *last;

    last = data + num_elements;
    while (data != last)
    {
        *data = value;
        data++;
    }
}

static void fill_double(long num_elements, double *data, double value)
{
    double *last;

    last = data + num_elements;
    while (data != last)
    {
        *data = value;
        data++;
    }
}

static void null_string(long num_elements, char **data)
{
    char **last;

    last = data + num_elements;
    while (data != last)
    {
        if (*data != NULL)
        {
            free(*data);
            *data = NULL;
        }
        data++;
    }
}

/** Fill an array with the default HARP fill value for the specified data type.
 * \param data_type Data type of the array.
 * \param num_elements Number of elements in the array.
 * \param data Array that should be nulled.
 */
void harp_array_null(harp_data_type data_type, long num_elements, harp_array data)
{
    switch (data_type)
    {
        case harp_type_int8:
            fill_int8(num_elements, data.int8_data, 0);
            break;
        case harp_type_int16:
            fill_int16(num_elements, data.int16_data, 0);
            break;
        case harp_type_int32:
            fill_int32(num_elements, data.int32_data, 0);
            break;
        case harp_type_float:
            fill_float(num_elements, data.float_data, (float)harp_nan());
            break;
        case harp_type_double:
            fill_double(num_elements, data.double_data, harp_nan());
            break;
        case harp_type_string:
            null_string(num_elements, data.string_data);
            break;
        default:
            assert(0);
            exit(1);
    }
}

/** Replace each occurrence of a specific (fill) value in an array with the default HARP fill value for the specified
 *  data type.
 * \param data_type Data type of the array.
 * \param num_elements Number of elements in the array.
 * \param data Array to operate on.
 * \param fill_value Value to be replaced by the default HARP fill value for \a data_type.
 */
void harp_array_replace_fill_value(harp_data_type data_type, long num_elements, harp_array data, harp_scalar fill_value)
{
    harp_scalar harp_fill_value;
    long i;

    if (harp_is_fill_value_for_type(data_type, fill_value))
    {
        return;
    }

    harp_fill_value = harp_get_fill_value_for_type(data_type);
    switch (data_type)
    {
        case harp_type_int8:
            for (i = 0; i < num_elements; i++)
            {
                if (data.int8_data[i] == fill_value.int8_data)
                {
                    data.int8_data[i] = harp_fill_value.int8_data;
                }
            }
            break;
        case harp_type_int16:
            for (i = 0; i < num_elements; i++)
            {
                if (data.int16_data[i] == fill_value.int16_data)
                {
                    data.int16_data[i] = harp_fill_value.int16_data;
                }
            }
            break;
        case harp_type_int32:
            for (i = 0; i < num_elements; i++)
            {
                if (data.int32_data[i] == fill_value.int32_data)
                {
                    data.int32_data[i] = harp_fill_value.int32_data;
                }
            }
            break;
        case harp_type_float:
            for (i = 0; i < num_elements; i++)
            {
                if (data.float_data[i] == fill_value.float_data)
                {
                    data.float_data[i] = harp_fill_value.float_data;
                }
            }
            break;
        case harp_type_double:
            for (i = 0; i < num_elements; i++)
            {
                if (data.double_data[i] == fill_value.double_data)
                {
                    data.double_data[i] = harp_fill_value.double_data;
                }
            }
            break;
        default:
            assert(0);
            exit(1);
    }
}

/** Invert the array across a given dimension
 * \param data_type Data type of the array.
 * \param dim_id Index of the dimension that should be inverted.
 * \param num_dimensions Number of dimensions in the array.
 * \param dimension Dimension sizes of the array.
 * \param data Array that should have a dimension inverted.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
int harp_array_invert(harp_data_type data_type, int dim_id, int num_dimensions, const long *dimension, harp_array data)
{
    long num_elements;
    long block_size;
    long length;
    long i, j;

    assert(dim_id >= 0 && dim_id < num_dimensions);

    /* total byte size of array = num_elements * length * block_size */

    num_elements = 1;
    for (i = 0; i < dim_id; i++)
    {
        num_elements *= dimension[i];
    }

    length = dimension[dim_id];
    if (length <= 1)
    {
        /* nothing to do */
        return 0;
    }

    block_size = harp_get_size_for_type(data_type);
    for (i = dim_id + 1; i < num_dimensions; i++)
    {
        block_size *= dimension[i];
    }

    if (block_size == 1)
    {
        for (i = 0; i < num_elements; i++)
        {
            int8_t *block = &data.int8_data[i * length];

            for (j = 0; j < length / 2; j++)
            {
                int8_t temp = block[j];

                block[j] = block[length - 1 - j];
                block[length - 1 - j] = temp;
            }
        }
    }
    else if (block_size == 2)
    {
        for (i = 0; i < num_elements; i++)
        {
            int16_t *block = &data.int16_data[i * length];

            for (j = 0; j < length / 2; j++)
            {
                int16_t temp = block[j];

                block[j] = block[length - 1 - j];
                block[length - 1 - j] = temp;
            }
        }
    }
    else if (block_size == 4)
    {
        for (i = 0; i < num_elements; i++)
        {
            int32_t *block = &data.int32_data[i * length];

            for (j = 0; j < length / 2; j++)
            {
                int32_t temp = block[j];

                block[j] = block[length - 1 - j];
                block[length - 1 - j] = temp;
            }
        }
    }
    else if (block_size == 8)
    {
        for (i = 0; i < num_elements; i++)
        {
            double *block = &data.double_data[i * length];

            for (j = 0; j < length / 2; j++)
            {
                double temp = block[j];

                block[j] = block[length - 1 - j];
                block[length - 1 - j] = temp;
            }
        }
    }
    else
    {
        uint8_t *buffer;

        buffer = (uint8_t *)malloc(length * block_size);
        if (buffer == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           dimension[dim_id] * block_size, __FILE__, __LINE__);
            return -1;
        }
        for (i = 0; i < num_elements; i++)
        {
            int8_t *block = &data.int8_data[i * length * block_size];

            for (j = 0; j < length; j++)
            {
                memcpy(&buffer[j * block_size], &block[(length - 1 - j) * block_size], block_size);
            }
            memcpy(block, buffer, length * block_size);
        }
        free(buffer);
    }

    return 0;
}

/** Permute the dimensions of an array.
 *
 * If \a order is NULL, the order of the dimensions of the source array will be reversed, i.e. the array will be
 * transposed. For example, if the dimensions of the source array are [10, 20, 30], the dimensions of the destination
 * array will be [30, 20, 10]. (This is equivalent to specifying an order of [2, 1, 0].)
 *
 * Otherwise, the order of the dimensions of the source array will permuted according to \a order. For example, if the
 * dimensions of the source array are [10, 20, 30] and the specified order is [1, 0, 2], the dimensions of the
 * destination array will be [20, 10, 30].
 *
 * \param data_type Data type of the array.
 * \param num_dimensions Number of dimensions in the array.
 * \param dimension Dimension lengths of the array.
 * \param order If NULL, reverse the order of the dimensions of the array, otherwise permute the order of the dimensions
 *   of the array according to the order specified.
 * \param data Array of which the dimensions should be permuted.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
int harp_array_transpose(harp_data_type data_type, int num_dimensions, const long *dimension, const int *order,
                         harp_array data)
{
    long rindex[HARP_MAX_NUM_DIMS] = { 0 };     /* reversed index in multi-dimensional array */
    long rdim[HARP_MAX_NUM_DIMS];       /* reversed order of dim[] */
    long stride[HARP_MAX_NUM_DIMS];     /* stride in the destination array (in reverse order) */
    long num_elements;
    long element_size;
    long index = 0;
    long i;
    uint8_t *src;
    uint8_t *dst;

    if (num_dimensions <= 1)
    {
        /* nothing to do */
        return 0;
    }

    num_elements = harp_get_num_elements(num_dimensions, dimension);
    if (num_elements <= 1)
    {
        /* nothing to do */
        return 0;
    }

    for (i = 0; i < num_dimensions; i++)
    {
        rdim[i] = dimension[num_dimensions - 1 - i];
    }

    if (order == NULL)
    {
        /* By default, reverse the order of the dimensions. */
        stride[num_dimensions - 1] = 1;
        for (i = num_dimensions - 1; i > 0; i--)
        {
            stride[i - 1] = stride[i] * rdim[i];
        }
    }
    else
    {
        int iorder[HARP_MAX_NUM_DIMS] = { 0 };  /* map from destination dimension index to source dimension index */

        /* Compute the map from destination dimension index to source dimension index (i.e. the inverse of order, which
         * is the map from source dimension index to destination dimension index.
         */
        for (i = 0; i < num_dimensions; i++)
        {
            if (order[i] < 0 || order[i] >= num_dimensions)
            {
                harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "dimension index '%d' out of bounds at index %d of "
                               "dimension order (%s:%lu)", order[i], i, __FILE__, __LINE__);
                return -1;
            }

            if (iorder[order[i]] != 0)
            {
                harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "duplicate dimension index '%d' at index %d of dimension "
                               "order (%s:%u)", order[i], i, __FILE__, __LINE__);
                return -1;
            }

            iorder[order[i]] = i;
        }

        /* Compute the stride in the destination array for each dimension of the source array in reverse order. For
         * example, stride[0] is the stride in the destination array when moving along the fastest running dimension of
         * the source array (i.e. the dimension with index num_dimensions - 1).
         */
        for (i = 0; i < num_dimensions; i++)
        {
            long j;

            stride[num_dimensions - 1 - i] = 1;
            for (j = iorder[i] + 1; j < num_dimensions; j++)
            {
                stride[num_dimensions - 1 - i] *= dimension[order[j]];
            }
        }
    }

    element_size = harp_get_size_for_type(data_type);
    src = (uint8_t *)data.ptr;

    dst = (uint8_t *)malloc(num_elements * element_size);
    if (dst == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_elements * element_size, __FILE__, __LINE__);
        return -1;
    }

    switch (element_size)
    {
        case 1:
            dst[index] = src[i];
            index = stride[0];
            rindex[0] = 1;
            for (i = 1; i < num_elements; i++)
            {
                int j = 0;

                while (rindex[j] == rdim[j])
                {
                    rindex[j] = 0;
                    index -= stride[j] * rdim[j];
                    j++;
                    index += stride[j];
                    rindex[j]++;
                }
                dst[index] = src[i];
                index += stride[0];
                rindex[0]++;
            }
            break;
        case 2:
            ((uint16_t *)dst)[0] = ((uint16_t *)src)[0];
            index = stride[0];
            rindex[0] = 1;
            for (i = 1; i < num_elements; i++)
            {
                int j = 0;

                while (rindex[j] == rdim[j])
                {
                    rindex[j] = 0;
                    index -= stride[j] * rdim[j];
                    j++;
                    index += stride[j];
                    rindex[j]++;
                }
                ((uint16_t *)dst)[index] = ((uint16_t *)src)[i];
                index += stride[0];
                rindex[0]++;
            }
            break;
        case 4:
            ((uint32_t *)dst)[0] = ((uint32_t *)src)[0];
            index = stride[0];
            rindex[0] = 1;
            for (i = 1; i < num_elements; i++)
            {
                int j = 0;

                while (rindex[j] == rdim[j])
                {
                    rindex[j] = 0;
                    index -= stride[j] * rdim[j];
                    j++;
                    index += stride[j];
                    rindex[j]++;
                }
                ((uint32_t *)dst)[index] = ((uint32_t *)src)[i];
                index += stride[0];
                rindex[0]++;
            }
            break;
        case 8:
            ((uint64_t *)dst)[0] = ((uint64_t *)src)[0];
            index = stride[0];
            rindex[0] = 1;
            for (i = 1; i < num_elements; i++)
            {
                long j = 0;

                while (rindex[j] == rdim[j])
                {
                    rindex[j] = 0;
                    index -= stride[j] * rdim[j];
                    j++;
                    index += stride[j];
                    rindex[j]++;
                }
                ((uint64_t *)dst)[index] = ((uint64_t *)src)[i];
                index += stride[0];
                rindex[0]++;
            }
            break;
        default:
            assert(0);
            exit(1);
    }

    memcpy(data.ptr, dst, num_elements * element_size);

    free(dst);

    return 0;
}
