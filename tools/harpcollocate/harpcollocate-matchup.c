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

#include "harpcollocate.h"

#include <assert.h>

typedef enum Reduced_product_variable_type_enum
{
    reduced_product_variable_type_index = 0,
    reduced_product_variable_type_datetime = 1,
    reduced_product_variable_type_latitude = 2,
    reduced_product_variable_type_longitude = 3,
    reduced_product_variable_type_latitude_bounds = 4,
    reduced_product_variable_type_longitude_bounds = 5,
    reduced_product_variable_type_sza = 6,
    reduced_product_variable_type_saa = 7,
    reduced_product_variable_type_vza = 8,
    reduced_product_variable_type_vaa = 9,
    reduced_product_variable_type_theta = 10
} Reduced_product_variable_type;

/* Reduced data product that contains only the parameters that are needed for collocation. These are datetime, latitude,
 * longitude, and measurement geometry parameters.
 */
typedef struct Reduced_product_struct
{
    char *filename;
    char *source_product;
    harp_variable *index;
    harp_variable *datetime;
    harp_variable *latitude;
    harp_variable *longitude;
    harp_variable *latitude_bounds;
    harp_variable *longitude_bounds;
    harp_variable *sza;
    harp_variable *saa;
    harp_variable *vza;
    harp_variable *vaa;
    harp_variable *theta;
} Reduced_product;

/* Define the cache to store multiple reduced_products. */
typedef struct Cache_struct
{
    int num_files;
    int *file_is_needed;
    double *datetime_start;
    double *datetime_stop;
    int num_subset_files;
    Reduced_product **reduced_product;
} Cache;

void reduced_product_delete(Reduced_product *reduced_product)
{
    if (reduced_product != NULL)
    {
        if (reduced_product->filename != NULL)
        {
            free(reduced_product->filename);
        }

        if (reduced_product->source_product != NULL)
        {
            free(reduced_product->source_product);
        }

        if (reduced_product->index != NULL)
        {
            harp_variable_delete(reduced_product->index);
        }

        if (reduced_product->datetime != NULL)
        {
            harp_variable_delete(reduced_product->datetime);
        }

        if (reduced_product->latitude != NULL)
        {
            harp_variable_delete(reduced_product->latitude);
        }

        if (reduced_product->longitude != NULL)
        {
            harp_variable_delete(reduced_product->longitude);
        }

        if (reduced_product->latitude_bounds != NULL)
        {
            harp_variable_delete(reduced_product->latitude_bounds);
        }

        if (reduced_product->longitude_bounds != NULL)
        {
            harp_variable_delete(reduced_product->longitude_bounds);
        }

        if (reduced_product->sza != NULL)
        {
            harp_variable_delete(reduced_product->sza);
        }

        if (reduced_product->saa != NULL)
        {
            harp_variable_delete(reduced_product->saa);
        }

        if (reduced_product->vza != NULL)
        {
            harp_variable_delete(reduced_product->vza);
        }

        if (reduced_product->vaa != NULL)
        {
            harp_variable_delete(reduced_product->vaa);
        }

        if (reduced_product->theta != NULL)
        {
            harp_variable_delete(reduced_product->theta);
        }

        free(reduced_product);
    }
}

static void cache_delete(Cache *cache)
{
    if (cache != NULL)
    {
        if (cache->file_is_needed != NULL)
        {
            free(cache->file_is_needed);
        }

        if (cache->datetime_start != NULL)
        {
            free(cache->datetime_start);
        }

        if (cache->datetime_stop != NULL)
        {
            free(cache->datetime_stop);
        }

        if (cache->reduced_product != NULL)
        {
            int i;

            for (i = 0; i < cache->num_files; i++)
            {
                reduced_product_delete(cache->reduced_product[i]);
            }

            free(cache->reduced_product);
        }

        free(cache);
    }
}

static int cache_new(Cache **new_cache, int num_files)
{
    Cache *cache = NULL;
    Reduced_product **reduced_product = NULL;
    int i;

    cache = (Cache *)malloc(sizeof(Cache));
    if (cache == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)", sizeof(Cache),
                       __FILE__, __LINE__);
        return -1;
    }

    cache->num_files = num_files;

    cache->file_is_needed = calloc((size_t)num_files, sizeof(int));
    if (cache->file_is_needed == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       (size_t)num_files * sizeof(int), __FILE__, __LINE__);
        cache_delete(cache);
        return -1;
    }

    cache->datetime_start = calloc((size_t)num_files, sizeof(double));
    if (cache->datetime_start == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       (size_t)num_files * sizeof(double), __FILE__, __LINE__);
        cache_delete(cache);
        return -1;
    }

    cache->datetime_stop = calloc((size_t)num_files, sizeof(double));
    if (cache->datetime_stop == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       (size_t)num_files * sizeof(double), __FILE__, __LINE__);
        cache_delete(cache);
        return -1;
    }

    cache->num_subset_files = 0;

    reduced_product = malloc(num_files * sizeof(Reduced_product *));
    if (reduced_product == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_files * sizeof(Reduced_product *), __FILE__, __LINE__);
        cache_delete(cache);
        return -1;
    }

    cache->reduced_product = reduced_product;

    for (i = 0; i < num_files; i++)
    {
        cache->reduced_product[i] = NULL;
    }

    *new_cache = cache;
    return 0;
}

static int cache_add_reduced_product(Cache *cache, Reduced_product *reduced_product, int index)
{
    if (cache->reduced_product[index] != NULL)
    {
        /* Reduced product is already in the cache, keep it */
        return 0;
    }

    cache->reduced_product[index] = reduced_product;
    cache->num_subset_files++;
    return 0;
}

static int cache_detach_product(Cache *cache, Reduced_product *reduced_product)
{
    int i;
    int found_reduced_product = 0;

    if (reduced_product == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "could not detach empty reduced product");
        return -1;
    }

    for (i = 0; i < cache->num_files; i++)
    {
        if (cache->reduced_product[i] == reduced_product)
        {
            found_reduced_product = 1;
            cache->reduced_product[i] = NULL;
            cache->num_subset_files--;
        }
    }

    if (!found_reduced_product)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "could not find reduced product '%s' in cache",
                       reduced_product->filename);
        return -1;
    }

    return 0;
}

