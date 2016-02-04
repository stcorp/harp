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

#ifndef HARP_INTERNAL_H
#define HARP_INTERNAL_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define HARP_INTERNAL

#include "harp.h"
#include "harp-chemistry.h"
#include "harp-dimension-mask.h"
#include "harp-units.h"
#include "harp-vertical-profiles.h"

#include "coda.h"

#include <stdarg.h>

/* This defines the amount of items that will be allocated per block for an auto-growing array (using realloc) */
#define BLOCK_SIZE 16

/* Maximum number of source variables that can be used to create derived variables */
#define MAX_NUM_SOURCE_VARIABLES 10

/* floating point precission */
#define EPSILON 1e-10

extern int harp_option_enable_aux_afgl86;
extern int harp_option_enable_aux_usstd76;

typedef int (*harp_conversion_function) (harp_variable *variable, const harp_variable **source_variable);
typedef int (*harp_conversion_enabled_function) (void);

typedef struct harp_source_variable_struct
{
    char *variable_name;
    harp_data_type data_type;
    char *unit;
    int num_dimensions;
    harp_dimension_type dimension_type[HARP_MAX_NUM_DIMS];
    long independent_dimension_length;
} harp_source_variable_definition;

typedef struct harp_variable_conversion_struct
{
    char *variable_name;
    harp_data_type data_type;
    char *unit;
    int num_dimensions;
    harp_dimension_type dimension_type[HARP_MAX_NUM_DIMS];
    long independent_dimension_length;
    int num_source_variables;
    harp_source_variable_definition *source_definition;
    char *source_description;
    harp_conversion_function set_variable_data;
    harp_conversion_enabled_function enabled;
} harp_variable_conversion;

typedef struct harp_variable_conversion_list_struct
{
    int num_conversions;
    harp_variable_conversion **conversion;
} harp_variable_conversion_list;

typedef struct harp_derived_variable_list_struct
{
    int num_variables;
    struct hashtable_struct *hash_data;
    harp_variable_conversion_list **conversions_for_variable;
} harp_derived_variable_list;

extern harp_derived_variable_list *harp_derived_variable_conversions;

typedef enum harp_overlapping_scenario_enum
{
    harp_overlapping_scenario_no_overlap_b_a = 0,
    harp_overlapping_scenario_no_overlap_a_b = 1,
    harp_overlapping_scenario_overlap_a_equals_b = 2,
    harp_overlapping_scenario_partial_overlap_a_b = 3,
    harp_overlapping_scenario_partial_overlap_b_a = 4,
    harp_overlapping_scenario_overlap_a_contains_b = 5,
    harp_overlapping_scenario_overlap_b_contains_a = 6
} harp_overlapping_scenario;

/* Utility functions */
long harp_parse_double(const char *buffer, long buffer_length, double *dst, int ignore_trailing_bytes);
long harp_get_max_string_length(long num_strings, char **string_data);
int harp_get_char_array_from_string_array(long num_strings, char **string_data, long min_string_length,
                                          long *string_length, char **char_data);
long harp_get_num_elements(int num_dimensions, const long *dimension);
void harp_array_null(harp_data_type data_type, long num_elements, harp_array data);
void harp_array_replace_fill_value(harp_data_type data_type, long num_elements, harp_array data,
                                   harp_scalar fill_value);
int harp_array_invert(harp_data_type data_type, int dim_id, int num_dimensions, const long *dimension, harp_array data);
int harp_array_transpose(harp_data_type data_type, int num_dimensions, const long *dimension, const int *order,
                         harp_array data);
harp_scalar harp_get_fill_value_for_type(harp_data_type data_type);
harp_scalar harp_get_valid_min_for_type(harp_data_type data_type);
harp_scalar harp_get_valid_max_for_type(harp_data_type data_type);
int harp_is_fill_value_for_type(harp_data_type data_type, harp_scalar value);
int harp_is_valid_min_for_type(harp_data_type data_type, harp_scalar valid_min);
int harp_is_valid_max_for_type(harp_data_type data_type, harp_scalar valid_max);
const char *harp_basename(const char *path);
void harp_remove_extension(char *path);
void harp_strip_path_from_filename(const char *path_plus_filename, const char **filename);
void harp_strip_extension_from_filename(char *filename);
void harp_strip_descriptor_from_variable_name(char *variable_name);

/* Auxiliary data sources */
int harp_aux_afgl86_get_profile(const char *name, double datetime, double latitude, int *num_vertical,
                                const double **values);
int harp_aux_usstd76_get_profile(const char *name, int *num_vertical, const double **values);

/* Error messaging */
void harp_hdf4_add_error_message(void);
void harp_hdf5_add_error_message(void);
void harp_add_coda_cursor_path_to_error_message(const coda_cursor *cursor);

