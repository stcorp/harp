/*
 * Copyright (C) 2015 S[&]T, The Netherlands.
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

#include <assert.h>
#include <stdlib.h>

#include "hdf.h"
#include "mfhdf.h"

#define MAX_HDF4_NAME_LENGTH 256
#define MAX_HDF4_VAR_DIMS 32

static int get_harp_type(int32 hdf4_data_type, harp_data_type *data_type)
{
    switch (hdf4_data_type)
    {
        case DFNT_CHAR:
            *data_type = harp_type_string;
            break;
        case DFNT_INT8:
            *data_type = harp_type_int8;
            break;
        case DFNT_INT16:
            *data_type = harp_type_int16;
            break;
        case DFNT_INT32:
            *data_type = harp_type_int32;
            break;
        case DFNT_FLOAT32:
            *data_type = harp_type_float;
            break;
        case DFNT_FLOAT64:
            *data_type = harp_type_double;
            break;
        default:
            harp_set_error(HARP_ERROR_PRODUCT, "unsupported HDF4 data type");
            return -1;
    }

    return 0;
}

static int32 get_hdf4_type(harp_data_type data_type)
{
    switch (data_type)
    {
        case harp_type_int8:
            return DFNT_INT8;
        case harp_type_int16:
            return DFNT_INT16;
        case harp_type_int32:
            return DFNT_INT32;
        case harp_type_float:
            return DFNT_FLOAT32;
        case harp_type_double:
            return DFNT_FLOAT64;
        case harp_type_string:
            return DFNT_CHAR;
        default:
            assert(0);
            exit(1);
    }
}

static int read_string_attribute(int32 obj_id, int32 index, char **data)
{
    char *str;
    char name[MAX_HDF4_NAME_LENGTH + 1];
    int32 data_type;
    int32 num_elements;

    if (SDattrinfo(obj_id, index, name, &data_type, &num_elements) != 0)
    {
        harp_set_error(HARP_ERROR_HDF4, NULL);
        return -1;
    }

    if (data_type != DFNT_CHAR)
    {
        harp_set_error(HARP_ERROR_PRODUCT, "attribute '%s' has invalid type", name);
        return -1;
    }

    str = malloc((num_elements + 1) * sizeof(char));
    if (str == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       (num_elements + 1) * sizeof(char), __FILE__, __LINE__);
        return -1;
    }

    if (SDreadattr(obj_id, index, str) != 0)
    {
        harp_set_error(HARP_ERROR_HDF4, NULL);
        free(str);
        return -1;
    }
    str[num_elements] = '\0';

    *data = str;
    return 0;
}

static int read_numeric_attribute(int32 obj_id, int32 index, harp_data_type *data_type, harp_scalar *data)
{
    char name[MAX_HDF4_NAME_LENGTH + 1];
    int32 hdf4_data_type;
    int32 num_elements;
    int result;

    if (SDattrinfo(obj_id, index, name, &hdf4_data_type, &num_elements) != 0)
    {
        harp_set_error(HARP_ERROR_HDF4, NULL);
        return -1;
    }

    if (get_harp_type(hdf4_data_type, data_type) != 0)
    {
        return -1;
    }

    if (num_elements != 1)
    {
        harp_set_error(HARP_ERROR_PRODUCT, "attribute '%s' has invalid format", name);
        return -1;
    }

    switch (*data_type)
    {
        case harp_type_int8:
            result = SDreadattr(obj_id, index, &data->int8_data);
            break;
        case harp_type_int16:
            result = SDreadattr(obj_id, index, &data->int16_data);
            break;
        case harp_type_int32:
            result = SDreadattr(obj_id, index, &data->int32_data);
            break;
        case harp_type_float:
            result = SDreadattr(obj_id, index, &data->float_data);
            break;
        case harp_type_double:
            result = SDreadattr(obj_id, index, &data->double_data);
            break;
        default:
            harp_set_error(HARP_ERROR_PRODUCT, "attribute '%s' has invalid type", name);
            return -1;
    }

    if (result != 0)
    {
        harp_set_error(HARP_ERROR_HDF4, NULL);
        return -1;
    }

    return 0;
}

static int read_dimensions(int32 sds_id, int *num_dimensions, harp_dimension_type *dimension_type)
{
    int32_t index;
    char *cursor;
    char *dims;

    index = SDfindattr(sds_id, "dims");
    if (index < 0)
    {
        /* If the 'dims' attribute does not exist, the corresponding variable is scalar (i.e. has zero dimensions). */
        *num_dimensions = 0;
        return 0;
    }

    if (read_string_attribute(sds_id, index, &dims) != 0)
    {
        return -1;
    }
    assert(*dims != '\0');

    cursor = dims;
    *num_dimensions = 0;
    while (*cursor != '\0' && *num_dimensions < HARP_MAX_NUM_DIMS)
    {
        char *mark;

        mark = cursor;
        while (*cursor != '\0' && *cursor != ',')
        {
            cursor++;
        }

        if (*cursor == ',')
        {
            *cursor = '\0';
            cursor++;
        }

        if (harp_parse_dimension_type(mark, &dimension_type[*num_dimensions]) != 0)
        {
            free(dims);
            return -1;
        }

        (*num_dimensions)++;
    }

    if (*num_dimensions == HARP_MAX_NUM_DIMS && *cursor != '\0')
    {
        harp_set_error(HARP_ERROR_PRODUCT, "too many dimensions in dimension list");
        free(dims);
        return -1;
    }

    if (*(cursor - 1) == '\0')
    {
        harp_set_error(HARP_ERROR_PRODUCT, "trailing ',' in dimension list");
        free(dims);
        return -1;
    }

    free(dims);
    return 0;
}

