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

#include "harpcollocate.h"

void print_version(void)
{
    printf("harpcollocate version %s\n", libharp_version);
    printf("Copyright (C) 2015 S[&]T, The Netherlands.\n");
}

static void print_help_collocation_options(void)
{
    printf("      Collocation options,\n");
    printf("      set at least one of the following ([unit] is optional):\n");
    printf("      -dt 'value [unit]'          : sets maximum allowed difference in time\n");
    printf("      -dp 'value [unit]'          : sets maximum allowed point distance\n");
    printf("      -dlat 'value [unit]'        : sets maximum allowed point difference\n");
    printf("                                    in latitude\n");
    printf("      -dlon 'value [unit]'        : sets maximum allowed point difference\n");
    printf("                                    in longitude\n");
    printf("      -da 'value [unit]'          : sets minimum allowed overlapping\n");
    printf("                                    percentage of polygon areas\n");
    printf("      -dsza 'value [unit]'        : sets allowed maximum difference\n");
    printf("                                    in solar zenith angle\n");
    printf("      -dsaa 'value [unit]'        : sets allowed maximum difference\n");
    printf("                                    in solar azimuth angle\n");
    printf("      -dvza 'value [unit]'        : sets allowed maximum difference\n");
    printf("                                    in viewing zenith angle\n");
    printf("      -dvaa 'value [unit]'        : sets allowed maximum difference\n");
    printf("                                    in viewing azimuth angle\n");
    printf("      -dtheta 'value [unit]'      : sets allowed maximum difference\n");
    printf("                                    in scattering angle\n");
    printf("      -overlap                    : sets that polygon areas must overlap\n");
    printf("      -painab                     : sets that points of dataset A must fall\n");
    printf("                                    in polygon areas of B\n");
    printf("      -pbinaa                     : sets that points of dataset B must fall\n");
    printf("                                    in polygon areas of A\n");
    printf("      When '[unit]' is not specified, a default unit is used:\n");
    printf("        Criteria; [default unit]\n");
    printf("        -dt; [%s]\n", HARP_UNIT_TIME);
    printf("        -dp; [%s]\n", HARP_UNIT_LENGTH);
    printf("        -dlat; [%s]\n", HARP_UNIT_LATITUDE);
    printf("        -dlon; [%s]\n", HARP_UNIT_LONGITUDE);
    printf("        -da; [%s]\n", HARP_UNIT_PERCENT);
    printf("        -dsza, -dsaa, -dvza, -dvaa, -dvaa, -dtheta; [%s]\n", HARP_UNIT_ANGLE);
}

static void print_help_resampling_options(void)
{
    printf("      Resampling options:\n");
    printf("      -Rnna, --nearest-neighbour-a: keep only nearest neighbour,\n");
    printf("                                    dataset A is the master dataset\n");
    printf("      -Rnnb, --nearest-neighbour-b: keep only nearest neighbour, \n");
    printf("                                    dataset B is the master dataset\n");
    printf("      The nearest neighbour is the sample with which the squared sum\n");
    printf("      of the weighted differences is minimal\n");
    printf("      When resampling is set to 'Rnna' and/or 'Rnnb',\n");
    printf("      the following parameters can be set:\n");
    printf("      -wft 'value [unit]'         : sets the weighting factor for time\n");
    printf("      -wfdp 'value [unit]'        : sets the weighting factor for\n");
    printf("                                    point distance\n");
    printf("      -wfa 'value [unit]'         : sets the weighting factor for\n");
    printf("                                    overlapping percentage\n");
    printf("      -wfsza 'value [unit]'       : sets the weighting factor\n");
    printf("                                    for solar zenith angle\n");
    printf("      -wfsaa 'value [unit]'       : sets the weighting factor\n");
    printf("                                    for solar azimuth angle\n");
    printf("      -wfvza 'value [unit]'       : sets the weighting factor\n");
    printf("                                    for viewing zenith angle\n");
    printf("      -wfvaa 'value [unit]'       : sets the weighting factor\n");
    printf("                                    for viewing azimuth angle\n");
    printf("      -wftheta 'value [unit]'     : sets the weighting factor\n");
    printf("                                    for scattering angle\n");
    printf("      When '[unit]' is not specified in the above, a default unit will be\n");
    printf("      adopted:\n");
    printf("        Weighting factors; [default unit]\n");
    printf("        -wft; [1/%s]\n", HARP_UNIT_TIME);
    printf("        -wfdp; [1/%s]\n", HARP_UNIT_LENGTH);
    printf("        -wfa; [1/%s]\n", HARP_UNIT_PERCENT);
    printf("        -wfsza, -wfsaa, -wfvza, -wfvaa, -wfvaa, -wftheta; [1/%s]\n", HARP_UNIT_ANGLE);
    printf("      When a weighting factor is not set, a default value of 1 and\n");
    printf("      the default unit are adopted. Recommend value and unit for the\n");
    printf("      weighting factors are the reciprocals of the corresponding\n");
    printf("      collocation criteria value and unit that is used.\n");
}

