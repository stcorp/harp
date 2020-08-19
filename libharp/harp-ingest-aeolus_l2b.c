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

#define MAX_PATH_LENGTH 256

typedef struct ingest_info_struct
{
    coda_product *product;
    int rayleigh;       /* 1 for rayleigh data, 0 for mie data */
    int num_hloswind;
    int num_profiles;
    coda_cursor *geolocation_cursor;
    coda_cursor *hloswind_cursor;
    coda_cursor *profile_cursor;
} ingest_info;


static int read_dimensions(void *user_data, long dimension[HARP_NUM_DIM_TYPES])
{
    dimension[harp_dimension_time] = ((ingest_info *)user_data)->num_profiles;
    dimension[harp_dimension_vertical] = 24;
    return 0;
}

static int get_int8_profile(coda_cursor profile_cursor, coda_cursor *result_cursor, const char *path, harp_array data)
{
    uint32_t result_id[24];
    int index = 0;
    int i;

    if (coda_cursor_goto(&profile_cursor, "l2b_wind_profiles/wind_result_id_number") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_uint32_array(&profile_cursor, result_id, coda_array_ordering_c) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    for (i = 0; i < 24; i++)
    {
        /* invert the result index since data is stored from top to bottom */
        if (result_id[23 - i] != 0)
        {
            coda_cursor cursor = result_cursor[result_id[23 - i] - 1];

            if (coda_cursor_goto(&cursor, path) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            if (coda_cursor_read_uint8(&cursor, (uint8_t *)&data.int8_data[index]) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            index++;
        }
    }
    for (i = index; i < 24; i++)
    {
        data.int8_data[i] = 0;
    }

    return 0;
}

static int get_double_value(coda_cursor cursor, const char *field_name, harp_array data)
{
    if (coda_cursor_goto_record_field_by_name(&cursor, field_name) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_double(&cursor, data.double_data) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    return 0;
}

static int get_double_profile(coda_cursor profile_cursor, coda_cursor *result_cursor, const char *path, harp_array data)
{
    uint32_t result_id[24];
    int index = 0;
    int i;

    if (coda_cursor_goto(&profile_cursor, "l2b_wind_profiles/wind_result_id_number") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_uint32_array(&profile_cursor, result_id, coda_array_ordering_c) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    for (i = 0; i < 24; i++)
    {
        /* invert the result index since data is stored from top to bottom */
        if (result_id[23 - i] != 0)
        {
            coda_cursor cursor = result_cursor[result_id[23 - i] - 1];

            if (coda_cursor_goto(&cursor, path) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            if (coda_cursor_read_double(&cursor, &data.double_data[index]) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            index++;
        }
    }
    for (i = index; i < 24; i++)
    {
        data.double_data[i] = harp_nan();
    }

    return 0;
}

static int init_cursors(ingest_info *info)
{
    coda_cursor cursor;
    long num_elements;
    int i;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, info->rayleigh ? "/rayleigh_profile" : "/mie_profile") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &num_elements) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    info->num_profiles = num_elements;

    info->profile_cursor = malloc(info->num_profiles * sizeof(coda_cursor));
    if (info->profile_cursor == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       info->num_profiles * sizeof(coda_cursor), __FILE__, __LINE__);
        return -1;
    }
    if (coda_cursor_goto_first_array_element(&cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    for (i = 0; i < info->num_profiles; i++)
    {
        info->profile_cursor[i] = cursor;
        if (i < info->num_profiles - 1)
        {
            if (coda_cursor_goto_next_array_element(&cursor) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
        }
    }

    if (coda_cursor_goto(&cursor, info->rayleigh ? "/rayleigh_hloswind" : "/mie_hloswind") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &num_elements) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    info->num_hloswind = num_elements;

    info->hloswind_cursor = malloc(info->num_hloswind * sizeof(coda_cursor));
    if (info->hloswind_cursor == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       info->num_hloswind * sizeof(coda_cursor), __FILE__, __LINE__);
        return -1;
    }
    if (coda_cursor_goto_first_array_element(&cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    for (i = 0; i < info->num_hloswind; i++)
    {
        info->hloswind_cursor[i] = cursor;
        if (i < info->num_hloswind - 1)
        {
            if (coda_cursor_goto_next_array_element(&cursor) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
        }
    }

    if (coda_cursor_goto(&cursor, info->rayleigh ? "/rayleigh_geolocation" : "/mie_geolocation") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &num_elements) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (num_elements != info->num_hloswind)
    {
        harp_set_error(HARP_ERROR_INGESTION, "product error detected (geolocation data set contains %ld records, "
                       "but expected %d (= number of hloswind)", num_elements, info->num_hloswind);
        return -1;
    }

    info->geolocation_cursor = malloc(info->num_hloswind * sizeof(coda_cursor));
    if (info->geolocation_cursor == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       info->num_hloswind * sizeof(coda_cursor), __FILE__, __LINE__);
        return -1;
    }
    if (coda_cursor_goto_first_array_element(&cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    for (i = 0; i < info->num_hloswind; i++)
    {
        info->geolocation_cursor[i] = cursor;
        if (i < info->num_hloswind - 1)
        {
            if (coda_cursor_goto_next_array_element(&cursor) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
        }
    }

    return 0;
}

static int read_datetime(void *user_data, long index, harp_array data)
{
    return get_double_value(((ingest_info *)user_data)->profile_cursor[index], "profile_datetime_average", data);
}

static int read_orbit_index(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    coda_cursor cursor;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, "/mph/abs_orbit") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    /* we hard cast the unsigned integer to signed (we don't expect orbit numbers > 2^31) */
    if (coda_cursor_read_uint32(&cursor, (uint32_t *)data.int32_data) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    return 0;
}

static int read_latitude(void *user_data, long index, harp_array data)
{
    return get_double_value(((ingest_info *)user_data)->profile_cursor[index], "profile_lat_average", data);
}

static int read_longitude(void *user_data, long index, harp_array data)
{
    return get_double_value(((ingest_info *)user_data)->profile_cursor[index], "profile_lon_average", data);
}

static int read_altitude(void *user_data, long index, harp_array data)
{
    return get_double_profile(((ingest_info *)user_data)->profile_cursor[index],
                              ((ingest_info *)user_data)->geolocation_cursor, "windresult_geolocation/altitude_vcog",
                              data);
}

static int read_altitude_bounds(void *user_data, long index, harp_array data)
{
    harp_array top_data;
    double values[24];
    int i;

    if (get_double_profile(((ingest_info *)user_data)->profile_cursor[index],
                           ((ingest_info *)user_data)->geolocation_cursor, "windresult_geolocation/altitude_bottom",
                           data) != 0)
    {
        return -1;
    }
    top_data.double_data = values;
    if (get_double_profile(((ingest_info *)user_data)->profile_cursor[index],
                           ((ingest_info *)user_data)->geolocation_cursor, "windresult_geolocation/altitude_top",
                           top_data) != 0)
    {
        return -1;
    }
    for (i = 23; i >= 0; i--)
    {
        data.double_data[2 * i] = data.double_data[i];
        data.double_data[2 * i + 1] = top_data.double_data[i];
    }

    return 0;
}

static int read_wind_velocity(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char path[MAX_PATH_LENGTH];

    snprintf(path, MAX_PATH_LENGTH, "windresult/%s_wind_velocity", info->rayleigh ? "rayleigh" : "mie");
    return get_double_profile(info->profile_cursor[index], info->hloswind_cursor, path, data);
}

static int read_wind_velocity_validity(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char path[MAX_PATH_LENGTH];

    snprintf(path, MAX_PATH_LENGTH, "windresult/validity_flag");
    return get_int8_profile(info->profile_cursor[index], info->hloswind_cursor, path, data);
}

static void ingestion_done(void *user_data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->geolocation_cursor != NULL)
    {
        free(info->geolocation_cursor);
    }
    if (info->hloswind_cursor != NULL)
    {
        free(info->hloswind_cursor);
    }
    if (info->profile_cursor != NULL)
    {
        free(info->profile_cursor);
    }

    free(info);
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
    info->product = product;
    info->rayleigh = 1;
    info->geolocation_cursor = NULL;
    info->hloswind_cursor = NULL;
    info->profile_cursor = NULL;

    if (harp_ingestion_options_has_option(options, "data"))
    {
        if (harp_ingestion_options_get_option(options, "data", &option_value) != 0)
        {
            ingestion_done(info);
            return -1;
        }
        if (strcmp(option_value, "mie") == 0)
        {
            info->rayleigh = 0;
        }
        /* nothing to do for rayleigh, since it is the default */
    }

    if (init_cursors(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }

    *definition = *module->product_definition;
    *user_data = info;

    return 0;
}

static void register_common_variables(harp_product_definition *product_definition, int rayleigh)
{
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[3];
    const char *description;
    char path[MAX_PATH_LENGTH];
    long dimension[3];

    dimension_type[0] = harp_dimension_time;
    dimension_type[1] = harp_dimension_vertical;
    dimension_type[2] = harp_dimension_independent;

    /* for altitude bounds */
    dimension[0] = -1;
    dimension[1] = -1;
    dimension[2] = 2;

    /* datetime */
    description = "average datetime of the measurements used for the wind profile";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "datetime_start",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, "seconds since 2000-01-01",
                                                                      NULL, read_datetime);
    snprintf(path, MAX_PATH_LENGTH, "/%s_profile[]/profile_datetime_average", rayleigh ? "rayleigh" : "mie");
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* orbit_index */
    description = "absolute orbit number";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "orbit_index", harp_type_int32, 0, NULL, NULL,
                                                   description, NULL, NULL, read_orbit_index);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/mph/abs_orbit", NULL);

    /* latitude */
    description = "average latitude of the measurements used for the wind profile";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "latitude",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, "degree_north", NULL, read_latitude);
    snprintf(path, MAX_PATH_LENGTH, "/%s_profile[]/profile_lat_average", rayleigh ? "rayleigh" : "mie");
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* longitude */
    description = "average longitude of the measurements used for the wind profile";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "longitude",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, "degree_east", NULL, read_longitude);
    snprintf(path, MAX_PATH_LENGTH, "/%s_profile[]/profile_lon_average", rayleigh ? "rayleigh" : "mie");
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* altitude */
    description = "vertical COG altitude relative to geoid for each accumulation";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "altitude",
                                                                      harp_type_double, 2, dimension_type, dimension,
                                                                      description, "m", NULL, read_altitude);
    snprintf(path, MAX_PATH_LENGTH, "/%s_geolocation[/%s_profile/l2b_wind_profies/wind_result_id_number - 1]/"
             "windresult_geolocation/altitude_vcog", rayleigh ? "rayleigh" : "mie", rayleigh ? "rayleigh" : "mie");
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* altitude_bounds */
    description = "altitude relative to geoid of layer boundaries for each accumulation";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "altitude_bounds",
                                                                      harp_type_double, 3, dimension_type, dimension,
                                                                      description, "m", NULL, read_altitude_bounds);
    snprintf(path, MAX_PATH_LENGTH, "/%s_geolocation[/%s_profile/l2b_wind_profies/wind_result_id_number - 1]"
             "/windresult_geolocation/altitude_bottom, /%s_geolocation[/%s_profile/l2b_wind_profies/"
             "wind_result_id_number - 1]/windresult_geolocation/altitude_top", rayleigh ? "rayleigh" : "mie",
             rayleigh ? "rayleigh" : "mie", rayleigh ? "rayleigh" : "mie", rayleigh ? "rayleigh" : "mie");
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* hlos_wind_velocity */
    description = "HLOS wind velocity";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "hlos_wind_velocity",
                                                                      harp_type_double, 2, dimension_type, NULL,
                                                                      description, "cm/s", NULL, read_wind_velocity);
    snprintf(path, MAX_PATH_LENGTH, "/%s_hloswind[/%s_profile/l2b_wind_profies/wind_result_id_number - 1]"
             "/windresult/%s_wind_velocity", rayleigh ? "rayleigh" : "mie", rayleigh ? "rayleigh" : "mie",
             rayleigh ? "rayleigh" : "mie");
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* hlos_wind_velocity_validity */
    description = "validity flag of the HLOS wind velocity";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "hlos_wind_velocity_validity", harp_type_int8,
                                                                      2, dimension_type, NULL, description, NULL,
                                                                      NULL, read_wind_velocity_validity);
    snprintf(path, MAX_PATH_LENGTH, "/%s_hloswind[/%s_profile/l2b_wind_profies/wind_result_id_number - 1]"
             "/windresult/validity_flag", rayleigh ? "rayleigh" : "mie", rayleigh ? "rayleigh" : "mie");
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

int harp_ingestion_module_aeolus_l2b_init(void)
{
    const char *dataset_options[] = { "rayleigh", "mie" };
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    const char *description;

    description = "AEOLUS Level 2B Product";
    module = harp_ingestion_register_module("AEOLUS_L2B", "AEOLUS", "AEOLUS", "ALD_U_N_2B", description,
                                                 ingestion_init, ingestion_done);
    harp_ingestion_register_option(module, "data", "the type of profiles to ingest; option values are 'rayleigh' "
                                   "(default), 'mie'", 2, dataset_options);

    description = "Rayleigh HLOS wind profile";
    product_definition = harp_ingestion_register_product(module, "AEOLUS_L2B_Rayleigh", description, read_dimensions);
    harp_product_definition_add_mapping(product_definition, NULL, "data=rayleigh or data unset");
    register_common_variables(product_definition, 1);

    description = "Mie HLOS wind profile";
    product_definition = harp_ingestion_register_product(module, "AEOLUS_L2B_Mie", description, read_dimensions);
    harp_product_definition_add_mapping(product_definition, NULL, "data=mie");
    register_common_variables(product_definition, 0);

    return 0;
}
