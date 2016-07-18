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
#include "harp-constants.h"
#include "harp-geometry.h"

#include "hashtable.h"

#define MAX_NAME_LENGTH 128

harp_derived_variable_list *harp_derived_variable_conversions = NULL;

static int get_altitude_from_gph_and_latitude(harp_variable *variable, const harp_variable **source_variable)
{
    long i;

    for (i = 0; i < variable->num_elements; i++)
    {
        variable->data.double_data[i] = harp_altitude_from_gph_and_latitude(source_variable[0]->data.double_data[i],
                                                                            source_variable[1]->data.double_data[i]);
    }

    return 0;
}

static int get_air_nd_from_pressure_and_temperature(harp_variable *variable, const harp_variable **source_variable)
{
    long i;

    for (i = 0; i < variable->num_elements; i++)
    {
        /* VMR of air is 1.0 */
        variable->data.double_data[i] = harp_number_density_from_volume_mixing_ratio_pressure_and_temperature
            (1.0, source_variable[0]->data.double_data[i], source_variable[1]->data.double_data[i]);
    }

    return 0;
}

static int get_alt_bounds_from_alt(harp_variable *variable, const harp_variable **source_variable)
{
    if (variable->num_dimensions == 2)
    {
        harp_profile_altitude_bounds_from_altitude(source_variable[0]->num_elements,
                                                   source_variable[0]->data.double_data, variable->data.double_data);
    }
    else
    {
        long length = variable->dimension[1];
        long i;

        for (i = 0; i < variable->dimension[0]; i++)
        {
            harp_profile_altitude_bounds_from_altitude(length, &source_variable[0]->data.double_data[i * length],
                                                       &variable->data.double_data[i * length * 2]);
        }
    }

    return 0;
}

static int get_aux_variable_afgl86(harp_variable *variable, const harp_variable **source_variable)
{
    int i;

    for (i = 0; i < variable->dimension[0]; i++)
    {
        int num_levels = variable->dimension[1];
        int num_levels_afgl86;
        const double *altitude;
        const double *values;

        if (harp_aux_afgl86_get_profile("altitude", source_variable[0]->data.double_data[i],
                                        source_variable[1]->data.double_data[i], &num_levels_afgl86, &altitude) != 0)
        {
            return -1;
        }
        if (harp_aux_afgl86_get_profile(variable->name, source_variable[0]->data.double_data[i],
                                        source_variable[1]->data.double_data[i], &num_levels_afgl86, &values) != 0)
        {
            return -1;
        }
        harp_interpolate_array_linear(num_levels_afgl86, altitude, values, num_levels,
                                      &source_variable[2]->data.double_data[i * num_levels], 0,
                                      &variable->data.double_data[i * num_levels]);
    }

    return 0;
}

static int get_aux_variable_usstd76(harp_variable *variable, const harp_variable **source_variable)
{
    int num_levels_usstd76;
    const double *altitude;
    const double *values;
    int i;

    if (harp_aux_usstd76_get_profile("altitude", &num_levels_usstd76, &altitude) != 0)
    {
        return -1;
    }
    if (harp_aux_usstd76_get_profile(variable->name, &num_levels_usstd76, &values) != 0)
    {
        return -1;
    }

    for (i = 0; i < variable->dimension[0]; i++)
    {
        int num_levels = variable->dimension[1];

        harp_interpolate_array_linear(num_levels_usstd76, altitude, values, num_levels,
                                      &source_variable[0]->data.double_data[i * num_levels], 0,
                                      &variable->data.double_data[i * num_levels]);
    }

    return 0;
}

static int get_bounds_from_midpoints(harp_variable *variable, const harp_variable **source_variable)
{
    long num_elements;
    long i;

    num_elements = source_variable[0]->num_elements;
    if (num_elements < 2)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "need >= 2 midpoints to compute bounds (%s:%u)", __FILE__,
                       __LINE__);
        return -1;
    }

    /* Lower boundary of [0]. */
    variable->data.double_data[0] =
        0.5 * (3.0 * source_variable[0]->data.double_data[0] - source_variable[0]->data.double_data[1]);

    for (i = 0; i < num_elements - 1; i++)
    {
        double bound = 0.5 * (source_variable[0]->data.double_data[i] + source_variable[0]->data.double_data[i + 1]);

        /* Upper boundary of [i]. */
        variable->data.double_data[i * 2 + 1] = bound;

        /* Lower boundary of [i + 1]. */
        variable->data.double_data[(i + 1) * 2] = bound;
    }

    /* Upper boundary of [num_elements - 1]. */
    variable->data.double_data[(num_elements - 1) * 2 + 1] =
        0.5 * (3.0 * source_variable[0]->data.double_data[num_elements - 1] -
               source_variable[0]->data.double_data[num_elements - 2]);

    return 0;
}

static int get_column_from_partial_column(harp_variable *variable, const harp_variable **source_variable)
{
    long num_levels;
    long i;

    num_levels = source_variable[0]->dimension[1];
    for (i = 0; i < variable->dimension[0]; i++)
    {
        variable->data.double_data[i] =
            harp_profile_column_from_partial_column(num_levels, &source_variable[0]->data.double_data[i * num_levels]);
    }

    return 0;
}

static int get_column_uncertainty_from_partial_column_uncertainty(harp_variable *variable,
                                                                  const harp_variable **source_variable)
{
    long num_levels;
    long i;

    num_levels = source_variable[0]->dimension[1];
    for (i = 0; i < variable->dimension[0]; i++)
    {
        variable->data.double_data[i] =
            harp_profile_column_uncertainty_from_partial_column_uncertainty
            (num_levels, &source_variable[0]->data.double_data[i * num_levels]);
    }

    return 0;
}

static int get_copy(harp_variable *variable, const harp_variable **source_variable)
{
    assert(variable->data_type != harp_type_string);

    memcpy(variable->data.ptr, source_variable[0]->data.ptr,
           (size_t)variable->num_elements * harp_get_size_for_type(variable->data_type));

    return 0;
}

static int get_cov_from_systematic_and_random_cov(harp_variable *variable, const harp_variable **source_variable)
{
    long i;

    memcpy(variable->data.ptr, source_variable[0]->data.ptr, variable->num_elements * sizeof(double));
    for (i = 0; i < variable->num_elements; i++)
    {
        variable->data.double_data[i] += source_variable[1]->data.double_data[i];
    }

    return 0;
}

static int get_datetime_from_datetime_start_and_stop(harp_variable *variable, const harp_variable **source_variable)
{
    long i;

    for (i = 0; i < variable->num_elements; i++)
    {
        variable->data.double_data[i] = (source_variable[0]->data.double_data[i] +
                                         source_variable[1]->data.double_data[i]) / 2;
    }

    return 0;
}

static int get_datetime_length_from_datetime_start_and_stop(harp_variable *variable,
                                                            const harp_variable **source_variable)
{
    long i;

    for (i = 0; i < variable->num_elements; i++)
    {
        variable->data.double_data[i] =
            source_variable[1]->data.double_data[i] - source_variable[0]->data.double_data[i];
    }

    return 0;
}

static int get_datetime_start_from_datetime_and_length(harp_variable *variable, const harp_variable **source_variable)
{
    long i;

    for (i = 0; i < variable->num_elements; i++)
    {
        variable->data.double_data[i] =
            source_variable[0]->data.double_data[i] - source_variable[1]->data.double_data[i] / 2;
    }

    return 0;
}

static int get_datetime_stop_from_datetime_start_and_length(harp_variable *variable,
                                                            const harp_variable **source_variable)
{
    long i;

    for (i = 0; i < variable->num_elements; i++)
    {
        variable->data.double_data[i] =
            source_variable[0]->data.double_data[i] + source_variable[1]->data.double_data[i];
    }

    return 0;
}

static int get_daytime_ampm_from_longitude(harp_variable *variable, const harp_variable **source_variable)
{
    long i;

    for (i = 0; i < variable->num_elements; i++)
    {
        const char *flag;

        flag = harp_daytime_ampm_from_datetime_and_longitude(source_variable[0]->data.double_data[i],
                                                             source_variable[1]->data.double_data[i]);
        variable->data.string_data[i] = strdup(flag);
        if (variable->data.string_data[i] == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                           __LINE__);
            return -1;
        }
    }

    return 0;
}

static int get_daytime_from_solar_zenith_angle(harp_variable *variable, const harp_variable **source_variable)
{
    long i;

    for (i = 0; i < variable->num_elements; i++)
    {
        variable->data.int8_data[i] = harp_daytime_from_solar_zenith_angle(source_variable[0]->data.double_data[i]);
    }

    return 0;
}

static int get_density_from_nd(harp_variable *variable, const harp_variable **source_variable)
{
    harp_chemical_species species;
    long i;

    species = harp_chemical_species_from_variable_name(variable->name);

    for (i = 0; i < variable->num_elements; i++)
    {
        variable->data.double_data[i] =
            harp_mass_density_from_number_density(source_variable[0]->data.double_data[i], species);
    }

    return 0;
}

static int get_density_from_partial_column_and_alt_bounds(harp_variable *variable,
                                                          const harp_variable **source_variable)
{
    long i;

    for (i = 0; i < variable->num_elements; i++)
    {
        variable->data.double_data[i] =
            harp_density_from_partial_column_and_altitude_bounds(source_variable[0]->data.double_data[i],
                                                                 &source_variable[1]->data.double_data[2 * i]);
    }

    return 0;
}


static int get_elevation_angle_from_zenith_angle(harp_variable *variable, const harp_variable **source_variable)
{
    long i;

    for (i = 0; i < variable->num_elements; i++)
    {
        variable->data.double_data[i] = harp_elevation_angle_from_zenith_angle(source_variable[0]->data.double_data[i]);
    }

    return 0;
}

static int get_empty_double(harp_variable *variable, const harp_variable **source_variable)
{
    long i;

    (void)source_variable;

    for (i = 0; i < variable->num_elements; i++)
    {
        variable->data.double_data[i] = harp_nan();
    }

    return 0;
}

static int get_frequency_from_wavelength(harp_variable *variable, const harp_variable **source_variable)
{
    long i;

    for (i = 0; i < variable->num_elements; i++)
    {
        variable->data.double_data[i] = harp_frequency_from_wavelength(source_variable[0]->data.double_data[i]);
    }

    return 0;
}

static int get_frequency_from_wavenumber(harp_variable *variable, const harp_variable **source_variable)
{
    long i;

    for (i = 0; i < variable->num_elements; i++)
    {
        variable->data.double_data[i] = harp_frequency_from_wavenumber(source_variable[0]->data.double_data[i]);
    }

    return 0;
}

static int get_geopotential_from_gph(harp_variable *variable, const harp_variable **source_variable)
{
    long i;

    for (i = 0; i < variable->num_elements; i++)
    {
        variable->data.double_data[i] = harp_geopotential_from_gph(source_variable[0]->data.double_data[i]);
    }

    return 0;
}

static int get_gph_from_altitude_and_latitude(harp_variable *variable, const harp_variable **source_variable)
{
    long i;

    for (i = 0; i < variable->num_elements; i++)
    {
        variable->data.double_data[i] = harp_gph_from_altitude_and_latitude(source_variable[0]->data.double_data[i],
                                                                            source_variable[1]->data.double_data[i]);
    }

    return 0;
}

static int get_gph_from_geopotential(harp_variable *variable, const harp_variable **source_variable)
{
    long i;

    for (i = 0; i < variable->num_elements; i++)
    {
        variable->data.double_data[i] = harp_gph_from_geopotential(source_variable[0]->data.double_data[i]);
    }

    return 0;
}

static int get_illumination_condition_from_solar_zenith_angle(harp_variable *variable,
                                                              const harp_variable **source_variable)
{
    int i;

    for (i = 0; i < variable->num_elements; i++)
    {
        const char *illumination_condition;

        illumination_condition =
            harp_illumination_condition_from_solar_zenith_angle(source_variable[0]->data.double_data[i]);
        variable->data.string_data[i] = strdup(illumination_condition);
        if (variable->data.string_data[i] == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                           __LINE__);
            return -1;
        }
    }

    return 0;
}

static int get_index(harp_variable *variable, const harp_variable **source_variable)
{
    int32_t i;

    (void)source_variable;

    for (i = 0; i < variable->num_elements; i++)
    {
        variable->data.int32_data[i] = i;
    }

    return 0;
}

