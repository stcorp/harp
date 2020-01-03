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

#include "harp-matlab.h"

#include <string.h>

static void harp_matlab_add_harp_product_variable(mxArray *mx_struct, harp_product **product, int index)
{
    harp_variable *variable = (**product).variable[index];
    long dim[HARP_MAX_NUM_DIMS];
    harp_dimension_type dim_type[HARP_MAX_NUM_DIMS];
    mwSize matlabdim[HARP_MAX_NUM_DIMS];
    mwSize matlabdim_type[HARP_MAX_NUM_DIMS];
    mxArray *mx_data = NULL;
    mxArray *struct_data = NULL;
    int num_dims = variable->num_dimensions;
    long num_elements = variable->num_elements;
    int variable_is_scalar = 0;
    long i;

    if (harp_product_get_variable_by_name(*product, variable->name, &variable) != 0)
    {
        harp_matlab_harp_error();
    }
    if (harp_product_get_variable_index_by_name(*product, variable->name, &index) != 0)
    {
        harp_matlab_harp_error();
    }

    mxAssert(num_dims >= 0, "Number of dimensions is invalid");
    mxAssert(num_dims <= HARP_MAX_NUM_DIMS, "Number of dimensions is too high");
    mxAssert(num_elements > 0, "Number of elements in array is zero");

    for (i = 0; i < num_dims; i++)
    {
        dim[i] = variable->dimension[i];
        dim_type[i] = variable->dimension_type[i];
    }

    /* top-level */
    struct_data = mxCreateStructMatrix(1, 1, 0, NULL);

    if (variable->description != NULL)
    {
        mxAddField(struct_data, "description");
        mxSetField(struct_data, 0, "description", mxCreateString(variable->description));
    }

    if (variable->unit != NULL)
    {
        mxAddField(struct_data, "unit");
        mxSetField(struct_data, 0, "unit", mxCreateString(variable->unit));
    }

    /* MATLAB does not allow creation of arrays with num_dims == 0 */
    if (num_dims == 0 && variable->data_type != harp_type_string)
    {
        variable_is_scalar = true;
        dim[0] = 1;
        num_dims = 1;
    }

    for (i = 0; i < num_dims; i++)
    {
        matlabdim[i] = (mwSize) dim[num_dims - i - 1];
    }

    if (!variable_is_scalar)
    {
        mxArray *dim_info_type;

        matlabdim_type[0] = num_dims;
        dim_info_type = mxCreateCellArray(1, matlabdim_type);

        for (i = 0; i < num_dims; i++)
        {
            switch (dim_type[num_dims - i - 1])
            {
                case -1:
                    mxSetCell(dim_info_type, (mwIndex) i, mxCreateString("independent"));
                    break;
                case 0:
                    mxSetCell(dim_info_type, (mwIndex) i, mxCreateString("time"));
                    break;
                case 1:
                    mxSetCell(dim_info_type, (mwIndex) i, mxCreateString("latitude"));
                    break;
                case 2:
                    mxSetCell(dim_info_type, (mwIndex) i, mxCreateString("longitude"));
                    break;
                case 3:
                    mxSetCell(dim_info_type, (mwIndex) i, mxCreateString("vertical"));
                    break;
                case 4:
                    mxSetCell(dim_info_type, (mwIndex) i, mxCreateString("spectral"));
                    break;
            }

        }
        mxAddField(struct_data, "dimension");
        mxSetField(struct_data, 0, "dimension", dim_info_type);
    }

    switch (variable->data_type)
    {
        case harp_type_int8:
            mx_data = mxCreateNumericArray(num_dims, matlabdim, mxINT8_CLASS, mxREAL);
            memcpy(mxGetData(mx_data), variable->data.int8_data, num_elements * sizeof(int8_t));
            break;
        case harp_type_int16:
            mx_data = mxCreateNumericArray(num_dims, matlabdim, mxINT16_CLASS, mxREAL);
            memcpy(mxGetData(mx_data), variable->data.int16_data, num_elements * sizeof(int16_t));
            break;
        case harp_type_int32:
            mx_data = mxCreateNumericArray(num_dims, matlabdim, mxINT32_CLASS, mxREAL);
            memcpy(mxGetData(mx_data), variable->data.int32_data, num_elements * sizeof(int32_t));
            break;
        case harp_type_float:
            mx_data = mxCreateNumericArray(num_dims, matlabdim, mxSINGLE_CLASS, mxREAL);
            memcpy(mxGetData(mx_data), variable->data.float_data, num_elements * sizeof(float));
            break;
        case harp_type_double:
            mx_data = mxCreateNumericArray(num_dims, matlabdim, mxDOUBLE_CLASS, mxREAL);
            memcpy(mxGetData(mx_data), variable->data.double_data, num_elements * sizeof(double));
            break;
        case harp_type_string:
            if (num_dims == 0)
            {
                mx_data = mxCreateString(variable->data.string_data[0]);
            }
            else
            {
                mx_data = mxCreateCellArray(num_dims, matlabdim);
                for (i = 0; i < num_elements; i++)
                {
                    mxSetCell(mx_data, i, mxCreateString(variable->data.string_data[i]));
                }
            }
            break;
    }

    mxAddField(struct_data, "data");
    mxSetField(struct_data, 0, "data", mx_data);

    /* back to the top-level again */
    mxAddField(mx_struct, variable->name);
    mxSetField(mx_struct, 0, variable->name, struct_data);
}

