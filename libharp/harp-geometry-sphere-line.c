/*
 * Copyright (C) 2015-2024 S[&]T, The Netherlands.
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

/* A spherical_line is defined by a length and
 *   an Euler transformation that defines
 *   the begin point of the line. This point is obtained
 *   by rotating (lat,lon) = (0,0) with three angles:
 *     phi    the first  rotation angle around z-axis
 *     theta  the second rotation angle around x-axis
 *     psi    the last   rotation angle around z-axis
 */

/* Convert a line to an Euler transformation */
static void euler_transformation_from_spherical_line(harp_euler_transformation *transformation,
                                                     const harp_spherical_line *line)
{
    harp_euler_transformation_set_to_zxz(transformation);

    transformation->phi = line->phi;
    transformation->theta = line->theta;
    transformation->psi = line->psi;
}

/* Convert a line to an inverse Euler transformation */
void harp_inverse_euler_transformation_from_spherical_line(harp_euler_transformation *inverse_transformation,
                                                           const harp_spherical_line *line)
{
    /* First, derive the not-inverted transformation */
    euler_transformation_from_spherical_line(inverse_transformation, line);

    /* Invert */
    harp_euler_transformation_invert(inverse_transformation);
}

/* Transform a spherical line using an Euler transformation */
static void spherical_line_apply_euler_transformation(harp_spherical_line *lineout, const harp_spherical_line *linein,
                                                      const harp_euler_transformation *transformation)
{
    harp_euler_transformation transformationtemp[2];

    euler_transformation_from_spherical_line(&transformationtemp[0], linein);

    harp_euler_transformation_transform_to_zxz_euler_transformation(&transformationtemp[1], &transformationtemp[0],
                                                                    transformation);
    lineout->phi = transformationtemp[1].phi;
    lineout->theta = transformationtemp[1].theta;
    lineout->psi = transformationtemp[1].psi;
    lineout->length = linein->length;
}

/* Swap the begin point and end point of a spherical line */
static void spherical_line_swap_begin_end(harp_spherical_line *lineout, const harp_spherical_line *linein)
{
    harp_euler_transformation transformation;
    harp_spherical_line linetemp;

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

    spherical_line_apply_euler_transformation(lineout, &linetemp, &transformation);
}

