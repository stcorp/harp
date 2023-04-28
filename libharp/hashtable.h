/*
 * Copyright (C) 2015-2023 S[&]T, The Netherlands.
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

#ifndef HASHTABLE_H
#define HASHTABLE_H

/* This hashtable is a special hash table in which 'name' is the key and 'index' is the value.
 * The index is the 0-based index that represents the order in which the names were added to the hash table.
 * In other words, the first name that gets added to the hashtable will have index value 0, the second name will
 * have index value 1, etc.
 * Mind that the hashtable does not create a copy of the 'name' string, so you should keep a reference of this
 * string active until after you have called delete_hashtable().
 */

#define hashtable_add_name harp_hashtable_add_name
#define hashtable_delete harp_hashtable_delete
#define hashtable_get_index_from_name harp_hashtable_get_index_from_name
#define hashtable_get_index_from_name_n harp_hashtable_get_index_from_name_n
#define hashtable_insert_name harp_hashtable_insert_name
#define hashtable_new harp_hashtable_new

typedef struct hashtable_struct hashtable;

hashtable *hashtable_new(int case_sensitive);
int hashtable_add_name(hashtable *table, const char *name);
int hashtable_insert_name(hashtable *table, long index, const char *name);
long hashtable_get_index_from_name(hashtable *table, const char *name);
long hashtable_get_index_from_name_n(hashtable *table, const char *name, int name_length);
void hashtable_delete(hashtable *table);

#endif
