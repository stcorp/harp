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

#include "harp-geometry.h"

#include <math.h>

/* Define all possible relationships between spherical lines */
const int8_t harp_spherical_line_relationship_avoid = HARP_GEOMETRY_LINE_AVOID; /* line avoids other line */
const int8_t harp_spherical_line_relationship_equal = HARP_GEOMETRY_LINE_EQUAL; /* lines are equal */
const int8_t harp_spherical_line_relationship_contain_line = HARP_GEOMETRY_LINE_CONT_LINE;      /* line contains line */
const int8_t harp_spherical_line_relationship_cross = HARP_GEOMETRY_LINE_CROSS; /* lines cross each other */
const int8_t harp_spherical_line_relationship_connect = HARP_GEOMETRY_LINE_CONNECT;     /* line are "connected" */
const int8_t harp_spherical_line_relationship_overlap = HARP_GEOMETRY_LINE_OVER;        /* lines overlap each other */

/* A spherical_line is defined by a length and
 *   an Euler transformation that defines
 *   the begin point of the line. This point is obtained
 *   by rotating (lat,lon) = (0,0) with three angles:
 *     phi    the first  rotation angle around z-axis
 *     theta  the second rotation angle around x-axis
 *     psi    the last   rotation angle around z-axis
 */

/* Convert a line to an inverse Euler transformation */
void harp_inverse_euler_transformation_from_spherical_line(harp_euler_transformation *inverse_transformation,
                                                           const harp_spherical_line *line)
{
    /* First, derive the not-inverted transformation */
    harp_euler_transformation_from_spherical_line(inverse_transformation, line);

    /* Invert */
    harp_euler_transformation_invert(inverse_transformation);
}

/* Convert a line to an Euler transformation */
void harp_euler_transformation_from_spherical_line(harp_euler_transformation *transformation,
                                                   const harp_spherical_line *line)
{
    harp_euler_transformation_set_to_zxz(transformation);

    transformation->phi = line->phi;
    transformation->theta = line->theta;
    transformation->psi = line->psi;
}

/* Swap the begin point and end point of a spherical line */
static void harp_spherical_line_swap_begin_end(harp_spherical_line *lineout, const harp_spherical_line *linein)
{
    static harp_spherical_line linetemp;

    static harp_euler_transformation transformation;

    /* Define a temporary line */
    linetemp.length = linein->length;

    /* Rotate the temporary line around the Z-axis */
    linetemp.phi = -linein->length;
    linetemp.theta = M_PI;
    linetemp.psi = 0.0;

    /* Set the Euler transformation */
    harp_euler_transformation_set_to_zxz(&transformation);

    transformation.phi = linein->phi;
    transformation.theta = linein->theta;
    transformation.psi = linein->psi;

    harp_spherical_line_apply_euler_transformation(lineout, &linetemp, &transformation);
}

/* Check if two spherical lines are equal */
int harp_spherical_line_equal(const harp_spherical_line *line1, const harp_spherical_line *line2)
{
    if (HARP_GEOMETRY_FPne(line1->length, line2->length))
    {
        return 0;
    }
    else
    {
        static harp_euler_transformation euler1, euler2;

        harp_euler_transformation_set_to_zxz(&euler1);
        harp_euler_transformation_set_to_zxz(&euler2);

        euler1.phi = line1->phi;
        euler1.theta = line1->theta;
        euler1.psi = line1->psi;

        euler2.phi = (HARP_GEOMETRY_FPeq(line2->length, 2.0 * M_PI)) ? (line1->phi) : (line2->phi);
        euler2.theta = line2->theta;
        euler2.psi = line2->psi;

        return (harp_euler_transformation_equal(&euler1, &euler2));
    }

    return 0;
}

/* Transform a spherical line using an Euler transformation */
void harp_spherical_line_apply_euler_transformation(harp_spherical_line *lineout, const harp_spherical_line *linein,
                                                    const harp_euler_transformation *transformation)
{
    static harp_euler_transformation transformationtemp[2];

    harp_euler_transformation_from_spherical_line(&transformationtemp[0], linein);

    harp_euler_transformation_transform_to_zxz_euler_transformation(&transformationtemp[1], &transformationtemp[0],
                                                                    transformation);
    lineout->phi = transformationtemp[1].phi;
    lineout->theta = transformationtemp[1].theta;
    lineout->psi = transformationtemp[1].psi;
    lineout->length = linein->length;
}

