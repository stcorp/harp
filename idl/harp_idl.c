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
#define HARP_IDL_ERR_INVALID_PRODUCT                              (-910)
#define HARP_IDL_ERR_INVALID_VARIABLE                             (-911)
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
        harp_set_coda_definition_path_conditional("harp_idl.dlm", getenv("IDL_DLM_PATH"),
                                                  "../../../share/coda/definitions");
        harp_set_udunits2_xml_path_conditional("harp_idl.dlm", getenv("IDL_DLM_PATH"),
                                               "../../../share/harp/udunits2.xml");

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

    message = harp_errno_to_string(err);
    if (message[0] == '\0')
    {
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
            case HARP_IDL_ERR_INVALID_PRODUCT:
                message = "invalid product record";
                break;
            case HARP_IDL_ERR_INVALID_VARIABLE:
                message = "invalid variable record";
                break;
            case HARP_IDL_ERR_UNKNOWN_OPTION:
                message = "unknown option";
                break;
            default:
                message = "unkown error";
                break;
        }
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

static int harp_idl_get_struct_def_for_variable(harp_variable *variable, IDL_StructDefPtr *sdef)
{
    IDL_STRUCT_TAG_DEF *record_tags;
    int index = 0;
    int i;

    record_tags = (IDL_STRUCT_TAG_DEF *)malloc(8 * sizeof(IDL_STRUCT_TAG_DEF));
    if (record_tags == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       8 * sizeof(IDL_STRUCT_TAG_DEF), __FILE__, __LINE__);
        return -1;
    }

    /* name */
    record_tags[index].name = "NAME";
    record_tags[index].type = (void *)IDL_TYP_STRING;
    record_tags[index].dims = NULL;
    record_tags[index].flags = 0;
    index++;

    /* data */
    record_tags[index].name = "DATA";
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
            free(record_tags);
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           (variable->num_dimensions + 1) * sizeof(IDL_MEMINT), __FILE__, __LINE__);
            return -1;
        }
        record_tags[index].dims[0] = variable->num_dimensions;
        for (i = 0; i < variable->num_dimensions; i++)
        {
            record_tags[index].dims[variable->num_dimensions - i] = variable->dimension[i];
        }
    }
    record_tags[index].flags = 0;
    index++;

    /* dimension */
    if (variable->num_dimensions > 0)
    {
        record_tags[index].name = "DIMENSION";
        record_tags[index].type = (void *)IDL_TYP_STRING;
        record_tags[index].dims = (IDL_MEMINT *)malloc((variable->num_dimensions + 1) * sizeof(IDL_MEMINT));
        if (record_tags[index].dims == NULL)
        {
            for (i = 0; i < index; i++)
            {
                if (record_tags[i].dims != NULL)
                {
                    free(record_tags[i].dims);
                }
            }
            free(record_tags);
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           (variable->num_dimensions + 1) * sizeof(IDL_MEMINT), __FILE__, __LINE__);
            return -1;
        }
        record_tags[index].dims[0] = 1;
        record_tags[index].dims[1] = variable->num_dimensions;
        record_tags[index].flags = 0;
        index++;
    }

    /* unit */
    if (variable->unit != NULL)
    {
        record_tags[index].name = "UNIT";
        record_tags[index].type = (void *)IDL_TYP_STRING;
        record_tags[index].dims = NULL;
        record_tags[index].flags = 0;
        index++;
    }

    if (variable->data_type != harp_type_string)
    {
        /* valid_min */
        if (!harp_is_valid_min_for_type(variable->data_type, variable->valid_min))
        {
            record_tags[index].name = "VALID_MIN";
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
            record_tags[index].flags = 0;
            index++;
        }

        /* valid_max */
        if (!harp_is_valid_max_for_type(variable->data_type, variable->valid_max))
        {
            record_tags[index].name = "VALID_MAX";
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
            record_tags[index].flags = 0;
            index++;
        }
    }

    /* description */
    record_tags[index].name = "DESCRIPTION";
    record_tags[index].type = (void *)IDL_TYP_STRING;
    record_tags[index].dims = NULL;
    record_tags[index].flags = 0;
    index++;

    /* enum */
    if (variable->num_enum_values > 0)
    {
        record_tags[index].name = "ENUM";
        record_tags[index].type = (void *)IDL_TYP_STRING;
        record_tags[index].dims = (IDL_MEMINT *)malloc((variable->num_enum_values + 1) * sizeof(IDL_MEMINT));
        if (record_tags[index].dims == NULL)
        {
            for (i = 0; i < index; i++)
            {
                if (record_tags[i].dims != NULL)
                {
                    free(record_tags[i].dims);
                }
            }
            free(record_tags);
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           (variable->num_enum_values + 1) * sizeof(IDL_MEMINT), __FILE__, __LINE__);
            return -1;
        }
        record_tags[index].dims[0] = 1;
        record_tags[index].dims[1] = variable->num_enum_values;
        record_tags[index].flags = 0;
        index++;
    }

    record_tags[index].name = NULL;
    record_tags[index].dims = NULL;
    record_tags[index].type = 0;
    record_tags[index].flags = 0;

    *sdef = IDL_MakeStruct(0, record_tags);

    for (i = 0; i < index; i++)
    {
        if (record_tags[i].dims != NULL)
        {
            free(record_tags[i].dims);
        }
    }
    free(record_tags);

    return 0;
}

