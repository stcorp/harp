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

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/** \addtogroup harp_algorithm
 * @{
 */

/** Construct an altitude boundaries profile from an altitude profile
 * \param num_levels Number of levels
 * \param altitude_profile Altitude vertical profile
 * \param altitude_bounds_profile variable in which the altitude boundaries profile with dimensions
 *        [num_levels, 2] will be stored
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
int harp_profile_altitude_bounds_from_altitude(long num_levels, const double *altitude_profile,
                                               double *altitude_bounds_profile)
{
    long k;

    if (num_levels < 2)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "num_levels should be >= 2 (%s:%u)", __FILE__, __LINE__);
        return -1;
    }
    if (altitude_profile == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "altitude profile is empty (%s:%u)", __FILE__, __LINE__);
        return -1;
    }
    if (altitude_bounds_profile == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "altitude boundaries profile is empty (%s:%u)", __FILE__, __LINE__);
        return -1;
    }

    /* set lower boundary for [0] */
    altitude_bounds_profile[0] = altitude_profile[0] - 0.5 * fabs(altitude_profile[1] - altitude_profile[0]);
    for (k = 0; k < num_levels - 1; k++)
    {
        double average = 0.5 * (altitude_profile[k] + altitude_profile[k + 1]);

        /* set upper boundary for [k] */
        altitude_bounds_profile[2 * k + 1] = average;
        /* set lower boundary for [k + 1] */
        altitude_bounds_profile[2 * (k + 1)] = average;
    }
    /* set upper boundary for [n-1] */
    altitude_bounds_profile[2 * (num_levels - 1) + 1] = altitude_profile[num_levels - 1] +
        0.5 * fabs(altitude_profile[num_levels - 1] - altitude_profile[num_levels - 2]);

    /* make sure our lower altitude does not become negative (unless the lower altitude was already negative) and
     * our upper altitude does not exceed the top of the atmosphere (unless the upper altitude was already higher) */
    if (altitude_profile[0] < altitude_profile[num_levels - 1])
    {
        /* ascending */
        if (altitude_bounds_profile[0] < 0 && altitude_profile[0] >= 0)
        {
            altitude_bounds_profile[0] = 0;
        }
        if (altitude_bounds_profile[2 * num_levels - 1] > CONST_TOA_ALTITUDE &&
            altitude_profile[num_levels - 1] < CONST_TOA_ALTITUDE)
        {
            altitude_bounds_profile[2 * num_levels - 1] = CONST_TOA_ALTITUDE;
        }
    }
    else
    {
        /* descending */
        if (altitude_bounds_profile[2 * num_levels - 1] < 0 && altitude_profile[num_levels - 1] >= 0)
        {
            altitude_bounds_profile[2 * num_levels - 1] = 0;
        }
        if (altitude_bounds_profile[0] > CONST_TOA_ALTITUDE && altitude_profile[0] < CONST_TOA_ALTITUDE)
        {
            altitude_bounds_profile[0] = CONST_TOA_ALTITUDE;
        }
    }

    return 0;
}

/** Convert geopotential height to geometric height (= altitude)
 * \param gph  Geopotential height [m]
 * \param latitude   Latitude [degree_north]
 * \return the altitude [m]
 */
double harp_altitude_from_gph_and_latitude(double gph, double latitude)
{
    double altitude;
    double g0 = (double)CONST_GRAV_ACCEL_45LAT_WGS84_SPHERE;    /* gravitational accel. [m s-2] at latitude 45o32'33'' */
    double gsurf;       /* gravitational acceleration at surface [m s-2] */
    double Rsurf;       /* local curvature radius [m] */

    gsurf = harp_gravity_at_surface_from_latitude(latitude);
    Rsurf = harp_local_curvature_radius_at_surface_from_latitude(latitude);

    altitude = g0 * Rsurf * gph / (gsurf * Rsurf - g0 * gph);
    return altitude;
}

/** Convert a pressure profile to an altitude profile
 * If the h2o_mmr_profile variable is set to NULL a constant mean molar mass for wet air will be used for the conversion
 * (instead of a calculated molar mass of humid air).
 * If the temperature_profile variable is set to NULL the standard temperature will be used for the conversion.
 * \param num_levels Length of vertical axis
 * \param pressure_profile Pressure vertical profile [hPa]
 * \param temperature_profile Temperature vertical profile [K]
 * \param h2o_mmr_profile Humidity (q) vertical profile [ug/g] (optional)
 * \param surface_pressure Surface pressure [hPa]
 * \param surface_height Surface height [m]
 * \param latitude Latitude [degree_north]
 * \param altitude_profile variable in which the vertical profile will be stored [m]
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
void harp_profile_altitude_from_pressure_temperature_h2o_mmr_and_latitude(long num_levels,
                                                                          const double *pressure_profile,
                                                                          const double *temperature_profile,
                                                                          const double *h2o_mmr_profile,
                                                                          double surface_pressure,
                                                                          double surface_height, double latitude,
                                                                          double *altitude_profile)
{
    double z, prev_z = 0, p, prev_p = 0, T, prev_T = 0, molar_mass_air, prev_molar_mass_air = 0;
    long i;

    surface_height *= 1.0e-3;   /* convert from [m] to [km] */

    /* convert pressure to altitude, using humidity and temperature information */
    for (i = 0; i < num_levels; i++)
    {
        long k = i;

        if (pressure_profile[0] < pressure_profile[num_levels - 1])
        {
            /* vertical axis is from TOA to surface -> invert the loop index */
            k = num_levels - 1 - i;
        }

        p = pressure_profile[k];
        if (temperature_profile != NULL)
        {
            T = temperature_profile[k];
        }
        else
        {
            T = CONST_STD_TEMPERATURE;
        }

        if (h2o_mmr_profile != NULL)
        {
            /* determine molar mass of humid air */
            molar_mass_air = harp_molar_mass_for_wet_air(h2o_mmr_profile[k]);
        }
        else
        {
            /* use mean molar mass of wet air */
            molar_mass_air = CONST_MEAN_MOLAR_MASS_WET_AIR;
        }

        if (i == 0)
        {
            z = surface_height +
                ((T * CONST_MOLAR_GAS) / (molar_mass_air * harp_gravity_at_surface_from_latitude(latitude))) *
                log(surface_pressure / p);
        }
        else
        {
            z = prev_z + ((prev_T + T) / (molar_mass_air + prev_molar_mass_air)) *
                (CONST_MOLAR_GAS / harp_gravity_at_surface_from_latitude_and_height(latitude, prev_z)) *
                log(prev_p / p);
        }

        altitude_profile[k] = z * 1.0e3;        /* convert from [km] to [m] */

        prev_p = p;
        prev_molar_mass_air = molar_mass_air;
        prev_T = T;
        prev_z = z;
    }
}

