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

int harp_spherical_polygon_equal(const harp_spherical_polygon *polygon_a, const harp_spherical_polygon *polygon_b,
                                 int direction)
{
    int ret = 0;        /* false */

    if (polygon_a->numberofpoints == polygon_b->numberofpoints)
    {
        int32_t i, k, counter, shift;

        for (shift = 0; shift < polygon_a->numberofpoints; shift++)
        {
            counter = 0;

            for (i = 0; i < polygon_a->numberofpoints; i++)
            {
                k = (direction) ? (polygon_a->numberofpoints - i - 1) : (i);
                k += shift;
                k = (k < polygon_a->numberofpoints) ? (k) : (k - polygon_a->numberofpoints);

                if (harp_spherical_point_equal(&polygon_a->point[i], &polygon_b->point[k]))
                {
                    counter++;
                }
            }
            if (counter == polygon_a->numberofpoints)
            {
                ret = 1;        /* false */
                break;
            }
        }

        /* Try other direction, if not equal */
        if (!direction && !ret)
        {
            ret = harp_spherical_polygon_equal(polygon_a, polygon_b, 1);
        }
    }

    return ret;
}

/* Derive line segment from edge of polygon */
static int harp_spherical_line_segment_from_polygon(harp_spherical_line *line, const harp_spherical_polygon *polygon,
                                                    int32_t i)
{
    if (i >= 0 && i < polygon->numberofpoints)
    {
        if (i == (polygon->numberofpoints - 1))
        {
            harp_spherical_line_from_spherical_points(line, &polygon->point[i], &polygon->point[0]);
        }
        else
        {
            harp_spherical_line_from_spherical_points(line, &polygon->point[i], &polygon->point[i + 1]);
        }
        return 0;
    }
    else
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "index (%d) out of range [%d,%d)", i, 0, polygon->numberofpoints);

        return -1;
    }
}

/* Check the polygon for the following:
 * - is the centre of the polygon equal to the origin?
 * - are line segments crossing?
 * Return 0, if the centre is the 0-vector
 * or if the line segments are crossing. In other cases, return 1 (i.e. true)
 */
int harp_spherical_polygon_check(const harp_spherical_polygon *polygon)
{
    harp_spherical_line linei, linek;
    harp_vector3d vector;
    harp_spherical_point point;
    harp_euler_transformation se;
    int8_t relationship;        /* Relationship between lines */
    int32_t i, k;

    /* Centre should not correspond to 0-vector */
    harp_spherical_polygon_centre(&vector, (harp_spherical_polygon *)polygon);

    if (HARP_GEOMETRY_FPzero(vector.x) && HARP_GEOMETRY_FPzero(vector.y) && HARP_GEOMETRY_FPzero(vector.z))
    {
        /* return false */
        return 0;
    }

    /* Line segments should not cross each other */
    for (i = 0; i < polygon->numberofpoints; i++)
    {
        /* Grab line segment from polygon */
        harp_spherical_line_segment_from_polygon(&linei, polygon, i);

        for (k = (i + 1); k < polygon->numberofpoints; k++)
        {
            /* Grab line segment from polygon */
            harp_spherical_line_segment_from_polygon(&linek, polygon, k);

            /* Determine the relationship between two line segments */
            relationship = harp_spherical_line_spherical_line_relationship(&linei, &linek);

            /* Line segments should not cross each other, i.e. they should connect or avoid each other entirely */
            if (!(relationship == HARP_GEOMETRY_LINE_CONNECT || relationship == HARP_GEOMETRY_LINE_AVOID))
            {
                /* return false */
                return 0;
            }
        }
    }

    /* Convert Cartesian vector to spherical point on sphere */
    harp_spherical_point_from_vector3d(&point, &vector);

    /* Set ZXZ Euler transformation */
    harp_euler_transformation_set_to_zxz(&se);
    se.phi = -M_PI_2 - point.lon;
    se.theta = -M_PI_2 + point.lat;
    se.psi = 0.0;

    for (i = 0; i < polygon->numberofpoints; ++i)
    {
        harp_spherical_point_apply_euler_transformation(&point, &(polygon->point[i]), &se);

        /* Less _AND_ equal are important !! */
        /* Do not change it! */
        if (HARP_GEOMETRY_FPle(point.lat, 0.0))
        {
            /* return false */
            return 0;
        }
    }

    /* return true */
    return 1;
}

/* Does a transformation of polygon using Euler transformation
 *   se = pointer to Euler transformation
 *   in = pointer to polygon
 *   out pointer to transformed polygon
 */
static int harp_spherical_polygon_apply_euler_transformation(harp_spherical_polygon *polygon_out,
                                                             const harp_spherical_polygon *polygon_in,
                                                             const harp_euler_transformation *se)
{
    int32_t i;

    /* Copy the size and number of points */
    polygon_out->size = polygon_in->size;
    polygon_out->numberofpoints = polygon_in->numberofpoints;

    /* Apply the Euler transformation on each point of the polygon */
    for (i = 0; i < polygon_in->numberofpoints; i++)
    {
        harp_spherical_point_apply_euler_transformation(&polygon_out->point[i], &polygon_in->point[i], se);
    }

    return 0;
}

/*##################
 * Single polygons
 *##################*/

