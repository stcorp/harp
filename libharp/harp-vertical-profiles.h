/*
 * Copyright (C) 2015-2021 S[&]T, The Netherlands.
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

#ifndef HARP_VERTICAL_PROFILES_H
#define HARP_VERTICAL_PROFILES_H

#include "harp-internal.h"

/* Conversions */
double harp_geopotential_from_gph(double gph);
double harp_gph_from_geopotential(double geopotential);
double harp_altitude_from_gph_and_latitude(double gph, double latitude);
double harp_gph_from_altitude_and_latitude(double altitude, double latitude);
double harp_gph_from_pressure(double pressure);

double harp_column_mass_density_from_surface_pressure_and_profile(double surface_pressure, long num_levels,
                                                                  const double *pressure_bounds,
                                                                  const double *altitude_profile, double latitude);

long harp_tropopause_index_from_altitude_and_temperature(long num_levels, const double *altitude_profile,
                                                         const double *pressure_profile,
                                                         const double *temperature_profile);

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
double harp_profile_tropo_column_from_partial_column_and_altitude(long num_levels, const double *partial_column_profile,
                                                                  const double *altitude_bounds,
                                                                  double tropopause_altitude);
double harp_profile_strato_column_from_partial_column_and_altitude(long num_levels,
                                                                   const double *partial_column_profile,
                                                                   const double *altitude_bounds,
                                                                   double tropopause_altitude);
double harp_profile_tropo_column_from_partial_column_and_pressure(long num_levels, const double *partial_column_profile,
                                                                  const double *pressure_bounds,
                                                                  double tropopause_pressure);
double harp_profile_strato_column_from_partial_column_and_pressure(long num_levels,
                                                                   const double *partial_column_profile,
                                                                   const double *pressure_bounds,
                                                                   double tropopause_pressure);

/* AVK conversions */
void harp_profile_column_avk_from_partial_column_avk(long num_levels, const double *column_density_avk_2d,
                                                     double *column_density_avk_1d);
void harp_profile_tropospheric_column_avk_from_column_avk(long num_levels, const double *column_density_avk,
                                                          const double *altitude_bounds, double tropopause_altitude,
                                                          double *tropospheric_column_density_avk);
void harp_profile_stratospheric_column_avk_from_column_avk(long num_levels, const double *column_density_avk,
                                                           const double *altitude_bounds, double tropopause_altitude,
                                                           double *stratospheric_column_density_avk);
void harp_density_avk_from_partial_column_avk_and_altitude_bounds(long num_levels, const double *partial_column_avk,
                                                                  const double *altitude_bounds, double *density_avk);
void harp_partial_column_avk_from_density_avk_and_altitude_bounds(long num_levels, const double *density_avk,
                                                                  const double *altitude_bounds,
                                                                  double *partial_column_avk);
void harp_number_density_avk_from_volume_mixing_ratio_avk(long num_levels, const double *volume_mixing_ratio_avk,
                                                          const double *number_density_air, double *number_density_avk);
void harp_volume_mixing_ratio_avk_from_number_density_avk(long num_levels, const double *number_density_avk,
                                                          const double *number_density_air,
                                                          double *volume_mixing_ratio_avk);

#endif
