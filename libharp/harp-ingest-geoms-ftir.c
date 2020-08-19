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

#include "coda.h"
#include "harp-ingestion.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_UNIT_LENGTH 30

#define MAX_NAME_LENGTH 80
#define MAX_DESCRIPTION_LENGTH 100
#define MAX_PATH_LENGTH 100
#define MAX_MAPPING_LENGTH 100

typedef enum ftir_gas_enum
{
    ftir_C2H2,
    ftir_C2H6,
    ftir_CCl2F2,
    ftir_CCl3F,
    ftir_CH3OH,
    ftir_CH4,
    ftir_CHF2Cl,
    ftir_ClONO2,
    ftir_CO,
    ftir_CO2,
    ftir_COF2,
    ftir_H2CO,
    ftir_H2O,
    ftir_HCl,
    ftir_HCN,
    ftir_HCOOH,
    ftir_HF,
    ftir_HNO3,
    ftir_N2O,
    ftir_NH3,
    ftir_NO,
    ftir_NO2,
    ftir_O3,
    ftir_OCS,
    ftir_SF6,
    num_ftir_gas
} ftir_gas;

static const char *geoms_gas_name[num_ftir_gas] = {
    "C2H2",
    "C2H6",
    "CCl2F2",
    "CCl3F",
    "CH3OH",
    "CH4",
    "CHF2Cl",
    "ClONO2",
    "CO",
    "CO2",
    "COF2",
    "H2CO",
    "H2O",
    "HCl",
    "HCN",
    "HCOOH",
    "HF",
    "HNO3",
    "N2O",
    "NH3",
    "NO",
    "NO2",
    "O3",
    "OCS",
    "SF6"
};

static const char *harp_gas_name[num_ftir_gas] = {
    "C2H2",
    "C2H6",
    "CCl2F2",
    "CCl3F",
    "CH3OH",
    "CH4",
    "CHClF2",
    "ClNO3",
    "CO",
    "CO2",
    "COF2",
    "HCHO",
    "H2O",
    "HCl",
    "HCN",
    "HCOOH",
    "HF",
    "HNO3",
    "N2O",
    "NH3",
    "NO",
    "NO2",
    "O3",
    "COS",
    "SF6"
};

typedef struct ingest_info_struct
{
    const harp_product_definition *definition;
    int product_version;
    coda_product *product;
    ftir_gas gas;
    int lunar;  /* 1: lunar, 0: solar */
    int time_dep_altitude;
    int time_dep_altitude_bounds;
    const char *template;
    long num_time;
    long num_vertical;
    int invert_vertical;        /* should all data long the vertical axis be inverted? */
    int has_vmr_absorption;
    char vmr_unit[MAX_UNIT_LENGTH];
    char vmr_covariance_unit[MAX_UNIT_LENGTH];
    char column_unit[MAX_UNIT_LENGTH];
    char h2o_vmr_unit[MAX_UNIT_LENGTH];
    char h2o_column_unit[MAX_UNIT_LENGTH];
} ingest_info;

static int read_dimensions(void *user_data, long dimension[HARP_NUM_DIM_TYPES])
{
    ingest_info *info = (ingest_info *)user_data;

    dimension[harp_dimension_time] = info->num_time;
    dimension[harp_dimension_vertical] = info->num_vertical;

    return 0;
}

static int read_attribute(void *user_data, const char *path, harp_array data)
{
    coda_cursor cursor;
    long length;

    if (coda_cursor_set_product(&cursor, ((ingest_info *)user_data)->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, path) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_string_length(&cursor, &length) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    data.string_data[0] = malloc(length + 1);
    if (data.string_data[0] == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)", length + 1,
                       __FILE__, __LINE__);
        return -1;
    }
    if (coda_cursor_read_string(&cursor, data.string_data[0], length + 1) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    return 0;
}

static int read_variable_double(void *user_data, const char *path, long num_elements, harp_array data)
{
    coda_cursor cursor;
    long actual_num_elements, i;
    double fill_value;

    if (coda_cursor_set_product(&cursor, ((ingest_info *)user_data)->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, path) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &actual_num_elements) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (actual_num_elements != num_elements)
    {
        harp_set_error(HARP_ERROR_INGESTION, "variable %s has %ld elements (expected %ld)", path, actual_num_elements,
                       num_elements);
        return -1;
    }
    if (coda_cursor_read_double_array(&cursor, data.double_data, coda_array_ordering_c) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, "@VAR_FILL_VALUE") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_double(&cursor, &fill_value) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (!harp_isnan(fill_value))
    {
        for (i = 0; i < num_elements; i++)
        {
            if (data.double_data[i] == fill_value)
            {
                data.double_data[i] = harp_nan();
            }
        }
    }

    return 0;
}

static int read_vertical_variable_double(void *user_data, const char *path, long num_elements, harp_array data)
{
    if (read_variable_double(user_data, path, num_elements, data) != 0)
    {
        return -1;
    }
    if (((ingest_info *)user_data)->invert_vertical)
    {
        long dimension[2];

        dimension[1] = ((ingest_info *)user_data)->num_vertical;
        dimension[0] = num_elements / dimension[1];
        if (harp_array_invert(harp_type_double, 1, 2, dimension, data) != 0)
        {
            return -1;
        }
    }

    return 0;
}

static int read_vertical2d_variable_double(void *user_data, const char *path, long num_elements, harp_array data)
{
    if (read_variable_double(user_data, path, num_elements, data) != 0)
    {
        return -1;
    }

    if (((ingest_info *)user_data)->invert_vertical)
    {
        long dimension[3];

        dimension[1] = ((ingest_info *)user_data)->num_vertical;
        dimension[2] = dimension[1];
        dimension[0] = num_elements / (dimension[1] * dimension[2]);
        if (harp_array_invert(harp_type_double, 1, 3, dimension, data) != 0)
        {
            return -1;
        }
        if (harp_array_invert(harp_type_double, 2, 3, dimension, data) != 0)
        {
            return -1;
        }
    }

    return 0;
}

static int read_vertical_sqrt_2dtrace_variable_double(void *user_data, const char *path, long num_elements,
                                                      harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    harp_array matrix_data;
    long num_blocks = num_elements / info->num_vertical;
    long i, j;

    matrix_data.double_data = malloc(num_elements * info->num_vertical * sizeof(double));
    if (matrix_data.double_data == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_elements * info->num_vertical * sizeof(double), __FILE__, __LINE__);
        return -1;
    }

    if (read_variable_double(user_data, path, num_elements * info->num_vertical, matrix_data) != 0)
    {
        free(matrix_data.double_data);
        return -1;
    }
    for (i = 0; i < num_blocks; i++)
    {
        for (j = 0; j < info->num_vertical; j++)
        {
            data.double_data[i * info->num_vertical + j] =
                sqrt(matrix_data.double_data[(i * info->num_vertical + j) * info->num_vertical + j]);
        }
    }
    free(matrix_data.double_data);

    if (info->invert_vertical)
    {
        long dimension[2];

        dimension[1] = info->num_vertical;
        dimension[0] = num_elements / dimension[1];
        if (harp_array_invert(harp_type_double, 1, 2, dimension, data) != 0)
        {
            return -1;
        }
    }

    return 0;
}

