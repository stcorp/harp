/*
 * Copyright (C) 2015-2024 S[&]T, The Netherlands.
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
#include "harp-program.h"
#include "harp-csv.h"
#include "hashtable.h"

#include "coda.h"

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

static int parse_metadata_from_csv_line(char *line, harp_product_metadata *metadata)
{
    char *string = NULL;

    /* filename */
    if (harp_csv_parse_string(&line, &string) != 0)
    {
        return -1;
    }
    metadata->filename = strdup(string);
    if (!metadata->filename)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        return -1;
    }

    /* datetime_start */
    if (harp_csv_parse_string(&line, &string) != 0)
    {
        return -1;
    }
    if (string[0] == '\0')
    {
        metadata->datetime_start = harp_mininf();
    }
    else
    {
        if (coda_time_string_to_double("yyyyMMdd'T'HHmmss", string, &metadata->datetime_start) != 0)
        {
            harp_set_error(HARP_ERROR_INVALID_FORMAT, "invalid datetime string '%s' in csv element", string);
            return -1;
        }
        metadata->datetime_start /= 86400;
    }

    /* datetime_stop */
    if (harp_csv_parse_string(&line, &string) != 0)
    {
        return -1;
    }
    if (string[0] == '\0')
    {
        metadata->datetime_stop = harp_plusinf();
    }
    else
    {
        if (coda_time_string_to_double("yyyyMMdd'T'HHmmss", string, &metadata->datetime_stop) != 0)
        {
            harp_set_error(HARP_ERROR_INVALID_FORMAT, "invalid datetime string '%s' in csv element", string);
            return -1;
        }
        metadata->datetime_stop /= 86400;
    }

    /* time dimension */
    if (harp_csv_parse_long(&line, &metadata->dimension[harp_dimension_time]) != 0)
    {
        return -1;
    }

    /* latitude dimension */
    if (harp_csv_parse_long(&line, &metadata->dimension[harp_dimension_latitude]) != 0)
    {
        return -1;
    }

    /* longitude dimension */
    if (harp_csv_parse_long(&line, &metadata->dimension[harp_dimension_longitude]) != 0)
    {
        return -1;
    }

    /* vertical dimension */
    if (harp_csv_parse_long(&line, &metadata->dimension[harp_dimension_vertical]) != 0)
    {
        return -1;
    }

    /* spectral dimension */
    if (harp_csv_parse_long(&line, &metadata->dimension[harp_dimension_spectral]) != 0)
    {
        return -1;
    }

    /* source_product */
    if (harp_csv_parse_string(&line, &string) != 0)
    {
        return -1;
    }
    metadata->source_product = strdup(string);
    if (!metadata->source_product)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        return -1;
    }

    return 0;
}

static int add_path_csv_file(harp_dataset *dataset, const char *filename, FILE *stream)
{
    char line[HARP_CSV_LINE_LENGTH + 1];

    /* header line was already read by add_path_file(), so we only need to read the lines with metadata */
    while (fgets(line, HARP_CSV_LINE_LENGTH + 1, stream) != NULL)
    {
        harp_product_metadata *metadata = NULL;
        long length = (long)strlen(line);

        /* Trim the line */
        while (length > 0 && (line[length - 1] == '\r' || line[length - 1] == '\n'))
        {
            length--;
        }
        line[length] = '\0';

        if (length == HARP_CSV_LINE_LENGTH)
        {
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "line exceeds max line length (%ld) in file '%s'",
                           HARP_CSV_LINE_LENGTH, filename);
            return -1;
        }

        /* Do not allow empty lines */
        if (length == 0)
        {
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "empty line in file '%s'", filename);
            return -1;
        }

        if (harp_product_metadata_new(&metadata) != 0)
        {
            return -1;
        }

        if (parse_metadata_from_csv_line(line, metadata) != 0)
        {
            harp_product_metadata_delete(metadata);
            return -1;
        }

        if (harp_dataset_add_product(dataset, metadata->source_product, metadata) != 0)
        {
            harp_product_metadata_delete(metadata);
            return -1;
        }
    }

    return 0;
}

