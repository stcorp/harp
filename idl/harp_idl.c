/*
 * Copyright (C) 2002-2016 S[&]T, The Netherlands.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "export.h"

#include "coda.h"
#include "harp.h"

/* ---------- defines ---------- */

#define HARP_IDL_ERR_EXPECTED_STRUCT                              (-901)
#define HARP_IDL_ERR_EXPECTED_STRING                              (-902)
#define HARP_IDL_ERR_EXPECTED_SINGLE_ELM                          (-903)
#define HARP_IDL_ERR_EMPTY_ARRAY                                  (-904)
#define HARP_IDL_ERR_EXPECTED_NUMERICAL_ARRAY                     (-905)
#define HARP_IDL_ERR_INVALID_RECORD                               (-910)
#define HARP_IDL_ERR_INVALID_FIELD_TYPE                           (-911)
#define HARP_IDL_ERR_INVALID_FIELD_ARRAY                          (-912)
#define HARP_IDL_ERR_UNKNOWN_OPTION                               (-921)

/* ---------- typedefs ---------- */

typedef struct harp_IDLError
{
    short number;
    IDL_STRING message;
} harp_IDLError;

/* ---------- global variables ---------- */

IDL_StructDefPtr harp_error_sdef;

static int harp_idl_initialised = 0;

static int harp_idl_option_verbose = 1;

/* ---------- code ---------- */

static int harp_idl_init(void)
{
    if (!harp_idl_initialised)
    {
#ifdef CODA_DEFINITION_IDL
        coda_set_definition_path_conditional("harp_idl.dlm", getenv("IDL_DLM_PATH"), CODA_DEFINITION_IDL);
#else
        coda_set_definition_path_conditional("harp_idl.dlm", getenv("IDL_DLM_PATH"), "../../../share/coda/definitions");
#endif

        if (harp_init() != 0)
        {
            return -1;
        }

        harp_idl_initialised = 1;
    }
    harp_set_error(HARP_SUCCESS, NULL);

    return 0;
}

static void harp_idl_cleanup(void)
{
    /* clean up HARP */
    if (harp_idl_initialised)
    {
        harp_done();
        harp_idl_initialised = 0;
    }
}

static void harp_idl_fill_error_struct(harp_IDLError *fill, const short err)
{
    const char *message;

    switch (err)
    {
        case HARP_IDL_ERR_EXPECTED_STRUCT:
            message = "structure argument expected";
            break;
        case HARP_IDL_ERR_EXPECTED_STRING:
            message = "string argument expected";
            break;
        case HARP_IDL_ERR_EXPECTED_SINGLE_ELM:
            message = "argument should be a single element";
            break;
        case HARP_IDL_ERR_EMPTY_ARRAY:
            message = "argument contains empty array";
            break;
        case HARP_IDL_ERR_EXPECTED_NUMERICAL_ARRAY:
            message = "argument should be numerical array";
            break;
        case HARP_IDL_ERR_INVALID_RECORD:
            message = "invalid record";
            break;
        case HARP_IDL_ERR_INVALID_FIELD_TYPE:
            message = "invalid type for record field";
            break;
        case HARP_IDL_ERR_INVALID_FIELD_ARRAY:
            message = "invalid array for record field";
            break;
        case HARP_IDL_ERR_UNKNOWN_OPTION:
            message = "unknown option";
            break;
        default:
            message = harp_errno_to_string(err);
    }

    fill->number = err;
    IDL_StrStore(&fill->message, (char *)message);
}

static IDL_VPTR harp_idl_get_error_struct(const int err)
{
    IDL_VPTR retval;
    harp_IDLError *data;

    data = (harp_IDLError *)IDL_MakeTempStructVector(harp_error_sdef, 1, &retval, FALSE);
    harp_idl_fill_error_struct(data, (short)err);

    if (data->number != HARP_SUCCESS && harp_idl_option_verbose)
    {
        char errmsg[1001];

        snprintf(errmsg, 1000, "HARP-IDL ERROR %d: \"%s\"\n", data->number, IDL_STRING_STR(&data->message));
        IDL_Message(IDL_M_GENERIC, IDL_MSG_INFO, errmsg);
    }

    return retval;
}

