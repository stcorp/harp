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

#define SECONDS_PER_DAY                  86400
#define SECONDS_FROM_1970_TO_2000    946684800

#define CHECKED_MALLOC(v, s) v = malloc(s); if (v == NULL) { harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)", s, __FILE__, __LINE__); return -1;}

#define MAX_WAVELENGTHS                     10

/* ------------------ Typedefs ------------------ */

typedef struct ingest_info_struct
{
    coda_product *product;
    long num_time;
    long num_wavelengths;
    long aod_wavelengths[MAX_WAVELENGTHS];
    double *values_buffer;
    char *aod_fieldname;
    char *aod_uncertainty_name;
} ingest_info;

/* -------------------- Code -------------------- */

static void ingestion_done(void *user_data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->values_buffer != NULL)
    {
        free(info->values_buffer);
    }
    if (info->aod_fieldname != NULL)
    {
        free(info->aod_fieldname);
    }
    if (info->aod_uncertainty_name != NULL)
    {
        free(info->aod_uncertainty_name);
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
        *double_data = (*double_data * SECONDS_PER_DAY) - SECONDS_FROM_1970_TO_2000;
        double_data++;
    }
    return retval;
}

static int read_latitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "/latitude", info->num_time, data);
}

static int read_longitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "/longitude", info->num_time, data);
}

static int read_one_corner(ingest_info *info, const char *fieldname, long corner_offset, double *double_data)
{
    harp_array latitudes_one_corner;
    double *src, *dest;
    long i;

    latitudes_one_corner.double_data = info->values_buffer;
    if (read_dataset(info, fieldname, info->num_time, latitudes_one_corner) != 0)
    {
        return -1;
    }
    src = latitudes_one_corner.double_data;
    dest = double_data + corner_offset;
    for (i = 0; i < info->num_time; i++)
    {
        *dest = *src;
        src++;
        dest += info->num_wavelengths;
    }
    return 0;
}

static int read_latitude_bounds(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (read_one_corner(info, "/pixel_corner_latitude1", 0, data.double_data) != 0)
    {
        return -1;
    }
    if (read_one_corner(info, "/pixel_corner_latitude2", 1, data.double_data) != 0)
    {
        return -1;
    }
    if (read_one_corner(info, "/pixel_corner_latitude3", 2, data.double_data) != 0)
    {
        return -1;
    }
    if (read_one_corner(info, "/pixel_corner_latitude4", 3, data.double_data) != 0)
    {
        return -1;
    }
    return 0;
}

static int read_longitude_bounds(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (read_one_corner(info, "/pixel_corner_longitude1", 0, data.double_data) != 0)
    {
        return -1;
    }
    if (read_one_corner(info, "/pixel_corner_longitude2", 1, data.double_data) != 0)
    {
        return -1;
    }
    if (read_one_corner(info, "/pixel_corner_longitude3", 2, data.double_data) != 0)
    {
        return -1;
    }
    if (read_one_corner(info, "/pixel_corner_longitude4", 3, data.double_data) != 0)
    {
        return -1;
    }
    return 0;
}

static int read_aod_one_wavelength(ingest_info *info, harp_array data, const char *fieldname, long offset_of_wavelength)
{
    harp_array measurements_one_wavelength;
    double *dest, *src;
    long i;

    measurements_one_wavelength.double_data = info->values_buffer;
    if (read_dataset(info, fieldname, info->num_time, measurements_one_wavelength) != 0)
    {
        return -1;
    }
    dest = data.double_data + offset_of_wavelength;
    src = measurements_one_wavelength.double_data;
    for (i = 0; i < info->num_time; i++)
    {
        *dest = *src;
        dest += info->num_wavelengths;
        src++;
    }

    return 0;
}

static int read_aerosol_optical_depth(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long i;
    char fieldname[81];

    for (i = 0; i < info->num_wavelengths; i++)
    {
        sprintf(fieldname, "/%s%ld", info->aod_fieldname, info->aod_wavelengths[i]);
        if (read_aod_one_wavelength(info, data, fieldname, i) != 0)
        {
            return -1;
        }
    }

    return 0;
}