static int cache_b_update(Cache *cache_b, const Collocation_options *collocation_options, const Dataset *dataset_a,
                          int i)
{
    double datetime_start_a = dataset_a->datetime_start[i];
    double dt;
    int j;

    if (collocation_options->criterion_is_set[collocation_criterion_type_time] != 1)
    {
        /* Do not remove any files from the cache */
        return 0;
    }

    /* Enlarge the datetime range */
    dt = collocation_options->criterion[collocation_criterion_type_time]->value;

    if (cache_b == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "cache is NULL");
        return -1;
    }

    /* Remove a reduced_product b from the cache when it is outdated:
     * the stop time of reduced_product b is before the start time of reduced_product a */
    for (j = 0; j < cache_b->num_files; j++)
    {
        if (cache_b->datetime_stop[j] + dt <= datetime_start_a)
        {
            Reduced_product *reduced_product = cache_b->reduced_product[j];

            if (reduced_product != NULL)
            {
                if (cache_detach_product(cache_b, reduced_product) != 0)
                {
                    return -1;
                }
                reduced_product_delete(reduced_product);
            }
        }
    }

    return 0;
}

static int reduced_product_new(Reduced_product **new_reduced_product)
{
    Reduced_product *reduced_product = NULL;

    reduced_product = (Reduced_product *)malloc(sizeof(Reduced_product));
    if (reduced_product == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(Reduced_product), __FILE__, __LINE__);
        return -1;
    }

    reduced_product->filename = NULL;
    reduced_product->source_product = NULL;
    reduced_product->index = NULL;
    reduced_product->datetime = NULL;
    reduced_product->latitude = NULL;
    reduced_product->longitude = NULL;
    reduced_product->latitude_bounds = NULL;
    reduced_product->longitude_bounds = NULL;
    reduced_product->sza = NULL;
    reduced_product->saa = NULL;
    reduced_product->vza = NULL;
    reduced_product->vaa = NULL;
    reduced_product->theta = NULL;

    *new_reduced_product = reduced_product;

    return 0;
}

static void get_variable_name_and_unit_from_variable_type(const Reduced_product_variable_type variable_type,
                                                          const char **variable_name, const char **unit)
{
    switch (variable_type)
    {
        case reduced_product_variable_type_index:
            *variable_name = "index";
            *unit = NULL;
            break;

        case reduced_product_variable_type_datetime:
            *variable_name = "datetime";
            *unit = HARP_UNIT_DATETIME;
            break;

        case reduced_product_variable_type_latitude:
            *variable_name = "latitude";
            *unit = HARP_UNIT_LATITUDE;
            break;

        case reduced_product_variable_type_longitude:
            *variable_name = "longitude";
            *unit = HARP_UNIT_LONGITUDE;
            break;

        case reduced_product_variable_type_latitude_bounds:
            *variable_name = "latitude_bounds";
            *unit = HARP_UNIT_LATITUDE;
            break;

        case reduced_product_variable_type_longitude_bounds:
            *variable_name = "longitude_bounds";
            *unit = HARP_UNIT_LONGITUDE;
            break;

        case reduced_product_variable_type_sza:
            *variable_name = "solar_zenith_angle";
            *unit = HARP_UNIT_ANGLE;
            break;

        case reduced_product_variable_type_saa:
            *variable_name = "solar_azimuth_angle";
            *unit = HARP_UNIT_ANGLE;
            break;

        case reduced_product_variable_type_vza:
            *variable_name = "viewing_zenith_angle";
            *unit = HARP_UNIT_ANGLE;
            break;

        case reduced_product_variable_type_vaa:
            *variable_name = "viewing_azimuth_angle";
            *unit = HARP_UNIT_ANGLE;
            break;

        case reduced_product_variable_type_theta:
            *variable_name = "scattering_angle";
            *unit = HARP_UNIT_ANGLE;
            break;

        default:
            assert(0);
            exit(1);
    }
}

static int get_derived_variable(harp_product *product, Reduced_product_variable_type variable_type,
                                harp_variable **new_variable)
{
    harp_dimension_type dimension_type = harp_dimension_time;
    char *variable_name = NULL;
    char *unit = NULL;

    /* Get target variable name and unit. */
    get_variable_name_and_unit_from_variable_type(variable_type, (const char **)&variable_name, (const char **)&unit);

    if (variable_type == reduced_product_variable_type_latitude_bounds ||
        variable_type == reduced_product_variable_type_longitude_bounds)
    {
        harp_dimension_type dims[] = { harp_dimension_time, harp_dimension_independent };

        /* Get the derived parameter from the product. */
        return harp_product_get_derived_variable(product, variable_name, unit, 2, dims, new_variable);
    }
    else
    {
        /* Get the derived parameter from the product. */
        return harp_product_get_derived_variable(product, variable_name, unit, 1, &dimension_type, new_variable);
    }
}

