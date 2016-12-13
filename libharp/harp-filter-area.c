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

#include "harp-filter.h"
#include "harp-area-mask.h"
#include "harp-geometry.h"

static void area_mask_delete_func(void *untyped_args)
{
    harp_area_mask_delete((harp_area_mask *)untyped_args);
}

static uint8_t test_area_mask_covers_point(void *untyped_args, const void *untyped_value)
{
    harp_area_mask *area_mask = (harp_area_mask *)untyped_args;

    return harp_area_mask_covers_point(area_mask, (const harp_spherical_point *)untyped_value);
}

int harp_area_mask_covers_point_filter_predicate_new(const harp_area_mask_covers_point_filter_args *args,
                                                     harp_predicate **new_predicate)
{
    harp_predicate *predicate;
    harp_area_mask *area_mask;

    if (harp_area_mask_read(args->filename, &area_mask) != 0)
    {
        return -1;
    }

    if (harp_predicate_new(test_area_mask_covers_point, area_mask, area_mask_delete_func, &predicate) != 0)
    {
        harp_area_mask_delete(area_mask);
        return -1;
    }

    *new_predicate = predicate;
    return 0;
}

static uint8_t test_area_mask_covers_area(void *untyped_args, const void *untyped_value)
{
    harp_area_mask *area_mask = (harp_area_mask *)untyped_args;

    return harp_area_mask_covers_area(area_mask, (const harp_spherical_polygon *)untyped_value);
}

int harp_area_mask_covers_area_filter_predicate_new(const harp_area_mask_covers_area_filter_args *args,
                                                    harp_predicate **new_predicate)
{
    harp_predicate *predicate;
    harp_area_mask *area_mask;

    if (harp_area_mask_read(args->filename, &area_mask) != 0)
    {
        return -1;
    }

    if (harp_predicate_new(test_area_mask_covers_area, area_mask, area_mask_delete_func, &predicate) != 0)
    {
        harp_area_mask_delete(area_mask);
        return -1;
    }

    *new_predicate = predicate;
    return 0;
}

typedef struct area_mask_intersects_area_test_args_struct
{
    harp_area_mask *area_mask;
    double min_percentage;
} area_mask_intersects_area_test_args;

static void area_mask_intersects_area_test_args_delete(area_mask_intersects_area_test_args *args)
{
    if (args != NULL)
    {
        harp_area_mask_delete(args->area_mask);
        free(args);
    }
}

static void area_mask_intersects_area_test_args_delete_func(void *untyped_args)
{
    area_mask_intersects_area_test_args_delete((area_mask_intersects_area_test_args *)untyped_args);
}

static uint8_t test_area_mask_intersects_area(void *untyped_args, const void *untyped_value)
{
    area_mask_intersects_area_test_args *args = (area_mask_intersects_area_test_args *)untyped_args;

    return harp_area_mask_intersects_area(args->area_mask, (const harp_spherical_polygon *)untyped_value,
                                          args->min_percentage);
}

int harp_area_mask_intersects_area_filter_predicate_new(const harp_area_mask_intersects_area_filter_args *args,
                                                        harp_predicate **new_predicate)
{
    harp_predicate *predicate;
    area_mask_intersects_area_test_args *predicate_args;

    predicate_args = (area_mask_intersects_area_test_args *)malloc(sizeof(area_mask_intersects_area_test_args));
    if (predicate_args == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(area_mask_intersects_area_test_args), __FILE__, __LINE__);
        return -1;
    }
    predicate_args->area_mask = NULL;
    predicate_args->min_percentage = args->min_percentage;

    if (harp_area_mask_read(args->filename, &predicate_args->area_mask) != 0)
    {
        area_mask_intersects_area_test_args_delete(predicate_args);
        return -1;
    }

    if (harp_predicate_new(test_area_mask_intersects_area, predicate_args,
                           area_mask_intersects_area_test_args_delete_func, &predicate) != 0)
    {
        area_mask_intersects_area_test_args_delete(predicate_args);
        return -1;
    }

    *new_predicate = predicate;
    return 0;
}

static long update_mask(int num_predicates, harp_predicate **predicate, long num_areas, long num_points,
                        const double *latitude_bounds, const double *longitude_bounds, uint8_t *mask)
{
    uint8_t *mask_end;
    long num_masked = 0;

    for (mask_end = mask + num_areas; mask != mask_end; mask++)
    {
        if (*mask)
        {
            harp_spherical_polygon *area;
            int i;

            if (harp_spherical_polygon_from_latitude_longitude_bounds(0, num_points, latitude_bounds, longitude_bounds,
                                                                      &area) != 0)
            {
                *mask = 0;
            }
            else
            {
                for (i = 0; i < num_predicates; i++)
                {
                    if (!predicate[i]->eval(predicate[i]->args, area))
                    {
                        *mask = 0;
                        break;
                    }
                }

                harp_spherical_polygon_delete(area);
            }

            if (*mask)
            {
                num_masked++;
            }
        }

        latitude_bounds += num_points;
        longitude_bounds += num_points;
    }

    return num_masked;
}