static int harp_idl_get_struct_def(harp_product *product, IDL_StructDefPtr *sdef)
{
    IDL_STRUCT_TAG_DEF *record_tags;
    int num_variables;
    int index;
    int i;

    num_variables = product->num_variables;
    if (num_variables < 0)
    {
        return HARP_ERROR_NO_DATA;
    }

    if (num_variables == 0)
    {
        /* Since IDL can not handle empty structs we return a 'no data' error for empty HARP records */
        return HARP_ERROR_NO_DATA;
    }

    record_tags = (IDL_STRUCT_TAG_DEF *)malloc(sizeof(IDL_STRUCT_TAG_DEF) * (num_variables + 1));
    if (record_tags == NULL)
    {
        return HARP_ERROR_OUT_OF_MEMORY;
    }

    for (index = 0; index < num_variables; index++)
    {
        harp_variable *variable;
        int field_name_length;

        variable = product->variable[index];
        if ((variable == NULL) || (variable->name == NULL))
        {
            for (i = 0; i < index; i++)
            {
                free(record_tags[i].name);
                if (record_tags[i].dims != NULL)
                {
                    free(record_tags[i].dims);
                }
            }
            free(record_tags);
            return HARP_ERROR_NO_DATA;
        }
        field_name_length = strlen(variable->name);
        record_tags[index].name = (char *)malloc(field_name_length + 1);
        if (record_tags[index].name == NULL)
        {
            for (i = 0; i < index; i++)
            {
                free(record_tags[i].name);
                if (record_tags[i].dims != NULL)
                {
                    free(record_tags[i].dims);
                }
            }
            free(record_tags);
            return HARP_ERROR_OUT_OF_MEMORY;
        }
        for (i = 0; i < field_name_length; i++)
        {
            record_tags[index].name[i] = toupper(variable->name[i]);
        }
        record_tags[index].name[field_name_length] = '\0';

        switch (variable->data_type)
        {
            case harp_type_int8:
                record_tags[index].type = (void *)IDL_TYP_BYTE;
                break;
            case harp_type_int16:
                record_tags[index].type = (void *)IDL_TYP_INT;
                break;
            case harp_type_int32:
                record_tags[index].type = (void *)IDL_TYP_LONG;
                break;
            case harp_type_float:
                record_tags[index].type = (void *)IDL_TYP_FLOAT;
                break;
            case harp_type_double:
                record_tags[index].type = (void *)IDL_TYP_DOUBLE;
                break;
            case harp_type_string:
                record_tags[index].type = (void *)IDL_TYP_STRING;
                break;
        }

        record_tags[index].dims = NULL;
        if (variable->num_dimensions > 0)
        {
            record_tags[index].dims = (IDL_MEMINT *)malloc((variable->num_dimensions + 1) * sizeof(IDL_MEMINT));
            if (record_tags[index].dims == NULL)
            {
                free(record_tags[index].name);
                for (i = 0; i < index; i++)
                {
                    free(record_tags[i].name);
                    if (record_tags[i].dims != NULL)
                    {
                        free(record_tags[i].dims);
                    }
                }
                free(record_tags);
                return HARP_ERROR_OUT_OF_MEMORY;
            }
            record_tags[index].dims[0] = variable->num_dimensions;
            for (i = 0; i < variable->num_dimensions; i++)
            {
                record_tags[index].dims[variable->num_dimensions - i] = variable->dimension[i];
            }
        }

        record_tags[index].flags = 0;
    }

    record_tags[num_variables].name = NULL;
    record_tags[num_variables].dims = NULL;
    record_tags[num_variables].type = 0;
    record_tags[num_variables].flags = 0;

    *sdef = IDL_MakeStruct(0, record_tags);

    for (i = 0; i < num_variables; i++)
    {
        free(record_tags[i].name);
        if (record_tags[i].dims != NULL)
        {
            free(record_tags[i].dims);
        }
    }
    free(record_tags);

    return 0;
}

