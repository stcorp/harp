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

#ifndef HARP_CHEMISTRY_H
#define HARP_CHEMISTRY_H

/* Naming of species follows the 'chemical formula' notation.
 * Strict Hill notation is not suitable, since it would represent SO2 as O2S (which nobody uses).
 * So the current naming convention is a bit of a mix between Hill notation and 'most used' notation.
 * Sorting of species does follow the Hill convention.
 */
typedef enum harp_chemical_species_enum
{
    harp_chemical_species_dry_air,
    harp_chemical_species_BrO,
    harp_chemical_species_BrO2,
    harp_chemical_species_CCl2F2,       /* CFC-12 / F12 / Freon-12 */
    harp_chemical_species_CCl3F,        /* CFC-11 / F11 / Freon-11 */
    harp_chemical_species_CF4,
    harp_chemical_species_CHClF2,
    harp_chemical_species_CH3Cl,
    harp_chemical_species_CH3CN,
    harp_chemical_species_CH3OH,
    harp_chemical_species_CH4,
    harp_chemical_species_CO,
    harp_chemical_species_COF2,
    harp_chemical_species_COS,
    harp_chemical_species_CO2,
    harp_chemical_species_C2H2,
    harp_chemical_species_C2H2O2,
    harp_chemical_species_C2H3NO5,      /* PAN */
    harp_chemical_species_C2H6,
    harp_chemical_species_C3H8,
    harp_chemical_species_C5H8,
    harp_chemical_species_ClNO3,        /* ClONO2 */
    harp_chemical_species_ClO,
    harp_chemical_species_HCHO,
    harp_chemical_species_HCOOH,
    harp_chemical_species_HCN,
    harp_chemical_species_HCl,
    harp_chemical_species_HF,
    harp_chemical_species_HNO2,
    harp_chemical_species_HNO3,
    harp_chemical_species_HNO4,
    harp_chemical_species_HOCl,
    harp_chemical_species_HO2,
    harp_chemical_species_H2O,
    harp_chemical_species_H2O_161,
    harp_chemical_species_H2O_162,
    harp_chemical_species_H2O_171,
    harp_chemical_species_H2O_181,
    harp_chemical_species_H2O2,
    harp_chemical_species_IO,
    harp_chemical_species_NO,
    harp_chemical_species_NOCl,
    harp_chemical_species_NO2,
    harp_chemical_species_NO3,
    harp_chemical_species_N2,
    harp_chemical_species_N2O,
    harp_chemical_species_N2O5,
    harp_chemical_species_OClO,
    harp_chemical_species_OH,
    harp_chemical_species_O2,
    harp_chemical_species_O3,
    harp_chemical_species_O3_666,
    harp_chemical_species_O3_667,
    harp_chemical_species_O3_668,
    harp_chemical_species_O3_686,
    harp_chemical_species_O4,
    harp_chemical_species_SF6,
    harp_chemical_species_SO2,
    harp_chemical_species_unknown
} harp_chemical_species;

#define harp_num_chemical_species harp_chemical_species_unknown

const char *harp_chemical_species_name(harp_chemical_species species);
harp_chemical_species harp_chemical_species_from_variable_name(const char *variable_name);

double harp_density_from_partial_column_and_altitude_bounds(double partial_column, const double *altitude_bounds);

double harp_mass_density_from_number_density(double number_density, double molar_mass);
double harp_mass_mixing_ratio_from_volume_mixing_ratio(double volume_mixing_ratio, double molar_mass_species,
                                                       double molar_mass_air);
double harp_molar_mass_air_from_density_and_number_density(double density, double number_density);
double harp_molar_mass_air_from_h2o_mass_mixing_ratio(double h2o_mass_mixing_ratio);
double harp_molar_mass_air_from_h2o_volume_mixing_ratio(double h2o_volume_mixing_ratio);
double harp_molar_mass_for_species(harp_chemical_species species);

double harp_number_density_from_mass_density(double mass_density, double molar_mass);
double harp_number_density_from_pressure_and_temperature(double pressure, double temperature);
double harp_number_density_from_volume_mixing_ratio(double volume_mixing_ratio, double number_density_air);
double harp_partial_column_from_density_and_altitude_bounds(double density, const double *altitude_bounds);
double harp_partial_column_number_density_from_volume_mixing_ratio(double volume_mixing_ratio, double latitude,
                                                                   double molar_mass_air,
                                                                   const double *pressure_bounds);
double harp_partial_pressure_from_volume_mixing_ratio_and_pressure(double volume_mixing_ratio, double pressure);
double harp_pressure_from_number_density_and_temperature(double number_density, double temperature);
double harp_relative_humidity_from_h2o_partial_pressure_and_temperature(double partial_pressure_h2o,
                                                                        double temperature);

double harp_temperature_from_number_density_and_pressure(double number_density, double pressure);
double harp_temperature_from_virtual_temperature(double virtual_temperature, double molar_mass_air);
double harp_virtual_temperature_from_temperature(double virtual_temperature, double molar_mass_air);
double harp_volume_mixing_ratio_from_mass_mixing_ratio(double mass_mixing_ratio, double molar_mass_species,
                                                       double molar_mass_air);
double harp_volume_mixing_ratio_from_number_density(double number_density, double number_density_air);
double harp_volume_mixing_ratio_from_partial_pressure_and_pressure(double partial_pressure, double pressure);

#endif