static int get_latitude_from_latlon_bounds(harp_variable *variable, const harp_variable **source_variable)
{
    long num_vertices;
    long i;

    num_vertices = source_variable[0]->dimension[source_variable[0]->num_dimensions - 1];

    for (i = 0; i < variable->num_elements; i++)
    {
        harp_spherical_polygon *polygon = NULL;
        harp_vector3d vector_center;
        harp_spherical_point point;

        /* Convert to a spherical polygon */
        if (harp_spherical_polygon_from_longitude_latitude_bounds(i, num_vertices, source_variable[1]->data.double_data,
                                                                  source_variable[0]->data.double_data, &polygon) != 0)
        {
            return -1;
        }

        /* Derive the centre point coordinates */
        if (harp_spherical_polygon_centre(&vector_center, polygon) != 0)
        {
            free(polygon);
            return -1;
        }

        harp_spherical_point_from_vector3d(&point, &vector_center);
        harp_spherical_point_check(&point);
        harp_spherical_point_deg_from_rad(&point);
        variable->data.double_data[i] = point.lat;
        free(polygon);
    }

    return 0;
}

static int get_longitude_from_latlon_bounds(harp_variable *variable, const harp_variable **source_variable)
{
    long num_vertices;
    long i;

    num_vertices = source_variable[0]->dimension[source_variable[0]->num_dimensions - 1];

    for (i = 0; i < variable->num_elements; i++)
    {
        harp_spherical_polygon *polygon = NULL;
        harp_vector3d vector_center;
        harp_spherical_point point;

        /* Convert to a spherical polygon */
        if (harp_spherical_polygon_from_longitude_latitude_bounds(i, num_vertices, source_variable[1]->data.double_data,
                                                                  source_variable[0]->data.double_data, &polygon) != 0)
        {
            return -1;
        }

        /* Derive the centre point coordinates */
        if (harp_spherical_polygon_centre(&vector_center, polygon) != 0)
        {
            free(polygon);
            return -1;
        }

        harp_spherical_point_from_vector3d(&point, &vector_center);
        harp_spherical_point_check(&point);
        harp_spherical_point_deg_from_rad(&point);
        variable->data.double_data[i] = point.lon;
        free(polygon);
    }

    return 0;
}

static int get_mmr_from_vmr(harp_variable *variable, const harp_variable **source_variable)
{
    harp_chemical_species species;
    long i;

    species = harp_chemical_species_from_variable_name(variable->name);

    for (i = 0; i < variable->num_elements; i++)
    {
        variable->data.double_data[i] =
            harp_mass_mixing_ratio_from_volume_mixing_ratio(source_variable[0]->data.double_data[i], species);
    }

    return 0;
}

static int get_matrix_from_sqrt_trace(harp_variable *variable, const harp_variable **source_variable)
{
    long length = variable->dimension[1];
    int i, j;

    for (i = 0; i < variable->num_elements; i++)
    {
        variable->data.double_data[i] = 0;
    }
    for (i = 0; i < variable->dimension[0]; i++)
    {
        for (j = 0; j < length; j++)
        {
            variable->data.double_data[(i * length + j) * length + j] =
                source_variable[0]->data.double_data[i * length + j] *
                source_variable[0]->data.double_data[i * length + j];
        }
    }

    return 0;
}

static int get_midpoint_from_bounds(harp_variable *variable, const harp_variable **source_variable)
{
    int i;

    for (i = 0; i < variable->num_elements; i++)
    {
        variable->data.double_data[i] =
            (source_variable[0]->data.double_data[2 * i] + source_variable[0]->data.double_data[2 * i + 1]) / 2.0;
    }

    return 0;
}

static int get_midpoint_from_bounds_log(harp_variable *variable, const harp_variable **source_variable)
{
    int i;

    for (i = 0; i < variable->num_elements; i++)
    {
        variable->data.double_data[i] = exp((log(source_variable[0]->data.double_data[2 * i]) +
                                             log(source_variable[0]->data.double_data[2 * i + 1])) / 2.0);
    }

    return 0;
}

static int get_nd_cov_from_vmr_cov_pressure_and_temperature(harp_variable *variable,
                                                            const harp_variable **source_variable)
{
    long length = variable->dimension[1];
    long i;

    for (i = 0; i < variable->dimension[0]; i++)
    {
        harp_profile_nd_cov_from_vmr_cov_pressure_and_temperature
            (length, &source_variable[1]->data.double_data[i * length * length],
             &source_variable[2]->data.double_data[i * length], &source_variable[3]->data.double_data[i * length],
             &variable->data.double_data[i * length * length]);
    }
    return 0;
}

static int get_nd_from_density(harp_variable *variable, const harp_variable **source_variable)
{
    harp_chemical_species species;
    long i;

    species = harp_chemical_species_from_variable_name(variable->name);

    for (i = 0; i < variable->num_elements; i++)
    {
        variable->data.double_data[i] =
            harp_number_density_from_mass_density(source_variable[0]->data.double_data[i], species);
    }

    return 0;
}

static int get_nd_from_vmr_pressure_and_temperature(harp_variable *variable, const harp_variable **source_variable)
{
    long i;

    for (i = 0; i < variable->num_elements; i++)
    {
        variable->data.double_data[i] = harp_number_density_from_volume_mixing_ratio_pressure_and_temperature
            (source_variable[0]->data.double_data[i], source_variable[1]->data.double_data[i],
             source_variable[2]->data.double_data[i]);
    }

    return 0;
}

static int get_normalized_radiance_from_radiance_and_solar_irradiance(harp_variable *variable,
                                                                      const harp_variable **source_variable)
{
    long i;

    for (i = 0; i < variable->num_elements; i++)
    {
        variable->data.double_data[i] =
            harp_normalized_radiance_from_radiance_and_solar_irradiance(source_variable[0]->data.double_data[i],
                                                                        source_variable[1]->data.double_data[i]);
    }
    return 0;
}

static int get_normalized_radiance_from_reflectance_and_solar_zenith_angle(harp_variable *variable,
                                                                           const harp_variable **source_variable)
{
    long i;

    if (variable->num_dimensions == 1)
    {
        for (i = 0; i < variable->num_elements; i++)
        {
            variable->data.double_data[i] =
                harp_normalized_radiance_from_reflectance_and_solar_zenith_angle
                (source_variable[0]->data.double_data[i], source_variable[1]->data.double_data[i]);
        }
    }
    else
    {
        long length = variable->dimension[0];
        long j;

        /* num_dimensions == 2 */
        for (i = 0; i < length; i++)
        {
            for (j = 0; j < variable->dimension[1]; j++)
            {
                variable->data.double_data[i * length + j] =
                    harp_normalized_radiance_from_reflectance_and_solar_zenith_angle
                    (source_variable[0]->data.double_data[i * length + j], source_variable[1]->data.double_data[i]);
            }
        }

    }
    return 0;
}

static int get_partial_column_from_density_and_alt_bounds(harp_variable *variable,
                                                          const harp_variable **source_variable)
{
    long i;

    for (i = 0; i < variable->num_elements; i++)
    {
        variable->data.double_data[i] =
            harp_partial_column_from_density_and_altitude_bounds(source_variable[0]->data.double_data[i],
                                                                 &source_variable[1]->data.double_data[2 * i]);
    }

    return 0;
}

static int get_partial_column_cov_from_density_cov_and_alt_bounds(harp_variable *variable,
                                                                  const harp_variable **source_variable)
{
    long length = variable->dimension[1];
    long i;

    for (i = 0; i < variable->dimension[0]; i++)
    {
        harp_profile_partial_column_cov_from_density_cov_and_altitude_bounds
            (length, &source_variable[1]->data.double_data[i * length * length],
             &source_variable[2]->data.double_data[i * length * 2], &variable->data.double_data[i * length * length]);
    }

    return 0;
}

static int get_partial_pressure_from_vmr_and_pressure(harp_variable *variable, const harp_variable **source_variable)
{
    long i;

    for (i = 0; i < variable->num_elements; i++)
    {
        variable->data.double_data[i] =
            harp_partial_pressure_from_volume_mixing_ratio_and_pressure(source_variable[0]->data.double_data[i],
                                                                        source_variable[1]->data.double_data[i]);
    }

    return 0;
}

static int get_pressure_from_altitude_temperature_h2o_mmr_and_latitude(harp_variable *variable,
                                                                       const harp_variable **source_variable)
{
    long length = variable->dimension[1];
    long i;

    for (i = 0; i < variable->dimension[0]; i++)
    {
        if (harp_profile_pressure_from_altitude_temperature_h2o_mmr_and_latitude
            (length, &source_variable[0]->data.double_data[i * length],
             &source_variable[1]->data.double_data[i * length], &source_variable[2]->data.double_data[i * length],
             CONST_STD_PRESSURE, 0, source_variable[3]->data.double_data[i],
             &variable->data.double_data[i * length]) != 0)
        {
            return -1;
        }
    }

    return 0;
}

static int get_pressure_from_altitude_temperature_and_latitude(harp_variable *variable,
                                                               const harp_variable **source_variable)
{
    long length = variable->dimension[1];
    long i;

    for (i = 0; i < variable->dimension[0]; i++)
    {
        if (harp_profile_pressure_from_altitude_temperature_h2o_mmr_and_latitude
            (length, &source_variable[0]->data.double_data[i * length],
             &source_variable[1]->data.double_data[i * length], NULL, CONST_STD_PRESSURE, 0,
             source_variable[2]->data.double_data[i], &variable->data.double_data[i * length]) != 0)
        {
            return -1;
        }
    }

    return 0;
}

static int get_pressure_from_gph_temperature_and_h2o_mmr(harp_variable *variable, const harp_variable **source_variable)
{
    long length = variable->dimension[1];
    long i;

    for (i = 0; i < variable->dimension[0]; i++)
    {
        if (harp_profile_pressure_from_gph_temperature_and_h2o_mmr(length,
                                                                   &source_variable[0]->data.double_data[i * length],
                                                                   &source_variable[1]->data.double_data[i * length],
                                                                   &source_variable[2]->data.double_data[i * length],
                                                                   CONST_STD_PRESSURE, 0,
                                                                   &variable->data.double_data[i * length]) != 0)
        {
            return -1;
        }
    }

    return 0;
}

static int get_pressure_from_gph_and_temperature(harp_variable *variable, const harp_variable **source_variable)
{
    long length = variable->dimension[1];
    long i;

    for (i = 0; i < variable->dimension[0]; i++)
    {
        if (harp_profile_pressure_from_gph_temperature_and_h2o_mmr(length,
                                                                   &source_variable[0]->data.double_data[i * length],
                                                                   &source_variable[1]->data.double_data[i * length],
                                                                   NULL, CONST_STD_PRESSURE, 0,
                                                                   &variable->data.double_data[i * length]) != 0)
        {
            return -1;
        }
    }

    return 0;
}

static int get_radiance_from_normalized_radiance_and_solar_irradiance(harp_variable *variable,
                                                                      const harp_variable **source_variable)
{
    long i;

    for (i = 0; i < variable->num_elements; i++)
    {
        variable->data.double_data[i] =
            harp_radiance_from_normalized_radiance_and_solar_irradiance(source_variable[0]->data.double_data[i],
                                                                        source_variable[1]->data.double_data[i]);
    }

    return 0;
}

static int get_reflectance_from_normalized_radiance_and_solar_zenith_angle(harp_variable *variable,
                                                                           const harp_variable **source_variable)
{
    long i;

    if (variable->num_dimensions == 1)
    {
        for (i = 0; i < variable->num_elements; i++)
        {
            variable->data.double_data[i] =
                harp_reflectance_from_normalized_radiance_and_solar_zenith_angle
                (source_variable[0]->data.double_data[i], source_variable[1]->data.double_data[i]);
        }
    }
    else
    {
        long length = variable->dimension[0];
        long j;

        /* num_dimensions == 2 */
        for (i = 0; i < length; i++)
        {
            for (j = 0; j < variable->dimension[1]; j++)
            {
                variable->data.double_data[i * length + j] =
                    harp_reflectance_from_normalized_radiance_and_solar_zenith_angle
                    (source_variable[0]->data.double_data[i * length + j], source_variable[1]->data.double_data[i]);
            }
        }

    }

    return 0;
}

