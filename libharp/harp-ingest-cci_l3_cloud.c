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

/* ------------------ Typedefs ------------------ */

typedef enum orbit_data_enum
{
    ORBIT_IS_ASCENDING,
    ORBIT_IS_DESCENDING,
    NO_ORBIT_DATA
} orbit_data;

typedef struct ingest_info_struct
{
    coda_product *product;
    long num_latitudes;
    long num_longitudes;
    orbit_data orbit;
    short corrected;
    short qcflag_present;
    short stemp_present;
} ingest_info;

/* -------------------- Code -------------------- */

static void ingestion_done(void *user_data)
{
    ingest_info *info = (ingest_info *)user_data;

    free(info);
}

static int read_dataset(ingest_info *info, const char *path, long num_elements, harp_array data, double fill_value)
{
    coda_cursor cursor;
    long coda_num_elements;
    harp_scalar scalar_fill_value;

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
    scalar_fill_value.double_data = fill_value;
    harp_array_replace_fill_value(harp_type_double, num_elements, data, scalar_fill_value);

    return 0;
}

static int read_latitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "/lat", info->num_latitudes, data, -999.0);
}

static int read_longitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "/lon", info->num_longitudes, data, -999.0);
}

static int read_cloud_data(ingest_info *info, const char *field_code, short corrected, short uncertainty_field,
                           double fill_value, harp_array data)
{
    char field_name[81];

    strcpy(field_name, "/");
    strcat(field_name, field_code);
    if (corrected)
    {
        strcat(field_name, "_corrected");
    }
    switch (info->orbit)
    {
        case ORBIT_IS_ASCENDING:
            strcat(field_name, "_asc");
            break;

        case ORBIT_IS_DESCENDING:
            strcat(field_name, "_desc");
            break;

        case NO_ORBIT_DATA:
            /* Do nothing */
            break;
    }
    if (uncertainty_field)
    {
        strcat(field_name, "_unc");
    }

    return read_dataset(info, field_name, info->num_latitudes * info->num_longitudes, data, fill_value);
}

static int read_cloud_optical_depth(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_cloud_data(info, "cot", FALSE, FALSE, -999.0, data);
}

static int read_cloud_optical_depth_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_cloud_data(info, "cot", FALSE, TRUE, -999.0, data);
}

static int read_cloud_top_height(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_cloud_data(info, "cth", info->corrected, FALSE, -32767.0, data);
}

static int read_cloud_top_height_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_cloud_data(info, "cth", info->corrected, TRUE, -32767.0, data);
}

static int read_cloud_top_pressure(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_cloud_data(info, "ctp", info->corrected, FALSE, -32767.0, data);
}

static int read_cloud_top_pressure_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_cloud_data(info, "ctp", info->corrected, TRUE, -32767.0, data);
}

static int read_cloud_top_temperature(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_cloud_data(info, "ctt", info->corrected, FALSE, -32767.0, data);
}

static int read_cloud_top_temperature_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_cloud_data(info, "ctt", info->corrected, TRUE, -32767.0, data);
}

static int read_validity(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    coda_cursor cursor;
    long coda_num_elements;
    harp_scalar scalar_fill_value;
    char field_name[81];

    strcpy(field_name, "/qcflag");
    switch (info->orbit)
    {
        case ORBIT_IS_ASCENDING:
            strcat(field_name, "_asc");
            break;

        case ORBIT_IS_DESCENDING:
            strcat(field_name, "_desc");
            break;

        case NO_ORBIT_DATA:
            /* Do nothing */
            break;
    }
    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, field_name) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &coda_num_elements) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_num_elements != info->num_latitudes * info->num_longitudes)
    {
        harp_set_error(HARP_ERROR_INGESTION, "dataset has %ld elements (expected %ld)", coda_num_elements,
                       info->num_latitudes * info->num_longitudes);
        harp_add_coda_cursor_path_to_error_message(&cursor);
        harp_add_error_message(" (%s:%lu)", __FILE__, __LINE__);
        return -1;
    }
    if (coda_cursor_read_int16_array(&cursor, data.int16_data, coda_array_ordering_c) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    scalar_fill_value.int16_data = -999;
    harp_array_replace_fill_value(harp_type_int16, info->num_latitudes * info->num_longitudes, data, scalar_fill_value);
    return 0;
}

