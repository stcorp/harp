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
    while (isspace(str))
    {
        str++;
    }

    return str;
}
