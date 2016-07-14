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

static int print_warning(const char *message, va_list ap)
{
    int result;

    fprintf(stderr, "WARNING: ");
    result = vfprintf(stderr, message, ap);
    fprintf(stderr, "\n");

    return result;
}

static void print_version()
{
    printf("harpfilter version %s\n", libharp_version);
    printf("Copyright (C) 2015-2016 S[&]T, The Netherlands.\n\n");
}

static void print_help()
{
    printf("Usage:\n");
    printf("    harpfilter [options] <input product file> [output product file]\n");
    printf("        Filter a HARP compliant netCDF/HDF4/HDF5 product.\n");
    printf("\n");

    printf("        Options:\n");
    printf("            -a, --actions <action list>\n");
    printf("                List of actions to apply to the product.\n");
    printf("                An action list needs to be provided as a single expression.\n");
    printf("\n");
    printf("            -f, --format <format>\n");
    printf("                Output format:\n");
    printf("                    netcdf (default)\n");
    printf("                    hdf4\n");
    printf("                    hdf5\n");
    printf("\n");
    printf("        Action list:\n");
    printf("            Actions are separated by semi-colons. Each action is either\n");
    printf("            a comparison filter, a membership test filter, or a function\n");
    printf("            call. Strings used in actions should be quoted with double\n");
    printf("            quotes.\n");
    printf("\n");
    printf("            Comparison filter:\n");
    printf("                variable operator value [unit]\n");
    printf("                    Exclude measurements for which the comparison evaluates\n");
    printf("                    to false.\n");
    printf("\n");
    printf("                Supported operators:\n");
    printf("                    == !=\n");
    printf("                    < <= >= > (for numerical variables only)\n");
    printf("\n");
    printf("                If a unit is specified, the comparison will be performed in\n");
    printf("                the specified unit. Otherwise, it will be performed in the\n");
    printf("                unit of the variable. Units can only be specified for\n");
    printf("                numerical variables.\n");
    printf("\n");
    printf("            Membership test filter:\n");
    printf("                variable in (value, ...) [unit]\n");
    printf("                variable not in (value, ...) [unit]\n");
    printf("                    Exclude measurements that do not occur in the specified\n");
    printf("                    list.\n");
    printf("\n");
    printf("                If a unit is specified, the comparison will be performed in\n");
    printf("                the specified unit. Otherwise, it will be performed in the\n");
    printf("                unit of the variable. Units can only be specified for\n");
    printf("                numerical variables.\n");
    printf("\n");
    printf("            Function call:\n");
    printf("                function(argument, ...)\n");
    printf("\n");
    printf("            Supported functions:\n");
    printf("                collocate-left(collocation-result-file)\n");
    printf("                    Apply the specified collocation result file as an index\n");
    printf("                    filter assuming the product is part of dataset A.\n");
    printf("\n");
    printf("                collocate-right(collocation-result-file)\n");
    printf("                    Apply the specified collocation result file as an index\n");
    printf("                    filter assuming the product is part of dataset B.\n");
    printf("\n");
    printf("                valid(variable)\n");
    printf("                    Exclude invalid values of the specified variable (values\n");
    printf("                    outside the valid range of the variable, or NaN).\n");
    printf("\n");
    printf("                longitude-range(minimum [unit], maximum [unit])\n");
    printf("                    Exclude measurements of which the longitude of the\n");
    printf("                    measurement location falls outside the specified range.\n");
    printf("                    This function correctly handles longitude ranges that\n");
    printf("                    cross the international date line.\n");
    printf("\n");
    printf("                point-distance(longitude [unit], latitude [unit],\n");
    printf("                               distance [unit])\n");
    printf("                    Exclude measurements situated further than the specified\n");
    printf("                    distance from the specified location.\n");
    printf("\n");
    printf("                area-mask-covers-point(area-mask-file)\n");
    printf("                    Exclude measurements for which no area from the area\n");
    printf("                    mask file contains the measurement location.\n");
    printf("\n");
    printf("                area-mask-covers-area(area-mask-file)\n");
    printf("                    Exclude measurements for which no area from the area\n");
    printf("                    mask file covers the measurement area completely.\n");
    printf("\n");
    printf("                area-mask-intersects-area(area-mask-file,\n");
    printf("                                          minimum-overlap-percentage)\n");
    printf("                    Exclude measurements for which no area from the area\n");
    printf("                    mask file overlaps at least the specified percentage of\n");
    printf("                    the measurement area.\n");
    printf("\n");
    printf("                derive(variable {dimension-type, ...} [unit])\n");
    printf("                    Derive the specified variable from other variables found\n");
    printf("                    in the product. The --list-conversions option of\n");
    printf("                    harpfilter can be used to list available variable\n");
    printf("                    conversions.\n");
    printf("\n");
    printf("                include(variable, ...)\n");
    printf("                    Mark the specified variable(s) for inclusion in the\n");
    printf("                    filtered product. All variables marked for inclusion\n");
    printf("                    will be included in the filtered product, all other\n");
    printf("                    variables will be excluded. By default, all variables\n");
    printf("                    will be included.\n");
    printf("\n");
    printf("                exclude(variable, ...)\n");
    printf("                    Mark the specified variable(s) for exclusion from the\n");
    printf("                    filtered product. All variables marked for exclusion\n");
    printf("                    will be excluded from the filtered product, all other\n");
    printf("                    variables will be included. Variable exclusions will be\n");
    printf("                    evaluated after evaluating all variable inclusions (if\n");
    printf("                    any).\n");
    printf("\n");
    printf("                The unit qualifier is optional for all function arguments\n");
    printf("                that support it. If a unit is not specified, the unit of the\n");
    printf("                corresponding variable will be used.\n");
    printf("\n");
    printf("            Examples:\n");
    printf("                -a 'derive(altitude {time} [km]); pressure > 3.0 [bar];'\n");
    printf("                -a 'point-distance(-52.5 [degree], 1.0 [rad], 1e3 [km])'\n");
    printf("                -a 'index in (0, 10, 20, 30, 40); valid(pressure)'\n");
    printf("\n");
    printf("        If the filtered product is empty, a warning will be printed and the\n");
    printf("        tool will return with exit code 2 (without writing a file).\n");
    printf("\n");
    printf("    harpfilter --list-conversions [input product file]\n");
    printf("        List all available variable conversions. If an input product file is\n");
    printf("        specified, limit the list to variable conversions that are possible\n");
    printf("        given the specified product.\n");
    printf("\n");
    printf("    harpfilter -h, --help\n");
    printf("        Show help (this text).\n");
    printf("\n");
    printf("    harpfilter -v, --version\n");
    printf("        Print the version number of HARP and exit.\n");
    printf("\n");
}

