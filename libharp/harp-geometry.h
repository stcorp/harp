/*
 * Copyright (C) 2015-2021 S[&]T, The Netherlands.
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

#ifndef HARP_GEOMETRY_H
#define HARP_GEOMETRY_H

#include "harp-internal.h"
#include "harp-constants.h"

#define HARP_GEOMETRY_LINE_SEPARATE 1   /* lines are separate */
#define HARP_GEOMETRY_LINE_EQUAL 2      /* lines are equal */
#define HARP_GEOMETRY_LINE_CONTAINS 3   /* line contains line */
#define HARP_GEOMETRY_LINE_CONTAINED 4  /* line is contained by line */
#define HARP_GEOMETRY_LINE_CROSS 5      /* lines cross each other */
#define HARP_GEOMETRY_LINE_CONNECTED 6  /* lines are connected at the end points */
#define HARP_GEOMETRY_LINE_OVERLAP 7    /* lines overlap each other */

#define HARP_GEOMETRY_POLY_SEPARATE 0   /* polygons are separate */
#define HARP_GEOMETRY_POLY_CONTAINS 1   /* polygon contains polygon */
#define HARP_GEOMETRY_POLY_CONTAINED 2  /* polygon is contained by polygon */
#define HARP_GEOMETRY_POLY_OVERLAP 3    /* polygons overlap each other */

#define HARP_GEOMETRY_LINE_POLY_SEPARATE 0      /* line and polygon are separate */
#define HARP_GEOMETRY_LINE_POLY_CONTAINED 1     /* line is contained by polygon */
#define HARP_GEOMETRY_LINE_POLY_OVERLAP 2       /* line overlap polygon */

#define HARP_GEOMETRY_EPSILON    (1.0E-10)
#define HARP_GEOMETRY_FPzero(A)  (fabs(A) <= HARP_GEOMETRY_EPSILON)
#define HARP_GEOMETRY_FPeq(A, B) (fabs((A) - (B)) <= HARP_GEOMETRY_EPSILON)
#define HARP_GEOMETRY_FPne(A, B) (fabs((A) - (B)) > HARP_GEOMETRY_EPSILON)
#define HARP_GEOMETRY_FPlt(A, B) ((B) - (A) > HARP_GEOMETRY_EPSILON)
#define HARP_GEOMETRY_FPle(A, B) ((A) - (B) <= HARP_GEOMETRY_EPSILON)
#define HARP_GEOMETRY_FPgt(A, B) ((A) - (B) > HARP_GEOMETRY_EPSILON)
#define HARP_GEOMETRY_FPge(A, B) ((B) - (A) <= HARP_GEOMETRY_EPSILON)

#define HARP_GEOMETRY_NUM_PLANE_COEFFICIENTS 4
#define HARP_GEOMETRY_NUM_MATRIX_3X3_ELEMENTS 9

/*-----------------------------------------*
 * Define geometric data structures:
 *
 *   harp_spherical_point
 *   harp_spherical_line
 *   harp_spherical_polygon
 *   harp_spherical_polygon_array
 *   harp_euler_transformation
 *   harp_euler_transformationAxis
 * In addition,
 *   harp_vector3d
 *-----------------------------------------*/

/* Define a 3D vector with Cartesian coordinates (x,y,z) */
typedef struct harp_vector3d_struct
{
    double x;
    double y;
    double z;
} harp_vector3d;

/* Define point on sphere */
typedef struct harp_spherical_point_struct
{
    double lon; /* in [rad] */
    double lat; /* in [rad] */
} harp_spherical_point;

/* Define line on sphere. It is defined by an
 * Euler transformation and a length.
 * The "untransformed" line starts on the equator at (0,0) and ends at (length, 0).
 * The Euler transformation is defined by 3 rotation angles:
 *    phi   = the first rotation angle, around Z-axis
 *    theta = the second rotation angle, around X-axis
 *    psi   = the last rotation angle, around Z-axis */
typedef struct harp_spherical_line_struct
{
    double phi; /* the first  rotation angle around z axis */
    double theta;       /* the second rotation angle around x axis */
    double psi; /* the last   rotation angle around z axis */
    double length;      /* the length of the line */
} harp_spherical_line;

/* Define polygon on a sphere */
/* A variable length array of points is used, which means that the length is determine at run time */
typedef struct harp_spherical_polygon_struct
{
    int32_t size;       /* total size in bytes */
    int32_t numberofpoints;     /* count of points */
    harp_spherical_point point[1];      /* variable length array of "spherical_point"s */
} harp_spherical_polygon;

/* Define an array of points on a sphere */
typedef struct harp_spherical_point_array_struct
{
    int32_t numberofpoints;     /* count of polygons */
    harp_spherical_point *point;        /* variable length array of "spherical_point"s */
} harp_spherical_point_array;

/* Define an array of polygons on a sphere */
typedef struct harp_spherical_polygon_array_struct
{
    int32_t numberofpolygons;   /* count of polygons */
    harp_spherical_polygon **polygon;   /* variable length array of "spherical_polygon"s */
} harp_spherical_polygon_array;

/* Define Euler transformation
 * An Euler transformation
 * is defined by 3 rotation angles:
 *    phi   = the first rotation angle, around phi-axis
 *    theta = the second rotation angle, around theta-axis
 *    psi   = the last rotation angle, around psi-axis
 *  The default choice is ZXZ, i.e.
 *    phi_axis   = 'Z'
 *    theta_axis = 'X'
 *    psi_axis   = 'Z'
 */
typedef struct harp_euler_transformation_struct
{
    /* Specifying bitfield ':2' is GCC extension */
    /* unsigned char phi_axis:2;
       unsigned char theta_axis:2;
       unsigned char psi_axis:2;  */
    unsigned char phi_axis;     /* first axis */
    unsigned char theta_axis;   /* second axis */
    unsigned char psi_axis;     /* third axis */
    double phi; /* first rotation angle */
    double theta;       /* second rotation angle */
    double psi; /* third rotation angle */
} harp_euler_transformation;