/* Determine begin point of spherical line */
void harp_spherical_line_begin(harp_spherical_point *point, const harp_spherical_line *line)
{
    static harp_spherical_point pointtmp = { 0.0, 0.0 };
    static harp_euler_transformation euler;

    harp_euler_transformation_from_spherical_line(&euler, line);
    harp_spherical_point_apply_euler_transformation(point, &pointtmp, &euler);
}

/* Determine end point of spherical line */
void harp_spherical_line_end(harp_spherical_point *point, const harp_spherical_line *line)
{
    static harp_spherical_point pointtmp = { 0.0, 0.0 };
    static harp_euler_transformation euler;

    pointtmp.lon = line->length;

    harp_euler_transformation_from_spherical_line(&euler, line);
    harp_spherical_point_apply_euler_transformation(point, &pointtmp, &euler);
}

/* Return a point at a line at given length position */
int harp_spherical_line_point_by_length(harp_spherical_point *point, const harp_spherical_line *line, double length)
{
    static harp_euler_transformation se;
    static harp_spherical_point sp = { 0.0, 0.0 };

    if (0.0 > length || length > line->length)
    {
        /* Return false */
        return 0;
    }

    harp_euler_transformation_set_to_zxz(&se);

    se.phi = line->phi;
    se.theta = line->theta;
    se.psi = line->psi;

    sp.lon = length;

    harp_spherical_point_apply_euler_transformation(point, &sp, &se);

    /* Return true */
    return 1;
}

