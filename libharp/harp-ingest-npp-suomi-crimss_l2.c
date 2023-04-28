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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ------------------- Defines ------------------ */

#define CHECKED_MALLOC(v, s) v = malloc(s); if (v == NULL) { harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)", s, __FILE__, __LINE__); return -1;}

#define MICROSECONDS_IN_SECOND                 1000000
#define SECONDS_FROM_1958_TO_2000           1325376000

#define DATASET_MOISTURE_PROFILE                     0
#define DATASET_TEMPERATURE_PROFILE                  1
#define DATASET_PRESSURE_PROFILE                     2

/* ------------------ Typedefs ------------------ */

typedef struct ingest_info_struct
{
    coda_product *product;
    short dataset;
    coda_cursor geo_cursor;
    coda_cursor data_cursor;
    const char *data_field_name;
    const char *axis_field_name;
    long num_granules;
    long num_scans;
    long num_retrievals_per_scan;
    long num_vertical;
} ingest_info;

/* -------------- Global variables --------------- */

static double nan;

/* -------------------- Code -------------------- */

static void ingestion_done(void *user_data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info != NULL)
    {
        free(info);
    }
}

static int read_variable(coda_cursor *cursor, const char *name, int num_dimensions, long dimension_0, long dimension_1,
                         double error_range_start, double error_range_end, harp_array data)
{
    double *double_data;
    long num_elements, i;
    long coda_dimension[CODA_MAX_NUM_DIMS];
    int num_coda_dimensions;

    if (coda_cursor_goto_record_field_by_name(cursor, name) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_array_dim(cursor, &num_coda_dimensions, coda_dimension) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (num_coda_dimensions != num_dimensions)
    {
        harp_set_error(HARP_ERROR_INGESTION,
                       "product error detected in NPP Suomi L2 product (variable %s has %d dimensions, " "expected %d)",
                       name, num_coda_dimensions, num_dimensions);
        return -1;
    }
    if (dimension_0 != coda_dimension[0])
    {
        harp_set_error(HARP_ERROR_INGESTION,
                       "product error detected in NPP Suomi L2 product (first dimension for variable %s "
                       "has %ld elements, expected %ld", name, coda_dimension[0], dimension_0);
        return -1;
    }
    num_elements = coda_dimension[0];
    if (num_dimensions > 1)
    {
        if (dimension_1 != coda_dimension[1])
        {
            harp_set_error(HARP_ERROR_INGESTION, "product error detected in NPP Suomi L2 product (second dimension for "
                           "variable %s has %ld elements, expected %ld", name, coda_dimension[1], dimension_1);
            return -1;
        }
        num_elements *= coda_dimension[1];
    }
    if (coda_cursor_read_double_array(cursor, data.double_data, coda_array_ordering_c) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    if (error_range_start <= error_range_end)
    {
        double_data = data.double_data;
        for (i = 0; i < num_elements; i++)
        {
            if ((*double_data >= error_range_start) && (*double_data <= error_range_end))
            {
                *double_data = nan;
            }
            double_data++;
        }
    }

    coda_cursor_goto_parent(cursor);

    return 0;
}

static int read_dimensions(void *user_data, long dimension[HARP_NUM_DIM_TYPES])
{
    ingest_info *info = (ingest_info *)user_data;

    dimension[harp_dimension_time] = info->num_scans * info->num_retrievals_per_scan;
    dimension[harp_dimension_vertical] = info->num_vertical;

    return 0;
}

static int read_datetime(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    harp_array start_times, middle_times;
    double *double_data, timestep, curtime;
    long i, j;

    CHECKED_MALLOC(start_times.double_data, info->num_scans * sizeof(double));
    if (read_variable(&info->geo_cursor, "StartTime", 1, info->num_scans, 0, -999.5, -992.5, start_times) != 0)
    {
        free(start_times.double_data);
        return -1;
    }
    CHECKED_MALLOC(middle_times.double_data, info->num_scans * sizeof(double));
    if (read_variable(&info->geo_cursor, "MidTime", 1, info->num_scans, 0, -999.5, -992.5, middle_times) != 0)
    {
        free(middle_times.double_data);
        free(start_times.double_data);
        return -1;
    }

    double_data = data.double_data;
    for (i = 0; i < info->num_scans; i++)
    {
        if (!coda_isNaN(start_times.double_data[i]) && !coda_isNaN(middle_times.double_data[i]))
        {
            timestep =
                (2.0 * (middle_times.double_data[i] - start_times.double_data[i]) /
                 (info->num_retrievals_per_scan * MICROSECONDS_IN_SECOND));
            curtime = (start_times.double_data[i] / MICROSECONDS_IN_SECOND) - SECONDS_FROM_1958_TO_2000;
            for (j = 0; j < info->num_retrievals_per_scan; j++)
            {
                *double_data = curtime;
                double_data++;
                curtime += timestep;
            }
        }
        else
        {
            for (j = 0; j < info->num_retrievals_per_scan; j++)
            {
                *double_data = nan;
                double_data++;
            }
        }
    }

    free(middle_times.double_data);
    free(start_times.double_data);
    return 0;
}

static int read_latitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable(&info->geo_cursor, "Latitude", 1, info->num_scans * info->num_retrievals_per_scan, 0, -1000.0,
                         -999.0, data);
}

static int read_longitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable(&info->geo_cursor, "Longitude", 1, info->num_scans * info->num_retrievals_per_scan, 0, -1000.0,
                         -999.0, data);
}

