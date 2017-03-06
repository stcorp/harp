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

#include "harp-internal.h"
#include "harp-csv.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COLLOCATION_RESULT_BLOCK_SIZE 1024

/** \defgroup harp_collocation HARP Collocation
 * The HARP Collocation module contains the functionality that deals with collocation two datasets
 * of products. The two datasets are refered to as dataset A (primary) and dataset B (secondary).
 * The result of a collocation is a list of pairs. Each pair references a measurement from dataset A
 * (using the source product name and measurement index within that product) and a measurement from dataset B.
 * Each collocation pair also gets a unique collocation_index sequence number.
 * For each collocation criteria used in the matchup the actual difference is stored as part of the pair as well.
 * Collocation results can be written to and read from a csv file.
 */

static void collocation_pair_swap_datasets(harp_collocation_pair *pair)
{
    long index_a = pair->product_index_a;
    long sample_a = pair->sample_index_a;

    pair->product_index_a = pair->product_index_b;
    pair->sample_index_a = pair->sample_index_b;
    pair->product_index_b = index_a;
    pair->sample_index_b = sample_a;
}

static int collocation_pair_new(long collocation_index, long product_index_a, long sample_index_a, long product_index_b,
                                long sample_index_b, const double *difference, harp_collocation_pair **new_pair)
{
    harp_collocation_pair *pair;
    int k;

    pair = (harp_collocation_pair *)malloc(sizeof(harp_collocation_pair));
    if (pair == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_collocation_pair), __FILE__, __LINE__);
        return -1;
    }

    pair->collocation_index = collocation_index;

    pair->product_index_a = product_index_a;
    pair->sample_index_a = sample_index_a;

    pair->product_index_b = product_index_b;
    pair->sample_index_b = sample_index_b;

    for (k = 0; k < HARP_COLLOCATION_RESULT_MAX_NUM_DIFFERENCES; k++)
    {
        pair->difference[k] = difference[k];
    }

    *new_pair = pair;
    return 0;
}

static void collocation_pair_delete(harp_collocation_pair *pair)
{
    if (pair == NULL)
    {
        return;
    }

    free(pair);
}

/** \addtogroup harp_collocation
 * @{
 */

/** Create a new collocation result set
 * \param new_collocation_result Pointer to the C variable where the new result set will be stored.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_collocation_result_new(harp_collocation_result **new_collocation_result)
{
    harp_collocation_result *collocation_result = NULL;
    int i;

    collocation_result = (harp_collocation_result *)malloc(sizeof(harp_collocation_result));
    if (collocation_result == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_collocation_result), __FILE__, __LINE__);
        return -1;
    }

    /* create the datasets */
    harp_dataset_new(&collocation_result->dataset_a);
    harp_dataset_new(&collocation_result->dataset_b);

    for (i = 0; i < HARP_COLLOCATION_RESULT_MAX_NUM_DIFFERENCES; i++)
    {
        collocation_result->difference_available[i] = 0;
        collocation_result->difference_unit[i] = NULL;
    }
    collocation_result->num_pairs = 0;
    collocation_result->pair = NULL;

    *new_collocation_result = collocation_result;

    return 0;
}

/** Remove a collocation result set
 * \param collocation_result Result set that will be removed.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API void harp_collocation_result_delete(harp_collocation_result *collocation_result)
{
    int k;

    if (collocation_result == NULL)
    {
        return;
    }

    if (collocation_result->dataset_a != NULL)
    {
        harp_dataset_delete(collocation_result->dataset_a);
    }
    if (collocation_result->dataset_b != NULL)
    {
        harp_dataset_delete(collocation_result->dataset_b);
    }

    for (k = 0; k < HARP_COLLOCATION_RESULT_MAX_NUM_DIFFERENCES; k++)
    {
        if (collocation_result->difference_unit[k])
        {
            free(collocation_result->difference_unit[k]);
        }
    }

    if (collocation_result->pair)
    {
        long i;

        for (i = 0; i < collocation_result->num_pairs; i++)
        {
            collocation_pair_delete(collocation_result->pair[i]);
        }
        free(collocation_result->pair);
    }

    free(collocation_result);
}

/**
 * @}
 */

