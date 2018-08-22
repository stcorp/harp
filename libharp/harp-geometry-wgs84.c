/*
 * Copyright (C) 2015-2018 S[&]T, The Netherlands.
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

#include "harp-geometry.h"

#include <math.h>

/* Convert latitude, longitude [deg] to Cartesian coordinates [m] */
void harp_wgs84_ellipsoid_cartesian_coordinates_from_latitude_and_longitude(double latitude, double longitude,
                                                                            double *new_x, double *new_y, double *new_z)
{
    double deg2rad = (double)(CONST_DEG2RAD);
    double lambda = longitude * deg2rad;
    double phi = latitude * deg2rad;
    double a = (double)(CONST_SEMI_MAJOR_AXIS_WGS84_ELLIPSOID);
    double e = (double)(CONST_ECCENTRICITY_WGS84_ELLIPSOID);
    double v = a / sqrt(1.0 - e * e * sin(phi) * sin(phi));     /* radius of curvature in the prime vertical */

    *new_x = v * cos(phi) * cos(lambda);
    *new_y = v * cos(phi) * sin(lambda);
    *new_z = (1.0 - e * e) * v * sin(phi);
}

/* Convert latitude, longitude [deg] to Cartesian coordinates [m] */
void harp_wgs84_ellipsoid_latitude_and_longitude_from_cartesian_coordinates(double x, double y, double z,
                                                                            double *new_latitude, double *new_longitude)
{
    double rad2deg = (double)(CONST_RAD2DEG);
    double a = (double)(CONST_SEMI_MAJOR_AXIS_WGS84_ELLIPSOID);
    double e = (double)(CONST_ECCENTRICITY_WGS84_ELLIPSOID);
    double v = 0.0;
    double phi_c = 0.0;
    double phi_prev;
    double phi_next;
    double rho = sqrt(x * x + y * y);
    double hg;  /* Geodetic height */
    int iter = 0;
    double lambda;
    double phi;

    lambda = atan2(y, x);

    /* To obtain the geodetic latitude phi,
     * start with calculation of geocentric latitude phi_c (exact) as a first guess:*/
    if (rho == 0.0)
    {
        if (z == 0.0)
        {
            phi_c = 0.0;
        }
        else if (z > 0.0)
        {
            phi_c = 0.5 * M_PI;
        }
        else if (z < 0.0)
        {
            phi_c = -0.5 * M_PI;
        }
    }
    else
    {
        phi_c = atan(z / rho);
    }
    phi_prev = phi_c;

    /* Use 4 iterations */
    while (iter < 4)
    {
        v = a / sqrt(1.0 - e * e * sin(phi_prev) * sin(phi_prev));
        hg = rho / cos(phi_prev) - v;
        phi_next = atan(z / rho * (1.0 - e * e * v / (v + hg)));

        iter++;
    }

    phi = phi_next;
    v = a / sqrt(1.0 - e * e * sin(phi) * sin(phi));
    hg = rho / cos(phi) - v;

    *new_latitude = phi * rad2deg;
    *new_longitude = lambda * rad2deg;
}

