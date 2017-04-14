/*
 * Copyright (C) 2015-2017 S[&]T, The Netherlands.
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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "netcdf.h"

typedef enum netcdf_dimension_type_enum
{
    netcdf_dimension_time,
    netcdf_dimension_latitude,
    netcdf_dimension_longitude,
    netcdf_dimension_vertical,
    netcdf_dimension_spectral,
    netcdf_dimension_independent,
    netcdf_dimension_string
} netcdf_dimension_type;

typedef struct netcdf_dimensions_struct
{
    int num_dimensions;
    netcdf_dimension_type *type;
    long *length;
} netcdf_dimensions;

static const char *get_dimension_type_name(netcdf_dimension_type dimension_type)
{
    switch (dimension_type)
    {
        case netcdf_dimension_time:
            return "time";
        case netcdf_dimension_latitude:
            return "latitude";
        case netcdf_dimension_longitude:
            return "longitude";
        case netcdf_dimension_spectral:
            return "spectral";
        case netcdf_dimension_vertical:
            return "vertical";
        case netcdf_dimension_independent:
            return "independent";
        case netcdf_dimension_string:
            return "string";
        default:
            assert(0);
            exit(1);
    }
}

static int parse_dimension_type(const char *str, netcdf_dimension_type *dimension_type)
{
    int num_consumed;
    long length;

    if (strcmp(str, get_dimension_type_name(netcdf_dimension_time)) == 0)
    {
        *dimension_type = netcdf_dimension_time;
    }
    else if (strcmp(str, get_dimension_type_name(netcdf_dimension_latitude)) == 0)
    {
        *dimension_type = netcdf_dimension_latitude;
    }
    else if (strcmp(str, get_dimension_type_name(netcdf_dimension_longitude)) == 0)
    {
        *dimension_type = netcdf_dimension_longitude;
    }
    else if (strcmp(str, get_dimension_type_name(netcdf_dimension_spectral)) == 0)
    {
        *dimension_type = netcdf_dimension_spectral;
    }
    else if (strcmp(str, get_dimension_type_name(netcdf_dimension_vertical)) == 0)
    {
        *dimension_type = netcdf_dimension_vertical;
    }
    else if (sscanf(str, "independent_%ld%n", &length, &num_consumed) == 1 && (size_t)num_consumed == strlen(str))
    {
        *dimension_type = netcdf_dimension_independent;
    }
    else if (sscanf(str, "string_%ld%n", &length, &num_consumed) == 1 && (size_t)num_consumed == strlen(str))
    {
        *dimension_type = netcdf_dimension_string;
    }
    else
    {
        harp_set_error(HARP_ERROR_IMPORT, "unsupported dimension '%s'", str);
        return -1;
    }

    return 0;
}

static int get_harp_dimension_type(netcdf_dimension_type netcdf_dim_type, harp_dimension_type *harp_dim_type)
{
    switch (netcdf_dim_type)
    {
        case netcdf_dimension_time:
            *harp_dim_type = harp_dimension_time;
            break;
        case netcdf_dimension_latitude:
            *harp_dim_type = harp_dimension_latitude;
            break;
        case netcdf_dimension_longitude:
            *harp_dim_type = harp_dimension_longitude;
            break;
        case netcdf_dimension_spectral:
            *harp_dim_type = harp_dimension_spectral;
            break;
        case netcdf_dimension_vertical:
            *harp_dim_type = harp_dimension_vertical;
            break;
        case netcdf_dimension_independent:
            *harp_dim_type = harp_dimension_independent;
            break;
        default:
            harp_set_error(HARP_ERROR_IMPORT, "unsupported dimension type '%s'",
                           get_dimension_type_name(netcdf_dim_type));
            return -1;
    }

    return 0;
}

static netcdf_dimension_type get_netcdf_dimension_type(harp_dimension_type dimension_type)
{
    switch (dimension_type)
    {
        case harp_dimension_independent:
            return netcdf_dimension_independent;
        case harp_dimension_time:
            return netcdf_dimension_time;
        case harp_dimension_latitude:
            return netcdf_dimension_latitude;
        case harp_dimension_longitude:
            return netcdf_dimension_longitude;
        case harp_dimension_spectral:
            return netcdf_dimension_spectral;
        case harp_dimension_vertical:
            return netcdf_dimension_vertical;
        default:
            assert(0);
            exit(1);
    }
}

static int get_harp_type(int netcdf_data_type, harp_data_type *data_type)
{
    switch (netcdf_data_type)
    {
        case NC_BYTE:
            *data_type = harp_type_int8;
            break;
        case NC_SHORT:
            *data_type = harp_type_int16;
            break;
        case NC_INT:
            *data_type = harp_type_int32;
            break;
        case NC_FLOAT:
            *data_type = harp_type_float;
            break;
        case NC_DOUBLE:
            *data_type = harp_type_double;
            break;
        case NC_CHAR:
            *data_type = harp_type_string;
            break;
        default:
            harp_set_error(HARP_ERROR_IMPORT, "unsupported data type");
            return -1;
    }

    return 0;
}

static int get_netcdf_type(harp_data_type data_type)
{
    switch (data_type)
    {
        case harp_type_int8:
            return NC_BYTE;
        case harp_type_int16:
            return NC_SHORT;
        case harp_type_int32:
            return NC_INT;
        case harp_type_float:
            return NC_FLOAT;
        case harp_type_double:
            return NC_DOUBLE;
        case harp_type_string:
            return NC_CHAR;
        default:
            assert(0);
            exit(1);
    }
}

static void dimensions_init(netcdf_dimensions *dimensions)
{
    dimensions->num_dimensions = 0;
    dimensions->type = NULL;
    dimensions->length = NULL;
}

static void dimensions_done(netcdf_dimensions *dimensions)
{
    if (dimensions->length != NULL)
    {
        free(dimensions->length);
    }
    if (dimensions->type != NULL)
    {
        free(dimensions->type);
    }
}

/* Returns the id of dimension matching the specified type (or the specified length for independent and string
 * dimensions). Returns -1 if no matching dimension can be found.
 */
