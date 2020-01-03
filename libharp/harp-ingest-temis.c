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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct ingest_info_struct
{
    coda_product *product;
    int is_mean;
    double time;
    int num_latitudes;
    double latitude_min;
    double latitude_max;
    int num_longitudes;
    double longitude_min;
    double longitude_max;
} ingest_info;

static int read_data_set(ingest_info *info, const char *data_set_name, double *buffer)
{
    coda_cursor cursor;
    long num_elements;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&cursor, data_set_name) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &num_elements) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (num_elements != info->num_latitudes * info->num_longitudes)
    {
        harp_set_error(HARP_ERROR_INGESTION, "product error detected (inconsistent grid array size %ld != %ld)",
                       info->num_latitudes * info->num_longitudes, num_elements);
        return -1;
    }
    if (coda_cursor_read_double_array(&cursor, buffer, coda_array_ordering_c) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    return 0;
}

static int read_dimensions(void *user_data, long dimension[HARP_NUM_DIM_TYPES])
{
    ingest_info *info = (ingest_info *)user_data;

    dimension[harp_dimension_time] = 1;
    dimension[harp_dimension_longitude] = info->num_longitudes;
    dimension[harp_dimension_latitude] = info->num_latitudes;

    return 0;
}

static int read_datetime(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    *data.double_data = info->time;

    return 0;
}

static int read_longitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long i;

    for (i = 0; i < info->num_longitudes; i++)
    {
        data.double_data[i] = info->longitude_min +
            (info->longitude_max - info->longitude_min) * i / (info->num_longitudes - 1);
    }

    return 0;
}

static int read_latitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long i;

    for (i = 0; i < info->num_latitudes; i++)
    {
        data.double_data[i] = info->latitude_min +
            (info->latitude_max - info->latitude_min) * i / (info->num_latitudes - 1);
    }

    return 0;
}

static int read_o3_column(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (read_data_set(info, info->is_mean ? "Average_O3_column" : "O3_column", data.double_data) != 0)
    {
        return -1;
    }

    return 0;
}

static int read_o3_std(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (read_data_set(info, info->is_mean ? "Average_O3_std" : "O3_std", data.double_data) != 0)
    {
        return -1;
    }

    return 0;
}

