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