static int read_aerosol_optical_depth_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    double nan, *dest;
    long i, j;
    char fieldname[81];

    nan = coda_NaN();
    for (i = 0; i < info->num_wavelengths; i++)
    {
        sprintf(fieldname, "/%s%ld_%s", info->aod_fieldname, info->aod_wavelengths[i], info->aod_uncertainty_name);
        if (read_aod_one_wavelength(info, data, fieldname, i) != 0)
        {
            if (coda_errno == CODA_ERROR_INVALID_NAME)
            {
                dest = data.double_data + i;
                for (j = 0; j < info->num_time; j++)
                {
                    *dest = nan;
                    dest += info->num_wavelengths;
                }
            }
            else
            {
                return -1;
            }
        }
    }

    return 0;
}

static int read_wavelength(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    double *dest;
    long i, j;

    dest = data.double_data;
    for (i = 0; i < info->num_time; i++)
    {
        for (j = 0; j < info->num_wavelengths; j++)
        {
            *dest = info->aod_wavelengths[j];
            dest++;
        }
    }

    return 0;
}

static int read_sensor_zenith_angle(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int retval;

    retval = read_dataset(info, "/satellite_zenith_at_center", info->num_time, data);
    if (retval != 0)
    {
        retval = read_dataset(info, "/satellite_zenith", info->num_time, data);
    }
    return 0;
}

static int read_solar_zenith_angle(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int retval;

    retval = read_dataset(info, "/sun_zenith_at_center", info->num_time, data);
    return retval;
}

static int read_dimensions(void *user_data, long dimension[HARP_NUM_DIM_TYPES])
{
    ingest_info *info = (ingest_info *)user_data;

    dimension[harp_dimension_time] = info->num_time;
    dimension[harp_dimension_spectral] = info->num_wavelengths;

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
    if (coda_cursor_goto(&cursor, "/latitude") != 0)
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
    CHECKED_MALLOC(info->values_buffer, info->num_time * sizeof(double));

    return 0;
}

/* Start of code that is specific for the AATSR and ATSR2 instruments */

static int exclude_when_multiple_zenith_angles(void *user_data)
{
    ingest_info *info = (ingest_info *)user_data;
    coda_cursor cursor;
    long coda_num_elements;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        return TRUE;
    }
    if (coda_cursor_goto(&cursor, "/satellite_zenith_at_center") != 0)
    {
        return TRUE;
    }
    if (coda_cursor_get_num_elements(&cursor, &coda_num_elements) != 0)
    {
        return TRUE;
    }
    if (coda_num_elements != info->num_time)
    {
        return TRUE;
    }
    return FALSE;
}

static int ingestion_init_aatsr_atsr2(const harp_ingestion_module *module, coda_product *product,
                                      const harp_ingestion_options *options, harp_product_definition **definition,
                                      void **user_data)
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
    info->num_wavelengths = 4;
    info->aod_wavelengths[0] = 550;
    info->aod_wavelengths[1] = 670;
    info->aod_wavelengths[2] = 870;
    info->aod_wavelengths[3] = 1600;
    info->aod_fieldname = strdup("AOD");
    info->aod_uncertainty_name = strdup("uncertainty");
    *definition = *module->product_definition;
    *user_data = info;

    return 0;
}