static int get_global_attributes(ingest_info *info)
{
    coda_cursor cursor;
    int32_t comp[6];
    double range[2];
    int32_t length;
    long num_elements;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, "@Ozone_field_date") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &num_elements) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (num_elements != 2 && num_elements != 6)
    {
        harp_set_error(HARP_ERROR_INGESTION,
                       "product error detected (invalid number of elements for /@Ozone_field_date)");
        return -1;
    }
    /* use 1st day of the month 00:00:00 as default */
    comp[2] = 1;
    comp[3] = 0;
    comp[4] = 0;
    comp[5] = 0;
    if (coda_cursor_read_int32_array(&cursor, comp, coda_array_ordering_c) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_time_parts_to_double(comp[0], comp[1], comp[2], comp[3], comp[4], comp[5], 0, &info->time) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    info->time /= 86400.0;
    coda_cursor_goto_parent(&cursor);

    if (coda_cursor_goto(&cursor, "Number_of_longitudes") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_int32(&cursor, &length) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    info->num_longitudes = length;
    coda_cursor_goto_parent(&cursor);

    if (coda_cursor_goto(&cursor, "Longitude_range") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &num_elements) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (num_elements != 2)
    {
        harp_set_error(HARP_ERROR_INGESTION,
                       "product error detected (invalid number of elements for /@Longitude_range)");
        return -1;
    }
    if (coda_cursor_read_double_array(&cursor, range, coda_array_ordering_c) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    info->longitude_min = range[0];
    info->longitude_max = range[1];
    coda_cursor_goto_parent(&cursor);

    if (coda_cursor_goto(&cursor, "Number_of_latitudes") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_int32(&cursor, &length) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    info->num_latitudes = length;
    coda_cursor_goto_parent(&cursor);

    if (coda_cursor_goto(&cursor, "Latitude_range") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &num_elements) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (num_elements != 2)
    {
        harp_set_error(HARP_ERROR_INGESTION,
                       "product error detected (invalid number of elements for /@Latitude_range)");
        return -1;
    }
    if (coda_cursor_read_double_array(&cursor, range, coda_array_ordering_c) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    info->latitude_min = range[0];
    info->latitude_max = range[1];
    coda_cursor_goto_parent(&cursor);

    return 0;
}

static void ingest_info_delete(ingest_info *info)
{
    if (info != NULL)
    {
        free(info);
    }
}

static int ingest_info_new(coda_product *product, ingest_info **new_info)
{
    ingest_info *info;

    info = malloc(sizeof(ingest_info));
    if (info == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(ingest_info), __FILE__, __LINE__);
        return -1;
    }

    info->product = product;
    info->is_mean = 0;
    info->time = 0;
    info->num_latitudes = 0;
    info->latitude_min = 0.0;
    info->latitude_max = 0.0;
    info->num_longitudes = 0;
    info->longitude_min = 0.0;
    info->longitude_max = 0.0;

    *new_info = info;

    return 0;
}

static void ingestion_done(void *user_data)
{
    ingest_info_delete((ingest_info *)user_data);
}

static int ingestion_init(const harp_ingestion_module *module, coda_product *product,
                          const harp_ingestion_options *options, harp_product_definition **definition,
                          void **user_data, int is_mean)
{
    ingest_info *info;

    (void)options;

    if (ingest_info_new(product, &info) != 0)
    {
        return -1;
    }
    info->is_mean = is_mean;

    if (get_global_attributes(info) != 0)
    {
        ingest_info_delete(info);
        return -1;
    }

    *definition = *module->product_definition;
    *user_data = info;

    return 0;
}

static int ingestion_init_o3field(const harp_ingestion_module *module, coda_product *product,
                                  const harp_ingestion_options *options, harp_product_definition **definition,
                                  void **user_data)
{
    return ingestion_init(module, product, options, definition, user_data, 0);
}

static int ingestion_init_o3mean(const harp_ingestion_module *module, coda_product *product,
                                 const harp_ingestion_options *options, harp_product_definition **definition,
                                 void **user_data)
{
    return ingestion_init(module, product, options, definition, user_data, 1);
}

static void register_variables(harp_product_definition *product_definition, const char *column_path,
                               const char *std_path)
{
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[3] = { harp_dimension_time, harp_dimension_latitude, harp_dimension_longitude };
    const char *description;
    const char *path;

    /* datetime */
    description = "Time of the field";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double,
                                                                     1, dimension_type, NULL, description,
                                                                     "days since 2000-01-01", NULL, read_datetime);
    path = "/@Ozone_field_date";
    description = "interpret the attribute array as [year, month, day, hour, minute, second]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* latitude */
    description = "latitude of the grid cell mid-point (WGS84)";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "latitude",
                                                                     harp_type_double, 1, &dimension_type[1], NULL,
                                                                     description, "degree_north", NULL, read_latitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0, 180.0);
    path = "/@Latitude_Range, /@Latitude_Step";
    description = "Linear array from Latitude_Range[0] to Latitude_Range[1], using Latitude_Step steps";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* longitude */
    description = "longitude of the grid cell mid-point (WGS84)";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "longitude",
                                                                     harp_type_double, 1, &dimension_type[2], NULL,
                                                                     description, "degree_east", NULL, read_longitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0, 180.0);
    path = "/@Longitude_Range, /@Longitude_Step";
    description = "Linear array from Longitude_Range[0] to Longitude_Range[1], using Longitude_Step steps";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* O3_column_number_density */
    description = "O3 column number density";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "O3_column_number_density",
                                                                     harp_type_double, 2, &dimension_type[1], NULL,
                                                                     description, "DU", NULL, read_o3_column);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, column_path, NULL);

    /* O3_column_number_density_uncertainty */
    description = "O3 column number density uncertainty";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition,
                                                                     "O3_column_number_density_uncertainty",
                                                                     harp_type_double, 2, &dimension_type[1], NULL,
                                                                     description, "DU", NULL, read_o3_std);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, std_path, NULL);
}

int harp_ingestion_module_temis_init(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;

    /* o3field product */
    module = harp_ingestion_register_module_coda("TEMIS_o3field", "TEMIS", "TEMIS", "o3field",
                                                 "Assimilated Ozone Field", ingestion_init_o3field, ingestion_done);
    product_definition = harp_ingestion_register_product(module, "TEMIS_o3field", NULL, read_dimensions);
    register_variables(product_definition, "O3_column", "O3_std");

    /* o3mean product */
    module = harp_ingestion_register_module_coda("TEMIS_o3mean", "TEMIS", "TEMIS", "o3mean",
                                                 "Monthly Mean Ozone", ingestion_init_o3mean, ingestion_done);
    product_definition = harp_ingestion_register_product(module, "TEMIS_o3mean", NULL, read_dimensions);
    register_variables(product_definition, "Average_O3_column", "Average_O3_std");

    return 0;
}
