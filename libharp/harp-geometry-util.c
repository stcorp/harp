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

#include "harp-geometry.h"

#include <math.h>
#include <stdlib.h>

/** Calculate the point u on the greatcircle through p and q such that u is the average of p and q.
 *
 *        q
 *       /
 *      u
 *     /
 *    p
 *
 * u = (p + q) / 2
 * if p and q are on opposite sides of the sphere an average of the latitudes and longitudes is taken.
 */
void harp_geographic_average(double latitude_p, double longitude_p, double latitude_q, double longitude_q,
                             double *latitude_u, double *longitude_u)
{
    double theta_p = latitude_p * CONST_DEG2RAD;
    double phi_p = longitude_p * CONST_DEG2RAD;
    double theta_q = latitude_q * CONST_DEG2RAD;
    double phi_q = longitude_q * CONST_DEG2RAD;
    double theta_u;
    double phi_u;

    double px = cos(phi_p) * cos(theta_p);
    double py = sin(phi_p) * cos(theta_p);
    double pz = sin(theta_p);
    double qx = cos(phi_q) * cos(theta_q);
    double qy = sin(phi_q) * cos(theta_q);
    double qz = sin(theta_q);

    /* average p and q */
    double ux = 0.5 * (px + qx);
    double uy = 0.5 * (py + qy);
    double uz = 0.5 * (pz + qz);

    /* calculate ||u|| */
    double norm_u = sqrt(ux * ux + uy * uy + uz * uz);

    /* if ||u|| == 0 then p and q are on opposite sides of the sphere -> use simple lat/long average */
    if (norm_u == 0)
    {
        *latitude_u = (latitude_p + latitude_q) / 2;
        *longitude_u = (longitude_p + longitude_q) / 2;
        if (longitude_p - longitude_q > 180 || longitude_p - longitude_q < -180)
        {
            if (*longitude_u > 0)
            {
                *longitude_u -= 180;
            }
            else
            {
                *longitude_u += 180;
            }
        }
        return;
    }

    /* normalize u */
    ux = ux / norm_u;
    uy = uy / norm_u;
    uz = uz / norm_u;

    /* calculate phi_u and tau_u */
    theta_u = asin(uz);

    /* atan2 automatically 'does the right thing' ((ux,uy)=(0,0) -> pu=0) */
    phi_u = atan2(uy, ux);

    *latitude_u = theta_u * CONST_RAD2DEG;
    *longitude_u = phi_u * CONST_RAD2DEG;
}

/** Calculates the intersection point u of the greatcircles through p1/p2 and q1/q2
 * (given in latitude(tau)/longitude(phi) coordinates) where p1/p2/q1/q2 form a rectangular region
 *
 *    \        /
 *     q2    p2
 *       \  /
 *        u
 *       /  \
 *     p1    q1
 *    /        \
 *
 * The intersection point 'u' is calculated via: u = (p1 x p2) x (q1 x q2) (a cross product of cross products)
 */
