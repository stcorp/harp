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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "harp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <harp-internal.h>

#define LINE_LENGTH 1024

static int grab_string(char LINE[], int *new_cursor_position, char **new_string)
{
    char *cursor = NULL;
    int original_cursor_position = *new_cursor_position;
    int cursor_position = *new_cursor_position;
    char *string = NULL;
    size_t stringlength = 0;

    cursor = LINE + original_cursor_position;

    /* Skip leading white space */
    while (*cursor == ' ')
    {
        cursor++;
        cursor_position++;
    }

    /* Grab string */
    stringlength = 0;
    original_cursor_position = cursor_position;
    while (*cursor != ',' && *cursor != '\0')
    {
        cursor++;
        cursor_position++;
        stringlength++;
    }
    cursor = LINE + original_cursor_position;
    string = calloc(stringlength + 1, sizeof(char));
    if (string == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)",
                       __FILE__, __LINE__);
        return -1;
    }

    strncpy(string, cursor, stringlength);
    cursor = LINE + cursor_position;

    *new_cursor_position = cursor_position;
    *new_string = string;
    return 0;
}

static void grab_double(char LINE[], int *new_cursor_position, double *new_value)
{
    char *cursor = NULL;
    int original_cursor_position = *new_cursor_position;
    int cursor_position = *new_cursor_position;
    char *value_string = NULL;
    double value = harp_nan();
    size_t stringlength = 0;

    cursor = LINE + original_cursor_position;

    /* Skip leading white space */
    while (*cursor == ' ')
    {
        cursor++;
        cursor_position++;
    }

    /* Grab string */
    stringlength = 0;
    original_cursor_position = cursor_position;
    while (*cursor != ',' && *cursor != '\0')
    {
        cursor++;
        cursor_position++;
        stringlength++;
    }
    cursor = LINE + original_cursor_position;
    value_string = calloc(stringlength + 1, sizeof(char));
    strncpy(value_string, cursor, stringlength);
    sscanf(value_string, "%lf", &value);
    free(value_string);

    *new_cursor_position = cursor_position;
    *new_value = value;
}

static void rtrim_string(char *str)
{
    if (str != NULL)
    {
        int l;

        l = (int)strlen(str);
        while (l > 0)
        {
            l--;
            if (str[l] != ' ' && str[l] != '\t' && str[l] != '\n' && str[l] != '\f' && str[l] != '\r')
            {
                return;
            }
            str[l] = '\0';
        }
    }
}

int get_num_lines(const char *filename, FILE *ifp, long *new_num_lines)
{
    long length;
    char LINE[LINE_LENGTH];

    long num_lines = 0;

    while (fgets(LINE, LINE_LENGTH, ifp) != NULL)
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
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "Empty line in file '%s'", filename);
            return -1;
        }

        num_lines++;
    }

    *new_num_lines = num_lines;
    return 0;
}

static int grab_name_and_unit_from_string(char *string, char **new_name, char **new_unit)
{
    char *name = NULL;
    char *unit = NULL;
    char *cursor = NULL;
    size_t stringlength = 0;
    int original_cursor_position = 0;
    int cursor_position = 0;

    cursor = string + cursor_position;

    /* Skip commas and white space */
    while (*cursor == ',' || *cursor == ' ')
    {
        cursor++;
        cursor_position++;
    }
    original_cursor_position = cursor_position;

    /* Grab name */
    stringlength = 0;
    while (*cursor != '[' && *cursor != ',' && *cursor != '\0')
    {
        cursor++;
        cursor_position++;
        stringlength++;
    }
    cursor = string + original_cursor_position;
    name = calloc((stringlength + 1), sizeof(char));
    if (name == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       (stringlength + 1) * sizeof(char), __FILE__, __LINE__);

        return -1;
    }
    strncpy(name, cursor, stringlength);

    /* Trim end of difference string */
    rtrim_string(name);

    /* Skip leading white space */
    cursor = string + cursor_position;
    while (*cursor == ' ')
    {
        cursor++;
        cursor_position++;
    }
    original_cursor_position = cursor_position;

    if (*cursor == ',')
    {
        /* No unit is found */
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "No unit in header", string);
        free(name);
        return -1;
    }
    if (*cursor != '[')
    {
        /* No unit is found */
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "No unit in header", string);
        free(name);
        return -1;
    }

    /* Grab the unit */
    if (*cursor == '[')
    {
        cursor++;
        cursor_position++;
        original_cursor_position = cursor_position;
    }
    stringlength = 0;
    while (*cursor != ']' && *cursor != ';' && *cursor != '\0')
    {
        cursor++;
        cursor_position++;
        stringlength++;
    }
    cursor = string + original_cursor_position;
    unit = calloc((stringlength + 1), sizeof(char));
    if (unit == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       (stringlength + 1) * sizeof(char), __FILE__, __LINE__);
        free(name);
        return -1;
    }
    strncpy(unit, cursor, stringlength);

    /* Done, return result */
    *new_name = name;
    *new_unit = unit;

    return 0;
}