static int get_relative_humidity_from_h2o_nd_and_temperature(harp_variable *variable,
                                                             const harp_variable **source_variable)
{
    long i;

    for (i = 0; i < variable->num_elements; i++)
    {
        variable->data.double_data[i] =
            harp_relative_humidity_from_h2o_number_density_and_temperature(source_variable[0]->data.double_data[i],
                                                                           source_variable[1]->data.double_data[i]);
    }

    return 0;
}

static int get_scattering_angle_from_solar_angles_and_viewing_angles(harp_variable *variable,
                                                                     const harp_variable **source_variable)
{
    long i;

    for (i = 0; i < variable->num_elements; i++)
    {
        variable->data.double_data[i] =
            harp_scattering_angle_from_solar_angles_and_viewing_angles(source_variable[0]->data.double_data[i],
                                                                       source_variable[1]->data.double_data[i],
                                                                       source_variable[2]->data.double_data[i],
                                                                       source_variable[3]->data.double_data[i]);
    }

    return 0;
}

static int get_solar_elevation_angle_from_datetime_and_latlon(harp_variable *variable,
                                                              const harp_variable **source_variable)
{
    long i;

    for (i = 0; i < variable->num_elements; i++)
    {
        variable->data.double_data[i] =
            harp_solar_elevation_angle_from_datetime_longitude_and_latitude(source_variable[0]->data.double_data[i],
                                                                            source_variable[2]->data.double_data[i],
                                                                            source_variable[1]->data.double_data[i]);
    }

    return 0;
}

static int get_sqrt_trace_from_matrix(harp_variable *variable, const harp_variable **source_variable)
{
    long length = variable->dimension[1];
    int i, j;

    for (i = 0; i < variable->dimension[0]; i++)
    {
        for (j = 0; j < length; j++)
        {
            variable->data.double_data[i * length + j] =
                sqrt(source_variable[0]->data.double_data[(i * length + j) * length + j]);
        }
    }

    return 0;
}

static int get_uncertainty_from_systematic_and_random_uncertainty(harp_variable *variable,
                                                                  const harp_variable **source_variable)
{
    long i;

    for (i = 0; i < variable->num_elements; i++)
    {
        variable->data.double_data[i] =
            sqrt(source_variable[0]->data.double_data[i] * source_variable[0]->data.double_data[i] +
                 source_variable[1]->data.double_data[i] * source_variable[1]->data.double_data[i]);
    }

    return 0;
}

static int get_time_dependent_from_time_independent(harp_variable *variable, const harp_variable **source_variable)
{
    int i;

    if (source_variable[0]->data_type == harp_type_string)
    {
        long num_block_elements = source_variable[0]->num_elements;
        int j;

        for (i = 0; i < variable->dimension[0]; i++)
        {
            for (j = 0; j < num_block_elements; j++)
            {
                variable->data.string_data[i * num_block_elements + j] =
                    strdup(source_variable[0]->data.string_data[j]);
                if (variable->data.string_data[i * num_block_elements + j] == NULL)
                {
                    harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)",
                                   __FILE__, __LINE__);
                    return -1;
                }
            }
        }
    }
    else
    {
        size_t block_size = source_variable[0]->num_elements * harp_get_size_for_type(source_variable[0]->data_type);

        for (i = 0; i < variable->dimension[0]; i++)
        {
            memcpy(&variable->data.int8_data[i * block_size], source_variable[0]->data.ptr, block_size);
        }
    }

    return 0;
}

static int get_virtual_temperature_from_pressure_temperature_and_relative_humidity
    (harp_variable *variable, const harp_variable **source_variable)
{
    long i;

    for (i = 0; i < variable->num_elements; i++)
    {
        variable->data.double_data[i] =
            harp_virtual_temperature_from_pressure_temperature_and_relative_humidity
            (source_variable[0]->data.double_data[i], source_variable[1]->data.double_data[i],
             source_variable[2]->data.double_data[i]);
    }

    return 0;

}

static int get_vmr_cov_from_nd_cov_pressure_and_temperature(harp_variable *variable,
                                                            const harp_variable **source_variable)
{
    long length = variable->dimension[1];
    long i;

    for (i = 0; i < variable->dimension[0]; i++)
    {
        harp_profile_vmr_cov_from_nd_cov_pressure_and_temperature
            (length, &source_variable[1]->data.double_data[i * length * length],
             &source_variable[2]->data.double_data[i * length], &source_variable[3]->data.double_data[i * length],
             &variable->data.double_data[i * length * length]);
    }
    return 0;
}

static int get_vmr_from_mmr(harp_variable *variable, const harp_variable **source_variable)
{
    harp_chemical_species species;
    long i;

    species = harp_chemical_species_from_variable_name(variable->name);

    for (i = 0; i < variable->num_elements; i++)
    {
        variable->data.double_data[i] =
            harp_volume_mixing_ratio_from_mass_mixing_ratio(source_variable[0]->data.double_data[i], species);
    }

    return 0;
}

static int get_vmr_from_mmrw_and_humidity(harp_variable *variable, const harp_variable **source_variable)
{
    harp_chemical_species species;
    long i;

    species = harp_chemical_species_from_variable_name(variable->name);

    for (i = 0; i < variable->num_elements; i++)
    {
        variable->data.double_data[i] =
            harp_volume_mixing_ratio_from_mass_mixing_ratio_wet_and_humidity(source_variable[0]->data.double_data[i],
                                                                             source_variable[1]->data.double_data[i],
                                                                             species);
    }

    return 0;
}

static int get_vmr_from_nd_pressure_and_temperature(harp_variable *variable, const harp_variable **source_variable)
{
    long i;

    for (i = 0; i < variable->num_elements; i++)
    {
        variable->data.double_data[i] = harp_volume_mixing_ratio_from_number_density_pressure_and_temperature
            (source_variable[0]->data.double_data[i], source_variable[1]->data.double_data[i],
             source_variable[2]->data.double_data[i]);
    }

    return 0;
}

static int get_vmr_from_partial_pressure_and_pressure(harp_variable *variable, const harp_variable **source_variable)
{
    long i;

    for (i = 0; i < variable->num_elements; i++)
    {
        variable->data.double_data[i] =
            harp_volume_mixing_ratio_from_partial_pressure_and_pressure(source_variable[0]->data.double_data[i],
                                                                        source_variable[1]->data.double_data[i]);
    }
    return 0;
}

static int get_wavelength_from_frequency(harp_variable *variable, const harp_variable **source_variable)
{
    long i;

    for (i = 0; i < variable->num_elements; i++)
    {
        variable->data.double_data[i] = harp_wavelength_from_frequency(source_variable[0]->data.double_data[i]);
    }

    return 0;
}

static int get_wavelength_from_wavenumber(harp_variable *variable, const harp_variable **source_variable)
{
    long i;

    for (i = 0; i < variable->num_elements; i++)
    {
        variable->data.double_data[i] = harp_wavelength_from_wavenumber(source_variable[0]->data.double_data[i]);
    }

    return 0;
}

static int get_wavenumber_from_frequency(harp_variable *variable, const harp_variable **source_variable)
{
    long i;

    for (i = 0; i < variable->num_elements; i++)
    {
        variable->data.double_data[i] = harp_wavenumber_from_frequency(source_variable[0]->data.double_data[i]);
    }

    return 0;
}

static int get_wavenumber_from_wavelength(harp_variable *variable, const harp_variable **source_variable)
{
    long i;

    for (i = 0; i < variable->num_elements; i++)
    {
        variable->data.double_data[i] = harp_wavenumber_from_wavelength(source_variable[0]->data.double_data[i]);
    }

    return 0;
}

static int get_zenith_angle_from_elevation_angle(harp_variable *variable, const harp_variable **source_variable)
{
    long i;

    for (i = 0; i < variable->num_elements; i++)
    {
        variable->data.double_data[i] = harp_zenith_angle_from_elevation_angle(source_variable[0]->data.double_data[i]);
    }

    return 0;
}

/* the provided dimension information should be the one that is already time dependent */
static int add_time_indepedent_to_dependent_conversion(const char *variable_name, harp_data_type data_type,
                                                       const char *unit, int num_dimensions,
                                                       harp_dimension_type dimension_type[HARP_MAX_NUM_DIMS],
                                                       long independent_dimension_length)
{
    harp_variable_conversion *conversion;

    if (harp_variable_conversion_new(variable_name, data_type, unit, num_dimensions, dimension_type,
                                     independent_dimension_length, get_time_dependent_from_time_independent,
                                     &conversion) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, variable_name, data_type, unit, num_dimensions - 1,
                                            &dimension_type[1], independent_dimension_length) != 0)
    {
        return -1;
    }

    return 0;
}

static int add_aux_afgl86_conversion(const char *variable_name, const char *unit)
{
    harp_variable_conversion *conversion;
    harp_dimension_type dimension_type[HARP_MAX_NUM_DIMS];

    dimension_type[0] = harp_dimension_time;
    dimension_type[1] = harp_dimension_vertical;

    if (harp_variable_conversion_new(variable_name, harp_type_double, unit, 2, dimension_type, 0,
                                     get_aux_variable_afgl86, &conversion) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, "datetime", harp_type_double, HARP_UNIT_DATETIME, 1,
                                            dimension_type, 0) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, "latitude", harp_type_double, HARP_UNIT_LATITUDE, 1,
                                            dimension_type, 0) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, "altitude", harp_type_double, HARP_UNIT_LENGTH, 2,
                                            dimension_type, 0) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_set_source_description(conversion, "using built-in AFGL86 climatology") != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_set_enabled_function(conversion, harp_get_option_enable_aux_afgl86) != 0)
    {
        return -1;
    }

    return 0;
}

static int add_aux_usstd76_conversion(const char *variable_name, const char *unit)
{
    harp_variable_conversion *conversion;
    harp_dimension_type dimension_type[HARP_MAX_NUM_DIMS];

    dimension_type[0] = harp_dimension_time;
    dimension_type[1] = harp_dimension_vertical;

    if (harp_variable_conversion_new(variable_name, harp_type_double, unit, 2, dimension_type, 0,
                                     get_aux_variable_usstd76, &conversion) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, "altitude", harp_type_double, HARP_UNIT_LENGTH, 2,
                                            dimension_type, 0) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_set_source_description(conversion, "using built-in US Standard 76 climatology") != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_set_enabled_function(conversion, harp_get_option_enable_aux_usstd76) != 0)
    {
        return -1;
    }

    return 0;
}

static int add_bounds_to_midpoint_conversion(const char *variable_name, harp_data_type data_type, const char *unit,
                                             harp_dimension_type axis_dimension_type,
                                             harp_conversion_function conversion_function)
{
    harp_variable_conversion *conversion;
    harp_dimension_type dimension_type[HARP_MAX_NUM_DIMS];
    char name_bounds[MAX_NAME_LENGTH];

    snprintf(name_bounds, MAX_NAME_LENGTH, "%s_bounds", variable_name);

    dimension_type[0] = harp_dimension_independent;
    if (harp_variable_conversion_new(variable_name, data_type, unit, 0, dimension_type, 0, conversion_function,
                                     &conversion) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, name_bounds, data_type, unit, 1, dimension_type, 2) != 0)
    {
        return -1;
    }

    dimension_type[0] = axis_dimension_type;
    dimension_type[1] = harp_dimension_independent;
    if (harp_variable_conversion_new(variable_name, data_type, unit, 1, dimension_type, 0, conversion_function,
                                     &conversion) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, name_bounds, data_type, unit, 2, dimension_type, 2) != 0)
    {
        return -1;
    }

    dimension_type[0] = harp_dimension_time;
    if (harp_variable_conversion_new(variable_name, data_type, unit, 1, dimension_type, 0, conversion_function,
                                     &conversion) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, name_bounds, data_type, unit, 2, dimension_type, 2) != 0)
    {
        return -1;
    }

    dimension_type[1] = axis_dimension_type;
    dimension_type[2] = harp_dimension_independent;
    if (harp_variable_conversion_new(variable_name, data_type, unit, 2, dimension_type, 0, conversion_function,
                                     &conversion) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, name_bounds, data_type, unit, 3, dimension_type, 2) != 0)
    {
        return -1;
    }

    return 0;
}

