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

static int get_begin_from_midpoint_and_length(harp_variable *variable, const harp_variable **source_variable)
{
    long i;

    for (i = 0; i < variable->num_elements; i++)
    {
        variable->data.double_data[i] =
            source_variable[0]->data.double_data[i] - source_variable[1]->data.double_data[i] / 2;
    }

    return 0;
}

static int get_bounds_from_midpoints(harp_variable *variable, const harp_variable **source_variable)
{
    long length = source_variable[0]->dimension[source_variable[0]->num_dimensions - 1];
    long num_blocks = source_variable[0]->num_elements / length;
    long i;

    for (i = 0; i < num_blocks; i++)
    {
        harp_bounds_from_midpoints_linear(length, &source_variable[0]->data.double_data[i * length],
                                          &variable->data.double_data[i * length * 2]);
    }

    return 0;
}

static int get_bounds_from_midpoints_log(harp_variable *variable, const harp_variable **source_variable)
{
    long length = source_variable[0]->dimension[source_variable[0]->num_dimensions - 1];
    long num_blocks = source_variable[0]->num_elements / length;
    long i;

    for (i = 0; i < num_blocks; i++)
    {
        harp_bounds_from_midpoints_loglinear(length, &source_variable[0]->data.double_data[i * length],
                                             &variable->data.double_data[i * length * 2]);
    }

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

static int get_copy(harp_variable *variable, const harp_variable **source_variable)
{
    assert(variable->data_type != harp_type_string);

    memcpy(variable->data.ptr, source_variable[0]->data.ptr,
           (size_t)variable->num_elements * harp_get_size_for_type(variable->data_type));

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

static int get_end_from_begin_and_length(harp_variable *variable, const harp_variable **source_variable)
{
    long i;

    for (i = 0; i < variable->num_elements; i++)
    {
        variable->data.double_data[i] =
            source_variable[0]->data.double_data[i] + source_variable[1]->data.double_data[i];
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
        double latitude, longitude;

        if (harp_geographic_center_from_bounds(num_vertices, &source_variable[1]->data.double_data[i * num_vertices],
                                               &source_variable[0]->data.double_data[i * num_vertices], &longitude,
                                               &latitude) != 0)
        {
            return -1;
        }

        variable->data.double_data[i] = latitude;
    }

    return 0;
}

static int get_length_from_begin_and_end(harp_variable *variable, const harp_variable **source_variable)
{
    long i;

    for (i = 0; i < variable->num_elements; i++)
    {
        variable->data.double_data[i] =
            source_variable[1]->data.double_data[i] - source_variable[0]->data.double_data[i];
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
        double latitude, longitude;

        if (harp_geographic_center_from_bounds(num_vertices, &source_variable[1]->data.double_data[i * num_vertices],
                                               &source_variable[0]->data.double_data[i * num_vertices], &longitude,
                                               &latitude) != 0)
        {
            return -1;
        }

        variable->data.double_data[i] = longitude;
    }

    return 0;
}

static int get_midpoint_from_begin_and_end(harp_variable *variable, const harp_variable **source_variable)
{
    long i;

    for (i = 0; i < variable->num_elements; i++)
    {
        variable->data.double_data[i] = (source_variable[0]->data.double_data[i] +
                                         source_variable[1]->data.double_data[i]) / 2;
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

        assert(variable->num_dimensions == 2);
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

        assert(variable->num_dimensions == 2);
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

#if 0
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
#endif

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

static int add_time_indepedent_to_dependent_conversion(const char *variable_name, harp_data_type data_type,
                                                       const char *unit, int num_dimensions,
                                                       harp_dimension_type dimension_type[HARP_MAX_NUM_DIMS],
                                                       long independent_dimension_length)
{
    harp_variable_conversion *conversion;

    /* if the target dimension is not time dependent then don't add a conversion */
    if (num_dimensions == 0 || dimension_type[0] != harp_dimension_time)
    {
        return 0;
    }

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

static int add_model_conversions()
{
    const char *items[] = {
        "number_density", "CH4_number_density", "CO_number_density", "CO2_number_density", "H2O_number_density",
        "N2O_number_density", "NO2_number_density", "O2_number_density", "O3_number_density"
    };
    int i;

    if (add_aux_afgl86_conversion("pressure", HARP_UNIT_PRESSURE) != 0)
    {
        return -1;
    }
    if (add_aux_usstd76_conversion("pressure", HARP_UNIT_PRESSURE) != 0)
    {
        return -1;
    }
    if (add_aux_afgl86_conversion("temperature", HARP_UNIT_TEMPERATURE) != 0)
    {
        return -1;
    }
    if (add_aux_usstd76_conversion("temperature", HARP_UNIT_TEMPERATURE) != 0)
    {
        return -1;
    }
    for (i = 0; i < 9; i++)
    {
        if (add_aux_afgl86_conversion(items[i], HARP_UNIT_NUMBER_DENSITY) != 0)
        {
            return -1;
        }
        if (add_aux_usstd76_conversion(items[i], HARP_UNIT_NUMBER_DENSITY) != 0)
        {
            return -1;
        }
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

    /* scalar (time independent and axis independent) */
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

    /* time independent and axis dependent */
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

    /* time dependent and axis independent */
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

    /* time dependent and axis dependent */
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

    /* time independent */
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

    /* time dependent */
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

    /* time independent */
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

    /* time dependent */
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

static int add_uncertainty_conversions(const char *variable_name, const char *unit, int num_dimensions,
                                       harp_dimension_type dimension_type[HARP_MAX_NUM_DIMS])
{
    harp_variable_conversion *conversion;
    char name_uncertainty[MAX_NAME_LENGTH];
    char name_uncertainty_sys[MAX_NAME_LENGTH];
    char name_uncertainty_rnd[MAX_NAME_LENGTH];

    snprintf(name_uncertainty, MAX_NAME_LENGTH, "%s_uncertainty", variable_name);
    snprintf(name_uncertainty_sys, MAX_NAME_LENGTH, "%s_uncertainty_systematic", variable_name);
    snprintf(name_uncertainty_rnd, MAX_NAME_LENGTH, "%s_uncertainty_random", variable_name);

    /* time dependent from independent */
    if (add_time_indepedent_to_dependent_conversion(name_uncertainty, harp_type_double, unit, num_dimensions,
                                                    dimension_type, 0) != 0)
    {
        return -1;
    }
    if (add_time_indepedent_to_dependent_conversion(name_uncertainty_sys, harp_type_double, unit, num_dimensions,
                                                    dimension_type, 0) != 0)
    {
        return -1;
    }
    if (add_time_indepedent_to_dependent_conversion(name_uncertainty_rnd, harp_type_double, unit, num_dimensions,
                                                    dimension_type, 0) != 0)
    {
        return -1;
    }


    /* total uncertainty from systematic and random */
    if (harp_variable_conversion_new(name_uncertainty, harp_type_double, unit, num_dimensions, dimension_type, 0,
                                     get_uncertainty_from_systematic_and_random_uncertainty, &conversion) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, name_uncertainty_sys, harp_type_double, unit, num_dimensions,
                                            dimension_type, 0) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, name_uncertainty_rnd, harp_type_double, unit, num_dimensions,
                                            dimension_type, 0) != 0)
    {
        return -1;
    }

    /*  if the last dimension is the vertical dimension add covariance related conversions */
    if (num_dimensions > 0 && dimension_type[num_dimensions - 1] == harp_dimension_vertical)
    {
        harp_dimension_type covar_dimension_type[HARP_MAX_NUM_DIMS];
        char name_covariance[MAX_NAME_LENGTH];
        char unit_squared[MAX_NAME_LENGTH];
        int i;

        assert(unit != NULL);

        snprintf(name_covariance, MAX_NAME_LENGTH, "%s_covariance", variable_name);
        if (unit[0] == '\0')
        {
            unit_squared[0] = '\0';
        }
        else
        {
            snprintf(unit_squared, MAX_NAME_LENGTH, "(%s)2", unit);
        }

        for (i = 0; i < num_dimensions; i++)
        {
            covar_dimension_type[i] = dimension_type[i];
        }
        covar_dimension_type[num_dimensions] = covar_dimension_type[num_dimensions - 1];

        /* time dependent from independent */
        if (add_time_indepedent_to_dependent_conversion(name_covariance, harp_type_double, unit_squared,
                                                        num_dimensions + 1, covar_dimension_type, 0) != 0)
        {
            return -1;
        }

        /* total uncertainty from covariance matrix trace */
        if (harp_variable_conversion_new(name_uncertainty, harp_type_double, unit, num_dimensions, dimension_type, 0,
                                         get_sqrt_trace_from_matrix, &conversion) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, name_covariance, harp_type_double, unit_squared,
                                                num_dimensions + 1, covar_dimension_type, 0) != 0)
        {
            return -1;
        }
    }

    return 0;
}

static int add_species_conversions_for_grid(const char *species, int num_dimensions,
                                            harp_dimension_type target_dimension_type[HARP_MAX_NUM_DIMS])
{
    harp_variable_conversion *conversion;
    harp_dimension_type dimension_type[HARP_MAX_NUM_DIMS];
    char name_column_density[MAX_NAME_LENGTH];
    char name_strato_column_density[MAX_NAME_LENGTH];
    char name_tropo_column_density[MAX_NAME_LENGTH];
    char name_column_nd[MAX_NAME_LENGTH];
    char name_strato_column_nd[MAX_NAME_LENGTH];
    char name_tropo_column_nd[MAX_NAME_LENGTH];
    char name_column_mmr[MAX_NAME_LENGTH];
    char name_column_mmr_dry[MAX_NAME_LENGTH];
    char name_strato_column_mmr[MAX_NAME_LENGTH];
    char name_strato_column_mmr_dry[MAX_NAME_LENGTH];
    char name_tropo_column_mmr[MAX_NAME_LENGTH];
    char name_tropo_column_mmr_dry[MAX_NAME_LENGTH];
    char name_column_vmr[MAX_NAME_LENGTH];
    char name_column_vmr_dry[MAX_NAME_LENGTH];
    char name_strato_column_vmr[MAX_NAME_LENGTH];
    char name_strato_column_vmr_dry[MAX_NAME_LENGTH];
    char name_tropo_column_vmr[MAX_NAME_LENGTH];
    char name_tropo_column_vmr_dry[MAX_NAME_LENGTH];
    char name_density[MAX_NAME_LENGTH];
    char name_mmr[MAX_NAME_LENGTH];
    char name_mmr_dry[MAX_NAME_LENGTH];
    char name_nd[MAX_NAME_LENGTH];
    char name_pp[MAX_NAME_LENGTH];
    char name_pp_dry[MAX_NAME_LENGTH];
    char name_vmr[MAX_NAME_LENGTH];
    char name_vmr_dry[MAX_NAME_LENGTH];
    int i;

    /* we need to be able to add at least one dimension of our own */
    assert(num_dimensions < HARP_MAX_NUM_DIMS);

    for (i = 0; i < num_dimensions; i++)
    {
        dimension_type[i] = target_dimension_type[i];
    }

    snprintf(name_column_density, MAX_NAME_LENGTH, "%s_column_density", species);
    snprintf(name_strato_column_density, MAX_NAME_LENGTH, "stratospheric_%s_column_density", species);
    snprintf(name_tropo_column_density, MAX_NAME_LENGTH, "tropospheric_%s_column_density", species);
    snprintf(name_column_nd, MAX_NAME_LENGTH, "%s_column_number_density", species);
    snprintf(name_strato_column_nd, MAX_NAME_LENGTH, "stratospheric_%s_column_number_density", species);
    snprintf(name_tropo_column_nd, MAX_NAME_LENGTH, "tropospheric_%s_column_number_density", species);
    snprintf(name_column_mmr, MAX_NAME_LENGTH, "%s_column_mass_mixing_ratio", species);
    snprintf(name_column_mmr_dry, MAX_NAME_LENGTH, "%s_column_mass_mixing_ratio_dry_air", species);
    snprintf(name_strato_column_mmr, MAX_NAME_LENGTH, "stratospheric_%s_column_mass_mixing_ratio", species);
    snprintf(name_strato_column_mmr_dry, MAX_NAME_LENGTH, "stratospheric_%s_column_mass_mixing_ratio_dry_air", species);
    snprintf(name_tropo_column_mmr, MAX_NAME_LENGTH, "tropospheric_%s_column_mass_mixing_ratio", species);
    snprintf(name_tropo_column_mmr_dry, MAX_NAME_LENGTH, "tropospheric_%s_column_mass_mixing_ratio_dry_air", species);
    snprintf(name_column_vmr, MAX_NAME_LENGTH, "%s_column_volume_mixing_ratio", species);
    snprintf(name_column_vmr_dry, MAX_NAME_LENGTH, "%s_column_volume_mixing_ratio_dry_air", species);
    snprintf(name_strato_column_vmr, MAX_NAME_LENGTH, "stratospheric_%s_column_volume_mixing_ratio", species);
    snprintf(name_strato_column_vmr_dry, MAX_NAME_LENGTH, "stratospheric_%s_column_volume_mixing_ratio_dry_air",
             species);
    snprintf(name_tropo_column_vmr, MAX_NAME_LENGTH, "tropospheric_%s_column_volume_mixing_ratio", species);
    snprintf(name_tropo_column_vmr_dry, MAX_NAME_LENGTH, "tropospheric_%s_column_volume_mixing_ratio_dry_air", species);
    snprintf(name_density, MAX_NAME_LENGTH, "%s_density", species);
    snprintf(name_mmr, MAX_NAME_LENGTH, "%s_mass_mixing_ratio", species);
    snprintf(name_mmr_dry, MAX_NAME_LENGTH, "%s_mass_mixing_ratio_dry_air", species);
    snprintf(name_nd, MAX_NAME_LENGTH, "%s_number_density", species);
    snprintf(name_pp, MAX_NAME_LENGTH, "%s_partial_pressure", species);
    snprintf(name_pp_dry, MAX_NAME_LENGTH, "%s_partial_pressure_dry_air", species);
    snprintf(name_vmr, MAX_NAME_LENGTH, "%s_volume_mixing_ratio", species);
    snprintf(name_vmr_dry, MAX_NAME_LENGTH, "%s_volume_mixing_ratio_dry_air", species);

    /*** column (mass) density ***/

    /* time dependent from independent */
    if (add_time_indepedent_to_dependent_conversion(name_column_density, harp_type_double,
                                                    HARP_UNIT_COLUMN_MASS_DENSITY, num_dimensions, dimension_type, 0) !=
        0)
    {
        return -1;
    }

    /* uncertainties */
    if (add_uncertainty_conversions(name_column_density, HARP_UNIT_COLUMN_MASS_DENSITY, num_dimensions, dimension_type)
        != 0)
    {
        return -1;
    }

    /* mass density from number density */
    if (harp_variable_conversion_new(name_density, harp_type_double, HARP_UNIT_COLUMN_MASS_DENSITY, num_dimensions,
                                     dimension_type, 0, get_density_from_nd, &conversion) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, name_nd, harp_type_double, HARP_UNIT_COLUMN_NUMBER_DENSITY,
                                            num_dimensions, dimension_type, 0) != 0)
    {
        return -1;
    }

    /*** stratospheric column (mass) density ***/

    /* time dependent from independent */
    if (add_time_indepedent_to_dependent_conversion(name_strato_column_density, harp_type_double,
                                                    HARP_UNIT_COLUMN_MASS_DENSITY, num_dimensions, dimension_type, 0)
        != 0)
    {
        return -1;
    }

    /* uncertainties */
    if (add_uncertainty_conversions(name_strato_column_density, HARP_UNIT_COLUMN_MASS_DENSITY, num_dimensions,
                                    dimension_type) != 0)
    {
        return -1;
    }

    /*** tropospheric column (mass) density ***/

    /* time dependent from independent */
    if (add_time_indepedent_to_dependent_conversion(name_tropo_column_density, harp_type_double,
                                                    HARP_UNIT_COLUMN_MASS_DENSITY, num_dimensions, dimension_type, 0)
        != 0)
    {
        return -1;
    }

    /* uncertainties */
    if (add_uncertainty_conversions(name_tropo_column_density, HARP_UNIT_COLUMN_MASS_DENSITY, num_dimensions,
                                    dimension_type) != 0)
    {
        return -1;
    }

    /*** column number density ***/

    /* time dependent from independent */
    if (add_time_indepedent_to_dependent_conversion(name_column_nd, harp_type_double, HARP_UNIT_COLUMN_NUMBER_DENSITY,
                                                    num_dimensions, dimension_type, 0) != 0)
    {
        return -1;
    }

    /* uncertainties */
    if (add_uncertainty_conversions(name_column_nd, HARP_UNIT_COLUMN_NUMBER_DENSITY, num_dimensions, dimension_type) !=
        0)
    {
        return -1;
    }

    /* column from partial column profile */
    if (num_dimensions == 0 || dimension_type[num_dimensions - 1] != harp_dimension_vertical)
    {
        if (harp_variable_conversion_new(name_column_nd, harp_type_double, HARP_UNIT_COLUMN_NUMBER_DENSITY,
                                         num_dimensions, dimension_type, 0, get_column_from_partial_column,
                                         &conversion) != 0)
        {
            return -1;
        }
        dimension_type[num_dimensions] = harp_dimension_vertical;
        if (harp_variable_conversion_add_source(conversion, name_column_nd, harp_type_double,
                                                HARP_UNIT_COLUMN_NUMBER_DENSITY, num_dimensions + 1, dimension_type, 0)
            != 0)
        {
            return -1;
        }
    }

    /* create partial column profile from densities */
    dimension_type[num_dimensions] = harp_dimension_independent;
    if (harp_variable_conversion_new(name_column_nd, harp_type_double, HARP_UNIT_COLUMN_NUMBER_DENSITY, num_dimensions,
                                     dimension_type, 0, get_partial_column_from_density_and_alt_bounds, &conversion) !=
        0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, name_nd, harp_type_double, HARP_UNIT_NUMBER_DENSITY,
                                            num_dimensions, dimension_type, 0) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, "altitude_bounds", harp_type_double, HARP_UNIT_LENGTH,
                                            num_dimensions + 1, dimension_type, 2) != 0)
    {
        return -1;
    }

    /*** stratospheric column number density ***/

    /* time dependent from independent */
    if (add_time_indepedent_to_dependent_conversion(name_strato_column_nd, harp_type_double,
                                                    HARP_UNIT_COLUMN_NUMBER_DENSITY, num_dimensions, dimension_type, 0)
        != 0)
    {
        return -1;
    }

    /* uncertainties */
    if (add_uncertainty_conversions(name_strato_column_nd, HARP_UNIT_COLUMN_NUMBER_DENSITY, num_dimensions,
                                    dimension_type) != 0)
    {
        return -1;
    }

    /*** tropospheric column number density ***/

    /* time dependent from independent */
    if (add_time_indepedent_to_dependent_conversion(name_tropo_column_nd, harp_type_double,
                                                    HARP_UNIT_COLUMN_NUMBER_DENSITY, num_dimensions, dimension_type, 0)
        != 0)
    {
        return -1;
    }

    /* uncertainties */
    if (add_uncertainty_conversions(name_tropo_column_nd, HARP_UNIT_COLUMN_NUMBER_DENSITY, num_dimensions,
                                    dimension_type) != 0)
    {
        return -1;
    }

    if (num_dimensions == 0 || dimension_type[num_dimensions - 1] != harp_dimension_vertical)
    {
        /*** column mass mixing ratio ***/

        /* time dependent from independent */
        if (add_time_indepedent_to_dependent_conversion(name_column_mmr, harp_type_double, HARP_UNIT_MASS_MIXING_RATIO,
                                                        num_dimensions, dimension_type, 0) != 0)
        {
            return -1;
        }

        /* uncertainties */
        if (add_uncertainty_conversions(name_column_mmr, HARP_UNIT_MASS_MIXING_RATIO, num_dimensions, dimension_type) !=
            0)
        {
            return -1;
        }

        /* mmr from vmr */
        if (harp_variable_conversion_new(name_column_mmr, harp_type_double, HARP_UNIT_MASS_MIXING_RATIO, num_dimensions,
                                         dimension_type, 0, get_mmr_from_vmr, &conversion) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, name_column_vmr, harp_type_double,
                                                HARP_UNIT_VOLUME_MIXING_RATIO, num_dimensions, dimension_type, 0) != 0)
        {
            return -1;
        }

        /*** column mass mixing ratio dry air ***/

        /* time dependent from independent */
        if (add_time_indepedent_to_dependent_conversion(name_column_mmr_dry, harp_type_double,
                                                        HARP_UNIT_MASS_MIXING_RATIO, num_dimensions, dimension_type, 0)
            != 0)
        {
            return -1;
        }

        /* uncertainties */
        if (add_uncertainty_conversions(name_column_mmr_dry, HARP_UNIT_MASS_MIXING_RATIO, num_dimensions,
                                        dimension_type) != 0)
        {
            return -1;
        }

        /* mmr from vmr */
        if (harp_variable_conversion_new(name_column_mmr_dry, harp_type_double, HARP_UNIT_MASS_MIXING_RATIO,
                                         num_dimensions, dimension_type, 0, get_mmr_from_vmr, &conversion) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, name_column_vmr_dry, harp_type_double,
                                                HARP_UNIT_VOLUME_MIXING_RATIO, num_dimensions, dimension_type, 0) != 0)
        {
            return -1;
        }

        /*** stratospheric column mass mixing ratio ***/

        /* time dependent from independent */
        if (add_time_indepedent_to_dependent_conversion(name_strato_column_mmr, harp_type_double,
                                                        HARP_UNIT_MASS_MIXING_RATIO, num_dimensions, dimension_type, 0)
            != 0)
        {
            return -1;
        }

        /* uncertainties */
        if (add_uncertainty_conversions(name_strato_column_mmr, HARP_UNIT_MASS_MIXING_RATIO, num_dimensions,
                                        dimension_type) != 0)
        {
            return -1;
        }

        /* mmr from vmr */
        if (harp_variable_conversion_new(name_strato_column_mmr, harp_type_double, HARP_UNIT_MASS_MIXING_RATIO,
                                         num_dimensions, dimension_type, 0, get_mmr_from_vmr, &conversion) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, name_strato_column_vmr, harp_type_double,
                                                HARP_UNIT_VOLUME_MIXING_RATIO, num_dimensions, dimension_type, 0) != 0)
        {
            return -1;
        }

        /*** stratospheric column mass mixing ratio dry air ***/

        /* time dependent from independent */
        if (add_time_indepedent_to_dependent_conversion(name_strato_column_mmr_dry, harp_type_double,
                                                        HARP_UNIT_MASS_MIXING_RATIO, num_dimensions, dimension_type, 0)
            != 0)
        {
            return -1;
        }

        /* uncertainties */
        if (add_uncertainty_conversions(name_strato_column_mmr_dry, HARP_UNIT_MASS_MIXING_RATIO, num_dimensions,
                                        dimension_type) != 0)
        {
            return -1;
        }

        /* mmr from vmr */
        if (harp_variable_conversion_new(name_strato_column_mmr_dry, harp_type_double, HARP_UNIT_MASS_MIXING_RATIO,
                                         num_dimensions, dimension_type, 0, get_mmr_from_vmr, &conversion) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, name_strato_column_vmr_dry, harp_type_double,
                                                HARP_UNIT_VOLUME_MIXING_RATIO, num_dimensions, dimension_type, 0) != 0)
        {
            return -1;
        }

        /*** tropospheric column mass mixing ratio ***/

        /* time dependent from independent */
        if (add_time_indepedent_to_dependent_conversion(name_tropo_column_mmr, harp_type_double,
                                                        HARP_UNIT_MASS_MIXING_RATIO, num_dimensions, dimension_type, 0)
            != 0)
        {
            return -1;
        }

        /* uncertainties */
        if (add_uncertainty_conversions(name_tropo_column_mmr, HARP_UNIT_MASS_MIXING_RATIO, num_dimensions,
                                        dimension_type) != 0)
        {
            return -1;
        }

        /* mmr from vmr */
        if (harp_variable_conversion_new(name_tropo_column_mmr, harp_type_double, HARP_UNIT_MASS_MIXING_RATIO,
                                         num_dimensions, dimension_type, 0, get_mmr_from_vmr, &conversion) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, name_tropo_column_vmr, harp_type_double,
                                                HARP_UNIT_VOLUME_MIXING_RATIO, num_dimensions, dimension_type, 0) != 0)
        {
            return -1;
        }


        /*** tropospheric column mass mixing ratio dry air ***/

        /* time dependent from independent */
        if (add_time_indepedent_to_dependent_conversion(name_tropo_column_mmr_dry, harp_type_double,
                                                        HARP_UNIT_MASS_MIXING_RATIO, num_dimensions, dimension_type, 0)
            != 0)
        {
            return -1;
        }

        /* uncertainties */
        if (add_uncertainty_conversions(name_tropo_column_mmr_dry, HARP_UNIT_MASS_MIXING_RATIO, num_dimensions,
                                        dimension_type) != 0)
        {
            return -1;
        }

        /* mmr from vmr */
        if (harp_variable_conversion_new(name_tropo_column_mmr_dry, harp_type_double, HARP_UNIT_MASS_MIXING_RATIO,
                                         num_dimensions, dimension_type, 0, get_mmr_from_vmr, &conversion) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, name_tropo_column_vmr_dry, harp_type_double,
                                                HARP_UNIT_VOLUME_MIXING_RATIO, num_dimensions, dimension_type, 0) != 0)
        {
            return -1;
        }

        /*** column volume mixing ratio ***/

        /* time dependent from independent */
        if (add_time_indepedent_to_dependent_conversion(name_column_vmr, harp_type_double,
                                                        HARP_UNIT_VOLUME_MIXING_RATIO, num_dimensions, dimension_type,
                                                        0) != 0)
        {
            return -1;
        }

        /* uncertainties */
        if (add_uncertainty_conversions(name_column_vmr, HARP_UNIT_VOLUME_MIXING_RATIO, num_dimensions, dimension_type)
            != 0)
        {
            return -1;
        }

        /* vmr from mmr */
        if (harp_variable_conversion_new(name_column_vmr, harp_type_double, HARP_UNIT_VOLUME_MIXING_RATIO,
                                         num_dimensions, dimension_type, 0, get_vmr_from_mmr, &conversion) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, name_column_mmr, harp_type_double,
                                                HARP_UNIT_MASS_MIXING_RATIO, num_dimensions, dimension_type, 0) != 0)
        {
            return -1;
        }

        /*** column volume mixing ratio dry air ***/

        /* time dependent from independent */
        if (add_time_indepedent_to_dependent_conversion(name_column_vmr_dry, harp_type_double,
                                                        HARP_UNIT_VOLUME_MIXING_RATIO, num_dimensions, dimension_type,
                                                        0) != 0)
        {
            return -1;
        }

        /* uncertainties */
        if (add_uncertainty_conversions(name_column_vmr_dry, HARP_UNIT_VOLUME_MIXING_RATIO, num_dimensions,
                                        dimension_type) != 0)
        {
            return -1;
        }

        /* vmr from mmr */
        if (harp_variable_conversion_new(name_column_vmr_dry, harp_type_double, HARP_UNIT_VOLUME_MIXING_RATIO,
                                         num_dimensions, dimension_type, 0, get_vmr_from_mmr, &conversion) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, name_column_mmr_dry, harp_type_double,
                                                HARP_UNIT_MASS_MIXING_RATIO, num_dimensions, dimension_type, 0) != 0)
        {
            return -1;
        }

        /*** stratospheric column volume mixing ratio ***/

        /* time dependent from independent */
        if (add_time_indepedent_to_dependent_conversion(name_strato_column_vmr, harp_type_double,
                                                        HARP_UNIT_VOLUME_MIXING_RATIO, num_dimensions, dimension_type,
                                                        0) != 0)
        {
            return -1;
        }

        /* uncertainties */
        if (add_uncertainty_conversions(name_strato_column_vmr, HARP_UNIT_VOLUME_MIXING_RATIO, num_dimensions,
                                        dimension_type) != 0)
        {
            return -1;
        }

        /* vmr from mmr */
        if (harp_variable_conversion_new(name_strato_column_vmr, harp_type_double, HARP_UNIT_VOLUME_MIXING_RATIO,
                                         num_dimensions, dimension_type, 0, get_vmr_from_mmr, &conversion) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, name_strato_column_mmr, harp_type_double,
                                                HARP_UNIT_MASS_MIXING_RATIO, num_dimensions, dimension_type, 0) != 0)
        {
            return -1;
        }

        /*** stratospheric column volume mixing ratio dry air ***/

        /* time dependent from independent */
        if (add_time_indepedent_to_dependent_conversion(name_strato_column_vmr_dry, harp_type_double,
                                                        HARP_UNIT_VOLUME_MIXING_RATIO, num_dimensions, dimension_type,
                                                        0) != 0)
        {
            return -1;
        }

        /* uncertainties */
        if (add_uncertainty_conversions(name_strato_column_vmr_dry, HARP_UNIT_VOLUME_MIXING_RATIO, num_dimensions,
                                        dimension_type) != 0)
        {
            return -1;
        }

        /* vmr from mmr */
        if (harp_variable_conversion_new(name_strato_column_vmr_dry, harp_type_double, HARP_UNIT_VOLUME_MIXING_RATIO,
                                         num_dimensions, dimension_type, 0, get_vmr_from_mmr, &conversion) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, name_strato_column_mmr_dry, harp_type_double,
                                                HARP_UNIT_MASS_MIXING_RATIO, num_dimensions, dimension_type, 0) != 0)
        {
            return -1;
        }

        /*** tropospheric column volume mixing ratio ***/

        /* time dependent from independent */
        if (add_time_indepedent_to_dependent_conversion(name_tropo_column_vmr, harp_type_double,
                                                        HARP_UNIT_VOLUME_MIXING_RATIO, num_dimensions, dimension_type,
                                                        0) != 0)
        {
            return -1;
        }

        /* uncertainties */
        if (add_uncertainty_conversions(name_tropo_column_vmr, HARP_UNIT_VOLUME_MIXING_RATIO, num_dimensions,
                                        dimension_type) != 0)
        {
            return -1;
        }

        /* vmr from mmr */
        if (harp_variable_conversion_new(name_tropo_column_vmr, harp_type_double, HARP_UNIT_VOLUME_MIXING_RATIO,
                                         num_dimensions, dimension_type, 0, get_vmr_from_mmr, &conversion) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, name_tropo_column_mmr, harp_type_double,
                                                HARP_UNIT_MASS_MIXING_RATIO, num_dimensions, dimension_type, 0) != 0)
        {
            return -1;
        }

        /*** tropospheric column volume mixing ratio dry air ***/

        /* time dependent from independent */
        if (add_time_indepedent_to_dependent_conversion(name_tropo_column_vmr_dry, harp_type_double,
                                                        HARP_UNIT_VOLUME_MIXING_RATIO, num_dimensions, dimension_type,
                                                        0) != 0)
        {
            return -1;
        }

        /* uncertainties */
        if (add_uncertainty_conversions(name_tropo_column_vmr_dry, HARP_UNIT_VOLUME_MIXING_RATIO, num_dimensions,
                                        dimension_type) != 0)
        {
            return -1;
        }

        /* vmr from mmr */
        if (harp_variable_conversion_new(name_tropo_column_vmr_dry, harp_type_double, HARP_UNIT_VOLUME_MIXING_RATIO,
                                         num_dimensions, dimension_type, 0, get_vmr_from_mmr, &conversion) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, name_tropo_column_mmr_dry, harp_type_double,
                                                HARP_UNIT_MASS_MIXING_RATIO, num_dimensions, dimension_type, 0) != 0)
        {
            return -1;
        }
    }

    /*** (mass) density ***/

    /* time dependent from independent */
    if (add_time_indepedent_to_dependent_conversion(name_density, harp_type_double, HARP_UNIT_MASS_DENSITY,
                                                    num_dimensions, dimension_type, 0) != 0)
    {
        return -1;
    }

    /* uncertainties */
    if (add_uncertainty_conversions(name_density, HARP_UNIT_NUMBER_DENSITY, num_dimensions, dimension_type) != 0)
    {
        return -1;
    }

    /* mass density from number density */
    if (harp_variable_conversion_new(name_density, harp_type_double, HARP_UNIT_MASS_DENSITY, num_dimensions,
                                     dimension_type, 0, get_density_from_nd, &conversion) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, name_nd, harp_type_double, HARP_UNIT_NUMBER_DENSITY,
                                            num_dimensions, dimension_type, 0) != 0)
    {
        return -1;
    }

    /*** mass mixing ratio ***/

    /* time dependent from independent */
    if (add_time_indepedent_to_dependent_conversion(name_mmr, harp_type_double, HARP_UNIT_MASS_MIXING_RATIO,
                                                    num_dimensions, dimension_type, 0) != 0)
    {
        return -1;
    }

    /* uncertainties */
    if (add_uncertainty_conversions(name_mmr, HARP_UNIT_MASS_MIXING_RATIO, num_dimensions, dimension_type) != 0)
    {
        return -1;
    }

    /* mmr from vmr */
    if (harp_variable_conversion_new(name_mmr, harp_type_double, HARP_UNIT_MASS_MIXING_RATIO, num_dimensions,
                                     dimension_type, 0, get_mmr_from_vmr, &conversion) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, name_vmr, harp_type_double, HARP_UNIT_VOLUME_MIXING_RATIO,
                                            num_dimensions, dimension_type, 0) != 0)
    {
        return -1;
    }

    /*** mass mixing ratio dry air ***/

    /* time dependent from independent */
    if (add_time_indepedent_to_dependent_conversion(name_mmr_dry, harp_type_double, HARP_UNIT_MASS_MIXING_RATIO,
                                                    num_dimensions, dimension_type, 0) != 0)
    {
        return -1;
    }

    /* uncertainties */
    if (add_uncertainty_conversions(name_mmr_dry, HARP_UNIT_MASS_MIXING_RATIO, num_dimensions, dimension_type) != 0)
    {
        return -1;
    }

    /* mmr from vmr */
    if (harp_variable_conversion_new(name_mmr_dry, harp_type_double, HARP_UNIT_MASS_MIXING_RATIO, num_dimensions,
                                     dimension_type, 0, get_mmr_from_vmr, &conversion) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, name_vmr, harp_type_double, HARP_UNIT_VOLUME_MIXING_RATIO,
                                            num_dimensions, dimension_type, 0) != 0)
    {
        return -1;
    }

    /*** number density ***/

    /* time dependent from independent */
    if (add_time_indepedent_to_dependent_conversion(name_nd, harp_type_double, HARP_UNIT_NUMBER_DENSITY, num_dimensions,
                                                    dimension_type, 0) != 0)
    {
        return -1;
    }

    /* uncertainties */
    if (add_uncertainty_conversions(name_nd, HARP_UNIT_NUMBER_DENSITY, num_dimensions, dimension_type) != 0)
    {
        return -1;
    }

    /* number density from mass density */
    if (harp_variable_conversion_new(name_nd, harp_type_double, HARP_UNIT_NUMBER_DENSITY, num_dimensions,
                                     dimension_type, 0, get_nd_from_density, &conversion) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, name_density, harp_type_double, HARP_UNIT_MASS_DENSITY,
                                            num_dimensions, dimension_type, 0) != 0)
    {
        return -1;
    }

    /* number density from vmr/p/T */
    if (harp_variable_conversion_new(name_nd, harp_type_double, HARP_UNIT_NUMBER_DENSITY, num_dimensions,
                                     dimension_type, 0, get_nd_from_vmr_pressure_and_temperature, &conversion) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, name_vmr, harp_type_double, HARP_UNIT_VOLUME_MIXING_RATIO,
                                            num_dimensions, dimension_type, 0) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, "pressure", harp_type_double, HARP_UNIT_PRESSURE,
                                            num_dimensions, dimension_type, 0) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, "temperature", harp_type_double, HARP_UNIT_TEMPERATURE,
                                            num_dimensions, dimension_type, 0) != 0)
    {
        return -1;
    }

    /* number density from partial column profile */
    if (harp_variable_conversion_new(name_nd, harp_type_double, HARP_UNIT_NUMBER_DENSITY, num_dimensions,
                                     dimension_type, 0, get_density_from_partial_column_and_alt_bounds, &conversion) !=
        0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, name_column_nd, harp_type_double,
                                            HARP_UNIT_COLUMN_NUMBER_DENSITY, num_dimensions, dimension_type, 0) != 0)
    {
        return -1;
    }
    dimension_type[num_dimensions] = harp_dimension_independent;
    if (harp_variable_conversion_add_source(conversion, "altitude_bounds", harp_type_double, HARP_UNIT_LENGTH,
                                            num_dimensions + 1, dimension_type, 2) != 0)
    {
        return -1;
    }

    /*** partial pressure ***/

    /* time dependent from independent */
    if (add_time_indepedent_to_dependent_conversion(name_pp, harp_type_double, HARP_UNIT_PRESSURE, num_dimensions,
                                                    dimension_type, 0) != 0)
    {
        return -1;
    }

    /* uncertainties */
    if (add_uncertainty_conversions(name_pp, HARP_UNIT_PRESSURE, num_dimensions, dimension_type) != 0)
    {
        return -1;
    }

    /* pp from vmr/p */
    if (harp_variable_conversion_new(name_pp, harp_type_double, HARP_UNIT_PRESSURE, num_dimensions, dimension_type, 0,
                                     get_partial_pressure_from_vmr_and_pressure, &conversion) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, name_vmr, harp_type_double, HARP_UNIT_VOLUME_MIXING_RATIO,
                                            num_dimensions, dimension_type, 0) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, "pressure", harp_type_double, HARP_UNIT_PRESSURE,
                                            num_dimensions, dimension_type, 0) != 0)
    {
        return -1;
    }

    /*** partial pressure dry air ***/

    /* time dependent from independent */
    if (add_time_indepedent_to_dependent_conversion(name_pp_dry, harp_type_double, HARP_UNIT_PRESSURE, num_dimensions,
                                                    dimension_type, 0) != 0)
    {
        return -1;
    }

    /* uncertainties */
    if (add_uncertainty_conversions(name_pp_dry, HARP_UNIT_PRESSURE, num_dimensions, dimension_type) != 0)
    {
        return -1;
    }

    /* pp from vmr/p */
    if (harp_variable_conversion_new(name_pp_dry, harp_type_double, HARP_UNIT_PRESSURE, num_dimensions, dimension_type,
                                     0, get_partial_pressure_from_vmr_and_pressure, &conversion) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, name_vmr_dry, harp_type_double, HARP_UNIT_VOLUME_MIXING_RATIO,
                                            num_dimensions, dimension_type, 0) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, "pressure", harp_type_double, HARP_UNIT_PRESSURE,
                                            num_dimensions, dimension_type, 0) != 0)
    {
        return -1;
    }

    /*** volume mixing ratio ****/

    /* time dependent from independent */
    if (add_time_indepedent_to_dependent_conversion(name_vmr, harp_type_double, HARP_UNIT_VOLUME_MIXING_RATIO,
                                                    num_dimensions, dimension_type, 0) != 0)
    {
        return -1;
    }

    /* uncertainties */
    if (add_uncertainty_conversions(name_vmr, HARP_UNIT_VOLUME_MIXING_RATIO, num_dimensions, dimension_type) != 0)
    {
        return -1;
    }

    /* vmr from nd/p/T */
    if (harp_variable_conversion_new(name_vmr, harp_type_double, HARP_UNIT_VOLUME_MIXING_RATIO, num_dimensions,
                                     dimension_type, 0, get_vmr_from_nd_pressure_and_temperature, &conversion) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, name_nd, harp_type_double, HARP_UNIT_NUMBER_DENSITY,
                                            num_dimensions, dimension_type, 0) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, "pressure", harp_type_double, HARP_UNIT_PRESSURE,
                                            num_dimensions, dimension_type, 0) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, "temperature", harp_type_double, HARP_UNIT_TEMPERATURE,
                                            num_dimensions, dimension_type, 0) != 0)
    {
        return -1;
    }

    /* vmr from mmr */
    if (harp_variable_conversion_new(name_vmr, harp_type_double, HARP_UNIT_VOLUME_MIXING_RATIO, num_dimensions,
                                     dimension_type, 0, get_vmr_from_mmr, &conversion) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, name_mmr, harp_type_double, HARP_UNIT_MASS_MIXING_RATIO,
                                            num_dimensions, dimension_type, 0) != 0)
    {
        return -1;
    }

