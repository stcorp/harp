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
#include "harp-constants.h"
#include "harp-ingestion.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ------------------- Defines ------------------ */

#define SECONDS_PER_DAY                  86400
#define SECONDS_FROM_1970_TO_2000    946684800

#define CHECKED_MALLOC(v, s) v = malloc(s); if (v == NULL) { harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)", s, __FILE__, __LINE__); return -1;}

#define MAX_WAVELENGTHS                     10

/* ------------------ Typedefs ------------------ */

typedef struct ingest_info_struct
{
    coda_product *product;
    long num_time;
} ingest_info;

typedef enum ghg_data_source
{
    EMMA,
    GOSAT,
    SCIAMACHY,
    TROPOMI
} ghg_data_source;

/* -------------------- Code -------------------- */

static void ingestion_done(void *user_data)
{
    ingest_info *info = (ingest_info *)user_data;

    free(info);
}

static int read_dataset(ingest_info *info, const char *path, long num_elements, harp_array data)
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
    if (coda_cursor_read_double_array(&cursor, data.double_data, coda_array_ordering_c) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    fill_value.double_data = -999.0;
    harp_array_replace_fill_value(harp_type_double, num_elements, data, fill_value);

    return 0;
}

static int read_datetime(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    double *double_data = data.double_data;
    long i;
    int retval;

    retval = read_dataset(info, "/time", info->num_time, data);
    for (i = 0; i < info->num_time; i++)
    {
        *double_data = *double_data - SECONDS_FROM_1970_TO_2000;
        double_data++;
    }
    return retval;
}

static int read_surface_altitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (read_dataset(info, "altitude", info->num_time, data) != 0)
    {
        return read_dataset(info, "surface_altitude", info->num_time, data);
    }
    return 0;
}

static int read_latitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (read_dataset(info, "latitude", info->num_time, data) != 0)
    {
        return read_dataset(info, "latitude_centre", info->num_time, data);
    }
    return 0;
}

static int read_longitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (read_dataset(info, "longitude", info->num_time, data) != 0)
    {
        return read_dataset(info, "longitude_centre", info->num_time, data);
    }
    return 0;
}

static int read_latitude_bounds(void *user_data, harp_array data)
{
    return read_dataset((ingest_info *)user_data, "latitude_corners", 4 * ((ingest_info *)user_data)->num_time, data);
}

static int read_latitude_bounds_bdca(void *user_data, harp_array data)
{
    double *double_data = data.double_data;
    double a, b, c, d;
    long i;

    if (read_latitude_bounds(user_data, data) != 0)
    {
        return -1;
    }

    /* Rearrange the corners ABCD as BDCA */
    for (i = 0; i < ((ingest_info *)user_data)->num_time; i++)
    {
        a = *double_data;
        b = *(double_data + 1);
        c = *(double_data + 2);
        d = *(double_data + 3);
        *double_data = b;
        *(double_data + 1) = d;
        *(double_data + 2) = c;
        *(double_data + 3) = a;
        double_data += 4;
    }

    return 0;
}

static int read_longitude_bounds(void *user_data, harp_array data)
{
    return read_dataset((ingest_info *)user_data, "longitude_corners", 4 * ((ingest_info *)user_data)->num_time, data);
}

static int read_longitude_bounds_bdca(void *user_data, harp_array data)
{
    double *double_data = data.double_data;
    double a, b, c, d;
    long i;

    if (read_longitude_bounds(user_data, data) != 0)
    {
        return -1;
    }

    /* Rearrange the corners ABCD as BDCA */
    for (i = 0; i < ((ingest_info *)user_data)->num_time; i++)
    {
        a = *double_data;
        b = *(double_data + 1);
        c = *(double_data + 2);
        d = *(double_data + 3);
        *double_data = b;
        *(double_data + 1) = d;
        *(double_data + 2) = c;
        *(double_data + 3) = a;
        double_data += 4;
    }

    return 0;
}

static int read_sensor_zenith_angle(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (read_dataset(info, "sensor_zenith_angle", info->num_time, data) != 0)
    {
        read_dataset(info, "viewing_zenith_angle", info->num_time, data);
    }
    return 0;
}

static int read_solar_zenith_angle(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "solar_zenith_angle", info->num_time, data);
}

static int read_CH4_column_volume_mixing_ratio(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "xch4", info->num_time, data);
}

static int read_CH4_column_volume_mixing_ratio_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "xch4_uncertainty", info->num_time, data);
}