void print_help_matchup(void)
{
    printf("Usage:\n");
    printf("  harpcollocate matchup [options]\n");
    printf("    Determine the collocation filter for two sets of HARP files,\n");
    printf("    and optionally resample the collocation result\n");
    printf("\n");
    printf("    Options:\n");
    printf("\n");
    printf("      -h, --help\n");
    printf("           Show matchup help (this text)\n");
    printf("      -ia, --input-a <input>\n");
    printf("           Specifies directory or names of input files of dataset A\n");
    printf("      -ib, --input-b <input>\n");
    printf("           Specifies directory or names of input files of dataset B\n");
    printf("      -or, --output-result <output>\n");
    printf("           Specifies collocation result file (comma separated values)\n");
    printf("\n");
    print_help_collocation_options();
    printf("\n");
    print_help_resampling_options();
    printf("\n");
}

void print_help_resample(void)
{
    printf("Usage:\n");
    printf("  harpcollocate resample [options]\n");
    printf("    Resample an existing collocation result file\n");
    printf("\n");
    printf("    Options:\n");
    printf("\n");
    printf("      -h, --help\n");
    printf("           Show resample help (this text)\n");
    printf("      -ir, --input-result <input>\n");
    printf("           Input collocation result file (comma separated values)\n");
    printf("      -or, --output-result <output>\n");
    printf("           Create a new file, and do not overwrite the input\n");
    printf("           collocation result file\n");
    printf("\n");
    print_help_resampling_options();
    printf("\n");
}

void print_help_update(void)
{
    printf("Usage:\n");
    printf("  harpcollocate update [options]\n");
    printf("    Update an existing collocation result file by checking\n");
    printf("    the measurements in two sets of HARP files that still exist\n");
    printf("\n");
    printf("    Options:\n");
    printf("      -ia, --input-a <input>\n");
    printf("           Specifies directory or names of input files of dataset A\n");
    printf("      -ib, --input-b <input>\n");
    printf("           Specifies directory or names of input files of dataset B\n");
    printf("      -ir, --input-result <input>\n");
    printf("           Input collocation result file (comma separated values)\n");
    printf("      -or, --output-result <output>\n");
    printf("           Create a new file, and do not overwrite the input\n");
    printf("           collocation result file\n");
    printf("\n");
}

void print_help(void)
{
    printf("Usage:\n");
    printf("  harpcollocate sub-command [options]\n");
    printf("    Determine the collocation filter for two sets of HARP files.\n");
    printf("\n");
    printf("    Available sub-commands:\n");
    printf("      matchup\n");
    printf("      resample\n");
    printf("      update\n");
    printf("\n");
    printf("    Use 'harpcollocate <sub-command> --help' to get help on a specific\n");
    printf("    sub-command.\n");
    printf("\n");
    printf("  harpcollocate -h, --help\n");
    printf("    Show help (this text).\n");
    printf("\n");
    printf("  harpcollocate -v, --version\n");
    printf("    Print the version number of HARP and exit.\n");
    printf("\n");
}

