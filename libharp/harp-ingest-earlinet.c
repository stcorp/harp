/*
 * Copyright (C) 2015-2023 S[&]T, The Netherlands.
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
#include <stdlib.h>
#include <string.h>

typedef struct ingest_info_struct
{
    coda_product *product;
    long num_times;
    long num_altitudes;
    long num_wavelengths;
    int has_backscatter;
    int has_extinction;
} ingest_info;

static void ingestion_done(void *user_data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info != NULL)
    {
        free(info);
    }
}

static int read_array_variable(ingest_info *info, const char *name, harp_data_type data_type, long num_elements,
                               harp_array data)
{
    harp_scalar fill_value;
    coda_cursor cursor;
    long actual_num_elements;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&cursor, name) != 0)
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
        harp_set_error(HARP_ERROR_INGESTION, "variable %s has %ld elements (expected %ld)", name, actual_num_elements,
                       num_elements);
        return -1;
    }
    switch (data_type)
    {
        case harp_type_float:
            if (coda_cursor_read_float_array(&cursor, data.float_data, coda_array_ordering_c) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            if (coda_cursor_goto(&cursor, "@FillValue[0]") == 0)
            {
                if (coda_cursor_read_float(&cursor, &fill_value.float_data) != 0)
                {
                    harp_set_error(HARP_ERROR_CODA, NULL);
                    return -1;
                }
                /* Replace values equal to the _FillValue variable attribute by NaN. */
                harp_array_replace_fill_value(data_type, num_elements, data, fill_value);
            }
            break;
        case harp_type_double:
            if (coda_cursor_read_double_array(&cursor, data.double_data, coda_array_ordering_c) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            if (coda_cursor_goto(&cursor, "@FillValue[0]") == 0)
            {
                if (coda_cursor_read_double(&cursor, &fill_value.double_data) != 0)
                {
                    harp_set_error(HARP_ERROR_CODA, NULL);
                    return -1;
                }
                /* Replace values equal to the _FillValue variable attribute by NaN. */
                harp_array_replace_fill_value(data_type, num_elements, data, fill_value);
            }
            break;
        default:
            assert(0);
            exit(1);
    }

    return 0;
}

/* Specific read functions */

static int read_latitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array_variable(info, "latitude", harp_type_float, 1, data);
}

static int read_longitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array_variable(info, "longitude", harp_type_float, 1, data);
}

static int read_time_start(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    harp_array time_bounds;
    long i;

    time_bounds.ptr = malloc(info->num_times * 2 * sizeof(double));
    if (time_bounds.ptr == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       info->num_times * 2 * sizeof(double), __FILE__, __LINE__);
        return -1;
    }

    if (read_array_variable(info, "time_bounds", harp_type_double, info->num_times * 2, time_bounds) != 0)
    {
        free(time_bounds.ptr);
        return -1;
    }

    for (i = 0; i < info->num_times; i++)
    {
        data.double_data[i] = time_bounds.double_data[i * 2];
    }

    free(time_bounds.ptr);

    return 0;
}

static int read_time_stop(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    harp_array time_bounds;
    long i;

    time_bounds.ptr = malloc(info->num_times * 2 * sizeof(double));
    if (time_bounds.ptr == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       info->num_times * 2 * sizeof(double), __FILE__, __LINE__);
        return -1;
    }

    if (read_array_variable(info, "time_bounds", harp_type_double, info->num_times * 2, time_bounds) != 0)
    {
        free(time_bounds.ptr);
        return -1;
    }

    for (i = 0; i < info->num_times; i++)
    {
        data.double_data[i] = time_bounds.double_data[i * 2 + 1];
    }

    free(time_bounds.ptr);

    return 0;
}

static int read_station_altitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array_variable(info, "station_altitude", harp_type_float, 1, data);
}

static int read_zenith_angle(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array_variable(info, "zenith_angle", harp_type_float, 1, data);
}

static int read_wavelength(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array_variable(info, "wavelength", harp_type_float, info->num_wavelengths, data);
}

static int read_altitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array_variable(info, "altitude", harp_type_double, info->num_altitudes, data);
}

static int read_backscatter(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long dimension[3] = { info->num_wavelengths, info->num_times, info->num_altitudes };
    int order[3] = { 1, 0, 2 };

    if (read_array_variable(info, "backscatter", harp_type_double,
                            info->num_wavelengths * info->num_times * info->num_altitudes, data) != 0)
    {
        return -1;
    }

    return harp_array_transpose(harp_type_double, 3, dimension, order, data);
}

static int read_error_backscatter(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long dimension[3] = { info->num_wavelengths, info->num_times, info->num_altitudes };
    int order[3] = { 1, 0, 2 };

    if (read_array_variable(info, "error_backscatter", harp_type_double,
                            info->num_wavelengths * info->num_times * info->num_altitudes, data) != 0)
    {
        return -1;
    }

    return harp_array_transpose(harp_type_double, 3, dimension, order, data);
}

static int read_extinction(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long dimension[3] = { info->num_wavelengths, info->num_times, info->num_altitudes };
    int order[3] = { 1, 0, 2 };

    if (read_array_variable(info, "extinction", harp_type_double,
                            info->num_wavelengths * info->num_times * info->num_altitudes, data) != 0)
    {
        return -1;
    }

    return harp_array_transpose(harp_type_double, 3, dimension, order, data);
}

