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
#include "harp-constants.h"
#include "harp-ingestion.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ------------------- Defines ------------------ */

#ifndef FALSE
#define FALSE    0
#define TRUE     1
#endif

#define SECONDS_FROM_1993_TO_2000    220838400
#define CELSIUS_TO_KELVIN                273.15

#define CHECKED_MALLOC(v, s) v = malloc(s); if (v == NULL) { harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)", s, __FILE__, __LINE__); return -1;}

/* ------------------ Typedefs ------------------ */

typedef struct ingest_info_struct
{
    coda_product *product;
    long num_times;
    long num_altitudes;
    long extra_timefields_dimension;
    double *values_buffer;
    short wavelength;
} ingest_info;

/* -------------- Global variables -------------- */

static const char *wavelength_options[] = { "532", "1064" };

/* -------------------- Code -------------------- */

static void ingestion_done(void *user_data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->values_buffer != NULL)
    {
        free(info->values_buffer);
    }
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
    fill_value.double_data = -9999.0;
    harp_array_replace_fill_value(harp_type_double, num_elements, data, fill_value);

    return 0;
}

static int read_average_value(ingest_info *info, const char *field_name, harp_array data)
{
    harp_array two_dim_values;
    double *src, *dest, average_value;
    long i, j;
    int retval;

    two_dim_values.double_data = info->values_buffer;
    if ((retval =
         read_dataset(info, field_name, info->num_times * info->extra_timefields_dimension, two_dim_values)) != 0)
    {
        return retval;
    }

    src = two_dim_values.double_data;
    dest = data.double_data;
    for (i = 0; i < info->num_times; i++)
    {
        average_value = 0.0;
        for (j = 0; j < info->extra_timefields_dimension; j++)
        {
            average_value += *src;
            src++;
        }
        *dest = (average_value / info->extra_timefields_dimension);
        dest++;
    }
    return 0;
}

static int read_datetime(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    double *double_data;
    long i;

    if (read_average_value(info, "/Profile_Time", data) != 0)
    {
        return -1;
    }
    double_data = data.double_data;
    for (i = 0; i < info->num_times; i++)
    {
        *double_data = *double_data - SECONDS_FROM_1993_TO_2000;
        double_data++;
    }
    return 0;
}

static int read_latitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_average_value(info, "/Latitude", data);
}

static int read_longitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_average_value(info, "/Longitude", data);
}

static int read_solar_zenith_angle(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "/Solar_Zenith_Angle", info->num_times, data);
}

static int read_solar_azimuth_angle(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "/Solar_Azimuth_Angle", info->num_times, data);
}

static int read_tropopause_altitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "/Tropopause_Height", info->num_times, data);
}

static int read_tropospheric_aerosol_optical_depth(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char field_name[81];

    sprintf(field_name, "/Column_Optical_Depth_Tropospheric_Aerosols_%d", info->wavelength);
    return read_dataset(info, field_name, info->num_times, data);
}

static int read_tropospheric_aerosol_optical_depth_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char field_name[81];

    sprintf(field_name, "/Column_Optical_Depth_Tropospheric_Aerosols_Uncertainty_%d", info->wavelength);
    return read_dataset(info, field_name, info->num_times, data);
}

static int read_stratospheric_aerosol_optical_depth(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char field_name[81];

    sprintf(field_name, "/Column_Optical_Depth_Stratospheric_Aerosols_%d", info->wavelength);
    return read_dataset(info, field_name, info->num_times, data);
}

static int read_stratospheric_aerosol_optical_depth_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char field_name[81];

    sprintf(field_name, "/Column_Optical_Depth_Stratospheric_Aerosols_Uncertainty_%d", info->wavelength);
    return read_dataset(info, field_name, info->num_times, data);
}

static int read_cloud_optical_depth(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    /* The cloud data is always with respect to wavelength 532 */
    return read_dataset(info, "/Column_Optical_Depth_Cloud_532", info->num_times, data);
}

