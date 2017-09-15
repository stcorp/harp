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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ------------------- Defines ------------------ */

#define FILL_VALUE_NO_DATA              -999.0

#define SECONDS_FROM_1970_TO_2000    946684800
#define M_TO_KM                          0.001

#define CHECKED_MALLOC(v, s) v = malloc(s); if (v == NULL) { harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)", s, __FILE__, __LINE__); return -1;}

/* ------------------ Typedefs ------------------ */

typedef struct ingest_info_struct
{
    coda_product *product;
    long num_times;
    long num_altitudes;
    double *values_buffer;
} ingest_info;

/* -------------- Global variables --------------- */

static double nan;

/* -------------------- Code -------------------- */

static void ingestion_done(void *user_data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info != NULL)
    {
        if (info->values_buffer != NULL)
        {
            free(info->values_buffer);
        }
        free(info);
    }
}

/* General read functions */

static int read_scalar_attribute(ingest_info *info, const char *name, harp_data_type type, harp_array data)
{
    coda_cursor cursor;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_attributes(&cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&cursor, name) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (type == harp_type_double)
    {
        if (coda_cursor_read_double(&cursor, data.double_data) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
    }
    else if (type == harp_type_int32)
    {
        if (coda_cursor_read_int32(&cursor, data.int32_data) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
    }

    return 0;
}

static int read_array_variable(ingest_info *info, const char *name, harp_array data, short *unit_is_percent)
{
    coda_cursor cursor;
    double *double_data;
    long num_elements, l;
    char units[81];

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
    if (coda_cursor_read_double_array(&cursor, data.double_data, coda_array_ordering_c) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &num_elements) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    double_data = data.double_data;
    for (l = 0; l < num_elements; l++)
    {
        if (*double_data == FILL_VALUE_NO_DATA)
        {
            *double_data = nan;
        }
        double_data++;
    }
    if (unit_is_percent != NULL)
    {
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
        if (coda_cursor_read_string(&cursor, units, sizeof(units)) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        *unit_is_percent = (strstr(units, "percent") != NULL);
    }

    return 0;
}

/* Specific read functions */

static int read_latitude(void *user_data, harp_array data)
{
    return read_scalar_attribute((ingest_info *)user_data, "Latitude_degrees_north", harp_type_double, data);
}

static int read_longitude(void *user_data, harp_array data)
{
    return read_scalar_attribute((ingest_info *)user_data, "Longitude_degrees_east", harp_type_double, data);
}

static int read_sensor_altitude(void *user_data, harp_array data)
{
    if (read_scalar_attribute((ingest_info *)user_data, "Altitude_meter_asl", harp_type_double, data) != 0)
    {
        return -1;
    }

    /* Convert from m to km */
    *(data.double_data) *= M_TO_KM;
    return 0;
}

static int read_sensor_zenith_angle(void *user_data, harp_array data)
{
    return read_scalar_attribute((ingest_info *)user_data, "ZenithAngle_degrees", harp_type_double, data);
}

static int read_datetime(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    harp_array temp_array;
    double *double_data;
    double datetime;
    long i;
    int32_t start_date, start_time;
    int year, month, day;

    if (read_array_variable(info, "Time", data, NULL) != 0)
    {
        temp_array.int32_data = &start_date;
        if (read_scalar_attribute(info, "StartDate", harp_type_int32, temp_array) != 0)
        {
            return -1;
        }
        temp_array.int32_data = &start_time;
        if (read_scalar_attribute(info, "StartTime_UT", harp_type_int32, temp_array) != 0)
        {
            return -1;
        }
        year = start_date / 10000;
        month = (start_date - (10000 * year)) / 100;
        day = start_date - (10000 * year) - (100 * month);
        coda_time_parts_to_double(year, month, day, 0, 0, 0, 0, &datetime);
        *(data.double_data) = (datetime + (double)start_time);
        return 0;
    }

    double_data = data.double_data;
    for (i = 0; i < info->num_times; i++)
    {
        *double_data = *double_data - SECONDS_FROM_1970_TO_2000;
        double_data++;
    }
    return 0;
}

static int read_altitude(void *user_data, harp_array data)
{
    double *double_data;
    ingest_info *info = (ingest_info *)user_data;
    long i;

    if (read_array_variable(info, "Altitude", data, NULL) != 0)
    {
        return -1;
    }

    double_data = data.double_data;
    for (i = 0; i < info->num_altitudes; i++)
    {
        *double_data = *double_data * M_TO_KM;
        double_data++;
    }
    return 0;
}

static int read_backscatter(void *user_data, harp_array data)
{
    return read_array_variable((ingest_info *)user_data, "Backscatter", data, NULL);
}

static int read_backscatter_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    harp_array backscatter_values;
    double *value, *uncertainty;
    long l;
    short units_is_percent;

    if (read_array_variable(info, "ErrorBackscatter", data, &units_is_percent) != 0)
    {
        return -1;
    }
    if (units_is_percent)
    {
        backscatter_values.double_data = info->values_buffer;
        if (read_array_variable(info, "Backscatter", backscatter_values, NULL) != 0)
        {
            return -1;
        }
        value = backscatter_values.double_data;
        uncertainty = data.double_data;
        for (l = 0; l < (info->num_times * info->num_altitudes); l++)
        {
            if (harp_isnan(*value) || harp_isnan(*uncertainty))
            {
                *uncertainty = nan;
            }
            else
            {
                /* Calculate from the uncertainty as a percentage the uncertainty as a backscatter value */
                *uncertainty = (*value * *uncertainty / 100.0);
            }
            value++;
            uncertainty++;
        }
    }
    return 0;
}

static int read_extinction(void *user_data, harp_array data)
{
    return read_array_variable((ingest_info *)user_data, "Extinction", data, NULL);
}

static int read_extinction_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    harp_array extinction_values;
    double *value, *uncertainty;
    long l;
    short units_is_percent;

    if (read_array_variable(info, "ErrorExtinction", data, &units_is_percent) != 0)
    {
        return -1;
    }
    if (units_is_percent)
    {
        extinction_values.double_data = info->values_buffer;
        if (read_array_variable(info, "Extinction", extinction_values, NULL) != 0)
        {
            return -1;
        }
        value = extinction_values.double_data;
        uncertainty = data.double_data;
        for (l = 0; l < (info->num_times * info->num_altitudes); l++)
        {
            if (harp_isnan(*value) || harp_isnan(*uncertainty))
            {
                *uncertainty = nan;
            }
            else
            {
                /* Calculate from the uncertainty as a percentage the uncertainty as an extinction value */
                *uncertainty = (*value * *uncertainty / 100.0);
            }
            value++;
            uncertainty++;
        }
    }
    return 0;
}

static int read_h2o_mass_mixing_ratio(void *user_data, harp_array data)
{
    return read_array_variable((ingest_info *)user_data, "WaterVaporMixingRatio", data, NULL);
}

static int read_h2o_mass_mixing_ratio_uncertainty(void *user_data, harp_array data)
{
    return read_array_variable((ingest_info *)user_data, "ErrorWaterVapor", data, NULL);
}

/* Exclude functions */

static int exclude_field_if_not_existing(void *user_data, const char *field_name)
{
    coda_cursor cursor;
    ingest_info *info = (ingest_info *)user_data;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&cursor, field_name) != 0)
    {
        return 1;
    }
    return 0;
}