int8_t harp_spherical_line_spherical_line_relationship(const harp_spherical_line *line1,
                                                       const harp_spherical_line *line2)
{
    static harp_euler_transformation se;
    static harp_spherical_line sl1, sl2;
    static harp_spherical_point p[4];
    static int a1, a2, switched;
    static double i, k, mi, mk;
    static const double step = (M_PI - 0.1);
    static int res;

    switched = 0;

    if (harp_spherical_line_equal(line1, line2))
    {
        return HARP_GEOMETRY_LINE_EQUAL;
    }

    harp_spherical_line_swap_begin_end(&sl1, line1);
    if (harp_spherical_line_equal(&sl1, line2))
    {
        return HARP_GEOMETRY_LINE_CONT_LINE;
    }

    /* transform the larger line into equator ( begin at (0,0) ) */
    sl1.phi = sl1.theta = sl1.psi = 0.0;
    if (HARP_GEOMETRY_FPge(line1->length, line2->length))
    {
        harp_inverse_euler_transformation_from_spherical_line(&se, line1);
        sl1.length = line1->length;
        harp_spherical_line_apply_euler_transformation(&sl2, line2, &se);
        switched = 0;
    }
    else if (HARP_GEOMETRY_FPge(line2->length, line1->length))
    {
        harp_inverse_euler_transformation_from_spherical_line(&se, line2);
        sl1.length = line2->length;
        harp_spherical_line_apply_euler_transformation(&sl2, line1, &se);
        switched = 1;
    }
    if (HARP_GEOMETRY_FPzero(sl1.length))
    {   /* both are points */
        return HARP_GEOMETRY_LINE_AVOID;
    }

    harp_spherical_line_begin(&p[0], &sl1);
    harp_spherical_line_end(&p[1], &sl1);
    harp_spherical_line_begin(&p[2], &sl2);
    harp_spherical_line_end(&p[3], &sl2);

    /* Check, sl2 is at equator */
    if (HARP_GEOMETRY_FPzero(p[2].lat) && HARP_GEOMETRY_FPzero(p[3].lat))
    {
        a1 = harp_spherical_point_is_at_spherical_line(&p[2], &sl1);
        a2 = harp_spherical_point_is_at_spherical_line(&p[3], &sl1);

        if (a1 && a2)
        {
            if (switched)
            {
                return HARP_GEOMETRY_LINE_OVER;
            }
            else
            {
                return HARP_GEOMETRY_LINE_CONT_LINE;
            }
        }
        else if (a1 || a2)
        {
            return HARP_GEOMETRY_LINE_OVER;
        }

        return HARP_GEOMETRY_LINE_AVOID;
    }

    /* Now sl2 is not at equator */
    res = 0;

    /* check connected lines */
    if (HARP_GEOMETRY_FPgt(sl2.length, 0.0))
    {
        if (harp_spherical_point_equal(&p[0], &p[2]))
        {
            res = (1 << HARP_GEOMETRY_LINE_CONNECT);
        }

        if (harp_spherical_point_equal(&p[0], &p[3]))
        {
            res = (1 << HARP_GEOMETRY_LINE_CONNECT);
        }

        if (harp_spherical_point_equal(&p[1], &p[2]))
        {
            res = (1 << HARP_GEOMETRY_LINE_CONNECT);
        }

        if (harp_spherical_point_equal(&p[1], &p[3]))
        {
            res = (1 << HARP_GEOMETRY_LINE_CONNECT);
        }
    }

    /* split lines in segments less than 180 degrees and check for each of it */
    mi = sl1.length / step;
    mk = sl2.length / step;

    if (HARP_GEOMETRY_FPzero(mk))
    {
        mk = 0.1;       /* force one loop for sl2 */
    }

    sl2.psi += step;

    for (i = 0.0; i < mi; i += 1.0)
    {
        sl2.psi -= step;

        if ((i + 1) >= mi)
        {
            harp_spherical_line_point_by_length(&p[0], &sl1, 0.0);
            harp_spherical_line_point_by_length(&p[1], &sl1, sl1.length - (i * step));
        }
        else if (i == 0.0)
        {
            harp_spherical_line_point_by_length(&p[0], &sl1, 0.0);
            harp_spherical_line_point_by_length(&p[1], &sl1, step);
        }

        for (k = 0.0; k < mk; k += 1.0)
        {
            harp_spherical_line_point_by_length(&p[2], &sl2, k * step);
            harp_spherical_line_point_by_length(&p[3], &sl2, (((k + 1.0) > mk) ? (sl2.length) : ((k + 1.0) * step)));

            a1 = (HARP_GEOMETRY_FPge(p[2].lat, 0.0) && HARP_GEOMETRY_FPle(p[3].lat, 0.0));      /* sl2 crosses equator desc. */
            a2 = (HARP_GEOMETRY_FPle(p[2].lat, 0.0) && HARP_GEOMETRY_FPge(p[3].lat, 0.0));      /* sl1 crosses equator asc. */

            if (!(a1 || a2))
            {
                res |= (1 << HARP_GEOMETRY_LINE_AVOID);
            }
            else
            {
                static harp_vector3d v[2][2];
                static int lbeg, lend;
                static harp_spherical_point sp;

                /* Now we take the vectors of line's begin and end */
                harp_vector3d_from_spherical_point(&v[0][0], &p[0]);
                harp_vector3d_from_spherical_point(&v[0][1], &p[1]);
                harp_vector3d_from_spherical_point(&v[1][0], &p[2]);
                harp_vector3d_from_spherical_point(&v[1][1], &p[3]);

                if (v[0][1].x <= 0.0)
                {
                    v[0][1].y = 1.0;
                }

                /* check whether sl2's longitudes are in sl1 range ( begin and end ) */
                lbeg = HARP_GEOMETRY_FPle(v[0][1].x, v[1][0].x) &&
                    HARP_GEOMETRY_FPle(v[1][0].x, 1.0) &&
                    HARP_GEOMETRY_FPle(0.0, v[1][0].y) && HARP_GEOMETRY_FPle(v[1][0].y, v[0][1].y);

                lend = HARP_GEOMETRY_FPle(v[0][1].x, v[1][1].x) &&
                    HARP_GEOMETRY_FPle(v[1][1].x, 1.0) &&
                    HARP_GEOMETRY_FPle(0.0, v[1][1].y) && HARP_GEOMETRY_FPle(v[1][1].y, v[0][1].y);

                (void)lbeg;     /* Prevent compiler warning */
                (void)lend;

                harp_inverse_euler_transformation_from_spherical_line(&se, &sl2);

                sp.lat = 0;
                sp.lon = ((a1) ? (M_PI) : (0.0)) - se.phi;      /* node */

                harp_spherical_point_check(&sp);

                if (HARP_GEOMETRY_FPge(sp.lon, 0.0) && HARP_GEOMETRY_FPle(sp.lon, p[1].lon))
                {
                    res |= (1 << HARP_GEOMETRY_LINE_CROSS);
                }
                else
                {
                    res |= (1 << HARP_GEOMETRY_LINE_AVOID);
                }
            }
        }
    }

    if (res == (1 << HARP_GEOMETRY_LINE_AVOID))
    {
        return HARP_GEOMETRY_LINE_AVOID;
    }

    if (res & (1 << HARP_GEOMETRY_LINE_CONNECT))
    {
        return HARP_GEOMETRY_LINE_CONNECT;
    }

    if (res & (1 << HARP_GEOMETRY_LINE_CONT_LINE))
    {
        return HARP_GEOMETRY_LINE_CONT_LINE;
    }

    if (res & (1 << HARP_GEOMETRY_LINE_CROSS))
    {
        return HARP_GEOMETRY_LINE_CROSS;
    }

    return HARP_GEOMETRY_LINE_AVOID;
}