#if 0
    if (strcmp(species, "H2O") != 0)
    {
        /* vmr from mmr(wet)/Q */
        if (harp_variable_conversion_new(name_vmr, harp_type_double, HARP_UNIT_VOLUME_MIXING_RATIO, num_dimensions,
                                         dimension_type, 0, get_vmr_from_mmrw_and_humidity, &conversion) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, name_mmrw, harp_type_double, HARP_UNIT_MASS_MIXING_RATIO,
                                                num_dimensions, dimension_type, 0) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, "H2O_mass_mixing_ratio", harp_type_double,
                                                HARP_UNIT_MASS_MIXING_RATIO, num_dimensions, dimension_type, 0) != 0)
        {
            return -1;
        }
    }
#endif

    /* vmr from pp/p */
    if (harp_variable_conversion_new(name_vmr, harp_type_double, HARP_UNIT_VOLUME_MIXING_RATIO, num_dimensions,
                                     dimension_type, 0, get_vmr_from_partial_pressure_and_pressure, &conversion) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, name_pp, harp_type_double, HARP_UNIT_PRESSURE, num_dimensions,
                                            dimension_type, 0) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, "pressure", harp_type_double, HARP_UNIT_PRESSURE,
                                            num_dimensions, dimension_type, 0) != 0)
    {
        return -1;
    }

    /*** volume mixing ratio dry air ****/

    /* time dependent from independent */
    if (add_time_indepedent_to_dependent_conversion(name_vmr_dry, harp_type_double, HARP_UNIT_VOLUME_MIXING_RATIO,
                                                    num_dimensions, dimension_type, 0) != 0)
    {
        return -1;
    }

    /* uncertainties */
    if (add_uncertainty_conversions(name_vmr_dry, HARP_UNIT_VOLUME_MIXING_RATIO, num_dimensions, dimension_type) != 0)
    {
        return -1;
    }

    /* vmr from mmr */
    if (harp_variable_conversion_new(name_vmr_dry, harp_type_double, HARP_UNIT_VOLUME_MIXING_RATIO, num_dimensions,
                                     dimension_type, 0, get_vmr_from_mmr, &conversion) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, name_mmr_dry, harp_type_double, HARP_UNIT_MASS_MIXING_RATIO,
                                            num_dimensions, dimension_type, 0) != 0)
    {
        return -1;
    }

    /* vmr from pp/p */
    if (harp_variable_conversion_new(name_vmr_dry, harp_type_double, HARP_UNIT_VOLUME_MIXING_RATIO, num_dimensions,
                                     dimension_type, 0, get_vmr_from_partial_pressure_and_pressure, &conversion) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, name_pp_dry, harp_type_double, HARP_UNIT_PRESSURE,
                                            num_dimensions, dimension_type, 0) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, "pressure", harp_type_double, HARP_UNIT_PRESSURE,
                                            num_dimensions, dimension_type, 0) != 0)
    {
        return -1;
    }

    return 0;
}

