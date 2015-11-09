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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_version()
{
    printf("harpdump version %s\n", libharp_version);
    printf("Copyright (C) 2015 S[&]T, The Netherlands.\n\n");
}

static void print_help()
{
    printf("Usage:\n");
    printf("    harpdump [options] <input product file>\n");
    printf("        Print the contents of a HARP compliant netCDF/HDF4/HDF5 product.\n");
    printf("\n");
    printf("        Options:\n");
    printf("            -l, --list:\n");
    printf("                    Only show list of variables (no attributes).\n");
    printf("            -d, --data:\n");
    printf("                    Show data values for each variable.\n");
    printf("\n");
    printf("    harpdump -h, --help\n");
    printf("        Show help (this text).\n");
    printf("\n");
    printf("    harpdump -v, --version\n");
    printf("        Print the version number of HARP and exit.\n");
    printf("\n");
}

static void write_scalar(harp_scalar data, harp_data_type data_type)
{
    switch (data_type)
    {
        case harp_type_int8:
            printf("%d", (int)data.int8_data);
            break;
        case harp_type_int16:
            printf("%d", (int)data.int16_data);
            break;
        case harp_type_int32:
            printf("%ld", (long)data.int32_data);
            break;
        case harp_type_float:
            printf("%.16g", (double)data.float_data);
            break;
        case harp_type_double:
            printf("%.16g", (double)data.double_data);
            break;
        default:
            assert(0);
            exit(1);
    }
}

static void write_array(harp_array data, harp_data_type data_type, long num_elements)
{
    long i;

    for (i = 0; i < num_elements; i++)
    {
        switch (data_type)
        {
            case harp_type_int8:
                printf("%d", (int)data.int8_data[i]);
                break;
            case harp_type_int16:
                printf("%d", (int)data.int16_data[i]);
                break;
            case harp_type_int32:
                printf("%ld", (long)data.int32_data[i]);
                break;
            case harp_type_float:
                printf("%.16g", (double)data.float_data[i]);
                break;
            case harp_type_double:
                printf("%.16g", (double)data.double_data[i]);
                break;
            case harp_type_string:
                printf("\"%s\"", data.string_data[i]);
                break;
        }
        if (i < num_elements - 1)
        {
            printf(", ");
        }
    }
}

static void write_variable(harp_variable *variable, int show_attributes)
{
    int i;

    printf("    ");
    switch (variable->data_type)
    {
        case harp_type_int8:
            printf("byte");
            break;
        case harp_type_int16:
            printf("int");
            break;
        case harp_type_int32:
            printf("long");
            break;
        case harp_type_float:
            printf("float");
            break;
        case harp_type_double:
            printf("double");
            break;
        case harp_type_string:
            printf("string");
            break;
    }
    printf(" %s", variable->name);
    if (variable->num_dimensions > 0)
    {
        printf(" {");
        for (i = 0; i < variable->num_dimensions; i++)
        {
            if (variable->dimension_type[i] != harp_dimension_independent)
            {
                printf("%s = ", harp_get_dimension_type_name(variable->dimension_type[i]));
            }
            printf("%ld", variable->dimension[i]);
            if (i < variable->num_dimensions - 1)
            {
                printf(", ");
            }
        }
        printf("}");
    }
    if (variable->unit != NULL)
    {
        printf(" [%s]", variable->unit);
    }
    printf("\n");

    if (!show_attributes)
    {
        return;
    }

    if (variable->description != NULL)
    {
        printf("        description = \"%s\"\n", variable->description);
    }
    if (variable->data_type != harp_type_string)
    {
        if (!harp_is_valid_min_for_type(variable->data_type, variable->valid_min))
        {
            printf("        valid min = ");
            write_scalar(variable->valid_min, variable->data_type);
            printf("\n");
        }
        if (!harp_is_valid_max_for_type(variable->data_type, variable->valid_max))
        {
            printf("        valid max = ");
            write_scalar(variable->valid_max, variable->data_type);
            printf("\n");
        }
    }
    printf("\n");
}

static void write_variable_data(harp_variable *variable)
{
    printf("%s", variable->name);
    printf(" = ");
    write_array(variable->data, variable->data_type, variable->num_elements);
    printf("\n\n");
}

static int dump_definition(const harp_product *product, int show_attributes)
{
    int i;

    printf("dimensions:\n");
    for (i = 0; i < HARP_NUM_DIM_TYPES; i++)
    {
        if (product->dimension[i] > 0)
        {
            printf("    %s = %ld\n", harp_get_dimension_type_name(i), product->dimension[i]);
        }
    }
    printf("\n");

    printf("attributes:\n");
    if (product->source_product != NULL)
    {
        printf("    source_product = \"%s\"\n", product->source_product);
    }
    if (product->history != NULL)
    {
        printf("    history = \"%s\"\n", product->history);
    }
    printf("\n");

    printf("variables:\n");
    for (i = 0; i < product->num_variables; i++)
    {
        write_variable(product->variable[i], show_attributes);
    }
    printf("\n");

    return 0;
}

static int dump_data(const harp_product *product)
{
    int i;

    printf("data:\n");
    for (i = 0; i < product->num_variables; i++)
    {
        write_variable_data(product->variable[i]);
    }

    return 0;
}

int main(int argc, char *argv[])
{
    harp_product *product;
    int data = 0;
    int list = 0;
    int i;

    if (argc == 1 || strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)
    {
        print_help();
        exit(0);
    }

    if (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--version") == 0)
    {
        print_version();
        exit(0);
    }

    if (argc < 2)
    {
        fprintf(stderr, "ERROR: invalid arguments\n");
        print_help();
        exit(1);
    }

    /* parse argumenst */
    for (i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--list") == 0)
        {
            list = 1;
        }
        else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--data") == 0)
        {
            data = 1;
        }
        else if (argv[i][0] != '-')
        {
            /* assume all arguments from here on are files */
            break;
        }
        else
        {
            fprintf(stderr, "ERROR: invalid arguments\n");
            print_help();
            exit(1);
        }
    }

    if (i != argc - 1)
    {
        fprintf(stderr, "ERROR: invalid arguments\n");
        print_help();
        exit(1);
    }

    if (harp_init() != 0)
    {
        fprintf(stderr, "ERROR: %s\n", harp_errno_to_string(harp_errno));
        exit(1);
    }

    if (harp_import(argv[argc - 1], &product) != 0)
    {
        fprintf(stderr, "ERROR: %s\n", harp_errno_to_string(harp_errno));
        harp_done();
        exit(1);
    }
    if (dump_definition(product, !list) != 0)
    {
        fprintf(stderr, "ERROR: %s\n", harp_errno_to_string(harp_errno));
        harp_product_delete(product);
        harp_done();
        exit(1);
    }
    if (data && !list)
    {
        if (dump_data(product) != 0)
        {
            fprintf(stderr, "ERROR: %s\n", harp_errno_to_string(harp_errno));
            harp_product_delete(product);
            harp_done();
            exit(1);
        }
    }
    harp_product_delete(product);

    harp_done();

    return 0;
}
