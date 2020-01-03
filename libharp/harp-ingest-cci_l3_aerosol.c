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
#include "harp-constants.h"
#include "harp-ingestion.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ------------------- Defines ------------------ */

#define SECONDS_PER_DAY                  86400
#define SECONDS_FROM_1970_TO_2000    946684800

#define CHECKED_MALLOC(v, s) v = malloc(s); if (v == NULL) { harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)", s, __FILE__, __LINE__); return -1;}

#define MAX_WAVELENGTHS                     10

/* ------------------ Typedefs ------------------ */

typedef struct ingest_info_struct
{
    coda_product *product;
    long num_wavelengths;
    long num_latitudes;
    long num_longitudes;
    long num_altitudes;
    long aod_wavelengths[MAX_WAVELENGTHS];
    double *values_buffer;
    char *aod_fieldname;
    char *aod_uncertainty_name;
    int zenith_fields_present;
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

static int read_partial_dataset(ingest_info *info, const char *path, long offset, long num_elements, harp_array data)
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
    if (coda_num_elements < num_elements)
    {
        harp_set_error(HARP_ERROR_INGESTION, "dataset has %ld elements (expected %ld)", coda_num_elements,
                       num_elements);
        harp_add_coda_cursor_path_to_error_message(&cursor);
        harp_add_error_message(" (%s:%lu)", __FILE__, __LINE__);
        return -1;
    }
    if (coda_cursor_read_double_partial_array(&cursor, offset, num_elements, data.double_data) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    fill_value.double_data = -999.0;
    harp_array_replace_fill_value(harp_type_double, num_elements, data, fill_value);

    return 0;
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

static int read_latitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "/latitude", info->num_latitudes, data);
}

static int read_longitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "/longitude", info->num_longitudes, data);
}

static int read_aod_one_wavelength(ingest_info *info, harp_array data, const char *fieldname, long offset_of_wavelength)
{
    harp_array measurements_one_wavelength;
    double *dest, *src;
    long i;

    measurements_one_wavelength.double_data = info->values_buffer;
    if (read_dataset(info, fieldname, info->num_latitudes * info->num_longitudes, measurements_one_wavelength) != 0)
    {
        return -1;
    }
    dest = data.double_data + offset_of_wavelength;
    src = measurements_one_wavelength.double_data;
    for (i = 0; i < (info->num_latitudes * info->num_longitudes); i++)
    {
        *dest = *src;
        dest++;
        src++;
    }

    return 0;
}