static int harp_idl_get_struct_def_for_product(harp_product *product, IDL_StructDefPtr *sdef)
{
    IDL_STRUCT_TAG_DEF *record_tags;
    int num_fields;
    int index;
    int i;

    if (product->num_variables == 0)
    {
        /* Since IDL can not handle empty structs we return a 'no data' error for empty HARP products */
        harp_set_error(HARP_ERROR_NO_DATA, NULL);
        return -1;
    }

    num_fields = product->num_variables;
    if (product->source_product != NULL)
    {
        num_fields++;
    }
    if (product->history != NULL)
    {
        num_fields++;
    }

    record_tags = (IDL_STRUCT_TAG_DEF *)malloc(sizeof(IDL_STRUCT_TAG_DEF) * (num_fields + 1));
    if (record_tags == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(IDL_STRUCT_TAG_DEF) * (num_fields + 1), __FILE__, __LINE__);
        return -1;
    }

    for (index = 0; index < product->num_variables; index++)
    {
        harp_variable *variable;
        int field_name_length;

        variable = product->variable[index];
        field_name_length = (int)strlen(variable->name);
        record_tags[index].name = (char *)malloc(field_name_length + 1);
        if (record_tags[index].name == NULL)
        {
            for (i = 0; i < index; i++)
            {
                free(record_tags[i].name);
            }
            free(record_tags);
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           (long)(field_name_length + 1), __FILE__, __LINE__);
            return -1;
        }
        for (i = 0; i < field_name_length; i++)
        {
            record_tags[index].name[i] = toupper(variable->name[i]);
        }
        record_tags[index].name[field_name_length] = '\0';

        if (harp_idl_get_struct_def_for_variable(variable, (IDL_StructDefPtr *)&record_tags[index].type) != 0)
        {
            for (i = 0; i < index; i++)
            {
                free(record_tags[i].name);
            }
            free(record_tags);
            return -1;
        }
        record_tags[index].dims = NULL;
        record_tags[index].flags = 0;
    }

    if (product->source_product != NULL)
    {
        record_tags[index].name = strdup("SOURCE_PRODUCT");
        if (record_tags[index].name == NULL)
        {
            for (i = 0; i < index; i++)
            {
                free(record_tags[i].name);
            }
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                           __LINE__);
            return -1;
        }
        record_tags[index].dims = NULL;
        record_tags[index].type = (void *)IDL_TYP_STRING;
        record_tags[index].flags = 0;
        index++;
    }
    if (product->history != NULL)
    {
        record_tags[index].name = strdup("HISTORY");
        if (record_tags[index].name == NULL)
        {
            for (i = 0; i < index; i++)
            {
                free(record_tags[i].name);
            }
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                           __LINE__);
            return -1;
        }
        record_tags[index].dims = NULL;
        record_tags[index].type = (void *)IDL_TYP_STRING;
        record_tags[index].flags = 0;
        index++;
    }

    record_tags[index].name = NULL;
    record_tags[index].dims = NULL;
    record_tags[index].type = 0;
    record_tags[index].flags = 0;

    *sdef = IDL_MakeStruct(0, record_tags);

    for (i = 0; i < product->num_variables; i++)
    {
        free(record_tags[i].name);
    }
    free(record_tags);

    return 0;
}

