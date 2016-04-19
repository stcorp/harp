/*
 * Copyright (C) 2015-2016 S[&]T, The Netherlands.
 *
 * This file is part of HARP.
 *
 * HARP is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * HARP is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with HARP; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "coda.h"
#include "harp-ingestion.h"

#include <stdlib.h>
#include <string.h>

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

static int ingestion_init_h2o_nadir(const harp_ingestion_module *module, coda_product *product,
                                    const harp_ingestion_options *options, harp_product_definition **definition,
                                    void **user_data)
{
    return ingestion_init(module, product, options, definition, user_data, "H2ONadirSwath", "H2O", "H2OPrecision");
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

static int verify_product_type(coda_product *product, const char *swath_name)
{
    coda_cursor cursor;
    char buffer[100];
    long string_length;

    if (coda_cursor_set_product(&cursor, product) != 0)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, "/HDFEOS/ADDITIONAL/FILE_ATTRIBUTES@InstrumentName") != 0)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, NULL);
        return -1;
    }
    if (coda_cursor_get_string_length(&cursor, &string_length) != 0)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, NULL);
        return -1;
    }
    if (string_length != 3)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, NULL);
        return -1;
    }
    if (coda_cursor_read_string(&cursor, buffer, 4) != 0)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, NULL);
        return -1;
    }
    if (strcmp(buffer, "TES") != 0)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, NULL);
        return -1;
    }

    if (coda_cursor_goto(&cursor, "../ProcessLevel") != 0)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, NULL);
        return -1;
    }
    if (coda_cursor_get_string_length(&cursor, &string_length) != 0)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, NULL);
        return -1;
    }
    if (string_length > 99)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, NULL);
        return -1;
    }
    if (coda_cursor_read_string(&cursor, buffer, 100) != 0)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, NULL);
        return -1;
    }
    if (strncmp(buffer, "2", 1) != 0 && strncmp(buffer, "L2", 2) != 0)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, NULL);
        return -1;
    }

    if (coda_cursor_goto(&cursor, "/HDFEOS/SWATHS") != 0)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&cursor, swath_name) != 0)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, NULL);
        return -1;
    }

    return 0;
}

static int verify_ch4_nadir(const harp_ingestion_module *module, coda_product *product)
{
    (void)module;
    return verify_product_type(product, "CH4NadirSwath");
}

static int verify_co_nadir(const harp_ingestion_module *module, coda_product *product)
{
    (void)module;
    return verify_product_type(product, "CONadirSwath");
}

static int verify_h2o_nadir(const harp_ingestion_module *module, coda_product *product)
{
    (void)module;
    return verify_product_type(product, "H2ONadirSwath");
}

static int verify_o3_nadir(const harp_ingestion_module *module, coda_product *product)
{
    (void)module;
    return verify_product_type(product, "O3NadirSwath");
}

static int verify_tatm_nadir(const harp_ingestion_module *module, coda_product *product)
{
    (void)module;
    return verify_product_type(product, "TATMNadirSwath");
}

static void register_datetime_variable(harp_product_definition *product_definition, const char *path)
{
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };
    const char *description;

    description = "time of the measurement (in seconds since 2000-01-01 00:00:00)";
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

static void register_ch4_nadir_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_vertical };
    const char *description;
    const char *path;

    module = harp_ingestion_register_module_coda("TES_L2_CH4_Nadir", NULL, NULL, "TES CH4 nadir profile",
                                                 verify_ch4_nadir, ingestion_init_ch4_nadir, ingestion_done);

    /* CH4_Nadir product */
    product_definition = harp_ingestion_register_product(module, "TES_L2_CH4_Nadir", NULL, read_dimensions);

    /* datetime */
    path = "/HDFEOS/SWATHS/CH4NadirSwath/Geolocation_Fields/Time[]";
    register_datetime_variable(product_definition, path);

    /* longitude and latitude */
    path = "/HDFEOS/SWATHS/CH4NadirSwath/Geolocation_Fields/Longitude[]";
    register_longitude_variable(product_definition, path);
    path = "/HDFEOS/SWATHS/CH4NadirSwath/Geolocation_Fields/Latitude[]";
    register_latitude_variable(product_definition, path);

    /* altitude */
    path = "/HDFEOS/SWATHS/CH4NadirSwath/Data_Fields/Altitude[]";
    register_altitude_variable(product_definition, path);

    /* pressure */
    path = "/HDFEOS/SWATHS/CH4NadirSwath/Data_Fields/Pressure[]";
    register_pressure_variable(product_definition, path);

    /* CH4_volume_mixing_ratio */
    description = "CH4 volume mixing ratio";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "CH4_volume_mixing_ratio",
                                                                     harp_type_double, 2, dimension_type, NULL,
                                                                     description, "1e6 ppmv", NULL, read_value);
    path = "/HDFEOS/SWATHS/CH4NadirSwath/Data_Fields/CH4[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* CH4_volume_mixing_ratio_stdev */
    description = "CH4 volume mixing ratio precision";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition,
                                                                     "CH4_volume_mixing_ratio_stdev", harp_type_double,
                                                                     2, dimension_type, NULL, description, "1e6 ppmv", NULL,
                                                                     read_error);
    path = "/HDFEOS/SWATHS/CH4NadirSwath/Data_Fields/CH4Precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

