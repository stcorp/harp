/*
 * Copyright (C) 2015-2017 S[&]T, The Netherlands.
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

typedef enum uvvis_doas_type_enum
{
    uvvis_doas_directsun,
    uvvis_doas_offaxis,
    uvvis_doas_offaxis_aerosol,
    uvvis_doas_zenith,
} uvvis_doas_type;

typedef enum uvvis_doas_gas_enum
{
    uvvis_doas_BrO,
    uvvis_doas_CHOCHO,
    uvvis_doas_H2CO,
    uvvis_doas_H2O,
    uvvis_doas_HONO,
    uvvis_doas_IO,
    uvvis_doas_NO2,
    uvvis_doas_O3,
    uvvis_doas_OClO,
    uvvis_doas_SO2,
    num_uvvis_doas_gas
} uvvis_doas_gas;

static const char *geoms_gas_name[num_uvvis_doas_gas] = {
    "BrO",
    "CHOCHO",
    "H2CO",
    "H2O",
    "HONO",
    "IO",
    "NO2",
    "O3",
    "OClO",
    "SO2",
};

static const char *harp_gas_name[num_uvvis_doas_gas] = {
    "BrO",
    "C2H2O2",
    "HCOH",
    "H2O",
    "HNO2",
    "IO",
    "NO2",
    "O3",
    "OClO",
    "SO2",
};

typedef struct ingest_info_struct
{
    const harp_product_definition *definition;
    int product_version;
    coda_product *product;
    uvvis_doas_gas gas;
    uvvis_doas_type template_type;
    const char *template;
    long num_time;
    long num_spectral;
    long num_vertical;
    int invert_vertical;        /* should all data long the vertical axis be inverted? */
    int swap_alt_bounds;        /* convert [INDEPENDENT;ALTITUDE] to [ALTITUDE;INDEPENDENT]? */
    int aod_variant;    /* 0:modeled, 1:measured */
    int has_latitude;
    int has_longitude;
    int has_stratospheric_aod;
    int has_vmr_zenith;
    int has_tropo_column_zenith;
    int has_wind_direction;
    int has_wind_speed;
    char vmr_unit[MAX_UNIT_LENGTH];
    char vmr_covariance_unit[MAX_UNIT_LENGTH];
} ingest_info;

static int read_dimensions(void *user_data, long dimension[HARP_NUM_DIM_TYPES])
{
    ingest_info *info = (ingest_info *)user_data;

    dimension[harp_dimension_time] = info->num_time;
    dimension[harp_dimension_spectral] = info->num_spectral;
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

static int read_vertical_variable_double_replicated(void *user_data, const char *path, long num_time, long num_elements,
                                                    harp_array data)
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
    if (actual_num_elements != num_elements && actual_num_elements != (num_elements / num_time))
    {
        harp_set_error(HARP_ERROR_INGESTION, "variable %s has %ld elements (expected %ld or %ld)", path,
                       actual_num_elements, (num_elements / num_time), num_elements);
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
        for (i = 0; i < actual_num_elements; i++)
        {
            if (data.double_data[i] == fill_value)
            {
                data.double_data[i] = harp_nan();
            }
        }
    }

    if (actual_num_elements < num_elements)
    {
        if (((ingest_info *)user_data)->invert_vertical)
        {
            if (harp_array_invert(harp_type_double, 0, 1, &actual_num_elements, data) != 0)
            {
                return -1;
            }
        }
        for (i = 1; i < num_time; i++)
        {
            memcpy(&data.double_data[i * actual_num_elements], data.double_data, actual_num_elements * sizeof(double));
        }
    }
    else
    {
        if (((ingest_info *)user_data)->invert_vertical)
        {
            long dimension[2];

            dimension[0] = num_time;
            dimension[1] = num_elements / num_time;
            if (harp_array_invert(harp_type_double, 1, 2, dimension, data) != 0)
            {
                return -1;
            }
        }
    }

    return 0;
}

static int read_variable_string(void *user_data, const char *path, long index, long num_elements, harp_array data)
{
    coda_cursor cursor;
    long dim[CODA_MAX_NUM_DIMS];
    int num_dims;
    long length;

    if (coda_cursor_set_product(&cursor, ((ingest_info *)user_data)->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, "/CLOUD_CONDITIONS") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_array_dim(&cursor, &num_dims, dim) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (dim[0] != num_elements)
    {
        harp_set_error(HARP_ERROR_INGESTION, "first dimension of variable %s has %ld elements (expected %ld)", path,
                       dim[0], num_elements);
        return -1;
    }
    if (num_dims > 2)
    {
        harp_set_error(HARP_ERROR_INGESTION, "variable %s has %d dimensions (expected <= 2)", num_dims);
        return -1;
    }
    else if (num_dims == 2)
    {
        /* assume that this is a character array where the last dimension is the string length */
        length = dim[1];
        data.string_data[0] = malloc(length + 1);
        if (data.string_data[0] == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)", length + 1,
                           __FILE__, __LINE__);
            return -1;
        }
        if (coda_cursor_read_char_partial_array(&cursor, index * length, length, data.string_data[0]) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        data.string_data[0][length] = '\0';
    }
    else
    {
        if (coda_cursor_goto_array_element_by_index(&cursor, index) != 0)
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
    }

    return 0;
}

static int read_altitude_bounds(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (read_vertical_variable_double_replicated(user_data, "ALTITUDE_BOUNDARIES", info->num_time,
                                                 info->num_time * info->num_vertical * 2, data) != 0)
    {
        return -1;
    }

    if (info->swap_alt_bounds)
    {
        harp_array sub_array;
        long dimension[2];
        int i;

        dimension[0] = 2;
        dimension[1] = info->num_vertical;
        for (i = 0; i < info->num_time; i++)
        {
            sub_array.double_data = &data.double_data[i * 2 * info->num_vertical];
            /* swap [2,ALTITUDE] to [ALTITUDE,2] */
            if (harp_array_transpose(harp_type_double, 2, dimension, NULL, sub_array) != 0)
            {
                return -1;
            }
        }
    }

    if (info->invert_vertical)
    {
        long dimension[2];

        /* swap 'low'/'high' for each layer */
        dimension[0] = info->num_time * info->num_vertical;
        dimension[1] = 2;
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

static int read_datetime(void *user_data, harp_array data)
{
    return read_variable_double(user_data, "DATETIME", ((ingest_info *)user_data)->num_time, data);
}

static int read_datetime_start(void *user_data, harp_array data)
{
    return read_variable_double(user_data, "DATETIME_START", ((ingest_info *)user_data)->num_time, data);
}

static int read_datetime_stop(void *user_data, harp_array data)
{
    return read_variable_double(user_data, "DATETIME_STOP", ((ingest_info *)user_data)->num_time, data);
}

static int read_instrument_latitude(void *user_data, harp_array data)
{
    return read_variable_double(user_data, "LATITUDE_INSTRUMENT", 1, data);
}

static int read_instrument_longitude(void *user_data, harp_array data)
{
    return read_variable_double(user_data, "LONGITUDE_INSTRUMENT", 1, data);
}

static int read_instrument_altitude(void *user_data, harp_array data)
{
    return read_variable_double(user_data, "ALTITUDE_INSTRUMENT", 1, data);
}

static int read_wavelength(void *user_data, harp_array data)
{
    return read_variable_double(user_data, "WAVELENGTH", ((ingest_info *)user_data)->num_spectral, data);
}

static int read_altitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_vertical_variable_double_replicated(user_data, "ALTITUDE", info->num_time,
                                                    info->num_time * info->num_vertical, data);
}

static int read_viewing_azimuth_angle(void *user_data, harp_array data)
{
    return read_variable_double(user_data, "ANGLE_VIEW_AZIMUTH", ((ingest_info *)user_data)->num_time, data);
}

static int read_viewing_zenith_angle(void *user_data, harp_array data)
{
    return read_variable_double(user_data, "ANGLE_VIEW_ZENITH", ((ingest_info *)user_data)->num_time, data);
}

static int read_solar_azimuth_angle(void *user_data, harp_array data)
{
    return read_variable_double(user_data, "ANGLE_SOLAR_AZIMUTH", ((ingest_info *)user_data)->num_time, data);
}

static int read_solar_zenith_angle(void *user_data, harp_array data)
{
    return read_variable_double(user_data, "ANGLE_SOLAR_ZENITH_ASTRONOMICAL", ((ingest_info *)user_data)->num_time,
                                data);
}

static int read_latitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_vertical_variable_double(user_data, "LATITUDE", info->num_time * info->num_vertical, data);
}

static int read_longitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_vertical_variable_double(user_data, "LONGITUDE", info->num_time * info->num_vertical, data);
}

static int read_wind_direction(void *user_data, harp_array data)
{
    return read_variable_double(user_data, "WIND_DIRECTION_SURFACE_INDEPENDENT", ((ingest_info *)user_data)->num_time,
                                data);
}

static int read_wind_speed(void *user_data, harp_array data)
{
    return read_variable_double(user_data, "WIND_SPEED_SURFACE_INDEPENDENT", ((ingest_info *)user_data)->num_time,
                                data);
}

static int read_cloud_conditions(void *user_data, long index, harp_array data)
{
    return read_variable_string(user_data, "CLOUD_CONDITIONS", index, ((ingest_info *)user_data)->num_time, data);
}

static int read_stratospheric_aod(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    const char *path;

    path = info->aod_variant == 0 ? "/AEROSOL_OPTICAL_DEPTH_STRATOSPHERIC_INDEPENDENT" :
        "/AEROSOL_OPTICAL_DEPTH_STRATOSPHERIC_SCATTER_SOLAR_ZENITH";
    return read_variable_double(user_data, path, info->num_time, data);
}

static int read_pressure_ind(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_vertical_variable_double(user_data, "PRESSURE_INDEPENDENT", info->num_time * info->num_vertical, data);
}

static int read_temperature_ind(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_vertical_variable_double_replicated(user_data, "TEMPERATURE_INDEPENDENT", info->num_time,
                                                    info->num_time * info->num_vertical, data);
}

static int read_column_solar(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char path[MAX_PATH_LENGTH];

    snprintf(path, MAX_PATH_LENGTH, "/%s_COLUMN_ABSORPTION_SOLAR", geoms_gas_name[info->gas]);
    return read_variable_double(user_data, path, info->num_time, data);
}

static int read_column_solar_uncertainty_random(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char path[MAX_PATH_LENGTH];

    snprintf(path, MAX_PATH_LENGTH, "/%s_COLUMN_ABSORPTION_SOLAR_UNCERTAINTY_RANDOM_STANDARD",
             geoms_gas_name[info->gas]);
    return read_variable_double(user_data, path, info->num_time, data);
}

static int read_column_solar_uncertainty_systematic(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char path[MAX_PATH_LENGTH];

    snprintf(path, MAX_PATH_LENGTH, "/%s_COLUMN_ABSORPTION_SOLAR_UNCERTAINTY_SYSTEMATIC_STANDARD",
             geoms_gas_name[info->gas]);
    return read_variable_double(user_data, path, info->num_time, data);
}

static int read_column_solar_apriori(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char path[MAX_PATH_LENGTH];

    snprintf(path, MAX_PATH_LENGTH, "/%s_COLUMN_ABSORPTION_SOLAR_APRIORI", geoms_gas_name[info->gas]);
    return read_variable_double(user_data, path, info->num_time, data);
}