/* Create a new polygon from an array of points */
int harp_spherical_polygon_from_point_array(const harp_spherical_point_array *point_array,
                                            harp_spherical_polygon **new_polygon)
{
    harp_spherical_polygon *polygon = NULL;
    size_t size_polygon;
    int32_t i;

    if (point_array == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "point array is empty");
        return -1;
    }
    if (point_array->numberofpoints < 2)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "invalid number of points (%d) to create polygon",
                       point_array->numberofpoints);
        return -1;
    }

    /* Allocate memory for the polygon  */
    size_polygon = offsetof(harp_spherical_polygon, point)+sizeof(harp_spherical_point) * point_array->numberofpoints;
    polygon = (harp_spherical_polygon *)malloc(size_polygon);
    if (polygon == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)\n", size_polygon,
                       __FILE__, __LINE__);
        return -1;
    }
    polygon->size = (int32_t)size_polygon;
    polygon->numberofpoints = point_array->numberofpoints;

    /* Copy the spherical point into the polygon structure */
    for (i = 0; i < point_array->numberofpoints; ++i)
    {
        memcpy((void *)&polygon->point[i], (void *)&(point_array->point[i]), sizeof(harp_spherical_point));
    }

    *new_polygon = polygon;
    return 0;
}

/* Duplicate a polygon */
harp_spherical_polygon *harp_spherical_polygon_duplicate(const harp_spherical_polygon *polygon_in)
{
    /* Declare a new polygon */
    harp_spherical_polygon *polygon_copy = NULL;

    size_t size_polygon = offsetof(harp_spherical_polygon, point) +
        sizeof(harp_spherical_point) * polygon_in->numberofpoints;

    /* Allocate memory for the polygon */
    polygon_copy = harp_spherical_polygon_new(polygon_in->numberofpoints);

    /* Copy the complete polygon */
    memcpy((void *)polygon_copy, (void *)polygon_in, size_polygon);

    return polygon_copy;
}

/* Derive the centre coordinates of a polygon */
/* TODO: Replace this 'PgSphere' algorithm with the one in the DPM */
int harp_spherical_polygon_centre(harp_vector3d *vector_centre, const harp_spherical_polygon *polygon)
{
    harp_vector3d vector_polygon_point;
    harp_spherical_point pointa, pointb;        /* Start with two 3D vectors */
    harp_vector3d vectora, vectorb;
    int32_t i;

    vectora.x = 2.0;
    vectora.y = 2.0;
    vectora.z = 2.0;

    vectorb.x = -2.0;
    vectorb.y = -2.0;
    vectorb.z = -2.0;

    /* Search for minimum and maximum value of (x,y,z);
     * store minimum in vector a and maximum in vector b */
    for (i = 0; i < polygon->numberofpoints; ++i)
    {
        harp_vector3d_from_spherical_point(&vector_polygon_point, (harp_spherical_point *)&polygon->point[i]);

        /* Store minimum in vector a */
        if (vector_polygon_point.x < vectora.x)
        {
            vectora.x = vector_polygon_point.x;
        }
        if (vector_polygon_point.y < vectora.y)
        {
            vectora.y = vector_polygon_point.y;
        }
        if (vector_polygon_point.z < vectora.z)
        {
            vectora.z = vector_polygon_point.z;
        }

        /* Store maximum in vector b */
        if (vector_polygon_point.x > vectorb.x)
        {
            vectorb.x = vector_polygon_point.x;
        }
        if (vector_polygon_point.y > vectorb.y)
        {
            vectorb.y = vector_polygon_point.y;
        }
        if (vector_polygon_point.z > vectorb.z)
        {
            vectorb.z = vector_polygon_point.z;
        }
    }

    /* Points a and b */
    harp_spherical_point_from_vector3d(&pointa, &vectora);
    harp_spherical_point_from_vector3d(&pointb, &vectorb);

    vector_centre->x = (vectora.x + vectorb.x) / 2.0;
    vector_centre->y = (vectora.y + vectorb.y) / 2.0;
    vector_centre->z = (vectora.z + vectorb.z) / 2.0;

    return 0;
}