mxArray *harp_matlab_get_product(harp_product **product)
{
    char *source_product = (**product).source_product;
    char *history = (**product).history;
    int num_variables = (**product).num_variables;
    mxArray *mx_data = NULL;
    int index;

    mx_data = mxCreateStructMatrix(1, 1, 0, NULL);

    /* add meta information for each product */
    if (source_product != NULL)
    {
        mxAddField(mx_data, "source_product");
        mxSetField(mx_data, 0, "source_product", mxCreateString(source_product));
    }
    if (history != NULL)
    {
        mxAddField(mx_data, "history");
        mxSetField(mx_data, 0, "history", mxCreateString(history));
    }

    /* add variables for each product */
    for (index = 0; index < num_variables; index++)
    {
        harp_matlab_add_harp_product_variable(mx_data, product, index);
    }

    return mx_data;
}


static char *get_matlab_string_value(mxArray *mx_data)
{
    char *string_data;
    int buflen;

    buflen = (int)((mxGetNumberOfElements(mx_data) * sizeof(mxChar)) + 1);
    string_data = mxCalloc(buflen, 1);
    mxGetString(mx_data, string_data, buflen);

    return string_data;
}


static void harp_matlab_add_matlab_product_variable(harp_product **product, const char *variable_name,
                                                    mxArray *mx_variable)
{
    mxClassID class = 0;
    harp_variable *variable_new;
    char *string_data;
    long dim[HARP_MAX_NUM_DIMS];
    harp_dimension_type dim_type[HARP_MAX_NUM_DIMS];
    int matlab_num_dims;
    int harp_num_dims = 0;
    char *des_string = NULL;
    char *unit_string = NULL;
    mxArray *datastructure = NULL;
    mxArray *meta_variable_des = NULL;
    mxArray *meta_variable_unit = NULL;
    mxArray *meta_variable_dim_type = NULL;
    long num_elements;
    long i;

    /* get top level from matlab */
    if (!mxIsStruct(mx_variable))
    {
        mexErrMsgTxt("This variable is not a struct.");
    }

    datastructure = mxGetField(mx_variable, 0, "data");
    if (datastructure != NULL)
    {
        class = mxGetClassID(datastructure);
    }
    else
    {
        mexErrMsgTxt("Field with data is missing.");
    }

    /* set meta info for each variable */
    meta_variable_des = mxGetField(mx_variable, 0, "description");
    if (meta_variable_des != NULL)
    {
        des_string = mxArrayToString(meta_variable_des);
    }

    meta_variable_unit = mxGetField(mx_variable, 0, "unit");
    if (meta_variable_unit != NULL)
    {
        unit_string = mxArrayToString(meta_variable_unit);
    }

    num_elements = (long)mxGetNumberOfElements(datastructure);
    if (num_elements == 0)
    {
        mexErrMsgTxt("Empty arrays are not allowed for a product variable.");
    }

    /* dimension type */
    meta_variable_dim_type = mxGetField(mx_variable, 0, "dimension");

    harp_num_dims = 0;
    if (meta_variable_dim_type != NULL)
    {
        int num_items = (int)mxGetNumberOfElements(meta_variable_dim_type);
        char *dimtypevalue = NULL;

        for (i = 0; i < num_items; i++)
        {
            mxArray *mx_cell = mxGetCell(meta_variable_dim_type, num_items - i - 1);

            dimtypevalue = get_matlab_string_value(mx_cell);
            if (dimtypevalue != NULL)
            {
                if (strcmp(dimtypevalue, "independent") == 0)
                {
                    dim_type[i] = harp_dimension_independent;
                    harp_num_dims++;
                }
                else if (strcmp(dimtypevalue, "time") == 0)
                {
                    dim_type[i] = harp_dimension_time;
                    harp_num_dims++;
                }
                else if (strcmp(dimtypevalue, "latitude") == 0)
                {
                    dim_type[i] = harp_dimension_latitude;
                    harp_num_dims++;
                }
                else if (strcmp(dimtypevalue, "longitude") == 0)
                {
                    dim_type[i] = harp_dimension_longitude;
                    harp_num_dims++;
                }
                else if (strcmp(dimtypevalue, "vertical") == 0)
                {
                    dim_type[i] = harp_dimension_vertical;
                    harp_num_dims++;
                }
                else if (strcmp(dimtypevalue, "spectral") == 0)
                {
                    dim_type[i] = harp_dimension_spectral;
                    harp_num_dims++;
                }
            }
        }
    }
    mxAssert(harp_num_dims >= 0, "Number of HARP dimensions is invalid");
    mxAssert(harp_num_dims <= HARP_MAX_NUM_DIMS, "Number of HARP dimensions is too high");

    /* set value to variables after the meta data is ready */
    matlab_num_dims = (int)mxGetNumberOfDimensions(datastructure);
    while (matlab_num_dims > harp_num_dims && mxGetDimensions(datastructure)[matlab_num_dims - 1] == 1)
    {
        matlab_num_dims--;
    }
    mxAssert(harp_num_dims == matlab_num_dims, "Number of HARP dimensions should match dimensions of variable");
    for (i = 0; i < matlab_num_dims; i++)
    {
        dim[i] = (long)mxGetDimensions(datastructure)[matlab_num_dims - i - 1];
    }

    switch (class)
    {
        case mxINT8_CLASS:
            if (harp_variable_new(variable_name, harp_type_int8, harp_num_dims, dim_type, dim, &variable_new) != 0)
            {
                harp_matlab_harp_error();
            }
            memcpy(variable_new->data.int8_data, mxGetData(datastructure), num_elements * sizeof(int8_t));
            if (harp_product_add_variable(*product, variable_new) != 0)
            {
                harp_matlab_harp_error();
            }

            /* assigning meta data */
            if (unit_string != NULL)
            {
                if (harp_variable_set_unit(variable_new, unit_string) != 0)
                {
                    harp_matlab_harp_error();
                }
            }
            if (des_string != NULL)
            {
                if (harp_variable_set_description(variable_new, des_string) != 0)
                {
                    harp_matlab_harp_error();
                }
            }

            mxFree(unit_string);
            mxFree(des_string);
            break;
        case mxINT16_CLASS:
            if (harp_variable_new(variable_name, harp_type_int16, harp_num_dims, dim_type, dim, &variable_new) != 0)
            {
                harp_matlab_harp_error();
            }
            memcpy(variable_new->data.int16_data, mxGetData(datastructure), num_elements * sizeof(int16_t));
            if (harp_product_add_variable(*product, variable_new) != 0)
            {
                harp_matlab_harp_error();
            }

            /* assigning meta data */
            if (unit_string != NULL)
            {
                if (harp_variable_set_unit(variable_new, unit_string) != 0)
                {
                    harp_matlab_harp_error();
                }
            }
            if (des_string != NULL)
            {
                if (harp_variable_set_description(variable_new, des_string) != 0)
                {
                    harp_matlab_harp_error();
                }
            }

            mxFree(unit_string);
            mxFree(des_string);
            break;
        case mxINT32_CLASS:
            if (harp_variable_new(variable_name, harp_type_int32, harp_num_dims, dim_type, dim, &variable_new) != 0)
            {
                harp_matlab_harp_error();
            }
            memcpy(variable_new->data.int32_data, mxGetData(datastructure), num_elements * sizeof(int32_t));
            if (harp_product_add_variable(*product, variable_new) != 0)
            {
                harp_matlab_harp_error();
            }

            /* assigning meta data */
            if (unit_string != NULL)
            {
                if (harp_variable_set_unit(variable_new, unit_string) != 0)
                {
                    harp_matlab_harp_error();
                }
            }
            if (des_string != NULL)
            {
                if (harp_variable_set_description(variable_new, des_string) != 0)
                {
                    harp_matlab_harp_error();
                }
            }

            mxFree(unit_string);
            mxFree(des_string);
            break;
        case mxSINGLE_CLASS:
            if (harp_variable_new(variable_name, harp_type_float, harp_num_dims, dim_type, dim, &variable_new) != 0)
            {
                harp_matlab_harp_error();
            }
            memcpy(variable_new->data.float_data, mxGetData(datastructure), num_elements * sizeof(float));
            if (harp_product_add_variable(*product, variable_new) != 0)
            {
                harp_matlab_harp_error();
            }

            /* assigning meta data */
            if (unit_string != NULL)
            {
                if (harp_variable_set_unit(variable_new, unit_string) != 0)
                {
                    harp_matlab_harp_error();
                }
            }
            if (des_string != NULL)
            {
                if (harp_variable_set_description(variable_new, des_string) != 0)
                {
                    harp_matlab_harp_error();
                }
            }
            mxFree(unit_string);
            mxFree(des_string);
            break;
        case mxDOUBLE_CLASS:
            if (harp_variable_new(variable_name, harp_type_double, harp_num_dims, dim_type, dim, &variable_new) != 0)
            {
                harp_matlab_harp_error();
            }
            memcpy(variable_new->data.double_data, mxGetData(datastructure), num_elements * sizeof(double));
            if (harp_product_add_variable(*product, variable_new) != 0)
            {
                harp_matlab_harp_error();
            }

            /* assigning meta data */
            if (unit_string != NULL)
            {
                if (harp_variable_set_unit(variable_new, unit_string) != 0)
                {
                    harp_matlab_harp_error();
                }
            }
            if (des_string != NULL)
            {
                if (harp_variable_set_description(variable_new, des_string) != 0)
                {
                    harp_matlab_harp_error();
                }
            }

            mxFree(unit_string);
            mxFree(des_string);
            break;
        case mxCHAR_CLASS:
            if (mxGetNumberOfDimensions(datastructure) != 2 || mxGetDimensions(datastructure)[0] != 1)
            {
                mexErrMsgTxt("Multi-dimensional string arrays are not allowed. Use a cell array of strings instead.");
            }

            if (harp_variable_new(variable_name, harp_type_string, harp_num_dims, dim_type, dim, &variable_new) != 0)
            {
                harp_matlab_harp_error();
            }
            if (harp_product_add_variable(*product, variable_new) != 0)
            {
                harp_matlab_harp_error();
            }

            string_data = get_matlab_string_value(datastructure);
            if (harp_variable_set_string_data_element(variable_new, 0, string_data) != 0)
            {
                harp_matlab_harp_error();
            }
            mxFree(string_data);

            /* assigning meta data */
            if (unit_string != NULL)
            {
                if (harp_variable_set_unit(variable_new, unit_string) != 0)
                {
                    harp_matlab_harp_error();
                }
            }
            if (des_string != NULL)
            {
                if (harp_variable_set_description(variable_new, des_string) != 0)
                {
                    harp_matlab_harp_error();
                }
            }

            mxFree(unit_string);
            mxFree(des_string);
            break;
        case mxCELL_CLASS:
            {
                mxArray *mx_cell;

                for (i = 0; i < num_elements; i++)
                {
                    mx_cell = mxGetCell(datastructure, i);
                    if (mxGetClassID(mx_cell) != mxCHAR_CLASS || mxGetNumberOfDimensions(mx_cell) != 2 ||
                        mxGetDimensions(mx_cell)[0] > 1)
                    {
                        mexErrMsgTxt("Cell arrays are only allowed for one dimensional string data.");
                    }
                }
                if (harp_variable_new(variable_name, harp_type_string, harp_num_dims, dim_type, dim, &variable_new) !=
                    0)
                {
                    harp_matlab_harp_error();
                }
                if (harp_product_add_variable(*product, variable_new) != 0)
                {
                    harp_matlab_harp_error();
                }

                if (unit_string != NULL)
                {
                    if (harp_variable_set_unit(variable_new, unit_string) != 0)
                    {
                        harp_matlab_harp_error();
                    }
                }
                if (des_string != NULL)
                {
                    if (harp_variable_set_description(variable_new, des_string) != 0)
                    {
                        harp_matlab_harp_error();
                    }
                }

                mxFree(unit_string);
                mxFree(des_string);

                for (i = 0; i < num_elements; i++)
                {
                    mx_cell = mxGetCell(datastructure, i);
                    string_data = get_matlab_string_value(mx_cell);
                    if (harp_variable_set_string_data_element(variable_new, i, string_data) != 0)
                    {
                        mxFree(string_data);
                        harp_matlab_harp_error();
                    }
                    mxFree(string_data);
                }
            }
            break;
        default:
            mexErrMsgTxt("Unsupported class for variable data.");
            return;
    }

}

