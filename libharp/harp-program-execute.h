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

#include "harp-internal.h"
#include "harp-program.h"
#include "harp-operation.h"

#define OPERATION_FILTER_NON_EXISTANT_VARIABLE_FORMAT "cannot filter on non-existant variable %s"
#define OPERATION_FILTER_TOO_GREAT_DIMENSION_FORMAT "cannot filter on variable %s of dimension greater than 2D"
#define OPERATION_KEEP_NON_EXISTANT_VARIABLE_FORMAT "cannot keep non-existant variable %s"
#define OPERATION_FILTER_POINT_MISSING_LON "point filter expected variable longitude"
#define OPERATION_FILTER_POINT_MISSING_LAT "point filter expected variable latitude"
#define OPERATION_FILTER_POINT_WRONG_DIMENSION_FORMAT "point filter expected lat/long of dimension %s"
#define OPERATION_FILTER_AREA_MISSING_LON_BOUNDS "area filter expectded variable longitude_bounds"
#define OPERATION_FILTER_AREA_MISSING_LAT_BOUNDS "area filter expected variable latitude_bounds"
#define OPERATION_FILTER_AREA_WRONG_DIMENSION_FORMAT "area filter expected lat/lon-bounds of dimensions %s"
#define OPERATION_FILTER_COLLOCATION_MISSING_INDEX "collocation filter expected either collocation index or index variable of dimension {time}"

int harp_product_execute_program(harp_product *product, harp_program *program);
