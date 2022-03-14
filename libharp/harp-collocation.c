/*
 * Copyright (C) 2015-2022 S[&]T, The Netherlands.
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
 * of products. The two datasets are referred to as dataset A (primary) and dataset B (secondary).
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

static void collocation_pair_delete(harp_collocation_pair *pair)
{
    if (pair != NULL)
    {
        if (pair->difference != NULL)
        {
            free(pair->difference);
        }

        free(pair);
    }
}

static int collocation_pair_new(long collocation_index, long product_index_a, long sample_index_a, long product_index_b,
                                long sample_index_b, int num_differences, const double *difference,
                                harp_collocation_pair **new_pair)
{
    harp_collocation_pair *pair;
    int i;

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

    pair->num_differences = num_differences;
    pair->difference = NULL;

    pair->difference = malloc(num_differences * sizeof(double));
    if (pair->difference == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_collocation_pair), __FILE__, __LINE__);
        collocation_pair_delete(pair);
        return -1;
    }

    for (i = 0; i < num_differences; i++)
    {
        pair->difference[i] = difference[i];
    }

    *new_pair = pair;
    return 0;
}

/** \addtogroup harp_collocation
 * @{
 */

/** Create a new collocation result set
 * \param new_collocation_result Pointer to the C variable where the new result set will be stored.
 * \param num_differences The number of differences that have been calculated per pair for the collocation result
 * \param difference_variable_name An array of variable names describing the type of difference for each calculated
 *        difference
 * \param difference_unit An array of units for each calculated difference
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_collocation_result_new(harp_collocation_result **new_collocation_result, int num_differences,
                                            const char **difference_variable_name, const char **difference_unit)
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

    collocation_result->dataset_a = NULL;
    collocation_result->dataset_b = NULL;
    collocation_result->num_differences = 0;
    collocation_result->difference_variable_name = NULL;
    collocation_result->difference_unit = NULL;
    collocation_result->num_pairs = 0;
    collocation_result->pair = NULL;

    if (harp_dataset_new(&collocation_result->dataset_a) != 0)
    {
        harp_collocation_result_delete(collocation_result);
        return -1;
    }
    if (harp_dataset_new(&collocation_result->dataset_b) != 0)
    {
        harp_collocation_result_delete(collocation_result);
        return -1;
    }

    if (num_differences > 0)
    {
        collocation_result->difference_variable_name = malloc(num_differences * sizeof(char *));
        if (collocation_result->difference_variable_name == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           num_differences * sizeof(char *), __FILE__, __LINE__);
            harp_collocation_result_delete(collocation_result);
            return -1;
        }
        for (i = 0; i < num_differences; i++)
        {
            collocation_result->difference_variable_name[i] = NULL;
        }
        collocation_result->difference_unit = malloc(num_differences * sizeof(char *));
        if (collocation_result->difference_unit == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           num_differences * sizeof(char *), __FILE__, __LINE__);
            harp_collocation_result_delete(collocation_result);
            return -1;
        }
        for (i = 0; i < num_differences; i++)
        {
            collocation_result->difference_unit[i] = NULL;
        }
        if (difference_variable_name != NULL)
        {
            for (i = 0; i < num_differences; i++)
            {
                collocation_result->difference_variable_name[i] = strdup(difference_variable_name[i]);
                if (collocation_result->difference_variable_name[i] == NULL)
                {
                    harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)",
                                   __FILE__, __LINE__);
                    harp_collocation_result_delete(collocation_result);
                    return -1;
                }
            }
        }
        if (difference_unit != NULL)
        {
            for (i = 0; i < num_differences; i++)
            {
                collocation_result->difference_unit[i] = strdup(difference_unit[i]);
                if (collocation_result->difference_unit[i] == NULL)
                {
                    harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)",
                                   __FILE__, __LINE__);
                    harp_collocation_result_delete(collocation_result);
                    return -1;
                }
            }
        }
    }

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
    int i;

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

    if (collocation_result->difference_variable_name != NULL)
    {
        for (i = 0; i < collocation_result->num_differences; i++)
        {
            if (collocation_result->difference_variable_name[i] != NULL)
            {
                free(collocation_result->difference_variable_name[i]);
            }
        }
        free(collocation_result->difference_variable_name);
    }
    if (collocation_result->difference_unit != NULL)
    {
        for (i = 0; i < collocation_result->num_differences; i++)
        {
            if (collocation_result->difference_unit[i] != NULL)
            {
                free(collocation_result->difference_unit[i]);
            }
        }
        free(collocation_result->difference_unit);
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

int harp_collocation_result_add_difference(harp_collocation_result *collocation_result,
                                           const char *difference_variable_name, const char *difference_unit)
{
    char **new_string_array;
    int index;

    new_string_array = realloc(collocation_result->difference_variable_name,
                               (collocation_result->num_differences + 1) * sizeof(char *));
    if (new_string_array == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       (collocation_result->num_differences + 1) * sizeof(char *), __FILE__, __LINE__);
        return -1;
    }
    collocation_result->difference_variable_name = new_string_array;

    new_string_array = realloc(collocation_result->difference_unit,
                               (collocation_result->num_differences + 1) * sizeof(char *));
    if (new_string_array == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       (collocation_result->num_differences + 1) * sizeof(char *), __FILE__, __LINE__);
        return -1;
    }
    collocation_result->difference_unit = new_string_array;

    index = collocation_result->num_differences;
    collocation_result->difference_variable_name[index] = NULL;
    collocation_result->difference_unit[index] = NULL;
    collocation_result->num_differences++;

    collocation_result->difference_variable_name[index] = strdup(difference_variable_name);
    if (collocation_result->difference_variable_name[index] == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)",
                       __FILE__, __LINE__);
        return -1;
    }
    collocation_result->difference_unit[index] = strdup(difference_unit);
    if (collocation_result->difference_unit[index] == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)",
                       __FILE__, __LINE__);
        return -1;
    }

    return 0;
}

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

/**
 * @}
 */

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