static int read_partial_column_solar_apriori(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char path[MAX_PATH_LENGTH];

    snprintf(path, MAX_PATH_LENGTH, "/%s_COLUMN_PARTIAL_ABSORPTION_SOLAR_APRIORI", geoms_gas_name[info->gas]);
    return read_vertical_variable_double(user_data, path, info->num_time * info->num_vertical, data);
}

static int read_column_solar_avk(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char path[MAX_PATH_LENGTH];

    snprintf(path, MAX_PATH_LENGTH, "/%s_COLUMN_ABSORPTION_SOLAR_AVK", geoms_gas_name[info->gas]);
    return read_vertical_variable_double(user_data, path, info->num_time * info->num_vertical, data);
}

static int read_vmr_offaxis(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char path[MAX_PATH_LENGTH];

    snprintf(path, MAX_PATH_LENGTH, "/%s_MIXING_RATIO_VOLUME_SCATTER_SOLAR_OFFAXIS", geoms_gas_name[info->gas]);
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

static int read_vmr_offaxis_covariance(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char path[MAX_PATH_LENGTH];

    snprintf(path, MAX_PATH_LENGTH, "/%s_MIXING_RATIO_VOLUME_SCATTER_SOLAR_OFFAXIS_UNCERTAINTY_RANDOM_COVARIANCE",
             geoms_gas_name[info->gas]);
    if (read_vertical2d_variable_double(user_data, path, info->num_time * info->num_vertical * info->num_vertical, data)
        != 0)
    {
        return -1;
    }

    if (harp_convert_unit(info->vmr_covariance_unit, "(ppmv)2",
                          info->num_time * info->num_vertical * info->num_vertical, data.double_data) != 0)
    {
        return -1;
    }

    return 0;
}

static int read_vmr_offaxis_uncertainty_random(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char path[MAX_PATH_LENGTH];

    snprintf(path, MAX_PATH_LENGTH, "/%s_MIXING_RATIO_VOLUME_SCATTER_SOLAR_OFFAXIS_UNCERTAINTY_RANDOM_COVARIANCE",
             geoms_gas_name[info->gas]);
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

static int read_vmr_offaxis_uncertainty_systematic(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char path[MAX_PATH_LENGTH];

    snprintf(path, MAX_PATH_LENGTH, "/%s_MIXING_RATIO_VOLUME_SCATTER_SOLAR_OFFAXIS_UNCERTAINTY_SYSTEMATIC_COVARIANCE",
             geoms_gas_name[info->gas]);
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

static int read_vmr_offaxis_apriori(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char path[MAX_PATH_LENGTH];

    snprintf(path, MAX_PATH_LENGTH, "/%s_MIXING_RATIO_VOLUME_SCATTER_SOLAR_OFFAXIS_APRIORI", geoms_gas_name[info->gas]);
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

static int read_vmr_offaxis_avk(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char path[MAX_PATH_LENGTH];

    snprintf(path, MAX_PATH_LENGTH, "/%s_MIXING_RATIO_VOLUME_SCATTER_SOLAR_OFFAXIS_AVK", geoms_gas_name[info->gas]);
    return read_vertical2d_variable_double(user_data, path, info->num_time * info->num_vertical * info->num_vertical,
                                           data);
}

static int read_tropo_column_offaxis(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char path[MAX_PATH_LENGTH];

    snprintf(path, MAX_PATH_LENGTH, "/%s_COLUMN_TROPOSPHERIC_SCATTER_SOLAR_OFFAXIS", geoms_gas_name[info->gas]);
    return read_variable_double(user_data, path, info->num_time, data);
}

static int read_tropo_column_offaxis_uncertainty_random(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char path[MAX_PATH_LENGTH];

    snprintf(path, MAX_PATH_LENGTH, "/%s_COLUMN_TROPOSPHERIC_SCATTER_SOLAR_OFFAXIS_UNCERTAINTY_RANDOM_STANDARD",
             geoms_gas_name[info->gas]);
    return read_variable_double(user_data, path, info->num_time, data);
}

static int read_tropo_column_offaxis_uncertainty_systematic(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char path[MAX_PATH_LENGTH];

    snprintf(path, MAX_PATH_LENGTH, "/%s_COLUMN_TROPOSPHERIC_SCATTER_SOLAR_OFFAXIS_UNCERTAINTY_SYSTEMATIC_STANDARD",
             geoms_gas_name[info->gas]);
    return read_variable_double(user_data, path, info->num_time, data);
}

static int read_tropo_column_offaxis_apriori(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char path[MAX_PATH_LENGTH];

    snprintf(path, MAX_PATH_LENGTH, "/%s_COLUMN_TROPOSPHERIC_SCATTER_SOLAR_OFFAXIS_APRIORI", geoms_gas_name[info->gas]);
    return read_variable_double(user_data, path, info->num_time, data);
}

static int read_tropo_column_offaxis_avk(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char path[MAX_PATH_LENGTH];

    snprintf(path, MAX_PATH_LENGTH, "/%s_COLUMN_TROPOSPHERIC_SCATTER_SOLAR_OFFAXIS_AVK", geoms_gas_name[info->gas]);
    return read_vertical_variable_double(user_data, path, info->num_time * info->num_vertical, data);
}

static int read_partial_column_offaxis(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char path[MAX_PATH_LENGTH];

    snprintf(path, MAX_PATH_LENGTH, "/%s_COLUMN_PARTIAL_SCATTER_SOLAR_OFFAXIS", geoms_gas_name[info->gas]);
    return read_vertical_variable_double(user_data, path, info->num_time * info->num_vertical, data);
}

static int read_partial_column_offaxis_apriori(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char path[MAX_PATH_LENGTH];

    snprintf(path, MAX_PATH_LENGTH, "/%s_COLUMN_PARTIAL_SCATTER_SOLAR_OFFAXIS_APRIORI", geoms_gas_name[info->gas]);
    return read_vertical_variable_double(user_data, path, info->num_time * info->num_vertical, data);
}

static int read_vmr_zenith(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char path[MAX_PATH_LENGTH];

    snprintf(path, MAX_PATH_LENGTH, "/%s_MIXING_RATIO_VOLUME_SCATTER_SOLAR_ZENITH", geoms_gas_name[info->gas]);
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

static int read_vmr_zenith_covariance(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char path[MAX_PATH_LENGTH];

    snprintf(path, MAX_PATH_LENGTH, "/%s_MIXING_RATIO_VOLUME_SCATTER_SOLAR_ZENITH_UNCERTAINTY_RANDOM_COVARIANCE",
             geoms_gas_name[info->gas]);
    if (read_vertical2d_variable_double(user_data, path, info->num_time * info->num_vertical * info->num_vertical, data)
        != 0)
    {
        return -1;
    }

    if (harp_convert_unit(info->vmr_covariance_unit, "(ppmv)2",
                          info->num_time * info->num_vertical * info->num_vertical, data.double_data) != 0)
    {
        return -1;
    }

    return 0;
}

static int read_vmr_zenith_uncertainty_random(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char path[MAX_PATH_LENGTH];

    snprintf(path, MAX_PATH_LENGTH, "/%s_MIXING_RATIO_VOLUME_SCATTER_SOLAR_ZENITH_UNCERTAINTY_RANDOM_COVARIANCE",
             geoms_gas_name[info->gas]);
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

static int read_vmr_zenith_uncertainty_systematic(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char path[MAX_PATH_LENGTH];

    snprintf(path, MAX_PATH_LENGTH, "/%s_MIXING_RATIO_VOLUME_SCATTER_SOLAR_ZENITH_UNCERTAINTY_SYSTEMATIC_COVARIANCE",
             geoms_gas_name[info->gas]);
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

static int read_vmr_zenith_apriori(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char path[MAX_PATH_LENGTH];

    snprintf(path, MAX_PATH_LENGTH, "/%s_MIXING_RATIO_VOLUME_SCATTER_SOLAR_ZENITH_APRIORI", geoms_gas_name[info->gas]);
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

static int read_vmr_zenith_avk(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char path[MAX_PATH_LENGTH];

    snprintf(path, MAX_PATH_LENGTH, "/%s_MIXING_RATIO_VOLUME_SCATTER_SOLAR_ZENITH_AVK", geoms_gas_name[info->gas]);
    return read_vertical2d_variable_double(user_data, path, info->num_time * info->num_vertical * info->num_vertical,
                                           data);
}

static int read_tropo_column_zenith(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char path[MAX_PATH_LENGTH];

    snprintf(path, MAX_PATH_LENGTH, "/%s_COLUMN_TROPOSPHERIC_SCATTER_SOLAR_ZENITH", geoms_gas_name[info->gas]);
    return read_variable_double(user_data, path, info->num_time, data);
}

static int read_tropo_column_zenith_uncertainty_random(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char path[MAX_PATH_LENGTH];

    snprintf(path, MAX_PATH_LENGTH, "/%s_COLUMN_TROPOSPHERIC_SCATTER_SOLAR_ZENITH_UNCERTAINTY_RANDOM_STANDARD",
             geoms_gas_name[info->gas]);
    return read_variable_double(user_data, path, info->num_time, data);
}

static int read_tropo_column_zenith_uncertainty_systematic(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char path[MAX_PATH_LENGTH];

    snprintf(path, MAX_PATH_LENGTH, "/%s_COLUMN_TROPOSPHERIC_SCATTER_SOLAR_ZENITH_UNCERTAINTY_SYSTEMATIC_STANDARD",
             geoms_gas_name[info->gas]);
    return read_variable_double(user_data, path, info->num_time, data);
}

static int read_tropo_column_zenith_apriori(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char path[MAX_PATH_LENGTH];

    snprintf(path, MAX_PATH_LENGTH, "/%s_COLUMN_TROPOSPHERIC_SCATTER_SOLAR_ZENITH_APRIORI", geoms_gas_name[info->gas]);
    return read_variable_double(user_data, path, info->num_time, data);
}

static int read_tropo_column_zenith_avk(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char path[MAX_PATH_LENGTH];

    snprintf(path, MAX_PATH_LENGTH, "/%s_COLUMN_TROPOSPHERIC_SCATTER_SOLAR_ZENITH_AVK", geoms_gas_name[info->gas]);
    return read_vertical_variable_double(user_data, path, info->num_time * info->num_vertical, data);
}

static int read_strat_column_zenith(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char path[MAX_PATH_LENGTH];

    snprintf(path, MAX_PATH_LENGTH, "/%s_COLUMN_STRATOSPHERIC_SCATTER_SOLAR_ZENITH", geoms_gas_name[info->gas]);
    return read_variable_double(user_data, path, info->num_time, data);
}

static int read_strat_column_zenith_uncertainty_random(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char path[MAX_PATH_LENGTH];

    snprintf(path, MAX_PATH_LENGTH, "/%s_COLUMN_STRATOSPHERIC_SCATTER_SOLAR_ZENITH_UNCERTAINTY_RANDOM_STANDARD",
             geoms_gas_name[info->gas]);
    return read_variable_double(user_data, path, info->num_time, data);
}

static int read_strat_column_zenith_uncertainty_systematic(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char path[MAX_PATH_LENGTH];

    snprintf(path, MAX_PATH_LENGTH, "/%s_COLUMN_STRATOSPHERIC_SCATTER_SOLAR_ZENITH_UNCERTAINTY_SYSTEMATIC_STANDARD",
             geoms_gas_name[info->gas]);
    return read_variable_double(user_data, path, info->num_time, data);
}

static int read_strat_column_zenith_apriori(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char path[MAX_PATH_LENGTH];

    snprintf(path, MAX_PATH_LENGTH, "/%s_COLUMN_STRATOSPHERIC_SCATTER_SOLAR_ZENITH_APRIORI", geoms_gas_name[info->gas]);
    return read_variable_double(user_data, path, info->num_time, data);
}

static int read_strat_column_zenith_avk(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char path[MAX_PATH_LENGTH];

    snprintf(path, MAX_PATH_LENGTH, "/%s_COLUMN_STRATOSPHERIC_SCATTER_SOLAR_ZENITH_AVK", geoms_gas_name[info->gas]);
    return read_vertical_variable_double(user_data, path, info->num_time * info->num_vertical, data);
}

static int read_strat_column_zenith_amf(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char path[MAX_PATH_LENGTH];

    snprintf(path, MAX_PATH_LENGTH, "/%s_COLUMN_STRATOSPHERIC_SCATTER_SOLAR_ZENITH_AMF", geoms_gas_name[info->gas]);
    return read_variable_double(user_data, path, info->num_time, data);
}

static int read_partial_column_zenith(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char path[MAX_PATH_LENGTH];

    snprintf(path, MAX_PATH_LENGTH, "/%s_COLUMN_PARTIAL_SCATTER_SOLAR_ZENITH", geoms_gas_name[info->gas]);
    return read_vertical_variable_double(user_data, path, info->num_time * info->num_vertical, data);
}

static int read_partial_column_zenith_apriori(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char path[MAX_PATH_LENGTH];

    snprintf(path, MAX_PATH_LENGTH, "/%s_COLUMN_PARTIAL_SCATTER_SOLAR_ZENITH_APRIORI", geoms_gas_name[info->gas]);
    return read_vertical_variable_double(user_data, path, info->num_time * info->num_vertical, data);
}

static int read_aerosol_extinction_coefficient(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_vertical_variable_double(user_data, "/AEROSOL_EXTINCTION_COEFFICIENT_SCATTER_SOLAR_OFFAXIS",
                                         info->num_time * info->num_spectral * info->num_vertical, data);
}

static int read_aerosol_extinction_coefficient_covariance(void *user_data, harp_array data)
{
    const char *path = "/AEROSOL_EXTINCTION_COEFFICIENT_SCATTER_SOLAR_OFFAXIS_UNCERTAINTY_RANDOM_COVARIANCE";
    ingest_info *info = (ingest_info *)user_data;

    return read_vertical2d_variable_double(user_data, path, info->num_time * info->num_spectral * info->num_vertical *
                                           info->num_vertical, data);
}

static int read_aerosol_extinction_coefficient_uncertainty_random(void *user_data, harp_array data)
{
    const char *path = "/AEROSOL_EXTINCTION_COEFFICIENT_SCATTER_SOLAR_OFFAXIS_UNCERTAINTY_RANDOM_COVARIANCE";
    ingest_info *info = (ingest_info *)user_data;

    return read_vertical_sqrt_2dtrace_variable_double(user_data, path, info->num_time * info->num_spectral *
                                                      info->num_vertical, data);
}

static int read_aerosol_extinction_coefficient_uncertainty_systematic(void *user_data, harp_array data)
{
    const char *path = "/AEROSOL_EXTINCTION_COEFFICIENT_SCATTER_SOLAR_OFFAXIS_UNCERTAINTY_SYSTEMATIC_COVARIANCE";
    ingest_info *info = (ingest_info *)user_data;

    return read_vertical_sqrt_2dtrace_variable_double(user_data, path, info->num_time * info->num_spectral *
                                                      info->num_vertical, data);
}

static int read_aerosol_extinction_coefficient_apriori(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_vertical_variable_double(user_data, "/AEROSOL_EXTINCTION_COEFFICIENT_SCATTER_SOLAR_OFFAXIS_APRIORI",
                                         info->num_time * info->num_spectral * info->num_vertical, data);
}

static int read_aerosol_extinction_coefficient_avk(void *user_data, harp_array data)
{
    const char *path = "/AEROSOL_EXTINCTION_COEFFICIENT_SCATTER_SOLAR_OFFAXIS_AVK";
    ingest_info *info = (ingest_info *)user_data;

    return read_vertical2d_variable_double(user_data, path, info->num_time * info->num_spectral * info->num_vertical *
                                           info->num_vertical, data);
}

static int read_tropo_aerosol_optical_depth(void *user_data, harp_array data)
{
    const char *path = "/AEROSOL_OPTICAL_DEPTH_TROPOSPHERIC_SCATTER_SOLAR_OFFAXIS";
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_double(user_data, path, info->num_time * info->num_spectral, data);
}

static int read_tropo_aerosol_optical_depth_uncertainty_random(void *user_data, harp_array data)
{
    const char *path = "/AEROSOL_OPTICAL_DEPTH_TROPOSPHERIC_SCATTER_SOLAR_OFFAXIS_UNCERTAINTY_RANDOM_STANDARD";
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_double(user_data, path, info->num_time * info->num_spectral, data);
}

static int read_tropo_aerosol_optical_depth_uncertainty_systematic(void *user_data, harp_array data)
{
    const char *path = "/AEROSOL_OPTICAL_DEPTH_TROPOSPHERIC_SCATTER_SOLAR_OFFAXIS_UNCERTAINTY_SYSTEMATIC_STANDARD";
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_double(user_data, path, info->num_time * info->num_spectral, data);
}

static int read_tropo_aerosol_optical_depth_apriori(void *user_data, harp_array data)
{
    const char *path = "/AEROSOL_OPTICAL_DEPTH_TROPOSPHERIC_SCATTER_SOLAR_OFFAXIS_APRIORI";
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_double(user_data, path, info->num_time * info->num_spectral, data);
}

static int read_tropo_aerosol_optical_depth_avk(void *user_data, harp_array data)
{
    const char *path = "/AEROSOL_OPTICAL_DEPTH_TROPOSPHERIC_SCATTER_SOLAR_OFFAXIS_AVK";
    ingest_info *info = (ingest_info *)user_data;

    return read_vertical_variable_double(user_data, path, info->num_time * info->num_spectral * info->num_vertical,
                                         data);
}


static int exclude_latitude(void *user_data)
{
    return !((ingest_info *)user_data)->has_latitude;
}

static int exclude_longitude(void *user_data)
{
    return !((ingest_info *)user_data)->has_longitude;
}

static int exclude_stratospheric_aod(void *user_data)
{
    return !((ingest_info *)user_data)->has_stratospheric_aod;
}

static int exclude_vmr_zenith(void *user_data)
{
    return !((ingest_info *)user_data)->has_vmr_zenith;
}

static int exclude_tropo_column_zenith(void *user_data)
{
    return !((ingest_info *)user_data)->has_tropo_column_zenith;
}

static int exclude_wind_direction(void *user_data)
{
    return !((ingest_info *)user_data)->has_wind_direction;
}

static int exclude_wind_speed(void *user_data)
{
    return !((ingest_info *)user_data)->has_wind_speed;
}

static void ingestion_done(void *user_data)
{
    free(user_data);
}

static uvvis_doas_type get_template_type_from_string(const char *str)
{
    if (strncmp(str, "DIRECTSUN", 9) == 0)
    {
        return uvvis_doas_directsun;
    }
    if (strncmp(str, "OFFAXIS-AEROSOL", 15) == 0)
    {
        return uvvis_doas_offaxis_aerosol;
    }
    if (strncmp(str, "OFFAXIS", 7) == 0)
    {
        return uvvis_doas_offaxis;
    }
    if (strncmp(str, "ZENITH", 6) == 0)
    {
        return uvvis_doas_zenith;
    }

    assert(0);
    exit(1);
}

static uvvis_doas_gas get_gas_from_string(const char *str)
{
    int i;

    for (i = 0; i < num_uvvis_doas_gas; i++)
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
    uvvis_doas_type expected_type;
    char template_name[40];
    char data_source[30];
    char *gas;
    long length;
    long i;
    int result;

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
    /* template should match the pattern "GEOMS-TE-UVVIS-DOAS-[DIRECTSUN-GAS|OFFAXIS-GAS|OFFAXIS-AEROSOL|ZENITH-GAS]-xxx" */
    if (length == 37)
    {
        expected_type = uvvis_doas_directsun;
    }
    else if (length == 35)
    {
        expected_type = uvvis_doas_offaxis;
    }
    else if (length == 39)
    {
        expected_type = uvvis_doas_offaxis_aerosol;
    }
    else if (length == 34)
    {
        expected_type = uvvis_doas_zenith;
    }
    else
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, "invalid string length for DATA_TEMPLATE global attribute");
        return -1;
    }
    if (coda_cursor_read_string(&cursor, template_name, 40) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (strncmp(template_name, "GEOMS-TE-UVVIS-DOAS-", 20) != 0)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, "invalid GEOMS template name '%s", template_name);
        return -1;
    }

    if (expected_type == uvvis_doas_offaxis_aerosol)
    {
        for (i = 0; i < module->num_product_definitions; i++)
        {
            /* match against product definition name: '<template_name>' */
            if (strcmp(template_name, module->product_definition[i]->name) == 0)
            {
                *definition = module->product_definition[i];
                return 0;
            }
        }
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, "GEOMS template '%s' not supported", template_name);
    }
    else
    {
        if (coda_cursor_goto(&cursor, "/@DATA_SOURCE") != 0)
        {
            harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, "could not find DATA_SOURCE global attribute");
            return -1;
        }
        if (coda_cursor_read_string(&cursor, data_source, 30) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        /* data source should match the pattern "UVVIS_DOAS.[DIRECTSUN|OFFAXIS|ZENITH].<SPECIES>_xxxx" */
        if (strncmp(data_source, "UVVIS.DOAS.", 11) != 0)
        {
            harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, "DATA_SOURCE global attribute has an invalid value");
            return -1;
        }
        switch (expected_type)
        {
            case uvvis_doas_directsun:
                result = strncmp(&data_source[11], "DIRECTSUN.", 10);
                i = 21;
                break;
            case uvvis_doas_offaxis:
                result = strncmp(&data_source[11], "OFFAXIS.", 8);
                i = 19;
                break;
            case uvvis_doas_zenith:
                result = strncmp(&data_source[11], "ZENITH.", 7);
                i = 18;
                break;
            case uvvis_doas_offaxis_aerosol:
                assert(0);
                exit(1);
        }
        if (result != 0)
        {
            harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, "DATA_SOURCE global attribute has an invalid value");
            return -1;
        }
        /* truncate data_source at first '_' occurence */
        gas = &data_source[i];
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

        for (i = 0; i < module->num_product_definitions; i++)
        {
            /* match against product definition name: '<template_name>-<gas>' */
            if (strncmp(template_name, module->product_definition[i]->name, length) == 0 &&
                strcmp(gas, &module->product_definition[i]->name[length + 1]) == 0)
            {
                *definition = module->product_definition[i];
                return 0;
            }
        }
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, "GEOMS template '%s' for gas '%s' not supported", template_name,
                       gas);
    }

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

    info->num_spectral = 0;
    if (info->template_type == uvvis_doas_offaxis_aerosol)
    {
        if (coda_cursor_set_product(&cursor, info->product) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_goto(&cursor, "/WAVELENGTH") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_get_num_elements(&cursor, &info->num_spectral) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (info->num_spectral > 1)
        {
            if (coda_cursor_read_double_partial_array(&cursor, 0, 2, values) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            if (values[1] < values[0])
            {
                harp_set_error(HARP_ERROR_INGESTION, "spectral dimension should use a wavelength ascending ordering");
                return -1;
            }
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

static int get_optional_variable_availability(ingest_info *info)
{
    coda_cursor cursor;
    char path[MAX_PATH_LENGTH];

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    info->has_latitude = (coda_cursor_goto(&cursor, "/LATITUDE") == 0);

    info->has_longitude = (coda_cursor_goto(&cursor, "/LONGITUDE") == 0);

    snprintf(path, MAX_PATH_LENGTH, "%s", info->aod_variant == 0 ? "/AEROSOL_OPTICAL_DEPTH_STRATOSPHERIC_INDEPENDENT" :
             "/AEROSOL_OPTICAL_DEPTH_STRATOSPHERIC_SCATTER_SOLAR_ZENITH");
    info->has_stratospheric_aod = (coda_cursor_goto(&cursor, path) == 0);

    if (info->template_type != uvvis_doas_offaxis_aerosol)
    {
        snprintf(path, MAX_PATH_LENGTH, "/%s_MIXING_RATIO_VOLUME_SCATTER_SOLAR_ZENITH", geoms_gas_name[info->gas]);
        info->has_vmr_zenith = (coda_cursor_goto(&cursor, path) == 0);

        snprintf(path, MAX_PATH_LENGTH, "/%s_COLUMN_TROPOSPHERIC_SCATTER_SOLAR_ZENITH", geoms_gas_name[info->gas]);
        info->has_tropo_column_zenith = (coda_cursor_goto(&cursor, path) == 0);
    }

    info->has_wind_direction = (coda_cursor_goto(&cursor, "/WIND.DIRECTION.SURFACE_INDEPENDENT") == 0);
    info->has_wind_speed = (coda_cursor_goto(&cursor, "/WIND.SPEED.SURFACE_INDEPENDENT") == 0);

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
    if (info->template_type == uvvis_doas_offaxis)
    {
        snprintf(path, MAX_PATH_LENGTH, "/%s_MIXING_RATIO_VOLUME_SCATTER_SOLAR_OFFAXIS", geoms_gas_name[info->gas]);
        if (read_unit(&cursor, path, info->vmr_unit) != 0)
        {
            return -1;
        }
        snprintf(path, MAX_PATH_LENGTH, "/%s_MIXING_RATIO_VOLUME_SCATTER_SOLAR_OFFAXIS_UNCERTAINTY_RANDOM_COVARIANCE",
                 geoms_gas_name[info->gas]);
        if (read_unit(&cursor, path, info->vmr_covariance_unit) != 0)
        {
            return -1;
        }
    }
    else if (info->template_type == uvvis_doas_zenith)
    {
        if (info->has_vmr_zenith)
        {
            snprintf(path, MAX_PATH_LENGTH, "/%s_MIXING_RATIO_VOLUME_SCATTER_SOLAR_ZENITH", geoms_gas_name[info->gas]);
            if (read_unit(&cursor, path, info->vmr_unit) != 0)
            {
                return -1;
            }
            snprintf(path, MAX_PATH_LENGTH,
                     "/%s_MIXING_RATIO_VOLUME_SCATTER_SOLAR_ZENITH_UNCERTAINTY_RANDOM_COVARIANCE",
                     geoms_gas_name[info->gas]);
            if (read_unit(&cursor, path, info->vmr_covariance_unit) != 0)
            {
                return -1;
            }
        }
    }

    return 0;
}

static int ingestion_init(const harp_ingestion_module *module, coda_product *product,
                          const harp_ingestion_options *options, harp_product_definition **definition, void **user_data)
{
    const char *option_value;
    ingest_info *info;

    info = malloc(sizeof(ingest_info));
    if (info == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(ingest_info), __FILE__, __LINE__);
        return -1;
    }

    info->definition = NULL;
    info->product = product;

    if (coda_get_product_version(product, &info->product_version) != 0)
    {
        ingestion_done(info);
        return -1;
    }
    /* the lower 3 digits provide the template version number */
    info->product_version = info->product_version % 1000;

    if (get_product_definition(module, info->product, definition) != 0)
    {
        ingestion_done(info);
        return -1;
    }
    info->definition = *definition;
    info->template_type = get_template_type_from_string(&info->definition->name[20]);
    switch (info->template_type)
    {
        case uvvis_doas_directsun:
            info->gas = get_gas_from_string(&info->definition->name[38]);
            break;
        case uvvis_doas_offaxis:
            info->gas = get_gas_from_string(&info->definition->name[36]);
            break;
        case uvvis_doas_zenith:
            info->gas = get_gas_from_string(&info->definition->name[35]);
            break;
        case uvvis_doas_offaxis_aerosol:
            info->gas = -1;
            break;
    }

    info->swap_alt_bounds = 0;
    if (info->product_version == 4)
    {
        info->swap_alt_bounds = 1;
    }

    info->aod_variant = 0;
    if (harp_ingestion_options_get_option(options, "AOD", &option_value) == 0)
    {
        /* 0:modeled, 1:measured */
        info->aod_variant = (strcmp(option_value, "measured") == 0);
    }

    info->invert_vertical = 0;
    if (get_dimensions(info) != 0)
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

static int init_product_definition(harp_ingestion_module *module, uvvis_doas_gas gas,
                                   uvvis_doas_type template_type, int version)
{
    harp_variable_definition *variable_definition;
    harp_product_definition *product_definition;
    harp_dimension_type dimension_type[4];
    char product_name[MAX_NAME_LENGTH];
    char product_description[MAX_DESCRIPTION_LENGTH];
    char gas_var_name[MAX_NAME_LENGTH];
    char gas_mapping_path[MAX_PATH_LENGTH];
    char gas_description[MAX_DESCRIPTION_LENGTH];
    const char *description;
    long dimension[4];

    switch (template_type)
    {
        case uvvis_doas_directsun:
            snprintf(product_name, MAX_NAME_LENGTH, "GEOMS-TE-UVVIS-DOAS-DIRECTSUN-GAS-%03d-%s", version,
                     geoms_gas_name[gas]);
            snprintf(product_description, MAX_NAME_LENGTH,
                     "GEOMS template for UVVIS-DOAS direct-sun measurements v%03d - %s", version, geoms_gas_name[gas]);
            break;
        case uvvis_doas_offaxis:
            snprintf(product_name, MAX_NAME_LENGTH, "GEOMS-TE-UVVIS-DOAS-OFFAXIS-GAS-%03d-%s", version,
                     geoms_gas_name[gas]);
            snprintf(product_description, MAX_NAME_LENGTH,
                     "GEOMS template for UVVIS-DOAS MAXDOAS measurements v%03d - %s", version, geoms_gas_name[gas]);
            break;
        case uvvis_doas_zenith:
            snprintf(product_name, MAX_NAME_LENGTH, "GEOMS-TE-UVVIS-DOAS-ZENITH-GAS-%03d-%s", version,
                     geoms_gas_name[gas]);
            snprintf(product_description, MAX_NAME_LENGTH,
                     "GEOMS template for UVVIS-DOAS DOAS measurements v%03d - %s", version, geoms_gas_name[gas]);
            break;
        case uvvis_doas_offaxis_aerosol:
            snprintf(product_name, MAX_NAME_LENGTH, "GEOMS-TE-UVVIS-DOAS-OFFAXIS-AEROSOL-%03d", version);
            snprintf(product_description, MAX_NAME_LENGTH,
                     "GEOMS template for UVVIS-DOAS MAXDOAS measurements v%03d - Aerosol", version);
            break;
    }
    product_definition = harp_ingestion_register_product(module, product_name, product_description, read_dimensions);

    dimension_type[0] = harp_dimension_time;
    dimension_type[1] = harp_dimension_vertical;
    dimension_type[2] = harp_dimension_vertical;

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

    /* datetime */
    description = "mean time of the measurement";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double,
                                                                     1, dimension_type, NULL, description,
                                                                     "days since 2000-01-01", NULL, read_datetime);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/DATETIME", NULL);

    /* datetime_start */
    description = "start time of the measurement";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "datetime_start",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, "days since 2000-01-01", NULL,
                                                                     read_datetime_start);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/DATETIME.START", NULL);

    /* datetime_stop */
    description = "stop time of the measurement";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "datetime_stop",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, "days since 2000-01-01", NULL,
                                                                     read_datetime_stop);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/DATETIME.STOP", NULL);

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
    description = "altitude of the sensor relative to the location site";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "sensor_altitude",
                                                                     harp_type_double, 0, NULL, NULL, description, "m",
                                                                     NULL, read_instrument_altitude);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ALTITUDE.INSTRUMENT", NULL);

    if (template_type == uvvis_doas_offaxis_aerosol)
    {
        /* wavelength */
        dimension_type[0] = harp_dimension_spectral;
        description = "wavelength at which aerosol is retrieved";
        variable_definition = harp_ingestion_register_variable_full_read
            (product_definition, "wavelength", harp_type_double, 1, dimension_type, NULL, description, "nm", NULL,
             read_wavelength);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/WAVELENGTH", NULL);
        dimension_type[0] = harp_dimension_time;
    }

    /* altitude */
    description = "effective retrieval altitude";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "altitude", harp_type_double,
                                                                     2, dimension_type, NULL, description, "km",
                                                                     NULL, read_altitude);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ALTITUDE", NULL);

    /* pressure */
    description = "independent pressure profile";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "pressure", harp_type_double,
                                                                     2, dimension_type, NULL, description, "hPa", NULL,
                                                                     read_pressure_ind);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/PRESSURE_INDEPENDENT", NULL);

    /* temperature */
    description = "independent temperature profile";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "temperature",
                                                                     harp_type_double, 2, dimension_type, NULL,
                                                                     description, "K", NULL, read_temperature_ind);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/TEMPERATURE_INDEPENDENT", NULL);

    /* altitude_bounds */
    dimension_type[2] = harp_dimension_independent;
    dimension[0] = -1;
    dimension[1] = -1;
    dimension[2] = 2;
    description = "lower and upper boundaries of the height layers";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "altitude_bounds",
                                                                     harp_type_double, 3, dimension_type, dimension,
                                                                     description, "km", NULL, read_altitude_bounds);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ALTITUDE.BOUNDARIES", NULL);
    dimension_type[2] = harp_dimension_vertical;

    if (template_type != uvvis_doas_directsun && version >= 7)
    {
        /* surface_wind_direction */
        description = "Wind direction at the station using WMO definition (wind from the north is 360; from the east "
            "is 90 and so on. No wind (calm) is 0)";
        variable_definition = harp_ingestion_register_variable_full_read
            (product_definition, "surface_wind_direction", harp_type_double, 1, dimension_type, NULL, description,
             "degree", exclude_wind_direction, read_wind_direction);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/WIND.DIRECTION.SURFACE_INDEPENDENT",
                                             NULL);

        /* surface_wind_speed */
        description = "Wind speed at the station";
        variable_definition = harp_ingestion_register_variable_full_read
            (product_definition, "surface_wind_speed", harp_type_double, 1, dimension_type, NULL, description, "m/s",
             exclude_wind_speed, read_wind_speed);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/WIND.SPEED.SURFACE_INDEPENDENT", NULL);
    }

    /* solar_zenith_angle */
    description = "solar astronomical zenith angle";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "solar_zenith_angle",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, "degree", NULL,
                                                                     read_solar_zenith_angle);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ANGLE.SOLAR_ZENITH.ASTRONOMICAL", NULL);

    /* solar_azimuth_angle */
    description = "solar azimuth angle";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "solar_azimuth_angle",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, "degree", NULL,
                                                                     read_solar_azimuth_angle);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ANGLE.SOLAR_AZIMUTH", NULL);

    /* viewing_azimuth_angle */
    description = "viewing azimuth angle of the sensor";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "viewing_azimuth_angle",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, "degree", NULL,
                                                                     read_viewing_azimuth_angle);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ANGLE.VIEW_AZIMUTH", NULL);

    /* viewing_zenith_angle */
    description = "viewing zenith angle of the sensor";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "viewing_zenith_angle",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, "degree", NULL,
                                                                     read_viewing_zenith_angle);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ANGLE.VIEW_ZENITH", NULL);

    /* latitude */
    description = "latitude of effective air mass at each altitude";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_double,
                                                                     2, dimension_type, NULL, description,
                                                                     "degree_north", exclude_latitude, read_latitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/LATITUDE", NULL);

    /* longitude */
    description = "longitude of effective air mass at each altitude";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_double,
                                                                     2, dimension_type, NULL, description,
                                                                     "degree_east", exclude_longitude, read_longitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0, 180.0);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/LONGITUDE", NULL);

    if (template_type != uvvis_doas_directsun)
    {
        /* cloud_flag */
        description = "one of clear-sky, thin-clouds, thick-clouds, broken-clouds, unavailable";
        variable_definition = harp_ingestion_register_variable_sample_read
            (product_definition, "cloud_flag", harp_type_string, 1, dimension_type, NULL, description, NULL, NULL,
             read_cloud_conditions);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/CLOUD.CONDITIONS", NULL);
    }

    if (template_type != uvvis_doas_offaxis_aerosol)
    {
        /* stratospheric_aerosol_optical_depth */
        description = "total stratospheric aerosol optical depth user for the retrieval ";
        variable_definition = harp_ingestion_register_variable_full_read
            (product_definition, "stratospheric_aerosol_optical_depth", harp_type_double, 1, dimension_type, NULL,
             description, HARP_UNIT_DIMENSIONLESS, exclude_stratospheric_aod, read_stratospheric_aod);
        harp_variable_definition_add_mapping(variable_definition, "AOD=modeled (default)", NULL,
                                             "/AEROSOL.OPTICAL.DEPTH.STRATOSPHERIC_INDEPENDENT", NULL);
        harp_variable_definition_add_mapping(variable_definition, "AOD=measured", NULL,
                                             "/AEROSOL.OPTICAL.DEPTH.STRATOSPHERIC_SCATTER.SOLAR.ZENITH", NULL);
    }

    if (template_type == uvvis_doas_directsun)
    {
        /* <gas>_column_number_density */
        snprintf(gas_var_name, MAX_NAME_LENGTH, "%s_column_number_density", harp_gas_name[gas]);
        snprintf(gas_description, MAX_DESCRIPTION_LENGTH, "%s column number density", harp_gas_name[gas]);
        snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.COLUMN.ABSORPTION.SOLAR", geoms_gas_name[gas]);
        variable_definition = harp_ingestion_register_variable_full_read
            (product_definition, gas_var_name, harp_type_double, 1, dimension_type, NULL, gas_description,
             "Pmolec cm-2", NULL, read_column_solar);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, gas_mapping_path, NULL);

        /* <gas>_column_number_density_uncertainty_random */
        snprintf(gas_var_name, MAX_NAME_LENGTH, "%s_column_number_density_uncertainty_random", harp_gas_name[gas]);
        snprintf(gas_description, MAX_DESCRIPTION_LENGTH, "random uncertainty of the %s column number density",
                 harp_gas_name[gas]);
        snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.COLUMN.ABSORPTION.SOLAR_UNCERTAINTY.RANDOM.STANDARD",
                 geoms_gas_name[gas]);
        variable_definition = harp_ingestion_register_variable_full_read
            (product_definition, gas_var_name, harp_type_double, 1, dimension_type, NULL, gas_description,
             "Pmolec cm-2", NULL, read_column_solar_uncertainty_random);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, gas_mapping_path, NULL);

        /* <gas>_column_number_density_uncertainty_systematic */
        snprintf(gas_var_name, MAX_NAME_LENGTH, "%s_column_number_density_uncertainty_systematic", harp_gas_name[gas]);
        snprintf(gas_description, MAX_DESCRIPTION_LENGTH, "systematic uncertainty of the %s column number density",
                 harp_gas_name[gas]);
        snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.COLUMN.ABSORPTION.SOLAR_UNCERTAINTY.SYSTEMATIC.STANDARD",
                 geoms_gas_name[gas]);
        variable_definition = harp_ingestion_register_variable_full_read
            (product_definition, gas_var_name, harp_type_double, 1, dimension_type, NULL, gas_description,
             "Pmolec cm-2", NULL, read_column_solar_uncertainty_systematic);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, gas_mapping_path, NULL);

        if (version < 5)
        {
            /* <gas>_column_number_density_apriori */
            snprintf(gas_var_name, MAX_NAME_LENGTH, "%s_column_number_density_apriori", harp_gas_name[gas]);
            snprintf(gas_description, MAX_DESCRIPTION_LENGTH, "a priori %s column number density", harp_gas_name[gas]);
            snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.COLUMN_ABSORPTION.SOLAR_APRIORI", geoms_gas_name[gas]);
            variable_definition = harp_ingestion_register_variable_full_read
                (product_definition, gas_var_name, harp_type_double, 1, dimension_type, NULL, gas_description,
                 "Pmolec cm-2", NULL, read_column_solar_apriori);
            harp_variable_definition_add_mapping(variable_definition, NULL, NULL, gas_mapping_path, NULL);
        }
        else
        {
            /* <gas>_column_number_density_apriori */
            snprintf(gas_var_name, MAX_NAME_LENGTH, "%s_column_number_density_apriori", harp_gas_name[gas]);
            snprintf(gas_description, MAX_DESCRIPTION_LENGTH, "a priori %s column number density", harp_gas_name[gas]);
            snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.COLUMN.PARTIAL_ABSORPTION.SOLAR_APRIORI",
                     geoms_gas_name[gas]);
            variable_definition = harp_ingestion_register_variable_full_read
                (product_definition, gas_var_name, harp_type_double, 2, dimension_type, NULL, gas_description,
                 "Pmolec cm-2", NULL, read_partial_column_solar_apriori);
            harp_variable_definition_add_mapping(variable_definition, NULL, NULL, gas_mapping_path, NULL);
        }

        /* <gas>_column_number_density_avk */
        snprintf(gas_var_name, MAX_NAME_LENGTH, "%s_column_number_density_avk", harp_gas_name[gas]);
        snprintf(gas_description, MAX_DESCRIPTION_LENGTH, "averaging kernel for the %s column number density",
                 harp_gas_name[gas]);
        snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.COLUMN_ABSORPTION.SOLAR_AVK", geoms_gas_name[gas]);
        variable_definition = harp_ingestion_register_variable_full_read
            (product_definition, gas_var_name, harp_type_double, 2, dimension_type, NULL, gas_description,
             HARP_UNIT_DIMENSIONLESS, NULL, read_column_solar_avk);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, gas_mapping_path, NULL);
    }
    else if (template_type == uvvis_doas_offaxis)
    {
        /* <gas>_volume_mixing_ratio */
        snprintf(gas_var_name, MAX_NAME_LENGTH, "%s_volume_mixing_ratio", harp_gas_name[gas]);
        snprintf(gas_description, MAX_DESCRIPTION_LENGTH, "%s volume mixing ratio", harp_gas_name[gas]);
        snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.MIXING.RATIO.VOLUME_SCATTER.SOLAR.OFFAXIS",
                 geoms_gas_name[gas]);
        variable_definition = harp_ingestion_register_variable_full_read
            (product_definition, gas_var_name, harp_type_double, 2, dimension_type, NULL, gas_description, "ppmv", NULL,
             read_vmr_offaxis);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, gas_mapping_path, NULL);

        /* <gas>_volume_mixing_ratio_covariance */
        snprintf(gas_var_name, MAX_NAME_LENGTH, "%s_volume_mixing_ratio_covariance", harp_gas_name[gas]);
        snprintf(gas_description, MAX_DESCRIPTION_LENGTH, "covariance of the %s volume mixing ratio",
                 harp_gas_name[gas]);
        snprintf(gas_mapping_path, MAX_PATH_LENGTH,
                 "/%s.MIXING.RATIO.VOLUME_SCATTER.SOLAR.OFFAXIS_UNCERTAINTY.RANDOM.COVARIANCE", geoms_gas_name[gas]);
        variable_definition = harp_ingestion_register_variable_full_read
            (product_definition, gas_var_name, harp_type_double, 3, dimension_type, NULL, gas_description, "(ppmv)2",
             NULL, read_vmr_offaxis_covariance);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, gas_mapping_path, NULL);

        /* <gas>_volume_mixing_ratio_uncertainty_random */
        snprintf(gas_var_name, MAX_NAME_LENGTH, "%s_volume_mixing_ratio_uncertainty_random", harp_gas_name[gas]);
        snprintf(gas_description, MAX_DESCRIPTION_LENGTH, "random uncertainty of the %s volume mixing ratio",
                 harp_gas_name[gas]);
        snprintf(gas_mapping_path, MAX_PATH_LENGTH,
                 "/%s.MIXING.RATIO.VOLUME_SCATTER.SOLAR.OFFAXIS_UNCERTAINTY.RANDOM.COVARIANCE", geoms_gas_name[gas]);
        variable_definition = harp_ingestion_register_variable_full_read
            (product_definition, gas_var_name, harp_type_double, 2, dimension_type, NULL, gas_description, "ppmv", NULL,
             read_vmr_offaxis_uncertainty_random);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, gas_mapping_path,
                                             "the uncertainty is the square root of the trace of the covariance");

        /* <gas>_volume_mixing_ratio_uncertainty_systematic */
        snprintf(gas_var_name, MAX_NAME_LENGTH, "%s_volume_mixing_ratio_uncertainty_systematic", harp_gas_name[gas]);
        snprintf(gas_description, MAX_DESCRIPTION_LENGTH, "systematic uncertainty of the %s volume mixing ratio",
                 harp_gas_name[gas]);
        snprintf(gas_mapping_path, MAX_PATH_LENGTH,
                 "/%s.MIXING.RATIO.VOLUME_SCATTER.SOLAR.OFFAXIS_UNCERTAINTY.SYSTEMATIC.COVARIANCE",
                 geoms_gas_name[gas]);
        variable_definition = harp_ingestion_register_variable_full_read
            (product_definition, gas_var_name, harp_type_double, 2, dimension_type, NULL, gas_description, "ppmv", NULL,
             read_vmr_offaxis_uncertainty_systematic);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, gas_mapping_path,
                                             "the uncertainty is the square root of the trace of the covariance");

        /* <gas>_volume_mixing_ratio_apriori */
        snprintf(gas_var_name, MAX_NAME_LENGTH, "%s_volume_mixing_ratio_apriori", harp_gas_name[gas]);
        snprintf(gas_description, MAX_DESCRIPTION_LENGTH, "a priori %s volume mixing ratio", harp_gas_name[gas]);
        snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.MIXING.RATIO.VOLUME_SCATTER.SOLAR.OFFAXIS_APRIORI",
                 geoms_gas_name[gas]);
        variable_definition = harp_ingestion_register_variable_full_read
            (product_definition, gas_var_name, harp_type_double, 2, dimension_type, NULL, gas_description, "ppmv", NULL,
             read_vmr_offaxis_apriori);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, gas_mapping_path, NULL);

        /* <gas>_volume_mixing_ratio_avk */
        snprintf(gas_var_name, MAX_NAME_LENGTH, "%s_volume_mixing_ratio_avk", harp_gas_name[gas]);
        snprintf(gas_description, MAX_DESCRIPTION_LENGTH, "averaging kernel for the %s volume mixing ratio",
                 harp_gas_name[gas]);
        snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.MIXING.RATIO.VOLUME_SCATTER.SOLAR.OFFAXIS_AVK",
                 geoms_gas_name[gas]);
        variable_definition = harp_ingestion_register_variable_full_read
            (product_definition, gas_var_name, harp_type_double, 3, dimension_type, NULL, gas_description,
             HARP_UNIT_DIMENSIONLESS, NULL, read_vmr_offaxis_avk);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, gas_mapping_path, NULL);

        /* tropospheric_<gas>_column_number_density */
        snprintf(gas_var_name, MAX_NAME_LENGTH, "tropospheric_%s_column_number_density", harp_gas_name[gas]);
        snprintf(gas_description, MAX_DESCRIPTION_LENGTH, "tropospheric %s column number density", harp_gas_name[gas]);
        snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.COLUMN.TROPOSPHERIC_SCATTER.SOLAR.OFFAXIS",
                 geoms_gas_name[gas]);
        variable_definition = harp_ingestion_register_variable_full_read
            (product_definition, gas_var_name, harp_type_double, 1, dimension_type, NULL, gas_description,
             "Pmolec cm-2", NULL, read_tropo_column_offaxis);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, gas_mapping_path, NULL);

        /* tropospheric_<gas>_column_number_density_uncertainty_random */
        snprintf(gas_var_name, MAX_NAME_LENGTH, "tropospheric_%s_column_number_density_uncertainty_random",
                 harp_gas_name[gas]);
        snprintf(gas_description, MAX_DESCRIPTION_LENGTH,
                 "random uncertainty of the tropospheric %s column number density", harp_gas_name[gas]);
        snprintf(gas_mapping_path, MAX_PATH_LENGTH,
                 "/%s.COLUMN.TROPOSPHERIC_SCATTER.SOLAR.OFFAXIS_UNCERTAINTY.RANDOM.STANDARD", geoms_gas_name[gas]);
        variable_definition = harp_ingestion_register_variable_full_read
            (product_definition, gas_var_name, harp_type_double, 1, dimension_type, NULL, gas_description,
             "Pmolec cm-2", NULL, read_tropo_column_offaxis_uncertainty_random);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, gas_mapping_path, NULL);

        /* tropospheric_<gas>_column_number_density_uncertainty_systematic */
        snprintf(gas_var_name, MAX_NAME_LENGTH, "tropospheric_%s_column_number_density_uncertainty_systematic",
                 harp_gas_name[gas]);
        snprintf(gas_description, MAX_DESCRIPTION_LENGTH,
                 "systematic uncertainty of the tropospheric %s column number density", harp_gas_name[gas]);
        snprintf(gas_mapping_path, MAX_PATH_LENGTH,
                 "/%s.COLUMN.TROPOSPHERIC_SCATTER.SOLAR.OFFAXIS_UNCERTAINTY.SYSTEMATIC.STANDARD", geoms_gas_name[gas]);
        variable_definition = harp_ingestion_register_variable_full_read
            (product_definition, gas_var_name, harp_type_double, 1, dimension_type, NULL, gas_description,
             "Pmolec cm-2", NULL, read_tropo_column_offaxis_uncertainty_systematic);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, gas_mapping_path, NULL);

        /* tropospheric_<gas>_column_number_density_apriori */
        snprintf(gas_var_name, MAX_NAME_LENGTH, "tropospheric_%s_column_number_density_apriori", harp_gas_name[gas]);
        snprintf(gas_description, MAX_DESCRIPTION_LENGTH, "a priori tropospheric %s column number density",
                 harp_gas_name[gas]);
        snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.COLUMN.TROPOSPHERIC_SCATTER.SOLAR.OFFAXIS_APRIORI",
                 geoms_gas_name[gas]);
        variable_definition = harp_ingestion_register_variable_full_read
            (product_definition, gas_var_name, harp_type_double, 1, dimension_type, NULL, gas_description,
             "Pmolec cm-2", NULL, read_tropo_column_offaxis_apriori);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, gas_mapping_path, NULL);

        /* tropospheric_<gas>_column_number_density_avk */
        snprintf(gas_var_name, MAX_NAME_LENGTH, "tropospheric_%s_column_number_density_avk", harp_gas_name[gas]);
        snprintf(gas_description, MAX_DESCRIPTION_LENGTH,
                 "averaging kernel for the tropospheric %s column number density", harp_gas_name[gas]);
        snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.COLUMN.TROPOSPHERIC_SCATTER.SOLAR.OFFAXIS_AVK",
                 geoms_gas_name[gas]);
        variable_definition = harp_ingestion_register_variable_full_read
            (product_definition, gas_var_name, harp_type_double, 2, dimension_type, NULL, gas_description,
             HARP_UNIT_DIMENSIONLESS, NULL, read_tropo_column_offaxis_avk);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, gas_mapping_path, NULL);


        /* <gas>_column_number_density */
        snprintf(gas_var_name, MAX_NAME_LENGTH, "%s_column_number_density", harp_gas_name[gas]);
        snprintf(gas_description, MAX_DESCRIPTION_LENGTH, "%s partial column number density profile",
                 harp_gas_name[gas]);
        snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.COLUMN.PARTIAL_SCATTER.SOLAR.OFFAXIS", geoms_gas_name[gas]);
        variable_definition = harp_ingestion_register_variable_full_read
            (product_definition, gas_var_name, harp_type_double, 2, dimension_type, NULL, gas_description,
             "Pmolec cm-2", NULL, read_partial_column_offaxis);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, gas_mapping_path, NULL);

        /* <gas>_column_number_density_apriori */
        snprintf(gas_var_name, MAX_NAME_LENGTH, "%s_column_number_density_apriori", harp_gas_name[gas]);
        snprintf(gas_description, MAX_DESCRIPTION_LENGTH, "a priori %s partial column number density profile",
                 harp_gas_name[gas]);
        snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.COLUMN.PARTIAL_SCATTER.SOLAR.OFFAXIS_APRIORI",
                 geoms_gas_name[gas]);
        variable_definition = harp_ingestion_register_variable_full_read
            (product_definition, gas_var_name, harp_type_double, 2, dimension_type, NULL, gas_description,
             "Pmolec cm-2", NULL, read_partial_column_offaxis_apriori);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, gas_mapping_path, NULL);
    }
    else if (template_type == uvvis_doas_zenith)
    {
        /* <gas>_volume_mixing_ratio */
        snprintf(gas_var_name, MAX_NAME_LENGTH, "%s_volume_mixing_ratio", harp_gas_name[gas]);
        snprintf(gas_description, MAX_DESCRIPTION_LENGTH, "%s volume mixing ratio", harp_gas_name[gas]);
        snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.MIXING.RATIO.VOLUME_SCATTER.SOLAR.ZENITH",
                 geoms_gas_name[gas]);
        variable_definition = harp_ingestion_register_variable_full_read
            (product_definition, gas_var_name, harp_type_double, 2, dimension_type, NULL, gas_description, "ppmv",
             exclude_vmr_zenith, read_vmr_zenith);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, gas_mapping_path, NULL);

        /* <gas>_volume_mixing_ratio_covariance */
        snprintf(gas_var_name, MAX_NAME_LENGTH, "%s_volume_mixing_ratio_covariance", harp_gas_name[gas]);
        snprintf(gas_description, MAX_DESCRIPTION_LENGTH, "covariance of the %s volume mixing ratio",
                 harp_gas_name[gas]);
        snprintf(gas_mapping_path, MAX_PATH_LENGTH,
                 "/%s.MIXING.RATIO.VOLUME_SCATTER.SOLAR.ZENITH_UNCERTAINTY.RANDOM.COVARIANCE", geoms_gas_name[gas]);
        variable_definition = harp_ingestion_register_variable_full_read
            (product_definition, gas_var_name, harp_type_double, 3, dimension_type, NULL, gas_description, "(ppmv)2",
             exclude_vmr_zenith, read_vmr_zenith_covariance);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, gas_mapping_path, NULL);

        /* <gas>_volume_mixing_ratio_uncertainty_random */
        snprintf(gas_var_name, MAX_NAME_LENGTH, "%s_volume_mixing_ratio_uncertainty_random", harp_gas_name[gas]);
        snprintf(gas_description, MAX_DESCRIPTION_LENGTH, "random uncertainty of the %s volume mixing ratio",
                 harp_gas_name[gas]);
        snprintf(gas_mapping_path, MAX_PATH_LENGTH,
                 "/%s.MIXING.RATIO.VOLUME_SCATTER.SOLAR.ZENITH_UNCERTAINTY.RANDOM.COVARIANCE", geoms_gas_name[gas]);
        variable_definition = harp_ingestion_register_variable_full_read
            (product_definition, gas_var_name, harp_type_double, 2, dimension_type, NULL, gas_description, "ppmv",
             exclude_vmr_zenith, read_vmr_zenith_uncertainty_random);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, gas_mapping_path,
                                             "the uncertainty is the square root of the trace of the covariance");

        /* <gas>_volume_mixing_ratio_uncertainty_systematic */
        snprintf(gas_var_name, MAX_NAME_LENGTH, "%s_volume_mixing_ratio_uncertainty_systematic", harp_gas_name[gas]);
        snprintf(gas_description, MAX_DESCRIPTION_LENGTH, "systematic uncertainty of the %s volume mixing ratio",
                 harp_gas_name[gas]);
        snprintf(gas_mapping_path, MAX_PATH_LENGTH,
                 "/%s.MIXING.RATIO.VOLUME_SCATTER.SOLAR.ZENITH_UNCERTAINTY.SYSTEMATIC.COVARIANCE", geoms_gas_name[gas]);
        variable_definition = harp_ingestion_register_variable_full_read
            (product_definition, gas_var_name, harp_type_double, 2, dimension_type, NULL, gas_description, "ppmv",
             exclude_vmr_zenith, read_vmr_zenith_uncertainty_systematic);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, gas_mapping_path,
                                             "the uncertainty is the square root of the trace of the covariance");

        /* <gas>_volume_mixing_ratio_apriori */
        snprintf(gas_var_name, MAX_NAME_LENGTH, "%s_volume_mixing_ratio_apriori", harp_gas_name[gas]);
        snprintf(gas_description, MAX_DESCRIPTION_LENGTH, "a priori %s volume mixing ratio", harp_gas_name[gas]);
        snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.MIXING.RATIO.VOLUME_SCATTER.SOLAR.ZENITH_APRIORI",
                 geoms_gas_name[gas]);
        variable_definition = harp_ingestion_register_variable_full_read
            (product_definition, gas_var_name, harp_type_double, 2, dimension_type, NULL, gas_description, "ppmv",
             version >= 7 ? NULL : exclude_vmr_zenith, read_vmr_zenith_apriori);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, gas_mapping_path, NULL);

        /* <gas>_volume_mixing_ratio_avk */
        snprintf(gas_var_name, MAX_NAME_LENGTH, "%s_volume_mixing_ratio_avk", harp_gas_name[gas]);
        snprintf(gas_description, MAX_DESCRIPTION_LENGTH, "averaging kernel for the %s volume mixing ratio",
                 harp_gas_name[gas]);
        snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.MIXING.RATIO.VOLUME_SCATTER.SOLAR.ZENITH_AVK",
                 geoms_gas_name[gas]);
        variable_definition = harp_ingestion_register_variable_full_read
            (product_definition, gas_var_name, harp_type_double, 3, dimension_type, NULL, gas_description,
             HARP_UNIT_DIMENSIONLESS, exclude_vmr_zenith, read_vmr_zenith_avk);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, gas_mapping_path, NULL);

        /* tropospheric_<gas>_column_number_density */
        snprintf(gas_var_name, MAX_NAME_LENGTH, "tropospheric_%s_column_number_density", harp_gas_name[gas]);
        snprintf(gas_description, MAX_DESCRIPTION_LENGTH, "tropospheric %s column number density", harp_gas_name[gas]);
        snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.COLUMN.TROPOSPHERIC_SCATTER.SOLAR.ZENITH",
                 geoms_gas_name[gas]);
        variable_definition = harp_ingestion_register_variable_full_read
            (product_definition, gas_var_name, harp_type_double, 1, dimension_type, NULL, gas_description,
             "Pmolec cm-2", exclude_tropo_column_zenith, read_tropo_column_zenith);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, gas_mapping_path, NULL);

        /* tropospheric_<gas>_column_number_density_uncertainty_random */
        snprintf(gas_var_name, MAX_NAME_LENGTH, "tropospheric_%s_column_number_density_uncertainty_random",
                 harp_gas_name[gas]);
        snprintf(gas_description, MAX_DESCRIPTION_LENGTH,
                 "random uncertainty of the tropospheric %s column number density", harp_gas_name[gas]);
        snprintf(gas_mapping_path, MAX_PATH_LENGTH,
                 "/%s.COLUMN.TROPOSPHERIC_SCATTER.SOLAR.ZENITH_UNCERTAINTY.RANDOM.STANDARD", geoms_gas_name[gas]);
        variable_definition = harp_ingestion_register_variable_full_read
            (product_definition, gas_var_name, harp_type_double, 1, dimension_type, NULL, gas_description,
             "Pmolec cm-2", exclude_tropo_column_zenith, read_tropo_column_zenith_uncertainty_random);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, gas_mapping_path, NULL);

        /* tropospheric_<gas>_column_number_density_uncertainty_systematic */
        snprintf(gas_var_name, MAX_NAME_LENGTH, "tropospheric_%s_column_number_density_uncertainty_systematic",
                 harp_gas_name[gas]);
        snprintf(gas_description, MAX_DESCRIPTION_LENGTH,
                 "systematic uncertainty of the tropospheric %s column number density", harp_gas_name[gas]);
        snprintf(gas_mapping_path, MAX_PATH_LENGTH,
                 "/%s.COLUMN.TROPOSPHERIC_SCATTER.SOLAR.ZENITH_UNCERTAINTY.SYSTEMATIC.STANDARD", geoms_gas_name[gas]);
        variable_definition = harp_ingestion_register_variable_full_read
            (product_definition, gas_var_name, harp_type_double, 1, dimension_type, NULL, gas_description,
             "Pmolec cm-2", exclude_tropo_column_zenith, read_tropo_column_zenith_uncertainty_systematic);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, gas_mapping_path, NULL);

        /* tropospheric_<gas>_column_number_density_apriori */
        snprintf(gas_var_name, MAX_NAME_LENGTH, "tropospheric_%s_column_number_density_apriori", harp_gas_name[gas]);
        snprintf(gas_description, MAX_DESCRIPTION_LENGTH, "a priori tropospheric %s column number density",
                 harp_gas_name[gas]);
        snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.COLUMN.TROPOSPHERIC_SCATTER.SOLAR.ZENITH_APRIORI",
                 geoms_gas_name[gas]);
        variable_definition = harp_ingestion_register_variable_full_read
            (product_definition, gas_var_name, harp_type_double, 1, dimension_type, NULL, gas_description,
             "Pmolec cm-2", exclude_tropo_column_zenith, read_tropo_column_zenith_apriori);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, gas_mapping_path, NULL);

        /* tropospheric_<gas>_column_number_density_avk */
        snprintf(gas_var_name, MAX_NAME_LENGTH, "tropospheric_%s_column_number_density_avk", harp_gas_name[gas]);
        snprintf(gas_description, MAX_DESCRIPTION_LENGTH,
                 "averaging kernel for the tropospheric %s column number density", harp_gas_name[gas]);
        snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.COLUMN.TROPOSPHERIC_SCATTER.SOLAR.ZENITH_AVK",
                 geoms_gas_name[gas]);
        variable_definition = harp_ingestion_register_variable_full_read
            (product_definition, gas_var_name, harp_type_double, 2, dimension_type, NULL, gas_description,
             HARP_UNIT_DIMENSIONLESS, exclude_tropo_column_zenith, read_tropo_column_zenith_avk);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, gas_mapping_path, NULL);

        /* stratospheric_<gas>_column_number_density */
        snprintf(gas_var_name, MAX_NAME_LENGTH, "stratospheric_%s_column_number_density", harp_gas_name[gas]);
        snprintf(gas_description, MAX_DESCRIPTION_LENGTH, "stratospheric %s column number density", harp_gas_name[gas]);
        snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.COLUMN.STRATOSPHERIC_SCATTER.SOLAR.ZENITH",
                 geoms_gas_name[gas]);
        variable_definition = harp_ingestion_register_variable_full_read
            (product_definition, gas_var_name, harp_type_double, 1, dimension_type, NULL, gas_description,
             "Pmolec cm-2", NULL, read_strat_column_zenith);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, gas_mapping_path, NULL);

        /* stratospheric_<gas>_column_number_density_uncertainty_random */
        snprintf(gas_var_name, MAX_NAME_LENGTH, "stratospheric_%s_column_number_density_uncertainty_random",
                 harp_gas_name[gas]);
        snprintf(gas_description, MAX_DESCRIPTION_LENGTH,
                 "random uncertainty of the stratospheric %s column number density", harp_gas_name[gas]);
        snprintf(gas_mapping_path, MAX_PATH_LENGTH,
                 "/%s.COLUMN.STRATOSPHERIC_SCATTER.SOLAR.ZENITH_UNCERTAINTY.RANDOM.STANDARD", geoms_gas_name[gas]);
        variable_definition = harp_ingestion_register_variable_full_read
            (product_definition, gas_var_name, harp_type_double, 1, dimension_type, NULL, gas_description,
             "Pmolec cm-2", NULL, read_strat_column_zenith_uncertainty_random);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, gas_mapping_path, NULL);

        /* stratospheric_<gas>_column_number_density_uncertainty_systematic */
        snprintf(gas_var_name, MAX_NAME_LENGTH, "stratospheric_%s_column_number_density_uncertainty_systematic",
                 harp_gas_name[gas]);
        snprintf(gas_description, MAX_DESCRIPTION_LENGTH,
                 "systematic uncertainty of the stratospheric %s column number density", harp_gas_name[gas]);
        snprintf(gas_mapping_path, MAX_PATH_LENGTH,
                 "/%s.COLUMN.STRATOSPHERIC_SCATTER.SOLAR.ZENITH_UNCERTAINTY.SYSTEMATIC.STANDARD", geoms_gas_name[gas]);
        variable_definition = harp_ingestion_register_variable_full_read
            (product_definition, gas_var_name, harp_type_double, 1, dimension_type, NULL, gas_description,
             "Pmolec cm-2", NULL, read_strat_column_zenith_uncertainty_systematic);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, gas_mapping_path, NULL);

        /* stratospheric_<gas>_column_number_density_apriori */
        snprintf(gas_var_name, MAX_NAME_LENGTH, "stratospheric_%s_column_number_density_apriori", harp_gas_name[gas]);
        snprintf(gas_description, MAX_DESCRIPTION_LENGTH, "a priori stratospheric %s column number density",
                 harp_gas_name[gas]);
        snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.COLUMN.STRATOSPHERIC_SCATTER.SOLAR.ZENITH_APRIORI",
                 geoms_gas_name[gas]);
        variable_definition = harp_ingestion_register_variable_full_read
            (product_definition, gas_var_name, harp_type_double, 1, dimension_type, NULL, gas_description,
             "Pmolec cm-2", NULL, read_strat_column_zenith_apriori);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, gas_mapping_path, NULL);

        /* stratospheric_<gas>_column_number_density_avk */
        snprintf(gas_var_name, MAX_NAME_LENGTH, "stratospheric_%s_column_number_density_avk", harp_gas_name[gas]);
        snprintf(gas_description, MAX_DESCRIPTION_LENGTH,
                 "averaging kernel for the stratospheric %s column number density", harp_gas_name[gas]);
        snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.COLUMN.STRATOSPHERIC_SCATTER.SOLAR.ZENITH_AVK",
                 geoms_gas_name[gas]);
        variable_definition = harp_ingestion_register_variable_full_read
            (product_definition, gas_var_name, harp_type_double, 2, dimension_type, NULL, gas_description,
             HARP_UNIT_DIMENSIONLESS, NULL, read_strat_column_zenith_avk);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, gas_mapping_path, NULL);

        /* stratospheric_<gas>_column_number_density_amf */
        snprintf(gas_var_name, MAX_NAME_LENGTH, "stratospheric_%s_column_number_density_amf", harp_gas_name[gas]);
        snprintf(gas_description, MAX_DESCRIPTION_LENGTH,
                 "air mass factor for the stratospheric %s column number density", harp_gas_name[gas]);
        snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.COLUMN.STRATOSPHERIC_SCATTER.SOLAR.ZENITH_AMF",
                 geoms_gas_name[gas]);
        variable_definition = harp_ingestion_register_variable_full_read
            (product_definition, gas_var_name, harp_type_double, 1, dimension_type, NULL, gas_description,
             HARP_UNIT_DIMENSIONLESS, NULL, read_strat_column_zenith_amf);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, gas_mapping_path, NULL);

        /* <gas>_column_number_density */
        snprintf(gas_var_name, MAX_NAME_LENGTH, "%s_column_number_density", harp_gas_name[gas]);
        snprintf(gas_description, MAX_DESCRIPTION_LENGTH, "%s partial column number density profile",
                 harp_gas_name[gas]);
        snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.COLUMN.PARTIAL_SCATTER.SOLAR.ZENITH", geoms_gas_name[gas]);
        variable_definition = harp_ingestion_register_variable_full_read
            (product_definition, gas_var_name, harp_type_double, 2, dimension_type, NULL, gas_description,
             "Pmolec cm-2", exclude_vmr_zenith, read_partial_column_zenith);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, gas_mapping_path, NULL);

        /* <gas>_column_number_density_apriori */
        snprintf(gas_var_name, MAX_NAME_LENGTH, "%s_column_number_density_apriori", harp_gas_name[gas]);
        snprintf(gas_description, MAX_DESCRIPTION_LENGTH, "a priori %s partial column number density profile",
                 harp_gas_name[gas]);
        snprintf(gas_mapping_path, MAX_PATH_LENGTH, "/%s.COLUMN.PARTIAL_SCATTER.SOLAR.ZENITH_APRIORI",
                 geoms_gas_name[gas]);
        variable_definition = harp_ingestion_register_variable_full_read
            (product_definition, gas_var_name, harp_type_double, 2, dimension_type, NULL, gas_description,
             "Pmolec cm-2", version >= 7 ? NULL : exclude_vmr_zenith, read_partial_column_zenith_apriori);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, gas_mapping_path, NULL);
    }
    else if (template_type == uvvis_doas_offaxis_aerosol)
    {
        const char *mapping_path;

        dimension_type[0] = harp_dimension_time;
        dimension_type[1] = harp_dimension_spectral;
        dimension_type[2] = harp_dimension_vertical;
        dimension_type[3] = harp_dimension_vertical;

        /* aerosol_extinction_coefficient */
        variable_definition = harp_ingestion_register_variable_full_read
            (product_definition, "aerosol_extinction_coefficient", harp_type_double, 3, dimension_type, NULL,
             "aerosol extinction coefficient", "km^-1", NULL, read_aerosol_extinction_coefficient);
        mapping_path = "/AEROSOL.EXTINCTION.COEFFICIENT_SCATTER.SOLAR.OFFAXIS";
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, mapping_path, NULL);

        /* aerosol_extinction_coefficient_covariance */
        variable_definition = harp_ingestion_register_variable_full_read
            (product_definition, "aerosol_extinction_coefficient_covariance", harp_type_double, 4, dimension_type, NULL,
             "covariance of the aerosol extinction coefficient", "km^-2", NULL,
             read_aerosol_extinction_coefficient_covariance);
        mapping_path = "/AEROSOL.EXTINCTION.COEFFICIENT_SCATTER.SOLAR.OFFAXIS_UNCERTAINTY.RANDOM.COVARIANCE";
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, mapping_path, NULL);

        /* aerosol_extinction_coefficient_uncertainty_random */
        variable_definition = harp_ingestion_register_variable_full_read
            (product_definition, "aerosol_extinction_coefficient_uncertainty_random", harp_type_double, 3,
             dimension_type, NULL, "random uncertainty of the aerosol extinction coefficient", "km^-1", NULL,
             read_aerosol_extinction_coefficient_uncertainty_random);
        mapping_path = "/AEROSOL.EXTINCTION.COEFFICIENT_SCATTER.SOLAR.OFFAXIS_UNCERTAINTY.RANDOM.COVARIANCE";
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, mapping_path,
                                             "the uncertainty is the square root of the trace of the covariance");

        /* aerosol_extinction_coefficient_uncertainty_systematic */
        variable_definition = harp_ingestion_register_variable_full_read
            (product_definition, "aerosol_extinction_coefficient_uncertainty_systematic", harp_type_double, 3,
             dimension_type, NULL, "systematic uncertainty of the aerosol extinction coefficient", "km^-1", NULL,
             read_aerosol_extinction_coefficient_uncertainty_systematic);
        mapping_path = "/AEROSOL.EXTINCTION.COEFFICIENT_SCATTER.SOLAR.OFFAXIS_UNCERTAINTY.SYSTEMATIC.COVARIANCE";
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, mapping_path,
                                             "the uncertainty is the square root of the trace of the covariance");

        /* aerosol_extinction_coefficient_apriori */
        variable_definition = harp_ingestion_register_variable_full_read
            (product_definition, "aerosol_extinction_coefficient_apriori", harp_type_double, 3, dimension_type, NULL,
             "a priori aerosol extinction coefficient", "km^-1", NULL, read_aerosol_extinction_coefficient_apriori);
        mapping_path = "/AEROSOL.EXTINCTION.COEFFICIENT_SCATTER.SOLAR.OFFAXIS_APRIORI";
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, mapping_path, NULL);

        /* aerosol_extinction_coefficient_avk */
        variable_definition = harp_ingestion_register_variable_full_read
            (product_definition, "aerosol_extinction_coefficient_avk", harp_type_double, 4, dimension_type,
             NULL, "averaging kernel of the aerosol extinction coefficient", HARP_UNIT_DIMENSIONLESS, NULL,
             read_aerosol_extinction_coefficient_avk);
        mapping_path = "/AEROSOL.EXTINCTION.COEFFICIENT_SCATTER.SOLAR.OFFAXIS_AVK";
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, mapping_path, NULL);

        /* tropospheric_aerosol_optical_depth */
        variable_definition = harp_ingestion_register_variable_full_read
            (product_definition, "tropospheric_aerosol_optical_depth", harp_type_double, 2, dimension_type, NULL,
             "tropospheric aerosol optical depth", HARP_UNIT_DIMENSIONLESS, NULL, read_tropo_aerosol_optical_depth);
        mapping_path = "/AEROSOL.OPTICAL.DEPTH.TROPOSPHERIC_SCATTER.SOLAR.OFFAXIS";
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, mapping_path, NULL);

        /* tropospheric_aerosol_optical_depth_uncertainty_random */
        variable_definition = harp_ingestion_register_variable_full_read
            (product_definition, "tropospheric_aerosol_optical_depth_uncertainty_random", harp_type_double, 2,
             dimension_type, NULL, "random uncertainty of the tropospheric aerosol optical depth",
             HARP_UNIT_DIMENSIONLESS, NULL, read_tropo_aerosol_optical_depth_uncertainty_random);
        mapping_path = "/AEROSOL.OPTICAL.DEPTH.TROPOSPHERIC_SCATTER.SOLAR.OFFAXIS_UNCERTAINTY.RANDOM.STANDARD";
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, mapping_path, NULL);

        /* tropospheric_aerosol_optical_depth_uncertainty_systematic */
        variable_definition = harp_ingestion_register_variable_full_read
            (product_definition, "tropospheric_aerosol_optical_depth_uncertainty_systematic", harp_type_double, 2,
             dimension_type, NULL, "systematic uncertainty of the tropospheric aerosol optical depth",
             HARP_UNIT_DIMENSIONLESS, NULL, read_tropo_aerosol_optical_depth_uncertainty_systematic);
        mapping_path = "/AEROSOL.OPTICAL.DEPTH.TROPOSPHERIC_SCATTER.SOLAR.OFFAXIS_UNCERTAINTY.SYSTEMATIC.STANDARD";
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, mapping_path, NULL);

        /* tropospheric_aerosol_optical_depth_apriori */
        variable_definition = harp_ingestion_register_variable_full_read
            (product_definition, "tropospheric_aerosol_optical_depth_apriori", harp_type_double, 2, dimension_type,
             NULL, "a priori tropospheric aerosol optical depth", HARP_UNIT_DIMENSIONLESS, NULL,
             read_tropo_aerosol_optical_depth_apriori);
        mapping_path = "/AEROSOL.OPTICAL.DEPTH.TROPOSPHERIC_SCATTER.SOLAR.OFFAXIS_APRIORI";
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, mapping_path, NULL);

        /* tropospheric_aerosol_optical_depth_avk */
        variable_definition = harp_ingestion_register_variable_full_read
            (product_definition, "tropospheric_aerosol_optical_depth_avk", harp_type_double, 3, dimension_type,
             NULL, "averaging kernel of the tropospheric aerosol optical depth", HARP_UNIT_DIMENSIONLESS, NULL,
             read_tropo_aerosol_optical_depth_avk);
        mapping_path = "/AEROSOL.OPTICAL.DEPTH.TROPOSPHERIC_SCATTER.SOLAR.OFFAXIS_AVK";
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, mapping_path, NULL);
    }

    return 0;
}