static int add_aerosol_conversions_for_grid(int num_dimensions, harp_dimension_type dimension_type[HARP_MAX_NUM_DIMS])
{
    const char *prefix[] = { "", "sea_salt_", "dust_", "organic_matter_", "black_carbon_", "sulphate_" };
    int i;

    for (i = 0; i < 6; i++)
    {
        harp_variable_conversion *conversion;
        char name_aod[MAX_NAME_LENGTH];
        char name_ext[MAX_NAME_LENGTH];

        snprintf(name_aod, MAX_NAME_LENGTH, "%saerosol_optical_depth", prefix[i]);
        snprintf(name_ext, MAX_NAME_LENGTH, "%saerosol_extinction_coefficient", prefix[i]);

        /*** aerosol extinction coefficient ***/

        /* time dependent from independent */
        if (add_time_indepedent_to_dependent_conversion(name_ext, harp_type_double, HARP_UNIT_AEROSOL_EXTINCTION,
                                                        num_dimensions, dimension_type, 0) != 0)
        {
            return -1;
        }

        /* uncertainties */
        if (add_uncertainty_conversions(name_ext, HARP_UNIT_AEROSOL_EXTINCTION, num_dimensions, dimension_type) != 0)
        {
            return -1;
        }

        /* ext from aod */
        if (harp_variable_conversion_new(name_ext, harp_type_double, HARP_UNIT_AEROSOL_EXTINCTION,
                                         num_dimensions, dimension_type, 0,
                                         get_density_from_partial_column_and_alt_bounds, &conversion) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, name_aod, harp_type_double, HARP_UNIT_DIMENSIONLESS,
                                                num_dimensions, dimension_type, 0) != 0)
        {
            return -1;
        }
        dimension_type[num_dimensions] = harp_dimension_independent;
        if (harp_variable_conversion_add_source(conversion, "altitude_bounds", harp_type_double, HARP_UNIT_LENGTH,
                                                num_dimensions + 1, dimension_type, 2) != 0)
        {
            return -1;
        }

        /*** aerosol optical depth ***/

        /* time dependent from independent */
        if (add_time_indepedent_to_dependent_conversion(name_aod, harp_type_double, HARP_UNIT_DIMENSIONLESS,
                                                        num_dimensions, dimension_type, 0) != 0)
        {
            return -1;
        }

        /* uncertainties */
        if (add_uncertainty_conversions(name_aod, HARP_UNIT_DIMENSIONLESS, num_dimensions, dimension_type) != 0)
        {
            return -1;
        }

        /* aod from partial aod profile */
        if (num_dimensions == 0 || dimension_type[num_dimensions - 1] != harp_dimension_vertical)
        {
            if (harp_variable_conversion_new(name_aod, harp_type_double, HARP_UNIT_DIMENSIONLESS, num_dimensions,
                                             dimension_type, 0, get_column_from_partial_column, &conversion) != 0)
            {
                return -1;
            }
            dimension_type[num_dimensions] = harp_dimension_vertical;
            if (harp_variable_conversion_add_source(conversion, name_aod, harp_type_double, HARP_UNIT_DIMENSIONLESS,
                                                    num_dimensions + 1, dimension_type, 0) != 0)
            {
                return -1;
            }
        }

        /* aod from ext */
        if (harp_variable_conversion_new(name_aod, harp_type_double, HARP_UNIT_DIMENSIONLESS, num_dimensions,
                                         dimension_type, 0, get_partial_column_from_density_and_alt_bounds, &conversion)
            != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, name_ext, harp_type_double, HARP_UNIT_AEROSOL_EXTINCTION,
                                                num_dimensions, dimension_type, 0) != 0)
        {
            return -1;
        }
        dimension_type[num_dimensions] = harp_dimension_independent;
        if (harp_variable_conversion_add_source(conversion, "altitude_bounds", harp_type_double, HARP_UNIT_LENGTH,
                                                num_dimensions + 1, dimension_type, 2) != 0)
        {
            return -1;
        }
    }

    return 0;
}