static harp_dataset *sort_dataset_a = NULL;
static harp_dataset *sort_dataset_b = NULL;

static int compare_by_a(const void *a, const void *b)
{
    harp_collocation_pair *pair_a = *(harp_collocation_pair **)a;
    harp_collocation_pair *pair_b = *(harp_collocation_pair **)b;

    if (pair_a->product_index_a != pair_b->product_index_a)
    {
        return strcmp(sort_dataset_a->source_product[pair_a->product_index_a],
                      sort_dataset_a->source_product[pair_b->product_index_a]);
    }
    if (pair_a->sample_index_a < pair_b->sample_index_a)
    {
        return -1;
    }
    if (pair_a->sample_index_a > pair_b->sample_index_a)
    {
        return 1;
    }

    /* If a is equal, then further sort by b to get a fixed ordering. */
    if (pair_a->product_index_b != pair_b->product_index_b)
    {
        return strcmp(sort_dataset_b->source_product[pair_a->product_index_b],
                      sort_dataset_b->source_product[pair_b->product_index_b]);
    }
    if (pair_a->sample_index_b < pair_b->sample_index_b)
    {
        return -1;
    }
    if (pair_a->sample_index_b > pair_b->sample_index_b)
    {
        return 1;
    }

    return 0;
}

static int compare_by_b(const void *a, const void *b)
{
    harp_collocation_pair *pair_a = *(harp_collocation_pair **)a;
    harp_collocation_pair *pair_b = *(harp_collocation_pair **)b;

    if (pair_a->product_index_b != pair_b->product_index_b)
    {
        return strcmp(sort_dataset_b->source_product[pair_a->product_index_b],
                      sort_dataset_b->source_product[pair_b->product_index_b]);
    }
    if (pair_a->sample_index_b < pair_b->sample_index_b)
    {
        return -1;
    }
    if (pair_a->sample_index_b > pair_b->sample_index_b)
    {
        return 1;
    }

    /* If b is equal, then further sort by a to get a fixed ordering. */
    if (pair_a->product_index_a != pair_b->product_index_a)
    {
        return strcmp(sort_dataset_a->source_product[pair_a->product_index_b],
                      sort_dataset_a->source_product[pair_b->product_index_b]);
    }
    if (pair_a->sample_index_a < pair_b->sample_index_a)
    {
        return -1;
    }
    if (pair_a->sample_index_a > pair_b->sample_index_a)
    {
        return 1;
    }

    return 0;
}

static int compare_by_collocation_index(const void *a, const void *b)
{
    harp_collocation_pair *pair_a = *(harp_collocation_pair **)a;
    harp_collocation_pair *pair_b = *(harp_collocation_pair **)b;

    if (pair_a->collocation_index < pair_b->collocation_index)
    {
        return -1;
    }
    if (pair_a->collocation_index > pair_b->collocation_index)
    {
        return 1;
    }

    return 0;
}

/** \addtogroup harp_collocation
 * @{
 */