static int read_sensor_azimuth_angle(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable(&info->geo_cursor, "SatelliteAzimuthAngle", 1, info->num_scans * info->num_retrievals_per_scan,
                         0, -1000.0, -999.0, data);
}

static int read_sensor_zenith_angle(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable(&info->geo_cursor, "SatelliteZenithAngle", 1, info->num_scans * info->num_retrievals_per_scan,
                         0, -1000.0, -999.0, data);
}

static int read_solar_azimuth_angle(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable(&info->geo_cursor, "SolarAzimuthAngle", 1, info->num_scans * info->num_retrievals_per_scan,
                         0, -1000.0, -999.0, data);
}

static int read_solar_zenith_angle(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable(&info->geo_cursor, "SolarZenithAngle", 1, info->num_scans * info->num_retrievals_per_scan,
                         0, -1000.0, -999.0, data);
}

static int read_data_field(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable(&info->data_cursor, info->data_field_name, 2, info->num_scans * info->num_retrievals_per_scan,
                         info->num_vertical, -999.95, -999.25, data);
}

static int read_axis_field(void *user_data, harp_array data)
{
    harp_array axis_data;
    ingest_info *info = (ingest_info *)user_data;
    long i;

    CHECKED_MALLOC(axis_data.double_data, info->num_granules * info->num_vertical * sizeof(double));
    if (read_variable(&info->data_cursor, info->axis_field_name, 1, info->num_granules * info->num_vertical, 0,
                      -999.95, -999.25, axis_data) != 0)
    {
        free(axis_data.double_data);
        return -1;
    }
    /* For now we assume that the pressure levels are the same for all granules in the file */
    for (i = 0; i < info->num_vertical; i++)
    {
        data.double_data[i] = axis_data.double_data[i];
    }
    free(axis_data.double_data);
    return 0;
}

static int init_swath_names_and_cursors(ingest_info *info)
{
    coda_cursor cursor;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&cursor, "All_Data") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&cursor, "CrIMSS_EDR_GEO_TC_All") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    info->geo_cursor = cursor;
    if (coda_cursor_goto_parent(&cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&cursor, "CrIMSS_EDR_All") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    info->data_cursor = cursor;

    return 0;
}