static int list_conversions(int argc, char *argv[])
{
    harp_product *product = NULL;
    const char *input_filename = NULL;

    if (argc == 2)
    {
        return harp_doc_list_conversions(NULL, printf);
    }

    if (argc != 3)
    {
        fprintf(stderr, "ERROR: invalid arguments\n");
        print_help();
        exit(1);
    }

    input_filename = argv[argc - 1];

    /* Import the product */
    if (harp_import(input_filename, &product) != 0)
    {
        return -1;
    }

    /* List possible conversions */
    if (harp_doc_list_conversions(product, printf) != 0)
    {
        harp_product_delete(product);
        return -1;
    }

    harp_product_delete(product);
    return 0;
}

static int filter(int argc, char *argv[])
{
    harp_product *product;
    const char *actions = NULL;
    const char *output_filename = NULL;
    const char *output_format = "netcdf";
    const char *input_filename = NULL;
    int i;

    /* Parse arguments after list/'export format' */
    for (i = 1; i < argc; i++)
    {
        if ((strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "--actions") == 0) && i + 1 < argc && argv[i + 1][0] != '-')
        {
            actions = argv[i + 1];
            i++;
        }
        else if ((strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--format") == 0) && i + 1 < argc
                 && argv[i + 1][0] != '-')
        {
            output_format = argv[i + 1];
            i++;
        }
        else if (argv[i][0] != '-')
        {
            /* Assume the next argument is an input file. */
            break;
        }
        else
        {
            fprintf(stderr, "ERROR: invalid argument: '%s'\n", argv[i]);
            print_help();
            return -1;
        }
    }

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
        print_help();
        return -1;
    }

    if (harp_import(input_filename, &product) != 0)
    {
        return -1;
    }

    if (actions != NULL)
    {
        if (harp_product_execute_actions(product, actions) != 0)
        {
            harp_product_delete(product);
            return -1;
        }
    }

    if (harp_product_is_empty(product))
    {
        fprintf(stderr, "WARNING: filtered product is empty\n");
        harp_product_delete(product);
        return -2;
    }
    else
    {
        /* Update the product */
        if (harp_product_update_history(product, "harpfilter", argc, argv) != 0)
        {
            harp_product_delete(product);
            return -1;
        }

        /* Export the product */
        if (harp_export(output_filename, output_format, product) != 0)
        {
            harp_product_delete(product);
            return -1;
        }
    }

    harp_product_delete(product);
    return 0;
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

    harp_set_warning_handler(print_warning);

    if (harp_init() != 0)
    {
        fprintf(stderr, "ERROR: %s\n", harp_errno_to_string(harp_errno));
        return 1;
    }

    if (strcmp(argv[1], "--list-conversions") == 0)
    {
        if (list_conversions(argc, argv) != 0)
        {
            fprintf(stderr, "ERROR: %s\n", harp_errno_to_string(harp_errno));
            harp_done();
            return 1;
        }
    }
    else
    {
        int result;

        result = filter(argc, argv);

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
            fprintf(stderr, "WARNING: filtered product is empty\n");
            harp_done();
            return 2;
        }
    }

    harp_done();
    return 0;
}