/** \addtogroup harp_collocation
 * @{
 */

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
 * \param num_differences Number of calculated differences (should equal the number of differences with which the
 *        collocation result was initialized)
 * \param difference Array of difference values
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_collocation_result_add_pair(harp_collocation_result *collocation_result, long collocation_index,
                                                 const char *source_product_a, long index_a,
                                                 const char *source_product_b, long index_b, int num_differences,
                                                 const double *difference)
{
    harp_collocation_pair *pair;
    long product_index_a, product_index_b;

    if (num_differences != collocation_result->num_differences)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT,
                       "number of differences for pair (%d) does not equal that of collocation result (%d) (%s:%u)",
                       num_differences, collocation_result->num_differences, __FILE__, __LINE__);
        return -1;
    }

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
    if (collocation_pair_new(collocation_index, product_index_a, index_a, product_index_b, index_b, num_differences,
                             difference, &pair) != 0)
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

static int read_header(FILE *file, harp_collocation_result *collocation_result)
{
    char line[HARP_CSV_LINE_LENGTH + 1];
    char *cursor = line;
    char *string = NULL;
    size_t length;

    rewind(file);

    if (fgets(line, HARP_CSV_LINE_LENGTH + 1, file) == NULL)
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

    if (length == HARP_CSV_LINE_LENGTH)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "header exceeds max line length (%ld)", HARP_CSV_LINE_LENGTH);
        return -1;
    }

    if (harp_csv_parse_string(&cursor, &string) != 0)
    {
        return -1;
    }
    if (strcmp(string, "collocation_index") != 0)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "error reading 'collocation_index' in header");
        return -1;
    }
    if (harp_csv_parse_string(&cursor, &string) != 0)
    {
        return -1;
    }
    if (strcmp(string, "source_product_a") != 0)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "error reading 'source_product_a' in header");
        return -1;
    }
    if (harp_csv_parse_string(&cursor, &string) != 0)
    {
        return -1;
    }
    if (strcmp(string, "index_a") != 0)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "error reading 'index_a' in header");
        return -1;
    }
    if (harp_csv_parse_string(&cursor, &string) != 0)
    {
        return -1;
    }
    if (strcmp(string, "source_product_b") != 0)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "error reading 'source_product_b' in header");
        return -1;
    }
    if (harp_csv_parse_string(&cursor, &string) != 0)
    {
        return -1;
    }
    if (strcmp(string, "index_b") != 0)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "error reading 'index_b' in header");
        return -1;
    }
    while (*cursor != '\0')
    {
        char *variable_name;
        char *unit;

        if (harp_csv_parse_variable_name_and_unit(&cursor, &variable_name, &unit) != 0)
        {
            return -1;
        }
        if (harp_collocation_result_add_difference(collocation_result, variable_name, unit) != 0)
        {
            return -1;
        }
    }

    return 0;
}

