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

#ifndef HARP_COLLOCATION_H
#define HARP_COLLOCATION_H

/* Enumerate all possible differences for collocation result */
typedef enum harp_collocation_difference_type_enum
{
    difference_type_unknown = -1,
    difference_type_absolute_difference_in_time,
    difference_type_absolute_difference_in_latitude,
    difference_type_absolute_difference_in_longitude,
    difference_type_point_distance,
    difference_type_overlapping_percentage,
    difference_type_absolute_difference_in_sza,
    difference_type_absolute_difference_in_saa,
    difference_type_absolute_difference_in_vza,
    difference_type_absolute_difference_in_vaa,
    difference_type_absolute_difference_in_theta,
    difference_type_delta       /* weighted norm of difference */
} harp_collocation_difference_type;

#define HARP_COLLOCATION_RESULT_MAX_NUM_DIFFERENCES 11

typedef struct harp_collocation_pair_struct
{
    long collocation_index;
    char *source_product_a;
    long index_a;
    char *source_product_b;
    long index_b;
    double difference[HARP_COLLOCATION_RESULT_MAX_NUM_DIFFERENCES];
} harp_collocation_pair;

typedef struct harp_collocation_result_struct
{
    int difference_available[HARP_COLLOCATION_RESULT_MAX_NUM_DIFFERENCES];
    char *difference_unit[HARP_COLLOCATION_RESULT_MAX_NUM_DIFFERENCES];
    long num_pairs;
    harp_collocation_pair **pair;
} harp_collocation_result;

/* Collocation result functions */
int harp_collocation_pair_new(long collocation_index, const char *source_product_a, long index_a,
                              const char *source_product_b, long index_b, const double *difference,
                              harp_collocation_pair **new_pair);
void harp_collocation_pair_delete(harp_collocation_pair *pair);
int harp_collocation_pair_copy(const harp_collocation_pair *input_pair, harp_collocation_pair **new_pair);

int harp_collocation_result_new(harp_collocation_result **new_collocation_result);
void harp_collocation_result_delete(harp_collocation_result *collocation_result);
int harp_collocation_result_sort_by_a(harp_collocation_result *collocation_result);
int harp_collocation_result_sort_by_b(harp_collocation_result *collocation_result);
int harp_collocation_result_sort_by_collocation_index(harp_collocation_result *collocation_result);
int harp_collocation_result_filter_for_source_product_a(harp_collocation_result *collocation_result,
                                                        const char *source_product);
int harp_collocation_result_filter_for_source_product_b(harp_collocation_result *collocation_result,
                                                        const char *source_product);
int harp_collocation_result_add_pair(harp_collocation_result *collocation_result, harp_collocation_pair *pair);
int harp_collocation_result_read(const char *collocation_result_filename,
                                 harp_collocation_result **new_collocation_result);
int harp_collocation_result_write(const char *collocation_result_filename, harp_collocation_result *collocation_result);

#endif
