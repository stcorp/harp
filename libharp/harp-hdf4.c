/*
 * Copyright (C) 2015-2020 S[&]T, The Netherlands.
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
#include <stdlib.h>

#include "hdf.h"
#include "mfhdf.h"

#define MAX_HDF4_NAME_LENGTH 256
#define MAX_HDF4_VAR_DIMS 32

typedef enum hdf4_dimension_type_enum
{
    hdf4_dimension_time,
    hdf4_dimension_latitude,
    hdf4_dimension_longitude,
    hdf4_dimension_vertical,
    hdf4_dimension_spectral,
    hdf4_dimension_independent,
    hdf4_dimension_string,
    hdf4_dimension_scalar
} hdf4_dimension_type;

static const char *get_dimension_type_name(hdf4_dimension_type dimension_type)
{
    switch (dimension_type)
    {
        case hdf4_dimension_time:
            return "time";
        case hdf4_dimension_latitude:
            return "latitude";
        case hdf4_dimension_longitude:
            return "longitude";
        case hdf4_dimension_spectral:
            return "spectral";
        case hdf4_dimension_vertical:
            return "vertical";
        case hdf4_dimension_independent:
            return "independent";
        case hdf4_dimension_string:
            return "string";
        case hdf4_dimension_scalar:
            return "scalar";
        default:
            assert(0);
            exit(1);
    }
}

static int parse_dimension_type(const char *str, hdf4_dimension_type *dimension_type)
{
    if (strcmp(str, get_dimension_type_name(hdf4_dimension_time)) == 0)
    {
        *dimension_type = hdf4_dimension_time;
    }
    else if (strcmp(str, get_dimension_type_name(hdf4_dimension_latitude)) == 0)
    {
        *dimension_type = hdf4_dimension_latitude;
    }
    else if (strcmp(str, get_dimension_type_name(hdf4_dimension_longitude)) == 0)
    {
        *dimension_type = hdf4_dimension_longitude;
    }
    else if (strcmp(str, get_dimension_type_name(hdf4_dimension_spectral)) == 0)
    {
        *dimension_type = hdf4_dimension_spectral;
    }
    else if (strcmp(str, get_dimension_type_name(hdf4_dimension_vertical)) == 0)
    {
        *dimension_type = hdf4_dimension_vertical;
    }
    else if (strcmp(str, get_dimension_type_name(hdf4_dimension_independent)) == 0)
    {
        *dimension_type = hdf4_dimension_independent;
    }
    else if (strcmp(str, get_dimension_type_name(hdf4_dimension_string)) == 0)
    {
        *dimension_type = hdf4_dimension_string;
    }
    else if (strcmp(str, get_dimension_type_name(hdf4_dimension_scalar)) == 0)
    {
        *dimension_type = hdf4_dimension_scalar;
    }
    else
    {
        harp_set_error(HARP_ERROR_IMPORT, "unsupported dimension '%s'", str);
        return -1;
    }

    return 0;
}

static int get_harp_dimension_type(hdf4_dimension_type hdf4_dim_type, harp_dimension_type *harp_dim_type)
{
    switch (hdf4_dim_type)
    {
        case hdf4_dimension_time:
            *harp_dim_type = harp_dimension_time;
            break;
        case hdf4_dimension_latitude:
            *harp_dim_type = harp_dimension_latitude;
            break;
        case hdf4_dimension_longitude:
            *harp_dim_type = harp_dimension_longitude;
            break;
        case hdf4_dimension_spectral:
            *harp_dim_type = harp_dimension_spectral;
            break;
        case hdf4_dimension_vertical:
            *harp_dim_type = harp_dimension_vertical;
            break;
        case hdf4_dimension_independent:
            *harp_dim_type = harp_dimension_independent;
            break;
        default:
            harp_set_error(HARP_ERROR_IMPORT, "unsupported dimension type '%s'",
                           get_dimension_type_name(hdf4_dim_type));
            return -1;
    }

    return 0;
}

static hdf4_dimension_type get_hdf4_dimension_type(harp_dimension_type dimension_type)
{
    switch (dimension_type)
    {
        case harp_dimension_independent:
            return hdf4_dimension_independent;
        case harp_dimension_time:
            return hdf4_dimension_time;
        case harp_dimension_latitude:
            return hdf4_dimension_latitude;
        case harp_dimension_longitude:
            return hdf4_dimension_longitude;
        case harp_dimension_spectral:
            return hdf4_dimension_spectral;
        case harp_dimension_vertical:
            return hdf4_dimension_vertical;
        default:
            assert(0);
            exit(1);
    }
}

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
            harp_set_error(HARP_ERROR_IMPORT, "unsupported data type");
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
        harp_set_error(HARP_ERROR_IMPORT, "attribute '%s' has invalid type", name);
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
        harp_set_error(HARP_ERROR_IMPORT, "attribute '%s' has invalid format", name);
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
            harp_set_error(HARP_ERROR_IMPORT, "attribute '%s' has invalid type", name);
            return -1;
    }

    if (result != 0)
    {
        harp_set_error(HARP_ERROR_HDF4, NULL);
        return -1;
    }

    return 0;
}

static int read_dimensions(int32 sds_id, int *num_dimensions, hdf4_dimension_type *dimension_type)
{
    int32_t index;
    char *cursor;
    char *dims;

    index = SDfindattr(sds_id, "dims");
    if (index < 0)
    {
        harp_set_error(HARP_ERROR_IMPORT, "dimension list not found");
        return -1;
    }

    if (read_string_attribute(sds_id, index, &dims) != 0)
    {
        return -1;
    }

    cursor = dims;
    *num_dimensions = 0;
    while (*cursor != '\0' && *num_dimensions < MAX_HDF4_VAR_DIMS)
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

        if (parse_dimension_type(mark, &dimension_type[*num_dimensions]) != 0)
        {
            free(dims);
            return -1;
        }

        (*num_dimensions)++;
    }

    if (*num_dimensions == 0)
    {
        harp_set_error(HARP_ERROR_IMPORT, "empty dimension list");
        return -1;
    }

    if (*num_dimensions == MAX_HDF4_VAR_DIMS && *cursor != '\0')
    {
        harp_set_error(HARP_ERROR_IMPORT, "too many dimensions in dimension list");
        free(dims);
        return -1;
    }

    if (*(cursor - 1) == '\0')
    {
        harp_set_error(HARP_ERROR_IMPORT, "trailing ',' in dimension list");
        free(dims);
        return -1;
    }

    free(dims);
    return 0;
}

static int read_variable(harp_product *product, int32 sds_id)
{
    char hdf4_name[MAX_HDF4_NAME_LENGTH + 1];
    int32 hdf4_dimension[MAX_HDF4_VAR_DIMS];
    int32 hdf4_start[MAX_HDF4_VAR_DIMS] = { 0 };
    int32 hdf4_data_type;
    int32 hdf4_num_dimensions;
    int32 hdf4_dont_care;
    int32 hdf4_index;
    int dims_num_dimensions;
    hdf4_dimension_type dims_dimension_type[MAX_HDF4_VAR_DIMS];
    harp_variable *variable;
    harp_dimension_type dimension_type[HARP_MAX_NUM_DIMS];
    long dimension[HARP_MAX_NUM_DIMS];
    harp_data_type data_type;
    int num_dimensions;
    long i;

    if (SDgetinfo(sds_id, hdf4_name, &hdf4_num_dimensions, hdf4_dimension, &hdf4_data_type, &hdf4_dont_care) != 0)
    {
        harp_set_error(HARP_ERROR_HDF4, NULL);
        return -1;
    }
    assert(hdf4_num_dimensions > 0);

    /* Determine HARP data type. */
    if (get_harp_type(hdf4_data_type, &data_type) != 0)
    {
        harp_add_error_message(" (dataset '%s')", hdf4_name);
        return -1;
    }

    /* Determine HARP number of dimensions, dimension types, and dimension lengths. */
    if (read_dimensions(sds_id, &dims_num_dimensions, dims_dimension_type) != 0)
    {
        harp_add_error_message(" (dataset '%s')", hdf4_name);
        return -1;
    }

    if (hdf4_num_dimensions != dims_num_dimensions)
    {
        harp_set_error(HARP_ERROR_IMPORT, "dataset '%s' has %d dimensions; expected %d", hdf4_name,
                       hdf4_num_dimensions, dims_num_dimensions);
        return -1;
    }

    num_dimensions = hdf4_num_dimensions;

    if (data_type == harp_type_string)
    {
        /* HARP represents scalars in HDF4 by adding an additional dimension of type scalar and length 1. Therefore, any
         * dataset of type string will have at least two dimensions, one scalar dimension and one string dimension.
         */
        if (hdf4_num_dimensions < 2)
        {
            harp_set_error(HARP_ERROR_IMPORT, "dataset '%s' of type '%s' has %d dimensions; expected >= 2", hdf4_name,
                           harp_get_data_type_name(harp_type_string), hdf4_num_dimensions);
            return -1;
        }

        /* Last dimension should be of type string. */
        if (dims_dimension_type[hdf4_num_dimensions - 1] != hdf4_dimension_string)
        {
            harp_set_error(HARP_ERROR_IMPORT, "inner-most dimension of dataset '%s' is of type '%s'; expected '%s'",
                           hdf4_name, get_dimension_type_name(dims_dimension_type[hdf4_num_dimensions - 1]),
                           get_dimension_type_name(hdf4_dimension_string));
            return -1;
        }

        num_dimensions--;
    }

    if (dims_dimension_type[0] == hdf4_dimension_scalar)
    {
        if (num_dimensions != 1)
        {
            harp_set_error(HARP_ERROR_IMPORT, "dataset '%s' has %d dimensions; expected %d", hdf4_name,
                           hdf4_num_dimensions, (data_type == harp_type_string ? 2 : 1));
            return -1;
        }

        if (hdf4_dimension[0] != 1)
        {
            harp_set_error(HARP_ERROR_IMPORT, "dataset '%s' has scalar dimension of length %d; expected 1", hdf4_name,
                           hdf4_dimension[0]);
            return -1;
        }

        num_dimensions = 0;
    }

    if (num_dimensions > HARP_MAX_NUM_DIMS)
    {
        harp_set_error(HARP_ERROR_IMPORT, "dataset '%s' has too many dimensions", hdf4_name);
        return -1;
    }

    for (i = 0; i < num_dimensions; i++)
    {
        if (get_harp_dimension_type(dims_dimension_type[i], &dimension_type[i]) != 0)
        {
            harp_add_error_message(" (dataset '%s')", hdf4_name);
            return -1;
        }
    }

    for (i = 0; i < num_dimensions; i++)
    {
        dimension[i] = (long)hdf4_dimension[i];
    }

    /* Create HARP variable. */
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
    if (data_type == harp_type_string)
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
        if (strcmp(variable->unit, "1") == 0)
        {
            /* convert "1" to "" */
            variable->unit[0] = '\0';
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
            harp_set_error(HARP_ERROR_IMPORT, "attribute 'valid_min' of dataset '%s' has invalid type", hdf4_name);
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
            harp_set_error(HARP_ERROR_IMPORT, "attribute 'valid_max' of dataset '%s' has invalid type", hdf4_name);
            return -1;
        }
    }

    if (data_type == harp_type_int8)
    {
        hdf4_index = SDfindattr(sds_id, "flag_meanings");
        if (hdf4_index >= 0)
        {
            char *flag_meanings;

            if (read_string_attribute(sds_id, hdf4_index, &flag_meanings) != 0)
            {
                return -1;
            }
            if (harp_variable_set_enumeration_values_using_flag_meanings(variable, flag_meanings) != 0)
            {
                free(flag_meanings);
                return -1;
            }
            free(flag_meanings);
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
                    harp_set_error(HARP_ERROR_FILE_OPEN, "unsupported HARP format version %d.%d", major, minor);
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

int harp_import_hdf4(const char *filename, harp_product **product)
{
    harp_product *new_product;
    int32 sd_id;

    sd_id = SDstart(filename, DFACC_READ);
    if (sd_id == -1)
    {
        harp_set_error(HARP_ERROR_HDF4, NULL);
        harp_add_error_message(" (%s)", filename);
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
        harp_add_error_message(" (%s)", filename);
        harp_product_delete(new_product);
        SDend(sd_id);
        return -1;
    }

    SDend(sd_id);

    *product = new_product;
    return 0;
}

static int update_dimensions_with_variable(long dimension[], int32 sds_id)
{
    char hdf4_name[MAX_HDF4_NAME_LENGTH + 1];
    int32 hdf4_dimension[MAX_HDF4_VAR_DIMS];
    int32 hdf4_data_type;
    int32 hdf4_num_dimensions;
    int32 hdf4_dont_care;
    int dims_num_dimensions;
    hdf4_dimension_type dims_dimension_type[MAX_HDF4_VAR_DIMS];
    long i;

    if (SDgetinfo(sds_id, hdf4_name, &hdf4_num_dimensions, hdf4_dimension, &hdf4_data_type, &hdf4_dont_care) != 0)
    {
        harp_set_error(HARP_ERROR_HDF4, NULL);
        return -1;
    }
    assert(hdf4_num_dimensions > 0);

    /* Determine HARP number of dimensions, dimension types, and dimension lengths. */
    if (read_dimensions(sds_id, &dims_num_dimensions, dims_dimension_type) != 0)
    {
        harp_add_error_message(" (dataset '%s')", hdf4_name);
        return -1;
    }

    if (hdf4_num_dimensions != dims_num_dimensions)
    {
        harp_set_error(HARP_ERROR_IMPORT, "dataset '%s' has %d dimensions; expected %d", hdf4_name,
                       hdf4_num_dimensions, dims_num_dimensions);
        return -1;
    }

    for (i = 0; i < dims_num_dimensions; i++)
    {
        switch (dims_dimension_type[i])
        {
            case hdf4_dimension_time:
                dimension[harp_dimension_time] = hdf4_dimension[i];
                break;
            case hdf4_dimension_latitude:
                dimension[harp_dimension_latitude] = hdf4_dimension[i];
                break;
            case hdf4_dimension_longitude:
                dimension[harp_dimension_longitude] = hdf4_dimension[i];
                break;
            case hdf4_dimension_vertical:
                dimension[harp_dimension_vertical] = hdf4_dimension[i];
                break;
            case hdf4_dimension_spectral:
                dimension[harp_dimension_spectral] = hdf4_dimension[i];
                break;
            case hdf4_dimension_independent:
            case hdf4_dimension_string:
            case hdf4_dimension_scalar:
                /* ignore */
                break;
        }
    }
    return 0;
}

int harp_import_global_attributes_hdf4(const char *filename, double *datetime_start, double *datetime_stop,
                                       long dimension[], char **source_product)
{
    char *attr_source_product = NULL;
    harp_scalar attr_datetime_start;
    harp_scalar attr_datetime_stop;
    harp_data_type attr_data_type;
    long attr_dimension[HARP_NUM_DIM_TYPES];
    int32 hdf4_index;
    int32 sd_id;
    int i;

    if (filename == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "filename is NULL (%s:%u)", __FILE__, __LINE__);
        return -1;
    }

    sd_id = SDstart(filename, DFACC_READ);
    if (sd_id == -1)
    {
        harp_set_error(HARP_ERROR_HDF4, NULL);
        harp_add_error_message(" (%s)", filename);
        return -1;
    }

    if (verify_product(sd_id) != 0)
    {
        SDend(sd_id);
        return -1;
    }

    if (datetime_start != NULL)
    {
        hdf4_index = SDfindattr(sd_id, "datetime_start");
        if (hdf4_index >= 0)
        {
            if (read_numeric_attribute(sd_id, hdf4_index, &attr_data_type, &attr_datetime_start) != 0)
            {
                SDend(sd_id);
                return -1;
            }

            if (attr_data_type != harp_type_double)
            {
                harp_set_error(HARP_ERROR_IMPORT, "attribute 'datetime_start' has invalid type");
                SDend(sd_id);
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
        hdf4_index = SDfindattr(sd_id, "datetime_stop");
        if (hdf4_index >= 0)
        {
            if (read_numeric_attribute(sd_id, hdf4_index, &attr_data_type, &attr_datetime_stop) != 0)
            {
                SDend(sd_id);
                return -1;
            }

            if (attr_data_type != harp_type_double)
            {
                harp_set_error(HARP_ERROR_IMPORT, "attribute 'datetime_stop' has invalid type");
                SDend(sd_id);
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
        int32 hdf4_num_attributes;
        int32 num_sds;

        for (i = 0; i < HARP_NUM_DIM_TYPES; i++)
        {
            attr_dimension[i] = -1;
        }

        if (SDfileinfo(sd_id, &num_sds, &hdf4_num_attributes) != 0)
        {
            harp_set_error(HARP_ERROR_HDF4, NULL);
            SDend(sd_id);
            return -1;
        }

        for (i = 0; i < num_sds; i++)
        {
            int32 sds_id;

            sds_id = SDselect(sd_id, i);
            if (sds_id == -1)
            {
                harp_set_error(HARP_ERROR_HDF4, NULL);
                return -1;
            }

            if (update_dimensions_with_variable(attr_dimension, sds_id) != 0)
            {
                SDendaccess(sds_id);
                return -1;
            }

            SDendaccess(sds_id);
        }
    }

    if (source_product != NULL)
    {
        hdf4_index = SDfindattr(sd_id, "source_product");
        if (hdf4_index >= 0)
        {
            if (read_string_attribute(sd_id, hdf4_index, &attr_source_product) != 0)
            {
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

    SDend(sd_id);

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

static int write_string_attribute(int32 obj_id, const char *name, const char *data)
{
    if (SDsetattr(obj_id, name, DFNT_CHAR, (int32_t)strlen(data), data) != 0)
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

static int write_dimensions(int32 sds_id, int num_dimensions, const hdf4_dimension_type *dimension_type)
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
        length += (int)strlen(get_dimension_type_name(dimension_type[i]));

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
        strcat(dimension_str, get_dimension_type_name(dimension_type[i]));

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
    hdf4_dimension_type dimension_type[MAX_HDF4_VAR_DIMS];
    int32 dimension[MAX_HDF4_VAR_DIMS];
    int32 start[MAX_HDF4_VAR_DIMS] = { 0 };
    int32 sds_id;
    int32 num_dimensions;
    int i;

    if (variable->num_dimensions == 0)
    {
        dimension_type[0] = hdf4_dimension_scalar;
        dimension[0] = 1;
        num_dimensions = 1;
    }
    else
    {
        for (i = 0; i < variable->num_dimensions; i++)
        {
            dimension_type[i] = get_hdf4_dimension_type(variable->dimension_type[i]);
            dimension[i] = variable->dimension[i];
        }
        num_dimensions = variable->num_dimensions;
    }

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
        dimension_type[num_dimensions] = hdf4_dimension_string;
        dimension[num_dimensions] = length;
        num_dimensions++;

        sds_id = SDcreate(sd_id, variable->name, DFNT_CHAR, num_dimensions, dimension);
        if (sds_id == -1)
        {
            harp_set_error(HARP_ERROR_HDF4, NULL);
            free(buffer);
            return -1;
        }

        if (SDwritedata(sds_id, start, NULL, dimension, buffer) != 0)
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
        sds_id = SDcreate(sd_id, variable->name, get_hdf4_type(variable->data_type), num_dimensions, dimension);
        if (sds_id == -1)
        {
            harp_set_error(HARP_ERROR_HDF4, NULL);
            return -1;
        }

        if (SDwritedata(sds_id, start, NULL, dimension, variable->data.ptr) != 0)
        {
            harp_set_error(HARP_ERROR_HDF4, NULL);
            SDendaccess(sds_id);
            return -1;
        }
    }

    /* Write dimensions. */
    if (write_dimensions(sds_id, num_dimensions, dimension_type) != 0)
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

    if (variable->unit != NULL)
    {
        const char *unit = variable->unit[0] == '\0' ? "1" : variable->unit;    /* convert "" to "1" */

        if (write_string_attribute(sds_id, "units", unit) != 0)
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

    if (variable->num_enum_values > 0 && variable->data_type == harp_type_int8)
    {
        char *attribute_value;

        if (harp_variable_get_flag_values_string(variable, &attribute_value) != 0)
        {
            return -1;
        }
        if (write_string_attribute(sds_id, "flag_values", attribute_value) != 0)
        {
            free(attribute_value);
            return -1;
        }
        free(attribute_value);

        if (harp_variable_get_flag_meanings_string(variable, &attribute_value) != 0)
        {
            return -1;
        }
        if (write_string_attribute(sds_id, "flag_meanings", attribute_value) != 0)
        {
            free(attribute_value);
            return -1;
        }
        free(attribute_value);
    }

    SDendaccess(sds_id);

    return 0;
}

static int write_product(const harp_product *product, int32 sd_id)
{
    harp_scalar datetime_start;
    harp_scalar datetime_stop;
    int i;

    /* Write file convention. */
    if (write_string_attribute(sd_id, "Conventions", HARP_CONVENTION) != 0)
    {
        return -1;
    }

    /* Write attributes. */
    if (harp_product_get_datetime_range(product, &datetime_start.double_data, &datetime_stop.double_data) == 0)
    {
        if (write_numeric_attribute(sd_id, "datetime_start", harp_type_double, datetime_start) != 0)
        {
            return -1;
        }

        if (write_numeric_attribute(sd_id, "datetime_stop", harp_type_double, datetime_stop) != 0)
        {
            return -1;
        }
    }

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
        harp_add_error_message(" (%s)", filename);
        return -1;
    }

    if (write_product(product, sd_id) != 0)
    {
        harp_add_error_message(" (%s)", filename);
        SDend(sd_id);
        return -1;
    }

    if (SDend(sd_id) != 0)
    {
        harp_set_error(HARP_ERROR_HDF4, NULL);
        harp_add_error_message(" (%s)", filename);
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