int harp_spherical_polygon_contains_point(const harp_spherical_polygon *polygon, const harp_spherical_point *point)
{
    static int32_t i;
    int result = 0;     /* false */
    static harp_spherical_line sl;
    static harp_vector3d vector_centre, vector_point;
    static double dotproduct = 0.0;

    /*---------------------------------------------------
     * First check, if point is outside polygon (behind)
     *--------------------------------------------------*/

    /* Determine the centre coordinates of the polygon */
    harp_spherical_polygon_centre(&vector_centre, polygon);

    /* Convert (lat,lon) to (x,y,z) */
    harp_vector3d_from_spherical_point(&vector_point, point);

    /* Check if the point is on the other side of the sphere, behind the polygon area.
     * If the inproduct is negative or zero, the point lies outside the bounds of the polygon. */
    dotproduct = harp_vector3d_dotproduct(&vector_point, &vector_centre);
    if (dotproduct <= 0.0)
    {
        /* return false */
        return 0;
    }

    /*--------------------------------
     * Check whether point is on edge.
     *--------------------------------*/

    /* Check whether the spherical point lies on a vertex of the polygon */
    for (i = 0; i < polygon->numberofpoints; ++i)
    {
        if (harp_spherical_point_equal(&polygon->point[i], point))
        {
            /* return true */
            return 1;
        }
    }

    /*-------------------------------------------
     * Check whether point is on a line segment.
     *------------------------------------------*/

    /* Check whether the spherical point lies on a line segment of the polygon */
    for (i = 0; i < polygon->numberofpoints; ++i)
    {
        harp_spherical_polygon_get_segment(&sl, polygon, i);

        if (harp_spherical_point_is_at_spherical_line(point, &sl))
        {
            /* return true */
            return 1;
        }
    }

    /*------------------------
     * Make some other checks
     *------------------------*/
    do
    {
        harp_euler_transformation se;
        harp_euler_transformation te;
        harp_spherical_point p;
        harp_spherical_point lp[2];

        /* Define some Booleans */
        int a1;
        int a2;
        int on_equator;

        /* Set counter to zero */
        int32_t counter = 0;

        /* Create a temporary polygon with same number of points as input polygon */
        harp_spherical_polygon *tmp = harp_spherical_polygon_new(polygon->numberofpoints);

        /* Make a transformation, so that point is (0,0) */
        harp_euler_transformation_set_to_zxz(&se);
        se.phi = (double)M_PI_2 - point->lon;
        se.theta = -1.0 * point->lat;
        se.psi = -1.0 * (double)M_PI_2;

        harp_spherical_polygon_apply_euler_transformation(tmp, polygon, &se);

        p.lat = 0.0;
        p.lon = 0.0;

        harp_spherical_point_check(&p);

        /* Initialize Euler transformation te */
        harp_euler_transformation_set_to_zxz(&te);
        te.phi = 0.0;
        te.theta = 0.0;
        te.psi = 0.0;

        /*---------------------------------------------
         * Check, whether an edge lies on the equator.
         * If yes, rotate randomized around (0,0)
         *--------------------------------------------*/

        counter = 0;

        do
        {
            on_equator = 0;
            for (i = 0; i < polygon->numberofpoints; i++)
            {
                if (HARP_GEOMETRY_FPzero(tmp->point[i].lat))
                {
                    if (HARP_GEOMETRY_FPeq(cos(tmp->point[i].lon), -1.0))
                    {
                        /* return false */
                        return 0;
                    }
                    else
                    {
                        on_equator = 1;
                        break;
                    }
                }
            }

            /* Rotate the polygon randomized around (0,0) */
            if (on_equator)
            {
                /* Define a new polygon */
                harp_spherical_polygon *ttt = harp_spherical_polygon_new(polygon->numberofpoints);

                /* Determine the sizo of the input polygon */
                size_t size_polygon = offsetof(harp_spherical_polygon, point) +
                    sizeof(harp_spherical_point) * polygon->numberofpoints;

                /* Set the seed */
                srand((unsigned int)counter);

                /* Set the rotation */
                se.phi_axis = se.theta_axis = se.psi_axis = 'X';
                se.phi = ((double)rand() / RAND_MAX) * 2.0 * M_PI;
                se.theta = 0.0;
                se.psi = 0.0;

                /* Apply the rotation */
                harp_spherical_polygon_apply_euler_transformation(ttt, tmp, &se);

                /* Copy the polygon ttt back to tmp */
                memcpy((void *)tmp, (void *)ttt, size_polygon);

                free(ttt);
            }

            assert(counter <= 10000);
            counter++;

        } while (on_equator);

        /*--------------------------------------------
         * Count line segments crossing the "equator"
         *--------------------------------------------*/

        counter = 0;

        for (i = 0; i < polygon->numberofpoints; i++)
        {
            /* Create a single line from the segment */
            harp_spherical_line_segment_from_polygon(&sl, tmp, i);

            /* Determine begin and point of the spherical line */
            harp_spherical_line_begin(&lp[0], &sl);
            harp_spherical_line_end(&lp[1], &sl);

            a1 = (HARP_GEOMETRY_FPgt(lp[0].lat, 0.0) && HARP_GEOMETRY_FPlt(lp[1].lat, 0.0));
            a2 = (HARP_GEOMETRY_FPlt(lp[0].lat, 0.0) && HARP_GEOMETRY_FPgt(lp[1].lat, 0.0));

            if (a1 || a2)
            {
                /* If crossing */
                harp_inverse_euler_transformation_from_spherical_line(&te, &sl);

                if (a2)
                {
                    /* Crossing ascending */
                    p.lon = 2.0 * M_PI - te.phi;
                }
                else
                {
                    p.lon = M_PI - te.phi;
                }

                harp_spherical_point_check(&p);

                if (p.lon < M_PI)
                {
                    /* Crossing between 0 and 180 deg */
                    counter++;
                }
            }
        }

        /* Delete the temporary polygon */
        free(tmp);

        /* Check if counter is odd */
        if (counter % 2 == 1)
        {
            result = 1;
        }

    } while (0);

    return result;
}