int harp_area_predicate_update_mask_0d(int num_predicates, harp_predicate **predicate,
                                       const harp_variable *latitude_bounds, const harp_variable *longitude_bounds,
                                       uint8_t *product_mask)
{
    harp_variable *latitude_bounds_copy = NULL;
    harp_variable *longitude_bounds_copy = NULL;
    long num_points;

    if (num_predicates == 0)
    {
        return 0;
    }
    if (predicate == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "predicate is NULL (%s:%lu)", __FILE__, __LINE__);
        return -1;

    }
    if (latitude_bounds == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "latitude_bounds is NULL (%s:%lu)", __FILE__, __LINE__);
        return -1;

    }
    if (longitude_bounds == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "longitude_bounds is NULL (%s:%lu)", __FILE__, __LINE__);
        return -1;

    }
    if (product_mask == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "product_mask is NULL (%s:%lu)", __FILE__, __LINE__);
        return -1;

    }
    if (!*product_mask)
    {
        /* Product mask is false. */
        return 0;
    }

    if (latitude_bounds->num_dimensions != 1)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variable '%s' has %d dimensions; expected 1",
                       latitude_bounds->name, latitude_bounds->num_dimensions);
        return -1;
    }
    if (latitude_bounds->dimension_type[0] != harp_dimension_independent)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variable '%s' has dimensions {%s}; expected {%s}",
                       latitude_bounds->name, harp_get_dimension_type_name(latitude_bounds->dimension_type[0]),
                       harp_get_dimension_type_name(harp_dimension_independent));
        return -1;
    }
    if (longitude_bounds->num_dimensions != 1)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variable '%s' has %d dimensions; expected 1",
                       longitude_bounds->name, longitude_bounds->num_dimensions);
        return -1;
    }
    if (longitude_bounds->dimension_type[0] != harp_dimension_independent)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variable '%s' has dimensions {%s}; expected {%s}",
                       longitude_bounds->name, harp_get_dimension_type_name(longitude_bounds->dimension_type[0]),
                       harp_get_dimension_type_name(harp_dimension_independent));
        return -1;
    }
    if (latitude_bounds->dimension[0] != longitude_bounds->dimension[0])
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "the length of the independent dimension of variable '%s' (%ld) "
                       "does not match the length of the independent dimension of variable '%s' (%ld)",
                       latitude_bounds->name, latitude_bounds->dimension[0], longitude_bounds->name,
                       longitude_bounds->dimension[0]);
        return -1;
    }

    num_points = longitude_bounds->dimension[0];
    if (num_points < 3)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "the length of the independent dimension of variables '%s' and "
                       "'%s' should be 3 or more", latitude_bounds->name, longitude_bounds->name);
        return -1;
    }

    /* Update product mask. */
    update_mask(num_predicates, predicate, 1, num_points, latitude_bounds->data.double_data,
                longitude_bounds->data.double_data, product_mask);

    /* Clean-up. */
    harp_variable_delete(latitude_bounds_copy);
    harp_variable_delete(longitude_bounds_copy);

    return 0;
}