/** Convert geopotential height to geopotential
 * \param gph Geopotential height [m]
 * \return the geopotential [m2/s2]
 */
double harp_geopotential_from_gph(double gph)
{
    double g0 = (double)CONST_GRAV_ACCEL_45LAT_WGS84_SPHERE;    /* gravitation accel. [m s-2] at latitude 45o32'33'' */

    return g0 * gph;
}

/** Convert geopotential to geopotential height
 * \param geopotential Geopotential [m2/s2]
 * \return the geopotential height [m]
 */
double harp_gph_from_geopotential(double geopotential)
{
    double g0 = (double)CONST_GRAV_ACCEL_45LAT_WGS84_SPHERE;    /* gravitation accel. [m s-2] at latitude 45o32'33'' */

    return geopotential / g0;
}

/** Convert geometric height (= altitude) to geopotential height
 * \param altitude  Altitude [m]
 * \param latitude   Latitude [degree_north]
 * \return the geopotential height [m]
 */
double harp_gph_from_altitude_and_latitude(double altitude, double latitude)
{
    double gph;
    double g0 = (double)CONST_GRAV_ACCEL_45LAT_WGS84_SPHERE;    /* gravitation accel. [m s-2] at latitude 45o32'33'' */
    double gsurf;       /* gravitational acceleration at surface [m] */
    double Rsurf;       /* local curvature radius [m] */

    gsurf = harp_gravity_at_surface_from_latitude(latitude);
    Rsurf = harp_local_curvature_radius_at_surface_from_latitude(latitude);

    gph = gsurf / g0 * Rsurf * altitude / (altitude + Rsurf);
    return gph;
}

/** Convert a pressure value to a geopotential height value using model values
 * This is a rather inaccurate way of calculating the geopotential height, so only use it when you can't use
 * any of the other approaches.
 * \param pressure Pressure value to be converted [hPa]
 * \return geopotential height [m]
 */
double harp_gph_from_pressure(double pressure)
{
    /* use a very simple approach using constant values for most of the needed quantities */
    return ((CONST_STD_TEMPERATURE * CONST_MOLAR_GAS) /
            (CONST_MEAN_MOLAR_MASS_WET_AIR * CONST_GRAV_ACCEL_45LAT_WGS84_SPHERE)) *
        log(CONST_STD_PRESSURE / pressure) * 1.0e3;
}

/** Convert a pressure profile to a geopotential height profile
 * If the h2o_mmr_profile variable is set to NULL a constant mean molar mass for wet air will be used for the conversion
 * (instead of a calculated molar mass of humid air).
 * If the temperature_profile variable is set to NULL the standard temperature will be used for the conversion.
 * \param num_levels Length of vertical axis
 * \param pressure_profile Pressure vertical profile [hPa]
 * \param temperature_profile Temperature vertical profile [K]
 * \param h2o_mmr_profile Humidity (q) vertical profile [ug/g] (optional)
 * \param surface_pressure Surface pressure [hPa]
 * \param surface_height Surface height [m]
 * \param gph_profile Variable in which the vertical profile will be stored [m]
 */
