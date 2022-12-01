/*
 * Copyright (C) 2015-2022 S[&]T, The Netherlands.
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define SECONDS_FROM_1993_TO_2000 (220838400 + 5)

typedef struct ingest_info_struct
{
    coda_product *product;
    int format_version;
    long num_profiles;  /* number of profiles */
    long num_altitudes; /* number of altitudes in a profile */
    const char *swath_name;
} ingest_info;

static int read_dimensions(void *user_data, long dimension[HARP_NUM_DIM_TYPES])
{
    dimension[harp_dimension_time] = ((ingest_info *)user_data)->num_profiles;
    dimension[harp_dimension_vertical] = ((ingest_info *)user_data)->num_altitudes;
    return 0;
}

static int get_data(ingest_info *info, const char *datasetname, const char *fieldname, harp_array data)
{
    coda_cursor cursor;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, "/HDFEOS/SWATHS") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&cursor, info->swath_name) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&cursor, datasetname) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&cursor, fieldname) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_double_array(&cursor, data.double_data, coda_array_ordering_c) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_parent(&cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    return 0;
}

static int read_datetime(void *user_data, harp_array data)
{
    int retval, i;

    retval = get_data((ingest_info *)user_data, "Geolocation_Fields", "Time", data);
    for (i = 0; i < ((ingest_info *)user_data)->num_profiles; i++)
    {
        data.double_data[i] = data.double_data[i] - SECONDS_FROM_1993_TO_2000;
    }
    return retval;
}

static int read_latitude(void *user_data, harp_array data)
{
    return get_data((ingest_info *)user_data, "Geolocation_Fields", "Latitude", data);
}

static int read_longitude(void *user_data, harp_array data)
{
    return get_data((ingest_info *)user_data, "Geolocation_Fields", "Longitude", data);
}

static int read_altitude(void *user_data, harp_array data)
{
    long profile_nr;
    int retval;
    ingest_info *info;

    info = (ingest_info *)user_data;
    retval = get_data(info, "Geolocation_Fields", "Altitude", data);
    for (profile_nr = 1; profile_nr < info->num_profiles; profile_nr++)
    {
        memcpy(data.double_data + (profile_nr * info->num_altitudes), data.double_data,
               sizeof(double) * info->num_altitudes);
    }
    return retval;
}

static int read_aerosol_number_density(void *user_data, harp_array data)
{
    return get_data((ingest_info *)user_data, "Data_Fields", "Aerosol", data);
}

static int read_aerosol_number_density_uncertainty(void *user_data, harp_array data)
{
    return get_data((ingest_info *)user_data, "Data_Fields", "AerosolPrecision", data);
}

static int read_no2_vmr(void *user_data, harp_array data)
{
    return get_data((ingest_info *)user_data, "Data_Fields", "NO2", data);
}

static int read_no2_vmr_error(void *user_data, harp_array data)
{
    return get_data((ingest_info *)user_data, "Data_Fields", "NO2Precision", data);
}

static int read_no2(void *user_data, harp_array data)
{
    return get_data((ingest_info *)user_data, "Data_Fields", "NO2NumberDensity", data);
}

static int read_o3_vmr(void *user_data, harp_array data)
{
    return get_data((ingest_info *)user_data, "Data_Fields", "O3", data);
}

static int read_o3_vmr_error(void *user_data, harp_array data)
{
    return get_data((ingest_info *)user_data, "Data_Fields", "O3Precision", data);
}

static int read_o3(void *user_data, harp_array data)
{
    return get_data((ingest_info *)user_data, "Data_Fields", "O3NumberDensity", data);
}

static int read_solar_zenith_angle(void *user_data, harp_array data)
{
    return get_data((ingest_info *)user_data, "Geolocation_Fields", "SolarZenithAngle", data);
}

static int read_solar_azimuth_angle(void *user_data, harp_array data)
{
    return get_data((ingest_info *)user_data, "Geolocation_Fields", "SolarAzimuthAngle", data);
}

