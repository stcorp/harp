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

typedef struct harp_product_lazy_info_struct {
    int (*get_variable_dimension_by_name) (void *product, char *name, long *num_dimensions, long **dimension);
} harp_product_lazy_info;

int harp_program_make_mask(harp_product_lazy_info *lazy_product_info, harp_program *filters,
                           harp_product_mask *mask);

int harp_product_execute_program(harp_product *product, harp_program *program);