static int dimensions_find(netcdf_dimensions *dimensions, netcdf_dimension_type type, long length)
{
    int i;

    if (type == netcdf_dimension_independent || type == netcdf_dimension_string)
    {
        /* find independent and string dimensions by length */
        for (i = 0; i < dimensions->num_dimensions; i++)
        {
            if (dimensions->type[i] == type && dimensions->length[i] == length)
            {
                return i;
            }
        }
    }
    else
    {
        /* find by type */
        for (i = 0; i < dimensions->num_dimensions; i++)
        {
            if (dimensions->type[i] == type)
            {
                return i;
            }
        }
    }

    return -1;
}

/* Returns the id of the new dimension on success, -1 otherwise. */
static int dimensions_add(netcdf_dimensions *dimensions, netcdf_dimension_type type, long length)
{
    int index;

    index = dimensions_find(dimensions, type, length);
    if (index >= 0)
    {
        if (dimensions->length[index] != length)
        {
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "duplicate dimensions with name '%s' and different sizes "
                           "'%ld' '%ld'", get_dimension_type_name(type), dimensions->length[index], length);
            return -1;
        }

        return index;
    }

    /* dimension does not yet exist -> add it */
    if (dimensions->num_dimensions % BLOCK_SIZE == 0)
    {
        long *new_length;
        netcdf_dimension_type *new_type;

        new_length = realloc(dimensions->length, (dimensions->num_dimensions + BLOCK_SIZE) * sizeof(long));
        if (new_length == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           (dimensions->num_dimensions + BLOCK_SIZE) * sizeof(long), __FILE__, __LINE__);
            return -1;
        }
        dimensions->length = new_length;
        new_type = realloc(dimensions->type, (dimensions->num_dimensions + BLOCK_SIZE) * sizeof(netcdf_dimension_type));
        if (new_type == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           (dimensions->num_dimensions + BLOCK_SIZE) * sizeof(netcdf_dimension_type), __FILE__,
                           __LINE__);
            return -1;
        }
        dimensions->type = new_type;
    }
    dimensions->type[dimensions->num_dimensions] = type;
    dimensions->length[dimensions->num_dimensions] = length;
    dimensions->num_dimensions++;

    return dimensions->num_dimensions - 1;
}

static int read_string_attribute(int ncid, int varid, const char *name, char **data)
{
    char *str;
    nc_type data_type;
    size_t netcdf_num_elements;
    int result;

    result = nc_inq_att(ncid, varid, name, &data_type, &netcdf_num_elements);
    if (result != NC_NOERR)
    {
        harp_set_error(HARP_ERROR_NETCDF, "%s", nc_strerror(result));
        return -1;
    }

    if (data_type != NC_CHAR)
    {
        harp_set_error(HARP_ERROR_IMPORT, "attribute '%s' has invalid type", name);
        return -1;
    }

    str = malloc((netcdf_num_elements + 1) * sizeof(char));
    if (str == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       (netcdf_num_elements + 1) * sizeof(char), __FILE__, __LINE__);
        return -1;
    }

    result = nc_get_att_text(ncid, varid, name, str);
    if (result != NC_NOERR)
    {
        harp_set_error(HARP_ERROR_NETCDF, "%s", nc_strerror(result));
        free(str);
        return -1;
    }
    str[netcdf_num_elements] = '\0';

    *data = str;
    return 0;
}

