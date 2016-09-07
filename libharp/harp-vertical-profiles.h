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

#ifndef HARP_VERTICAL_PROFILES_H
#define HARP_VERTICAL_PROFILES_H

#include "harp-internal.h"

typedef enum profile_resample_type_enum
{
    profile_resample_skip,
    profile_resample_remove,
    profile_resample_linear,
    profile_resample_log,
    profile_resample_interval
} profile_resample_type;

/* Conversions */
double harp_geopotential_from_gph(double gph);
double harp_gph_from_geopotential(double geopotential);
double harp_altitude_from_gph_and_latitude(double gph, double latitude);
double harp_gph_from_altitude_and_latitude(double altitude, double latitude);
double harp_gph_from_pressure(double pressure);

void harp_profile_altitude_from_pressure(long num_levels, const double *pressure_profile,
                                         const double *temperature_profile, const double *molar_mass_air,
                                         double surface_pressure, double surface_height, double latitude,
                                         double *altitude_profile);
void harp_profile_gph_from_pressure(long num_levels, const double *pressure_profile, const double *temperature_profile,
                                    const double *molar_mass_air, double surface_pressure, double surface_height,
                                    double *gph_profile);
void harp_profile_pressure_from_altitude(long num_levels, const double *altitude_profile,
                                         const double *temperature_profile, const double *molar_mass_air,
                                         double surface_pressure, double surface_height, double latitude,
                                         double *pressure_profile);
void harp_profile_pressure_from_gph(long num_levels, const double *gph_profile, const double *temperature_profile,
                                    const double *molar_mass_air, double surface_pressure, double surface_height,
                                    double *pressure_profile);

double harp_profile_column_from_partial_column(long num_levels, const double *partial_column_profile);
double harp_profile_column_uncertainty_from_partial_column_uncertainty
    (long num_levels, const double *partial_column_uncertainty_profile);

int harp_profile_import_grid(const char *filename, harp_variable **new_vertical_axis);

#endif
