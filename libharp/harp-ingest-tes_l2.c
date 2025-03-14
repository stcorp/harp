/*
 * Copyright (C) 2015-2025 S[&]T, The Netherlands.
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

#define SECONDS_FROM_1993_TO_2000 (220838400 + 5)

typedef struct ingest_info_struct
{
    const char *swath_name;
    const char *value_variable_name;
    const char *error_variable_name;

    coda_product *product;
    coda_cursor swath_cursor;
    coda_cursor geo_cursor;

    long num_times;
    long num_levels;
} ingest_info;

static int init_cursors(ingest_info *info)
{
    if (coda_cursor_set_product(&info->swath_cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&info->swath_cursor, "/HDFEOS/SWATHS") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&info->swath_cursor, info->swath_name) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    info->geo_cursor = info->swath_cursor;
    if (coda_cursor_goto_record_field_by_name(&info->swath_cursor, "Data_Fields") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&info->geo_cursor, "Geolocation_Fields") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    return 0;
}

static int get_dimensions(ingest_info *info)
{
    coda_cursor cursor;
    long coda_dimension[CODA_MAX_NUM_DIMS];
    int num_coda_dimensions;

    cursor = info->swath_cursor;
    if (coda_cursor_goto_record_field_by_name(&cursor, "Altitude") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_array_dim(&cursor, &num_coda_dimensions, coda_dimension) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    info->num_times = coda_dimension[0];
    info->num_levels = coda_dimension[1];

    return 0;
}

static int get_variable_attributes(coda_cursor *cursor, double *missing_value, double *scale_factor, double *offset)
{
    if (coda_cursor_goto_attributes(cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(cursor, "MissingValue") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_first_array_element(cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_double(cursor, missing_value) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    coda_cursor_goto_parent(cursor);
    coda_cursor_goto_parent(cursor);

    if (coda_cursor_goto_record_field_by_name(cursor, "ScaleFactor") != 0)
    {
        /* use a scale factor of 1 */
        *scale_factor = 1;
    }
    else
    {
        if (coda_cursor_goto_first_array_element(cursor) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_read_double(cursor, scale_factor) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        coda_cursor_goto_parent(cursor);
        coda_cursor_goto_parent(cursor);
    }

    if (coda_cursor_goto_record_field_by_name(cursor, "Offset") != 0)
    {
        /* use an offset of 0 */
        *offset = 0;
    }
    else
    {
        if (coda_cursor_goto_first_array_element(cursor) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_read_double(cursor, offset) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        coda_cursor_goto_parent(cursor);
        coda_cursor_goto_parent(cursor);
    }
    coda_cursor_goto_parent(cursor);

    return 0;
}

static int read_variable(coda_cursor *cursor, const char *name, int num_dimensions, long dimension_0, long dimension_1,
                         harp_array data)
{
    double missing_value;
    double scale_factor;
    double offset;
    long num_elements;
    long coda_dimension[CODA_MAX_NUM_DIMS];
    int num_coda_dimensions;
    long i;

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
        harp_set_error(HARP_ERROR_INGESTION, "product error detected in TES L2 product (variable %s has %d dimensions, "
                       "expected %d)", name, num_coda_dimensions, num_dimensions);
        return -1;
    }
    if (dimension_0 != coda_dimension[0])
    {
        harp_set_error(HARP_ERROR_INGESTION,
                       "product error detected in TES L2 product (first dimension for variable %s "
                       "has %ld elements, expected %ld", name, coda_dimension[0], dimension_0);
        return -1;
    }
    num_elements = coda_dimension[0];
    if (num_dimensions > 1)
    {
        if (dimension_1 != coda_dimension[1])
        {
            harp_set_error(HARP_ERROR_INGESTION, "product error detected in TES L2 product (second dimension for "
                           "variable %s has %ld elements, expected %ld", name, coda_dimension[1], dimension_1);
            return -1;
        }
        num_elements *= coda_dimension[1];
    }
    if (get_variable_attributes(cursor, &missing_value, &scale_factor, &offset) != 0)
    {
        return -1;
    }
    if (coda_cursor_read_double_array(cursor, data.double_data, coda_array_ordering_c) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    /* apply scaling and filter for NaN */
    for (i = 0; i < num_elements; i++)
    {
        if (data.double_data[i] == missing_value)
        {
            data.double_data[i] = coda_NaN();
        }
        else
        {
            data.double_data[i] = offset + scale_factor * data.double_data[i];
        }
    }

    coda_cursor_goto_parent(cursor);

    return 0;
}

static int read_dimensions(void *user_data, long dimension[HARP_NUM_DIM_TYPES])
{
    ingest_info *info = (ingest_info *)user_data;

    dimension[harp_dimension_time] = info->num_times;
    dimension[harp_dimension_vertical] = info->num_levels;

    return 0;
}

static int read_time(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long i;

    if (read_variable(&info->geo_cursor, "Time", 1, info->num_times, 0, data) != 0)
    {
        return -1;
    }

    /* convert time values from TAI93 to seconds since 2000-01-01 */
    for (i = 0; i < info->num_times; i++)
    {
        data.double_data[i] -= SECONDS_FROM_1993_TO_2000;
    }

    return 0;
}

static int read_longitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable(&info->geo_cursor, "Longitude", 1, info->num_times, 0, data);
}