int8_t harp_spherical_polygon_spherical_line_relationship(const harp_spherical_polygon *polygon,
                                                          const harp_spherical_line *line)
{
    static int32_t i;
    static harp_spherical_line sl;
    static harp_spherical_point slbeg, slend;
    static int8_t p1, p2, pos, res;
    static const int8_t sl_os = (int8_t)(1 << HARP_GEOMETRY_LINE_AVOID);
    static const int8_t sl_cl = (int8_t)(1 << HARP_GEOMETRY_LINE_CONT_LINE);
    static const int8_t sl_cr = (int8_t)(1 << HARP_GEOMETRY_LINE_CROSS);
    static const int8_t sl_cn = (int8_t)(1 << HARP_GEOMETRY_LINE_CONNECT);
    static const int8_t sl_ov = (int8_t)(1 << HARP_GEOMETRY_LINE_OVER);
    static const int8_t sl_eq = (int8_t)(1 << HARP_GEOMETRY_LINE_EQUAL);

    pos = 0;
    res = 0;
    harp_spherical_line_begin(&slbeg, line);
    harp_spherical_line_end(&slend, line);
    p1 = (int8_t)harp_spherical_polygon_contains_point(polygon, &slbeg);
    p2 = (int8_t)harp_spherical_polygon_contains_point(polygon, &slend);
    for (i = 0; i < polygon->numberofpoints; i++)
    {
        harp_spherical_polygon_get_segment(&sl, polygon, i);

        pos = (int8_t)(1 << harp_spherical_line_spherical_line_relationship(&sl, line));
        if (pos == sl_eq)
        {
            pos = sl_cl;        /* is contain */
        }

        if (pos == sl_ov)
        {
            return HARP_GEOMETRY_LINE_POLY_OVER;        /* overlap */
        }
        /* Recheck line crossing */
        if (pos == sl_cr)
        {
            static int8_t bal, eal;

            bal = (int8_t)harp_spherical_point_is_at_spherical_line(&slbeg, &sl);
            eal = (int8_t)harp_spherical_point_is_at_spherical_line(&slend, &sl);
            if (!bal && !eal)
            {
                return HARP_GEOMETRY_LINE_POLY_OVER;    /* overlap */
            }
            if ((bal && p2) || (eal && p1))
            {
                pos = sl_cl;    /* is contain */
            }
            else
            {
                return HARP_GEOMETRY_LINE_POLY_OVER;    /* overlap */
            }
        }

        res |= pos;
    }
    if ((res & sl_cl) && ((res - sl_cl - sl_os - sl_cn - 1) < 0))
    {
        return HARP_GEOMETRY_POLY_CONT_LINE;
    }
    else if (p1 && p2 && ((res - sl_os - sl_cn - 1) < 0))
    {
        return HARP_GEOMETRY_POLY_CONT_LINE;
    }
    else if (!p1 && !p2 && ((res - sl_os - 1) < 0))
    {
        return HARP_GEOMETRY_LINE_POLY_AVOID;
    }

    return HARP_GEOMETRY_LINE_POLY_OVER;
}

/* Determine relationship of two polygon areas */
int8_t harp_spherical_polygon_spherical_polygon_relationship(const harp_spherical_polygon *polygon_a,
                                                             const harp_spherical_polygon *polygon_b, int recheck)
{
    int32_t i;
    harp_spherical_line sl;
    int8_t pos = 0, res = 0;
    static const int8_t sp_os = (int8_t)(1 << HARP_GEOMETRY_LINE_POLY_AVOID);
    static const int8_t sp_ct = (int8_t)(1 << HARP_GEOMETRY_POLY_CONT_LINE);
    static const int8_t sp_ov = (int8_t)(1 << HARP_GEOMETRY_LINE_POLY_OVER);

    for (i = 0; i < polygon_b->numberofpoints; i++)
    {
        harp_spherical_polygon_get_segment(&sl, polygon_b, i);

        pos = (int8_t)(1 << harp_spherical_polygon_spherical_line_relationship(polygon_a, &sl));
        if (pos == sp_ov)
        {
            /* overlap */
            return HARP_GEOMETRY_POLY_OVER;
        }

        res |= pos;
    }

    if (res == sp_os)
    {
        if (!recheck)
        {
            pos = harp_spherical_polygon_spherical_polygon_relationship(polygon_b, polygon_a, 1);
        }

        if (pos == HARP_GEOMETRY_POLY_CONT)
        {
            return HARP_GEOMETRY_POLY_OVER;
        }
        else
        {
            return HARP_GEOMETRY_POLY_AVOID;
        }
    }

    if (res == sp_ct)
    {
        return HARP_GEOMETRY_POLY_CONT;
    }

    return HARP_GEOMETRY_POLY_OVER;
}

/* Determine whether two polygons overlap */
int harp_spherical_polygon_overlapping(const harp_spherical_polygon *polygon_a, const harp_spherical_polygon *polygon_b,
                                       int *polygons_are_overlapping)
{
    int8_t relationship;

    if (harp_spherical_polygon_check(polygon_a) != 1)

    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT,
                       "a line segment overlaps or polygon too large for polygon overlap percentage");
        return -1;
    }

    if (harp_spherical_polygon_check(polygon_b) != 1)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT,
                       "a line segment overlaps or polygon too large for polygon overlap percentage");
        return -1;
    }

    /* First, determine relationship of two areas */
    relationship = harp_spherical_polygon_spherical_polygon_relationship(polygon_a, polygon_b, 0);
    if (relationship == HARP_GEOMETRY_POLY_CONT || relationship == HARP_GEOMETRY_POLY_OVER)
    {
        *polygons_are_overlapping = 1;
    }
    else
    {
        /* No overlap */
        *polygons_are_overlapping = 0;
    }

    return 0;
}

/* Determine whether two polygons overlap, and if so
 * calculate the overlapping percentage of the two polygons */