static int read_aerosol_optical_depth(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    double nan, *dest;
    long i, j;
    char fieldname[81];

    nan = coda_NaN();
    for (i = 0; i < info->num_wavelengths; i++)
    {
        sprintf(fieldname, info->aod_fieldname, info->aod_wavelengths[i]);
        if (read_aod_one_wavelength(info, data, fieldname, i * info->num_latitudes * info->num_longitudes) != 0)
        {
            if (coda_errno == CODA_ERROR_INVALID_NAME)
            {
                dest = data.double_data + (i * info->num_latitudes * info->num_longitudes);
                for (j = 0; j < (info->num_latitudes * info->num_longitudes); j++)
                {
                    *dest = nan;
                    dest++;
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

static int read_aerosol_optical_depth_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    double nan, *dest;
    long i, j;
    char fieldname[81];

    nan = coda_NaN();
    for (i = 0; i < info->num_wavelengths; i++)
    {
        sprintf(fieldname, info->aod_uncertainty_name, info->aod_wavelengths[i]);
        if (read_aod_one_wavelength(info, data, fieldname, i * info->num_latitudes * info->num_longitudes) != 0)
        {
            if (coda_errno == CODA_ERROR_INVALID_NAME)
            {
                dest = data.double_data + (i * info->num_latitudes * info->num_longitudes);
                for (j = 0; j < (info->num_latitudes * info->num_longitudes); j++)
                {
                    *dest = nan;
                    dest++;
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

static int read_altitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (read_dataset(info, "/altitude", info->num_altitudes, data) != 0)
    {
        return -1;
    }

    return 0;
}

static int read_aerosol_extinction_coefficient(void *user_data, harp_array data)
{
    harp_array measurements_one_wavelength;
    ingest_info *info = (ingest_info *)user_data;
    double *src, *dest;
    long i, j, k;
    char fieldname[81];

    measurements_one_wavelength.double_data = info->values_buffer;
    for (i = 0; i < info->num_wavelengths; i++)
    {
        sprintf(fieldname, "/AEX%ld", info->aod_wavelengths[i]);
        if (read_dataset
            (info, fieldname, info->num_altitudes * info->num_latitudes * info->num_longitudes,
             measurements_one_wavelength) != 0)
        {
            return -1;
        }
        /* Copy from [altitude][latitude][longitude]             */
        /* to the [wavelengths][latitudes][longitudes][altitude] */
        src = info->values_buffer;
        for (j = 0; j < info->num_altitudes; j++)
        {
            dest = data.double_data + (i * info->num_latitudes * info->num_longitudes * info->num_altitudes) + j;
            for (k = 0; k < (info->num_latitudes * info->num_longitudes); k++)
            {
                *dest = *src;
                dest += info->num_altitudes;
                src++;
            }
        }
    }

    return 0;
}

static int read_aerosol_extinction_coefficient_uncertainty(void *user_data, harp_array data)
{
    harp_array measurements_one_wavelength;
    ingest_info *info = (ingest_info *)user_data;
    double *src, *dest;
    long i, j, k;
    char fieldname[81];

    measurements_one_wavelength.double_data = info->values_buffer;
    for (i = 0; i < info->num_wavelengths; i++)
    {
        sprintf(fieldname, "/AEX%ld_uncertainty", info->aod_wavelengths[i]);
        if (read_dataset
            (info, fieldname, info->num_altitudes * info->num_latitudes * info->num_longitudes,
             measurements_one_wavelength) != 0)
        {
            return -1;
        }
        /* Copy from [altitude][latitude][longitude]             */
        /* to the [wavelengths][latitudes][longitudes][altitude] */
        src = info->values_buffer;
        for (j = 0; j < info->num_altitudes; j++)
        {
            dest = data.double_data + (i * info->num_latitudes * info->num_longitudes * info->num_altitudes) + j;
            for (k = 0; k < (info->num_latitudes * info->num_longitudes); k++)
            {
                *dest = *src;
                dest += info->num_altitudes;
                src++;
            }
        }
    }

    return 0;
}

static int read_wavelength(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    double *dest;
    long i;

    dest = data.double_data;
    for (i = 0; i < info->num_wavelengths; i++)
    {
        *dest = info->aod_wavelengths[i];
        dest++;
    }

    return 0;
}

static int read_absorbing_aerosol_index(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "/absorbing_aerosol_index", info->num_latitudes * info->num_longitudes, data);
}

static int read_dimensions(void *user_data, long dimension[HARP_NUM_DIM_TYPES])
{
    ingest_info *info = (ingest_info *)user_data;

    dimension[harp_dimension_spectral] = info->num_wavelengths;
    dimension[harp_dimension_latitude] = info->num_latitudes;
    dimension[harp_dimension_longitude] = info->num_longitudes;
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
        harp_set_error(HARP_ERROR_INGESTION, "latitude dataset has %d dimensions, expected 1", num_coda_dims);
        harp_add_coda_cursor_path_to_error_message(&cursor);
        return -1;
    }
    info->num_latitudes = coda_dim[0];
    coda_cursor_goto_parent(&cursor);

    if (coda_cursor_goto(&cursor, "/longitude") != 0)
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
        harp_set_error(HARP_ERROR_INGESTION, "longitude dataset has %d dimensions, expected 1", num_coda_dims);
        harp_add_coda_cursor_path_to_error_message(&cursor);
        return -1;
    }
    info->num_longitudes = coda_dim[0];

    if (coda_cursor_goto(&cursor, "/altitude") != 0)
    {
        info->num_altitudes = 1;
    }
    else
    {
        if (coda_cursor_get_array_dim(&cursor, &num_coda_dims, coda_dim) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (num_coda_dims != 1)
        {
            harp_set_error(HARP_ERROR_INGESTION, "altitude dataset has %d dimensions, expected 1", num_coda_dims);
            harp_add_coda_cursor_path_to_error_message(&cursor);
            return -1;
        }
        info->num_altitudes = coda_dim[0];
    }

    if (coda_cursor_goto(&cursor, "/sun_zenith_mean") != 0)
    {
        info->zenith_fields_present = 0;
    }
    else
    {
        info->zenith_fields_present = 1;
    }

    return 0;
}

/* Start of code that is specific for the AATSR and ATSR2 instruments */

static int read_aatsr_atsr2_sensor_zenith_angle(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    /* The field is [2][latitude][longitude] but we only read the first [latitude][longitude] values */
    return read_partial_dataset(info, "/satellite_zenith_mean", 0, info->num_latitudes * info->num_longitudes, data);
}

static int read_aatsr_atsr2_solar_zenith_angle(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    /* The field is [2][latitude][longitude] but we only read the first [latitude][longitude] values */
    return read_partial_dataset(info, "/sun_zenith_mean", 0, info->num_latitudes * info->num_longitudes, data);
}

static int ingestion_init_aatsr_atsr2(const harp_ingestion_module *module, coda_product *product,
                                      const harp_ingestion_options *options, harp_product_definition **definition,
                                      void **user_data)
{
    coda_cursor cursor;
    ingest_info *info;

    (void)options;

    CHECKED_MALLOC(info, sizeof(ingest_info));
    info->product = product;
    if (init_dimensions(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }
    CHECKED_MALLOC(info->values_buffer,
                   info->num_latitudes * info->num_longitudes * info->num_altitudes * sizeof(double));

    info->num_wavelengths = 4;
    info->aod_wavelengths[0] = 550;
    info->aod_wavelengths[1] = 670;
    info->aod_wavelengths[2] = 870;
    info->aod_wavelengths[3] = 1600;
    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    info->aod_fieldname = strdup("//AOD%ld_mean");
    info->aod_uncertainty_name = strdup("//AOD%ld_sdev");
    *definition = *module->product_definition;
    *user_data = info;

    return 0;
}

static int include_zenith_angle(void *user_data)
{
    return ((ingest_info *)user_data)->zenith_fields_present;
}

static void register_aatsr_atsr2_product(harp_ingestion_module *module, char *productname)
{
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[3] =
        { harp_dimension_spectral, harp_dimension_latitude, harp_dimension_longitude };
    const char *description;
    const char *path;

    product_definition = harp_ingestion_register_product(module, productname, NULL, read_dimensions);

    /* latitude */
    description = "latitude of the ground pixel center";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_double, 1,
                                                   &(dimension_type[1]), NULL, description, "degree_north", NULL,
                                                   read_latitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0f, 90.0f);
    path = "/latitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* longitude */
    description = "longitude of the ground pixel center";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_double, 1,
                                                   &(dimension_type[2]), NULL, description, "degree_east", NULL,
                                                   read_longitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0f, 180.0f);
    path = "/longitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* aerosol_optical_depth */
    description = "aerosol optical depth";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "aerosol_optical_depth", harp_type_double, 3,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_aerosol_optical_depth);
    path = "/AOD550_mean[], /AOD670_mean[], /AOD870_mean[], /AOD1600_mean[]";
    description =
        "depending on how the data is processed, data is not always available for all wavelengths. If the data is not available, NaN values are used.";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* aerosol_optical_depth_uncertainty */
    description = "uncertainty of the aerosol optical depth";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "aerosol_optical_depth_uncertainty",
                                                   harp_type_double, 3, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_aerosol_optical_depth_uncertainty);
    path = "/AOD550_sdev[], /AOD670_sdev[], /AOD870_sdev[], /AOD1600_sdev[]";
    description =
        "depending on how the data is processed, uncertainty data is not always available for all wavelengths. If the data is not available, NaN values are used.";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* wavelength */
    description = "wavelengths of the measurements";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "wavelength", harp_type_double, 1,
                                                   dimension_type, NULL, description, "nm", NULL, read_wavelength);
    description = "fixed values";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, NULL, description);

    /* sensor_zenith_angle */
    description = "sensor zenith angle";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "sensor_zenith_angle", harp_type_double, 2,
                                                   &(dimension_type[1]), NULL, description, "degree",
                                                   include_zenith_angle, read_aatsr_atsr2_sensor_zenith_angle);
    path = "/satellite_zenith_mean[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* solar_zenith_angle */
    description = "solar zenith angle";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "solar_zenith_angle", harp_type_double, 2,
                                                   &(dimension_type[1]), NULL, description, "degree",
                                                   include_zenith_angle, read_aatsr_atsr2_solar_zenith_angle);
    path = "/sun_zenith_mean[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

