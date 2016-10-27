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
#ifndef HARP_PARSER_STATE_H
#define HARP_PARSER_STATE_H

#include "harp-operation.h"
#include "harp-program.h"

#include <assert.h>
#include <stdlib.h>

typedef struct harp_parser_state_struct
{
    int hasError;
    char *error;
    harp_program *result;
} harp_parser_state;

typedef struct float_with_unit_struct {
    float value;
    char *unit;
} float_with_unit;

typedef struct int_with_unit_struct {
    int value;
    char *unit;
} int_with_unit;

typedef struct harp_sized_array_struct {
    int num_elements;
    harp_array array;
} harp_sized_array;

int harp_parser_state_new(harp_parser_state **new_state);
void harp_parser_state_delete(harp_parser_state *state);
void harp_parser_state_set_error(harp_parser_state *state, const char *error);
int harp_sized_array_new(harp_sized_array **new_array);
void harp_sized_array_delete(harp_sized_array *l);
int harp_sized_array_add_string(harp_sized_array *harp_sized_array, const char *str);
int harp_sized_array_add_double(harp_sized_array *harp_sized_array, const double d);

#endif
