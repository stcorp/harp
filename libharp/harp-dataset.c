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

#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif

#ifdef WIN32
#include "windows.h"
#endif

/** \defgroup harp_dataset HARP harp_dataset
 * The HARP harp_dataset module contains everything regarding HARP datasets.
 *
 * A Dataset contains a list of references to HARP products together with optional metadata on each product.
 * The primary reference to a product is the value of the 'source_product' global attribute of a HARP product.
 */

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
            harp_set_error(HARP_ERROR_FILE_NOT_FOUND, "could not find '%s'", directoryname);
        }
        else
        {
            harp_set_error(HARP_ERROR_FILE_OPEN, "could not open '%s' (%s)", directoryname, strerror(errno));
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

static int add_path_file(harp_dataset *dataset, const char *filename, const char *options)
{
    char line[HARP_MAX_PATH_LENGTH];
    FILE *stream;

    stream = fopen(filename, "r");
    if (stream == NULL)
    {
        harp_set_error(HARP_ERROR_FILE_OPEN, "cannot open pth file '%s'", filename);
        return -1;
    }

    while (fgets(line, HARP_MAX_PATH_LENGTH, stream) != NULL)
    {
        long length = strlen(line);

        /* Trim the line */
        while (length > 0 && (line[length - 1] == '\r' || line[length - 1] == '\n'))
        {
            length--;
        }
        line[length] = '\0';

        /* skip empty lines and lines starting with '#' */
        if (length > 0 && line[0] != '#')
        {
            if (harp_dataset_import(dataset, line, options) != 0)
            {
                fclose(stream);
                return -1;
            }
        }
    }

    fclose(stream);

    return 0;
}

static int add_directory(harp_dataset *dataset, const char *pathname, const char *options)
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
            if (harp_dataset_import(dataset, filepath, options) != 0)
            {
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

        if (harp_dataset_import(dataset, filepath, options) != 0)
        {
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

/** \addtogroup harp_dataset
 * @{
 */

/** Create new HARP dataset.
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

    dataset->product_to_index = hashtable_new(0);
    dataset->sorted_index = NULL;
    dataset->num_products = 0;
    dataset->metadata = NULL;
    dataset->source_product = NULL;

    *new_dataset = dataset;

    return 0;
}

/** Delete HARP dataset.
 * \param dataset Pointer to the dataset to free.
 */
LIBHARP_API void harp_dataset_delete(harp_dataset *dataset)
{
    if (dataset == NULL)
    {
        return;
    }

    if (dataset->product_to_index != NULL)
    {
        hashtable_delete(dataset->product_to_index);
    }

    if (dataset->sorted_index != NULL)
    {
        free(dataset->sorted_index);
    }

    if (dataset->source_product != NULL)
    {
        long i;

        for (i = 0; i < dataset->num_products; i++)
        {
            free(dataset->source_product[i]);
        }

        free(dataset->source_product);
    }

    if (dataset->metadata != NULL)
    {
        long i;

        for (i = 0; i < dataset->num_products; i++)
        {
            harp_product_metadata_delete(dataset->metadata[i]);
        }

        free(dataset->metadata);
    }

    free(dataset);
}

/** Print HARP dataset.
 * \param dataset Pointer to the dataset to print.
 * \param print Pointer to the function that should be used for printing.
 */
LIBHARP_API void harp_dataset_print(harp_dataset *dataset, int (*print) (const char *, ...))
{
    long i;

    for (i = 0; i < dataset->num_products; i++)
    {
        if (dataset->metadata[i] != NULL)
        {
            harp_product_metadata_print(dataset->metadata[i], print);
        }
        else
        {
            print("source_product: %s", dataset->source_product[i]);
        }
        print("\n");
    }
}

/** Import metadata for products into the dataset.
 * If path is a directory then all files (recursively) from that directory are added to the dataset.
 * If path references a .pth file then the file paths from that text file (one per line) are imported.
 * These file paths can be absolute or relative and can point to files, directories, or other .pth files.
 * If path references a product file then that file is added to the dataset. Trying to add a file that is not supported
 * by HARP will result in an error.
 *
 * Note that datasets cannot have multiple entries with the same 'source_product' value. Therefore, for each product
 * where the dataset already contained an entry with the same 'source_product' value, the metadata of that entry is
 * replaced with the new metadata (instead of adding a new entry to the dataset or raising an error).
 *
 * \param dataset Dataset into which to import the metadata.
 * \param path Path to either a directory containing product files, a .pth file, or a single product file.
 * \param options Ingestion module specific options (optional); should be specified as a semi-colon separated
 * string of key=value pair; only used for product files that are not already in HARP format.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_dataset_import(harp_dataset *dataset, const char *path, const char *options)
{
    int result;

    result = is_directory(path);
    if (result == -1)
    {
        return -1;
    }
    if (result)
    {
        return add_directory(dataset, path, options);
    }
    else
    {
        harp_product_metadata *metadata = NULL;
        long length = strlen(path);

        if (length > 4 && strcmp(&path[length - 4], ".pth") == 0)
        {
            return add_path_file(dataset, path, options);
        }

        /* Import the metadata */
        if (harp_import_product_metadata(path, options, &metadata) != 0)
        {
            return -1;
        }

        return harp_dataset_add_product(dataset, metadata->source_product, metadata);
    }
}

/** Lookup the index of source_product in the given dataset.
 * \param dataset Dataset to get index in.
 * \param source_product Source product reference.
 * \param index Pointer to the C variable where the index in the dataset for the product is returned.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_dataset_get_index_from_source_product(harp_dataset *dataset, const char *source_product,
                                                           long *index)
{
    long product_index;

    product_index = hashtable_get_index_from_name(dataset->product_to_index, source_product);
    if (product_index < 0)
    {
        harp_set_error(HARP_ERROR_INVALID_NAME, "source product '%s' does not exist", source_product);
        return -1;
    }

    *index = product_index;

    return 0;
}

/** Test if dataset contains an entry with the specified source product reference.
 * \param dataset Dataset in which to find the product.
 * \param source_product Source product reference.
 * \return
 *   \arg \c 0, Dataset does not contain a product with the specific source reference.
 *   \arg \c 1, Dataset contains a product with the specific source reference.
*/
LIBHARP_API int harp_dataset_has_product(harp_dataset *dataset, const char *source_product)
{
    return hashtable_get_index_from_name(dataset->product_to_index, source_product) >= 0;
}

/** Add a product reference to a dataset.
 * \param dataset Dataset in which to add a new entry.
 * \param source_product The source product reference of the new entry.
 * \param metadata The product metadata of the new entry (can be NULL); the dataset is the new owner of metadata.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_dataset_add_product(harp_dataset *dataset, const char *source_product,
                                         harp_product_metadata *metadata)
{
    if (metadata != NULL && strcmp(metadata->source_product, source_product) != 0)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "invalid source product '%s' in metadata, expected '%s'",
                       metadata->source_product, source_product);
        return -1;
    }

    /* if source product does not already appear, add it */
    if (!harp_dataset_has_product(dataset, source_product))
    {
        long index;
        long i;

        /* Make space for new entry */
        if (dataset->num_products % BLOCK_SIZE == 0)
        {
            char **new_source_product;
            long *new_sorted_index;
            harp_product_metadata **new_metadata;

            /* grow the source_product array by one block */
            new_source_product = realloc(dataset->source_product,
                                         (dataset->num_products + BLOCK_SIZE) * sizeof(char **));
            if (new_source_product == NULL)
            {
                harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                               (dataset->num_products + BLOCK_SIZE) * sizeof(char **), __FILE__, __LINE__);
                return -1;
            }
            dataset->source_product = new_source_product;

            new_sorted_index = realloc(dataset->sorted_index, (dataset->num_products + BLOCK_SIZE) * sizeof(long));
            if (new_sorted_index == NULL)
            {
                harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                               (dataset->num_products + BLOCK_SIZE) * sizeof(long), __FILE__, __LINE__);
                return -1;
            }
            dataset->sorted_index = new_sorted_index;

            new_metadata = realloc(dataset->metadata,
                                   (dataset->num_products + BLOCK_SIZE) * sizeof(harp_product_metadata *));
            if (new_metadata == NULL)
            {
                harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                               (dataset->num_products + BLOCK_SIZE) * sizeof(harp_product_metadata *), __FILE__,
                               __LINE__);
                return -1;
            }
            dataset->metadata = new_metadata;

            /* zero-out metadata entries; as these are only optionally set in the future */
            for (i = dataset->num_products; i < (dataset->num_products + BLOCK_SIZE); i++)
            {
                dataset->metadata[i] = NULL;
            }
        }

        /* add newly appended item into the list of sorted indices */
        index = 0;
        while (index < dataset->num_products &&
               strcmp(source_product, dataset->source_product[dataset->sorted_index[index]]) > 0)
        {
            index++;
        }
        for (i = dataset->num_products; i > index; i--)
        {
            dataset->sorted_index[i] = dataset->sorted_index[i - 1];
        }
        dataset->sorted_index[index] = dataset->num_products;

        dataset->num_products++;

        dataset->source_product[dataset->num_products - 1] = strdup(source_product);
        if (dataset->source_product[dataset->num_products - 1] == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (failed to duplicate string) (%s:%u)",
                           __FILE__, __LINE__);
            return -1;
        }

        if (hashtable_add_name(dataset->product_to_index, dataset->source_product[dataset->num_products - 1]) != 0)
        {
            assert(0);
            exit(1);
        }
    }

    if (metadata)
    {
        long index;

        if (harp_dataset_get_index_from_source_product(dataset, source_product, &index))
        {
            return -1;
        }

        /* Delete existing metadata for this product */
        if (dataset->metadata[index] != NULL)
        {
            harp_product_metadata_delete(dataset->metadata[index]);
        }

        /* Set the metadata for this source_product */
        dataset->metadata[index] = metadata;
    }

    return 0;
}

/** @} */
