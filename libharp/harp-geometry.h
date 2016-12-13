/*
 * Copyright (C) 2015-2016 S[&]T, The Netherlands.
 *
 * This file is part of HARP.
 *
 * HARP is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * HARP is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with HARP; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef HARP_GEOMETRY_H
#define HARP_GEOMETRY_H

#include "harp-internal.h"
#include "harp-constants.h"

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <math.h>

#define HARP_GEOMETRY_LINE_AVOID     1  /* line avoids other line */
#define HARP_GEOMETRY_LINE_EQUAL     2  /* lines are equal */
#define HARP_GEOMETRY_LINE_CONT_LINE 3  /* line contains line */
#define HARP_GEOMETRY_LINE_CROSS     4  /* lines cross each other */
#define HARP_GEOMETRY_LINE_CONNECT   5  /* lines are "connected" */
#define HARP_GEOMETRY_LINE_OVER      6  /* lines overlap each other */

#define HARP_GEOMETRY_POLY_AVOID 0      /* polygon avoids other polygon */
#define HARP_GEOMETRY_POLY_CONT  1      /* polygon contains other polygon */
#define HARP_GEOMETRY_POLY_OVER  2      /* polygons overlap */

#define HARP_GEOMETRY_LINE_POLY_AVOID 0 /* line avoids polygon */
#define HARP_GEOMETRY_POLY_CONT_LINE  1 /* polygon contains line */
#define HARP_GEOMETRY_LINE_POLY_OVER  2 /* line overlap polygon */

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

/* Define a circle on a sphere */
typedef struct harp_spherical_circle_struct
{
    harp_spherical_point center;        /* the center of circle */
    double radius;      /* the circle radius in [rad] */
} harp_spherical_circle;

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

/* Define all possible relationships between spherical lines */
extern const int8_t harp_spherical_line_relationship_avoid;     /* line avoids other line */
extern const int8_t harp_spherical_line_relationship_equal;     /* lines are equal */
extern const int8_t harp_spherical_line_relationship_contain_line;      /* line contains line */
extern const int8_t harp_spherical_line_relationship_cross;     /* lines cross each other */
extern const int8_t harp_spherical_line_relationship_connect;   /* line are "connected" */
extern const int8_t harp_spherical_line_relationship_overlap;   /* lines overlap each other */

/* Define all possible relationships between spherical polygons */
typedef enum harp_spherical_polygon_relationship_enum
{
    harp_spherical_polygon_relationship_unknown = -1,
    harp_spherical_polygon_relationship_avoid = 0,
    harp_spherical_polygon_relationship_near = 1,
    harp_spherical_polygon_relationship_equal = 2,
    harp_spherical_polygon_relationship_a_contains_b = 3,
    harp_spherical_polygon_relationship_b_contains_a = 4,
    harp_spherical_polygon_relationship_overlap = 5,
    harp_spherical_polygon_relationship_touch = 6
} harp_spherical_polygon_relationship;

/* 3D vector functions */
harp_vector3d *harp_vector3d_new(double x, double y, double z);
void harp_vector3d_delete(harp_vector3d *vector);
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
double harp_spherical_point_distance_in_meters(const harp_spherical_point *pointp, const harp_spherical_point *pointq);
int harp_spherical_point_at_distance_and_angle(const harp_spherical_point *point_a, double radius,
                                               double azimuth_angle, harp_spherical_point *point_b);

/* Spherical point array functions */
int harp_spherical_point_array_new(harp_spherical_point_array **new_point_array);
void harp_spherical_point_array_delete(harp_spherical_point_array *point_array);
int harp_spherical_point_array_add_point(harp_spherical_point_array *point_array, const harp_spherical_point *point_in);
void harp_spherical_point_array_remove_point_at_index(harp_spherical_point_array *point_array, int32_t index);

/* Spherical line functions */
int harp_spherical_line_equal(const harp_spherical_line *line1, const harp_spherical_line *line2);
void harp_spherical_line_meridian(harp_spherical_line *line, double lon);
void harp_spherical_line_begin(harp_spherical_point *point, const harp_spherical_line *line);
void harp_spherical_line_end(harp_spherical_point *point, const harp_spherical_line *line);
void harp_inverse_euler_transformation_from_spherical_line(harp_euler_transformation *inverse_transformation,
                                                           const harp_spherical_line *line);
void harp_euler_transformation_from_spherical_line(harp_euler_transformation *transformation,
                                                   const harp_spherical_line *line);
int harp_spherical_point_is_at_spherical_line(const harp_spherical_point *point, const harp_spherical_line *line);
void harp_inverse_euler_transformation_from_spherical_line(harp_euler_transformation *inverse_transformation,
                                                           const harp_spherical_line *line);
void harp_spherical_line_apply_euler_transformation(harp_spherical_line *lineout, const harp_spherical_line *linein,
                                                    const harp_euler_transformation *transformation);
int8_t harp_spherical_line_spherical_line_relationship(const harp_spherical_line *linea,
                                                       const harp_spherical_line *lineb);
int harp_spherical_line_from_spherical_points(harp_spherical_line *line, const harp_spherical_point *point_begin,
                                              const harp_spherical_point *point_end);
int harp_spherical_line_point_by_length(harp_spherical_point *point, const harp_spherical_line *line, double length);
void harp_spherical_line_spherical_line_intersection_point(const harp_spherical_line *line_p,
                                                           const harp_spherical_line *line_q,
                                                           harp_spherical_point *point_u);