static int parse_vertical_grid_header(char LINE[], char **new_name, char **new_unit)
{
    char *name = NULL;
    char *unit = NULL;
    char *string = NULL;
    int cursor_position = 0;

    /* Grab name and unit */
    if (grab_string(LINE, &cursor_position, &string) != 0 ||
        grab_name_and_unit_from_string(string, &name, &unit) != 0)
    {
        goto error;
    }

    *new_name = name;
    *new_unit = unit;

    return 0;

    error:
    free(string);
    free(unit);
    free(name);

    return -1;
}

static int read_vertical_grid_header(FILE *file, const char *filename, char **new_name, char **new_unit)
{
    char LINE[LINE_LENGTH];
    size_t length;
    char *name = NULL;
    char *unit = NULL;

    rewind(file);

    if (fgets(LINE, LINE_LENGTH, file) == NULL)
    {
        harp_set_error(HARP_ERROR_FILE_READ,
                       "Error reading header line in vertical grid '%s'",
                       filename);
        return -1;
    }

    /* Trim end-of-line */
    length = strlen(LINE);
    while (length > 0 && (LINE[length - 1] == '\r' || LINE[length - 1] == '\n'))
    {
        length--;
    }
    LINE[length] = '\0';

    /* Parse header */
    if (parse_vertical_grid_header(LINE, &name, &unit) != 0)
    {
        return -1;
    }

    *new_name = name;
    *new_unit = unit;

    return 0;
}

static int parse_vertical_grid_line(char LINE[], double *new_value)
{
    double value;
    int cursor_position = 0;

    /* Grab difference */
    grab_double(LINE, &cursor_position, &value);

    *new_value = value;
    return 0;
}

static int read_vertical_grid_line(FILE *file, const char *filename, double *new_value)
{
    char LINE[LINE_LENGTH];
    double value;
    size_t length;

    if (fgets(LINE, LINE_LENGTH, file) == NULL)
    {
        harp_set_error(HARP_ERROR_FILE_READ, "Error reading line in vertical grid '%s'", filename);
        return -1;
    }

    /* Trim end-of-line */
    length = strlen(LINE);
    while (length > 0 && (LINE[length - 1] == '\r' || LINE[length - 1] == '\n'))
    {
        length--;
    }
    LINE[length] = '\0';

    /* Parse line */
    if (parse_vertical_grid_line(LINE, &value) != 0)
    {
        return -1;
    }

    *new_value = value;
    return 0;
}

