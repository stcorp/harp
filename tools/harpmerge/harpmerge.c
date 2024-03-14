/*
 * Copyright (C) 2015-2024 S[&]T, The Netherlands.
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

static void print_version(void)
{
    printf("harpmerge version %s\n", libharp_version);
    printf("Copyright (C) 2015-2024 S[&]T, The Netherlands.\n\n");
}

static void print_help(void)
{
    printf("Usage:\n");
    printf("    harpmerge [options] <file|dir> [<file|dir> ...] <output product file>\n");
    printf("        Concatenate all products as specified by the file and directory paths\n");
    printf("        into a single product.\n");
    printf("        If a directory is specified then all files (recursively) from that\n");
    printf("        directory are included.\n");
    printf("        If a file is a .pth file then the file paths from that text file\n");
    printf("        (one per line) are included. These file paths can be absolute or\n");
    printf("        relative and can point to files, directories, or other .pth files.\n");
    printf("\n");
    printf("        Options:\n");
    printf("            -a, --operations <operation list>\n");
    printf("                List of operations to apply to each product.\n");
    printf("                An operation list needs to be provided as a single expression.\n");
    printf("                See the 'operations' section of the HARP documentation for\n");
    printf("                more details.\n");
    printf("                Operations will be performed before a product is appended.\n");
    printf("\n");
    printf("            -ar, --reduce-operations <operation list>\n");
    printf("                List of operations to apply after each append.\n");
    printf("                This advanced option allows for memory efficient application\n");
    printf("                of time reduction operations (such as bin()) that would\n");
    printf("                normally be provided as part of the post operations.\n");
    printf("\n");
    printf("            -ap, --post-operations <operation list>\n");
    printf("                List of operations to apply to the merged product.\n");
    printf("                An operation list needs to be provided as a single expression.\n");
    printf("                See the 'operations' section of the HARP documentation for\n");
    printf("                more details.\n");
    printf("\n");
    printf("            -o, --options <option list>\n");
    printf("                List of options to pass to the ingestion module.\n");
    printf("                Only applicable if an input product is not in HARP format.\n");
    printf("                Options are separated by semi-colons. Each option consists\n");
    printf("                of an <option name>=<value> pair. An option list needs to be\n");
    printf("                provided as a single expression.\n");
    printf("\n");
    printf("            -l, --list\n");
    printf("                Print to stdout each filename that is currently being merged.\n");
    printf("\n");
    printf("            -f, --format <format>\n");
    printf("                Output format:\n");
    printf("                    netcdf (default)\n");
    printf("                    hdf4\n");
    printf("                    hdf5\n");
    printf("\n");
    printf("            --hdf5-compression <level>\n");
    printf("                Set data compression level for storing in HDF5 format.\n");
    printf("                0=disabled, 1=low, ..., 9=high.\n");
    printf("\n");
    printf("            --no-history\n");
    printf("                Do not update the global history attribute.\n");
    printf("\n");
    printf("        If the merged product is empty, a warning will be printed and the\n");
    printf("        tool will return with exit code 2 (without writing a file).\n");
    printf("\n");
    printf("    harpmerge -h, --help\n");
    printf("        Show help (this text).\n");
    printf("\n");
    printf("    harpmerge -v, --version\n");
    printf("        Print the version number of HARP and exit.\n");
    printf("\n");
}

int merge_dataset(harp_product **merged_product, harp_dataset *dataset, const char *operations, const char *options,
                  const char *reduce_operations, int verbose)
{
    int i;

    for (i = 0; i < dataset->num_products; i++)
    {
        harp_product *product;
        int index;

        /* add products in sorted order (sorted by source_product value) */
        index = dataset->sorted_index[i];

        if (verbose)
        {
            printf("%s\n", dataset->metadata[index]->filename);
        }
        if (harp_import(dataset->metadata[index]->filename, operations, options, &product) != 0)
        {
            harp_add_error_message(" (while merging '%s')", dataset->metadata[index]->filename);
            return -1;
        }
        if (!harp_product_is_empty(product))
        {
            if (*merged_product == NULL)
            {
                *merged_product = product;
                /* if this remains the only product then make sure it still looks like it was the result of a merge */
                if (harp_product_append(*merged_product, NULL) != 0)
                {
                    harp_add_error_message(" (while merging '%s')", dataset->metadata[index]->filename);
                    return -1;
                }
            }
            else
            {
                if (harp_product_append(*merged_product, product) != 0)
                {
                    harp_add_error_message(" (while merging '%s')", dataset->metadata[index]->filename);
                    harp_product_delete(product);
                    return -1;
                }
                harp_product_delete(product);
            }
            if (reduce_operations != NULL)
            {
                /* perform reduction operations on the partially merged product after each append */
                if (harp_product_execute_operations(*merged_product, reduce_operations) != 0)
                {
                    return -1;
                }
            }
        }
    }

    return 0;
}

