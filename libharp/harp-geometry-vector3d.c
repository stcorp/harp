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
