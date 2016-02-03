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
    "air",
    "BrO",
    "C2H2",
    "C2H6",
    "CCl2F2",
    "CCl3F",
    "CF4",
    "CH2O",
    "CH3Cl",
    "CH4",
    "CHF2Cl",
    "ClNO",
    "ClONO2",
    "ClO",
    "CO2",
    "COF2",
    "CO",
    "H2O_161",
    "H2O_162",
    "H2O_171",
    "H2O_181",
    "H2O2",
    "H2O",
    "HCl",
    "HCN",
    "HCOOH",
    "HF",
    "HO2NO2",
    "HO2",
    "HOCl",
    "HNO3",
    "N2O",
    "N2O5",
    "N2",
    "NO2",
    "NO3",
    "NO",
    "O2",
    "O3_666",
    "O3_667",
    "O3_668",
    "O3_686",
    "O3",
    "O4",
    "OBrO",
    "OClO",
    "OCS",
    "OH",
    "SF6",
    "SO2",
    "unknown"
};

double chemical_species_molar_mass[] = {
    CONST_MOLAR_MASS_DRY_AIR,
    CONST_MOLAR_MASS_BrO,
    CONST_MOLAR_MASS_C2H2,
    CONST_MOLAR_MASS_C2H6,
    CONST_MOLAR_MASS_CCl2F2,
    CONST_MOLAR_MASS_CCl3F,
    CONST_MOLAR_MASS_CF4,
    CONST_MOLAR_MASS_CH2O,
    CONST_MOLAR_MASS_CH3Cl,
    CONST_MOLAR_MASS_CH4,
    CONST_MOLAR_MASS_CHF2Cl,
    CONST_MOLAR_MASS_ClNO,
    CONST_MOLAR_MASS_ClONO2,
    CONST_MOLAR_MASS_ClO,
    CONST_MOLAR_MASS_CO2,
    CONST_MOLAR_MASS_COF2,
    CONST_MOLAR_MASS_CO,
    CONST_MOLAR_MASS_H2O_161,
    CONST_MOLAR_MASS_H2O_162,
    CONST_MOLAR_MASS_H2O_171,
    CONST_MOLAR_MASS_H2O_181,
    CONST_MOLAR_MASS_H2O2,
    CONST_MOLAR_MASS_H2O,
    CONST_MOLAR_MASS_HCl,
    CONST_MOLAR_MASS_HCN,
    CONST_MOLAR_MASS_HCOOH,
    CONST_MOLAR_MASS_HF,
    CONST_MOLAR_MASS_HO2NO2,
    CONST_MOLAR_MASS_HO2,
    CONST_MOLAR_MASS_HOCl,
    CONST_MOLAR_MASS_HNO3,
    CONST_MOLAR_MASS_N2O,
    CONST_MOLAR_MASS_N2O5,
    CONST_MOLAR_MASS_N2,
    CONST_MOLAR_MASS_NO2,
    CONST_MOLAR_MASS_NO3,
    CONST_MOLAR_MASS_NO,
    CONST_MOLAR_MASS_O2,
    CONST_MOLAR_MASS_O3_666,
    CONST_MOLAR_MASS_O3_667,
    CONST_MOLAR_MASS_O3_668,
    CONST_MOLAR_MASS_O3_686,
    CONST_MOLAR_MASS_O3,
    CONST_MOLAR_MASS_O4,
    CONST_MOLAR_MASS_OBrO,
    CONST_MOLAR_MASS_OClO,
    CONST_MOLAR_MASS_OCS,
    CONST_MOLAR_MASS_OH,
    CONST_MOLAR_MASS_SF6,
    CONST_MOLAR_MASS_SO2,
    0   /* value for 'unknown' */
};

/** \addtogroup harp_algorithm
 * @{
 */

/** Calculate water vapour saturation pressure.
 * Formula from Bolton 1980.
 * \param temperature  Temperature [K]
 * \return the water vapour saturation pressure [hPa]
 */
static double get_water_vapour_saturation_pressure_from_temperature(double temperature)
{
    double e_sat;       /* Water vapour saturation pressure [hPa] */
    double temperature_C;       /* Temperature [degreeC] */

    /* Convert to degrees Celsius */
    temperature_C = temperature - 273.15;       /* [degreeC] */

    /* Calculate the water vapour saturation pressure */
    e_sat = 6.112 * exp(17.67 * temperature_C / (temperature_C + 243.5));       /* [hPa] */
    return e_sat;
}

/** Calculate water water vapour saturation density
 * \param temperature  Temperature [K]
 * \return the water vapour saturation density [molec/m3]
 */