harp_product *harp_matlab_set_product(const mxArray *mx_struct)
{
    harp_product *product;
    int num_variables;
    int field_num;

    if (!mxIsStruct(mx_struct))
    {
        mexErrMsgTxt("Not a struct.");
    }
    num_variables = mxGetNumberOfFields(mx_struct);

    if (harp_product_new(&product) != 0)
    {
        harp_matlab_harp_error();
    }

    for (field_num = 0; field_num < num_variables; field_num++)
    {
        const char *variable_name;

        variable_name = mxGetFieldNameByNumber(mx_struct, field_num);

        /* set meta info for each product from matlab input */
        if (strcmp(variable_name, "source_product") == 0)
        {
            mxArray *meta = mxGetFieldByNumber(mx_struct, 0, field_num);
            char *metastring = mxArrayToString(meta);

            if (metastring != NULL)
            {
                if (harp_product_set_source_product(product, metastring) != 0)
                {
                    harp_matlab_harp_error();
                }
            }
        }
        else if (strcmp(variable_name, "history") == 0)
        {
            mxArray *meta = mxGetFieldByNumber(mx_struct, 0, field_num);
            char *metastring = mxArrayToString(meta);

            if (metastring != NULL)
            {
                if (harp_product_set_history(product, metastring) != 0)
                {
                    harp_matlab_harp_error();
                }
            }
        }
        else
        {
            mxArray *mx_variable;

            /* mx_variable is a 1x1 array with fields like description, dimension, data etc. */
            mx_variable = mxGetFieldByNumber(mx_struct, 0, field_num);
            harp_matlab_add_matlab_product_variable(&product, variable_name, mx_variable);
        }
    }

    return product;
}
