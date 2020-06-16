/*
 * Copyright (C) 2015-2020 S[&]T, The Netherlands.
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
#include "harp-geometry.h"

#include <math.h>
#include <stdlib.h>

/** Calculate the angstrom exponent for aerosol optical depths at different wavelength values
 * \param num_wavelength  Length of the spectral dimension (should be >= 2)
 * \param wavelength  Wavelength [m]
 * \param aod  Aerosol optical depth []
 * \return the angstrom exponent [1]
 */
double harp_angstrom_exponent_from_aod(long num_wavelengths, const double *wavelength, const double *aod)
{
    double mean_log_wavelength = 0;
    double mean_log_aod = 0;
    double numerator = 0;
    double denominator = 0;
    long i;

    if (num_wavelengths < 2)
    {
        return harp_nan();
    }

    for (i = 0; i < num_wavelengths; i++)
    {
        mean_log_wavelength += log(wavelength[i]);
        mean_log_aod += log(aod[i]);
    }
    mean_log_wavelength /= num_wavelengths;
    mean_log_aod /= num_wavelengths;

    for (i = 0; i < num_wavelengths; i++)
    {
        double log_wavelength = log(wavelength[i]);
        numerator += (log_wavelength - mean_log_wavelength) * (log(aod[i]) - mean_log_aod);
        denominator += (log_wavelength - mean_log_wavelength) * (log_wavelength - mean_log_wavelength);
    }

    return -(numerator/denominator);
}

/** Calculate the fraction of the day
 * \param datetime   Datetime [s since 2000-01-01]
 * \return the fraction of the day [1]
 */
double harp_fraction_of_day_from_datetime(double datetime)
{
    double datetime_in_days = datetime / 86400.0;

    return datetime_in_days - floor(datetime_in_days);
}

/** Calculate the fraction of the year
 * \param datetime   Datetime [s since 2000-01-01]
 * \return the fraction of the year [1]
 */
double harp_fraction_of_year_from_datetime(double datetime)
{
    double datetime_in_years = datetime / (365.2422 * 86400.0);

    return datetime_in_years - floor(datetime_in_years);
}

/** Calculate the equation of time (EOT) angle
 * \param datetime   Datetime [s since 2000-01-01]
 * \return the equation of time [minutes]
 */
static double get_equation_of_time_from_datetime(double datetime)
{
    double mean_angle, corrected_angle, angle_difference;

    /* calculate Earths orbit angle at date (relative to solstice) */
    /* add 10 days due to difference between December solstice and Jan 1st */
    mean_angle = 2 * M_PI * harp_fraction_of_year_from_datetime(datetime + 10 * 86400);

    /* correct for Earth's orbital eccentricity (0.0167) */
    /* subtract 2 days due to difference between Jan 1st and Earth's perihelion */
    corrected_angle =
        mean_angle + 2 * 0.0167 * sin(2 * M_PI * harp_fraction_of_year_from_datetime(datetime - 2 * 86400));

    /* calculate difference between mean spead and corrected speed angles (projected onto equatorial plane) */
    /* divide by pi to get the difference as a fraction of a 'half turn' */
    /* 23.44 [deg] is the obliquity (tilt) of the Earth's axis */
    angle_difference = (mean_angle - atan2(tan(corrected_angle), cos(CONST_DEG2RAD * 23.44))) / M_PI;

    /* wrap the fraction to [-0.5,0.5] and multiply by 720 (12 hours * 60 minutes) to get the amount of minutes */
    return 720 * (angle_difference - floor(angle_difference + 0.5));
}

/** Convert (electromagnetic wave) wavelength to (electromagnetic wave) frequency
 * \param wavelength      Wavelength [m]
 * \return the frequency [Hz]
 */
double harp_frequency_from_wavelength(double wavelength)
{
    /* frequency = c / wavelength */
    return (double)CONST_SPEED_OF_LIGHT / wavelength;
}

/** Convert (electromagnetic wave) wavenumber to (electromagnetic wave) frequency
 * \param wavenumber      Wavenumber [1/m]
 * \return the frequency [Hz]
 */
double harp_frequency_from_wavenumber(double wavenumber)
{
    /* frequency = c * wavenumber */
    return (double)CONST_SPEED_OF_LIGHT *wavenumber;
}