static int read_relative_azimuth_angle(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_cloud_data(info, "relazi", FALSE, FALSE, -32767.0, data);
}

static int read_viewing_zenith_angle(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_cloud_data(info, "satzen", FALSE, FALSE, -999.0, data);
}

static int read_solar_zenith_angle(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_cloud_data(info, "solzen", FALSE, FALSE, -999.0, data);
}

static int read_surface_temperature(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_cloud_data(info, "stemp", FALSE, FALSE, -32767.0, data);
}

static int read_surface_temperature_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_cloud_data(info, "stemp", FALSE, TRUE, -32767.0, data);
}

static int read_datetime(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int retval;

    retval = read_dataset(info, "/time", 1, data, -999.99);
    *(data.double_data) = (*(data.double_data) * SECONDS_PER_DAY) - SECONDS_FROM_1970_TO_2000;
    return retval;
}

static int read_datetime_from_attributes(ingest_info *info, const char *path, double *datetime)
{
    coda_cursor cursor;
    char buffer[17];
    long length;

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
    if (coda_cursor_get_string_length(&cursor, &length) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (length != 16)
    {
        harp_set_error(HARP_ERROR_INGESTION, "datetime value has length %ld; expected 16 (yyyyMMdd'T'HHmmss'Z')",
                       length);
        harp_add_coda_cursor_path_to_error_message(&cursor);
        return -1;
    }
    if (coda_cursor_read_string(&cursor, buffer, 17) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_time_string_to_double("yyyyMMdd'T'HHmmss'Z'", buffer, datetime) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        harp_add_coda_cursor_path_to_error_message(&cursor);
        return -1;
    }

    return 0;
}

static int read_datetime_start(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_datetime_from_attributes(info, "/@time_coverage_start", &data.double_data[0]);
}

static int read_datetime_stop(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_datetime_from_attributes(info, "/@time_coverage_end", &data.double_data[0]);
}

static int read_dimensions(void *user_data, long dimension[HARP_NUM_DIM_TYPES])
{
    ingest_info *info = (ingest_info *)user_data;

    dimension[harp_dimension_time] = 1;
    dimension[harp_dimension_latitude] = info->num_latitudes;
    dimension[harp_dimension_longitude] = info->num_longitudes;

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

    if (coda_cursor_goto(&cursor, "/lat") != 0)
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

    if (coda_cursor_goto(&cursor, "/lon") != 0)
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

    return 0;
}

static int exclude_validity_field(void *user_data)
{
    return !(((ingest_info *)user_data)->qcflag_present);
}

static int exclude_surface_temperature_field(void *user_data)
{
    return !(((ingest_info *)user_data)->stemp_present);
}

/* Code specific for daily data */

static int ingestion_daily_l3u_init(const harp_ingestion_module *module, coda_product *product,
                                    const harp_ingestion_options *options, harp_product_definition **definition,
                                    void **user_data)
{
    const char *option_value;
    coda_cursor cursor;
    ingest_info *info;

    CHECKED_MALLOC(info, sizeof(ingest_info));
    info->product = product;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        ingestion_done(info);
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (!harp_ingestion_options_has_option(options, "orbit"))
    {
        ingestion_done(info);
        harp_set_error(HARP_ERROR_INGESTION, "orbit ascending/descending not specified");
        return -1;
    }
    if (harp_ingestion_options_get_option(options, "orbit", &option_value) != 0)
    {
        ingestion_done(info);
        return -1;
    }
    if (strcmp(option_value, "ascending") == 0)
    {
        if (coda_cursor_goto(&cursor, "/cot_asc") != 0)
        {
            ingestion_done(info);
            harp_set_error(HARP_ERROR_INGESTION, "this product does not contain data of ascending orbits");
            return -1;
        }
        info->orbit = ORBIT_IS_ASCENDING;
    }
    else if (strcmp(option_value, "descending") == 0)
    {
        if (coda_cursor_goto(&cursor, "/cot_desc") != 0)
        {
            ingestion_done(info);
            harp_set_error(HARP_ERROR_INGESTION, "this product does not contain data of descending orbits");
            return -1;
        }
        info->orbit = ORBIT_IS_DESCENDING;
    }
    else
    {
        ingestion_done(info);
        harp_set_error(HARP_ERROR_INGESTION, "orbit option must be ascending or descending");
        return -1;
    }

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        ingestion_done(info);
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    info->corrected = TRUE;
    if (harp_ingestion_options_has_option(options, "corrected"))
    {
        if (harp_ingestion_options_get_option(options, "corrected", &option_value) != 0)
        {
            ingestion_done(info);
            return -1;
        }
        if (strcmp(option_value, "false") == 0)
        {
            info->corrected = FALSE;
        }
    }
    if (info->corrected && (coda_cursor_goto(&cursor, "ctt_corrected_desc") != 0))
    {
        ingestion_done(info);
        harp_set_error(HARP_ERROR_INGESTION, "this product does not contain corrected data");
        return -1;
    }

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        ingestion_done(info);
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    info->qcflag_present = (coda_cursor_goto(&cursor, "qcflag_desc") == 0);
    coda_cursor_goto_parent(&cursor);
    info->stemp_present = (coda_cursor_goto(&cursor, "stemp_desc") == 0);

    if (init_dimensions(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }

    *definition = *module->product_definition;
    *user_data = info;

    return 0;
}

void register_fields_for_daily_l3u_cloud_data(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_latitude, harp_dimension_longitude };
    harp_dimension_type datetime_dimension_type[1] = { harp_dimension_time };
    const char *description;
    const char *path;
    const char *orbit_options[] = { "ascending", "descending" };
    const char *corrected_options[] = { "false", "\"\" (default)" };

    module =
        harp_ingestion_register_module_coda("ESACCI_CLOUD_L3U", "Cloud CCI", "ESACCI_CLOUD", "L3_DAILY",
                                            "CCI L3U Cloud profile", ingestion_daily_l3u_init, ingestion_done);
    harp_ingestion_register_option(module, "orbit",
                                   "the orbit of the L3 product to ingest; option values are 'ascending', 'descending'",
                                   2, orbit_options);
    harp_ingestion_register_option(module, "corrected",
                                   "ingest the corrected or uncorrected data; option values are 'false', '' (default, we ingest the corrected data)",
                                   2, corrected_options);

    product_definition = harp_ingestion_register_product(module, "ESACCI_CLOUD_L3_Daily", NULL, read_dimensions);

    /* latitude */
    description = "latitude of the ground pixel center";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_double, 1,
                                                   dimension_type, NULL, description, "degree_north", NULL,
                                                   read_latitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0f, 90.0f);
    path = "/lat[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* longitude */
    description = "longitude of the ground pixel center";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_double, 1,
                                                   &(dimension_type[1]), NULL, description, "degree_east", NULL,
                                                   read_longitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0f, 180.0f);
    path = "/lon[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_optical_depth */
    description = "cloud optical depth";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_optical_depth", harp_type_double, 2,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_cloud_optical_depth);
    path = "/cot_asc[,,]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "orbit=ascending", path, NULL);
    path = "/cot_desc[,,]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "orbit=descending", path, NULL);

    /* cloud_optical_depth_uncertainty */
    description = "uncertainty of the cloud optical depth";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_optical_depth_uncertainty",
                                                   harp_type_double, 2, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL, read_cloud_optical_depth_uncertainty);
    path = "/cot_asc_unc[,,]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "orbit=ascending", path, NULL);
    path = "/cot_desc_unc[,,]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "orbit=descending", path, NULL);

    /* cloud_top_height */
    description = "cloud top height";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_top_height", harp_type_double, 2,
                                                   dimension_type, NULL, description, "m", NULL, read_cloud_top_height);
    path = "/cth_asc[,,]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "orbit=ascending", path, NULL);
    path = "/cth_desc[,,]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "orbit=descending", path, NULL);

    /* cloud_top_height_uncertainty */
    description = "uncertainty of the cloud top height";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_top_height_uncertainty",
                                                   harp_type_double, 2, dimension_type, NULL, description, "m", NULL,
                                                   read_cloud_top_height_uncertainty);
    path = "/cth_asc_unc[,,]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "orbit=ascending", path, NULL);
    path = "/cth_desc_unc[,,]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "orbit=descending", path, NULL);

    /* cloud_top_pressure */
    description = "cloud top pressure";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_top_pressure", harp_type_double, 2,
                                                   dimension_type, NULL, description, "hPa", NULL,
                                                   read_cloud_top_pressure);
    path = "/ctp_asc[,,]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "orbit=ascending", path, NULL);
    path = "/ctp_desc[,,]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "orbit=descending", path, NULL);

    /* cloud_top_pressure_uncertainty */
    description = "uncertainty of the cloud top pressure";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_top_pressure_uncertainty",
                                                   harp_type_double, 2, dimension_type, NULL, description, "hPa", NULL,
                                                   read_cloud_top_pressure_uncertainty);
    path = "/ctp_asc_unc[,,]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "orbit=ascending", path, NULL);
    path = "/ctp_desc_unc[,,]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "orbit=descending", path, NULL);

    /* cloud_top_temperature */
    description = "cloud top temperature";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_top_temperature", harp_type_double, 2,
                                                   dimension_type, NULL, description, "K", NULL,
                                                   read_cloud_top_temperature);
    path = "/ctt_asc[,,]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "orbit=ascending", path, NULL);
    path = "/ctt_desc[,,]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "orbit=descending", path, NULL);

    /* cloud_top_temperature_uncertainty */
    description = "uncertainty of the cloud top temperature";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_top_temperature_uncertainty",
                                                   harp_type_double, 2, dimension_type, NULL, description, "K", NULL,
                                                   read_cloud_top_temperature_uncertainty);
    path = "/ctt_asc_unc[,,]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "orbit=ascending", path, NULL);
    path = "/ctt_desc_unc[,,]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "orbit=descending", path, NULL);

    /* validity */
    description = "validity of the measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "validity",
                                                   harp_type_int16, 2, dimension_type, NULL, description, NULL,
                                                   exclude_validity_field, read_validity);
    path = "/qcflag_asc[,,]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "orbit=ascending", path, NULL);
    path = "/qcflag_desc[,,]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "orbit=descending", path, NULL);

    /* relative_azimuth_angle */
    description = "relative azimuth angle";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "relative_azimuth_angle", harp_type_double, 2,
                                                   dimension_type, NULL, description, "degree",
                                                   NULL, read_relative_azimuth_angle);
    path = "/relazi_asc[,,]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "orbit=ascending", path, NULL);
    path = "/relazi_desc[,,]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "orbit=descending", path, NULL);

    /* viewing_zenith_angle */
    description = "viewing zenith angle";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "sensor_zenith_angle", harp_type_double, 2,
                                                   dimension_type, NULL, description, "degree",
                                                   NULL, read_viewing_zenith_angle);
    path = "/satzen_asc[,,]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "orbit=ascending", path, NULL);
    path = "/satzen_desc[,,]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "orbit=descending", path, NULL);

    /* solar_zenith_angle */
    description = "solar zenith angle";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "solar_zenith_angle", harp_type_double, 2,
                                                   dimension_type, NULL, description, "degree",
                                                   NULL, read_solar_zenith_angle);
    path = "/solzen_asc[,,]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "orbit=ascending", path, NULL);
    path = "/solzen_desc[,,]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "orbit=descending", path, NULL);

    /* surface_temperature */
    description = "surface_temperature";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "surface_temperature", harp_type_double, 2,
                                                   dimension_type, NULL, description, "K",
                                                   exclude_surface_temperature_field, read_surface_temperature);
    path = "/stemp_asc[,,]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "orbit=ascending", path, NULL);
    path = "/stemp_desc[,,]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "orbit=descending", path, NULL);

    /* surface_temperature_uncertainty */
    description = "surface_temperature_uncertainty";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "surface_temperature_uncertainty",
                                                   harp_type_double, 2, dimension_type, NULL, description, "K",
                                                   exclude_surface_temperature_field,
                                                   read_surface_temperature_uncertainty);
    path = "/stemp_asc_unc[,,]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "orbit=ascending", path, NULL);
    path = "/stemp_desc_unc[,,]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "orbit=descending", path, NULL);

    /* datetime */
    description = "datetime";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double, 1,
                                                   datetime_dimension_type, NULL, description,
                                                   "seconds since 2000-01-01", NULL, read_datetime);
    path = "/time";
    description = "datetime converted from days sinds 1970-01-01 to seconds since 2000-01-01";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* datetime_start */
    description = "time coverage start";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime_start", harp_type_double, 1,
                                                   datetime_dimension_type, NULL, description,
                                                   "seconds since 2000-01-01", NULL, read_datetime_start);
    path = "/@time_coverage_start";
    description = "datetime converted from a start date to seconds since 2000-01-01";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* datetime_stop */
    description = "time coverage end";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime_stop", harp_type_double, 1,
                                                   datetime_dimension_type, NULL, description,
                                                   "seconds since 2000-01-01", NULL, read_datetime_stop);
    path = "/@time_coverage_end";
    description = "datetime converted from an end date to seconds since 2000-01-01";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);
}