static void register_co_nadir_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_vertical };
    const char *description;
    const char *path;

    module = harp_ingestion_register_module_coda("TES_L2_CO_Nadir", NULL, NULL, "TES CO nadir profile", verify_co_nadir,
                                                 ingestion_init_co_nadir, ingestion_done);

    /* CO_Nadir product */
    product_definition = harp_ingestion_register_product(module, "TES_L2_CO_Nadir", NULL, read_dimensions);

    /* datetime */
    path = "/HDFEOS/SWATHS/CONadirSwath/Geolocation_Fields/Time[]";
    register_datetime_variable(product_definition, path);

    /* longitude and latitude */
    path = "/HDFEOS/SWATHS/CONadirSwath/Geolocation_Fields/Longitude[]";
    register_longitude_variable(product_definition, path);
    path = "/HDFEOS/SWATHS/CONadirSwath/Geolocation_Fields/Latitude[]";
    register_latitude_variable(product_definition, path);

    /* altitude */
    path = "/HDFEOS/SWATHS/CONadirSwath/Data_Fields/Altitude[]";
    register_altitude_variable(product_definition, path);

    /* pressure */
    path = "/HDFEOS/SWATHS/CONadirSwath/Data_Fields/Pressure[]";
    register_pressure_variable(product_definition, path);

    /* CO_volume_mixing_ratio */
    description = "CO volume mixing ratio";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "CO_volume_mixing_ratio",
                                                                     harp_type_double, 2, dimension_type, NULL,
                                                                     description, "1e6 ppmv", NULL, read_value);
    path = "/HDFEOS/SWATHS/CONadirSwath/Data_Fields/CO[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* CO_volume_mixing_ratio_stdev */
    description = "CO volume mixing ratio precision";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "CO_volume_mixing_ratio_stdev",
                                                                     harp_type_double, 2, dimension_type, NULL,
                                                                     description, "1e6 ppmv", NULL, read_error);
    path = "/HDFEOS/SWATHS/CONadirSwath/Data_Fields/COPrecision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

static void register_h2o_nadir_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_vertical };
    const char *description;
    const char *path;

    module = harp_ingestion_register_module_coda("TES_L2_H2O_Nadir", NULL, NULL, "TES H2O nadir profile",
                                                 verify_h2o_nadir, ingestion_init_h2o_nadir, ingestion_done);

    /* H2O_Nadir product */
    product_definition = harp_ingestion_register_product(module, "TES_L2_H2O_Nadir", NULL, read_dimensions);

    /* datetime */
    path = "/HDFEOS/SWATHS/H2ONadirSwath/Geolocation_Fields/Time[]";
    register_datetime_variable(product_definition, path);

    /* longitude and latitude */
    path = "/HDFEOS/SWATHS/H2ONadirSwath/Geolocation_Fields/Longitude[]";
    register_longitude_variable(product_definition, path);
    path = "/HDFEOS/SWATHS/H2ONadirSwath/Geolocation_Fields/Latitude[]";
    register_latitude_variable(product_definition, path);

    /* altitude */
    path = "/HDFEOS/SWATHS/H2ONadirSwath/Data_Fields/Altitude[]";
    register_altitude_variable(product_definition, path);

    /* pressure */
    path = "/HDFEOS/SWATHS/H2ONadirSwath/Data_Fields/Pressure[]";
    register_pressure_variable(product_definition, path);

    /* H2O_volume_mixing_ratio */
    description = "H2O volume mixing ratio";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "H2O_volume_mixing_ratio",
                                                                     harp_type_double, 2, dimension_type, NULL,
                                                                     description, "1e6 ppmv", NULL, read_value);
    path = "/HDFEOS/SWATHS/H2ONadirSwath/Data_Fields/H2O[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* H2O_volume_mixing_ratio_stdev */
    description = "H2O volume mixing ratio precision";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition,
                                                                     "H2O_volume_mixing_ratio_stdev", harp_type_double,
                                                                     2, dimension_type, NULL, description, "1e6 ppmv", NULL,
                                                                     read_error);
    path = "/HDFEOS/SWATHS/H2ONadirSwath/Data_Fields/H2OPrecision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