static int read_numeric_attribute(int ncid, int varid, const char *name, harp_data_type *data_type, harp_scalar *data)
{
    nc_type netcdf_data_type;
    size_t netcdf_num_elements;
    int result;

    result = nc_inq_att(ncid, varid, name, &netcdf_data_type, &netcdf_num_elements);
    if (result != NC_NOERR)
    {
        harp_set_error(HARP_ERROR_NETCDF, "%s", nc_strerror(result));
        return -1;
    }

    if (netcdf_num_elements != 1)
    {
        harp_set_error(HARP_ERROR_IMPORT, "attribute '%s' has invalid format", name);
        return -1;
    }

    if (get_harp_type(netcdf_data_type, data_type) != 0)
    {
        harp_add_error_message(" (attribute '%s')", name);
        return -1;
    }

    switch (netcdf_data_type)
    {
        case NC_BYTE:
            result = nc_get_att_schar(ncid, varid, name, &data->int8_data);
            break;
        case NC_SHORT:
            result = nc_get_att_short(ncid, varid, name, &data->int16_data);
            break;
        case NC_INT:
            result = nc_get_att_int(ncid, varid, name, &data->int32_data);
            break;
        case NC_FLOAT:
            result = nc_get_att_float(ncid, varid, name, &data->float_data);
            break;
        case NC_DOUBLE:
            result = nc_get_att_double(ncid, varid, name, &data->double_data);
            break;
        default:
            harp_set_error(HARP_ERROR_IMPORT, "attribute '%s' has invalid type", name);
            return -1;
    }

    if (result != NC_NOERR)
    {
        harp_set_error(HARP_ERROR_NETCDF, "%s", nc_strerror(result));
        return -1;
    }

    return 0;
}