static void register_aatsr_atsr2_product(harp_ingestion_module *module, char *productname)
{
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_spectral };
    harp_dimension_type bounds_dimension_type[2] = { harp_dimension_time, harp_dimension_independent };
    long bounds_dimension[2] = { -1, 4 };
    const char *description;
    const char *path;

    product_definition = harp_ingestion_register_product(module, productname, NULL, read_dimensions);

    /* datetime */
    description = "time of the measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double, 1, dimension_type,
                                                   NULL, description, "seconds since 2000-01-01", NULL, read_datetime);
    path = "/time[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* latitude */
    description = "latitude of the ground pixel center";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_double, 1, dimension_type,
                                                   NULL, description, "degree_north", NULL, read_latitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0f, 90.0f);
    path = "/latitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* longitude */
    description = "longitude of the ground pixel center";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_double, 1, dimension_type,
                                                   NULL, description, "degree_east", NULL, read_longitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0f, 180.0f);
    path = "/longitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* latitude_bounds */
    description = "corner latitudes for the ground pixel of the measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "latitude_bounds", harp_type_double, 2,
                                                   bounds_dimension_type, bounds_dimension, description, "degree_north",
                                                   NULL, read_latitude_bounds);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    path = "/pixel_corner_latitude1[], /pixel_corner_latitude2[], /pixel_corner_latitude3[], /pixel_corner_latitude4[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* longitude_bounds */
    description = "corner longitudes for the ground pixel of the measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "longitude_bounds", harp_type_double, 2,
                                                   bounds_dimension_type, bounds_dimension, description, "degree_east",
                                                   NULL, read_longitude_bounds);
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0f, 180.0f);
    path =
        "/pixel_corner_longitude1[], /pixel_corner_longitude2[], /pixel_corner_longitude3[], /pixel_corner_longitude4[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* aerosol_optical_depth */
    description = "aerosol optical depth";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "aerosol_optical_depth", harp_type_double, 2,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_aerosol_optical_depth);
    path = "/AOD550[], /AOD670[], /AOD870[], /AOD1600[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* aerosol_optical_depth_uncertainty */
    description = "uncertainty of the aerosol optical depth";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "aerosol_optical_depth_uncertainty",
                                                   harp_type_double, 2, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_aerosol_optical_depth_uncertainty);
    path = "/AOD550_uncertainty[], /AOD670_uncertainty[], /AOD870_uncertainty[], /AOD1600_uncertainty[]";
    description =
        "depending on how the data is processed, uncertainty data is not always available for all wavelengths. If the data is not available, NaN values are used.";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* wavelength */
    description = "wavelengths of the measurements";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "wavelength", harp_type_double, 2,
                                                   dimension_type, NULL, description, "nm", NULL, read_wavelength);
    description = "fixed values";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, NULL, description);

    /* sensor_zenith_angle */
    description = "sensor zenith angle for nadir view";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "sensor_zenith_angle", harp_type_double, 1,
                                                   dimension_type, NULL, description, "degree",
                                                   exclude_when_multiple_zenith_angles, read_sensor_zenith_angle);
    path = "/satellite_zenith_at_center[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* solar_zenith_angle */
    description = "solar zenith angle for nadir view";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "solar_zenith_angle", harp_type_double, 1,
                                                   dimension_type, NULL, description, "degree",
                                                   exclude_when_multiple_zenith_angles, read_solar_zenith_angle);
    path = "/sun_zenith_at_center[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

static void register_module_l2_aatsr_atsr2(void)
{
    harp_ingestion_module *module;

    module =
        harp_ingestion_register_module_coda("ESACCI_AEROSOL_L2_AATSR", "Aerosol CCI", "ESACCI_AEROSOL",
                                            "AATSR_L2", "CCI L2 Aerosol profile from AATSR",
                                            ingestion_init_aatsr_atsr2, ingestion_done);
    register_aatsr_atsr2_product(module, "ESACCI_AEROSOL_L2_AATSR");

    module =
        harp_ingestion_register_module_coda("ESACCI_AEROSOL_L2_ATSR2", "Aerosol CCI", "ESACCI_AEROSOL",
                                            "ATSR2_L2", "CCI L2 Aerosol profile from ATSR-2",
                                            ingestion_init_aatsr_atsr2, ingestion_done);
    register_aatsr_atsr2_product(module, "ESACCI_AEROSOL_L2_ATSR2");
}

/* Start of code that is specific for the MERIS instrument */