void harp_profile_gph_from_pressure_temperature_and_h2o_mmr(long num_levels, const double *pressure_profile,
                                                            const double *temperature_profile,
                                                            const double *h2o_mmr_profile, double surface_pressure,
                                                            double surface_height, double *gph_profile)
{
    double z, prev_z = 0, p, prev_p = 0, T, prev_T = 0, molar_mass_air, prev_molar_mass_air = 0;
    long i;

    surface_height *= 1.0e-3;   /* convert from [m] to [km] */

    /* convert pressure to geopotential height, using humidity and temperature information */
    for (i = 0; i < num_levels; i++)
    {
        long k = i;

        if (pressure_profile[0] < pressure_profile[num_levels - 1])
        {
            /* vertical axis is from TOA to surface -> invert the loop index */
            k = num_levels - 1 - i;
        }

        p = pressure_profile[k];
        if (temperature_profile != NULL)
        {
            T = temperature_profile[k];
        }
        else
        {
            T = CONST_STD_TEMPERATURE;
        }

        if (h2o_mmr_profile != NULL)
        {
            /* determine molar mass of humid air */
            molar_mass_air = harp_molar_mass_for_wet_air(h2o_mmr_profile[k]);
        }
        else
        {
            /* use mean molar mass of humid air */
            molar_mass_air = CONST_MEAN_MOLAR_MASS_WET_AIR;
        }

        if (i == 0)
        {
            z = surface_height + ((T * CONST_MOLAR_GAS) / (molar_mass_air * CONST_GRAV_ACCEL_45LAT_WGS84_SPHERE)) *
                log(surface_pressure / p);
        }
        else
        {
            z = prev_z + ((prev_T + T) / (molar_mass_air + prev_molar_mass_air)) *
                (CONST_MOLAR_GAS / CONST_GRAV_ACCEL_45LAT_WGS84_SPHERE) * log(prev_p / p);
        }

        gph_profile[k] = z * 1.0e3;     /* convert from [km] to [m] */

        prev_p = p;
        prev_molar_mass_air = molar_mass_air;
        prev_T = T;
        prev_z = z;
    }
}

/** Integrate the partial column profile to obtain the column
 * \param num_levels              Number of levels of the partial column profile
 * \param partial_column_profile  Partial column profile [molec/m2]
 * \return column the integrated column [molec/m2]
 */
double harp_profile_column_from_partial_column(long num_levels, const double *partial_column_profile)
{
    double column = 0;
    int empty = 1;
    long k;

    /* Integrate, but ignore NaN values */
    for (k = 0; k < num_levels; k++)
    {
        if (!harp_isnan(partial_column_profile[k]))
        {
            column += partial_column_profile[k];
            empty = 0;
        }
    }

    /* Set column to NaN if all contributions were NaN */
    if (empty)
    {
        return harp_nan();
    }

    return column;
}

/** Integrate the partial column uncertainty profile to obtain the column uncertainty
 * \param num_levels              Number of levels of the partial column profile
 * \param partial_column_uncertainty_profile  Partial column profile [molec/m2]
 * \return the integrated column uncertainty [molec/m2]
 */
double harp_profile_column_uncertainty_from_partial_column_uncertainty(long num_levels,
                                                                       const double *partial_column_uncertainty_profile)
{
    double column_uncertainty = 0;
    long empty = 1;
    long k;

    /* Sum uncertainties quadratically, ignore NaN values */
    for (k = 0; k < num_levels; k++)
    {
        if (!harp_isnan(partial_column_uncertainty_profile[k]))
        {
            column_uncertainty += partial_column_uncertainty_profile[k] * partial_column_uncertainty_profile[k];
            empty = 0;
        }
    }

    /* Set column to NaN if all contributions were NaN */
    if (empty)
    {
        return harp_nan();
    }

    return sqrt(column_uncertainty);
}

/** Convert a volume mixing ratio covariance matrix to a number density covariance matrix
 * \param num_levels Number of levels of the profile
 * \param volume_mixing_ratio_covariance_matrix  volume mixing ratio covariance [(ppmv)^2]
 * \param pressure_profile  Pressure [hPa]
 * \param temperature_profile  Temperature [K]
 * \param number_density_covariance_matrix variable in which the number density covariance [(molec/m3)^2] will be stored
 */
void harp_profile_nd_cov_from_vmr_cov_pressure_and_temperature(long num_levels,
                                                               const double *volume_mixing_ratio_covariance_matrix,
                                                               const double *pressure_profile,
                                                               const double *temperature_profile,
                                                               double *number_density_covariance_matrix)
{
    long i, j;

    for (i = 0; i < num_levels; i++)
    {
        /* Convert [ppmv] to [1] */
        double ci = 1e-6 * CONST_STD_AIR_DENSITY *
            (CONST_STD_TEMPERATURE / temperature_profile[i]) * (pressure_profile[i] / CONST_STD_PRESSURE);

        for (j = 0; j < num_levels; j++)
        {
            /* Convert [ppmv] to [1] */
            double cj = 1e-6 * CONST_STD_AIR_DENSITY *
                (CONST_STD_TEMPERATURE / temperature_profile[j]) * (pressure_profile[j] / CONST_STD_PRESSURE);

            number_density_covariance_matrix[i * num_levels + j] =
                ci * cj * volume_mixing_ratio_covariance_matrix[i * num_levels + j];
        }
    }
}