static int read_error_extinction(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long dimension[3] = { info->num_wavelengths, info->num_times, info->num_altitudes };
    int order[3] = { 1, 0, 2 };

    if (read_array_variable(info, "error_extinction", harp_type_double,
                            info->num_wavelengths * info->num_times * info->num_altitudes, data) != 0)
    {
        return -1;
    }

    return harp_array_transpose(harp_type_double, 3, dimension, order, data);
}

static int include_backscatter(void *user_data)
{
    return ((ingest_info *)user_data)->has_backscatter;
}

static int include_extinction(void *user_data)
{
    return ((ingest_info *)user_data)->has_extinction;
}

static int read_dimensions(void *user_data, long dimension[HARP_NUM_DIM_TYPES])
{
    ingest_info *info = (ingest_info *)user_data;

    dimension[harp_dimension_time] = info->num_times;
    dimension[harp_dimension_vertical] = info->num_altitudes;
    dimension[harp_dimension_spectral] = info->num_wavelengths;

    return 0;
}

static int get_dimensions_and_availability(ingest_info *info)
{
    coda_cursor cursor;
    long index;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    if (coda_cursor_goto(&cursor, "time") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &info->num_times) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    coda_cursor_goto_parent(&cursor);

    if (coda_cursor_goto(&cursor, "altitude") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &info->num_altitudes) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    coda_cursor_goto_parent(&cursor);

    if (coda_cursor_goto(&cursor, "wavelength") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &info->num_wavelengths) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    if (coda_cursor_get_record_field_index_from_name(&cursor, "backscatter", &index) == 0)
    {
        info->has_backscatter = 1;
    }

    if (coda_cursor_get_record_field_index_from_name(&cursor, "extinction", &index) == 0)
    {
        info->has_extinction = 1;
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
    info->product = product;
    info->num_times = 0;
    info->num_altitudes = 0;
    info->num_wavelengths = 0;
    info->has_backscatter = 0;
    info->has_extinction = 0;

    if (get_dimensions_and_availability(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }

    *definition = *module->product_definition;
    *user_data = info;

    return 0;
}

int harp_ingestion_module_earlinet_init(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[3] = { harp_dimension_time, harp_dimension_spectral, harp_dimension_vertical };
    const char *description;
    const char *path;

    module = harp_ingestion_register_module("EARLINET", "EARLINET", "EARLINET", "EARLINET",
                                            "EARLINET aerosol backscatter and extinction profiles", ingestion_init,
                                            ingestion_done);
    product_definition = harp_ingestion_register_product(module, "EARLINET", NULL, read_dimensions);

    /* datetime_start */
    description = "start time of measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime_start", harp_type_double, 1,
                                                   dimension_type, NULL, description, "seconds since 1970-01-01", NULL,
                                                   read_time_start);
    path = "/time_bounds[:,0]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* datetime_stop */
    description = "stop time of measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime_stop", harp_type_double, 1,
                                                   dimension_type, NULL, description, "seconds since 1970-01-01", NULL,
                                                   read_time_stop);
    path = "/time_bounds[:,1]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* latitude */
    description = "latitude";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_float, 0, dimension_type,
                                                   NULL, description, "degrees", NULL, read_latitude);
    path = "/latitude";
    harp_variable_definition_set_valid_range_float(variable_definition, -90.0, 90.0);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* longitude */
    description = "longitude";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_float, 0, dimension_type,
                                                   NULL, description, "degrees", NULL, read_longitude);
    path = "/longitude";
    harp_variable_definition_set_valid_range_float(variable_definition, -180.0, 180.0);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* sensor_altitude */
    description = "sensor altitude";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "sensor_altitude", harp_type_float, 0,
                                                   dimension_type, NULL, description, "m", NULL, read_station_altitude);
    path = "/station_altitude";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* viewing_zenith_angle */
    description = "viewing zenith angle";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "viewing_zenith_angle", harp_type_float, 0,
                                                   dimension_type, NULL, description, "degrees", NULL,
                                                   read_zenith_angle);
    path = "/zenith_angle";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* wavelength */
    description = "wavelength";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "wavelength", harp_type_float, 1,
                                                   &dimension_type[1], NULL, description, "nm", NULL, read_wavelength);
    path = "/wavelength";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* altitude */
    description = "altitude";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "altitude", harp_type_double, 1,
                                                   &(dimension_type[2]), NULL, description, "m", NULL, read_altitude);
    path = "/altitude";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* backscatter_coefficient */
    description = "backscatter coefficient";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "backscatter_coefficient", harp_type_double, 3,
                                                   dimension_type, NULL, description, "1/(m*sr)", include_backscatter,
                                                   read_backscatter);
    path = "/backscatter";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* backscatter_coefficient_uncertainty */
    description = "backscatter coefficient uncertainty";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "backscatter_coefficient_uncertainty",
                                                   harp_type_double, 3, dimension_type, NULL, description, "1/(m*sr)",
                                                   include_backscatter, read_error_backscatter);
    path = "/error_backscatter";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* extinction_coefficient */
    description = "extinction coefficient";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "extinction_coefficient", harp_type_double, 2,
                                                   dimension_type, NULL, description, "1/m", include_extinction,
                                                   read_extinction);
    path = "/extinction";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* extinction_coefficient_uncertainty */
    description = "extinction coefficient uncertainty";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "extinction_coefficient_uncertainty",
                                                   harp_type_double, 3, dimension_type, NULL, description, "1/m",
                                                   include_extinction, read_error_extinction);
    path = "/error_extinction";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    return 0;
}