static int harp_idl_get_array_data(harp_data_type data_type, long num_elements, char *destination, char *source)
{
    switch (data_type)
    {
        case harp_type_int8:
            memcpy(destination, source, num_elements * sizeof(int8_t));
            break;
        case harp_type_int16:
            memcpy(destination, source, num_elements * sizeof(int16_t));
            break;
        case harp_type_int32:
            memcpy(destination, source, num_elements * sizeof(int32_t));
            break;
        case harp_type_float:
            memcpy(destination, source, num_elements * sizeof(float));
            break;
        case harp_type_double:
            memcpy(destination, source, num_elements * sizeof(double));
            break;
        case harp_type_string:
            {
                long i;

                for (i = 0; i < num_elements; i++)
                {
                    IDL_StrStore(&((IDL_STRING *)destination)[i], ((char **)source)[i]);
                }
            }
            break;
    }

    return 0;
}

static int harp_idl_get_struct_data_for_variable(harp_variable *variable, IDL_StructDefPtr sdef, char *data)
{
    IDL_VPTR field_info;
    char *idl_data;
    int index = 0;
    int i;

    /* name */
    idl_data = data + IDL_StructTagInfoByIndex(sdef, index, IDL_MSG_LONGJMP, &field_info);
    IDL_StrStore((IDL_STRING *)idl_data, variable->name);
    index++;

    /* data */
    idl_data = data + IDL_StructTagInfoByIndex(sdef, index, IDL_MSG_LONGJMP, &field_info);
    if (harp_idl_get_array_data(variable->data_type, variable->num_elements, idl_data, variable->data.ptr) != 0)
    {
        return -1;
    }
    index++;

    /* dimension */
    if (variable->num_dimensions > 0)
    {
        idl_data = data + IDL_StructTagInfoByIndex(sdef, index, IDL_MSG_LONGJMP, &field_info);
        for (i = 0; i < variable->num_dimensions; i++)
        {
            IDL_StrStore(&((IDL_STRING *)idl_data)[variable->num_dimensions - i - 1],
                         (char *)harp_get_dimension_type_name(variable->dimension_type[i]));
        }
        index++;
    }

    /* unit */
    if (variable->unit != NULL)
    {
        idl_data = data + IDL_StructTagInfoByIndex(sdef, index, IDL_MSG_LONGJMP, &field_info);
        IDL_StrStore((IDL_STRING *)idl_data, variable->unit);
        index++;
    }

    if (variable->data_type != harp_type_string)
    {
        /* valid_min */
        if (!harp_is_valid_min_for_type(variable->data_type, variable->valid_min))
        {
            idl_data = data + IDL_StructTagInfoByIndex(sdef, index, IDL_MSG_LONGJMP, &field_info);
            switch (variable->data_type)
            {
                case harp_type_int8:
                    *((UCHAR *)idl_data) = (UCHAR)variable->valid_min.int8_data;
                    break;
                case harp_type_int16:
                    *((int16_t *)idl_data) = variable->valid_min.int16_data;
                    break;
                case harp_type_int32:
                    *((int32_t *)idl_data) = variable->valid_min.int32_data;
                    break;
                case harp_type_float:
                    *((float *)idl_data) = variable->valid_min.float_data;
                    break;
                case harp_type_double:
                    *((double *)idl_data) = variable->valid_min.double_data;
                    break;
                case harp_type_string:
                    assert(0);
                    exit(1);
            }
            index++;
        }

        /* valid_max */
        if (!harp_is_valid_max_for_type(variable->data_type, variable->valid_max))
        {
            idl_data = data + IDL_StructTagInfoByIndex(sdef, index, IDL_MSG_LONGJMP, &field_info);
            switch (variable->data_type)
            {
                case harp_type_int8:
                    *((UCHAR *)idl_data) = (UCHAR)variable->valid_max.int8_data;
                    break;
                case harp_type_int16:
                    *((int16_t *)idl_data) = variable->valid_max.int16_data;
                    break;
                case harp_type_int32:
                    *((int32_t *)idl_data) = variable->valid_max.int32_data;
                    break;
                case harp_type_float:
                    *((float *)idl_data) = variable->valid_max.float_data;
                    break;
                case harp_type_double:
                    *((double *)idl_data) = variable->valid_max.double_data;
                    break;
                case harp_type_string:
                    assert(0);
                    exit(1);
            }
            index++;
        }
    }

    /* description */
    if (variable->description != NULL)
    {
        idl_data = data + IDL_StructTagInfoByIndex(sdef, index, IDL_MSG_LONGJMP, &field_info);
        IDL_StrStore((IDL_STRING *)idl_data, variable->description);
        index++;
    }

    /* enum */
    if (variable->num_enum_values > 0)
    {
        idl_data = data + IDL_StructTagInfoByIndex(sdef, index, IDL_MSG_LONGJMP, &field_info);
        for (i = 0; i < variable->num_enum_values; i++)
        {
            IDL_StrStore(&((IDL_STRING *)idl_data)[i], variable->enum_name[i]);
        }
        index++;
    }

    return 0;
}