static double get_saturation_density_from_temperature(double temperature)
{
    double rg = (double)CONST_MOLAR_GAS;        /* Molar gas constant [kg m2/(K mol s2)] */
    double na = (double)CONST_NUM_AVOGADRO;     /* Number of avogadro [1/mol] */

    /* Water vapour saturation pressure [Pa] */
    double e_sat = 100 * get_water_vapour_saturation_pressure_from_temperature(temperature);

    /* Saturation density [molec/m3] */
    double n_sat = e_sat * na / (rg * temperature);

    return n_sat;
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
 * \param number_density  Number density [molec/m3]
 * \param species  Molecular species
 * \return the mass density [ug/m3] */
double harp_mass_density_from_number_density(double number_density, harp_chemical_species species)
{
    /* Convert [g/m3] to [ug/m3] */
    return 1e6 * number_density * harp_molar_mass_for_species(species) / CONST_NUM_AVOGADRO;
}

/* Convert volume mixing ratio to mass mixing ratio
 * \param volume_mixing_ratio  Volume mixing ratio [ppmv]
 * \return the mass mixing ratio [ug/g] */
double harp_mass_mixing_ratio_from_volume_mixing_ratio(double volume_mixing_ratio, harp_chemical_species species)
{
    /*  Conversion factor = 1, from [g/g] to [ug/g] to [g/g] and from [ppmv] to [1] */
    return volume_mixing_ratio * harp_molar_mass_for_species(species) / CONST_MOLAR_MASS_DRY_AIR;
}

/* Convert volume mixing ratio to mass mixing ratio with regard to wet air
 * \param volume_mixing_ratio  Volume mixing ratio [ppmv]
 * \param h2o_mass_mixing_ratio Mass mixing ratio of H2O [ug/g]
 * \return the mass mixing ratio [ug/g] */
double harp_mass_mixing_ratio_wet_from_volume_mixing_ratio_and_humidity(double volume_mixing_ratio,
                                                                        double h2o_mass_mixing_ratio,
                                                                        harp_chemical_species species)
{
    double molar_mass_air = harp_molar_mass_for_wet_air(h2o_mass_mixing_ratio * 1e-6);  /* convert [ug/g] to [g/g] */

    /*  Conversion factor = 1, from [ppmv] to [1] and from [g/g] to [ug/g]  */
    return volume_mixing_ratio * harp_molar_mass_for_species(species) / molar_mass_air;
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

/** Get molar mass of wet air from H2O mass mixing ratio (humidity)
 * \param h2o_mass_mixing_ratio Humidity (q) [ug/g]
 * \return the molar mass of moist air [g/mol]
 */
double harp_molar_mass_for_wet_air(double h2o_mass_mixing_ratio)
{
    h2o_mass_mixing_ratio *= 1e-6;      /* convert from [ug/g] to [g/g] */
    /* n: number of molecules [mol], M: molar mass [g/mol], da: dry air, a: wet air, q: h2o_mmr [g/g]
     * 1) n_a = n_da + n_h2o
     * 2) M_a * n_a = M_da * n_da + M_h2o * n_h2o
     * 3) q = (M_h2o * n_h2o) / (M_a * n_a)
     * This gives:
     * M_a * n_a = M_da * n_a + (M_h2o - M_da) * M_a * n_a * q / M_h2o =>
     * 1 = M_da/M_a + (1 - M_da / M_h2o) * q =>
     * M_a = M_da * M_h2o / ( (1 - q) * M_h2o + q * M_da )
     */
    return (CONST_MOLAR_MASS_DRY_AIR * CONST_MOLAR_MASS_H2O) /
        ((1 - h2o_mass_mixing_ratio) * CONST_MOLAR_MASS_H2O + h2o_mass_mixing_ratio * CONST_MOLAR_MASS_DRY_AIR);
}

/** Convert mass density to number_density
 * \param mass_density Mass density [ug/m3]
 * \param species Species enum
 * \return the number density [molec/m3]
 */
double harp_number_density_from_mass_density(double mass_density, harp_chemical_species species)
{
    /* Convert [ug/m3] to [g/m3] */
    mass_density *= 1e-6;
    return mass_density * CONST_NUM_AVOGADRO / harp_molar_mass_for_species(species);
}

/** Convert mass mixing ratio to number density
 * \param mass_mixing_ratio mass mixing ratio [ug/g]
 * \param temperature  Temperature [K]
 * \param pressure  Pressure [hPa]
 * \param species Species enum
 * \return the number density [molec/m3]
 */
double harp_number_density_from_mass_mixing_ratio_pressure_and_temperature(double mass_mixing_ratio,
                                                                           double pressure, double temperature,
                                                                           harp_chemical_species species)
{
    double number_density;
    double volume_mixing_ratio;

    /* First, convert the mass mixing ratio to volume mixing ratio */
    volume_mixing_ratio = harp_volume_mixing_ratio_from_mass_mixing_ratio(mass_mixing_ratio, species);

    /* Second, convert the volume mixing ratio to number density */
    number_density = harp_number_density_from_volume_mixing_ratio_pressure_and_temperature(volume_mixing_ratio,
                                                                                           pressure, temperature);
    return number_density;
}

/** Convert partial pressure to number_density
 * \param partial_pressure  Partial pressure [hPa]
 * \param pressure  Pressure [hPa]
 * \param temperature Temperature [K]
 * \return the number density [molec/m3]
 */
double harp_number_density_from_partial_pressure_pressure_and_temperature(double partial_pressure, double pressure,
                                                                          double temperature)
{
    double number_density;
    double volume_mixing_ratio;

    /* First, convert partial pressure to volume mixing ratio */
    volume_mixing_ratio = harp_volume_mixing_ratio_from_partial_pressure_and_pressure(partial_pressure, pressure);

    /* Second, convert volume mixing ratio to number density */
    number_density = harp_number_density_from_volume_mixing_ratio_pressure_and_temperature(volume_mixing_ratio,
                                                                                           pressure, temperature);
    return number_density;
}

/** Convert volume mixing ratio to number_density
 * \param volume_mixing_ratio  volume mixing ratio [ppmv]
 * \param pressure  Pressure [hPa]
 * \param temperature  Temperature [K]
 * \return the number density [molec/m3]
 */
double harp_number_density_from_volume_mixing_ratio_pressure_and_temperature(double volume_mixing_ratio,
                                                                             double pressure, double temperature)
{
    /* Convert [ppmv] to [1] */
    return 1e-6 * volume_mixing_ratio * CONST_STD_AIR_DENSITY *
        (CONST_STD_TEMPERATURE / temperature) * (pressure / CONST_STD_PRESSURE);
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

/** Convert mass mixing ratio to partial pressure
 * \param mass_mixing_ratio Mass mixing ratio [ug/g]
 * \param pressure  Pressure [hPa]
 * \param species   The chemical species for which the mmr was provided
 * \return the partial pressure [hPa]
 */
double harp_partial_pressure_from_mass_mixing_ratio_and_pressure(double mass_mixing_ratio, double pressure,
                                                                 harp_chemical_species species)
{
    double partial_pressure;
    double volume_mixing_ratio;

    /* First, convert mass mixing ratio to volume mixing ratio */
    volume_mixing_ratio = harp_volume_mixing_ratio_from_mass_mixing_ratio(mass_mixing_ratio, species);

    /* Second, convert volume mixing ratio to partial pressure */
    partial_pressure = harp_partial_pressure_from_volume_mixing_ratio_and_pressure(volume_mixing_ratio, pressure);
    return partial_pressure;
}

/** Convert number density to partial pressure
 * \param number_density Number density [molec/m3]
 * \param pressure  Pressure [hPa]
 * \param temperature Temperature [K]
 * \return the partial pressure [hPa]
 */
double harp_partial_pressure_from_number_density_pressure_and_temperature(double number_density, double pressure,
                                                                          double temperature)
{
    double partial_pressure;
    double volume_mixing_ratio;

    /* First, convert number density to volume mixing ratio */
    volume_mixing_ratio =
        harp_volume_mixing_ratio_from_number_density_pressure_and_temperature(number_density, pressure, temperature);

    /* Second, convert volume mixing ratio to partial pressure */
    partial_pressure = harp_partial_pressure_from_volume_mixing_ratio_and_pressure(volume_mixing_ratio, pressure);
    return partial_pressure;
}

/** Convert volume mixing ratio to partial pressure
 * \param volume_mixing_ratio  volume mixing ratio [ppmv]
 * \param pressure  Pressure [hPa]
 * \return the partial pressure [hPa]
 */
double harp_partial_pressure_from_volume_mixing_ratio_and_pressure(double volume_mixing_ratio, double pressure)
{
    double partial_pressure;    /* partial pressure [hPa] */
    double factor = 1.0e-6;     /* Convert [ppmv] to [1] */

    partial_pressure = factor * volume_mixing_ratio * pressure;
    return partial_pressure;
}

/** Convert a geopotential height value value to a pressure value using model values
 * This is a rather inaccurate way of calculating the pressure, so only use it when you can't use
 * any of the other approaches.
 * \param gph geopotential height to be converted [m]
 * \return pressure value [hPa]
 */
double harp_pressure_from_gph(double gph)
{
    /* use a very simple approach using constant values for most of the needed quantities */
    return CONST_STD_PRESSURE * exp(-CONST_GRAV_ACCEL_45LAT_WGS84_SPHERE * CONST_MEAN_MOLAR_MASS_WET_AIR *
                                    gph * 1.0e-3 / (CONST_STD_TEMPERATURE * CONST_MOLAR_GAS));
}

/** Calculate the relative humidity from the given water vapour number density and temperature.
 * The relative humidity is the ratio of the partial pressure of water vapour in a
 * gaseous mixture of air and water vapour to the saturated vapour pressure of water at a given temperature.
 * \param number_density_h2o  Water vapour number density [molec/m3]
 * \param temperature  Temperature [K]
 * \return the relative humidity [%]
 */
double harp_relative_humidity_from_h2o_number_density_and_temperature(double number_density_h2o, double temperature)
{
    double relative_humidity;   /* Relative humidity [%] */
    double n_sat;       /* water vapour saturation density [molec/m3] */

    /* Calculate water vapour saturation density [molec/m3] */
    n_sat = get_saturation_density_from_temperature(temperature);
    /* Relative humidity [%] */
    relative_humidity = number_density_h2o / n_sat * 100.0;
    return relative_humidity;
}

/** Calculate the virtual temperature
 * \param pressure  Pressure [hPa]
 * \param temperature  Temperature [K]
 * \param relative_humidity  Relative humidity [%]
 * \return the virtual temperature [K]
 */
double harp_virtual_temperature_from_pressure_temperature_and_relative_humidity(double pressure, double temperature,
                                                                                double relative_humidity)
{
    double e_sat;       /* Water vapour saturation pressure [hPa] */
    double ea = 0.622;  /* Ratio of the molecular weights of wet and dry air */

    /* Water vapour saturation pressure [hPa] */
    e_sat = get_water_vapour_saturation_pressure_from_temperature(temperature);
    /* T_virtual = T / (1 - R_H * (1-e_a) * e_sat / (100 * p))) */
    return temperature / (1.0 - relative_humidity * (1.0 - ea) * e_sat / pressure);
}

/** Convert mass mixing ratio to volume mixing ratio
 * \param mass_mixing_ratio  Mass mixing ratio [ug/g]
 * \param species  Molecular species
 * \return the volume mixing ratio [ppmv]
 */
double harp_volume_mixing_ratio_from_mass_mixing_ratio(double mass_mixing_ratio, harp_chemical_species species)
{
    /*  Conversion factor = 1, from [ug/g] to [g/g] and from [1] to [ppmv] */
    return mass_mixing_ratio * CONST_MOLAR_MASS_DRY_AIR / harp_molar_mass_for_species(species);
}

/** Convert mass mixing ratio w.r.t moist air to volume mixing ratio
 * \param mass_mixing_ratio  Mass mixing ratio of species [ug/g]
 * \param h2o_mass_mixing_ratio Mass mixing ratio of H2O [ug/g]
 * \param species  Molecular species
 * \return the volume mixing ratio [ppmv]
 */
double harp_volume_mixing_ratio_from_mass_mixing_ratio_wet_and_humidity(double mass_mixing_ratio,
                                                                        double h2o_mass_mixing_ratio,
                                                                        harp_chemical_species species)
{
    double molar_mass_air = harp_molar_mass_for_wet_air(h2o_mass_mixing_ratio * 1e-6);  /* convert [ug/g] to [g/g] */

    /*  Conversion factor = 1, from [ug/g] to [g/g] and from [1] to [ppmv] */
    return mass_mixing_ratio * molar_mass_air / harp_molar_mass_for_species(species);
}

/** Convert number density to volume mixing ratio
 * \param number_density  Number density [molec/m3]
 * \param temperature  Temperature [K]
 * \param pressure  Pressure [hPa]
 * \return the volume mixing ratio [ppmv]
 */
double harp_volume_mixing_ratio_from_number_density_pressure_and_temperature(double number_density, double pressure,
                                                                             double temperature)
{
    /* Convert [1] to [ppmv] */
    return 1e6 * (number_density / CONST_STD_AIR_DENSITY) * (temperature / CONST_STD_TEMPERATURE) *
        (CONST_STD_PRESSURE / pressure);
}

/** Convert partial pressure to volume mixing ratio
 * \param partial_pressure  Partial pressure of constituent [hPa]
 * \param pressure   Pressure of air [hPa]
 * \return the volume mixing ratio [ppmv]
 */
double harp_volume_mixing_ratio_from_partial_pressure_and_pressure(double partial_pressure, double pressure)
{
    /* Convert [1] to [ppmv] */
    return 1.0e6 * partial_pressure / pressure;
}

/**
 * @}
 */
