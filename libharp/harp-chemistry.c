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
#include <string.h>

static const char *chemical_species_names[] = {
    "dry_air",
    "BrO",
    "BrO2",
    "CCl2F2",
    "CCl3F",
    "CF4",
    "CHClF2",
    "CH3Cl",
    "CH3CN",
    "CH3OH",
    "CH4",
    "CO",
    "COF2",
    "COS",
    "CO2",
    "C2H2",
    "C2H2O2",
    "C2H3NO5",
    "C2H6",
    "C3H8",
    "C5H8",
    "ClNO3",
    "ClO",
    "HCHO",
    "HCOOH",
    "HCN",
    "HCl",
    "HF",
    "HNO2",
    "HNO3",
    "HNO4",
    "HOCl",
    "HO2",
    "H2O",
    "H2O_161",
    "H2O_162",
    "H2O_171",
    "H2O_181",
    "H2O2",
    "IO",
    "NO",
    "NOCl",
    "NO2",
    "NO3",
    "N2",
    "N2O",
    "N2O5",
    "OClO",
    "OH",
    "O2",
    "O3",
    "O3_666",
    "O3_667",
    "O3_668",
    "O3_686",
    "O4",
    "SF6",
    "SO2",
    "unknown"
};

double chemical_species_molar_mass[] = {
    CONST_MOLAR_MASS_DRY_AIR,
    CONST_MOLAR_MASS_BrO,
    CONST_MOLAR_MASS_BrO2,
    CONST_MOLAR_MASS_CCl2F2,
    CONST_MOLAR_MASS_CCl3F,
    CONST_MOLAR_MASS_CF4,
    CONST_MOLAR_MASS_CHClF2,
    CONST_MOLAR_MASS_CH3Cl,
    CONST_MOLAR_MASS_CH3CN,
    CONST_MOLAR_MASS_CH3OH,
    CONST_MOLAR_MASS_CH4,
    CONST_MOLAR_MASS_CO,
    CONST_MOLAR_MASS_COF2,
    CONST_MOLAR_MASS_COS,
    CONST_MOLAR_MASS_CO2,
    CONST_MOLAR_MASS_C2H2,
    CONST_MOLAR_MASS_C2H2O2,
    CONST_MOLAR_MASS_C2H3NO5,
    CONST_MOLAR_MASS_C2H6,
    CONST_MOLAR_MASS_C3H8,
    CONST_MOLAR_MASS_C5H8,
    CONST_MOLAR_MASS_ClNO3,
    CONST_MOLAR_MASS_ClO,
    CONST_MOLAR_MASS_HCHO,
    CONST_MOLAR_MASS_HCOOH,
    CONST_MOLAR_MASS_HCN,
    CONST_MOLAR_MASS_HCl,
    CONST_MOLAR_MASS_HF,
    CONST_MOLAR_MASS_HNO2,
    CONST_MOLAR_MASS_HNO3,
    CONST_MOLAR_MASS_HNO4,
    CONST_MOLAR_MASS_HOCl,
    CONST_MOLAR_MASS_HO2,
    CONST_MOLAR_MASS_H2O,
    CONST_MOLAR_MASS_H2O_161,
    CONST_MOLAR_MASS_H2O_162,
    CONST_MOLAR_MASS_H2O_171,
    CONST_MOLAR_MASS_H2O_181,
    CONST_MOLAR_MASS_H2O2,
    CONST_MOLAR_MASS_IO,
    CONST_MOLAR_MASS_NO,
    CONST_MOLAR_MASS_NOCl,
    CONST_MOLAR_MASS_NO2,
    CONST_MOLAR_MASS_NO3,
    CONST_MOLAR_MASS_N2,
    CONST_MOLAR_MASS_N2O,
    CONST_MOLAR_MASS_N2O5,
    CONST_MOLAR_MASS_OClO,
    CONST_MOLAR_MASS_OH,
    CONST_MOLAR_MASS_O2,
    CONST_MOLAR_MASS_O3,
    CONST_MOLAR_MASS_O3_666,
    CONST_MOLAR_MASS_O3_667,
    CONST_MOLAR_MASS_O3_668,
    CONST_MOLAR_MASS_O3_686,
    CONST_MOLAR_MASS_O4,
    CONST_MOLAR_MASS_SF6,
    CONST_MOLAR_MASS_SO2,
    0   /* value for 'unknown' */
};

/** Calculate water vapour saturation pressure.
 * Use August-Roche-Magnus formula.
 * \param temperature  Temperature [K]
 * \return the water vapour saturation pressure [hPa]
 */
static double get_water_vapour_saturation_pressure_from_temperature(double temperature)
{
    double temperature_C = temperature - 273.15;        /* Temperature [degreeC] */

    return 6.1094 * exp(17.625 * temperature_C / (temperature_C + 243.04));
}

/** Return species name
 * \param species species enumeration value
 * \return string with name of the species
 */