/* Calculate the gravitational acceleration g for a given latitude and altitude.
 * Using WGS84 Gravity formula
 * \param latitude Latitude [degree_north]
 * \param altitude Altitude [m]
 * \return the gravitational acceleration at the given altitude [m/s2] */
double harp_gravity_from_latitude_and_altitude(double latitude, double altitude)
{
    double a = 6378137.0;
    double f = 1 / 298.257223563;
    double m = 0.00344978650684;
    double sinphi = sin(latitude * CONST_DEG2RAD);

    return harp_normal_gravity_from_latitude(latitude) *
        (1 - (2 * (1 + f + m - 2 * f * sinphi * sinphi) + 3 * altitude / a) * altitude / a);
}

/* Calculate the local curvature radius Rsurf at the Earth's surface for a given latitude
 * \param latitude  Latitude [degree_north]
 * \return the local curvature radius Rsurf [m] */
double harp_local_curvature_radius_at_surface_from_latitude(double latitude)
{
    double Rsurf;
    double deg2rad = (double)(CONST_DEG2RAD);
    double phi = latitude * deg2rad;
    double Rmin = 6356752.0;    /* [m] */
    double Rmax = 6378137.0;    /* [m] */

    Rsurf = 1.0 / sqrt(cos(phi) * cos(phi) / (Rmin * Rmin) + sin(phi) * sin(phi) / (Rmax * Rmax));
    return Rsurf;
}

/* Calculate the gravitational acceleration g at sea level for a given latitude
 * Using WGS84 Gravity formula
 * \param latitude Latitude [degree_north]
 * \return the gravitational acceleration at sea level [m/s2] */
double harp_normal_gravity_from_latitude(double latitude)
{
    double g_e = 9.7803253359;
    double k = 0.00193185265241;
    double e2 = 0.00669437999013;
    double sinphi = sin(latitude * CONST_DEG2RAD);

    return g_e * (1 + k * sinphi * sinphi) / sqrt(1 - e2 * sinphi * sinphi);
}

/** Convert radiance to normalized radiance
 * \param radiance  Radiance [mW m-2 sr-1]
 * \param solar_irradiance  Solar irradiance [mW m-2]
 * \return the normalized radiance [1]
 */
double harp_normalized_radiance_from_radiance_and_solar_irradiance(double radiance, double solar_irradiance)
{
    return M_PI * radiance / solar_irradiance;
}

/** Convert reflectance to normalized radiance
 * \param reflectance  Reflectance [1]
 * \param solar_zenith_angle  Solar zenith angle [degree]
 * \return the normalized radiance [1]
 */
double harp_normalized_radiance_from_reflectance_and_solar_zenith_angle(double reflectance, double solar_zenith_angle)
{
    return cos(solar_zenith_angle * CONST_DEG2RAD) * reflectance;
}

/** Convert normalized radiance to radiance
 * \param normalized_radiance  Normalized radiance [1]
 * \param solar_irradiance  Solar irradiance [mW m-2]
 * \return the radiance [mW m-2 sr-1]
 */
double harp_radiance_from_normalized_radiance_and_solar_irradiance(double normalized_radiance, double solar_irradiance)
{
    double radiance;    /* Radiance [mW m-2 sr-1] */
    double pi = (double)M_PI;

    radiance = normalized_radiance * solar_irradiance / pi;
    return radiance;
}

/** Convert reflectance to radiance
 * \param reflectance  Reflectance [1]
 * \param solar_irradiance  Solar irradiance [mW m-2]
 * \param solar_zenith_angle  Solar zenith angle [degree]
 * \return the radiance [mW m-2 sr-1]
 */
double harp_radiance_from_reflectance_solar_irradiance_and_solar_zenith_angle(double reflectance,
                                                                              double solar_irradiance,
                                                                              double solar_zenith_angle)
{
    double radiance;    /* Radiance [mW m-2 sr-1] */
    double pi = (double)M_PI;
    double deg2rad = (double)CONST_DEG2RAD;
    double mu0 = cos(solar_zenith_angle * deg2rad);

    radiance = reflectance * mu0 * solar_irradiance / pi;
    return radiance;
}

/** Convert radiance to reflectance
 * \param radiance  Radiance [mW m-2 sr-1]
 * \param solar_irradiance  Solar irradiance [mW m-2]
 * \param solar_zenith_angle  Solar zenith angle [degree]
 * \return the reflectance [1]
 */
