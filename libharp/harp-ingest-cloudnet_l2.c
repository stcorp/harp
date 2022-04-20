/*
 * Copyright (C) 2015-2022 S[&]T, The Netherlands.
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
#include <stdio.h>

/* ------------------ Typedefs ------------------ */

typedef struct ingest_info_struct
{
    coda_product *product;
    long num_times;
    long num_altitudes;
    int use_amsl_height;
} ingest_info;

/* -------------------- Code -------------------- */

static void ingestion_done(void *user_data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info != NULL)
    {
        free(info);
    }
}

/* General read functions */

static int read_scalar_variable(ingest_info *info, const char *name, harp_array data)
{
    coda_cursor cursor;
    coda_type_class type_class;
    int result;

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
    if (coda_cursor_get_type_class(&cursor, &type_class) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (type_class == coda_array_class)
    {
        long actual_num_elements;

        if (coda_cursor_get_num_elements(&cursor, &actual_num_elements) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (actual_num_elements != 1)
        {
            harp_set_error(HARP_ERROR_INGESTION, "variable %s has %ld elements (expected scalar)", name,
                           actual_num_elements);
            return -1;
        }
        if (coda_cursor_goto_first_array_element(&cursor) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
    }
    if (coda_cursor_read_float(&cursor, data.float_data) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    /* filter for NaN */
    result = coda_cursor_goto(&cursor, "@FillValue[0]");
    if (result == 0)
    {
        float fillValue;

        if (coda_cursor_read_float(&cursor, &fillValue) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (data.float_data[0] == fillValue)
        {
            data.float_data[0] = (float)coda_NaN();
        }
    }

    return 0;
}

static int read_array_variable(ingest_info *info, const char *name, long num_elements, harp_data_type data_type,
                               harp_array data)
{
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
    if (data_type == harp_type_float)
    {
        int result;
        long i;

        if (coda_cursor_read_float_array(&cursor, data.float_data, coda_array_ordering_c) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        /* filter for NaN */
        result = coda_cursor_goto(&cursor, "@FillValue[0]");
        if (result == 0)
        {
            float nan = (float)coda_NaN();
            float fillValue;

            if (coda_cursor_read_float(&cursor, &fillValue) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            for (i = 0; i < num_elements; i++)
            {
                if (data.float_data[i] == fillValue)
                {
                    data.float_data[i] = nan;
                }
            }
        }
    }
    else
    {
        coda_native_type native_type;

        assert(data_type == harp_type_int8);

        if (coda_cursor_goto_first_array_element(&cursor) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_get_read_type(&cursor, &native_type) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        coda_cursor_goto_parent(&cursor);
        if (native_type == coda_native_type_int32)
        {
            int32_t *int32_data;
            long i;

            /* this is data that should actually have been stored in an int8 array -> we cast it ourselves */

            int32_data = malloc(num_elements * sizeof(int32_t));
            if (int32_data == NULL)
            {
                harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                               num_elements * sizeof(int32_t), __FILE__, __LINE__);
                return -1;

            }
            if (coda_cursor_read_int32_array(&cursor, int32_data, coda_array_ordering_c) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                free(int32_data);
                return -1;
            }
            for (i = 0; i < num_elements; i++)
            {
                data.int8_data[i] = (int8_t)int32_data[i];
            }
            free(int32_data);
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

    return 0;
}

/* Specific read functions */

static int read_datetime(void *user_data, harp_array data)
{
    coda_cursor cursor;
    double *double_data, datetime_start_of_day;
    ingest_info *info = (ingest_info *)user_data;
    long i;
    char str[1024];

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&cursor, "time") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_double_array(&cursor, data.double_data, coda_array_ordering_c) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    if (coda_cursor_goto_attributes(&cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&cursor, "units") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_string(&cursor, str, sizeof(str)) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    /* Read datetime from unit string */
    str[12 + 19] = '\0';        /* 12 = strlen("hours since "), 19 = strlen("yyyy-MM-dd HH:mm:ss") */
    if (coda_time_string_to_double("yyyy-MM-dd HH:mm:ss", str + 12, &datetime_start_of_day) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    double_data = data.double_data;
    for (i = 0; i < info->num_times; i++)
    {
        *double_data = (*double_data * 60 * 60) + datetime_start_of_day;
        double_data++;
    }
    return 0;
}

static int read_sensor_latitude(void *user_data, harp_array data)
{
    return read_scalar_variable((ingest_info *)user_data, "latitude", data);
}

static int read_sensor_longitude(void *user_data, harp_array data)
{
    return read_scalar_variable((ingest_info *)user_data, "longitude", data);
}

static int read_sensor_altitude(void *user_data, harp_array data)
{
    return read_scalar_variable((ingest_info *)user_data, "altitude", data);
}

static int read_cloud_base_height(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    const char *variable_name = info->use_amsl_height ? "cloud_base_height_amsl" : "cloud_base_height";

    return read_array_variable(info, variable_name, info->num_times, harp_type_float, data);
}

static int read_cloud_top_height(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    const char *variable_name = info->use_amsl_height ? "cloud_top_height_amsl" : "cloud_top_height";

    return read_array_variable(info, variable_name, info->num_times, harp_type_float, data);
}

static int read_altitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array_variable(info, "height", info->num_altitudes, harp_type_float, data);
}

static int read_cloud_type(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array_variable(info, "target_classification", info->num_times * info->num_altitudes, harp_type_int8,
                               data);
}

static int read_detection_status(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array_variable(info, "detection_status", info->num_times * info->num_altitudes, harp_type_int8, data);
}

static int read_dimensions(void *user_data, long dimension[HARP_NUM_DIM_TYPES])
{
    ingest_info *info = (ingest_info *)user_data;

    dimension[harp_dimension_time] = info->num_times;
    dimension[harp_dimension_vertical] = info->num_altitudes;

    return 0;
}

static int get_dimensions(ingest_info *info)
{
    coda_cursor cursor;
    long coda_dimension[CODA_MAX_NUM_DIMS];
    int num_coda_dimensions;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, "time") != 0)
    {
        /* This productfile does not contain data */
        info->num_times = 0;
        return 0;
    }
    if (coda_cursor_get_array_dim(&cursor, &num_coda_dimensions, coda_dimension) != 0)
    {
        /* This productfile does not contain data */
        info->num_times = 0;
        return 0;
    }
    info->num_times = coda_dimension[0];

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, "height") != 0)
    {
        /* This productfile does not contain data */
        info->num_altitudes = 0;
        return 0;
    }
    if (coda_cursor_get_array_dim(&cursor, &num_coda_dimensions, coda_dimension) != 0)
    {
        /* This productfile does not contain data */
        info->num_altitudes = 0;
        return 0;
    }
    info->num_altitudes = coda_dimension[0];

    return 0;
}

static int get_cloud_height_name(ingest_info *info)
{
    coda_cursor cursor;
    coda_type *type;
    long index;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_type(&cursor, &type) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_type_get_record_field_index_from_name(type, "cloud_base_height_amsl", &index) == 0)
    {
        info->use_amsl_height = 1;
        return 0;
    }
    if (coda_type_get_record_field_index_from_name(type, "cloud_base_height", &index) == 0)
    {
        info->use_amsl_height = 0;
        return 0;
    }

    harp_set_error(HARP_ERROR_INGESTION, "product does not contain a cloud base height variable");
    return -1;
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
    memset(info, '\0', sizeof(ingest_info));
    info->product = product;

    if (get_dimensions(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }
    if (get_cloud_height_name(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }

    *definition = *module->product_definition;
    *user_data = info;

    return 0;
}

int harp_ingestion_module_actris_clouds_l2_aerosol_init(void)
{
    const char *cloud_type_values[] = {
        "clear_sky", "cloud_droplets", "drizzle_rain", "drizzle_rain_cloud_droplets", "ice", "ice_supercooled_droplets",
        "melting_ice", "melting_ice_cloud_droplets", "aerosol", "insects", "aerosol_insects"
    };
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_vertical };
    const char *description;
    const char *path;

    module = harp_ingestion_register_module("CLOUDNET_L2_classification", "CLOUDNET", "CLOUDNET", "classification",
                                            "Cloudnet L2A target classification and cloud boundaries",
                                            ingestion_init, ingestion_done);

    product_definition = harp_ingestion_register_product(module, "CLOUDNET_L2_classification", NULL, read_dimensions);

    /* datetime */
    description = "date and time";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double, 1, dimension_type,
                                                   NULL, description, "seconds since 2000-01-01", NULL, read_datetime);
    path = "/time";
    description = "convert hours since 00:00:00 of the day of the measurement to seconds since 2000-01-01";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* sensor_latitude */
    description = "latitude of the instrument";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "sensor_latitude", harp_type_float, 0,
                                                   dimension_type, NULL, description, "degree_north", NULL,
                                                   read_sensor_latitude);
    harp_variable_definition_set_valid_range_float(variable_definition, -90.0, 90.0);
    path = "/latitude";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* sensor_longitude */
    description = "longitude of the instrument";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "sensor_longitude", harp_type_float, 0,
                                                   dimension_type, NULL, description, "degree_east", NULL,
                                                   read_sensor_longitude);
    harp_variable_definition_set_valid_range_float(variable_definition, 0, 360.0);
    path = "/longitude";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* sensor_altitude */
    description = "altitude of the instrument above mean sea level";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "sensor_altitude", harp_type_float, 0,
                                                   dimension_type, NULL, description, "m", NULL, read_sensor_altitude);
    path = "/altitude";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* altitude */
    description = "altitude of the measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "altitude", harp_type_float, 1,
                                                   &(dimension_type[1]), NULL, description, "m", NULL, read_altitude);
    path = "/height";
    description = "height above mean see level";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* cloud_type */
    description = "cloud classification type";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_type", harp_type_int8, 2,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_cloud_type);
    harp_variable_definition_set_enumeration_values(variable_definition, 11, cloud_type_values);
    path = "/target_classification";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_type_validity */
    description = "detection status";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_type_validity", harp_type_int8, 2,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_detection_status);
    path = "/detection_status";
    description = "radar and lidar detection status";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* cloud_base_height */
    description = "cloud_base_height";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_base_height", harp_type_float, 1,
                                                   dimension_type, NULL, description, "m", NULL,
                                                   read_cloud_base_height);
    path = "/cloud_base_height";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, "legacy format");
    path = "/cloud_base_height_amsl";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, "cloudnetpy format");

    /* cloud_top_height */
    description = "cloud_top_height";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_top_height", harp_type_float, 1,
                                                   dimension_type, NULL, description, "m", NULL, read_cloud_top_height);
    path = "/cloud_top_height";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, "legacy format");
    path = "/cloud_top_height_amsl";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, "cloudnetpy format");

    return 0;
}