const char *harp_chemical_species_name(harp_chemical_species species)
{
    assert(sizeof(chemical_species_names) / sizeof(chemical_species_names[0]) == harp_chemical_species_unknown + 1);
    return chemical_species_names[species];
}

/** Determine species from variable name
 * \param variable_name variable name
 * \return species enum identifier
 */
harp_chemical_species harp_chemical_species_from_variable_name(const char *variable_name)
{
    int i;

    if (variable_name == NULL)
    {
        return harp_chemical_species_unknown;
    }

    for (i = 0; i < harp_num_chemical_species; i++)
    {
        if (strncmp(variable_name, chemical_species_names[i], strlen(chemical_species_names[i])) == 0)
        {
            return i;
        }
    }
    return harp_chemical_species_unknown;
}

/** Convert a partial column profile to a density profile using the altitude boundaries as provided
 * This is a generic routine to convert partial columns to a densitity profile. It works for all cases where the
 * conversion is a matter of dividing the partial column value by the altitude height to get the density value.
 * \param partial_column Partial column [?]
 * \param altitude_bounds Lower and upper altitude [m] boundaries [2]
 * \return the density profile [?/m]
 */
double harp_density_from_partial_column_and_altitude_bounds(double partial_column, const double *altitude_bounds)
{
    double height = fabs(altitude_bounds[1] - altitude_bounds[0]);

    return (height < EPSILON ? 0 : partial_column / height);
}

/* Convert number density to mass density
 * \param number_density Number density [molec/m3]
 * \param molar_mass Molar mass [g/mol]
 * \return the mass density [ug/m3] */
double harp_mass_density_from_number_density(double number_density, double molar_mass)
{
    return 1e6 * number_density * molar_mass / CONST_NUM_AVOGADRO;
}

/* Convert volume mixing ratio to mass mixing ratio
 * \param volume_mixing_ratio Volume mixing ratio of the air component [ppmv]
 * \param molar_mass_species Molar mass of the air component [g/mol]
 * \param molar_mass_air Molar mass of air [g/mol]
 * \return the mass mixing ratio [ug/g] */
double harp_mass_mixing_ratio_from_volume_mixing_ratio(double volume_mixing_ratio, double molar_mass_species,
                                                       double molar_mass_air)
{
    return volume_mixing_ratio * molar_mass_species / molar_mass_air;
}

/** Get molar mass of total (wet) air from density and number density
 * \param density mass density of total air [ug/g]
 * \param number_density number density of total air [molec/m3]
 * \return the molar mass of total air [g/mol]
 */
double harp_molar_mass_air_from_density_and_number_density(double density, double number_density)
{
    return 1e-6 * density * CONST_NUM_AVOGADRO / number_density;
}

/** Get molar mass of total (wet) air from H2O mass mixing ratio
 * \param h2o_mass_mixing_ratio H2O mass mixing ratio [ug/g]
 * \return the molar mass of total air [g/mol]
 */
double harp_molar_mass_air_from_h2o_mass_mixing_ratio(double h2o_mass_mixing_ratio)
{
    h2o_mass_mixing_ratio *= 1e-6;      /* convert from [ug/g] to [g/g] */
    return (CONST_MOLAR_MASS_DRY_AIR * CONST_MOLAR_MASS_H2O) /
        ((1 - h2o_mass_mixing_ratio) * CONST_MOLAR_MASS_H2O + h2o_mass_mixing_ratio * CONST_MOLAR_MASS_DRY_AIR);
}

/** Get molar mass of total (wet) air from H2O volume mixing ratio
 * \param h2o_volume_mixing_ratio H2O volume mixing ratio [ppmv]
 * \return the molar mass of total air [g/mol]
 */
double harp_molar_mass_air_from_h2o_volume_mixing_ratio(double h2o_volume_mixing_ratio)
{
    h2o_volume_mixing_ratio *= 1e-6;    /* convert from [ppmv] to [ppv] */
    return CONST_MOLAR_MASS_DRY_AIR * (1 - h2o_volume_mixing_ratio) + CONST_MOLAR_MASS_H2O * h2o_volume_mixing_ratio;
}

/** Get molar mass of species of interest
 * \param species  Species enum
 * \return the molar mass [g/mol]
 */
double harp_molar_mass_for_species(harp_chemical_species species)
{
    assert(sizeof(chemical_species_molar_mass) / sizeof(chemical_species_molar_mass[0]) ==
           harp_chemical_species_unknown + 1);
    return chemical_species_molar_mass[species];
}

/** Convert mass density to number_density
 * \param mass_density Mass density [ug/m3]
 * \param molar_mass Molar mass [g/mol]
 * \return the number density [molec/m3]
 */
double harp_number_density_from_mass_density(double mass_density, double molar_mass)
{
    return 1e-6 * mass_density * CONST_NUM_AVOGADRO / molar_mass;
}

/** Convert (partial) pressure and temperature to number_density
 * \param pressure (Partial) pressure [hPa]
 * \param temperature Temperature [K]
 * \return the number density [molec/m3]
 */