/* Return a meridian line for a given longitude [rad] */
void harp_spherical_line_meridian(harp_spherical_line *line, double lon)
{
    static harp_spherical_point point;

    line->phi = -M_PI_2;
    line->theta = M_PI_2;

    point.lat = 0.0;
    point.lon = lon;

    harp_spherical_point_check(&point);

    line->psi = point.lon;
    line->length = M_PI;
}

/* Derive a spherical line from two spherical points */
int harp_spherical_line_from_spherical_points(harp_spherical_line *line, const harp_spherical_point *point_begin,
                                              const harp_spherical_point *point_end)
{
    /* Declare an Euler transformation */
    static harp_euler_transformation se;

    /* Define the distance between begin and end point */
    static double length;

    /* Calculate the distance between begin and end point */
    length = harp_spherical_point_distance(point_begin, point_end);

    /* Deal with special case that the distance between begin and end point is exactly 180 deg. */
    /* Then, the line corresponds to a meridian. */
    if (HARP_GEOMETRY_FPeq(length, M_PI))
    {
        if (HARP_GEOMETRY_FPeq(point_begin->lon, point_end->lon))
        {
            harp_spherical_line_meridian(line, point_begin->lon);
            return 1;   /* true */
        }
        return 0;       /* false */
    }

    /* Transform the spherical point to an Euler transformation */
    if (HARP_GEOMETRY_FPeq(length, 0))
    {
        line->phi = M_PI_2;
        line->theta = point_begin->lat;
        line->psi = point_begin->lon - M_PI_2;
        line->length = 0.0;
    }
    else
    {
        /* A spherical line is defined with starting point (0,0) and ending point (length, 0)
           that is transformed with a ZXZ Euler transform with angles (phi, theta, psi) */
        harp_euler_transformation_from_spherical_vector(&se, point_begin, point_end);
        line->phi = se.phi;
        line->theta = se.theta;
        line->psi = se.psi;
        line->length = length;
    }

    return 1;   /* true */
}

/* Check if a point lies at a spherical line */
int harp_spherical_point_is_at_spherical_line(const harp_spherical_point *point, const harp_spherical_line *line)
{
    static harp_euler_transformation euler_rotation_inverse;
    static harp_spherical_point point_rotated;

    /* Derive the Euler transformation from the input line */
    harp_inverse_euler_transformation_from_spherical_line(&euler_rotation_inverse, line);

    /* Rotate the point */
    harp_spherical_point_apply_euler_transformation(&point_rotated, point, &euler_rotation_inverse);

    /* Check the rotated point */
    if (HARP_GEOMETRY_FPzero(point_rotated.lat))
    {
        if (HARP_GEOMETRY_FPge(point_rotated.lon, 0.0) && HARP_GEOMETRY_FPle(point_rotated.lon, line->length))
        {
            return 1;
        }
        else
        {
            return 0;
        }
    }
    else
    {
        return 0;
    }
}

/* Calculates the intersection point u of the greatcircles through p1/p2 and q1/q2
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
void harp_spherical_line_spherical_line_intersection_point(const harp_spherical_line *line_p,
                                                           const harp_spherical_line *line_q,
                                                           harp_spherical_point *point_u)
{
    harp_spherical_point point_p1, point_p2, point_q1, point_q2;
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

    harp_spherical_line_begin(&point_p1, line_p);
    harp_spherical_line_end(&point_p2, line_p);
    harp_spherical_line_begin(&point_q1, line_q);
    harp_spherical_line_end(&point_q2, line_q);

    t1 = point_p1.lat;  /* in rad */
    p1 = point_p1.lon;
    t2 = point_p2.lat;
    p2 = point_p2.lon;

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

    t1 = point_q1.lat;
    p1 = point_q1.lon;
    t2 = point_q2.lat;
    p2 = point_q2.lon;

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
        point_u->lat = harp_nan();
        point_u->lon = harp_nan();
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

    point_u->lat = tu;
    point_u->lon = pu;  /* in rad */

    harp_spherical_point_check(point_u);
}

