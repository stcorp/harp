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
#include "harp-action.h"
#include "harp-product-mask.h"

#define ACTION_FILTER_NON_EXISTANT_VARIABLE_FORMAT "Cannot filter on non-existant variable %s."
#define ACTION_FILTER_TOO_GREAT_DIMENSION_FORMAT "Cannot filter on variable %s of dimension greater than 2D."
#define ACTION_INCLUDE_NON_EXISTANT_VARIABLE_FORMAT "Cannot include non-existant variable %s."
#define ACTION_FILTER_POINT_MISSING_LON "Point filter expected variable longitude."
#define ACTION_FILTER_POINT_MISSING_LAT "Point filter expected variable latitude."
#define ACTION_FILTER_POINT_WRONG_DIMENSION_FORMAT "Point filter expected lat/long of dimension %s."
#define ACTION_FILTER_AREA_MISSING_LON_BOUNDS "Area filter expectded variable longitude_bounds."
#define ACTION_FILTER_AREA_MISSING_LAT_BOUNDS "Area filter expected variable latitude_bounds."
#define ACTION_FILTER_AREA_WRONG_DIMENSION_FORMAT "Area filter expected lat/lon-bounds of dimensions %s."

int harp_product_execute_program(harp_product *product, harp_program *program);