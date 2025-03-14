/*
 * Copyright (C) 2015-2025 S[&]T, The Netherlands.
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
#include "coda.h"

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
        if (metadata->format != NULL)
        {
            free(metadata->format);
        }
        if (metadata->source_product != NULL)
        {
            free(metadata->source_product);
        }
        if (metadata->history != NULL)
        {
            free(metadata->history);
        }
        free(metadata);
    }
}

/**
 * Create new product metadata.
 * The metadata will be initialized with 0.0 datetime_start/end.
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
    metadata->format = NULL;
    metadata->source_product = NULL;
    metadata->history = NULL;

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
 * This will print a comma-separated list of:
 *  - filename
 *  - datetime_start
 *  - datetime_stop
 *  - time (dimension length)
 *  - latitude (dimension length)
 *  - longitude (dimension length)
 *  - vertical (dimension length)
 *  - spectral (dimension length)
 *  - source_product
 * \param metadata Pointer to the metadata to print.
 * \param print Pointer to the function that should be used for printing.
 */
LIBHARP_API void harp_product_metadata_print(harp_product_metadata *metadata, int (*print)(const char *, ...))
{
    char datetime_string[16];
    int i;

    print("%s,", metadata->filename);
    if (coda_time_double_to_string(metadata->datetime_start * 86400, "yyyyMMdd'T'HHmmss", datetime_string) == 0)
    {
        print("%s,", datetime_string);
    }
    else
    {
        print(",");
    }
    if (coda_time_double_to_string(metadata->datetime_stop * 86400, "yyyyMMdd'T'HHmmss", datetime_string) == 0)
    {
        print("%s,", datetime_string);
    }
    else
    {
        print(",");
    }
    for (i = 0; i < HARP_NUM_DIM_TYPES; i++)
    {
        print("%ld", metadata->dimension[i]);
        print(",");
    }
    print("%s\n", metadata->source_product);
}

/** @} */
