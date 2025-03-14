/*
 * Copyright (C) 2015-2025 S[&]T, The Netherlands.
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
#include "harp-constants.h"
#include "harp-ingestion.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct ingest_info_struct
{
    coda_product *product;
    int product_version;
    long num_time;
    long num_vertical;
} ingest_info;

static void ingestion_done(void *user_data)
{
    ingest_info *info = (ingest_info *)user_data;

    free(info);
}

static int read_dataset(ingest_info *info, const char *path, harp_data_type data_type, long num_elements,
                        harp_array data)
{
    coda_cursor cursor;
    long coda_num_elements;
    harp_scalar fill_value;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, path) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &coda_num_elements) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_num_elements != num_elements)
    {
        harp_set_error(HARP_ERROR_INGESTION, "dataset has %ld elements (expected %ld)", coda_num_elements,
                       num_elements);
        harp_add_coda_cursor_path_to_error_message(&cursor);
        harp_add_error_message(" (%s:%lu)", __FILE__, __LINE__);
        return -1;
    }
    switch (data_type)
    {
        case harp_type_int8:
            {
                coda_native_type read_type;

                if (coda_cursor_goto_first_array_element(&cursor) != 0)
                {
                    harp_set_error(HARP_ERROR_CODA, NULL);
                    return -1;
                }
                if (coda_cursor_get_read_type(&cursor, &read_type) != 0)
                {
                    harp_set_error(HARP_ERROR_CODA, NULL);
                    return -1;
                }
                coda_cursor_goto_parent(&cursor);
                if (read_type == coda_native_type_uint8)
                {
                    if (coda_cursor_read_uint8_array(&cursor, (uint8_t *)data.int8_data, coda_array_ordering_c) != 0)
                    {
                        harp_set_error(HARP_ERROR_CODA, NULL);
                        return -1;
                    }
                }
                else
                {
                    if (coda_cursor_read_int8_array(&cursor, data.int8_data, coda_array_ordering_c) != 0)
                    {
                        harp_set_error(HARP_ERROR_CODA, NULL);
                        return -1;
                    }
                }
            }
            break;
        case harp_type_int32:
            {
                coda_native_type read_type;

                if (coda_cursor_goto_first_array_element(&cursor) != 0)
                {
                    harp_set_error(HARP_ERROR_CODA, NULL);
                    return -1;
                }
                if (coda_cursor_get_read_type(&cursor, &read_type) != 0)
                {
                    harp_set_error(HARP_ERROR_CODA, NULL);
                    return -1;
                }
                coda_cursor_goto_parent(&cursor);
                if (read_type == coda_native_type_uint32)
                {
                    if (coda_cursor_read_uint32_array(&cursor, (uint32_t *)data.int32_data, coda_array_ordering_c) != 0)
                    {
                        harp_set_error(HARP_ERROR_CODA, NULL);
                        return -1;
                    }
                }
                else
                {
                    if (coda_cursor_read_int32_array(&cursor, data.int32_data, coda_array_ordering_c) != 0)
                    {
                        harp_set_error(HARP_ERROR_CODA, NULL);
                        return -1;
                    }
                }
            }
            break;
        case harp_type_float:
            if (coda_cursor_read_float_array(&cursor, data.float_data, coda_array_ordering_c) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            if (coda_cursor_goto(&cursor, "@missing_value[0]") != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            if (coda_cursor_read_float(&cursor, &fill_value.float_data) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            /* Replace values equal to the missing_value variable attribute by NaN. */
            harp_array_replace_fill_value(data_type, num_elements, data, fill_value);
            break;
        case harp_type_double:
            if (coda_cursor_read_double_array(&cursor, data.double_data, coda_array_ordering_c) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            if (coda_cursor_goto(&cursor, "@missing_value[0]") != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            if (coda_cursor_read_double(&cursor, &fill_value.double_data) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            /* Replace values equal to the missing_value variable attribute by NaN. */
            harp_array_replace_fill_value(data_type, num_elements, data, fill_value);
            break;
        default:
            assert(0);
            exit(1);
    }

    return 0;
}

static int read_vertical_profile_dataset(ingest_info *info, const char *path, harp_data_type data_type, long num_time,
                                         long num_vertical, harp_array data)
{
    long dimension[2];

    if (read_dataset(info, path, data_type, num_time * num_vertical, data) != 0)
    {
        return -1;
    }

    dimension[0] = num_time;
    dimension[1] = num_vertical;
    return harp_array_invert(data_type, 1, 2, dimension, data);
}

static int read_datetime(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "/time", harp_type_double, info->num_time, data);
}