static int read_latitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable(&info->geo_cursor, "Latitude", 1, info->num_times, 0, data);
}

static int read_pressure(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable(&info->swath_cursor, "Pressure", 2, info->num_times, info->num_levels, data);
}

static int read_altitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (read_variable(&info->swath_cursor, "Altitude", 2, info->num_times, info->num_levels, data) != 0)
    {
        return -1;
    }

    return 0;
}

static int read_value(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable(&info->swath_cursor, info->value_variable_name, 2, info->num_times, info->num_levels, data);
}

static int read_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable(&info->swath_cursor, info->error_variable_name, 2, info->num_times, info->num_levels, data);
}

static void ingestion_done(void *user_data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info != NULL)
    {
        free(info);
    }
}

static int ingestion_init(const harp_ingestion_module *module, coda_product *product,
                          const harp_ingestion_options *options, harp_product_definition **definition, void **user_data,
                          const char *swath_name, const char *value_variable_name, const char *error_variable_name)
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
    info->swath_name = swath_name;
    info->value_variable_name = value_variable_name;
    info->error_variable_name = error_variable_name;

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

    return 0;
}

static int ingestion_init_ch3oh_nadir(const harp_ingestion_module *module, coda_product *product,
                                      const harp_ingestion_options *options, harp_product_definition **definition,
                                      void **user_data)
{
    return ingestion_init(module, product, options, definition, user_data, "CH3OHNadirSwath", "CH3OH",
                          "CH3OHPrecision");
}

static int ingestion_init_ch4_nadir(const harp_ingestion_module *module, coda_product *product,
                                    const harp_ingestion_options *options, harp_product_definition **definition,
                                    void **user_data)
{
    return ingestion_init(module, product, options, definition, user_data, "CH4NadirSwath", "CH4", "CH4Precision");
}

static int ingestion_init_co_nadir(const harp_ingestion_module *module, coda_product *product,
                                   const harp_ingestion_options *options, harp_product_definition **definition,
                                   void **user_data)
{
    return ingestion_init(module, product, options, definition, user_data, "CONadirSwath", "CO", "COPrecision");
}

static int ingestion_init_co2_nadir(const harp_ingestion_module *module, coda_product *product,
                                    const harp_ingestion_options *options, harp_product_definition **definition,
                                    void **user_data)
{
    return ingestion_init(module, product, options, definition, user_data, "CO2NadirSwath", "CO2", "CO2Precision");
}

