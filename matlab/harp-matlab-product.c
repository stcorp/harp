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

#include "harp-matlab.h"

#include <string.h>

#include "harp-utils.c"

static void harp_matlab_add_harp_product_variable(mxArray *mx_struct, harp_product **product, int index)
{

    harp_variable *variable = (**product).variable[index];
    harp_data_type type = variable->data_type;
    const char *variable_name = variable->name;
    harp_array variable_data = variable->data;

    /* add meta info for each variable */
    char *description = variable->description;
    char *unit = variable->unit;

    long dim[HARP_MAX_NUM_DIMS];
    harp_dimension_type dim_type[HARP_MAX_NUM_DIMS];

    mwSize matlabdim[HARP_MAX_NUM_DIMS];
    mwSize matlabdim_type[HARP_MAX_NUM_DIMS];
    int num_dims = variable->num_dimensions;
    long num_elements = variable->num_elements;
    long i;
    short variable_is_scalar = false;

    mxArray *mx_data = NULL;
    mxArray *struct_data = NULL;
    mxArray *string_des = NULL;
    mxArray *string_unit = NULL;

    if (harp_product_get_variable_by_name(*product, variable_name, &variable) != 0)
    {
        harp_matlab_harp_error();
    }
    if (harp_product_get_variable_index_by_name(*product, variable_name, &index) != 0)
    {
        harp_matlab_harp_error();
    }

    mxAssert(num_dims >= 0, "Number of dimensions is invalid");
    mxAssert(num_dims <= HARP_MAX_NUM_DIMS, "Number of dimensions is too high");
    mxAssert(num_elements > 0, "Number of elements in array is zero");


    for (i = 0; i < HARP_MAX_NUM_DIMS; i++)
    {
        dim[i] = variable->dimension[i];
        if (dim[i] > 0)
        {
            dim_type[i] = variable->dimension_type[i];
        }
    }

    /* top-level */
    struct_data = mxCreateStructMatrix(1, 1, 0, NULL);

    string_des = mxCreateString(description);

    mxAddField(struct_data, "description");
    if (description != NULL)
    {
        mxSetField(struct_data, 0, "description", string_des);
    }

    string_unit = mxCreateString(unit);

    mxAddField(struct_data, "unit");
    if (unit != NULL)
    {
        mxSetField(struct_data, 0, "unit", string_unit);
    }

    /* MATLAB does not allow creation of arrays with num_dims == 0 */
    if (num_dims == 0 && type != harp_type_string)
    {
        variable_is_scalar = true;
        dim[num_dims++] = 1;
    }

    for (i = 0; i < num_dims; i++)
    {
        matlabdim[i] = (mwSize) dim[i];
    }

    matlabdim_type[0] = num_dims;
    for (i = 1; i < num_dims; i++)
    {
        matlabdim_type[i] = 0;
    }

    if (!variable_is_scalar)
    {
        mxArray *dim_info_type = mxCreateCellArray(1, matlabdim_type);

        for (i = 0; i < num_dims; i++)
        {
            switch (dim_type[i])
            {
                case -1:
                    {
                        mxSetCell(dim_info_type, (mwIndex) i, mxCreateString("independent"));
                    }
                    break;
                case 0:
                    {
                        mxSetCell(dim_info_type, (mwIndex) i, mxCreateString("time"));
                    }
                    break;
                case 1:
                    {
                        mxSetCell(dim_info_type, (mwIndex) i, mxCreateString("latitude"));
                    }
                    break;
                case 2:
                    {
                        mxSetCell(dim_info_type, (mwIndex) i, mxCreateString("longitude"));
                    }
                    break;
                case 3:
                    {
                        mxSetCell(dim_info_type, (mwIndex) i, mxCreateString("vertical"));
                    }
                    break;
                case 4:
                    {
                        mxSetCell(dim_info_type, (mwIndex) i, mxCreateString("spectral"));
                    }
                    break;
            }

        }
        mxAddField(struct_data, "dimension");
        mxSetField(struct_data, 0, "dimension", dim_info_type);
    }

    switch (type)
    {
        case harp_type_int8:
            {
                long counter = 0;
                int8_t *data;

                mx_data = mxCreateNumericArray(num_dims, matlabdim, mxINT8_CLASS, mxREAL);
                data = mxGetData(mx_data);

                while (counter < num_elements)
                {
                    mwSize j, k;

                    for (j = 0; j < num_elements / matlabdim[num_dims - 1]; j++)
                    {
                        for (k = 0; k < matlabdim[num_dims - 1]; k++)
                        {
                            data[j + k * num_elements / matlabdim[num_dims - 1]] = variable_data.int8_data[counter++];
                        }
                    }
                }

            }
            break;
        case harp_type_int16:
            {
                long counter = 0;
                int16_t *data;

                mx_data = mxCreateNumericArray(num_dims, matlabdim, mxINT16_CLASS, mxREAL);
                data = mxGetData(mx_data);

                while (counter < num_elements)
                {
                    mwSize j, k;

                    for (j = 0; j < num_elements / matlabdim[num_dims - 1]; j++)
                    {
                        for (k = 0; k < matlabdim[num_dims - 1]; k++)
                        {
                            data[j + k * num_elements / matlabdim[num_dims - 1]] = variable_data.int16_data[counter++];
                        }
                    }
                }
            }
            break;
        case harp_type_int32:
            {
                long counter = 0;
                int32_t *data;

                mx_data = mxCreateNumericArray(num_dims, matlabdim, mxINT32_CLASS, mxREAL);
                data = mxGetData(mx_data);

                while (counter < num_elements)
                {
                    mwSize j, k;

                    for (j = 0; j < num_elements / matlabdim[num_dims - 1]; j++)
                    {
                        for (k = 0; k < matlabdim[num_dims - 1]; k++)
                        {
                            data[j + k * num_elements / matlabdim[num_dims - 1]] = variable_data.int32_data[counter++];
                        }
                    }
                }
            }
            break;
        case harp_type_double:
            {
                long counter = 0;
                double *data;

                mx_data = mxCreateNumericArray(num_dims, matlabdim, mxDOUBLE_CLASS, mxREAL);
                data = mxGetData(mx_data);

                while (counter < num_elements)
                {
                    mwSize j, k;

                    for (j = 0; j < num_elements / matlabdim[num_dims - 1]; j++)
                    {
                        for (k = 0; k < matlabdim[num_dims - 1]; k++)
                        {
                            data[j + k * num_elements / matlabdim[num_dims - 1]] = variable_data.double_data[counter++];
                        }
                    }
                }

            }
            break;
        case harp_type_float:
            {
                long counter = 0;
                float *data;

                mx_data = mxCreateNumericArray(num_dims, matlabdim, mxSINGLE_CLASS, mxREAL);
                data = mxGetData(mx_data);

                while (counter < num_elements)
                {
                    mwSize j, k;

                    for (j = 0; j < num_elements / matlabdim[num_dims - 1]; j++)
                    {
                        for (k = 0; k < matlabdim[num_dims - 1]; k++)
                        {
                            data[j + k * num_elements / matlabdim[num_dims - 1]] = variable_data.float_data[counter++];
                        }
                    }
                }

            }
            break;
        case harp_type_string:
            if (num_dims == 0)
            {
                mx_data = mxCreateString(variable_data.string_data[0]);
            }
            else
            {
                mx_data = mxCreateCellArray(num_dims, matlabdim);
                for (i = 0; i < num_elements; i++)
                {
                    mxSetCell(mx_data, coda_c_index_to_fortran_index(num_dims, dim, i),
                              mxCreateString(variable_data.string_data[i]));
                }
            }
            break;
    }



    mxAddField(struct_data, "data");
    mxSetField(struct_data, 0, "data", mx_data);


    /* back to the top-level again */
    mxAddField(mx_struct, variable_name);
    mxSetField(mx_struct, 0, variable_name, struct_data);

}