/* dataset_id is 0 for dataset A and 1 for dataset B */
static int reduced_product_import(const char *path, const Collocation_options *collocation_options, int dataset_id,
                                  Reduced_product **new_reduced_product)
{
    harp_product *product = NULL;
    Reduced_product *reduced_product = NULL;

    /* Import the product. */
    if (harp_import(path, &product) != 0)
    {
        return -1;
    }

    /* Create the reduced product. */
    if (reduced_product_new(&reduced_product) != 0)
    {
        harp_product_delete(product);
        return -1;
    }

    /* Set the filename. */
    reduced_product->filename = strdup(harp_basename(path));
    if (reduced_product->filename == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        reduced_product_delete(reduced_product);
        harp_product_delete(product);
        return -1;
    }

    /* Set the source product. */
    if (product->source_product != NULL)
    {
        reduced_product->source_product = strdup(product->source_product);
    }
    else
    {
        reduced_product->source_product = strdup(harp_basename(path));
    }

    if (reduced_product->source_product == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        reduced_product_delete(reduced_product);
        harp_product_delete(product);
        return -1;
    }

    /* Get index variable. */
    if (get_derived_variable(product, reduced_product_variable_type_index, &reduced_product->index) != 0)
    {
        reduced_product_delete(reduced_product);
        harp_product_delete(product);
        return -1;
    }

    if (collocation_options->criterion_is_set[collocation_criterion_type_time])
    {
        /* Get datetime variable. */
        if (get_derived_variable(product, reduced_product_variable_type_datetime, &reduced_product->datetime) != 0)
        {
            reduced_product_delete(reduced_product);
            harp_product_delete(product);
            return -1;
        }
    }

    /* Get latitude and longitude variables. */
    if (collocation_options->criterion_is_set[collocation_criterion_type_latitude]
        || collocation_options->criterion_is_set[collocation_criterion_type_longitude]
        || collocation_options->criterion_is_set[collocation_criterion_type_point_distance]
        || (dataset_id == 0 && collocation_options->criterion_is_set[collocation_criterion_type_point_a_in_area_b])
        || (dataset_id == 1 && collocation_options->criterion_is_set[collocation_criterion_type_point_b_in_area_a]))
    {
        if (get_derived_variable(product, reduced_product_variable_type_latitude, &reduced_product->latitude) != 0)
        {
            reduced_product_delete(reduced_product);
            harp_product_delete(product);
            return -1;
        }

        if (get_derived_variable(product, reduced_product_variable_type_longitude, &reduced_product->longitude) != 0)
        {
            reduced_product_delete(reduced_product);
            harp_product_delete(product);
            return -1;
        }
    }

    /* Get latitude_bounds and longitude_bounds variables. */
    if (collocation_options->criterion_is_set[collocation_criterion_type_overlapping]
        || collocation_options->criterion_is_set[collocation_criterion_type_overlapping_percentage]
        || (dataset_id == 0 && collocation_options->criterion_is_set[collocation_criterion_type_point_b_in_area_a])
        || (dataset_id == 1 && collocation_options->criterion_is_set[collocation_criterion_type_point_a_in_area_b]))
    {
        if (get_derived_variable(product, reduced_product_variable_type_latitude_bounds,
                                 &reduced_product->latitude_bounds) != 0)
        {
            reduced_product_delete(reduced_product);
            harp_product_delete(product);
            return -1;
        }

        if (get_derived_variable(product, reduced_product_variable_type_longitude_bounds,
                                 &reduced_product->longitude_bounds) != 0)
        {
            reduced_product_delete(reduced_product);
            harp_product_delete(product);
            return -1;
        }
    }

    /* Get all relevant geometry angles. */
    if (collocation_options->criterion_is_set[collocation_criterion_type_sza])
    {
        if (get_derived_variable(product, reduced_product_variable_type_sza, &reduced_product->sza) != 0)
        {
            reduced_product_delete(reduced_product);
            harp_product_delete(product);
            return -1;
        }
    }

    if (collocation_options->criterion_is_set[collocation_criterion_type_saa])
    {
        if (get_derived_variable(product, reduced_product_variable_type_saa, &reduced_product->saa) != 0)
        {
            reduced_product_delete(reduced_product);
            harp_product_delete(product);
            return -1;
        }
    }

    if (collocation_options->criterion_is_set[collocation_criterion_type_vza])
    {
        if (get_derived_variable(product, reduced_product_variable_type_vza, &reduced_product->vza) != 0)
        {
            reduced_product_delete(reduced_product);
            harp_product_delete(product);
            return -1;
        }
    }

    if (collocation_options->criterion_is_set[collocation_criterion_type_vaa])
    {
        if (get_derived_variable(product, reduced_product_variable_type_vaa, &reduced_product->vaa) != 0)
        {
            reduced_product_delete(reduced_product);
            harp_product_delete(product);
            return -1;
        }
    }

    if (collocation_options->criterion_is_set[collocation_criterion_type_theta])
    {
        if (get_derived_variable(product, reduced_product_variable_type_theta, &reduced_product->theta) != 0)
        {
            reduced_product_delete(reduced_product);
            harp_product_delete(product);
            return -1;
        }
    }

    harp_product_delete(product);

    *new_reduced_product = reduced_product;
    return 0;
}

/* Determine which subset of files in dataset need to be considered. Open only the files which overlap in time. */
static int dataset_b_determine_subset(Cache *cache_b, const Collocation_options *collocation_options,
                                      const Dataset *dataset_a, int i)
{
    double datetime_start_a = dataset_a->datetime_start[i];
    double datetime_stop_a = dataset_a->datetime_stop[i];
    int j;

    /* No optimization: just open all files */
    if (collocation_options->criterion_is_set[collocation_criterion_type_time] != 1)
    {
        for (j = 0; j < cache_b->num_files; j++)
        {
            cache_b->file_is_needed[j] = 1;
        }

        cache_b->num_subset_files = cache_b->num_files;
        return 0;
    }

    /* When dt is set, make the datetime range slightly larger */
    if (collocation_options->criterion_is_set[collocation_criterion_type_time])
    {
        double dt = collocation_options->criterion[collocation_criterion_type_time]->value;     /* in collocation unit */

        datetime_start_a = datetime_start_a - dt;
        datetime_stop_a = datetime_stop_a + dt;
    }

    cache_b->num_subset_files = 0;

    for (j = 0; j < cache_b->num_files; j++)
    {
        /* When datetime ranges overlap, the file needs to be used for matchup */
        if (cache_b->datetime_stop[j] >= datetime_start_a && cache_b->datetime_start[j] <= datetime_stop_a)
        {
            cache_b->file_is_needed[j] = 1;
            cache_b->num_subset_files++;
        }
        else
        {
            cache_b->file_is_needed[j] = 0;
        }
    }

    return 0;
}

/* Add difference to collocation result if collocation criterion is set */
static void collocation_result_add_difference(harp_collocation_result *collocation_result,
                                              const Collocation_options *collocation_options,
                                              const Collocation_criterion_type criterion_type,
                                              const harp_collocation_difference_type difference_type)
{
    collocation_result->difference_available[difference_type] = 1;

    /* Copy the original unit that needs to be used in the collocation result (set by user) */
    if (collocation_result->difference_unit[difference_type] != NULL)
    {
        free(collocation_result->difference_unit[difference_type]);
    }
    collocation_result->difference_unit[difference_type] =
        strdup(collocation_options->criterion[criterion_type]->original_unit);
    assert(collocation_result->difference_unit[difference_type] != NULL);
}

/* Set the format of the collocation result lines */
static int collocation_result_init(harp_collocation_result *collocation_result,
                                   const Collocation_options *collocation_options)
{
    int i;

    /* Add difference for each collocation criterion that is set, except for:
     * dlat, dlon */
    for (i = 0; i < MAX_NUM_COLLOCATION_CRITERIA; i++)
    {
        if (i == collocation_criterion_type_latitude || i == collocation_criterion_type_longitude ||
            i == collocation_criterion_type_point_a_in_area_b || i == collocation_criterion_type_point_b_in_area_a)
        {
            continue;
        }

        if (collocation_options->criterion_is_set[i])
        {
            harp_collocation_difference_type difference_type = harp_collocation_difference_unknown;

            /* Determine the difference index k from the collocation criterion */
            Collocation_criterion_type criterion_type = collocation_options->criterion[i]->type;

            get_difference_type_from_collocation_criterion_type(criterion_type, &difference_type);

            if (difference_type == harp_collocation_difference_unknown)
            {
                harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "unable to derive difference type for collocation"
                               " criterion '%s'",
                               collocation_criterion_command_line_option_from_criterion_type(criterion_type));
                return -1;
            }

            collocation_result_add_difference(collocation_result, collocation_options, criterion_type, difference_type);
        }
    }

    /* If dlat or dlon is set, add point distance as well */
    if ((collocation_options->criterion_is_set[collocation_criterion_type_latitude]
         || collocation_options->criterion_is_set[collocation_criterion_type_longitude])
        && collocation_options->criterion_is_set[collocation_criterion_type_point_distance] != 1)
    {
        /* collocation_result_add_difference(collocation_result,
           collocation_options,
           collocation_criterion_type_point_distance, harp_collocation_difference_point_distance); */
    }

    /* Add the weighted norm of all the differences that are set */
    collocation_result->difference_available[harp_collocation_difference_delta] = 1;
    if (collocation_result->difference_unit[harp_collocation_difference_delta] != NULL)
    {
        free(collocation_result->difference_unit[harp_collocation_difference_delta]);
    }
    collocation_result->difference_unit[harp_collocation_difference_delta] = strdup("");
    assert(collocation_result->difference_unit[harp_collocation_difference_delta] != NULL);

    return 0;
}