static void register_module_l3_aatsr_atsr2(void)
{
    harp_ingestion_module *module;

    module =
        harp_ingestion_register_module_coda("ESACCI_AEROSOL_L3_AATSR", "Aerosol CCI", "ESACCI_AEROSOL",
                                            "AATSR_L3", "CCI L3 Aerosol profile from AATSR",
                                            ingestion_init_aatsr_atsr2, ingestion_done);
    register_aatsr_atsr2_product(module, "ESACCI_AEROSOL_L3_AATSR");

    module =
        harp_ingestion_register_module_coda("ESACCI_AEROSOL_L3_ATSR2", "Aerosol CCI", "ESACCI_AEROSOL",
                                            "ATSR2_L3", "CCI L3 Aerosol profile from ATSR-2",
                                            ingestion_init_aatsr_atsr2, ingestion_done);
    register_aatsr_atsr2_product(module, "ESACCI_AEROSOL_L3_ATSR2");
}

/* Start of code that is specific for the GOMOS instrument */

static int ingestion_init_gomos(const harp_ingestion_module *module, coda_product *product,
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
    CHECKED_MALLOC(info->values_buffer,
                   info->num_latitudes * info->num_longitudes * info->num_altitudes * sizeof(double));

    info->num_wavelengths = 1;
    info->aod_wavelengths[0] = 550;
    info->aod_fieldname = strdup("//S_AOD%ld");
    info->aod_uncertainty_name = strdup("//S_AOD%ld_uncertainty");
    *definition = *module->product_definition;
    *user_data = info;

    return 0;
}

