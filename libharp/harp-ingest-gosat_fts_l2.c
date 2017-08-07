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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct ingest_info_struct
{
    coda_product *product;
    long num_main;

    double *corner_latitude;
    double *corner_longitude;
} ingest_info;

static int init_num_main(ingest_info *info)
{
    coda_cursor cursor;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, "scanAttribute/time") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &info->num_main) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    return 0;
}

static int init_corner_points(ingest_info *info)
{
    coda_cursor cursor;
    double *fplat;
    double *fplon;
    long num_elements;
    int i;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, "Data/geolocation/footPrintLatitude") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &num_elements) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (num_elements % 36 != 0)
    {
        harp_set_error(HARP_ERROR_INGESTION, "product error detected in GOSAT L2 product (dataset "
                       "'/Data/geolocation/footPrintLatitude' should have 36 points per footprint)");
        return -1;
    }
    num_elements /= 36;

    fplat = malloc(36 * num_elements * sizeof(double));
    if (fplat == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       36 * num_elements * sizeof(double), __FILE__, __LINE__);
        return -1;
    }
    fplon = malloc(36 * num_elements * sizeof(double));
    if (fplon == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       36 * num_elements * sizeof(double), __FILE__, __LINE__);
        free(fplat);
        return -1;
    }
    if (coda_cursor_read_double_array(&cursor, fplat, coda_array_ordering_c) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        free(fplat);
        free(fplon);
        return -1;
    }
    if (coda_cursor_goto(&cursor, "../footPrintLongitude") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        free(fplat);
        free(fplon);
        return -1;
    }
    if (coda_cursor_read_double_array(&cursor, fplon, coda_array_ordering_c) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        free(fplat);
        free(fplon);
        return -1;
    }

    info->corner_latitude = malloc(4 * num_elements * sizeof(double));
    if (info->corner_latitude == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       4 * num_elements * sizeof(double), __FILE__, __LINE__);
        free(fplat);
        free(fplon);
        return -1;
    }
    info->corner_longitude = malloc(4 * num_elements * sizeof(double));
    if (info->corner_longitude == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       4 * num_elements * sizeof(double), __FILE__, __LINE__);
        free(fplat);
        free(fplon);
        return -1;
    }

    /* We currently just use a simple lat/lon bounding box to reduce the bounding polygon of 36 points to one of
     * 4 points.
     */
    for (i = 0; i < num_elements; i++)
    {
        double min_lat, max_lat, min_lon, max_lon;
        int j;

        /* map -180,180 to 0,360 if footprint could overlap with the 180 deg dateline */
        if (fplon[i * 36] < -90 || fplon[i * 36] > 90)
        {
            for (j = 0; j < 36; j++)
            {
                if (fplon[i * 36] < 0)
                {
                    fplon[i * 36] += 360;
                }
            }
        }
        min_lat = fplat[i * 36];
        max_lat = fplat[i * 36];
        min_lon = fplon[i * 36];
        max_lon = fplon[i * 36];
        for (j = 1; j < 36; j++)
        {
            if (fplat[i * 36 + j] < min_lat)
            {
                min_lat = fplat[i * 36 + j];
            }
            if (fplat[i * 36 + j] > max_lat)
            {
                max_lat = fplat[i * 36 + j];
            }
            if (fplon[i * 36 + j] < min_lon)
            {
                min_lon = fplon[i * 36 + j];
            }
            if (fplon[i * 36 + j] > max_lon)
            {
                max_lon = fplon[i * 36 + j];
            }
        }
        if (min_lon >= 180)
        {
            min_lon -= 360;
        }
        if (max_lon >= 180)
        {
            max_lon -= 360;
        }
        info->corner_latitude[i * 4] = min_lat;
        info->corner_latitude[i * 4 + 1] = min_lat;
        info->corner_latitude[i * 4 + 2] = max_lat;
        info->corner_latitude[i * 4 + 3] = max_lat;
        info->corner_longitude[i * 4] = min_lon;
        info->corner_longitude[i * 4 + 1] = max_lon;
        info->corner_longitude[i * 4 + 2] = max_lon;
        info->corner_longitude[i * 4 + 3] = min_lon;
    }

    free(fplat);
    free(fplon);

    return 0;
}

static int read_dimensions(void *user_data, long dimension[HARP_NUM_DIM_TYPES])
{
    dimension[harp_dimension_time] = ((ingest_info *)user_data)->num_main;

    return 0;
}