static int init_dimensions(ingest_info *info)
{
    coda_cursor cursor;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    /* Count the number of profiles */
    if (coda_cursor_goto(&cursor, "/HDFEOS/SWATHS") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&cursor, info->swath_name) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&cursor, "Geolocation_Fields") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&cursor, "Latitude") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &info->num_profiles) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    coda_cursor_goto_parent(&cursor);

    /* Count the number of altitudes per profile */
    if (coda_cursor_goto_record_field_by_name(&cursor, "Altitude") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &info->num_altitudes) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    coda_cursor_goto_root(&cursor);
    return 0;
}

static void ingestion_done(void *user_data)
{
    ingest_info *info = (ingest_info *)user_data;

    free(info);
}

static int ingestion_init(const harp_ingestion_module *module, coda_product *product,
                          const harp_ingestion_options *options, harp_product_definition **definition, void **user_data,
                          const char *swath_name)
{
    int format_version;
    ingest_info *info;

    (void)options;

    if (coda_get_product_version(product, &format_version) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    info = malloc(sizeof(ingest_info));
    if (info == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(ingest_info), __FILE__, __LINE__);
        return -1;
    }
    info->product = product;
    info->format_version = format_version;
    info->num_profiles = 0;
    info->swath_name = swath_name;

    if (init_dimensions(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }

    *definition = *module->product_definition;
    *user_data = info;

    return 0;
}

static int ingestion_init_aerosol(const harp_ingestion_module *module, coda_product *product,
                                  const harp_ingestion_options *options, harp_product_definition **definition,
                                  void **user_data)
{
    return ingestion_init(module, product, options, definition, user_data, "OSIRIS_Odin_Aerosol_MART");
}

static int ingestion_init_no2_oe(const harp_ingestion_module *module, coda_product *product,
                                 const harp_ingestion_options *options, harp_product_definition **definition,
                                 void **user_data)
{
    return ingestion_init(module, product, options, definition, user_data, "OSIRIS_Odin_NO2_DOAS_OE");
}

static int ingestion_init_no2_mart(const harp_ingestion_module *module, coda_product *product,
                                   const harp_ingestion_options *options, harp_product_definition **definition,
                                   void **user_data)
{
    return ingestion_init(module, product, options, definition, user_data, "OSIRIS_Odin_NO2MART");
}

static int ingestion_init_o3_oe(const harp_ingestion_module *module, coda_product *product,
                                const harp_ingestion_options *options, harp_product_definition **definition,
                                void **user_data)
{
    return ingestion_init(module, product, options, definition, user_data, "OSIRIS_Odin_O3_Chappuis_triplet_OE");
}

static int ingestion_init_o3_mart(const harp_ingestion_module *module, coda_product *product,
                                  const harp_ingestion_options *options, harp_product_definition **definition,
                                  void **user_data)
{
    return ingestion_init(module, product, options, definition, user_data, "OSIRIS_Odin_O3MART");
}

static void register_aerosol_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2];
    const char *description;
    const char *path;

    description = "OSIRIS Level 2";
    module = harp_ingestion_register_module("OSIRIS_L2_Aerosol_MART", "OSIRIS", "ODIN_OSIRIS", "L2_Aerosol_MART",
                                            description, ingestion_init_aerosol, ingestion_done);

    description = "profile data";
    product_definition = harp_ingestion_register_product(module, "OSIRIS_L2_Aerosol_MART", description,
                                                         read_dimensions);
    description = "OSIRIS Level 2 products only contain a single profile; all measured profile points will be provided "
        "in reverse order (from low altitude to high altitude) in the profile";
    harp_product_definition_add_mapping(product_definition, description, NULL);

    dimension_type[0] = harp_dimension_time;
    dimension_type[1] = harp_dimension_vertical;

    /* datetime */
    description = "time of the measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double, 1, dimension_type,
                                                   NULL, description, "seconds since 2000-01-01", NULL, read_datetime);
    path = "/HDFEOS/SWATHS/OSIRIS_Odin_Aerosol_MART/Geolocation_Fields/Time[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path,
                                         "time converted from TAI93 to seconds since 2000-01-01");

    /* latitude */
    description = "center latitude for a profile";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_double, 1, dimension_type,
                                                   NULL, description, "degree_north", NULL, read_latitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    path = "/HDFEOS/SWATHS/OSIRIS_Odin_Aerosol_MART/Geolocation_Fields/Latitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* longitude */
    description = "center longitude for a profile";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_double, 1, dimension_type,
                                                   NULL, description, "degree_east", NULL, read_longitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0, 180.0);
    path = "/HDFEOS/SWATHS/OSIRIS_Odin_Aerosol_MART/Geolocation_Fields/Longitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* altitude */
    description = "altitude in km for each profile element";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "altitude", harp_type_double, 2, dimension_type,
                                                   NULL, description, "km", NULL, read_altitude);
    path = "/HDFEOS/SWATHS/OSIRIS_Odin_Aerosol_MART/Geolocation_Fields/Altitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path,
                                         "altitude information will be duplicated for each profile");

    /* aerosol_number_density */
    description = "aerosol number density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "aerosol_number_density", harp_type_double, 2,
                                                   dimension_type, NULL, description, "1/cm3", NULL,
                                                   read_aerosol_number_density);
    path = "/HDFEOS/SWATHS/OSIRIS_Odin_Aerosol_MART/Data_Fields/Aerosol[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* aerosol_number_density_uncertainty */
    description = "precision of the aerosol number density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "aerosol_number_density_uncertainty",
                                                   harp_type_double, 2, dimension_type, NULL, description, "1/cm3",
                                                   NULL, read_aerosol_number_density_uncertainty);
    path = "/HDFEOS/SWATHS/OSIRIS_Odin_Aerosol_MART/Data_Fields/AerosolPrecision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* solar_zenith_angle */
    description = "solar zenith angle at the tangent point of the measurement; 0 is sun overhead, 90 is sun on the "
        "horizon";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "solar_zenith_angle", harp_type_double, 1,
                                                   dimension_type, NULL, description, "degree", NULL,
                                                   read_solar_zenith_angle);
    path = "/HDFEOS/SWATHS/OSIRIS_Odin_Aerosol_MART/Geolocation_Fields/SolarZenithAngle[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* solar_azimuth_angle */
    description = "solar azimuth angle at the tangent point of the measurement; 0 is due North, 90 is due East, 180 "
        "is South and 270 is West";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "solar_azimuth_angle", harp_type_double, 1,
                                                   dimension_type, NULL, description, "degree", NULL,
                                                   read_solar_azimuth_angle);
    path = "/HDFEOS/SWATHS/OSIRIS_Odin_Aerosol_MART/Geolocation_Fields/SolarAzimuthAngle[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

