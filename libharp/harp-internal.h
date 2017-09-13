/*
 * Copyright (C) 2015-2017 S[&]T, The Netherlands.
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

#ifndef HARP_INTERNAL_H
#define HARP_INTERNAL_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define HARP_INTERNAL

#include "harp.h"
#include "harp-chemistry.h"
#include "harp-dimension-mask.h"
#include "harp-vertical-profiles.h"

#include "coda.h"

#include <stdarg.h>

/* make sure that math.h on Windows als includes the defines for e.g. M_PI */
#define _USE_MATH_DEFINES

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

typedef enum harp_collocation_filter_type_enum
{
    harp_collocation_left,
    harp_collocation_right
} harp_collocation_filter_type;

/* dimsvar_name is the variable name prefixed with HARP_MAX_NUM_DIMS characters defining the dimension types
 * dimsvar_name is thus the unique name for the combination of variable name + dimension types
 * the character code for a dimension type is: '0' + dimension_type, which gives:
 * '/' = indepent, '0' = time, '1' = latitude, '2' = longitude, '3' = vertical, '4' = spectral
 * unused dimensions use a space character.
 */
typedef struct harp_source_variable_struct
{
    char *dimsvar_name;
    const char *variable_name;
    harp_data_type data_type;
    char *unit;
    int num_dimensions;
    harp_dimension_type dimension_type[HARP_MAX_NUM_DIMS];
    long independent_dimension_length;
} harp_source_variable_definition;

