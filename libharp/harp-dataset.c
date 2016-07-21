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
#include "hashtable.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#define DATASET_BLOCK_SIZE 16

#include <stdlib.h>
#include <string.h>
#ifdef HAVE_DIRENT_H
#include <dirent.h>
#include <stdio.h>
#include <harp.h>
#include <assert.h>

#endif
#ifdef WIN32
#include "windows.h"
#endif

/** \defgroup harp_dataset HARP harp_dataset
 * The HARP harp_dataset module contains everything regarding HARP datasets.
 */

/**
 * Grow dataset with one block if necessary to accomodate another entry.
 */
static int grow_dataset(harp_dataset *dataset)
{
    /* Make space for new entry */
    if (dataset->num_products % DATASET_BLOCK_SIZE == 0)
    {
        /* grow the product_source array by one block */
        dataset->product_source = (char **) realloc(dataset->product_source,
                                                               (size_t) (dataset->num_products + DATASET_BLOCK_SIZE)
                                                               * sizeof(char **));
        if (!dataset->product_source)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           (long) (dataset->num_products + DATASET_BLOCK_SIZE) * sizeof(char **),
                           __FILE__, __LINE__);
            return -1;
        }

        if (dataset->metadata != NULL)
        {
            /* grow the metadata array by one block */
            dataset->metadata = (harp_product_metadata **) realloc(dataset->metadata,
                                                                   (size_t) (dataset->num_products + DATASET_BLOCK_SIZE)
                                                                   * sizeof(harp_product_metadata));
            if (!dataset->metadata)
            {
                harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                               (long) (dataset->num_products + DATASET_BLOCK_SIZE) * sizeof(char *), __FILE__,
                               __LINE__);
                return -1;
            }
        }
    }

    return 0;
}

/**
 * Check that filename is a directory and can be read.
 */
static int is_directory(const char *directoryname)
{
    struct stat statbuf;

    /* stat() the directory to be opened */
    if (stat(directoryname, &statbuf) != 0)
    {
        if (errno == ENOENT)
        {
            harp_set_error(HARP_ERROR_FILE_NOT_FOUND, "could not find %s", directoryname);
        }
        else
        {
            harp_set_error(HARP_ERROR_FILE_OPEN, "could not open %s (%s)", directoryname, strerror(errno));
        }
        return -1;
    }

    /* check that the file is a directory */
    if (statbuf.st_mode & S_IFDIR)
    {
        /* Return 'true' */
        return 1;
    }

    /* Return 'false' */
    return 0;
}

/**
 * Check that filename is a regular file and can be read.
 */
static int check_file(const char *filename)
{
    struct stat statbuf;

    /* stat() the file to be opened */
    if (stat(filename, &statbuf) != 0)
    {
        if (errno == ENOENT)
        {
            harp_set_error(HARP_ERROR_FILE_NOT_FOUND, "could not find %s", filename);
        }
        else
        {
            harp_set_error(HARP_ERROR_FILE_OPEN, "could not open %s (%s)", filename, strerror(errno));
        }
        return -1;
    }

    /* check that the file is a regular file */
    if ((statbuf.st_mode & S_IFREG) == 0)
    {
        harp_set_error(HARP_ERROR_FILE_OPEN, "could not open %s (not a regular file)", filename);
        return -1;
    }

    return 0;
}

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
        harp_product_metadata_delete(dataset->metadata[i]);
    }

    free(dataset->metadata);
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
        harp_product_metadata_print(dataset->metadata[i], print);
    }
}