static void register_no2_oe_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2];
    const char *description;
    const char *path;

    description = "OSIRIS Level 2";
    module = harp_ingestion_register_module("OSIRIS_L2_NO2_OE", "OSIRIS", "ODIN_OSIRIS", "L2_NO2_OE", description,
                                            ingestion_init_no2_oe, ingestion_done);

    description = "profile data";
    product_definition = harp_ingestion_register_product(module, "OSIRIS_L2_NO2_OE", description, read_dimensions);
    description = "OSIRIS Level 2 products only contain a single profile; all measured profile points will be provided "
        "in reverse order (from low altitude to high altitude) in the profile";
    harp_product_definition_add_mapping(product_definition, description, NULL);

    dimension_type[0] = harp_dimension_time;
    dimension_type[1] = harp_dimension_vertical;

    /* datetime */
    description = "time of the measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double, 1, dimension_type,
                                                   NULL, description, "seconds since 2000-01-01", NULL, read_datetime);
    path = "/HDFEOS/SWATHS/OSIRIS_Odin_NO2_DOAS_OE/Geolocation_Fields/Time[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path,
                                         "time converted from TAI93 to seconds since 2000-01-01");

    /* latitude */
    description = "center latitude for a profile";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_double, 1, dimension_type,
                                                   NULL, description, "degree_north", NULL, read_latitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    path = "/HDFEOS/SWATHS/OSIRIS_Odin_NO2_DOAS_OE/Geolocation_Fields/Latitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* longitude */
    description = "center longitude for a profile";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_double, 1, dimension_type,
                                                   NULL, description, "degree_east", NULL, read_longitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0, 180.0);
    path = "/HDFEOS/SWATHS/OSIRIS_Odin_NO2_DOAS_OE/Geolocation_Fields/Longitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* altitude */
    description = "altitude in km for each profile element";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "altitude", harp_type_double, 2, dimension_type,
                                                   NULL, description, "km", NULL, read_altitude);
    path = "/HDFEOS/SWATHS/OSIRIS_Odin_NO2_DOAS_OE/Geolocation_Fields/Altitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path,
                                         "altitude information will be duplicated for each profile");

    /* no2_volume_mixing_ratio */
    description = "volume mixing ratio of NO2";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "no2_volume_mixing_ratio", harp_type_double, 2,
                                                   dimension_type, NULL, description, "ppmv", NULL, read_no2_vmr);
    path = "/HDFEOS/SWATHS/OSIRIS_Odin_NO2_DOAS_OE/Data_Fields/NO2[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, "ppmv");

    /* no2_volume_mixing_ratio_uncertainty */
    description = "precision of the volume mixing ratio of NO2";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "no2_volume_mixing_ratio_uncertainty",
                                                   harp_type_double, 2, dimension_type, NULL, description, "ppmv", NULL,
                                                   read_no2_vmr_error);
    path = "/HDFEOS/SWATHS/OSIRIS_Odin_NO2_DOAS_OE/Data_Fields/NO2Precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, "ppmv");

    /* no2_number_density */
    description = "NO2 number density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "no2_number_density", harp_type_double, 2,
                                                   dimension_type, NULL, description, "molec/cm3", NULL, read_no2);
    path = "/HDFEOS/SWATHS/OSIRIS_Odin_NO2_DOAS_OE/Data_Fields/NO2NumberDensity[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* solar_zenith_angle */
    description = "solar zenith angle at the tangent point of the measurement; 0 is sun overhead, 90 is sun on the "
        "horizon";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "solar_zenith_angle", harp_type_double, 1,
                                                   dimension_type, NULL, description, "degree", NULL,
                                                   read_solar_zenith_angle);
    path = "/HDFEOS/SWATHS/OSIRIS_Odin_NO2_DOAS_OE/Geolocation_Fields/SolarZenithAngle[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* solar_azimuth_angle */
    description = "solar azimuth angle at the tangent point of the measurement; 0 is due North, 90 is due East, 180 "
        "is South and 270 is West";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "solar_azimuth_angle", harp_type_double, 1,
                                                   dimension_type, NULL, description, "degree", NULL,
                                                   read_solar_azimuth_angle);
    path = "/HDFEOS/SWATHS/OSIRIS_Odin_NO2_DOAS_OE/Geolocation_Fields/SolarAzimuthAngle[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

