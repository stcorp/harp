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
#include "harp-ingestion.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "coda.h"

#define DETECTION_BLOCK_SIZE 12

LIBHARP_API const char *libharp_version = HARP_VERSION;

static int harp_init_counter = 0;

int harp_option_enable_aux_afgl86 = 0;
int harp_option_enable_aux_usstd76 = 0;

typedef enum file_format_enum
{
    format_unknown = -1,
    format_hdf4,
    format_hdf5,
    format_netcdf
} file_format;

static file_format format_from_string(const char *format)
{
    if (strcasecmp(format, "HDF4") == 0)
    {
        return format_hdf4;
    }
    else if (strcasecmp(format, "HDF5") == 0)
    {
        return format_hdf5;
    }
    else if (strcasecmp(format, "netCDF") == 0)
    {
        return format_netcdf;
    }
    return format_unknown;
}

static int determine_file_format(const char *filename, file_format *format)
{
    unsigned char buffer[DETECTION_BLOCK_SIZE];
    struct stat statbuf;
    int open_flags;
    int fd;

    /* Call stat() on the file to be opened. */
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

    /* Check that the file is a regular file. */
    if ((statbuf.st_mode & S_IFREG) == 0)
    {
        harp_set_error(HARP_ERROR_FILE_OPEN, "could not open %s (not a regular file)", filename);
        return -1;
    }

    /* Open the file and read the detection block. */
    open_flags = O_RDONLY;
#ifdef WIN32
    open_flags |= _O_BINARY;
#endif
    fd = open(filename, open_flags);
    if (fd < 0)
    {
        harp_set_error(HARP_ERROR_FILE_OPEN, "could not open %s (%s)", filename, strerror(errno));
        return -1;
    }

    if (statbuf.st_size > 0)
    {
        if (read(fd, buffer, statbuf.st_size < DETECTION_BLOCK_SIZE ? (size_t)statbuf.st_size : DETECTION_BLOCK_SIZE)
            == -1)
        {
            harp_set_error(HARP_ERROR_FILE_READ, "could not read %s (%s)", filename, strerror(errno));
            close(fd);
            return -1;
        }
    }

    close(fd);

    /* HDF4 */
    if (statbuf.st_size >= 4 && memcmp(buffer, "\016\003\023\001", 4) == 0)
    {
        *format = format_hdf4;
        return 0;
    }

    /* HDF5 */
    if (statbuf.st_size >= 8 && memcmp(buffer, "\211HDF\r\n\032\n", 8) == 0)
    {
        *format = format_hdf5;
        return 0;
    }

    /* netCDF */
    if (statbuf.st_size >= 4 && memcmp(buffer, "CDF", 3) == 0 && (buffer[3] == '\001' || buffer[3] == '\002'))
    {
        *format = format_netcdf;
        return 0;
    }

    *format = format_unknown;
    return 0;
}

static int auxiliary_data_init(void)
{
    if (getenv("HARP_AUX_AFGL86") != NULL)
    {
        harp_option_enable_aux_afgl86 = 1;
    }
    if (getenv("HARP_AUX_USSTD76") != NULL)
    {
        harp_option_enable_aux_usstd76 = 1;
    }
    return 0;
}

/** \defgroup harp_general HARP General
 * The HARP General module contains all general and miscellaneous functions and procedures of HARP.
 */

/** \defgroup harp_documentation HARP Generated documentation
 * The HARP Generated documentation module contains public functions to output documentation that can be generated
 * automatically by HARP. This includes the ingestion definitions and derived variable conversions.
 */

/** \addtogroup harp_general
 * @{
 */