static int add_path_file(harp_dataset *dataset, const char *filename, const char *options)
{
    char line[HARP_MAX_PATH_LENGTH];
    int first_line = 1;
    FILE *stream;

    stream = fopen(filename, "r");
    if (stream == NULL)
    {
        harp_set_error(HARP_ERROR_FILE_OPEN, "cannot open pth file '%s'", filename);
        return -1;
    }

    while (fgets(line, HARP_MAX_PATH_LENGTH, stream) != NULL)
    {
        long length = (long)strlen(line);

        /* Trim the line */
        while (length > 0 && (line[length - 1] == '\r' || line[length - 1] == '\n'))
        {
            length--;
        }
        line[length] = '\0';

        /* Do not allow empty lines */
        if (length == 0)
        {
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "empty line in file '%s'", filename);
            return -1;
        }

        if (first_line)
        {
            if (strcmp(line, "filename,datetime_start,datetime_stop,time,latitude,longitude,vertical,spectral,"
                       "source_product") == 0)
            {
                /* this is a dataset csv file, import accordingly */
                if (add_path_csv_file(dataset, filename, stream) != 0)
                {
                    fclose(stream);
                    return -1;
                }
                fclose(stream);
                return 0;
            }
            first_line = 0;
        }
        if (harp_dataset_import(dataset, line, options) != 0)
        {
            fclose(stream);
            return -1;
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
 * The metadata will be initialized with zero product metadata elements.
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

    dataset->product_to_index = hashtable_new(1);
    if (dataset->product_to_index == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not create hashtable) (%s:%u)", __FILE__,
                       __LINE__);
        free(dataset);
        return -1;
    }
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
LIBHARP_API void harp_dataset_print(harp_dataset *dataset, int (*print)(const char *, ...))
{
    long i;

    print("filename,datetime_start,datetime_stop,time,latitude,longitude,vertical,spectral,source_product\n");
    for (i = 0; i < dataset->num_products; i++)
    {
        if (dataset->metadata[i] != NULL)
        {
            harp_product_metadata_print(dataset->metadata[i], print);
        }
        else
        {
            print(",,,,,,,,%s\n", dataset->source_product[i]);
        }
    }
}

/** Import metadata for products into the dataset.
 * If path is a directory then all files (recursively) from that directory are added to the dataset.
 * If path references a .pth file then the file paths from that text file (one per line) are imported.
 * These file paths can be absolute or relative and can point to files, directories, or other .pth files.
 * If path references a product file then that file is added to the dataset. Trying to add a file that is not supported
 * by HARP will result in an error.
 * Directories and files whose names start with a '.' will be ignored.
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

    if (harp_basename(path)[0] == '.')
    {
        /* ignore directories/files whose name start with a '.' */
        return 0;
    }

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
        long length = (long)strlen(path);

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

        /* Clear metadata history field if it was set (to reduce memory overhead) */
        if (metadata->history != NULL)
        {
            free(metadata->history);
            metadata->history = NULL;
        }

        /* Set the metadata for this source_product */
        dataset->metadata[index] = metadata;
    }

    return 0;
}

/** @} */

static harp_dataset *sort_dataset = NULL;

static int compare_source_product(const void *a, const void *b)
{
    return strcmp(sort_dataset->source_product[*(long *)a], sort_dataset->source_product[*(long *)b]);
}

int harp_dataset_filter(harp_dataset *dataset, uint8_t *mask)
{
    long new_num_products = 0;
    long target_index;
    long i;

    for (i = 0; i < dataset->num_products; i++)
    {
        if (mask[i])
        {
            new_num_products++;
        }
    }

    if (new_num_products == dataset->num_products)
    {
        /* no change necessary */
        return 0;
    }

    target_index = 0;
    for (i = 0; i < dataset->num_products; i++)
    {
        if (mask[i])
        {
            if (target_index != i)
            {
                dataset->source_product[target_index] = dataset->source_product[i];
                dataset->metadata[target_index] = dataset->metadata[i];
            }
            dataset->sorted_index[target_index] = target_index; /* intialize unsorted */
            target_index++;
        }
        else
        {
            free(dataset->source_product[i]);
            harp_product_metadata_delete(dataset->metadata[i]);
        }
    }
    dataset->num_products = new_num_products;

    /* resort sorted_index */
    sort_dataset = dataset;
    qsort(dataset->sorted_index, dataset->num_products, sizeof(long), compare_source_product);

    /* rebuild hashtable */
    hashtable_delete(dataset->product_to_index);
    dataset->product_to_index = hashtable_new(1);
    if (dataset->product_to_index == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not create hashtable) (%s:%u)", __FILE__,
                       __LINE__);
        return -1;
    }
    for (i = 0; i < dataset->num_products; i++)
    {
        if (hashtable_add_name(dataset->product_to_index, dataset->source_product[i]) != 0)
        {
            assert(0);
            exit(1);
        }
    }

    return 0;
}