/* Code specific for montly data */

static int ingestion_monthly_l3c_init(const harp_ingestion_module *module, coda_product *product,
                                      const harp_ingestion_options *options, harp_product_definition **definition,
                                      void **user_data)
{
    const char *option_value;
    coda_cursor cursor;
    ingest_info *info;

    CHECKED_MALLOC(info, sizeof(ingest_info));
    info->product = product;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        ingestion_done(info);
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    info->orbit = NO_ORBIT_DATA;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        ingestion_done(info);
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    info->corrected = TRUE;
    if (harp_ingestion_options_has_option(options, "corrected"))
    {
        if (harp_ingestion_options_get_option(options, "corrected", &option_value) != 0)
        {
            ingestion_done(info);
            return -1;
        }
        if (strcmp(option_value, "false") == 0)
        {
            info->corrected = FALSE;
        }
    }
    if (info->corrected && (coda_cursor_goto(&cursor, "ctt_corrected") != 0))
    {
        ingestion_done(info);
        harp_set_error(HARP_ERROR_INGESTION, "this product does not contain corrected data");
        return -1;
    }

    info->qcflag_present = FALSE;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        ingestion_done(info);
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    info->stemp_present = (coda_cursor_goto(&cursor, "stemp") == 0);

    if (init_dimensions(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }

    *definition = *module->product_definition;
    *user_data = info;

    return 0;
}

void register_fields_for_monthly_l3c_cloud_data(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_latitude, harp_dimension_longitude };
    harp_dimension_type datetime_dimension_type[1] = { harp_dimension_time };
    const char *description;
    const char *path;
    const char *corrected_options[] = { "false", "\"\" (default)" };

    module =
        harp_ingestion_register_module_coda("ESACCI_CLOUD_L3C", "Cloud CCI", "ESACCI_CLOUD", "L3_MONTHLY",
                                            "CCI L3C Cloud profile", ingestion_monthly_l3c_init, ingestion_done);
    harp_ingestion_register_option(module, "corrected",
                                   "ingest the corrected or uncorrected data; option values are 'false', '' (default, we ingest the corrected data)",
                                   2, corrected_options);

    product_definition = harp_ingestion_register_product(module, "ESACCI_CLOUD_L3_Monthly", NULL, read_dimensions);

    /* latitude */
    description = "latitude of the ground pixel center";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_double, 1,
                                                   dimension_type, NULL, description, "degree_north", NULL,
                                                   read_latitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0f, 90.0f);
    path = "/lat[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* longitude */
    description = "longitude of the ground pixel center";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_double, 1,
                                                   &(dimension_type[1]), NULL, description, "degree_east", NULL,
                                                   read_longitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0f, 180.0f);
    path = "/lon[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_optical_depth */
    description = "cloud optical depth";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_optical_depth", harp_type_double, 2,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_cloud_optical_depth);
    path = "/cot[,,]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_optical_depth_uncertainty */
    description = "uncertainty of the cloud optical depth";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_optical_depth_uncertainty",
                                                   harp_type_double, 2, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL, read_cloud_optical_depth_uncertainty);
    path = "/cot_unc[,,]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_top_height */
    description = "cloud top eight";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_top_height", harp_type_double, 2,
                                                   dimension_type, NULL, description, "m", NULL, read_cloud_top_height);
    path = "/cth[,,]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_top_height_uncertainty */
    description = "uncertainty of the cloud top height";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_top_height_uncertainty",
                                                   harp_type_double, 2, dimension_type, NULL, description, "m", NULL,
                                                   read_cloud_top_height_uncertainty);
    path = "/cth_unc[,,]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_top_pressure */
    description = "cloud top pressure";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_top_pressure", harp_type_double, 2,
                                                   dimension_type, NULL, description, "hPa", NULL,
                                                   read_cloud_top_pressure);
    path = "/ctp[,,]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_top_pressure_uncertainty */
    description = "uncertainty of the cloud top pressure";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_top_pressure_uncertainty",
                                                   harp_type_double, 2, dimension_type, NULL, description, "hPa", NULL,
                                                   read_cloud_top_pressure_uncertainty);
    path = "/ctp_unc[,,]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_top_temperature */
    description = "cloud top temperature";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_top_temperature", harp_type_double, 2,
                                                   dimension_type, NULL, description, "K", NULL,
                                                   read_cloud_top_temperature);
    path = "/ctt[,,]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_top_temperature_uncertainty */
    description = "uncertainty of the cloud top temperature";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_top_temperature_uncertainty",
                                                   harp_type_double, 2, dimension_type, NULL, description, "K", NULL,
                                                   read_cloud_top_temperature_uncertainty);
    path = "/ctt_unc[,,]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* surface_temperature */
    description = "surface_temperature";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "surface_temperature", harp_type_double, 2,
                                                   dimension_type, NULL, description, "K",
                                                   exclude_surface_temperature_field, read_surface_temperature);
    path = "/stemp[,,]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* surface_temperature_uncertainty */
    description = "surface_temperature_uncertainty";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "surface_temperature_uncertainty",
                                                   harp_type_double, 2, dimension_type, NULL, description, "K",
                                                   exclude_surface_temperature_field,
                                                   read_surface_temperature_uncertainty);
    path = "/stemp_unc[,,]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* datetime */
    description = "datetime";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double, 1,
                                                   datetime_dimension_type, NULL, description,
                                                   "seconds since 2000-01-01", NULL, read_datetime);
    path = "/time";
    description = "datetime converted from days sinds 1970-01-01 to seconds since 2000-01-01";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* datetime_start */
    description = "time coverage start";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime_start", harp_type_double, 1,
                                                   datetime_dimension_type, NULL, description,
                                                   "seconds since 2000-01-01", NULL, read_datetime_start);
    path = "/@time_coverage_start";
    description = "datetime converted from a start date to seconds since 2000-01-01";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* datetime_stop */
    description = "time coverage end";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime_stop", harp_type_double, 1,
                                                   datetime_dimension_type, NULL, description,
                                                   "seconds since 2000-01-01", NULL, read_datetime_stop);
    path = "/@time_coverage_end";
    description = "datetime converted from an end date to seconds since 2000-01-01";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);
}

int harp_ingestion_module_cci_l3_cloud_init(void)
{
    register_fields_for_daily_l3u_cloud_data();
    register_fields_for_monthly_l3c_cloud_data();

    return 0;
}