static int read_data_source(void *user_data, harp_array data)
{
    return read_attribute(user_data, "@DATA_SOURCE", data);
}

static int read_data_location(void *user_data, harp_array data)
{
    return read_attribute(user_data, "@DATA_LOCATION", data);
}

static int read_measurement_mode(void *user_data, harp_array data)
{
    data.string_data[0] = strdup(((ingest_info *)user_data)->lunar ? "lunar" : "solar");
    if (data.string_data[0] == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        return -1;
    }

    return 0;
}

static int read_instrument_altitude(void *user_data, harp_array data)
{
    return read_variable_double(user_data, "ALTITUDE_INSTRUMENT", 1, data);
}

static int read_instrument_latitude(void *user_data, harp_array data)
{
    return read_variable_double(user_data, "LATITUDE_INSTRUMENT", 1, data);
}

static int read_instrument_longitude(void *user_data, harp_array data)
{
    return read_variable_double(user_data, "LONGITUDE_INSTRUMENT", 1, data);
}

static int read_datetime(void *user_data, harp_array data)
{
    return read_variable_double(user_data, "DATETIME", ((ingest_info *)user_data)->num_time, data);
}

static int read_datetime_length(void *user_data, harp_array data)
{
    return read_variable_double(user_data, "INTEGRATION_TIME", ((ingest_info *)user_data)->num_time, data);
}

static int read_column(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char path[MAX_PATH_LENGTH];

    snprintf(path, MAX_PATH_LENGTH, "/%s_COLUMN_ABSORPTION_%s", geoms_gas_name[info->gas],
             info->lunar ? "LUNAR" : "SOLAR");
    if (read_variable_double(user_data, path, info->num_time, data) != 0)
    {
        return -1;
    }

    if (harp_convert_unit(info->column_unit, "molec/m2", info->num_time, data.double_data) != 0)
    {
        return -1;
    }

    return 0;
}

static int read_h2o_column(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char path[MAX_PATH_LENGTH];

    snprintf(path, MAX_PATH_LENGTH, "/H2O_COLUMN_ABSORPTION_%s", info->lunar ? "LUNAR" : "SOLAR");
    if (read_variable_double(user_data, path, info->num_time, data) != 0)
    {
        return -1;
    }

    if (harp_convert_unit(info->h2o_column_unit, "molec/m2", info->num_time, data.double_data) != 0)
    {
        return -1;
    }

    return 0;
}

static int read_column_apriori(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char path[MAX_PATH_LENGTH];

    snprintf(path, MAX_PATH_LENGTH, "/%s_COLUMN_ABSORPTION_%s_APRIORI", geoms_gas_name[info->gas],
             info->lunar ? "LUNAR" : "SOLAR");
    if (read_variable_double(user_data, path, info->num_time, data) != 0)
    {
        return -1;
    }

    if (harp_convert_unit(info->column_unit, "molec/m2", info->num_time, data.double_data) != 0)
    {
        return -1;
    }

    return 0;
}

static int read_column_avk(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char path[MAX_PATH_LENGTH];

    snprintf(path, MAX_PATH_LENGTH, "/%s_COLUMN_ABSORPTION_%s_AVK", geoms_gas_name[info->gas],
             info->lunar ? "LUNAR" : "SOLAR");
    return read_vertical_variable_double(user_data, path, info->num_time * info->num_vertical, data);
}

static int read_column_uncertainty_random(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char path[MAX_PATH_LENGTH];

    snprintf(path, MAX_PATH_LENGTH, "/%s_COLUMN_ABSORPTION_%s_UNCERTAINTY_RANDOM%s", geoms_gas_name[info->gas],
             info->lunar ? "LUNAR" : "SOLAR", info->product_version == 1 ? "" : "_STANDARD");
    if (read_variable_double(user_data, path, info->num_time, data) != 0)
    {
        return -1;
    }

    if (harp_convert_unit(info->column_unit, "molec/m2", info->num_time, data.double_data) != 0)
    {
        return -1;
    }

    return 0;
}

static int read_column_uncertainty_systematic(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char path[MAX_PATH_LENGTH];

    snprintf(path, MAX_PATH_LENGTH, "/%s_COLUMN_ABSORPTION_%s_UNCERTAINTY_SYSTEMATIC%s", geoms_gas_name[info->gas],
             info->lunar ? "LUNAR" : "SOLAR", info->product_version == 1 ? "" : "_STANDARD");
    if (read_variable_double(user_data, path, info->num_time, data) != 0)
    {
        return -1;
    }

    if (harp_convert_unit(info->column_unit, "molec/m2", info->num_time, data.double_data) != 0)
    {
        return -1;
    }

    return 0;
}

static int read_vmr(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char path[MAX_PATH_LENGTH];

    snprintf(path, MAX_PATH_LENGTH, "/%s_MIXING_RATIO%s_ABSORPTION_%s", geoms_gas_name[info->gas],
             info->product_version == 1 ? "" : "_VOLUME", info->lunar ? "LUNAR" : "SOLAR");
    if (read_vertical_variable_double(user_data, path, info->num_time * info->num_vertical, data) != 0)
    {
        return -1;
    }

    if (harp_convert_unit(info->vmr_unit, "ppmv", info->num_time * info->num_vertical, data.double_data) != 0)
    {
        return -1;
    }

    return 0;
}

static int read_h2o_vmr(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char path[MAX_PATH_LENGTH];

    snprintf(path, MAX_PATH_LENGTH, "H2O_MIXING_RATIO%s_ABSORPTION_%s", info->product_version == 1 ? "" : "_VOLUME",
             info->lunar ? "LUNAR" : "SOLAR");
    if (read_vertical_variable_double(user_data, path, info->num_time * info->num_vertical, data) != 0)
    {
        return -1;
    }

    if (harp_convert_unit(info->h2o_vmr_unit, "ppmv", info->num_time * info->num_vertical, data.double_data) != 0)
    {
        return -1;
    }

    return 0;
}

static int read_vmr_apriori(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char path[MAX_PATH_LENGTH];

    snprintf(path, MAX_PATH_LENGTH, "/%s_MIXING_RATIO%s_ABSORPTION_%s_APRIORI", geoms_gas_name[info->gas],
             info->product_version == 1 ? "" : "_VOLUME", info->lunar ? "LUNAR" : "SOLAR");
    if (read_vertical_variable_double(user_data, path, info->num_time * info->num_vertical, data) != 0)
    {
        return -1;
    }

    if (harp_convert_unit(info->vmr_unit, "ppmv", info->num_time * info->num_vertical, data.double_data) != 0)
    {
        return -1;
    }

    return 0;
}

