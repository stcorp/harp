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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int matchup(int argc, char *argv[]);
int resample(int argc, char *argv[]);
int update(int argc, char *argv[]);

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
    printf("harpcollocate version %s\n", libharp_version);
    printf("Copyright (C) 2015-2017 S[&]T, The Netherlands.\n");
}

static void print_help(void)
{
    printf("Usage:\n");
    printf("    harpcollocate [options] <path_a> <path_b> <outputpath>\n");
    printf("        Find matching sample pairs between two datasets of HARP files.\n");
    printf("        The path for a dataset can be either a single file or a directory\n");
    printf("        containing files. The result will be write as a comma separate value\n");
    printf("        (csv) file to the provided output path\n");
    printf("\n");
    printf("        Options:\n");
    printf("            -d '<diffvariable> <value> [unit]'\n");
    printf("                Specifies a collocation criterium.\n");
    printf("                Only include pairs where the absolute difference between the\n");
    printf("                values of the given variable for dataset A and B are\n");
    printf("                less/equal than the given value.\n");
    printf("                There is a special variable name 'point_distance' to indicate\n");
    printf("                the earth surface distance between lat/lon points of A and B.\n");
    printf("                Examples:\n");
    printf("                    -d 'datetime 3 [h]'\n");
    printf("                    -d 'point_distance 10 [km]'\n");
    printf("                Criteria on azimuth angles, longitude, and wind direction\n");
    printf("                will be automatically mapped to [0..180] degrees.\n");
    printf("            --area-intersects\n");
    printf("                Specifies that latitude/longitude polygon areas of A and B\n");
    printf("                must overlap\n");
    printf("            --point-in-area-xy\n");
    printf("                Specifies that latitude/longitude points from dataset A must\n");
    printf("                fall in polygon areas of dataset B\n");
    printf("            --point-in-area-yx\n");
    printf("                Specifies that latitude/longitude points from dataset B must\n");
    printf("                fall in polygon areas of dataset A\n");
    printf("            -nx <diffvariable>\n");
    printf("                Filter collocation pairs such that for each sample from\n");
    printf("                dataset A only the nearest sample from dataset B (using the\n");
    printf("                given variable as difference) is kept\n");
    printf("            -ny <diffvariable>\n");
    printf("                Filter collocation pairs such that for each sample from\n");
    printf("                dataset B only the neareset sample from dataset A is kept.\n");
    printf("        The order in which -nx and -ny are provided determines the order in\n");
    printf("        which the nearest filters are executed.\n");
    printf("        When '[unit]' is not specified, the unit of the variable of the\n");
    printf("        first file from dataset A will be used.\n");
    printf("\n");
    printf("    harpcollocate --resample [options] <inputpath> [<outputpath>]\n");
    printf("        Filter an existing collocation result file by selecting only nearest\n");
    printf("        samples.\n");
    printf("\n");
    printf("        Options:\n");
    printf("            -nx <diffvariable>\n");
    printf("                Filter collocation pairs such that for each sample from\n");
    printf("                dataset A only the nearest sample from dataset B (using the\n");
    printf("                given variable as difference) is kept\n");
    printf("            -ny <diffvariable>\n");
    printf("                Filter collocation pairs such that for each sample from\n");
    printf("                dataset B only the neareset sample from dataset A is kept.\n");
    printf("        The order in which -nx and -ny are provided determines the order in\n");
    printf("        which the nearest filters are executed.\n");
    printf("\n");
    printf("    harpcollocate --update <inputpath> <path_a> <path_b> [<outputpath>]\n");
    printf("        Update an existing collocation result file by checking the\n");
    printf("        measurements in two sets of HARP files and only keeping pairs\n");
    printf("        for which measurements still exist\n");
    printf("\n");
    printf("    harpcollocate -h, --help\n");
    printf("        Show help (this text).\n");
    printf("\n");
    printf("    harpcollocate -v, --version\n");
    printf("        Print the version number of HARP and exit.\n");
    printf("\n");
}

int main(int argc, char *argv[])
{
    int result;

    if (argc == 1 || (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0))
    {
        print_help();
        exit(0);
    }

    if (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--version") == 0)
    {
        print_version();
        exit(0);
    }

    if (harp_set_coda_definition_path_conditional(argv[0], NULL, "../share/coda/definitions") != 0)
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

    if (strcmp(argv[1], "--resample") == 0)
    {
        result = resample(argc, argv);
    }
    else if (strcmp(argv[1], "--update") == 0)
    {
        result = update(argc, argv);
    }
    else
    {
        result = matchup(argc, argv);
    }
    if (result == -1)
    {
        if (harp_errno != HARP_SUCCESS)
        {
            fprintf(stderr, "ERROR: %s\n", harp_errno_to_string(harp_errno));
        }
        harp_done();
        exit(1);
    }
    else if (result == 1)
    {
        fprintf(stderr, "ERROR: invalid arguments\n");
        print_help();
        harp_done();
        exit(1);
    }

    harp_done();

    return 0;
}
