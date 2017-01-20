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

#ifndef HARPCOLLOCATE_H
#define HARPCOLLOCATE_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "harp.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>

#define DATASET_BLOCK_SIZE 16
#define MAX_NUM_COLLOCATION_CRITERIA 24

typedef enum Collocation_mode_enum
{
    collocation_mode_matchup,
    collocation_mode_resample,
    collocation_mode_update
} Collocation_mode;

typedef struct Dataset_struct
{
    int num_files;
    char **filename;
    double *datetime_start;
    double *datetime_stop;
} Dataset;

/* Enumerate all possible collocation criteria */
/* This ordering is used in collocation_options */
typedef enum Collocation_criterion_type_enum
{
    collocation_criterion_type_time,
    collocation_criterion_type_latitude,
    collocation_criterion_type_longitude,
    collocation_criterion_type_point_distance,
    collocation_criterion_type_overlapping_percentage,
    collocation_criterion_type_sza,
    collocation_criterion_type_saa,
    collocation_criterion_type_vza,
    collocation_criterion_type_vaa,
    collocation_criterion_type_theta,
    collocation_criterion_type_point_a_in_area_b,
    collocation_criterion_type_point_b_in_area_a,
    collocation_criterion_type_overlapping
} Collocation_criterion_type;

typedef struct Collocation_criterion_struct
{
    Collocation_criterion_type type;
    double value;
    char *original_unit;
    char *collocation_unit;
} Collocation_criterion;

/* Re-sampling method */
typedef enum Resampling_method_enum
{
    resampling_method_none = 0,
    resampling_method_nearest_neighbour_a = 1,
    resampling_method_nearest_neighbour_b = 2,
    resampling_method_nearest_neighbour_ab = 3,
    resampling_method_nearest_neighbour_ba = 4
} Resampling_method;

/* Define a weighting factor */
typedef struct Weighting_factor_struct
{
    harp_collocation_difference_type difference_type;   /* Use difference type to assign index to weighting factor */
    double value;
    char *original_unit;
    char *collocation_unit;
} Weighting_factor;

typedef struct Collocation_options_struct
{
    /* Resampling of existing collocation result file */
    int skip_collocate;
    char *filename_result_in;
    /* Input/output files */
    Dataset *dataset_a_in;
    Dataset *dataset_b_in;
    char *filename_result;
    /* Collocation criteria */
    int num_criteria;
    int criterion_is_set[MAX_NUM_COLLOCATION_CRITERIA];
    Collocation_criterion *criterion[MAX_NUM_COLLOCATION_CRITERIA];
    /* Resampling options */
    Resampling_method resampling_method;
    int num_weighting_factors;
    int weighting_factor_is_set[HARP_COLLOCATION_RESULT_MAX_NUM_DIFFERENCES];
    Weighting_factor *weighting_factor[HARP_COLLOCATION_RESULT_MAX_NUM_DIFFERENCES];
} Collocation_options;

void print_version(void);
void print_help(void);

void print_help_matchup(void);
void print_help_resample(void);
void print_help_update(void);

int matchup(const Collocation_options *collocation_options, harp_collocation_result **new_collocation_result);
int resample(const Collocation_options *collocation_options, harp_collocation_result *input_collocation_result);
int update(const Collocation_options *collocation_options, harp_collocation_result *collocation_result);

const char *collocation_criterion_command_line_option_from_criterion_type(Collocation_criterion_type
                                                                          Collocation_criterion_type);
const char *weighting_factor_command_line_option_from_difference_type(harp_collocation_difference_type difference_type);

/* Parse the command line options */
int parse_arguments(int argc, char *argv[], Collocation_mode *new_collocation_mode,
                    Collocation_options **new_collocation_options);
void get_difference_type_from_collocation_criterion_type(const Collocation_criterion_type criterion_type,
                                                         harp_collocation_difference_type *difference_type);

/* Collocation options */
int collocation_options_new(Collocation_options **new_collocation_options);
void collocation_options_delete(Collocation_options *collocation_options);

/* Dataset */
int dataset_new(Dataset **new_dataset);
int dataset_add_filename(Dataset *dataset, char *filename);
void dataset_delete(Dataset *dataset);

/* Collocation result functions */
int collocation_result_convert_units(harp_collocation_result *collocation_result);
int calculate_delta(const harp_collocation_result *collocation_result, const Collocation_options *collocation_options,
                    harp_collocation_pair *pair, double *delta);

#endif