static int read_vmr_avk(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char path[MAX_PATH_LENGTH];

    snprintf(path, MAX_PATH_LENGTH, "/%s_MIXING_RATIO%s_ABSORPTION_%s_AVK", geoms_gas_name[info->gas],
             info->product_version == 1 ? "" : "_VOLUME", info->lunar ? "LUNAR" : "SOLAR");
    return read_vertical2d_variable_double(user_data, path, info->num_time * info->num_vertical * info->num_vertical,
                                           data);
}

static int read_vmr_covariance(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char path[MAX_PATH_LENGTH];

    snprintf(path, MAX_PATH_LENGTH, "/%s_MIXING_RATIO%s_ABSORPTION_%s_UNCERTAINTY_RANDOM%s",
             geoms_gas_name[info->gas], info->product_version == 1 ? "" : "_VOLUME", info->lunar ? "LUNAR" : "SOLAR",
             info->product_version == 1 ? "" : "_COVARIANCE");
    if (read_vertical2d_variable_double(user_data, path, info->num_time * info->num_vertical * info->num_vertical, data)
        != 0)
    {
        return -1;
    }

    if (harp_convert_unit
        (info->vmr_covariance_unit, "(ppmv)2", info->num_time * info->num_vertical * info->num_vertical,
         data.double_data) != 0)
    {
        return -1;
    }

    return 0;
}

static int read_vmr_uncertainty_random(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char path[MAX_PATH_LENGTH];

    snprintf(path, MAX_PATH_LENGTH, "/%s_MIXING_RATIO%s_ABSORPTION_%s_UNCERTAINTY_RANDOM%s",
             geoms_gas_name[info->gas], info->product_version == 1 ? "" : "_VOLUME", info->lunar ? "LUNAR" : "SOLAR",
             info->product_version == 1 ? "" : "_COVARIANCE");
    if (read_vertical_sqrt_2dtrace_variable_double(user_data, path, info->num_time * info->num_vertical, data) != 0)
    {
        return -1;
    }

    if (harp_convert_unit(info->vmr_unit, "ppmv", info->num_time * info->num_vertical, data.double_data) != 0)
    {
        return -1;
    }

    return 0;
}

static int read_vmr_uncertainty_systematic(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char path[MAX_PATH_LENGTH];

    snprintf(path, MAX_PATH_LENGTH, "/%s_MIXING_RATIO%s_ABSORPTION_%s_UNCERTAINTY_SYSTEMATIC%s",
             geoms_gas_name[info->gas], info->product_version == 1 ? "" : "_VOLUME", info->lunar ? "LUNAR" : "SOLAR",
             info->product_version == 1 ? "" : "_COVARIANCE");
    if (read_vertical_sqrt_2dtrace_variable_double(user_data, path, info->num_time * info->num_vertical, data) != 0)
    {
        return -1;
    }

    if (harp_convert_unit(info->vmr_unit, "ppmv", info->num_time * info->num_vertical, data.double_data) != 0)
    {
        return -1;
    }

    return 0;
}

static int read_altitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->time_dep_altitude)
    {
        if (read_vertical_variable_double(user_data, "ALTITUDE", info->num_time * info->num_vertical, data) != 0)
        {
            return -1;
        }
    }
    else
    {
        long i;

        if (read_vertical_variable_double(user_data, "ALTITUDE", info->num_vertical, data) != 0)
        {
            return -1;
        }

        for (i = 0; i < info->num_time; i++)
        {
            memcpy(&data.double_data[i * info->num_vertical], data.double_data, info->num_vertical * sizeof(double));
        }
    }

    return 0;
}

static int read_altitude_bounds(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long dimension[3];
    long i;

    if (info->time_dep_altitude_bounds)
    {
        harp_array sub_array;

        if (read_variable_double(user_data, "ALTITUDE_BOUNDARIES", info->num_time * 2 * info->num_vertical, data) != 0)
        {
            return -1;
        }

        dimension[0] = info->num_time;
        dimension[1] = 2;
        dimension[2] = info->num_vertical;

        if (info->invert_vertical)
        {
            /* invert height axis */
            if (harp_array_invert(harp_type_double, 2, 3, dimension, data) != 0)
            {
                return -1;
            }
        }
        for (i = 0; i < info->num_time; i++)
        {
            sub_array.double_data = &data.double_data[i * 2 * info->num_vertical];
            /* swap [2,ALTITUDE] to [ALTITUDE,2] */
            if (harp_array_transpose(harp_type_double, 2, &dimension[1], NULL, sub_array) != 0)
            {
                return -1;
            }
        }
    }
    else
    {
        if (read_variable_double(user_data, "ALTITUDE_BOUNDARIES", 2 * info->num_vertical, data) != 0)
        {
            return -1;
        }

        dimension[0] = 2;
        dimension[1] = info->num_vertical;

        if (info->invert_vertical)
        {
            /* invert height axis */
            if (harp_array_invert(harp_type_double, 1, 2, dimension, data) != 0)
            {
                return -1;
            }
        }
        /* swap [2,ALTITUDE] to [ALTITUDE,2] */
        if (harp_array_transpose(harp_type_double, 2, dimension, NULL, data) != 0)
        {
            return -1;
        }
        /* replicate across time dimension */
        for (i = 1; i < info->num_time; i++)
        {
            memcpy(&data.double_data[i * info->num_vertical * 2], data.double_data,
                   2 * info->num_vertical * sizeof(double));
        }
    }

    /* note that 'low'/'high' for each layer are already in the right order */

    return 0;
}

static int read_pressure_ind(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_vertical_variable_double(user_data, "PRESSURE_INDEPENDENT", info->num_time * info->num_vertical, data);
}

static int read_temperature_ind(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_vertical_variable_double(user_data, "TEMPERATURE_INDEPENDENT", info->num_time * info->num_vertical,
                                         data);
}

static int read_surface_pressure_ind(void *user_data, harp_array data)
{
    return read_variable_double(user_data, "SURFACE_PRESSURE_INDEPENDENT", ((ingest_info *)user_data)->num_time, data);
}

static int read_surface_temperature_ind(void *user_data, harp_array data)
{
    return read_variable_double(user_data, "SURFACE_TEMPERATURE_INDEPENDENT", ((ingest_info *)user_data)->num_time,
                                data);
}

static int read_solar_azimuth_angle(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->lunar)
    {
        return read_variable_double(user_data, "ANGLE_LUNAR_AZIMUTH", info->num_time, data);
    }
    return read_variable_double(user_data, "ANGLE_SOLAR_AZIMUTH", info->num_time, data);
}

static int read_solar_zenith_angle(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->lunar)
    {
        return read_variable_double(user_data, "ANGLE_LUNAR_ZENITH_ASTRONOMICAL", info->num_time, data);
    }
    return read_variable_double(user_data, "ANGLE_SOLAR_ZENITH_ASTRONOMICAL", info->num_time, data);
}

