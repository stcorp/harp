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

static void harp_matlab_add_harp_product_variable(mxArray * mx_struct, harp_product **product, int index)
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

    mxArray *mx_data = NULL;

    if (harp_product_get_variable_by_name(*product, variable_name, &variable) != 0)
    {
        harp_matlab_harp_error();
    }
    if (harp_product_get_variable_id_by_name(*product, variable_name, &index) != 0)
    {
        harp_matlab_harp_error();
    }

    mxAssert(num_dims >= 0, "Number of dimensions is invalid");
    mxAssert(num_dims <= HARP_MAX_NUM_DIMS, "Number of dimensions is too high");
    mxAssert(num_elements > 0, "Number of elements in array is zero");


    for (i = 0; i < HARP_MAX_NUM_DIMS; i++)
    {
        dim[i] = variable->dimension[i];
        if (dim[i] > 1)
        {
            dim_type[i] = variable->dimension_type[i];
        }
    }

    /* top-level */
    mxArray *struct_data = mxCreateStructMatrix(1, 1, 0, NULL);

    mxArray *string_des = mxCreateString(description);

    mxAddField(struct_data, "description");
    if (description != NULL)
    {
        mxSetField(struct_data, 0, "description", string_des);
    }

    mxArray *string_unit = mxCreateString(unit);

    mxAddField(struct_data, "unit");
    if (unit != NULL)
    {
        mxSetField(struct_data, 0, "unit", string_unit);
    }

    /* MATLAB does not allow creation of arrays with num_dims == 0 */
    if (num_dims == 0 && type != harp_type_string)
    {
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

    mxArray *dim_info = mxCreateNumericArray(1, matlabdim_type, mxINT32_CLASS, mxREAL);
    int *data_dim = mxGetData(dim_info);
    
    mxArray *dim_info_type  = mxCreateCellArray(1, matlabdim_type);
    
    for (i = 0; i < num_dims; i++)
    {
        data_dim[i] = dim[i];
    }

    for (i = 0; i < num_dims; i++)
    {   
        switch(dim_type[i])
        {
            case -1:
                {
                   mxSetCell(dim_info_type, (mwIndex)i,mxCreateString("independent"));                   
                }
                break;
            case 0:
                {
                   mxSetCell(dim_info_type, (mwIndex)i,mxCreateString("time"));
                }
                break;
            case 1:
                {
                   mxSetCell(dim_info_type, (mwIndex)i,mxCreateString("latitude"));
                }
                break;
            case 2:
                {
                    mxSetCell(dim_info_type, (mwIndex)i,mxCreateString("longitude"));   
                }
                break;
            case 3:
                {
                    mxSetCell(dim_info_type, (mwIndex)i,mxCreateString("vertical"));
                }
                break;
            case 4:
                {
                    mxSetCell(dim_info_type, (mwIndex)i,mxCreateString("spectral"));
                }
                break;
        }    

    }

   
    mxAddField(struct_data, "dimension");
    mxSetField(struct_data, 0, "dimension", dim_info);

    mxAddField(struct_data, "dimension_type");
    mxSetField(struct_data, 0, "dimension_type", dim_info_type);

    switch (type)
    {
        case harp_type_int8:
            {
                int8_t *data;

                mx_data = mxCreateNumericArray(num_dims, matlabdim, mxINT8_CLASS, mxREAL);
                data = mxGetData(mx_data);
                int counter = 0;

                while (counter < num_elements)
                {
                    for (mwSize j = 0; j < num_elements / matlabdim[num_dims - 1]; j++)
                    {
                        for (mwSize k = 0; k < matlabdim[num_dims - 1]; k++)
                        {
                            data[j + k * num_elements / matlabdim[num_dims - 1]] = variable_data.int8_data[counter++];
                        }
                    }
                }

            }
            break;
        case harp_type_int16:
            {
                int16_t *data;

                mx_data = mxCreateNumericArray(num_dims, matlabdim, mxINT16_CLASS, mxREAL);
                data = mxGetData(mx_data);
                int counter = 0;

                while (counter < num_elements)
                {
                    for (mwSize j = 0; j < num_elements / matlabdim[num_dims - 1]; j++)
                    {
                        for (mwSize k = 0; k < matlabdim[num_dims - 1]; k++)
                        {
                            data[j + k * num_elements / matlabdim[num_dims - 1]] = variable_data.int16_data[counter++];
                        }
                    }
                }
            }
            break;
        case harp_type_int32:
            {
                int32_t *data;

                mx_data = mxCreateNumericArray(num_dims, matlabdim, mxINT32_CLASS, mxREAL);
                data = mxGetData(mx_data);
                int counter = 0;

                while (counter < num_elements)
                {
                    for (mwSize j = 0; j < num_elements / matlabdim[num_dims - 1]; j++)
                    {
                        for (mwSize k = 0; k < matlabdim[num_dims - 1]; k++)
                        {
                            data[j + k * num_elements / matlabdim[num_dims - 1]] = variable_data.int32_data[counter++];
                        }
                    }
                }
            }
            break;
        case harp_type_double:
            {
                double *data;

                mx_data = mxCreateNumericArray(num_dims, matlabdim, mxDOUBLE_CLASS, mxREAL);
                data = mxGetData(mx_data);
                int counter = 0;

                while (counter < num_elements)
                {
                    for (mwSize j = 0; j < num_elements / matlabdim[num_dims - 1]; j++)
                    {
                        for (mwSize k = 0; k < matlabdim[num_dims - 1]; k++)
                        {
                            data[j + k * num_elements / matlabdim[num_dims - 1]] = variable_data.double_data[counter++];
                        }
                    }
                }

            }
            break;
        case harp_type_float:
            {
                float *data;

                mx_data = mxCreateNumericArray(num_dims, matlabdim, mxSINGLE_CLASS, mxREAL);
                data = mxGetData(mx_data);
                int counter = 0;

                while (counter < num_elements)
                {
                    for (mwSize j = 0; j < num_elements / matlabdim[num_dims - 1]; j++)
                    {
                        for (mwSize k = 0; k < matlabdim[num_dims - 1]; k++)
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



    mxAddField(struct_data, "value");
    mxSetField(struct_data, 0, "value", mx_data);


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

    num_variables = (**product).num_variables;
    if (num_variables == 1)
    {
        harp_matlab_harp_error();
    }

    mx_data = mxCreateStructMatrix(1, 1, 0, NULL);

    /* add meta information for each product */
    mxArray *string_source = mxCreateString(source_product);
    mxArray *string_his = mxCreateString(history);


    mxAddField(mx_data, "source");
    mxSetField(mx_data, 0, "source", string_source);

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


static char *get_matlab_string_value(mxArray * mx_data)
{
    char *string_data;
    int buflen;

    buflen = (mxGetNumberOfElements(mx_data) * sizeof(mxChar)) + 1;
    string_data = mxCalloc(buflen, 1);
    mxGetString(mx_data, string_data, buflen);

    return string_data;
}


static void harp_matlab_add_matlab_product_variable(harp_product **product, const char *variable_name,
                                                    mxArray * mx_variable, int req_num_dims)
{
    mxClassID class;
    harp_variable *variable_new;
    char *string_data;
    long dim[HARP_MAX_NUM_DIMS];
    harp_dimension_type dim_type[HARP_MAX_NUM_DIMS];
    int num_dims = 0;
    long num_elements;
    long i;

    /* get top level from matlab */
    if (!mxIsStruct(mx_variable))
    {
        mexErrMsgTxt("This variable is not a struct.");
    }

    char *des_string = NULL;
    char *unit_string = NULL;
    char *dimtypevalue = NULL;
    int32_t *dimvalue = NULL;
    mxArray *datastructure;

    datastructure = mxGetField(mx_variable, 0, "value");
    if (datastructure != NULL)
    {
        class = mxGetClassID(datastructure);
        num_dims = mxGetNumberOfDimensions(datastructure);
    }
    else
    {
        mexErrMsgTxt("Field of value is missing.");
    }

    /* set meta info for each variable */
    mxArray *meta_variable_des = mxGetField(mx_variable, 0, "description");

    if (meta_variable_des != NULL)
    {
        des_string = mxArrayToString(meta_variable_des);
    }


    mxArray *meta_variable_unit = mxGetField(mx_variable, 0, "unit");

    if (meta_variable_unit != NULL)
    {
        unit_string = mxArrayToString(meta_variable_unit);
    }

    mxArray *meta_variable_dim = mxGetField(mx_variable, 0, "dimensions");

    if (meta_variable_dim != NULL)
    {
        dimvalue = mxGetData(meta_variable_dim);
    }

    


    /*set value to variables after the meta data is ready */

    if (num_dims > HARP_MAX_NUM_DIMS)
    {
        mexErrMsgTxt("Number of dimensions for product variable is too high.");
    }
    for (i = 0; i < num_dims; i++)
    {
        dim[i] = (long)mxGetDimensions(datastructure)[i];
    }    
    

    num_elements = mxGetNumberOfElements(datastructure);

    if (num_elements == 0)
    {
        mexErrMsgTxt("Empty arrays are not allowed for a product variable.");
    }


    /* check if we need to increase the number of dimensions to a requested number of dimensions */
    if (req_num_dims >= 0 && req_num_dims <= HARP_MAX_NUM_DIMS)
    {
        if (req_num_dims < num_dims)
        {
            mexWarnMsgTxt("num_dims variable ignored. Its value was lower than the actual number of dimensions.");
        }
        else
        {
            while (num_dims < req_num_dims)
            {
                dim[num_dims++] = 1;
            }
        }
    }

    /* descrease number of dimensions to the lowest value possible */
    while (num_dims > 0 && dim[num_dims - 1] == 1)
    {
        num_dims--;
    }

    mxAssert(num_dims >= 0, "Number of dimensions is invalid");
    mxAssert(num_dims <= HARP_MAX_NUM_DIMS, "Number of dimensions is too high");

    /* dimension type */ 
    mxArray *meta_variable_dim_type = mxGetField(mx_variable, 0, "dimension_type");
    int num_ele = mxGetNumberOfElements(meta_variable_dim_type);
 
    if (meta_variable_dim_type != NULL){
            for (i = 0; i < num_ele; i++)
            {   
                    mxArray * mx_cell = mxGetCell(meta_variable_dim_type,i);
                    dimtypevalue = get_matlab_string_value(mx_cell);
             }

            if(dimtypevalue!=NULL)
            {
                for (i = 0; i < num_dims; i++)
                {
                    if (strncmp(dimtypevalue, "independent", 11) ==0)
                    {
                        dim_type[i] = harp_dimension_independent;
                    }
                    else if (strncmp(dimtypevalue, "time", 4) ==0)       
                    {
                        dim_type[i] = harp_dimension_time;
                    }
                    else if (strncmp(dimtypevalue, "latitude", 8) ==0)
                    {
                        dim_type[i] = harp_dimension_latitude;
                    }                
                    else if (strncmp(dimtypevalue, "longitude", 9) ==0)
                    {
                        dim_type[i] = harp_dimension_longitude;
                    }
                    else if (strncmp(dimtypevalue, "vertical", 8) ==0) 
                    {
                        dim_type[i] = harp_dimension_vertical;
                    }
                    else if (strncmp(dimtypevalue, "spectral", 8) ==0)
                    {
                        dim_type[i] = harp_dimension_spectral;
                    }
                }
            }   
    }    
    else
    {
        mexErrMsgTxt("Field of dimension type is missing.");
    }
        


    switch (class)
    {
        case mxUINT8_CLASS:
            {
                int8_t *data;

                if (harp_variable_new(variable_name, harp_type_int8, num_dims, dim_type, dim, &variable_new) != 0)
                {
                    harp_matlab_harp_error();
                }
                if (harp_product_add_variable(*product, variable_new) != 0)
                {
                    harp_matlab_harp_error();
                }

                data = mxGetData(datastructure);
                int inner = (**product).num_variables;

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

                int counter = 0;

                while (counter < num_elements)
                {
                    for (long j = 0; j < num_elements / dim[req_num_dims - 1]; j++)
                    {
                        for (long k = 0; k < dim[req_num_dims - 1]; k++)
                        {
                            variable_new->data.int8_data[counter++] = data[j + k * num_elements / dim[num_dims - 1]];
                        }
                    }
                }
            }
            break;
        case mxUINT16_CLASS:
            {
                int16_t *data;

                if (harp_variable_new(variable_name, harp_type_int16, num_dims, dim_type, dim, &variable_new) != 0)
                {
                    harp_matlab_harp_error();
                }
                if (harp_product_add_variable(*product, variable_new) != 0)
                {
                    harp_matlab_harp_error();
                }

                int inner = (**product).num_variables;

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
                int counter = 0;

                while (counter < num_elements)
                {
                    for (long j = 0; j < num_elements / dim[req_num_dims - 1]; j++)
                    {
                        for (long k = 0; k < dim[req_num_dims - 1]; k++)
                        {
                            variable_new->data.int16_data[counter++] = data[j + k * num_elements / dim[num_dims - 1]];
                        }
                    }
                }

            }
            break;
        case mxINT32_CLASS:
            {
                int32_t *data;

                if (harp_variable_new(variable_name, harp_type_int32, num_dims, dim_type, dim, &variable_new) != 0)
                {
                    harp_matlab_harp_error();
                }
                if (harp_product_add_variable(*product, variable_new) != 0)
                {
                    harp_matlab_harp_error();
                }

                int inner = (**product).num_variables;

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
                int counter = 0;

                while (counter < num_elements)
                {
                    for (long j = 0; j < num_elements / dim[req_num_dims - 1]; j++)
                    {
                        for (long k = 0; k < dim[req_num_dims - 1]; k++)
                        {
                            variable_new->data.int32_data[counter++] = data[j + k * num_elements / dim[num_dims - 1]];
                        }
                    }
                }
            }
            break;
        case mxDOUBLE_CLASS:
            {
                double *data;

                if (harp_variable_new(variable_name, harp_type_double, num_dims, dim_type, dim, &variable_new) != 0)
                {
                    harp_matlab_harp_error();
                }
                if (harp_product_add_variable(*product, variable_new) != 0)
                {
                    harp_matlab_harp_error();
                }

                int inner = (**product).num_variables;

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
                int counter = 0;

                while (counter < num_elements)
                {
                    for (long j = 0; j < num_elements / dim[req_num_dims - 1]; j++)
                    {
                        for (long k = 0; k < dim[req_num_dims - 1]; k++)
                        {
                            variable_new->data.double_data[counter++] = data[j + k * num_elements / dim[num_dims - 1]];
                        }
                    }
                }

            }
            break;
        case mxSINGLE_CLASS:
            {
                float *data;

                if (harp_variable_new(variable_name, harp_type_float, num_dims, dim_type, dim, &variable_new) != 0)
                {
                    harp_matlab_harp_error();
                }
                if (harp_product_add_variable(*product, variable_new) != 0)
                {
                    harp_matlab_harp_error();
                }

                data = mxGetData(datastructure);
                int inner = (**product).num_variables;

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

                int counter = 0;

                while (counter < num_elements)
                {
                    for (long j = 0; j < num_elements / dim[req_num_dims - 1]; j++)
                    {
                        for (long k = 0; k < dim[req_num_dims - 1]; k++)
                        {
                            variable_new->data.float_data[counter++] = data[j + k * num_elements / dim[num_dims - 1]];
                        }
                    }
                }

            }
            break;
        case mxCHAR_CLASS:
            {
                if (mxGetNumberOfDimensions(datastructure) != 2 || mxGetDimensions(datastructure)[0] != 1)
                {
                    mexErrMsgTxt
                        ("Multi-dimensional string arrays are not allowed. Use a cell array of strings instead.");
                }
                string_data = get_matlab_string_value(datastructure);

                if (harp_variable_new(variable_name, harp_type_string, num_dims, dim_type, dim, &variable_new) != 0)
                {
                    harp_matlab_harp_error();
                }
                if (harp_product_add_variable(*product, variable_new) != 0)
                {
                    harp_matlab_harp_error();
                }
                int inner = (**product).num_variables;

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

                for (i = 0; i < num_elements; i++)
                {
                    mx_cell = mxGetCell(datastructure, i);
                    if (mxGetClassID(mx_cell) != mxCHAR_CLASS || mxGetNumberOfDimensions(mx_cell) != 2 ||
                        mxGetDimensions(mx_cell)[0] > 1)
                    {
                        mexErrMsgTxt("Cell arrays are only allowed for one dimensional string data.");
                    }
                }
                if (harp_variable_new(variable_name, harp_type_string, num_dims, dim_type, dim, &variable_new) != 0)
                {
                    harp_matlab_harp_error();
                }
                if (harp_product_add_variable(*product, variable_new) != 0)
                {
                    harp_matlab_harp_error();
                }

                int inner = (**product).num_variables;

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
                    mx_cell = mxGetCell(datastructure, coda_c_index_to_fortran_index(num_dims, dim, i));
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

harp_product *harp_matlab_set_product(const mxArray * mx_struct)
{
    harp_product *product;
    int num_variables;
    int index;

    if (!mxIsStruct(mx_struct))
    {
        mexErrMsgTxt("Not a struct.");
    }
    num_variables = mxGetNumberOfFields(mx_struct);

    if (harp_product_new(&product) != 0)
    {
        harp_matlab_harp_error();
    }

    for (index = 0; index < num_variables; index++)
    {
        const char *variable_name;

        variable_name = mxGetFieldNameByNumber(mx_struct, index);

        /* set meta info for each product from matlab input */
        if (strncmp(variable_name, "source", 6) == 0)
        {
            mxArray *meta = mxGetField(mx_struct, index, "source");
            char *metastring = mxArrayToString(meta);

            if (metastring != NULL)
            {
                if (harp_product_set_source_product(product, metastring) != 0)
                {
                    harp_matlab_harp_error();
                }
            }
        }
        else if (strncmp(variable_name, "history", 7) == 0)
        {
            mxArray *meta = mxGetField(mx_struct, index, "history");
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
            mxArray *mx_dim_variable;
            int dim_variable_index;
            int dim1 = 2;

            dim_variable_index = mxGetFieldNumber(mx_struct, variable_name);

            if (dim_variable_index >= 0)
            {
                mx_dim_variable = mxGetFieldByNumber(mx_struct, 0, dim_variable_index);
                dim1 = mxGetNumberOfDimensions(mx_dim_variable);
            }

            mx_variable = mxGetFieldByNumber(mx_struct, 0, index);

            harp_matlab_add_matlab_product_variable(&product, variable_name, mx_variable, dim1);
        }

    }

    return product;
}