double harp_reflectance_from_radiance_solar_irradiance_and_solar_zenith_angle(double radiance,
                                                                              double solar_irradiance,
                                                                              double solar_zenith_angle)
{
    double reflectance; /* Reflectance [1] */
    double pi = (double)M_PI;
    double deg2rad = (double)CONST_DEG2RAD;
    double mu0 = cos(solar_zenith_angle * deg2rad);

    reflectance = pi * radiance / (mu0 * solar_irradiance);
    return reflectance;
}

/** Convert normalized radiance to reflectance
 * \param normalized_radiance  Normalized radiance [mW m-2 sr-1]
 * \param solar_zenith_angle  Solar zenith angle [degree]
 * \return the reflectance [1]
 */
double harp_reflectance_from_normalized_radiance_and_solar_zenith_angle(double normalized_radiance,
                                                                        double solar_zenith_angle)
{
    double reflectance; /* Reflectance [1] */
    double deg2rad = (double)CONST_DEG2RAD;
    double mu0 = cos(solar_zenith_angle * deg2rad);

    reflectance = normalized_radiance / mu0;
    return reflectance;
}

/** Convert sensor and solar angles into scattering angle
 * \param sensor_zenith_angle Sensor Zenith Angle [degree]
 * \param solar_zenith_angle Solar Zenith Angle [degree]
 * \param relative_azimuth_angle Relative Azimuth Angle [degree]
 * \return the scattering angle [degree]
 */
double harp_scattering_angle_from_sensor_and_solar_angles(double sensor_zenith_angle, double solar_zenith_angle,
                                                          double relative_azimuth_angle)
{
    double cosangle = -cos(sensor_zenith_angle * CONST_DEG2RAD) * cos(solar_zenith_angle * CONST_DEG2RAD) -
        sin(sensor_zenith_angle * CONST_DEG2RAD) * sin(solar_zenith_angle * CONST_DEG2RAD) *
        cos(relative_azimuth_angle * CONST_DEG2RAD);
    HARP_CLAMP(cosangle, -1.0, 1.0);
    return CONST_RAD2DEG * acos(cosangle);
}

/** Calculate the solar azimuth angle for the given time and location
 * \param latitude Latitude [degree_north]
 * \param solar_declination_angle Solar declination angle [deg]
 * \param solar_hour_angle Solar hour angle [deg]
 * \param solar_zenith_angle Solar zenith angle [deg]
 * \return the solar elevation angle [degree]
 */
double harp_solar_azimuth_angle_from_latitude_and_solar_angles(double latitude, double solar_declination_angle,
                                                               double solar_hour_angle, double solar_zenith_angle)
{
    double cosangle;
    double angle;
    double sin_sza;

    /* Convert angles to [rad] */
    latitude *= CONST_DEG2RAD;
    solar_declination_angle *= CONST_DEG2RAD;
    solar_hour_angle *= CONST_DEG2RAD;
    solar_zenith_angle *= CONST_DEG2RAD;

    sin_sza = sin(solar_zenith_angle);
    if (sin_sza == 0)
    {
        return 0;
    }

    cosangle = (sin(solar_declination_angle) * cos(latitude) -
                cos(solar_hour_angle) * cos(solar_declination_angle) * sin(latitude)) / sin_sza;
    HARP_CLAMP(cosangle, -1.0, 1.0);
    angle = CONST_RAD2DEG * acos(cosangle);
    if (solar_hour_angle > 0)
    {
        /* afternoon */
        return -angle;
    }
    /* morning */
    return angle;
}

/** Calculate the solar declination angle
 * \param datetime   Datetime [s since 2000-01-01]
 * \return the solar declination angle [degree]
 */