int harp_spherical_polygon_overlapping_percentage(const harp_spherical_polygon *polygon_a,
                                                  const harp_spherical_polygon *polygon_b,
                                                  int *polygons_are_overlapping, double *overlapping_percentage)
{
    int8_t relationship;

    if (harp_spherical_polygon_check(polygon_a) != 1)

    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT,
                       "a line segment overlaps or polygon too large for polygon overlap percentage");
        return -1;
    }

    if (harp_spherical_polygon_check(polygon_b) != 1)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT,
                       "a line segment overlaps or polygon too large for polygon overlap percentage");
        return -1;
    }

    /* First, determine relationship of two areas */
    relationship = harp_spherical_polygon_spherical_polygon_relationship(polygon_a, polygon_b, 0);
    if (relationship == HARP_GEOMETRY_POLY_CONT)
    {
        *overlapping_percentage = 100.0;
        *polygons_are_overlapping = 1;
    }
    else if (relationship == HARP_GEOMETRY_POLY_OVER)
    {
        harp_spherical_polygon *polygon_intersect = NULL;
        double min_area_a_area_b;
        double area_a;
        double area_b;
        double area_ab;
        uint8_t *point_a_in_polygon_b;
        uint8_t *point_b_in_polygon_a;
        int32_t num_intersection_points = 0;
        int32_t offset_a = 0;
        int32_t offset_c = 0;   /* index in intersection polygon */
        int32_t i;

        /* check if polygon a is contained in polygon b */
        relationship = harp_spherical_polygon_spherical_polygon_relationship(polygon_b, polygon_a, 1);
        if (relationship == HARP_GEOMETRY_POLY_CONT)
        {
            *overlapping_percentage = 100.0;
            *polygons_are_overlapping = 1;
            return 0;
        }

        /* There must be an intersection, so try to find it */
        point_a_in_polygon_b = malloc((size_t)polygon_a->numberofpoints * sizeof(uint8_t));
        if (point_a_in_polygon_b == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)\n",
                           polygon_a->numberofpoints, __FILE__, __LINE__);
            return -1;
        }
        point_b_in_polygon_a = malloc((size_t)polygon_b->numberofpoints * sizeof(uint8_t));
        if (point_b_in_polygon_a == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)\n",
                           polygon_b->numberofpoints, __FILE__, __LINE__);
            free(point_a_in_polygon_b);
            return -1;
        }
        for (i = 0; i < polygon_a->numberofpoints; i++)
        {
            point_a_in_polygon_b[i] = (uint8_t)harp_spherical_polygon_contains_point(polygon_b, &polygon_a->point[i]);
            if (point_a_in_polygon_b[i])
            {
                num_intersection_points++;
            }
        }
        for (i = 0; i < polygon_b->numberofpoints; i++)
        {
            point_b_in_polygon_a[i] = (uint8_t)harp_spherical_polygon_contains_point(polygon_a, &polygon_b->point[i]);
            if (point_b_in_polygon_a[i])
            {
                num_intersection_points++;
            }
        }
        for (i = 0; i < polygon_a->numberofpoints; i++)
        {
            if (point_a_in_polygon_b[i] != point_a_in_polygon_b[i == 0 ? polygon_a->numberofpoints - 1 : i - 1])
            {
                num_intersection_points++;
            }
        }
        assert(num_intersection_points > 0);

        polygon_intersect = harp_spherical_polygon_new(num_intersection_points);
        if (polygon_intersect == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not create polygon) (%s:%u)" __FILE__,
                           __LINE__);
            free(point_a_in_polygon_b);
            free(point_b_in_polygon_a);
            return -1;
        }

        while (offset_a < polygon_a->numberofpoints)
        {
            harp_spherical_line line_a;
            int32_t next_offset_a = offset_a == polygon_a->numberofpoints - 1 ? 0 : offset_a + 1;

            if (point_a_in_polygon_b[offset_a])
            {
                assert(offset_c != num_intersection_points);
                polygon_intersect->point[offset_c] = polygon_a->point[offset_a];
                offset_c++;
            }
            /* are we switching from polygons? */
            if ((point_a_in_polygon_b[offset_a] && !point_a_in_polygon_b[next_offset_a]) ||
                (!point_a_in_polygon_b[offset_a] && point_a_in_polygon_b[next_offset_a]))
            {
                harp_spherical_line line_b;
                int32_t offset_b = 0;

                harp_spherical_line_segment_from_polygon(&line_a, polygon_a, offset_a);

                /* find line segment in polygon b that crosses line_a */
                while (offset_b < polygon_b->numberofpoints)
                {
                    int32_t next_offset_b = offset_b == polygon_b->numberofpoints - 1 ? 0 : offset_b + 1;

                    if ((point_b_in_polygon_a[offset_b] && !point_b_in_polygon_a[next_offset_b]) ||
                        (!point_b_in_polygon_a[offset_b] && point_b_in_polygon_a[next_offset_b]))
                    {
                        harp_spherical_line_segment_from_polygon(&line_b, polygon_b, offset_b);
                        if (harp_spherical_line_spherical_line_relationship(&line_a, &line_b) !=
                            HARP_GEOMETRY_LINE_AVOID)
                        {
                            if (harp_spherical_line_spherical_line_relationship(&line_a, &line_b) ==
                                HARP_GEOMETRY_LINE_CROSS)
                            {
                                harp_spherical_point intersection;

                                if (point_b_in_polygon_a[offset_b])
                                {
                                    /* p = line b && q = line a */
                                    harp_spherical_line_spherical_line_intersection_point(&line_b, &line_a,
                                                                                          &intersection);
                                }
                                else
                                {
                                    /* p = line a && q = line b */
                                    harp_spherical_line_spherical_line_intersection_point(&line_a, &line_b,
                                                                                          &intersection);
                                }
                                assert(offset_c != num_intersection_points);
                                polygon_intersect->point[offset_c] = intersection;
                                offset_c++;
                            }
                            else
                            {
                                /* line segements are on the same great circle, so no intermediate point needed */
                                num_intersection_points--;
                                polygon_intersect->numberofpoints--;
                            }
                            if (!point_a_in_polygon_b[next_offset_a])
                            {
                                /* add points from polygon b */
                                if (point_b_in_polygon_a[next_offset_b])
                                {
                                    /* add in ascending order */
                                    while (point_b_in_polygon_a[next_offset_b] && next_offset_b != offset_b)
                                    {
                                        assert(offset_c != num_intersection_points);
                                        polygon_intersect->point[offset_c] = polygon_b->point[next_offset_b];
                                        offset_c++;
                                        next_offset_b++;
                                        if (next_offset_b == polygon_b->numberofpoints)
                                        {
                                            next_offset_b = 0;
                                        }
                                    }
                                }
                                else
                                {
                                    /* add in descending order */
                                    while (point_b_in_polygon_a[offset_b] && offset_b != next_offset_b)
                                    {
                                        assert(offset_c != num_intersection_points);
                                        polygon_intersect->point[offset_c] = polygon_b->point[offset_b];
                                        offset_c++;
                                        offset_b--;
                                        if (offset_b == -1)
                                        {
                                            offset_b = polygon_b->numberofpoints - 1;
                                        }
                                    }
                                }
                            }
                            break;
                        }
                    }
                    offset_b++;
                }
            }
            offset_a++;
        }

        free(point_a_in_polygon_b);
        free(point_b_in_polygon_a);

        harp_spherical_polygon_check(polygon_intersect);

        /* Calculate areaAB = surface area of intersection polygon */
        harp_spherical_polygon_get_surface_area(polygon_intersect, &area_ab);

        /* Calculate areaA = surface area of polygon A */
        harp_spherical_polygon_get_surface_area(polygon_a, &area_a);

        /* Calculate areaB = surface area of polygon B */
        harp_spherical_polygon_get_surface_area(polygon_b, &area_b);

        /* Overlapping percentage = 100 * areaAB / min(areaA, areaB) */
        min_area_a_area_b = (area_a < area_b ? area_a : area_b);
        assert(min_area_a_area_b >= 0.0);
        if (HARP_GEOMETRY_FPzero(min_area_a_area_b))
        {
            /* just set to 100% if area_a/area_b is too small */
            *overlapping_percentage = 100.0;
            *polygons_are_overlapping = 1;
        }
        else
        {
            *overlapping_percentage = 100.0 * area_ab / min_area_a_area_b;
            *polygons_are_overlapping = 1;
        }

        harp_spherical_polygon_delete(polygon_intersect);
    }
    else
    {
        /* No overlap */
        *overlapping_percentage = 0.0;
        *polygons_are_overlapping = 0;
    }

    return 0;
}