/* Calculate delta from:

     absolute_difference_in_time
     point_distance
     absolute_difference_in_sza
     absolute_difference_in_saa
     absolute_difference_in_vza
     absolute_difference_in_vaa
     absolute_difference_in_theta

     overlapping_percentage

    (absolute_difference_in_latitude,
     absolute_difference_in_longitude,
     )
 */
int calculate_delta(const harp_collocation_result *collocation_result, const Collocation_options *collocation_options,
                    harp_collocation_pair *pair, double *new_delta)
{
    int k;
    double scaling_factor;
    int count_num_differences = 0;
    double delta = 0.0;

    /* TODO: Find out why point_distance_has_been_used is set but not used in this function. */
    // int point_distance_has_been_used = 0;

    for (k = 0; k < HARP_COLLOCATION_RESULT_MAX_NUM_DIFFERENCES; k++)
    {
        /* Determine which difference are used from collocation result data structure */
        if (collocation_result->difference_available[k])
        {
            /* Normal behaviour differences */
            if (k == harp_collocation_difference_absolute_time || k == harp_collocation_difference_point_distance ||
                k == harp_collocation_difference_absolute_sza || k == harp_collocation_difference_absolute_saa ||
                k == harp_collocation_difference_absolute_vza || k == harp_collocation_difference_absolute_vaa ||
                k == harp_collocation_difference_absolute_theta)
            {
                if (collocation_options->weighting_factor[k] == NULL)
                {
                    harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "weighting factor '%s' is not set",
                                   weighting_factor_command_line_option_from_difference_type(k));
                    return -1;
                }

                if (collocation_options->weighting_factor[k]->value < 0.0)
                {
                    harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "invalid weighting factor value '%g' < 0",
                                   collocation_options->weighting_factor[k]->value);
                    return -1;
                }

                if (collocation_options->weighting_factor[k]->difference_type != k)
                {
                    harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "inconsistent weighting factor difference type",
                                   collocation_options->weighting_factor[k]->value);
                    return -1;
                }

                scaling_factor = collocation_options->weighting_factor[k]->value;
                delta += (scaling_factor * pair->difference[k] * scaling_factor * pair->difference[k]);
                count_num_differences++;
                // if (k == harp_collocation_difference_point_distance)
                // {
                //     point_distance_has_been_used = 1;
                // }
            }
            else if (k == harp_collocation_difference_overlapping_percentage)
            {
                if (collocation_options->weighting_factor[k] == NULL)
                {
                    harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "weighting factor '%s' is not set",
                                   weighting_factor_command_line_option_from_difference_type(k));
                    return -1;
                }

                if (collocation_options->weighting_factor[k]->value < 0.0)
                {
                    harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "weighting factor value of '%s' (%g) must be larger or "
                                   "equal to zero", weighting_factor_command_line_option_from_difference_type(k),
                                   collocation_options->weighting_factor[k]->value);
                    return -1;
                }

                if (collocation_options->weighting_factor[k]->difference_type != k)
                {
                    harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "inconsistent difference type for weighting factor "
                                   "'%s'", weighting_factor_command_line_option_from_difference_type(k));
                    return -1;
                }

                /* Take the inverse for overlapping percentage; this criterion works the other way around:
                 * a large overlapping percentage is good (in contrast to a large absolute or relative difference) */
                if (collocation_options->weighting_factor[k]->value <= 0.0)
                {
                    harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "weighting factor value of '%s' (%g) must be larger "
                                   "than zero", weighting_factor_command_line_option_from_difference_type(k),
                                   collocation_options->weighting_factor[k]->value);
                    return -1;
                }
                scaling_factor = 1.0 / collocation_options->weighting_factor[k]->value;
                delta += (scaling_factor * pair->difference[k] * scaling_factor * pair->difference[k]);
                count_num_differences++;
            }
        }
    }

    delta = sqrt(delta / count_num_differences);
    pair->difference[harp_collocation_difference_delta] = delta;
    *new_delta = delta;

    return 0;
}

/* Matchup two measurements in point distance */
static int matchup_two_measurements_in_point_distance(const Reduced_product *reduced_product_a,
                                                      const Reduced_product *reduced_product_b,
                                                      const Collocation_options *collocation_options,
                                                      const long index_a, const long index_b,
                                                      const Collocation_criterion_type criterion_type,
                                                      double *new_point_distance, int *match)
{
    double latitude_a;
    double longitude_a;
    double latitude_b;
    double longitude_b;
    double point_distance;
    double ds;

    /* Make some checks */
    if (criterion_type != collocation_criterion_type_point_distance)
    {
        harp_set_error(HARP_ERROR_INVALID_TYPE, "incorrect collocation criterion");
        return -1;
    }
    if (collocation_options->criterion[criterion_type] == NULL ||
        collocation_options->criterion_is_set[criterion_type] != 1)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "collocation criterion %d is not set", criterion_type);
        return -1;
    }

    /* Grab the point distance criterion */
    ds = collocation_options->criterion[criterion_type]->value;

    /* Grab latitude and longitude of measurement A */
    if (reduced_product_a->latitude == NULL || reduced_product_a->longitude == NULL)
    {
        harp_set_error(HARP_ERROR_NO_DATA, "latitude and longitude not in product '%s' (dataset a)",
                       reduced_product_a->filename);
        return -1;
    }
    latitude_a = reduced_product_a->latitude->data.double_data[index_a];
    longitude_a = reduced_product_a->longitude->data.double_data[index_a];

    /* Grab latitude and longitude of measurement B */
    if (reduced_product_b->latitude == NULL || reduced_product_b->longitude == NULL)
    {
        harp_set_error(HARP_ERROR_NO_DATA, "latitude and longitude not in product '%s' (dataset b)",
                       reduced_product_b->filename);
        return -1;
    }
    latitude_b = reduced_product_b->latitude->data.double_data[index_b];
    longitude_b = reduced_product_b->longitude->data.double_data[index_b];

    /* Calculate the distance between two points on the surface of the Earth sphere in [m] */
    if (harp_geometry_get_point_distance(longitude_a, latitude_a, longitude_b, latitude_b, &point_distance) != 0)
    {
        return -1;
    }

    assert(harp_isnan(point_distance) || point_distance >= 0.0);

    *match = (point_distance <= ds);
    *new_point_distance = point_distance;

    return 0;
}