static int read_latitude(void *user_data, harp_array data)
{
    return read_dataset((ingest_info *)user_data, "latitude", harp_type_double,
                        ((ingest_info *)user_data)->num_time, data);
}

static int read_longitude(void *user_data, harp_array data)
{
    return read_dataset((ingest_info *)user_data, "longitude", harp_type_double,
                        ((ingest_info *)user_data)->num_time, data);
}

static int read_altitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "Sounding/altitude", harp_type_double, info->num_time, data);
}

static int read_psurf(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "Retrieval/psurf", harp_type_double, info->num_time, data);
}

static int read_pressure_levels(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_vertical_profile_dataset(info, "pressure_levels", harp_type_double, info->num_time,
                                         info->num_vertical, data);
}

static int read_sensor_azimuth_angle(void *user_data, harp_array data)
{
    return read_dataset((ingest_info *)user_data, "Sounding/sensor_azimuth_angle", harp_type_double,
                        ((ingest_info *)user_data)->num_time, data);
}

static int read_sensor_zenith_angle(void *user_data, harp_array data)
{
    return read_dataset((ingest_info *)user_data, "sensor_zenith_angle", harp_type_double,
                        ((ingest_info *)user_data)->num_time, data);
}

static int read_solar_azimuth_angle(void *user_data, harp_array data)
{
    return read_dataset((ingest_info *)user_data, "Sounding/solar_azimuth_angle", harp_type_double,
                        ((ingest_info *)user_data)->num_time, data);
}

static int read_solar_zenith_angle(void *user_data, harp_array data)
{
    return read_dataset((ingest_info *)user_data, "solar_zenith_angle", harp_type_double,
                        ((ingest_info *)user_data)->num_time, data);
}

static int read_vertex_latitude(void *user_data, harp_array data)
{
    return read_dataset((ingest_info *)user_data, "vertex_latitude", harp_type_double,
                        ((ingest_info *)user_data)->num_time * 4, data);
}

static int read_vertex_longitude(void *user_data, harp_array data)
{
    return read_dataset((ingest_info *)user_data, "vertex_longitude", harp_type_double,
                        ((ingest_info *)user_data)->num_time * 4, data);
}

static int read_xco2(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "xco2", harp_type_double, info->num_time, data);
}

static int read_xco2_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "xco2_uncertainty", harp_type_double, info->num_time, data);
}

static int read_xco2_qf_simple_bitflag(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "xco2_qf_simple_bitflag", harp_type_int8, info->num_time, data);
}

static int read_xco2_quality_flag(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "xco2_quality_flag", harp_type_int8, info->num_time, data);
}

static int read_xco2_apriori(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "xco2_apriori", harp_type_double, info->num_time, data);
}

static int read_xco2_averaging_kernel(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_vertical_profile_dataset(info, "xco2_averaging_kernel", harp_type_double, info->num_time,
                                         info->num_vertical, data);
}

static int read_co2_profile_apriori(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_vertical_profile_dataset(info, "co2_profile_apriori", harp_type_double, info->num_time,
                                         info->num_vertical, data);
}

static int read_dimensions(void *user_data, long dimension[HARP_NUM_DIM_TYPES])
{
    ingest_info *info = (ingest_info *)user_data;

    dimension[harp_dimension_time] = info->num_time;
    dimension[harp_dimension_vertical] = info->num_vertical;

    return 0;
}