int harp_ingestion_module_geoms_uvvis_doas_init()
{
    const char *aod_option_values[] = { "modeled", "measured" };
    harp_ingestion_module *module;
    int i;

    module = harp_ingestion_register_module_coda("GEOMS-TE-UVVIS-DOAS-DIRECTSUN", "GEOMS", "GEOMS",
                                                 "UVVIS_DOAS_DIRECTSUN_GAS",
                                                 "GEOMS template for UVVIS-DOAS direct sun measurements",
                                                 ingestion_init, ingestion_done);

    harp_ingestion_register_option(module, "AOD", "ingest the modeled or measured aerosol optical depth properties", 2,
                                   aod_option_values);

    for (i = 0; i < num_uvvis_doas_gas; i++)
    {
        init_product_definition(module, i, uvvis_doas_directsun, 4);
        init_product_definition(module, i, uvvis_doas_directsun, 5);
        init_product_definition(module, i, uvvis_doas_directsun, 6);
        init_product_definition(module, i, uvvis_doas_directsun, 7);
    }

    module = harp_ingestion_register_module_coda("GEOMS-TE-UVVIS-DOAS-OFFAXIS", "GEOMS", "GEOMS",
                                                 "UVVIS_DOAS_OFFAXIS_GAS",
                                                 "GEOMS template for UVVIS-DOAS off-axis gas measurements",
                                                 ingestion_init, ingestion_done);

    for (i = 0; i < num_uvvis_doas_gas; i++)
    {
        init_product_definition(module, i, uvvis_doas_offaxis, 4);
        init_product_definition(module, i, uvvis_doas_offaxis, 6);
        init_product_definition(module, i, uvvis_doas_offaxis, 7);
    }

    module = harp_ingestion_register_module_coda("GEOMS-TE-UVVIS-DOAS-OFFAXIS-AEROSOL", "GEOMS", "GEOMS",
                                                 "UVVIS_DOAS_OFFAXIS_AEROSOL",
                                                 "GEOMS template for UVVIS-DOAS off-axis aerosol measurements",
                                                 ingestion_init, ingestion_done);

    harp_ingestion_register_option(module, "AOD", "ingest the modeled or measured aerosol optical depth properties", 2,
                                   aod_option_values);

    init_product_definition(module, -1, uvvis_doas_offaxis_aerosol, 4);
    init_product_definition(module, -1, uvvis_doas_offaxis_aerosol, 6);

    module = harp_ingestion_register_module_coda("GEOMS-TE-UVVIS-DOAS-ZENITH", "GEOMS", "GEOMS",
                                                 "UVVIS_DOAS_ZENITH_GAS",
                                                 "GEOMS template for UVVIS-DOAS zenith measurements", ingestion_init,
                                                 ingestion_done);

    harp_ingestion_register_option(module, "AOD", "ingest the modeled or measured aerosol optical depth properties", 2,
                                   aod_option_values);

    for (i = 0; i < num_uvvis_doas_gas; i++)
    {
        init_product_definition(module, i, uvvis_doas_zenith, 4);
        init_product_definition(module, i, uvvis_doas_zenith, 6);
        init_product_definition(module, i, uvvis_doas_zenith, 7);
    }

    return 0;
}
