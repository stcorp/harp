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

#define MAX_PATH_LENGTH 256

typedef struct ingest_info_struct
{
    coda_product *product;
    int rayleigh;       /* 1 for rayleigh data, 0 for mie data */
    int observation;    /* 1 for observation, 0 for measurement */
    int32_t num_obs;
    int32_t n_max;
    int32_t n_max_actual;
    int num_profiles;   /* either 'num_obs' or 'num_obs * n_max_actual' */
    double *time;
    coda_cursor *geo_bin_cursor;
    coda_cursor *wv_bin_cursor;
} ingest_info;


static int read_dimensions(void *user_data, long dimension[HARP_NUM_DIM_TYPES])
{
    dimension[harp_dimension_time] = ((ingest_info *)user_data)->num_profiles;
    dimension[harp_dimension_vertical] = 24;
    return 0;
}

static int get_double_average_array(coda_cursor cursor, const char *field_name, harp_array data)
{
    int i;

    if (coda_cursor_goto_first_array_element(&cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    for (i = 0; i < 25; i++)
    {
        double value;

        if (coda_cursor_goto_record_field_by_name(&cursor, field_name) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_read_double(&cursor, &value) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        /* invert the index since data is stored from top to bottom */
        if (i < 24)
        {
            data.double_data[23 - i] = value;
        }
        if (i > 0)
        {
            data.double_data[24 - i] += value;
            data.double_data[24 - i] /= 2.0;
        }
        coda_cursor_goto_parent(&cursor);
        if (i < 24)
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

static int get_double_bounds_array(coda_cursor cursor, const char *field_name, harp_array data)
{
    int i;

    if (coda_cursor_goto_first_array_element(&cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    for (i = 0; i < 25; i++)
    {
        double value;

        if (coda_cursor_goto_record_field_by_name(&cursor, field_name) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_read_double(&cursor, &value) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        /* invert the index since data is stored from top to bottom */
        if (i < 24)
        {
            data.double_data[47 - i * 2] = value;
        }
        if (i > 0)
        {
            data.double_data[48 - i * 2] = value;
        }
        coda_cursor_goto_parent(&cursor);
        if (i < 24)
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

static int get_double_array_data(coda_cursor cursor, const char *field_name, harp_array data)
{
    int i;

    if (coda_cursor_goto_first_array_element(&cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    for (i = 0; i < 24; i++)
    {
        if (coda_cursor_goto_record_field_by_name(&cursor, field_name) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        /* invert the index since data is stored from top to bottom */
        if (coda_cursor_read_double(&cursor, &data.double_data[23 - i]) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        coda_cursor_goto_parent(&cursor);
        if (i < 23)
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

static int get_int32_array_data(coda_cursor cursor, const char *field_name, harp_array data)
{
    int i;

    if (coda_cursor_goto_first_array_element(&cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    for (i = 0; i < 24; i++)
    {
        if (coda_cursor_goto_record_field_by_name(&cursor, field_name) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        /* invert the index since data is stored from top to bottom */
        if (coda_cursor_read_int32(&cursor, &data.int32_data[23 - i]) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        coda_cursor_goto_parent(&cursor);
        if (i < 23)
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

static int init_sizes(ingest_info *info)
{
    coda_cursor cursor;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, "/sph/n_max") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_int32(&cursor, &info->n_max) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    coda_cursor_goto_parent(&cursor);
    if (coda_cursor_goto_record_field_by_name(&cursor, "n_max_actual") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_int32(&cursor, &info->n_max_actual) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    coda_cursor_goto_parent(&cursor);
    if (info->n_max_actual > info->n_max)
    {
        harp_set_error(HARP_ERROR_INGESTION, "product error detected (N_MAX_ACTUAL (%d) is larger than N_MAX (%d))",
                       (int)info->n_max_actual, (int)info->n_max);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&cursor, "total_num_of_observations") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_int32(&cursor, &info->num_obs) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    info->num_profiles = info->observation ? info->num_obs : info->num_obs * info->n_max_actual;

    return 0;
}

static int init_cursors(ingest_info *info)
{
    coda_cursor geo_cursor;
    coda_cursor hlw_cursor;
    long num_elements;
    int i;

    info->time = malloc(info->num_obs * sizeof(double));
    if (info->time == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       info->num_obs * sizeof(double), __FILE__, __LINE__);
        return -1;
    }
    info->geo_bin_cursor = malloc(info->num_profiles * sizeof(coda_cursor));
    if (info->geo_bin_cursor == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       info->num_profiles * sizeof(coda_cursor), __FILE__, __LINE__);
        return -1;
    }
    info->wv_bin_cursor = malloc(info->num_profiles * sizeof(coda_cursor));
    if (info->wv_bin_cursor == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       info->num_profiles * sizeof(coda_cursor), __FILE__, __LINE__);
        return -1;
    }

    if (coda_cursor_set_product(&geo_cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&geo_cursor, "geolocation") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&geo_cursor, &num_elements) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (num_elements != info->num_obs)
    {
        harp_set_error(HARP_ERROR_INGESTION, "product error detected (geolocation data set contains %ld records, "
                       "but expected %d (= number of BRC)", num_elements, (int)info->num_obs);
        return -1;
    }
    if (coda_cursor_set_product(&hlw_cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&hlw_cursor, "wind_velocity") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&hlw_cursor, &num_elements) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (num_elements != info->num_obs)
    {
        harp_set_error(HARP_ERROR_INGESTION, "product error detected (wind velocity data set contains %ld records, "
                       "but expected %d (= number of BRC)", num_elements, (int)info->num_obs);
        return -1;
    }

    if (coda_cursor_goto_first_array_element(&geo_cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_first_array_element(&hlw_cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    for (i = 0; i < info->num_obs; i++)
    {
        int j;

        if (coda_cursor_goto_record_field_by_name(&geo_cursor, "start_of_observation_time") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_read_double(&geo_cursor, &info->time[i]) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        coda_cursor_goto_parent(&geo_cursor);

        if (info->observation)
        {
            if (coda_cursor_goto_record_field_by_name(&geo_cursor, "observation_geolocation") != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            if (coda_cursor_goto_record_field_by_name(&geo_cursor, info->rayleigh ? "observation_rayleigh_geolocation" :
                                                      "observation_mie_geolocation") != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            info->geo_bin_cursor[i] = geo_cursor;
            coda_cursor_goto_parent(&geo_cursor);
            coda_cursor_goto_parent(&geo_cursor);
            if (coda_cursor_goto_record_field_by_name(&hlw_cursor, "observation_wind_profile") != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            if (coda_cursor_goto_record_field_by_name(&hlw_cursor, info->rayleigh ? "rayleigh_altitude_bin_wind_info" :
                                                      "mie_altitude_bin_wind_info") != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            info->wv_bin_cursor[i] = hlw_cursor;
            coda_cursor_goto_parent(&hlw_cursor);
            coda_cursor_goto_parent(&hlw_cursor);
        }
        else
        {
            if (coda_cursor_goto_record_field_by_name(&geo_cursor, "measurement_geolocation") != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            if (coda_cursor_goto_first_array_element(&geo_cursor) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            if (coda_cursor_goto_record_field_by_name(&hlw_cursor, "measurement_wind_profile") != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            if (coda_cursor_goto_first_array_element(&hlw_cursor) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            for (j = 0; j < info->n_max_actual; j++)
            {
                if (coda_cursor_goto_record_field_by_name(&geo_cursor, info->rayleigh ? "rayleigh_geolocation" :
                                                          "mie_geolocation") != 0)
                {
                    harp_set_error(HARP_ERROR_CODA, NULL);
                    return -1;
                }
                info->geo_bin_cursor[i * info->n_max_actual + j] = geo_cursor;
                coda_cursor_goto_parent(&geo_cursor);
                if (coda_cursor_goto_record_field_by_name(&hlw_cursor, info->rayleigh ?
                                                          "rayleigh_altitude_bin_wind_info" :
                                                          "mie_altitude_bin_wind_info") != 0)
                {
                    harp_set_error(HARP_ERROR_CODA, NULL);
                    return -1;
                }
                info->wv_bin_cursor[i * info->n_max_actual + j] = hlw_cursor;
                coda_cursor_goto_parent(&hlw_cursor);
                if (j < info->n_max_actual - 1)
                {
                    if (coda_cursor_goto_next_array_element(&geo_cursor) != 0)
                    {
                        harp_set_error(HARP_ERROR_CODA, NULL);
                        return -1;
                    }
                    if (coda_cursor_goto_next_array_element(&hlw_cursor) != 0)
                    {
                        harp_set_error(HARP_ERROR_CODA, NULL);
                        return -1;
                    }
                }
            }
            coda_cursor_goto_parent(&geo_cursor);
            coda_cursor_goto_parent(&geo_cursor);
            coda_cursor_goto_parent(&hlw_cursor);
            coda_cursor_goto_parent(&hlw_cursor);
        }

        if (i < info->num_obs - 1)
        {
            if (coda_cursor_goto_next_array_element(&geo_cursor) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            if (coda_cursor_goto_next_array_element(&hlw_cursor) != 0)
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
    ingest_info *info = (ingest_info *)user_data;

    if (info->observation)
    {
        *data.double_data = info->time[index];
    }
    else
    {
        *data.double_data = info->time[index / info->n_max_actual] +
            (index % info->n_max_actual) * (12.0 / info->n_max_actual);
    }

    return 0;
}

static int read_datetime_length(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    (void)index;

    *data.double_data = info->observation ? 12.0 : (12.0 / info->n_max_actual);

    return 0;
}

static int read_latitude(void *user_data, long index, harp_array data)
{
    return get_double_average_array(((ingest_info *)user_data)->geo_bin_cursor[index], "latitude_of_height_bin", data);
}

static int read_longitude(void *user_data, long index, harp_array data)
{
    return get_double_average_array(((ingest_info *)user_data)->geo_bin_cursor[index], "longitude_of_height_bin", data);
}

static int read_geoid_separation(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    coda_cursor cursor;

    cursor = ((ingest_info *)user_data)->geo_bin_cursor[index];
    coda_cursor_goto_parent(&cursor);
    if (!info->observation)
    {
        if (coda_cursor_goto(&cursor, "../../observation_geolocation") != 0)
        {
            return -1;
        }
    }
    if (coda_cursor_goto_record_field_by_name(&cursor, "geoid_separation") != 0)
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

static int read_altitude_bounds(void *user_data, long index, harp_array data)
{
    double geoid_separation;
    harp_array geoid_data;
    int i;

    geoid_data.double_data = &geoid_separation;
    if (read_geoid_separation(user_data, index, geoid_data) != 0)
    {
        return -1;
    }
    if (get_double_bounds_array(((ingest_info *)user_data)->geo_bin_cursor[index], "altitude_of_height_bin", data) != 0)
    {
        return -1;
    }

    for (i = 0; i < 2 * 24; i++)
    {
        data.double_data[i] -= geoid_separation;
    }

    return 0;
}

static int read_wind_velocity(void *user_data, long index, harp_array data)
{
    return get_double_array_data(((ingest_info *)user_data)->wv_bin_cursor[index], "wind_velocity", data);
}

static int read_wind_velocity_validity(void *user_data, long index, harp_array data)
{
    return get_int32_array_data(((ingest_info *)user_data)->wv_bin_cursor[index], "bin_quality_flag", data);
}

static void ingestion_done(void *user_data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->time != NULL)
    {
        free(info->time);
    }
    if (info->geo_bin_cursor != NULL)
    {
        free(info->geo_bin_cursor);
    }
    if (info->wv_bin_cursor != NULL)
    {
        free(info->wv_bin_cursor);
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
    info->observation = 1;
    info->time = NULL;
    info->geo_bin_cursor = NULL;
    info->wv_bin_cursor = NULL;

    if (harp_ingestion_options_has_option(options, "data"))
    {
        if (harp_ingestion_options_get_option(options, "data", &option_value) != 0)
        {
            ingestion_done(info);
            return -1;
        }
        if (strcmp(option_value, "rayleigh_measurement") == 0)
        {
            info->observation = 0;
        }
        else if (strcmp(option_value, "mie_measurement") == 0)
        {
            info->rayleigh = 0;
            info->observation = 0;
        }
        else if (strcmp(option_value, "mie_observation") == 0)
        {
            info->rayleigh = 0;
        }
        /* nothing to do for rayleigh_observation, since it is the default */
    }

    if (init_sizes(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }

    if (info->num_obs > 0 && info->n_max_actual > 0)
    {
        if (init_cursors(info) != 0)
        {
            ingestion_done(info);
            return -1;
        }
    }

    *definition = *module->product_definition;
    *user_data = info;

    return 0;
}

static void register_common_variables(harp_product_definition *product_definition, int rayleigh, int obs)
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


    /* datetime_start */
    if (obs)
    {
        description = "start time of the observation";
        variable_definition = harp_ingestion_register_variable_sample_read(product_definition, "datetime_start",
                                                                           harp_type_double, 1, dimension_type, NULL,
                                                                           description, "seconds since 2000-01-01",
                                                                           NULL, read_datetime);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                             "/geolocation[]/start_of_observation_time", NULL);
    }
    else
    {
        description = "start time of the measurement";
        variable_definition = harp_ingestion_register_variable_sample_read(product_definition, "datetime_start",
                                                                           harp_type_double, 1, dimension_type, NULL,
                                                                           description, "seconds since 2000-01-01",
                                                                           NULL, read_datetime);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                             "/geolocation[]/start_of_observation_time, /sph/n_max_actual",
                                             "start_of_observation_time + 12.0/n_max_actual * index within BRC");
    }

    /* datetime_length */
    if (obs)
    {
        description = "duration of the observation";
        variable_definition = harp_ingestion_register_variable_sample_read(product_definition, "datetime_length",
                                                                           harp_type_double, 1, dimension_type, NULL,
                                                                           description, "s", NULL,
                                                                           read_datetime_length);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, NULL, "set to fixed value of 12 seconds");
    }
    else
    {
        description = "duration of the measurement";
        variable_definition = harp_ingestion_register_variable_sample_read(product_definition, "datetime_length",
                                                                           harp_type_double, 1, dimension_type, NULL,
                                                                           description, "s", NULL,
                                                                           read_datetime_length);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/sph/n_max_actual",
                                             "set to 12.0/n_max_actual seconds");
    }

    /* latitude */
    description = "average of the latitudes of the edges of the height bin along the line-of-sight";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition, "latitude",
                                                                       harp_type_double, 2, dimension_type, NULL,
                                                                       description, "degree_north", NULL,
                                                                       read_latitude);
    snprintf(path, MAX_PATH_LENGTH, "/geolocation[]/%s_geolocation[]/%s%s_geolocation[]/latitude_of_height_bin",
             obs ? "observation" : "measurement", obs ? "observation_" : "", rayleigh ? "rayleigh" : "mie");
    description = "average of the value at the upper and lower edge of the height bin";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* longitude */
    description = "average of the longitude of the edges of the height bin along the line-of-sight";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition, "longitude",
                                                                       harp_type_double, 2, dimension_type, NULL,
                                                                       description, "degree_east", NULL,
                                                                       read_longitude);
    snprintf(path, MAX_PATH_LENGTH, "/geolocation[]/%s_geolocation[]/%s%s_geolocation[]/latitude_of_height_bin",
             obs ? "observation" : "measurement", obs ? "observation_" : "", rayleigh ? "rayleigh" : "mie");
    description = "average of the value at the upper and lower edge of the height bin";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* altitude_bounds */
    description = "altitude boundaries of the height bin along the line-of-sight; "
        "value is negative if below DEM surface";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition, "altitude_bounds",
                                                                       harp_type_double, 3, dimension_type, dimension,
                                                                       description, "m", NULL, read_altitude_bounds);
    snprintf(path, MAX_PATH_LENGTH, "/geolocation[]/%s_geolocation[]/%s%s_geolocation[]/altitude_of_height_bin, "
             "/geolocation[]/observation_geolocation/geoid_separation",
             obs ? "observation" : "measurement", obs ? "observation_" : "", rayleigh ? "rayleigh" : "mie");
    description = "actual altitude is the stored height vs. WGS84 - geoid_separation ";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* geoid_height */
    description = "Geoid separation";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition, "geoid_height",
                                                                       harp_type_double, 1, dimension_type, dimension,
                                                                       description, "m", NULL, read_geoid_separation);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/geolocation[]/observation_geolocation/geoid_separation", NULL);

    /* hlos_wind_velocity */
    description = "HLOS wind velocity at the altitude bin";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition, "hlos_wind_velocity",
                                                                       harp_type_double, 2, dimension_type, NULL,
                                                                       description, "m/s", NULL, read_wind_velocity);
    snprintf(path, MAX_PATH_LENGTH, "/wind_velocity[]/%s_wind_profile[]/%s_altitude_bin_wind_info[]/wind_velocity",
             obs ? "observation" : "measurement", rayleigh ? "rayleigh" : "mie");
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* hlos_wind_velocity_validity */
    description = "quality flag of the HLOS wind velocity at the altitude bin";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition,
                                                                       "hlos_wind_velocity_validity", harp_type_int32,
                                                                       2, dimension_type, NULL, description, NULL,
                                                                       NULL, read_wind_velocity_validity);
    snprintf(path, MAX_PATH_LENGTH, "/wind_velocity[]/%s_wind_profile[]/%s_altitude_bin_wind_info[]/bin_quality_flag",
             obs ? "observation" : "measurement", rayleigh ? "rayleigh" : "mie");
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

int harp_ingestion_module_aeolus_l1b_init(void)
{
    const char *dataset_options[] = {
        "rayleigh_measurement", "mie_measurement", "rayleigh_observation", "mie_observation"
    };
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    const char *description;

    description = "AEOLUS Level 1B Wind Measurement Product";
    module = harp_ingestion_register_module_coda("AEOLUS_L1B", "AEOLUS", "AEOLUS", "ALD_U_N_1B", description,
                                                 ingestion_init, ingestion_done);
    harp_ingestion_register_option(module, "data", "the type of wind profile to ingest (rayleigh/mie, "
                                   "observation/measurement)", 4, dataset_options);

    description = "Measurement Rayleigh HLOS wind profile";
    product_definition = harp_ingestion_register_product(module, "AEOLUS_L1B_Rayleigh", description, read_dimensions);
    harp_product_definition_add_mapping(product_definition, NULL, "data=rayleigh_measurement");
    register_common_variables(product_definition, 1, 0);

    description = "Measurement Mie HLOS wind profile";
    product_definition = harp_ingestion_register_product(module, "AEOLUS_L1B_Mie", description, read_dimensions);
    harp_product_definition_add_mapping(product_definition, NULL, "data=mie_measurement");
    register_common_variables(product_definition, 0, 0);

    description = "Observation Rayleigh HLOS wind profile";
    product_definition = harp_ingestion_register_product(module, "AEOLUS_L1B_Rayleigh_Observation", description,
                                                         read_dimensions);
    harp_product_definition_add_mapping(product_definition, NULL, "data=rayleigh_observation or data unset");
    register_common_variables(product_definition, 1, 1);

    description = "Observation Mie HLOS wind profile";
    product_definition =
        harp_ingestion_register_product(module, "AEOLUS_L1B_Mie_Observation", description, read_dimensions);
    harp_product_definition_add_mapping(product_definition, NULL, "data=mie_observation");
    register_common_variables(product_definition, 0, 1);

    return 0;
}