static int prefilter_comparison(harp_dataset *dataset, uint8_t *mask, harp_operation_comparison_filter *operation)
{
    long i;

    if (strcmp(operation->variable_name, "datetime") != 0 && strcmp(operation->variable_name, "datetime_start") != 0 &&
        strcmp(operation->variable_name, "datetime_stop") != 0)
    {
        /* not a variable we can pre-filter on */
        return 0;
    }

    if (harp_operation_set_value_unit((harp_operation *)operation, "days since 2000-01-01") != 0)
    {
        return -1;
    }
    for (i = 0; i < dataset->num_products; i++)
    {
        if (mask[i] && dataset->metadata[i] != NULL)
        {
            double datetime_start = harp_unit_converter_convert_double(operation->unit_converter,
                                                                       dataset->metadata[i]->datetime_start);
            double datetime_stop = harp_unit_converter_convert_double(operation->unit_converter,
                                                                      dataset->metadata[i]->datetime_stop);

            switch (operation->operator_type)
            {
                case operator_eq:
                    if (datetime_stop < operation->value || datetime_start > operation->value)
                    {
                        mask[i] = 0;
                    }
                    break;
                case operator_ne:
                    if (datetime_start == operation->value && datetime_stop == operation->value)
                    {
                        mask[i] = 0;
                    }
                    break;
                case operator_lt:
                    if (datetime_start >= operation->value)
                    {
                        mask[i] = 0;
                    }
                    break;
                case operator_le:
                    if (datetime_start > operation->value)
                    {
                        mask[i] = 0;
                    }
                    break;
                case operator_gt:
                    if (datetime_stop <= operation->value)
                    {
                        mask[i] = 0;
                    }
                    break;
                case operator_ge:
                    if (datetime_stop < operation->value)
                    {
                        mask[i] = 0;
                    }
                    break;
            }
        }
    }

    return 0;
}

static int match_collocation_line(char *line, harp_operation_collocation_filter *operation, harp_dataset *dataset,
                                  uint8_t *available)
{
    char *source_product;
    char *cursor = line;
    long length;
    long index;

    length = (long)strlen(line);

    /* Trim the line */
    while (length > 0 && (line[length - 1] == '\r' || line[length - 1] == '\n'))
    {
        length--;
    }
    line[length] = '\0';

    if (length == HARP_CSV_LINE_LENGTH)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "line exceeds max line length (%ld)", HARP_CSV_LINE_LENGTH);
        return -1;
    }

    if (length == 0)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "empty line");
        return -1;
    }

    if (harp_csv_parse_long(&cursor, &index) != 0)
    {
        return -1;
    }

    /* skip line if collocation_index is outside the requested range */
    if (operation->min_collocation_index >= 0 && index < operation->min_collocation_index)
    {
        return 0;
    }
    if (operation->max_collocation_index >= 0 && index > operation->max_collocation_index)
    {
        return 0;
    }

    if (harp_csv_parse_string(&cursor, &source_product) != 0)
    {
        return -1;
    }
    if (operation->filter_type == harp_collocation_left)
    {
        /* match source_product_a */
        index = hashtable_get_index_from_name(dataset->product_to_index, source_product);
        if (index >= 0)
        {
            available[index] = 1;
        }
        return 0;
    }
    if (harp_csv_parse_long(&cursor, &index) != 0)
    {
        return -1;
    }

    if (harp_csv_parse_string(&cursor, &source_product) != 0)
    {
        return -1;
    }
    /* match source_product_b */
    index = hashtable_get_index_from_name(dataset->product_to_index, source_product);
    if (index >= 0)
    {
        available[index] = 1;
    }

    return 0;
}