static int include_vmr_absorption(void *user_data)
{
    return ((ingest_info *)user_data)->has_vmr_absorption;
}

static void ingestion_done(void *user_data)
{
    free(user_data);
}

static ftir_gas get_gas_from_string(const char *str)
{
    int i;

    for (i = 0; i < num_ftir_gas; i++)
    {
        if (strcmp(str, geoms_gas_name[i]) == 0)
        {
            return i;
        }
    }

    assert(0);
    exit(1);
}

static int get_product_definition(const harp_ingestion_module *module, coda_product *product,
                                  harp_product_definition **definition)
{
    coda_cursor cursor;
    char template_name[18];
    char data_source[20];
    char *gas;
    long length;
    long i;

    if (coda_cursor_set_product(&cursor, product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, "@DATA_TEMPLATE") != 0)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, "could not find DATA_TEMPLATE global attribute");
        return -1;
    }
    if (coda_cursor_get_string_length(&cursor, &length) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    /* template should match the pattern "GEOMS-TE-FTIR-xxx" */
    if (length != 17)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, "invalid string length for DATA_TEMPLATE global attribute");
        return -1;
    }
    if (coda_cursor_read_string(&cursor, template_name, 18) != 0)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, NULL);
        return -1;
    }

    if (coda_cursor_goto(&cursor, "/@DATA_SOURCE") != 0)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, "could not find DATA_SOURCE global attribute");
        return -1;
    }
    if (coda_cursor_read_string(&cursor, data_source, 20) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    /* data source should match the pattern "FTIR.<SPECIES>_xxxx" */
    if (strncmp(data_source, "FTIR.", 5) != 0)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, "DATA_SOURCE global attribute has an invalid value");
        return -1;
    }
    /* truncate data_source at first '_' occurrence */
    i = 5;
    while (data_source[i] != '\0')
    {
        if (data_source[i] == '_')
        {
            data_source[i] = '\0';
        }
        else
        {
            i++;
        }
    }
    gas = &data_source[5];

    for (i = 0; i < module->num_product_definitions; i++)
    {
        /* match against product definition name: '<template_name>-<gas>' */
        if (strncmp(template_name, module->product_definition[i]->name, 17) == 0 &&
            strcmp(gas, &module->product_definition[i]->name[18]) == 0)
        {
            *definition = module->product_definition[i];
            return 0;
        }
    }

    harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, "GEOMS template '%s' for gas '%s' not supported", template_name,
                   gas);
    return -1;
}

