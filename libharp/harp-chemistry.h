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

#ifndef HARP_CHEMISTRY_H
#define HARP_CHEMISTRY_H

/* Naming of species follows the 'chemical formula' notation.
 * Strict Hill notation is not suitable, since it would represent SO2 as O2S (which nobody uses).
 * So the current naming convention is a bit of a mix between Hill notation and 'most used' notation.
 * Sorting of species does follow the Hill convention.
 */
typedef enum harp_chemical_species_enum
{
    harp_chemical_species_air,
    harp_chemical_species_BrO,
    harp_chemical_species_BrO2,
    harp_chemical_species_CCl2F2,       /* CFC-12 / F12 / Freon-12 */
    harp_chemical_species_CCl3F,        /* CFC-11 / F11 / Freon-11 */
    harp_chemical_species_CF4,
    harp_chemical_species_CHClF2,
    harp_chemical_species_CH2O, /* HCHO */
    harp_chemical_species_CH2O2,        /* HCOOH */
    harp_chemical_species_CH3Cl,
    harp_chemical_species_CH4,
    harp_chemical_species_CO,
    harp_chemical_species_COF2,
    harp_chemical_species_COS,
    harp_chemical_species_CO2,
    harp_chemical_species_C2H2,
    harp_chemical_species_C2H3NO5,      /* PAN */
    harp_chemical_species_C2H6,
    harp_chemical_species_C3H8,
    harp_chemical_species_C5H8,
    harp_chemical_species_ClNO3,        /* ClONO2 */
    harp_chemical_species_ClO,
    harp_chemical_species_ClO2,
    harp_chemical_species_HCN,
    harp_chemical_species_HCl,
    harp_chemical_species_HF,
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
    harp_chemical_species_NO,
    harp_chemical_species_NOCl,
    harp_chemical_species_NO2,
    harp_chemical_species_NO3,
    harp_chemical_species_N2,
    harp_chemical_species_N2O,
    harp_chemical_species_N2O5,
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

double harp_mass_density_from_number_density(double number_density, harp_chemical_species species);
double harp_mass_mixing_ratio_from_volume_mixing_ratio(double volume_mixing_ratio, harp_chemical_species species);
double harp_mass_mixing_ratio_wet_from_volume_mixing_ratio_and_humidity(double volume_mixing_ratio,
                                                                        double h2o_mass_mixing_ratio,
                                                                        harp_chemical_species species);
double harp_molar_mass_for_species(harp_chemical_species species);
double harp_molar_mass_for_wet_air(double h2o_mass_mixing_ratio);

double harp_number_density_from_mass_density(double mass_density, harp_chemical_species species);
double harp_number_density_from_mass_mixing_ratio_pressure_and_temperature(double mass_mixing_ratio, double pressure,
                                                                           double temperature,
                                                                           harp_chemical_species species);
double harp_number_density_from_partial_pressure_pressure_and_temperature(double partial_pressure, double pressure,
                                                                          double temperature);
double harp_number_density_from_volume_mixing_ratio_pressure_and_temperature(double volume_mixing_ratio,
                                                                             double pressure, double temperature);
double harp_partial_column_from_density_and_altitude_bounds(double density, const double *altitude_bounds);
double harp_partial_pressure_from_mass_mixing_ratio_and_pressure(double mass_mixing_ratio, double pressure,
                                                                 harp_chemical_species species);
double harp_partial_pressure_from_number_density_pressure_and_temperature(double number_density, double pressure,
                                                                          double temperature);
double harp_partial_pressure_from_volume_mixing_ratio_and_pressure(double volume_mixing_ratio, double pressure);
double harp_pressure_from_gph(double gph);
double harp_relative_humidity_from_h2o_number_density_and_temperature(double number_density_h2o, double temperature);

double harp_virtual_temperature_from_pressure_temperature_and_relative_humidity(double temperature, double pressure,
                                                                                double relative_humidity);
double harp_volume_mixing_ratio_from_mass_density_pressure_and_temperature(double mass_density, double pressure,
                                                                           double temperature,
                                                                           harp_chemical_species species);
double harp_volume_mixing_ratio_from_mass_mixing_ratio(double mass_mixing_ratio, harp_chemical_species species);
double harp_volume_mixing_ratio_from_mass_mixing_ratio_wet_and_humidity(double mass_mixing_ratio,
                                                                        double h2o_mass_mixing_ratio,
                                                                        harp_chemical_species species);
double harp_volume_mixing_ratio_from_number_density_pressure_and_temperature(double number_density, double pressure,
                                                                             double temperature);
double harp_volume_mixing_ratio_from_partial_pressure_and_pressure(double partial_pressure, double pressure);

#endif