static int read_pair(FILE *file, long min_collocation_index, long max_collocation_index,
                     const char *source_product_a_filter, const char *source_product_b_filter,
                     harp_collocation_result *collocation_result)
{
    char line[HARP_CSV_LINE_LENGTH + 1];
    char *cursor = line;
    long collocation_index;
    char *source_product_a;
    char *source_product_b;
    long index_a;
    long index_b;
    double *difference = NULL;
    long length;
    int i;

    if (fgets(line, HARP_CSV_LINE_LENGTH + 1, file) == NULL)
    {
        if (ferror(file))
        {
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "error reading line");
            return -1;
        }
        /* EOF */
        return 1;
    }

    length = (long)strlen(line);

    /* Trim the line */
    while (length > 0 && (line[length - 1] == '\r' || line[length - 1] == '\n'))
    {
        length--;
    }
    line[length] = '\0';

    if (length == HARP_CSV_LINE_LENGTH)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "line exceeds max line length (%ld)", HARP_CSV_LINE_LENGTH);
        return -1;
    }

    if (length == 0)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "empty line");
        return -1;
    }

    if (harp_csv_parse_long(&cursor, &collocation_index) != 0)
    {
        return -1;
    }

    /* skip line if collocation_index is outside the requested range */
    if (min_collocation_index >= 0 && collocation_index < min_collocation_index)
    {
        return 0;
    }
    if (max_collocation_index >= 0 && collocation_index > max_collocation_index)
    {
        return 0;
    }

    /* read pair and add it to the collocation_result */
    if (harp_csv_parse_string(&cursor, &source_product_a) != 0)
    {
        return -1;
    }
    if (source_product_a_filter != NULL)
    {
        if (strcmp(source_product_a, source_product_a_filter) != 0)
        {
            return 0;
        }
    }
    if (harp_csv_parse_long(&cursor, &index_a) != 0)
    {
        return -1;
    }
    if (harp_csv_parse_string(&cursor, &source_product_b) != 0)
    {
        return -1;
    }
    if (source_product_b_filter != NULL)
    {
        if (strcmp(source_product_b, source_product_b_filter) != 0)
        {
            return 0;
        }
    }
    if (harp_csv_parse_long(&cursor, &index_b) != 0)
    {
        return -1;
    }

    if (collocation_result->num_differences > 0)
    {
        difference = malloc(collocation_result->num_differences * sizeof(double));
        if (difference == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           collocation_result->num_differences * sizeof(double), __FILE__, __LINE__);
            return -1;
        }
        for (i = 0; i < collocation_result->num_differences; i++)
        {
            if (harp_csv_parse_double(&cursor, &difference[i]) != 0)
            {
                free(difference);
                return -1;
            }
        }
    }

    if (harp_collocation_result_add_pair(collocation_result, collocation_index, source_product_a, index_a,
                                         source_product_b, index_b, collocation_result->num_differences, difference)
        != 0)
    {
        if (difference != NULL)
        {
            free(difference);
        }
        return -1;
    }

    if (difference != NULL)
    {
        free(difference);
    }

    return 0;
}

