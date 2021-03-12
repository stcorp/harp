/*
 * Copyright (C) 2015-2021 S[&]T, The Netherlands.
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
#include <string.h>

#include "hdf5.h"
#include "hdf5_hl.h"

/* String value used in netCDF-4 files as the NAME attribute for dimension scales without coordinate variables. This
 * #define statement was copied verbatim from netcdf.h and should be kept in sync with future updates of the netCDF-4
 * library.
 */
#define DIM_WITHOUT_VARIABLE "This is a netCDF dimension but not a netCDF variable."

#define DIM_WITH_VARIABLE "This is a netCDF dimension that is also a netCDF variable."

/* Attribute name used in netCDF-4 files to mark a netCDF-4 file as a netCDF classic file. This #define statement was
 * copied verbatim from nc4internal.h and should be kept in sync with future updates of the netCDF-4 library.
 */
#define NC3_STRICT_ATT_NAME "_nc3_strict"

/* Attribute name used in netCDF-4 files to re-order dimensions. If this attribute is present on a dimension scale
 * dataset, its value is used as the (0-based) netCDF dimension id. This #define statement was copied verbatim from
 * nc4internal.h and should be kept in sync with future updates of the netCDF-4 library.
 */
#define NC_DIMID_ATT_NAME "_Netcdf4Dimid"

/* List of shared dimensions. */
typedef struct hdf5_dimensions_struct
{
    int num_dimensions;
    harp_dimension_type *type;
    long *length;
    hid_t *dataset_id;
} hdf5_dimensions;

/* A unique identifier for HDF5 objects based on the corresponding members of the H5O_info_t struct (defined in
 * H5Opublic.h). See also H5Oget_info().
 */
typedef struct hdf5_object_id_struct
{
    unsigned long fileno;
    haddr_t addr;
} hdf5_object_id;

/* List of HDF5 identifiers and sizes for each physical dimension.
 * The validity flag is needed because there is no obvious way to represent an uninitialized hdf5_object_id,
 * since in principle all combinations of fileno and addr could be valid.
 */
typedef struct hdf5_dimension_ids_struct
{
    int is_valid[HARP_NUM_DIM_TYPES];
    hdf5_object_id object_id[HARP_NUM_DIM_TYPES];
    long length[HARP_NUM_DIM_TYPES];
} hdf5_dimension_ids;

static void dimensions_init(hdf5_dimensions *dimensions)
{
    dimensions->num_dimensions = 0;
    dimensions->type = NULL;
    dimensions->length = NULL;
    dimensions->dataset_id = NULL;
}

static void dimensions_done(hdf5_dimensions *dimensions)
{
    if (dimensions->length != NULL)
    {
        free(dimensions->length);
    }
    if (dimensions->type != NULL)
    {
        free(dimensions->type);
    }
    if (dimensions->dataset_id != NULL)
    {
        int i;

        for (i = 0; i < dimensions->num_dimensions; i++)
        {
            if (dimensions->dataset_id[i] >= 0)
            {
                H5Dclose(dimensions->dataset_id[i]);
            }
        }

        free(dimensions->dataset_id);
    }
}

/* Returns the index of dimension scale matching the specified type (or the specified length for independent
 * dimensions). Returns -1 if no matching dimension scale can be found.
 */
static int dimensions_find(const hdf5_dimensions *dimensions, harp_dimension_type type, long length)
{
    int i;

    if (type == harp_dimension_independent)
    {
        /* Find independent dimensions by length. */
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
        /* Find by dimension type. */
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

/* Returns the index of the new dimension scale on success, -1 otherwise. */
static int dimensions_add(hdf5_dimensions *dimensions, harp_dimension_type type, long length, hid_t dataset_id)
{
    int index;

    index = dimensions_find(dimensions, type, length);
    if (index >= 0)
    {
        if (dimensions->length[index] != length)
        {
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "duplicate dimensions with name '%s' and different sizes "
                           "'%ld' '%ld'", harp_get_dimension_type_name(type), dimensions->length[index], length);
            return -1;
        }

        return index;
    }

    /* Dimension scale does not exist yes; add it. */
    if (dimensions->num_dimensions % BLOCK_SIZE == 0)
    {
        long *new_length;
        harp_dimension_type *new_type;
        hid_t *new_dataset_id;

        new_length = realloc(dimensions->length, (dimensions->num_dimensions + BLOCK_SIZE) * sizeof(long));
        if (new_length == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           (dimensions->num_dimensions + BLOCK_SIZE) * sizeof(long), __FILE__, __LINE__);
            return -1;
        }
        dimensions->length = new_length;

        new_type = realloc(dimensions->type, (dimensions->num_dimensions + BLOCK_SIZE) * sizeof(harp_dimension_type));
        if (new_type == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           (dimensions->num_dimensions + BLOCK_SIZE) * sizeof(harp_dimension_type), __FILE__, __LINE__);
            return -1;
        }
        dimensions->type = new_type;

        new_dataset_id = realloc(dimensions->dataset_id, (dimensions->num_dimensions + BLOCK_SIZE) * sizeof(hid_t));
        if (new_dataset_id == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           (dimensions->num_dimensions + BLOCK_SIZE) * sizeof(hid_t), __FILE__, __LINE__);
            return -1;
        }
        dimensions->dataset_id = new_dataset_id;
    }
    dimensions->type[dimensions->num_dimensions] = type;
    dimensions->length[dimensions->num_dimensions] = length;
    dimensions->dataset_id[dimensions->num_dimensions] = dataset_id;
    dimensions->num_dimensions++;

    return dimensions->num_dimensions - 1;
}

static int get_harp_type(hid_t datatype_id, harp_data_type *data_type)
{
    switch (H5Tget_class(datatype_id))
    {
        case H5T_INTEGER:
            if (H5Tget_sign(datatype_id) == H5T_SGN_2)
            {
                switch (H5Tget_size(datatype_id))
                {
                    case 1:
                        *data_type = harp_type_int8;
                        return 0;
                    case 2:
                        *data_type = harp_type_int16;
                        return 0;
                    case 4:
                        *data_type = harp_type_int32;
                        return 0;
                    default:
                        /* Intentional fall through to the end of the switch statement. */
                        break;
                }
            }
            /* Intentional fall through to the end of the switch statement. */
            break;
        case H5T_FLOAT:
            {
                hid_t native_type;

                native_type = H5Tget_native_type(datatype_id, H5T_DIR_ASCEND);
                if (native_type < 0)
                {
                    harp_set_error(HARP_ERROR_HDF5, NULL);
                    return -1;
                }

                if (H5Tequal(native_type, H5T_NATIVE_FLOAT))
                {
                    *data_type = harp_type_float;
                    H5Tclose(native_type);
                    return 0;
                }

                if (H5Tequal(native_type, H5T_NATIVE_DOUBLE))
                {
                    *data_type = harp_type_double;
                    H5Tclose(native_type);
                    return 0;
                }

                H5Tclose(native_type);
            }
            /* Intentional fall through to the end of the switch statement. */
            break;
        case H5T_STRING:
            *data_type = harp_type_string;
            return 0;
        default:
            /* Intentional fall through to the end of the switch statement. */
            break;
    }

    harp_set_error(HARP_ERROR_IMPORT, "unsupported data type");
    return -1;
}

static hid_t get_hdf5_type(harp_data_type data_type)
{
    switch (data_type)
    {
        case harp_type_int8:
            return H5T_NATIVE_SCHAR;
        case harp_type_int16:
            return H5T_NATIVE_SHORT;
        case harp_type_int32:
            return H5T_NATIVE_INT;
        case harp_type_float:
            return H5T_NATIVE_FLOAT;
        case harp_type_double:
            return H5T_NATIVE_DOUBLE;
        case harp_type_string:
            return H5T_C_S1;
        default:
            assert(0);
            exit(1);
    }
}