/** Convert a density uncertainty profile to a partial column covariance matrix using the altitude boundaries as provided
 * \param num_levels Number of levels of the profile
 * \param altitude_boundaries Lower and upper altitude [m] boundaries for each level [num_levels,2]
 * \param density_covariance_matrix Density covariance  [(?/m)^2]
 * \param partial_column_covariance_matrix variable in which the partial column covariance matrix [(?)^2] will be stored
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
int harp_profile_partial_column_cov_from_density_cov_and_altitude_bounds(long num_levels,
                                                                         const double *altitude_boundaries,
                                                                         const double *density_covariance_matrix,
                                                                         double *partial_column_covariance_matrix)
{
    long i, j;

    if (altitude_boundaries == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "altitude boundaries is empty (%s:%u)", __FILE__, __LINE__);
        return -1;
    }
    if (density_covariance_matrix == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "density covariance matrix is empty (%s:%u)", __FILE__, __LINE__);
        return -1;
    }
    if (partial_column_covariance_matrix == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "partial column covariance matrix is empty (%s:%u)", __FILE__,
                       __LINE__);
        return -1;
    }

    for (i = 0; i < num_levels; i++)
    {
        double dzi = fabs(altitude_boundaries[i * 2 + 1] - altitude_boundaries[i * 2]);

        for (j = 0; j < num_levels; j++)
        {
            double dzj = fabs(altitude_boundaries[j * 2 + 1] - altitude_boundaries[j * 2]);

            partial_column_covariance_matrix[i * num_levels + j] =
                density_covariance_matrix[i * num_levels + j] * dzi * dzj;
        }
    }

    return 0;
}

/** Regrid the density profile to obtain the partial column profile, using interval interpolation
 * \param source_num_levels              Number of levels of the source altitude profile
 * \param source_altitude_boundaries     Lower and upper boundaries [source_num_levels,2] of source altitude layers [m]
 * \param source_density_profile         Density profile [?/m3] on source altitude grid
 * \param target_num_levels              Number of levels of the target partial column profile
 * \param target_altitude_boundaries     Lower and upper boundaries [target_num_levels,2] of target altitude layers [m]
 * \param target_partial_column_profile  The partial column profile [?/m2]
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
int harp_partial_column_profile_regridded_from_density_profile_and_altitude_boundaries
    (long source_num_levels, const double *source_altitude_boundaries,
     const double *source_density_profile,
     long target_num_levels, const double *target_altitude_boundaries, double *target_partial_column_profile)
{
    long j, k;
    long count_valid_contributions = 0;

    if (source_altitude_boundaries == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "source altitude boundaries is empty (%s:%u)", __FILE__, __LINE__);
        return -1;
    }
    if (source_density_profile == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "source density profile is empty (%s:%u)", __FILE__, __LINE__);
        return -1;
    }
    if (target_altitude_boundaries == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "target altitude boundaries is empty (%s:%u)", __FILE__, __LINE__);
        return -1;
    }
    if (target_partial_column_profile == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "target partial column profile is empty (%s:%u)", __FILE__,
                       __LINE__);
        return -1;
    }

    /* Prepare the output vector */
    for (j = 0; j < target_num_levels; j++)
    {
        target_partial_column_profile[j] = harp_nan();
    }

    /* Check for input number density profiles with only NaNs */
    for (k = 0; k < source_num_levels; k++)
    {
        if (!harp_isnan(source_density_profile[k]))
        {
            count_valid_contributions++;
        }
    }

    if (count_valid_contributions != 0)
    {
        double *source_profile;
        double dz;

        /* Prepare the input vector */
        source_profile = calloc((size_t)source_num_levels, sizeof(double));
        if (source_profile == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY,
                           "out of memory (could not allocate %lu bytes) (%s:%u)",
                           source_num_levels * sizeof(double), __FILE__, __LINE__);
            return -1;
        }
        for (k = 0; k < source_num_levels; k++)
        {
            /* Layer thickness [m] */
            dz = fabs(source_altitude_boundaries[k * 2 + 1] - source_altitude_boundaries[k * 2]);

            if (harp_isnan(source_density_profile[k]))
            {
                source_profile[k] = 0.0;        /* Do not add NaN values */
            }
            else
            {
                source_profile[k] = source_density_profile[k] * dz;
            }
        }

        if (harp_interval_interpolate_array_linear(source_num_levels, source_altitude_boundaries,
                                                   source_profile, target_num_levels,
                                                   target_altitude_boundaries, target_partial_column_profile) != 0)
        {
            free(source_profile);
            return -1;
        }

        free(source_profile);
    }

    return 0;
}