int harp_collocation_result_read_range(const char *collocation_result_filename, long min_collocation_index,
                                       long max_collocation_index, const char *source_product_a,
                                       const char *source_product_b, harp_collocation_result **new_collocation_result)
{
    harp_collocation_result *collocation_result = NULL;
    FILE *file;
    int result = 0;

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

    /* Start new collocation result */
    if (harp_collocation_result_new(&collocation_result, 0, NULL, NULL) != 0)
    {
        fclose(file);
        return -1;
    }

    /* Initialize the collocation result and update the collocation differences with the information in the header */
    if (read_header(file, collocation_result) != 0)
    {
        harp_collocation_result_delete(collocation_result);
        fclose(file);
        return -1;
    }

    /* Read the matching pairs */
    while (result == 0)
    {
        result = read_pair(file, min_collocation_index, max_collocation_index, source_product_a, source_product_b,
                           collocation_result);
    }
    if (result < 0)
    {
        harp_collocation_result_delete(collocation_result);
        fclose(file);
        return -1;
    }

    /* Close the collocation result file */
    fclose(file);

    *new_collocation_result = collocation_result;
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
    return harp_collocation_result_read_range(collocation_result_filename, -1, -1, NULL, NULL, new_collocation_result);
}

/**
 * @}
 */

static void write_header(FILE *file, const harp_collocation_result *collocation_result)
{
    int i;

    fprintf(file, "collocation_index,source_product_a,index_a,source_product_b,index_b");
    for (i = 0; i < collocation_result->num_differences; i++)
    {
        fprintf(file, ",%s", collocation_result->difference_variable_name[i]);
        if (collocation_result->difference_unit[i] != NULL)
        {
            fprintf(file, " [%s]", collocation_result->difference_unit[i]);
        }
    }
    fprintf(file, "\n");
}

static void write_pair(FILE *file, const harp_collocation_result *collocation_result, long index)
{
    harp_collocation_pair *pair;
    int i;

    assert(collocation_result->pair != NULL);
    assert(index >= 0 && index < collocation_result->num_pairs);
    assert(collocation_result->pair[index] != NULL);

    pair = collocation_result->pair[index];

    /* Write filenames and measurement indices */
    fprintf(file, "%ld,%s,%ld,%s,%ld", pair->collocation_index,
            collocation_result->dataset_a->source_product[pair->product_index_a], pair->sample_index_a,
            collocation_result->dataset_b->source_product[pair->product_index_b], pair->sample_index_b);

    /* Write differences */
    for (i = 0; i < pair->num_differences; i++)
    {
        fprintf(file, ",%.8g", pair->difference[i]);
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
    fclose(file);

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
    long i;

    /* allocate memory for the result struct */
    result = (harp_collocation_result *)malloc(sizeof(harp_collocation_result));
    if (result == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_collocation_result), __FILE__, __LINE__);
        return -1;
    }
    result->dataset_a = collocation_result->dataset_a;
    result->dataset_b = collocation_result->dataset_b;
    result->num_differences = collocation_result->num_differences;
    result->difference_variable_name = collocation_result->difference_variable_name;
    result->difference_unit = collocation_result->difference_unit;
    result->num_pairs = collocation_result->num_pairs;
    result->pair = NULL;

    result->pair = malloc(collocation_result->num_pairs * sizeof(harp_collocation_pair *));
    if (result->pair == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       collocation_result->num_pairs * sizeof(harp_collocation_pair *), __FILE__, __LINE__);
        harp_collocation_result_shallow_delete(result);
        return -1;
    }
    for (i = 0; i < collocation_result->num_pairs; i++)
    {
        result->pair[i] = NULL;
    }

    for (i = 0; i < collocation_result->num_pairs; i++)
    {
        harp_collocation_pair *pair = collocation_result->pair[i];

        if (collocation_pair_new(pair->collocation_index, pair->product_index_a, pair->sample_index_a,
                                 pair->product_index_b, pair->sample_index_b, pair->num_differences, pair->difference,
                                 &result->pair[i]) != 0)
        {
            harp_collocation_result_shallow_delete(result);
            return -1;
        }
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
            }

            free(collocation_result->pair);
        }

        free(collocation_result);
    }
}