/* Calculate the signed surface area (in [m2]) of polygon */
int harp_spherical_polygon_get_signed_surface_area(const harp_spherical_polygon *polygon, double *signed_area_out)
{
    int32_t numberofpoints;
    const double Earth_radius = CONST_EARTH_RADIUS_WGS84_SPHERE;        /* Radius of Earth sphere [m] */
    double signed_area = 0.0;
    int32_t i;

    if (polygon == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "input polygon for signed surface area calculation is empty");
        return -1;
    }

    numberofpoints = polygon->numberofpoints;
    if (numberofpoints < 3)
    {
        signed_area = 0.0;
        return 0;
    }

    /* Calculate the signed area */
    signed_area = (polygon->point[numberofpoints - 1].lon - polygon->point[1].lon) * sin(polygon->point[0].lat);
    for (i = 1; i < numberofpoints - 1; i++)
    {
        signed_area += (polygon->point[i - 1].lon - polygon->point[i + 1].lon) * sin(polygon->point[i].lat);
    }
    signed_area += (polygon->point[numberofpoints - 2].lon - polygon->point[0].lon) *
        sin(polygon->point[numberofpoints - 1].lat);
    /* Convert area [rad2] to [m2] */
    *signed_area_out = Earth_radius * Earth_radius * signed_area / 2;
    return 0;
}

/* Calculate the surface area of a polygon (positive value in [m2]) */
int harp_spherical_polygon_get_surface_area(const harp_spherical_polygon *polygon_in, double *area_out)
{
    double signed_area;

    if (polygon_in == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "input polygon for surface area calculation is empty");
        return -1;
    }

    if (harp_spherical_polygon_get_signed_surface_area(polygon_in, &signed_area) != 0)

    {
        return -1;
    }
    *area_out = fabs(signed_area);
    return 0;
}

/* Given number of vertex points, return empty
 * spherical polygon data structure with points (lat,lon) in  [rad] */