static void copy_one_dimension_from_harp_to_idl(harp_variable *variable, int dimension_nr, long offset, char **idl_data)
{
    long i, size_lower_dimensions;
    int lower_dim;

    if (dimension_nr == 0)
    {
        for (i = 0; i < variable->dimension[dimension_nr]; i++)
        {
            switch (variable->data_type)
            {
                case harp_type_int8:
                    *((int8_t *)(*idl_data)) = variable->data.int8_data[offset + i];
                    *idl_data = *idl_data + sizeof(int8_t);
                    break;
                case harp_type_int16:
                    *((int16_t *)(*idl_data)) = variable->data.int16_data[offset + i];
                    *idl_data = *idl_data + sizeof(int16_t);
                    break;
                case harp_type_int32:
                    *((int32_t *)(*idl_data)) = variable->data.int32_data[offset + i];
                    *idl_data = *idl_data + sizeof(int32_t);
                    break;
                case harp_type_float:
                    *((float *)(*idl_data)) = variable->data.float_data[offset + i];
                    *idl_data = *idl_data + sizeof(float);
                    break;
                case harp_type_double:
                    *((double *)(*idl_data)) = variable->data.double_data[offset + i];
                    *idl_data = *idl_data + sizeof(double);
                    break;
                case harp_type_string:
                    IDL_StrStore(((IDL_STRING *)(*idl_data)), variable->data.string_data[i]);
                    *idl_data = *idl_data + sizeof(IDL_STRING);
                    break;
            }
        }
    }
    else
    {
        size_lower_dimensions = 1;
        for (lower_dim = dimension_nr - 1; lower_dim >= 0; lower_dim--)
        {
            size_lower_dimensions *= variable->dimension[lower_dim];
        }
        for (i = 0; i < variable->dimension[dimension_nr]; i++)
        {
            copy_one_dimension_from_harp_to_idl(variable, dimension_nr - 1, offset + i * size_lower_dimensions,
                                                idl_data);
        }
    }
}

static int harp_idl_get_struct_data(harp_product *product, IDL_StructDefPtr sdef, char *data)
{
    int num_variables;
    int index;

    num_variables = product->num_variables;
    if (num_variables < 0)
    {
        return HARP_ERROR_NO_DATA;
    }

    for (index = 0; index < num_variables; index++)
    {
        harp_variable *variable;
        char *idl_data;
        IDL_VPTR field_info;

        idl_data = data + IDL_StructTagInfoByIndex(sdef, index, IDL_MSG_LONGJMP, &field_info);
        variable = product->variable[index];
        copy_one_dimension_from_harp_to_idl(variable, variable->num_dimensions - 1, 0L, &idl_data);
    }

    return 0;
}

/* Create an IDL record with the data from a HARP record */
static IDL_VPTR harp_idl_get_record(harp_product *product)
{
    IDL_StructDefPtr sdef = NULL;
    IDL_VPTR idl_record;
    char *data;
    int result;

    result = harp_idl_get_struct_def(product, &sdef);
    if (result != 0)
    {
        return harp_idl_get_error_struct(result);
    }
    data = IDL_MakeTempStructVector(sdef, 1, &idl_record, IDL_TRUE);
    result = harp_idl_get_struct_data(product, sdef, data);
    if (result != 0)
    {
        IDL_Deltmp(idl_record);
        return harp_idl_get_error_struct(result);
    }

    return idl_record;
}

static void copy_one_dimension_from_idl_to_harp(harp_variable *variable, int dimension_nr, long offset, char **idl_data)
{
    long i, size_lower_dimensions;
    int lower_dim;
    const char *str;

    if (dimension_nr == 0)
    {
        for (i = 0; i < variable->dimension[dimension_nr]; i++)
        {
            switch (variable->data_type)
            {
                case harp_type_int8:
                    variable->data.int8_data[offset + i] = *((int8_t *)(*idl_data));
                    *idl_data = *idl_data + sizeof(int8_t);
                    break;
                case harp_type_int16:
                    variable->data.int16_data[offset + i] = *((int16_t *)(*idl_data));
                    *idl_data = *idl_data + sizeof(int16_t);
                    break;
                case harp_type_int32:
                    variable->data.int32_data[offset + i] = *((int32_t *)(*idl_data));
                    *idl_data = *idl_data + sizeof(int32_t);
                    break;
                case harp_type_float:
                    variable->data.float_data[offset + i] = *((float *)(*idl_data));
                    *idl_data = *idl_data + sizeof(float);
                    break;
                case harp_type_double:
                    variable->data.double_data[offset + i] = *((double *)(*idl_data));
                    *idl_data = *idl_data + sizeof(double);
                    break;
                case harp_type_string:
                    str = IDL_STRING_STR((IDL_STRING *)(*idl_data));
                    harp_variable_set_string_data_element(variable, offset + i, str);
                    *idl_data = *idl_data + sizeof(IDL_STRING);
                    break;
            }
        }
    }
    else
    {
        size_lower_dimensions = 1;
        for (lower_dim = dimension_nr - 1; lower_dim >= 0; lower_dim--)
        {
            size_lower_dimensions *= variable->dimension[lower_dim];
        }
        for (i = 0; i < variable->dimension[dimension_nr]; i++)
        {
            copy_one_dimension_from_idl_to_harp(variable, dimension_nr - 1, offset + i * size_lower_dimensions,
                                                idl_data);
        }
    }
}