/** Sort the collocation result pairs by dataset A
 * Results will be sorted first by product index of A and then by sample index of A
 * \param collocation_result Result set that will be sorted in place.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_collocation_result_sort_by_a(harp_collocation_result *collocation_result)
{
    sort_dataset_a = collocation_result->dataset_a;
    sort_dataset_b = collocation_result->dataset_b;
    qsort(collocation_result->pair, collocation_result->num_pairs, sizeof(harp_collocation_pair *), compare_by_a);
    sort_dataset_a = NULL;
    sort_dataset_b = NULL;
    return 0;
}

/** Sort the collocation result pairs by dataset B
 * Results will be sorted first by product index of B and then by sample index of B
 * \param collocation_result Result set that will be sorted in place.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_collocation_result_sort_by_b(harp_collocation_result *collocation_result)
{
    sort_dataset_a = collocation_result->dataset_a;
    sort_dataset_b = collocation_result->dataset_b;
    qsort(collocation_result->pair, collocation_result->num_pairs, sizeof(harp_collocation_pair *), compare_by_b);
    sort_dataset_a = NULL;
    sort_dataset_b = NULL;
    return 0;
}

/** Sort the collocation result pairs by collocation index
 * \param collocation_result Result set that will be sorted in place.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_collocation_result_sort_by_collocation_index(harp_collocation_result *collocation_result)
{
    qsort(collocation_result->pair, collocation_result->num_pairs, sizeof(harp_collocation_pair *),
          compare_by_collocation_index);
    return 0;
}

/** Filter collocation result set for a specific product from dataset A
 * Only results that contain the referenced source product will be retained.
 * \param collocation_result Result set that will be filtered in place.
 * \param source_product source product reference from dataset A that should be filtered on.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_collocation_result_filter_for_source_product_a(harp_collocation_result *collocation_result,
                                                                    const char *source_product)
{
    long product_index;
    long i, j;

    if (harp_dataset_get_index_from_source_product(collocation_result->dataset_a, source_product, &product_index) != 0)
    {
        return -1;
    }
    for (i = collocation_result->num_pairs - 1; i >= 0; i--)
    {
        if (collocation_result->pair[i]->product_index_a != product_index)
        {
            collocation_pair_delete(collocation_result->pair[i]);
            for (j = i + 1; j < collocation_result->num_pairs; j++)
            {
                collocation_result->pair[j - 1] = collocation_result->pair[j];
            }
            collocation_result->num_pairs--;
        }
    }
    return 0;
}

/** Filter collocation result set for a specific product from dataset B
 * Only results that contain the referenced source product will be retained.
 * \param collocation_result Result set that will be filtered in place.
 * \param source_product source product reference from dataset B that should be filtered on.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_collocation_result_filter_for_source_product_b(harp_collocation_result *collocation_result,
                                                                    const char *source_product)
{
    long product_index;
    long i, j;

    if (harp_dataset_get_index_from_source_product(collocation_result->dataset_b, source_product, &product_index) != 0)
    {
        return -1;
    }
    for (i = collocation_result->num_pairs - 1; i >= 0; i--)
    {
        if (collocation_result->pair[i]->product_index_b != product_index)
        {
            collocation_pair_delete(collocation_result->pair[i]);
            for (j = i + 1; j < collocation_result->num_pairs; j++)
            {
                collocation_result->pair[j - 1] = collocation_result->pair[j];
            }
            collocation_result->num_pairs--;
        }
    }
    return 0;
}

/* This function uses a binary search and assumes the collocation result to be sorted by collocation index */
static int find_collocation_pair_for_collocation_index(harp_collocation_result *collocation_result,
                                                       long collocation_index, long *index)
{
    long lower_index;
    long upper_index;

    lower_index = 0;
    upper_index = collocation_result->num_pairs - 1;

    while (upper_index >= lower_index)
    {
        /* Determine the index that splits the search space into two (approximately) equal halves. */
        long pivot_index = lower_index + ((upper_index - lower_index) / 2);

        /* If the pivot equals the key, terminate early. */
        if (collocation_result->pair[pivot_index]->collocation_index == collocation_index)
        {
            *index = pivot_index;
            return 0;
        }

        /* If the pivot is smaller than the key, search the upper sub array, otherwise search the lower sub array. */
        if (collocation_result->pair[pivot_index]->collocation_index < collocation_index)
        {
            lower_index = pivot_index + 1;
        }
        else
        {
            upper_index = pivot_index - 1;
        }
    }

    harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "cannot find collocation index %d in collocation results",
                   collocation_index);
    return -1;
}