/**
 * Read metadata for all products found in the directory indicated by path.
 * \param pathname Path of the directory to search for products to import as a HARP harp_dataset.
 * \param dataset Pointer to dataset to add the metadata to.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_dataset_add_directory(harp_dataset *dataset, const char *pathname)
{
#ifdef WIN32
    WIN32_FIND_DATA FileData;
    HANDLE hSearch;
    BOOL fFinished;
    char *pattern;

    pattern = malloc(strlen(pathname) + 4 + 1);
    if (pattern == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       (long)strlen(pathname) + 4 + 1, __FILE__, __LINE__);
        return -1;
    }
    sprintf(pattern, "%s\\*.*", pathname);
    hSearch = FindFirstFile(pattern, &FileData);
    free(pattern);

    if (hSearch == INVALID_HANDLE_VALUE)
    {
        if (GetLastError() == ERROR_FILE_NOT_FOUND || GetLastError() == ERROR_NO_MORE_FILES)
        {
            /* no files found */
            return 0;
        }
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "could not access directory '%s'", pathname);
        return -1;
    }

    fFinished = FALSE;
    while (!fFinished)
    {
        if (!(FileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        {
            char *filepath;

            filepath = malloc(strlen(pathname) + 1 + strlen(FileData.cFileName) + 1);
            if (filepath == NULL)
            {
                harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                               (long)strlen(pathname) + 1 + strlen(FileData.cFileName) + 1, __FILE__, __LINE__);
                FindClose(hSearch);
                return -1;
            }
            sprintf(filepath, "%s\\%s", pathname, FileData.cFileName);
            if (check_file(filepath) == 0)
            {
                harp_dataset_add_file(dataset, filepath);
            }
            else
            {
                harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "'%s' is not a valid HARP file", filepath);
                free(filepath);
                FindClose(hSearch);
                return -1;
            }
            free(filepath);
        }

        if (!FindNextFile(hSearch, &FileData))
        {
            if (GetLastError() == ERROR_NO_MORE_FILES)
            {
                fFinished = TRUE;
            }
            else
            {
                harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "could not retrieve directory entry");
                FindClose(hSearch);
                return -1;
            }
        }
    }
    FindClose(hSearch);
#else
    DIR *dirp = NULL;
    struct dirent *dp = NULL;

    /* Open the directory */
    dirp = opendir(pathname);

    if (dirp == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "could not open directory %s", pathname);
        closedir(dirp);
        return -1;
    }

    /* Walk through files in directory and add filenames to dataset */
    while ((dp = readdir(dirp)) != NULL)
    {
        char *filepath = NULL;

        /* Skip '.' and '..' */
        if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0)
        {
            continue;
        }

        /* Add path before filename */
        filepath = malloc(strlen(pathname) + 1 + strlen(dp->d_name) + 1);
        if (filepath == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           (long)strlen(pathname) + 1 + strlen(dp->d_name) + 1, __FILE__, __LINE__);
            closedir(dirp);
            return -1;
        }
        sprintf(filepath, "%s/%s", pathname, dp->d_name);

        if (is_directory(filepath))
        {
            /* Skip subdirectories */
            free(filepath);
            continue;
        }

        if (check_file(filepath) == 0)
        {
            harp_dataset_add_file(dataset, filepath);
        }
        else
        {
            /* Exit, file type is not supported */
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "'%s' is not a valid HARP file", filepath);
            free(filepath);
            closedir(dirp);
            return -1;
        }
        free(filepath);
    }

    closedir(dirp);
#endif

    return 0;
}

/**
 * Read metadata for product indicated by filename and add it to the dataset.
 * This will not check if filename already appears in the dataset.
 * \param filename Path of the directory to search for products to import as a HARP harp_dataset.
 * \param dataset Pointer to dataset to add the metadata to.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_dataset_add_file(harp_dataset *dataset, const char *filename)
{
    harp_product_metadata *metadata = NULL;

    /* Import the metadata */
    if (harp_import_product_metadata(filename, &metadata) != 0)
    {
        return -1;
    }

    return harp_dataset_add_product(dataset, metadata->source_product, metadata);
}

/**
 * Lookup the index of source_product in the given dataset.
 * \param dataset Pointer to dataset to get index in.
 * \param dataset Pointer of product to get index of.
 * \return index of source_product in dataset or -1 if not found
 */
LIBHARP_API long harp_dataset_get_index_from_source_product(harp_dataset *dataset, const char *source_product)
{
    return harp_hashtable_get_index_from_name(dataset->product_to_index, source_product);
}

/**
 * Add a product to a dataset. Metadata is only allowed to be NULL if the dataset does not track metadata.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_dataset_add_product(harp_dataset *dataset, const char *source_product,
                                         harp_product_metadata *metadata)
{
    long index = harp_dataset_get_index_from_source_product(dataset, source_product);

    /* if source product does not already appear, add it */
    if (index < 0)
    {
        grow_dataset(dataset);

        dataset->num_products++;

        dataset->product_source[dataset->num_products - 1] = strdup(source_product);
        if(dataset->product_source[dataset->num_products - 1] == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "Out of memory (failed to duplicate string) (%s:%u)",
                           __FILE__, __LINE__);
            return -1;
        }

        if (dataset->metadata != NULL)
        {
            if (metadata == NULL)
            {
                harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "Adding NULL as metadata not allowed in dataset. (%s:%u)",
                               __FILE__, __LINE__);
                return -1;
            }

            dataset->metadata[dataset->num_products - 1] = metadata;
        }

        /* add it to the index */
        assert(harp_hashtable_add_name(dataset->product_to_index, source_product) == 0);
    }

    return 0;
}

/** @} */
