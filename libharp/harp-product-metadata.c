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

#include "hashtable.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

/** \defgroup harp_product_metadata HARP Product Metadata
 * The HARP Product Metadata module contains everything related to HARP product metadata.
 */

/** \addtogroup harp_product_metadata
 * @{
 */

/**
 * Delete product metadata.
 * Remove metadata and all attached variables and attributes.
 * \param metadata HARP metadata.
 */
LIBHARP_API void harp_product_metadata_delete(harp_product_metadata *metadata)
{
    if (metadata != NULL)
    {
        if (metadata->filename != NULL)
        {
            free(metadata->filename);
        }
        if (metadata->source_product != NULL)
        {
            free(metadata->source_product);
        }
        free(metadata);
    }
}

/**
 * Create new product metadata.
 * The metadata will be intialized with 0.0 datetime_start/end.
 * \param new_metadata Pointer to the C variable where the new HARP product metadata will be stored.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_product_metadata_new(harp_product_metadata **new_metadata)
{
    harp_product_metadata *metadata;
    int i;

    metadata = (harp_product_metadata *)malloc(sizeof(harp_product_metadata));
    if (metadata == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_product_metadata), __FILE__, __LINE__);
        return -1;
    }

    metadata->filename = NULL;
    metadata->source_product = NULL;

    for (i = 0; i < HARP_NUM_DIM_TYPES; i++)
    {
        metadata->dimension[i] = 0;
    }

    metadata->datetime_start = 0.0;
    metadata->datetime_stop = 0.0;

    *new_metadata = metadata;

    return 0;
}

/**
 * Print product metadata.
 * \param metadata Pointer to the metadata to print.
 * \param print Pointer to the function that should be used for printing.
 */
LIBHARP_API void harp_product_metadata_print(harp_product_metadata *metadata, int (*print) (const char *, ...))
{
    int i;

    print("filename: %s, ", metadata->filename);
    print("source_product: %s, ", metadata->source_product);
    print("date_start: %f, ", metadata->datetime_start);
    print("date_stop: %f, ", metadata->datetime_stop);
    print("dimension: {");
    for (i = 0; i < HARP_NUM_DIM_TYPES; i++)
    {
        if (metadata->dimension[i] != 0)
        {
            print("%s = ", harp_get_dimension_type_name(i));
        }
        print("%ld", metadata->dimension[i]);
        if (i < HARP_NUM_DIM_TYPES - 1)
        {
            print(", ");
        }
    }
    print("}");
}

/** @} */