/* Variables */
int harp_variable_add_dimension(harp_variable *variable, int dim_index, harp_dimension_type dimension_type,
                                long length);
int harp_variable_rearrange_dimension(harp_variable *variable, int dim_index, long num_dim_elements,
                                      const long *dim_element_ids);
int harp_variable_filter_dimension(harp_variable *variable, int dim_index, const uint8_t *mask);
int harp_variable_resize_dimension(harp_variable *variable, int dim_index, long length);
int harp_variable_remove_dimension(harp_variable *variable, int dim_index, long index);

/* Products */
int harp_product_rearrange_dimension(harp_product *product, harp_dimension_type dimension_type, long num_dim_elements,
                                     const long *dim_element_ids);
int harp_product_filter_dimension(harp_product *product, harp_dimension_type dimension_type, const uint8_t *mask);
int harp_product_remove_dimension(harp_product *product, harp_dimension_type dimension_type);
void harp_product_remove_all_variables(harp_product *product);
int harp_product_get_datetime_range(const harp_product *product, double *datetime_start, double *datetime_stop);

/* Import */
int harp_import_hdf4(const char *filename, harp_product **product);
int harp_import_hdf5(const char *filename, harp_product **product);
int harp_import_netcdf(const char *filename, harp_product **product);
int harp_export_hdf4(const char *filename, const harp_product *product);
int harp_export_hdf5(const char *filename, const harp_product *product);
int harp_export_netcdf(const char *filename, const harp_product *product);
int harp_import_global_attributes_netcdf(const char *filename, double *datetime_start, double *datetime_stop,
                                         char **source_product);
int harp_parse_file_convention(const char *str, int *major, int *minor);

/* Units */
typedef struct harp_unit_converter_struct harp_unit_converter;
int harp_unit_converter_new(const char *from_unit, const char *to_unit, harp_unit_converter **new_unit_converter);
void harp_unit_converter_delete(harp_unit_converter *unit_converter);
double harp_unit_converter_convert(const harp_unit_converter *unit_converter, double value);
int harp_unit_compare(const char *unit_a, const char *unit_b);
int harp_unit_is_valid(const char *str);
void harp_unit_done(void);

/* Variable conversions */
int harp_variable_conversion_new(const char *variable_name, harp_data_type data_type, const char *unit,
                                 int num_dimensions, harp_dimension_type *dimension_type,
                                 long independent_dimension_length, harp_conversion_function get_variable,
                                 harp_variable_conversion **new_conversion);
int harp_variable_conversion_add_source(harp_variable_conversion *conversion, const char *variable_name,
                                        harp_data_type data_type, const char *unit, int num_dimensions,
                                        harp_dimension_type *dimension_type, long independent_dimension_length);
int harp_variable_conversion_set_enabled_function(harp_variable_conversion *conversion,
                                                  harp_conversion_enabled_function enabled);
int harp_variable_conversion_set_source_description(harp_variable_conversion *conversion, const char *description);
void harp_variable_conversion_delete(harp_variable_conversion *conversion);

/* Derived variables */
int harp_list_conversions(const harp_product *product);
int harp_derived_variable_list_init(void);
int harp_derived_variable_list_add_conversion(harp_variable_conversion *conversion);
void harp_derived_variable_list_done(void);

/* Analysis functions */
int harp_determine_overlapping_scenario(double xmin_a, double xmax_a,
                                        double xmin_b, double xmax_b,
                                        harp_overlapping_scenario *new_overlapping_scenario);
double harp_fraction_of_day_from_datetime(double datetime);
double harp_fraction_of_year_from_datetime(double datetime);
const char *harp_daytime_ampm_from_datetime_and_longitude(double datetime, double longitude);
int harp_daytime_from_solar_zenith_angle(double solar_zenith_angle);

double harp_frequency_from_wavelength(double wavelength);
double harp_frequency_from_wavenumber(double wavenumber);
double harp_gravity_at_surface_from_latitude(double latitude);
double harp_gravity_at_surface_from_latitude_and_height(double latitude, double height);
const char *harp_illumination_condition_from_solar_zenith_angle(double solar_zenith_angle);
double harp_local_curvature_radius_at_surface_from_latitude(double latitude);
double harp_normalized_radiance_from_radiance_and_solar_irradiance(double radiance, double solar_irradiance);
double harp_normalized_radiance_from_reflectance_and_solar_zenith_angle(double reflectance, double solar_zenith_angle);