/* Copy the data from an IDL record to a HARP record */
static int harp_idl_set_record(IDL_VPTR idl_record, harp_product *product)
{
    IDL_StructDefPtr sdef;
    int num_variables;
    int index;

    if ((idl_record->flags & IDL_V_STRUCT) == 0)
    {
        return HARP_IDL_ERR_INVALID_RECORD;
    }
    if (idl_record->value.s.arr->n_dim > 1 || idl_record->value.s.arr->dim[0] > 1)
    {
        return HARP_IDL_ERR_INVALID_RECORD;
    }
    sdef = idl_record->value.s.sdef;
    num_variables = IDL_StructNumTags(sdef);
    if (num_variables <= 0)
    {
        return HARP_IDL_ERR_INVALID_RECORD;
    }

    for (index = 0; index < num_variables; index++)
    {
        harp_data_type type;
        harp_variable *variable;
        const char *field_name;
        long dim[HARP_MAX_NUM_DIMS];
        harp_dimension_type dimension_type[HARP_MAX_NUM_DIMS];
        int num_dims;
        char *idl_data;
        IDL_VPTR field;
        long i;

        idl_data = ((char *)idl_record->value.s.arr->data) +
            IDL_StructTagInfoByIndex(sdef, index, IDL_MSG_LONGJMP, &field);

        field_name = IDL_StructTagNameByIndex(sdef, index, IDL_MSG_LONGJMP, NULL);

        switch (field->type)
        {
            case IDL_TYP_BYTE:
                type = harp_type_int8;
                break;
            case IDL_TYP_INT:
                type = harp_type_int16;
                break;
            case IDL_TYP_LONG:
                type = harp_type_int32;
                break;
            case IDL_TYP_FLOAT:
                type = harp_type_float;
                break;
            case IDL_TYP_DOUBLE:
                type = harp_type_double;
                break;
            case IDL_TYP_STRING:
                type = harp_type_string;
                break;
            default:
                return HARP_IDL_ERR_INVALID_FIELD_TYPE;
        }

        num_dims = 0;
        if ((field->flags & IDL_V_ARR) != 0)
        {
            num_dims = field->value.arr->n_dim;
            if (num_dims > HARP_MAX_NUM_DIMS)
            {
                return HARP_IDL_ERR_INVALID_FIELD_ARRAY;
            }
            for (i = 0; i < num_dims; i++)
            {
                dim[num_dims - i - 1] = field->value.arr->dim[i];
                dimension_type[num_dims - i - 1] = harp_dimension_independent;
            }
        }

        if (harp_variable_new(field_name, type, num_dims, dimension_type, dim, &variable) != 0)
        {
            return harp_errno;
        }
        copy_one_dimension_from_idl_to_harp(variable, num_dims - 1, 0L, &idl_data);
        if (harp_product_add_variable(product, variable) != 0)
        {
            return harp_errno;
        }
    }

    return 0;
}

static IDL_VPTR harp_idl_export(int argc, IDL_VPTR *argv)
{
    harp_product *product;
    const char *format;
    int result;

    assert(argc == 2 || argc == 3);

    if (argv[0]->type != IDL_TYP_STRUCT)
    {
        return harp_idl_get_error_struct(HARP_IDL_ERR_EXPECTED_STRUCT);
    }
    if ((argv[1]->type != IDL_TYP_STRING) || (argc > 2 && (argv[2]->type != IDL_TYP_STRING)))
    {
        return harp_idl_get_error_struct(HARP_IDL_ERR_EXPECTED_STRING);
    }
    if (((argv[1]->flags & IDL_V_ARR) != 0) || (argc > 2 && ((argv[2]->flags & IDL_V_ARR) != 0)))
    {
        return harp_idl_get_error_struct(HARP_IDL_ERR_EXPECTED_SINGLE_ELM);
    }

    if (harp_idl_init() != 0)
    {
        return harp_idl_get_error_struct(harp_errno);
    }

    /* First argument = productname, second argument is filename, third (optional) argument is format. */
    if (harp_product_new(&product) != 0)
    {
        return harp_idl_get_error_struct(harp_errno);
    }
    result = harp_idl_set_record(argv[0], product);
    if (result != 0)
    {
        harp_product_delete(product);
        return harp_idl_get_error_struct(result);
    }

    format = "netcdf";
    if (argc > 2)
    {
        format = IDL_STRING_STR(&argv[2]->value.str);
    }

    if (harp_export(IDL_STRING_STR(&argv[1]->value.str), format, product) != 0)
    {
        harp_product_delete(product);
        return harp_idl_get_error_struct(harp_errno);
    }
    harp_product_delete(product);

    return harp_idl_get_error_struct(HARP_SUCCESS);
}

