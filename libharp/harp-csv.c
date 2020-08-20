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
#include "harp-csv.h"

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

int harp_csv_parse_double(char **str, double *value)
{
    char *cursor = *str;
    int stringlength = 0;

    *value = harp_nan();

    /* Skip leading white space */
    while (*cursor == ' ')
    {
        cursor++;
    }

    /* Grab string */
    while (cursor[stringlength] != ',' && cursor[stringlength] != '\0')
    {
        stringlength++;
    }
    if (cursor[stringlength] == '\0')
    {
        *str = &cursor[stringlength];
    }
    else
    {
        cursor[stringlength] = '\0';
        *str = &cursor[stringlength + 1];
    }
    if (sscanf(cursor, "%lf", value) != 1)
    {
        harp_set_error(HARP_ERROR_INVALID_FORMAT, "could not parse floating point value from csv element '%s'", cursor);
        return -1;
    }

    return 0;
}

int harp_csv_parse_long(char **str, long *value)
{
    char *cursor = *str;
    size_t stringlength = 0;

    *value = 0;

    /* Skip leading white space */
    while (*cursor == ' ')
    {
        cursor++;
    }

    /* Grab string */
    while (cursor[stringlength] != ',' && cursor[stringlength] != '\0')
    {
        stringlength++;
    }
    if (cursor[stringlength] == '\0')
    {
        *str = &cursor[stringlength];
    }
    else
    {
        cursor[stringlength] = '\0';
        *str = &cursor[stringlength + 1];
    }
    if (sscanf(cursor, "%ld", value) != 1)
    {
        harp_set_error(HARP_ERROR_INVALID_FORMAT, "could not parse long value from csv element '%s'", cursor);
        return -1;
    }

    return 0;
}

int harp_csv_parse_string(char **str, char **value)
{
    char *cursor = *str;
    int stringlength = 0;

    /* Skip leading white space */
    while (*cursor == ' ')
    {
        cursor++;
    }

    /* Grab string */
    while (cursor[stringlength] != ',' && cursor[stringlength] != '\0')
    {
        stringlength++;
    }
    if (cursor[stringlength] == '\0')
    {
        if (stringlength == 0)
        {
            harp_set_error(HARP_ERROR_INVALID_FORMAT, "could not parse string value from csv element");
            return -1;
        }
        *str = &cursor[stringlength];
    }
    else
    {
        cursor[stringlength] = '\0';
        *str = &cursor[stringlength + 1];
    }
    *value = cursor;

    return 0;
}

int harp_csv_parse_variable_name_and_unit(char **str, char **variable_name, char **unit)
{
    char *cursor = *str;
    int stringlength = 0;

    /* Skip leading white space */
    while (*cursor == ' ')
    {
        cursor++;
    }

    /* Grab string */
    while (cursor[stringlength] != ',' && cursor[stringlength] != '\0')
    {
        stringlength++;
    }
    if (cursor[stringlength] == '\0')
    {
        *str = &cursor[stringlength];
    }
    else
    {
        cursor[stringlength] = '\0';
        *str = &cursor[stringlength + 1];
    }

    /* Split string in variable name + unit */
    *variable_name = cursor;
    *unit = NULL;

    /* parse variable name */
    stringlength = 0;
    while (cursor[stringlength] != ' ' && cursor[stringlength] != '\0')
    {
        stringlength++;
    }
    if (cursor[stringlength] != '\0')
    {
        cursor[stringlength] = '\0';
        cursor = &cursor[stringlength + 1];
    }
    if (!harp_is_identifier(*variable_name))
    {
        harp_set_error(HARP_ERROR_INVALID_FORMAT, "variable name '%s' in csv element is not an identifier",
                       *variable_name);
        return -1;
    }

    /* parse unit */
    if (cursor != *variable_name)
    {
        /* Skip leading white space */
        while (*cursor == ' ')
        {
            cursor++;
        }
        if (*cursor != '[')
        {
            harp_set_error(HARP_ERROR_INVALID_FORMAT, "invalid unit '%s' in csv element", cursor);
            return -1;
        }
        cursor++;
        stringlength = 0;
        while (cursor[stringlength] != ']' && cursor[stringlength] != '\0')
        {
            stringlength++;
        }
        if (cursor[stringlength] != ']')
        {
            harp_set_error(HARP_ERROR_INVALID_FORMAT, "invalid unit '%s' in csv element", cursor);
            return -1;
        }
        *unit = cursor;
        cursor[stringlength] = '\0';
        cursor = &cursor[stringlength + 1];
        while (*cursor != '\0')
        {
            if (*cursor != ' ')
            {
                harp_set_error(HARP_ERROR_INVALID_FORMAT, "invalid trailing characters in csv element");
                return -1;
            }
            cursor++;
        }
    }

    return 0;
}