/** Set the search path for CODA product definition files.
 * This function should be called before harp_init() is called.
 *
 * The CODA C library is used by the HARP C library for import of products that do not use the HARP format. To access
 * data in a product, the CODA C library requires a definition of the internal structure of the product file (unless
 * the product is stored in a self-describing file format). This information is stored in CODA product definition
 * (.codadef) files.
 *
 * The path should be a searchpath for CODA .codadef files similar like the PATH environment variable of your system.
 * Path components should be separated by ';' on Windows and by ':' on other systems.
 *
 * The path may contain both references to files and directories.
 * CODA will load all .codadef files in the path. Any specified files should be valid .codadef files. For directories,
 * CODA will (non-recursively) search the directory for all .codadef files.
 *
 * If multiple files for the same product class exist in the path, CODA will only use the one with the highest revision
 * number (this is normally equal to a last modification date that is stored in a .codadef file).
 * If there are two files for the same product class with identical revision numbers, CODA will use the definitions of
 * the first .codadef file in the path and ingore the second one.
 *
 * Specifying a path using this function will prevent CODA from using the CODA_DEFINITION environment variable.
 * If you still want CODA to acknowledge the CODA_DEFINITION environment variable then use something like this in your
 * code:
 * \code{.c}
 * if (getenv("CODA_DEFINITION") == NULL)
 * {
 *     harp_set_coda_definition_path("<your path>");
 * }
 * \endcode
 *
 *  \param path Search path for .codadef files
 *  \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_set_coda_definition_path(const char *path)
{
    if (coda_set_definition_path(path) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    return 0;
}

/** Set the directory for CODA product definition files based on the location of another file.
 * This function should be called before harp_init() is called.
 *
 * The CODA C library is used by the HARP C library for import of products that do not use the HARP format. To access
 * data in a product, the CODA C library requires a definition of the internal structure of the product file (unless
 * the product is stored in a self-describing file format). This information is stored in CODA product definition
 * (.codadef) files.
 *
 * This function will try to find the file with filename \a file in the provided searchpath \a searchpath.
 * The first directory in the searchpath where the file \a file exists will be appended with the relative directory
 * \a relative_location to determine the CODA product definition path. This path will be used as CODA definition path.
 * If the file could not be found in the searchpath then the CODA definition path will not be set.
 *
 * If the CODA_DEFINITION environment variable was set then this function will not perform a search or set the
 * definition path (i.e. the CODA definition path will be taken from the CODA_DEFINITION variable).
 *
 * If you provide NULL for \a searchpath then the PATH environment variable will be used as searchpath.
 * For instance, you can use harp_set_coda_definition_path_conditional(argv[0], NULL, "../somedir") to set the CODA
 * definition path to a location relative to the location of your executable.
 *
 * The searchpath, if provided, should have a similar format as the PATH environment variable of your system. Path
 * components should be separated by ';' on Windows and by ':' on other systems.
 *
 * The \a relative_location parameter can point either to a directory (in which case all .codadef files in this
 * directory will be used) or to a single .codadef file.
 *
 * Note that this function differs from harp_set_coda_definition_path() in two important ways:
 *  - it will not modify the definition path if the CODA_DEFINITION variable was set
 *  - it will set the definition path to just a single location (either a single file or a single directory)
 *
 * \param file Filename of the file to search for
 * \param searchpath Search path where to look for the file \a file (can be NULL)
 * \param relative_location Filepath relative to the directory from \a searchpath where \a file was found that should be
 * used to determine the CODA definition path.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_set_coda_definition_path_conditional(const char *file, const char *searchpath,
                                                          const char *relative_location)
{
    if (coda_set_definition_path_conditional(file, searchpath, relative_location) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    return 0;
}

/** Enable/Disable the use of AFGL86 climatology in variable conversions
 * Enabling this option will allow the derived variable functions to create variables using the built-in AFGL86
 * profiles. If datetime, latitude, and altitude variables are available then altitude regridded versions of the
 * following climatological quantities can be created:
 * - pressure
 * - temperature
 * - number_density (of air)
 * - CH4_number_density
 * - CO_number_density
 * - CO2_number_density
 * - H2O_number_density
 * - N2O_number_density
 * - NO2_number_density
 * - O2_number_density
 * - O3_number_density
 * By default the use of AFGL86 is disabled.
 * The use of AFGL86 can also be enabled by setting the HARP_AUX_AFGL86 environment variable.
 * \param enable
 *   \arg 0: Disable use of AFGL86.
 *   \arg 1: Enable use of AFGL86.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_set_option_enable_aux_afgl86(int enable)
{
    if (enable != 0 && enable != 1)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "enable argument (%d) is not valid (%s:%u)", enable, __FILE__,
                       __LINE__);
        return -1;
    }

    harp_option_enable_aux_afgl86 = enable;

    return 0;
}

/** Retrieve the current setting for the usage of AFGL86 option.
 * \see harp_set_option_enable_aux_afgl86()
 * \return
 *   \arg \c 0, Use of AFGL86 is disabled.
 *   \arg \c 1, Use of AFGL86 is enabled.
 */