static int read_float_dataset_value(ingest_info *info, const char *path, long index, float invalid_value, double *value)
{
    coda_cursor cursor;
    float flvalue;

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
    if (coda_cursor_goto_array_element_by_index(&cursor, index) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_float(&cursor, &flvalue) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (flvalue == invalid_value)
    {
        flvalue = (float)coda_NaN();
    }
    *value = flvalue;

    return 0;
}

static int read_time(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    coda_cursor cursor;
    char buffer[100];

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, "scanAttribute/time") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_array_element_by_index(&cursor, index) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_string(&cursor, buffer, 100) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    /* turn .xxx to .xxx000 */
    buffer[23] = '0';
    buffer[24] = '0';
    buffer[25] = '0';
    buffer[26] = '\0';
    if (coda_string_to_time(buffer, data.double_data) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    return 0;
}

static int read_latitude(void *user_data, long index, harp_array data)
{
    return read_float_dataset_value((ingest_info *)user_data, "Data/geolocation/latitude", index, -9999.0,
                                    data.double_data);
}

static int read_longitude(void *user_data, long index, harp_array data)
{
    return read_float_dataset_value((ingest_info *)user_data, "Data/geolocation/longitude", index, -9999.0,
                                    data.double_data);
}

static int read_corner_latitude(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->corner_latitude == NULL)
    {
        if (init_corner_points(info) != 0)
        {
            return -1;
        }
    }

    memcpy(data.double_data, &info->corner_latitude[index * 4], 4 * sizeof(double));

    return 0;
}

static int read_corner_longitude(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->corner_longitude == NULL)
    {
        if (init_corner_points(info) != 0)
        {
            return -1;
        }
    }

    memcpy(data.double_data, &info->corner_longitude[index * 4], 4 * sizeof(double));

    return 0;
}

static int read_co2_column(void *user_data, long index, harp_array data)
{
    return read_float_dataset_value((ingest_info *)user_data, "Data/totalColumn/CO2TotalColumn", index, -1.0E30f,
                                    data.double_data);
}

static int read_co2_column_error(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    double external_error;
    double interference_error;
    double retrieval_noise;
    double smoothing_error;

    *data.double_data = 0;
    if (read_float_dataset_value(info, "Data/totalColumn/CO2TotalColumnExternalError", index, -1.0E30f, &external_error)
        != 0)
    {
        return -1;
    }
    if (!coda_isNaN(external_error))
    {
        *data.double_data += external_error;
    }
    if (read_float_dataset_value
        (info, "Data/totalColumn/CO2TotalColumnInterferenceError", index, -1.0E30f, &interference_error) != 0)
    {
        return -1;
    }
    if (!coda_isNaN(interference_error))
    {
        *data.double_data += interference_error;
    }
    if (read_float_dataset_value
        (info, "Data/totalColumn/CO2TotalColumnRetrievalNoise", index, -1.0E30f, &retrieval_noise) != 0)
    {
        return -1;
    }
    if (!coda_isNaN(retrieval_noise))
    {
        *data.double_data += retrieval_noise;
    }
    if (read_float_dataset_value
        (info, "Data/totalColumn/CO2TotalColumnSmoothingError", index, -1.0E30f, &smoothing_error) != 0)
    {
        return -1;
    }
    if (!coda_isNaN(smoothing_error))
    {
        *data.double_data += smoothing_error;
    }

    return 0;
}

static int read_ch4_column(void *user_data, long index, harp_array data)
{
    return read_float_dataset_value((ingest_info *)user_data, "Data/totalColumn/CH4TotalColumn", index, -1.0E30f,
                                    data.double_data);
}

static int read_ch4_column_error(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    double external_error;
    double interference_error;
    double retrieval_noise;
    double smoothing_error;

    *data.double_data = 0;
    if (read_float_dataset_value(info, "Data/totalColumn/CH4TotalColumnExternalError", index, -1.0E30f, &external_error)
        != 0)
    {
        return -1;
    }
    if (!coda_isNaN(external_error))
    {
        *data.double_data += external_error;
    }
    if (read_float_dataset_value
        (info, "Data/totalColumn/CH4TotalColumnInterferenceError", index, -1.0E30f, &interference_error) != 0)
    {
        return -1;
    }
    if (!coda_isNaN(interference_error))
    {
        *data.double_data += interference_error;
    }
    if (read_float_dataset_value
        (info, "Data/totalColumn/CH4TotalColumnRetrievalNoise", index, -1.0E30f, &retrieval_noise) != 0)
    {
        return -1;
    }
    if (!coda_isNaN(retrieval_noise))
    {
        *data.double_data += retrieval_noise;
    }
    if (read_float_dataset_value
        (info, "Data/totalColumn/CH4TotalColumnSmoothingError", index, -1.0E30f, &smoothing_error) != 0)
    {
        return -1;
    }
    if (!coda_isNaN(smoothing_error))
    {
        *data.double_data += smoothing_error;
    }

    return 0;
}