static void register_no2_mart_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2];
    const char *description;
    const char *path;

    description = "OSIRIS Level 2";
    module = harp_ingestion_register_module("OSIRIS_L2_NO2_MART", "OSIRIS", "ODIN_OSIRIS", "L2_NO2_MART",
                                            description, ingestion_init_no2_mart, ingestion_done);

    description = "profile data";
    product_definition = harp_ingestion_register_product(module, "OSIRIS_L2_NO2_MART", description, read_dimensions);
    description = "OSIRIS Level 2 products only contain a single profile; all measured profile points will be provided "
        "in reverse order (from low altitude to high altitude) in the profile";
    harp_product_definition_add_mapping(product_definition, description, NULL);

    dimension_type[0] = harp_dimension_time;
    dimension_type[1] = harp_dimension_vertical;

    /* datetime */
    description = "time of the measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double, 1, dimension_type,
                                                   NULL, description, "seconds since 2000-01-01", NULL, read_datetime);
    path = "/HDFEOS/SWATHS/OSIRIS_Odin_NO2MART/Geolocation_Fields/Time[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path,
                                         "time converted from TAI93 to seconds since 2000-01-01");

    /* latitude */
    description = "center latitude for a profile";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_double, 1, dimension_type,
                                                   NULL, description, "degree_north", NULL, read_latitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    path = "/HDFEOS/SWATHS/OSIRIS_Odin_NO2MART/Geolocation_Fields/Latitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* longitude */
    description = "center longitude for a profile";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_double, 1, dimension_type,
                                                   NULL, description, "degree_east", NULL, read_longitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0, 180.0);
    path = "/HDFEOS/SWATHS/OSIRIS_Odin_NO2MART/Geolocation_Fields/Longitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* altitude */
    description = "altitude in km for each profile element";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "altitude", harp_type_double, 2, dimension_type,
                                                   NULL, description, "km", NULL, read_altitude);
    path = "/HDFEOS/SWATHS/OSIRIS_Odin_NO2MART/Geolocation_Fields/Altitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path,
                                         "altitude information will be duplicated for each profile");

    /* no2_volume_mixing_ratio */
    description = "volume mixing ratio of NO2";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "no2_volume_mixing_ratio", harp_type_double, 2,
                                                   dimension_type, NULL, description, "ppmv", NULL, read_no2_vmr);
    path = "/HDFEOS/SWATHS/OSIRIS_Odin_NO2MART/Data_Fields/NO2[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, "ppmv");

    /* no2_volume_mixing_ratio_uncertainty */
    description = "precision of the volume mixing ratio of NO2";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "no2_volume_mixing_ratio_uncertainty",
                                                   harp_type_double, 2, dimension_type, NULL, description, "ppmv", NULL,
                                                   read_no2_vmr_error);
    path = "/HDFEOS/SWATHS/OSIRIS_Odin_NO2MART/Data_Fields/NO2Precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, "ppmv");

    /* no2_number_density */
    description = "NO2 number density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "no2_number_density", harp_type_double, 2,
                                                   dimension_type, NULL, description, "molec/cm3", NULL, read_no2);
    path = "/HDFEOS/SWATHS/OSIRIS_Odin_NO2MART/Data_Fields/NO2NumberDensity[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* solar_zenith_angle */
    description = "solar zenith angle at the tangent point of the measurement; 0 is sun overhead, 90 is sun on the "
        "horizon";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "solar_zenith_angle", harp_type_double, 1,
                                                   dimension_type, NULL, description, "degree", NULL,
                                                   read_solar_zenith_angle);
    path = "/HDFEOS/SWATHS/OSIRIS_Odin_NO2MART/Geolocation_Fields/SolarZenithAngle[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* solar_azimuth_angle */
    description = "solar azimuth angle at the tangent point of the measurement; 0 is due North, 90 is due East, 180 "
        "is South and 270 is West";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "solar_azimuth_angle", harp_type_double, 1,
                                                   dimension_type, NULL, description, "degree", NULL,
                                                   read_solar_azimuth_angle);
    path = "/HDFEOS/SWATHS/OSIRIS_Odin_NO2MART/Geolocation_Fields/SolarAzimuthAngle[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