double harp_solar_declination_angle_from_datetime(double datetime)
{
    double mean_angle, corrected_angle;
    double sinangle;

    /* calculate Earths orbit angle at date (relative to solstice) */
    /* add 10 days due to difference between December solstice and Jan 1st */
    mean_angle = 2 * M_PI * harp_fraction_of_year_from_datetime(datetime + 10 * 86400);

    /* correct for Earth's orbital eccentricity (0.0167) */
    /* subtract 2 days due to difference between Jan 1st and Earth's perihelion */
    corrected_angle =
        mean_angle + 2 * 0.0167 * sin(2 * M_PI * harp_fraction_of_year_from_datetime(datetime - 2 * 86400));

    /* 23.44 [deg] is the obliquity (tilt) of the Earth's axis */
    sinangle = sin(CONST_DEG2RAD * 23.44) * cos(corrected_angle);
    HARP_CLAMP(sinangle, -1.0, 1.0);
    return CONST_RAD2DEG * -asin(sinangle);
}

/** Calculate the solar hour angle for the given time and location
 * \param datetime Datetime [s since 2000-01-01]
 * \param longitude Longitude [degree_east]
 * \return the solar hour angle [degree]
 */
double harp_solar_hour_angle_from_datetime_and_longitude(double datetime, double longitude)
{
    double local_fraction_of_day;

    local_fraction_of_day = harp_fraction_of_day_from_datetime(datetime) +
        get_equation_of_time_from_datetime(datetime) / (24 * 60);

    return harp_wrap(longitude + 360 * local_fraction_of_day - 180, -180, 180);
}

/** Calculate the solar zenith angle for the given time and location
 * \param latitude Latitude [degree_north]
 * \param solar_declination_angle Solar declination angle [deg]
 * \param solar_hour_angle Solar hour angle [deg]
 * \return the solar zenith angle [degree]
 */
double harp_solar_zenith_angle_from_latitude_and_solar_angles(double latitude, double solar_declination_angle,
                                                              double solar_hour_angle)
{
    double cosangle;

    /* Convert angles to [rad] */
    latitude *= CONST_DEG2RAD;
    solar_declination_angle *= CONST_DEG2RAD;
    solar_hour_angle *= CONST_DEG2RAD;

    cosangle = sin(solar_declination_angle) * sin(latitude) +
        cos(solar_hour_angle) * cos(solar_declination_angle) * cos(latitude);
    HARP_CLAMP(cosangle, -1.0, 1.0);
    return CONST_RAD2DEG * acos(cosangle);
}

/** Convert sensor and solar azimuth angles to relative azimuth angle
 * \param sensor_azimuth_angle Sensor azimuth angle[degree]
 * \param solar_azimuth_angle Solar azimuth angle[degree]
 * \return the relative azimuth angle [degree]
 */
double harp_relative_azimuth_angle_from_sensor_and_solar_azimuth_angles(double sensor_azimuth_angle,
                                                                        double solar_azimuth_angle)
{
    double angle = sensor_azimuth_angle - solar_azimuth_angle;

    while (angle < 0)
    {
        angle += 360;
    }
    while (angle >= 360)
    {
        angle -= 360;
    }

    if (angle > 180)
    {
        return 360 - angle;
    }

    return angle;
}

/** Convert zenith angle to elevation angle
 * \param zenith_angle Zenith angle[degree]
 * \return the elevation angle [degree]
 */
double harp_elevation_angle_from_zenith_angle(double zenith_angle)
{
    return 90.0 - zenith_angle;
}

/** Convert zenith angle to elevation angle
 * \param elevation_angle  elevation angle [degree]
 * \return the zenith angle [degree]
 */
double harp_zenith_angle_from_elevation_angle(double elevation_angle)
{
    return 90.0 - elevation_angle;
}

/** Convert viewing angle (zenith, elevation, or azimuth) to sensor angle
 * \param viewing_angle Viewing angle[degree]
 * \return the sensor angle [degree]
 */
double harp_sensor_angle_from_viewing_angle(double viewing_angle)
{
    return 180.0 - viewing_angle;
}

/** Convert sensor angle (zenith, elevation, or azimuth) to viewing angle
 * \param sensor_angle  sensor angle [degree]
 * \return the viewing angle [degree]
 */
double harp_viewing_angle_from_sensor_angle(double sensor_angle)
{
    return 180.0 - sensor_angle;
}

/** Convert the solar zenith angle, the sensor zenith angle and relative azimuth angle at one height to another height
 * \param source_altitude  Source altitude [m]
 * \param source_solar_zenith_angle  Solar zenith angle at source altitude [degree]
 * \param source_sensor_zenith_angle  Sensor zenith angle at source altitude [degree]
 * \param source_relative_azimuth_angle  Relative azimuth angle at source altitude [degree]
 * \param target_altitude  Target altitude [m]
 * \param new_target_solar_zenith_angle  Solar zenith angle at target altitude [degree]
 * \param new_target_sensor_zenith_angle  Sensor zenith angle at target altitude [degree]
 * \param new_target_relative_azimuth_angle  Relative azimuth angle at target altitude [degree]
 */
