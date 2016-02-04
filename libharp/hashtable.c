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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "hashtable.h"

struct hashtable_struct
{
    unsigned char *count;       /* number of stored names that match this specific hash */
    const char **name;
    int *name_length;
    long *index;
    unsigned char power;
    long size;
    long used;
    int case_sensitive;
};

#define INITIAL_POWER 5

static unsigned long strcasehash(const char *str, int *length)
{
    unsigned char *c = (unsigned char *)str;
    unsigned long hash = 0;
    int l = 0;

    while (*c != '\0')
    {
        unsigned char lc = *c++;

        /* we use hash = hash * 1000003 ^ char */
        hash = (hash * 0xF4243) ^ (lc + 32 * (lc >= 'A' && lc <= 'Z'));
        l++;
    }

    *length = l;
    return hash;
}

static unsigned long strncasehash(const char *str, int length)
{
    unsigned char *c = (unsigned char *)str;
    unsigned long hash = 0;
    int n = 0;

    while (n < length && *c != '\0')
    {
        unsigned char lc = *c++;

        /* we use hash = hash * 1000003 ^ char */
        hash = (hash * 0xF4243) ^ (lc + 32 * (lc >= 'A' && lc <= 'Z'));
        n++;
    }

    return hash;
}

static unsigned long strhash(const char *str, int *length)
{
    unsigned char *c = (unsigned char *)str;
    unsigned long hash = 0;
    int l = 0;

    while (*c != '\0')
    {
        /* we use hash = hash * 1000003 ^ char */
        hash = (hash * 0xF4243) ^ (unsigned char)(*c++);
        l++;
    }

    *length = l;
    return hash;
}

static unsigned long strnhash(const char *str, int length)
{
    unsigned char *c = (unsigned char *)str;
    unsigned long hash = 0;
    int n = 0;

    while (n < length && *c != '\0')
    {
        /* we use hash = hash * 1000003 ^ char */
        hash = (hash * 0xF4243) ^ (unsigned char)(*c++);
        n++;
    }

    return hash;
}

hashtable *hashtable_new(int case_sensitive)
{
    hashtable *table;

    table = malloc(sizeof(hashtable));
    if (table == NULL)
    {
        return NULL;
    }
    table->count = NULL;
    table->name = NULL;
    table->name_length = NULL;
    table->index = NULL;
    table->power = INITIAL_POWER;
    table->size = 0;
    table->used = 0;
    table->case_sensitive = case_sensitive;

    return table;
}