static int harp_idl_get_struct_data_for_product(harp_product *product, IDL_StructDefPtr sdef, char *data)
{
    IDL_VPTR field_info;
    char *idl_data;
    int index;

    for (index = 0; index < product->num_variables; index++)
    {
        idl_data = data + IDL_StructTagInfoByIndex(sdef, index, IDL_MSG_LONGJMP, &field_info);
        if (harp_idl_get_struct_data_for_variable(product->variable[index], field_info->value.s.sdef, idl_data) != 0)
        {
            return -1;
        }
    }
    if (product->source_product != NULL)
    {
        idl_data = data + IDL_StructTagInfoByIndex(sdef, index, IDL_MSG_LONGJMP, &field_info);
        IDL_StrStore((IDL_STRING *)idl_data, product->source_product);
        index++;
    }
    if (product->history != NULL)
    {
        idl_data = data + IDL_StructTagInfoByIndex(sdef, index, IDL_MSG_LONGJMP, &field_info);
        IDL_StrStore((IDL_STRING *)idl_data, product->history);
        index++;
    }

    return 0;
}

/* Create an IDL record with the data from a HARP product */
static IDL_VPTR harp_idl_get_record(harp_product *product)
{
    IDL_StructDefPtr sdef = NULL;
    IDL_VPTR idl_record;
    char *data;

    if (harp_idl_get_struct_def_for_product(product, &sdef) != 0)
    {
        return harp_idl_get_error_struct(harp_errno);
    }
    data = IDL_MakeTempStructVector(sdef, 1, &idl_record, IDL_TRUE);
    if (harp_idl_get_struct_data_for_product(product, sdef, data) != 0)
    {
        IDL_Deltmp(idl_record);
        return harp_idl_get_error_struct(harp_errno);
    }

    return idl_record;
}