static int get_dimensions(ingest_info *info)
{
    coda_cursor cursor;
    double values[2];
    long dim[CODA_MAX_NUM_DIMS];
    int num_dims;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, "/DATETIME") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &info->num_time) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (info->num_time > 1)
    {
        if (coda_cursor_read_double_partial_array(&cursor, 0, 2, values) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (values[1] < values[0])
        {
            harp_set_error(HARP_ERROR_INGESTION, "time dimension should use a chronological ordering");
            return -1;
        }
    }

    if (coda_cursor_goto(&cursor, "/ALTITUDE") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_array_dim(&cursor, &num_dims, dim) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (num_dims == 1)
    {
        info->num_vertical = dim[0];
    }
    else if (num_dims == 2)
    {
        info->num_vertical = dim[1];
    }
    else
    {
        harp_set_error(HARP_ERROR_INGESTION, "ALTITUDE variable should be one or two dimensional");
        return -1;
    }
    if (info->num_vertical > 1)
    {
        if (coda_cursor_read_double_partial_array(&cursor, 0, 2, values) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        info->invert_vertical = (values[1] < values[0]);
    }

    return 0;
}

static int get_lunar_switch(ingest_info *info)
{
    coda_cursor cursor;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, "/ANGLE_LUNAR_AZIMUTH") == 0)
    {
        info->lunar = 1;
        return 0;
    }
    if (coda_cursor_goto(&cursor, "/ANGLE_SOLAR_AZIMUTH") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    info->lunar = 0;
    return 0;
}

static int get_variable_time_dependencies(ingest_info *info)
{
    coda_cursor cursor;
    long dim[CODA_MAX_NUM_DIMS];
    int num_dims;

    /* determine whether variable have a time dimension */

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, "/ALTITUDE") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_array_dim(&cursor, &num_dims, dim) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    info->time_dep_altitude = (num_dims == 2);
    if (coda_cursor_goto(&cursor, "/ALTITUDE_BOUNDARIES") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_array_dim(&cursor, &num_dims, dim) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    info->time_dep_altitude_bounds = (num_dims == 3);

    return 0;
}

static int get_optional_variable_availability(ingest_info *info)
{
    coda_cursor cursor;
    char path[MAX_PATH_LENGTH];

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    snprintf(path, MAX_PATH_LENGTH, "/%s_MIXING_RATIO%s_ABSORPTION_%s", geoms_gas_name[info->gas],
             info->product_version == 1 ? "" : "_VOLUME", info->lunar ? "LUNAR" : "SOLAR");
    info->has_vmr_absorption = (coda_cursor_goto(&cursor, path) == 0);

    return 0;
}


static int read_unit(coda_cursor *cursor, const char *path, char unit[MAX_UNIT_LENGTH])
{
    if (coda_cursor_goto(cursor, path) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(cursor, "@VAR_UNITS") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_string(cursor, unit, MAX_UNIT_LENGTH) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    return 0;
}

static int get_dynamic_units(ingest_info *info)
{
    coda_cursor cursor;
    char path[MAX_PATH_LENGTH];

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (info->has_vmr_absorption)
    {
        snprintf(path, MAX_PATH_LENGTH, "/%s_MIXING_RATIO%s_ABSORPTION_%s", geoms_gas_name[info->gas],
                 info->product_version == 1 ? "" : "_VOLUME", info->lunar ? "LUNAR" : "SOLAR");
        if (read_unit(&cursor, path, info->vmr_unit) != 0)
        {
            return -1;
        }
        snprintf(path, MAX_PATH_LENGTH, "/%s_MIXING_RATIO%s_ABSORPTION_%s_UNCERTAINTY_RANDOM%s",
                 geoms_gas_name[info->gas], info->product_version == 1 ? "" : "_VOLUME",
                 info->lunar ? "LUNAR" : "SOLAR", info->product_version == 1 ? "" : "_COVARIANCE");
        if (read_unit(&cursor, path, info->vmr_covariance_unit) != 0)
        {
            return -1;
        }
    }
    snprintf(path, MAX_PATH_LENGTH, "/%s_COLUMN_ABSORPTION_%s", geoms_gas_name[info->gas],
             info->lunar ? "LUNAR" : "SOLAR");
    if (read_unit(&cursor, path, info->column_unit) != 0)
    {
        return -1;
    }
    if (info->gas != ftir_H2CO)
    {
        snprintf(path, MAX_PATH_LENGTH, "/H2O_MIXING_RATIO%s_ABSORPTION_%s",
                 info->product_version == 1 ? "" : "_VOLUME", info->lunar ? "LUNAR" : "SOLAR");
        if (read_unit(&cursor, path, info->h2o_vmr_unit) != 0)
        {
            return -1;
        }
        snprintf(path, MAX_PATH_LENGTH, "/H2O_COLUMN_ABSORPTION_%s", info->lunar ? "LUNAR" : "SOLAR");
        if (read_unit(&cursor, path, info->h2o_column_unit) != 0)
        {
            return -1;
        }
    }

    return 0;
}

static int ingestion_init(const harp_ingestion_module *module, coda_product *product,
                          const harp_ingestion_options *options, harp_product_definition **definition, void **user_data)
{
    ingest_info *info;

    (void)options;

    info = malloc(sizeof(ingest_info));
    if (info == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(ingest_info), __FILE__, __LINE__);
        return -1;
    }

    info->definition = NULL;
    info->product = product;

    if (get_product_definition(module, info->product, definition) != 0)
    {
        ingestion_done(info);
        return -1;
    }
    info->definition = *definition;
    info->product_version = info->definition->name[16] - '0';
    info->gas = get_gas_from_string(&info->definition->name[18]);
    if (get_lunar_switch(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }

    info->invert_vertical = 0;
    if (get_dimensions(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }

    if (get_variable_time_dependencies(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }

    if (get_optional_variable_availability(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }

    if (get_dynamic_units(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }

    *user_data = info;
    return 0;
}

static int init_product_definition(harp_ingestion_module *module, ftir_gas gas, int version)
{
    harp_variable_definition *variable_definition;
    harp_product_definition *product_definition;
    harp_dimension_type dimension_type[3];
    char gas_product_name[MAX_NAME_LENGTH];
    char gas_product_description[MAX_DESCRIPTION_LENGTH];
    char gas_var_name[MAX_NAME_LENGTH];
    char gas_mapping_path[MAX_PATH_LENGTH];
    char gas_description[MAX_DESCRIPTION_LENGTH];
    long dimension[3];
    const char *description;

    snprintf(gas_product_name, MAX_NAME_LENGTH, "GEOMS-TE-FTIR-%03d-%s", version, geoms_gas_name[gas]);
    snprintf(gas_product_description, MAX_DESCRIPTION_LENGTH, "GEOMS template for FTIR v%03d - %s", version,
             geoms_gas_name[gas]);
    product_definition = harp_ingestion_register_product(module, gas_product_name, gas_product_description,
                                                         read_dimensions);

    dimension_type[0] = harp_dimension_time;
    dimension_type[1] = harp_dimension_vertical;
    dimension_type[2] = harp_dimension_vertical;

    /* for altitude bounds */
    dimension[0] = -1;
    dimension[1] = -1;
    dimension[2] = 2;

    /* sensor_name */
    description = "name of the sensor";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "sensor_name",
                                                                     harp_type_string, 0, NULL, NULL, description, NULL,
                                                                     NULL, read_data_source);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/@DATA.SOURCE", NULL);

    /* site_name */
    description = "name of the site at which the sensor is located";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "site_name", harp_type_string,
                                                                     0, NULL, NULL, description, NULL, NULL,
                                                                     read_data_location);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/@DATA.LOCATION", NULL);

    /* measurement_mode */
    description = "'solar' or 'lunar' measurement";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "measurement_mode",
                                                                     harp_type_string, 0, NULL, NULL, description, NULL,
                                                                     NULL, read_measurement_mode);
    harp_variable_definition_add_mapping(variable_definition, NULL,
                                         "determined from 'variable mode' part of variable names", NULL, NULL);

    /* sensor_latitude */
    description = "latitude of the sensor";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "sensor_latitude",
                                                                     harp_type_double, 0, NULL, NULL, description,
                                                                     "degree_north", NULL, read_instrument_latitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/LATITUDE.INSTRUMENT", NULL);

    /* sensor_longitude */
    description = "longitude of the sensor";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "sensor_longitude",
                                                                     harp_type_double, 0, NULL, NULL, description,
                                                                     "degree_east", NULL, read_instrument_longitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0, 180.0);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/LONGITUDE.INSTRUMENT", NULL);

    /* sensor_altitude */
    description = "altitude of the sensor";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "sensor_altitude",
                                                                     harp_type_double, 0, NULL, NULL, description, "km",
                                                                     NULL, read_instrument_altitude);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ALTITUDE.INSTRUMENT", NULL);

    /* datetime */
    description = "time of the measurement";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double,
                                                                     1, dimension_type, NULL, description,
                                                                     "days since 2000-01-01", NULL, read_datetime);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/DATETIME", NULL);

    /* datetime_length */
    description = "duration of the measurement";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "datetime_length",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, "s", include_vmr_absorption,
                                                                     read_datetime_length);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/INTEGRATION.TIME", NULL);

    /* <gas>_column_number_density */
    snprintf(gas_var_name, MAX_NAME_LENGTH, "%s_column_number_density", harp_gas_name[gas]);
    snprintf(gas_description, MAX_DESCRIPTION_LENGTH, "total %s vertical column", harp_gas_name[gas]);
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, gas_var_name, harp_type_double, 1, dimension_type, NULL, gas_description,
         "molec/m2", NULL, read_column);
    description = "unit is converted to molec/m2";
    snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.COLUMN_ABSORPTION.SOLAR", geoms_gas_name[gas]);
    harp_variable_definition_add_mapping(variable_definition, NULL, "solar measurement", gas_mapping_path, description);
    snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.COLUMN_ABSORPTION.LUNAR", geoms_gas_name[gas]);
    harp_variable_definition_add_mapping(variable_definition, NULL, "lunar measurement", gas_mapping_path, description);

    /* <gas>_column_number_density_apriori */
    snprintf(gas_var_name, MAX_NAME_LENGTH, "%s_column_number_density_apriori", harp_gas_name[gas]);
    snprintf(gas_description, MAX_DESCRIPTION_LENGTH, "a priori total %s vertical column", harp_gas_name[gas]);
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, gas_var_name, harp_type_double, 1, dimension_type, NULL, gas_description,
         "molec/m2", NULL, read_column_apriori);
    description = "unit is converted to molec/m2";
    snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.COLUMN_ABSORPTION.SOLAR_APRIORI", geoms_gas_name[gas]);
    harp_variable_definition_add_mapping(variable_definition, NULL, "solar measurement", gas_mapping_path, description);
    snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.COLUMN_ABSORPTION.LUNAR_APRIORI", geoms_gas_name[gas]);
    harp_variable_definition_add_mapping(variable_definition, NULL, "lunar measurement", gas_mapping_path, description);

    /* <gas>_column_number_density_avk */
    snprintf(gas_var_name, MAX_NAME_LENGTH, "%s_column_number_density_avk", harp_gas_name[gas]);
    snprintf(gas_description, MAX_DESCRIPTION_LENGTH, "averaging kernel for the total %s vertical column",
             harp_gas_name[gas]);
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, gas_var_name, harp_type_double, 2, dimension_type, NULL, gas_description,
         HARP_UNIT_DIMENSIONLESS, NULL, read_column_avk);
    description = "the vertical axis is re-ordered from surface to top-of-atmosphere";
    snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.COLUMN_ABSORPTION.SOLAR_AVK", geoms_gas_name[gas]);
    harp_variable_definition_add_mapping(variable_definition, NULL, "solar measurement", gas_mapping_path, description);
    snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.COLUMN_ABSORPTION.LUNAR_AVK", geoms_gas_name[gas]);
    harp_variable_definition_add_mapping(variable_definition, NULL, "lunar measurement", gas_mapping_path, description);

    /* <gas>_column_number_density_uncertainty_random */
    snprintf(gas_var_name, MAX_NAME_LENGTH, "%s_column_number_density_uncertainty_random", harp_gas_name[gas]);
    snprintf(gas_description, MAX_DESCRIPTION_LENGTH, "random uncertainty of the total %s vertical column",
             harp_gas_name[gas]);
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, gas_var_name, harp_type_double, 1, dimension_type, NULL, gas_description,
         "molec/m2", NULL, read_column_uncertainty_random);
    description = "unit is converted to molec/m2";
    if (version == 1)
    {
        snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.COLUMN_ABSORPTION.SOLAR_UNCERTAINTY.RANDOM",
                 geoms_gas_name[gas]);
        harp_variable_definition_add_mapping(variable_definition, NULL, "solar measurement", gas_mapping_path,
                                             description);
        snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.COLUMN_ABSORPTION.LUNAR_UNCERTAINTY.RANDOM",
                 geoms_gas_name[gas]);
        harp_variable_definition_add_mapping(variable_definition, NULL, "lunar measurement", gas_mapping_path,
                                             description);
    }
    else
    {
        snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.COLUMN_ABSORPTION.SOLAR_UNCERTAINTY.RANDOM.STANDARD",
                 geoms_gas_name[gas]);
        harp_variable_definition_add_mapping(variable_definition, NULL, "solar measurement", gas_mapping_path,
                                             description);
        snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.COLUMN_ABSORPTION.LUNAR_UNCERTAINTY.RANDOM.STANDARD",
                 geoms_gas_name[gas]);
        harp_variable_definition_add_mapping(variable_definition, NULL, "lunar measurement", gas_mapping_path,
                                             description);
    }

    /* <gas>_column_number_density_uncertainty_systematic */
    snprintf(gas_var_name, MAX_NAME_LENGTH, "%s_column_number_density_uncertainty_systematic", harp_gas_name[gas]);
    snprintf(gas_description, MAX_DESCRIPTION_LENGTH, "systematic uncertainty of the total %s vertical column",
             harp_gas_name[gas]);
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, gas_var_name, harp_type_double, 1, dimension_type, NULL, gas_description,
         "molec/m2", NULL, read_column_uncertainty_systematic);
    description = "unit is converted to molec/m2";
    if (version == 1)
    {
        snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.COLUMN_ABSORPTION.SOLAR_UNCERTAINTY.SYSTEMATIC",
                 geoms_gas_name[gas]);
        harp_variable_definition_add_mapping(variable_definition, NULL, "solar measurement", gas_mapping_path,
                                             description);
        snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.COLUMN_ABSORPTION.LUNAR_UNCERTAINTY.SYSTEMATIC",
                 geoms_gas_name[gas]);
        harp_variable_definition_add_mapping(variable_definition, NULL, "lunar measurement", gas_mapping_path,
                                             description);
    }
    else
    {
        snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.COLUMN_ABSORPTION.SOLAR_UNCERTAINTY.SYSTEMATIC.STANDARD",
                 geoms_gas_name[gas]);
        harp_variable_definition_add_mapping(variable_definition, NULL, "solar measurement", gas_mapping_path,
                                             description);
        snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.COLUMN_ABSORPTION.LUNAR_UNCERTAINTY.SYSTEMATIC.STANDARD",
                 geoms_gas_name[gas]);
        harp_variable_definition_add_mapping(variable_definition, NULL, "lunar measurement", gas_mapping_path,
                                             description);
    }

    if (gas != ftir_H2O)
    {
        /* H2O_column_number_density */
        variable_definition = harp_ingestion_register_variable_full_read
            (product_definition, "H2O_column_number_density", harp_type_double, 1, dimension_type, NULL,
             "total H2O vertical column", "molec/m2", NULL, read_h2o_column);
        description = "unit is converted to molec/m2";
        harp_variable_definition_add_mapping(variable_definition, NULL, "solar measurement",
                                             "/H2O.COLUMN_ABSORPTION.SOLAR", description);
        harp_variable_definition_add_mapping(variable_definition, NULL, "lunar measurement",
                                             "/H2O.COLUMN_ABSORPTION.LUNAR", description);
    }

    /* <gas>_volume_mixing_ratio */
    snprintf(gas_var_name, MAX_NAME_LENGTH, "%s_volume_mixing_ratio", harp_gas_name[gas]);
    snprintf(gas_description, MAX_DESCRIPTION_LENGTH, "%s volume mixing ratio", harp_gas_name[gas]);
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, gas_var_name, harp_type_double, 2, dimension_type, NULL, gas_description, "ppmv",
         include_vmr_absorption, read_vmr);
    description = "unit is converted to ppmv; the vertical axis is re-ordered from surface to top-of-atmosphere";
    if (version == 1)
    {
        snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.MIXING.RATIO_ABSORPTION.SOLAR", geoms_gas_name[gas]);
        harp_variable_definition_add_mapping(variable_definition, NULL, "solar measurement", gas_mapping_path,
                                             description);
        snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.MIXING.RATIO_ABSORPTION.LUNAR", geoms_gas_name[gas]);
        harp_variable_definition_add_mapping(variable_definition, NULL, "lunar measurement", gas_mapping_path,
                                             description);
    }
    else
    {
        snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.MIXING.RATIO.VOLUME_ABSORPTION.SOLAR", geoms_gas_name[gas]);
        harp_variable_definition_add_mapping(variable_definition, NULL, "solar measurement", gas_mapping_path,
                                             description);
        snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.MIXING.RATIO.VOLUME_ABSORPTION.LUNAR", geoms_gas_name[gas]);
        harp_variable_definition_add_mapping(variable_definition, NULL, "lunar measurement", gas_mapping_path,
                                             description);
    }

    /* <gas>_volume_mixing_ratio_apriori */
    snprintf(gas_var_name, MAX_NAME_LENGTH, "%s_volume_mixing_ratio_apriori", harp_gas_name[gas]);
    snprintf(gas_description, MAX_DESCRIPTION_LENGTH, "a priori %s volume mixing ratio", harp_gas_name[gas]);
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, gas_var_name, harp_type_double, 2, dimension_type, NULL, gas_description, "ppmv",
         include_vmr_absorption, read_vmr_apriori);
    description = "unit is converted to ppmv; the vertical axis is re-ordered from surface to top-of-atmosphere";
    if (version == 1)
    {
        snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.MIXING.RATIO_ABSORPTION.SOLAR_APRIORI", geoms_gas_name[gas]);
        harp_variable_definition_add_mapping(variable_definition, NULL, "solar measurement", gas_mapping_path,
                                             description);
        snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.MIXING.RATIO_ABSORPTION.LUNAR_APRIORI", geoms_gas_name[gas]);
        harp_variable_definition_add_mapping(variable_definition, NULL, "lunar measurement", gas_mapping_path,
                                             description);
    }
    else
    {
        snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.MIXING.RATIO.VOLUME_ABSORPTION.SOLAR_APRIORI",
                 geoms_gas_name[gas]);
        harp_variable_definition_add_mapping(variable_definition, NULL, "solar measurement", gas_mapping_path,
                                             description);
        snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.MIXING.RATIO.VOLUME_ABSORPTION.LUNAR_APRIORI",
                 geoms_gas_name[gas]);
        harp_variable_definition_add_mapping(variable_definition, NULL, "lunar measurement", gas_mapping_path,
                                             description);
    }

    /* <gas>_volume_mixing_ratio_avk */
    snprintf(gas_var_name, MAX_NAME_LENGTH, "%s_volume_mixing_ratio_avk", harp_gas_name[gas]);
    snprintf(gas_description, MAX_DESCRIPTION_LENGTH, "averaging kernel for the %s volume mixing ratio",
             harp_gas_name[gas]);
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, gas_var_name, harp_type_double, 3, dimension_type, NULL, gas_description,
         HARP_UNIT_DIMENSIONLESS, include_vmr_absorption, read_vmr_avk);
    description = "the vertical axis is re-ordered from surface to top-of-atmosphere";
    if (version == 1)
    {
        snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.MIXING.RATIO_ABSORPTION.SOLAR_AVK", geoms_gas_name[gas]);
        harp_variable_definition_add_mapping(variable_definition, NULL, "solar measurement", gas_mapping_path,
                                             description);
        snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.MIXING.RATIO_ABSORPTION.LUNAR_AVK", geoms_gas_name[gas]);
        harp_variable_definition_add_mapping(variable_definition, NULL, "lunar measurement", gas_mapping_path,
                                             description);
    }
    else
    {
        snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.MIXING.RATIO.VOLUME_ABSORPTION.SOLAR_AVK",
                 geoms_gas_name[gas]);
        harp_variable_definition_add_mapping(variable_definition, NULL, "solar measurement", gas_mapping_path,
                                             description);
        snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.MIXING.RATIO.VOLUME_ABSORPTION.LUNAR_AVK",
                 geoms_gas_name[gas]);
        harp_variable_definition_add_mapping(variable_definition, NULL, "lunar measurement", gas_mapping_path,
                                             description);
    }

    /* <gas>_volume_mixing_ratio_covariance */
    snprintf(gas_var_name, MAX_NAME_LENGTH, "%s_volume_mixing_ratio_covariance", harp_gas_name[gas]);
    snprintf(gas_description, MAX_DESCRIPTION_LENGTH, "covariance of the %s volume mixing ratio", harp_gas_name[gas]);
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, gas_var_name, harp_type_double, 3, dimension_type, NULL, gas_description, "(ppmv)2",
         include_vmr_absorption, read_vmr_covariance);
    description = "unit is converted to (ppmv)2; the vertical axis is re-ordered from surface to top-of-atmosphere";
    if (version == 1)
    {
        snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.MIXING.RATIO_ABSORPTION.SOLAR_UNCERTAINTY.RANDOM",
                 geoms_gas_name[gas]);
        harp_variable_definition_add_mapping(variable_definition, NULL, "solar measurement", gas_mapping_path,
                                             description);
        snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.MIXING.RATIO_ABSORPTION.LUNAR_UNCERTAINTY.RANDOM",
                 geoms_gas_name[gas]);
        harp_variable_definition_add_mapping(variable_definition, NULL, "lunar measurement", gas_mapping_path,
                                             description);
    }
    else
    {
        snprintf(gas_mapping_path, MAX_PATH_LENGTH,
                 "/%s.MIXING.RATIO.VOLUME_ABSORPTION.SOLAR_UNCERTAINTY.RANDOM.COVARIANCE", geoms_gas_name[gas]);
        harp_variable_definition_add_mapping(variable_definition, NULL, "solar measurement", gas_mapping_path,
                                             description);
        snprintf(gas_mapping_path, MAX_PATH_LENGTH,
                 "/%s.MIXING.RATIO.VOLUME_ABSORPTION.LUNAR_UNCERTAINTY.RANDOM.COVARIANCE", geoms_gas_name[gas]);
        harp_variable_definition_add_mapping(variable_definition, NULL, "lunar measurement", gas_mapping_path,
                                             description);
    }

    /* <gas>_volume_mixing_ratio_uncertainty_random */
    snprintf(gas_var_name, MAX_NAME_LENGTH, "%s_volume_mixing_ratio_uncertainty_random", harp_gas_name[gas]);
    snprintf(gas_description, MAX_DESCRIPTION_LENGTH, "random uncertainty of the %s volume mixing ratio",
             harp_gas_name[gas]);
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, gas_var_name, harp_type_double, 2, dimension_type, NULL, gas_description, "ppmv",
         include_vmr_absorption, read_vmr_uncertainty_random);
    description = "the vertical axis is re-ordered from surface to top-of-atmosphere; "
        "the uncertainty is the square root of the trace of the covariance; unit is converted to ppmv";
    if (version == 1)
    {
        snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.MIXING.RATIO_ABSORPTION.SOLAR_UNCERTAINTY.RANDOM",
                 geoms_gas_name[gas]);
        harp_variable_definition_add_mapping(variable_definition, NULL, "solar measurement", gas_mapping_path,
                                             description);
        snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.MIXING.RATIO_ABSORPTION.LUNAR_UNCERTAINTY.RANDOM",
                 geoms_gas_name[gas]);
        harp_variable_definition_add_mapping(variable_definition, NULL, "lunar measurement", gas_mapping_path,
                                             description);
    }
    else
    {
        snprintf(gas_mapping_path, MAX_PATH_LENGTH,
                 "/%s.MIXING.RATIO.VOLUME_ABSORPTION.SOLAR_UNCERTAINTY.RANDOM.COVARIANCE", geoms_gas_name[gas]);
        harp_variable_definition_add_mapping(variable_definition, NULL, "solar measurement", gas_mapping_path,
                                             description);
        snprintf(gas_mapping_path, MAX_PATH_LENGTH,
                 "/%s.MIXING.RATIO.VOLUME_ABSORPTION.LUNAR_UNCERTAINTY.RANDOM.COVARIANCE", geoms_gas_name[gas]);
        harp_variable_definition_add_mapping(variable_definition, NULL, "lunar measurement", gas_mapping_path,
                                             description);
    }

    /* <gas>_volume_mixing_ratio_uncertainty_systematic */
    snprintf(gas_var_name, MAX_NAME_LENGTH, "%s_volume_mixing_ratio_uncertainty_systematic", harp_gas_name[gas]);
    snprintf(gas_description, MAX_DESCRIPTION_LENGTH, "systematic uncertainty of the %s volume mixing ratio",
             harp_gas_name[gas]);
    variable_definition = harp_ingestion_register_variable_full_read
        (product_definition, gas_var_name, harp_type_double, 2, dimension_type, NULL, gas_description, "ppmv",
         include_vmr_absorption, read_vmr_uncertainty_systematic);
    if (version == 1)
    {
        snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.MIXING.RATIO_ABSORPTION.SOLAR_UNCERTAINTY.SYSTEMATIC",
                 geoms_gas_name[gas]);
        harp_variable_definition_add_mapping(variable_definition, NULL, "solar measurement", gas_mapping_path,
                                             description);
        snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.MIXING.RATIO_ABSORPTION.LUNAR_UNCERTAINTY.SYSTEMATIC",
                 geoms_gas_name[gas]);
        harp_variable_definition_add_mapping(variable_definition, NULL, "lunar measurement", gas_mapping_path,
                                             description);
    }
    else
    {
        snprintf(gas_mapping_path, MAX_PATH_LENGTH,
                 "/%s.MIXING.RATIO.VOLUME_ABSORPTION.SOLAR_UNCERTAINTY.SYSTEMATIC.COVARIANCE", geoms_gas_name[gas]);
        harp_variable_definition_add_mapping(variable_definition, NULL, "solar measurement", gas_mapping_path,
                                             description);
        snprintf(gas_mapping_path, MAX_PATH_LENGTH,
                 "/%s.MIXING.RATIO.VOLUME_ABSORPTION.LUNAR_UNCERTAINTY.SYSTEMATIC.COVARIANCE", geoms_gas_name[gas]);
        harp_variable_definition_add_mapping(variable_definition, NULL, "lunar measurement", gas_mapping_path,
                                             description);
    }


    if (gas != ftir_H2O)
    {
        /* H2O_volume_mixing_ratio */
        variable_definition = harp_ingestion_register_variable_full_read
            (product_definition, "H2O_volume_mixing_ratio", harp_type_double, 2, dimension_type, NULL,
             "H2O volume mixing ratio", "ppmv", NULL, read_h2o_vmr);
        description = "unit is converted to ppmv; the vertical axis is re-ordered from surface to top-of-atmosphere";
        if (version == 1)
        {
            harp_variable_definition_add_mapping(variable_definition, NULL, "solar measurement",
                                                 "/H2O.MIXING.RATIO_ABSORPTION.SOLAR", description);
            harp_variable_definition_add_mapping(variable_definition, NULL, "lunar measurement",
                                                 "/H2O.MIXING.RATIO_ABSORPTION.LUNAR", description);
        }
        else
        {
            harp_variable_definition_add_mapping(variable_definition, NULL, "solar measurement",
                                                 "/H2O.MIXING.RATIO.VOLUME_ABSORPTION.SOLAR", description);
            harp_variable_definition_add_mapping(variable_definition, NULL, "lunar measurement",
                                                 "H2O.MIXING.RATIO.VOLUME_ABSORPTION.LUNAR", description);
        }
    }


    /* altitude */
    description = "retrieval effective altitude";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "altitude", harp_type_double,
                                                                     2, dimension_type, NULL, description, "km", NULL,
                                                                     read_altitude);
    description = "the vertical axis is re-ordered from surface to top-of-atmosphere";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ALTITUDE", description);

    /* altitude_bounds */
    dimension_type[2] = harp_dimension_independent;
    description = "lower and upper boundaries of the height layers";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "altitude_bounds",
                                                                     harp_type_double, 3, dimension_type, dimension,
                                                                     description, "km", NULL, read_altitude_bounds);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ALTITUDE.BOUNDS", description);

    /* pressure */
    description = "independent pressure profile";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "pressure", harp_type_double,
                                                                     2, dimension_type, NULL, description, "hPa", NULL,
                                                                     read_pressure_ind);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/PRESSURE_INDEPENDENT", description);

    /* temperature */
    description = "independent temperature profile";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "temperature",
                                                                     harp_type_double, 2, dimension_type, NULL,
                                                                     description, "K", NULL, read_temperature_ind);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/TEMPERATURE_INDEPENDENT", description);

    /* surface_pressure */
    description = "independent surface pressure";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "surface_pressure",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, "hPa", NULL,
                                                                     read_surface_pressure_ind);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/SURFACE.PRESSURE_INDEPENDENT", NULL);

    /* surface_temperature */
    description = "independent surface temperature";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "surface_temperature",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, "K", NULL,
                                                                     read_surface_temperature_ind);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/SURFACE.TEMPERATURE_INDEPENDENT", NULL);

    /* solar_azimuth_angle */
    description = "solar azimuth angle";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "solar_azimuth_angle",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, "degree", NULL,
                                                                     read_solar_azimuth_angle);
    harp_variable_definition_add_mapping(variable_definition, NULL, "solar measurement", "/ANGLE.SOLAR_AZIMUTH", NULL);
    harp_variable_definition_add_mapping(variable_definition, NULL, "lunar measurement", "/ANGLE.LUNAR_AZIMUTH", NULL);

    /* solar_zenith_angle */
    description = "solar zenith angle";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "solar_zenith_angle",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, "degree", NULL,
                                                                     read_solar_zenith_angle);
    harp_variable_definition_add_mapping(variable_definition, NULL, "solar measurement",
                                         "/ANGLE.SOLAR_ZENITH.ASTRONOMICAL", NULL);
    harp_variable_definition_add_mapping(variable_definition, NULL, "lunar measurement",
                                         "/ANGLE.LUNAR_ZENITH.ASTRONOMICAL", NULL);

    return 0;
}

int harp_ingestion_module_geoms_ftir_init(void)
{
    harp_ingestion_module *module;
    int i;

    module = harp_ingestion_register_module("GEOMS-TE-FTIR", "GEOMS", "GEOMS", "FTIR", "GEOMS template for FTIR",
                                                 ingestion_init, ingestion_done);

    for (i = 0; i < num_ftir_gas; i++)
    {
        init_product_definition(module, i, 1);
        init_product_definition(module, i, 2);
    }

    return 0;
}