static int add_latlon_bounds_to_midpoint_conversion(const char *variable_name, harp_data_type data_type,
                                                    const char *unit, harp_conversion_function conversion_function)
{
    harp_variable_conversion *conversion;
    harp_dimension_type dimension_type[HARP_MAX_NUM_DIMS];

    dimension_type[0] = harp_dimension_independent;
    if (harp_variable_conversion_new(variable_name, data_type, unit, 0, dimension_type, 0, conversion_function,
                                     &conversion) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, "latitude_bounds", data_type, HARP_UNIT_LATITUDE, 1,
                                            dimension_type, -1) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, "longitude_bounds", data_type, HARP_UNIT_LONGITUDE, 1,
                                            dimension_type, -1) != 0)
    {
        return -1;
    }

    dimension_type[0] = harp_dimension_time;
    dimension_type[1] = harp_dimension_independent;
    if (harp_variable_conversion_new(variable_name, data_type, unit, 1, dimension_type, 0, conversion_function,
                                     &conversion) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, "latitude_bounds", data_type, HARP_UNIT_LATITUDE, 2,
                                            dimension_type, -1) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, "longitude_bounds", data_type, HARP_UNIT_LONGITUDE, 2,
                                            dimension_type, -1) != 0)
    {
        return -1;
    }

    return 0;
}

static int add_midpoint_to_bounds_conversion(const char *variable_name, harp_data_type data_type, const char *unit,
                                             harp_dimension_type axis_dimension_type,
                                             harp_conversion_function conversion_function)
{
    harp_variable_conversion *conversion;
    harp_dimension_type dimension_type[HARP_MAX_NUM_DIMS];
    char name_bounds[MAX_NAME_LENGTH];

    snprintf(name_bounds, MAX_NAME_LENGTH, "%s_bounds", variable_name);

    dimension_type[0] = axis_dimension_type;
    dimension_type[1] = harp_dimension_independent;
    if (harp_variable_conversion_new(name_bounds, data_type, unit, 2, dimension_type, 2, conversion_function,
                                     &conversion) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, variable_name, data_type, unit, 1, dimension_type, 0) != 0)
    {
        return -1;
    }

    dimension_type[0] = harp_dimension_time;
    dimension_type[1] = axis_dimension_type;
    dimension_type[2] = harp_dimension_independent;
    if (add_time_indepedent_to_dependent_conversion(name_bounds, data_type, unit, 3, dimension_type, 2) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_new(name_bounds, data_type, unit, 3, dimension_type, 2, conversion_function,
                                     &conversion) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, variable_name, data_type, unit, 2, dimension_type, 0) != 0)
    {
        return -1;
    }

    return 0;
}

static int add_latlon_midpoints_to_bounds_conversion(const char *variable_name, harp_data_type data_type,
                                                     const char *unit, harp_dimension_type axis_dimension_type,
                                                     harp_conversion_function conversion_function)
{
    harp_variable_conversion *conversion;
    harp_dimension_type dimension_type[HARP_MAX_NUM_DIMS];
    char name_bounds[MAX_NAME_LENGTH];

    snprintf(name_bounds, MAX_NAME_LENGTH, "%s_bounds", variable_name);

    dimension_type[0] = axis_dimension_type;
    dimension_type[1] = harp_dimension_independent;
    if (harp_variable_conversion_new(name_bounds, data_type, unit, 2, dimension_type, 2, conversion_function,
                                     &conversion) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, variable_name, data_type, unit, 1, dimension_type, 0) != 0)
    {
        return -1;
    }

    return 0;
}

static int add_uncertainty_conversions(const char *variable_name, const char *unit)
{
    harp_variable_conversion *conversion;
    harp_dimension_type dimension_type[HARP_MAX_NUM_DIMS];
    char name_uncertainty[MAX_NAME_LENGTH];
    char name_uncertainty_sys[MAX_NAME_LENGTH];
    char name_uncertainty_rnd[MAX_NAME_LENGTH];

    snprintf(name_uncertainty, MAX_NAME_LENGTH, "%s_uncertainty", variable_name);
    snprintf(name_uncertainty_sys, MAX_NAME_LENGTH, "%s_uncertainty_systematic", variable_name);
    snprintf(name_uncertainty_rnd, MAX_NAME_LENGTH, "%s_uncertainty_random", variable_name);

    dimension_type[0] = harp_dimension_time;

    if (harp_variable_conversion_new(name_uncertainty, harp_type_double, unit, 1, dimension_type, 0,
                                     get_uncertainty_from_systematic_and_random_uncertainty, &conversion) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, name_uncertainty_sys, harp_type_double, unit, 1,
                                            dimension_type, 0) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, name_uncertainty_rnd, harp_type_double, unit, 1,
                                            dimension_type, 0) != 0)
    {
        return -1;
    }

    if (harp_variable_conversion_new(name_uncertainty, harp_type_double, unit, 1, dimension_type, 0, get_empty_double,
                                     &conversion) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, variable_name, harp_type_double, unit, 1, dimension_type, 0) !=
        0)
    {
        return -1;
    }
    if (harp_variable_conversion_set_source_description(conversion, "all values will be set to NaN") != 0)
    {
        return -1;
    }

    return 0;
}

static int add_spectral_uncertainty_conversions(const char *variable_name, const char *unit)
{
    harp_variable_conversion *conversion;
    harp_dimension_type dimension_type[HARP_MAX_NUM_DIMS];
    char name_uncertainty[MAX_NAME_LENGTH];
    char name_uncertainty_sys[MAX_NAME_LENGTH];
    char name_uncertainty_rnd[MAX_NAME_LENGTH];
    int i;

    snprintf(name_uncertainty, MAX_NAME_LENGTH, "%s_uncertainty", variable_name);
    snprintf(name_uncertainty_sys, MAX_NAME_LENGTH, "%s_uncertainty_systematic", variable_name);
    snprintf(name_uncertainty_rnd, MAX_NAME_LENGTH, "%s_uncertainty_random", variable_name);

    dimension_type[0] = harp_dimension_time;
    dimension_type[1] = harp_dimension_spectral;

    for (i = 1; i < 3; i++)
    {
        if (harp_variable_conversion_new(name_uncertainty, harp_type_double, unit, i, dimension_type, 0,
                                         get_uncertainty_from_systematic_and_random_uncertainty, &conversion) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, name_uncertainty_sys, harp_type_double, unit, i,
                                                dimension_type, 0) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, name_uncertainty_rnd, harp_type_double, unit, i,
                                                dimension_type, 0) != 0)
        {
            return -1;
        }

        if (harp_variable_conversion_new(name_uncertainty, harp_type_double, unit, i, dimension_type, 0,
                                         get_empty_double, &conversion) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, variable_name, harp_type_double, unit, i, dimension_type, 0)
            != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_set_source_description(conversion, "all values will be set to NaN") != 0)
        {
            return -1;
        }
    }

    return 0;
}

static int add_vertical_uncertainty_conversions(const char *variable_name, const char *unit, const char *unit_squared)
{
    harp_variable_conversion *conversion;
    harp_dimension_type dimension_type[HARP_MAX_NUM_DIMS];
    char name_uncertainty[MAX_NAME_LENGTH];
    char name_uncertainty_sys[MAX_NAME_LENGTH];
    char name_uncertainty_rnd[MAX_NAME_LENGTH];
    int i;

    snprintf(name_uncertainty, MAX_NAME_LENGTH, "%s_uncertainty", variable_name);
    snprintf(name_uncertainty_sys, MAX_NAME_LENGTH, "%s_uncertainty_systematic", variable_name);
    snprintf(name_uncertainty_rnd, MAX_NAME_LENGTH, "%s_uncertainty_random", variable_name);

    dimension_type[0] = harp_dimension_time;
    dimension_type[1] = harp_dimension_vertical;

    if (unit_squared != NULL)
    {
        char name_cov[MAX_NAME_LENGTH];
        char name_cov_sys[MAX_NAME_LENGTH];
        char name_cov_rnd[MAX_NAME_LENGTH];

        snprintf(name_cov, MAX_NAME_LENGTH, "%s_cov", variable_name);
        snprintf(name_cov_sys, MAX_NAME_LENGTH, "%s_cov_systematic", variable_name);
        snprintf(name_cov_rnd, MAX_NAME_LENGTH, "%s_cov_random", variable_name);

        dimension_type[2] = harp_dimension_vertical;

        if (harp_variable_conversion_new(name_cov, harp_type_double, unit, 3, dimension_type, 0,
                                         get_cov_from_systematic_and_random_cov, &conversion) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, name_cov_sys, harp_type_double, unit, 3, dimension_type,
                                                0) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, name_cov_rnd, harp_type_double, unit, 3, dimension_type,
                                                0) != 0)
        {
            return -1;
        }

        if (harp_variable_conversion_new(name_uncertainty, harp_type_double, unit, 2, dimension_type, 0,
                                         get_sqrt_trace_from_matrix, &conversion) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, name_cov, harp_type_double, unit_squared, 3, dimension_type,
                                                0) != 0)
        {
            return -1;
        }

        if (harp_variable_conversion_new(name_cov, harp_type_double, unit_squared, 3, dimension_type, 0,
                                         get_matrix_from_sqrt_trace, &conversion) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, name_uncertainty, harp_type_double, unit, 2,
                                                dimension_type, 0) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_set_source_description(conversion, "all off-diagonal values will be set to 0") !=
            0)
        {
            return -1;
        }
    }
    for (i = 1; i < 3; i++)
    {
        if (harp_variable_conversion_new(name_uncertainty, harp_type_double, unit, i, dimension_type, 0,
                                         get_uncertainty_from_systematic_and_random_uncertainty, &conversion) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, name_uncertainty_sys, harp_type_double, unit, i,
                                                dimension_type, 0) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, name_uncertainty_rnd, harp_type_double, unit, i,
                                                dimension_type, 0) != 0)
        {
            return -1;
        }

        if (harp_variable_conversion_new(name_uncertainty, harp_type_double, unit, i, dimension_type, 0,
                                         get_empty_double, &conversion) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, variable_name, harp_type_double, unit, i, dimension_type, 0)
            != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_set_source_description(conversion, "all values will be set to NaN") != 0)
        {
            return -1;
        }
    }

    return 0;
}