/* Matchup: point in area? */
static int matchup_two_measurements_point_in_area(const Reduced_product *reduced_product_points,
                                                  const Reduced_product *reduced_product_polygons,
                                                  const long index_point, const long index_polygon, int *match)
{
    double *longitude_bounds;
    double *latitude_bounds;
    int num_vertices;

    if (reduced_product_points->latitude == NULL || reduced_product_points->longitude == NULL)
    {
        harp_set_error(HARP_ERROR_NO_DATA, "latitude and longitude not in product with point measurements");
        return -1;
    }
    if (reduced_product_polygons->latitude_bounds == NULL || reduced_product_polygons->longitude_bounds == NULL)
    {
        harp_set_error(HARP_ERROR_NO_DATA, "latitude bounds and longitude bounds not in product with polygon area "
                       "measurements");
        return -1;
    }
    if (reduced_product_polygons->latitude_bounds->num_dimensions != 2 ||
        reduced_product_polygons->longitude_bounds->num_dimensions != 2)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "latitude bounds and longitude bounds must be 2D");
        return -1;
    }

    num_vertices = reduced_product_polygons->latitude_bounds->dimension[1];
    longitude_bounds = &reduced_product_polygons->longitude_bounds->data.double_data[index_polygon * num_vertices];
    latitude_bounds = &reduced_product_polygons->latitude_bounds->data.double_data[index_polygon * num_vertices];
    return harp_geometry_has_point_in_area(reduced_product_points->longitude->data.double_data[index_point],
                                           reduced_product_points->latitude->data.double_data[index_point],
                                           num_vertices, longitude_bounds, latitude_bounds, match);
}

/* Matchup: do areas overlap? */
static int matchup_two_measurements_areas_in_areas(const Reduced_product *reduced_product_a,
                                                   const Reduced_product *reduced_product_b,
                                                   const long index_a, const long index_b, int *match)
{
    double *longitude_bounds_a;
    double *latitude_bounds_a;
    double *longitude_bounds_b;
    double *latitude_bounds_b;
    int num_vertices_a;
    int num_vertices_b;

    if (reduced_product_a->latitude_bounds == NULL || reduced_product_a->longitude_bounds == NULL)
    {
        harp_set_error(HARP_ERROR_NO_DATA, "latitude bounds and longitude bounds not in product '%s' (dataset a)",
                       reduced_product_a->filename);
        return -1;
    }

    if (reduced_product_a->latitude_bounds->num_dimensions != 2 ||
        reduced_product_a->longitude_bounds->num_dimensions != 2)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "latitude bounds and longitude bounds must be 2D");
        return -1;
    }

    if (reduced_product_b->latitude_bounds == NULL || reduced_product_b->longitude_bounds == NULL)
    {
        harp_set_error(HARP_ERROR_NO_DATA, "latitude bounds and longitude bounds not in product '%s' (dataset b)",
                       reduced_product_b->filename);
        return -1;
    }

    if (reduced_product_b->latitude_bounds->num_dimensions != 2 ||
        reduced_product_b->longitude_bounds->num_dimensions != 2)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "latitude bounds and longitude bounds must be 2D");
        return -1;
    }

    num_vertices_a = reduced_product_a->latitude_bounds->dimension[1];
    longitude_bounds_a = &reduced_product_a->longitude_bounds->data.double_data[index_a * num_vertices_a];
    latitude_bounds_a = &reduced_product_a->latitude_bounds->data.double_data[index_a * num_vertices_a];
    num_vertices_b = reduced_product_b->latitude_bounds->dimension[1];
    longitude_bounds_b = &reduced_product_b->longitude_bounds->data.double_data[index_b * num_vertices_a];
    latitude_bounds_b = &reduced_product_b->latitude_bounds->data.double_data[index_b * num_vertices_a];

    return harp_geometry_has_area_overlap(num_vertices_a, longitude_bounds_a, latitude_bounds_a, num_vertices_b,
                                          longitude_bounds_b, latitude_bounds_b, match, NULL);
}

/* Matchup: do areas overlap with an overlapping percentage larger than the criterion? */
static int matchup_two_measurements_in_overlapping_percentage(const Reduced_product *reduced_product_a,
                                                              const Reduced_product *reduced_product_b,
                                                              const Collocation_options *collocation_options,
                                                              const long index_a, const long index_b,
                                                              const Collocation_criterion_type criterion_type,
                                                              double *overlapping_percentage, int *match)
{
    double *longitude_bounds_a;
    double *latitude_bounds_a;
    double *longitude_bounds_b;
    double *latitude_bounds_b;
    int num_vertices_a;
    int num_vertices_b;
    double da;

    /* Make some checks */
    if (criterion_type != collocation_criterion_type_overlapping_percentage)
    {
        harp_set_error(HARP_ERROR_INVALID_TYPE, "incorrect collocation criterion");
        return -1;
    }
    if (collocation_options->criterion[criterion_type] == NULL ||
        collocation_options->criterion_is_set[criterion_type] != 1)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "collocation criterion %d is not set", criterion_type);
        return -1;
    }

    /* Grab the overlapping percentage criterion */
    da = collocation_options->criterion[criterion_type]->value;

    if (reduced_product_a->latitude_bounds == NULL || reduced_product_a->longitude_bounds == NULL)
    {
        harp_set_error(HARP_ERROR_NO_DATA, "latitude bounds and longitude bounds not in product '%s' (dataset a)",
                       reduced_product_a->filename);
        return -1;
    }

    if (reduced_product_a->latitude_bounds->num_dimensions != 2 ||
        reduced_product_a->longitude_bounds->num_dimensions != 2)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "latitude bounds and longitude bounds must be 2D");
        return -1;
    }

    if (reduced_product_b->latitude_bounds == NULL || reduced_product_b->longitude_bounds == NULL)
    {
        harp_set_error(HARP_ERROR_NO_DATA, "latitude bounds and longitude bounds not in product '%s' (dataset b)",
                       reduced_product_b->filename);
        return -1;
    }

    if (reduced_product_b->latitude_bounds->num_dimensions != 2 ||
        reduced_product_b->longitude_bounds->num_dimensions != 2)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "latitude bounds and longitude bounds must be 2D");
        return -1;
    }

    num_vertices_a = reduced_product_a->latitude_bounds->dimension[1];
    longitude_bounds_a = &reduced_product_a->longitude_bounds->data.double_data[index_a * num_vertices_a];
    latitude_bounds_a = &reduced_product_a->latitude_bounds->data.double_data[index_a * num_vertices_a];
    num_vertices_b = reduced_product_b->latitude_bounds->dimension[1];
    longitude_bounds_b = &reduced_product_b->longitude_bounds->data.double_data[index_b * num_vertices_a];
    latitude_bounds_b = &reduced_product_b->latitude_bounds->data.double_data[index_b * num_vertices_a];

    if (harp_geometry_has_area_overlap(num_vertices_a, longitude_bounds_a, latitude_bounds_a, num_vertices_b,
                                       longitude_bounds_b, latitude_bounds_b, match, overlapping_percentage) != 0)
    {
        return -1;
    }

    if (*match)
    {
        *match = (*overlapping_percentage >= da);
    }

    return 0;
}

