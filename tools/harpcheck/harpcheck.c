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

#include "harp.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_version()
{
    printf("harpcheck version %s\n", libharp_version);
    printf("Copyright (C) 2015 S[&]T, The Netherlands.\n\n");
}

static void print_help()
{
    printf("Usage:\n");
    printf("    harpcheck <input product file> [input product file...]\n");
    printf("        Verify that the input products are HARP compliant netCDF/HDF4/HDF5\n");
    printf("        products.\n");
    printf("\n");
    printf("    harpcheck -h, --help\n");
    printf("        Show help (this text).\n");
    printf("\n");
    printf("    harpcheck -v, --version\n");
    printf("        Print the version number of HARP and exit.\n");
    printf("\n");
}

int main(int argc, char *argv[])
{
    int i;
    int k;

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

    /* parse argumenst */
    for (i = 1; i < argc; i++)
    {
        if (argv[i][0] != '-')
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

    if (harp_init() != 0)
    {
        fprintf(stderr, "ERROR: %s\n", harp_errno_to_string(harp_errno));
        exit(1);
    }

    for (k = i; k < argc; k++)
    {
        const char *filename = argv[k];
        harp_product *product;

        printf("%s\n", filename);
        if (harp_import(filename, &product) != 0)
        {
            fprintf(stderr, "ERROR: %s\n", harp_errno_to_string(harp_errno));
        }
        harp_product_delete(product);
        printf("\n");
    }

    harp_done();
    return 0;
}
