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

#include "harp.h"
#include "harp-internal.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>

/** \defgroup harp_dataset HARP Dataset
 * The HARP Dataset module contains everything regarding HARP datasets.
 */

/** \addtogroup harp_dataset
 * @{
 */

/**
 * Create new HARP dataset.
 * The metadata will be intialized with zero product metadata elements.
 * \param new_dataset Pointer to the C variable where the new HARP product metadata will be stored.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_dataset_new(harp_dataset **new_dataset)
{
    harp_dataset *dataset = NULL;

    dataset = (harp_dataset *)malloc(sizeof(harp_dataset));
    if (dataset == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_dataset), __FILE__, __LINE__);
        return -1;
    }

    dataset->num_products = 0;

    *new_dataset = dataset;

    return 0;
}

/**
 * Delete HARP dataset.
 * \param dataset Pointer to the dataset to free.
 */
LIBHARP_API void harp_dataset_delete(harp_dataset *dataset)
{
    int i;
    for (i = 0; i < dataset->num_products; i++)
    {
        harp_product_metadata_delete(dataset->products[i]);
    }

    free(dataset->products);
    free(dataset);
}

/**
 * Print HARP dataset.
 * \param dataset Pointer to the dataset to print.
 * \param print Pointer to the function that should be used for printing.
 */
LIBHARP_API void harp_dataset_print(harp_dataset *dataset, int (*print) (const char *, ...))
{
    int i;
    for (i = 0; i < dataset->num_products; i++)
    {
        harp_product_metadata_print(dataset->products[i], print);
    }
}

/**
 * Import HARP dataset.
 * \param directory Name of directory to search for products to import as a HARP Dataset.
 * \param dataset Pointer to variable to store the imported dataset.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_import_dataset(const char *directory, harp_dataset **dataset)
{
    // TODO
    return -1;
}

/** @} */