static int ingestion_init_meris(const harp_ingestion_module *module, coda_product *product,
                                const harp_ingestion_options *options, harp_product_definition **definition,
                                void **user_data)
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
    info->num_wavelengths = 2;
    info->aod_wavelengths[0] = 550;
    info->aod_wavelengths[1] = 865;
    info->aod_fieldname = strdup("AOD");
    info->aod_uncertainty_name = strdup("std");
    *definition = *module->product_definition;
    *user_data = info;

    return 0;
}

static void register_module_l2_meris(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_spectral };
    harp_dimension_type bounds_dimension_type[2] = { harp_dimension_time, harp_dimension_independent };
    long bounds_dimension[2] = { -1, 4 };
    const char *description;
    const char *path;

    module =
        harp_ingestion_register_module_coda("ESACCI_AEROSOL_L2_MERIS_ALAMO", "Aerosol CCI", "ESACCI_AEROSOL",
                                            "MERIS_ALAMO_L2", "CCI L2 Aerosol profile from MERIS processed by ALAMO",
                                            ingestion_init_meris, ingestion_done);

    product_definition =
        harp_ingestion_register_product(module, "ESACCI_AEROSOL_L2_MERIS_ALAMO", NULL, read_dimensions);

    /* latitude */
    description = "latitude of the ground pixel center";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_double, 1, dimension_type,
                                                   NULL, description, "degree_north", NULL, read_latitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0f, 90.0f);
    path = "/latitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* longitude */
    description = "longitude of the ground pixel center";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_double, 1, dimension_type,
                                                   NULL, description, "degree_east", NULL, read_longitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0f, 180.0f);
    path = "/longitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* latitude_bounds */
    description = "corner latitudes for the ground pixel of the measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "latitude_bounds", harp_type_double, 2,
                                                   bounds_dimension_type, bounds_dimension, description, "degree_north",
                                                   NULL, read_latitude_bounds);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    path = "/pixel_corner_latitude1[], /pixel_corner_latitude2[], /pixel_corner_latitude3[], /pixel_corner_latitude4[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* longitude_bounds */
    description = "corner longitudes for the ground pixel of the measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "longitude_bounds", harp_type_double, 2,
                                                   bounds_dimension_type, bounds_dimension, description, "degree_east",
                                                   NULL, read_longitude_bounds);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    path =
        "/pixel_corner_longitude1[], /pixel_corner_longitude2[], /pixel_corner_longitude3[], /pixel_corner_longitude4[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* aerosol_optical_depth */
    description = "aerosol optical depth";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "aerosol_optical_depth", harp_type_double, 2,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_aerosol_optical_depth);
    path = "/AOD550[], /AOD865[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* aerosol_optical_depth_uncertainty */
    description = "uncertainty of the aerosol optical depth";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "aerosol_optical_depth_uncertainty",
                                                   harp_type_double, 2, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_aerosol_optical_depth_uncertainty);
    path = "/AOD550_std[], /AOD865_std[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* wavelength */
    description = "wavelengths of the measurements";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "wavelength", harp_type_double, 2,
                                                   dimension_type, NULL, description, "nm", NULL, read_wavelength);
    description = "fixed values";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, NULL, description);
}

/* Start of code that is specific for the IASI instrument */

static int ingestion_init_iasi(const harp_ingestion_module *module, coda_product *product,
                               const harp_ingestion_options *options, harp_product_definition **definition,
                               void **user_data)
{
    ingest_info *info;
    coda_cursor cursor;

    (void)options;

    CHECKED_MALLOC(info, sizeof(ingest_info));
    info->product = product;
    if (init_dimensions(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }
    info->num_wavelengths = 3;
    info->aod_wavelengths[0] = 550;
    info->aod_wavelengths[1] = 10000;
    info->aod_wavelengths[2] = 11000;
    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, "/D_AOD550") == 0)
    {
        info->aod_fieldname = strdup("D_AOD");
    }
    else
    {
        info->aod_fieldname = strdup("Daod");
    }
    info->aod_uncertainty_name = strdup("uncertainty");
    *definition = *module->product_definition;
    *user_data = info;

    return 0;
}