static char *get_hdf5_variable_name(const harp_product *product, const harp_variable *variable)
{
    char *name = NULL;
    int i;

    for (i = 0; i < HARP_NUM_DIM_TYPES; i++)
    {
        harp_dimension_type dimension_type = (harp_dimension_type)i;

        if (product->dimension[i] > 0 && strcmp(variable->name, harp_get_dimension_type_name(dimension_type)) == 0 &&
            !(variable->num_dimensions == 1 && *variable->dimension_type == dimension_type))
        {
            /* we have a variable with the same name as a dimension but which is not an axis variable */
            /* -> prepend _nc4_non_coord_ to the name as is also done by the netcdf4 library */
            name = malloc(strlen(variable->name) + 16);
            if (name == NULL)
            {
                harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                               strlen(variable->name) + 16, __FILE__, __LINE__);
                return NULL;
            }
            strcpy(name, "_nc4_non_coord_");
            strcpy(&name[15], variable->name);
            return name;
        }
    }

    name = strdup(variable->name);
    if (name == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        return NULL;
    }

    return name;
}

static int get_link_iteration_index_type(hid_t group_id, H5_index_t * index_type)
{
    hid_t gcpl_id;
    unsigned int crt_order_flags;

    gcpl_id = H5Gget_create_plist(group_id);
    if (gcpl_id < 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
        return -1;
    }

    if (H5Pget_link_creation_order(gcpl_id, &crt_order_flags) < 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
        return -1;
    }

    H5Pclose(gcpl_id);

    if (crt_order_flags & H5P_CRT_ORDER_TRACKED)
    {
        *index_type = H5_INDEX_CRT_ORDER;
    }
    else
    {
        *index_type = H5_INDEX_NAME;
    }

    return 0;
}

static int set_compression(hid_t plist_id, harp_variable *variable)
{
    int level = harp_get_option_hdf5_compression();

    if (level > 0 && variable->num_dimensions > 0)
    {
        long max_length = 4294967295;
        hsize_t dimension[HARP_MAX_NUM_DIMS];
        int i;

        /* set chunk configuration (we need chunking to enable compression) */
        /* we want to use the largest block possible while staying within the 2^32-1 elements per chunk limit */
        for (i = 0; i < variable->num_dimensions; i++)
        {
            dimension[i] = variable->dimension[i];
        }
        if (variable->num_elements > max_length)
        {
            long num_elements = variable->num_elements;
            int i = 0;

            while (i < variable->num_dimensions - 1)
            {
                num_elements /= (long)dimension[i];
                if (num_elements <= max_length)
                {
                    dimension[i] = max_length / num_elements;
                    num_elements *= (long)dimension[i];
                    break;
                }
                dimension[i] = 1;
                i++;
            }
            if (num_elements > max_length)
            {
                dimension[i] = max_length;
            }
        }
        if (H5Pset_chunk(plist_id, variable->num_dimensions, dimension) < 0)
        {
            harp_set_error(HARP_ERROR_HDF5, NULL);
            return -1;
        }
        if (H5Pset_deflate(plist_id, level) < 0)
        {
            harp_set_error(HARP_ERROR_HDF5, NULL);
            return -1;
        }
    }
    return 0;
}

static int read_string_attribute(hid_t obj_id, const char *name, char **data)
{
    char *str;
    hid_t attr_id;
    hid_t data_type_id;
    hid_t space_id;
    hsize_t size;

    attr_id = H5Aopen_by_name(obj_id, ".", name, H5P_DEFAULT, H5P_DEFAULT);
    if (attr_id < 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
        return -1;
    }

    space_id = H5Aget_space(attr_id);
    if (space_id < 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
        H5Aclose(attr_id);
        return -1;
    }

    if (H5Sget_simple_extent_type(space_id) == H5S_NULL)
    {
        /* this is an empty string */
        H5Sclose(space_id);
        H5Aclose(attr_id);
        *data = strdup("");
        if (*data == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                           __LINE__);
            return -1;
        }
        return 0;
    }

    if (H5Sis_simple(space_id) <= 0 || H5Sget_simple_extent_type(space_id) != H5S_SCALAR)
    {
        harp_set_error(HARP_ERROR_IMPORT, "attribute '%s' has invalid format", name);
        H5Sclose(space_id);
        H5Aclose(attr_id);
        return -1;
    }
    H5Sclose(space_id);

    data_type_id = H5Aget_type(attr_id);
    if (data_type_id < 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
        H5Aclose(attr_id);
        return -1;
    }

    if (H5Tget_class(data_type_id) != H5T_STRING || H5Tis_variable_str(data_type_id))
    {
        harp_set_error(HARP_ERROR_IMPORT, "attribute '%s' has invalid type", name);
        H5Tclose(data_type_id);
        H5Aclose(attr_id);
        return -1;
    }

    size = H5Tget_size(data_type_id);
    if (size == 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
        H5Tclose(data_type_id);
        H5Aclose(attr_id);
        return -1;
    }

    str = malloc((size + 1) * sizeof(char));
    if (str == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       (size + 1) * sizeof(char), __FILE__, __LINE__);
        H5Tclose(data_type_id);
        H5Aclose(attr_id);
        return -1;
    }
    str[size] = '\0';

    if (H5Aread(attr_id, data_type_id, str) < 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
        free(str);
        H5Tclose(data_type_id);
        H5Aclose(attr_id);
        return -1;
    }
    H5Tclose(data_type_id);
    H5Aclose(attr_id);

    *data = str;
    return 0;
}

static int read_numeric_attribute(hid_t obj_id, const char *name, harp_data_type *data_type, harp_scalar *data)
{
    hid_t attr_id;
    hid_t data_type_id;
    hid_t space_id;
    herr_t result;

    attr_id = H5Aopen_by_name(obj_id, ".", name, H5P_DEFAULT, H5P_DEFAULT);
    if (attr_id < 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
        return -1;
    }

    data_type_id = H5Aget_type(attr_id);
    if (data_type_id < 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
        H5Aclose(attr_id);
        return -1;
    }

    if (get_harp_type(data_type_id, data_type) != 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
        H5Tclose(data_type_id);
        H5Aclose(attr_id);
        return -1;
    }
    H5Tclose(data_type_id);

    space_id = H5Aget_space(attr_id);
    if (space_id < 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
        H5Aclose(attr_id);
        return -1;
    }

    if (H5Sis_simple(space_id) <= 0)
    {
        harp_set_error(HARP_ERROR_IMPORT, "attribute '%s' has invalid format", name);
        H5Sclose(space_id);
        H5Aclose(attr_id);
        return -1;
    }
    if (H5Sget_simple_extent_type(space_id) != H5S_SCALAR)
    {
        hssize_t npoints;

        npoints = H5Sget_simple_extent_npoints(space_id);
        if (npoints != 1)
        {
            harp_set_error(HARP_ERROR_IMPORT, "attribute '%s' has invalid dimensions", name);
            H5Sclose(space_id);
            H5Aclose(attr_id);
            return -1;
        }
    }
    H5Sclose(space_id);

    switch (*data_type)
    {
        case harp_type_int8:
            result = H5Aread(attr_id, H5T_NATIVE_SCHAR, &data->int8_data);
            break;
        case harp_type_int16:
            result = H5Aread(attr_id, H5T_NATIVE_SHORT, &data->int16_data);
            break;
        case harp_type_int32:
            result = H5Aread(attr_id, H5T_NATIVE_INT, &data->int32_data);
            break;
        case harp_type_float:
            result = H5Aread(attr_id, H5T_NATIVE_FLOAT, &data->float_data);
            break;
        case harp_type_double:
            result = H5Aread(attr_id, H5T_NATIVE_DOUBLE, &data->double_data);
            break;
        default:
            harp_set_error(HARP_ERROR_IMPORT, "attribute '%s' has invalid type", name);
            H5Aclose(attr_id);
            return -1;
    }

    if (result < 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
        H5Aclose(attr_id);
        return -1;
    }

    H5Aclose(attr_id);
    return 0;
}

