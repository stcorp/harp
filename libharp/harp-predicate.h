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