static int read_CO_column_volume_mixing_ratio(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "xco", info->num_time, data);
}

static int read_CO_column_volume_mixing_ratio_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "xco_uncertainty", info->num_time, data);
}

static int read_CO2_column_volume_mixing_ratio(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "xco2", info->num_time, data);
}

static int read_CO2_column_volume_mixing_ratio_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "xco2_uncertainty", info->num_time, data);
}

static int read_dimensions(void *user_data, long dimension[HARP_NUM_DIM_TYPES])
{
    ingest_info *info = (ingest_info *)user_data;

    dimension[harp_dimension_time] = info->num_time;

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

    return 0;
}

static int include_surface_altitude(void *user_data)
{
    ingest_info *info = (ingest_info *)user_data;
    coda_cursor cursor;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        return 0;
    }
    if ((coda_cursor_goto(&cursor, "altitude") != 0) && (coda_cursor_goto(&cursor, "surface_altitude") != 0))
    {
        return 0;
    }
    return 1;
}

static int include_latitude_bounds(void *user_data)
{
    ingest_info *info = (ingest_info *)user_data;
    coda_cursor cursor;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        return 0;
    }
    if (coda_cursor_goto(&cursor, "latitude_corners") != 0)
    {
        return 0;
    }
    return 1;
}

static int include_longitude_bounds(void *user_data)
{
    ingest_info *info = (ingest_info *)user_data;
    coda_cursor cursor;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        return 0;
    }
    if (coda_cursor_goto(&cursor, "longitude_corners") != 0)
    {
        return 0;
    }
    return 1;
}

static int include_sensor_zenith_angle(void *user_data)
{
    ingest_info *info = (ingest_info *)user_data;
    coda_cursor cursor;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        return 0;
    }
    if ((coda_cursor_goto(&cursor, "sensor_zenith_angle") != 0) &&
        (coda_cursor_goto(&cursor, "viewing_zenith_angle") != 0))
    {
        return 0;
    }
    return 1;
}

static int include_solar_zenith_angle(void *user_data)
{
    ingest_info *info = (ingest_info *)user_data;
    coda_cursor cursor;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        return 0;
    }
    if (coda_cursor_goto(&cursor, "solar_zenith_angle") != 0)
    {
        return 0;
    }
    return 1;
}

static int include_ch4(void *user_data)
{
    ingest_info *info = (ingest_info *)user_data;
    coda_cursor cursor;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        return 0;
    }
    if (coda_cursor_goto(&cursor, "xch4") != 0)
    {
        return 0;
    }
    return 1;
}

static int include_co2(void *user_data)
{
    ingest_info *info = (ingest_info *)user_data;
    coda_cursor cursor;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        return 0;
    }
    if (coda_cursor_goto(&cursor, "xco2") != 0)
    {
        return 0;
    }
    return 1;
}