/** Filter collocation result set for the specified list of collocation indices.
 * The collocation result pairs will be sorted according to the order in the provided \a collocation_index parameter.
 * If a collocation index cannot be found in the collocation_result set then an error will be thrown.
 * \param collocation_result Result set that will be filtered in place.
 * \param num_indices Number of items in the collocation_index parameter.
 * \param collocation_index Array of collocation index values to match against the collocation_result set.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_collocation_result_filter_for_collocation_indices(harp_collocation_result *collocation_result,
                                                                       long num_indices, int32_t *collocation_index)
{
    harp_collocation_pair **pair = NULL;
    long num_pairs = 0;
    long i;

    if (harp_collocation_result_sort_by_collocation_index(collocation_result) != 0)
    {
        return -1;
    }

    /* create a temporary array to store all pairs */
    pair = malloc(collocation_result->num_pairs * sizeof(harp_collocation_pair *));
    if (!pair)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       collocation_result->num_pairs * sizeof(harp_collocation_pair *), __FILE__, __LINE__);
        return -1;
    }

    for (i = 0; i < num_indices; i++)
    {
        long index;
        long j;

        if (find_collocation_pair_for_collocation_index(collocation_result, collocation_index[i], &index) != 0)
        {
            goto error;
        }
        pair[num_pairs] = collocation_result->pair[index];
        num_pairs++;
        for (j = index + 1; j < collocation_result->num_pairs; j++)
        {
            collocation_result->pair[j - 1] = collocation_result->pair[j];
        }
        collocation_result->num_pairs--;
    }

    for (i = 0; i < collocation_result->num_pairs; i++)
    {
        collocation_pair_delete(collocation_result->pair[i]);
    }
    free(collocation_result->pair);
    collocation_result->pair = pair;
    collocation_result->num_pairs = num_pairs;

    return 0;

  error:
    for (i = 0; i < num_pairs; i++)
    {
        collocation_pair_delete(pair[i]);
    }
    free(pair);

    return -1;
}

/** Add collocation result entry to a result set
 * \note this function will not check for uniqueness of the collocation_index values in the resulting set
 * \param collocation_result Result set that will be extended
 * \param collocation_index Unique index of the pair in the overall collocation result
 * \param source_product_a Name of the source_product attribute of the product from dataset A
 * \param index_a Value of the index variable for the matching sample in the product from dataset A
 * \param source_product_b Name of the source_product attribute of the product from dataset B
 * \param index_b Value of the index variable for the matching sample in the product from dataset B
 * \param difference Array of difference values (should have length HARP_COLLOCATION_RESULT_MAX_NUM_DIFFERENCES)
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_collocation_result_add_pair(harp_collocation_result *collocation_result, long collocation_index,
                                                 const char *source_product_a, long index_a,
                                                 const char *source_product_b, long index_b, const double *difference)
{
    harp_collocation_pair *pair;
    long product_index_a, product_index_b;

    /* Ensure the products appear in the dataset */
    if (harp_dataset_add_product(collocation_result->dataset_a, source_product_a, NULL) != 0)
    {
        return -1;
    }
    if (harp_dataset_add_product(collocation_result->dataset_b, source_product_b, NULL) != 0)
    {
        return -1;
    }

    if (harp_dataset_get_index_from_source_product(collocation_result->dataset_a, source_product_a, &product_index_a)
        != 0)
    {
        return -1;
    }
    if (harp_dataset_get_index_from_source_product(collocation_result->dataset_b, source_product_b, &product_index_b)
        != 0)
    {
        return -1;
    }
    if (collocation_pair_new(collocation_index, product_index_a, index_a, product_index_b, index_b, difference, &pair)
        != 0)
    {
        return -1;
    }

    if (collocation_result->num_pairs % COLLOCATION_RESULT_BLOCK_SIZE == 0)
    {
        harp_collocation_pair **new_pair = NULL;

        new_pair = realloc(collocation_result->pair, (collocation_result->num_pairs + COLLOCATION_RESULT_BLOCK_SIZE) *
                           sizeof(harp_collocation_pair *));
        if (new_pair == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           (long)(collocation_result->num_pairs + COLLOCATION_RESULT_BLOCK_SIZE) *
                           sizeof(harp_collocation_pair *), __FILE__, __LINE__);
            collocation_pair_delete(pair);
            return -1;
        }

        collocation_result->pair = new_pair;
    }

    collocation_result->pair[collocation_result->num_pairs] = pair;
    collocation_result->num_pairs++;
    return 0;
}

