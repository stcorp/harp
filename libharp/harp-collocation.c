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

#include "harp-internal.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LINE_LENGTH 1024
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

/** \addtogroup harp_collocation
 * @{
 */

/** Create a new collocation result entry
 * \param collocation_index Unique index of the pair in the overall collocation result
 * \param source_product_a Name of the source_product attribute of the product from dataset A
 * \param index_a Value of the index variable for the matching sample in the product from dataset A
 * \param source_product_b Name of the source_product attribute of the product from dataset B
 * \param index_b Value of the index variable for the matching sample in the product from dataset B
 * \param difference Array of difference values (should have length HARP_COLLOCATION_RESULT_MAX_NUM_DIFFERENCES)
 * \param new_pair Pointer to the C variable where the new result entry will be stored.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_collocation_pair_new(long collocation_index, const char *source_product_a, long index_a,
                                          const char *source_product_b, long index_b, const double *difference,
                                          harp_collocation_pair **new_pair)
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

    pair->source_product_a = strdup(source_product_a);
    if (pair->source_product_a == NULL)
    {
        harp_collocation_pair_delete(pair);
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        return -1;
    }
    pair->index_a = index_a;

    pair->source_product_b = strdup(source_product_b);
    if (pair->source_product_b == NULL)
    {
        harp_collocation_pair_delete(pair);
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        return -1;
    }
    pair->index_b = index_b;

    for (k = 0; k < HARP_COLLOCATION_RESULT_MAX_NUM_DIFFERENCES; k++)
    {
        pair->difference[k] = difference[k];
    }

    *new_pair = pair;
    return 0;
}

/** Remove a collocation result entry
 * \param pair Record that will be removed
 */
LIBHARP_API void harp_collocation_pair_delete(harp_collocation_pair *pair)
{
    if (pair == NULL)
    {
        return;
    }

    if (pair->source_product_a)
    {
        free(pair->source_product_a);
    }

    if (pair->source_product_b)
    {
        free(pair->source_product_b);
    }

    free(pair);
}

/** Create a duplicate of a collocation result entry
 * \param input_pair Result entry that needs to duplicated
 * \param new_pair Pointer to the C variable where the new result entry will be stored.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_collocation_pair_copy(const harp_collocation_pair *input_pair, harp_collocation_pair **new_pair)
{
    harp_collocation_pair *pair = NULL;
    int k;

    if (input_pair->source_product_a == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "source_product_a in line is NULL (%s:%u)", __FILE__, __LINE__);
        return -1;
    }
    if (input_pair->source_product_b == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "source_product_b in line is NULL (%s:%u)", __FILE__, __LINE__);
        return -1;
    }

    pair = (harp_collocation_pair *)malloc(sizeof(harp_collocation_pair));
    if (pair == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_collocation_pair), __FILE__, __LINE__);
        return -1;
    }

    pair->collocation_index = input_pair->collocation_index;
    pair->index_a = input_pair->index_a;
    pair->index_b = input_pair->index_b;

    pair->source_product_a = strdup(input_pair->source_product_a);
    if (pair->source_product_a == NULL)
    {
        harp_collocation_pair_delete(pair);
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        return -1;
    }

    pair->source_product_b = strdup(input_pair->source_product_b);
    if (pair->source_product_b == NULL)
    {
        harp_collocation_pair_delete(pair);
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        return -1;
    }

    for (k = 0; k < HARP_COLLOCATION_RESULT_MAX_NUM_DIFFERENCES; k++)
    {
        pair->difference[k] = input_pair->difference[k];
    }

    *new_pair = pair;
    return 0;
}

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
            harp_collocation_pair_delete(collocation_result->pair[i]);
        }
        free(collocation_result->pair);
    }

    free(collocation_result);
}

/**
 * @}
 */

static int compare_by_a(const void *a, const void *b)
{
    harp_collocation_pair *pair_a = *(harp_collocation_pair **)a;
    harp_collocation_pair *pair_b = *(harp_collocation_pair **)b;
    int result;

    result = strcmp(pair_a->source_product_a, pair_b->source_product_a);
    if (result != 0)
    {
        return result;
    }
    if (pair_a->index_a < pair_b->index_a)
    {
        return -1;
    }
    if (pair_a->index_a > pair_b->index_a)
    {
        return 1;
    }

    /* If a is equal, then further sort by b to get a fixed ordering. */
    result = strcmp(pair_a->source_product_b, pair_b->source_product_b);
    if (result != 0)
    {
        return result;
    }
    if (pair_a->index_b < pair_b->index_b)
    {
        return -1;
    }
    if (pair_a->index_b > pair_b->index_b)
    {
        return 1;
    }

    return 0;
}