static int ingestion_init_h2o_nadir(const harp_ingestion_module *module, coda_product *product,
                                    const harp_ingestion_options *options, harp_product_definition **definition,
                                    void **user_data)
{
    return ingestion_init(module, product, options, definition, user_data, "H2ONadirSwath", "H2O", "H2OPrecision");
}

static int ingestion_init_hcooh_nadir(const harp_ingestion_module *module, coda_product *product,
                                      const harp_ingestion_options *options, harp_product_definition **definition,
                                      void **user_data)
{
    return ingestion_init(module, product, options, definition, user_data, "HCOOHNadirSwath", "HCOOH",
                          "HCOOHPrecision");
}

static int ingestion_init_hdo_nadir(const harp_ingestion_module *module, coda_product *product,
                                    const harp_ingestion_options *options, harp_product_definition **definition,
                                    void **user_data)
{
    return ingestion_init(module, product, options, definition, user_data, "HDONadirSwath", "HDO", "HDOPrecision");
}

static int ingestion_init_n2o_nadir(const harp_ingestion_module *module, coda_product *product,
                                    const harp_ingestion_options *options, harp_product_definition **definition,
                                    void **user_data)
{
    return ingestion_init(module, product, options, definition, user_data, "N2ONadirSwath", "N2O", "N2OPrecision");
}

static int ingestion_init_nh3_nadir(const harp_ingestion_module *module, coda_product *product,
                                    const harp_ingestion_options *options, harp_product_definition **definition,
                                    void **user_data)
{
    return ingestion_init(module, product, options, definition, user_data, "NH3NadirSwath", "NH3", "NH3Precision");
}

static int ingestion_init_o3_nadir(const harp_ingestion_module *module, coda_product *product,
                                   const harp_ingestion_options *options, harp_product_definition **definition,
                                   void **user_data)
{
    return ingestion_init(module, product, options, definition, user_data, "O3NadirSwath", "O3", "O3Precision");
}

static int ingestion_init_tatm_nadir(const harp_ingestion_module *module, coda_product *product,
                                     const harp_ingestion_options *options, harp_product_definition **definition,
                                     void **user_data)
{
    return ingestion_init(module, product, options, definition, user_data, "TATMNadirSwath", "TATM", "TATMPrecision");
}

static int ingestion_init_ch4_limb(const harp_ingestion_module *module, coda_product *product,
                                   const harp_ingestion_options *options, harp_product_definition **definition,
                                   void **user_data)
{
    return ingestion_init(module, product, options, definition, user_data, "CH4LimbSwath", "CH4", "CH4Precision");
}

static int ingestion_init_h2o_limb(const harp_ingestion_module *module, coda_product *product,
                                   const harp_ingestion_options *options, harp_product_definition **definition,
                                   void **user_data)
{
    return ingestion_init(module, product, options, definition, user_data, "H2OLimbSwath", "H2O", "H2OPrecision");
}

static int ingestion_init_hdo_limb(const harp_ingestion_module *module, coda_product *product,
                                   const harp_ingestion_options *options, harp_product_definition **definition,
                                   void **user_data)
{
    return ingestion_init(module, product, options, definition, user_data, "HDOLimbSwath", "HDO", "HDOPrecision");
}

static int ingestion_init_hno3_limb(const harp_ingestion_module *module, coda_product *product,
                                    const harp_ingestion_options *options, harp_product_definition **definition,
                                    void **user_data)
{
    return ingestion_init(module, product, options, definition, user_data, "HNO3LimbSwath", "HNO3", "HNO3Precision");
}

static int ingestion_init_no2_limb(const harp_ingestion_module *module, coda_product *product,
                                   const harp_ingestion_options *options, harp_product_definition **definition,
                                   void **user_data)
{
    return ingestion_init(module, product, options, definition, user_data, "O3LimbSwath", "O3", "O3Precision");
}