/* Check if two spherical lines are equal */
static int spherical_line_equal(const harp_spherical_line *line1, const harp_spherical_line *line2)
{
    if (HARP_GEOMETRY_FPne(line1->length, line2->length))
    {
        return 0;
    }
    else
    {
        harp_euler_transformation euler1, euler2;

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

/* Determine begin point of spherical line */
void harp_spherical_line_begin(harp_spherical_point *point, const harp_spherical_line *line)
{
    harp_spherical_point pointtmp = { 0.0, 0.0 };
    harp_euler_transformation euler;

    euler_transformation_from_spherical_line(&euler, line);
    harp_spherical_point_apply_euler_transformation(point, &pointtmp, &euler);
}

/* Determine end point of spherical line */
void harp_spherical_line_end(harp_spherical_point *point, const harp_spherical_line *line)
{
    harp_spherical_point pointtmp = { 0.0, 0.0 };
    harp_euler_transformation euler;

    pointtmp.lon = line->length;

    euler_transformation_from_spherical_line(&euler, line);
    harp_spherical_point_apply_euler_transformation(point, &pointtmp, &euler);
}

/* Returns 1, if the line, defined by two vectors, contains the point.
 * Otherwise returns 0. The input assumes normalized vectors and that the point
 * lies on the great circle of the line.
 */
static int8_t point_on_line(const harp_vector3d *line_begin, const harp_vector3d *line_end, const harp_vector3d *point)
{
    double theta_begin_point, theta_end_point, theta_line;

    /* Get the angle theta of the line and points.
     * The dot product is defined by:
     *
     *   a . b = ||a|| * ||b|| * cos(theta)
     *
     * if we re-arrange for theta we get
     *
     *   theta = acos((a . b) / (||a|| * ||b||))
     *
     * Since vectors are normalized we can simplify to:
     *
     *   theta = acos(a . b)
     */
    theta_begin_point = acos(harp_vector3d_dotproduct(line_begin, point));
    theta_end_point = acos(harp_vector3d_dotproduct(point, line_end));
    theta_line = acos(harp_vector3d_dotproduct(line_begin, line_end));

    /* If the angles from the start and end point of the line are equal to the
     * total angle of the line, then the point is on the line. */
    return HARP_GEOMETRY_FPeq(theta_begin_point + theta_end_point, theta_line);
}

/* Returns 1, if the two lines, each defined by two spherical points, intersect
 * or are equal. Returns 0 for connected lines, which are lines where one of
 * the points are equal, and for separate lines.
 *
 * \param p11 the starting point of first line
 * \param p12 the end point of first line
 * \param p21 the starting point of second line
 * \param p22 the end point of second line
 * \return
 *   \arg \c 1, Lines intersect or are equal
 *   \arg \c 0, Lines are separate or connected
 */
int8_t harp_spherical_line_intersects(const harp_spherical_point *p11, const harp_spherical_point *p12,
                                      const harp_spherical_point *p21, const harp_spherical_point *p22)
{
    /* The idea is to get the two intersection points of the great circles of
     * the lines, i.e. intersect the planes of the lines. We perform this in 3D
     * space. Then check if one of the intersection points is within the
     * boundaries of both lines by comparing the angles. */
    harp_vector3d v11, v12, v21, v22, n1, n2, i1, i2;
    double norm;

    /* Convert spherical points to normal vectors in 3D space */
    harp_vector3d_from_spherical_point(&v11, p11);
    harp_vector3d_from_spherical_point(&v12, p12);
    harp_vector3d_from_spherical_point(&v21, p21);
    harp_vector3d_from_spherical_point(&v22, p22);

    /* Compute normal of great circles, i.e. planes */
    harp_vector3d_crossproduct(&n1, &v11, &v12);
    harp_vector3d_crossproduct(&n2, &v21, &v22);

    /* Compute normal of normal of planes, this is equal to one intersection
     * point. */
    harp_vector3d_crossproduct(&i1, &n1, &n2);

    norm = harp_vector3d_norm(&i1);
    /* If the cross product is the zero vector then the lines are equal */
    if (norm == 0.0)
    {
        return 1;
    }
    else
    {
        /* Normalize intersection point */
        i1.x = i1.x / norm;
        i1.y = i1.y / norm;
        i1.z = i1.z / norm;

        /* The second intersection point is opposite of the first */
        i2.x = -i1.x;
        i2.y = -i1.y;
        i2.z = -i1.z;

        /* Return false, if an intersection point is equal to begin or end
         * point of a line */
        if (harp_vector3d_equal(&v11, &v21) || harp_vector3d_equal(&v11, &v22) || harp_vector3d_equal(&v12, &v21) ||
            harp_vector3d_equal(&v12, &v22))
        {
            return 0;
        }

        /* Check if the intersection points are within both original lines */
        return (point_on_line(&v11, &v12, &i1) && point_on_line(&v21, &v22, &i1)) ||
            (point_on_line(&v11, &v12, &i2) && point_on_line(&v21, &v22, &i2));
    }
}

int8_t harp_spherical_line_spherical_line_relationship(const harp_spherical_line *line1,
                                                       const harp_spherical_line *line2)
{
    harp_euler_transformation se;
    harp_spherical_line sl1, sl2;
    harp_spherical_point p[4];
    int a1, a2, switched;
    int res;

    switched = 0;

    if (spherical_line_equal(line1, line2))
    {
        return HARP_GEOMETRY_LINE_EQUAL;
    }

    spherical_line_swap_begin_end(&sl1, line1);
    if (spherical_line_equal(&sl1, line2))
    {
        return HARP_GEOMETRY_LINE_CONTAINS;
    }

    /* transform the larger line into equator ( begin at (0,0) ) */
    sl1.phi = sl1.theta = sl1.psi = 0.0;
    if (HARP_GEOMETRY_FPge(line1->length, line2->length))
    {
        harp_inverse_euler_transformation_from_spherical_line(&se, line1);
        sl1.length = line1->length;
        spherical_line_apply_euler_transformation(&sl2, line2, &se);
        switched = 0;
    }
    else if (HARP_GEOMETRY_FPge(line2->length, line1->length))
    {
        harp_inverse_euler_transformation_from_spherical_line(&se, line2);
        sl1.length = line2->length;
        spherical_line_apply_euler_transformation(&sl2, line1, &se);
        switched = 1;
    }
    else
    {
        /* length is NaN for at least one of the lines */
        return HARP_GEOMETRY_LINE_SEPARATE;
    }
    if (HARP_GEOMETRY_FPzero(sl1.length))
    {   /* both are points */
        return HARP_GEOMETRY_LINE_SEPARATE;
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
                return HARP_GEOMETRY_LINE_CONTAINED;
            }
            else
            {
                return HARP_GEOMETRY_LINE_CONTAINS;
            }
        }
        else if (a1)
        {
            if (HARP_GEOMETRY_FPeq(p[0].lon, p[2].lon) || HARP_GEOMETRY_FPeq(p[1].lon, p[2].lon))
            {
                return HARP_GEOMETRY_LINE_CONNECTED;
            }
            else
            {
                return HARP_GEOMETRY_LINE_OVERLAP;
            }
        }
        else if (a2)
        {
            if (HARP_GEOMETRY_FPeq(p[0].lon, p[3].lon) || HARP_GEOMETRY_FPeq(p[1].lon, p[3].lon))
            {
                return HARP_GEOMETRY_LINE_CONNECTED;
            }
            else
            {
                return HARP_GEOMETRY_LINE_OVERLAP;
            }
        }

        return HARP_GEOMETRY_LINE_SEPARATE;
    }

    /* Now sl2 is not at equator */
    res = 0;

    /* check connected lines */
    if (HARP_GEOMETRY_FPgt(sl2.length, 0.0))
    {
        if (harp_spherical_point_equal(&p[0], &p[2]))
        {
            res = (1 << HARP_GEOMETRY_LINE_CONNECTED);
        }

        if (harp_spherical_point_equal(&p[0], &p[3]))
        {
            res = (1 << HARP_GEOMETRY_LINE_CONNECTED);
        }

        if (harp_spherical_point_equal(&p[1], &p[2]))
        {
            res = (1 << HARP_GEOMETRY_LINE_CONNECTED);
        }

        if (harp_spherical_point_equal(&p[1], &p[3]))
        {
            res = (1 << HARP_GEOMETRY_LINE_CONNECTED);
        }
    }

    a1 = (HARP_GEOMETRY_FPge(p[2].lat, 0.0) && HARP_GEOMETRY_FPle(p[3].lat, 0.0));      /* sl2 crosses equator desc. */
    a2 = (HARP_GEOMETRY_FPle(p[2].lat, 0.0) && HARP_GEOMETRY_FPge(p[3].lat, 0.0));      /* sl1 crosses equator asc. */

    if (!(a1 || a2))
    {
        res |= (1 << HARP_GEOMETRY_LINE_SEPARATE);
    }
    else
    {
        harp_vector3d v[2][2];
        harp_spherical_point sp;

        /* Now we take the vectors of line's begin and end */
        harp_vector3d_from_spherical_point(&v[0][0], &p[0]);
        harp_vector3d_from_spherical_point(&v[0][1], &p[1]);
        harp_vector3d_from_spherical_point(&v[1][0], &p[2]);
        harp_vector3d_from_spherical_point(&v[1][1], &p[3]);

        if (v[0][1].x <= 0.0)
        {
            v[0][1].y = 1.0;
        }

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
            res |= (1 << HARP_GEOMETRY_LINE_SEPARATE);
        }
    }

    if (res == (1 << HARP_GEOMETRY_LINE_SEPARATE))
    {
        return HARP_GEOMETRY_LINE_SEPARATE;
    }

    if (res & (1 << HARP_GEOMETRY_LINE_CONNECTED))
    {
        return HARP_GEOMETRY_LINE_CONNECTED;
    }

    if (res & (1 << HARP_GEOMETRY_LINE_CROSS))
    {
        return HARP_GEOMETRY_LINE_CROSS;
    }

    return HARP_GEOMETRY_LINE_SEPARATE;
}