static int compare_by_b(const void *a, const void *b)
{
    harp_collocation_pair *pair_a = *(harp_collocation_pair **)a;
    harp_collocation_pair *pair_b = *(harp_collocation_pair **)b;
    int result;

    result = strcmp(pair_a->source_product_b, pair_b->source_product_b);
    if (result != 0)
    {
        return result;
    }
    if (pair_a->index_b < pair_b->index_b)
    {
        return -1;
    }
    if (pair_a->index_b > pair_b->index_b)
    {
        return 1;
    }

    /* If b is equal, then further sort by a to get a fixed ordering. */
    result = strcmp(pair_a->source_product_a, pair_b->source_product_a);
    if (result != 0)
    {
        return result;
    }
    if (pair_a->index_a < pair_b->index_a)
    {
        return -1;
    }
    if (pair_a->index_a > pair_b->index_a)
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
 * Results will be sorted first by product source name of A and then by sample index of A
 * \param collocation_result Result set that will be sorted in place.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_collocation_result_sort_by_a(harp_collocation_result *collocation_result)
{
    qsort(collocation_result->pair, collocation_result->num_pairs, sizeof(harp_collocation_pair *), compare_by_a);
    return 0;
}

/** Sort the collocation result pairs by dataset B
 * Results will be sorted first by product source name of B and then by sample index of B
 * \param collocation_result Result set that will be sorted in place.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_collocation_result_sort_by_b(harp_collocation_result *collocation_result)
{
    qsort(collocation_result->pair, collocation_result->num_pairs, sizeof(harp_collocation_pair *), compare_by_b);
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
    long i, j;

    for (i = collocation_result->num_pairs - 1; i >= 0; i--)
    {
        if (strcmp(collocation_result->pair[i]->source_product_a, source_product) != 0)
        {
            harp_collocation_pair_delete(collocation_result->pair[i]);
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
    long i, j;

    for (i = collocation_result->num_pairs - 1; i >= 0; i--)
    {
        if (strcmp(collocation_result->pair[i]->source_product_b, source_product) != 0)
        {
            harp_collocation_pair_delete(collocation_result->pair[i]);
            for (j = i + 1; j < collocation_result->num_pairs; j++)
            {
                collocation_result->pair[j - 1] = collocation_result->pair[j];
            }
            collocation_result->num_pairs--;
        }
    }
    return 0;
}

/** Add collocation result entry to a result set
 * \note this function will not check for uniqueness of the collocation_index values in the resulting set
 * \param collocation_result Result set that will be extended
 * \param pair Single collocation result entry that will be added.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_collocation_result_add_pair(harp_collocation_result *collocation_result,
                                                 harp_collocation_pair *pair)
{
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
            return -1;
        }

        collocation_result->pair = new_pair;
    }

    collocation_result->pair[collocation_result->num_pairs] = pair;
    collocation_result->num_pairs++;
    return 0;
}

/**
 * @}
 */

static int get_num_lines(FILE *file, const char *filename, long *new_num_lines)
{
    long length;
    char LINE[LINE_LENGTH];
    long num_lines = 0;

    while (fgets(LINE, LINE_LENGTH, file) != NULL)
    {
        /* Trim the line */
        length = (long)strlen(LINE);
        while (length > 0 && (LINE[length - 1] == '\r' || LINE[length - 1] == '\n'))
        {
            length--;
        }
        LINE[length] = '\0';

        /* Do not allow empty lines */
        if (length == 1)
        {
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "empty line in file '%s'", filename);
            return -1;
        }

        num_lines++;
    }

    *new_num_lines = num_lines;
    return 0;
}

static void parse_double(char **str, double *value)
{
    char *cursor = *str;
    int stringlength = 0;

    *value = harp_nan();

    /* Skip leading white space */
    while (*cursor == ' ')
    {
        cursor++;
    }

    /* Grab string */
    while (cursor[stringlength] != ',' && cursor[stringlength] != '\0')
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
    sscanf(cursor, "%lf", value);
}