static void register_iasi_product(harp_ingestion_module *module, char *productname)
{
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_spectral };
    const char *description;
    const char *path;

    product_definition = harp_ingestion_register_product(module, productname, NULL, read_dimensions);

    /* datetime */
    description = "time of the measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double, 1, dimension_type,
                                                   NULL, description, "seconds since 2000-01-01", NULL, read_datetime);
    path = "/time[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* latitude */
    description = "latitude of the ground pixel center";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_double, 1, dimension_type,
                                                   NULL, description, "degree_north", NULL, read_latitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0f, 90.0f);
    path = "/latitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* longitude */
    description = "longitude of the ground pixel center";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_double, 1, dimension_type,
                                                   NULL, description, "degree_east", NULL, read_longitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0f, 180.0f);
    path = "/longitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* dust_aerosol_optical_depth */
    description = "dust aerosol optical depth";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "dust_aerosol_optical_depth", harp_type_double,
                                                   2, dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_aerosol_optical_depth);
    path = "/D_AOD550[], /D_AOD10000[], /D_AOD11000[]";
    harp_variable_definition_add_mapping(variable_definition, "data processed by DLR or ULB", NULL, path, NULL);
    path = "/Daod550[], /Daod10000[], /Daod11000[]";
    harp_variable_definition_add_mapping(variable_definition, "data processed by LMD", NULL, path, NULL);

    /* dust_aerosol_optical_depth_uncertainty */
    description = "uncertainty of the dust aerosol optical depth";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "dust_aerosol_optical_depth_uncertainty",
                                                   harp_type_double, 2, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_aerosol_optical_depth_uncertainty);
    description =
        "depending on how the data is processed, uncertainty data is not always available for all wavelengths. If the data is not available, NaN values are used.";
    path = "/D_AOD11000_uncertainty[]";
    harp_variable_definition_add_mapping(variable_definition, "data processed by DLR", NULL, path, description);
    path = "/Daod10000_uncertainty[]";
    harp_variable_definition_add_mapping(variable_definition, "data processed by LMD", NULL, path, description);
    path = "/D_AOD10000_uncertainty[]";
    harp_variable_definition_add_mapping(variable_definition, "data processed by DLR", NULL, path, description);

    /* wavelength */
    description = "wavelengths of the measurements";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "wavelength", harp_type_double, 2,
                                                   dimension_type, NULL, description, "nm", NULL, read_wavelength);
    description = "fixed values";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, NULL, description);

    /* sensor_zenith_angle */
    description = "sensor zenith angle";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "sensor_zenith_angle", harp_type_double, 1,
                                                   dimension_type, NULL, description, "degree", NULL,
                                                   read_sensor_zenith_angle);
    path = "/satellite_zenith_at_center[]";
    harp_variable_definition_add_mapping(variable_definition, "data processed by DLR or ULB", NULL, path, NULL);
    path = "/satellite_zenith[]";
    harp_variable_definition_add_mapping(variable_definition, "data processed by LMD", NULL, path, NULL);
}

static void register_module_l2_iasi(void)
{
    harp_ingestion_module *module;

    module =
        harp_ingestion_register_module_coda("ESACCI_AEROSOL_L2_IASI", "Aerosol CCI", "ESACCI_AEROSOL",
                                            "IASI_L2", "CCI L2 Aerosol profile from IASI",
                                            ingestion_init_iasi, ingestion_done);
    register_iasi_product(module, "ESACCI_AEROSOL_L2_IASI");
}

/* Main procedure for all instruments */

int harp_ingestion_module_cci_l2_aerosol_init(void)
{
    register_module_l2_aatsr_atsr2();
    register_module_l2_meris();
    register_module_l2_iasi();
    return 0;
}