static int prefilter_collocation(harp_dataset *dataset, uint8_t *mask, harp_operation_collocation_filter *operation)
{
    char line[HARP_CSV_LINE_LENGTH + 1];
    uint8_t *available;
    FILE *file;
    long i;

    available = malloc(dataset->num_products);
    if (available == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %ld bytes) (%s:%u)",
                       dataset->num_products, __FILE__, __LINE__);
        return -1;
    }
    for (i = 0; i < dataset->num_products; i++)
    {
        available[i] = 0;
    }

    /* Open the collocation result file */
    file = fopen(operation->filename, "r");
    if (file == NULL)
    {
        harp_set_error(HARP_ERROR_FILE_OPEN, "error opening collocation result file '%s'", operation->filename);
        free(available);
        return -1;
    }

    /* read+skip header */
    if (fgets(line, HARP_CSV_LINE_LENGTH + 1, file) == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "error reading header");
        fclose(file);
        free(available);
        return -1;
    }

    /* Read the pairs */
    while (1)
    {
        if (fgets(line, HARP_CSV_LINE_LENGTH + 1, file) == NULL)
        {
            if (ferror(file))
            {
                harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "error reading line");
                fclose(file);
                free(available);
                return -1;
            }
            /* EOF */
            break;
        }
        if (match_collocation_line(line, operation, dataset, available) != 0)
        {
            fclose(file);
            free(available);
            return -1;
        }
    }

    fclose(file);

    /* mask out all products that are not in the collocation result file */
    for (i = 0; i < dataset->num_products; i++)
    {
        if (!available[i])
        {
            mask[i] = 0;
        }
    }
    free(available);

    return 0;
}

/** \addtogroup harp_dataset
 * @{
 */

/** Filter products in dataset based on operations.
 * Remove any entries from the dataset that can already be discarded based on filters at the start of the operations
 * string. This includes comparisons against datetime/datetime_start/datetime_stop and collocate_left/collocate_right
 * operations.
 * The filters will be matched against the metadata in the dataset. The datatime_start and datetime_stop attributes
 * will be used for the datetime filters and the source_product attribute for the collocation filters.
 * \param dataset Dataset that should be filtered.
 * \param operations Operations to execute; should be specified as a semi-colon separated string of operations.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_dataset_prefilter(harp_dataset *dataset, const char *operations)
{
    harp_program *program;
    uint8_t *mask;
    long i;

    if (operations == NULL || dataset->num_products == 0)
    {
        return 0;
    }

    if (harp_program_from_string(operations, &program) != 0)
    {
        return -1;
    }

    mask = malloc(dataset->num_products);
    if (mask == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %ld bytes) (%s:%u)",
                       dataset->num_products, __FILE__, __LINE__);
        return -1;
    }
    for (i = 0; i < dataset->num_products; i++)
    {
        mask[i] = 1;
    }

    for (i = 0; i < program->num_operations; i++)
    {
        harp_operation *operation = program->operation[i];

        switch (operation->type)
        {
            case operation_comparison_filter:
                if (prefilter_comparison(dataset, mask, (harp_operation_comparison_filter *)operation) != 0)
                {
                    harp_program_delete(program);
                    free(mask);
                    return -1;
                }
                /* we can skip over other variable filters, since they won't impact our pre-filtering */
                break;
            case operation_collocation_filter:
                if (prefilter_collocation(dataset, mask, (harp_operation_collocation_filter *)operation) != 0)
                {
                    harp_program_delete(program);
                    free(mask);
                    return -1;
                }
                break;
            default:
                /* unsupported -> terminate loop */
                i = program->num_operations;
                break;
        }
    }

    harp_program_delete(program);

    if (harp_dataset_filter(dataset, mask) != 0)
    {
        free(mask);
        return -1;
    }

    free(mask);

    return 0;
}

/** @} */