static int read_variable(harp_product *product, int32 sds_id)
{
    char hdf4_name[MAX_HDF4_NAME_LENGTH + 1];
    int32 hdf4_data_type;
    int32 hdf4_num_dimensions;
    int32 hdf4_dimension[MAX_HDF4_VAR_DIMS];
    int32 hdf4_dont_care;
    int32 hdf4_start[MAX_HDF4_VAR_DIMS] = { 0 };
    int32 hdf4_index;
    harp_variable *variable;
    harp_dimension_type dimension_type[HARP_MAX_NUM_DIMS];
    long dimension[HARP_MAX_NUM_DIMS];
    harp_data_type data_type;
    int num_dimensions;
    int i;

    if (SDgetinfo(sds_id, hdf4_name, &hdf4_num_dimensions, hdf4_dimension, &hdf4_data_type, &hdf4_dont_care) != 0)
    {
        harp_set_error(HARP_ERROR_HDF4, NULL);
        return -1;
    }
    assert(hdf4_num_dimensions > 0);

    if (get_harp_type(hdf4_data_type, &data_type) != 0)
    {
        harp_add_error_message(" (dataset '%s')", hdf4_name);
        return -1;
    }

    /* Read dimensions. NB. The number of dimensions in the dimension list (i.e. the contents of the 'dims' variable
     * attribute) can differ from the number of dimensions of the variable as stored in the HDF4 file. An additional
     * dimension of is added for scalars as well as for variables of type string (see also the write_variable()
     * function).
     */
    if (read_dimensions(sds_id, &num_dimensions, dimension_type) != 0)
    {
        harp_add_error_message(" (dataset '%s')", hdf4_name);
        return -1;
    }

    if (data_type == harp_type_string)
    {
        if (hdf4_num_dimensions != (num_dimensions + 1))
        {
            harp_set_error(HARP_ERROR_PRODUCT, "dataset '%s' has %d dimensions; expected %d", hdf4_name,
                           hdf4_num_dimensions, num_dimensions + 1);
            return -1;
        }
    }
    else if (num_dimensions == 0)
    {
        if (hdf4_num_dimensions != 1)
        {
            harp_set_error(HARP_ERROR_PRODUCT, "dataset '%s' has %d dimensions; expected 1", hdf4_name,
                           hdf4_num_dimensions);
            return -1;
        }

        if (hdf4_dimension[0] != 1)
        {
            harp_set_error(HARP_ERROR_PRODUCT, "dataset '%s' has %d elements; expected 1", hdf4_name,
                           hdf4_dimension[0]);
            return -1;
        }
    }
    else if (hdf4_num_dimensions != num_dimensions)
    {
        harp_set_error(HARP_ERROR_PRODUCT, "dataset '%s' has %d dimensions; expected %d", hdf4_name,
                       hdf4_num_dimensions, num_dimensions);
        return -1;
    }

    for (i = 0; i < num_dimensions; i++)
    {
        dimension[i] = (long)hdf4_dimension[i];
    }

    if (harp_variable_new(hdf4_name, data_type, num_dimensions, dimension_type, dimension, &variable) != 0)
    {
        return -1;
    }

    if (harp_product_add_variable(product, variable) != 0)
    {
        harp_variable_delete(variable);
        return -1;
    }

    /* Read data. */
    if (hdf4_data_type == DFNT_CHAR)
    {
        char *buffer = NULL;
        long length = hdf4_dimension[hdf4_num_dimensions - 1];

        buffer = malloc(variable->num_elements * length * sizeof(char));
        if (buffer == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           variable->num_elements * length * sizeof(char), __FILE__, __LINE__);
            return -1;
        }

        if (SDreaddata(sds_id, hdf4_start, NULL, hdf4_dimension, buffer) != 0)
        {
            harp_set_error(HARP_ERROR_HDF4, NULL);
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
        if (SDreaddata(sds_id, hdf4_start, NULL, hdf4_dimension, variable->data.ptr) != 0)
        {
            harp_set_error(HARP_ERROR_HDF4, NULL);
            return -1;
        }
    }

    /* Read attributes. */
    hdf4_index = SDfindattr(sds_id, "description");
    if (hdf4_index >= 0)
    {
        if (read_string_attribute(sds_id, hdf4_index, &variable->description) != 0)
        {
            return -1;
        }
    }

    hdf4_index = SDfindattr(sds_id, "units");
    if (hdf4_index >= 0)
    {
        if (read_string_attribute(sds_id, hdf4_index, &variable->unit) != 0)
        {
            return -1;
        }
    }

    hdf4_index = SDfindattr(sds_id, "valid_min");
    if (hdf4_index >= 0)
    {
        harp_data_type attr_data_type;

        if (read_numeric_attribute(sds_id, hdf4_index, &attr_data_type, &variable->valid_min) != 0)
        {
            return -1;
        }

        if (attr_data_type != data_type)
        {
            harp_set_error(HARP_ERROR_PRODUCT, "attribute 'valid_min' of variable '%s' has invalid type", hdf4_name);
            return -1;
        }
    }

    hdf4_index = SDfindattr(sds_id, "valid_max");
    if (hdf4_index >= 0)
    {
        harp_data_type attr_data_type;

        if (read_numeric_attribute(sds_id, hdf4_index, &attr_data_type, &variable->valid_max) != 0)
        {
            return -1;
        }

        if (attr_data_type != data_type)
        {
            harp_set_error(HARP_ERROR_PRODUCT, "attribute 'valid_max' of variable '%s' has invalid type", hdf4_name);
            return -1;
        }
    }

    return 0;
}

