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

#include <math.h>

/** Convert ocean wave period to ocean wave frequency
 * \param ocean_period      Ocean wave period [s]
 * \return the ocean frequency [Hz]
 */
double harp_ocean_frequency_from_ocean_period(double ocean_period)
{
    return 1.0 / ocean_period;
}

/** Convert ocean wave wavelength to ocean wave frequency
 * \param ocean_wavelength      Wavelength [m]
 * \return the ocean wave frequency f [Hz]
 */
double harp_ocean_frequency_from_ocean_wavelength(double ocean_wavelength)
{
    /* Deep water dispersion relation: k = (2*pi*f)^2 / g */
    /* k = 2 * pi/wavelength */
    /* Thus, f = sqrt(2*pi*wavelength/g) */
    return sqrt(2.0 * (double)M_PI * ocean_wavelength / ((double)CONST_GRAV_ACCEL));
}

/** Convert ocean wave wavenumber to ocean wave frequency
 * using the deep water dispersion relation
 * (the depth of the ocean is much greater than the ocean wave's wavelength)
 * \param ocean_wavenumber      Wavenumber [m]
 * \return the ocean wave frequency f [Hz]
 */
double harp_ocean_frequency_from_ocean_wavenumber(double ocean_wavenumber)
{
    /* Deep water dispersion relation: k = (2*pi*f)^2 / g */
    /* f = sqrt(k*g)/(2*pi) */
    return sqrt(ocean_wavenumber * (double)CONST_GRAV_ACCEL) / (2.0 * (double)M_PI);
}

/** Convert ocean wave frequency to ocean wave period
 * \param ocean_frequency      Frequency [Hz]
 * \return the ocean wave period T [s]
 */
double harp_ocean_period_from_ocean_frequency(double ocean_frequency)
{
    return 1.0 / ocean_frequency;
}

/** Convert ocean wave wavelength to ocean wave period
 * using the deep water dispersion relation
 * \param ocean_wavelength      Wavelength [m]
 * \return the ocean wave period T [s]
 */
double harp_ocean_period_from_ocean_wavelength(double ocean_wavelength)
{
    /* Deep water dispersion relation: k = (2*pi/T)^2 / g */
    /* k = 2 * pi/wavelength */
    /* Thus,  T = g/(2*pi*wavelength) */
    return sqrt((double)CONST_GRAV_ACCEL / (2.0 * (double)M_PI * ocean_wavelength));
}

/** Convert ocean wave wavenumber to ocean wave period
 * using the deep water dispersion relation
 * \param ocean_wavenumber      Wavenumber [m]
 * \return the ocean wave period T [s]
 */
double harp_ocean_period_from_ocean_wavenumber(double ocean_wavenumber)
{
    /* Deep water dispersion relation: k = (2*pi/T)^2 / g */
    /* Thus,  T = sqrt(k*g)/(2 * pi) */
    return sqrt(ocean_wavenumber * (double)CONST_GRAV_ACCEL) / (2.0 * (double)M_PI);
}

/** Convert ocean wave frequency to ocean wave wavelength,
 * using the deep water dispersion relation
 * \param ocean_frequency      Frequency [Hz]
 * \return the ocean wave wavelength [m]
 */
double harp_ocean_wavelength_from_ocean_frequency(double ocean_frequency)
{
    /* Deep water dispersion relation: k = (2*pi*f)^2 / g */
    /* k = 2 * pi/wavelength */
    /* Thus, wavelength = g/(2*pi*f^2) */
    return (double)CONST_GRAV_ACCEL / (2.0 * (double)M_PI * ocean_frequency * ocean_frequency);
}

/** Convert ocean wave period to ocean wave wavelength
 * using the deep water dispersion relation
 * \param ocean_period Ocean wave period T [s]
 * \return the ocean wave wavelength [m]
 */
double harp_ocean_wavelength_from_ocean_period(double ocean_period)
{
    /* Deep water dispersion relation: k = (2*pi/T)^2 / g */
    /* k = 2 * pi/wavelength */
    /* Thus, wavelength = g*T^2/(2*pi) */
    return ocean_period * ocean_period * (double)CONST_GRAV_ACCEL / (2.0 * (double)M_PI);
}

