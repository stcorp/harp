/*
 * Copyright (C) 2015 S[&]T, The Netherlands.
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
    if (coda_cursor_goto(&info->swath_cursor, "/HDFEOS/SWATHS/HIRDLS") != 0)
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

    cursor = info->geo_cursor;
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

static int get_variable_attributes(coda_cursor *cursor, double *missing_value)
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
    coda_cursor_goto_parent(cursor);

    return 0;
}

static int read_variable(coda_cursor *cursor, const char *name, int num_dimensions, long dimension_0, long dimension_1,
                         harp_array data)
{
    double missing_value;
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
        harp_set_error(HARP_ERROR_PRODUCT, "product error detected in HIRDLS L2 product (variable %s has %d "
                       "dimensions, expected %d)", name, num_coda_dimensions, num_dimensions);
        return -1;
    }
    if (dimension_0 != coda_dimension[0])
    {
        harp_set_error(HARP_ERROR_PRODUCT, "product error detected in HIRDLS L2 product (first dimension for variable "
                       "%s has %ld elements, expected %ld", name, coda_dimension[0], dimension_0);
        return -1;
    }
    num_elements = coda_dimension[0];
    if (num_dimensions > 1)
    {
        if (dimension_1 != coda_dimension[1])
        {
            harp_set_error(HARP_ERROR_PRODUCT, "product error detected in HIRDLS L2 product (second dimension for "
                           "variable %s has %ld elements, expected %ld", name, coda_dimension[1], dimension_1);
            return -1;
        }
        num_elements *= coda_dimension[1];
    }
    if (get_variable_attributes(cursor, &missing_value) != 0)
    {
        return -1;
    }
    if (coda_cursor_read_double_array(cursor, data.double_data, coda_array_ordering_c) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    /* replace missing values by NaN */
    for (i = 0; i < num_elements; i++)
    {
        if (data.double_data[i] == missing_value)
        {
            data.double_data[i] = coda_NaN();
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

    return read_variable(&info->geo_cursor, "Pressure", 1, info->num_levels, 0, data);
}

static int read_altitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable(&info->geo_cursor, "Altitude", 2, info->num_times, info->num_levels, data);
}

static int read_cfc11_vmr(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable(&info->swath_cursor, "CFC11", 2, info->num_times, info->num_levels, data);
}

static int read_cfc11_vmr_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable(&info->swath_cursor, "CFC11Precision", 2, info->num_times, info->num_levels, data);
}

static int read_cfc12_vmr(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable(&info->swath_cursor, "CFC12", 2, info->num_times, info->num_levels, data);
}

static int read_cfc12_vmr_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable(&info->swath_cursor, "CFC12Precision", 2, info->num_times, info->num_levels, data);
}

static int read_ch4_vmr(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable(&info->swath_cursor, "CH4", 2, info->num_times, info->num_levels, data);
}

static int read_ch4_vmr_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable(&info->swath_cursor, "CH4Precision", 2, info->num_times, info->num_levels, data);
}

static int read_clono2_vmr(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable(&info->swath_cursor, "ClONO2", 2, info->num_times, info->num_levels, data);
}

static int read_clono2_vmr_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable(&info->swath_cursor, "ClONO2Precision", 2, info->num_times, info->num_levels, data);
}

static int read_h2o_vmr(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable(&info->swath_cursor, "H2O", 2, info->num_times, info->num_levels, data);
}

static int read_h2o_vmr_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable(&info->swath_cursor, "H2OPrecision", 2, info->num_times, info->num_levels, data);
}

static int read_hno3_vmr(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable(&info->swath_cursor, "HNO3", 2, info->num_times, info->num_levels, data);
}

static int read_hno3_vmr_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable(&info->swath_cursor, "HNO3Precision", 2, info->num_times, info->num_levels, data);
}

static int read_n2o_vmr(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable(&info->swath_cursor, "N2O", 2, info->num_times, info->num_levels, data);
}