double harp_number_density_from_pressure_and_temperature(double pressure, double temperature)
{
    return 1e2 * pressure / (CONST_BOLTZMANN * temperature);
}

/** Convert volume mixing ratio to number_density
 * \param volume_mixing_ratio volume mixing ratio [ppmv]
 * \param number_density_air Number density of air [molec/cm3]
 * \return the number density [molec/m3]
 */
double harp_number_density_from_volume_mixing_ratio(double volume_mixing_ratio, double number_density_air)
{
    return 1e-6 * volume_mixing_ratio * number_density_air;
}

/** Convert a density to a partial column using the altitude boundaries
 * This is a generic routine to convert adensitity to a partial column. It works for all cases where the conversion
 * is a matter of multiplying the density by the altitude height to get the partial column value.
 * \param density Density profile [?/m]
 * \param altitude_bounds Lower and upper altitude [m] boundaries [2]
 * \return the partial column [?]
 */
double harp_partial_column_from_density_and_altitude_bounds(double density, const double *altitude_bounds)
{
    return density * fabs(altitude_bounds[1] - altitude_bounds[0]);
}

/** Convert volume mixing ratio to partial pressure
 * \param volume_mixing_ratio volume mixing ratio [ppmv]
 * \param pressure Pressure [hPa]
 * \return the partial pressure [hPa]
 */
double harp_partial_pressure_from_volume_mixing_ratio_and_pressure(double volume_mixing_ratio, double pressure)
{
    return 1e-6 * volume_mixing_ratio * pressure;
}

/** Convert number density to (partial) pressure
 * \param number_density Number density [molec/m3]
 * \param temperature Temperature [K]
 * \return the (partial) pressure [hPa]
 */
double harp_pressure_from_number_density_and_temperature(double number_density, double temperature)
{
    return 1e-2 * number_density * CONST_BOLTZMANN * temperature;
}

/** Calculate the relative humidity from the given temperature and water vapour partial pressure.
 * The relative humidity is the ratio of the partial pressure of water vapour in a
 * gaseous mixture of air and water vapour to the saturated vapour pressure of water at a given temperature.
 * \param partial_pressure_h2o  Water vapour partial pressure [hPa]
 * \param temperature  Temperature [K]
 * \return the relative humidity [%]
 */
double harp_relative_humidity_from_h2o_partial_pressure_and_temperature(double partial_pressure_h2o, double temperature)
{
    return partial_pressure_h2o / get_water_vapour_saturation_pressure_from_temperature(temperature);
}

/** Convert number density to temperature
 * \param number_density Number density [molec/m3]
 * \param pressure (partial) Pressure [hPa]
 * \return the temperature [K]
 */
double harp_temperature_from_number_density_and_pressure(double number_density, double pressure)
{
    return 1e2 * pressure / (number_density * CONST_BOLTZMANN);
}

/** Calculate temperature from virtual temperature
 * \param virtual_temperature Virtual temperature [K]
 * \param molar_mass_air Molar mass of air [g/mol]
 * \return the temperature [K]
 */
double harp_temperature_from_virtual_temperature(double virtual_temperature, double molar_mass_air)
{
    return virtual_temperature * molar_mass_air / CONST_MOLAR_MASS_DRY_AIR;
}

/** Calculate virtual temperature from temperature
 * \param temperature Temperature [K]
 * \param molar_mass_air Molar mass of air [g/mol]
 * \return the virtual temperature [K]
 */
double harp_virtual_temperature_from_temperature(double temperature, double molar_mass_air)
{
    return temperature * CONST_MOLAR_MASS_DRY_AIR / molar_mass_air;
}

/** Convert mass mixing ratio to volume mixing ratio
 * \param mass_mixing_ratio Mass mixing ratio [ug/g]
 * \param molar_mass_species Molar mass of the air component [g/mol]
 * \param molar_mass_air Molar mass of air [g/mol]
 * \return the volume mixing ratio [ppmv]
 */
double harp_volume_mixing_ratio_from_mass_mixing_ratio(double mass_mixing_ratio, double molar_mass_species,
                                                       double molar_mass_air)
{
    return mass_mixing_ratio * molar_mass_air / molar_mass_species;
}

/** Convert number density to volume mixing ratio
 * \param number_density Number density of air component [molec/m3]
 * \param number_density_air Number density of air [molec/cm3]
 * \return the volume mixing ratio [ppmv]
 */
double harp_volume_mixing_ratio_from_number_density(double number_density, double number_density_air)
{
    return 1e6 * number_density / number_density_air;
}

/** Convert partial pressure to volume mixing ratio
 * \param partial_pressure  Partial pressure of constituent [hPa]
 * \param pressure   Pressure of air [hPa]
 * \return the volume mixing ratio [ppmv]
 */
double harp_volume_mixing_ratio_from_partial_pressure_and_pressure(double partial_pressure, double pressure)
{
    /* Convert [1] to [ppmv] */
    return 1e6 * partial_pressure / pressure;
}

/**
 * @}
 */
