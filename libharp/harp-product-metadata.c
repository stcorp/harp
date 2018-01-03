/*
 * Copyright (C) 2015-2018 S[&]T, The Netherlands.
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
