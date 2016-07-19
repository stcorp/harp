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

#include <assert.h>
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
    printf("harpdump version %s\n", libharp_version);
    printf("Copyright (C) 2015-2016 S[&]T, The Netherlands.\n\n");
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

static void write_variable_data(harp_variable *variable)
{
    printf("%s", variable->name);
    printf(" = ");
    write_array(variable->data, variable->data_type, variable->num_elements);
    printf("\n\n");
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

    harp_set_warning_handler(print_warning);

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

    harp_product_print(product, !list, printf);

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