static int read_variable(harp_product *product, int ncid, int varid, netcdf_dimensions *dimensions)
{
    harp_variable *variable;
    harp_data_type data_type;
    int num_dimensions;
    harp_dimension_type dimension_type[HARP_MAX_NUM_DIMS];
    long dimension[HARP_MAX_NUM_DIMS];
    char netcdf_name[NC_MAX_NAME + 1];
    nc_type netcdf_data_type;
    int netcdf_num_dimensions;
    int netcdf_dim_id[NC_MAX_VAR_DIMS];
    int result;
    long i;

    result = nc_inq_var(ncid, varid, netcdf_name, &netcdf_data_type, &netcdf_num_dimensions, netcdf_dim_id, NULL);
    if (result != NC_NOERR)
    {
        harp_set_error(HARP_ERROR_NETCDF, "%s", nc_strerror(result));
        return -1;
    }

    if (get_harp_type(netcdf_data_type, &data_type) != 0)
    {
        harp_add_error_message(" (variable '%s')", netcdf_name);
        return -1;
    }

    num_dimensions = netcdf_num_dimensions;

    if (data_type == harp_type_string)
    {
        if (num_dimensions == 0)
        {
            harp_set_error(HARP_ERROR_IMPORT, "variable '%s' of type '%s' has 0 dimensions; expected >= 1",
                           netcdf_name, harp_get_data_type_name(harp_type_string));
            return -1;
        }

        if (dimensions->type[netcdf_dim_id[num_dimensions - 1]] != netcdf_dimension_string)
        {
            harp_set_error(HARP_ERROR_IMPORT, "inner-most dimension of variable '%s' is of type '%s'; expected '%s'",
                           netcdf_name, get_dimension_type_name(dimensions->type[netcdf_dim_id[num_dimensions - 1]]),
                           get_dimension_type_name(netcdf_dimension_string));
            return -1;
        }

        num_dimensions--;
    }

    if (num_dimensions > HARP_MAX_NUM_DIMS)
    {
        harp_set_error(HARP_ERROR_IMPORT, "variable '%s' has too many dimensions", netcdf_name);
        return -1;
    }

    for (i = 0; i < num_dimensions; i++)
    {
        if (get_harp_dimension_type(dimensions->type[netcdf_dim_id[i]], &dimension_type[i]) != 0)
        {
            harp_add_error_message(" (variable '%s')", netcdf_name);
            return -1;
        }
    }

    for (i = 0; i < num_dimensions; i++)
    {
        dimension[i] = dimensions->length[netcdf_dim_id[i]];
    }

    if (harp_variable_new(netcdf_name, data_type, num_dimensions, dimension_type, dimension, &variable) != 0)
    {
        return -1;
    }

    if (harp_product_add_variable(product, variable) != 0)
    {
        harp_variable_delete(variable);
        return -1;
    }

    /* Read data. */
    if (data_type == harp_type_string)
    {
        char *buffer;
        long length;

        assert(netcdf_num_dimensions > 0);
        length = dimensions->length[netcdf_dim_id[netcdf_num_dimensions - 1]];

        buffer = malloc(variable->num_elements * length * sizeof(char));
        if (buffer == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           variable->num_elements * length * sizeof(char), __FILE__, __LINE__);
            return -1;
        }

        result = nc_get_var_text(ncid, varid, buffer);
        if (result != NC_NOERR)
        {
            harp_set_error(HARP_ERROR_NETCDF, "%s", nc_strerror(result));
            free(buffer);
            return -1;
        }

        for (i = 0; i < variable->num_elements; i++)
        {
            char *str;

            str = malloc((length + 1) * sizeof(char));
            if (str == NULL)
            {
                harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                               (length + 1) * sizeof(char), __FILE__, __LINE__);
                free(buffer);
                return -1;
            }

            memcpy(str, &buffer[i * length], length);
            str[length] = '\0';
            variable->data.string_data[i] = str;
        }

        free(buffer);
    }
    else
    {
        switch (data_type)
        {
            case harp_type_int8:
                result = nc_get_var_schar(ncid, varid, variable->data.int8_data);
                break;
            case harp_type_int16:
                result = nc_get_var_short(ncid, varid, variable->data.int16_data);
                break;
            case harp_type_int32:
                result = nc_get_var_int(ncid, varid, variable->data.int32_data);
                break;
            case harp_type_float:
                result = nc_get_var_float(ncid, varid, variable->data.float_data);
                break;
            case harp_type_double:
                result = nc_get_var_double(ncid, varid, variable->data.double_data);
                break;
            default:
                assert(0);
                exit(1);
        }

        if (result != NC_NOERR)
        {
            harp_set_error(HARP_ERROR_NETCDF, "%s", nc_strerror(result));
            return -1;
        }
    }

    /* Read attributes. */
    result = nc_inq_att(ncid, varid, "description", NULL, NULL);
    if (result == NC_NOERR)
    {
        if (read_string_attribute(ncid, varid, "description", &variable->description) != 0)
        {
            harp_add_error_message(" (variable '%s')", netcdf_name);
            return -1;
        }
    }
    else if (result != NC_ENOTATT)
    {
        harp_set_error(HARP_ERROR_NETCDF, "%s", nc_strerror(result));
        return -1;
    }

    result = nc_inq_att(ncid, varid, "units", NULL, NULL);
    if (result == NC_NOERR)
    {
        if (read_string_attribute(ncid, varid, "units", &variable->unit) != 0)
        {
            harp_add_error_message(" (variable '%s')", netcdf_name);
            return -1;
        }
    }
    else if (result != NC_ENOTATT)
    {
        harp_set_error(HARP_ERROR_NETCDF, "%s", nc_strerror(result));
        return -1;
    }

    result = nc_inq_att(ncid, varid, "valid_min", NULL, NULL);
    if (result == NC_NOERR)
    {
        harp_data_type attr_data_type;

        if (read_numeric_attribute(ncid, varid, "valid_min", &attr_data_type, &variable->valid_min) != 0)
        {
            harp_add_error_message(" (variable '%s')", netcdf_name);
            return -1;
        }

        if (attr_data_type != data_type)
        {
            harp_set_error(HARP_ERROR_IMPORT, "attribute 'valid_min' of variable '%s' has invalid type", netcdf_name);
            return -1;
        }
    }
    else if (result != NC_ENOTATT)
    {
        harp_set_error(HARP_ERROR_NETCDF, "%s", nc_strerror(result));
        return -1;
    }

    result = nc_inq_att(ncid, varid, "valid_max", NULL, NULL);
    if (result == NC_NOERR)
    {
        harp_data_type attr_data_type;

        if (read_numeric_attribute(ncid, varid, "valid_max", &attr_data_type, &variable->valid_max) != 0)
        {
            harp_add_error_message(" (variable '%s')", netcdf_name);
            return -1;
        }

        if (attr_data_type != data_type)
        {
            harp_set_error(HARP_ERROR_IMPORT, "attribute 'valid_max' of variable '%s' has invalid type", netcdf_name);
            return -1;
        }
    }
    else if (result != NC_ENOTATT)
    {
        harp_set_error(HARP_ERROR_NETCDF, "%s", nc_strerror(result));
        return -1;
    }

    return 0;
}

static int verify_product(int ncid)
{
    int result;
    char *convention_str;

    result = nc_inq_att(ncid, NC_GLOBAL, "Conventions", NULL, NULL);
    if (result == NC_NOERR)
    {
        if (read_string_attribute(ncid, NC_GLOBAL, "Conventions", &convention_str) == 0)
        {
            int major, minor;

            if (harp_parse_file_convention(convention_str, &major, &minor) == 0)
            {
                free(convention_str);
                if (major > HARP_FORMAT_VERSION_MAJOR ||
                    (major == HARP_FORMAT_VERSION_MAJOR && minor > HARP_FORMAT_VERSION_MINOR))
                {
                    harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, "unsupported HARP format version %d.%d",
                                   major, minor);
                    return -1;
                }
                return 0;
            }
            free(convention_str);
        }
    }

    harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, "not a HARP product");

    return -1;
}