static int merge(int argc, char *argv[])
{
    harp_product *merged_product = NULL;
    const char *operations = NULL;
    const char *reduce_operations = NULL;
    const char *post_operations = NULL;
    const char *options = NULL;
    const char *output_filename = NULL;
    const char *output_format = "netcdf";
    int update_history = 1;
    int verbose = 0;
    int i;

    /* parse arguments after list/'export format' */
    for (i = 1; i < argc; i++)
    {
        if ((strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "--operations") == 0) && i + 1 < argc &&
            argv[i + 1][0] != '-')
        {
            operations = argv[i + 1];
            i++;
        }
        else if ((strcmp(argv[i], "-ar") == 0 || strcmp(argv[i], "--reduce-operations") == 0) && i + 1 < argc &&
                 argv[i + 1][0] != '-')
        {
            reduce_operations = argv[i + 1];
            i++;
        }
        else if ((strcmp(argv[i], "-ap") == 0 || strcmp(argv[i], "--post-operations") == 0) && i + 1 < argc &&
                 argv[i + 1][0] != '-')
        {
            post_operations = argv[i + 1];
            i++;
        }
        else if ((strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--options") == 0) && i + 1 < argc &&
                 argv[i + 1][0] != '-')
        {
            options = argv[i + 1];
            i++;
        }
        else if ((strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--format") == 0) && i + 1 < argc
                 && argv[i + 1][0] != '-')
        {
            output_format = argv[i + 1];
            i++;
        }
        else if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--list") == 0)
        {
            verbose = 1;
        }
        else if (strcmp(argv[i], "--hdf5-compression") == 0 && i + 1 < argc && argv[i + 1][0] != '-')
        {
            if (harp_set_option_hdf5_compression(atoi(argv[i + 1])) != 0)
            {
                fprintf(stderr, "ERROR: invalid hdf5 compression argument: '%s'\n", argv[i]);
                print_help();
                return -1;
            }
            i++;
        }
        else if (strcmp(argv[i], "--no-history") == 0)
        {
            update_history = 0;
        }
        else if (argv[i][0] != '-')
        {
            /* Assume the next argument is the dataset directory path. */
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
        fprintf(stderr, "ERROR: invalid arguments\n");
        print_help();
        return -1;
    }
    output_filename = argv[argc - 1];

    while (i < argc - 1)
    {
        harp_dataset *dataset;

        if (harp_dataset_new(&dataset) != 0)
        {
            return -1;
        }
        if (harp_dataset_import(dataset, argv[i], options) != 0)
        {
            harp_dataset_delete(dataset);
            return -1;
        }
        if (harp_dataset_prefilter(dataset, operations) != 0)
        {
            harp_dataset_delete(dataset);
            return -1;
        }
        if (merge_dataset(&merged_product, dataset, operations, options, reduce_operations, verbose) != 0)
        {
            harp_product_delete(merged_product);
            harp_dataset_delete(dataset);
            return -1;
        }
        harp_dataset_delete(dataset);
        i++;
    }

    if (merged_product == NULL)
    {
        return -2;
    }
    if (harp_product_is_empty(merged_product))
    {
        harp_product_delete(merged_product);
        return -2;
    }
    if (post_operations != NULL)
    {
        if (harp_product_execute_operations(merged_product, post_operations) != 0)
        {
            harp_product_delete(merged_product);
            return -1;
        }
        if (harp_product_is_empty(merged_product))
        {
            harp_product_delete(merged_product);
            return -2;
        }
    }

    if (update_history)
    {
        /* Update the product history */
        if (harp_product_update_history(merged_product, "harpmerge", argc, argv) != 0)
        {
            harp_product_delete(merged_product);
            return -1;
        }
    }

    /* export the product */
    if (harp_export(output_filename, output_format, merged_product) != 0)
    {
        harp_product_delete(merged_product);
        return -1;
    }

    harp_product_delete(merged_product);
    return 0;
}

int main(int argc, char *argv[])
{
    int result;

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

    if (harp_set_coda_definition_path_conditional(argv[0], NULL, "../share/coda/definitions") != 0)
    {
        fprintf(stderr, "ERROR: %s\n", harp_errno_to_string(harp_errno));
        exit(1);
    }
    if (harp_set_udunits2_xml_path_conditional(argv[0], NULL, "../share/harp/udunits2.xml") != 0)
    {
        fprintf(stderr, "ERROR: %s\n", harp_errno_to_string(harp_errno));
        exit(1);
    }

    harp_set_warning_handler(print_warning);

    if (harp_init() != 0)
    {
        fprintf(stderr, "ERROR: %s\n", harp_errno_to_string(harp_errno));
        exit(1);
    }

    result = merge(argc, argv);

    if (result == -1)
    {
        if (harp_errno != HARP_SUCCESS)
        {
            fprintf(stderr, "ERROR: %s\n", harp_errno_to_string(harp_errno));
        }
        harp_done();
        exit(1);
    }
    else if (result == -2)
    {
        harp_report_warning("merged product is empty");
        harp_done();
        exit(2);
    }

    harp_done();
    return 0;
}