/** Regrid the density profile covariance matrix to obtain the partial column profile covariance matrix,
 * using interval interpolation
 * \param source_num_levels              Number of levels of the source altitude profile
 * \param source_altitude_boundaries     Lower and upper boundaries [source_num_levels,2] of source altitude layers [m]
 * \param source_density_covariance_matrix  Density profile covariance matrix [(?/m3)^2] on source altitude grid
 * \param target_num_levels              Number of levels of the target partial column profile
 * \param target_altitude_boundaries     Lower and upper boundaries [target_num_levels,2] of target altitude layers [m]
 * \param target_partial_column_covariance_matrix  The partial column profile covariance matrix [(?/m2)^2]
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
int harp_partial_column_covariance_matrix_regridded_from_density_covariance_matrix_and_altitude_boundaries
    (long source_num_levels, const double *source_altitude_boundaries,
     const double *source_density_covariance_matrix,
     long target_num_levels, const double *target_altitude_boundaries, double *target_partial_column_covariance_matrix)
{
    double *transformation_matrix = NULL;
    double *temp_matrix = NULL;
    long i, j, k;

    if (source_altitude_boundaries == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "source altitude boundaries is empty (%s:%u)", __FILE__, __LINE__);
        return -1;
    }
    if (source_density_covariance_matrix == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "source density covariance matrix is empty (%s:%u)", __FILE__,
                       __LINE__);
        return -1;
    }
    if (target_altitude_boundaries == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "target altitude boundaries is empty (%s:%u)", __FILE__, __LINE__);
        return -1;
    }
    if (target_partial_column_covariance_matrix == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "target partial column covariance matrix is empty (%s:%u)",
                       __FILE__, __LINE__);
        return -1;
    }

    /* Prepare the output matrix */
    for (j = 0; j < target_num_levels; j++)
    {
        for (k = 0; k < target_num_levels; k++)
        {
            target_partial_column_covariance_matrix[j * target_num_levels + k] = harp_nan();
        }
    }

    /* Derive the matrix D with interpolation weights */
    transformation_matrix = calloc(target_num_levels * source_num_levels, sizeof(double));
    if (transformation_matrix == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       target_num_levels * source_num_levels * sizeof(double), __FILE__, __LINE__);
        return -1;
    }
    for (i = 0; i < target_num_levels; i++)
    {
        for (j = 0; j < source_num_levels; j++)
        {
            double xmina = source_altitude_boundaries[2 * j];
            double xmaxa = source_altitude_boundaries[2 * j + 1];
            double xminb = target_altitude_boundaries[2 * i];
            double xmaxb = target_altitude_boundaries[2 * i + 1];
            harp_overlapping_scenario overlapping_scenario;
            double weight = 0.0;

            if (harp_determine_overlapping_scenario(xmina, xmaxa, xminb, xmaxb, &overlapping_scenario) != 0)
            {
                free(transformation_matrix);
                return -1;
            }

            switch (overlapping_scenario)
            {
                case harp_overlapping_scenario_no_overlap_b_a:
                case harp_overlapping_scenario_no_overlap_a_b:
                    weight = 0.0;
                    break;
                case harp_overlapping_scenario_overlap_a_equals_b:
                    weight = 1.0;
                    break;
                case harp_overlapping_scenario_partial_overlap_a_b:
                    weight = (xmaxa - xminb) / (xmaxa - xmina);
                    break;
                case harp_overlapping_scenario_partial_overlap_b_a:
                    weight = (xmaxb - xmina) / (xmaxa - xmina);
                    break;
                case harp_overlapping_scenario_overlap_a_contains_b:
                    weight = (xmaxb - xminb) / (xmaxa - xmina);
                    break;
                case harp_overlapping_scenario_overlap_b_contains_a:
                    weight = 1.0;
                    break;
            }

            transformation_matrix[i * source_num_levels + j] = weight;
        }
    }

    /* Calculate temporary matrix, C * D^T, with dimensions [source_num_levels, target_num_levels] */
    temp_matrix = calloc(source_num_levels * target_num_levels, sizeof(double));
    if (temp_matrix == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       source_num_levels * target_num_levels * sizeof(double), __FILE__, __LINE__);
        free(transformation_matrix);
        return -1;
    }
    for (i = 0; i < source_num_levels; i++)
    {
        for (j = 0; j < target_num_levels; j++)
        {
            double dzi = fabs(target_altitude_boundaries[2 * i + 1] - target_altitude_boundaries[2 * i]);
            double dzj = fabs(source_altitude_boundaries[2 * j + 1] - source_altitude_boundaries[2 * j]);

            temp_matrix[i * target_num_levels + j] = 0.0;
            for (k = 0; k < source_num_levels; k++)
            {
                assert(i * source_num_levels + k >= 0 &&
                       i * source_num_levels + k < source_num_levels * source_num_levels);
                assert(j * source_num_levels + k >= 0 &&
                       j * source_num_levels + k < target_num_levels * source_num_levels);
                temp_matrix[i * target_num_levels + j] +=
                    source_density_covariance_matrix[i * source_num_levels +
                                                     k] * dzi * dzj * transformation_matrix[j * source_num_levels + k];
            }
        }
    }

    /* Calculate regridded covariance matrix, D * C * D^T, with dimensions [target_num_levels * target_num_levels] */
    for (i = 0; i < target_num_levels; i++)
    {
        for (j = 0; j < target_num_levels; j++)
        {
            target_partial_column_covariance_matrix[i * target_num_levels + j] = 0.0;
            for (k = 0; k < source_num_levels; k++)
            {
                assert(i * target_num_levels + j >= 0 &&
                       i * target_num_levels + j < target_num_levels * target_num_levels);
                assert(i * source_num_levels + k >= 0 &&
                       i * source_num_levels + k < source_num_levels * source_num_levels);
                assert(k * target_num_levels + j >= 0 &&
                       k * target_num_levels + j < target_num_levels * source_num_levels);
                target_partial_column_covariance_matrix[i * target_num_levels + j] +=
                    transformation_matrix[i * source_num_levels + k] * temp_matrix[k * target_num_levels + j];
            }
        }
    }

    free(temp_matrix);
    free(transformation_matrix);

    return 0;
}