int harp_sensor_geometry_angles_at_altitude_from_other_altitude(double source_altitude,
                                                                double source_solar_zenith_angle,
                                                                double source_sensor_zenith_angle,
                                                                double source_relative_azimuth_angle,
                                                                double target_altitude,
                                                                double *new_target_solar_zenith_angle,
                                                                double *new_target_sensor_zenith_angle,
                                                                double *new_target_relative_azimuth_angle)
{
    double target_solar_zenith_angle;
    double target_sensor_zenith_angle;
    double target_relative_azimuth_angle;
    double Earth_radius = (double)(CONST_EARTH_RADIUS_WGS84_SPHERE);
    double deg2rad = (double)(CONST_DEG2RAD);
    double rad2deg = (double)(CONST_RAD2DEG);
    double theta0 = source_solar_zenith_angle * deg2rad;        /* Solar zenith angle [rad] */
    double thetaV = source_sensor_zenith_angle * deg2rad;       /* Sensor zenith angle [rad] */
    double deltaphi = source_relative_azimuth_angle * deg2rad;  /* Relative azimuth angle [rad] */
    double sintheta0 = sin(theta0);
    double costheta0 = cos(theta0);
    double sinthetaV = sin(thetaV);
    double cosdeltaphi = cos(deltaphi);
    double sintheta0k;
    double costheta0k;
    double sinthetaVk;
    double theta0k;
    double thetaVk;
    double sinbeta;
    double cosbeta;
    double cosdeltaphik;
    double deltaphik;
    double fk;
    int nadir;

    nadir = (source_sensor_zenith_angle == 0.0);

    if (nadir || (target_altitude == source_altitude))
    {
        /*  The output angles are identical to the input angles */
        target_solar_zenith_angle = source_solar_zenith_angle;
        target_sensor_zenith_angle = source_sensor_zenith_angle;
        if (source_relative_azimuth_angle > 180.0)
        {
            target_relative_azimuth_angle = 360.0 - source_relative_azimuth_angle;
        }
        else
        {
            target_relative_azimuth_angle = source_relative_azimuth_angle;
        }
    }
    else
    {
        /* Calculate the sensor zenith angles */
        fk = (Earth_radius + source_altitude) / (Earth_radius + target_altitude);
        sinthetaVk = fk * sinthetaV;
        HARP_CLAMP(sinthetaVk, -1.0, 1.0);
        thetaVk = asin(sinthetaVk);

        /* Calculate the polar angle beta between the lines
         * (Earth centre -- target_altitude) and (Earth centre -- source_altitude) */
        sinbeta = thetaVk - thetaV;
        cosbeta = sqrt(1.0 - sinbeta * sinbeta);

        /* Calculate the solar zenith angles */
        costheta0k = costheta0 * cosbeta + sintheta0 * sinbeta * cosdeltaphi;
        HARP_CLAMP(costheta0k, -1.0, 1.0);
        theta0k = acos(costheta0k);
        sintheta0k = sqrt(1.0 - costheta0k * costheta0k);

        /* Calculate the sensor azimuth angles */
        if (sintheta0k == 0.0)
        {
            /* The sun is in zenith, so the azimuth angle is arbitrary. Set to zero. */
            deltaphik = 0.0;
        }
        else
        {
            cosdeltaphik = (costheta0 - costheta0k * cosbeta) / (sintheta0k * sinbeta);
            HARP_CLAMP(cosdeltaphik, -1.0, 1.0);
            deltaphik = M_PI - acos(cosdeltaphik);
            if (deltaphik > M_PI)
            {
                deltaphik = 2.0 * M_PI - deltaphik;
            }
        }

        target_solar_zenith_angle = theta0k * rad2deg;
        target_sensor_zenith_angle = thetaVk * rad2deg;
        target_relative_azimuth_angle = deltaphik * rad2deg;
    }

    *new_target_solar_zenith_angle = target_solar_zenith_angle;
    *new_target_sensor_zenith_angle = target_sensor_zenith_angle;
    *new_target_relative_azimuth_angle = target_relative_azimuth_angle;
    return 0;
}

