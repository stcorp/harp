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
#include <stdlib.h>
#include <string.h>

/* Check if two Euler transformation are equal */
int harp_euler_transformation_equal(const harp_euler_transformation *euler1, const harp_euler_transformation *euler2)
{
    harp_spherical_point pointin[2], point[4];

    pointin[0].lat = 0.0;
    pointin[0].lon = 0.0;

    pointin[1].lat = 0.0;
    pointin[1].lon = M_PI_2;

    harp_spherical_point_apply_euler_transformation(&point[0], &pointin[0], euler1);
    harp_spherical_point_apply_euler_transformation(&point[1], &pointin[1], euler1);

    harp_spherical_point_apply_euler_transformation(&point[2], &pointin[0], euler2);
    harp_spherical_point_apply_euler_transformation(&point[3], &pointin[1], euler2);

    return (harp_spherical_point_equal(&point[0], &point[2]) && harp_spherical_point_equal(&point[1], &point[3]));
}

/* This transforms an Euler transformation into an ZXZ-axis Euler transformation */
void harp_euler_transformation_transform_to_zxz_euler_transformation(harp_euler_transformation *transformationout,
                                                                     const harp_euler_transformation *transformationin,
                                                                     const harp_euler_transformation *transformation)
{
    harp_spherical_point point[4];

    point[0].lat = 0.0;
    point[0].lon = 0.0;

    point[1].lat = 0.0;
    point[1].lon = M_PI_2;

    harp_spherical_point_apply_euler_transformation(&point[2], &point[0], transformationin);
    harp_spherical_point_apply_euler_transformation(&point[3], &point[1], transformationin);

    harp_spherical_point_apply_euler_transformation(&point[0], &point[2], transformation);
    harp_spherical_point_apply_euler_transformation(&point[1], &point[3], transformation);

    /* Determine output transformation */
    harp_euler_transformation_from_spherical_vector(transformationout, &point[0], &point[1]);
}

/* Invert an Euler transformation
 *
 * Parameter:
 *    pointer to input transformation
 *    Replace input transformation with output transformation
 */
void harp_euler_transformation_invert(harp_euler_transformation *transformation)
{
    harp_spherical_point point[3];

    const unsigned char c = transformation->phi_axis;

    point[2].lat = 0.0;
    point[1].lat = 0.0;
    point[0].lat = 0.0;

    point[2].lon = -transformation->phi;
    point[1].lon = -transformation->theta;
    point[0].lon = -transformation->psi;

    /* Check spherical points */
    harp_spherical_point_check(&point[0]);
    harp_spherical_point_check(&point[1]);
    harp_spherical_point_check(&point[2]);

    transformation->phi = point[0].lon;
    transformation->theta = point[1].lon;
    transformation->psi = point[2].lon;

    /* Swap phi and psi-axis */
    transformation->phi_axis = transformation->psi_axis;
    transformation->psi_axis = c;
}

/* Sets axes of rotation to ZXZ */
void harp_euler_transformation_set_to_zxz(harp_euler_transformation *transformation)
{
    transformation->phi_axis = 'Z';
    transformation->theta_axis = 'X';
    transformation->psi_axis = 'Z';
}

/* Transform a spherical vector to an
 * inverse Euler transformation
 *
 * Parameters:
 *   inverse_transformation = pointer to
 *       inverse Euler transformation
 *   pointbegin = pointer to begin of spherical vector
 *   pointend = pointer to end of spherical vector
 */
static void inverse_euler_transformation_from_spherical_vector(harp_euler_transformation *inverse_transformation,
                                                               const harp_spherical_point *sphericalvectorbegin,
                                                               const harp_spherical_point *sphericalvectorend)
{
    if (harp_spherical_point_equal(sphericalvectorbegin, sphericalvectorend))
    {
        inverse_transformation->phi = 0.0;
        inverse_transformation->theta = 0.0;
        inverse_transformation->psi = 0.0;
    }
    else
    {
        harp_vector3d vectorbegin;
        harp_vector3d vectorend;
        harp_vector3d vectortemp;
        harp_spherical_point pointtemp[2];

        /* Convert (lat,lon) coordinates to Cartesian coordinates and
           calculate cross product of the two obtained vectors */
        harp_vector3d_from_spherical_point(&vectorbegin, sphericalvectorbegin);
        harp_vector3d_from_spherical_point(&vectorend, sphericalvectorend);
        harp_vector3d_crossproduct(&vectortemp, &vectorbegin, &vectorend);

        /* Convert (x,y,z) of obtained point (lat,lon) and store
           it in pointtemp[0] */
        harp_spherical_point_from_vector3d(&pointtemp[0], &vectortemp);

        inverse_transformation->phi = -pointtemp[0].lon - M_PI_2;
        inverse_transformation->theta = pointtemp[0].lat - M_PI_2;
        inverse_transformation->psi = 0.0;

        /* Use ZXZ as axes of transformation */
        harp_euler_transformation_set_to_zxz(inverse_transformation);

        /* Apply Euler transformation on the spherical point */
        harp_spherical_point_apply_euler_transformation(&pointtemp[1], sphericalvectorbegin, inverse_transformation);

        inverse_transformation->psi = -pointtemp[1].lon;
    }
}