static void register_module_l3_gomos(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[3] =
        { harp_dimension_spectral, harp_dimension_latitude, harp_dimension_longitude };
    harp_dimension_type aex_dimension_type[4] =
        { harp_dimension_spectral, harp_dimension_latitude, harp_dimension_longitude, harp_dimension_vertical };
    const char *description;
    const char *path;

    module =
        harp_ingestion_register_module_coda("ESACCI_AEROSOL_L3_GOMOS_AERGOM", "Aerosol CCI",
                                            "ESACCI_AEROSOL", "GOMOS_AERGOM_L3",
                                            "CCI L3 Aerosol profile from GOMOS processed by AERGOM",
                                            ingestion_init_gomos, ingestion_done);

    product_definition =
        harp_ingestion_register_product(module, "ESACCI_AEROSOL_L3_GOMOS_AERGOM", NULL, read_dimensions);

    /* latitude */
    description = "latitude of the ground pixel center";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_double, 1,
                                                   &(dimension_type[1]), NULL, description, "degree_north", NULL,
                                                   read_latitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0f, 90.0f);
    path = "/latitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* longitude */
    description = "longitude of the ground pixel center";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_double, 1,
                                                   &(dimension_type[2]), NULL, description, "degree_east", NULL,
                                                   read_longitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0f, 180.0f);
    path = "/longitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* stratospheric_aerosol_optical_depth */
    description = "stratospheric aerosol optical depth";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "stratospheric_aerosol_optical_depth",
                                                   harp_type_double, 3, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL, read_aerosol_optical_depth);
    path = "/S_AOD550[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* stratospheric_aerosol_optical_depth_uncertainty */
    description = "uncertainty of the stratospheric aerosol optical depth";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "stratospheric_aerosol_optical_depth_uncertainty", harp_type_double,
                                                   3, dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_aerosol_optical_depth_uncertainty);
    path = "/S_AOD550_uncertainty[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* altitude */
    description = "altitude of the aerosol extinction coefficient";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "altitude", harp_type_double, 1,
                                                   &(aex_dimension_type[3]), NULL, description, "km", NULL,
                                                   read_altitude);
    path = "/altitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* stratospheric_aerosol_extinction_coefficient */
    description = "stratospheric aerosol extinction";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "stratospheric_aerosol_extinction_coefficient",
                                                   harp_type_double, 4, aex_dimension_type, NULL, description, "km-1",
                                                   NULL, read_aerosol_extinction_coefficient);
    path = "/AEX550[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* stratospheric_aerosol_extinction_coefficient_uncertainty */
    description = "stratospheric aerosol extinction associated error";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "stratospheric_aerosol_extinction_coefficient_uncertainty",
                                                   harp_type_double, 4, aex_dimension_type, NULL, description, "km-1",
                                                   NULL, read_aerosol_extinction_coefficient_uncertainty);
    path = "/AEX550_uncertainty[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* wavelength */
    description = "wavelengths of the measurements";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "wavelength", harp_type_double, 1,
                                                   dimension_type, NULL, description, "nm", NULL, read_wavelength);
    description = "fixed values";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, NULL, description);
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
    CHECKED_MALLOC(info->values_buffer,
                   info->num_latitudes * info->num_longitudes * info->num_altitudes * sizeof(double));

    info->num_wavelengths = 2;
    info->aod_wavelengths[0] = 550;
    info->aod_wavelengths[1] = 865;
    info->aod_fieldname = strdup("//AOD%ld");
    info->aod_uncertainty_name = strdup("//AOD%ld_std");
    *definition = *module->product_definition;
    *user_data = info;

    return 0;
}