static int ingestion_init_o3_limb(const harp_ingestion_module *module, coda_product *product,
                                  const harp_ingestion_options *options, harp_product_definition **definition,
                                  void **user_data)
{
    return ingestion_init(module, product, options, definition, user_data, "O3LimbSwath", "O3", "O3Precision");
}

static int ingestion_init_tatm_limb(const harp_ingestion_module *module, coda_product *product,
                                    const harp_ingestion_options *options, harp_product_definition **definition,
                                    void **user_data)
{
    return ingestion_init(module, product, options, definition, user_data, "TATMLimbSwath", "TATM", "TATMPrecision");
}

static void register_datetime_variable(harp_product_definition *product_definition, const char *path)
{
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };
    const char *description;

    description = "time of the measurement";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double,
                                                                     1, dimension_type, NULL, description,
                                                                     "seconds since 2000-01-01", NULL, read_time);

    description = "the time converted from TAI93 to seconds since 2000-01-01T00:00:00";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);
}

static void register_longitude_variable(harp_product_definition *product_definition, const char *path)
{
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };
    const char *description;

    description = "tangent longitude";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_double,
                                                                     1, dimension_type, NULL, description,
                                                                     "degree_east", NULL, read_longitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0, 180.0);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

static void register_latitude_variable(harp_product_definition *product_definition, const char *path)
{
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };
    const char *description;

    description = "tangent latitude";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_double,
                                                                     1, dimension_type, NULL, description,
                                                                     "degree_north", NULL, read_latitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

static void register_altitude_variable(harp_product_definition *product_definition, const char *path)
{
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_vertical };
    const char *description;

    description = "altitude per profile level";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "altitude", harp_type_double,
                                                                     2, dimension_type, NULL, description, "m", NULL,
                                                                     read_altitude);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

static void register_pressure_variable(harp_product_definition *product_definition, const char *path)
{
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_vertical };
    const char *description;

    description = "pressure per profile level";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "pressure", harp_type_double,
                                                                     2, dimension_type, NULL, description, "hPa", NULL,
                                                                     read_pressure);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

static void register_nadir_product(const char *gas_code, const char *gas_name, const char *product_type,
                                   int (*ingestion_init)(const harp_ingestion_module *module,
                                                         coda_product *product,
                                                         const harp_ingestion_options *options,
                                                         harp_product_definition **definition, void **user_data))
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_vertical };
    char name[81], description[81], path[255];

    sprintf(name, "TES_L2_%s_Nadir", gas_code);
    sprintf(description, "TES %s nadir profile", gas_name);
    module = harp_ingestion_register_module(name, "TES", "AURA_TES", product_type, description,
                                            ingestion_init, ingestion_done);

    /* Nadir product */
    product_definition = harp_ingestion_register_product(module, name, NULL, read_dimensions);

    /* datetime */
    sprintf(path, "/HDFEOS/SWATHS/%sNadirSwath/Geolocation_Fields/Time[]", gas_code);
    register_datetime_variable(product_definition, path);

    /* longitude and latitude */
    sprintf(path, "/HDFEOS/SWATHS/%sNadirSwath/Geolocation_Fields/Longitude[]", gas_code);
    register_longitude_variable(product_definition, path);
    sprintf(path, "/HDFEOS/SWATHS/%sNadirSwath/Geolocation_Fields/Latitude[]", gas_code);
    register_latitude_variable(product_definition, path);

    /* altitude */
    sprintf(path, "/HDFEOS/SWATHS/%sNadirSwath/Data_Fields/Altitude[]", gas_code);
    register_altitude_variable(product_definition, path);

    /* pressure */
    sprintf(path, "/HDFEOS/SWATHS/%sNadirSwath/Data_Fields/Pressure[]", gas_code);
    register_pressure_variable(product_definition, path);

    if (strcmp(gas_code, "Temperature") == 0)
    {
        /* temperature */
        sprintf(name, "temperature");
        sprintf(description, "atmospheric temperature");
        sprintf(path, "/HDFEOS/SWATHS/%sNadirSwath/Data_Fields/%s[]", gas_code, gas_code);
        variable_definition = harp_ingestion_register_variable_full_read(product_definition, name, harp_type_double, 2,
                                                                         dimension_type, NULL, description, "K", NULL,
                                                                         read_value);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

        /* temperature_uncertainty */
        sprintf(name, "temperature_uncertainty");
        sprintf(description, "atmospheric temperature precision");
        sprintf(path, "/HDFEOS/SWATHS/%sNadirSwath/Data_Fields/%sPrecision[]", gas_code, gas_code);
        variable_definition = harp_ingestion_register_variable_full_read(product_definition, name, harp_type_double, 2,
                                                                         dimension_type, NULL, description, "K", NULL,
                                                                         read_error);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
    }
    else
    {
        /* volume_mixing_ratio */
        sprintf(name, "%s_volume_mixing_ratio", gas_code);
        sprintf(description, "%s volume mixing ratio", gas_name);
        sprintf(path, "/HDFEOS/SWATHS/%sNadirSwath/Data_Fields/%s[]", gas_code, gas_code);
        variable_definition = harp_ingestion_register_variable_full_read(product_definition, name, harp_type_double, 2,
                                                                         dimension_type, NULL, description, "ppv", NULL,
                                                                         read_value);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

        /* volume_mixing_ratio_uncertainty */
        sprintf(name, "%s_volume_mixing_ratio_uncertainty", gas_code);
        sprintf(description, "%s volume mixing ratio precision", gas_name);
        sprintf(path, "/HDFEOS/SWATHS/%sNadirSwath/Data_Fields/%sPrecision[]", gas_code, gas_code);
        variable_definition = harp_ingestion_register_variable_full_read(product_definition, name, harp_type_double, 2,
                                                                         dimension_type, NULL, description, "ppv", NULL,
                                                                         read_error);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
    }
}