int harp_spherical_polygon_get_segment(harp_spherical_line *line, const harp_spherical_polygon *polygon, int32_t i);
double harp_spherical_line_spherical_point_distance(const harp_spherical_line *line, const harp_spherical_point *point);
double harp_spherical_line_spherical_point_distance_in_meters(const harp_spherical_line *line,
                                                              const harp_spherical_point *point);

/* Euler rotation functions */
int harp_euler_transformation_equal(const harp_euler_transformation *euler1, const harp_euler_transformation *euler2);
void harp_euler_transformation_transform_to_zxz_euler_transformation(harp_euler_transformation *transformationout,
                                                                     const harp_euler_transformation *transformationin,
                                                                     const harp_euler_transformation *transformation);
void harp_euler_transformation_invert(harp_euler_transformation *transformation);
void harp_spherical_point_apply_euler_transformation(harp_spherical_point *pointout,
                                                     const harp_spherical_point *pointin,
                                                     const harp_euler_transformation *transformation);
int harp_vector3d_apply_euler_transformation(harp_vector3d *vectorout, const harp_vector3d *vectorin,
                                             const harp_euler_transformation *transformation);
void harp_euler_transformation_set_to_zxz(harp_euler_transformation *transformation);
void harp_inverse_euler_transformation_from_spherical_vector(harp_euler_transformation *inverse_transformation,
                                                             const harp_spherical_point *sphericalvectorbegin,
                                                             const harp_spherical_point *sphericalvectorend);
void harp_euler_transformation_from_spherical_vector(harp_euler_transformation *transformation,
                                                     const harp_spherical_point *sphericalvectorbegin,
                                                     const harp_spherical_point *sphericalvectorend);

/* Spherical polygon functions */
harp_spherical_polygon *harp_spherical_polygon_new(int32_t numberofpoints);
int harp_spherical_polygon_check(const harp_spherical_polygon *polygon);
int harp_spherical_polygon_equal(const harp_spherical_polygon *polygon_a, const harp_spherical_polygon *polygon_b,
                                 int direction);
void harp_spherical_polygon_delete(harp_spherical_polygon *polygon);
int harp_spherical_polygon_from_latitude_longitude_bounds(long measurement_id, long num_vertices,
                                                          const double *latitude_bounds, const double *longitude_bounds,
                                                          harp_spherical_polygon **new_polygon);
int harp_spherical_polygon_from_point_array(const harp_spherical_point_array *point_array,
                                            harp_spherical_polygon **new_polygon);
harp_spherical_polygon *harp_spherical_polygon_duplicate(const harp_spherical_polygon *polygon_in);
int harp_spherical_polygon_centre(harp_vector3d *vector_centre, const harp_spherical_polygon *polygon);
int harp_spherical_polygon_contains_point(const harp_spherical_polygon *polygon, const harp_spherical_point *point);
int8_t harp_spherical_polygon_spherical_line_relationship(const harp_spherical_polygon *polygon,
                                                          const harp_spherical_line *line);
int8_t harp_spherical_polygon_spherical_polygon_relationship(const harp_spherical_polygon *polygon_a,
                                                             const harp_spherical_polygon *polygon_b, int recheck);
int harp_spherical_polygon_get_signed_surface_area(const harp_spherical_polygon *polygon_in, double *signed_area);
int harp_spherical_polygon_get_surface_area(const harp_spherical_polygon *polygon_in, double *area);
int harp_spherical_polygon_intersect(const harp_spherical_polygon *polygon_a, const harp_spherical_polygon *polygon_b,
                                     harp_spherical_polygon **polygon_intersect, int *has_intersect);
double harp_spherical_polygon_spherical_point_distance(const harp_spherical_polygon *polygon,
                                                       const harp_spherical_point *point);
double harp_spherical_polygon_spherical_point_distance_in_meters(const harp_spherical_polygon *polygon,
                                                                 const harp_spherical_point *point);

/* Additional functions. */

/* Calculate the point distance [m] between two points on a sphere */
int harp_spherical_point_distance_from_latitude_longitude(double latitude_a, double longitude_a,
                                                          double latitude_b, double longitude_b,
                                                          double *point_distance);

/* Determine if two areas are overlapping */
int harp_spherical_polygon_overlapping(const harp_spherical_polygon *polygona, const harp_spherical_polygon *polygonb,
                                       int *polygons_are_overlapping);

/* Calculate the overlapping percentage [%] of two areas */
int harp_spherical_polygon_overlapping_percentage(const harp_spherical_polygon *polygona,
                                                  const harp_spherical_polygon *polygonb,
                                                  int *polygons_are_overlapping, double *overlapping_percentage);


/* Convert latitude, longitude [deg] to Cartesian coordinates [m] */
void harp_wgs84_ellipsoid_cartesian_coordinates_from_latitude_longitude(double latitude, double longitude,
                                                                        double *new_x, double *new_y, double *new_z);

/* Convert Cartesian coordinates [m] to latitude, longitude [deg] */
void harp_wgs84_ellipsoid_latitude_longitude_from_cartesian_coordinates(double x, double y, double z,
                                                                        double *new_latitude, double *new_longitude);

/* Calculate the point distance [m] between two points [deg] on the WGS84 ellipsoid */
int harp_wgs84_ellipsoid_point_distance_from_latitude_and_longitude(double latitude_a, double longitude_a,
                                                                    double latitude_b, double longitude_b,
                                                                    double *point_distance);

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