static int read_cloud_optical_depth_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    /* The cloud data is always with respect to wavelength 532 */
    return read_dataset(info, "/Column_Optical_Depth_Cloud_Uncertainty_532", info->num_times, data);
}

static int read_altitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "/metadata/Lidar_Data_Altitudes", info->num_altitudes, data);
}

static int read_pressure(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "/Pressure", info->num_times * info->num_altitudes, data);
}

static int read_temperature(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    double *double_data;
    long i;

    if (read_dataset(info, "/Temperature", info->num_times * info->num_altitudes, data) != 0)
    {
        return -1;
    }
    double_data = data.double_data;
    for (i = 0; i < (info->num_times * info->num_altitudes); i++)
    {
        *double_data = *double_data + CELSIUS_TO_KELVIN;
        double_data++;
    }
    return 0;
}

static int read_aerosol_extinction_coefficient(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char field_name[81];

    sprintf(field_name, "/Extinction_Coefficient_%d", info->wavelength);
    return read_dataset(info, field_name, info->num_times * info->num_altitudes, data);
}

static int read_aerosol_extinction_coefficient_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    char field_name[81];

    sprintf(field_name, "/Extinction_Coefficient_Uncertainty_%d", info->wavelength);
    return read_dataset(info, field_name, info->num_times * info->num_altitudes, data);
}

static int read_backscatter_coefficient(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->wavelength == 532)
    {
        return read_dataset(info, "/Total_Backscatter_Coefficient_532", info->num_times * info->num_altitudes, data);
    }
    else
    {
        return read_dataset(info, "/Backscatter_Coefficient_1064", info->num_times * info->num_altitudes, data);
    }
}

static int read_backscatter_coefficient_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->wavelength == 532)
    {
        return read_dataset(info, "/Total_Backscatter_Coefficient_Uncertainty_532",
                            info->num_times * info->num_altitudes, data);
    }
    else
    {
        return read_dataset(info, "/Backscatter_Coefficient_Uncertainty_1064", info->num_times * info->num_altitudes,
                            data);
    }
}

