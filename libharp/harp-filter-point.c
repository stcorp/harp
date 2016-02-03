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
#include "harp-geometry.h"

typedef struct point_distance_test_args_struct
{
    harp_unit_converter *unit_converter;
    harp_spherical_point origin;
    double distance;
} point_distance_test_args;

static void point_distance_test_args_delete(point_distance_test_args *args)
{
    if (args != NULL)
    {
        if (args->unit_converter != NULL)
        {
            harp_unit_converter_delete(args->unit_converter);
        }

        free(args);
    }
}

static void point_distance_test_args_delete_func(void *untyped_args)
{
    point_distance_test_args_delete((point_distance_test_args *)untyped_args);
}

static uint8_t test_point_distance(void *untyped_args, const void *untyped_value)
{
    double distance;
    point_distance_test_args *args = (point_distance_test_args *)untyped_args;

    distance = harp_spherical_point_distance_in_meters(&args->origin, (const harp_spherical_point *)untyped_value);
    if (args->unit_converter != NULL)
    {
        distance = harp_unit_converter_convert(args->unit_converter, distance);
    }

    return (distance <= args->distance);
}

int harp_point_distance_filter_predicate_new(const harp_point_distance_filter_args *args,
                                             harp_predicate **new_predicate)
{
    harp_predicate *predicate;
    point_distance_test_args *predicate_args;
    harp_spherical_point origin;

    /* Convert location information to harp_spherical_point. */
    origin.lon = args->longitude;
    origin.lat = args->latitude;

    if (args->longitude_unit != NULL && harp_unit_compare(args->longitude_unit, "degree_east") != 0)
    {
        if (harp_convert_unit(args->longitude_unit, "degree_east", 1, &origin.lon) != 0)
        {
            return -1;
        }
    }

    if (args->latitude_unit != NULL && harp_unit_compare(args->latitude_unit, "degree_north") != 0)
    {
        if (harp_convert_unit(args->latitude_unit, "degree_north", 1, &origin.lat) != 0)
        {
            return -1;
        }
    }

    harp_spherical_point_rad_from_deg(&origin);
    harp_spherical_point_check(&origin);

    predicate_args = (point_distance_test_args *)malloc(sizeof(point_distance_test_args));
    if (predicate_args == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(point_distance_test_args), __FILE__, __LINE__);
        return -1;
    }

    predicate_args->origin = origin;
    predicate_args->unit_converter = NULL;
    predicate_args->distance = args->distance;

    if (args->distance_unit != NULL && harp_unit_compare(args->distance_unit, "m") != 0)
    {
        if (harp_unit_converter_new("m", args->distance_unit, &predicate_args->unit_converter) != 0)
        {
            point_distance_test_args_delete(predicate_args);
            return -1;
        }
    }

    if (harp_predicate_new(test_point_distance, predicate_args, point_distance_test_args_delete_func, &predicate) != 0)
    {
        point_distance_test_args_delete(predicate_args);
        return -1;
    }

    *new_predicate = predicate;
    return 0;
}

static long update_mask(int num_predicates, harp_predicate **predicate, long num_points, const double *longitude,
                        const double *latitude, uint8_t *mask)
{
    uint8_t *mask_end;
    long num_masked = 0;

    for (mask_end = mask + num_points; mask != mask_end; ++mask)
    {
        if (*mask)
        {
            harp_spherical_point point;
            int i;

            point.lon = *longitude;
            point.lat = *latitude;
            harp_spherical_point_rad_from_deg(&point);
            harp_spherical_point_check(&point);

            for (i = 0; i < num_predicates; i++)
            {
                if (!predicate[i]->eval(predicate[i]->args, &point))
                {
                    *mask = 0;
                    break;
                }
            }

            if (*mask)
            {
                ++num_masked;
            }
        }

        ++longitude;
        ++latitude;
    }

    return num_masked;
}

int harp_point_predicate_update_mask_all_0d(int num_predicates, harp_predicate **predicate,
                                            const harp_variable *longitude, const harp_variable *latitude,
                                            uint8_t *product_mask)
{
    harp_variable *longitude_copy = NULL;
    harp_variable *latitude_copy = NULL;

    if (num_predicates == 0)
    {
        return 0;
    }
    if (predicate == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "predicate is NULL (%s:%lu)", __FILE__, __LINE__);
        return -1;

    }
    if (longitude == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "longitude is NULL (%s:%lu)", __FILE__, __LINE__);
        return -1;

    }
    if (latitude == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "latitude is NULL (%s:%lu)", __FILE__, __LINE__);
        return -1;

    }
    if (product_mask == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "product_mask is NULL (%s:%lu)", __FILE__, __LINE__);
        return -1;

    }
    if (longitude->num_dimensions != 0)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variable '%s' has %d dimensions; expected 0", longitude->name,
                       longitude->num_dimensions);
        return -1;
    }
    if (latitude->num_dimensions != 0)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variable '%s' has %d dimensions; expected 0", latitude->name,
                       latitude->num_dimensions);
        return -1;
    }
    if (!*product_mask)
    {
        /* Product mask is false. */
        return 0;
    }

    /* Harmonize unit and data type. */
    if (!harp_variable_has_unit(longitude, "degree_east"))
    {
        if (harp_variable_copy(longitude, &longitude_copy) != 0)
        {
            return -1;
        }
        if (harp_variable_convert_unit(longitude_copy, "degree_east") != 0)
        {
            harp_variable_delete(longitude_copy);
            return -1;
        }
        longitude = longitude_copy;
    }
    else if (longitude->data_type != harp_type_double)
    {
        if (harp_variable_copy(longitude, &longitude_copy) != 0)
        {
            return -1;
        }
        if (harp_variable_convert_data_type(longitude_copy, harp_type_double) != 0)
        {
            harp_variable_delete(longitude_copy);
            return -1;
        }
        longitude = longitude_copy;
    }

    if (!harp_variable_has_unit(latitude, "degree_north"))
    {
        if (harp_variable_copy(latitude, &latitude_copy) != 0)
        {
            harp_variable_delete(longitude_copy);
            return -1;
        }
        if (harp_variable_convert_unit(latitude_copy, "degree_north") != 0)
        {
            harp_variable_delete(longitude_copy);
            harp_variable_delete(latitude_copy);
            return -1;
        }
        latitude = latitude_copy;
    }
    else if (latitude->data_type != harp_type_double)
    {
        if (harp_variable_copy(latitude, &latitude_copy) != 0)
        {
            harp_variable_delete(longitude_copy);
            return -1;
        }
        if (harp_variable_convert_data_type(latitude_copy, harp_type_double) != 0)
        {
            harp_variable_delete(longitude_copy);
            harp_variable_delete(latitude_copy);
            return -1;
        }
        latitude = latitude_copy;
    }

    /* Update product mask. */
    update_mask(num_predicates, predicate, 1, longitude->data.double_data, latitude->data.double_data, product_mask);

    /* Clean-up. */
    harp_variable_delete(longitude_copy);
    harp_variable_delete(latitude_copy);

    return 0;
}