/** Convert an altitude profile to a pressure profile
 * If the h2o_mmr_profile variable is set to NULL a constant mean molar mass for wet air will be used for the conversion
 * (instead of a calculated molar mass of humid air).
 * If the temperature_profile variable is set to NULL the standard temperature will be used for the conversion.
 * \param num_levels Length of vertical axis
 * \param altitude_profile Altitude profile [m]
 * \param temperature_profile Temperature vertical profile [K]
 * \param h2o_mmr_profile Humidity (q) vertical profile [ug/g] (optional)
 * \param surface_pressure Surface pressure [hPa]
 * \param surface_height Surface height [m]
 * \param latitude Latitude [degree_north]
 * \param pressure_profile variable in which the vertical profile will be stored [hPa]
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
int harp_profile_pressure_from_altitude_temperature_h2o_mmr_and_latitude(long num_levels,
                                                                         const double *altitude_profile,
                                                                         const double *temperature_profile,
                                                                         const double *h2o_mmr_profile,
                                                                         double surface_pressure, double surface_height,
                                                                         double latitude, double *pressure_profile)
{
    double z, prev_z = 0, p, prev_p = 0, T, prev_T = 0, molar_mass_air, prev_molar_mass_air = 0, g, prev_g = 0;
    long i;

    if (altitude_profile == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "altitude profile is empty (%s:%u)", __FILE__, __LINE__);
        return -1;
    }
    if (pressure_profile == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "pressure profile is empty (%s:%u)", __FILE__, __LINE__);
        return -1;
    }

    /* convert altitude to pressure, using humidity and temperature information */
    for (i = 0; i < num_levels; i++)
    {
        long k = i;

        if (altitude_profile[0] > altitude_profile[num_levels - 1])
        {
            /* vertical axis is from TOA to surface -> invert the loop index */
            k = num_levels - 1 - i;
        }

        z = altitude_profile[k];
        if (temperature_profile != NULL)
        {
            T = temperature_profile[k];
        }
        else
        {
            T = CONST_STD_TEMPERATURE;
        }

        if (h2o_mmr_profile != NULL)
        {
            /* determine molar mass of humid air */
            molar_mass_air = harp_molar_mass_for_wet_air(h2o_mmr_profile[k]);
        }
        else
        {
            /* use mean molar mass of wet air */
            molar_mass_air = CONST_MEAN_MOLAR_MASS_WET_AIR;
        }

        g = harp_gravity_at_surface_from_latitude_and_height(latitude, z);
        if (i == 0)
        {
            prev_g = harp_gravity_at_surface_from_latitude(latitude);
            p = surface_pressure * exp(-((g + prev_g) * molar_mass_air * 1e-3 * (z - surface_height)) /
                                       (2 * T * CONST_MOLAR_GAS));
        }
        else
        {
            p = prev_p * exp(-((g + prev_g) * (molar_mass_air + prev_molar_mass_air) * 1e-3 * (z - prev_z)) /
                             (2 * (T + prev_T) * CONST_MOLAR_GAS));
        }

        pressure_profile[k] = p;

        prev_g = g;
        prev_p = p;
        prev_molar_mass_air = molar_mass_air;
        prev_T = T;
        prev_z = z;
    }

    return 0;
}

/** Convert a geopotential height profile to a pressure profile
 * If the h2o_mmr_profile variable is set to NULL a constant mean molar mass for wet air will be used for the conversion
 * (instead of a calculated molar mass of humid air).
 * If the temperature_profile variable is set to NULL the standard temperature will be used for the conversion.
 * \param num_levels Length of vertical axis
 * \param gph_profile Geopotential height profile [m]
 * \param temperature_profile Temperature vertical profile [K]
 * \param h2o_mmr_profile Humidity (q) vertical profile [ug/g] (optional)
 * \param surface_pressure Surface pressure [hPa]
 * \param surface_height Surface height [m]
 * \param pressure_profile Variable in which the vertical profile will be stored [hPa]
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
int harp_profile_pressure_from_gph_temperature_and_h2o_mmr(long num_levels, const double *gph_profile,
                                                           const double *temperature_profile,
                                                           const double *h2o_mmr_profile, double surface_pressure,
                                                           double surface_height, double *pressure_profile)
{
    double z, prev_z = 0, p, prev_p = 0, T, prev_T = 0, molar_mass_air, prev_molar_mass_air = 0;
    long i;

    if (gph_profile == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "altitude GPH profile is empty (%s:%u)", __FILE__, __LINE__);
        return -1;
    }
    if (pressure_profile == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "pressure profile is empty (%s:%u)", __FILE__, __LINE__);
        return -1;
    }

    /* convert geopotential height to pressure, using humidity and temperature information */
    for (i = 0; i < num_levels; i++)
    {
        long k = i;

        if (gph_profile[0] > gph_profile[num_levels - 1])
        {
            /* vertical axis is from TOA to surface -> invert the loop index */
            k = num_levels - 1 - i;
        }

        z = gph_profile[k];
        if (temperature_profile != NULL)
        {
            T = temperature_profile[k];
        }
        else
        {
            T = CONST_STD_TEMPERATURE;
        }

        if (h2o_mmr_profile != NULL)
        {
            /* determine molar mass of humid air */
            molar_mass_air = harp_molar_mass_for_wet_air(h2o_mmr_profile[k]);
        }
        else
        {
            /* use mean molar mass of humid air */
            molar_mass_air = CONST_MEAN_MOLAR_MASS_WET_AIR;
        }

        if (i == 0)
        {
            p = surface_pressure *
                exp(-(CONST_GRAV_ACCEL_45LAT_WGS84_SPHERE * molar_mass_air * 1e-3 * (z - surface_height)) /
                    (T * CONST_MOLAR_GAS));
        }
        else
        {
            p = prev_p * exp(-(CONST_GRAV_ACCEL_45LAT_WGS84_SPHERE *
                               (molar_mass_air + prev_molar_mass_air) * 1e-3 * (z - prev_z)) /
                             ((T + prev_T) * CONST_MOLAR_GAS));
        }

        pressure_profile[k] = p;

        prev_p = p;
        prev_molar_mass_air = molar_mass_air;
        prev_T = T;
        prev_z = z;
    }

    return 0;
}

