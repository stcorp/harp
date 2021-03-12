/*
 * Copyright (C) 2015-2021 S[&]T, The Netherlands.
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

#define MAX_CROSSTRACKS                            105
#define MAX_ALONGTRACKS_PER_GRANULE                 15

/* ------------------ Typedefs ------------------ */

typedef struct ingest_info_struct
{
    coda_product *product;
    coda_cursor geo_cursor;
    coda_cursor data_cursor;
    long num_crosstracks;
    long num_alongtracks;
    long num_granules;
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

static int read_variable(coda_cursor *cursor, const char *name, int num_dimensions, long dimensions[CODA_MAX_NUM_DIMS],
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
    num_elements = 1;
    for (i = 0; i < num_dimensions; i++)
    {
        if (dimensions[i] != coda_dimension[i])
        {
            harp_set_error(HARP_ERROR_INGESTION,
                           "product error detected in NPP Suomi L2 product (dimension %ld for variable %s "
                           "has %ld elements, expected %ld", i + 1, name, coda_dimension[i], dimensions[i]);
            return -1;
        }
        num_elements *= coda_dimension[i];
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

    /* Note: Do not set an independent dimension here, that causes memory errors */
    dimension[harp_dimension_time] = info->num_granules * info->num_alongtracks * info->num_crosstracks;

    return 0;
}

static int read_datetime(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    harp_array middle_times;
    double *double_data, value;
    long i, j, k, dimensions[1];

    dimensions[0] = info->num_granules * MAX_ALONGTRACKS_PER_GRANULE;
    CHECKED_MALLOC(middle_times.double_data, dimensions[0] * sizeof(double));
    if (read_variable(&info->geo_cursor, "MidTime", 1, dimensions, -999.5, -992.5, middle_times) != 0)
    {
        free(middle_times.double_data);
        return -1;
    }

    double_data = data.double_data;
    for (i = 0; i < info->num_granules; i++)
    {
        for (j = 0; j < info->num_alongtracks; j++)
        {
            value = middle_times.double_data[i * MAX_ALONGTRACKS_PER_GRANULE + j];
            if (!coda_isNaN(value))
            {
                value = (value / MICROSECONDS_IN_SECOND) - SECONDS_FROM_1958_TO_2000;
            }
            for (k = 0; k < info->num_crosstracks; k++)
            {
                *double_data = value;
                double_data++;
            }
        }
    }

    free(middle_times.double_data);
    return 0;
}

static int read_2_dim_geo_variable(const char *fieldname, void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    harp_array data_plus_filler;
    double *double_data, value;
    long i, j, k, dimensions[2];

    dimensions[0] = info->num_granules * MAX_ALONGTRACKS_PER_GRANULE;
    dimensions[1] = MAX_CROSSTRACKS;
    CHECKED_MALLOC(data_plus_filler.double_data, dimensions[0] * dimensions[1] * sizeof(double));
    if (read_variable(&info->geo_cursor, fieldname, 2, dimensions, -999.95, -999.25, data_plus_filler) != 0)
    {
        free(data_plus_filler.double_data);
        return -1;
    }

    double_data = data.double_data;
    for (i = 0; i < info->num_granules; i++)
    {
        for (j = 0; j < info->num_alongtracks; j++)
        {
            for (k = 0; k < info->num_crosstracks; k++)
            {
                value =
                    data_plus_filler.double_data[i * MAX_ALONGTRACKS_PER_GRANULE * MAX_CROSSTRACKS +
                                                 j * MAX_CROSSTRACKS + k];
                *double_data = value;
                double_data++;
            }
        }
    }

    free(data_plus_filler.double_data);
    return 0;
}

static int read_latitude(void *user_data, harp_array data)
{
    return read_2_dim_geo_variable("Latitude", user_data, data);
}

static int read_longitude(void *user_data, harp_array data)
{
    return read_2_dim_geo_variable("Longitude", user_data, data);
}

static int read_3_dim_geo_variable(const char *fieldname, void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    harp_array data_plus_filler;
    double *double_data, value;
    long i, j, k, l, dimensions[3];

    dimensions[0] = info->num_granules * MAX_ALONGTRACKS_PER_GRANULE;
    dimensions[1] = MAX_CROSSTRACKS;
    dimensions[2] = 4;
    CHECKED_MALLOC(data_plus_filler.double_data, dimensions[0] * dimensions[1] * dimensions[2] * sizeof(double));
    if (read_variable(&info->geo_cursor, fieldname, 3, dimensions, -999.95, -999.25, data_plus_filler) != 0)
    {
        free(data_plus_filler.double_data);
        return -1;
    }

    double_data = data.double_data;
    for (i = 0; i < info->num_granules; i++)
    {
        for (j = 0; j < info->num_alongtracks; j++)
        {
            for (k = 0; k < info->num_crosstracks; k++)
            {
                for (l = 0; l < 4; l++)
                {
                    value =
                        data_plus_filler.double_data[i * MAX_ALONGTRACKS_PER_GRANULE * MAX_CROSSTRACKS * 4 +
                                                     j * MAX_CROSSTRACKS * 4 + k * 4 + l];
                    *double_data = value;
                    double_data++;
                }
            }
        }
    }

    free(data_plus_filler.double_data);
    return 0;
}

static int read_latitude_bounds(void *user_data, harp_array data)
{
    return read_3_dim_geo_variable("LatitudeCorners", user_data, data);
}

static int read_longitude_bounds(void *user_data, harp_array data)
{
    return read_3_dim_geo_variable("LongitudeCorners", user_data, data);
}

static int read_sensor_azimuth_angle(void *user_data, harp_array data)
{
    return read_2_dim_geo_variable("SatelliteAzimuthAngle", user_data, data);
}

static int read_sensor_zenith_angle(void *user_data, harp_array data)
{
    return read_2_dim_geo_variable("SatelliteZenithAngle", user_data, data);
}

static int read_solar_azimuth_angle(void *user_data, harp_array data)
{
    return read_2_dim_geo_variable("SolarAzimuthAngle", user_data, data);
}

static int read_solar_zenith_angle(void *user_data, harp_array data)
{
    return read_2_dim_geo_variable("SolarZenithAngle", user_data, data);
}

static int read_ozone_column_number_density(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long dimensions[2];

    dimensions[0] = info->num_granules * info->num_alongtracks;
    dimensions[1] = info->num_crosstracks;
    return read_variable(&info->data_cursor, "ColumnAmountO3", 2, dimensions, -999.95, -999.25, data);
}

static int init_cursors(ingest_info *info)
{
    const char *swath_name;
    coda_cursor cursor;
    coda_type *type;
    long num_swaths, swath_index;

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
    if (coda_cursor_get_type(&cursor, &type) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_type_get_num_record_fields(type, &num_swaths) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    for (swath_index = 0; swath_index < num_swaths; swath_index++)
    {
        if (coda_type_get_record_field_name(type, swath_index, &swath_name) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_goto_record_field_by_index(&cursor, swath_index) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (strcmp(swath_name + strlen(swath_name) - 7, "GEO_All") == 0)
        {
            info->geo_cursor = cursor;
        }
        if (strcmp(swath_name + strlen(swath_name) - 7, "EDR_All") == 0)
        {
            info->data_cursor = cursor;
        }
        if (coda_cursor_goto_parent(&cursor) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
    }

    return 0;
}

static int get_dimensions(ingest_info *info)
{
    coda_cursor cursor;
    harp_array latitudes_plus_filler;
    long dimensions[CODA_MAX_NUM_DIMS], start_row, end_row, col;
    int num_dimensions;

    cursor = info->geo_cursor;
    if (coda_cursor_goto_record_field_by_name(&cursor, "Latitude") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_array_dim(&cursor, &num_dimensions, dimensions) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (num_dimensions != 2)
    {
        harp_set_error(HARP_ERROR_INGESTION, "latitude field in NPP Suomi Ozone Total Column is not two-dimensional");
        return -1;
    }

    CHECKED_MALLOC(latitudes_plus_filler.double_data, dimensions[0] * dimensions[1] * sizeof(double));
    if (coda_cursor_read_double_array(&cursor, latitudes_plus_filler.double_data, coda_array_ordering_c) != 0)
    {
        free(latitudes_plus_filler.double_data);
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    for (start_row = 0; start_row < dimensions[0]; start_row++)
    {
        if (latitudes_plus_filler.double_data[start_row * dimensions[1]] > -999.25)
        {
            break;
        }
    }
    if (start_row >= dimensions[0])
    {
        free(latitudes_plus_filler.double_data);
        harp_set_error(HARP_ERROR_INGESTION, "latitude field in NPP Suomi Ozone Total Column does not contain data");
        return -1;
    }
    for (end_row = start_row; end_row < dimensions[0]; end_row++)
    {
        if (latitudes_plus_filler.double_data[end_row * dimensions[1]] < -999.25)
        {
            break;
        }
    }

    info->num_alongtracks = (end_row - start_row);
    info->num_granules = dimensions[0] / MAX_ALONGTRACKS_PER_GRANULE;
    for (col = 0; col < dimensions[1]; col++)
    {
        if (latitudes_plus_filler.double_data[start_row * dimensions[1] + col] < -999.25)
        {
            break;
        }
    }
    info->num_crosstracks = col;
    free(latitudes_plus_filler.double_data);

    return 0;
}

static int ingestion_init(const harp_ingestion_module *module, coda_product *product,
                          const harp_ingestion_options *options, harp_product_definition **definition, void **user_data)
{
    ingest_info *info;

    (void)options;

    CHECKED_MALLOC(info, sizeof(ingest_info));
    memset(info, '\0', sizeof(ingest_info));
    info->product = product;

    if (init_cursors(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }
    if (get_dimensions(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }

    *definition = *module->product_definition;
    *user_data = info;

    nan = coda_NaN();

    return 0;
}

/* Register the Ozone Total Column (product type OOTC) in the OMPS EDR files */
int harp_ingestion_module_npp_suomi_omps_totals_l2_init(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };
    harp_dimension_type bounds_dimension_type[2] = { harp_dimension_time, harp_dimension_independent };
    long bounds_dimension[2] = { -1, 4 };
    const char *description;
    const char *path;

    module = harp_ingestion_register_module("NPP_SUOMI_L2_OMPS_EDR_OOTC", "NPP", "NPP_SUOMI", "OMPS_EDR_OOTC_L2",
                                            "NPP Suomi OMPS EDR Ozone Total Column", ingestion_init, ingestion_done);
    product_definition = harp_ingestion_register_product(module, "NPP_SUOMI_L2_OMPS_EDR_OOTC", NULL, read_dimensions);

    /* datetime */
    description = "time of the measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double, 1, dimension_type,
                                                   NULL, description, "seconds since 2000-01-01", NULL, read_datetime);
    path = "/All_Data/OMPS_TC_GEO_All/MidTime";
    description = "the time converted from seconds since 1958-01-01 to seconds since 2000-01-01T00:00:00";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* latitude */
    description = "latitude";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_double, 1, dimension_type,
                                                   NULL, description, "degree_north", NULL, read_latitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    path = "/All_Data/OMPS_TC_GEO_All/Latitude";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* longitude */
    description = "longitude";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_double, 1, dimension_type,
                                                   NULL, description, "degree_east", NULL, read_longitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0, 180.0);
    path = "/All_Data/OMPS_TC_GEO_ALL/Longitude";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* latitude_bounds */
    description = "latitude corners";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "latitude_bounds", harp_type_double, 2,
                                                   bounds_dimension_type, bounds_dimension, description, "degree_north",
                                                   NULL, read_latitude_bounds);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    path = "/All_Data/OMPS_TC_GEO_All/LatitudeCorners";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* longitude_bounds */
    description = "longitude corners";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "longitude_bounds", harp_type_double, 2,
                                                   bounds_dimension_type, bounds_dimension, description, "degree_east",
                                                   NULL, read_longitude_bounds);
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0, 180.0);
    path = "/All_Data/OMPS_TC_GEO_ALL/LongitudeCorners";

    /* sensor_azimuth_angle */
    description = "azimuth angle (measured clockwise positive from North) to Satellite at each retrieval position";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "sensor_azimuth_angle",
                                                   harp_type_double, 1, dimension_type, NULL, description, "degree",
                                                   NULL, read_sensor_azimuth_angle);
    harp_variable_definition_set_valid_range_double(variable_definition, 0.0, 180.0);
    path = "/All_Data/OMPS_TC_GEO_All/SatelliteAzimuthAngle";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* sensor_zenith_angle */
    description = "zenith angle to Satellite at each retrieval position";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "sensor_zenith_angle",
                                                   harp_type_double, 1, dimension_type, NULL, description, "degree",
                                                   NULL, read_sensor_zenith_angle);
    harp_variable_definition_set_valid_range_double(variable_definition, 0.0, 180.0);
    path = "/All_Data/OMPS_TC_GEO_All/SatelliteZenithAngle";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* solar_azimuth_angle */
    description = "azimuth angle of sun (measured clockwise positive from North) at each retrieval position";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "solar_azimuth_angle",
                                                   harp_type_double, 1, dimension_type, NULL, description, "degree",
                                                   NULL, read_solar_azimuth_angle);
    harp_variable_definition_set_valid_range_double(variable_definition, 0.0, 180.0);
    path = "/All_Data/OMPS_TC_GEO_All/SatelliteAzimuthAngle";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* solar_zenith_angle */
    description = "zenith angle of sun at each retrieval position";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "solar_zenith_angle",
                                                   harp_type_double, 1, dimension_type, NULL, description, "degree",
                                                   NULL, read_solar_zenith_angle);
    harp_variable_definition_set_valid_range_double(variable_definition, 0.0, 180.0);
    path = "/All_Data/OMPS_TC_GEO_All/SatelliteZenithAngle";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* O3_column_number_density */
    description = "ozone column number density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_column_number_density",
                                                   harp_type_double, 1, dimension_type, NULL, description, "DU",
                                                   NULL, read_ozone_column_number_density);
    path = "/All_Data/OMPS_TC_EDR_All/ColumnAmountO3";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    return 0;
}