mxArray *harp_matlab_get_product(harp_product **product)
{
    mxArray *mx_data = NULL;
    int num_variables;
    int index;

    char *source_product = (**product).source_product;
    char *history = (**product).history;

    mxArray *string_source = NULL;
    mxArray *string_his = NULL;

    num_variables = (**product).num_variables;
    if (num_variables == 1)
    {
        harp_matlab_harp_error();
    }

    mx_data = mxCreateStructMatrix(1, 1, 0, NULL);

    /* add meta information for each product */
    string_source = mxCreateString(source_product);
    string_his = mxCreateString(history);

    mxAddField(mx_data, "source_product");
    mxSetField(mx_data, 0, "source_product", string_source);

    mxAddField(mx_data, "history");
    if (history != NULL)
    {
        mxSetField(mx_data, 1, "history", string_his);
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

    buflen = (mxGetNumberOfElements(mx_data) * sizeof(mxChar)) + 1;
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
    int matlab_num_dims, harp_num_dims = 0;
    long num_elements;
    long i;

    char *des_string = NULL;
    char *unit_string = NULL;
    mxArray *datastructure = NULL;
    mxArray *meta_variable_des = NULL;
    mxArray *meta_variable_unit = NULL;
    mxArray *meta_variable_dim_type = NULL;

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

    /*set value to variables after the meta data is ready */
    matlab_num_dims = mxGetNumberOfDimensions(datastructure);
    for (i = 0; i < matlab_num_dims; i++)
    {
        dim[i] = (long)mxGetDimensions(datastructure)[i];
    }

    num_elements = mxGetNumberOfElements(datastructure);

    if (num_elements == 0)
    {
        mexErrMsgTxt("Empty arrays are not allowed for a product variable.");
    }


    /* dimension type */
    meta_variable_dim_type = mxGetField(mx_variable, 0, "dimension");

    harp_num_dims = 0;
    if (meta_variable_dim_type != NULL)
    {
        int num_ele = mxGetNumberOfElements(meta_variable_dim_type);
        char *dimtypevalue = NULL;

        for (i = 0; i < num_ele; i++)
        {
            mxArray *mx_cell = mxGetCell(meta_variable_dim_type, i);

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

    switch (class)
    {
        case mxINT8_CLASS:
            {
                long counter = 0;
                int8_t *data;
                int inner;

                if (harp_variable_new(variable_name, harp_type_int8, harp_num_dims, dim_type, dim, &variable_new) != 0)
                {
                    harp_matlab_harp_error();
                }
                if (harp_product_add_variable(*product, variable_new) != 0)
                {
                    harp_matlab_harp_error();
                }

                data = mxGetData(datastructure);
                inner = (**product).num_variables;

                /* assigning meta data */
                if (unit_string != NULL)
                {
                    if (harp_variable_set_unit((**product).variable[inner - 1], unit_string) != 0)
                    {
                        harp_matlab_harp_error();
                    }
                }
                if (des_string != NULL)
                {
                    if (harp_variable_set_description((**product).variable[inner - 1], des_string) != 0)
                    {
                        harp_matlab_harp_error();
                    }
                }

                mxFree(unit_string);
                mxFree(des_string);

                while (counter < num_elements)
                {
                    long j, k;

                    for (j = 0; j < num_elements / dim[matlab_num_dims - 1]; j++)
                    {
                        for (k = 0; k < dim[matlab_num_dims - 1]; k++)
                        {
                            variable_new->data.int8_data[counter++] =
                                data[j + k * num_elements / dim[matlab_num_dims - 1]];
                        }
                    }
                }
            }
            break;
        case mxINT16_CLASS:
            {
                long counter = 0;
                int16_t *data;
                int inner;

                if (harp_variable_new(variable_name, harp_type_int16, harp_num_dims, dim_type, dim, &variable_new) != 0)
                {
                    harp_matlab_harp_error();
                }
                if (harp_product_add_variable(*product, variable_new) != 0)
                {
                    harp_matlab_harp_error();
                }

                inner = (**product).num_variables;

                /* assigning meta data */
                if (unit_string != NULL)
                {
                    if (harp_variable_set_unit((**product).variable[inner - 1], unit_string) != 0)
                    {
                        harp_matlab_harp_error();
                    }
                }
                if (des_string != NULL)
                {
                    if (harp_variable_set_description((**product).variable[inner - 1], des_string) != 0)
                    {
                        harp_matlab_harp_error();
                    }
                }

                mxFree(unit_string);
                mxFree(des_string);

                data = mxGetData(datastructure);

                while (counter < num_elements)
                {
                    long j, k;

                    for (j = 0; j < num_elements / dim[matlab_num_dims - 1]; j++)
                    {
                        for (k = 0; k < dim[matlab_num_dims - 1]; k++)
                        {
                            variable_new->data.int16_data[counter++] =
                                data[j + k * num_elements / dim[matlab_num_dims - 1]];
                        }
                    }
                }

            }
            break;
        case mxINT32_CLASS:
            {
                long counter = 0;
                int32_t *data;
                int inner;

                if (harp_variable_new(variable_name, harp_type_int32, harp_num_dims, dim_type, dim, &variable_new) != 0)
                {
                    harp_matlab_harp_error();
                }
                if (harp_product_add_variable(*product, variable_new) != 0)
                {
                    harp_matlab_harp_error();
                }

                inner = (**product).num_variables;

                /* assigning meta data */
                if (unit_string != NULL)
                {
                    if (harp_variable_set_unit((**product).variable[inner - 1], unit_string) != 0)
                    {
                        harp_matlab_harp_error();
                    }
                }
                if (des_string != NULL)
                {
                    if (harp_variable_set_description((**product).variable[inner - 1], des_string) != 0)
                    {
                        harp_matlab_harp_error();
                    }
                }

                mxFree(unit_string);
                mxFree(des_string);

                data = mxGetData(datastructure);

                while (counter < num_elements)
                {
                    long j, k;

                    for (j = 0; j < num_elements / dim[matlab_num_dims - 1]; j++)
                    {
                        for (k = 0; k < dim[matlab_num_dims - 1]; k++)
                        {
                            variable_new->data.int32_data[counter++] =
                                data[j + k * num_elements / dim[matlab_num_dims - 1]];
                        }
                    }
                }
            }
            break;
        case mxDOUBLE_CLASS:
            {
                long counter = 0;
                double *data;
                int inner;

                if (harp_variable_new(variable_name, harp_type_double, harp_num_dims, dim_type, dim, &variable_new) !=
                    0)
                {
                    harp_matlab_harp_error();
                }
                if (harp_product_add_variable(*product, variable_new) != 0)
                {
                    harp_matlab_harp_error();
                }

                inner = (**product).num_variables;

                /* assigning meta data */
                if (unit_string != NULL)
                {
                    if (harp_variable_set_unit((**product).variable[inner - 1], unit_string) != 0)
                    {
                        harp_matlab_harp_error();
                    }
                }
                if (des_string != NULL)
                {
                    if (harp_variable_set_description((**product).variable[inner - 1], des_string) != 0)
                    {
                        harp_matlab_harp_error();
                    }
                }

                mxFree(unit_string);
                mxFree(des_string);

                data = mxGetPr(datastructure);
                while (counter < num_elements)
                {
                    long j, k;

                    for (j = 0; j < num_elements / dim[matlab_num_dims - 1]; j++)
                    {
                        for (k = 0; k < dim[matlab_num_dims - 1]; k++)
                        {
                            variable_new->data.double_data[counter++] =
                                data[j + k * num_elements / dim[matlab_num_dims - 1]];
                        }
                    }
                }
            }
            break;
        case mxSINGLE_CLASS:
            {
                long counter = 0;
                float *data;
                int inner;

                if (harp_variable_new(variable_name, harp_type_float, harp_num_dims, dim_type, dim, &variable_new) != 0)
                {
                    harp_matlab_harp_error();
                }
                if (harp_product_add_variable(*product, variable_new) != 0)
                {
                    harp_matlab_harp_error();
                }

                data = mxGetData(datastructure);
                inner = (**product).num_variables;

                /* assigning meta data */
                if (unit_string != NULL)
                {
                    if (harp_variable_set_unit((**product).variable[inner - 1], unit_string) != 0)
                    {
                        harp_matlab_harp_error();
                    }
                }
                if (des_string != NULL)
                {
                    if (harp_variable_set_description((**product).variable[inner - 1], des_string) != 0)
                    {
                        harp_matlab_harp_error();
                    }
                }
                mxFree(unit_string);
                mxFree(des_string);

                while (counter < num_elements)
                {
                    long j, k;

                    for (j = 0; j < num_elements / dim[matlab_num_dims - 1]; j++)
                    {
                        for (k = 0; k < dim[matlab_num_dims - 1]; k++)
                        {
                            variable_new->data.float_data[counter++] =
                                data[j + k * num_elements / dim[matlab_num_dims - 1]];
                        }
                    }
                }

            }
            break;
        case mxCHAR_CLASS:
            {
                int inner;

                if (mxGetNumberOfDimensions(datastructure) != 2 || mxGetDimensions(datastructure)[0] != 1)
                {
                    mexErrMsgTxt
                        ("Multi-dimensional string arrays are not allowed. Use a cell array of strings instead.");
                }
                string_data = get_matlab_string_value(datastructure);

                if (harp_variable_new(variable_name, harp_type_string, harp_num_dims, dim_type, dim, &variable_new) !=
                    0)
                {
                    harp_matlab_harp_error();
                }
                if (harp_product_add_variable(*product, variable_new) != 0)
                {
                    harp_matlab_harp_error();
                }
                inner = (**product).num_variables;

                /* assigning meta data */
                if (unit_string != NULL)
                {
                    if (harp_variable_set_unit((**product).variable[inner - 1], unit_string) != 0)
                    {
                        harp_matlab_harp_error();
                    }
                }
                if (des_string != NULL)
                {
                    if (harp_variable_set_description((**product).variable[inner - 1], des_string) != 0)
                    {
                        harp_matlab_harp_error();
                    }
                }

                mxFree(unit_string);
                mxFree(des_string);

                mxFree(string_data);
                break;
            }
        case mxCELL_CLASS:
            {
                mxArray *mx_cell;
                int inner;

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

                inner = (**product).num_variables;

                if (unit_string != NULL)
                {
                    if (harp_variable_set_unit((**product).variable[inner - 1], unit_string) != 0)
                    {
                        harp_matlab_harp_error();
                    }
                }
                if (des_string != NULL)
                {
                    if (harp_variable_set_description((**product).variable[inner - 1], des_string) != 0)
                    {
                        harp_matlab_harp_error();
                    }
                }

                mxFree(unit_string);
                mxFree(des_string);

                for (i = 0; i < num_elements; i++)
                {
                    mx_cell = mxGetCell(datastructure, coda_c_index_to_fortran_index(matlab_num_dims, dim, i));
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