LIBHARP_API int harp_get_option_enable_aux_afgl86(void)
{
    return harp_option_enable_aux_afgl86;
}

/** Enable/Disable the use of US Standard 76 climatology in variable conversions
 * Enabling this option will allow the derived variable functions to create variables using the built-in US Standard 76
 * profiles. If an altitude variable is available then altitude regridded versions of the following climatological
 * quantities can be created:
 * - pressure
 * - temperature
 * - number_density (of air)
 * - CH4_number_density
 * - CO_number_density
 * - CO2_number_density
 * - H2O_number_density
 * - N2O_number_density
 * - NO2_number_density
 * - O2_number_density
 * - O3_number_density
 * By default the use of US Standard 76 is disabled.
 * The use of US Standard 76 can also be enabled by setting the HARP_AUX_USSTD76 environment variable.
 * \param enable
 *   \arg 0: Disable use of US Standard 76.
 *   \arg 1: Enable use of US Standard 76.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_set_option_enable_aux_usstd76(int enable)
{
    if (enable != 0 && enable != 1)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "enable argument (%d) is not valid (%s:%u)", enable, __FILE__,
                       __LINE__);
        return -1;
    }

    harp_option_enable_aux_usstd76 = enable;

    return 0;
}

/** Retrieve the current setting for the usage of US Standard 76 option.
 * \see harp_set_option_enable_aux_usstd76()
 * \return
 *   \arg \c 0, Use of US Standard 76 is disabled.
 *   \arg \c 1, Use of US Standard 76 is enabled.
 */
LIBHARP_API int harp_get_option_enable_aux_usstd76(void)
{
    return harp_option_enable_aux_usstd76;
}

/** Initializes the HARP C library.
 * This function should be called before any other HARP C library function is called (except for
 * harp_set_coda_definition_path(), harp_set_coda_definition_path_conditional(), and harp_set_warning_handler()).
 *
 * HARP may at some point after calling harp_init() also initialize the underlying CODA C library that is used for
 * ingestion of products that are not using the HARP format.
 * The CODA C library may require access to .codadef files that describe the formats of certain product files.
 * In order to tell CODA where these .codadef files are located you will have to either set the CODA_DEFINITION
 * environment variable to the proper path, or set the path programmatically using the
 * harp_set_coda_definition_path() or harp_set_coda_definition_path_conditional() function, before calling harp_init()
 * for the first time.
 *
 * If you use CODA functions directly in combination with HARP functions you should call coda_init() and coda_done()
 * explictly yourself and not rely on HARP having performed the coda_init() for you.
 *
 * It is valid to perform multiple calls to harp_init() after each other. Only the first call to harp_init() will do
 * the actual initialization and all following calls to harp_init() will only increase an initialization counter. Each
 * call to harp_init() needs to be matched by a call to harp_done() at clean-up time (i.e. the number of calls to
 * harp_done() needs to be equal to the number of calls to harp_init()). Only the final harp_done() call (when the
 * initialization counter has reached 0) will perform the actual clean-up of the HARP C library.
 *
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_init(void)
{
    if (harp_init_counter == 0)
    {
        if (auxiliary_data_init() != 0)
        {
            return -1;
        }
    }

    harp_init_counter++;

    return 0;
}

/** Finalizes the HARP C library.
 * This function should be called to let the HARP C library free up any resources it has claimed since initialization.
 *
 * It is valid to perform multiple calls to harp_init() after each other. Only the first call to harp_init() will do
 * the actual initialization and all following calls to harp_init() will only increase an initialization counter. Each
 * call to harp_init() needs to be matched by a call to harp_done() at clean-up time (i.e. the number of calls to
 * harp_done() needs to be equal to the number of calls to harp_init()). Only the final harp_done() call (when the
 * initialization counter has reached 0) will perform the actual clean-up of the HARP C library.
 *
 * Calling a HARP function other than harp_init() after the final harp_done() call will result in undefined behavior.
 */