int grid_import(const char *filename, harp_variable **new_vertical_axis)
{
    FILE *file = NULL;
    long num_vertical;
    char *name = NULL;
    char *unit = NULL;
    double *values = NULL;
    double value;
    harp_variable *vertical_axis = NULL;

    /* open the grid file */
    file = fopen(filename, "r+");
    if (file == NULL)
    {
        harp_set_error(HARP_ERROR_FILE_OPEN, "Error opening vertical grid file '%s'", filename);
        return -1;
    }

    /* Determine number of values */
    if (get_num_lines(filename, file, &num_vertical) != 0)
    {
        fclose(file);
        return -1;
    }

    /* Exclude the header line */
    num_vertical--;

    if (num_vertical < 1)
    {
        /* No lines to read */
        harp_set_error(HARP_ERROR_FILE_READ, "Vertical grid file '%s' has no values", filename);
        fclose(file);
        return -1;
    }

    /* Obtain the name and unit of the quantity */
    if (read_vertical_grid_header(file, filename, &name, &unit) != 0)
    {
        fclose(file);
        return -1;
    }

    /* Obtain the values */
    values = malloc((size_t)num_vertical * sizeof(double));
    for (int i = 0; i < num_vertical; i++)
    {
        if (read_vertical_grid_line(file, filename, &value) != 0)
        {
            fclose(file);
            free(values);
            free(name);
            free(unit);
            return -1;
        }

        values[i] = value;
    }

    /* io cleanup */
    if(fclose(file) != 0)
    {
        harp_set_error(HARP_ERROR_FILE_READ, "Error closing vertical grid definition file '%s'", filename);
        free(values);
        free(name);
        free(unit);
        return -1;
    }

    /* validate the axis variable name */
    if ((strcmp(name, "altitude") == 0 || strcmp(name, "pressure") == 0) != 1)
    {
        harp_set_error(HARP_ERROR_INVALID_NAME,
                       "Invalid vertical axis name '%s' in header of csv file '%s'", name,
                       filename);
        free(values);
        free(name);
        free(unit);
        return -1;
    }

    /* create the axis variable */
    if (harp_variable_new(name,
                          harp_type_double,
                          1,
                          (const harp_dimension_type[]) { harp_dimension_vertical },
                          (const long[]) { num_vertical },
                          &vertical_axis) != 0)
    {
        free(values);
        free(name);
        free(unit);
        return -1;
    }

    /* Set the axis unit */
    vertical_axis->unit = strdup(unit);
    if (vertical_axis->unit == NULL)
    {
        harp_variable_delete(vertical_axis);
        free(values);
        free(name);
        free(unit);
        return -1;
    }

    /* Copy the axis data */
    for (int i = 0; i < num_vertical; i++)
    {
        vertical_axis->data.double_data[i] = values[i];
    }

    *new_vertical_axis = vertical_axis;

    return 0;
}

static void print_version()
{
    printf("harpprofile version %s\n", libharp_version);
    printf("Copyright (C) 2015-2016 S[&]T, The Netherlands.\n\n");
}