static int add_species_conversions(const char *species)
{
    harp_variable_conversion *conversion;
    harp_dimension_type dimension_type[HARP_MAX_NUM_DIMS];
    char name_column_nd[MAX_NAME_LENGTH];
    char name_column_nd_cov[MAX_NAME_LENGTH];
    char name_column_nd_uncertainty[MAX_NAME_LENGTH];
    char name_density[MAX_NAME_LENGTH];
    char name_mmr[MAX_NAME_LENGTH];
    char name_mmr_cov[MAX_NAME_LENGTH];
    char name_mmr_uncertainty[MAX_NAME_LENGTH];
    char name_mmrw[MAX_NAME_LENGTH];
    char name_nd[MAX_NAME_LENGTH];
    char name_nd_cov[MAX_NAME_LENGTH];
    char name_nd_uncertainty[MAX_NAME_LENGTH];
    char name_pp[MAX_NAME_LENGTH];
    char name_vmr[MAX_NAME_LENGTH];
    char name_vmr_cov[MAX_NAME_LENGTH];
    char name_vmr_uncertainty[MAX_NAME_LENGTH];
    int i;

    if (strcmp(species, "air") == 0)
    {
        /* These conversions are not applicable to air */
        return 0;
    }

    snprintf(name_column_nd, MAX_NAME_LENGTH, "%s_column_number_density", species);
    snprintf(name_column_nd_cov, MAX_NAME_LENGTH, "%s_column_number_density_cov", species);
    snprintf(name_column_nd_uncertainty, MAX_NAME_LENGTH, "%s_column_number_density_uncertainty", species);
    snprintf(name_density, MAX_NAME_LENGTH, "%s_density", species);
    snprintf(name_mmr, MAX_NAME_LENGTH, "%s_mass_mixing_ratio", species);
    snprintf(name_mmr_cov, MAX_NAME_LENGTH, "%s_mass_mixing_ratio_cov", species);
    snprintf(name_mmr_uncertainty, MAX_NAME_LENGTH, "%s_mass_mixing_ratio_uncertainty", species);
    snprintf(name_mmrw, MAX_NAME_LENGTH, "%s_mass_mixing_ratio_wet", species);
    snprintf(name_nd, MAX_NAME_LENGTH, "%s_number_density", species);
    snprintf(name_nd_cov, MAX_NAME_LENGTH, "%s_number_density_cov", species);
    snprintf(name_nd_uncertainty, MAX_NAME_LENGTH, "%s_number_density_uncertainty", species);
    snprintf(name_pp, MAX_NAME_LENGTH, "%s_partial_pressure", species);
    snprintf(name_vmr, MAX_NAME_LENGTH, "%s_volume_mixing_ratio", species);
    snprintf(name_vmr_cov, MAX_NAME_LENGTH, "%s_volume_mixing_ratio_cov", species);
    snprintf(name_vmr_uncertainty, MAX_NAME_LENGTH, "%s_volume_mixing_ratio_uncertainty", species);

    dimension_type[0] = harp_dimension_time;
    dimension_type[1] = harp_dimension_vertical;
    dimension_type[2] = harp_dimension_vertical;


    /* column number density */
    if (harp_variable_conversion_new(name_column_nd, harp_type_double, HARP_UNIT_COLUMN_NUMBER_DENSITY, 1,
                                     dimension_type, 0, get_column_from_partial_column, &conversion) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, name_column_nd, harp_type_double,
                                            HARP_UNIT_COLUMN_NUMBER_DENSITY, 2, dimension_type, 0) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_new(name_column_nd_uncertainty, harp_type_double, HARP_UNIT_COLUMN_NUMBER_DENSITY, 1,
                                     dimension_type, 0, get_column_uncertainty_from_partial_column_uncertainty,
                                     &conversion) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, name_column_nd_uncertainty, harp_type_double,
                                            HARP_UNIT_COLUMN_NUMBER_DENSITY, 2, dimension_type, 0) != 0)
    {
        return -1;
    }
    if (add_uncertainty_conversions(name_column_nd, HARP_UNIT_COLUMN_NUMBER_DENSITY) != 0)
    {
        return -1;
    }

    if (harp_variable_conversion_new(name_column_nd, harp_type_double, HARP_UNIT_COLUMN_NUMBER_DENSITY, 2,
                                     dimension_type, 0, get_partial_column_from_density_and_alt_bounds, &conversion) !=
        0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, name_nd, harp_type_double, HARP_UNIT_NUMBER_DENSITY, 2,
                                            dimension_type, 0) != 0)
    {
        return -1;
    }
    dimension_type[2] = harp_dimension_independent;
    if (harp_variable_conversion_add_source(conversion, "altitude_bounds", harp_type_double, HARP_UNIT_LENGTH, 3,
                                            dimension_type, 2) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_new(name_column_nd_uncertainty, harp_type_double, HARP_UNIT_COLUMN_NUMBER_DENSITY, 2,
                                     dimension_type, 0, get_partial_column_from_density_and_alt_bounds, &conversion) !=
        0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, name_nd_uncertainty, harp_type_double, HARP_UNIT_NUMBER_DENSITY,
                                            2, dimension_type, 0) != 0)
    {
        return -1;
    }
    dimension_type[2] = harp_dimension_independent;
    if (harp_variable_conversion_add_source(conversion, "altitude_bounds", harp_type_double, HARP_UNIT_LENGTH, 3,
                                            dimension_type, 2) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_new(name_column_nd_cov, harp_type_double, HARP_UNIT_COLUMN_NUMBER_DENSITY_SQUARED, 3,
                                     dimension_type, 0, get_partial_column_cov_from_density_cov_and_alt_bounds,
                                     &conversion) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, name_nd_cov, harp_type_double,
                                            HARP_UNIT_NUMBER_DENSITY_SQUARED, 3, dimension_type, 0) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, "altitude_bounds", harp_type_double, HARP_UNIT_LENGTH, 3,
                                            dimension_type, 2) != 0)
    {
        return -1;
    }

    if (add_vertical_uncertainty_conversions(name_column_nd, HARP_UNIT_COLUMN_NUMBER_DENSITY,
                                             HARP_UNIT_COLUMN_NUMBER_DENSITY_SQUARED) != 0)
    {
        return -1;
    }

    /* number density */
    for (i = 1; i < 3; i++)
    {
        if (harp_variable_conversion_new(name_nd, harp_type_double, HARP_UNIT_NUMBER_DENSITY, i, dimension_type, 0,
                                         get_nd_from_density, &conversion) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, name_density, harp_type_double, HARP_UNIT_MASS_DENSITY, i,
                                                dimension_type, 0) != 0)
        {
            return -1;
        }

        if (harp_variable_conversion_new(name_nd, harp_type_double, HARP_UNIT_NUMBER_DENSITY, i, dimension_type, 0,
                                         get_nd_from_vmr_pressure_and_temperature, &conversion) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, name_vmr, harp_type_double, HARP_UNIT_VOLUME_MIXING_RATIO,
                                                i, dimension_type, 0) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, "pressure", harp_type_double, HARP_UNIT_PRESSURE, i,
                                                dimension_type, 0) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, "temperature", harp_type_double, HARP_UNIT_TEMPERATURE, i,
                                                dimension_type, 0) != 0)
        {
            return -1;
        }
    }
    if (harp_variable_conversion_new(name_nd_cov, harp_type_double, HARP_UNIT_NUMBER_DENSITY_SQUARED, 3, dimension_type,
                                     0, get_nd_cov_from_vmr_cov_pressure_and_temperature, &conversion) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, name_vmr_cov, harp_type_double,
                                            HARP_UNIT_VOLUME_MIXING_RATIO_SQUARED, 3, dimension_type, 0) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, "pressure", harp_type_double, HARP_UNIT_PRESSURE, 2,
                                            dimension_type, 0) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, "temperature", harp_type_double, HARP_UNIT_TEMPERATURE, 2,
                                            dimension_type, 0) != 0)
    {
        return -1;
    }

    if (harp_variable_conversion_new(name_nd, harp_type_double, HARP_UNIT_NUMBER_DENSITY, 2, dimension_type, 0,
                                     get_density_from_partial_column_and_alt_bounds, &conversion) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, name_column_nd, harp_type_double,
                                            HARP_UNIT_COLUMN_NUMBER_DENSITY, 2, dimension_type, 0) != 0)
    {
        return -1;
    }
    dimension_type[2] = harp_dimension_independent;
    if (harp_variable_conversion_add_source(conversion, "altitude_bounds", harp_type_double, HARP_UNIT_LENGTH, 3,
                                            dimension_type, 0) != 0)
    {
        return -1;
    }
    dimension_type[2] = harp_dimension_vertical;

    if (strcmp(species, "CH4") == 0 || strcmp(species, "CO") == 0 || strcmp(species, "CO2") == 0 ||
        strcmp(species, "H2O") == 0 || strcmp(species, "N2O") == 0 || strcmp(species, "NO2") == 0 ||
        strcmp(species, "O2") == 0 || strcmp(species, "O3") == 0)
    {
        if (add_aux_afgl86_conversion(name_nd, HARP_UNIT_NUMBER_DENSITY) != 0)
        {
            return -1;
        }
        if (add_aux_usstd76_conversion(name_nd, HARP_UNIT_NUMBER_DENSITY) != 0)
        {
            return -1;
        }
    }

    if (harp_variable_conversion_new(name_nd_uncertainty, harp_type_double, HARP_UNIT_NUMBER_DENSITY, 2, dimension_type,
                                     0, get_nd_from_vmr_pressure_and_temperature, &conversion) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, name_vmr_uncertainty, harp_type_double,
                                            HARP_UNIT_VOLUME_MIXING_RATIO, 2, dimension_type, 0) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, "pressure", harp_type_double, HARP_UNIT_PRESSURE, 2,
                                            dimension_type, 0) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, "temperature", harp_type_double, HARP_UNIT_TEMPERATURE, 2,
                                            dimension_type, 0) != 0)
    {
        return -1;
    }

    if (add_vertical_uncertainty_conversions(name_nd, HARP_UNIT_NUMBER_DENSITY, HARP_UNIT_NUMBER_DENSITY_SQUARED) != 0)
    {
        return -1;
    }

    /* mass density */
    dimension_type[0] = harp_dimension_time;
    dimension_type[1] = harp_dimension_vertical;
    dimension_type[2] = harp_dimension_vertical;
    for (i = 1; i < 3; i++)
    {
        if (harp_variable_conversion_new(name_density, harp_type_double, HARP_UNIT_MASS_DENSITY, i, dimension_type, 0,
                                         get_density_from_nd, &conversion) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, name_nd, harp_type_double, HARP_UNIT_NUMBER_DENSITY,
                                                i, dimension_type, 0) != 0)
        {
            return -1;
        }
    }

    /* mass mixing ratio */
    for (i = 1; i < 3; i++)
    {
        if (harp_variable_conversion_new(name_mmr, harp_type_double, HARP_UNIT_MASS_MIXING_RATIO, i, dimension_type, 0,
                                         get_mmr_from_vmr, &conversion) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, name_vmr, harp_type_double, HARP_UNIT_VOLUME_MIXING_RATIO,
                                                i, dimension_type, 0) != 0)
        {
            return -1;
        }
    }

    if (add_vertical_uncertainty_conversions(name_mmr, HARP_UNIT_MASS_MIXING_RATIO, HARP_UNIT_MASS_MIXING_RATIO_SQUARED)
        != 0)
    {
        return -1;
    }

    /* partial pressure */
    for (i = 1; i < 3; i++)
    {
        if (harp_variable_conversion_new(name_pp, harp_type_double, HARP_UNIT_PRESSURE, i, dimension_type, 0,
                                         get_partial_pressure_from_vmr_and_pressure, &conversion) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, name_vmr, harp_type_double, HARP_UNIT_VOLUME_MIXING_RATIO,
                                                i, dimension_type, 0) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, "pressure", harp_type_double, HARP_UNIT_PRESSURE, i,
                                                dimension_type, 0) != 0)
        {
            return -1;
        }
    }

    /* volume mixing ratio */
    for (i = 1; i < 3; i++)
    {
        if (harp_variable_conversion_new(name_vmr, harp_type_double, HARP_UNIT_VOLUME_MIXING_RATIO, i, dimension_type,
                                         0, get_vmr_from_nd_pressure_and_temperature, &conversion) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, name_nd, harp_type_double, HARP_UNIT_NUMBER_DENSITY, i,
                                                dimension_type, 0) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, "pressure", harp_type_double, HARP_UNIT_PRESSURE, i,
                                                dimension_type, 0) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, "temperature", harp_type_double, HARP_UNIT_TEMPERATURE, i,
                                                dimension_type, 0) != 0)
        {
            return -1;
        }
    }
    if (harp_variable_conversion_new(name_vmr_cov, harp_type_double, HARP_UNIT_VOLUME_MIXING_RATIO_SQUARED, 3,
                                     dimension_type, 0, get_vmr_cov_from_nd_cov_pressure_and_temperature, &conversion)
        != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, name_nd_cov, harp_type_double, HARP_UNIT_NUMBER_DENSITY_SQUARED,
                                            3, dimension_type, 0) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, "pressure", harp_type_double, HARP_UNIT_PRESSURE, 2,
                                            dimension_type, 0) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, "temperature", harp_type_double, HARP_UNIT_TEMPERATURE, 2,
                                            dimension_type, 0) != 0)
    {
        return -1;
    }
    for (i = 1; i < 3; i++)
    {
        if (harp_variable_conversion_new(name_vmr, harp_type_double, HARP_UNIT_VOLUME_MIXING_RATIO, i, dimension_type,
                                         0, get_vmr_from_mmr, &conversion) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, name_mmr, harp_type_double, HARP_UNIT_MASS_MIXING_RATIO, i,
                                                dimension_type, 0) != 0)
        {
            return -1;
        }
    }

    if (strcmp(species, "H2O") != 0)
    {
        for (i = 1; i < 3; i++)
        {
            if (harp_variable_conversion_new
                (name_vmr, harp_type_double, HARP_UNIT_VOLUME_MIXING_RATIO, i, dimension_type, 0,
                 get_vmr_from_mmrw_and_humidity, &conversion) != 0)
            {
                return -1;
            }
            if (harp_variable_conversion_add_source
                (conversion, name_mmrw, harp_type_double, HARP_UNIT_MASS_MIXING_RATIO, i, dimension_type, 0) != 0)
            {
                return -1;
            }
            if (harp_variable_conversion_add_source(conversion, "H2O_mass_mixing_ratio", harp_type_double,
                                                    HARP_UNIT_MASS_MIXING_RATIO, i, dimension_type, 0) != 0)
            {
                return -1;
            }
        }
    }

    for (i = 1; i < 3; i++)
    {
        if (harp_variable_conversion_new(name_vmr, harp_type_double, HARP_UNIT_VOLUME_MIXING_RATIO, i, dimension_type,
                                         0, get_vmr_from_partial_pressure_and_pressure, &conversion) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, name_pp, harp_type_double, HARP_UNIT_PRESSURE, i,
                                                dimension_type, 0) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, "pressure", harp_type_double, HARP_UNIT_PRESSURE, i,
                                                dimension_type, 0) != 0)
        {
            return -1;
        }
    }

    if (harp_variable_conversion_new(name_vmr_uncertainty, harp_type_double, HARP_UNIT_VOLUME_MIXING_RATIO, 2,
                                     dimension_type, 0, get_vmr_from_nd_pressure_and_temperature, &conversion) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, name_nd_uncertainty, harp_type_double, HARP_UNIT_NUMBER_DENSITY,
                                            2, dimension_type, 0) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, "pressure", harp_type_double, HARP_UNIT_PRESSURE, 2,
                                            dimension_type, 0) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, "temperature", harp_type_double, HARP_UNIT_TEMPERATURE, 2,
                                            dimension_type, 0) != 0)
    {
        return -1;
    }

    if (add_vertical_uncertainty_conversions(name_vmr, HARP_UNIT_VOLUME_MIXING_RATIO,
                                             HARP_UNIT_VOLUME_MIXING_RATIO_SQUARED) != 0)
    {
        return -1;
    }

    return 0;
}