/** Convert a number density covariance matrix to a volume mixing ratio covariance matrix
 * \param num_levels Length of vertical axis
 * \param number_density_covariance_matrix  number density covariance [(molec/m3)^2]
 * \param pressure_profile  Pressure [hPa]
 * \param temperature_profile  Temperature [K]
 * \param volume_mixing_ratio_covariance_matrix variable in which the volume mixing ratio covariance [(ppmv)^2] will be stored
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
int harp_profile_vmr_cov_from_nd_cov_pressure_and_temperature(long num_levels,
                                                              const double *number_density_covariance_matrix,
                                                              const double *pressure_profile,
                                                              const double *temperature_profile,
                                                              double *volume_mixing_ratio_covariance_matrix)
{
    long i, j;

    if (number_density_covariance_matrix == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "number density covariance matrix is empty (%s:%u)", __FILE__,
                       __LINE__);
        return -1;
    }
    if (pressure_profile == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "pressure profile is empty (%s:%u)", __FILE__, __LINE__);
        return -1;
    }
    if (temperature_profile == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "temperature profile is empty (%s:%u)", __FILE__, __LINE__);
        return -1;
    }
    if (volume_mixing_ratio_covariance_matrix == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "volume mixing ratio covariance matrix is empty (%s:%u)", __FILE__,
                       __LINE__);
        return -1;
    }

    for (i = 0; i < num_levels; i++)
    {
        /* Convert [ppmv] to [1] */
        double ci = (1.0e6 / CONST_STD_AIR_DENSITY) * (temperature_profile[i] / CONST_STD_TEMPERATURE) *
            (CONST_STD_PRESSURE / pressure_profile[i]);

        for (j = 0; j < num_levels; j++)
        {
            /* Convert [ppmv] to [1] */
            double cj = (1.0e6 / CONST_STD_AIR_DENSITY) * (temperature_profile[j] / CONST_STD_TEMPERATURE) *
                (CONST_STD_PRESSURE / pressure_profile[j]);

            volume_mixing_ratio_covariance_matrix[i * num_levels + j] =
                ci * cj * number_density_covariance_matrix[i * num_levels + j];
        }
    }

    return 0;
}

/**
 * Resamples all variables in product against a specified grid.
 * Target_grid is expected to be a variable of dimensions {vertical} or {time, vertical}.
 * The source grid is determined by derivation of a matching vertical quantity on the specified product.
 *
 * Grid-dimension-wise we distinguish 4 cases:
 *  - target 1D, source 1D: performs time independent vertical interpolation
 *  - target 1D, source 2D: extends target dimensions & performs time dep. interpolation
 *  - target 2D, source 1D: extends source dimensions & performs time dep. interpolation
 *  - target 2D, source 2D: performs time dependent interpolation
 *
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
int harp_profile_resample(harp_product *product, harp_variable *target_grid)
{
    harp_variable *source_grid = NULL;
    harp_variable *vertical_axis = NULL;
    long target_vertical_elements = target_grid->dimension[target_grid->num_dimensions - 1];
    long source_time_dim_length = 0;    /* 0 indicates that we do time-independent regridding */
    long source_vertical_elements;
    int i;

    harp_dimension_type vertical_1d_dim_type[1] = { harp_dimension_vertical };
    harp_dimension_type vertical_2d_dim_type[2] = { harp_dimension_time, harp_dimension_vertical };

    /* Derive the source grid (will give doubles because unit is passed) */
    if (harp_product_add_derived_variable(product, target_grid->name, target_grid->unit, 1, vertical_1d_dim_type) != 0)
    {
        /* Failed to derive 1D source grid. Try 2D */
        if (harp_product_add_derived_variable(product,
                                              target_grid->name, target_grid->unit, 2, vertical_2d_dim_type) != 0)
        {
            return -1;
        }

    }

    /* Retrieve basic info about the source grid */
    harp_product_get_variable_by_name(product, target_grid->name, &source_grid);
    if(source_grid->num_dimensions > 1)
    {
        source_time_dim_length = source_grid->dimension[0];
    }
    source_vertical_elements = source_grid->dimension[source_grid->num_dimensions - 1];

    /* Resample all variables if we know how */
    for (i = product->num_variables - 1; i >= 0; i--)
    {
        harp_variable *variable = product->variable[i];

        long new_data_num_elements = variable->num_elements / source_vertical_elements * target_vertical_elements;
        double *new_data = NULL;
        long num_blocks = variable->num_elements / source_vertical_elements;
        long time_blocks = num_blocks;

        int dim_id;
        long time;
        long block_id;
        int skip;       /* <0: don't skipt, 0: skip, >0: delete */

        /* Calculate the number of num_blocks for which time is constant for time-dependent resampling */
        if (source_time_dim_length != 0)
        {
            time_blocks = num_blocks / source_time_dim_length;
        }

        /* Ensure that there is only 1 vertical dimension, that it's the fastest running one and has scalar values */
        skip = 0;       /* assume that the variable has no vertical component */
        for (dim_id = 0; dim_id < variable->num_dimensions; dim_id++)
        {
            if (variable->dimension_type[dim_id] == harp_dimension_vertical)
            {
                skip = -1;      /* variable has vertical component */
                if (dim_id != variable->num_dimensions - 1)
                {
                    /* variable has vertical dimension but cannot be resampled */
                    skip = 1;
                    break;
                }
            }
        }

        /* We can't resample string-typed variables */
        if (variable->data_type == harp_type_string)
        {
            skip = 1;
        }

        /* skip the source grid variable, we'll set that afterwards */
        if (variable == source_grid)
        {
            skip = 0;
        }

        /* TODO skip based on name restrictions */

        if (skip == 0)
        {
            continue;
        }
        else if (skip > 0)
        {
            harp_report_warning("Removing variable %s; unresamplable dimensions\n", variable->name);
            harp_product_remove_variable(product, variable);
            continue;
        }

        /* Ensure that the variable data consists of doubles */
        if (variable->data_type != harp_type_double && harp_variable_convert_data_type(variable, harp_type_double) != 0)
        {
            harp_variable_delete(target_grid);
            return -1;
        }

        /* time independent variables with a time-dependent source grid are time-extended */
        if (variable->dimension_type[0] != harp_dimension_time && source_grid->dimension[0] == harp_dimension_time)
        {
            harp_variable_add_dimension(variable, 0, harp_dimension_time, source_time_dim_length);
        }

        /* Setup target array */
        new_data = (double *)malloc((size_t)new_data_num_elements * sizeof(double));
        if (new_data == NULL)
        {
            harp_variable_delete(target_grid);
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)",
                           __FILE__, __LINE__);
            return -1;
        }

        /* Interpolate the data of the variable over the vertical axis */
        time = -1;
        for (block_id = 0; block_id < num_blocks; block_id++)
        {
            /* keep track of time for time-dependent vertical grids */
            if (block_id % time_blocks == 0)
            {
                time++;
            }

            harp_interpolate_array_linear(source_vertical_elements,
                                          source_grid->data.double_data + time * source_vertical_elements,
                                          variable->data.double_data + block_id * source_vertical_elements,
                                          target_vertical_elements, target_grid->data.double_data, 0,
                                          new_data + block_id * target_vertical_elements);
        }

        /* Update the vertical dimension length */
        variable->dimension[variable->num_dimensions - 1] = target_vertical_elements;

        /* Set the new variable data */
        harp_array old_data = variable->data;

        variable->data.double_data = new_data;
        variable->num_elements = new_data_num_elements;

        /* Clean up the old data */
        free(old_data.double_data);
    }

    /* ensure consistent axis variable in product */
    product->dimension[harp_dimension_vertical] = target_vertical_elements;
    harp_variable_copy(target_grid, &vertical_axis);
    if (harp_product_replace_variable(product, vertical_axis) != 0)
    {
        return -1;
    }

    return 0;
}