static int add_spectral_grouping_conversions_for_grid(int num_dimensions,
                                                      harp_dimension_type target_dimension_type[HARP_MAX_NUM_DIMS])
{
    harp_dimension_type dimension_type[HARP_MAX_NUM_DIMS];
    int i;

    if (add_aerosol_conversions_for_grid(num_dimensions, target_dimension_type) != 0)
    {
        return -1;
    }

    /* the spectral dimension comes right after the time dimension (if it is present) */
    if (num_dimensions == 0)
    {
        dimension_type[0] = harp_dimension_spectral;
    }
    else if (target_dimension_type[0] == harp_dimension_time)
    {
        dimension_type[0] = harp_dimension_time;
        dimension_type[1] = harp_dimension_spectral;
        for (i = 1; i < num_dimensions; i++)
        {
            dimension_type[i + 1] = target_dimension_type[i];
        }
    }
    else
    {
        dimension_type[0] = harp_dimension_spectral;
        for (i = 0; i < num_dimensions; i++)
        {
            dimension_type[i + 1] = target_dimension_type[i];
        }
    }
    if (add_aerosol_conversions_for_grid(num_dimensions + 1, dimension_type) != 0)
    {
        return -1;
    }

    return 0;
}

static int add_conversions_for_grid(int num_dimensions, harp_dimension_type dimension_type[HARP_MAX_NUM_DIMS],
                                    int has_latlon)
{
    harp_variable_conversion *conversion;
    int i;

    /* Add conversions for variables that start with a species name */
    for (i = 0; i < harp_num_chemical_species; i++)
    {
        if (add_species_conversions_for_grid(harp_chemical_species_name(i), num_dimensions, dimension_type) != 0)
        {
            return -1;
        }
    }

    /* Add conversions for variables that can be spectral dependent (with spectral dimension used for grouping) */
    if (add_spectral_grouping_conversions_for_grid(num_dimensions, dimension_type) != 0)
    {
        return -1;
    }

    /*** number density ***/

    /* time dependent from independent */
    if (add_time_indepedent_to_dependent_conversion("number_density", harp_type_double, HARP_UNIT_NUMBER_DENSITY,
                                                    num_dimensions, dimension_type, 0) != 0)
    {
        return -1;
    }

    /* uncertainties */
    if (add_uncertainty_conversions("number_density", HARP_UNIT_NUMBER_DENSITY, num_dimensions, dimension_type) != 0)
    {
        return -1;
    }

    /* nd from p/T */
    if (harp_variable_conversion_new("number_density", harp_type_double, HARP_UNIT_NUMBER_DENSITY, num_dimensions,
                                     dimension_type, 0, get_air_nd_from_pressure_and_temperature, &conversion) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, "pressure", harp_type_double, HARP_UNIT_PRESSURE,
                                            num_dimensions, dimension_type, 0) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, "temperature", harp_type_double, HARP_UNIT_TEMPERATURE,
                                            num_dimensions, dimension_type, 0) != 0)
    {
        return -1;
    }

    /*** altitude ***/

    /* time dependent from independent */
    if (add_time_indepedent_to_dependent_conversion("altitude", harp_type_double, HARP_UNIT_LENGTH, num_dimensions,
                                                    dimension_type, 0) != 0)
    {
        return -1;
    }

    if (!has_latlon)
    {
        /* altitude from gph */
        if (harp_variable_conversion_new
            ("altitude", harp_type_double, HARP_UNIT_LENGTH, num_dimensions, dimension_type, 0,
             get_altitude_from_gph_and_latitude, &conversion) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, "geopotential_heigth", harp_type_double, HARP_UNIT_LENGTH,
                                                num_dimensions, dimension_type, 0) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, "latitude", harp_type_double, HARP_UNIT_LATITUDE,
                                                num_dimensions, dimension_type, 0) != 0)
        {
            return -1;
        }
    }

    /*** altitude_bounds ***/

    /* time dependent from independent */
    dimension_type[num_dimensions] = harp_dimension_independent;
    if (add_time_indepedent_to_dependent_conversion("altitude_bounds", harp_type_double, HARP_UNIT_LENGTH,
                                                    num_dimensions + 1, dimension_type, 2) != 0)
    {
        return -1;
    }

    /*** geopotential ***/

    /* time dependent from independent */
    if (add_time_indepedent_to_dependent_conversion("geopotential", harp_type_double, HARP_UNIT_GEOPOTENTIAL,
                                                    num_dimensions, dimension_type, 0) != 0)
    {
        return -1;
    }

    /* uncertainties */
    if (add_uncertainty_conversions("geopotential", HARP_UNIT_GEOPOTENTIAL, num_dimensions, dimension_type) != 0)
    {
        return -1;
    }

    /* geopotential from gph */
    if (harp_variable_conversion_new("geopotential", harp_type_double, HARP_UNIT_GEOPOTENTIAL, num_dimensions,
                                     dimension_type, 0, get_geopotential_from_gph, &conversion) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, "geopotential_height", harp_type_double, HARP_UNIT_LENGTH,
                                            num_dimensions, dimension_type, 0) != 0)
    {
        return -1;
    }

    /*** geopotential_height ***/

    /* time dependent from independent */
    if (add_time_indepedent_to_dependent_conversion("geopotential_height", harp_type_double, HARP_UNIT_LENGTH,
                                                    num_dimensions, dimension_type, 0) != 0)
    {
        return -1;
    }

    /* gph from geopotential */
    if (harp_variable_conversion_new("geopotential_height", harp_type_double, HARP_UNIT_LENGTH, num_dimensions,
                                     dimension_type, 0, get_gph_from_geopotential, &conversion) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, "geopotential", harp_type_double, HARP_UNIT_GEOPOTENTIAL,
                                            num_dimensions, dimension_type, 0) != 0)
    {
        return -1;
    }

    if (!has_latlon)
    {
        /* gph from altitude */
        if (harp_variable_conversion_new("geopotential_height", harp_type_double, HARP_UNIT_LENGTH, num_dimensions,
                                         dimension_type, 0, get_gph_from_altitude_and_latitude, &conversion) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source
            (conversion, "altitude", harp_type_double, HARP_UNIT_LENGTH, num_dimensions, dimension_type, 0) != 0)
        {
            return -1;
        }
        if (harp_variable_conversion_add_source(conversion, "latitude", harp_type_double, HARP_UNIT_LATITUDE,
                                                num_dimensions, dimension_type, 0) != 0)
        {
            return -1;
        }
    }

    /*** pressure ***/

    /* time dependent from independent */
    if (add_time_indepedent_to_dependent_conversion("pressure", harp_type_double, HARP_UNIT_PRESSURE, num_dimensions,
                                                    dimension_type, 0) != 0)
    {
        return -1;
    }

    /* uncertainties */
    if (add_uncertainty_conversions("pressure", HARP_UNIT_PRESSURE, num_dimensions, dimension_type) != 0)
    {
        return -1;
    }

    /*** pressure_bounds ***/

    /* time dependent from independent */
    dimension_type[num_dimensions] = harp_dimension_independent;
    if (add_time_indepedent_to_dependent_conversion("pressure", harp_type_double, HARP_UNIT_PRESSURE,
                                                    num_dimensions + 1, dimension_type, 2) != 0)
    {
        return -1;
    }

    /*** relative humidity ***/

    /* time dependent from independent */
    if (add_time_indepedent_to_dependent_conversion("relative_humidity", harp_type_double, HARP_UNIT_DIMENSIONLESS,
                                                    num_dimensions, dimension_type, 0) != 0)
    {
        return -1;
    }

    /* uncertainties */
    if (add_uncertainty_conversions("relative_humidity", HARP_UNIT_DIMENSIONLESS, num_dimensions, dimension_type) != 0)
    {
        return -1;
    }

    /* humidity from H2O/T */
    if (harp_variable_conversion_new("relative_humidity", harp_type_double, HARP_UNIT_DIMENSIONLESS, num_dimensions,
                                     dimension_type, 0, get_relative_humidity_from_h2o_nd_and_temperature,
                                     &conversion) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, "H2O_number_density", harp_type_double,
                                            HARP_UNIT_NUMBER_DENSITY, num_dimensions, dimension_type, 0) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, "temperature", harp_type_double, HARP_UNIT_TEMPERATURE,
                                            num_dimensions, dimension_type, 0) != 0)
    {
        return -1;
    }

    /*** temperature ***/

    /* time dependent from independent */
    if (add_time_indepedent_to_dependent_conversion("temperature", harp_type_double, HARP_UNIT_TEMPERATURE,
                                                    num_dimensions, dimension_type, 0) != 0)
    {
        return -1;
    }

    /* uncertainties */
    if (add_uncertainty_conversions("temperature", HARP_UNIT_TEMPERATURE, num_dimensions, dimension_type) != 0)
    {
        return -1;
    }

    /*** virtual temperature ***/

    /* time dependent from independent */
    if (add_time_indepedent_to_dependent_conversion("virtual_temperature", harp_type_double, HARP_UNIT_TEMPERATURE,
                                                    num_dimensions, dimension_type, 0) != 0)
    {
        return -1;
    }

    /* uncertainties */
    if (add_uncertainty_conversions("virtual_temperature", HARP_UNIT_TEMPERATURE, num_dimensions, dimension_type) != 0)
    {
        return -1;
    }

    /* virtual temp from p/T/humidity */
    if (harp_variable_conversion_new("virtual_temperature", harp_type_double, HARP_UNIT_TEMPERATURE, num_dimensions,
                                     dimension_type, 0,
                                     get_virtual_temperature_from_pressure_temperature_and_relative_humidity,
                                     &conversion) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, "pressure", harp_type_double, HARP_UNIT_PRESSURE,
                                            num_dimensions, dimension_type, 0) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, "temperature", harp_type_double, HARP_UNIT_TEMPERATURE,
                                            num_dimensions, dimension_type, 0) != 0)
    {
        return -1;
    }
    if (harp_variable_conversion_add_source(conversion, "relative_humidity", harp_type_double, HARP_UNIT_DIMENSIONLESS,
                                            num_dimensions, dimension_type, 0) != 0)
    {
        return -1;
    }

    return 0;
}