static int read_product(int ncid, harp_product *product, netcdf_dimensions *dimensions)
{
    int num_dimensions;
    int num_variables;
    int num_attributes;
    int unlim_dim;
    int result;
    int i;

    result = nc_inq(ncid, &num_dimensions, &num_variables, &num_attributes, &unlim_dim);
    if (result != NC_NOERR)
    {
        harp_set_error(HARP_ERROR_NETCDF, "%s", nc_strerror(result));
        return -1;
    }

    for (i = 0; i < num_dimensions; i++)
    {
        netcdf_dimension_type dimension_type;
        char name[NC_MAX_NAME + 1];
        size_t length;

        result = nc_inq_dim(ncid, i, name, &length);
        if (result != NC_NOERR)
        {
            harp_set_error(HARP_ERROR_NETCDF, "%s", nc_strerror(result));
            return -1;
        }

        if (parse_dimension_type(name, &dimension_type) != 0)
        {
            return -1;
        }

        if (dimensions_add(dimensions, dimension_type, length) != i)
        {
            harp_set_error(HARP_ERROR_IMPORT, "duplicate dimensions with name '%s'", name);
            return -1;
        }
    }

    for (i = 0; i < num_variables; i++)
    {
        if (read_variable(product, ncid, i, dimensions) != 0)
        {
            return -1;
        }
    }

    result = nc_inq_att(ncid, NC_GLOBAL, "source_product", NULL, NULL);
    if (result == NC_NOERR)
    {
        if (read_string_attribute(ncid, NC_GLOBAL, "source_product", &product->source_product) != 0)
        {
            return -1;
        }
    }
    else if (result != NC_ENOTATT)
    {
        harp_set_error(HARP_ERROR_NETCDF, "%s", nc_strerror(result));
        return -1;
    }

    result = nc_inq_att(ncid, NC_GLOBAL, "history", NULL, NULL);
    if (result == NC_NOERR)
    {
        if (read_string_attribute(ncid, NC_GLOBAL, "history", &product->history) != 0)
        {
            return -1;
        }
    }
    else if (result != NC_ENOTATT)
    {
        harp_set_error(HARP_ERROR_NETCDF, "%s", nc_strerror(result));
        return -1;
    }

    return 0;
}

int harp_import_netcdf(const char *filename, harp_product **product)
{
    harp_product *new_product;
    netcdf_dimensions dimensions;
    int ncid;
    int result;

    if (filename == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "filename is NULL (%s:%u)", __FILE__, __LINE__);
        return -1;
    }

    result = nc_open(filename, 0, &ncid);
    if (result != NC_NOERR)
    {
        harp_set_error(HARP_ERROR_NETCDF, "%s", nc_strerror(result));
        return -1;
    }

    if (verify_product(ncid) != 0)
    {
        nc_close(ncid);
        return -1;
    }

    if (harp_product_new(&new_product) != 0)
    {
        nc_close(ncid);
        return -1;
    }

    dimensions_init(&dimensions);

    if (read_product(ncid, new_product, &dimensions) != 0)
    {
        dimensions_done(&dimensions);
        harp_product_delete(new_product);
        nc_close(ncid);
        return -1;
    }

    dimensions_done(&dimensions);

    result = nc_close(ncid);
    if (result != NC_NOERR)
    {
        harp_set_error(HARP_ERROR_NETCDF, "%s", nc_strerror(result));
        harp_product_delete(new_product);
        return -1;
    }

    *product = new_product;
    return 0;
}