static void parse_long(char **str, long *value)
{
    char *cursor = *str;
    size_t stringlength = 0;

    *value = 0;

    /* Skip leading white space */
    while (*cursor == ' ')
    {
        cursor++;
    }

    /* Grab string */
    while (cursor[stringlength] != ',' && cursor[stringlength] != '\0')
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
    sscanf(cursor, "%ld", value);
}

static void parse_string(char **str, char **value)
{
    char *cursor = *str;
    int stringlength = 0;

    /* Skip leading white space */
    while (*cursor == ' ')
    {
        cursor++;
    }

    /* Grab string */
    while (cursor[stringlength] != ',' && cursor[stringlength] != '\0')
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
    *value = cursor;
}

static int parse_difference_type_and_unit(char **str, harp_collocation_difference_type *difference_type, char **unit)
{
    char *cursor = *str;
    int stringlength = 0;

    /* Skip leading white space */
    while (*cursor == ' ')
    {
        cursor++;
    }

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
    char line[LINE_LENGTH];
    char *cursor = line;
    char *string = NULL;
    size_t length;

    rewind(file);

    if (fgets(line, LINE_LENGTH, file) == NULL)
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

    parse_string(&cursor, &string);
    if (strcmp(string, "collocation_index") != 0)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "error reading 'collocation_index' in header");
        return -1;
    }
    parse_string(&cursor, &string);
    if (strcmp(string, "source_product_a") != 0)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "error reading 'source_product_a' in header");
        return -1;
    }
    parse_string(&cursor, &string);
    if (strcmp(string, "index_a") != 0)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "error reading 'index_a' in header");
        return -1;
    }
    parse_string(&cursor, &string);
    if (strcmp(string, "source_product_b") != 0)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "error reading 'source_product_b' in header");
        return -1;
    }
    parse_string(&cursor, &string);
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

static int read_pair(FILE *file, const harp_collocation_result *collocation_result, harp_collocation_pair **pair)
{
    char line[LINE_LENGTH];
    char *cursor = line;
    long collocation_index;
    char *source_product_a;
    char *source_product_b;
    long index_a;
    long index_b;
    double differences[HARP_COLLOCATION_RESULT_MAX_NUM_DIFFERENCES];
    size_t length;
    int k;

    if (fgets(line, LINE_LENGTH, file) == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "error reading line");
        return -1;
    }

    /* trim end-of-line */
    length = strlen(line);
    while (length > 0 && (line[length - 1] == '\r' || line[length - 1] == '\n'))
    {
        length--;
    }
    line[length] = '\0';

    /* Parse line */
    parse_long(&cursor, &collocation_index);
    parse_string(&cursor, &source_product_a);
    parse_long(&cursor, &index_a);
    parse_string(&cursor, &source_product_b);
    parse_long(&cursor, &index_b);
    for (k = 0; k < HARP_COLLOCATION_RESULT_MAX_NUM_DIFFERENCES; k++)
    {
        if (collocation_result->difference_available[k] && k != harp_collocation_difference_delta)
        {
            parse_double(&cursor, &differences[k]);
        }
        else
        {
            differences[k] = harp_nan();
        }
    }

    if (harp_collocation_pair_new(collocation_index, source_product_a, index_a, source_product_b,
                                  index_b, differences, pair) != 0)
    {
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
    if (get_num_lines(file, collocation_result_filename, &num_lines) != 0)
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
        harp_collocation_pair *pair = NULL;

        if (read_pair(file, collocation_result, &pair) != 0)
        {
            harp_collocation_result_delete(collocation_result);
            fclose(file);
            return -1;
        }

        if (harp_collocation_result_add_pair(collocation_result, pair) != 0)
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
            collocation_result->pair[i]->source_product_a, collocation_result->pair[i]->index_a,
            collocation_result->pair[i]->source_product_b, collocation_result->pair[i]->index_b);

    /* Write differences */
    /* don't write harp_collocation_difference_delta, so stop at HARP_COLLOCATION_RESULT_MAX_NUM_DIFFERENCES - 1 */
    for (k = 0; k < HARP_COLLOCATION_RESULT_MAX_NUM_DIFFERENCES - 1; k++)
    {
        if (collocation_result->difference_available[k])
        {
            fprintf(file, ",%.8e", collocation_result->pair[i]->difference[k]);
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

/**
 * @}
 */