double harp_ocean_frequency_from_ocean_period(double ocean_period);
double harp_ocean_frequency_from_ocean_wavelength(double ocean_wavelength);
double harp_ocean_frequency_from_ocean_wavenumber(double ocean_wavenumber);
double harp_ocean_period_from_ocean_frequency(double ocean_frequency);
double harp_ocean_period_from_ocean_wavelength(double ocean_wavelength);
double harp_ocean_period_from_ocean_wavenumber(double ocean_wavenumber);
double harp_ocean_wavelength_from_ocean_frequency(double ocean_frequency);
double harp_ocean_wavelength_from_ocean_period(double ocean_period);
double harp_ocean_wavelength_from_ocean_wavenumber(double ocean_wavenumber);
double harp_ocean_wavenumber_from_ocean_frequency(double ocean_frequency);
double harp_ocean_wavenumber_from_ocean_period(double ocean_period);
double harp_ocean_wavenumber_from_ocean_wavelength(double ocean_wavelength);

double harp_radiance_from_normalized_radiance_and_solar_irradiance(double normalized_radiance, double solar_irradiance);
double harp_radiance_from_reflectance_solar_irradiance_and_solar_zenith_angle(double reflectance,
                                                                              double solar_irradiance,
                                                                              double solar_zenith_angle);
double harp_reflectance_from_radiance_solar_irradiance_and_solar_zenith_angle(double radiance, double solar_irradiance,
                                                                              double solar_zenith_angle);
double harp_reflectance_from_normalized_radiance_and_solar_zenith_angle(double normalized_radiance,
                                                                        double solar_zenith_angle);
double harp_scattering_angle_from_solar_angles_and_viewing_angles(double sza, double saa, double vza, double vaa);
double harp_sea_surface_temperature_skin_from_subskin_wind_speed_and_solar_zenith_angle(double sst_skin,
                                                                                        double wind_speed,
                                                                                        double solar_zenith_angle);
double harp_sea_surface_temperature_subskin_from_skin_wind_speed_and_solar_zenith_angle(double sst_skin,
                                                                                        double wind_speed,
                                                                                        double solar_zenith_angle);
double harp_solar_azimuth_angle_from_datetime_longitude_and_latitude(double datetime, double longitude,
                                                                     double latitude);
double harp_solar_elevation_angle_from_datetime_longitude_and_latitude(double datetime, double longitude,
                                                                       double latitude);
double harp_elevation_angle_from_zenith_angle(double zenith_angle);
double harp_zenith_angle_from_elevation_angle(double elevation_angle);
int harp_viewing_geometry_angles_at_altitude_from_other_altitude(double source_altitude,
                                                                 double source_solar_zenith_angle,
                                                                 double source_viewing_zenith_angle,
                                                                 double source_relative_azimuth_angle,
                                                                 double target_altitude,
                                                                 double *new_target_solar_zenith_angle,
                                                                 double *new_target_viewing_zenith_angle,
                                                                 double *new_target_relative_azimuth_angle);
int harp_viewing_geometry_angle_profiles_from_viewing_geometry_angles(double altitude,
                                                                      double solar_zenith_angle,
                                                                      double viewing_zenith_angle,
                                                                      double relative_azimuth_angle,
                                                                      long num_levels,
                                                                      const double *altitude_profile,
                                                                      double *solar_zenith_angle_profile,
                                                                      double *viewing_zenith_angle_profile,
                                                                      double *relative_azimuth_angle_profile);
double harp_wavelength_from_frequency(double frequency);
double harp_wavelength_from_wavenumber(double wavenumber);
double harp_wavenumber_from_frequency(double frequency);
double harp_wavenumber_from_wavelength(double wavelength);

/* Interpolation */
void harp_interpolate_find_index(long source_length, const double *source_grid, double target_grid_point, long *index);
int harp_cubic_spline_interpolation(const double *xx, const double *yy, long n, const double xp, double *new_yp);
int harp_bilinear_interpolation(const double *xx, const double *yy, const double **zz, long m, long n,
                                double xp, double yp, double *new_zp);
int harp_bicubic_spline_interpolation(const double *xx, const double *yy, const double **zz, long m, long n,
                                      double xp, double yp, double *new_zp);
void harp_interpolate_value_linear(long source_length, const double *source_grid, const double *source_array,
                                   double target_grid_point, int out_of_bound_flag, double *target_value);
void harp_interpolate_array_linear(long source_length, const double *source_grid, const double *source_array,
                                   long target_length, const double *target_grid, int out_of_bound_flag,
                                   double *target_array);
void harp_interpolate_value_loglinear(long source_length, const double *source_grid, const double *source_array,
                                      double target_grid_point, int out_of_bound_flag, double *target_value);
void harp_interpolate_array_loglinear(long source_length, const double *source_grid, const double *source_array,
                                      long target_length, const double *target_grid, int out_of_bound_flag,
                                      double *target_array);
int harp_interval_interpolate_array_linear(long source_length, const double *source_grid_boundaries,
                                           const double *source_array, long target_length,
                                           const double *target_grid_boundaries, double *target_array);
#endif