int harp_import_global_attributes_netcdf(const char *filename, double *datetime_start, double *datetime_stop,
                                         long dimension[], char **source_product)
{
    char *attr_source_product = NULL;
    harp_scalar attr_datetime_start;
    harp_scalar attr_datetime_stop;
    harp_data_type attr_data_type;
    long attr_dimension[HARP_NUM_DIM_TYPES];
    int result;
    int ncid;
    int i;

    if (datetime_start == NULL && datetime_stop == NULL)
    {
        return 0;
    }

    if (filename == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "filename is NULL (%s:%u)", __FILE__, __LINE__);
        return -1;
    }

    result = nc_open(filename, 0, &ncid);
    if (result != NC_NOERR)
    {
        harp_set_error(HARP_ERROR_NETCDF, "%s", nc_strerror(result));
        return -1;
    }

    if (verify_product(ncid) != 0)
    {
        nc_close(ncid);
        return -1;
    }

    if (datetime_start != NULL)
    {
        if (nc_inq_att(ncid, NC_GLOBAL, "datetime_start", NULL, NULL) == NC_NOERR)
        {
            if (read_numeric_attribute(ncid, NC_GLOBAL, "datetime_start", &attr_data_type, &attr_datetime_start) != 0)
            {
                nc_close(ncid);
                return -1;
            }

            if (attr_data_type != harp_type_double)
            {
                harp_set_error(HARP_ERROR_IMPORT, "attribute 'datetime_start' has invalid type");
                nc_close(ncid);
                return -1;
            }
        }
        else
        {
            attr_datetime_start.double_data = harp_mininf();
        }
    }

    if (datetime_stop != NULL)
    {
        if (nc_inq_att(ncid, NC_GLOBAL, "datetime_stop", NULL, NULL) == NC_NOERR)
        {
            if (read_numeric_attribute(ncid, NC_GLOBAL, "datetime_stop", &attr_data_type, &attr_datetime_stop) != 0)
            {
                nc_close(ncid);
                return -1;
            }

            if (attr_data_type != harp_type_double)
            {
                harp_set_error(HARP_ERROR_IMPORT, "attribute 'datetime_stop' has invalid type");
                nc_close(ncid);
                return -1;
            }
        }
        else
        {
            attr_datetime_stop.double_data = harp_plusinf();
        }
    }

    if (dimension != NULL)
    {
        int num_dimensions;
        int num_variables;
        int num_attributes;
        int unlim_dim;
        int result;

        for (i = 0; i < HARP_NUM_DIM_TYPES; i++)
        {
            attr_dimension[i] = -1;
        }

        result = nc_inq(ncid, &num_dimensions, &num_variables, &num_attributes, &unlim_dim);
        if (result != NC_NOERR)
        {
            harp_set_error(HARP_ERROR_NETCDF, "%s", nc_strerror(result));
            return -1;
        }

        for (i = 0; i < num_dimensions; i++)
        {
            netcdf_dimension_type netcdf_dim_type;
            harp_dimension_type harp_dim_type;
            char name[NC_MAX_NAME + 1];
            size_t length;

            result = nc_inq_dim(ncid, i, name, &length);
            if (result != NC_NOERR)
            {
                harp_set_error(HARP_ERROR_NETCDF, "%s", nc_strerror(result));
                return -1;
            }

            if (parse_dimension_type(name, &netcdf_dim_type) != 0)
            {
                return -1;
            }
            if (netcdf_dim_type != netcdf_dimension_independent && netcdf_dim_type != netcdf_dimension_string)
            {
                if (get_harp_dimension_type(netcdf_dim_type, &harp_dim_type) != 0)
                {
                    return -1;
                }
                attr_dimension[harp_dim_type] = length;
            }
        }
    }

    if (source_product != NULL)
    {
        if (nc_inq_att(ncid, NC_GLOBAL, "source_product", NULL, NULL) == NC_NOERR)
        {
            if (read_string_attribute(ncid, NC_GLOBAL, "source_product", &attr_source_product) != 0)
            {
                nc_close(ncid);
                return -1;
            }
        }
        else
        {
            /* use filename if there is no source_product attribute */
            attr_source_product = strdup(harp_basename(filename));
            if (attr_source_product == NULL)
            {
                harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                               __LINE__);
                return -1;
            }
        }
    }

    result = nc_close(ncid);
    if (result != NC_NOERR)
    {
        harp_set_error(HARP_ERROR_NETCDF, "%s", nc_strerror(result));
        free(attr_source_product);
        return -1;
    }

    if (datetime_start != NULL)
    {
        *datetime_start = attr_datetime_start.double_data;
    }

    if (datetime_stop != NULL)
    {
        *datetime_stop = attr_datetime_stop.double_data;
    }

    if (source_product != NULL)
    {
        *source_product = attr_source_product;
    }

    if (dimension != NULL)
    {
        for (i = 0; i < HARP_NUM_DIM_TYPES; i++)
        {
            dimension[i] = attr_dimension[i];
        }
    }

    return 0;
}

static int write_dimensions(int ncid, const netcdf_dimensions *dimensions)
{
    int result;
    int i;

    for (i = 0; i < dimensions->num_dimensions; i++)
    {
        int dim_id;

        if (dimensions->type[i] == netcdf_dimension_independent)
        {
            char name[64];

            sprintf(name, "independent_%ld", dimensions->length[i]);
            result = nc_def_dim(ncid, name, dimensions->length[i], &dim_id);
        }
        else if (dimensions->type[i] == netcdf_dimension_string)
        {
            char name[64];

            sprintf(name, "string_%ld", dimensions->length[i]);
            result = nc_def_dim(ncid, name, dimensions->length[i], &dim_id);
        }
        else
        {
            result = nc_def_dim(ncid, get_dimension_type_name(dimensions->type[i]), dimensions->length[i], &dim_id);
        }

        if (result != NC_NOERR)
        {
            harp_set_error(HARP_ERROR_NETCDF, "%s", nc_strerror(result));
            return -1;
        }

        assert(dim_id == i);
    }

    return 0;
}