/* Transform a spherical vector to a Euler transformation
 *
 * Parameters:
 *   transformation = pointer to Euler transformation
 *   pointbegin = pointer to begin of spherical vector
 *   pointend = pointer to end of spherical vector
 */
void harp_euler_transformation_from_spherical_vector(harp_euler_transformation *transformation,
                                                     const harp_spherical_point *sphericalvectorbegin,
                                                     const harp_spherical_point *sphericalvectorend)
{
    /* Determine inverse Euler transformation and save the inverse transformation in "transformation" */
    inverse_euler_transformation_from_spherical_vector(transformation, sphericalvectorbegin, sphericalvectorend);

    /* Invert the inverse Euler transformation */
    harp_euler_transformation_invert(transformation);
}

/* Apply Euler transformation of 3d vector.
 * This involves a transformation over three angles:
 *
 *   psi
 *   theta
 *   phi
 *
 * Here, the angles are in [rad]
 */
static int vector3d_apply_euler_transformation(harp_vector3d *vectorout, const harp_vector3d *vectorin,
                                               const harp_euler_transformation *transformation)
{
    double u[3], v[3], sin_angle, cos_angle;
    const double *angle;
    unsigned char axis;
    int i;

    /* Input vector */
    axis = 'X'; /* Assume X */
    angle = NULL;
    u[0] = vectorin->x;
    u[1] = vectorin->y;
    u[2] = vectorin->z;

    for (i = 0; i < 3; i++)
    {
        switch (i)
        {
            case 0:
                angle = &transformation->phi;
                axis = transformation->phi_axis;
                break;
            case 1:
                angle = &transformation->theta;
                axis = transformation->theta_axis;
                break;
            case 2:
                angle = &transformation->psi;
                axis = transformation->psi_axis;
                break;
        }

        if (HARP_GEOMETRY_FPzero(*angle))
        {
            continue;
        }

        sin_angle = sin(*angle);
        cos_angle = cos(*angle);

        switch (axis)
        {
            case 'X':
                /* transformation around X-axis */
                v[0] = u[0];
                v[1] = cos_angle * u[1] - sin_angle * u[2];
                v[2] = sin_angle * u[1] + cos_angle * u[2];

                break;

            case 'Y':
                /* transformation around Y-axis */
                v[0] = cos_angle * u[0] + sin_angle * u[2];
                v[1] = u[1];
                v[2] = -sin_angle * u[0] + cos_angle * u[2];
                break;

            case 'Z':
                /* transformation around Z-axis */
                v[0] = cos_angle * u[0] - sin_angle * u[1];
                v[1] = sin_angle * u[0] + cos_angle * u[1];
                v[2] = u[2];
                break;

            default:
                harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "invalid Euler axis");
                return -1;
        }

        memcpy((void *)&u[0], (void *)&v[0], sizeof(u));
    }

    vectorout->x = u[0];
    vectorout->y = u[1];
    vectorout->z = u[2];

    return 0;
}

/* Apply Euler transformation of spherical point */
void harp_spherical_point_apply_euler_transformation(harp_spherical_point *pointout,
                                                     const harp_spherical_point *pointin,
                                                     const harp_euler_transformation *transformation)
{
    harp_vector3d vectorin;
    harp_vector3d vectorout;

    /* First, convert (lat,lon) to (x,y,z) coordinates */
    harp_vector3d_from_spherical_point(&vectorin, pointin);

    /* Rotate the vector around the 3 Euler axes to get the output vector */
    vector3d_apply_euler_transformation(&vectorout, &vectorin, transformation);

    /* Finally, convert the rotated vector (x,y,z) to (lat,lon) coordinates */
    harp_spherical_point_from_vector3d(pointout, &vectorout);

    harp_spherical_point_check(pointout);
}