void print_help_resample(void)
{
    printf("Usage:\n");
    printf("\n");
    printf("    harpprofile resample -h, --help\n");
    printf("        Show help for harpprofile resample (this text)\n");
    printf("\n");
    printf("    harpprofile resample [options] <product file> [output product file]\n");
    printf("        Regrid the vertical profiles in the file\n");
    printf("\n");
    printf("        Options:\n");
    printf("            -of, --output-format <format> :\n");
    printf("                    Possible values for <format> (the output format) are:\n");
    printf("                      netcdf (the default)\n");
    printf("                      hdf4\n");
    printf("                      hdf5\n");
    printf("\n");
    printf("            One of the following:\n");
    printf("            -a, --a-to-b <result_csv_file> <source_datasetdir_b> :\n");
    printf("                    resample the vertical profiles of the input file (part of\n");
    printf("                    dataset A) to the vertical grid of the vertical profiles\n");
    printf("                    in dataset B\n");
    printf("            -b, --b-to-a <result_csv_file> <source_datasetdir_a> :\n");
    printf("                    resample the vertical profiles of the input file (part of\n");
    printf("                    dataset B) to the vertical grid of the vertical profiles\n");
    printf("                    in dataset A\n");
    printf("            -c, --common <input>\n");
    printf("                    resample vertical profiles (in datasets A and B)\n");
    printf("                    to a common grid before calculating the columns.\n");
    printf("                    The common vertical grid is defined in file C.\n");
    printf("                    <input> denotes the filename\n");
    printf("\n");
    printf("        Optional options:\n");
    printf("            -c, --convert-variable 'conv1; conv2; ...; convN' :\n");
    printf("                    convert the unit of variables in the file.\n");
    printf("                    Conversion expressions are semicolon separated and composed\n");
    printf("                    of:\n");
    printf("                       variablename [unit]\n");
    printf("                    For example:\n");
    printf("                     -c 'variablename1 [unit1]; variablename2 [unit2]'\n");
    printf("\n");
    printf("            -va, --vertical-axis <variable_name>\n");
    printf("                    override the default vertical axis. <variable_name> can be\n");
    printf("                    ALTITUDE, PRESSURE, or ALTITUDE.GPH. By default, ALTITUDE\n");
    printf("                    is selected, if not available PRESSURE, otherwise\n");
    printf("                    ALTITUDE.GPH.\n");
    printf("\n");
    printf("            -cd, --collapse-dimension <variable_name>\n");
    printf("                    collapse the vertical dimension of this variable.\n");
    printf("                    Take the midpoint.\n");
    printf("\n");
    printf("        In case -ha/-hb is set, a generated averaging kernel matrix variable\n");
    printf("        can be added with:\n");
    printf("            -aga, --add-generated-akm 'value [unit]'\n");
    printf("                    add generated averaging kernel matrices\n");
    printf("                    Here, 'value' indicates the full-width at half maximum\n");
    printf("                    vertical resolution in altitude.\n");
    printf("            -sf, --smoothing-function 'type'\n");
    printf("                    set the smoothing function type for the generated AKM:\n");
    printf("                       gaussian\n");
    printf("                       triangle\n");
    printf("                       boxcar\n");
    printf("                    By default a Gaussian smoothing function is employed.\n");
    printf("\n");
}

void print_help_smooth(void)
{
    printf("Usage:\n");
    printf("\n");
    printf("    harpprofile smooth -h, --help\n");
    printf("        Show help for harpprofile smooth (this text)\n");
    printf("\n");
    printf("    harpprofile smooth [options] <product file>\n");
    printf("        Smooth the vertical profiles in the file with the averaging kernel\n");
    printf("        matrices and add a priori\n");
    printf("\n");
    printf("        Mandatory options:\n");
    printf("            -o, --output <filename> :\n");
    printf("                    write output to specified file\n");
    printf("                    (by default the input file will be replaced)\n");
    printf("\n");
    printf("            -of, --output-format <format> :\n");
    printf("                    Possible values for <format> (the output format) are:\n");
    printf("                      netcdf (the default)\n");
    printf("                      hdf4\n");
    printf("                      hdf5\n");
    printf("\n");
    printf("            One of the following:\n");
    printf("            -sa, --smooth-a-with-akm-b <result_csv_file> <source_datasetdir_b> :\n");
    printf("                    smooth the vertical profiles of the input file (part of\n");
    printf("                    dataset A) with the averaging kernel matrices and a priori\n");
    printf("                    in dataset B\n");
    printf("            -sb, --smooth-b-with-akm-a <result_csv_file> <source_datasetdir_a> :\n");
    printf("                    smooth the vertical profiles of the input file (part of\n");
    printf("                    dataset B) with the averaging kernel matrices and a priori\n");
    printf("                    in dataset A\n");
    printf("\n");
    printf("            -sga, --smooth-a-with-generated-akm-b <result_csv_file>\n");
    printf("            <source_datasetdir_b> 'value [unit]'\n");
    printf("                    smooth the vertical profiles of the input file (part of\n");
    printf("                    dataset A) with the generated averaging kernel matrices\n");
    printf("                    and add a priori in dataset B\n");
    printf("                    Here, 'value' indicates the full-width at half maximum\n");
    printf("                    vertical resolution in altitude.\n");
    printf("            -sgb, --smooth-b-with-generated-akm-a <result_csv_file>\n");
    printf("            <source_datasetdir_a> 'value [unit]'\n");
    printf("                    smooth the vertical profiles of the input file (part of\n");
    printf("                    dataset A) with the generated averaging kernel matrices\n");
    printf("                    and add a priori in dataset B\n");
    printf("                    When '[unit]' is not specified, the default unit 'km' is\n");
    printf("                    adopted\n");
    printf("            -sf, --smoothing-function 'type'\n");
    printf("                    set the smoothing function type for the generated AKM:\n");
    printf("                       gaussian\n");
    printf("                       triangle\n");
    printf("                       boxcar\n");
    printf("                    By default a Gaussian smoothing function is employed.\n");
    printf("\n");
    printf("        Optional options:\n");
    printf("            -c, --convert-variable 'conv1; conv2; ...; convN' :\n");
    printf("                    convert the unit of variables in the file.\n");
    printf("                    Conversion expressions are semicolon separated and composed\n");
    printf("                    of:\n");
    printf("                       variablename [unit]\n");
    printf("                    For example:\n");
    printf("                     -c 'variablename1 [unit1]; variablename2 [unit2]'\n");
    printf("\n");
    printf("            -va, --vertical-axis <variable_name>\n");
    printf("                    override the default vertical axis. <variable_name> can be\n");
    printf("                    ALTITUDE, PRESSURE, or ALTITUDE.GPH. By default, ALTITUDE\n");
    printf("                    is selected, if not available PRESSURE, otherwise\n");
    printf("                    ALTITUDE.GPH.\n");
    printf("\n");
    printf("            -cd, --collapse-dimension <variable_name>\n");
    printf("                    collapse the vertical dimension of this variable.\n");
    printf("                    Take the midpoint.\n");
    printf("\n");
}