static int read_solar_zenith_angle(void *user_data, long index, harp_array data)
{
    return read_float_dataset_value((ingest_info *)user_data, "Data/geolocation/solarZenith", index, -9999.0,
                                    data.double_data);
}

static int read_solar_azimuth_angle(void *user_data, long index, harp_array data)
{
    return read_float_dataset_value((ingest_info *)user_data, "Data/geolocation/solarAzimuth", index, -9999.0,
                                    data.double_data);
}

static int read_los_zenith_angle(void *user_data, long index, harp_array data)
{
    if (read_float_dataset_value
        ((ingest_info *)user_data, "Data/geolocation/satelliteZenith", index, -9999.0, data.double_data) != 0)
    {
        return -1;
    }

    *data.double_data = 180 - *data.double_data;
    return 0;
}

static int read_los_azimuth_angle(void *user_data, long index, harp_array data)
{
    if (read_float_dataset_value
        ((ingest_info *)user_data, "Data/geolocation/satelliteAzimuth", index, -9999.0, data.double_data) != 0)
    {
        return -1;
    }

    *data.double_data = *data.double_data + 180;
    while (*data.double_data > 360)
    {
        *data.double_data -= 360;
    }
    return 0;
}

static void ingestion_done(void *user_data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->corner_latitude != NULL)
    {
        free(info->corner_latitude);
    }
    if (info->corner_longitude != NULL)
    {
        free(info->corner_longitude);
    }

    free(info);
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
    info->corner_latitude = NULL;
    info->corner_longitude = NULL;

    if (init_num_main(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }

    *definition = *module->product_definition;
    *user_data = info;

    return 0;
}

static void register_common_variables(harp_product_definition *product_definition)
{
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };
    harp_dimension_type dimension_type_bounds[2] = { harp_dimension_time, harp_dimension_independent };
    long dimension_bounds[2] = { -1, 4 };
    const char *description;
    const char *path;

    /* datetime */
    description = "time of the measurement at end of integration time (in seconds since 2000-01-01 00:00:00)";
    variable_definition =
        harp_ingestion_register_variable_block_read(product_definition, "datetime", harp_type_double, 1,
                                                    dimension_type, NULL, description, "seconds since 2000-01-01",
                                                    NULL, read_time);
    path = "/scanAttribute/time[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* longitude */
    description = "longitude of the measurement";
    variable_definition =
        harp_ingestion_register_variable_block_read(product_definition, "longitude", harp_type_double, 1,
                                                    dimension_type, NULL, description, "degree_east", NULL,
                                                    read_longitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0, 180.0);
    path = "/Data/geolocation/longitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* latitude */
    description = "latitude of the measurement";
    variable_definition =
        harp_ingestion_register_variable_block_read(product_definition, "latitude", harp_type_double, 1,
                                                    dimension_type, NULL, description, "degree_north", NULL,
                                                    read_latitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    path = "/Data/geolocation/latitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* longitude_bounds */
    description = "corner longitudes for the geospatial footprint of the measurement";
    variable_definition =
        harp_ingestion_register_variable_block_read(product_definition, "longitude_bounds", harp_type_double, 2,
                                                    dimension_type_bounds, dimension_bounds, description,
                                                    "degree_east", NULL, read_corner_longitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0, 180.0);
    path = "/Data/geolocation/footPrintLongitude";
    description = "the corners are calculated by defining a bounding box around the circular footprint area";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* latitude_bounds */
    description = "corner latitudes for the geospatial footprint of the measurement";
    variable_definition =
        harp_ingestion_register_variable_block_read(product_definition, "latitude_bounds", harp_type_double, 2,
                                                    dimension_type_bounds, dimension_bounds, description,
                                                    "degree_north", NULL, read_corner_latitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    path = "/Data/geolocation/footPrintLatitude";
    description = "the corners are calculated by defining a bounding box around the circular footprint area";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* solar_azimuth_angle */
    description = "solar azimuth angle at the observation point";
    variable_definition =
        harp_ingestion_register_variable_block_read(product_definition, "solar_azimuth_angle", harp_type_double, 1,
                                                    dimension_type, NULL, description, "degree", NULL,
                                                    read_solar_azimuth_angle);
    harp_variable_definition_set_valid_range_double(variable_definition, 0.0, 360.0);
    path = "/Data/geolocation/solarAzimuth[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* solar_zenith_angle */
    description = "solar zenith angle at the observation point";
    variable_definition =
        harp_ingestion_register_variable_block_read(product_definition, "solar_zenith_angle", harp_type_double, 1,
                                                    dimension_type, NULL, description, "degree", NULL,
                                                    read_solar_zenith_angle);
    harp_variable_definition_set_valid_range_double(variable_definition, 0.0, 180.0);
    path = "/Data/geolocation/solarZenith[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* sensor_azimuth_angle */
    description = "sensor azimuth angle at the surface";
    variable_definition =
        harp_ingestion_register_variable_block_read(product_definition, "sensor_azimuth_angle", harp_type_double, 1,
                                                    dimension_type, NULL, description, "degree", NULL,
                                                    read_los_azimuth_angle);
    harp_variable_definition_set_valid_range_double(variable_definition, 0.0, 360.0);
    path = "/Data/geolocation/satelliteAzimuth[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* sensor_zenith_angle */
    description = "sensor zenith angle at the observation point";
    variable_definition =
        harp_ingestion_register_variable_block_read(product_definition, "sensor_zenith_angle", harp_type_double, 1,
                                                    dimension_type, NULL, description, "degree", NULL,
                                                    read_los_zenith_angle);
    harp_variable_definition_set_valid_range_double(variable_definition, 0.0, 180.0);
    path = "/Data/geolocation/satelliteZenith[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

static void register_co2_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };
    const char *description;
    const char *path;

    module = harp_ingestion_register_module_coda("GOSAT_FTS_L2_CO2_TC", "GOSAT FTS", "GOSAT", "L2_FTS_C01S",
                                                 "GOSAT FTS L2 CO2 total column density", ingestion_init,
                                                 ingestion_done);

    /* GOSAT_FTS_L2_CO2_TC product */
    product_definition =
        harp_ingestion_register_product(module, "GOSAT_FTS_L2_CO2_TC", "GOSAT FTS L2 CO2 total column density",
                                        read_dimensions);
    register_common_variables(product_definition);

    /* CO2_column_number_density */
    description = "CO2 column number density";
    variable_definition =
        harp_ingestion_register_variable_block_read(product_definition, "CO2_column_number_density", harp_type_double,
                                                    1, dimension_type, NULL, description, "molec/cm^2", NULL,
                                                    read_co2_column);
    path = "/Data/totalColumn/CO2TotalColumn";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* CO2_column_number_density_uncertainty */
    description = "uncertainty of the CO2 column number density";
    variable_definition =
        harp_ingestion_register_variable_block_read(product_definition, "CO2_column_number_density_uncertainty",
                                                    harp_type_double, 1, dimension_type, NULL, description,
                                                    "molec/cm^2", NULL, read_co2_column_error);
    path =
        "/Data/totalColumn/CO2TotalColumnSmoothingError, /Data/totalColumn/CO2TotalColumnRetrievalNoise, "
        "/Data/totalColumn/CO2TotalColumnInterferenceError, /Data/totalColumn/CO2TotalColumnExternalNoise";
    description = "the uncertainty returned is the sum of all four error components";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);
}