harp_spherical_polygon *harp_spherical_polygon_new(int32_t numberofpoints)
{
    harp_spherical_polygon *polygon = NULL;
    size_t size = offsetof(harp_spherical_polygon, point) + sizeof(harp_spherical_point) * numberofpoints;

    polygon = (harp_spherical_polygon *)malloc(size);
    polygon->size = size;
    polygon->numberofpoints = numberofpoints;
    return polygon;
}

void harp_spherical_polygon_delete(harp_spherical_polygon *polygon)
{
    free(polygon);
}

/* Convert unit of each spherical polygon vertex
 * (lat,lon) from [rad] to [deg] */
void spherical_polygon_deg_from_rad(harp_spherical_polygon *polygon)
{
    int32_t i;

    for (i = 0; i < polygon->numberofpoints; ++i)
    {
        polygon->point[i].lat = polygon->point[i].lat * (double)CONST_RAD2DEG;
        polygon->point[i].lon = polygon->point[i].lon * (double)CONST_RAD2DEG;
    }
}

/* Convert unit of each spherical polygon vertex
 * (lat,lon) from [deg] to [rad] */
void spherical_polygon_rad_from_deg(harp_spherical_polygon *polygon)
{
    int32_t i;

    for (i = 0; i < polygon->numberofpoints; ++i)
    {
        polygon->point[i].lat = polygon->point[i].lat * (double)CONST_DEG2RAD;
        polygon->point[i].lon = polygon->point[i].lon * (double)CONST_DEG2RAD;
    }
}

static int harp_spherical_polygon_begin_end_point_equal(long measurement_id, long num_vertices,
                                                        const double *latitude_bounds, const double *longitude_bounds)
{
    long ii_begin, ii_end;
    double deg2rad = (double)(CONST_DEG2RAD);
    harp_spherical_point p_begin, p_end;

    /* Determine first and last index */
    ii_begin = measurement_id * num_vertices + 0;
    ii_end = measurement_id * num_vertices + num_vertices - 1;

    /* Get begin and end point */
    p_begin.lat = latitude_bounds[ii_begin] * deg2rad;
    p_begin.lon = longitude_bounds[ii_begin] * deg2rad;
    p_end.lat = latitude_bounds[ii_end] * deg2rad;
    p_end.lon = longitude_bounds[ii_end] * deg2rad;
    return harp_spherical_point_equal(&p_begin, &p_end);
}

/* Obtain spherical polygon from two double arrays with latitude_bounds [degree_north] and longitude_bounds [degree_east]
 * Make sure that the points are organized as follows:
 * - counter-clockwise (right-hand rule)
 * - no duplicate points (i.e. begin and end point must not be the same) */
int harp_spherical_polygon_from_latitude_longitude_bounds(long measurement_id, long num_vertices,
                                                          const double *latitude_bounds,
                                                          const double *longitude_bounds,
                                                          harp_spherical_polygon **new_polygon)
{
    harp_spherical_polygon *polygon = NULL;
    double deg2rad = (double)(CONST_DEG2RAD);
    harp_spherical_point_array *point_array = NULL;
    harp_spherical_point *point = NULL;
    int32_t numberofpoints = (int32_t)num_vertices;     /* Start with num_vertices */
    long ii;
    int32_t i;

    /* Check if the first and last spherical point of the polygon are equal */
    if (harp_spherical_polygon_begin_end_point_equal(measurement_id, num_vertices, latitude_bounds, longitude_bounds))
    {
        /* If this is the case, do not include the last point */
        numberofpoints--;
    }
    if (numberofpoints <= 0)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "numberofpoints must be larger than zero");
        return -1;
    }

    if (harp_spherical_point_array_new(&point_array) != 0)
    {
        return -1;
    }

    point = malloc(sizeof(harp_spherical_point));
    if (point == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_spherical_point), __FILE__, __LINE__);
        harp_spherical_point_array_delete(point_array);
        return -1;
    }

    for (i = 0; i < numberofpoints; i++)
    {
        /* Element index in the data array */
        /* Remember: At this point 'numberofpoints' can be smaller than original input 'num_vertices'. */
        ii = measurement_id * num_vertices + i;

        /* Make sure we have the coordinates in the correct units */
        point->lat = latitude_bounds[ii] * deg2rad;
        point->lon = longitude_bounds[ii] * deg2rad;
        harp_spherical_point_check(point);

        /* Add the point to the point array */
        if (harp_spherical_point_array_add_point(point_array, point) != 0)
        {
            harp_spherical_point_array_delete(point_array);
            free(point);
            return -1;
        }
    }

    /* Create the polygon */
    if (harp_spherical_polygon_from_point_array(point_array, &polygon) != 0)
    {
        harp_spherical_point_array_delete(point_array);
        free(point);
        return -1;
    }

    /* Check the polygon */
    if (harp_spherical_polygon_check(polygon) != 1)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "invalid polygon from input latitude bounds and longitude bounds");
        harp_spherical_point_array_delete(point_array);
        harp_spherical_polygon_delete(polygon);
        free(point);
        return -1;
    }

    harp_spherical_point_array_delete(point_array);
    free(point);

    *new_polygon = polygon;
    return 0;
}