typedef struct harp_variable_conversion_struct
{
    char *dimsvar_name;
    const char *variable_name;
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

/* Utility functions */
int harp_path_find_file(const char *searchpath, const char *filename, char **location);
int harp_path_from_path(const char *initialpath, int is_filepath, const char *appendpath, char **resultpath);
int harp_path_for_program(const char *argv0, char **location);
int harp_is_identifier(const char *name);
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

/* Auxiliary data sources */
int harp_aux_afgl86_get_profile(const char *name, double datetime, double latitude, int *num_vertical,
                                const double **values);
int harp_aux_usstd76_get_profile(const char *name, int *num_vertical, const double **values);

/* Error messaging */
#ifdef HAVE_HDF4
void harp_hdf4_add_error_message(void);
#endif
#ifdef HAVE_HDF5
void harp_hdf5_add_error_message(void);
#endif
void harp_add_coda_cursor_path_to_error_message(const coda_cursor *cursor);

/* Variables */
int harp_variable_get_flag_values_string(const harp_variable *variable, char **flag_values);
int harp_variable_get_flag_meanings_string(const harp_variable *variable, char **flag_meanings);
int harp_variable_set_enumeration_values_using_flag_meanings(harp_variable *variable, const char *flag_meanings);
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
int harp_product_filter_by_index(harp_product *product, const char *index_variable, long num_elements, int32_t *index);
int harp_product_filter_dimension(harp_product *product, harp_dimension_type dimension_type, const uint8_t *mask);
int harp_product_remove_dimension(harp_product *product, harp_dimension_type dimension_type);
void harp_product_remove_all_variables(harp_product *product);
int harp_product_get_datetime_range(const harp_product *product, double *datetime_start, double *datetime_stop);
int harp_product_get_derived_bounds_for_grid(harp_product *product, harp_variable *grid, harp_variable **bounds);
int harp_product_get_storage_size(const harp_product *product, int with_attributes, int64_t *size);
int harp_product_bin_with_collocated_dataset(harp_product *product, harp_collocation_result *collocation_result);
int harp_product_bin_with_variable(harp_product *product, const char *variable_name);

/* Import */
#ifdef HAVE_HDF4
int harp_import_hdf4(const char *filename, harp_product **product);
#endif
#ifdef HAVE_HDF5
int harp_import_hdf5(const char *filename, harp_product **product);
#endif
int harp_import_netcdf(const char *filename, harp_product **product);

#ifdef HAVE_HDF4
int harp_export_hdf4(const char *filename, const harp_product *product);
#endif
#ifdef HAVE_HDF5
int harp_export_hdf5(const char *filename, const harp_product *product);
#endif
int harp_export_netcdf(const char *filename, const harp_product *product);
#ifdef HAVE_HDF5
int harp_import_global_attributes_hdf5(const char *filename, double *datetime_start, double *datetime_stop,
                                       long dimension[], char **source_product);
#endif
int harp_import_global_attributes_netcdf(const char *filename, double *datetime_start, double *datetime_stop,
                                         long dimension[], char **source_product);
int harp_parse_file_convention(const char *str, int *major, int *minor);

/* Ingest */
int harp_ingest(const char *filename, const char *operations, const char *options, harp_product **product);
int harp_ingest_test(const char *filename, int (*print) (const char *, ...));
void harp_ingestion_done(void);

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
int harp_derived_variable_list_init(void);
int harp_derived_variable_list_add_conversion(harp_variable_conversion *conversion);
void harp_derived_variable_list_done(void);

/* Analysis functions */
double harp_fraction_of_day_from_datetime(double datetime);
double harp_fraction_of_year_from_datetime(double datetime);

double harp_frequency_from_wavelength(double wavelength);
double harp_frequency_from_wavenumber(double wavenumber);
double harp_gravity_at_surface_from_latitude(double latitude);
double harp_gravity_from_latitude_and_height(double latitude, double height);
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
double harp_scattering_angle_from_sensor_and_solar_angles(double sensor_zenith_angle, double solar_zenith_angle,
                                                          double relative_azimuth_angle);
double harp_sea_surface_temperature_skin_from_subskin_wind_speed_and_solar_zenith_angle(double sst_skin,
                                                                                        double wind_speed,
                                                                                        double solar_zenith_angle);
double harp_sea_surface_temperature_subskin_from_skin_wind_speed_and_solar_zenith_angle(double sst_skin,
                                                                                        double wind_speed,
                                                                                        double solar_zenith_angle);
void harp_solar_angles_from_datetime_latitude_and_longitude(double datetime, double latitude, double longitude,
                                                            double *solar_elevation_angle, double *solar_azimuth_angle);
double harp_relative_azimuth_angle_from_sensor_and_solar_azimuth_angles(double sensor_azimuth_angle,
                                                                        double solar_azimuth_angle);
double harp_elevation_angle_from_zenith_angle(double zenith_angle);
double harp_zenith_angle_from_elevation_angle(double elevation_angle);
double harp_sensor_angle_from_viewing_angle(double viewing_angle);
double harp_viewing_angle_from_sensor_angle(double sensor_angle);
int harp_sensor_geometry_angles_at_altitude_from_other_altitude(double source_altitude,
                                                                double source_solar_zenith_angle,
                                                                double source_sensor_zenith_angle,
                                                                double source_relative_azimuth_angle,
                                                                double target_altitude,
                                                                double *new_target_solar_zenith_angle,
                                                                double *new_target_sensor_zenith_angle,
                                                                double *new_target_relative_azimuth_angle);
int harp_sensor_geometry_angle_profiles_from_sensor_geometry_angles(double altitude,
                                                                    double solar_zenith_angle,
                                                                    double sensor_zenith_angle,
                                                                    double relative_azimuth_angle,
                                                                    long num_levels,
                                                                    const double *altitude_profile,
                                                                    double *solar_zenith_angle_profile,
                                                                    double *sensor_zenith_angle_profile,
                                                                    double *relative_azimuth_angle_profile);
double harp_wavelength_from_frequency(double frequency);
double harp_wavelength_from_wavenumber(double wavenumber);
double harp_wavenumber_from_frequency(double frequency);
double harp_wavenumber_from_wavelength(double wavelength);
double harp_wrap(double value, double min, double max);

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
void harp_interval_interpolate_array_linear(long source_length, const double *source_grid_boundaries,
                                            const double *source_array, long target_length,
                                            const double *target_grid_boundaries, double *target_array);
void harp_bounds_from_midpoints_linear(long num_midpoints, const double *midpoints, int extrapolate, double *intervals);
void harp_bounds_from_midpoints_loglinear(long num_midpoints, const double *midpoints, int extrapolate,
                                          double *intervals);

/* Collocation */
int harp_collocation_result_shallow_copy(const harp_collocation_result *collocation_result,
                                         harp_collocation_result **new_result);
void harp_collocation_result_shallow_delete(harp_collocation_result *collocation_result);

int harp_collocation_result_get_filtered_product_b(harp_collocation_result *collocation_result,
                                                   const char *source_product, harp_product **product);

#endif