static int harp_idl_get_variable(IDL_VPTR idl_record, char *data, harp_variable **variable)
{
    harp_data_type valid_min_data_type;
    harp_data_type valid_max_data_type;
    harp_data_type data_type;
    IDL_StructDefPtr sdef;
    const char *variable_name = NULL;
    const char *description = NULL;
    const char *unit = NULL;
    char *idl_data;
    IDL_VPTR field_info;
    harp_dimension_type dimension_type[HARP_MAX_NUM_DIMS];
    long dim[HARP_MAX_NUM_DIMS];
    int num_dims;
    int num_fields;
    int data_index = -1;
    int dimension_index = -1;
    int enum_index = -1;
    int valid_min_index = -1;
    int valid_max_index = -1;
    int index;
    int i;

    if ((idl_record->flags & IDL_V_STRUCT) == 0)
    {
        harp_set_error(HARP_IDL_ERR_INVALID_VARIABLE, "field is not a structure");
        return -1;
    }
    if (idl_record->value.s.arr->n_dim > 1 || idl_record->value.s.arr->dim[0] > 1)
    {
        harp_set_error(HARP_IDL_ERR_INVALID_VARIABLE, "field should be a single structure");
        return -1;
    }
    sdef = idl_record->value.s.sdef;
    num_fields = IDL_StructNumTags(sdef);
    if (num_fields <= 0)
    {
        harp_set_error(HARP_IDL_ERR_INVALID_VARIABLE, NULL);
        return -1;
    }

    for (index = 0; index < num_fields; index++)
    {
        const char *field_name;

        idl_data = data + IDL_StructTagInfoByIndex(sdef, index, IDL_MSG_LONGJMP, &field_info);

        field_name = IDL_StructTagNameByIndex(sdef, index, IDL_MSG_LONGJMP, NULL);
        if (strcasecmp(field_name, "name") == 0)
        {
            if (field_info->type != IDL_TYP_STRING || (field_info->flags & IDL_V_ARR) != 0)
            {
                harp_set_error(HARP_IDL_ERR_INVALID_PRODUCT, "invalid type for variable field 'name'");
                return -1;
            }
            variable_name = IDL_STRING_STR((IDL_STRING *)idl_data);
        }
        else if (strcasecmp(field_name, "data") == 0)
        {
            data_index = index;

            switch (field_info->type)
            {
                case IDL_TYP_BYTE:
                    data_type = harp_type_int8;
                    break;
                case IDL_TYP_INT:
                    data_type = harp_type_int16;
                    break;
                case IDL_TYP_LONG:
                    data_type = harp_type_int32;
                    break;
                case IDL_TYP_FLOAT:
                    data_type = harp_type_float;
                    break;
                case IDL_TYP_DOUBLE:
                    data_type = harp_type_double;
                    break;
                case IDL_TYP_STRING:
                    data_type = harp_type_string;
                    break;
                default:
                    harp_set_error(HARP_IDL_ERR_INVALID_VARIABLE, "invalid data type for variable field 'data'");
                    return -1;
            }

            num_dims = 0;
            if ((field_info->flags & IDL_V_ARR) != 0)
            {
                num_dims = field_info->value.arr->n_dim;
                if (num_dims > HARP_MAX_NUM_DIMS)
                {
                    harp_set_error(HARP_IDL_ERR_INVALID_VARIABLE, "number of dimensions (%d) exceeds maximum (%d)",
                                   num_dims, HARP_MAX_NUM_DIMS);
                    return -1;
                }
                for (i = 0; i < num_dims; i++)
                {
                    dim[num_dims - i - 1] = (long)field_info->value.arr->dim[i];
                }
            }
        }
        else if (strcasecmp(field_name, "dimension") == 0)
        {
            dimension_index = index;

            if (field_info->type != IDL_TYP_STRING)
            {
                harp_set_error(HARP_IDL_ERR_INVALID_PRODUCT, "invalid type for variable field 'dimension'");
                return -1;
            }
        }
        else if (strcasecmp(field_name, "unit") == 0)
        {
            if (field_info->type != IDL_TYP_STRING || (field_info->flags & IDL_V_ARR) != 0)
            {
                harp_set_error(HARP_IDL_ERR_INVALID_PRODUCT, "invalid type for variable field 'unit'");
                return -1;
            }
            unit = IDL_STRING_STR((IDL_STRING *)idl_data);
        }
        else if (strcasecmp(field_name, "valid_min") == 0)
        {
            valid_min_index = index;
            switch (field_info->type)
            {
                case IDL_TYP_BYTE:
                    valid_min_data_type = harp_type_int8;
                    break;
                case IDL_TYP_INT:
                    valid_min_data_type = harp_type_int16;
                    break;
                case IDL_TYP_LONG:
                    valid_min_data_type = harp_type_int32;
                    break;
                case IDL_TYP_FLOAT:
                    valid_min_data_type = harp_type_float;
                    break;
                case IDL_TYP_DOUBLE:
                    valid_min_data_type = harp_type_double;
                    break;
                default:
                    harp_set_error(HARP_IDL_ERR_INVALID_VARIABLE, "invalid data type for variable field 'valid_min'");
                    return -1;
            }
        }
        else if (strcasecmp(field_name, "valid_max") == 0)
        {
            valid_max_index = index;
            switch (field_info->type)
            {
                case IDL_TYP_BYTE:
                    valid_max_data_type = harp_type_int8;
                    break;
                case IDL_TYP_INT:
                    valid_max_data_type = harp_type_int16;
                    break;
                case IDL_TYP_LONG:
                    valid_max_data_type = harp_type_int32;
                    break;
                case IDL_TYP_FLOAT:
                    valid_max_data_type = harp_type_float;
                    break;
                case IDL_TYP_DOUBLE:
                    valid_max_data_type = harp_type_double;
                    break;
                default:
                    harp_set_error(HARP_IDL_ERR_INVALID_VARIABLE, "invalid data type for variable field 'valid_max'");
                    return -1;
            }
        }
        else if (strcasecmp(field_name, "description") == 0)
        {
            if (field_info->type != IDL_TYP_STRING || (field_info->flags & IDL_V_ARR) != 0)
            {
                harp_set_error(HARP_IDL_ERR_INVALID_PRODUCT, "invalid type for variable field 'description'");
                return -1;
            }
            description = IDL_STRING_STR((IDL_STRING *)idl_data);
        }
        else if (strcasecmp(field_name, "enum") == 0)
        {
            enum_index = index;

            if (field_info->type != IDL_TYP_STRING)
            {
                harp_set_error(HARP_IDL_ERR_INVALID_PRODUCT, "invalid type for variable field 'enum'");
                return -1;
            }
        }
        else
        {
            harp_set_error(HARP_IDL_ERR_INVALID_VARIABLE, "invalid variable field '%s'", field_name);
            return -1;
        }
    }

    if (variable_name == NULL)
    {
        harp_set_error(HARP_IDL_ERR_INVALID_VARIABLE, "missing mandatory variable field 'name'");
        return -1;
    }
    if (data_index < 0)
    {
        harp_set_error(HARP_IDL_ERR_INVALID_VARIABLE, "missing mandatory variable field 'data'");
        return -1;
    }
    if (num_dims > 0)
    {
        if (dimension_index < 0)
        {
            harp_set_error(HARP_IDL_ERR_INVALID_VARIABLE, "missing mandatory variable field 'dimension'");
            return -1;
        }
        idl_data = data + IDL_StructTagInfoByIndex(sdef, dimension_index, IDL_MSG_LONGJMP, &field_info);
        if ((field_info->flags & IDL_V_ARR) != 0)
        {
            if (field_info->value.arr->n_dim != 1)
            {
                harp_set_error(HARP_IDL_ERR_INVALID_PRODUCT, "invalid type for variable field 'dimension'");
                return -1;
            }
            if (field_info->value.arr->dim[0] != num_dims)
            {
                harp_set_error(HARP_IDL_ERR_INVALID_VARIABLE,
                               "invalid number of entries for variable field 'dimension' (expected %d)", num_dims);
                return -1;
            }
            for (i = 0; i < num_dims; i++)
            {
                if (harp_parse_dimension_type(IDL_STRING_STR(&((IDL_STRING *)idl_data)[i]),
                                              &dimension_type[num_dims - i - 1]) != 0)
                {
                    harp_set_error(HARP_IDL_ERR_INVALID_VARIABLE, "invalid dimension type ('%s')",
                                   IDL_STRING_STR(&((IDL_STRING *)idl_data)[i]));
                    return -1;
                }
            }
        }
        else
        {
            if (num_dims != 1)
            {
                harp_set_error(HARP_IDL_ERR_INVALID_VARIABLE,
                               "invalid number of entries for variable field 'dimension' (expected 1)");
                return -1;

            }
            if (harp_parse_dimension_type(IDL_STRING_STR((IDL_STRING *)idl_data), &dimension_type[0]) != 0)
            {
                harp_set_error(HARP_IDL_ERR_INVALID_VARIABLE, "invalid dimension type ('%s')",
                               IDL_STRING_STR(&field_info->value.str));
                return -1;
            }
        }
    }
    else if (dimension_index >= 0)
    {
        harp_set_error(HARP_IDL_ERR_INVALID_VARIABLE, "unexpected variable field 'dimension' for scalar variable");
        return -1;
    }

    /* create variable */
    if (harp_variable_new(variable_name, data_type, num_dims, dimension_type, dim, variable) != 0)
    {
        return -1;
    }

    /* data */
    idl_data = data + IDL_StructTagInfoByIndex(sdef, data_index, IDL_MSG_LONGJMP, &field_info);
    if (harp_idl_get_array_data((*variable)->data_type, (*variable)->num_elements, (*variable)->data.ptr, idl_data) !=
        0)
    {
        harp_variable_delete(*variable);
        return -1;
    }

    /* unit */
    if (unit != NULL)
    {
        if (harp_variable_set_unit(*variable, unit) != 0)
        {
            harp_variable_delete(*variable);
            return -1;
        }
    }

    /* valid_min */
    if (valid_min_index >= 0)
    {
        if (valid_min_data_type != (*variable)->data_type)
        {
            harp_set_error(HARP_IDL_ERR_INVALID_VARIABLE, "invalid data type for variable field 'valid_min' "
                           "(should match data type of 'data')");
            harp_variable_delete(*variable);
            return -1;
        }
        idl_data = data + IDL_StructTagInfoByIndex(sdef, valid_min_index, IDL_MSG_LONGJMP, &field_info);
        switch (valid_min_data_type)
        {
            case harp_type_int8:
                (*variable)->valid_min.int8_data = (int8_t)*((UCHAR *)idl_data);
                break;
            case harp_type_int16:
                (*variable)->valid_min.int16_data = *((int16_t *)idl_data);
                break;
            case harp_type_int32:
                (*variable)->valid_min.int32_data = *((int32_t *)idl_data);
                break;
            case harp_type_float:
                (*variable)->valid_min.float_data = *((float *)idl_data);
                break;
            case harp_type_double:
                (*variable)->valid_min.double_data = *((double *)idl_data);
                break;
            case harp_type_string:
                assert(0);
                exit(1);
        }
    }

    /* valid_max */
    if (valid_max_index >= 0)
    {
        if (valid_max_data_type != (*variable)->data_type)
        {
            harp_set_error(HARP_IDL_ERR_INVALID_VARIABLE, "invalid data type for variable field 'valid_max' "
                           "(should match data type of 'data')");
            harp_variable_delete(*variable);
            return -1;
        }
        idl_data = data + IDL_StructTagInfoByIndex(sdef, valid_max_index, IDL_MSG_LONGJMP, &field_info);
        switch (valid_max_data_type)
        {
            case harp_type_int8:
                (*variable)->valid_max.int8_data = (int8_t)*((UCHAR *)idl_data);
                break;
            case harp_type_int16:
                (*variable)->valid_max.int16_data = *((int16_t *)idl_data);
                break;
            case harp_type_int32:
                (*variable)->valid_max.int32_data = *((int32_t *)idl_data);
                break;
            case harp_type_float:
                (*variable)->valid_max.float_data = *((float *)idl_data);
                break;
            case harp_type_double:
                (*variable)->valid_max.double_data = *((double *)idl_data);
                break;
            case harp_type_string:
                assert(0);
                exit(1);
        }
    }

    /* description */
    if (description != NULL)
    {
        if (harp_variable_set_description(*variable, description) != 0)
        {
            harp_variable_delete(*variable);
            return -1;
        }
    }

    /* enum */
    if (enum_index >= 0)
    {
        const char **enum_name;
        long num_enum_values = 1;

        idl_data = data + IDL_StructTagInfoByIndex(sdef, enum_index, IDL_MSG_LONGJMP, &field_info);
        if ((field_info->flags & IDL_V_ARR) != 0)
        {
            if (field_info->value.arr->n_dim != 1)
            {
                harp_set_error(HARP_IDL_ERR_INVALID_PRODUCT, "invalid type for variable field 'enum'");
                harp_variable_delete(*variable);
                return -1;
            }
            num_enum_values = (long)field_info->value.arr->dim[0];
        }
        enum_name = malloc(num_enum_values * sizeof(char *));
        if (enum_name == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           num_enum_values * sizeof(char *), __FILE__, __LINE__);
            harp_variable_delete(*variable);
            return -1;
        }

        for (i = 0; i < num_enum_values; i++)
        {
            enum_name[i] = IDL_STRING_STR(&((IDL_STRING *)idl_data)[i]);
        }

        if (harp_variable_set_enumeration_values(*variable, num_enum_values, enum_name) != 0)
        {
            free(enum_name);
            harp_variable_delete(*variable);
            return -1;
        }
        free(enum_name);
    }

    return 0;
}

