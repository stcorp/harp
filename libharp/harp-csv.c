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

#include "harp-internal.h"
#include "harp-csv.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

void harp_csv_parse_double(char **str, double *value)
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
    sscanf(cursor, "%lf", value);
}

void harp_csv_parse_long(char **str, long *value)
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
    sscanf(cursor, "%ld", value);
}

void harp_csv_parse_string(char **str, char **value)
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
    *value = cursor;
}

int harp_csv_get_num_lines(FILE *file, const char *filename, long *new_num_lines)
{
    long length;
    char LINE[HARP_CSV_LINE_LENGTH];
    long num_lines = 0;

    while (fgets(LINE, HARP_CSV_LINE_LENGTH, file) != NULL)
    {
        /* Trim the line */
        length = (long)strlen(LINE);
        while (length > 0 && (LINE[length - 1] == '\r' || LINE[length - 1] == '\n'))
        {
            length--;
        }
        LINE[length] = '\0';

        /* Do not allow empty lines */
        if (length == 1)
        {
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "empty line in file '%s'", filename);
            return -1;
        }

        num_lines++;
    }

    *new_num_lines = num_lines;
    return 0;
}

/* Modifies the string end */
void harp_csv_rtrim(char *str)
{
    size_t length = strlen(str);

    while (length > 0 && (str[length - 1] == '\r' || str[length - 1] == '\n' || str[length - 1] == '\t' ||
                          str[length - 1] == ' '))
    {
        length--;
    }
    str[length] = '\0';
}

/* Returns a pointer to a substring */
char *harp_csv_ltrim(char *str)
{
    while (isspace(str[0]))
    {
        str++;
    }

    return str;
}
