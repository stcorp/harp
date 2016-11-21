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
    printf("            -a, --a-to-b <result_csv_file> <source_datasetdir_b> <vertical_axis>:\n");
    printf("                    resample the vertical profiles of the input file (part of\n");
    printf("                    dataset A) to the vertical grid of the vertical profiles\n");
    printf("                    in dataset B\n");
    printf("            -b, --b-to-a <result_csv_file> <source_datasetdir_a> <vertical_axis>:\n");
    printf("                    resample the vertical profiles of the input file (part of\n");
    printf("                    dataset B) to the <vertical_axis> grid of the vertical profiles\n");
    printf("                    in dataset A\n");
    printf("            -c, --common <input>\n");
    printf("                    resample vertical profiles (in datasets A and B)\n");
    printf("                    to a common grid before calculating the columns.\n");
    printf("                    The common <vertical_axis> grid is defined in file C.\n");
    printf("                    <input> denotes the filename\n");
    printf("\n");
}

void print_help_smooth(void)
{
    printf("Usage:\n");
    printf("\n");
    printf("    harpprofile smooth -h, --help\n");
    printf("        Show help for harpprofile smooth (this text)\n");
    printf("\n");
    printf("    harpprofile smooth [options] <varname> <vertical_axis> <product file> [output product file]\n");
    printf("        Smooth the vertical profile <varname> in the <product file> with averaging kernel\n");
    printf("        matrices and add a priori. Resampling is done beforehand against the specified vertical axis.\n");
    printf("\n");
    printf("        Options:\n");
    printf("            -of, --output-format <format> :\n");
    printf("                    Possible values for <format> (the output format) are:\n");
    printf("                      netcdf (the default)\n");
    printf("                      hdf4\n");
    printf("                      hdf5\n");
    printf("\n");
    printf("            One of the following:\n");
    printf("            -a, --a-with-b <result_csv_file> <source_datasetdir_b>:\n");
    printf("                    resample and smooth the vertical profiles of the input file (part of\n");
    printf("                    dataset A) with the <vertical_axis>, averaging kernel matrices and a priori\n");
    printf("                    in dataset B\n");
    printf("            -b, --b-with-a <result_csv_file> <source_datasetdir_a>:\n");
    printf("                    resample and smooth the vertical profiles of the input file (part of\n");
    printf("                    dataset B) with the <vertical_axis>, averaging kernel matrices and a priori\n");
    printf("                    in dataset A\n");
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

/**
 * Resample against grid as read from specified CSV file.
 *
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
static int resample_common_grid(harp_product *product, const char *grid_input_filename)
{
    harp_variable *target_grid = NULL;
    char operations[1024];

    sprintf(operations, "regrid(\"%s\");", grid_input_filename);

    if (harp_product_execute_operations(product, operations) != 0)
    {
        return -1;
    }

    harp_variable_delete(target_grid);

    return 0;
}

static int resample(int argc, char *argv[])
{
    harp_product *product;
    harp_collocation_result *collocation_result = NULL;

    const char *output_filename = NULL;
    const char *output_format = "netcdf";
    const char *input_filename = NULL;

    /* valued option */
    const char *grid_input_filename = NULL;

    const char *result_csv_file = NULL;
    char *vertical_axis_name = NULL;
    const char *source_dataset_a = NULL;
    const char *source_dataset_b = NULL;

    int export = 0;
    int i;

    /* parse arguments after the 'action' argument */
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
        else if ((strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "--a-to-b") == 0)
                 && i + 3 < argc && argv[i + 1][0] != '-' && argv[i + 2][0] != '-' && argv[i + 3][0] != '-')
        {
            if (source_dataset_a)
            {
                fprintf(stderr, "ERROR: you cannot specify both --b-with-a/-b and %s", argv[i]);
                return -1;
            }
            result_csv_file = argv[i + 1];
            source_dataset_b = argv[i + 2];
            vertical_axis_name = argv[i + 3];

            i += 3;
        }
        else if ((strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "--b-to-a") == 0)
                 && i + 3 < argc && argv[i + 1][0] != '-' && argv[i + 2][0] != '-' && argv[i + 3][0] != '-')
        {
            if (source_dataset_b)
            {
                fprintf(stderr, "ERROR: you cannot specify both --b-with-a/-b and %s", argv[i]);
                return -1;
            }
            result_csv_file = argv[i + 1];
            source_dataset_a = argv[i + 2];
            vertical_axis_name = argv[i + 3];

            i += 3;
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
            print_help_resample();
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
        if (resample_common_grid(product, grid_input_filename) != 0)
        {
            return -1;
        }
        export = 1;
    }

    if (result_csv_file)
    {
        if (harp_collocation_result_read(result_csv_file, &collocation_result) != 0)
        {
            return -1;
        }
    }

    if (source_dataset_b)
    {
        /* Import the column b metadata */
        if (harp_dataset_import(collocation_result->dataset_b, source_dataset_b) != 0)
        {
            harp_collocation_result_delete(collocation_result);
            return -1;
        }

        if (harp_product_regrid_vertical_with_collocated_dataset(product, vertical_axis_name, collocation_result) != 0)
        {
            harp_collocation_result_delete(collocation_result);
            return -1;
        }
        export = 1;
    }
    if (source_dataset_a)
    {
        /* Import the column a metadata */
        if (harp_dataset_import(collocation_result->dataset_a, source_dataset_a) != 0)
        {
            harp_collocation_result_delete(collocation_result);
            return -1;
        }

        harp_collocation_result_swap_datasets(collocation_result);
        if (harp_product_regrid_vertical_with_collocated_dataset(product, vertical_axis_name, collocation_result) != 0)
        {
            harp_collocation_result_delete(collocation_result);
            return -1;
        }
        export = 1;
    }

    if (export)
    {
        if (harp_product_update_history(product, "harpprofile", argc, argv) != 0)
        {
            harp_product_delete(product);
            return -1;
        }
        if (harp_export(output_filename, output_format, product) != 0)
        {
            return -1;
        }
    }

    if (collocation_result)
    {
        harp_collocation_result_delete(collocation_result);
    }
    harp_product_delete(product);

    return 0;
}