static void register_o3_nadir_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_vertical };
    const char *description;
    const char *path;

    module = harp_ingestion_register_module_coda("TES_L2_O3_Nadir", NULL, NULL, "TES O3 nadir profile", verify_o3_nadir,
                                                 ingestion_init_o3_nadir, ingestion_done);

    /* O3_Nadir product */
    product_definition = harp_ingestion_register_product(module, "TES_L2_O3_Nadir", NULL, read_dimensions);

    /* datetime */
    path = "/HDFEOS/SWATHS/O3NadirSwath/Geolocation_Fields/Time[]";
    register_datetime_variable(product_definition, path);

    /* longitude and latitude */
    path = "/HDFEOS/SWATHS/O3NadirSwath/Geolocation_Fields/Longitude[]";
    register_longitude_variable(product_definition, path);
    path = "/HDFEOS/SWATHS/O3NadirSwath/Geolocation_Fields/Latitude[]";
    register_latitude_variable(product_definition, path);

    /* altitude */
    path = "/HDFEOS/SWATHS/O3NadirSwath/Data_Fields/Altitude[]";
    register_altitude_variable(product_definition, path);

    /* pressure */
    path = "/HDFEOS/SWATHS/O3NadirSwath/Data_Fields/Pressure[]";
    register_pressure_variable(product_definition, path);

    /* O3_volume_mixing_ratio */
    description = "O3 volume mixing ratio";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "O3_volume_mixing_ratio",
                                                                     harp_type_double, 2, dimension_type, NULL,
                                                                     description, "1e6 ppmv", NULL, read_value);
    path = "/HDFEOS/SWATHS/O3NadirSwath/Data_Fields/O3[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* O3_volume_mixing_ratio_stdev */
    description = "O3 volume mixing ratio precision";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "O3_volume_mixing_ratio_stdev",
                                                                     harp_type_double, 2, dimension_type, NULL,
                                                                     description, "1e6 ppmv", NULL, read_error);
    path = "/HDFEOS/SWATHS/O3NadirSwath/Data_Fields/O3Precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

static void register_tatm_nadir_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_vertical };
    const char *description;
    const char *path;

    module = harp_ingestion_register_module_coda("TES_L2_Temperature_Nadir", NULL, NULL,
                                                 "TES atmospheric temperature nadir profile", verify_tatm_nadir,
                                                 ingestion_init_tatm_nadir, ingestion_done);

    /* Temperature_Nadir product */
    product_definition = harp_ingestion_register_product(module, "TES_L2_Temperature_Nadir", NULL, read_dimensions);

    /* datetime */
    path = "/HDFEOS/SWATHS/TATMNadirSwath/Geolocation_Fields/Time[]";
    register_datetime_variable(product_definition, path);

    /* longitude and latitude */
    path = "/HDFEOS/SWATHS/TATMNadirSwath/Geolocation_Fields/Longitude[]";
    register_longitude_variable(product_definition, path);
    path = "/HDFEOS/SWATHS/TATMNadirSwath/Geolocation_Fields/Latitude[]";
    register_latitude_variable(product_definition, path);

    /* altitude */
    path = "/HDFEOS/SWATHS/TATMNadirSwath/Data_Fields/Altitude[]";
    register_altitude_variable(product_definition, path);

    /* pressure */
    path = "/HDFEOS/SWATHS/TATMNadirSwath/Data_Fields/Pressure[]";
    register_pressure_variable(product_definition, path);

    /* temperature */
    description = "temperature";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "temperature",
                                                                     harp_type_double, 2, dimension_type, NULL,
                                                                     description, "K", NULL, read_value);
    path = "/HDFEOS/SWATHS/TATMNadirSwath/Data_Fields/TATM[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* temperature_stdev */
    description = "temperature precision";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "temperature_stdev",
                                                                     harp_type_double, 2, dimension_type, NULL,
                                                                     description, "K", NULL, read_error);
    path = "/HDFEOS/SWATHS/TATMNadirSwath/Data_Fields/TATMPrecision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

int harp_ingestion_module_tes_l2_init(void)
{
    register_ch4_nadir_product();
    register_co_nadir_product();
    register_h2o_nadir_product();
    register_o3_nadir_product();
    register_tatm_nadir_product();

    return 0;
}
