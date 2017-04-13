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

#include "harp-internal.h"
#include "harp-program.h"
#include "harp-operation.h"

#define OPERATION_FILTER_NON_EXISTANT_VARIABLE_FORMAT "cannot filter on non-existant variable %s"
#define OPERATION_FILTER_TOO_GREAT_DIMENSION_FORMAT "cannot filter on variable %s of dimension greater than 2D"
#define OPERATION_KEEP_NON_EXISTANT_VARIABLE_FORMAT "cannot keep non-existant variable %s"
#define OPERATION_FILTER_POINT_MISSING_LON "point filter expected variable longitude"
#define OPERATION_FILTER_POINT_MISSING_LAT "point filter expected variable latitude"
#define OPERATION_FILTER_POINT_WRONG_DIMENSION_FORMAT "point filter expected lat/long of dimension %s"
#define OPERATION_FILTER_AREA_MISSING_LON_BOUNDS "area filter expected variable longitude_bounds"
#define OPERATION_FILTER_AREA_MISSING_LAT_BOUNDS "area filter expected variable latitude_bounds"
#define OPERATION_FILTER_AREA_WRONG_DIMENSION_FORMAT "area filter expected lat/lon-bounds of dimensions %s"
#define OPERATION_FILTER_COLLOCATION_MISSING_INDEX "collocation filter expected either collocation index or index variable of dimension {time}"
