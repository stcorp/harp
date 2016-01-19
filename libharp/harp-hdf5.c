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
#include <string.h>

#include "hdf5.h"

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

static int read_string_attribute(hid_t obj_id, const char *name, char **data)
{
    char *str;
    hid_t attr_id;
    hid_t data_type_id;
    hid_t mem_type_id;
    hid_t space_id;
    hsize_t size;

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

    if (H5Tget_class(data_type_id) != H5T_STRING || H5Tis_variable_str(data_type_id) > 0)
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
    H5Tclose(data_type_id);

    space_id = H5Aget_space(attr_id);
    if (space_id < 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
        H5Aclose(attr_id);
        return -1;
    }

    if (H5Sis_simple(space_id) <= 0 || H5Sget_simple_extent_type(space_id) != H5S_SCALAR)
    {
        harp_set_error(HARP_ERROR_IMPORT, "attribute '%s' has invalid format", name);
        H5Sclose(space_id);
        H5Aclose(attr_id);
        return -1;
    }
    H5Sclose(space_id);

    mem_type_id = H5Tcopy(H5T_C_S1);
    if (mem_type_id < 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
        H5Aclose(attr_id);
        return -1;
    }

    if (H5Tset_size(mem_type_id, size) < 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
        H5Tclose(mem_type_id);
        H5Aclose(attr_id);
        return -1;
    }

    if (H5Tset_strpad(mem_type_id, H5T_STR_NULLPAD) < 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
        H5Tclose(mem_type_id);
        H5Aclose(attr_id);
        return -1;
    }

    str = malloc((size + 1) * sizeof(char));
    if (str == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       (size + 1) * sizeof(char), __FILE__, __LINE__);
        H5Tclose(mem_type_id);
        H5Aclose(attr_id);
        return -1;
    }
    str[size] = '\0';

    if (H5Aread(attr_id, mem_type_id, str) < 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
        free(str);
        H5Tclose(mem_type_id);
        H5Aclose(attr_id);
        return -1;
    }
    H5Tclose(mem_type_id);
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

    if (H5Sis_simple(space_id) <= 0 || H5Sget_simple_extent_type(space_id) != H5S_SCALAR)
    {
        harp_set_error(HARP_ERROR_IMPORT, "attribute '%s' has invalid format", name);
        H5Sclose(space_id);
        H5Aclose(attr_id);
        return -1;
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

static int read_dimensions(hid_t obj_id, int *num_dimensions, harp_dimension_type *dimension_type)
{
    char *cursor;
    char *dims;

    if (H5Aexists(obj_id, "dims") <= 0)
    {
        /* If the 'dims' attribute does not exist, the corresponding variable is scalar (i.e. has zero dimensions). */
        *num_dimensions = 0;
        return 0;
    }

    if (read_string_attribute(obj_id, "dims", &dims) != 0)
    {
        return -1;
    }

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

    if (*num_dimensions == 0)
    {
        harp_set_error(HARP_ERROR_IMPORT, "empty dimension list");
        return -1;
    }

    if (*num_dimensions == HARP_MAX_NUM_DIMS && *cursor != '\0')
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

static int read_variable(harp_product *product, hid_t dataset_id, const char *name)
{
    hsize_t hdf5_dimension[HARP_MAX_NUM_DIMS];
    hid_t data_type_id;
    hid_t space_id;
    hsize_t size;
    int hdf5_num_dimensions;
    harp_variable *variable;
    harp_dimension_type dimension_type[HARP_MAX_NUM_DIMS];
    long dimension[HARP_MAX_NUM_DIMS];
    harp_data_type data_type;
    int num_dimensions;
    herr_t result;
    long i;

    data_type_id = H5Dget_type(dataset_id);
    if (data_type_id < 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
        return -1;
    }

    if (get_harp_type(data_type_id, &data_type) != 0)
    {
        harp_add_error_message(" (dataset '%s')", name);
        H5Tclose(data_type_id);
        return -1;
    }

    size = H5Tget_size(data_type_id);
    if (size == 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
        H5Tclose(data_type_id);
        return -1;
    }
    H5Tclose(data_type_id);

    space_id = H5Dget_space(dataset_id);
    if (space_id < 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
        return -1;
    }

    if (H5Sis_simple(space_id) <= 0)
    {
        harp_set_error(HARP_ERROR_IMPORT, "complex HDF5 dataspaces not supported");
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

    if (H5Sget_simple_extent_dims(space_id, hdf5_dimension, NULL) < 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
        H5Sclose(space_id);
        return -1;
    }
    H5Sclose(space_id);

    if (read_dimensions(dataset_id, &num_dimensions, dimension_type) != 0)
    {
        harp_add_error_message(" (dataset '%s')", name);
        return -1;
    }

    if (hdf5_num_dimensions != num_dimensions)
    {
        harp_set_error(HARP_ERROR_IMPORT, "dataset '%s' has %d dimensions; expected %d", name, hdf5_num_dimensions,
                       num_dimensions);
        return -1;
    }

    for (i = 0; i < num_dimensions; i++)
    {
        dimension[i] = (long)hdf5_dimension[i];
    }

    if (harp_variable_new(name, data_type, num_dimensions, dimension_type, dimension, &variable) != 0)
    {
        return -1;
    }

    if (harp_product_add_variable(product, variable) != 0)
    {
        harp_variable_delete(variable);
        return -1;
    }

    if (data_type == harp_type_string)
    {
        char *buffer;
        hid_t mem_type_id;

        mem_type_id = H5Tcopy(H5T_C_S1);
        if (mem_type_id < 0)
        {
            harp_set_error(HARP_ERROR_HDF5, NULL);
            return -1;
        }

        if (H5Tset_size(mem_type_id, size) < 0)
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

        buffer = malloc(variable->num_elements * size * sizeof(char));
        if (buffer == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           variable->num_elements * size * sizeof(char), __FILE__, __LINE__);
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

        for (i = 0; i < variable->num_elements; i++)
        {
            char *str;

            str = malloc((size + 1) * sizeof(char));
            if (str == NULL)
            {
                harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                               (size + 1) * sizeof(char), __FILE__, __LINE__);
                free(buffer);
                H5Tclose(mem_type_id);
                return -1;
            }

            memcpy(str, &buffer[i * size], size);
            str[size] = '\0';
            variable->data.string_data[i] = str;
        }

        free(buffer);
        H5Tclose(mem_type_id);
    }
    else
    {
        if (H5Dread(dataset_id, get_hdf5_type(data_type), H5S_ALL, H5S_ALL, H5P_DEFAULT, variable->data.ptr) < 0)
        {
            harp_set_error(HARP_ERROR_HDF5, NULL);
            return -1;
        }
    }

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

        if (attr_data_type != data_type)
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

        if (attr_data_type != data_type)
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

    return 0;
}

static int read_product(harp_product *product, hid_t file_id)
{
    hsize_t num_group_objects;
    hid_t root_id;
    herr_t result;
    int i;

    root_id = H5Gopen(file_id, "/");
    if (root_id < 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
        return -1;
    }

    /* Read variables. */
    if (H5Gget_num_objs(root_id, &num_group_objects) < 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
        H5Gclose(root_id);
        return -1;
    }

    for (i = 0; i < (long)num_group_objects; i++)
    {
        if (H5Gget_objtype_by_idx(root_id, i) == H5G_DATASET)
        {
            char *name;
            long length;
            hid_t dataset_id;

            length = H5Gget_objname_by_idx(root_id, i, NULL, 0);

            name = malloc((length + 1) * sizeof(char));
            if (name == NULL)
            {
                harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                               (length + 1) * sizeof(char), __FILE__, __LINE__);
                H5Gclose(root_id);
                return -1;
            }

            if (H5Gget_objname_by_idx(root_id, i, name, length + 1) < 0)
            {
                harp_set_error(HARP_ERROR_HDF5, NULL);
                H5Gclose(root_id);
                return -1;
            }

            dataset_id = H5Dopen(root_id, name);
            if (dataset_id < 0)
            {
                harp_set_error(HARP_ERROR_HDF5, NULL);
                H5Gclose(root_id);
                return -1;
            }

            if (read_variable(product, dataset_id, name) != 0)
            {
                H5Dclose(dataset_id);
                H5Gclose(root_id);
                return -1;
            }

            H5Dclose(dataset_id);
        }
    }

    /* Read attributes. */
    result = H5Aexists(file_id, "source_product");
    if (result > 0)
    {
        if (read_string_attribute(file_id, "source_product", &product->source_product) != 0)
        {
            H5Gclose(root_id);
            return -1;
        }
    }
    else if (result < 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
        H5Gclose(root_id);
        return -1;
    }

    result = H5Aexists(file_id, "history");
    if (result > 0)
    {
        if (read_string_attribute(file_id, "history", &product->history) != 0)
        {
            H5Gclose(root_id);
            return -1;
        }
    }
    else if (result < 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
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

int harp_import_hdf5(const char *filename, harp_product **product)
{
    harp_product *new_product;
    hid_t file_id;

    file_id = H5Fopen(filename, H5F_ACC_RDONLY, H5P_DEFAULT);
    if (file_id < 0)
    {
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

    if (read_product(new_product, file_id) != 0)
    {
        harp_product_delete(new_product);
        H5Fclose(file_id);
        return -1;
    }

    H5Fclose(file_id);

    *product = new_product;
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

static int write_dimensions(hid_t obj_id, int num_dimensions, const harp_dimension_type *dimension_type)
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
        length += strlen(harp_get_dimension_type_name(dimension_type[i]));

        /* Reserve additional space for the ',' separator. */
        if (i < num_dimensions - 1)
        {
            length += 1;
        }
    }

    dimension_str = (char *)malloc((length + 1) * sizeof(char));
    if (dimension_str == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)", (length + 1)
                       * sizeof(char), __FILE__, __LINE__);
        return -1;
    }

    dimension_str[0] = '\0';
    for (i = 0; i < num_dimensions; i++)
    {
        strcat(dimension_str, harp_get_dimension_type_name(dimension_type[i]));

        if (i < num_dimensions - 1)
        {
            strcat(dimension_str, ",");
        }
    }

    if (write_string_attribute(obj_id, "dims", dimension_str) != 0)
    {
        free(dimension_str);
        return -1;
    }

    free(dimension_str);

    return 0;
}

static int write_variable(harp_variable *variable, hid_t file_id)
{
    hsize_t dimension[HARP_MAX_NUM_DIMS];
    hid_t dataset_id;
    hid_t space_id;
    int i;

    for (i = 0; i < variable->num_dimensions; i++)
    {
        dimension[i] = variable->dimension[i];
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

        dataset_id = H5Dcreate(file_id, variable->name, data_type_id, space_id, H5P_DEFAULT);
        if (dataset_id < 0)
        {
            harp_set_error(HARP_ERROR_HDF5, NULL);
            H5Sclose(space_id);
            H5Tclose(data_type_id);
            free(buffer);
            return -1;
        }
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

        dataset_id = H5Dcreate(file_id, variable->name, get_hdf5_type(variable->data_type), space_id, H5P_DEFAULT);
        if (dataset_id < 0)
        {
            harp_set_error(HARP_ERROR_HDF5, NULL);
            H5Sclose(space_id);
            return -1;
        }
        H5Sclose(space_id);

        if (H5Dwrite(dataset_id, get_hdf5_type(variable->data_type), H5S_ALL, H5S_ALL, H5P_DEFAULT,
                     variable->data.ptr) < 0)
        {
            harp_set_error(HARP_ERROR_HDF5, NULL);
            H5Dclose(dataset_id);
            return -1;
        }
    }

    /* Write dimensions. */
    if (write_dimensions(dataset_id, variable->num_dimensions, variable->dimension_type) != 0)
    {
        H5Dclose(dataset_id);
        return -1;
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

    if (variable->unit != NULL && strcmp(variable->unit, "") != 0)
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

    H5Dclose(dataset_id);
    return 0;
}

static int write_product(const harp_product *product, hid_t file_id)
{
    hid_t root_id;
    int i;

    root_id = H5Gopen(file_id, "/");
    if (root_id < 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
        return -1;
    }

    /* Write file convention. */
    if (write_string_attribute(file_id, "Conventions", HARP_CONVENTION) != 0)
    {
        return -1;
    }

    /* Write attributes. */
    if (product->source_product != NULL && strcmp(product->source_product, "") != 0)
    {
        if (write_string_attribute(file_id, "source_product", product->source_product) != 0)
        {
            H5Gclose(root_id);
            return -1;
        }
    }

    if (product->history != NULL && strcmp(product->history, "") != 0)
    {
        if (write_string_attribute(file_id, "history", product->history) != 0)
        {
            H5Gclose(root_id);
            return -1;
        }
    }

    /* Write variables. */
    for (i = 0; i < product->num_variables; i++)
    {
        if (write_variable(product->variable[i], file_id) != 0)
        {
            H5Gclose(root_id);
            return -1;
        }
    }

    H5Gclose(root_id);
    return 0;
}

int harp_export_hdf5(const char *filename, const harp_product *product)
{
    hid_t file_id;

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

    file_id = H5Fcreate(filename, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
    if (file_id < 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
        return -1;
    }

    if (write_product(product, file_id) != 0)
    {
        H5Fclose(file_id);
        return -1;
    }

    if (H5Fclose(file_id) < 0)
    {
        harp_set_error(HARP_ERROR_HDF5, NULL);
        return -1;
    }

    return 0;
}

static herr_t add_error_message(int n, H5E_error_t *err_desc, void *client_data)
{
    (void)client_data;

    if (n == 0)
    {
        /* we only display the deepest error in the stack */
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