int harp_area_predicate_update_mask_1d(int num_predicates, harp_predicate **predicate,
                                       const harp_variable *latitude_bounds, const harp_variable *longitude_bounds,
                                       harp_dimension_mask *dimension_mask)
{
    harp_variable *latitude_bounds_copy = NULL;
    harp_variable *longitude_bounds_copy = NULL;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_independent };
    long num_areas;
    long num_points;

    if (num_predicates == 0)
    {
        return 0;
    }
    if (predicate == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "predicate is NULL (%s:%lu)", __FILE__, __LINE__);
        return -1;

    }
    if (latitude_bounds == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "latitude_bounds is NULL (%s:%lu)", __FILE__, __LINE__);
        return -1;

    }
    if (longitude_bounds == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "longitude_bounds is NULL (%s:%lu)", __FILE__, __LINE__);
        return -1;

    }
    if (dimension_mask == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "dimension_mask is NULL (%s:%lu)", __FILE__, __LINE__);
        return -1;

    }
    if (latitude_bounds->num_dimensions != 2)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variable '%s' has %d dimensions; expected 2",
                       latitude_bounds->name, latitude_bounds->num_dimensions);
        return -1;
    }
    if (!harp_variable_has_dimension_types(latitude_bounds, 2, dimension_type))
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variable '%s' has dimensions {%s, %s}; expected {%s, %s}",
                       latitude_bounds->name, harp_get_dimension_type_name(latitude_bounds->dimension_type[0]),
                       harp_get_dimension_type_name(latitude_bounds->dimension_type[1]),
                       harp_get_dimension_type_name(dimension_type[0]),
                       harp_get_dimension_type_name(dimension_type[1]));
        return -1;
    }
    if (longitude_bounds->num_dimensions != 2)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variable '%s' has %d dimensions; expected 2",
                       longitude_bounds->name, longitude_bounds->num_dimensions);
        return -1;
    }
    if (!harp_variable_has_dimension_types(longitude_bounds, 2, dimension_type))
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variable '%s' has dimensions {%s, %s}; expected {%s, %s}",
                       longitude_bounds->name, harp_get_dimension_type_name(longitude_bounds->dimension_type[0]),
                       harp_get_dimension_type_name(longitude_bounds->dimension_type[1]),
                       harp_get_dimension_type_name(dimension_type[0]),
                       harp_get_dimension_type_name(dimension_type[1]));
        return -1;
    }

    /* Both variables should have the same number of elements, since they depend on the same dimension (time). */
    assert(latitude_bounds->dimension[0] == longitude_bounds->dimension[0]);
    num_areas = longitude_bounds->dimension[0];
    if (latitude_bounds->dimension[1] != longitude_bounds->dimension[1])
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "the length of the independent dimension of variable '%s' (%ld) "
                       "does not match the length of the independent dimension of variable '%s' (%ld)",
                       latitude_bounds->name, latitude_bounds->dimension[1], longitude_bounds->name,
                       longitude_bounds->dimension[1]);
        return -1;
    }

    num_points = longitude_bounds->dimension[1];
    if (num_points < 3)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "the length of the independent dimension of variables '%s' and "
                       "'%s' should be 3 or more", latitude_bounds->name, longitude_bounds->name);
        return -1;
    }

    if (dimension_mask->num_dimensions != 1)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "dimension mask has %d dimensions; expected 1",
                       dimension_mask->num_dimensions);
        return -1;
    }
    if (dimension_mask->num_elements != num_areas)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "dimension mask has %ld elements, expected %ld",
                       dimension_mask->num_elements, num_areas);
        return -1;
    }
    if (dimension_mask->masked_dimension_length == 0)
    {
        /* Dimension mask is false. */
        return 0;
    }
    assert(dimension_mask->mask != NULL);

    /* Harmonize unit and data type. */
    if (!harp_variable_has_unit(latitude_bounds, "degree_north"))
    {
        if (harp_variable_copy(latitude_bounds, &latitude_bounds_copy) != 0)
        {
            harp_variable_delete(longitude_bounds_copy);
            return -1;
        }
        if (harp_variable_convert_unit(latitude_bounds_copy, "degree_north") != 0)
        {
            harp_variable_delete(longitude_bounds_copy);
            harp_variable_delete(latitude_bounds_copy);
            return -1;
        }
        latitude_bounds = latitude_bounds_copy;
    }
    else if (latitude_bounds->data_type != harp_type_double)
    {
        if (harp_variable_copy(latitude_bounds, &latitude_bounds_copy) != 0)
        {
            harp_variable_delete(longitude_bounds_copy);
            return -1;
        }
        if (harp_variable_convert_data_type(latitude_bounds_copy, harp_type_double) != 0)
        {
            harp_variable_delete(longitude_bounds_copy);
            harp_variable_delete(latitude_bounds_copy);
            return -1;
        }
        latitude_bounds = latitude_bounds_copy;
    }

    if (!harp_variable_has_unit(longitude_bounds, "degree_east"))
    {
        if (harp_variable_copy(longitude_bounds, &longitude_bounds_copy) != 0)
        {
            return -1;
        }
        if (harp_variable_convert_unit(longitude_bounds_copy, "degree_east") != 0)
        {
            harp_variable_delete(longitude_bounds_copy);
            return -1;
        }
        longitude_bounds = longitude_bounds_copy;
    }
    else if (longitude_bounds->data_type != harp_type_double)
    {
        if (harp_variable_copy(longitude_bounds, &longitude_bounds_copy) != 0)
        {
            return -1;
        }
        if (harp_variable_convert_data_type(longitude_bounds_copy, harp_type_double) != 0)
        {
            harp_variable_delete(longitude_bounds_copy);
            return -1;
        }
        longitude_bounds = longitude_bounds_copy;
    }

    /* Update dimension mask. */
    dimension_mask->masked_dimension_length = update_mask(num_predicates, predicate, num_areas, num_points,
                                                          latitude_bounds->data.double_data,
                                                          longitude_bounds->data.double_data, dimension_mask->mask);

    /* Clean-up. */
    harp_variable_delete(latitude_bounds_copy);
    harp_variable_delete(longitude_bounds_copy);

    return 0;
}