static IDL_VPTR harp_idl_import(int argc, IDL_VPTR *argv)
{
    harp_product *product;
    const char *operations;
    const char *options;
    IDL_VPTR retval;

    assert(argc == 1 || argc == 2 || argc == 3);

    if ((argv[0]->type != IDL_TYP_STRING) || (argc > 1 && (argv[1]->type != IDL_TYP_STRING)) ||
        (argc > 2 && (argv[2]->type != IDL_TYP_STRING)))
    {
        return harp_idl_get_error_struct(HARP_IDL_ERR_EXPECTED_STRING);
    }
    if (((argv[0]->flags & IDL_V_ARR) != 0) || (argc > 1 && ((argv[1]->flags & IDL_V_ARR) != 0)) ||
        (argc > 2 && ((argv[2]->flags & IDL_V_ARR) != 0)))
    {
        return harp_idl_get_error_struct(HARP_IDL_ERR_EXPECTED_SINGLE_ELM);
    }

    if (harp_idl_init() != 0)
    {
        return harp_idl_get_error_struct(harp_errno);
    }

    /* First argument = filename, second (optional) argument is operations, third (optional) argument is options. */
    operations = NULL;
    if (argc > 1)
    {
        operations = IDL_STRING_STR(&argv[1]->value.str);
    }
    options = NULL;
    if (argc > 2)
    {
        options = IDL_STRING_STR(&argv[2]->value.str);
    }

    if (harp_import(IDL_STRING_STR(&argv[0]->value.str), operations, options, &product) != 0)
    {
        return harp_idl_get_error_struct(harp_errno);
    }

    retval = harp_idl_get_record(product);
    harp_product_delete(product);

    return retval;
}

static void harp_idl_unload(int argc, IDL_VPTR *argv)
{
    assert(argc == 0);
    (void)argv;

    harp_idl_cleanup();
}

static void register_idl_struct_types(void)
{
    /* define the HARP_ERROR structure type */

    static IDL_STRUCT_TAG_DEF harp_error_tags[] = {
        {"ERRNO", 0, (void *)IDL_TYP_INT, 0},
        {"MESSAGE", 0, (void *)IDL_TYP_STRING, 0},
        {0, 0, 0, 0}
    };

    harp_error_sdef = IDL_MakeStruct("HARP_ERROR", harp_error_tags);
}

static int register_idl_functions_and_procedures(void)
{
    /* function declarations */

    static IDL_SYSFUN_DEF2 idl_func_addr[] = {
        {{harp_idl_export}, "HARP_EXPORT", 2, 3, 0, 0}, /* harp_export(product, filename, <format>) */
        {{harp_idl_import}, "HARP_IMPORT", 1, 3, 0, 0}, /* product = harp_import(filename, <operations>, <options>) */
    };

    /* procedure declarations */

    static IDL_SYSFUN_DEF2 idl_proc_addr[] = {
        {{(IDL_SYSRTN_GENERIC)harp_idl_unload}, "HARP_UNLOAD", 0, 0, 0, 0}
    };

    return IDL_SysRtnAdd(idl_func_addr, TRUE, sizeof(idl_func_addr) / sizeof(IDL_SYSFUN_DEF2)) &&
        IDL_SysRtnAdd(idl_proc_addr, FALSE, sizeof(idl_proc_addr) / sizeof(IDL_SYSFUN_DEF2));
}

int IDL_Load(void)
{
    /* register types, functions, and procedures */

    register_idl_struct_types();
    return register_idl_functions_and_procedures();
}
