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

#ifndef HARP_PREDICATE_H
#define HARP_PREDICATE_H

typedef uint8_t harp_predicate_eval_func(void *args, const void *argument);
typedef void harp_predicate_delete_func(void *args);

typedef struct harp_predicate_struct
{
    void *args;
    harp_predicate_eval_func *eval;
    harp_predicate_delete_func *delete;
} harp_predicate;

typedef struct harp_predicate_set_struct
{
    int num_predicates;
    harp_predicate **predicate;
} harp_predicate_set;

int harp_predicate_new(harp_predicate_eval_func * eval_func, void *args, harp_predicate_delete_func * delete_func,
                       harp_predicate **new_predicate);
void harp_predicate_delete(harp_predicate *predicate);

void harp_predicate_set_delete(harp_predicate_set *predicate_set);
int harp_predicate_set_new(harp_predicate_set **new_predicate_set);
int harp_predicate_set_add_predicate(harp_predicate_set *predicate_set, harp_predicate *predicate);

#endif