static int exclude_extinction(void *user_data)
{
    return exclude_field_if_not_existing(user_data, "Extinction");
}

static int exclude_extinction_uncertainty(void *user_data)
{
    return exclude_field_if_not_existing(user_data, "ErrorExtinction");
}

static int exclude_h2o_mass_mixing_ratio(void *user_data)
{
    return exclude_field_if_not_existing(user_data, "WaterVaporMixingRatio");
}

static int exclude_h2o_mass_mixing_ratio_uncertainty(void *user_data)
{
    return exclude_field_if_not_existing(user_data, "ErrorWaterVapor");
}

/* General functions to define fields and dimensions */

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

    if (coda_cursor_goto(&cursor, "Time") != 0)
    {
        /* This is a single profile file (i.e. all measurements are taken at one time) */
        info->num_times = 1;
    }
    else if (coda_cursor_get_array_dim(&cursor, &num_coda_dimensions, coda_dimension) != 0)
    {
        /* This productfile does not contain data */
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    else
    {
        info->num_times = coda_dimension[0];
    }

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, "Altitude") != 0)
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

static int ingestion_init(const harp_ingestion_module *module, coda_product *product,
                          const harp_ingestion_options *options, harp_product_definition **definition, void **user_data)
{
    ingest_info *info;

    (void)options;

    nan = harp_nan();
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

    CHECKED_MALLOC(info->values_buffer, info->num_times * info->num_altitudes * sizeof(double));

    *definition = *module->product_definition;
    *user_data = info;

    return 0;
}