static int read_dimensions(void *user_data, long dimension[HARP_NUM_DIM_TYPES])
{
    ingest_info *info = (ingest_info *)user_data;

    dimension[harp_dimension_time] = info->num_times;
    dimension[harp_dimension_vertical] = info->num_altitudes;

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
    if (coda_cursor_goto(&cursor, "/Latitude") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_array_dim(&cursor, &num_coda_dims, coda_dim) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    info->num_times = coda_dim[0];
    if (num_coda_dims > 1)
    {
        info->extra_timefields_dimension = coda_dim[1];
    }
    else
    {
        info->extra_timefields_dimension = 1;
    }
    coda_cursor_goto_parent(&cursor);
    CHECKED_MALLOC(info->values_buffer, info->num_times * info->extra_timefields_dimension * sizeof(double));

    if (coda_cursor_goto(&cursor, "/Extinction_Coefficient_532") != 0)
    {
        /* The altitude specific fields that HARP ingests are not present */
        info->num_altitudes = 1;
        return 0;
    }
    if (coda_cursor_get_array_dim(&cursor, &num_coda_dims, coda_dim) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    info->num_altitudes = coda_dim[1];

    return 0;
}

static int ingestion_init(const harp_ingestion_module *module, coda_product *product,
                          const harp_ingestion_options *options, harp_product_definition **definition, void **user_data)
{
    const char *option_value;
    ingest_info *info;

    CHECKED_MALLOC(info, sizeof(ingest_info));
    info->product = product;
    info->values_buffer = NULL;

    if (harp_ingestion_options_has_option(options, "wavelength"))
    {
        if (harp_ingestion_options_get_option(options, "wavelength", &option_value) != 0)
        {
            ingestion_done(info);
            return -1;
        }
        if (strcmp(option_value, "532") == 0)
        {
            info->wavelength = 532;
        }
        else if (strcmp(option_value, "1064") == 0)
        {
            info->wavelength = 1064;
        }
        else
        {
            harp_set_error(HARP_ERROR_INGESTION, "incorrect wavelength option, it must be 532 or 1064.");
            ingestion_done(info);
            return -1;
        }
    }
    else
    {
        harp_set_error(HARP_ERROR_INGESTION, "the wavelength option has not been filled in.");
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

void register_time_specific_fields(harp_product_definition *product_definition, short include_angles)
{
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };
    const char *description;
    const char *path;

    /* datetime */
    description = "time during midpoint of the measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double, 1, dimension_type,
                                                   NULL, description, "seconds since 2000-01-01", NULL, read_datetime);
    path = "/Profile_Time[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* latitude */
    description = "latitude of the ground pixel center";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_double, 1, dimension_type,
                                                   NULL, description, "degree_north", NULL, read_latitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0f, 90.0f);
    path = "/Latitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* longitude */
    description = "longitude of the ground pixel center";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_double, 1, dimension_type,
                                                   NULL, description, "degree_east", NULL, read_longitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0f, 180.0f);
    path = "/Longitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    if (include_angles)
    {
        /* solar_zenith_angle */
        description = "solar zenith angle";
        variable_definition =
            harp_ingestion_register_variable_full_read(product_definition, "solar_zenith_angle", harp_type_double, 1,
                                                       dimension_type, NULL, description, "degree", NULL,
                                                       read_solar_zenith_angle);
        path = "/Solar_Zenith_Angle[]";
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

        /* solar_azimuth_angle */
        description = "solar azimuth angle";
        variable_definition =
            harp_ingestion_register_variable_full_read(product_definition, "solar_azimuth_angle", harp_type_double, 1,
                                                       dimension_type, NULL, description, "degree", NULL,
                                                       read_solar_azimuth_angle);
        path = "/Solar_Azimuth_Angle[]";
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
    }

    /* tropopause_altitude */
    description = "tropopause altitude";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "tropopause_altitude", harp_type_double, 1,
                                                   dimension_type, NULL, description, "km", NULL,
                                                   read_tropopause_altitude);
    path = "/Tropopause_Height[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

void register_optical_thickness_fields(harp_product_definition *product_definition)
{
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };
    const char *description;
    const char *path;

    /* tropospheric_aerosol_optical_depth */
    description = "tropospheric aerosol optical depth";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "tropospheric_aerosol_optical_depth",
                                                   harp_type_double, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_tropospheric_aerosol_optical_depth);
    path = "/Column_Optical_Depth_Tropospheric_Aerosols_532[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "wavelength=532", path, NULL);
    path = "/Column_Optical_Depth_Tropospheric_Aerosols_1064[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "wavelength=1064", path, NULL);

    /* tropospheric_aerosol_optical_depth_uncertainty */
    description = "tropospheric aerosol optical depth uncertainty";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "tropospheric_aerosol_optical_depth_uncertainty",
                                                   harp_type_double, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_tropospheric_aerosol_optical_depth_uncertainty);
    path = "/Column_Optical_Depth_Tropospheric_Aerosols_Uncertainty_532[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "wavelength=532", path, NULL);
    path = "/Column_Optical_Depth_Tropospheric_Aerosols_Uncertainty_1064[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "wavelength=1064", path, NULL);

    /* stratospheric_aerosol_optical_depth */
    description = "stratospheric aerosol optical depth";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "stratospheric_aerosol_optical_depth",
                                                   harp_type_double, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_stratospheric_aerosol_optical_depth);
    path = "/Column_Optical_Depth_Stratospheric_Aerosols_532[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "wavelength=532", path, NULL);
    path = "/Column_Optical_Depth_Stratospheric_Aerosols_1064[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "wavelength=1064", path, NULL);

    /* stratospheric_aerosol_optical_depth_uncertainty */
    description = "stratospheric aerosol optical depth uncertainty";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "stratospheric_aerosol_optical_depth_uncertainty", harp_type_double,
                                                   1, dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_stratospheric_aerosol_optical_depth_uncertainty);
    path = "/Column_Optical_Depth_Stratospheric_Aerosols_Uncertainty_532[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "wavelength=532", path, NULL);
    path = "/Column_Optical_Depth_Stratospheric_Aerosols_Uncertainty_1064[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "wavelength=1064", path, NULL);

    /* cloud_optical_depth */
    description = "cloud optical depth";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_optical_depth", harp_type_double, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_cloud_optical_depth);
    path = "/Column_Optical_Depth_Cloud_532[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_optical_depth_uncertainty */
    description = "cloud optical depth uncertainty";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_optical_depth_uncertainty",
                                                   harp_type_double, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL, read_cloud_optical_depth_uncertainty);
    path = "/Column_Optical_Depth_Cloud_Uncertainty_532[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