/* Matchup two measurements in time, space, and measurement geometry */
static int matchup_two_measurements(harp_collocation_result *collocation_result,
                                    const Reduced_product *reduced_product_a, const Reduced_product *reduced_product_b,
                                    const Collocation_options *collocation_options,
                                    const long original_index_a, const long index_a,
                                    const long original_index_b, const long index_b, int *new_match)
{
    int match;
    double difference;
    double differences[HARP_COLLOCATION_RESULT_MAX_NUM_DIFFERENCES];
    harp_collocation_pair *pair = NULL;
    long collocation_index;
    double delta;

    /* Matchup two measurements in time */
    if (collocation_options->criterion_is_set[collocation_criterion_type_time])
    {
        difference = fabs(reduced_product_a->datetime->data.double_data[index_a] -
                          reduced_product_b->datetime->data.double_data[index_b]);
        if (difference > collocation_options->criterion[collocation_criterion_type_time]->value)
        {
            *new_match = 0;
            return 0;
        }

        differences[harp_collocation_difference_absolute_time] = difference;
    }

    /* Matchup two measurements in latitude */
    if (collocation_options->criterion_is_set[collocation_criterion_type_latitude])
    {
        difference = fabs(reduced_product_a->latitude->data.double_data[index_a] -
                          reduced_product_b->latitude->data.double_data[index_b]);
        if (difference > collocation_options->criterion[collocation_criterion_type_latitude]->value)
        {
            *new_match = 0;
            return 0;
        }
        differences[harp_collocation_difference_absolute_latitude] = difference;
    }

    /* Matchup two measurements in longitude */
    if (collocation_options->criterion_is_set[collocation_criterion_type_longitude])
    {
        difference = fabs(reduced_product_a->longitude->data.double_data[index_a] -
                          reduced_product_b->longitude->data.double_data[index_b]);
        if (difference > collocation_options->criterion[collocation_criterion_type_longitude]->value)
        {
            *new_match = 0;
            return 0;
        }
        differences[harp_collocation_difference_absolute_longitude] = difference;
    }

    /* Matchup two measurements in SZA */
    if (collocation_options->criterion_is_set[collocation_criterion_type_sza])
    {
        difference = fabs(reduced_product_a->sza->data.double_data[index_a] -
                          reduced_product_b->sza->data.double_data[index_b]);
        if (difference > collocation_options->criterion[collocation_criterion_type_sza]->value)
        {
            *new_match = 0;
            return 0;
        }
        differences[harp_collocation_difference_absolute_sza] = difference;
    }

    /* Matchup two measurements in SAA */
    if (collocation_options->criterion_is_set[collocation_criterion_type_saa])
    {
        difference = fabs(reduced_product_a->saa->data.double_data[index_a] -
                          reduced_product_b->saa->data.double_data[index_b]);
        if (difference > collocation_options->criterion[collocation_criterion_type_saa]->value)
        {
            *new_match = 0;
            return 0;
        }
        differences[harp_collocation_difference_absolute_saa] = difference;
    }

    /* Matchup two measurements in VZA */
    if (collocation_options->criterion_is_set[collocation_criterion_type_vza])
    {
        difference = fabs(reduced_product_a->vza->data.double_data[index_a] -
                          reduced_product_b->vza->data.double_data[index_b]);
        if (difference > collocation_options->criterion[collocation_criterion_type_vza]->value)
        {
            *new_match = 0;
            return 0;
        }
        differences[harp_collocation_difference_absolute_vza] = difference;
    }

    /* Matchup two measurements in VAA */
    if (collocation_options->criterion_is_set[collocation_criterion_type_vaa])
    {
        difference = fabs(reduced_product_a->vaa->data.double_data[index_a] -
                          reduced_product_b->vaa->data.double_data[index_b]);
        if (difference > collocation_options->criterion[collocation_criterion_type_vaa]->value)
        {
            *new_match = 0;
            return 0;
        }
        differences[harp_collocation_difference_absolute_vaa] = difference;
    }

    /* Matchup two measurements in scattering angle */
    if (collocation_options->criterion_is_set[collocation_criterion_type_theta])
    {
        difference = fabs(reduced_product_a->theta->data.double_data[index_a] -
                          reduced_product_b->theta->data.double_data[index_b]);
        if (difference > collocation_options->criterion[collocation_criterion_type_theta]->value)
        {
            *new_match = 0;
            return 0;
        }
        differences[harp_collocation_difference_absolute_theta] = difference;
    }

    /* Matchup two measurements in point distance */
    if (collocation_options->criterion_is_set[collocation_criterion_type_point_distance])
    {
        match = 0;
        if (matchup_two_measurements_in_point_distance(reduced_product_a, reduced_product_b, collocation_options,
                                                       index_a, index_b,
                                                       collocation_criterion_type_point_distance, &difference, &match)
            != 0)
        {
            return -1;
        }

        differences[harp_collocation_difference_point_distance] = difference;
        if (match == 0)
        {
            *new_match = 0;
            return 0;
        }
    }

    /* Matchup two measurements: point A must lie in area B */
    if (collocation_options->criterion_is_set[collocation_criterion_type_point_a_in_area_b])
    {
        match = 0;
        if (matchup_two_measurements_point_in_area(reduced_product_a, reduced_product_b, index_a, index_b, &match) != 0)
        {
            return -1;
        }

        if (match == 0)
        {
            *new_match = 0;
            return 0;
        }
    }

    /* Matchup two measurements: point B must lie in area A */
    if (collocation_options->criterion_is_set[collocation_criterion_type_point_b_in_area_a])
    {
        match = 0;
        if (matchup_two_measurements_point_in_area(reduced_product_b, reduced_product_a, index_b, index_a, &match) != 0)
        {
            return -1;
        }

        if (match == 0)
        {
            *new_match = 0;
            return 0;
        }
    }

    /* Matchup two measurements: areas must be overlapping */
    if (collocation_options->criterion_is_set[collocation_criterion_type_overlapping])
    {
        match = 0;
        if (matchup_two_measurements_areas_in_areas(reduced_product_a, reduced_product_b, index_a,
                                                    index_b, &match) != 0)
        {
            return -1;
        }

        if (match == 0)
        {
            *new_match = 0;
            return 0;
        }
    }

    /* Matchup two measurements in overlapping percentage */
    if (collocation_options->criterion_is_set[collocation_criterion_type_overlapping_percentage])
    {
        match = 0;
        if (matchup_two_measurements_in_overlapping_percentage(reduced_product_a, reduced_product_b,
                                                               collocation_options, index_a, index_b,
                                                               collocation_criterion_type_overlapping_percentage,
                                                               &difference, &match) != 0)
        {
            return -1;
        }

        differences[harp_collocation_difference_overlapping_percentage] = difference;

        if (match == 0)
        {
            *new_match = 0;
            return 0;
        }
    }

    /* Store this id in the result to be able to reproduce
     * the chronological order of the measurements in the re-sampled collocation result later on */
    collocation_index = collocation_result->num_pairs;

    /* If we have survived so far, we have a match */
    /* Write the original file and measurement ids */
    if (harp_collocation_pair_new(collocation_index, reduced_product_a->source_product, original_index_a,
                                  reduced_product_b->source_product, original_index_b, differences, &pair) != 0)
    {
        return -1;
    }

    /* Calculate the weighted norm of the differences */
    if (calculate_delta(collocation_result, collocation_options, pair, &delta) != 0)
    {
        harp_collocation_pair_delete(pair);
        return -1;
    }

    if (harp_collocation_result_add_pair(collocation_result, pair) != 0)
    {
        harp_collocation_pair_delete(pair);
        return -1;
    }

    *new_match = 1;
    return 0;
}