/** Iterates over the product metadata of all the products in column b of the collocation result and
 * determines the maximum vertical dimension size.
 */
static int get_maximum_vertical_dimension(harp_collocation_result *collocation_result, long *max_vertical)
{
    int i;
    long max = 0;

    for (i = 0; i < collocation_result->num_pairs; i++)
    {
        harp_collocation_pair *pair = collocation_result->pair[i];
        long matching_product_index = pair->product_index_b;
        harp_product_metadata *match_metadata = collocation_result->dataset_b->metadata[matching_product_index];

        if (!match_metadata)
        {
            /* TODO error type */
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "Metadata unavailable for match pair product %s.",
                           collocation_result->dataset_b->source_product[matching_product_index]);
            return -1;
        }

        long match_vertical_dim_size = match_metadata->dimension[harp_dimension_vertical];

        if (match_vertical_dim_size > max)
        {
            max = match_vertical_dim_size;
        }
    }

    *max_vertical = max;

    return 0;
}

/**
 * Smooth the product (from dataset a in the collocation result) using the avks in dataset b.
 *
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
int harp_profile_smooth(harp_product *product, harp_collocation_result *collocation_result, const char *dataset_b_dir)
{
    int i; +    harp_variable *collocation_indices = NULL;
    long max_vertical_dim;

    /* Get the source product's collocation index variable */
    for (i = product->num_variables - 1; i >= 0; i--)
    {
        harp_variable *variable = product->variable[i];

        if (variable->num_dimensions == 1 && variable->dimension_type[0] == harp_dimension_time &&
                variable->name != NULL && strcmp(variable->name, "collocation_index") == 0)
        {
            collocation_indices = variable;
        }
    }

    if (!collocation_indices)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT,
                       "Smoothing requires a source product with a collocation index variable (%s:%u)",
                       __FILE__, __LINE__);
        return -1;
    }

    /* Prepare the collocation result for efficient iteration over the pairs */
    harp_collocation_result_filter_for_source_product_a(collocation_result, product->source_product);
    harp_collocation_result_sort_by_a(collocation_result);
    harp_collocation_result_sort_by_collocation_index(collocation_result);*/

    for (i = product->num_variables - 1; i >= 0; i--)
    {
        harp_variable *variable = product->variable[i];
        long time_id;

        /* TODO Ensure that this variable has a time dimension */
        /* Iterate over the time dimension */
        for (time_id = 0; time_id < variable->dimension[0]; time_id++)
        {
        }

    }

    return 0;
}

/**
 * @}
 */
