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
#ifndef HARP_PROGRAM_H
#define HARP_PROGRAM_H

#include "harp-operation.h"

#include <assert.h>
#include <stdlib.h>

/* HARP programs are list of harp_operations */
typedef struct harp_program_struct
{
    int num_operations;
    harp_operation **operation;
} harp_program;

int harp_program_new(harp_program **new_program);
void harp_program_delete(harp_program *program);
int harp_program_add_operation(harp_program *program, harp_operation *operation);
int harp_program_remove_operation_at_index(harp_program *program, int index);
int harp_program_remove_operation(harp_program *program, harp_operation *operation);
int harp_program_copy(const harp_program *other_program, harp_program **new_program);

/* Parser */
int harp_program_from_string(const char *str, harp_program **new_program);

/* "static analysis" */
int harp_program_verify(const harp_program *program);

#endif