void register_altitude_specific_fields(harp_product_definition *product_definition)
{
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_vertical };
    const char *description;
    const char *path;

    /* altitude */
    description = "altitude";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "altitude", harp_type_double, 1,
                                                   &(dimension_type[1]), NULL, description, "km", NULL, read_altitude);
    path = "/metadata/Lidar_Data_Altitudes[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* pressure */
    description = "pressure";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "pressure", harp_type_double, 2,
                                                   dimension_type, NULL, description, "hPa", NULL, read_pressure);
    path = "/Pressure[,]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* temperature */
    description = "temperature";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "temperature", harp_type_double, 2,
                                                   dimension_type, NULL, description, "K", NULL, read_temperature);
    path = "/Temperature[,]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* aerosol_extinction_coefficient */
    description = "aerosol_extinction_coefficient";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "aerosol_extinction_coefficient",
                                                   harp_type_double, 2, dimension_type, NULL, description,
                                                   "km^-1", NULL, read_aerosol_extinction_coefficient);
    path = "/Extinction_Coefficient_532[,]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "wavelength=532", path, NULL);
    path = "/Extinction_Coefficient_1064[,]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "wavelength=1064", path, NULL);

    /* aerosol_extinction_coefficient_uncertainty */
    description = "aerosol_extinction_coefficient_uncertainty";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "aerosol_extinction_coefficient_uncertainty",
                                                   harp_type_double, 2, dimension_type, NULL, description,
                                                   "km^-1", NULL, read_aerosol_extinction_coefficient_uncertainty);
    path = "/Extinction_Coefficient_Uncertainty_532[,]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "wavelength=532", path, NULL);
    path = "/Extinction_Coefficient_Uncertainty_1064[,]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "wavelength=1064", path, NULL);

    /* backscatter_coefficient */
    description = "backscatter_coefficient";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "backscatter_coefficient",
                                                   harp_type_double, 2, dimension_type, NULL, description,
                                                   "km^-1", NULL, read_backscatter_coefficient);
    path = "/Total_Backscatter_Coefficient_532[,]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "wavelength=532", path, NULL);
    path = "/Backscatter_Coefficient_1064[,]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "wavelength=1064", path, NULL);

    /* backscatter_coefficient_uncertainty */
    description = "backscatter_coefficient_uncertainty";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "backscatter_coefficient_uncertainty",
                                                   harp_type_double, 2, dimension_type, NULL, description,
                                                   "km^-1", NULL, read_backscatter_coefficient_uncertainty);
    path = "/Total_Backscatter_Coefficient_Uncertainty_532[,]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "wavelength=532", path, NULL);
    path = "/Backscatter_Coefficient_Uncertainty_1064[,]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "wavelength=1064", path, NULL);
}