static void register_o3_oe_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2];
    const char *description;
    const char *path;

    description = "OSIRIS Level 2";
    module = harp_ingestion_register_module("OSIRIS_L2_O3_OE", "OSIRIS", "ODIN_OSIRIS", "L2_O3_OE", description,
                                            ingestion_init_o3_oe, ingestion_done);

    description = "profile data";
    product_definition = harp_ingestion_register_product(module, "OSIRIS_L2_O3_OE", description, read_dimensions);
    description = "OSIRIS Level 2 products only contain a single profile; all measured profile points will be provided "
        "in reverse order (from low altitude to high altitude) in the profile";
    harp_product_definition_add_mapping(product_definition, description, NULL);

    dimension_type[0] = harp_dimension_time;
    dimension_type[1] = harp_dimension_vertical;

    /* datetime */
    description = "time of the measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double, 1, dimension_type,
                                                   NULL, description, "seconds since 2000-01-01", NULL, read_datetime);
    path = "/HDFEOS/SWATHS/OSIRIS_Odin_O3_Chappuis_triplet_OE/Geolocation_Fields/Time[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path,
                                         "time converted from TAI93 to seconds since 2000-01-01");

    /* latitude */
    description = "center latitude for a profile";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_double, 1, dimension_type,
                                                   NULL, description, "degree_north", NULL, read_latitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    path = "/HDFEOS/SWATHS/OSIRIS_Odin_O3_Chappuis_triplet_OE/Geolocation_Fields/Latitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* longitude */
    description = "center longitude for a profile";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_double, 1, dimension_type,
                                                   NULL, description, "degree_east", NULL, read_longitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0, 180.0);
    path = "/HDFEOS/SWATHS/OSIRIS_Odin_O3_Chappuis_triplet_OE/Geolocation_Fields/Longitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* altitude */
    description = "altitude in km for each profile element";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "altitude", harp_type_double, 2, dimension_type,
                                                   NULL, description, "km", NULL, read_altitude);
    path = "/HDFEOS/SWATHS/OSIRIS_Odin_O3_Chappuis_triplet_OE/Geolocation_Fields/Altitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path,
                                         "altitude information will be duplicated for each profile");

    /* o3_volume_mixing_ratio */
    description = "volume mixing ratio of O3";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "o3_volume_mixing_ratio", harp_type_double, 2,
                                                   dimension_type, NULL, description, "ppmv", NULL, read_o3_vmr);
    path = "/HDFEOS/SWATHS/OSIRIS_Odin_O3_Chappuis_triplet_OE/Data_Fields/O3[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, "ppmv");

    /* o3_volume_mixing_ratio_uncertainty */
    description = "precision of the volume mixing ratio of O3";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "o3_volume_mixing_ratio_uncertainty",
                                                   harp_type_double, 2, dimension_type, NULL, description, "ppmv", NULL,
                                                   read_o3_vmr_error);
    path = "/HDFEOS/SWATHS/OSIRIS_Odin_O3_Chappuis_triplet_OE/Data_Fields/O3Precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, "ppmv");

    /* o3_number_density */
    description = "O3 number density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "o3_number_density", harp_type_double, 2,
                                                   dimension_type, NULL, description, "molec/cm3", NULL, read_o3);
    path = "/HDFEOS/SWATHS/OSIRIS_Odin_O3_Chappuis_triplet_OE/Data_Fields/O3NumberDensity[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* solar_zenith_angle */
    description = "solar zenith angle at the tangent point of the measurement; 0 is sun overhead, 90 is sun on the "
        "horizon";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "solar_zenith_angle", harp_type_double, 1,
                                                   dimension_type, NULL, description, "degree", NULL,
                                                   read_solar_zenith_angle);
    path = "/HDFEOS/SWATHS/OSIRIS_Odin_O3_Chappuis_triplet_OE/Geolocation_Fields/SolarZenithAngle[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* solar_azimuth_angle */
    description = "solar azimuth angle at the tangent point of the measurement; 0 is due North, 90 is due East, 180 "
        "is South and 270 is West";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "solar_azimuth_angle", harp_type_double, 1,
                                                   dimension_type, NULL, description, "degree", NULL,
                                                   read_solar_azimuth_angle);
    path = "/HDFEOS/SWATHS/OSIRIS_Odin_O3_Chappuis_triplet_OE/Geolocation_Fields/SolarAzimuthAngle[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