static int init_conversions(void)
{
    harp_variable_conversion *conversion;
    harp_dimension_type dimension_type[HARP_MAX_NUM_DIMS];
    int i;

    /* Append conversions for variables that start with a species name */
    for (i = 0; i < harp_num_chemical_species; i++)
    {
        if (add_species_conversions(harp_chemical_species_name(i)) != 0)
        {
            return -1;
        }
    }

    dimension_type[0] = harp_dimension_time;
    dimension_type[1] = harp_dimension_vertical;
    dimension_type[2] = harp_dimension_vertical;

    /* aerosol extinction coefficient */
    if (harp_variable_conversion_new("aerosol_extinction_coefficient", harp_type_double, HARP_UNIT_AEROSOL_EXTINCTION,
                                     2, dimension_type, 0, get_density_from_partial_column_and_alt_bounds,
                                     &conversion) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, "aerosol_optical_depth", harp_type_double,
                                            HARP_UNIT_DIMENSIONLESS, 2, dimension_type, 0) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, "altitude_bounds", harp_type_double, HARP_UNIT_LENGTH, 3,
                                            dimension_type, 2) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_new("aerosol_extinction_coefficient_uncertainty", harp_type_double,
                                     HARP_UNIT_AEROSOL_EXTINCTION, 2, dimension_type, 0,
                                     get_density_from_partial_column_and_alt_bounds, &conversion) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, "aerosol_optical_depth_uncertainty", harp_type_double,
                                            HARP_UNIT_DIMENSIONLESS, 2, dimension_type, 0) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, "altitude_bounds", harp_type_double, HARP_UNIT_LENGTH, 3,
                                            dimension_type, 2) != 0)
    {
        return -1;
    }

    if (add_vertical_uncertainty_conversions("aerosol_extinction_coefficient", HARP_UNIT_AEROSOL_EXTINCTION,
                                             HARP_UNIT_AEROSOL_EXTINCTION_SQUARED) != 0)
    {
        return -1;
    }

    /* aerosol optical depth */
    if (harp_variable_conversion_new("aerosol_optical_depth", harp_type_double, HARP_UNIT_DIMENSIONLESS, 1,
                                     dimension_type, 0, get_column_from_partial_column, &conversion) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, "aerosol_optical_depth", harp_type_double,
                                            HARP_UNIT_DIMENSIONLESS, 2, dimension_type, 0) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_new("aerosol_optical_depth_uncertainty", harp_type_double, HARP_UNIT_DIMENSIONLESS, 1,
                                     dimension_type, 0, get_column_uncertainty_from_partial_column_uncertainty,
                                     &conversion) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, "aerosol_optical_depth_uncertainty", harp_type_double,
                                            HARP_UNIT_DIMENSIONLESS, 2, dimension_type, 0) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_new("aerosol_optical_depth", harp_type_double, HARP_UNIT_DIMENSIONLESS, 2,
                                     dimension_type, 0, get_partial_column_from_density_and_alt_bounds,
                                     &conversion) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, "aerosol_extinction_coefficient", harp_type_double,
                                            HARP_UNIT_AEROSOL_EXTINCTION, 2, dimension_type, 0) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, "altitude_bounds", harp_type_double, HARP_UNIT_LENGTH, 3,
                                            dimension_type, 2) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_new("aerosol_optical_depth_uncertainty", harp_type_double, HARP_UNIT_DIMENSIONLESS, 2,
                                     dimension_type, 0, get_partial_column_from_density_and_alt_bounds,
                                     &conversion) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, "aerosol_extinction_coefficient_uncertainty", harp_type_double,
                                            HARP_UNIT_AEROSOL_EXTINCTION, 2, dimension_type, 0) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, "altitude_bounds", harp_type_double, HARP_UNIT_LENGTH, 3,
                                            dimension_type, 2) != 0)
    {
        return -1;
    }

    if (add_vertical_uncertainty_conversions("aerosol_optical_depth", HARP_UNIT_DIMENSIONLESS, HARP_UNIT_DIMENSIONLESS)
        != 0)
    {
        return -1;
    }

    /* altitude */
    for (i = 1; i < 3; i++)
    {
        if (add_time_indepedent_to_dependent_conversion("altitude", harp_type_double, HARP_UNIT_LENGTH, i,
                                                        dimension_type, 0) != 0)
        {
            return -1;
        }
    }
    if (add_bounds_to_midpoint_conversion("altitude", harp_type_double, HARP_UNIT_LENGTH, harp_dimension_vertical,
                                          get_midpoint_from_bounds) != 0)
    {
        return -1;
    }
    for (i = 0; i < 3; i++)
    {
        if (harp_variable_conversion_new("altitude", harp_type_double, HARP_UNIT_LENGTH, i, dimension_type, 0,
                                         get_altitude_from_gph_and_latitude, &conversion) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, "geopotential_heigth", harp_type_double, HARP_UNIT_LENGTH,
                                                i, dimension_type, 0) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, "latitude", harp_type_double, HARP_UNIT_LATITUDE, i,
                                                dimension_type, 0) != 0)
        {
            return -1;
        }
    }

    for (i = 0; i < 2; i++)
    {
        if (harp_variable_conversion_new("altitude", harp_type_double, HARP_UNIT_LENGTH, i, dimension_type, 0, get_copy,
                                         &conversion) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, "instrument_altitude", harp_type_double, HARP_UNIT_LENGTH,
                                                i, dimension_type, 0) != 0)
        {
            return -1;
        }
    }

    /* altitude boundaries */
    if (add_midpoint_to_bounds_conversion("altitude", harp_type_double, HARP_UNIT_LENGTH, harp_dimension_vertical,
                                          get_alt_bounds_from_alt) != 0)
    {
        return -1;
    }

    /* datetime */
    if (harp_variable_conversion_new("datetime", harp_type_double, HARP_UNIT_DATETIME, 1, dimension_type, 0,
                                     get_datetime_from_datetime_start_and_stop, &conversion) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, "datetime_start", harp_type_double, HARP_UNIT_DATETIME, 1,
                                            dimension_type, 0) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, "datetime_stop", harp_type_double, HARP_UNIT_DATETIME, 1,
                                            dimension_type, 0) != 0)
    {
        return -1;
    }

    /* datetime_length */
    if (harp_variable_conversion_new("datetime_length", harp_type_double, HARP_UNIT_TIME, 1, dimension_type, 0,
                                     get_datetime_length_from_datetime_start_and_stop, &conversion) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, "datetime_start", harp_type_double, HARP_UNIT_DATETIME, 1,
                                            dimension_type, 0) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, "datetime_stop", harp_type_double, HARP_UNIT_DATETIME, 1,
                                            dimension_type, 0) != 0)
    {
        return -1;
    }

    /* datetime_start */
    if (harp_variable_conversion_new("datetime_start", harp_type_double, HARP_UNIT_DATETIME, 1, dimension_type, 0,
                                     get_datetime_start_from_datetime_and_length, &conversion) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, "datetime", harp_type_double, HARP_UNIT_DATETIME, 1,
                                            dimension_type, 0) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, "datetime_length", harp_type_double, HARP_UNIT_TIME, 1,
                                            dimension_type, 0) != 0)
    {
        return -1;
    }

    /* datetime_stop */
    if (harp_variable_conversion_new("datetime_stop", harp_type_double, HARP_UNIT_DATETIME, 1, dimension_type, 0,
                                     get_datetime_stop_from_datetime_start_and_length, &conversion) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, "datetime_start", harp_type_double, HARP_UNIT_DATETIME, 1,
                                            dimension_type, 0) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, "datetime_length", harp_type_double, HARP_UNIT_TIME, 1,
                                            dimension_type, 0) != 0)
    {
        return -1;
    }

    /* flag_am_pm */
    if (add_time_indepedent_to_dependent_conversion("flag_am_pm", harp_type_string, NULL, 1, dimension_type, 0) != 0)
    {
        return -1;
    }
    for (i = 0; i < 2; i++)
    {
        if (harp_variable_conversion_new("flag_am_pm", harp_type_string, NULL, i, dimension_type, 0,
                                         get_daytime_ampm_from_longitude, &conversion) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, "datetime", harp_type_double, HARP_UNIT_DATETIME, i,
                                                dimension_type, 0) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, "longitude", harp_type_double, HARP_UNIT_LONGITUDE, i,
                                                dimension_type, 0) != 0)
        {
            return -1;
        }
    }

    /* flag_day_twilight_night */
    if (add_time_indepedent_to_dependent_conversion("flag_day_twilight_night", harp_type_string, NULL, 1,
                                                    dimension_type, 0) != 0)
    {
        return -1;
    }
    for (i = 0; i < 2; i++)
    {
        if (harp_variable_conversion_new("flag_day_twilight_night", harp_type_string, NULL, i, dimension_type, 0,
                                         get_illumination_condition_from_solar_zenith_angle, &conversion) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, "solar_zenith_angle", harp_type_double, HARP_UNIT_ANGLE, i,
                                                dimension_type, 0) != 0)
        {
            return -1;
        }
    }

    /* flag_daytime */
    if (add_time_indepedent_to_dependent_conversion("flag_daytime", harp_type_string, NULL, 1, dimension_type, 0) != 0)
    {
        return -1;
    }
    for (i = 0; i < 2; i++)
    {
        if (harp_variable_conversion_new("flag_daytime", harp_type_int8, NULL, i, dimension_type, 0,
                                         get_daytime_from_solar_zenith_angle, &conversion) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, "solar_zenith_angle", harp_type_double, HARP_UNIT_ANGLE, i,
                                                dimension_type, 0) != 0)
        {
            return -1;
        }
    }

    /* frequency */
    dimension_type[1] = harp_dimension_spectral;
    for (i = 0; i < 3; i++)
    {
        if (i > 0)
        {
            if (add_time_indepedent_to_dependent_conversion("frequency", harp_type_double, HARP_UNIT_FREQUENCY, i,
                                                            dimension_type, 0) != 0)
            {
                return -1;
            }
        }
        if (harp_variable_conversion_new("frequency", harp_type_double, HARP_UNIT_FREQUENCY, i, dimension_type, 0,
                                         get_frequency_from_wavelength, &conversion) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, "wavelength", harp_type_double, HARP_UNIT_WAVELENGTH, i,
                                                dimension_type, 0) != 0)
        {
            return -1;
        }

        if (harp_variable_conversion_new("frequency", harp_type_double, HARP_UNIT_FREQUENCY, i, dimension_type, 0,
                                         get_frequency_from_wavenumber, &conversion) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, "wavenumber", harp_type_double, HARP_UNIT_WAVENUMBER, i,
                                                dimension_type, 0) != 0)
        {
            return -1;
        }
    }

    /* geopotential */
    dimension_type[1] = harp_dimension_vertical;
    for (i = 0; i < 3; i++)
    {
        if (i > 0)
        {
            if (add_time_indepedent_to_dependent_conversion("geopotential", harp_type_double, HARP_UNIT_GEOPOTENTIAL,
                                                            i, dimension_type, 0) != 0)
            {
                return -1;
            }
        }
        if (harp_variable_conversion_new("geopotential", harp_type_double, HARP_UNIT_GEOPOTENTIAL, i, dimension_type,
                                         0, get_geopotential_from_gph, &conversion) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, "geopotential_height", harp_type_double, HARP_UNIT_LENGTH,
                                                i, dimension_type, 0) != 0)
        {
            return -1;
        }
    }

    /* geopotential height */
    dimension_type[1] = harp_dimension_vertical;
    for (i = 0; i < 3; i++)
    {
        if (i > 0)
        {
            if (add_time_indepedent_to_dependent_conversion("geopotential_height", harp_type_double, HARP_UNIT_LENGTH,
                                                            i, dimension_type, 0) != 0)
            {
                return -1;
            }
        }
        if (harp_variable_conversion_new("geopotential_height", harp_type_double, HARP_UNIT_LENGTH, i, dimension_type,
                                         0, get_gph_from_geopotential, &conversion) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, "geopotential", harp_type_double, HARP_UNIT_GEOPOTENTIAL, i,
                                                dimension_type, 0) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_new("geopotential_height", harp_type_double, HARP_UNIT_LENGTH, i, dimension_type,
                                         0, get_gph_from_altitude_and_latitude, &conversion) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, "altitude", harp_type_double, HARP_UNIT_LENGTH, i,
                                                dimension_type, 0) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, "latitude", harp_type_double, HARP_UNIT_LATITUDE, i,
                                                dimension_type, 0) != 0)
        {
            return -1;
        }

    }

    /* index */
    if (harp_variable_conversion_new("index", harp_type_int32, NULL, 1, dimension_type, 0, get_index, &conversion) != 0)
    {
        return -1;
    }

    /* latitude */
    if (add_time_indepedent_to_dependent_conversion("latitude", harp_type_double, HARP_UNIT_LATITUDE, 1,
                                                    dimension_type, 0) != 0)
    {
        return -1;
    }
    if (add_latlon_bounds_to_midpoint_conversion("latitude", harp_type_double, HARP_UNIT_LATITUDE,
                                                 get_latitude_from_latlon_bounds) != 0)
    {
        return -1;
    }
    for (i = 0; i < 2; i++)
    {
        if (harp_variable_conversion_new("latitude", harp_type_double, HARP_UNIT_LATITUDE, i, dimension_type, 0,
                                         get_copy, &conversion) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, "instrument_latitude", harp_type_double,
                                                HARP_UNIT_LATITUDE, i, dimension_type, 0) != 0)
        {
            return -1;
        }
    }
    if (add_latlon_midpoints_to_bounds_conversion("latitude", harp_type_double, HARP_UNIT_LATITUDE,
                                                  harp_dimension_latitude, get_bounds_from_midpoints) != 0)
    {
        return -1;
    }

    /* longitude */
    if (add_time_indepedent_to_dependent_conversion("longitude", harp_type_double, HARP_UNIT_LONGITUDE, 1,
                                                    dimension_type, 0) != 0)
    {
        return -1;
    }
    if (add_latlon_bounds_to_midpoint_conversion("longitude", harp_type_double, HARP_UNIT_LONGITUDE,
                                                 get_longitude_from_latlon_bounds) != 0)
    {
        return -1;
    }
    for (i = 0; i < 2; i++)
    {
        if (harp_variable_conversion_new("longitude", harp_type_double, HARP_UNIT_LONGITUDE, i, dimension_type, 0,
                                         get_copy, &conversion) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, "instrument_longitude", harp_type_double,
                                                HARP_UNIT_LONGITUDE, i, dimension_type, 0) != 0)
        {
            return -1;
        }
    }
    if (add_latlon_midpoints_to_bounds_conversion("longitude", harp_type_double, HARP_UNIT_LONGITUDE,
                                                  harp_dimension_longitude, get_bounds_from_midpoints) != 0)
    {
        return -1;
    }

    /* normalized radiance */
    dimension_type[1] = harp_dimension_spectral;
    for (i = 1; i < 3; i++)
    {
        if (harp_variable_conversion_new("normalized_radiance", harp_type_double, HARP_UNIT_DIMENSIONLESS, i,
                                         dimension_type, 0, get_normalized_radiance_from_radiance_and_solar_irradiance,
                                         &conversion) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, "radiance", harp_type_double, HARP_UNIT_RADIANCE, i,
                                                dimension_type, 0) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, "solar_irradiance", harp_type_double, HARP_UNIT_IRRADIANCE,
                                                i, dimension_type, 0) != 0)
        {
            return -1;
        }

        if (harp_variable_conversion_new("normalized_radiance", harp_type_double, HARP_UNIT_DIMENSIONLESS, i,
                                         dimension_type, 0,
                                         get_normalized_radiance_from_reflectance_and_solar_zenith_angle, &conversion)
            != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, "reflectance", harp_type_double, HARP_UNIT_DIMENSIONLESS, i,
                                                dimension_type, 0) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, "solar_zenith_angle", harp_type_double, HARP_UNIT_ANGLE, 1,
                                                dimension_type, 0) != 0)
        {
            return -1;
        }
    }

    if (add_spectral_uncertainty_conversions("normalized_radiance", HARP_UNIT_DIMENSIONLESS) != 0)
    {
        return -1;
    }

    /* number density */
    dimension_type[1] = harp_dimension_vertical;
    for (i = 1; i < 3; i++)
    {
        if (harp_variable_conversion_new("number_density", harp_type_double, HARP_UNIT_NUMBER_DENSITY, i,
                                         dimension_type, 0, get_air_nd_from_pressure_and_temperature, &conversion) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, "pressure", harp_type_double, HARP_UNIT_PRESSURE, i,
                                                dimension_type, 0) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, "temperature", harp_type_double, HARP_UNIT_TEMPERATURE, i,
                                                dimension_type, 0) != 0)
        {
            return -1;
        }
    }

    if (add_aux_afgl86_conversion("number_density", HARP_UNIT_NUMBER_DENSITY) != 0)
    {
        return -1;
    }
    if (add_aux_usstd76_conversion("number_density", HARP_UNIT_NUMBER_DENSITY) != 0)
    {
        return -1;
    }

    if (add_uncertainty_conversions("number_density", HARP_UNIT_NUMBER_DENSITY) != 0)
    {
        return -1;
    }

    /* pressure */
    for (i = 1; i < 3; i++)
    {
        if (add_time_indepedent_to_dependent_conversion("pressure", harp_type_double, HARP_UNIT_PRESSURE, i,
                                                        dimension_type, 0) != 0)
        {
            return -1;
        }
    }

    if (add_bounds_to_midpoint_conversion("pressure", harp_type_double, HARP_UNIT_PRESSURE, harp_dimension_vertical,
                                          get_midpoint_from_bounds_log) != 0)
    {
        return -1;
    }

    if (harp_variable_conversion_new("pressure", harp_type_double, HARP_UNIT_PRESSURE, 2, dimension_type, 0,
                                     get_pressure_from_altitude_temperature_h2o_mmr_and_latitude, &conversion) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, "altitude", harp_type_double, HARP_UNIT_LENGTH, 2,
                                            dimension_type, 0) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, "temperature", harp_type_double, HARP_UNIT_TEMPERATURE, 2,
                                            dimension_type, 0) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, "H2O_mass_mixing_ratio", harp_type_double,
                                            HARP_UNIT_MASS_MIXING_RATIO, 2, dimension_type, 0) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, "latitude", harp_type_double, HARP_UNIT_LATITUDE, 1,
                                            dimension_type, 0) != 0)
    {
        return -1;
    }

    if (harp_variable_conversion_new("pressure", harp_type_double, HARP_UNIT_PRESSURE, 2, dimension_type, 0,
                                     get_pressure_from_gph_temperature_and_h2o_mmr, &conversion) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, "geopotential_height", harp_type_double, HARP_UNIT_LENGTH, 2,
                                            dimension_type, 0) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, "temperature", harp_type_double, HARP_UNIT_TEMPERATURE, 2,
                                            dimension_type, 0) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, "H2O_mass_mixing_ratio", harp_type_double,
                                            HARP_UNIT_MASS_MIXING_RATIO, 2, dimension_type, 0) != 0)
    {
        return -1;
    }

    if (harp_variable_conversion_new("pressure", harp_type_double, HARP_UNIT_PRESSURE, 2, dimension_type, 0,
                                     get_pressure_from_altitude_temperature_and_latitude, &conversion) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, "pressure", harp_type_double, HARP_UNIT_PRESSURE, 2,
                                            dimension_type, 0) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, "temperature", harp_type_double, HARP_UNIT_TEMPERATURE, 2,
                                            dimension_type, 0) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, "latitude", harp_type_double, HARP_UNIT_LATITUDE, 1,
                                            dimension_type, 0) != 0)
    {
        return -1;
    }

    if (harp_variable_conversion_new("pressure", harp_type_double, HARP_UNIT_PRESSURE, 2, dimension_type, 0,
                                     get_pressure_from_gph_and_temperature, &conversion) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, "geopotential_height", harp_type_double, HARP_UNIT_LENGTH, 2,
                                            dimension_type, 0) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, "temperature", harp_type_double, HARP_UNIT_TEMPERATURE, 2,
                                            dimension_type, 0) != 0)
    {
        return -1;
    }


    if (add_aux_afgl86_conversion("pressure", HARP_UNIT_PRESSURE) != 0)
    {
        return -1;
    }
    if (add_aux_usstd76_conversion("pressure", HARP_UNIT_PRESSURE) != 0)
    {
        return -1;
    }

    if (add_vertical_uncertainty_conversions("pressure", HARP_UNIT_PRESSURE, NULL) != 0)
    {
        return -1;
    }

    /* radiance */
    dimension_type[1] = harp_dimension_spectral;
    for (i = 1; i < 3; i++)
    {
        if (harp_variable_conversion_new("radiance", harp_type_double, HARP_UNIT_RADIANCE, i, dimension_type, 0,
                                         get_radiance_from_normalized_radiance_and_solar_irradiance, &conversion) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, "normalized_radiance", harp_type_double,
                                                HARP_UNIT_DIMENSIONLESS, i, dimension_type, 0) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, "solar_irradiance", harp_type_double, HARP_UNIT_IRRADIANCE,
                                                i, dimension_type, 0) != 0)
        {
            return -1;
        }
    }

    dimension_type[1] = harp_dimension_spectral;
    if (add_uncertainty_conversions("radiance", HARP_UNIT_RADIANCE) != 0)
    {
        return -1;
    }

    /* reflectance */
    dimension_type[1] = harp_dimension_spectral;
    for (i = 1; i < 3; i++)
    {
        if (harp_variable_conversion_new("reflectance", harp_type_double, HARP_UNIT_DIMENSIONLESS, i, dimension_type, 0,
                                         get_reflectance_from_normalized_radiance_and_solar_zenith_angle, &conversion)
            != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, "normalized_radiance", harp_type_double,
                                                HARP_UNIT_DIMENSIONLESS, i, dimension_type, 0) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, "solar_zenith_angle", harp_type_double, HARP_UNIT_ANGLE, 1,
                                                dimension_type, 0) != 0)
        {
            return -1;
        }
    }

    if (add_uncertainty_conversions("reflectance", HARP_UNIT_DIMENSIONLESS) != 0)
    {
        return -1;
    }

    /* relative humidity */
    dimension_type[1] = harp_dimension_vertical;
    for (i = 1; i < 3; i++)
    {
        if (harp_variable_conversion_new("relative_humidity", harp_type_double, HARP_UNIT_DIMENSIONLESS, i,
                                         dimension_type, 0, get_relative_humidity_from_h2o_nd_and_temperature,
                                         &conversion) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, "H2O_number_density", harp_type_double,
                                                HARP_UNIT_NUMBER_DENSITY, i, dimension_type, 0) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, "temperature", harp_type_double, HARP_UNIT_TEMPERATURE, i,
                                                dimension_type, 0) != 0)
        {
            return -1;
        }
    }

    if (add_uncertainty_conversions("relative_humidity", HARP_UNIT_DIMENSIONLESS) != 0)
    {
        return -1;
    }

    /* scattering angle */
    for (i = 0; i < 2; i++)
    {
        if (harp_variable_conversion_new("scattering_angle", harp_type_double, HARP_UNIT_ANGLE, i, dimension_type, 0,
                                         get_scattering_angle_from_solar_angles_and_viewing_angles, &conversion) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, "solar_zenith_angle", harp_type_double, HARP_UNIT_ANGLE, i,
                                                dimension_type, 0) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, "solar_azimuth_angle", harp_type_double, HARP_UNIT_ANGLE, i,
                                                dimension_type, 0) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, "viewing_zenith_angle", harp_type_double, HARP_UNIT_ANGLE,
                                                i, dimension_type, 0) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, "viewing_azimuth_angle", harp_type_double, HARP_UNIT_ANGLE,
                                                i, dimension_type, 0) != 0)
        {
            return -1;
        }
    }

    /* solar elevation angle */
    if (add_time_indepedent_to_dependent_conversion("solar_elevation_angle", harp_type_double, HARP_UNIT_ANGLE, 1,
                                                    dimension_type, 0) != 0)
    {
        return -1;
    }
    for (i = 0; i < 2; i++)
    {
        if (harp_variable_conversion_new("solar_elevation_angle", harp_type_double, HARP_UNIT_ANGLE, i, dimension_type,
                                         0, get_elevation_angle_from_zenith_angle, &conversion) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, "solar_zenith_angle", harp_type_double, HARP_UNIT_ANGLE, i,
                                                dimension_type, 0) != 0)
        {
            return -1;
        }
    }
    for (i = 0; i < 2; i++)
    {
        if (harp_variable_conversion_new("solar_elevation_angle", harp_type_double, HARP_UNIT_ANGLE, i, dimension_type,
                                         0, get_solar_elevation_angle_from_datetime_and_latlon, &conversion) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, "datetime", harp_type_double, HARP_UNIT_DATETIME, i,
                                                dimension_type, 0) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, "latitude", harp_type_double, HARP_UNIT_LATITUDE, i,
                                                dimension_type, 0) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, "longitude", harp_type_double, HARP_UNIT_LONGITUDE, i,
                                                dimension_type, 0) != 0)
        {
            return -1;
        }
    }

    /* solar irradiance */
    dimension_type[1] = harp_dimension_spectral;
    if (add_uncertainty_conversions("solar_irradiance", HARP_UNIT_IRRADIANCE) != 0)
    {
        return -1;
    }


    /* solar zenith angle */
    if (add_time_indepedent_to_dependent_conversion("solar_zenith_angle", harp_type_double, HARP_UNIT_ANGLE, 1,
                                                    dimension_type, 0) != 0)
    {
        return -1;
    }
    for (i = 0; i < 2; i++)
    {
        if (harp_variable_conversion_new("solar_zenith_angle", harp_type_double, HARP_UNIT_ANGLE, i, dimension_type,
                                         0, get_zenith_angle_from_elevation_angle, &conversion) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, "solar_elevation_angle", harp_type_double, HARP_UNIT_ANGLE,
                                                i, dimension_type, 0) != 0)
        {
            return -1;
        }
    }

    /* temperature */
    dimension_type[1] = harp_dimension_vertical;
    for (i = 1; i < 3; i++)
    {
        if (add_time_indepedent_to_dependent_conversion("temperature", harp_type_double, HARP_UNIT_TEMPERATURE, i,
                                                        dimension_type, 0) != 0)
        {
            return -1;
        }
    }
    if (add_aux_afgl86_conversion("temperature", HARP_UNIT_TEMPERATURE) != 0)
    {
        return -1;
    }
    if (add_aux_usstd76_conversion("temperature", HARP_UNIT_TEMPERATURE) != 0)
    {
        return -1;
    }

    if (add_vertical_uncertainty_conversions("temperature", HARP_UNIT_TEMPERATURE, NULL) != 0)
    {
        return -1;
    }

    /* viewing azimuth angle */
    if (add_time_indepedent_to_dependent_conversion("viewing_azimuth_angle", harp_type_double, HARP_UNIT_ANGLE, 1,
                                                    dimension_type, 0) != 0)
    {
        return -1;
    }

    /* viewing zenith angle */
    if (add_time_indepedent_to_dependent_conversion("viewing_zenith_angle", harp_type_double, HARP_UNIT_ANGLE, 1,
                                                    dimension_type, 0) != 0)
    {
        return -1;
    }

    /* virtual temperature */
    for (i = 1; i < 3; i++)
    {
        if (add_time_indepedent_to_dependent_conversion("virtual_temperature", harp_type_double, HARP_UNIT_TEMPERATURE,
                                                        i, dimension_type, 0) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_new("virtual_temperature", harp_type_double, HARP_UNIT_TEMPERATURE, i,
                                         dimension_type, 0,
                                         get_virtual_temperature_from_pressure_temperature_and_relative_humidity,
                                         &conversion) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, "pressure", harp_type_double, HARP_UNIT_PRESSURE, i,
                                                dimension_type, 0) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, "temperature", harp_type_double, HARP_UNIT_TEMPERATURE, i,
                                                dimension_type, 0) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, "relative_humidity", harp_type_double,
                                                HARP_UNIT_DIMENSIONLESS, i, dimension_type, 0) != 0)
        {
            return -1;
        }
    }

    /* wavelength */
    dimension_type[1] = harp_dimension_spectral;
    for (i = 0; i < 3; i++)
    {
        if (i > 0)
        {
            if (add_time_indepedent_to_dependent_conversion("wavelength", harp_type_double, HARP_UNIT_WAVELENGTH, i,
                                                            dimension_type, 0) != 0)
            {
                return -1;
            }
        }
        if (harp_variable_conversion_new("wavelength", harp_type_double, HARP_UNIT_WAVELENGTH, i, dimension_type, 0,
                                         get_wavelength_from_frequency, &conversion) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, "frequency", harp_type_double, HARP_UNIT_FREQUENCY, i,
                                                dimension_type, 0) != 0)
        {
            return -1;
        }

        if (harp_variable_conversion_new("wavelength", harp_type_double, HARP_UNIT_WAVELENGTH, i, dimension_type, 0,
                                         get_wavelength_from_wavenumber, &conversion) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, "wavenumber", harp_type_double, HARP_UNIT_WAVENUMBER, i,
                                                dimension_type, 0) != 0)
        {
            return -1;
        }
    }

    /* wavenumber */
    for (i = 0; i < 3; i++)
    {
        if (i > 0)
        {
            if (add_time_indepedent_to_dependent_conversion("wavenumber", harp_type_double, HARP_UNIT_WAVENUMBER, i,
                                                            dimension_type, 0) != 0)
            {
                return -1;
            }
        }
        if (harp_variable_conversion_new("wavenumber", harp_type_double, HARP_UNIT_WAVENUMBER, i, dimension_type, 0,
                                         get_wavenumber_from_frequency, &conversion) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, "frequency", harp_type_double, HARP_UNIT_FREQUENCY, i,
                                                dimension_type, 0) != 0)
        {
            return -1;
        }

        if (harp_variable_conversion_new("wavenumber", harp_type_double, HARP_UNIT_WAVENUMBER, i, dimension_type, 0,
                                         get_wavenumber_from_wavelength, &conversion) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, "wavelength", harp_type_double, HARP_UNIT_WAVELENGTH, i,
                                                dimension_type, 0) != 0)
        {
            return -1;
        }
    }

    return 0;
}