static int read_variable_data_type(hid_t dataset_id, harp_data_type *data_type)
{
    hid_t data_type_id;

    data_type_id = H5Dget_type(dataset_id);
    if (data_type_id < 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
        return -1;
    }

    if (get_harp_type(data_type_id, data_type) != 0)
    {
        H5Tclose(data_type_id);
        return -1;
    }

    H5Tclose(data_type_id);

    return 0;
}

/* don't use -1 on error, otherwise the HDF5 library starts printing error messages to the console */
static herr_t hdf5_read_dimension_scale_func(hid_t dataset_id, unsigned dim, hid_t dimension_scale_id, void *user_data)
{
    hdf5_object_id *object_id;
    H5O_info_t object_info;

    (void)dataset_id;
    (void)dim;

    if (H5Oget_info(dimension_scale_id, &object_info) < 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
        return 1;
    }

    object_id = (hdf5_object_id *)user_data;
    object_id->fileno = object_info.fileno;
    object_id->addr = object_info.addr;

    return 0;
}

static int read_variable_dimensions(const char *variable_name, hid_t dataset_id,
                                    const hdf5_dimension_ids *dimension_ids, int *num_dimensions,
                                    harp_dimension_type *dimension_type, long *dimension)
{
    hid_t space_id;
    int hdf5_num_dimensions;
    harp_dimension_type hdf5_dimension_type[HARP_MAX_NUM_DIMS];
    hsize_t hdf5_dimension[HARP_MAX_NUM_DIMS];
    int i, j;

    space_id = H5Dget_space(dataset_id);
    if (space_id < 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
        return -1;
    }

    if (H5Sis_simple(space_id) <= 0)
    {
        harp_set_error(HARP_ERROR_IMPORT, "dataspace is complex; only simple dataspaces are supported");
        H5Sclose(space_id);
        return -1;
    }

    hdf5_num_dimensions = H5Sget_simple_extent_ndims(space_id);
    if (hdf5_num_dimensions < 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
        H5Sclose(space_id);
        return -1;
    }

    if (hdf5_num_dimensions > HARP_MAX_NUM_DIMS)
    {
        harp_set_error(HARP_ERROR_IMPORT, "dataspace has %d dimensions; expected <= %d", hdf5_num_dimensions,
                       HARP_MAX_NUM_DIMS);
        H5Sclose(space_id);
        return -1;
    }

    if (H5Sget_simple_extent_dims(space_id, hdf5_dimension, NULL) < 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
        H5Sclose(space_id);
        return -1;
    }

    H5Sclose(space_id);

    for (i = 0; i < hdf5_num_dimensions; i++)
    {
        int hdf5_num_scales;

        hdf5_num_scales = H5DSget_num_scales(dataset_id, i);
        if (hdf5_num_scales < 0)
        {
            harp_set_error(HARP_ERROR_HDF5, NULL);
            return -1;
        }

        if (hdf5_num_scales == 0)
        {
            hdf5_dimension_type[i] = harp_dimension_independent;
            if ((i == 0) && H5DSis_scale(dataset_id))
            {
                /* This variable has the same name as a dimension */
                for (j = 0; j < HARP_NUM_DIM_TYPES; j++)
                {
                    if (strcmp(harp_get_dimension_type_name(j), variable_name) == 0)
                    {
                        hdf5_dimension_type[i] = (harp_dimension_type)j;
                        break;
                    }
                }
            }
        }
        else if (hdf5_num_scales == 1)
        {
            hdf5_object_id hdf5_dimensions_id;
            int j;

            if (H5DSiterate_scales(dataset_id, i, NULL, hdf5_read_dimension_scale_func, &hdf5_dimensions_id) != 0)
            {
                return -1;
            }

            hdf5_dimension_type[i] = harp_dimension_independent;

            for (j = 0; j < HARP_NUM_DIM_TYPES; j++)
            {
                if (!dimension_ids->is_valid[j])
                {
                    continue;
                }

                if (dimension_ids->object_id[j].fileno == hdf5_dimensions_id.fileno
                    && dimension_ids->object_id[j].addr == hdf5_dimensions_id.addr)
                {
                    hdf5_dimension_type[i] = (harp_dimension_type)j;
                    break;
                }
            }
        }
        else
        {
            harp_set_error(HARP_ERROR_IMPORT, "dimension at index %d has %d attached dimension scales; expected 0 or 1",
                           i, hdf5_num_scales);
            return -1;
        }
    }

    *num_dimensions = hdf5_num_dimensions;
    for (i = 0; i < hdf5_num_dimensions; i++)
    {
        dimension_type[i] = hdf5_dimension_type[i];
        dimension[i] = (long)hdf5_dimension[i];
    }

    return 0;
}