/* 3D vector functions */
int harp_vector3d_equal(const harp_vector3d *vectora, const harp_vector3d *vectorb);
double harp_vector3d_dotproduct(const harp_vector3d *vectora, const harp_vector3d *vectorb);
void harp_vector3d_crossproduct(harp_vector3d *vectorc, const harp_vector3d *vectora, const harp_vector3d *vectorb);
double harp_vector3d_norm(const harp_vector3d *vector);

/* Spherical point functions */
int harp_spherical_point_equal(const harp_spherical_point *pointa, const harp_spherical_point *pointb);
void harp_spherical_point_check(harp_spherical_point *point);
void harp_vector3d_from_spherical_point(harp_vector3d *vector, const harp_spherical_point *point);
void harp_spherical_point_from_vector3d(harp_spherical_point *point, const harp_vector3d *vector);
void harp_spherical_point_rad_from_deg(harp_spherical_point *point);
void harp_spherical_point_deg_from_rad(harp_spherical_point *point);
double harp_spherical_point_distance(const harp_spherical_point *pointp, const harp_spherical_point *pointq);

/* Spherical line functions */
void harp_spherical_line_begin(harp_spherical_point *point, const harp_spherical_line *line);
void harp_spherical_line_end(harp_spherical_point *point, const harp_spherical_line *line);
int harp_spherical_point_is_at_spherical_line(const harp_spherical_point *point, const harp_spherical_line *line);
void harp_inverse_euler_transformation_from_spherical_line(harp_euler_transformation *inverse_transformation,
                                                           const harp_spherical_line *line);
int8_t harp_spherical_line_spherical_line_relationship(const harp_spherical_line *linea,
                                                       const harp_spherical_line *lineb);
int harp_spherical_line_from_spherical_points(harp_spherical_line *line, const harp_spherical_point *point_begin,
                                              const harp_spherical_point *point_end);
void harp_spherical_line_spherical_line_intersection_point(const harp_spherical_line *line_p,
                                                           const harp_spherical_line *line_q,
                                                           harp_spherical_point *point_u);
int harp_spherical_polygon_get_segment(harp_spherical_line *line, const harp_spherical_polygon *polygon, int32_t i);
double harp_spherical_line_spherical_point_distance(const harp_spherical_line *line, const harp_spherical_point *point);

/* Euler rotation functions */
int harp_euler_transformation_equal(const harp_euler_transformation *euler1, const harp_euler_transformation *euler2);
void harp_euler_transformation_transform_to_zxz_euler_transformation(harp_euler_transformation *transformationout,
                                                                     const harp_euler_transformation *transformationin,
                                                                     const harp_euler_transformation *transformation);
void harp_euler_transformation_invert(harp_euler_transformation *transformation);
void harp_spherical_point_apply_euler_transformation(harp_spherical_point *pointout,
                                                     const harp_spherical_point *pointin,
                                                     const harp_euler_transformation *transformation);
void harp_euler_transformation_set_to_zxz(harp_euler_transformation *transformation);
void harp_euler_transformation_from_spherical_vector(harp_euler_transformation *transformation,
                                                     const harp_spherical_point *sphericalvectorbegin,
                                                     const harp_spherical_point *sphericalvectorend);

/* Spherical polygon functions */
int harp_spherical_polygon_new(int32_t numberofpoints, harp_spherical_polygon **polygon);
int harp_spherical_polygon_check(const harp_spherical_polygon *polygon);
void harp_spherical_polygon_delete(harp_spherical_polygon *polygon);
int harp_spherical_polygon_from_latitude_longitude_bounds(long measurement_id, long num_vertices,
                                                          const double *latitude_bounds, const double *longitude_bounds,
                                                          harp_spherical_polygon **new_polygon);
int harp_spherical_polygon_centre(harp_vector3d *vector_centre, const harp_spherical_polygon *polygon);
int harp_spherical_polygon_contains_point(const harp_spherical_polygon *polygon, const harp_spherical_point *point);
int8_t harp_spherical_polygon_spherical_line_relationship(const harp_spherical_polygon *polygon,
                                                          const harp_spherical_line *line);
int8_t harp_spherical_polygon_spherical_polygon_relationship(const harp_spherical_polygon *polygon_a,
                                                             const harp_spherical_polygon *polygon_b, int recheck);

/* Additional functions. */

/* Determine if two areas are overlapping */
int harp_spherical_polygon_overlapping(const harp_spherical_polygon *polygona, const harp_spherical_polygon *polygonb,
                                       int *polygons_are_overlapping);

/* Calculate the overlapping fraction of two areas */
int harp_spherical_polygon_overlapping_fraction(const harp_spherical_polygon *polygona,
                                                const harp_spherical_polygon *polygonb,
                                                int *polygons_are_overlapping, double *overlapping_fraction);

void harp_geographic_average(double latitude_p, double longitude_p, double latitude_q, double longitude_q,
                             double *average_latitude, double *average_longitude);
void harp_geographic_intersection(double latitude_p1, double longitude_p1, double latitude_p2, double longitude_p2,
                                  double latitude_q1, double longitude_q1, double latitude_q2, double longitude_q2,
                                  double *latitude_u, double *longitude_u);
void harp_geographic_extrapolation(double latitude_p, double longitude_p, double latitude_q, double longitude_q,
                                   double *latitude_u, double *longitude_u);

int harp_geographic_center_from_bounds(long num_vertices, const double *latitude_bounds,
                                       const double *longitude_bounds, double *center_latitude,
                                       double *center_longitude);

#endif