int harp_derived_variable_list_init(void)
{
    assert(harp_derived_variable_conversions == NULL);
    harp_derived_variable_conversions = malloc(sizeof(harp_derived_variable_list));
    if (harp_derived_variable_conversions == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate % lu bytes) (%s:%u)",
                       sizeof(harp_derived_variable_list), __FILE__, __LINE__);
        return -1;
    }
    harp_derived_variable_conversions->num_variables = 0;
    harp_derived_variable_conversions->hash_data = NULL;
    harp_derived_variable_conversions->conversions_for_variable = NULL;
    harp_derived_variable_conversions->hash_data = hashtable_new(1);
    if (harp_derived_variable_conversions->hash_data == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not create hashtable) (%s:%u)", __FILE__,
                       __LINE__);
        harp_derived_variable_list_done();
        return -1;
    }

    if (init_conversions() != 0)
    {
        harp_derived_variable_list_done();
        return -1;
    }

    return 0;
}

int harp_derived_variable_list_add_conversion(harp_variable_conversion *conversion)
{
    harp_variable_conversion_list *conversion_list;
    int index;
    int i;

    index = hashtable_get_index_from_name(harp_derived_variable_conversions->hash_data, conversion->variable_name);
    if (index < 0)
    {
        /* no conversions for this variable name exists -> create new conversion list */
        if (harp_derived_variable_conversions->num_variables % BLOCK_SIZE == 0)
        {
            harp_variable_conversion_list **new_list;

            new_list = realloc(harp_derived_variable_conversions->conversions_for_variable,
                               (harp_derived_variable_conversions->num_variables + BLOCK_SIZE) *
                               sizeof(harp_variable_conversion_list *));
            if (new_list == NULL)
            {
                harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory(could not allocate % lu bytes) (%s:%u)",
                               (harp_derived_variable_conversions->num_variables + BLOCK_SIZE) *
                               sizeof(harp_variable_conversion_list *), __FILE__, __LINE__);
                return -1;
            }
            harp_derived_variable_conversions->conversions_for_variable = new_list;
        }

        conversion_list = malloc(sizeof(harp_variable_conversion_list));
        if (conversion_list == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate % lu bytes) (%s:%u)",
                           sizeof(harp_variable_conversion_list), __FILE__, __LINE__);
            return -1;
        }
        conversion_list->num_conversions = 0;
        conversion_list->conversion = NULL;

        hashtable_add_name(harp_derived_variable_conversions->hash_data, conversion->variable_name);

        harp_derived_variable_conversions->num_variables++;
        harp_derived_variable_conversions->conversions_for_variable[harp_derived_variable_conversions->num_variables -
                                                                    1] = conversion_list;
    }
    else
    {
        conversion_list = harp_derived_variable_conversions->conversions_for_variable[index];
    }

    if (conversion_list->num_conversions % BLOCK_SIZE == 0)
    {
        harp_variable_conversion **new_conversion;

        new_conversion = realloc(conversion_list->conversion,
                                 (conversion_list->num_conversions + BLOCK_SIZE) * sizeof(harp_variable_conversion *));
        if (new_conversion == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory(could not allocate % lu bytes) (%s:%u)",
                           (conversion_list->num_conversions + BLOCK_SIZE) * sizeof(harp_variable_conversion *),
                           __FILE__, __LINE__);
            return -1;
        }
        conversion_list->conversion = new_conversion;
    }

    for (index = 0; index < conversion_list->num_conversions; index++)
    {
        if (conversion_list->conversion[index]->num_dimensions > conversion->num_dimensions)
        {
            break;
        }
        if (conversion_list->conversion[index]->num_dimensions == conversion->num_dimensions)
        {
            for (i = 0; i < conversion->num_dimensions; i++)
            {
                if (conversion_list->conversion[index]->dimension_type[i] > conversion->dimension_type[i])
                {
                    break;
                }
            }
            if (i != conversion->num_dimensions)
            {
                break;
            }
        }
    }
    for (i = conversion_list->num_conversions; i > index; i--)
    {
        conversion_list->conversion[i] = conversion_list->conversion[i - 1];
    }
    conversion_list->conversion[index] = conversion;
    conversion_list->num_conversions++;

    return 0;
}

void harp_derived_variable_list_done(void)
{
    if (harp_derived_variable_conversions != NULL)
    {
        if (harp_derived_variable_conversions->hash_data != NULL)
        {
            hashtable_delete(harp_derived_variable_conversions->hash_data);
        }
        if (harp_derived_variable_conversions->conversions_for_variable != NULL)
        {
            int i;

            if (harp_derived_variable_conversions->num_variables > 0)
            {
                for (i = 0; i < harp_derived_variable_conversions->num_variables; i++)
                {
                    harp_variable_conversion_list *conversion_list;

                    conversion_list = harp_derived_variable_conversions->conversions_for_variable[i];
                    if (conversion_list->conversion != NULL)
                    {
                        int j;

                        if (conversion_list->num_conversions > 0)
                        {
                            for (j = 0; j < conversion_list->num_conversions; j++)
                            {
                                harp_variable_conversion_delete(conversion_list->conversion[j]);
                            }
                        }
                        free(conversion_list->conversion);
                    }
                    free(conversion_list);
                }
            }
            free(harp_derived_variable_conversions->conversions_for_variable);
        }
        free(harp_derived_variable_conversions);
        harp_derived_variable_conversions = NULL;
    }
}