/** Remove collocation result entry from a result set
 * \param collocation_result Result set from which to remove the entry
 * \param index Zero-based index in the collocation result set of the entry that should be removed
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_collocation_result_remove_pair_at_index(harp_collocation_result *collocation_result, long index)
{
    long i;

    if (index < 0 || index >= collocation_result->num_pairs)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "index (%ld) is not in the range of collocation results [0,%ld)",
                       index, collocation_result->num_pairs);
        return -1;
    }

    collocation_pair_delete(collocation_result->pair[index]);
    for (i = index + 1; i < collocation_result->num_pairs; i++)
    {
        collocation_result->pair[i - 1] = collocation_result->pair[i];
    }
    collocation_result->num_pairs--;

    return 0;
}

/**
 * @}
 */

static int parse_difference_type_and_unit(char **str, harp_collocation_difference_type *difference_type, char **unit)
{
    char *cursor = *str;
    int stringlength = 0;

    /* Skip leading white space */
    cursor = harp_csv_ltrim(cursor);

    /* Grab difference string */
    while (cursor[stringlength] != '[' && cursor[stringlength] != ',' && cursor[stringlength] != '\0')
    {
        stringlength++;
    }
    *str = &cursor[stringlength];
    while (stringlength > 0 && (cursor[stringlength - 1] == ' ' || cursor[stringlength - 1] == '\t'))
    {
        stringlength--;
    }
    cursor[stringlength] = '\0';

    if (strcmp(cursor, "absolute difference in time") == 0)
    {
        *difference_type = harp_collocation_difference_absolute_time;
    }
    else if (strcmp(cursor, "absolute difference in latitude") == 0)
    {
        *difference_type = harp_collocation_difference_absolute_latitude;
    }
    else if (strcmp(cursor, "absolute difference in longitude") == 0)
    {
        *difference_type = harp_collocation_difference_absolute_longitude;
    }
    else if (strcmp(cursor, "point distance") == 0)
    {
        *difference_type = harp_collocation_difference_point_distance;
    }
    else if (strcmp(cursor, "overlapping percentage") == 0)
    {
        *difference_type = harp_collocation_difference_overlapping_percentage;
    }
    else if (strcmp(cursor, "absolute difference in SZA") == 0)
    {
        *difference_type = harp_collocation_difference_absolute_sza;
    }
    else if (strcmp(cursor, "absolute difference in SAA") == 0)
    {
        *difference_type = harp_collocation_difference_absolute_saa;
    }
    else if (strcmp(cursor, "absolute difference in VZA") == 0)
    {
        *difference_type = harp_collocation_difference_absolute_vza;
    }
    else if (strcmp(cursor, "absolute difference in VAA") == 0)
    {
        *difference_type = harp_collocation_difference_absolute_vaa;
    }
    else if (strcmp(cursor, "absolute difference in Theta") == 0)
    {
        *difference_type = harp_collocation_difference_absolute_theta;
    }
    else
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "could not derive difference type from collocation result file"
                       " header (%s)", cursor);
        return -1;
    }

    cursor = *str;
    if (*cursor != '[')
    {
        /* No unit is found */
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "no unit in header");
        return -1;
    }
    cursor++;
    stringlength = 0;
    while (cursor[stringlength] != ']' && cursor[stringlength] != '\0')
    {
        stringlength++;
    }
    if (cursor[stringlength] == '\0')
    {
        *str = &cursor[stringlength];
    }
    else
    {
        cursor[stringlength] = '\0';
        *str = &cursor[stringlength + 1];
    }
    *unit = cursor;

    /* skip trailing whitespace and next ',' */
    cursor = *str;
    while (*cursor != ',' && *cursor != '\0')
    {
        cursor++;
    }
    if (*cursor == ',')
    {
        cursor++;
    }
    *str = cursor;

    return 0;
}