static int read_n2o_vmr_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable(&info->swath_cursor, "N2OPrecision", 2, info->num_times, info->num_levels, data);
}

static int read_n2o5_vmr(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable(&info->swath_cursor, "N2O5", 2, info->num_times, info->num_levels, data);
}

static int read_n2o5_vmr_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable(&info->swath_cursor, "N2O5Precision", 2, info->num_times, info->num_levels, data);
}

static int read_no2_vmr(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable(&info->swath_cursor, "NO2", 2, info->num_times, info->num_levels, data);
}

static int read_no2_vmr_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable(&info->swath_cursor, "NO2Precision", 2, info->num_times, info->num_levels, data);
}

static int read_o3_vmr(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable(&info->swath_cursor, "O3", 2, info->num_times, info->num_levels, data);
}

static int read_o3_vmr_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable(&info->swath_cursor, "O3Precision", 2, info->num_times, info->num_levels, data);
}

static int read_temperature(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable(&info->swath_cursor, "Temperature", 2, info->num_times, info->num_levels, data);
}

static int read_temperature_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable(&info->swath_cursor, "TemperaturePrecision", 2, info->num_times, info->num_levels, data);
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

static int verify_product_type(const harp_ingestion_module *module, coda_product *product)
{
    coda_cursor cursor;
    char buffer[100];

    (void)module;

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
    if (coda_cursor_read_string(&cursor, buffer, 100) != 0)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, NULL);
        return -1;
    }
    if (strcmp(buffer, "HIRDLS") != 0)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, NULL);
        return -1;
    }

    if (coda_cursor_goto(&cursor, "../HIRDLSFileType") != 0)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, NULL);
        return -1;
    }
    if (coda_cursor_read_string(&cursor, buffer, 100) != 0)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, NULL);
        return -1;
    }
    if (strncmp(buffer, "HIRDLS2", 7) != 0)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, NULL);
        return -1;
    }

    return 0;
}