/* Return the point distance [m] from the input latitudes and longitudes [deg] */
int harp_wgs84_ellipsoid_point_distance_from_latitude_and_longitude(double latitude_a, double longitude_a,
                                                                    double latitude_b, double longitude_b,
                                                                    double *new_point_distance)
{
    double point_distance;      /* Surface distance [m] */
    double deg2rad = (double)(CONST_DEG2RAD);
    double pi = (double)M_PI;
    double lambda_a = longitude_a * deg2rad;
    double lambda_b = longitude_b * deg2rad;
    double phi_a = latitude_a * deg2rad;
    double phi_b = latitude_b * deg2rad;
    double u_a;
    double u_b;
    double L;
    double u2;
    double sin_ua;
    double cos_ua;
    double sin_ub;
    double cos_ub;
    double sin_sigma = 0.0;
    double cos_sigma = 0.0;
    double cos_alpha = 0.0;
    double sin_lambda;
    double cos_lambda;
    double lambda;
    double A;
    double B;
    double C;
    double sigma = 0.0;
    double delta_sigma;
    double sin_alpha;
    double cos2alpha;
    double cos2sigmam = 0.0;
    double a = (double)(CONST_SEMI_MAJOR_AXIS_WGS84_ELLIPSOID);
    double f = (double)(CONST_FLATTENING_WGS84_ELLIPSOID);
    double b = (double)(CONST_SEMI_MINOR_AXIS_WGS84_ELLIPSOID);
    double lambda_difference_limit = 1.0e-12;
    double lambda_previous;
    int iteration_limit = 20;
    int iteration = 0;

    /* Calculate the reduced latitudes */
    u_a = atan((1.0 - f) * tan(phi_a));
    u_b = atan((1.0 - f) * tan(phi_b));
    sin_ua = sin(u_a);
    cos_ua = cos(u_a);
    sin_ub = sin(u_b);
    cos_ub = cos(u_b);

    /* Compute the difference in longitude */
    L = lambda_b - lambda_a;

    /* Start with lamda = L (first approximation) and set lambda_previous */
    lambda = L;
    lambda_previous = 2.0 * pi;

    /* Iterate until change in lambda is negligible */
    while (fabs(lambda - lambda_previous) > lambda_difference_limit && iteration <= iteration_limit)
    {
        sin_lambda = sin(lambda);
        cos_lambda = cos(lambda);

        sin_sigma =
            sqrt(cos_ub * cos_ub * sin_lambda * sin_lambda +
                 (cos_ub * sin_ua - sin_ua * cos_ub * cos_lambda) * (cos_ub * sin_ua - sin_ua * cos_ub * cos_lambda));

        if (sin_sigma == 0.0)
        {
            /* No coincidence points, set surface distance to 0.0 */
            point_distance = 0.0;
            *new_point_distance = point_distance;
            return 0;
        }

        cos_sigma = sin_ua * sin_ub + cos_ua * cos_ub * cos_lambda;
        sigma = atan2(sin_sigma, cos_sigma);

        sin_alpha = cos_ua * sin_ua * sin_lambda / sin_sigma;

        /* cos2alpha = cos(alpha) * cos(alpha) */
        cos2alpha = 1.0 - sin_alpha * sin_alpha;
        cos_alpha = sqrt(cos2alpha);

        /* cos2sigmam = cos(2.0 * sigma_m) */
        if (cos2alpha == 0.0)
        {
            /* This corresponds to equatorial lines */
            cos2sigmam = 0.0;
        }
        else
        {
            cos2sigmam = cos_sigma - 2.0 * sin_ua * sin_ub / cos2alpha;
        }

        C = f / 16.0 * cos2alpha * (4.0 - 3.0 * cos2alpha);

        lambda_previous = lambda;

        lambda =
            L + (1.0 - C) * f * sin_alpha * (sigma +
                                             C * sin_sigma * (cos2sigmam +
                                                              C * cos_sigma * (-1.0 + 2.0 * cos2sigmam * cos2sigmam)));

        iteration++;
    }

    u2 = cos_alpha * cos_alpha * (a * a - b * b) / (b * b);

    A = 1.0 + u2 / 16384.0 * (4096.0 + u2 * (-768.0 + u2 * (320.0 - 175.0 * u2)));
    B = u2 / 1024.0 * (256 + u2 * (-128.0 + u2 * (74.0 - 47.0 * u2)));

    delta_sigma =
        B * sin_sigma * (cos2sigmam +
                         B / 4.0 *
                         ((cos_sigma * (-1.0 + 2.0 * cos2sigmam * cos2sigmam) -
                           B / 6.0 * cos2sigmam * (-3.0 + 4.0 * sin_sigma * sin_sigma) * (-3.0 +
                                                                                          4.0 * cos2sigmam *
                                                                                          cos2sigmam))));

    point_distance = b * A * (sigma - delta_sigma);
    *new_point_distance = point_distance;
    return 0;
}