/* grid conversions are for variables that can have a latitude, longitude, and/or vertical dimension */
static int add_grid_conversions(void)
{
    harp_dimension_type dimension_type[HARP_MAX_NUM_DIMS];

    /* {} */
    if (add_conversions_for_grid(0, dimension_type, 0) != 0)
    {
        return -1;
    }

    /* {vertical} */
    dimension_type[0] = harp_dimension_vertical;
    if (add_conversions_for_grid(1, dimension_type, 0) != 0)
    {
        return -1;
    }

    /* {latitude,longitude} */
    dimension_type[0] = harp_dimension_latitude;
    dimension_type[1] = harp_dimension_longitude;
    if (add_conversions_for_grid(2, dimension_type, 1) != 0)
    {
        return -1;
    }

    /* {latitude,longitude,vertical} */
    dimension_type[2] = harp_dimension_vertical;
    if (add_conversions_for_grid(3, dimension_type, 1) != 0)
    {
        return -1;
    }

    /* {time} */
    dimension_type[0] = harp_dimension_time;
    if (add_conversions_for_grid(1, dimension_type, 0) != 0)
    {
        return -1;
    }

    /* {time,vertical} */
    dimension_type[1] = harp_dimension_vertical;
    if (add_conversions_for_grid(2, dimension_type, 0) != 0)
    {
        return -1;
    }

    /* {time,latitude,longitude} */
    dimension_type[1] = harp_dimension_latitude;
    dimension_type[2] = harp_dimension_longitude;
    if (add_conversions_for_grid(3, dimension_type, 1) != 0)
    {
        return -1;
    }

    /* {time,latitude,longitude,vertical} */
    dimension_type[3] = harp_dimension_vertical;
    if (add_conversions_for_grid(4, dimension_type, 1) != 0)
    {
        return -1;
    }

    return 0;
}