int main(int argc, char *argv[])
{
    Collocation_mode collocation_mode;
    Collocation_options *collocation_options = NULL;
    harp_collocation_result *collocation_result = NULL;

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

    if (parse_arguments(argc, argv, &collocation_mode, &collocation_options) != 0)
    {
        fprintf(stderr, "ERROR: %s\n", harp_errno_to_string(harp_errno));
        exit(1);
    }

    if (harp_init() != 0)
    {
        fprintf(stderr, "ERROR: %s\n", harp_errno_to_string(harp_errno));
        exit(1);
    }

    switch (collocation_mode)
    {
        case collocation_mode_matchup:

            /* Perform a full-blown collocation */
            if (matchup(collocation_options, &collocation_result) != 0)
            {
                collocation_options_delete(collocation_options);
                fprintf(stderr, "ERROR: %s\n", harp_errno_to_string(harp_errno));
                harp_done();
                exit(1);
            }
            if (collocation_result_convert_units(collocation_result) != 0)
            {
                collocation_options_delete(collocation_options);
                harp_collocation_result_delete(collocation_result);
                fprintf(stderr, "ERROR: %s\n", harp_errno_to_string(harp_errno));
                harp_done();
                exit(1);
            }

            /* Optional: Resample the collocation result */
            if (resample(collocation_options, collocation_result) != 0)
            {
                collocation_options_delete(collocation_options);
                harp_collocation_result_delete(collocation_result);
                fprintf(stderr, "ERROR: %s\n", harp_errno_to_string(harp_errno));
                harp_done();
                exit(1);
            }
            if (harp_collocation_result_sort_by_collocation_index(collocation_result) != 0)
            {
                collocation_options_delete(collocation_options);
                harp_collocation_result_delete(collocation_result);
                fprintf(stderr, "ERROR: %s\n", harp_errno_to_string(harp_errno));
                harp_done();
                exit(1);
            }
            break;

        case collocation_mode_resample:

            /* Skip the collocation, and resample an existing input collocation result file */
            if (harp_collocation_result_read(collocation_options->filename_result_in, &collocation_result) != 0)
            {
                collocation_options_delete(collocation_options);
                fprintf(stderr, "ERROR: %s\n", harp_errno_to_string(harp_errno));
                harp_done();
                exit(1);
            }

            /* Resample the collocation result */
            if (resample(collocation_options, collocation_result) != 0)
            {
                collocation_options_delete(collocation_options);
                harp_collocation_result_delete(collocation_result);
                fprintf(stderr, "ERROR: %s\n", harp_errno_to_string(harp_errno));
                harp_done();
                exit(1);
            }
            if (harp_collocation_result_sort_by_collocation_index(collocation_result) != 0)
            {
                collocation_options_delete(collocation_options);
                harp_collocation_result_delete(collocation_result);
                fprintf(stderr, "ERROR: %s\n", harp_errno_to_string(harp_errno));
                harp_done();
                exit(1);
            }

            break;

        case collocation_mode_update:

            /* Skip the collocation, and update an existing input collocation result file */
            if (harp_collocation_result_read(collocation_options->filename_result_in, &collocation_result) != 0)
            {
                collocation_options_delete(collocation_options);
                fprintf(stderr, "ERROR: %s\n", harp_errno_to_string(harp_errno));
                harp_done();
                exit(1);
            }

            /* Update the collocation result */
            if (update(collocation_options, collocation_result) != 0)
            {
                collocation_options_delete(collocation_options);
                harp_collocation_result_delete(collocation_result);
                fprintf(stderr, "ERROR: %s\n", harp_errno_to_string(harp_errno));
                harp_done();
                exit(1);
            }
            break;
    }

    /* Write the collocation result to file */
    if (harp_collocation_result_write(collocation_options->filename_result, collocation_result) != 0)
    {
        collocation_options_delete(collocation_options);
        harp_collocation_result_delete(collocation_result);
        fprintf(stderr, "ERROR: %s\n", harp_errno_to_string(harp_errno));
        harp_done();
        exit(1);
    }

    /* Done */
    collocation_options_delete(collocation_options);
    harp_collocation_result_delete(collocation_result);
    harp_done();

    return 0;
}
