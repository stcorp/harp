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

#include "harp-geometry.h"

/* Create new vector */
harp_vector3d *harp_vector3d_new(double x, double y, double z)
{
    /* Allocate memory for the new vector */
    harp_vector3d *vector = malloc(3 * sizeof(double));

    /* Set the 3 Cartesian coordinates */
    vector->x = x;
    vector->y = y;
    vector->z = z;

    return vector;
}

/* Delete vector */
void harp_vector3d_delete(harp_vector3d *vector)
{
    if (vector)
    {
        free(vector);
    }
    vector = NULL;
}

/* Check if two 3D vectors are equal */
int harp_vector3d_equal(const harp_vector3d *vectora, const harp_vector3d *vectorb)
{
    return (HARP_GEOMETRY_FPeq(vectora->x, vectorb->x) &&
            HARP_GEOMETRY_FPeq(vectora->y, vectorb->y) && HARP_GEOMETRY_FPeq(vectora->z, vectorb->z));
}

/* Calculate the dotproduct of two 3D vectors
 *
 * Given two 3D vectors, a and b, their dot product
 * (i.e. a scalar value) is given by:
 *
 *   dotproduct = (a . b) =  a.x * b.x + a.y * b.y + a.z * b.z
 */
double harp_vector3d_dotproduct(const harp_vector3d *vectora, const harp_vector3d *vectorb)
{
    return ((vectora->x * vectorb->x) + (vectora->y * vectorb->y) + (vectora->z * vectorb->z));
}

/* Calculate the cross product of two 3d vectors
 *
 * Given two 3D vectors, a and b, their cross product
 * (i.e. a new 3D vector c) is given by:
 *
 *   c = (a x b) = (a.y * b.z - a.z * b.y,
 *                  a.z * b.x - a.x * b.z,
 *                  a.x * b.y - a.y * b.x)
 */
void harp_vector3d_crossproduct(harp_vector3d *vectorc, const harp_vector3d *vectora, const harp_vector3d *vectorb)
{
    vectorc->x = vectora->y * vectorb->z - vectora->z * vectorb->y;
    vectorc->y = vectora->z * vectorb->x - vectora->x * vectorb->z;
    vectorc->z = vectora->x * vectorb->y - vectora->y * vectorb->x;
}

/* Calculate the norm of a 3D vector */
double harp_vector3d_norm(const harp_vector3d *vector)
{
    double norm_squared = harp_vector3d_dotproduct(vector, vector);

    return sqrt(norm_squared);
}