static void register_module_l3_meris(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[3] =
        { harp_dimension_spectral, harp_dimension_latitude, harp_dimension_longitude };
    const char *description;
    const char *path;

    module =
        harp_ingestion_register_module_coda("ESACCI_AEROSOL_L3_MERIS_ALAMO", "Aerosol CCI",
                                            "ESACCI_AEROSOL", "MERIS_ALAMO_L3",
                                            "CCI L3 Aerosol profile from MERIS processed by ALAMO",
                                            ingestion_init_meris, ingestion_done);

    product_definition =
        harp_ingestion_register_product(module, "ESACCI_AEROSOL_L3_MERIS_ALAMO", NULL, read_dimensions);

    /* latitude */
    description = "latitude of the ground pixel center";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_double, 1,
                                                   &(dimension_type[1]), NULL, description, "degree_north", NULL,
                                                   read_latitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0f, 90.0f);
    path = "/latitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* longitude */
    description = "longitude of the ground pixel center";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_double, 1,
                                                   &(dimension_type[2]), NULL, description, "degree_east", NULL,
                                                   read_longitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0f, 180.0f);
    path = "/longitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* aerosol_optical_depth */
    description = "aerosol optical depth";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "aerosol_optical_depth", harp_type_double, 3,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_aerosol_optical_depth);
    path = "/AOD550[], /AOD865[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* aerosol_optical_depth_uncertainty */
    description = "uncertainty of the aerosol optical depth";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "aerosol_optical_depth_uncertainty",
                                                   harp_type_double, 3, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_aerosol_optical_depth_uncertainty);
    path = "/AOD550_std[], /AOD865_std[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* wavelength */
    description = "wavelengths of the measurements";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "wavelength", harp_type_double, 1,
                                                   dimension_type, NULL, description, "nm", NULL, read_wavelength);
    description = "fixed values";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, NULL, description);
}

/* Start of code that is specific for the IASI instrument */

static int ingestion_init_iasi(const harp_ingestion_module *module, coda_product *product,
                               const harp_ingestion_options *options, harp_product_definition **definition,
                               void **user_data)
{
    coda_cursor cursor;
    ingest_info *info;

    (void)options;

    CHECKED_MALLOC(info, sizeof(ingest_info));
    info->product = product;
    if (init_dimensions(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }
    CHECKED_MALLOC(info->values_buffer,
                   info->num_latitudes * info->num_longitudes * info->num_altitudes * sizeof(double));

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
        info->aod_fieldname = strdup("//D_AOD%ld");
    }
    else
    {
        info->aod_fieldname = strdup("//Daod%ld");
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
    harp_dimension_type dimension_type[3] =
        { harp_dimension_spectral, harp_dimension_latitude, harp_dimension_longitude };
    const char *description;
    const char *path;

    product_definition = harp_ingestion_register_product(module, productname, NULL, read_dimensions);

    /* latitude */
    description = "latitude of the ground pixel center";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_double, 1,
                                                   &(dimension_type[1]), NULL, description, "degree_north", NULL,
                                                   read_latitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0f, 90.0f);
    path = "/latitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* longitude */
    description = "longitude of the ground pixel center";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_double, 1,
                                                   &(dimension_type[2]), NULL, description, "degree_east", NULL,
                                                   read_longitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0f, 180.0f);
    path = "/longitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* dust_aerosol_optical_depth */
    description = "dust aerosol optical depth";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "dust_aerosol_optical_depth", harp_type_double,
                                                   3, dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_aerosol_optical_depth);
    path = "/D_AOD550[], /D_AOD10000[], /D_AOD11000[]";
    harp_variable_definition_add_mapping(variable_definition, "data processed by DLR", NULL, path, NULL);
    path = "/Daod550[], /Daod10000[], /Daod11000[]";
    harp_variable_definition_add_mapping(variable_definition, "data processed by LMD", NULL, path, NULL);
    path = "/D_AOD550_mean[], /D_AOD_10000_mean[], /D_AOD11000_mean[]";
    harp_variable_definition_add_mapping(variable_definition, "data processed by ULB", NULL, path, NULL);

    /* dust_aerosol_optical_depth_uncertainty */
    description = "uncertainty of the dust aerosol optical depth";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "dust_aerosol_optical_depth_uncertainty",
                                                   harp_type_double, 3, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_aerosol_optical_depth_uncertainty);
    path = "/D_AOD11000_uncertainty[]";
    harp_variable_definition_add_mapping(variable_definition, "data processed by DLR", NULL, path, NULL);
    path = "/Daod10000_uncertainty[]";
    harp_variable_definition_add_mapping(variable_definition, "data processed by LMD", NULL, path, NULL);
    path = "/D_AOD10000_uncertainty_mean[]";
    harp_variable_definition_add_mapping(variable_definition, "data processed by ULB", NULL, path, NULL);

    /* wavelength */
    description = "wavelengths of the measurements";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "wavelength", harp_type_double, 1,
                                                   dimension_type, NULL, description, "nm", NULL, read_wavelength);
    description = "fixed values";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, NULL, description);
}