static int write_string_attribute(int ncid, int varid, const char *name, const char *data)
{
    int result;

    result = nc_put_att_text(ncid, varid, name, strlen(data), data);
    if (result != NC_NOERR)
    {
        harp_set_error(HARP_ERROR_NETCDF, "%s", nc_strerror(result));
        return -1;
    }

    return 0;
}

static int write_numeric_attribute(int ncid, int varid, const char *name, harp_data_type data_type, harp_scalar data)
{
    int result;

    result = NC_NOERR;
    switch (data_type)
    {
        case harp_type_int8:
            result = nc_put_att_schar(ncid, varid, name, NC_BYTE, 1, &data.int8_data);
            break;
        case harp_type_int16:
            result = nc_put_att_short(ncid, varid, name, NC_SHORT, 1, &data.int16_data);
            break;
        case harp_type_int32:
            result = nc_put_att_int(ncid, varid, name, NC_INT, 1, &data.int32_data);
            break;
        case harp_type_float:
            result = nc_put_att_float(ncid, varid, name, NC_FLOAT, 1, &data.float_data);
            break;
        case harp_type_double:
            result = nc_put_att_double(ncid, varid, name, NC_DOUBLE, 1, &data.double_data);
            break;
        default:
            assert(0);
            exit(1);
    }

    if (result != NC_NOERR)
    {
        harp_set_error(HARP_ERROR_NETCDF, "%s", nc_strerror(result));
        return -1;
    }

    return 0;
}

static int write_variable_definition(int ncid, const harp_variable *variable, netcdf_dimensions *dimensions, int *varid)
{
    int num_dimensions;
    int dim_id[NC_MAX_VAR_DIMS];
    int result;
    int i;

    num_dimensions = variable->num_dimensions;
    assert(num_dimensions <= NC_MAX_VAR_DIMS);

    for (i = 0; i < num_dimensions; i++)
    {
        dim_id[i] = dimensions_find(dimensions, get_netcdf_dimension_type(variable->dimension_type[i]),
                                    variable->dimension[i]);
        assert(dim_id[i] >= 0);
    }

    /* A variable of type string is stored as a contiguous array of characters. The array has an additional dimension
     * the length of which is set to the length of the longest string. Shorter strings will be padded with NUL '\0'
     * termination characters.
     *
     * netCDF does not support zero length dimensions, so a dimension of length 1 is added if the maximum string length
     * is 0. In this case a single NUL character will be written for each string.
     */
    if (variable->data_type == harp_type_string)
    {
        long length;

        assert((num_dimensions + 1) < NC_MAX_VAR_DIMS);

        /* determine length for the string dimension (ensure a minimum length of 1) */
        length = harp_get_max_string_length(variable->num_elements, variable->data.string_data);
        if (length == 0)
        {
            length = 1;
        }

        dim_id[num_dimensions] = dimensions_find(dimensions, netcdf_dimension_string, length);
        assert(dim_id[num_dimensions] >= 0);

        num_dimensions++;
    }

    result = nc_def_var(ncid, variable->name, get_netcdf_type(variable->data_type), num_dimensions, dim_id, varid);
    if (result != NC_NOERR)
    {
        harp_set_error(HARP_ERROR_NETCDF, "%s", nc_strerror(result));
        return -1;
    }

    if (variable->description != NULL && strcmp(variable->description, "") != 0)
    {
        if (write_string_attribute(ncid, *varid, "description", variable->description) != 0)
        {
            return -1;
        }
    }

    if (variable->unit != NULL)
    {
        if (write_string_attribute(ncid, *varid, "units", variable->unit) != 0)
        {
            return -1;
        }
    }

    if (variable->data_type != harp_type_string)
    {
        if (!harp_is_valid_min_for_type(variable->data_type, variable->valid_min))
        {
            if (write_numeric_attribute(ncid, *varid, "valid_min", variable->data_type, variable->valid_min) != 0)
            {
                return -1;
            }
        }

        if (!harp_is_valid_max_for_type(variable->data_type, variable->valid_max))
        {
            if (write_numeric_attribute(ncid, *varid, "valid_max", variable->data_type, variable->valid_max) != 0)
            {
                return -1;
            }
        }
    }

    return 0;
}

static int write_variable(int ncid, int varid, const harp_variable *variable)
{
    int result = NC_NOERR;

    switch (variable->data_type)
    {
        case harp_type_int8:
            result = nc_put_var_schar(ncid, varid, variable->data.ptr);
            break;
        case harp_type_int16:
            result = nc_put_var_short(ncid, varid, variable->data.ptr);
            break;
        case harp_type_int32:
            result = nc_put_var_int(ncid, varid, variable->data.ptr);
            break;
        case harp_type_float:
            result = nc_put_var_float(ncid, varid, variable->data.ptr);
            break;
        case harp_type_double:
            result = nc_put_var_double(ncid, varid, variable->data.ptr);
            break;
        case harp_type_string:
            {
                char *buffer;

                if (harp_get_char_array_from_string_array(variable->num_elements, variable->data.string_data, 1, NULL,
                                                          &buffer) != 0)
                {
                    return -1;
                }

                result = nc_put_var_text(ncid, varid, buffer);
                free(buffer);
            }
            break;
    }

    if (result != NC_NOERR)
    {
        harp_set_error(HARP_ERROR_NETCDF, "%s", nc_strerror(result));
        return -1;
    }

    return 0;
}