static int read_product(harp_product *product, int32 sd_id)
{
    int32 num_sds;
    int32 hdf4_num_attributes;
    int32 hdf4_index;
    int i;

    if (SDfileinfo(sd_id, &num_sds, &hdf4_num_attributes) != 0)
    {
        harp_set_error(HARP_ERROR_HDF4, NULL);
        return -1;
    }

    /* Read variables. */
    for (i = 0; i < num_sds; i++)
    {
        int32 sds_id;

        sds_id = SDselect(sd_id, i);
        if (sds_id == -1)
        {
            harp_set_error(HARP_ERROR_HDF4, NULL);
            return -1;
        }

        if (read_variable(product, sds_id) != 0)
        {
            SDendaccess(sds_id);
            return -1;
        }

        SDendaccess(sds_id);
    }

    /* Read attributes. */
    hdf4_index = SDfindattr(sd_id, "source_product");
    if (hdf4_index >= 0)
    {
        if (read_string_attribute(sd_id, hdf4_index, &product->source_product) != 0)
        {
            return -1;
        }
    }

    hdf4_index = SDfindattr(sd_id, "history");
    if (hdf4_index >= 0)
    {
        if (read_string_attribute(sd_id, hdf4_index, &product->history) != 0)
        {
            return -1;
        }
    }

    return 0;
}

static int verify_product(int32 sd_id)
{
    int32 index;
    char *convention_str;

    index = SDfindattr(sd_id, "Conventions");
    if (index >= 0)
    {
        if (read_string_attribute(sd_id, index, &convention_str) == 0)
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

    harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, "not a valid HARP product");
    return -1;
}