static void register_module_l3_iasi(void)
{
    harp_ingestion_module *module;

    module =
        harp_ingestion_register_module_coda("ESACCI_AEROSOL_L3_IASI", "Aerosol CCI", "ESACCI_AEROSOL",
                                            "IASI_L3",
                                            "CCI L3 aerosol profile from IASI", ingestion_init_iasi, ingestion_done);
    register_iasi_product(module, "ESACCI_AEROSOL_L3_IASI");
}

/* Start of code that is specific for the Multi Sensor instrument */

static int read_multi_sensor_solar_zenith_angle(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "/solar_zenith_angle", info->num_latitudes * info->num_longitudes, data);
}

static int ingestion_init_multi_sensor(const harp_ingestion_module *module, coda_product *product,
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
    CHECKED_MALLOC(info->values_buffer,
                   info->num_latitudes * info->num_longitudes * info->num_altitudes * sizeof(double));

    info->num_wavelengths = 1;
    info->aod_fieldname = NULL;
    info->aod_uncertainty_name = NULL;
    *definition = *module->product_definition;
    *user_data = info;

    return 0;
}

static void register_module_l3_multi_sensor(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_latitude, harp_dimension_longitude };
    const char *description;
    const char *path;

    module =
        harp_ingestion_register_module_coda("ESACCI_AEROSOL_L3_Multi_Sensor_AAI", "Aerosol CCI", "ESACCI_AEROSOL",
                                            "Multi_Sensor_AAI_L3", "CCI L3 Absorbing Aerosol Index from Multi Sensor",
                                            ingestion_init_multi_sensor, ingestion_done);

    product_definition =
        harp_ingestion_register_product(module, "ESACCI_AEROSOL_L3_Multi_Sensor_AAI", NULL, read_dimensions);

    /* latitude */
    description = "latitude of the ground pixel center";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_double, 1,
                                                   &(dimension_type[0]), NULL, description, "degree_north", NULL,
                                                   read_latitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0f, 90.0f);
    path = "/latitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* longitude */
    description = "longitude of the ground pixel center";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_double, 1,
                                                   &(dimension_type[1]), NULL, description, "degree_east", NULL,
                                                   read_longitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0f, 180.0f);
    path = "/longitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* absorbing_aerosol_index */
    description = "absorbing aerosol index";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "absorbing_aerosol_index", harp_type_double, 2,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_absorbing_aerosol_index);
    path = "/absorbing_aerosol_index[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* solar_zenith_angle */
    description = "solar zenith angle";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "solar_zenith_angle", harp_type_double, 2,
                                                   dimension_type, NULL, description, "degree", NULL,
                                                   read_multi_sensor_solar_zenith_angle);
    path = "/solar_zenith_angle[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

/* Main procedure for all instruments */

int harp_ingestion_module_cci_l3_aerosol_init(void)
{
    register_module_l3_aatsr_atsr2();
    register_module_l3_gomos();
    register_module_l3_meris();
    register_module_l3_iasi();
    register_module_l3_multi_sensor();
    return 0;
}
