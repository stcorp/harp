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
    printf("Copyright (C) 2015-2017 S[&]T, The Netherlands.\n\n");
}

static void print_help()
{
    printf("Usage:\n");
    printf("    harpdump [options] <input product file>\n");
    printf("        Print the contents of a HARP imported product.\n");
    printf("\n");
    printf("        Options:\n");
    printf("            -a, --operations <operation list>\n");
    printf("                List of operations to apply to the product before printing.\n");
    printf("                An operation list needs to be provided as a single expression.\n");
    printf("                See the 'operations' section of the HARP documentation for\n");
    printf("                more details.\n");
    printf("\n");
    printf("            -o, --options <option list>\n");
    printf("                List of options to pass to the ingestion module.\n");
    printf("                Only applicable of the input product is not in HARP format.\n");
    printf("                Options are separated by semi-colons. Each option consists\n");
    printf("                of an <option name>=<value> pair. An option list needs to be\n");
    printf("                provided as a single expression.\n");
    printf("\n");
    printf("            -l, --list\n");
    printf("                Only show list of variables (no attributes).\n");
    printf("\n");
    printf("            -d, --data\n");
    printf("                Show data values for each variable.\n");
    printf("\n");
    printf("    harpdump --list-derivations [options] [input product file]\n");
    printf("        List all available variable conversions. If an input product file is\n");
    printf("        specified, limit the list to variable conversions that are possible\n");
    printf("        given the specified product.\n");
    printf("\n");
    printf("        Options:\n");
    printf("            -a, --operations <operation list>\n");
    printf("                List of operations to apply to the product before determining\n");
    printf("                the possible derivations.\n");
    printf("                An operation list needs to be provided as a single expression.\n");
    printf("                See the 'operations' section of the HARP documentation for\n");
    printf("                more details.\n");
    printf("\n");
    printf("            -o, --options <option list>\n");
    printf("                List of options to pass to the ingestion module.\n");
    printf("                Only applicable of the input product is not in HARP format.\n");
    printf("                Options are separated by semi-colons. Each option consists\n");
    printf("                of an <option name>=<value> pair. An option list needs to be\n");
    printf("                provided as a single expression.\n");
    printf("\n");
    printf("    harpdump -h, --help\n");
    printf("        Show help (this text).\n");
    printf("\n");
    printf("    harpdump -v, --version\n");
    printf("        Print the version number of HARP and exit.\n");
    printf("\n");
}

static int list_derivations(int argc, char *argv[])
{
    const char *operations = NULL;
    const char *options = NULL;
    harp_product *product = NULL;
    const char *input_filename = NULL;
    int i;

    if (argc == 2)
    {
        return harp_doc_list_conversions(NULL, printf);
    }

    for (i = 2; i < argc; i++)
    {
        if ((strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "--operations") == 0) && i + 1 < argc &&
            argv[i + 1][0] != '-')
        {
            operations = argv[i + 1];
            i++;
        }
        else if ((strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--options") == 0) && i + 1 < argc &&
                 argv[i + 1][0] != '-')
        {
            options = argv[i + 1];
            i++;
        }
        else if (argv[i][0] != '-' && i == argc - 1)
        {
            input_filename = argv[i];
        }
        else
        {
            fprintf(stderr, "ERROR: invalid arguments\n");
            print_help();
            exit(1);
        }
    }

    if (input_filename == NULL)
    {
        fprintf(stderr, "ERROR: input product file not specified\n");
        print_help();
        return -1;
    }

    /* Import the product */
    if (harp_import(input_filename, operations, options, &product) != 0)
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

static int dump(int argc, char *argv[])
{
    const char *operations = NULL;
    const char *options = NULL;
    harp_product *product;
    int data = 0;
    int list = 0;
    int i;

    /* parse argumenst */
    for (i = 1; i < argc; i++)
    {
        if ((strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "--operations") == 0) && i + 1 < argc &&
            argv[i + 1][0] != '-')
        {
            operations = argv[i + 1];
            i++;
        }
        else if ((strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--options") == 0) && i + 1 < argc &&
                 argv[i + 1][0] != '-')
        {
            options = argv[i + 1];
            i++;
        }
        else if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--list") == 0)
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
            return -1;
        }
    }

    if (i != argc - 1)
    {
        fprintf(stderr, "ERROR: invalid arguments\n");
        print_help();
        return -1;
    }

    if (harp_import(argv[argc - 1], operations, options, &product) != 0)
    {
        return -1;
    }

    harp_product_print(product, !list, data && !list, printf);

    harp_product_delete(product);

    return 0;
}

int main(int argc, char *argv[])
{
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

    if (strcmp(argv[1], "--list-derivations") == 0)
    {
        if (list_derivations(argc, argv) != 0)
        {
            fprintf(stderr, "ERROR: %s\n", harp_errno_to_string(harp_errno));
            harp_done();
            exit(1);
        }
    }
    else
    {
        if (dump(argc, argv) != 0)
        {
            if (harp_errno != HARP_SUCCESS)
            {
                fprintf(stderr, "ERROR: %s\n", harp_errno_to_string(harp_errno));
            }
            harp_done();
            exit(1);
        }
    }

    harp_done();

    return 0;
}