void print_help(void)
{
    printf("Usage:\n");
    printf("  harpprofile subcommand [options]\n");
    printf("    Manipulate vertical profiles (resampling, filtering, etc.)\n");
    printf("\n");
    printf("    Available subcommands:\n");
    printf("      resample\n");
    printf("      smooth\n");
    printf("\n");
    printf("    Type 'harpprofile <subcommand> --help' for help on a specific subcommand.\n");
    printf("\n");
    printf("  harpprofile -h, --help\n");
    printf("    Show help (this text)\n");
    printf("\n");
    printf("  harpprofile -v, --version\n");
    printf("    Print the version number of the HARP Toolset and exit\n");
    printf("\n");
}

/* Resamples all variables in product against a specified grid.
 * Target_grid is expected to be a variable of dimensions {vertical} or {time, vertical}.
 * The source grid is determined by derivation of a matching vertical quantity on the specified product.
 *
 * Grid-dimension-wise we distinguish 4 cases:
 *  - target 1D, source 1D: performs time independent vertical interpolation
 *  - target 1D, source 2D: extends target dimensions & performs time dep. interpolation
 *  - target 2D, source 1D: extends source dimensions & performs time dep. interpolation
 *  - target 2D, source 2D: performs time dependent interpolation
 */
static int resample_against_grid(harp_product *product, harp_variable* target_grid)
{
    harp_variable *source_grid = NULL;
    long target_vertical_elements = target_grid->dimension[target_grid->num_dimensions-1];
    long source_time_dim_length = 0; /* 0 indicates that we do time-independent regridding */
    long source_vertical_elements;
    int i;

    /* Derive the source grid (will give doubles because unit is passed) */
    if(harp_product_add_derived_variable(product,
                                         target_grid->name,
                                         target_grid->unit,
                                         1,
                                         (const harp_dimension_type[]) {harp_dimension_vertical}) != 0)
    {
        /* Failed to derive 1D source grid. Try 2D */
        if(harp_product_add_derived_variable(product,
                                             target_grid->name,
                                             target_grid->unit,
                                             2,
                                             (const harp_dimension_type[]) {harp_dimension_time,
                                                                            harp_dimension_vertical}) != 0)
        {
            printf("Failed to derive source grid: %s\n", harp_errno_to_string(harp_errno));
            return -1;
        }

        harp_product_get_variable_by_name(product, target_grid->name, &source_grid);
        source_time_dim_length = source_grid->dimension[0];
    }
    source_vertical_elements = source_grid->dimension[source_grid->num_dimensions-1];

    /* Resample all variables if we know how */
    for (i = product->num_variables - 1; i >= 0; i--)
    {
        harp_variable *variable = product->variable[i];

        long new_data_num_elements = variable->num_elements / source_vertical_elements * target_vertical_elements;
        double *new_data = NULL;
        long num_blocks = variable->num_elements / source_vertical_elements;
        long time_blocks = num_blocks;

        int dim_id;
        long time;
        long block_id;
        int skip; /* <0: don't skipt, 0: skip, >0: delete */

        /* Calculate the number of num_blocks for which time is constant for time-dependent resampling */
        if (source_time_dim_length != 0)
        {
            time_blocks = num_blocks / source_time_dim_length;
        }

        /* Ensure that there is only 1 vertical dimension, that it's the fastest running one and has scalar values */
        skip = 0; /* assume that the variable has no vertical component */
        for (dim_id = 0; dim_id < variable->num_dimensions; dim_id++)
        {
            if (variable->dimension_type[dim_id] == harp_dimension_vertical)
            {
                skip = -1; /* variable has vertical component */
                if (dim_id != variable->num_dimensions - 1) {
                    /* variable has vertical dimension but cannot be resampled */
                    skip = 1;
                    break;
                }
            }
        }

        /* We can't resample string-typed variables */
        if (variable->data_type == harp_type_string)
        {
            skip = 1;
        }

        /* TODO skip based on name restrictions */

        if (skip == 0)
        {
            continue;
        }
        else if (skip > 0)
        {
            printf("Removing variable %s; unresamplable dimensions\n", variable->name);
            harp_product_remove_variable(product, variable);
            continue;
        }

        /* Ensure that the variable data consists of doubles */
        if (variable->data_type != harp_type_double && harp_variable_convert_data_type(variable, harp_type_double) != 0)
        {
            printf("Cannot resample variable '%s': '%s'\n", variable->name, harp_errno_to_string(harp_errno));
            harp_variable_delete(target_grid);
            return -1;
        }

        /* time independent variables with a time-dependent source grid are time-extended */
        if (variable->dimension_type[0] != harp_dimension_time && source_grid->dimension[0] == harp_dimension_time)
        {
            harp_variable_add_dimension(variable, 0, harp_dimension_time, source_time_dim_length);
        }

        /* Setup target array */
        new_data = (double *) malloc((size_t) new_data_num_elements * sizeof(double));
        if (new_data == NULL)
        {
            harp_variable_delete(target_grid);
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)",
                           __FILE__, __LINE__);
            return -1;
        }

        /* Interpolate the data of the variable over the vertical axis */
        time = 0;
        for (block_id = 0; block_id < num_blocks; block_id++)
        {
            /* keep track of time for time-dependent vertical grids */
            if (block_id % time_blocks == 0)
            {
                time++;
            }

            harp_interpolate_array_linear(
                    source_vertical_elements,
                    source_grid->data.double_data + time * source_vertical_elements,
                    variable->data.double_data + block_id * source_vertical_elements,
                    target_vertical_elements,
                    target_grid->data.double_data,
                    0, /* TODO: out_of_bound_flag set correctly? */
                    new_data + block_id * target_vertical_elements
            );
        }

        /* Update the vertical dimension length */
        variable->dimension[variable->num_dimensions] = target_grid->num_elements;

        /* Set the new variable data */
        harp_array old_data = variable->data;
        variable->data.double_data = new_data;
        variable->num_elements = new_data_num_elements;

        /* Clean up the old data */
        free(old_data.double_data);
    }

    /* ensure consistent axis variable in product */
    harp_product_replace_variable(product, target_grid);
    product->dimension[harp_dimension_vertical] = target_vertical_elements;

    return 0;
}