/* Matchup measurements in two files */
static int reduced_product_derive_number_of_measurements(const Reduced_product *reduced_product, long *num_measurements)
{
    /* Grab the number of elements for file A and file B */
    if (reduced_product->datetime != NULL)
    {
        /* Try DATETIME first ... */
        *num_measurements = reduced_product->datetime->num_elements;
        return 0;
    }

    if (reduced_product->latitude != NULL)
    {
        /* else try LATITUDE */
        *num_measurements = reduced_product->latitude->num_elements;
        return 0;
    }

    if (reduced_product->latitude_bounds != NULL)
    {
        /* ... last resort: try latitude_bounds */
        *num_measurements = reduced_product->latitude_bounds->dimension[0];
        return 0;
    }

    harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "could not derive number of samples from variable 'datetime', "
                   "'latitude', or 'latitude_bounds'");
    return -1;
}

/* Check for INDEX variable that contains the original measurement id */
static int get_original_index(const Reduced_product *reduced_product, long index, long *original_index)
{
    if (reduced_product->index == NULL)
    {
        /* Use the measurement id itself */
        *original_index = index;
        return 0;
    }
    else
    {
        if (index < 0 || index > reduced_product->index->num_elements)
        {
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "index argument (%ld) is not in the range [0,%ld) (%s:%u)",
                           index, reduced_product->index->num_elements, __FILE__, __LINE__);
            return -1;
        }

        *original_index = (long)(reduced_product->index->data.int32_data[index]);
        return 0;
    }
}

static int matchup_measurements_in_two_files(harp_collocation_result *collocation_result,
                                             const Reduced_product *reduced_product_a,
                                             const Reduced_product *reduced_product_b,
                                             const Collocation_options *collocation_options, int *files_match)
{
    int measurements_match = 0;
    long original_index_a;
    long index_a;
    long num_measurements_a;
    long original_index_b;
    long index_b;
    long num_measurements_b;

    if (reduced_product_a == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "reduced_product_a is NULL");
        return -1;
    }
    if (reduced_product_b == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "reduced_product_b is NULL");
        return -1;
    }

    /* Derive the number of elements of the main dimension */
    if (reduced_product_derive_number_of_measurements(reduced_product_a, &num_measurements_a) != 0)
    {
        return -1;
    }
    if (reduced_product_derive_number_of_measurements(reduced_product_b, &num_measurements_b) != 0)
    {
        return -1;
    }

    /* Matchup the two measurements;
     * the original measurement id are needed for the collocation result */
    for (index_a = 0; index_a < num_measurements_a; index_a++)
    {
        if (get_original_index(reduced_product_a, index_a, &original_index_a) != 0)
        {
            return -1;
        }

        for (index_b = 0; index_b < num_measurements_b; index_b++)
        {
            if (get_original_index(reduced_product_b, index_b, &original_index_b) != 0)
            {
                return -1;
            }

            if (matchup_two_measurements(collocation_result, reduced_product_a, reduced_product_b, collocation_options,
                                         original_index_a, index_a, original_index_b,
                                         index_b, &measurements_match) != 0)
            {
                return -1;
            }

            /* One matchup of two measurements is enough to be interesting for the collocation result */
            if (measurements_match == 1)
            {
                *files_match = 1;
            }
        }
    }

    return 0;
}

/* Get the start and stop time for each file in the dataset and put this information in the cache structure */
static int cache_set_dataset_start_stop_times(Cache *cache, const Dataset *dataset)
{
    int j;

    for (j = 0; j < cache->num_files; j++)
    {
        /* Set the start and stop time for each file */
        cache->datetime_start[j] = dataset->datetime_start[j];
        cache->datetime_stop[j] = dataset->datetime_stop[j];
    }

    return 0;
}

static void swap_files(char **name_a, char **name_b, double *datetime_start_a,
                       double *datetime_start_b, double *datetime_stop_a, double *datetime_stop_b)
{
    char *name_tmp;
    double datetime_start_tmp;
    double datetime_stop_tmp;

    name_tmp = *name_a;
    *name_a = *name_b;
    *name_b = name_tmp;
    datetime_start_tmp = *datetime_start_a;
    *datetime_start_a = *datetime_start_b;
    *datetime_start_b = datetime_start_tmp;
    datetime_stop_tmp = *datetime_stop_a;
    *datetime_stop_a = *datetime_stop_b;
    *datetime_stop_b = datetime_stop_tmp;
}

/* Bubble sort filenames based on datetime start value */
static void sort_filenames(char **filename, double *datetime_start, double *datetime_stop, int num_files)
{
    int i, j;

    for (i = 0; i < num_files; ++i)
    {
        for (j = i + 1; j < num_files; ++j)
        {
            if (datetime_start[i] > datetime_start[j])
            {
                swap_files(&filename[i], &filename[j], &datetime_start[i], &datetime_start[j],
                           &datetime_stop[i], &datetime_stop[j]);
            }
        }
    }
}

static void dataset_sort_by_datetime_start(Dataset *dataset)
{
    sort_filenames(dataset->filename, dataset->datetime_start, dataset->datetime_stop, dataset->num_files);
}