int harp_ingestion_module_earlinet_l2_aerosol_init(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_vertical };
    const char *description;
    const char *path;

    module =
        harp_ingestion_register_module_coda("EARLINET", "EARLINET", "EARLINET", "EARLINET",
                                            "EARLINET aerosol backscatter and extinction profiles", ingestion_init,
                                            ingestion_done);
    product_definition = harp_ingestion_register_product(module, "EARLINET", NULL, read_dimensions);

    /* latitude */
    description = "latitude";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_double, 0, dimension_type,
                                                   NULL, description, "degrees", NULL, read_latitude);
    path = "/@Latitude_degrees_north";
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* longitude */
    description = "longitude";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_double, 0, dimension_type,
                                                   NULL, description, "degrees", NULL, read_longitude);
    path = "/@Longitude_degrees_east";
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0, 180.0);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* sensor_altitude */
    description = "sensor altitude";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "sensor_altitude", harp_type_double, 0,
                                                   dimension_type, NULL, description, "km", NULL, read_sensor_altitude);
    path = "/@Altitude_meter_asl";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* sensor_zenith_angle */
    description = "sensor zenith angle";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "sensor_zenith_angle", harp_type_double, 0,
                                                   dimension_type, NULL, description, "degrees", NULL,
                                                   read_sensor_zenith_angle);
    path = "/@ZenithAngle_degrees";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* datetime */
    description = "date and time";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double, 1, dimension_type,
                                                   NULL, description, "seconds since 2000-01-01", NULL, read_datetime);
    path = "/Time";
    description = "seconds sinds 1970-01-01 00:00:00";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* altitude */
    description = "altitude";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "altitude", harp_type_double, 1,
                                                   &(dimension_type[1]), NULL, description, "km", NULL, read_altitude);
    path = "/Altitude";
    description = "height above sea level";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* backscatter_coefficient */
    description = "backscatter coefficient";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "backscatter_coefficient", harp_type_double, 2,
                                                   dimension_type, NULL, description, "1/(m*sr)", NULL,
                                                   read_backscatter);
    path = "/Backscatter";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* backscatter_coefficient_uncertainty */
    description = "backscatter coefficient uncertainty";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "backscatter_coefficient_uncertainty",
                                                   harp_type_double, 2, dimension_type, NULL, description, "1/(m*sr)",
                                                   NULL, read_backscatter_uncertainty);
    path = "/ErrorBackscatter";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* extinction_coefficient */
    description = "extinction coefficient";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "extinction_coefficient", harp_type_double, 2,
                                                   dimension_type, NULL, description, "1/m", exclude_extinction,
                                                   read_extinction);
    path = "/Extinction";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* extinction_coefficient_uncertainty */
    description = "extinction coefficient uncertainty";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "extinction_coefficient_uncertainty",
                                                   harp_type_double, 2, dimension_type, NULL, description, "1/m",
                                                   exclude_extinction_uncertainty, read_extinction_uncertainty);
    path = "/ErrorExtinction";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* H2O_mass_mixing_ratio */
    description = "water mass mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "H2O_mass_mixing_ratio", harp_type_double, 2,
                                                   dimension_type, NULL, description, "g/kg",
                                                   exclude_h2o_mass_mixing_ratio, read_h2o_mass_mixing_ratio);
    path = "/WaterVaporMixingRatio";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* H2O_mass_mixing_ratio_uncertainty */
    description = "water mass mixing ratio uncertainty";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "H2O_mass_mixing_ratio_uncertainty",
                                                   harp_type_double, 2, dimension_type, NULL, description, "g/kg",
                                                   exclude_h2o_mass_mixing_ratio_uncertainty,
                                                   read_h2o_mass_mixing_ratio_uncertainty);
    path = "/ErrorWaterVapor";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    return 0;
}
