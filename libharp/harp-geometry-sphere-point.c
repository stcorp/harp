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

#include <assert.h>
#include <math.h>
#include <stdlib.h>

/* Check if two spherical points are equal */
int harp_spherical_point_equal(const harp_spherical_point *pointa, const harp_spherical_point *pointb)
{
    harp_vector3d vectora, vectorb;

    harp_vector3d_from_spherical_point(&vectora, pointa);
    harp_vector3d_from_spherical_point(&vectorb, pointb);

    return (harp_vector3d_equal(&vectora, &vectorb));
}

/* Check spherical point */
void harp_spherical_point_check(harp_spherical_point *point)
{
    int lat_is_negative = (point->lat < 0.0);

    point->lat = point->lat - floor(point->lat / (2.0 * M_PI)) * (2.0 * M_PI);
    point->lon = point->lon - floor(point->lon / (2.0 * M_PI)) * (2.0 * M_PI);

    if (point->lon < 0.0)
    {
        point->lon += 2.0 * M_PI;
    }

    if (point->lat > M_PI)
    {
        point->lat -= 2.0 * M_PI;
    }
    if (point->lat > M_PI_2)
    {
        point->lat = M_PI - point->lat;
        point->lon += ((point->lon < M_PI) ? (M_PI) : (-M_PI));
    }
    if (point->lat < -M_PI_2)
    {
        point->lat = (double)(-M_PI) - point->lat;
        point->lon += ((point->lon < (double)M_PI) ? ((double)M_PI) : ((double)(-M_PI)));
    }

    if (HARP_GEOMETRY_FPeq(point->lat, M_PI_2) && lat_is_negative)
    {
        point->lat = -M_PI_2;
    }

    if (HARP_GEOMETRY_FPeq(point->lon, 2.0 * M_PI))
    {
        point->lon = 0.0;
    }

    if (HARP_GEOMETRY_FPzero(point->lon))
    {
        point->lon = 0.0;
    }

    if (HARP_GEOMETRY_FPzero(point->lat))
    {
        point->lat = 0.0;
    }
}

/* Convert spherical point (lat,lon) to
 * point (x,y,z) in Cartesian coordinates
 *
 * Input:
 *   Spherical point p = (p.lat, p.lon) in [rad]
 *
 * Output:
 *   3D vector vector = (vector.x, vector.y, vector.z) [dl]
 *
 * Details:
 *
 *   Convert (lat,lon) coordinates on unit sphere to
 *   Cartesian coordinates (x,y,z) with:
 *
 *     x = cos(lat) * cos(lon);
 *     y = cos(lat) * sin(lon);
 *     z = sin(lat)
 *
 *   Here, (lat,lon) are in [rad].
 */
void harp_vector3d_from_spherical_point(harp_vector3d *vector, const harp_spherical_point *point)
{
    double sinlat = sin(point->lat);
    double sinlon = sin(point->lon);
    double coslat = cos(point->lat);
    double coslon = cos(point->lon);

    vector->x = coslat * coslon;
    vector->y = coslat * sinlon;
    vector->z = sinlat;
}

/* Convert point (x,y,z) in Cartesian coordinates to
 * spherical point (lat,lon)
 *
 * Input:
 *   3D vector vector = (vector.x, vector.y, vector.z) [dl]
 *
 * Output:
 *   Spherical point p = (p.lat, p.lon) in [rad]
 */
void harp_spherical_point_from_vector3d(harp_spherical_point *point, const harp_vector3d *vector)
{
    /* Calculate the radius in the (x,y)-plane */
    double rho = sqrt(vector->x * vector->x + vector->y * vector->y);

    if (rho == 0.0)
    {
        if (vector->z == 0.0)
        {
            point->lat = 0.0;
        }
        else if (vector->z > 0.0)
        {
            point->lat = M_PI_2;
        }
        else if (vector->z < 0.0)
        {
            point->lat = -M_PI_2;
        }
    }
    else
    {
        point->lat = atan(vector->z / rho);
    }
    point->lon = atan2(vector->y, vector->x);
}

/* Convert unit of spherical point (lat,lon)
 * from [deg] to [rad]
 *
 * Input:
 *   Spherical point p = (p.lat, p.lon) in [deg]
 *
 * Output:
 *   Same point in [rad]

 * Details:
 *   Conversion factor to convert from
 *   [deg] to [rad] = pi/180
 */
void harp_spherical_point_rad_from_deg(harp_spherical_point *point)
{
    point->lat = point->lat * (double)(CONST_DEG2RAD);
    point->lon = point->lon * (double)(CONST_DEG2RAD);
}

/* Convert unit of spherical point (lat,lon)
 * from [rad] to [deg]
 *
 * Input:
 *   Spherical point p = (p.lat, p.lon) in [rad]
 *
 * Output:
 *   Same point in [deg]
 *
 * Details:
 *   Conversion factor to convert from
 *   [rad] to [deg] = 180/pi
*/
void harp_spherical_point_deg_from_rad(harp_spherical_point *point)
{
    point->lat = point->lat * (double)(CONST_RAD2DEG);
    point->lon = point->lon * (double)(CONST_RAD2DEG);
}

/* Calculate the surface_distance between two points
 * on the surface of a sphere */
double harp_spherical_point_distance(const harp_spherical_point *pointp, const harp_spherical_point *pointq)
{
    double distance = acos(sin(pointp->lat) * sin(pointq->lat) + cos(pointp->lat) * cos(pointq->lat) *
                           cos(pointp->lon - pointq->lon));

    if (HARP_GEOMETRY_FPzero(distance))
    {
        return 0.0;
    }
    else
    {
        return distance;
    }
}


/** Calculate the distance between two points on the surface of the Earth in meters
 * \ingroup harp_geometry
 * This function assumes a spherical earth
 * \param latitude_a Latitude of first point
 * \param longitude_a Longitude of first point
 * \param latitude_b Latitude of second point
 * \param longitude_b Longitude of second point
 * \param distance Pointer to the C variable where the surface distance in [m] between the two points will be stored.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_geometry_get_point_distance(double latitude_a, double longitude_a, double latitude_b,
                                                 double longitude_b, double *distance)
{
    harp_spherical_point point_a, point_b;

    point_a.lat = latitude_a * (double)(CONST_DEG2RAD);
    point_a.lon = longitude_a * (double)(CONST_DEG2RAD);
    point_b.lat = latitude_b * (double)(CONST_DEG2RAD);
    point_b.lon = longitude_b * (double)(CONST_DEG2RAD);

    harp_spherical_point_check(&point_a);
    harp_spherical_point_check(&point_b);

    *distance = harp_spherical_point_distance(&point_a, &point_b) * CONST_EARTH_RADIUS_WGS84_SPHERE;

    return 0;
}
