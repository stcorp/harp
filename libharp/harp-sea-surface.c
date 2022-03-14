/*
 * Copyright (C) 2015-2022 S[&]T, The Netherlands.
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

    /* Impose limit */
    if (solar_zenith_angle <= 90.0 && wind_speed < 6.0)
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

    /* Impose limit */
    if (solar_zenith_angle <= 90.0 && wind_speed < 6.0)
    {
        wind_speed = 6.0;
    }

    /* Skin sea surface temperature [K] */
    sst_subskin = sst_skin + 0.14 + 0.30 * exp(-wind_speed / 3.7);

    return sst_subskin;
}
