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

typedef enum vertical_profile_variable_type_enum
{
    vertical_profile_variable_skip,
    vertical_profile_variable_remove,
    vertical_profile_variable_resample
} vertical_profile_variable_type;

/* Conversions */
double harp_geopotential_from_gph(double gph);
double harp_gph_from_geopotential(double geopotential);
double harp_altitude_from_gph_and_latitude(double gph, double latitude);
double harp_gph_from_altitude_and_latitude(double altitude, double latitude);
double harp_gph_from_pressure(double pressure);

int harp_profile_altitude_bounds_from_altitude(long num_levels, const double *altitude_profile,
                                               double *altitude_bounds_profile);

void harp_profile_altitude_from_pressure_temperature_h2o_mmr_and_latitude(long num_levels,
                                                                          const double *pressure_profile,
                                                                          const double *temperature_profile,
                                                                          const double *h2o_mmr_profile,
                                                                          double surface_pressure,
                                                                          double surface_height, double latitude,
                                                                          double *altitude_profile);
void harp_profile_gph_from_pressure_temperature_and_h2o_mmr(long num_levels, const double *pressure_profile,
                                                            const double *temperature_profile,
                                                            const double *h2o_mmr_profile, double surface_pressure,
                                                            double surface_height, double *gph_profile);

int harp_profile_pressure_from_altitude_temperature_h2o_mmr_and_latitude(long num_levels,
                                                                         const double *altitude_profile,
                                                                         const double *temperature_profile,
                                                                         const double *h2o_mmr_profile,
                                                                         double surface_pressure, double surface_height,
                                                                         double latitude, double *pressure_profile);

int harp_profile_pressure_from_gph_temperature_and_h2o_mmr(long num_levels, const double *gph_profile,
                                                           const double *temperature_profile,
                                                           const double *h2o_mmr_profile, double surface_pressure,
                                                           double surface_height, double *pressure_profile);
int harp_profile_vmr_cov_from_nd_cov_pressure_and_temperature(long num_levels,
                                                              const double *number_density_covariance_matrix,
                                                              const double *pressure_profile,
                                                              const double *temperature_profile,
                                                              double *volume_mixing_ratio_covariance_matrix);

void harp_profile_nd_cov_from_vmr_cov_pressure_and_temperature
    (long num_levels, const double *volume_mixing_ratio_covariance_matrix, const double *pressure_profile,
     const double *temperature_profile, double *number_density_covariance_matrix);

int harp_profile_partial_column_cov_from_density_cov_and_altitude_bounds(long num_levels,
                                                                         const double *altitude_boundaries,
                                                                         const double *density_covariance_matrix,
                                                                         double *partial_column_covariance_matrix);


int harp_partial_column_profile_regridded_from_density_profile_and_altitude_boundaries
    (long source_num_levels, const double *source_altitude_boundaries,
     const double *source_number_density_profile,
     long target_num_levels, const double *target_altitude_boundaries, double *target_partial_column_profile);
int harp_partial_column_covariance_matrix_regridded_from_density_covariance_matrix_and_altitude_boundaries
    (long source_num_levels, const double *source_altitude_boundaries,
     const double *source_density_covariance_matrix,
     long target_num_levels, const double *target_altitude_boundaries, double *target_partial_column_covariance_matrix);


double harp_profile_column_from_partial_column(long num_levels, const double *partial_column_profile);
double harp_profile_column_uncertainty_from_partial_column_uncertainty
    (long num_levels, const double *partial_column_uncertainty_profile);

int harp_profile_resample(harp_product *product, harp_variable *target_grid);
int harp_profile_resample_and_smooth_a_to_b(harp_product *product, harp_collocation_result *collocation_result,
                                            const char *dataset_b_dir, const char *vertical_axis,
                                            const char *vertical_unit, int smooth);

#endif