LIBHARP_API void harp_done(void)
{
    if (harp_init_counter > 0)
    {
        harp_init_counter--;
        if (harp_init_counter == 0)
        {
            harp_unit_done();
            harp_derived_variable_list_done();
            harp_ingestion_done();
        }
    }
}

/** @} */

/** Import HARP product from file.
 * \ingroup harp_product
 * Try to import an HDF4, HDF5, or netCDF file that complies to the HARP Data Format.
 * You should pass a variable to \a product that was initialized with harp_product_new().
 * \param filename Path to the file that is to be imported.
 * \param product Empty product that is to be filled with information from the imported file.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_import(const char *filename, harp_product **product)
{
    harp_product *imported_product;
    file_format format;

    if (determine_file_format(filename, &format) != 0)
    {
        return -1;
    }

    switch (format)
    {
        case format_hdf4:
            if (harp_import_hdf4(filename, &imported_product) != 0)
            {
                return -1;
            }
            break;
        case format_hdf5:
            if (harp_import_hdf5(filename, &imported_product) != 0)
            {
                return -1;
            }
            break;
        case format_netcdf:
            if (harp_import_netcdf(filename, &imported_product) != 0)
            {
                return -1;
            }
            break;
        default:
            harp_set_error(HARP_ERROR_FILE_OPEN, "unsupported file format for %s", filename);
            return -1;
    }

    if (harp_product_verify(imported_product) != 0)
    {
        harp_product_delete(imported_product);
        return -1;
    }

    *product = imported_product;

    return 0;
}

/** Retrieve global attributes from a HARP product file.
 * \ingroup harp_product
 * This function retrieves the product metadata * without performing a full import.
 * This function is only supported for netCDF files.
 * \param  filename Path to the file for which to retrieve global attributes.
 * \param  new_metadata Pointer to the variable where the metadata should be stored.
 * \return
 *   \arg \c 0, Succes.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_import_product_metadata(const char *filename, harp_product_metadata **new_metadata)
{
    file_format format;
    harp_product_metadata *metadata = NULL;

    if (determine_file_format(filename, &format) != 0)
    {
        return -1;
    }

    if (harp_product_metadata_new(&metadata) != 0)
    {
        return -1;
    }

    /* set the filename attr */
    metadata->filename = strdup(filename);
    if (!metadata->filename)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        harp_product_metadata_delete(metadata);
        return -1;
    }

    switch (format)
    {
        case format_hdf4:
            harp_product_metadata_delete(metadata);
            harp_set_error(HARP_ERROR_FILE_OPEN, "extraction of global attributes not yet supported for HDF4");
            return -1;
        case format_hdf5:
            harp_product_metadata_delete(metadata);
            harp_set_error(HARP_ERROR_FILE_OPEN, "extraction of global attributes not yet supported for HDF5");
            return -1;
        case format_netcdf:
            if (harp_import_global_attributes_netcdf(filename,
                                                     &metadata->datetime_start, &metadata->datetime_stop,
                                                     metadata->dimension, &metadata->source_product) != 0)
            {
                harp_product_metadata_delete(metadata);
                return -1;
            }
            break;
        default:
            harp_product_metadata_delete(metadata);
            harp_set_error(HARP_ERROR_FILE_OPEN, "unsupported file format for %s", filename);
            return -1;
    }

    *new_metadata = metadata;

    return 0;
}