/* Return a meridian line for a given longitude [rad] */
static void spherical_line_meridian(harp_spherical_line *line, double lon)
{
    harp_spherical_point point;

    line->phi = -M_PI_2;
    line->theta = M_PI_2;

    point.lat = 0.0;
    point.lon = lon;

    harp_spherical_point_check(&point);

    line->psi = point.lon;
    line->length = M_PI;
}

/* Derive a spherical line from two spherical points */
void harp_spherical_line_from_spherical_points(harp_spherical_line *line, const harp_spherical_point *point_begin,
                                               const harp_spherical_point *point_end)
{
    /* Declare an Euler transformation */
    harp_euler_transformation se;

    /* Define the distance between begin and end point */
    double length;

    /* Calculate the distance between begin and end point */
    length = harp_spherical_point_distance(point_begin, point_end);

    /* Deal with special case that the line corresponds to a meridian. */
    if (HARP_GEOMETRY_FPeq(length, M_PI))
    {
        if (HARP_GEOMETRY_FPeq(point_begin->lon, point_end->lon))
        {
            spherical_line_meridian(line, point_begin->lon);
        }
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
}

/* Check if a point lies at a spherical line */
int harp_spherical_point_is_at_spherical_line(const harp_spherical_point *point, const harp_spherical_line *line)
{
    harp_euler_transformation euler_rotation_inverse;
    harp_spherical_point point_rotated;

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