static int read_variable(hid_t dataset_id, const char *name, const hdf5_dimension_ids *dimension_ids,
                         harp_product *product)
{
    const char *variable_name;
    harp_variable *variable;
    harp_dimension_type dimension_type[HARP_MAX_NUM_DIMS];
    long dimension[HARP_MAX_NUM_DIMS];
    harp_data_type data_type;
    int num_dimensions;
    herr_t result;

    if (read_variable_data_type(dataset_id, &data_type) != 0)
    {
        return -1;
    }

    if (read_variable_dimensions(name, dataset_id, dimension_ids, &num_dimensions, dimension_type, dimension) != 0)
    {
        return -1;
    }

    variable_name = name;
    if (strncmp(name, "_nc4_non_coord_", 15) == 0)
    {
        variable_name = &name[15];
    }
    if (harp_variable_new(variable_name, data_type, num_dimensions, dimension_type, dimension, &variable) != 0)
    {
        return -1;
    }

    if (harp_product_add_variable(product, variable) != 0)
    {
        harp_variable_delete(variable);
        return -1;
    }

    /* Read variable data. */
    if (variable->data_type == harp_type_string)
    {
        char *buffer;
        hid_t type_id;
        hsize_t type_size;
        hid_t mem_type_id;
        long i;

        type_id = H5Dget_type(dataset_id);
        if (type_id < 0)
        {
            harp_set_error(HARP_ERROR_HDF5, NULL);
            return -1;
        }

        type_size = H5Tget_size(type_id);
        if (type_size == 0)
        {
            harp_set_error(HARP_ERROR_HDF5, NULL);
            H5Tclose(type_id);
            return -1;
        }

        H5Tclose(type_id);

        mem_type_id = H5Tcopy(H5T_C_S1);
        if (mem_type_id < 0)
        {
            harp_set_error(HARP_ERROR_HDF5, NULL);
            return -1;
        }

        if (H5Tset_size(mem_type_id, type_size) < 0)
        {
            harp_set_error(HARP_ERROR_HDF5, NULL);
            H5Tclose(mem_type_id);
            return -1;
        }

        if (H5Tset_strpad(mem_type_id, H5T_STR_NULLPAD) < 0)
        {
            harp_set_error(HARP_ERROR_HDF5, NULL);
            H5Tclose(mem_type_id);
            return -1;
        }

        buffer = malloc(variable->num_elements * type_size * sizeof(char));
        if (buffer == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           variable->num_elements * type_size * sizeof(char), __FILE__, __LINE__);
            H5Tclose(mem_type_id);
            return -1;
        }

        if (H5Dread(dataset_id, mem_type_id, H5S_ALL, H5S_ALL, H5P_DEFAULT, buffer) < 0)
        {
            harp_set_error(HARP_ERROR_HDF5, NULL);
            free(buffer);
            H5Tclose(mem_type_id);
            return -1;
        }

        H5Tclose(mem_type_id);

        for (i = 0; i < variable->num_elements; i++)
        {
            char *str;

            str = malloc((type_size + 1) * sizeof(char));
            if (str == NULL)
            {
                harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                               (type_size + 1) * sizeof(char), __FILE__, __LINE__);
                free(buffer);
                return -1;
            }

            memcpy(str, &buffer[i * type_size], type_size);
            str[type_size] = '\0';
            variable->data.string_data[i] = str;
        }

        free(buffer);
    }
    else
    {
        if (H5Dread(dataset_id, get_hdf5_type(variable->data_type), H5S_ALL, H5S_ALL, H5P_DEFAULT,
                    variable->data.ptr) < 0)
        {
            harp_set_error(HARP_ERROR_HDF5, NULL);
            return -1;
        }
    }

    /* Read variable attributes. */
    result = H5Aexists(dataset_id, "description");
    if (result > 0)
    {
        if (read_string_attribute(dataset_id, "description", &variable->description) != 0)
        {
            return -1;
        }
    }
    else if (result < 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
        return -1;
    }

    result = H5Aexists(dataset_id, "units");
    if (result > 0)
    {
        if (read_string_attribute(dataset_id, "units", &variable->unit) != 0)
        {
            return -1;
        }
        if (strcmp(variable->unit, "1") == 0)
        {
            /* convert "1" to "" */
            variable->unit[0] = '\0';
        }
    }
    else if (result < 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
        return -1;
    }

    result = H5Aexists(dataset_id, "valid_min");
    if (result > 0)
    {
        harp_data_type attr_data_type;

        if (read_numeric_attribute(dataset_id, "valid_min", &attr_data_type, &variable->valid_min) != 0)
        {
            return -1;
        }

        if (attr_data_type != variable->data_type)
        {
            harp_set_error(HARP_ERROR_IMPORT, "attribute 'valid_min' of dataset '%s' has invalid type", name);
            return -1;
        }
    }
    else if (result < 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
        return -1;
    }

    result = H5Aexists(dataset_id, "valid_max");
    if (result > 0)
    {
        harp_data_type attr_data_type;

        if (read_numeric_attribute(dataset_id, "valid_max", &attr_data_type, &variable->valid_max) != 0)
        {
            return -1;
        }

        if (attr_data_type != variable->data_type)
        {
            harp_set_error(HARP_ERROR_IMPORT, "attribute 'valid_max' of dataset '%s' has invalid type", name);
            return -1;
        }
    }
    else if (result < 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
        return -1;
    }

    if (data_type == harp_type_int8)
    {
        result = H5Aexists(dataset_id, "flag_meanings");
        if (result > 0)
        {
            char *flag_meanings;

            if (read_string_attribute(dataset_id, "flag_meanings", &flag_meanings) != 0)
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

/* don't use -1 on error, otherwise the HDF5 library starts printing error messages to the console */
static herr_t hdf5_find_dimensions_func(hid_t group_id, const char *name, const H5L_info_t * info, void *user_data)
{
    H5O_info_t object_info;
    hdf5_dimension_ids *dimension_ids;
    hid_t dataset_id;
    htri_t is_dimension_scale;
    harp_dimension_type dimension_type;
    hsize_t length = 0;

    (void)info;
    dimension_ids = (hdf5_dimension_ids *)user_data;

    if (H5Oget_info_by_name(group_id, name, &object_info, H5P_DEFAULT) < 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
        return 1;
    }

    if (object_info.type != H5O_TYPE_DATASET)
    {
        return 0;
    }

    dataset_id = H5Dopen(group_id, name);
    if (dataset_id < 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
        return 1;
    }

    is_dimension_scale = H5DSis_scale(dataset_id);
    if (is_dimension_scale < 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
        H5Dclose(dataset_id);
        return 1;
    }

    if (is_dimension_scale)
    {
        hid_t space_id;
        int hdf5_num_dimensions;

        space_id = H5Dget_space(dataset_id);
        if (space_id < 0)
        {
            harp_set_error(HARP_ERROR_HDF5, NULL);
            H5Dclose(dataset_id);
            return 1;
        }

        if (H5Sis_simple(space_id) <= 0)
        {
            harp_set_error(HARP_ERROR_IMPORT, "dataspace is complex; only simple dataspaces are supported");
            H5Sclose(space_id);
            H5Dclose(dataset_id);
            return 1;
        }

        hdf5_num_dimensions = H5Sget_simple_extent_ndims(space_id);
        if (hdf5_num_dimensions < 0)
        {
            harp_set_error(HARP_ERROR_HDF5, NULL);
            H5Sclose(space_id);
            H5Dclose(dataset_id);
            return 1;
        }

        if (hdf5_num_dimensions != 1)
        {
            harp_set_error(HARP_ERROR_IMPORT, "dataspace for dimensions scale has %d dimensions; expected 1",
                           hdf5_num_dimensions);
            H5Sclose(space_id);
            H5Dclose(dataset_id);
            return 1;
        }

        if (H5Sget_simple_extent_dims(space_id, &length, NULL) < 0)
        {
            harp_set_error(HARP_ERROR_HDF5, NULL);
            H5Sclose(space_id);
            H5Dclose(dataset_id);
            return 1;
        }

        H5Sclose(space_id);
    }

    H5Dclose(dataset_id);

    if (is_dimension_scale
        && harp_parse_dimension_type(name, &dimension_type) == 0
        && dimension_type != harp_dimension_independent && !dimension_ids->is_valid[dimension_type])
    {
        dimension_ids->is_valid[dimension_type] = 1;
        dimension_ids->object_id[dimension_type].fileno = object_info.fileno;
        dimension_ids->object_id[dimension_type].addr = object_info.addr;
        dimension_ids->length[dimension_type] = (long)length;
    }

    return 0;
}

static int find_dimensions(hid_t group_id, hdf5_dimension_ids *dimension_ids)
{
    H5_index_t index_type;

    if (get_link_iteration_index_type(group_id, &index_type) != 0)
    {
        return -1;
    }

    if (H5Literate(group_id, index_type, H5_ITER_NATIVE, NULL, hdf5_find_dimensions_func, dimension_ids) != 0)
    {
        return -1;
    }

    return 0;
}

/* Additional arguments for hdf5_read_variable_func(), which is a visitor function that is called for all variables in
 * the root group via H5Literate(), see also read_variables().
 */
typedef struct hdf5_read_variable_func_args_struct
{
    hdf5_dimension_ids *dimension_ids;
    harp_product *product;
} hdf5_read_variable_func_args;

/* don't use -1 on error, otherwise the HDF5 library starts printing error messages to the console */
static herr_t hdf5_read_variable_func(hid_t group_id, const char *name, const H5L_info_t * info, void *user_data)
{
    hdf5_read_variable_func_args *args;
    H5O_info_t object_info;
    hid_t dataset_id;
    htri_t is_dimension_scale;

    (void)info;

    args = (hdf5_read_variable_func_args *)user_data;

    if (H5Oget_info_by_name(group_id, name, &object_info, H5P_DEFAULT) < 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
        return 1;
    }

    if (object_info.type != H5O_TYPE_DATASET)
    {
        /* Skip everything that is not a dataset. */
        return 0;
    }

    dataset_id = H5Dopen(group_id, name);
    if (dataset_id < 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
        return 1;
    }

    is_dimension_scale = H5DSis_scale(dataset_id);
    if (is_dimension_scale < 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
        H5Dclose(dataset_id);
        return 1;
    }

    if (is_dimension_scale)
    {
        char scale_name[255];

        if (H5DSget_scale_name(dataset_id, scale_name, sizeof(scale_name)) < 0)
        {
            harp_set_error(HARP_ERROR_HDF5, NULL);
            H5Dclose(dataset_id);
            return 1;
        }

        if (strncmp(scale_name, DIM_WITHOUT_VARIABLE, sizeof(DIM_WITHOUT_VARIABLE) - 1) == 0)
        {
            /* Skip dimension scales without a coordinate variable. */
            H5Dclose(dataset_id);
            return 0;
        }
    }

    if (read_variable(dataset_id, name, args->dimension_ids, args->product) != 0)
    {
        H5Dclose(dataset_id);
        return 1;
    }

    H5Dclose(dataset_id);

    return 0;
}

static int read_variables(hid_t group_id, hdf5_dimension_ids *dimension_ids, harp_product *product)
{
    hdf5_read_variable_func_args args;
    H5_index_t index_type;

    if (get_link_iteration_index_type(group_id, &index_type) != 0)
    {
        return -1;
    }

    args.dimension_ids = dimension_ids;
    args.product = product;

    return (H5Literate(group_id, index_type, H5_ITER_INC, NULL, hdf5_read_variable_func, &args) != 0 ? -1 : 0);
}

static int read_attributes(hid_t group_id, harp_product *product)
{
    htri_t result;

    result = H5Aexists(group_id, "source_product");
    if (result > 0)
    {
        if (read_string_attribute(group_id, "source_product", &product->source_product) != 0)
        {
            return -1;
        }
    }
    else if (result < 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
        return -1;
    }

    result = H5Aexists(group_id, "history");
    if (result > 0)
    {
        if (read_string_attribute(group_id, "history", &product->history) != 0)
        {
            return -1;
        }
    }
    else if (result < 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
        return -1;
    }

    return 0;
}

static int read_product(hid_t file_id, harp_product *product)
{
    hdf5_dimension_ids dimension_ids = { {0}, {{0, 0}}, {0} };
    hid_t root_id;

    root_id = H5Gopen(file_id, "/");
    if (root_id < 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
        return -1;
    }

    /* Find dimension scales. */
    if (find_dimensions(root_id, &dimension_ids) != 0)
    {
        H5Gclose(root_id);
        return -1;
    }

    /* Read variables. */
    if (read_variables(root_id, &dimension_ids, product) != 0)
    {
        H5Gclose(root_id);
        return -1;
    }

    /* Read attributes. */
    if (read_attributes(root_id, product) != 0)
    {
        H5Gclose(root_id);
        return -1;
    }

    H5Gclose(root_id);

    return 0;
}

static int verify_product(hid_t file_id)
{
    int result;
    char *convention_str;

    result = H5Aexists(file_id, "Conventions");
    if (result > 0)
    {
        if (read_string_attribute(file_id, "Conventions", &convention_str) == 0)
        {
            int major, minor;

            if (harp_parse_file_convention(convention_str, &major, &minor) == 0)
            {
                free(convention_str);
                if (major > HARP_FORMAT_VERSION_MAJOR
                    || (major == HARP_FORMAT_VERSION_MAJOR && minor > HARP_FORMAT_VERSION_MINOR))
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

int harp_import_hdf5(const char *filename, harp_product **product)
{
    harp_product *new_product;
    hid_t file_id;

    file_id = H5Fopen(filename, H5F_ACC_RDONLY, H5P_DEFAULT);
    if (file_id < 0)
    {
        harp_add_error_message(" (%s)", filename);
        harp_set_error(HARP_ERROR_HDF5, NULL);
        return -1;
    }

    if (verify_product(file_id) != 0)
    {
        H5Fclose(file_id);
        return -1;
    }

    if (harp_product_new(&new_product) != 0)
    {
        H5Fclose(file_id);
        return -1;
    }

    if (read_product(file_id, new_product) != 0)
    {
        harp_add_error_message(" (%s)", filename);
        harp_product_delete(new_product);
        H5Fclose(file_id);
        return -1;
    }

    *product = new_product;

    H5Fclose(file_id);

    return 0;
}

int harp_import_metadata_hdf5(const char *filename, harp_product_metadata *metadata)
{
    hdf5_dimension_ids dimension_ids = { {0}, {{0, 0}}, {0} };
    harp_scalar value;
    harp_data_type data_type;
    hid_t file_id;
    hid_t root_id;
    int result;
    int i;

    if (filename == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "filename is NULL (%s:%u)", __FILE__, __LINE__);
        return -1;
    }

    file_id = H5Fopen(filename, H5F_ACC_RDONLY, H5P_DEFAULT);
    if (file_id < 0)
    {
        harp_add_error_message(" (%s)", filename);
        harp_set_error(HARP_ERROR_HDF5, NULL);
        return -1;
    }

    if (verify_product(file_id) != 0)
    {
        H5Fclose(file_id);
        return -1;
    }

    root_id = H5Gopen(file_id, "/");
    if (root_id < 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
        H5Fclose(file_id);
        return -1;
    }

    /* datetime_start */
    result = H5Aexists(root_id, "datetime_start");
    if (result > 0)
    {
        if (read_numeric_attribute(root_id, "datetime_start", &data_type, &value) != 0)
        {
            H5Gclose(root_id);
            H5Fclose(file_id);
            return -1;
        }
        if (data_type != harp_type_double)
        {
            harp_set_error(HARP_ERROR_IMPORT, "attribute 'datetime_start' has invalid type");
            H5Gclose(root_id);
            H5Fclose(file_id);
            return -1;
        }
        metadata->datetime_start = value.double_data;
    }
    else if (result < 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
        H5Gclose(root_id);
        H5Fclose(file_id);
        return -1;
    }
    else
    {
        metadata->datetime_start = harp_mininf();
    }

    /* datetime_stop */
    result = H5Aexists(root_id, "datetime_stop");
    if (result > 0)
    {
        if (read_numeric_attribute(root_id, "datetime_stop", &data_type, &value) != 0)
        {
            H5Gclose(root_id);
            H5Fclose(file_id);
            return -1;
        }
        if (data_type != harp_type_double)
        {
            harp_set_error(HARP_ERROR_IMPORT, "attribute 'datetime_stop' has invalid type");
            H5Gclose(root_id);
            H5Fclose(file_id);
            return -1;
        }
        metadata->datetime_stop = value.double_data;
    }
    else if (result < 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
        H5Gclose(root_id);
        H5Fclose(file_id);
        return -1;
    }
    else
    {
        metadata->datetime_stop = harp_plusinf();
    }

    /* dimension */
    /* Find dimension scales. */
    if (find_dimensions(root_id, &dimension_ids) != 0)
    {
        H5Gclose(root_id);
        H5Fclose(file_id);
        return -1;
    }

    for (i = 0; i < HARP_NUM_DIM_TYPES; i++)
    {
        if (dimension_ids.is_valid[i])
        {
            metadata->dimension[i] = dimension_ids.length[i];
        }
    }

    /* format */
    metadata->format = strdup("HARP_HDF5");
    if (metadata->format == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        H5Gclose(root_id);
        H5Fclose(file_id);
        return -1;
    }

    /* source_product */
    result = H5Aexists(root_id, "source_product");
    if (result > 0)
    {
        if (read_string_attribute(root_id, "source_product", &metadata->source_product) != 0)
        {
            H5Gclose(root_id);
            H5Fclose(file_id);
            return -1;
        }
    }
    else if (result < 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
        H5Gclose(root_id);
        H5Fclose(file_id);
        return -1;
    }
    else
    {
        /* use filename if there is no source_product attribute */
        metadata->source_product = strdup(harp_basename(filename));
        if (metadata->source_product == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                           __LINE__);
            H5Gclose(root_id);
            H5Fclose(file_id);
            return -1;
        }
    }

    /* history */
    result = H5Aexists(root_id, "history");
    if (result > 0)
    {
        if (read_string_attribute(root_id, "history", &metadata->history) != 0)
        {
            H5Gclose(root_id);
            H5Fclose(file_id);
            return -1;
        }
    }
    else if (result < 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
        H5Gclose(root_id);
        H5Fclose(file_id);
        return -1;
    }

    H5Fclose(file_id);

    return 0;
}

static int write_string_attribute(hid_t obj_id, const char *name, const char *data)
{
    hid_t attr_id;
    hid_t space_id;
    hid_t data_type_id;

    data_type_id = H5Tcopy(H5T_C_S1);
    if (data_type_id < 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
        return -1;
    }

    if (strlen(data) == 0)
    {
        space_id = H5Screate(H5S_NULL);
        if (space_id < 0)
        {
            harp_set_error(HARP_ERROR_HDF5, NULL);
            H5Tclose(data_type_id);
            return -1;
        }

        attr_id = H5Acreate(obj_id, name, data_type_id, space_id, H5P_DEFAULT);
        H5Sclose(space_id);
        H5Tclose(data_type_id);
        if (attr_id < 0)
        {
            harp_set_error(HARP_ERROR_HDF5, NULL);
            return -1;
        }
        H5Aclose(attr_id);

        return 0;
    }

    if (H5Tset_size(data_type_id, strlen(data)) < 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
        H5Tclose(data_type_id);
        return -1;
    }

    space_id = H5Screate(H5S_SCALAR);
    if (space_id < 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
        H5Tclose(data_type_id);
        return -1;
    }

    attr_id = H5Acreate(obj_id, name, data_type_id, space_id, H5P_DEFAULT);
    if (attr_id < 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
        H5Sclose(space_id);
        H5Tclose(data_type_id);
        return -1;
    }
    H5Sclose(space_id);

    if (H5Awrite(attr_id, data_type_id, data) < 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
        H5Aclose(attr_id);
        H5Tclose(data_type_id);
        return -1;
    }

    H5Aclose(attr_id);
    H5Tclose(data_type_id);
    return 0;
}

static int write_numeric_attribute(hid_t obj_id, const char *name, harp_data_type data_type, harp_scalar data)
{
    hid_t attr_id;
    hid_t space_id;
    herr_t result;

    space_id = H5Screate(H5S_SCALAR);
    if (space_id < 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
        return -1;
    }

    attr_id = H5Acreate(obj_id, name, get_hdf5_type(data_type), space_id, H5P_DEFAULT);
    if (attr_id < 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
        H5Sclose(space_id);
        return -1;
    }
    H5Sclose(space_id);

    switch (data_type)
    {
        case harp_type_int8:
            result = H5Awrite(attr_id, H5T_NATIVE_SCHAR, &data.int8_data);
            break;
        case harp_type_int16:
            result = H5Awrite(attr_id, H5T_NATIVE_SHORT, &data.int16_data);
            break;
        case harp_type_int32:
            result = H5Awrite(attr_id, H5T_NATIVE_INT, &data.int32_data);
            break;
        case harp_type_float:
            result = H5Awrite(attr_id, H5T_NATIVE_FLOAT, &data.float_data);
            break;
        case harp_type_double:
            result = H5Awrite(attr_id, H5T_NATIVE_DOUBLE, &data.double_data);
            break;
        default:
            assert(0);
            exit(1);
    }

    if (result < 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
        H5Aclose(attr_id);
        return -1;
    }

    H5Aclose(attr_id);
    return 0;
}

static int write_variable(hid_t group_id, const char *name, harp_variable *variable)
{
    hsize_t dimension[HARP_MAX_NUM_DIMS];
    hid_t space_id;
    hid_t dcpl_id;
    hid_t dataset_id;
    int i;

    for (i = 0; i < variable->num_dimensions; i++)
    {
        dimension[i] = (hsize_t)variable->dimension[i];
    }

    if (variable->data_type == harp_type_string)
    {
        hid_t data_type_id;
        long length;
        char *buffer;

        if (harp_get_char_array_from_string_array(variable->num_elements, variable->data.string_data, 1, &length,
                                                  &buffer) != 0)
        {
            return -1;
        }

        data_type_id = H5Tcopy(H5T_C_S1);
        if (data_type_id < 0)
        {
            harp_set_error(HARP_ERROR_HDF5, NULL);
            free(buffer);
            return -1;
        }

        if (H5Tset_size(data_type_id, length) < 0)
        {
            harp_set_error(HARP_ERROR_HDF5, NULL);
            H5Tclose(data_type_id);
            free(buffer);
            return -1;
        }

        if (H5Tset_strpad(data_type_id, H5T_STR_NULLPAD) < 0)
        {
            harp_set_error(HARP_ERROR_HDF5, NULL);
            H5Tclose(data_type_id);
            free(buffer);
            return -1;
        }

        space_id = H5Screate_simple(variable->num_dimensions, dimension, NULL);
        if (space_id < 0)
        {
            harp_set_error(HARP_ERROR_HDF5, NULL);
            H5Tclose(data_type_id);
            free(buffer);
            return -1;
        }

        /* Setup dataset creation property list to enable attribute creation order tracking and indexing. */
        dcpl_id = H5Pcreate(H5P_DATASET_CREATE);
        if (dcpl_id < 0)
        {
            harp_set_error(HARP_ERROR_HDF5, NULL);
            H5Sclose(space_id);
            H5Tclose(data_type_id);
            free(buffer);
            return -1;
        }

        if (H5Pset_attr_creation_order(dcpl_id, H5P_CRT_ORDER_TRACKED | H5P_CRT_ORDER_INDEXED) < 0)
        {
            harp_set_error(HARP_ERROR_HDF5, NULL);
            H5Pclose(dcpl_id);
            H5Sclose(space_id);
            H5Tclose(data_type_id);
            free(buffer);
            return -1;
        }

        if (set_compression(dcpl_id, variable) != 0)
        {
            H5Pclose(dcpl_id);
            H5Sclose(space_id);
            H5Tclose(data_type_id);
            free(buffer);
            return -1;
        }

        dataset_id = H5Dcreate(group_id, name, data_type_id, space_id, dcpl_id);
        if (dataset_id < 0)
        {
            harp_set_error(HARP_ERROR_HDF5, NULL);
            H5Pclose(dcpl_id);
            H5Sclose(space_id);
            H5Tclose(data_type_id);
            free(buffer);
            return -1;
        }

        H5Pclose(dcpl_id);
        H5Sclose(space_id);

        if (H5Dwrite(dataset_id, data_type_id, H5S_ALL, H5S_ALL, H5P_DEFAULT, buffer) < 0)
        {
            harp_set_error(HARP_ERROR_HDF5, NULL);
            H5Dclose(dataset_id);
            H5Tclose(data_type_id);
            free(buffer);
            return -1;
        }

        H5Tclose(data_type_id);
        free(buffer);
    }
    else
    {
        space_id = H5Screate_simple(variable->num_dimensions, dimension, NULL);
        if (space_id < 0)
        {
            harp_set_error(HARP_ERROR_HDF5, NULL);
            return -1;
        }

        /* Setup dataset creation property list to enable attribute creation order tracking and indexing. */
        dcpl_id = H5Pcreate(H5P_DATASET_CREATE);
        if (dcpl_id < 0)
        {
            harp_set_error(HARP_ERROR_HDF5, NULL);
            H5Sclose(space_id);
            return -1;
        }

        if (H5Pset_attr_creation_order(dcpl_id, H5P_CRT_ORDER_TRACKED | H5P_CRT_ORDER_INDEXED) < 0)
        {
            harp_set_error(HARP_ERROR_HDF5, NULL);
            H5Pclose(dcpl_id);
            H5Sclose(space_id);
            return -1;
        }

        if (set_compression(dcpl_id, variable) != 0)
        {
            H5Pclose(dcpl_id);
            H5Sclose(space_id);
            return -1;
        }

        dataset_id = H5Dcreate(group_id, name, get_hdf5_type(variable->data_type), space_id, dcpl_id);
        if (dataset_id < 0)
        {
            harp_set_error(HARP_ERROR_HDF5, NULL);
            H5Pclose(dcpl_id);
            H5Sclose(space_id);
            return -1;
        }

        H5Pclose(dcpl_id);
        H5Sclose(space_id);

        if (H5Dwrite(dataset_id, get_hdf5_type(variable->data_type), H5S_ALL, H5S_ALL, H5P_DEFAULT,
                     variable->data.ptr) < 0)
        {
            harp_set_error(HARP_ERROR_HDF5, NULL);
            H5Dclose(dataset_id);
            return -1;
        }
    }

    /* Write attributes. */
    if (variable->description != NULL && strcmp(variable->description, "") != 0)
    {
        if (write_string_attribute(dataset_id, "description", variable->description) != 0)
        {
            H5Dclose(dataset_id);
            return -1;
        }
    }

    if (variable->unit != NULL)
    {
        if (write_string_attribute(dataset_id, "units", variable->unit) != 0)
        {
            H5Dclose(dataset_id);
            return -1;
        }
    }

    if (variable->data_type != harp_type_string)
    {
        if (!harp_is_valid_min_for_type(variable->data_type, variable->valid_min))
        {
            if (write_numeric_attribute(dataset_id, "valid_min", variable->data_type, variable->valid_min) != 0)
            {
                H5Dclose(dataset_id);
                return -1;
            }
        }

        if (!harp_is_valid_max_for_type(variable->data_type, variable->valid_max))
        {
            if (write_numeric_attribute(dataset_id, "valid_max", variable->data_type, variable->valid_max) != 0)
            {
                H5Dclose(dataset_id);
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
        if (write_string_attribute(dataset_id, "flag_values", attribute_value) != 0)
        {
            free(attribute_value);
            return -1;
        }
        free(attribute_value);

        if (harp_variable_get_flag_meanings_string(variable, &attribute_value) != 0)
        {
            return -1;
        }
        if (write_string_attribute(dataset_id, "flag_meanings", attribute_value) != 0)
        {
            free(attribute_value);
            return -1;
        }
        free(attribute_value);
    }

    H5Dclose(dataset_id);

    return 0;
}

static int write_dimension(hid_t group_id, harp_dimension_type dimension_type, long length, hid_t *dimensions_id)
{
    hid_t space_id;
    hid_t dcpl_id;
    hid_t dataset_id;
    hsize_t dimension[1];

    dimension[0] = length;

    space_id = H5Screate_simple(1, dimension, NULL);
    if (space_id < 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
        return -1;
    }

    /* Setup dataset creation property list to enable attribute creation order tracking and indexing. */
    dcpl_id = H5Pcreate(H5P_DATASET_CREATE);
    if (dcpl_id < 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
        H5Sclose(space_id);
        return -1;
    }

    if (H5Pset_attr_creation_order(dcpl_id, H5P_CRT_ORDER_TRACKED | H5P_CRT_ORDER_INDEXED) < 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
        H5Pclose(dcpl_id);
        H5Sclose(space_id);
        return -1;
    }

    if (dimension_type == harp_dimension_independent)
    {
        char dataset_name[64];

        sprintf(dataset_name, "independent_%ld", length);
        dataset_id = H5Dcreate(group_id, dataset_name, H5T_NATIVE_FLOAT, space_id, dcpl_id);
    }
    else
    {
        dataset_id = H5Dcreate(group_id, harp_get_dimension_type_name(dimension_type), H5T_NATIVE_FLOAT, space_id,
                               dcpl_id);
    }

    if (dataset_id < 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
        H5Pclose(dcpl_id);
        H5Sclose(space_id);
        return -1;
    }

    H5Pclose(dcpl_id);
    H5Sclose(space_id);

    if (H5DSset_scale(dataset_id, DIM_WITHOUT_VARIABLE) < 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
        H5Dclose(dataset_id);
        return -1;
    }

    if (dimensions_id != NULL)
    {
        *dimensions_id = dataset_id;
    }
    else
    {
        H5Dclose(dataset_id);
    }

    return 0;
}

static int write_dimensions(hid_t group_id, const harp_product *product, hdf5_dimensions *dimensions)
{
    harp_scalar netcdf4_dimension_id;
    int i;

    netcdf4_dimension_id.int32_data = 0;

    /* Order netCDF dimension ids such that physical dimensions appear before independent dimensions. */

    for (i = 0; i < HARP_NUM_DIM_TYPES; i++)
    {
        harp_variable *variable;
        harp_dimension_type dimension_type;
        hid_t dataset_id;

        if (product->dimension[i] == 0)
        {
            /* Product does not depend on this dimension. */
            continue;
        }

        dimension_type = (harp_dimension_type)i;
        if (harp_product_get_variable_by_name(product, harp_get_dimension_type_name(dimension_type), &variable) < 0)
        {
            if (harp_errno != HARP_ERROR_VARIABLE_NOT_FOUND)
            {
                return -1;
            }

            /* Intentional fall-through. */
        }
        else if (variable->num_dimensions == 1 && *variable->dimension_type == dimension_type)
        {
            /* This variable will be upgraded to a dimension scale in finalize_dimensions(). */
            netcdf4_dimension_id.int32_data++;
            continue;
        }

        /* Write a dimension scale without a coordinate variable attached to it. */
        if (write_dimension(group_id, dimension_type, product->dimension[i], &dataset_id) != 0)
        {
            H5Dclose(dataset_id);
            return -1;
        }

        if (write_numeric_attribute(dataset_id, NC_DIMID_ATT_NAME, harp_type_int32, netcdf4_dimension_id) != 0)
        {
            H5Dclose(dataset_id);
            return -1;
        }

        if (dimensions_add(dimensions, dimension_type, product->dimension[i], dataset_id) < 0)
        {
            H5Dclose(dataset_id);
            return -1;
        }

        netcdf4_dimension_id.int32_data++;
    }

    for (i = 0; i < product->num_variables; i++)
    {
        harp_variable *variable;
        int j;

        variable = product->variable[i];
        for (j = 0; j < variable->num_dimensions; j++)
        {
            hid_t dataset_id;

            if (variable->dimension_type[j] != harp_dimension_independent)
            {
                continue;
            }

            if (dimensions_find(dimensions, harp_dimension_independent, variable->dimension[j]) >= 0)
            {
                continue;
            }

            /* Write a dimension scale without a coordinate variable attached to it. */
            if (write_dimension(group_id, harp_dimension_independent, variable->dimension[j], &dataset_id) != 0)
            {
                H5Dclose(dataset_id);
                return -1;
            }

            if (write_numeric_attribute(dataset_id, NC_DIMID_ATT_NAME, harp_type_int32, netcdf4_dimension_id) != 0)
            {
                H5Dclose(dataset_id);
                return -1;
            }

            if (dimensions_add(dimensions, harp_dimension_independent, variable->dimension[j], dataset_id) < 0)
            {
                H5Dclose(dataset_id);
                return -1;
            }

            netcdf4_dimension_id.int32_data++;
        }
    }

    return 0;
}

static int finalize_dimensions(hid_t group_id, const harp_product *product, hdf5_dimensions *dimensions)
{
    harp_scalar netcdf4_dimension_id;
    int i;

    netcdf4_dimension_id.int32_data = 0;

    for (i = 0; i < HARP_NUM_DIM_TYPES; i++)
    {
        const char *dataset_name;
        harp_dimension_type dimension_type;
        hid_t dataset_id;

        if (product->dimension[i] == 0)
        {
            /* Product does not depend on this dimension. */
            continue;
        }

        dimension_type = (harp_dimension_type)i;
        dataset_name = harp_get_dimension_type_name(dimension_type);

        if (dimensions_find(dimensions, dimension_type, product->dimension[i]) >= 0)
        {
            netcdf4_dimension_id.int32_data++;
            continue;
        }

        dataset_id = H5Dopen(group_id, dataset_name);
        if (dataset_id < 0)
        {
            harp_set_error(HARP_ERROR_EXPORT, "dataset '%s' does not exist", dataset_name);
            return -1;
        }

        /* Upgrade dataset to a dimension scale. The dataset acts as the attached coordinate variable. */
        if (H5DSset_scale(dataset_id, DIM_WITH_VARIABLE) != 0)
        {
            harp_set_error(HARP_ERROR_HDF5, NULL);
            H5Dclose(dataset_id);
            return -1;
        }

        /* Re-order netCDF dimension ids such that physical dimensions appear before independent dimensions. */
        if (write_numeric_attribute(dataset_id, NC_DIMID_ATT_NAME, harp_type_int32, netcdf4_dimension_id) != 0)
        {
            H5Dclose(dataset_id);
            return -1;
        }

        if (dimensions_add(dimensions, dimension_type, product->dimension[i], dataset_id) < 0)
        {
            H5Dclose(dataset_id);
            return -1;
        }

        netcdf4_dimension_id.int32_data++;
    }

    return 0;
}

static int attach_dimensions(hid_t group_id, const harp_product *product, const hdf5_dimensions *dimensions)
{
    int i;

    for (i = 0; i < product->num_variables; i++)
    {
        const harp_variable *variable;
        char *name;
        hid_t dataset_id;
        int j;

        variable = product->variable[i];
        if (variable->num_dimensions == 0)
        {
            continue;
        }

        name = get_hdf5_variable_name(product, variable);
        if (name == NULL)
        {
            return -1;
        }
        dataset_id = H5Dopen(group_id, name);
        free(name);
        if (dataset_id < 0)
        {
            harp_set_error(HARP_ERROR_HDF5, NULL);
            return -1;
        }

        if (variable->num_dimensions == 1)
        {
            htri_t is_dimension_scale;

            is_dimension_scale = H5DSis_scale(dataset_id);
            if (is_dimension_scale < 0)
            {
                H5Dclose(dataset_id);
                harp_set_error(HARP_ERROR_HDF5, NULL);
                return -1;
            }

            if (is_dimension_scale)
            {
                /* Dimension scales cannot be attached to other dimension scales. */
                H5Dclose(dataset_id);
                continue;
            }
        }

        for (j = 0; j < variable->num_dimensions; j++)
        {
            int index;

            index = dimensions_find(dimensions, variable->dimension_type[j], variable->dimension[j]);
            if (index < 0)
            {
                harp_set_error(HARP_ERROR_EXPORT, "no dimension of type '%s' and length %ld",
                               harp_get_dimension_type_name(variable->dimension_type[j]), variable->dimension[j]);
                H5Dclose(dataset_id);
                return -1;
            }

            if (H5DSattach_scale(dataset_id, dimensions->dataset_id[index], j) != 0)
            {
                harp_set_error(HARP_ERROR_HDF5, NULL);
                H5Dclose(dataset_id);
                return -1;
            }
        }

        H5Dclose(dataset_id);
    }

    return 0;
}

static int write_nc3_strict_attribute(hid_t group_id)
{
    harp_scalar nc3_strict;

    nc3_strict.int32_data = 1;
    return write_numeric_attribute(group_id, NC3_STRICT_ATT_NAME, harp_type_int32, nc3_strict);
}

static int write_attributes(hid_t group_id, const harp_product *product)
{
    harp_scalar datetime_start;
    harp_scalar datetime_stop;

    if (harp_product_get_datetime_range(product, &datetime_start.double_data, &datetime_stop.double_data) == 0)
    {
        if (write_numeric_attribute(group_id, "datetime_start", harp_type_double, datetime_start) != 0)
        {
            return -1;
        }

        if (write_numeric_attribute(group_id, "datetime_stop", harp_type_double, datetime_stop) != 0)
        {
            return -1;
        }
    }

    if (product->source_product != NULL && strcmp(product->source_product, "") != 0)
    {
        if (write_string_attribute(group_id, "source_product", product->source_product) != 0)
        {
            return -1;
        }
    }

    if (product->history != NULL && strcmp(product->history, "") != 0)
    {
        if (write_string_attribute(group_id, "history", product->history) != 0)
        {
            return -1;
        }
    }

    return 0;
}

static int write_product(hid_t file_id, const harp_product *product)
{
    hid_t root_id;
    hdf5_dimensions dimensions;
    int i;

    root_id = H5Gopen(file_id, "/");
    if (root_id < 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
        return -1;
    }

    /* Mark the file as a netCDF classic netCDF-4 file. */
    if (write_nc3_strict_attribute(root_id) != 0)
    {
        H5Gclose(root_id);
        return -1;
    }

    /* Write file convention. */
    if (write_string_attribute(root_id, "Conventions", HARP_CONVENTION) != 0)
    {
        H5Gclose(root_id);
        return -1;
    }

    /* Write product attributes. */
    if (write_attributes(root_id, product) != 0)
    {
        H5Gclose(root_id);
        return -1;
    }

    /* Write dimensions and variables. */
    dimensions_init(&dimensions);

    if (write_dimensions(root_id, product, &dimensions) != 0)
    {
        dimensions_done(&dimensions);
        H5Gclose(root_id);
        return -1;
    }

    for (i = 0; i < product->num_variables; i++)
    {
        char *name;

        name = get_hdf5_variable_name(product, product->variable[i]);
        if (name == NULL)
        {
            return -1;
        }
        if (write_variable(root_id, name, product->variable[i]) != 0)
        {
            free(name);
            dimensions_done(&dimensions);
            H5Gclose(root_id);
            return -1;
        }
        free(name);
    }

    if (finalize_dimensions(root_id, product, &dimensions) != 0)
    {
        dimensions_done(&dimensions);
        H5Gclose(root_id);
        return -1;
    }

    if (attach_dimensions(root_id, product, &dimensions) != 0)
    {
        dimensions_done(&dimensions);
        H5Gclose(root_id);
        return -1;
    }

    dimensions_done(&dimensions);
    H5Gclose(root_id);

    return 0;
}

int harp_export_hdf5(const char *filename, const harp_product *product)
{
    hid_t file_id;
    hid_t fcpl_id;

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

    /* Setup file creation property list to enable link and attribute creation
     * order tracking and indexing.
     */
    fcpl_id = H5Pcreate(H5P_FILE_CREATE);
    if (fcpl_id < 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
        return -1;
    }

    if (H5Pset_link_creation_order(fcpl_id, (H5P_CRT_ORDER_TRACKED | H5P_CRT_ORDER_INDEXED)) < 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
        H5Pclose(fcpl_id);
        return -1;
    }

    if (H5Pset_attr_creation_order(fcpl_id, (H5P_CRT_ORDER_TRACKED | H5P_CRT_ORDER_INDEXED)) < 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
        H5Pclose(fcpl_id);
        return -1;
    }

    file_id = H5Fcreate(filename, H5F_ACC_TRUNC, fcpl_id, H5P_DEFAULT);
    if (file_id < 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
        harp_add_error_message(" (%s)", filename);
        H5Pclose(fcpl_id);
        return -1;
    }

    H5Pclose(fcpl_id);

    if (write_product(file_id, product) != 0)
    {
        harp_add_error_message(" (%s)", filename);
        H5Fclose(file_id);
        return -1;
    }

    if (H5Fclose(file_id) < 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
        harp_add_error_message(" (%s)", filename);
        return -1;
    }

    return 0;
}

static herr_t add_error_message(int n, H5E_error_t *err_desc, void *client_data)
{
    (void)client_data;

    if (n == 0)
    {
        /* Display only the deepest error in the stack. */
        harp_add_error_message("[HDF5] %s(): %s (major=\"%s\", minor=\"%s\") (%s:%u)", err_desc->func_name,
                               err_desc->desc, H5Eget_major(err_desc->maj_num), H5Eget_minor(err_desc->min_num),
                               err_desc->file_name, err_desc->line);
    }

    return 0;
}

void harp_hdf5_add_error_message(void)
{
    H5Ewalk(H5E_WALK_UPWARD, add_error_message, NULL);
}