/** Calculate the solar zenith angle, the sensor zenith angle, and the relative azimuth angle for the requested altitudes
 * \param altitude  Height corresponding to the input angles (reference altitude) [m]
 * \param solar_zenith_angle  Solar zenith angle at reference altitude [degree]
 * \param sensor_zenith_angle  Sensor zenith angle at reference altitude [degree]
 * \param relative_azimuth_angle  Relative azimuth angle at reference altitude [degree]
 * \param num_levels Number of levels
 * \param altitude_profile  Altitude profile [m]
 * \param solar_zenith_angle_profile  Solar zenith angles at profile altitudes [degree]
 * \param sensor_zenith_angle_profile  Sensor zenith angles at profile altitudes [degree]
 * \param relative_azimuth_angle_profile  Relative azimuth angles at profile altitudes [degree]
 */
int harp_sensor_geometry_angle_profiles_from_sensor_geometry_angles(double altitude,
                                                                    double solar_zenith_angle,
                                                                    double sensor_zenith_angle,
                                                                    double relative_azimuth_angle,
                                                                    long num_levels,
                                                                    const double *altitude_profile,
                                                                    double *solar_zenith_angle_profile,
                                                                    double *sensor_zenith_angle_profile,
                                                                    double *relative_azimuth_angle_profile)
{
    long k;

    if (altitude_profile == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "altitude profile is empty (%s:%u)", __FILE__, __LINE__);
        return -1;
    }
    if (solar_zenith_angle_profile == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "solar zenith angle profile is empty (%s:%u)", __FILE__, __LINE__);
        return -1;
    }
    if (sensor_zenith_angle_profile == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "sensor zenith angle profile is empty (%s:%u)", __FILE__, __LINE__);
        return -1;
    }
    if (relative_azimuth_angle_profile == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "relative azimuth angle profile is empty (%s:%u)", __FILE__,
                       __LINE__);
        return -1;
    }

    for (k = 0; k < num_levels; k++)
    {
        if (harp_sensor_geometry_angles_at_altitude_from_other_altitude(altitude,
                                                                        solar_zenith_angle,
                                                                        sensor_zenith_angle,
                                                                        relative_azimuth_angle,
                                                                        altitude_profile[k],
                                                                        &(solar_zenith_angle_profile[k]),
                                                                        &(sensor_zenith_angle_profile[k]),
                                                                        &(relative_azimuth_angle_profile[k])) != 0)
        {
            return -1;
        }
    }

    return 0;
}

/** Convert (electromagnetic wave) frequency to (electromagnetic wave) wavelength
 * \param frequency Frequency [Hz]
 * \return Wavelength [m]
 */
double harp_wavelength_from_frequency(double frequency)
{
    return (double)CONST_SPEED_OF_LIGHT / frequency;
}

/** Convert (electromagnetic wave) wavenumber to (electromagnetic wave) wavelength
 * \param wavenumber Wavenumber [1/m]
 * \return Wavelength [m]
 */
double harp_wavelength_from_wavenumber(double wavenumber)
{
    return 1.0 / wavenumber;
}

/** Convert (electromagnetic wave) frequency to (electromagnetic wave) wavenumber
 * \param frequency Frequency [Hz]
 * \return Wavenumber [1/m]
 */
double harp_wavenumber_from_frequency(double frequency)
{
    return frequency / (double)CONST_SPEED_OF_LIGHT;
}

/** Convert (electromagnetic wave) wavelength to (electromagnetic wave) wavenumber
 * \param wavelength Wavelength [m]
 * \return Wavenumber [1/m]
 */
double harp_wavenumber_from_wavelength(double wavelength)
{
    return 1.0 / wavelength;
}

/** Wrap a value to the given min/max range
 * The result is: min + (value - min) % (max - min)
 * \param value Value to wrap to the given range
 * \param min Minimum value of the range
 * \param max Maximum value of the range
 * \return Wrapped value
 */
double harp_wrap(double value, double min, double max)
{
    if (value < min)
    {
        return max + fmod(value - min, max - min);
    }
    return min + fmod(value - min, max - min);
}