/* Copy the data from an IDL record to a HARP product */
static int harp_idl_get_product(IDL_VPTR idl_record, harp_product *product)
{
    IDL_StructDefPtr sdef;
    int num_fields;
    int index;

    if ((idl_record->flags & IDL_V_STRUCT) == 0)
    {
        harp_set_error(HARP_IDL_ERR_INVALID_PRODUCT, NULL);
        return -1;
    }
    if (idl_record->value.s.arr->n_dim > 1 || idl_record->value.s.arr->dim[0] > 1)
    {
        harp_set_error(HARP_IDL_ERR_INVALID_PRODUCT, NULL);
        return -1;
    }
    sdef = idl_record->value.s.sdef;
    num_fields = IDL_StructNumTags(sdef);
    if (num_fields <= 0)
    {
        harp_set_error(HARP_IDL_ERR_INVALID_PRODUCT, NULL);
        return -1;
    }

    for (index = 0; index < num_fields; index++)
    {
        const char *field_name;
        char *idl_data;
        IDL_VPTR field_info;

        idl_data = ((char *)idl_record->value.s.arr->data) +
            IDL_StructTagInfoByIndex(sdef, index, IDL_MSG_LONGJMP, &field_info);

        field_name = IDL_StructTagNameByIndex(sdef, index, IDL_MSG_LONGJMP, NULL);
        if (strcasecmp(field_name, "source_product") == 0)
        {
            if (field_info->type != IDL_TYP_STRING || (field_info->flags & IDL_V_ARR) != 0)
            {
                harp_set_error(HARP_IDL_ERR_INVALID_PRODUCT, "invalid type for global attribute 'source_product'");
                return -1;
            }
            if (harp_product_set_source_product(product, IDL_STRING_STR((IDL_STRING *)idl_data)) != 0)
            {
                return -1;
            }
        }
        else if (strcasecmp(field_name, "history") == 0)
        {
            if (field_info->type != IDL_TYP_STRING || (field_info->flags & IDL_V_ARR) != 0)
            {
                harp_set_error(HARP_IDL_ERR_INVALID_PRODUCT, "invalid type for global attribute 'history'");
                return -1;
            }
            if (harp_product_set_history(product, IDL_STRING_STR((IDL_STRING *)idl_data)) != 0)
            {
                return -1;
            }
        }
        else
        {
            harp_variable *variable;

            if (harp_idl_get_variable(field_info, idl_data, &variable) != 0)
            {
                harp_add_error_message(" for product field '%s'", field_name);
                return -1;
            }
            if (harp_product_add_variable(product, variable) != 0)
            {
                return -1;
            }
        }
    }

    return 0;
}

static IDL_VPTR harp_idl_export(int argc, IDL_VPTR *argv)
{
    harp_product *product;
    const char *format;

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
    if (harp_idl_get_product(argv[0], product) != 0)
    {
        harp_product_delete(product);
        return harp_idl_get_error_struct(harp_errno);
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

static IDL_VPTR harp_idl_version(int argc, IDL_VPTR *argv)
{
    assert(argc == 0);
    (void)argv;
    if (harp_idl_init() != 0)
    {
        return harp_idl_get_error_struct(harp_errno);
    }

    return IDL_StrToSTRING(VERSION);
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
        {{harp_idl_version}, "HARP_VERSION", 0, 0, 0, 0}
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