void harp_geographic_intersection(double latitude_p1, double longitude_p1, double latitude_p2, double longitude_p2,
                                  double latitude_q1, double longitude_q1, double latitude_q2, double longitude_q2,
                                  double *latitude_u, double *longitude_u)
{
    double p1, t1, p2, t2;
    double cp1, sp1, ct1, st1;
    double cp2, sp2, ct2, st2;
    double x1, y1, z1, x2, y2, z2;
    double npx, npy, npz;       /* np = p1 x p2 */
    double nqx, nqy, nqz;       /* nq = q1 x q2 */
    double ux, uy, uz;  /* u = np x nq */
    double pu, tu;
    double norm_u;      /* ||u|| */

    /* calculate np */

    t1 = latitude_p1 * CONST_DEG2RAD;
    p1 = longitude_p1 * CONST_DEG2RAD;
    t2 = latitude_p2 * CONST_DEG2RAD;
    p2 = longitude_p2 * CONST_DEG2RAD;

    cp1 = cos(p1);
    sp1 = sin(p1);
    ct1 = cos(t1);
    st1 = sin(t1);
    cp2 = cos(p2);
    sp2 = sin(p2);
    ct2 = cos(t2);
    st2 = sin(t2);

    x1 = cp1 * ct1;
    y1 = sp1 * ct1;
    z1 = st1;

    x2 = cp2 * ct2;
    y2 = sp2 * ct2;
    z2 = st2;

    /* np = (x1,y1,z1) x (x2,y2,z2) (cross product) */
    npx = y1 * z2 - z1 * y2;
    npy = -(x1 * z2 - z1 * x2);
    npz = x1 * y2 - y1 * x2;


    /* calculate nq */

    t1 = latitude_q1 * CONST_DEG2RAD;
    p1 = longitude_q1 * CONST_DEG2RAD;
    t2 = latitude_q2 * CONST_DEG2RAD;
    p2 = longitude_q2 * CONST_DEG2RAD;

    cp1 = cos(p1);
    sp1 = sin(p1);
    ct1 = cos(t1);
    st1 = sin(t1);
    cp2 = cos(p2);
    sp2 = sin(p2);
    ct2 = cos(t2);
    st2 = sin(t2);

    x1 = cp1 * ct1;
    y1 = sp1 * ct1;
    z1 = st1;

    x2 = cp2 * ct2;
    y2 = sp2 * ct2;
    z2 = st2;

    /* nq = (x1,y1,z1) x (x2,y2,z2) (cross product) */
    nqx = y1 * z2 - z1 * y2;
    nqy = -(x1 * z2 - z1 * x2);
    nqz = x1 * y2 - y1 * x2;

    /* calculate u */

    /* u = (npx,npy,npz) x (nqx,nqy,nqz) (cross product) */
    ux = npy * nqz - npz * nqy;
    uy = -(npx * nqz - npz * nqx);
    uz = npx * nqy - npy * nqx;

    /* calculate ||u|| */
    norm_u = sqrt(ux * ux + uy * uy + uz * uz);

    /* if ||u|| == 0 then p1/p2 and q1/q2 produce the same greatcircle and
     * we can't interpolate -> return NaN values
     */
    if (norm_u == 0)
    {
        *latitude_u = harp_nan();
        *longitude_u = harp_nan();
        return;
    }

    /* normalize u */
    ux = ux / norm_u;
    uy = uy / norm_u;
    uz = uz / norm_u;

    /* calculate phi_u and tau_u */
    tu = asin(uz);

    /* atan2 automatically 'does the right thing' ((ux,uy)=(0,0) -> pu=0) */
    pu = atan2(uy, ux);

    *latitude_u = tu * CONST_RAD2DEG;
    *longitude_u = pu * CONST_RAD2DEG;
}

/** Calculate the point u on the greatcircle through p and q such that u is as far from p as p is from q.
 *
 *        u
 *       /
 *      p
 *     /
 *    q
 *
 * u = 2(p.q)p - q,
 * or in words: u is -q plus 2 times the projection of q on the vector p
 * the projection of q on p is the inproduct of p and q in the direction of the unit vector p.
 */
void harp_geographic_extrapolation(double latitude_p, double longitude_p, double latitude_q, double longitude_q,
                                   double *latitude_u, double *longitude_u)
{
    double tp = latitude_p * CONST_DEG2RAD;
    double pp = longitude_p * CONST_DEG2RAD;
    double tq = latitude_q * CONST_DEG2RAD;
    double pq = longitude_q * CONST_DEG2RAD;

    double cpp = cos(pp);
    double spp = sin(pp);
    double ctp = cos(tp);
    double stp = sin(tp);
    double cpq = cos(pq);
    double spq = sin(pq);
    double ctq = cos(tq);
    double stq = sin(tq);

    double px = cpp * ctp;
    double py = spp * ctp;
    double pz = stp;

    double qx = cpq * ctq;
    double qy = spq * ctq;
    double qz = stq;

    double inprod = px * qx + py * qy + pz * qz;

    double ux = 2 * inprod * px - qx;
    double uy = 2 * inprod * py - qy;
    double uz = 2 * inprod * pz - qz;

    /* calculate phi_u and tau_u */
    double tu = asin(uz);

    /* atan2 automatically 'does the right thing' ((ux,uy)=(0,0) -> pu=0) */
    double pu = atan2(uy, ux);

    *latitude_u = tu * CONST_RAD2DEG;
    *longitude_u = pu * CONST_RAD2DEG;
}

int harp_geographic_center_from_bounds(long num_vertices, const double *latitude_bounds, const double *longitude_bounds,
                                       double *center_latitude, double *center_longitude)
{
    harp_spherical_polygon *polygon = NULL;
    harp_vector3d vector_center;
    harp_spherical_point point;

    /* Convert to a spherical polygon */
    if (harp_spherical_polygon_from_latitude_longitude_bounds(0, num_vertices, latitude_bounds, longitude_bounds,
                                                              &polygon) != 0)
    {
        return -1;
    }

    /* Derive the centre point coordinates */
    if (harp_spherical_polygon_centre(&vector_center, polygon) != 0)
    {
        harp_spherical_polygon_delete(polygon);
        return -1;
    }

    harp_spherical_point_from_vector3d(&point, &vector_center);
    harp_spherical_point_check(&point);
    harp_spherical_point_deg_from_rad(&point);
    *center_latitude = point.lat;
    *center_longitude = point.lon;
    harp_spherical_polygon_delete(polygon);

    return 0;
}