static int read_header(FILE *file, harp_collocation_result *collocation_result)
{
    char line[HARP_CSV_LINE_LENGTH];
    char *cursor = line;
    char *string = NULL;
    size_t length;

    rewind(file);

    if (fgets(line, HARP_CSV_LINE_LENGTH, file) == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "error reading header");
        return -1;
    }

    /* trim end-of-line */
    length = strlen(line);
    while (length > 0 && (line[length - 1] == '\r' || line[length - 1] == '\n'))
    {
        length--;
    }
    line[length] = '\0';

    harp_csv_parse_string(&cursor, &string);
    if (strcmp(string, "collocation_index") != 0)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "error reading 'collocation_index' in header");
        return -1;
    }
    harp_csv_parse_string(&cursor, &string);
    if (strcmp(string, "source_product_a") != 0)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "error reading 'source_product_a' in header");
        return -1;
    }
    harp_csv_parse_string(&cursor, &string);
    if (strcmp(string, "index_a") != 0)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "error reading 'index_a' in header");
        return -1;
    }
    harp_csv_parse_string(&cursor, &string);
    if (strcmp(string, "source_product_b") != 0)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "error reading 'source_product_b' in header");
        return -1;
    }
    harp_csv_parse_string(&cursor, &string);
    if (strcmp(string, "index_b") != 0)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "error reading 'index_b' in header");
        return -1;
    }
    while (*cursor != '\0')
    {
        harp_collocation_difference_type difference_type;

        if (parse_difference_type_and_unit(&cursor, &difference_type, &string) != 0)
        {
            return -1;
        }
        collocation_result->difference_available[difference_type] = 1;
        collocation_result->difference_unit[difference_type] = strdup(string);
        if (collocation_result->difference_unit[difference_type] == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                           __LINE__);
            return -1;
        }
    }

    return 0;
}

static int read_pair(FILE *file, harp_collocation_result *collocation_result)
{
    char line[HARP_CSV_LINE_LENGTH];
    char *cursor = line;
    long collocation_index;
    char *source_product_a;
    char *source_product_b;
    long index_a;
    long index_b;
    double differences[HARP_COLLOCATION_RESULT_MAX_NUM_DIFFERENCES];
    int k;

    if (fgets(line, HARP_CSV_LINE_LENGTH, file) == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "error reading line");
        return -1;
    }

    harp_csv_rtrim(line);

    /* Parse line */
    harp_csv_parse_long(&cursor, &collocation_index);
    harp_csv_parse_string(&cursor, &source_product_a);
    harp_csv_parse_long(&cursor, &index_a);
    harp_csv_parse_string(&cursor, &source_product_b);
    harp_csv_parse_long(&cursor, &index_b);
    for (k = 0; k < HARP_COLLOCATION_RESULT_MAX_NUM_DIFFERENCES; k++)
    {
        if (collocation_result->difference_available[k] && k != harp_collocation_difference_delta)
        {
            harp_csv_parse_double(&cursor, &differences[k]);
        }
        else
        {
            differences[k] = harp_nan();
        }
    }

    if (harp_collocation_result_add_pair(collocation_result, collocation_index, source_product_a, index_a,
                                         source_product_b, index_b, differences) != 0)
    {
        fclose(file);
        return -1;
    }

    return 0;
}

/** \addtogroup harp_collocation
 * @{
 */

/** Read collocation result set from a csv file
 * The csv file should follow the HARP format for collocation result files.
 * \param collocation_result_filename Full file path to the csv file.
 * \param new_collocation_result Pointer to the C variable where the new result set will be stored.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_collocation_result_read(const char *collocation_result_filename,
                                             harp_collocation_result **new_collocation_result)
{
    harp_collocation_result *collocation_result = NULL;
    FILE *file;
    long i;
    long num_lines;

    if (collocation_result_filename == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "collocation_result_filename is NULL");
        return -1;
    }

    /* Open the collocation result file */
    file = fopen(collocation_result_filename, "r");
    if (file == NULL)
    {
        harp_set_error(HARP_ERROR_FILE_OPEN, "error opening collocation result file '%s'", collocation_result_filename);
        return -1;
    }

    /* Get number of lines */
    if (harp_csv_get_num_lines(file, collocation_result_filename, &num_lines) != 0)
    {
        fclose(file);
        return -1;
    }

    /* Start new collocation result */
    if (harp_collocation_result_new(&collocation_result) != 0)
    {
        fclose(file);
        return -1;
    }

    /* Exclude the header line */
    num_lines--;
    if (num_lines < 1)
    {
        /* No lines to read */
        fclose(file);

        /* Return an empty collocation result */
        *new_collocation_result = collocation_result;
        return 0;
    }

    /* Initialize the collocation result and update the collocation options
     * with the information in the header */
    if (read_header(file, collocation_result) != 0)
    {
        harp_collocation_result_delete(collocation_result);
        fclose(file);
        return -1;
    }

    /* Read the matching pairs */
    for (i = 0; i < num_lines; i++)
    {
        if (read_pair(file, collocation_result) != 0)
        {
            harp_collocation_result_delete(collocation_result);
            fclose(file);
            return -1;
        }
    }

    /* Close the collocation result file */
    if (fclose(file) != 0)
    {
        harp_set_error(HARP_ERROR_FILE_CLOSE, "error closing collocation result file");
        harp_collocation_result_delete(collocation_result);
        return -1;
    }

    *new_collocation_result = collocation_result;
    return 0;
}