static void register_aerosol_layer_l2(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;

    module =
        harp_ingestion_register_module_coda("CALIPSO_L2_CAL_LID_ALay", "CALIPSO LIDAR", "CALIPSO", "CAL_LID_L2_ALay",
                                            "CALIOP L2 Aerosol Layers", ingestion_init, ingestion_done);

    harp_ingestion_register_option(module, "wavelength", "the wavelength whose measurements are ingested; option values"
                                   " are '532' and '1064'", 2, wavelength_options);

    product_definition = harp_ingestion_register_product(module, "CALIPSO_L2_CAL_LID_ALay", NULL, read_dimensions);

    register_time_specific_fields(product_definition, TRUE);
    register_optical_thickness_fields(product_definition);
}

static void register_aerosol_profile_l2(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;

    module =
        harp_ingestion_register_module_coda("CALIPSO_L2_CAL_LID_APro", "CALIPSO LIDAR", "CALIPSO", "CAL_LID_L2_APro",
                                            "CALIOP L2 Aerosol Profiles", ingestion_init, ingestion_done);

    harp_ingestion_register_option(module, "wavelength", "the wavelength whose measurements are ingested; option values"
                                   " are '532' and '1064'", 2, wavelength_options);

    product_definition = harp_ingestion_register_product(module, "CALIPSO_L2_CAL_LID_APro", NULL, read_dimensions);

    register_time_specific_fields(product_definition, FALSE);
    register_optical_thickness_fields(product_definition);
    register_altitude_specific_fields(product_definition);
}

static void register_cloud_layer_l2(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;

    module =
        harp_ingestion_register_module_coda("CALIPSO_L2_CAL_LID_CLay", "CALIPSO LIDAR", "CALIPSO", "CAL_LID_L2_CLay",
                                            "CALIOP L2 Cloud Layers", ingestion_init, ingestion_done);

    harp_ingestion_register_option(module, "wavelength", "the wavelength whose measurements are ingested; option values"
                                   " are '532' and '1064'", 2, wavelength_options);

    product_definition = harp_ingestion_register_product(module, "CALIPSO_L2_CAL_LID_CLay", NULL, read_dimensions);

    register_time_specific_fields(product_definition, TRUE);
    register_optical_thickness_fields(product_definition);
}

static void register_cloud_profile_l2(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;

    module =
        harp_ingestion_register_module_coda("CALIPSO_L2_CAL_LID_CPro", "CALIPSO LIDAR", "CALIPSO", "CAL_LID_L2_CPro",
                                            "CALIOP L2 Cloud Profiles", ingestion_init, ingestion_done);

    harp_ingestion_register_option(module, "wavelength", "the wavelength whose measurements are ingested; option values"
                                   " are '532' and '1064'", 2, wavelength_options);

    product_definition = harp_ingestion_register_product(module, "CALIPSO_L2_CAL_LID_CPro", NULL, read_dimensions);

    register_time_specific_fields(product_definition, FALSE);
    register_optical_thickness_fields(product_definition);
    register_altitude_specific_fields(product_definition);
}

static void register_merged_layer_l2(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;

    module =
        harp_ingestion_register_module_coda("CALIPSO_L2_CAL_LID_MLay", "CALIPSO LIDAR", "CALIPSO", "CAL_LID_L2_MLay",
                                            "CALIOP L2 Merged Aerosol and Cloud Layers", ingestion_init,
                                            ingestion_done);

    harp_ingestion_register_option(module, "wavelength", "the wavelength whose measurements are ingested; option values"
                                   " are '532' and '1064'", 2, wavelength_options);

    product_definition = harp_ingestion_register_product(module, "CALIPSO_L2_CAL_LID_MLay", NULL, read_dimensions);

    register_time_specific_fields(product_definition, TRUE);
}

int harp_ingestion_module_calipso_l2_init(void)
{
    register_aerosol_layer_l2();
    register_aerosol_profile_l2();
    register_cloud_layer_l2();
    register_cloud_profile_l2();
    register_merged_layer_l2();

    return 0;
}