static int get_dimensions(ingest_info *info)
{
    coda_cursor cursor;
    long coda_dimension[CODA_MAX_NUM_DIMS];
    int num_coda_dimensions;

    cursor = info->data_cursor;
    if (coda_cursor_goto_record_field_by_name(&cursor, "NumRetrievals") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_array_dim(&cursor, &num_coda_dimensions, coda_dimension) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    info->num_granules = coda_dimension[0];

    cursor = info->geo_cursor;
    if (coda_cursor_goto_record_field_by_name(&cursor, "StartTime") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_array_dim(&cursor, &num_coda_dimensions, coda_dimension) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    info->num_scans = coda_dimension[0];

    cursor = info->geo_cursor;
    if (coda_cursor_goto_record_field_by_name(&cursor, "Latitude") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_array_dim(&cursor, &num_coda_dimensions, coda_dimension) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    info->num_retrievals_per_scan = coda_dimension[0] / info->num_scans;

    switch (info->dataset)
    {
        case DATASET_MOISTURE_PROFILE:
        default:
            info->data_field_name = "H2O";
            info->axis_field_name = "PressureLevels_H2O";
            break;

        case DATASET_TEMPERATURE_PROFILE:
            info->data_field_name = "Temperature";
            info->axis_field_name = "PressureLevels_Temperature";
            break;

        case DATASET_PRESSURE_PROFILE:
            info->data_field_name = "Pressure";
            info->axis_field_name = "AltitudeLevels_Pressure";
            break;
    }
    cursor = info->data_cursor;
    if (coda_cursor_goto_record_field_by_name(&cursor, info->axis_field_name) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_array_dim(&cursor, &num_coda_dimensions, coda_dimension) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    info->num_vertical = coda_dimension[0] / info->num_granules;

    return 0;
}

static int ingestion_init(const harp_ingestion_module *module, coda_product *product,
                          const harp_ingestion_options *options, harp_product_definition **definition, void **user_data)
{
    const char *option_value;
    ingest_info *info;

    CHECKED_MALLOC(info, sizeof(ingest_info));
    memset(info, '\0', sizeof(ingest_info));
    info->product = product;
    info->dataset = DATASET_MOISTURE_PROFILE;   /* default value */
    if (harp_ingestion_options_has_option(options, "dataset"))
    {
        if (harp_ingestion_options_get_option(options, "dataset", &option_value) != 0)
        {
            ingestion_done(info);
            return -1;
        }
        if (strcmp(option_value, "temp") == 0)
        {
            info->dataset = DATASET_TEMPERATURE_PROFILE;
        }
        else if (strcmp(option_value, "press") == 0)
        {
            info->dataset = DATASET_PRESSURE_PROFILE;
        }
    }

    if (init_swath_names_and_cursors(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }
    if (get_dimensions(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }
    *user_data = info;

    *definition = module->product_definition[info->dataset];

    nan = coda_NaN();

    return 0;
}

static void register_product(harp_ingestion_module *module, short dataset)
{
    const char *description;
    const char *path;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_vertical };

    switch (dataset)
    {
        case DATASET_MOISTURE_PROFILE:
        default:
            product_definition =
                harp_ingestion_register_product(module, "NPP_SUOMI_L2_CRIMSS_EDR_MOISTURE", NULL, read_dimensions);
            harp_product_definition_add_mapping(product_definition, NULL, "dataset unset");
            break;

        case DATASET_TEMPERATURE_PROFILE:
            product_definition =
                harp_ingestion_register_product(module, "NPP_SUOMI_L2_CRIMSS_EDR_TEMPERATURE", NULL, read_dimensions);
            harp_product_definition_add_mapping(product_definition, NULL, "dataset=temp");
            break;

        case DATASET_PRESSURE_PROFILE:
            product_definition =
                harp_ingestion_register_product(module, "NPP_SUOMI_L2_CRIMSS_EDR_PRESSURE", NULL, read_dimensions);
            harp_product_definition_add_mapping(product_definition, NULL, "dataset=pres");
            break;
    }

    /* datetime */
    description = "time of the measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double, 1, dimension_type,
                                                   NULL, description, "seconds since 2000-01-01", NULL, read_datetime);
    description = "the time converted from seconds since 1958-01-01 to seconds since 2000-01-01T00:00:00";
    path = "/All_Data/CrIMSS-EDR-GEO-TC_All/StartTime, /All_Data/CrIMSS-EDR-GEO-TC_All/MidTime";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* latitude */
    description = "tangent latitude";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_double, 1, dimension_type,
                                                   NULL, description, "degree_north", NULL, read_latitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    path = "/All_Data/CrIMSS-EDR-GEO-TC_All/Latitude";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* longitude */
    description = "tangent longitude";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_double, 1, dimension_type,
                                                   NULL, description, "degree_east", NULL, read_longitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0, 180.0);
    path = "/All_Data/CrIMSS-EDR-GEO-TC_All/Longitude";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* The Height-field contains the difference between the geoid (avery sea level of the globe) and the ellipsoid */
    /* (against which GPS coordinates are specified). This is not the altitude-field we use in HARP so we will not */
    /* ingest the Height-field.                                                                                    */

    /* sensor_azimuth_angle */
    description = "azimuth angle (measured clockwise positive from North) to Satellite at each retrieval position";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "sensor_azimuth_angle",
                                                   harp_type_double, 1, dimension_type, NULL, description, "degree",
                                                   NULL, read_sensor_azimuth_angle);
    harp_variable_definition_set_valid_range_double(variable_definition, 0.0, 180.0);
    path = "/All_Data/CrIMSS-EDR-GEO-TC_All/SatelliteAzimuthAngle";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* sensor_zenith_angle */
    description = "zenith angle to Satellite at each retrieval position";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "sensor_zenith_angle",
                                                   harp_type_double, 1, dimension_type, NULL, description, "degree",
                                                   NULL, read_sensor_zenith_angle);
    harp_variable_definition_set_valid_range_double(variable_definition, 0.0, 180.0);
    path = "/All_Data/CrIMSS-EDR-GEO-TC_All/SatelliteZenithAngle";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* solar_azimuth_angle */
    description = "azimuth angle of sun (measured clockwise positive from North) at each retrieval position";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "solar_azimuth_angle",
                                                   harp_type_double, 1, dimension_type, NULL, description, "degree",
                                                   NULL, read_solar_azimuth_angle);
    harp_variable_definition_set_valid_range_double(variable_definition, 0.0, 180.0);
    path = "/All_Data/CrIMSS-EDR-GEO-TC_All/SatelliteAzimuthAngle";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* solar_zenith_angle */
    description = "zenith angle of sun at each retrieval position";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "solar_zenith_angle",
                                                   harp_type_double, 1, dimension_type, NULL, description, "degree",
                                                   NULL, read_solar_zenith_angle);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    if (dataset == DATASET_MOISTURE_PROFILE)
    {
        /* H2O_column_mass_mixing_ratio */
        description = "water vapor mass mixing ratio profile";
        variable_definition =
            harp_ingestion_register_variable_full_read(product_definition, "H2O_column_mass_mixing_ratio",
                                                       harp_type_double, 2, dimension_type, NULL, description, "g/kg",
                                                       NULL, read_data_field);
        path = "/All_Data/CrIMSS-EDR_All/H2O";
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

        /* pressure */
        description = "pressure levels for H2O retrieval";
        variable_definition =
            harp_ingestion_register_variable_full_read(product_definition, "pressure", harp_type_double, 1,
                                                       &(dimension_type[1]), NULL, description, "hPa", NULL,
                                                       read_axis_field);
        path = "/All_Data/CrIMSS-EDR_All/PressureLevels_H2O";
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
    }

    if (dataset == DATASET_TEMPERATURE_PROFILE)
    {
        /* temperature */
        description = "temperature profile";
        variable_definition =
            harp_ingestion_register_variable_full_read(product_definition, "temperature",
                                                       harp_type_double, 2, dimension_type, NULL, description, "K",
                                                       NULL, read_data_field);
        path = "/All_Data/CrIMSS-EDR_All/Temperature";
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

        /* pressure */
        description = "pressure levels for temperature retrieval";
        variable_definition =
            harp_ingestion_register_variable_full_read(product_definition, "pressure", harp_type_double, 1,
                                                       &(dimension_type[1]), NULL, description, "hPa", NULL,
                                                       read_axis_field);
        path = "/All_Data/CrIMSS-EDR_All/PressureLevels_Temperature";
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
    }

    if (dataset == DATASET_PRESSURE_PROFILE)
    {
        /* pressure */
        description = "pressure profile";
        variable_definition =
            harp_ingestion_register_variable_full_read(product_definition, "pressure",
                                                       harp_type_double, 2, dimension_type, NULL, description, "hPa",
                                                       NULL, read_data_field);
        path = "/All_Data/CrIMSS-EDR_All/Pressure";
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

        /* altitude */
        description = "altitudes corresponding to pressure";
        variable_definition =
            harp_ingestion_register_variable_full_read(product_definition, "altitude", harp_type_double, 1,
                                                       &(dimension_type[1]), NULL, description, "km", NULL,
                                                       read_axis_field);
        path = "/All_Data/CrIMSS-EDR_All/AltitudeLevels_Pressure";
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
    }
}

int harp_ingestion_module_npp_suomi_crimss_l2_init(void)
{
    harp_ingestion_module *module;
    const char *dataset_options[] = { "temp", "press" };

    module = harp_ingestion_register_module("NPP_SUOMI_L2_CRIMSS_EDR_REDR", "NPP", "NPP_SUOMI", "CRIMSS_EDR_REDR_L2",
                                            "NPP Suomi CRIMSS EDR Atmospheric Vertical Profile", ingestion_init,
                                            ingestion_done);

    harp_ingestion_register_option(module, "dataset", "whether to ingest h2o mass mixing ratio vs pressure (default), "
                                   "temperature vs pressure (dataset=temp) or pressure vs altitude (dataset=pres)", 2,
                                   dataset_options);

    register_product(module, DATASET_MOISTURE_PROFILE);
    register_product(module, DATASET_TEMPERATURE_PROFILE);
    register_product(module, DATASET_PRESSURE_PROFILE);

    return 0;
}