int hashtable_insert_name(hashtable *table, long index, const char *name)
{
    unsigned long mask;
    unsigned long hash;
    unsigned char step;
    int name_length;
    long i;

    hash = (table->case_sensitive ? strhash(name, &name_length) : strcasehash(name, &name_length));

    if (table->size == 0)
    {
        table->size = 1 << table->power;
        table->count = (unsigned char *)malloc(table->size * sizeof(unsigned char));
        assert(table->count != NULL);
        table->name = (const char **)malloc(table->size * sizeof(const char *));
        assert(table->name != NULL);
        table->name_length = (int *)malloc(table->size * sizeof(int));
        assert(table->name_length != NULL);
        table->index = (long *)malloc(table->size * sizeof(long));
        assert(table->index != NULL);
        memset(table->count, 0, table->size);
    }
    else
    {
        /* check if the entry is not already in the table */
        mask = (unsigned long)table->size - 1;
        i = hash & mask;
        step = 0;
        while (table->count[i])
        {
            if (name_length == table->name_length[i] &&
                (table->case_sensitive ? strcmp(name, table->name[i]) : strcasecmp(name, table->name[i])) == 0)
            {
                return -1;
            }
            if (!step)
            {
                step = (unsigned char)((((hash & ~mask) >> (table->power - 1)) & (mask >> 2)) | 1);
            }
            i += (i < step ? table->size : 0) - step;
        }
    }

    /* enlarge table if necessary */
    if (table->used == (table->size >> 1))
    {
        unsigned char *new_count;
        const char **new_name;
        int *new_name_length;
        long *new_index;
        unsigned long new_mask;
        unsigned char new_power;
        long new_size;

        /* if the table is half full we need to extend it */

        new_power = table->power + 1;
        new_size = table->size << 1;
        new_mask = (unsigned long)new_size - 1;

        new_count = (unsigned char *)malloc(new_size * sizeof(unsigned char));
        assert(new_count != NULL);
        new_name = (const char **)malloc(new_size * sizeof(const char *));
        assert(new_name != NULL);
        new_name_length = (int *)malloc(new_size * sizeof(int));
        assert(new_name_length != NULL);
        new_index = (long *)malloc(new_size * sizeof(long));
        assert(new_index != NULL);

        memset(new_count, 0, new_size);
        for (i = 0; i < table->size; i++)
        {
            if (table->count[i])
            {
                unsigned long new_hash;
                int length;
                long j;

                new_hash = (table->case_sensitive ? strhash(table->name[i], &length) :
                            strcasehash(table->name[i], &length));
                j = new_hash & new_mask;
                step = 0;
                while (new_count[j])
                {
                    new_count[j]++;
                    if (!step)
                    {
                        step = (unsigned char)((((new_hash & ~new_mask) >> (new_power - 1)) & (new_mask >> 2)) | 1);
                    }
                    j += (j < step ? new_size : 0) - step;
                }
                new_count[j] = 1;
                new_name[j] = table->name[i];
                new_name_length[j] = table->name_length[i];
                new_index[j] = table->index[i];
            }
        }
        free(table->count);
        free(table->name);
        free(table->name_length);
        free(table->index);
        table->count = new_count;
        table->name = new_name;
        table->name_length = new_name_length;
        table->index = new_index;
        table->power = new_power;
        table->size = new_size;
    }

    /* increase index of all items that come after the new one */
    if (index < table->used)
    {
        for (i = 0; i < table->size; i++)
        {
            if (table->count[i] && table->index[i] >= index)
            {
                table->index[i]++;
            }
        }
    }

    /* add entry */
    mask = (unsigned long)table->size - 1;
    i = hash & mask;
    step = 0;
    while (table->count[i])
    {
        table->count[i]++;
        if (!step)
        {
            step = (unsigned char)((((hash & ~mask) >> (table->power - 1)) & (mask >> 2)) | 1);
        }
        i += (i < step ? table->size : 0) - step;
    }

    table->count[i] = 1;
    table->name[i] = name;
    table->name_length[i] = name_length;
    table->index[i] = index;
    table->used++;

    return 0;
}

int hashtable_add_name(hashtable *table, const char *name)
{
    return hashtable_insert_name(table, table->used, name);
}

long hashtable_get_index_from_name(hashtable *table, const char *name)
{
    unsigned long mask;
    unsigned long hash;
    unsigned char step;
    int name_length;
    long i;

    if (table->count == NULL)
    {
        return -1;
    }

    hash = (table->case_sensitive ? strhash(name, &name_length) : strcasehash(name, &name_length));
    mask = (unsigned long)table->size - 1;
    i = hash & mask;
    step = 0;
    while (table->count[i])
    {
        if (name_length == table->name_length[i] &&
            (table->case_sensitive ? strcmp(name, table->name[i]) : strcasecmp(name, table->name[i])) == 0)
        {
            return table->index[i];
        }
        if (!step)
        {
            step = (unsigned char)((((hash & ~mask) >> (table->power - 1)) & (mask >> 2)) | 1);
        }
        i += (i < step ? table->size : 0) - step;
    }

    return -1;
}

long hashtable_get_index_from_name_n(hashtable *table, const char *name, int name_length)
{
    unsigned long mask;
    unsigned long hash;
    unsigned char step;
    long i;

    if (table->count == NULL)
    {
        return -1;
    }

    hash = (table->case_sensitive ? strnhash(name, name_length) : strncasehash(name, name_length));
    mask = (unsigned long)table->size - 1;
    i = hash & mask;
    step = 0;
    while (table->count[i])
    {
        if (name_length == table->name_length[i] &&
            (table->case_sensitive ?
             strncmp(name, table->name[i], name_length) : strncasecmp(name, table->name[i], name_length)) == 0)
        {
            return table->index[i];
        }
        if (!step)
        {
            step = (unsigned char)((((hash & ~mask) >> (table->power - 1)) & (mask >> 2)) | 1);
        }
        i += (i < step ? table->size : 0) - step;
    }

    return -1;
}

void hashtable_delete(hashtable *table)
{
    if (table != NULL)
    {
        if (table->count != NULL)
        {
            free(table->count);
        }
        if (table->name != NULL)
        {
            free(table->name);
        }
        if (table->name_length != NULL)
        {
            free(table->name_length);
        }
        if (table->index != NULL)
        {
            free(table->index);
        }
        free(table);
    }
}