/** Export HARP product to a file.
 * \ingroup harp_product
 * Export product to an HDF4, HDF5, or netCDF file that complies to the HARP Data Format.
 * \param filename Path to the file to which the product is to be exported.
 * \param export_format Either "HDF4", "HDF5", or "netCDF".
 * \param product Product that should be exported to file.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_export(const char *filename, const char *export_format, const harp_product *product)
{
    file_format format;

    format = format_from_string(export_format);
    if (format == format_unknown)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "unsupported export format '%s'", export_format);
        return -1;
    }

    switch (format)
    {
        case format_hdf4:
            return harp_export_hdf4(filename, product);
        case format_hdf5:
            return harp_export_hdf5(filename, product);
        case format_netcdf:
            return harp_export_netcdf(filename, product);
        default:
            assert(0);
            exit(1);
    }

    return 0;
}

/**
 * Return a string describing the dimension type.
 */
LIBHARP_API const char *harp_get_dimension_type_name(harp_dimension_type dimension_type)
{
    switch (dimension_type)
    {
        case harp_dimension_independent:
            return "independent";
        case harp_dimension_time:
            return "time";
        case harp_dimension_latitude:
            return "latitude";
        case harp_dimension_longitude:
            return "longitude";
        case harp_dimension_spectral:
            return "spectral";
        case harp_dimension_vertical:
            return "vertical";
        default:
            assert(0);
            exit(1);
    }
}

/**
 * Try to parse the specified string as a valid dimension type name and store the corresponding enumeration value in \a
 * dimension_type if successful.
 *
 * \param[in]   str            Dimension type name as string.
 * \param[out]  dimension_type Dimension type enumeration value.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_parse_dimension_type(const char *str, harp_dimension_type *dimension_type)
{
    if (strcmp(str, harp_get_dimension_type_name(harp_dimension_independent)) == 0)
    {
        *dimension_type = harp_dimension_independent;
    }
    else if (strcmp(str, harp_get_dimension_type_name(harp_dimension_time)) == 0)
    {
        *dimension_type = harp_dimension_time;
    }
    else if (strcmp(str, harp_get_dimension_type_name(harp_dimension_latitude)) == 0)
    {
        *dimension_type = harp_dimension_latitude;
    }
    else if (strcmp(str, harp_get_dimension_type_name(harp_dimension_longitude)) == 0)
    {
        *dimension_type = harp_dimension_longitude;
    }
    else if (strcmp(str, harp_get_dimension_type_name(harp_dimension_spectral)) == 0)
    {
        *dimension_type = harp_dimension_spectral;
    }
    else if (strcmp(str, harp_get_dimension_type_name(harp_dimension_vertical)) == 0)
    {
        *dimension_type = harp_dimension_vertical;
    }
    else
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "unknown dimension type '%s' (%s:%d)", str, __FILE__, __LINE__);
        return -1;
    }

    return 0;
}

/**
 * Try to parse the specified string as a valid HARP file convention name and retrieve the major and minor HARP
 * format version numbers.
 *
 * \param[in]   str File convention name as string.
 * \param[out]  major Major version number of the HARP format
 * \param[out]  minor Minor version number of the HARP format
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
int harp_parse_file_convention(const char *str, int *major, int *minor)
{
    int result;

    result = sscanf(str, "HARP-%d.%d", major, minor);
    if (result != 2)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "unknown file convention '%s' (%s:%d)", str, __FILE__, __LINE__);
        return -1;
    }

    return 0;
}