int harp_point_predicate_update_mask_all_1d(int num_predicates, harp_predicate **predicate,
                                            const harp_variable *longitude, const harp_variable *latitude,
                                            harp_dimension_mask *dimension_mask)
{
    harp_variable *longitude_copy = NULL;
    harp_variable *latitude_copy = NULL;
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
    if (longitude == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "longitude is NULL (%s:%lu)", __FILE__, __LINE__);
        return -1;

    }
    if (latitude == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "latitude is NULL (%s:%lu)", __FILE__, __LINE__);
        return -1;

    }
    if (dimension_mask == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "dimension_mask is NULL (%s:%lu)", __FILE__, __LINE__);
        return -1;

    }
    if (longitude->num_dimensions != 1)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variable '%s' has %d dimensions; expected 1", longitude->name,
                       longitude->num_dimensions);
        return -1;
    }
    if (longitude->dimension_type[0] != harp_dimension_time)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variable '%s' has dimensions {%s}; expected {%s}", longitude->name,
                       harp_get_dimension_type_name(longitude->dimension_type[0]),
                       harp_get_dimension_type_name(harp_dimension_time));
        return -1;
    }
    if (latitude->num_dimensions != 1)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variable '%s' has %d dimensions; expected 1", latitude->name,
                       latitude->num_dimensions);
        return -1;
    }
    if (latitude->dimension_type[0] != harp_dimension_time)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variable '%s' has dimensions {%s}; expected {%s}", latitude->name,
                       harp_get_dimension_type_name(latitude->dimension_type[0]),
                       harp_get_dimension_type_name(harp_dimension_time));
        return -1;
    }

    /* Both variables should have the same number of elements, since they depend on the same dimension (time). */
    assert(longitude->num_elements == latitude->num_elements);
    num_points = longitude->num_elements;

    if (dimension_mask->num_dimensions != 1)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "dimension mask has %d dimensions; expected 1",
                       dimension_mask->num_dimensions);
        return -1;
    }
    if (dimension_mask->num_elements != num_points)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "dimension mask has %ld elements, expected %ld",
                       dimension_mask->num_elements, num_points);
        return -1;
    }
    if (dimension_mask->masked_dimension_length == 0)
    {
        /* Dimension mask is false. */
        return 0;
    }
    assert(dimension_mask->mask != NULL);

    /* Harmonize unit and data type. */
    if (!harp_variable_has_unit(longitude, "degree_east"))
    {
        if (harp_variable_copy(longitude, &longitude_copy) != 0)
        {
            return -1;
        }
        if (harp_variable_convert_unit(longitude_copy, "degree_east") != 0)
        {
            harp_variable_delete(longitude_copy);
            return -1;
        }
        longitude = longitude_copy;
    }
    else if (longitude->data_type != harp_type_double)
    {
        if (harp_variable_copy(longitude, &longitude_copy) != 0)
        {
            return -1;
        }
        if (harp_variable_convert_data_type(longitude_copy, harp_type_double) != 0)
        {
            harp_variable_delete(longitude_copy);
            return -1;
        }
        longitude = longitude_copy;
    }

    if (!harp_variable_has_unit(latitude, "degree_north"))
    {
        if (harp_variable_copy(latitude, &latitude_copy) != 0)
        {
            harp_variable_delete(longitude_copy);
            return -1;
        }
        if (harp_variable_convert_unit(latitude_copy, "degree_north") != 0)
        {
            harp_variable_delete(longitude_copy);
            harp_variable_delete(latitude_copy);
            return -1;
        }
        latitude = latitude_copy;
    }
    else if (latitude->data_type != harp_type_double)
    {
        if (harp_variable_copy(latitude, &latitude_copy) != 0)
        {
            harp_variable_delete(longitude_copy);
            return -1;
        }
        if (harp_variable_convert_data_type(latitude_copy, harp_type_double) != 0)
        {
            harp_variable_delete(longitude_copy);
            harp_variable_delete(latitude_copy);
            return -1;
        }
        latitude = latitude_copy;
    }

    /* Update dimension mask. */
    dimension_mask->masked_dimension_length = update_mask(num_predicates, predicate, num_points,
                                                          longitude->data.double_data, latitude->data.double_data,
                                                          dimension_mask->mask);

    /* Clean-up. */
    harp_variable_delete(longitude_copy);
    harp_variable_delete(latitude_copy);

    return 0;
}