/** Convert ocean wave wavenumber to ocean wave wavelength
 * \param ocean_wavenumber      Wavenumber k [1/m]
 * \return the ocean wave wavelength [m]
 */
double harp_ocean_wavelength_from_ocean_wavenumber(double ocean_wavenumber)
{
    /* wavelength = 2 * pi/k */
    return 2.0 * (double)M_PI / ocean_wavenumber;
}

/** Convert ocean wave frequency to ocean wave wavenumber,
 * using the deep water dispersion relation
 * \param ocean_frequency      Frequency f [Hz]
 * \return the ocean wave wavenumber k [1/m]
 */
double harp_ocean_wavenumber_from_ocean_frequency(double ocean_frequency)
{
    /* Deep water dispersion relation: k = (2*pi*f)^2/g */
    return 4.0 * (double)M_PI *(double)M_PI *ocean_frequency * ocean_frequency / ((double)CONST_GRAV_ACCEL);
}

/** Convert ocean wave period to ocean wave wavelength
 * using the deep water dispersion relation
 * \param ocean_period Ocean wave period T [s]
 * \return the ocean wave wavenumber [1/m]
 */
double harp_ocean_wavenumber_from_ocean_period(double ocean_period)
{
    /* Deep water dispersion relation: k = (2*pi*f)^2/g */
    /* and f = 1/T. Thus, k = (2*pi)^2/(g*T^2) */
    return 4.0 * (double)M_PI *(double)M_PI / (ocean_period * ocean_period * (double)CONST_GRAV_ACCEL);
}

/** Convert ocean wave wavelength to ocean wave wavenumber
 * \param ocean_wavelength      Wavelength [Hz]
 * \return the ocean wave wavenumber k [1/m]
 */
double harp_ocean_wavenumber_from_ocean_wavelength(double ocean_wavelength)
{
    /* k = 2 * pi/wavelength */
    return 2.0 * (double)M_PI / ocean_wavelength;
}

/** Convert subskin to skin sea surface temperature
 * \param sst_subskin  Subskin sea surface temperature [K]
 * \param wind_speed  Wind speed [m/s]
 * \param solar_zenith_angle  Solar zenith angle [degree]
 * \return the skin sea surface temperature [K]
 */
double harp_sea_surface_temperature_skin_from_subskin_wind_speed_and_solar_zenith_angle(double sst_subskin,
                                                                                        double wind_speed,
                                                                                        double solar_zenith_angle)
{
    double sst_skin;
    int daytime;

    /* Determine whether measurement was taken during day or night */
    daytime = harp_daytime_from_solar_zenith_angle(solar_zenith_angle);

    /* Impose limit */
    if (daytime && wind_speed < 6.0)
    {
        wind_speed = 6.0;
    }

    /* Skin sea surface temperature [K] */
    sst_skin = sst_subskin - 0.14 - 0.30 * exp(-wind_speed / 3.7);

    return sst_skin;
}

/** Convert skin to subskin sea surface temperature
 * \param sst_skin  Skin sea surface temperature [K]
 * \param wind_speed  Wind speed [m/s]
 * \param solar_zenith_angle  Solar zenith angle [degree]
 * \return the subskin sea surface temperature [K]
 */
double harp_sea_surface_temperature_subskin_from_skin_wind_speed_and_solar_zenith_angle
    (double sst_skin, double wind_speed, double solar_zenith_angle)
{
    double sst_subskin; /* Skin sea surface temperature [K] */
    int daytime;

    /* Determine whether measurement was taken during day or night */
    daytime = harp_daytime_from_solar_zenith_angle(solar_zenith_angle);
    /* Impose limit */
    if (daytime && wind_speed < 6.0)
    {
        wind_speed = 6.0;
    }

    /* Skin sea surface temperature [K] */
    sst_subskin = sst_skin + 0.14 + 0.30 * exp(-wind_speed / 3.7);

    return sst_subskin;
}