int harp_import_hdf4(const char *filename, harp_product **product)
{
    harp_product *new_product;
    int32 sd_id;

    sd_id = SDstart(filename, DFACC_READ);
    if (sd_id == -1)
    {
        harp_set_error(HARP_ERROR_HDF4, NULL);
        return -1;
    }

    if (verify_product(sd_id) != 0)
    {
        SDend(sd_id);
        return -1;
    }

    if (harp_product_new(&new_product) != 0)
    {
        SDend(sd_id);
        return -1;
    }

    if (read_product(new_product, sd_id) != 0)
    {
        harp_product_delete(new_product);
        SDend(sd_id);
        return -1;
    }

    SDend(sd_id);

    *product = new_product;
    return 0;
}

static int write_string_attribute(int32 obj_id, const char *name, const char *data)
{
    if (SDsetattr(obj_id, name, DFNT_CHAR, strlen(data), data) != 0)
    {
        harp_set_error(HARP_ERROR_HDF4, NULL);
        return -1;
    }

    return 0;
}

static int write_numeric_attribute(int32 obj_id, const char *name, harp_data_type data_type, harp_scalar data)
{
    int result;

    switch (data_type)
    {
        case harp_type_int8:
            result = SDsetattr(obj_id, name, get_hdf4_type(data_type), 1, &data.int8_data);
            break;
        case harp_type_int16:
            result = SDsetattr(obj_id, name, get_hdf4_type(data_type), 1, &data.int16_data);
            break;
        case harp_type_int32:
            result = SDsetattr(obj_id, name, get_hdf4_type(data_type), 1, &data.int32_data);
            break;
        case harp_type_float:
            result = SDsetattr(obj_id, name, get_hdf4_type(data_type), 1, &data.float_data);
            break;
        case harp_type_double:
            result = SDsetattr(obj_id, name, get_hdf4_type(data_type), 1, &data.double_data);
            break;
        default:
            assert(0);
            exit(1);
    }

    if (result != 0)
    {
        harp_set_error(HARP_ERROR_HDF4, NULL);
        return -1;
    }

    return 0;
}

static int write_dimensions(int32 sds_id, int num_dimensions, const harp_dimension_type *dimension)
{
    char *dimension_str;
    int length;
    int i;

    if (num_dimensions == 0)
    {
        return 0;
    }

    length = 0;
    for (i = 0; i < num_dimensions; i++)
    {
        length += strlen(harp_get_dimension_type_name(dimension[i]));

        /* Reserve additional space for the ',' separator. */
        if (i < num_dimensions - 1)
        {
            length += 1;
        }
    }

    dimension_str = (char *)malloc((length + 1) * sizeof(char));
    if (dimension_str == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       (length + 1) * sizeof(char), __FILE__, __LINE__);
        return -1;
    }

    dimension_str[0] = '\0';
    for (i = 0; i < num_dimensions; i++)
    {
        strcat(dimension_str, harp_get_dimension_type_name(dimension[i]));

        if (i < num_dimensions - 1)
        {
            strcat(dimension_str, ",");
        }
    }

    if (write_string_attribute(sds_id, "dims", dimension_str) != 0)
    {
        free(dimension_str);
        return -1;
    }

    free(dimension_str);

    return 0;
}