/* Derive line segment from spherical polygon */
int harp_spherical_polygon_get_segment(harp_spherical_line *line, const harp_spherical_polygon *polygon, int32_t i)
{
    /* First, make sure that the index is valid */
    if (i >= 0 && i < polygon->numberofpoints)
    {
        harp_spherical_point point_begin;
        harp_spherical_point point_end;

        /* We are dealing with index of last point;
           derive the line segment connecting last point with first point of polygon */
        if (i == (polygon->numberofpoints - 1))
        {
            point_begin = polygon->point[i];

            point_end = polygon->point[0];

            harp_spherical_line_from_spherical_points(line, &point_begin, &point_end);
        }
        /* Derive the line segment connecting the current point with the nex point */
        else
        {
            point_begin = polygon->point[i];
            point_end = polygon->point[i + 1];

            harp_spherical_line_from_spherical_points(line, &point_begin, &point_end);
        }

        return 0;
    }
    /* The index is outside the valid range */
    else
    {
        return -1;
    }
}

/* Point-line distance in 3D
 *
 * Given a point u = (xu,yu,zu)
 * and the begin and end point of a line sement, p = (xp,yp,zp) and q = (xq,yq,zq),
 * calculate the point-line distance:
 *
 *    d = |(u-p) x (u-q)| / | p-q |
 */
double harp_spherical_line_spherical_point_distance(const harp_spherical_line *line, const harp_spherical_point *point)
{
    harp_spherical_point point_begin, point_end;
    harp_vector3d p, q, u, u_min_p, u_min_q, p_min_q, cross_product;
    double norm_cross_product, norm_p_min_q, d;

    /* Convert all points to Cartesian coordinates */
    harp_spherical_line_begin(&point_begin, line);
    harp_spherical_line_end(&point_end, line);

    harp_vector3d_from_spherical_point(&p, &point_begin);
    harp_vector3d_from_spherical_point(&q, &point_end);
    harp_vector3d_from_spherical_point(&u, point);

    /* Calculate u-p, u-q, and p-q */
    u_min_p.x = u.x - p.x;
    u_min_p.y = u.y - p.y;
    u_min_p.z = u.z - p.z;

    u_min_q.x = u.x - q.x;
    u_min_q.y = u.y - q.y;
    u_min_q.z = u.z - q.z;

    p_min_q.x = p.x - q.x;
    p_min_q.y = p.y - q.y;
    p_min_q.z = p.z - q.z;

    /* Calculate |(u-p) x (u-q)| */
    harp_vector3d_crossproduct(&cross_product, &u_min_p, &u_min_q);
    norm_cross_product = harp_vector3d_norm(&cross_product);

    /* Calculate | p-q | */
    norm_p_min_q = harp_vector3d_norm(&p_min_q);

    if (norm_p_min_q == 0.0)
    {
        d = harp_nan();
    }
    else
    {
        d = norm_cross_product / norm_p_min_q;
    }

    return d;
}

/* Point-line distance in 3D
 *
 * Given a point u = (xu,yu,zu)
 * and the begin and end point of a line sement, p = (xp,yp,zp) and q = (xq,yq,zq),
 * calculate the point-line distance:
 *
 *    d = |(u-p) x (u-q)| / | p-q |
 */
double harp_spherical_line_spherical_point_distance_in_meters(const harp_spherical_line *line,
                                                              const harp_spherical_point *point)
{
    harp_spherical_point point_begin, point_end;
    harp_vector3d p, q, u, u_min_p, u_min_q, p_min_q, cross_product;
    double norm_cross_product, norm_p_min_q, d;

    /* Convert all points to Cartesian coordinates */
    harp_spherical_line_begin(&point_begin, line);
    harp_spherical_line_end(&point_end, line);

    harp_vector3d_from_spherical_point(&p, &point_begin);
    harp_vector3d_from_spherical_point(&q, &point_end);
    harp_vector3d_from_spherical_point(&u, point);

    /* Calculate u-p, u-q, and p-q */
    u_min_p.x = u.x - p.x;
    u_min_p.y = u.y - p.y;
    u_min_p.z = u.z - p.z;

    u_min_q.x = u.x - q.x;
    u_min_q.y = u.y - q.y;
    u_min_q.z = u.z - q.z;

    p_min_q.x = p.x - q.x;
    p_min_q.y = p.y - q.y;
    p_min_q.z = p.z - q.z;

    /* Calculate |(u-p) x (u-q)| */
    harp_vector3d_crossproduct(&cross_product, &u_min_p, &u_min_q);
    norm_cross_product = harp_vector3d_norm(&cross_product);

    /* Calculate | p-q | */
    norm_p_min_q = harp_vector3d_norm(&p_min_q);

    if (norm_p_min_q == 0.0)
    {
        d = harp_nan();
    }
    else
    {
        d = norm_cross_product / norm_p_min_q;
    }

    /* Multiply the radius of Earth sphere [m] */
    return (double)(CONST_EARTH_RADIUS_WGS84_SPHERE) * d;
}