static int resample_common_grid(harp_product *product, const char *grid_input_filename)
{
    harp_variable *target_grid = NULL;

    if (grid_import(grid_input_filename, &target_grid) != 0)
    {
        printf("Failed to import grid from '%s'\n", grid_input_filename);
        return -1;
    }

    /* TODO is this sensible or redundant? */
    if(harp_variable_verify(target_grid) != 0)
    {
        printf("Common vertical axis loading failed: %s\n", harp_errno_to_string(harp_errno));
        harp_variable_delete(target_grid);
        return -1;
    }

    resample_against_grid(product, target_grid);

    // cleanup
    harp_variable_delete(target_grid);

    return 0;
}

static int resample(int argc, char *argv[])
{
    harp_product *product;
    const char *output_filename = NULL;
    const char *output_format = "netcdf";
    const char *input_filename = NULL;

    /* valued option */
    const char *grid_input_filename = NULL;

    /* parse arguments after the 'action' argument */
    int i;
    for (i = 2; i < argc; i++)
    {
        if ((strcmp(argv[i], "-h") == 0 || (strcmp(argv[i], "--help") == 0)))
        {
            print_help_resample();
            harp_done();
            return 0;
        }
        else if ((strcmp(argv[i], "-of") == 0 || strcmp(argv[i], "--output-format") == 0)
                 && i + 1 < argc && argv[i + 1][0] != '-')
        {
            output_format = argv[i + 1];
            i++;
        }
        else if ((strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--common") == 0)
                 && i + 1 < argc && argv[i + 1][0] != '-')
        {
            grid_input_filename = argv[i + 1];
            i++;
        }
        else if (argv[i][0] != '-')
        {
            /* positional arguments follow */
            break;
        }
        else
        {
            fprintf(stderr, "ERROR: invalid argument: '%s'\n", argv[i]);
            print_help();
            return -1;
        }
    }

    /* positional argument parsing */
    if (i == argc - 1)
    {
        input_filename = argv[argc - 1];
        output_filename = input_filename;
    }
    else if (i == argc - 2)
    {
        input_filename = argv[argc - 2];
        output_filename = argv[argc - 1];
    }
    else
    {
        fprintf(stderr, "ERROR: input product file not specified\n");
        print_help_resample();
        return -1;
    }

    /* import the input product */
    if (harp_import(input_filename, &product) != 0)
    {
        fprintf(stderr, "ERROR: could not import product from '%s'", input_filename);
        return -1;
    }

    /* perform the resampling */
    if (grid_input_filename != NULL)
    {
        if(resample_common_grid(product, grid_input_filename) != 0)
        {
            fprintf(stderr, "ERROR: failed resampling to common grid.\n");
            return -1;
        }

        if(harp_export(output_filename, output_format, product) != 0)
        {
            fprintf(stderr, "ERROR: failed to export resampled product: %s\n", harp_errno_to_string(harp_errno));
            return -1;
        }
    }
}

static int smooth(int argc, char *argv[])
{
    
}

int main(int argc, char *argv[])
{
    if (argc == 1 || strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)
    {
        print_help();
        return 0;
    }

    if (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--version") == 0)
    {
        print_version();
        return 0;
    }

    if (argc < 2)
    {
        fprintf(stderr, "ERROR: invalid arguments\n");
        print_help();
        return 1;
    }

    if (harp_init() != 0)
    {
        fprintf(stderr, "ERROR: %s\n", harp_errno_to_string(harp_errno));
        return 1;
    }

    int result;

    /* parse actions */
    if (strcmp(argv[1], "smooth") == 0)
    {
        result = smooth(argc, argv);
    }
    else if (strcmp(argv[1], "resample") == 0)
    {
        result = resample(argc, argv);
    }

    if (result == -1)
    {
        if (harp_errno != HARP_SUCCESS)
        {
            fprintf(stderr, "ERROR: %s\n", harp_errno_to_string(harp_errno));
        }
        harp_done();
        return 1;
    }
    else if (result == -2)
    {
        harp_done();
        return 2;
    }

    harp_done();
    return 0;
}