static int init_dimensions(ingest_info *info)
{
    coda_cursor cursor;
    long coda_dim[CODA_MAX_NUM_DIMS];
    int num_coda_dims;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, "/time") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_array_dim(&cursor, &num_coda_dims, coda_dim) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (num_coda_dims != 1)
    {
        harp_set_error(HARP_ERROR_INGESTION, "dataset has %d dimensions, expected 1", num_coda_dims);
        harp_add_coda_cursor_path_to_error_message(&cursor);
        return -1;
    }
    info->num_time = coda_dim[0];

    if (coda_cursor_goto(&cursor, "/levels") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_array_dim(&cursor, &num_coda_dims, coda_dim) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (num_coda_dims != 1)
    {
        harp_set_error(HARP_ERROR_INGESTION, "dataset has %d dimensions, expected 1", num_coda_dims);
        harp_add_coda_cursor_path_to_error_message(&cursor);
        return -1;
    }
    info->num_vertical = coda_dim[0];

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
    info->product_version = -1;
    if (coda_get_product_version(info->product, &info->product_version) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        ingestion_done(info);
        return -1;
    }
    if (init_dimensions(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }
    *definition = *module->product_definition;
    *user_data = info;

    return 0;
}

static int include_xco2_qf_simple_bitflag(void *user_data)
{
    return ((ingest_info *)user_data)->product_version > 9;
}

static void register_fields(harp_product_definition *product_definition, int has_corner_coordinates)
{
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_independent };
    long bounds_dimension[2] = { -1, 4 };
    const char *description;
    const char *path;

    /* datetime */
    description = "time of the measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double, 1, dimension_type,
                                                   NULL, description, "seconds since 1970-01-01", NULL, read_datetime);
    path = "/time[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* latitude */
    description = "center latitude of the measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_double, 1, dimension_type,
                                                   NULL, description, "degree_north", NULL, read_latitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    path = "/latitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* longitude */
    description = "center longitude of the measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_double, 1, dimension_type,
                                                   NULL, description, "degree_east", NULL, read_longitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0, 180.0);
    path = "/longitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    if (has_corner_coordinates)
    {
        /* latitude_bounds */
        description = "corner latitudes of the measurement";
        variable_definition =
            harp_ingestion_register_variable_full_read(product_definition, "latitude_bounds", harp_type_double, 2,
                                                       dimension_type, bounds_dimension, description, "degree_north",
                                                       NULL, read_vertex_latitude);
        harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
        path = "/vertex_latitude[]";
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

        /* longitude */
        description = "corner longitudes of the measurement";
        variable_definition =
            harp_ingestion_register_variable_full_read(product_definition, "longitude_bounds", harp_type_double, 2,
                                                       dimension_type, bounds_dimension, description, "degree_east",
                                                       NULL, read_vertex_longitude);
        harp_variable_definition_set_valid_range_double(variable_definition, -180.0, 180.0);
        path = "/vertex_longitude[]";
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
    }

    /* surface_altitude */
    description = "surface altitude";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "surface_altitude",
                                                   harp_type_double, 1, dimension_type, NULL, description, "m",
                                                   NULL, read_altitude);
    path = "/Sounding/altitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* surface_pressure */
    description = "retrieved surface pressure";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "surface_pressure",
                                                   harp_type_double, 1, dimension_type, NULL, description, "hPa",
                                                   NULL, read_psurf);
    path = "/Retrieval/psurf[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* pressure */
    dimension_type[1] = harp_dimension_vertical;
    description = "pressure levels";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "pressure",
                                                   harp_type_double, 2, dimension_type, NULL, description, "hPa",
                                                   NULL, read_pressure_levels);
    path = "/pressure_levels[]";
    description = "the vertical grid is inverted to make it ascending";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* sensor_azimuth_angle */
    description = "sensor azimuth angle";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "sensor_azimuth_angle", harp_type_double, 1,
                                                   dimension_type, NULL, description, "degree", NULL,
                                                   read_sensor_azimuth_angle);
    path = "/Sounding/sensor_azimuth_angle[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* sensor_zenith_angle */
    description = "sensor zenith angle";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "sensor_zenith_angle", harp_type_double, 1,
                                                   dimension_type, NULL, description, "degree", NULL,
                                                   read_sensor_zenith_angle);
    path = "/sensor_zenith_angle[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* solar_azimuth_angle */
    description = "solar azimuth angle";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "solar_azimuth_angle", harp_type_double, 1,
                                                   dimension_type, NULL, description, "degree", NULL,
                                                   read_solar_azimuth_angle);
    path = "/Sounding/solar_azimuth_angle[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* solar_zenith_angle */
    description = "solar zenith angle";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "solar_zenith_angle", harp_type_double, 1,
                                                   dimension_type, NULL, description, "degree", NULL,
                                                   read_solar_zenith_angle);
    path = "/solar_zenith_angle[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* CO2_column_volume_mixing_ratio_dry_air */
    description = "Column-averaged dry-air mole fraction of CO2 (includes bias correction)";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "CO2_column_volume_mixing_ratio_dry_air",
                                                   harp_type_double, 1, dimension_type, NULL, description, "ppmv", NULL,
                                                   read_xco2);
    path = "/xco2[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* CO2_column_volume_mixing_ratio_dry_air_uncertainty */
    description = "XCO2 posterior error";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "CO2_column_volume_mixing_ratio_dry_air_uncertainty",
                                                   harp_type_double, 1, dimension_type, NULL, description, "ppmv", NULL,
                                                   read_xco2_uncertainty);
    path = "/xco2_uncertainty[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* CO2_column_volume_mixing_ratio_dry_air_validity */
    description = "XCO2 simple quality bitflag";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "CO2_column_volume_mixing_ratio_dry_air_validity", harp_type_int8, 1,
                                                   dimension_type, NULL, description, NULL,
                                                   include_xco2_qf_simple_bitflag, read_xco2_qf_simple_bitflag);
    path = "/xco2_qf_simple_bitflag[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "version>9", path, NULL);

    /* CO2_column_volume_mixing_ratio_dry_air_apriori */
    description = "XCO2 a-priori";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "CO2_column_volume_mixing_ratio_dry_air_apriori",
                                                   harp_type_double, 1, dimension_type, NULL, description, "ppmv", NULL,
                                                   read_xco2_apriori);
    path = "/xco2_apriori[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* CO2_column_volume_mixing_ratio_dry_avk */
    description = "XCO2 column averaging kernel";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "CO2_column_volume_mixing_ratio_dry_avk",
                                                   harp_type_double, 2, dimension_type, NULL, description, "ppmv/ppmv",
                                                   NULL, read_xco2_averaging_kernel);
    path = "/xco2_averaging_kernel[]";
    description = "the vertical grid is inverted to make it ascending";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* CO2_volume_mixing_ratio_dry_air_apriori */
    description = "CO2 a-priori profile";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "CO2_volume_mixing_ratio_dry_air_apriori",
                                                   harp_type_double, 2, dimension_type, NULL, description, "ppmv", NULL,
                                                   read_co2_profile_apriori);
    path = "/co2_profile_apriori[]";
    description = "the vertical grid is inverted to make it ascending";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* validity */
    description = "XCO2 quality flag";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "validity", harp_type_int8, 1, dimension_type,
                                                   NULL, description, NULL, NULL, read_xco2_quality_flag);
    path = "/xco2_quality_flag[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