int harp_ingestion_module_hirdls_l2_init(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_vertical };
    harp_dimension_type pressure_dimension_type[1] = { harp_dimension_vertical };
    const char *description;
    const char *path;

    module =
        harp_ingestion_register_module_coda("HIRDLS_L2", NULL, NULL, "HIRDLS L2 product", verify_product_type,
                                            ingestion_init, ingestion_done);

    /* HIRDLS product */
    product_definition = harp_ingestion_register_product(module, "HIRDLS_L2", NULL, read_dimensions);

    /* datetime */
    description = "time of the measurement (in seconds since 2000-01-01 00:00:00)";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double, 1, dimension_type,
                                                   NULL, description, "seconds since 2000-01-01", NULL, read_time);
    path = "/HDFEOS/SWATHS/HIRDLS/Geolocation_Fields/Time[]";
    description = "the time converted from TAI93 to seconds since 2000-01-01T00:00:00";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* longitude */
    description = "tangent longitude";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_double, 1, dimension_type,
                                                   NULL, description, "degree_east", NULL, read_longitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0, 180.0);
    path = "/HDFEOS/SWATHS/HIRDLS/Geolocation_Fields/Longitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* latitude */
    description = "tangent latitude";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_double, 1, dimension_type,
                                                   NULL, description, "degree_north", NULL, read_latitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    path = "/HDFEOS/SWATHS/HIRDLS/Geolocation_Fields/Latitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* altitude */
    description = "altitude per profile level";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "altitude", harp_type_double, 2, dimension_type,
                                                   NULL, description, "m", NULL, read_altitude);
    path = "/HDFEOS/SWATHS/HIRDLS/Data_Fields/Altitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* pressure */
    description = "pressure per profile level";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "pressure", harp_type_double, 1,
                                                   pressure_dimension_type, NULL, description, "hPa", NULL,
                                                   read_pressure);
    path = "/HDFEOS/SWATHS/HIRDLS/Data_Fields/Pressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* CCl3F_volume_mixing_ratio */
    description = "CCl3F (CFC-11) volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "CCl3F_volume_mixing_ratio", harp_type_double, 2,
                                                   dimension_type, NULL, description, "ppv", NULL, read_cfc11_vmr);
    path = "/HDFEOS/SWATHS/HIRDLS/Data_Fields/CFC11[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* CCl3F_volume_mixing_ratio_stdev */
    description = "uncertainty of the CCl3F (CFC-11) volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "CCl3F_volume_mixing_ratio_stdev",
                                                   harp_type_double, 2, dimension_type, NULL, description, "ppv", NULL,
                                                   read_cfc11_vmr_error);
    path = "/HDFEOS/SWATHS/HIRDLS/Data_Fields/CFC12Precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* CCl2F2_volume_mixing_ratio */
    description = "CCl2F2 (CFC-12) volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "CCl2F2_volume_mixing_ratio", harp_type_double,
                                                   2, dimension_type, NULL, description, "ppv", NULL, read_cfc12_vmr);
    path = "/HDFEOS/SWATHS/HIRDLS/Data_Fields/CFC12[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* CCl2F2_volume_mixing_ratio_stdev */
    description = "uncertainty of the CCl2F2 (CFC-12) volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "CCl2F2_volume_mixing_ratio_stdev",
                                                   harp_type_double, 2, dimension_type, NULL, description, "ppv", NULL,
                                                   read_cfc12_vmr_error);
    path = "/HDFEOS/SWATHS/HIRDLS/Data_Fields/CFC12Precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* CH4_volume_mixing_ratio */
    description = "CH4 volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "CH4_volume_mixing_ratio", harp_type_double, 2,
                                                   dimension_type, NULL, description, "ppv", NULL, read_ch4_vmr);
    path = "/HDFEOS/SWATHS/HIRDLS/Data_Fields/CH4[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* CH4_volume_mixing_ratio_stdev */
    description = "uncertainty of the CH4 volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "CH4_volume_mixing_ratio_stdev",
                                                   harp_type_double, 2, dimension_type, NULL, description, "ppv", NULL,
                                                   read_ch4_vmr_error);
    path = "/HDFEOS/SWATHS/HIRDLS/Data_Fields/CH4Precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* ClONO2_volume_mixing_ratio */
    description = "ClONO2 volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "ClONO2_volume_mixing_ratio", harp_type_double,
                                                   2, dimension_type, NULL, description, "ppv", NULL, read_clono2_vmr);
    path = "/HDFEOS/SWATHS/HIRDLS/Data_Fields/ClONO2[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* ClONO2_volume_mixing_ratio_stdev */
    description = "uncertainty of the ClONO2 volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "ClONO2_volume_mixing_ratio_stdev",
                                                   harp_type_double, 2, dimension_type, NULL, description, "ppv", NULL,
                                                   read_clono2_vmr_error);
    path = "/HDFEOS/SWATHS/HIRDLS/Data_Fields/ClONO2Precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* H2O_volume_mixing_ratio */
    description = "H2O volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "H2O_volume_mixing_ratio", harp_type_double, 2,
                                                   dimension_type, NULL, description, "ppv", NULL, read_h2o_vmr);
    path = "/HDFEOS/SWATHS/HIRDLS/Data_Fields/H2O[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* H2O_volume_mixing_ratio_stdev */
    description = "uncertainty of the H2O volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "H2O_volume_mixing_ratio_stdev",
                                                   harp_type_double, 2, dimension_type, NULL, description, "ppv", NULL,
                                                   read_h2o_vmr_error);
    path = "/HDFEOS/SWATHS/HIRDLS/Data_Fields/H2OPrecision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* HNO3_volume_mixing_ratio */
    description = "HNO3 volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "HNO3_volume_mixing_ratio", harp_type_double, 2,
                                                   dimension_type, NULL, description, "ppv", NULL, read_hno3_vmr);
    path = "/HDFEOS/SWATHS/HIRDLS/Data_Fields/HNO3Precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* HNO3_volume_mixing_ratio_stdev */
    description = "uncertainty of the HNO3 volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "HNO3_volume_mixing_ratio_stdev",
                                                   harp_type_double, 2, dimension_type, NULL, description, "ppv", NULL,
                                                   read_hno3_vmr_error);
    path = "/HDFEOS/SWATHS/HIRDLS/Data_Fields/HNO3Precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* N2O_volume_mixing_ratio */
    description = "N2O volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "N2O_volume_mixing_ratio", harp_type_double, 2,
                                                   dimension_type, NULL, description, "ppv", NULL, read_n2o_vmr);
    path = "/HDFEOS/SWATHS/HIRDLS/Data_Fields/N2O[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* N2O_volume_mixing_ratio_stdev */
    description = "uncertainty of the N2O volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "N2O_volume_mixing_ratio_stdev",
                                                   harp_type_double, 2, dimension_type, NULL, description, "ppv", NULL,
                                                   read_n2o_vmr_error);
    path = "/HDFEOS/SWATHS/HIRDLS/Data_Fields/N2OPrecision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* N2O5_volume_mixing_ratio */
    description = "N2O5 volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "N2O5_volume_mixing_ratio", harp_type_double, 2,
                                                   dimension_type, NULL, description, "ppv", NULL, read_n2o5_vmr);
    path = "/HDFEOS/SWATHS/HIRDLS/Data_Fields/N2O5[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* N2O5_volume_mixing_ratio_stdev */
    description = "uncertainty of the N2O5 volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "N2O5_volume_mixing_ratio_stdev",
                                                   harp_type_double, 2, dimension_type, NULL, description, "ppv", NULL,
                                                   read_n2o5_vmr_error);
    path = "/HDFEOS/SWATHS/HIRDLS/Data_Fields/N2O5Precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* NO2_volume_mixing_ratio */
    description = "NO2 volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "NO2_volume_mixing_ratio", harp_type_double, 2,
                                                   dimension_type, NULL, description, "ppv", NULL, read_no2_vmr);
    path = "/HDFEOS/SWATHS/HIRDLS/Data_Fields/NO2[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* NO2_volume_mixing_ratio_stdev */
    description = "uncertainty of the NO2 volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "NO2_volume_mixing_ratio_stdev",
                                                   harp_type_double, 2, dimension_type, NULL, description, "ppv", NULL,
                                                   read_no2_vmr_error);
    path = "/HDFEOS/SWATHS/HIRDLS/Data_Fields/NO2Precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* O3_volume_mixing_ratio */
    description = "O3 volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_volume_mixing_ratio", harp_type_double, 2,
                                                   dimension_type, NULL, description, "ppv", NULL, read_o3_vmr);
    path = "/HDFEOS/SWATHS/HIRDLS/Data_Fields/O3[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* O3_volume_mixing_ratio_stdev */
    description = "uncertainty of the O3 volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_volume_mixing_ratio_stdev", harp_type_double,
                                                   2, dimension_type, NULL, description, "ppv", NULL,
                                                   read_o3_vmr_error);
    path = "/HDFEOS/SWATHS/HIRDLS/Data_Fields/O3Precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* temperature */
    description = "temperature";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "temperature", harp_type_double, 2,
                                                   dimension_type, NULL, description, "K", NULL, read_temperature);
    path = "/HDFEOS/SWATHS/HIRDLS/Data_Fields/Temperature[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* temperature_stdev */
    description = "uncertainty of the temperature";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "temperature_stdev", harp_type_double, 2,
                                                   dimension_type, NULL, description, "K", NULL,
                                                   read_temperature_error);
    path = "/HDFEOS/SWATHS/HIRDLS/Data_Fields/TemperaturePrecision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    return 0;
}