static void register_fields(harp_product_definition *product_definition, ghg_data_source source)
{
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };
    harp_dimension_type bounds_dimension_type[2] = { harp_dimension_time, harp_dimension_independent };
    long bounds_dimension[2] = { -1, 4 };
    const char *description;
    const char *condition;
    const char *path;

    /* datetime */
    description = "time of the measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double, 1, dimension_type,
                                                   NULL, description, "seconds since 2000-01-01", NULL, read_datetime);
    path = "time[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    if (source == SCIAMACHY)
    {
        /* surface_altitude */
        description = "average surface altitude w.r.t. geoid";
        variable_definition =
            harp_ingestion_register_variable_full_read(product_definition, "surface_altitude", harp_type_double, 1,
                                                       dimension_type, NULL, description, "m", include_surface_altitude,
                                                       read_surface_altitude);
        path = "altitude[]";
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
    }
    if (source == GOSAT)
    {
        /* surface_altitude */
        description = "average surface altitude w.r.t. geoid";
        variable_definition =
            harp_ingestion_register_variable_full_read(product_definition, "surface_altitude", harp_type_double, 1,
                                                       dimension_type, NULL, description, "m", NULL,
                                                       read_surface_altitude);
        condition = "data processed by OCFP or OCPR algorithm";
        path = "surface_altitude[]";
        harp_variable_definition_add_mapping(variable_definition, NULL, condition, path, NULL);
        condition = "data processed by SRPR or SRFP algorithm";
        path = "altitude[]";
        harp_variable_definition_add_mapping(variable_definition, NULL, condition, path, NULL);
    }

    /* latitude */
    description = "latitude of the ground pixel center";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_double, 1, dimension_type,
                                                   NULL, description, "degree_north", NULL, read_latitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    path = "latitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* longitude */
    description = "longitude of the ground pixel center";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_double, 1, dimension_type,
                                                   NULL, description, "degree_east", NULL, read_longitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0, 180.0);
    path = "longitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    if (source == SCIAMACHY)
    {
        /* latitude_bounds */
        description = "corner latitudes for the ground pixel of the measurement";
        variable_definition =
            harp_ingestion_register_variable_full_read(product_definition, "latitude_bounds", harp_type_double, 2,
                                                       bounds_dimension_type, bounds_dimension, description,
                                                       "degree_north", include_latitude_bounds,
                                                       read_latitude_bounds_bdca);
        harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
        description = "The corners ABCD are reordered as BDCA.";
        path = "latitude_corners[]";
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

        /* longitude_bounds */
        description = "corner longitudes for the ground pixel of the measurement";
        variable_definition =
            harp_ingestion_register_variable_full_read(product_definition, "longitude_bounds", harp_type_double, 2,
                                                       bounds_dimension_type, bounds_dimension, description,
                                                       "degree_east", include_longitude_bounds,
                                                       read_longitude_bounds_bdca);
        harp_variable_definition_set_valid_range_double(variable_definition, -180.0, 180.0);
        description = "The corners ABCD are reordered as BDCA.";
        path = "longitude_corners[]";
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);
    }
    if (source == TROPOMI)
    {
        /* latitude_bounds */
        description = "corner latitudes for the ground pixel of the measurement";
        variable_definition =
            harp_ingestion_register_variable_full_read(product_definition, "latitude_bounds", harp_type_double, 2,
                                                       bounds_dimension_type, bounds_dimension, description,
                                                       "degree_north", NULL, read_latitude_bounds);
        harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "latitude_corners[]", NULL);

        /* longitude_bounds */
        description = "corner longitudes for the ground pixel of the measurement";
        variable_definition =
            harp_ingestion_register_variable_full_read(product_definition, "longitude_bounds", harp_type_double, 2,
                                                       bounds_dimension_type, bounds_dimension, description,
                                                       "degree_east", NULL, read_longitude_bounds);
        harp_variable_definition_set_valid_range_double(variable_definition, -180.0, 180.0);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "longitude_corners[]", NULL);
    }

    /* sensor_zenith_angle */
    description = "sensor zenith angle";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "sensor_zenith_angle", harp_type_double, 1,
                                                   dimension_type, NULL, description, "degree",
                                                   (source == EMMA) ? include_sensor_zenith_angle : NULL,
                                                   read_sensor_zenith_angle);
    path = "sensor_zenith_angle[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* solar_zenith_angle */
    description = "solar zenith angle";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "solar_zenith_angle", harp_type_double, 1,
                                                   dimension_type, NULL, description, "degree",
                                                   (source == EMMA) ? include_solar_zenith_angle : NULL,
                                                   read_solar_zenith_angle);
    path = "solar_zenith_angle[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* CH4_column_volume_mixing_ratio */
    description = "CH4 column volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "CH4_column_volume_mixing_ratio",
                                                   harp_type_double, 1, dimension_type, NULL, description, "ppmv",
                                                   include_ch4, read_CH4_column_volume_mixing_ratio);
    path = "xch4[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* CH4_column_volume_mixing_ratio_uncertainty */
    description = "CH4 column volume mixing ratio uncertainty";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "CH4_column_volume_mixing_ratio_uncertainty",
                                                   harp_type_double, 1, dimension_type, NULL, description, "ppmv",
                                                   include_ch4, read_CH4_column_volume_mixing_ratio_uncertainty);
    path = "xch4_uncertainty[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    if (source == TROPOMI)
    {
        /* CO_column_volume_mixing_ratio */
        description = "CO column volume mixing ratio";
        variable_definition =
            harp_ingestion_register_variable_full_read(product_definition, "CO_column_volume_mixing_ratio",
                                                       harp_type_double, 1, dimension_type, NULL, description, "ppmv",
                                                       NULL, read_CO_column_volume_mixing_ratio);
        path = "xco[]";
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

        /* CO_column_volume_mixing_ratio_uncertainty */
        description = "CO column volume mixing ratio uncertainty";
        variable_definition =
            harp_ingestion_register_variable_full_read(product_definition, "CO_column_volume_mixing_ratio_uncertainty",
                                                       harp_type_double, 1, dimension_type, NULL, description, "ppmv",
                                                       NULL, read_CO_column_volume_mixing_ratio_uncertainty);
        path = "xco_uncertainty[]";
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
    }

    if (source == EMMA || source == GOSAT || source == SCIAMACHY)
    {
        /* CO2_column_volume_mixing_ratio */
        description = "CO2 column volume mixing ratio";
        variable_definition =
            harp_ingestion_register_variable_full_read(product_definition, "CO2_column_volume_mixing_ratio",
                                                       harp_type_double, 1, dimension_type, NULL, description, "ppmv",
                                                       include_co2, read_CO2_column_volume_mixing_ratio);
        path = "xco2[]";
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

        /* CO2_column_volume_mixing_ratio_uncertainty */
        description = "CO2 column volume mixing ratio uncertainty";
        variable_definition =
            harp_ingestion_register_variable_full_read(product_definition, "CO2_column_volume_mixing_ratio_uncertainty",
                                                       harp_type_double, 1, dimension_type, NULL, description, "ppmv",
                                                       include_co2, read_CO2_column_volume_mixing_ratio_uncertainty);
        path = "xco2_uncertainty[]";
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
    }
}