static void register_ch4_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };
    const char *description;
    const char *path;

    module = harp_ingestion_register_module_coda("GOSAT_FTS_L2_CH4_TC", "GOSAT FTS", "GOSAT", "L2_FTS_C02S",
                                                 "GOSAT FTS L2 CH4 total column density", ingestion_init,
                                                 ingestion_done);

    /* GOSAT_FTS_L2_CH4_TC */
    product_definition =
        harp_ingestion_register_product(module, "GOSAT_FTS_L2_CH4_TC", "GOSAT FTS L2 CH4 total column density",
                                        read_dimensions);
    register_common_variables(product_definition);

    /* CH4_column_number_density */
    description = "CH4 column number density";
    variable_definition =
        harp_ingestion_register_variable_block_read(product_definition, "CH4_column_number_density", harp_type_double,
                                                    1, dimension_type, NULL, description, "molec/cm^2", NULL,
                                                    read_ch4_column);
    path = "/Data/totalColumn/CH4TotalColumn";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* CH4_column_number_density_uncertainty */
    description = "uncertainty of the CH4 column number density";
    variable_definition =
        harp_ingestion_register_variable_block_read(product_definition, "CH4_column_number_density_uncertainty",
                                                    harp_type_double, 1, dimension_type, NULL, description,
                                                    "molec/cm^2", NULL, read_ch4_column_error);
    path =
        "/Data/totalColumn/CH4TotalColumnSmoothingError, /Data/totalColumn/CH4TotalColumnRetrievalNoise, "
        "/Data/totalColumn/CH4TotalColumnInterferenceError, /Data/totalColumn/CH4TotalColumnExternalNoise";
    description = "the uncertainty returned is the sum of all four error components";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);
}

int harp_ingestion_module_gosat_fts_l2_init(void)
{
    register_co2_product();
    register_ch4_product();

    return 0;
}