static void register_o3_mart_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2];
    const char *description;
    const char *path;

    description = "OSIRIS Level 2";
    module = harp_ingestion_register_module("OSIRIS_L2_O3_MART", "OSIRIS", "ODIN_OSIRIS", "L2_O3_MART", description,
                                            ingestion_init_o3_mart, ingestion_done);

    description = "profile data";
    product_definition = harp_ingestion_register_product(module, "OSIRIS_L2_O3_MART", description, read_dimensions);
    description = "OSIRIS Level 2 products only contain a single profile; all measured profile points will be provided "
        "in reverse order (from low altitude to high altitude) in the profile";
    harp_product_definition_add_mapping(product_definition, description, NULL);

    dimension_type[0] = harp_dimension_time;
    dimension_type[1] = harp_dimension_vertical;

    /* datetime */
    description = "time of the measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double, 1, dimension_type,
                                                   NULL, description, "seconds since 2000-01-01", NULL, read_datetime);
    path = "/HDFEOS/SWATHS/OSIRIS_Odin_O3MART/Geolocation_Fields/Time[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path,
                                         "time converted from TAI93 to seconds since 2000-01-01");

    /* latitude */
    description = "center latitude for a profile";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_double, 1, dimension_type,
                                                   NULL, description, "degree_north", NULL, read_latitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    path = "/HDFEOS/SWATHS/OSIRIS_Odin_O3MART/Geolocation_Fields/Latitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* longitude */
    description = "center longitude for a profile";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_double, 1, dimension_type,
                                                   NULL, description, "degree_east", NULL, read_longitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0, 180.0);
    path = "/HDFEOS/SWATHS/OSIRIS_Odin_O3MART/Geolocation_Fields/Longitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* altitude */
    description = "altitude in km for each profile element";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "altitude", harp_type_double, 2, dimension_type,
                                                   NULL, description, "km", NULL, read_altitude);
    path = "/HDFEOS/SWATHS/OSIRIS_Odin_O3MART/Geolocation_Fields/Altitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path,
                                         "altitude information will be duplicated for each profile");

    /* o3_volume_mixing_ratio */
    description = "volume mixing ratio of O3";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "o3_volume_mixing_ratio", harp_type_double, 2,
                                                   dimension_type, NULL, description, "ppmv", NULL, read_o3_vmr);
    path = "/HDFEOS/SWATHS/OSIRIS_Odin_O3MART/Data_Fields/O3[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, "ppmv");

    /* o3_volume_mixing_ratio_uncertainty */
    description = "precision of the volume mixing ratio of O3";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "o3_volume_mixing_ratio_uncertainty",
                                                   harp_type_double, 2, dimension_type, NULL, description, "ppmv", NULL,
                                                   read_o3_vmr_error);
    path = "/HDFEOS/SWATHS/OSIRIS_Odin_O3MART/Data_Fields/O3Precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, "ppmv");

    /* o3_number_density */
    description = "O3 number density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "o3_number_density", harp_type_double, 2,
                                                   dimension_type, NULL, description, "molec/cm3", NULL, read_o3);
    path = "/HDFEOS/SWATHS/OSIRIS_Odin_O3MART/Data_Fields/O3NumberDensity[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* solar_zenith_angle */
    description = "solar zenith angle at the tangent point of the measurement; 0 is sun overhead, 90 is sun on the "
        "horizon";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "solar_zenith_angle", harp_type_double, 1,
                                                   dimension_type, NULL, description, "degree", NULL,
                                                   read_solar_zenith_angle);
    path = "/HDFEOS/SWATHS/OSIRIS_Odin_O3MART/Geolocation_Fields/SolarZenithAngle[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* solar_azimuth_angle */
    description = "solar azimuth angle at the tangent point of the measurement; 0 is due North, 90 is due East, 180 "
        "is South and 270 is West";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "solar_azimuth_angle", harp_type_double, 1,
                                                   dimension_type, NULL, description, "degree", NULL,
                                                   read_solar_azimuth_angle);
    path = "/HDFEOS/SWATHS/OSIRIS_Odin_O3MART/Geolocation_Fields/SolarAzimuthAngle[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

int harp_ingestion_module_osiris_l2_init(void)
{
    register_aerosol_product();
    register_no2_oe_product();
    register_no2_mart_product();
    register_o3_oe_product();
    register_o3_mart_product();

    return 0;
}