static int ingestion_init(const harp_ingestion_module *module, coda_product *product,
                          const harp_ingestion_options *options, harp_product_definition **definition, void **user_data)
{
    ingest_info *info;

    (void)options;

    CHECKED_MALLOC(info, sizeof(ingest_info));
    info->product = product;
    if (init_dimensions(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }
    *definition = *module->product_definition;
    *user_data = info;

    return 0;
}

/* Start of code that is specific for the EMMA algorithm */

static void register_module_l2_EMMA(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;

    module = harp_ingestion_register_module("ESACCI_GHG_L2_EMMA", "Green House Gases CCI", "ESACCI_GHG",
                                            "EMMA_L2", "CCI L2 Green House Gases calculated by EMMA",
                                            ingestion_init, ingestion_done);
    product_definition = harp_ingestion_register_product(module, "ESACCI_GHG_L2_EMMA", NULL, read_dimensions);

    register_fields(product_definition, EMMA);
}

/* Start of code that is specific for the GOSAT satellite */

static void register_module_l2_GOSAT(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;

    module = harp_ingestion_register_module("ESACCI_GHG_L2_GOSAT", "Green House Gases CCI", "ESACCI_GHG",
                                            "GOSAT_L2", "CCI L2 Green House Gases from GOSAT",
                                            ingestion_init, ingestion_done);
    product_definition = harp_ingestion_register_product(module, "ESACCI_GHG_L2_GOSAT", NULL, read_dimensions);

    register_fields(product_definition, GOSAT);
}

/* Start of code that is specific for the SCIAMACHY instrument */

static void register_module_l2_SCIAMACHY(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;

    module = harp_ingestion_register_module("ESACCI_GHG_L2_SCIAMACHY", "Green House Gases CCI", "ESACCI_GHG",
                                            "SCIAMACHY_L2", "CCI L2 Green House Gases from SCIAMACHY",
                                            ingestion_init, ingestion_done);
    product_definition = harp_ingestion_register_product(module, "ESACCI_GHG_L2_SCIAMACHY", NULL, read_dimensions);

    register_fields(product_definition, SCIAMACHY);
}

/* Start of code that is specific for the TROPOMI instrument */

static void register_module_l2_TROPOMI(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;

    module = harp_ingestion_register_module("ESACCI_GHG_L2_TROPOMI", "Green House Gases CCI", "ESACCI_GHG",
                                            "TROPOMI_L2", "CCI L2 Green House Gases from TROPOMI",
                                            ingestion_init, ingestion_done);
    product_definition = harp_ingestion_register_product(module, "ESACCI_GHG_L2_TROPOMI", NULL, read_dimensions);

    register_fields(product_definition, TROPOMI);
}

/* Main procedure for all instruments */

int harp_ingestion_module_cci_l2_ghg_init(void)
{
    /* GHG-CCI core products generated with ECAs */
    register_module_l2_EMMA();
    register_module_l2_GOSAT();
    register_module_l2_SCIAMACHY();
    register_module_l2_TROPOMI();
    return 0;
}