/* Determine start and stop time in the unit that is used for collocation */
static int dataset_add_start_stop_datetime(Dataset *dataset)
{
    double datetime_start;
    double datetime_stop;
    int i;

    if (dataset->datetime_start == NULL)
    {
        dataset->datetime_start = malloc((size_t)dataset->num_files * sizeof(double));
        if (dataset->datetime_start == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           dataset->num_files * sizeof(double), __FILE__, __LINE__);
            return -1;
        }
    }

    if (dataset->datetime_stop == NULL)
    {
        dataset->datetime_stop = malloc((size_t)dataset->num_files * sizeof(double));
        if (dataset->datetime_stop == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           dataset->num_files * sizeof(double), __FILE__, __LINE__);
            if (dataset->datetime_start != NULL)
            {
                free(dataset->datetime_start);
            }
            return -1;
        }
    }

    for (i = 0; i < dataset->num_files; i++)
    {
        /* This function will not perform any unit conversion on the datetime start/stop values, but take the values as
         * is in the product.
         */
        if (harp_import_global_attributes(dataset->filename[i], &datetime_start, &datetime_stop, NULL) != 0)
        {
            if (dataset->datetime_start != NULL)
            {
                free(dataset->datetime_start);
            }
            if (dataset->datetime_stop != NULL)
            {
                free(dataset->datetime_stop);
            }
            return -1;
        }

        dataset->datetime_start[i] = datetime_start;
        dataset->datetime_stop[i] = datetime_stop;
    }

    return 0;
}

/* Collocate two datasets */
int matchup(const Collocation_options *collocation_options, harp_collocation_result **new_collocation_result)
{
    harp_collocation_result *collocation_result = NULL;
    Dataset *dataset_a = NULL;
    Dataset *dataset_b = NULL;
    Cache *cache_b = NULL;
    int files_match = 1;
    int i;
    int j;

    /* Validate input arguments */
    if (collocation_options == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "collocation options is empty");
        return -1;
    }
    if (collocation_options->dataset_a_in == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "collocation options: dataset a not set");
        return -1;
    }
    if (collocation_options->dataset_b_in == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "collocation options: dataset b not set");
        return -1;
    }

    dataset_a = collocation_options->dataset_a_in;
    dataset_b = collocation_options->dataset_b_in;

    /* If no criteria are set, this means all data is kept, not needed to collocate */
    if (collocation_options->num_criteria == 0)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "no collocation criteria are set");
        return -1;
    }

    /* Start with a new collocation result */
    if (harp_collocation_result_new(&collocation_result) != 0)
    {
        return -1;
    }

    /* Determine the format for the collocation result */
    if (collocation_result_init(collocation_result, collocation_options) != 0)
    {
        harp_collocation_result_delete(collocation_result);
        return -1;
    }

    /* Check for empty datasets */
    if (dataset_a->num_files <= 0 || dataset_b->num_files <= 0)
    {
        *new_collocation_result = collocation_result;
        return 0;
    }

    /* Determine the start time for each product for sorting purposes */
    if (dataset_add_start_stop_datetime(dataset_a) != 0)
    {
        harp_collocation_result_delete(collocation_result);
        return -1;
    }
    if (dataset_add_start_stop_datetime(dataset_b) != 0)
    {
        harp_collocation_result_delete(collocation_result);
        return -1;
    }

    /* Sort the filenames in the datasets, ordered by start datetime
       (i.e. monotonically increasing) */
    dataset_sort_by_datetime_start(dataset_a);
    dataset_sort_by_datetime_start(dataset_b);

    /* Setup the cache */
    if (cache_new(&cache_b, dataset_b->num_files) != 0)
    {
        harp_collocation_result_delete(collocation_result);
        return -1;
    }

    if (dataset_b->num_files != cache_b->num_files)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "maximum cache size (%d) and number of files in dataset b (%d) are "
                       "not consistent", cache_b->num_files, dataset_b->num_files);
        cache_delete(cache_b);
        harp_collocation_result_delete(collocation_result);
        return -1;
    }

    /* Determine the start and stop time for each file in dataset B */
    if (cache_set_dataset_start_stop_times(cache_b, dataset_b) != 0)
    {
        cache_delete(cache_b);
        harp_collocation_result_delete(collocation_result);
        return -1;
    }

    /* Loop over files in dataset A */
    for (i = 0; i < dataset_a->num_files; i++)
    {
        Reduced_product *reduced_product_a = NULL;

        /* Import each product of dataset A */
        if (reduced_product_import(dataset_a->filename[i], collocation_options, 0, &reduced_product_a) != 0)
        {
            cache_delete(cache_b);
            harp_collocation_result_delete(collocation_result);
            return -1;
        }

        /* Determine which subset of files in dataset B need to be considered */
        if (dataset_b_determine_subset(cache_b, collocation_options, dataset_a, i) != 0)
        {
            reduced_product_delete(reduced_product_a);
            cache_delete(cache_b);
            harp_collocation_result_delete(collocation_result);
            return -1;
        }

        if (cache_b->num_subset_files > 0)
        {
            /* Update the cache */
            if (cache_b_update(cache_b, collocation_options, dataset_a, i) != 0)
            {
                reduced_product_delete(reduced_product_a);
                cache_delete(cache_b);
                harp_collocation_result_delete(collocation_result);
                return -1;
            }

            for (j = 0; j < cache_b->num_files; j++)
            {
                /* Perform collocation only when needed */
                if (cache_b->file_is_needed[j])
                {
                    Reduced_product *reduced_product_b = NULL;

                    if (cache_b->reduced_product[j] == NULL)
                    {
                        /* If not yet open, import product of dataset B ... */
                        if (reduced_product_import(dataset_b->filename[j], collocation_options, 1, &reduced_product_b)
                            != 0)
                        {
                            reduced_product_delete(reduced_product_a);
                            cache_delete(cache_b);
                            harp_collocation_result_delete(collocation_result);
                            return -1;
                        }

                        /* ... and add it to the cache */
                        if (cache_add_reduced_product(cache_b, reduced_product_b, j) != 0)
                        {
                            reduced_product_delete(reduced_product_b);
                            reduced_product_delete(reduced_product_a);
                            cache_delete(cache_b);
                            harp_collocation_result_delete(collocation_result);
                            return -1;
                        }
                    }
                    else
                    {
                        /* otherwise, grab it from the cache */
                        reduced_product_b = cache_b->reduced_product[j];
                    }

                    /* Matchup measurements in two files */
                    if (matchup_measurements_in_two_files(collocation_result, reduced_product_a, reduced_product_b,
                                                          collocation_options, &files_match) != 0)
                    {
                        reduced_product_delete(reduced_product_a);
                        cache_delete(cache_b);
                        harp_collocation_result_delete(collocation_result);
                        return -1;
                    }
                }
            }
        }

        reduced_product_delete(reduced_product_a);
    }

    /* Delete the cache */
    cache_delete(cache_b);
    *new_collocation_result = collocation_result;
    return 0;
}