static int write_variable(harp_variable *variable, int32 sd_id)
{
    int32 sds_id;
    int32 hdf4_start[MAX_HDF4_VAR_DIMS] = { 0 };
    int32 hdf4_num_dimensions;
    int32 hdf4_dimension[MAX_HDF4_VAR_DIMS];
    int i;

    for (i = 0; i < variable->num_dimensions; i++)
    {
        hdf4_dimension[i] = variable->dimension[i];
    }
    hdf4_num_dimensions = variable->num_dimensions;

    /* Write data. */
    if (variable->data_type == harp_type_string)
    {
        char *buffer;
        long length;

        if (harp_get_char_array_from_string_array(variable->num_elements, variable->data.string_data, 1, &length,
                                                  &buffer) != 0)
        {
            return -1;
        }

        /* Add an additional dimension with a length equal to the length of the longest string, or 1 if the longest
         * string is of length zero.
         */
        hdf4_dimension[hdf4_num_dimensions] = length;
        hdf4_num_dimensions++;

        sds_id = SDcreate(sd_id, variable->name, DFNT_CHAR, hdf4_num_dimensions, hdf4_dimension);
        if (sds_id == -1)
        {
            harp_set_error(HARP_ERROR_HDF4, NULL);
            free(buffer);
            return -1;
        }

        if (SDwritedata(sds_id, hdf4_start, NULL, hdf4_dimension, buffer) != 0)
        {
            harp_set_error(HARP_ERROR_HDF4, NULL);
            SDendaccess(sds_id);
            free(buffer);
            return -1;
        }

        free(buffer);
    }
    else
    {
        /* HDF4 does not support data sets with zero dimensions. */
        if (hdf4_num_dimensions == 0)
        {
            hdf4_dimension[0] = 1;
            hdf4_num_dimensions++;
        }

        sds_id = SDcreate(sd_id, variable->name, get_hdf4_type(variable->data_type), hdf4_num_dimensions,
                          hdf4_dimension);
        if (sds_id == -1)
        {
            harp_set_error(HARP_ERROR_HDF4, NULL);
            return -1;
        }

        if (SDwritedata(sds_id, hdf4_start, NULL, hdf4_dimension, variable->data.ptr) != 0)
        {
            harp_set_error(HARP_ERROR_HDF4, NULL);
            SDendaccess(sds_id);
            return -1;
        }
    }

    /* Write dimensions. */
    if (write_dimensions(sds_id, variable->num_dimensions, variable->dimension_type) != 0)
    {
        SDendaccess(sds_id);
        return -1;
    }

    /* Write attributes. */
    if (variable->description != NULL && strcmp(variable->description, "") != 0)
    {
        if (write_string_attribute(sds_id, "description", variable->description) != 0)
        {
            SDendaccess(sds_id);
            return -1;
        }
    }

    if (variable->unit != NULL && strcmp(variable->unit, "") != 0)
    {
        if (write_string_attribute(sds_id, "units", variable->unit) != 0)
        {
            SDendaccess(sds_id);
            return -1;
        }
    }

    if (variable->data_type != harp_type_string)
    {
        if (!harp_is_valid_min_for_type(variable->data_type, variable->valid_min))
        {
            if (write_numeric_attribute(sds_id, "valid_min", variable->data_type, variable->valid_min) != 0)
            {
                SDendaccess(sds_id);
                return -1;
            }
        }

        if (!harp_is_valid_max_for_type(variable->data_type, variable->valid_max))
        {
            if (write_numeric_attribute(sds_id, "valid_max", variable->data_type, variable->valid_max) != 0)
            {
                SDendaccess(sds_id);
                return -1;
            }
        }
    }

    SDendaccess(sds_id);

    return 0;
}

static int write_product(const harp_product *product, int32 sd_id)
{
    int i;

    /* Write file convention. */
    if (write_string_attribute(sd_id, "Conventions", HARP_CONVENTION) != 0)
    {
        return -1;
    }

    /* Write attributes. */
    if (product->source_product != NULL && strcmp(product->source_product, "") != 0)
    {
        if (write_string_attribute(sd_id, "source_product", product->source_product) != 0)
        {
            return -1;
        }
    }

    if (product->history != NULL && strcmp(product->history, "") != 0)
    {
        if (write_string_attribute(sd_id, "history", product->history) != 0)
        {
            return -1;
        }
    }

    /* Write variables. */
    for (i = 0; i < product->num_variables; i++)
    {
        if (write_variable(product->variable[i], sd_id) != 0)
        {
            return -1;
        }
    }

    return 0;
}

int harp_export_hdf4(const char *filename, const harp_product *product)
{
    int32 sd_id;

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

    sd_id = SDstart(filename, DFACC_CREATE);
    if (sd_id == -1)
    {
        harp_set_error(HARP_ERROR_HDF4, NULL);
        return -1;
    }

    if (write_product(product, sd_id) != 0)
    {
        SDend(sd_id);
        return -1;
    }

    if (SDend(sd_id) != 0)
    {
        harp_set_error(HARP_ERROR_HDF4, NULL);
        return -1;
    }

    return 0;
}

void harp_hdf4_add_error_message(void)
{
    int error = HEvalue(1);

    if (error != 0)
    {
        harp_add_error_message("[HDF4] %s", HEstring(error));
    }
}