/**
 * @}
 */

static void write_header(FILE *file, const harp_collocation_result *collocation_result)
{
    int k;

    fprintf(file, "collocation_index,source_product_a,index_a,source_product_b,index_b");
    /* don't write harp_collocation_difference_delta, so stop at HARP_COLLOCATION_RESULT_MAX_NUM_DIFFERENCES - 1 */
    for (k = 0; k < HARP_COLLOCATION_RESULT_MAX_NUM_DIFFERENCES - 1; k++)
    {
        if (collocation_result->difference_available[k])
        {
            fprintf(file, ",");
            switch (k)
            {
                case harp_collocation_difference_absolute_time:
                    fprintf(file, "absolute difference in time");
                    break;

                case harp_collocation_difference_absolute_latitude:
                    fprintf(file, "absolute difference in latitude");
                    break;

                case harp_collocation_difference_absolute_longitude:
                    fprintf(file, "absolute difference in longitude");
                    break;

                case harp_collocation_difference_point_distance:
                    fprintf(file, "point distance");
                    break;

                case harp_collocation_difference_overlapping_percentage:
                    fprintf(file, "overlapping percentage");
                    break;

                case harp_collocation_difference_absolute_sza:
                    fprintf(file, "absolute difference in SZA");
                    break;

                case harp_collocation_difference_absolute_saa:
                    fprintf(file, "absolute difference in SAA");
                    break;

                case harp_collocation_difference_absolute_vza:
                    fprintf(file, "absolute difference in VZA");
                    break;

                case harp_collocation_difference_absolute_vaa:
                    fprintf(file, "absolute difference in VAA");
                    break;

                case harp_collocation_difference_absolute_theta:
                    fprintf(file, "absolute difference in Theta");
                    break;

                case harp_collocation_difference_unknown:
                case harp_collocation_difference_delta:
                    assert(0);
                    exit(1);
            }
            fprintf(file, " [%s]", collocation_result->difference_unit[k]);
        }
    }
    fprintf(file, "\n");
}

static void write_pair(FILE *file, const harp_collocation_result *collocation_result, long i)
{
    int k;

    assert(collocation_result->pair != NULL);
    assert(collocation_result->pair[i] != NULL);

    /* Write filenames and measurement indices */
    fprintf(file, "%ld,%s,%ld,%s,%ld", collocation_result->pair[i]->collocation_index,
            collocation_result->dataset_a->source_product[collocation_result->pair[i]->product_index_a],
            collocation_result->pair[i]->sample_index_a,
            collocation_result->dataset_b->source_product[collocation_result->pair[i]->product_index_b],
            collocation_result->pair[i]->sample_index_b);

    /* Write differences */
    /* don't write harp_collocation_difference_delta, so stop at HARP_COLLOCATION_RESULT_MAX_NUM_DIFFERENCES - 1 */
    for (k = 0; k < HARP_COLLOCATION_RESULT_MAX_NUM_DIFFERENCES - 1; k++)
    {
        if (collocation_result->difference_available[k])
        {
            fprintf(file, ",%.8g", collocation_result->pair[i]->difference[k]);
        }
    }
    fprintf(file, "\n");
}

/** \addtogroup harp_collocation
 * @{
 */