static void register_limb_product(const char *gas_code, const char *gas_name, const char *product_type,
                                  int (*ingestion_init)(const harp_ingestion_module *module,
                                                        coda_product *product,
                                                        const harp_ingestion_options *options,
                                                        harp_product_definition **definition, void **user_data))
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_vertical };
    char name[81], description[81], path[255];

    sprintf(name, "TES_L2_%s_Limb", gas_code);
    sprintf(description, "TES %s limb profile", gas_name);
    module = harp_ingestion_register_module(name, "TES", "AURA_TES", product_type, description,
                                            ingestion_init, ingestion_done);

    /* Limb product */
    product_definition = harp_ingestion_register_product(module, name, NULL, read_dimensions);

    /* datetime */
    sprintf(path, "/HDFEOS/SWATHS/%sLimbSwath/Geolocation_Fields/Time[]", gas_code);
    register_datetime_variable(product_definition, path);

    /* longitude and latitude */
    sprintf(path, "/HDFEOS/SWATHS/%sLimbSwath/Geolocation_Fields/Longitude[]", gas_code);
    register_longitude_variable(product_definition, path);
    sprintf(path, "/HDFEOS/SWATHS/%sLimbSwath/Geolocation_Fields/Latitude[]", gas_code);
    register_latitude_variable(product_definition, path);

    /* altitude */
    sprintf(path, "/HDFEOS/SWATHS/%sLimbSwath/Data_Fields/Altitude[]", gas_code);
    register_altitude_variable(product_definition, path);

    /* pressure */
    sprintf(path, "/HDFEOS/SWATHS/%sLimbSwath/Data_Fields/Pressure[]", gas_code);
    register_pressure_variable(product_definition, path);

    if (strcmp(gas_code, "Temperature") == 0)
    {
        /* temperature */
        sprintf(name, "temperature");
        sprintf(description, "atmospheric temperature");
        sprintf(path, "/HDFEOS/SWATHS/%sLimbSwath/Data_Fields/%s[]", gas_code, gas_code);
        variable_definition = harp_ingestion_register_variable_full_read(product_definition, name, harp_type_double, 2,
                                                                         dimension_type, NULL, description, "K", NULL,
                                                                         read_value);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

        /* volume_mixing_ratio_uncertainty */
        sprintf(name, "temperature_uncertainty");
        sprintf(description, "atmospheric temperature precision");
        sprintf(path, "/HDFEOS/SWATHS/%sLimbSwath/Data_Fields/%sPrecision[]", gas_code, gas_code);
        variable_definition = harp_ingestion_register_variable_full_read(product_definition, name, harp_type_double, 2,
                                                                         dimension_type, NULL, description, "K", NULL,
                                                                         read_error);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
    }
    else
    {
        /* volume_mixing_ratio */
        sprintf(name, "%s_volume_mixing_ratio", gas_code);
        sprintf(description, "%s volume mixing ratio", gas_name);
        sprintf(path, "/HDFEOS/SWATHS/%sLimbSwath/Data_Fields/%s[]", gas_code, gas_code);
        variable_definition = harp_ingestion_register_variable_full_read(product_definition, name, harp_type_double, 2,
                                                                         dimension_type, NULL, description, "ppv", NULL,
                                                                         read_value);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

        /* volume_mixing_ratio_uncertainty */
        sprintf(name, "%s_volume_mixing_ratio_uncertainty", gas_code);
        sprintf(description, "%s volume mixing ratio precision", gas_name);
        sprintf(path, "/HDFEOS/SWATHS/%sLimbSwath/Data_Fields/%sPrecision[]", gas_code, gas_code);
        variable_definition = harp_ingestion_register_variable_full_read(product_definition, name, harp_type_double, 2,
                                                                         dimension_type, NULL, description, "ppv", NULL,
                                                                         read_error);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
    }
}