/* Calculate the distance to the nearest line segment of the polygon */
double harp_spherical_polygon_spherical_point_distance(const harp_spherical_polygon *polygon,
                                                       const harp_spherical_point *point)
{
    harp_spherical_line linei;
    int32_t i;
    double d;
    double d_nearest = 10.0;    /* 10 times sphere radius */

    for (i = 0; i < polygon->numberofpoints; i++)
    {
        /* Grab line segment from polygon */
        harp_spherical_line_segment_from_polygon(&linei, polygon, i);

        /* Calculate distance point-line */
        d = harp_spherical_line_spherical_point_distance(&linei, point);
        if (d < d_nearest)
        {
            d_nearest = d;
        }
    }

    if (d_nearest >= 10.0)
    {
        d_nearest = harp_nan();
    }

    return d_nearest;
}

/* Calculate the distance to the nearest line segment of the polygon */
double harp_spherical_polygon_spherical_point_distance_in_meters(const harp_spherical_polygon *polygon,
                                                                 const harp_spherical_point *point)
{
    const double Earth_radius = (double)(CONST_EARTH_RADIUS_WGS84_SPHERE);
    harp_spherical_line linei;
    int32_t i;
    double d;
    double d_nearest = 10.0 * Earth_radius;

    for (i = 0; i < polygon->numberofpoints; i++)
    {
        /* Grab line segment from polygon */
        harp_spherical_line_segment_from_polygon(&linei, polygon, i);

        /* Calculate distance point-line */
        d = harp_spherical_line_spherical_point_distance_in_meters(&linei, point);
        if (d < d_nearest)
        {
            d_nearest = d;
        }
    }

    if (d_nearest >= 10.0 * Earth_radius)
    {
        d_nearest = harp_nan();
    }

    return d_nearest;
}

/** Determine whether a point is in an area on the surface of the Earth
 * \ingroup harp_geometry
 * This function assumes a spherical earth
 * \param latitude_point Latitude of the point
 * \param longitude_point Longitude of the point
 * \param num_vertices The number of vertices of the bounding polygon of the area
 * \param latitude_bounds Latitude values of the bounds of the area polygon
 * \param longitude_bounds Longitude values of the bounds of the area polygon
 * \param in_area Pointer to the C variable where the result will be stored (1 if point is in the area, 0 otherwise).
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_geometry_has_point_in_area(double latitude_point, double longitude_point, int num_vertices,
                                                double *latitude_bounds, double *longitude_bounds, int *in_area)
{
    harp_spherical_point point;
    harp_spherical_polygon *polygon = NULL;

    point.lat = latitude_point;
    point.lon = longitude_point;
    harp_spherical_point_rad_from_deg(&point);
    harp_spherical_point_check(&point);

    if (harp_spherical_polygon_from_latitude_longitude_bounds(0, num_vertices, latitude_bounds, longitude_bounds,
                                                              &polygon) != 0)
    {
        return -1;
    }

    *in_area = harp_spherical_polygon_contains_point(polygon, &point);

    harp_spherical_polygon_delete(polygon);

    return 0;
}

/** Determine whether a point is in an area on the surface of the Earth
 * \ingroup harp_geometry
 * This function assumes a spherical earth
 * \param num_vertices_a The number of vertices of the bounding polygon of the first area
 * \param latitude_bounds_a Latitude values of the bounds of the area of the first polygon
 * \param longitude_bounds_a Longitude values of the bounds of the area of the first polygon
 * \param num_vertices_b The number of vertices of the bounding polygon of the second area
 * \param latitude_bounds_b Latitude values of the bounds of the area of the second polygon
 * \param longitude_bounds_b Longitude values of the bounds of the area of the second polygon
 * \param has_overlap Pointer to the C variable where the result will be stored (1 if there is overlap, 0 otherwise).
 * \param percentage Pointer to the C variable where the overlap percentage will be stored (use NULL if not needed).
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_geometry_has_area_overlap(int num_vertices_a, double *latitude_bounds_a,
                                               double *longitude_bounds_a, int num_vertices_b,
                                               double *latitude_bounds_b, double *longitude_bounds_b, int *has_overlap,
                                               double *percentage)
{
    harp_spherical_polygon *polygon_a = NULL;
    harp_spherical_polygon *polygon_b = NULL;

    if (harp_spherical_polygon_from_latitude_longitude_bounds(0, num_vertices_a, latitude_bounds_a, longitude_bounds_a,
                                                              &polygon_a) != 0)
    {
        return -1;
    }
    if (harp_spherical_polygon_from_latitude_longitude_bounds(0, num_vertices_b, latitude_bounds_b, longitude_bounds_b,
                                                              &polygon_b) != 0)
    {
        return -1;
    }

    if (percentage != NULL)
    {
        /* Determine overlapping percentage */
        if (harp_spherical_polygon_overlapping_percentage(polygon_a, polygon_b, has_overlap, percentage) != 0)
        {
            harp_spherical_polygon_delete(polygon_a);
            harp_spherical_polygon_delete(polygon_b);
            return -1;
        }
    }
    else
    {
        if (harp_spherical_polygon_overlapping(polygon_a, polygon_b, has_overlap) != 0)
        {
            harp_spherical_polygon_delete(polygon_a);
            harp_spherical_polygon_delete(polygon_b);
            return -1;
        }
    }

    harp_spherical_polygon_delete(polygon_a);
    harp_spherical_polygon_delete(polygon_b);

    return 0;
}