/** Read collocation result set to a csv file
 * The csv file will follow the HARP format for collocation result files.
 * \param collocation_result_filename Full file path to the csv file.
 * \param collocation_result Collocation result set that will be written to file.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_collocation_result_write(const char *collocation_result_filename,
                                              harp_collocation_result *collocation_result)
{
    FILE *file;
    long i;

    if (collocation_result_filename == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "collocation_result_filename is NULL");
        return -1;
    }
    if (collocation_result == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "collocation_result is NULL");
        return -1;
    }

    /* Open the collocation result file */
    file = fopen(collocation_result_filename, "w");
    if (file == NULL)
    {
        harp_set_error(HARP_ERROR_FILE_OPEN, "error opening collocation result file '%s'", collocation_result_filename);
        return -1;
    }

    /* Write the header */
    write_header(file, collocation_result);

    /* Write the matching pairs */
    for (i = 0; i < collocation_result->num_pairs; i++)
    {
        write_pair(file, collocation_result, i);
    }

    /* Close the collocation result file */
    if (fclose(file) != 0)
    {
        harp_set_error(HARP_ERROR_FILE_READ, "error closing collocation result file");
        return -1;
    }

    return 0;
}

/** Swap the columns of this collocation result inplace.
 *
 * This swaps datasets A and B (such that A becomes B and B becomes A).
 * \param collocation_result Collocation result whose datasets should be swapped.
 */
LIBHARP_API void harp_collocation_result_swap_datasets(harp_collocation_result *collocation_result)
{
    long i;
    harp_dataset *data_a;

    for (i = 0; i < collocation_result->num_pairs; i++)
    {
        collocation_pair_swap_datasets(collocation_result->pair[i]);
    }

    data_a = collocation_result->dataset_a;

    collocation_result->dataset_a = collocation_result->dataset_b;
    collocation_result->dataset_b = data_a;
}

/**
 * @}
 */

/* Creates a shallow copy of a collocation result for filtering purposes */
int harp_collocation_result_shallow_copy(const harp_collocation_result *collocation_result,
                                         harp_collocation_result **new_result)
{
    harp_collocation_result *result = NULL;
    harp_collocation_pair **pairs = NULL;
    long i;

    /* allocate memory for the result struct */
    result = (harp_collocation_result *)malloc(sizeof(harp_collocation_result));
    if (result == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_collocation_result), __FILE__, __LINE__);
        free(result);
        return -1;
    }

    /* allocate memory for the pairs array */
    pairs = malloc(collocation_result->num_pairs * sizeof(harp_collocation_pair *));
    if (!pairs)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       collocation_result->num_pairs * sizeof(harp_collocation_pair *), __FILE__, __LINE__);
        return -1;
    }
    result->pair = pairs;

    /* populate the pairs with copies */
    for (i = 0; i < collocation_result->num_pairs; i++)
    {
        harp_collocation_pair *pair = collocation_result->pair[i];

        collocation_pair_new(pair->collocation_index, pair->product_index_a, pair->sample_index_a,
                             pair->product_index_b, pair->sample_index_b, pair->difference, &result->pair[i]);
    }
    result->num_pairs = collocation_result->num_pairs;

    /* copy other attributes */
    result->dataset_a = collocation_result->dataset_a;
    result->dataset_b = collocation_result->dataset_b;
    for (i = 0; i < HARP_COLLOCATION_RESULT_MAX_NUM_DIFFERENCES; i++)
    {
        result->difference_available[i] = collocation_result->difference_available[i];
        result->difference_unit[i] = collocation_result->difference_unit[i];
    }

    *new_result = result;

    return 0;
}

void harp_collocation_result_shallow_delete(harp_collocation_result *collocation_result)
{
    if (collocation_result != NULL)
    {
        if (collocation_result->pair != NULL)
        {
            long i;

            for (i = 0; i < collocation_result->num_pairs; i++)
            {
                collocation_pair_delete(collocation_result->pair[i]);
                collocation_result->pair[i] = NULL;
            }
        }

        free(collocation_result->pair);
        free(collocation_result);
    }
}