int harp_ingestion_module_tes_l2_init(void)
{
    register_nadir_product("CH3OH", "methanol", "TL2MTLN", ingestion_init_ch3oh_nadir);
    register_nadir_product("CH4", "methane", "TL2CH4N", ingestion_init_ch4_nadir);
    register_nadir_product("CO", "carbon monoxide", "TL2CON", ingestion_init_co_nadir);
    register_nadir_product("CO2", "carbon dioxide", "TL2CO2N", ingestion_init_co2_nadir);
    register_nadir_product("H2O", "water vapor", "TL2H2ON", ingestion_init_h2o_nadir);
    register_nadir_product("HCOOH", "formic acid", "TL2FORN", ingestion_init_hcooh_nadir);
    register_nadir_product("HDO", "deuterium oxide", "TL2HDON", ingestion_init_hdo_nadir);
    register_nadir_product("N2O", "nitrous oxide", "TL2N2ON", ingestion_init_n2o_nadir);
    register_nadir_product("NH3", "ammonia", "TL2NH3N", ingestion_init_nh3_nadir);
    register_nadir_product("O3", "ozone", "TL2O3N", ingestion_init_o3_nadir);
    register_nadir_product("Temperature", NULL, "TL2ATMTN", ingestion_init_tatm_nadir);

    register_limb_product("CH4", "methane", "TL2CH4L", ingestion_init_ch4_limb);
    register_limb_product("H2O", "water vapor", "TL2H2OL", ingestion_init_h2o_limb);
    register_limb_product("HDO", "deuterium oxide", "TL2HDOL", ingestion_init_hdo_limb);
    register_limb_product("HNO3", "nitric acid", "TL2HNO3L", ingestion_init_hno3_limb);
    register_limb_product("NO2", "nitrogen dioxide", "TL2NO2L", ingestion_init_no2_limb);
    register_limb_product("O3", "ozone", "TL2O3L", ingestion_init_o3_limb);
    register_limb_product("Temperature", NULL, "TL2ATMTL", ingestion_init_tatm_limb);

    return 0;
}