static void register_module_acos_LtCO2(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;

    module = harp_ingestion_register_module("OCO_ACOS_LtCO2", "OCO", "OCO", "acos_LtCO2", "ACOS GOSAT L2 Lite CO2",
                                            ingestion_init, ingestion_done);
    product_definition = harp_ingestion_register_product(module, "OCO_ACOS_LtCO2", NULL, read_dimensions);

    register_fields(product_definition, 0);
}

static void register_module_oco2_LtCO2(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;

    module = harp_ingestion_register_module("OCO_OCO2_LtCO2", "OCO", "OCO", "oco2_LtCO2", "OCO-2 L2 Lite CO2",
                                            ingestion_init, ingestion_done);
    product_definition = harp_ingestion_register_product(module, "OCO_OCO2_LtCO2", NULL, read_dimensions);

    register_fields(product_definition, 1);
}

static void register_module_oco3_LtCO2(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;

    module = harp_ingestion_register_module("OCO_OCO3_LtCO2", "OCO", "OCO", "oco3_LtCO2", "OCO-3 L2 Lite CO2",
                                            ingestion_init, ingestion_done);
    product_definition = harp_ingestion_register_product(module, "OCO_OCO3_LtCO2", NULL, read_dimensions);

    register_fields(product_definition, 1);
}

int harp_ingestion_module_oco_ltco2_init(void)
{
    register_module_acos_LtCO2();
    register_module_oco2_LtCO2();
    register_module_oco3_LtCO2();
    return 0;
}