static int write_product(int ncid, const harp_product *product, netcdf_dimensions *dimensions)
{
    harp_scalar datetime_start;
    harp_scalar datetime_stop;
    int result;
    int i;

    /* write conventions */
    if (write_string_attribute(ncid, NC_GLOBAL, "Conventions", HARP_CONVENTION) != 0)
    {
        return -1;
    }

    /* write attributes */
    if (harp_product_get_datetime_range(product, &datetime_start.double_data, &datetime_stop.double_data) == 0)
    {
        if (write_numeric_attribute(ncid, NC_GLOBAL, "datetime_start", harp_type_double, datetime_start) != 0)
        {
            return -1;
        }

        if (write_numeric_attribute(ncid, NC_GLOBAL, "datetime_stop", harp_type_double, datetime_stop) != 0)
        {
            return -1;
        }
    }

    if (product->source_product != NULL && strcmp(product->source_product, "") != 0)
    {
        if (write_string_attribute(ncid, NC_GLOBAL, "source_product", product->source_product) != 0)
        {
            return -1;
        }
    }

    if (product->history != NULL && strcmp(product->history, "") != 0)
    {
        if (write_string_attribute(ncid, NC_GLOBAL, "history", product->history) != 0)
        {
            return -1;
        }
    }

    /* determine dimensions */
    for (i = 0; i < product->num_variables; i++)
    {
        harp_variable *variable;
        int j;

        variable = product->variable[i];
        for (j = 0; j < variable->num_dimensions; j++)
        {
            netcdf_dimension_type dimension_type;

            dimension_type = get_netcdf_dimension_type(variable->dimension_type[j]);
            if (dimensions_add(dimensions, dimension_type, variable->dimension[j]) < 0)
            {
                return -1;
            }
        }

        if (variable->data_type == harp_type_string)
        {
            long length;

            /* determine length for the string dimension (ensure a minimum length of 1) */
            length = harp_get_max_string_length(variable->num_elements, variable->data.string_data);
            if (length == 0)
            {
                length = 1;
            }

            if (dimensions_add(dimensions, netcdf_dimension_string, length) < 0)
            {
                return -1;
            }
        }
    }

    /* write dimensions */
    if (write_dimensions(ncid, dimensions) != 0)
    {
        return -1;
    }

    /* write variable definitions + attributes */
    for (i = 0; i < product->num_variables; i++)
    {
        int varid;

        if (write_variable_definition(ncid, product->variable[i], dimensions, &varid) != 0)
        {
            return -1;
        }
        assert(varid == i);
    }

    result = nc_enddef(ncid);
    if (result != NC_NOERR)
    {
        harp_set_error(HARP_ERROR_NETCDF, "%s", nc_strerror(result));
        return -1;
    }

    /* write variable data */
    for (i = 0; i < product->num_variables; i++)
    {
        if (write_variable(ncid, i, product->variable[i]) != 0)
        {
            return -1;
        }
    }

    return 0;
}

int harp_export_netcdf(const char *filename, const harp_product *product)
{
    netcdf_dimensions dimensions;
    int64_t size;
    int flags = 0;
    int result;
    int ncid;

    if (filename == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "filename is NULL");
        return -1;
    }

    if (product == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "product is NULL");
        return -1;
    }

    if (harp_product_get_storage_size(product, 1, &size) != 0)
    {
        return -1;
    }
    if (size > 1073741824)
    {
        /* files larger than 1GB will be stored using 64-bit offsets */
        flags |= NC_64BIT_OFFSET;
    }
    result = nc_create(filename, flags, &ncid);
    if (result != NC_NOERR)
    {
        harp_set_error(HARP_ERROR_NETCDF, "%s", nc_strerror(result));
        harp_add_error_message(" (%s)", filename);
        return -1;
    }

    dimensions_init(&dimensions);

    if (write_product(ncid, product, &dimensions) != 0)
    {
        harp_add_error_message(" (%s)", filename);
        nc_close(ncid);
        dimensions_done(&dimensions);
        return -1;
    }

    dimensions_done(&dimensions);

    result = nc_close(ncid);
    if (result != NC_NOERR)
    {
        harp_set_error(HARP_ERROR_NETCDF, "%s", nc_strerror(result));
        harp_add_error_message(" (%s)", filename);
        return -1;
    }

    return 0;
}