static int add_radiance_conversions(void)
{
    harp_variable_conversion *conversion;
    harp_dimension_type dimension_type[HARP_MAX_NUM_DIMS];
    int i;

    dimension_type[0] = harp_dimension_time;
    dimension_type[1] = harp_dimension_spectral;

    /*** normalized radiance ***/

    dimension_type[1] = harp_dimension_spectral;
    for (i = 1; i < 3; i++)
    {
        if (add_uncertainty_conversions("normalized_radiance", HARP_UNIT_DIMENSIONLESS, i, dimension_type) != 0)
        {
            return -1;
        }

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

    /*** radiance ***/

    dimension_type[1] = harp_dimension_spectral;
    for (i = 1; i < 3; i++)
    {
        if (add_uncertainty_conversions("radiance", HARP_UNIT_RADIANCE, i, dimension_type) != 0)
        {
            return -1;
        }

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

    /*** reflectance ***/

    dimension_type[1] = harp_dimension_spectral;
    for (i = 1; i < 3; i++)
    {
        if (add_uncertainty_conversions("reflectance", HARP_UNIT_DIMENSIONLESS, i, dimension_type) != 0)
        {
            return -1;
        }

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

    /*** solar irradiance ***/

    dimension_type[1] = harp_dimension_spectral;
    for (i = 1; i < 3; i++)
    {
        if (add_uncertainty_conversions("solar_irradiance", HARP_UNIT_IRRADIANCE, i, dimension_type) != 0)
        {
            return -1;
        }
    }

    return 0;
}

static int add_angle_conversions(void)
{
    harp_variable_conversion *conversion;
    harp_dimension_type dimension_type[HARP_MAX_NUM_DIMS];
    int i;

    dimension_type[0] = harp_dimension_time;

    /*** scattering angle ***/

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

    /*** solar elevation angle ***/

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

    /*** solar zenith angle ***/

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

    /*** viewing azimuth angle ***/

    if (add_time_indepedent_to_dependent_conversion("viewing_azimuth_angle", harp_type_double, HARP_UNIT_ANGLE, 1,
                                                    dimension_type, 0) != 0)
    {
        return -1;
    }

    /*** viewing zenith angle ***/

    if (add_time_indepedent_to_dependent_conversion("viewing_zenith_angle", harp_type_double, HARP_UNIT_ANGLE, 1,
                                                    dimension_type, 0) != 0)
    {
        return -1;
    }

    return 0;
}

static int add_axis_conversions(void)
{
    harp_variable_conversion *conversion;
    harp_dimension_type dimension_type[HARP_MAX_NUM_DIMS];
    int i;

    dimension_type[0] = harp_dimension_time;

    /*** datetime ***/

    if (add_time_indepedent_to_dependent_conversion("datetime", harp_type_double, HARP_UNIT_DATETIME, 1,
                                                    dimension_type, 0) != 0)
    {
        return -1;
    }

    /* midpoint from start/top */
    if (harp_variable_conversion_new("datetime", harp_type_double, HARP_UNIT_DATETIME, 1, dimension_type, 0,
                                     get_midpoint_from_begin_and_end, &conversion) != 0)
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

    /*** datetime_length ***/

    if (add_time_indepedent_to_dependent_conversion("datetime_length", harp_type_double, HARP_UNIT_TIME, 1,
                                                    dimension_type, 0) != 0)
    {
        return -1;
    }

    /* length from start/top */
    if (harp_variable_conversion_new("datetime_length", harp_type_double, HARP_UNIT_TIME, 1, dimension_type, 0,
                                     get_length_from_begin_and_end, &conversion) != 0)
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

    /*** datetime_start ***/

    if (add_time_indepedent_to_dependent_conversion("datetime_start", harp_type_double, HARP_UNIT_DATETIME, 1,
                                                    dimension_type, 0) != 0)
    {
        return -1;
    }

    /* start from mid/length */
    if (harp_variable_conversion_new("datetime_start", harp_type_double, HARP_UNIT_DATETIME, 1, dimension_type, 0,
                                     get_begin_from_midpoint_and_length, &conversion) != 0)
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

    /*** datetime_stop ***/

    if (add_time_indepedent_to_dependent_conversion("datetime_stop", harp_type_double, HARP_UNIT_DATETIME, 1,
                                                    dimension_type, 0) != 0)
    {
        return -1;
    }

    /* stop from start/length */
    if (harp_variable_conversion_new("datetime_stop", harp_type_double, HARP_UNIT_DATETIME, 1, dimension_type, 0,
                                     get_end_from_begin_and_length, &conversion) != 0)
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

    /*** latitude ***/

    /* TODO: add conversion to {[time,]lat,lon[,vertical]} */

    if (add_time_indepedent_to_dependent_conversion("latitude", harp_type_double, HARP_UNIT_LATITUDE, 1,
                                                    dimension_type, 0) != 0)
    {
        return -1;
    }

    /* midpoint from polygon */
    if (add_latlon_bounds_to_midpoint_conversion("latitude", harp_type_double, HARP_UNIT_LATITUDE,
                                                 get_latitude_from_latlon_bounds) != 0)
    {
        return -1;
    }

    /* midpoint from range */
    if (add_bounds_to_midpoint_conversion("latitude", harp_type_double, HARP_UNIT_LATITUDE, harp_dimension_latitude,
                                          get_midpoint_from_bounds) != 0)
    {
        return -1;
    }

    /* latitude from instrument latitude */
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


    /*** latitude_bounds ***/

    if (add_time_indepedent_to_dependent_conversion("latitude_bounds", harp_type_double, HARP_UNIT_DATETIME, 1,
                                                    dimension_type, 0) != 0)
    {
        return -1;
    }

    /* range from midpoints */
    if (add_midpoint_to_bounds_conversion("latitude", harp_type_double, HARP_UNIT_LATITUDE, harp_dimension_latitude,
                                          get_bounds_from_midpoints) != 0)
    {
        return -1;
    }

    /*** longitude ***/

    /* TODO: add conversion to {[time,]lat,lon[,vertical]} */

    if (add_time_indepedent_to_dependent_conversion("longitude", harp_type_double, HARP_UNIT_LONGITUDE, 1,
                                                    dimension_type, 0) != 0)
    {
        return -1;
    }

    /* midpoint from polygon */
    if (add_latlon_bounds_to_midpoint_conversion("longitude", harp_type_double, HARP_UNIT_LONGITUDE,
                                                 get_longitude_from_latlon_bounds) != 0)
    {
        return -1;
    }

    /* midpoint from range */
    if (add_bounds_to_midpoint_conversion("longitude", harp_type_double, HARP_UNIT_LONGITUDE, harp_dimension_longitude,
                                          get_midpoint_from_bounds) != 0)
    {
        return -1;
    }

    /* longitude from instrument longitude */
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

    /*** longitude_bounds ***/

    if (add_time_indepedent_to_dependent_conversion("longitude_bounds", harp_type_double, HARP_UNIT_LONGITUDE, 1,
                                                    dimension_type, 0) != 0)
    {
        return -1;
    }

    /* range from midpoints */
    if (add_midpoint_to_bounds_conversion("longitude", harp_type_double, HARP_UNIT_LONGITUDE, harp_dimension_longitude,
                                          get_bounds_from_midpoints) != 0)
    {
        return -1;
    }

    /*** altitude ***/

    dimension_type[1] = harp_dimension_vertical;

    /* TODO: add conversion from {[time,]vertical} to {[time,],lat,lon,vertical} */

    /* midpoint from bounds */
    if (add_bounds_to_midpoint_conversion("altitude", harp_type_double, HARP_UNIT_LENGTH, harp_dimension_vertical,
                                          get_midpoint_from_bounds) != 0)
    {
        return -1;
    }

    /* altitude from instrument altitude */
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

    /*** altitude_bounds ***/

    /* range from midpoints */
    if (add_midpoint_to_bounds_conversion("altitude", harp_type_double, HARP_UNIT_LENGTH, harp_dimension_vertical,
                                          get_bounds_from_midpoints) != 0)
    {
        return -1;
    }

    /*** pressure ***/

    /* TODO: add conversion from {[time,]vertical} to {[time,],lat,lon,vertical} */

    /* midpoint from bounds */
    if (add_bounds_to_midpoint_conversion("pressure", harp_type_double, HARP_UNIT_PRESSURE, harp_dimension_vertical,
                                          get_midpoint_from_bounds_log) != 0)
    {
        return -1;
    }

    /* pressure from alt/T/Q */
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

    /* pressure from gph/T/Q */
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

    /* pressure from alt/T */
    if (harp_variable_conversion_new("pressure", harp_type_double, HARP_UNIT_PRESSURE, 2, dimension_type, 0,
                                     get_pressure_from_altitude_temperature_and_latitude, &conversion) != 0)
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
    if (harp_variable_conversion_add_source(conversion, "latitude", harp_type_double, HARP_UNIT_LATITUDE, 1,
                                            dimension_type, 0) != 0)
    {
        return -1;
    }

    /* pressure from gph/T */
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

    /*** pressure_bounds ***/

    /* range from midpoints */
    if (add_midpoint_to_bounds_conversion("pressure", harp_type_double, HARP_UNIT_PRESSURE, harp_dimension_vertical,
                                          get_bounds_from_midpoints_log) != 0)
    {
        return -1;
    }


    /*** frequency ***/

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

        /* frequency from wavelength */
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

        /* frequency from wavenumber */
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

    /*** wavelength ***/

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

        /* wavelength from frequency */
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

        /* wavelength from wavenumber */
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

    /*** wavenumber ***/

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

        /* wavenumber from frequency */
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

        /* wavenumber from wavelength */
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

static int add_misc_conversions(void)
{
    harp_variable_conversion *conversion;
    harp_dimension_type dimension_type[HARP_MAX_NUM_DIMS];
    int i;

    dimension_type[0] = harp_dimension_time;

    /*** index ***/

    if (harp_variable_conversion_new("index", harp_type_int32, NULL, 1, dimension_type, 0, get_index, &conversion) != 0)
    {
        return -1;
    }

    /*** flag_am_pm ***/

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

    /*** flag_day_twilight_night ***/

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

    /*** flag_daytime ***/

    if (add_time_indepedent_to_dependent_conversion("flag_daytime", harp_type_string, NULL, 1, dimension_type, 0) != 0)
    {
        return -1;
    }
    for (i = 0; i < 2; i++)
    {
        if (harp_variable_conversion_new("flag_daytime", harp_type_string, NULL, i, dimension_type, 0,
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

    /*** instrument_altitude ***/

    if (add_time_indepedent_to_dependent_conversion("instrument_altitude", harp_type_double, HARP_UNIT_LENGTH, 1,
                                                    dimension_type, 0) != 0)
    {
        return -1;
    }

    /*** instrument_latitude ***/

    if (add_time_indepedent_to_dependent_conversion("instrument_latitude", harp_type_double, HARP_UNIT_LATITUDE, 1,
                                                    dimension_type, 0) != 0)
    {
        return -1;
    }

    /*** instrument_longitude ***/

    if (add_time_indepedent_to_dependent_conversion("instrument_longitude", harp_type_double, HARP_UNIT_LONGITUDE, 1,
                                                    dimension_type, 0) != 0)
    {
        return -1;
    }

    return 0;
}

static int init_conversions(void)
{
    if (add_axis_conversions() != 0)
    {
        return -1;
    }

    if (add_angle_conversions() != 0)
    {
        return -1;
    }

    if (add_radiance_conversions() != 0)
    {
        return -1;
    }

    if (add_grid_conversions() != 0)
    {
        return -1;
    }

    if (add_radiance_conversions() != 0)
    {
        return -1;
    }

    if (add_model_conversions() != 0)
    {
        return -1;
    }

    if (add_misc_conversions() != 0)
    {
        return -1;
    }

    return 0;
}

static int compare_conversion_lists(const void *a, const void *b)
{
    harp_variable_conversion *conv_a = (*(harp_variable_conversion_list **)a)->conversion[0];
    harp_variable_conversion *conv_b = (*(harp_variable_conversion_list **)b)->conversion[0];
    int result;

    /* first compare based on the actual variable name of the first conversion */
    result = strcmp(conv_a->variable_name, conv_b->variable_name);
    if (result == 0)
    {
        /* if variable names are equal, compare based on the dimensions (using the dimsvar name) */
        result = strcmp(conv_a->dimsvar_name, conv_b->dimsvar_name);
    }

    return result;
}

int harp_derived_variable_list_sort(void)
{
    long i;

    qsort(harp_derived_variable_conversions->conversions_for_variable,
          harp_derived_variable_conversions->num_variables, sizeof(harp_variable_conversion_list *),
          compare_conversion_lists);

    /* recreate the hash table for the new ordering */
    hashtable_delete(harp_derived_variable_conversions->hash_data);
    harp_derived_variable_conversions->hash_data = hashtable_new(1);
    if (harp_derived_variable_conversions->hash_data == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not create hashtable) (%s:%u)", __FILE__,
                       __LINE__);
        return -1;
    }
    for (i = 0; i < harp_derived_variable_conversions->num_variables; i++)
    {
        hashtable_add_name(harp_derived_variable_conversions->hash_data,
                           harp_derived_variable_conversions->conversions_for_variable[i]->conversion[0]->dimsvar_name);
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

    harp_derived_variable_list_sort();

    return 0;
}

int harp_derived_variable_list_add_conversion(harp_variable_conversion *conversion)
{
    harp_variable_conversion_list *conversion_list;
    int index;

    index = hashtable_get_index_from_name(harp_derived_variable_conversions->hash_data, conversion->dimsvar_name);
    if (index < 0)
    {
        /* no conversions for this variable name+dims exists -> create new conversion list */
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

        hashtable_add_name(harp_derived_variable_conversions->hash_data, conversion->dimsvar_name);

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

    conversion_list->conversion[conversion_list->num_conversions] = conversion;
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