static int smooth(int argc, char *argv[])
{
    const char *output_filename = NULL;
    const char *output_format = "netcdf";
    const char *input_filename = NULL;

    const char *result_csv_file = NULL;
    char *vertical_axis_name = NULL;
    const char *source_dataset_a = NULL;
    const char *source_dataset_b = NULL;
    const char *smooth_vars[1] = { NULL };

    harp_product *product;
    harp_collocation_result *collocation_result = NULL;

    int export = 0;

    /* parse arguments after the 'action' argument */
    int i;

    for (i = 2; i < argc; i++)
    {
        if ((strcmp(argv[i], "-h") == 0 || (strcmp(argv[i], "--help") == 0)))
        {
            print_help_smooth();
            harp_done();
            return 0;
        }
        else if ((strcmp(argv[i], "-of") == 0 || strcmp(argv[i], "--output-format") == 0)
                 && i + 1 < argc && argv[i + 1][0] != '-')
        {
            output_format = argv[i + 1];
            i++;
        }
        else if ((strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "--a-with-b") == 0)
                 && i + 2 < argc && argv[i + 1][0] != '-' && argv[i + 2][0] != '-')
        {
            if (source_dataset_a)
            {
                fprintf(stderr, "ERROR: you cannot specify both --b-with-a/-b and %s", argv[i]);
                return -1;
            }
            result_csv_file = argv[i + 1];
            source_dataset_b = argv[i + 2];

            i += 2;
        }
        else if ((strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "--b-with-a") == 0)
                 && i + 2 < argc && argv[i + 1][0] != '-' && argv[i + 2][0] != '-')
        {
            if (source_dataset_b)
            {
                fprintf(stderr, "ERROR: you cannot specify both --a-with-b/-a and %s", argv[i]);
                return -1;
            }
            result_csv_file = argv[i + 1];
            source_dataset_a = argv[i + 2];

            i += 2;
        }
        else if (argv[i][0] != '-')
        {
            /* positional arguments follow */
            break;
        }
        else
        {
            fprintf(stderr, "ERROR: invalid argument: '%s'\n", argv[i]);
            print_help_smooth();
            return -1;
        }
    }

    /* positional argument parsing */
    smooth_vars[0] = argv[i++];
    vertical_axis_name = argv[i++];

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
        print_help_smooth();
        return -1;
    }

    /* import the input product */
    if (harp_import(input_filename, &product) != 0)
    {
        fprintf(stderr, "ERROR: could not import product from '%s'", input_filename);
        return -1;
    }

    if (result_csv_file)
    {
        if (harp_collocation_result_read(result_csv_file, &collocation_result) != 0)
        {
            return -1;
        }
    }

    if (source_dataset_b)
    {
        /* Import the column b metadata */
        if (harp_dataset_import(collocation_result->dataset_b, source_dataset_b) != 0)
        {
            harp_collocation_result_delete(collocation_result);
            return -1;
        }

        /* smooth the source product (from dataset a) against the avks in dataset b */
        if (harp_product_smooth_vertical(product, 1, smooth_vars, vertical_axis_name, collocation_result) != 0)
        {
            return -1;
        }
        export = 1;
    }
    if (source_dataset_a)
    {
        /* Import the column a metadata */
        if (harp_dataset_import(collocation_result->dataset_a, source_dataset_a) != 0)
        {
            harp_collocation_result_delete(collocation_result);
            return -1;
        }

        /* smooth the source product (from dataset b) against the avks in dataset a */
        harp_collocation_result_swap_datasets(collocation_result);
        if (harp_product_smooth_vertical(product, 1, smooth_vars, vertical_axis_name, collocation_result) != 0)
        {
            return -1;
        }
        export = 1;
    }


    if (export)
    {
        if (harp_product_update_history(product, "harpprofile", argc, argv) != 0)
        {
            harp_product_delete(product);
            return -1;
        }
        if (harp_export(output_filename, output_format, product) != 0)
        {
            return -1;
        }
    }

    if (collocation_result)
    {
        harp_collocation_result_delete(collocation_result);
    }
    harp_product_delete(product);

    return 0;
}

int main(int argc, char *argv[])
{
    int result = 0;

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

    /* parse actions */
    if (strcmp(argv[1], "smooth") == 0)
    {
        result = smooth(argc, argv);
    }
    else if (strcmp(argv[1], "resample") == 0)
    {
        result = resample(argc, argv);
    }
    else
    {
        fprintf(stderr, "ERROR: invalid command '%s'\n", argv[1]);
        harp_done();
        return 1;
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
