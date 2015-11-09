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

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct ingest_info_struct
{
    const harp_product_definition *definition;
    coda_product *product;
    const char *template;
    long num_time;
    long num_vertical;
} ingest_info;

static int read_dimensions(void *user_data, long dimension[HARP_NUM_DIM_TYPES])
{
    ingest_info *info = (ingest_info *)user_data;

    dimension[harp_dimension_time] = info->num_time;
    dimension[harp_dimension_vertical] = info->num_vertical;

    return 0;
}

static int read_attribute(void *user_data, const char *path, harp_array data)
{
    coda_cursor cursor;
    long length;

    if (coda_cursor_set_product(&cursor, ((ingest_info *)user_data)->product) != 0)
    {
        return -1;
    }
    if (coda_cursor_goto(&cursor, path) != 0)
    {
        return -1;
    }
    if (coda_cursor_get_string_length(&cursor, &length) != 0)
    {
        return -1;
    }
    data.string_data[0] = malloc(length + 1);
    if (data.string_data[0] == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)", length + 1,
                       __FILE__, __LINE__);
        return -1;
    }
    if (coda_cursor_read_string(&cursor, data.string_data[0], length + 1) != 0)
    {
        return -1;
    }

    return 0;
}

static int read_variable_double(void *user_data, const char *path, long num_elements, harp_array data)
{
    coda_cursor cursor;
    long actual_num_elements, i;
    double fill_value;

    if (coda_cursor_set_product(&cursor, ((ingest_info *)user_data)->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, path) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &actual_num_elements) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (actual_num_elements != num_elements)
    {
        harp_set_error(HARP_ERROR_PRODUCT, "variable %s has %ld elements (expected %ld)", path, actual_num_elements,
                       num_elements);
        return -1;
    }
    if (coda_cursor_read_double_array(&cursor, data.double_data, coda_array_ordering_c) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, "@VAR_FILL_VALUE") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_double(&cursor, &fill_value) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (!harp_isnan(fill_value))
    {
        for (i = 0; i < num_elements; i++)
        {
            if (data.double_data[i] == fill_value)
            {
                data.double_data[i] = harp_nan();
            }
        }
    }

    return 0;
}

static int read_altitude(void *user_data, harp_array data)
{
    return read_variable_double(user_data, "ALTITUDE", ((ingest_info *)user_data)->num_vertical, data);
}

static int read_data_source(void *user_data, harp_array data)
{
    return read_attribute(user_data, "@DATA_SOURCE", data);
}

static int read_data_location(void *user_data, harp_array data)
{
    return read_attribute(user_data, "@DATA_LOCATION", data);
}

static int read_datetime(void *user_data, harp_array data)
{
    return read_variable_double(user_data, "DATETIME", ((ingest_info *)user_data)->num_time, data);
}

static int read_datetime_start(void *user_data, harp_array data)
{
    return read_variable_double(user_data, "DATETIME_START", ((ingest_info *)user_data)->num_time, data);
}

static int read_datetime_stop(void *user_data, harp_array data)
{
    return read_variable_double(user_data, "DATETIME_STOP", ((ingest_info *)user_data)->num_time, data);
}

static int read_instrument_altitude(void *user_data, harp_array data)
{
    return read_variable_double(user_data, "ALTITUDE_INSTRUMENT", 1, data);
}

static int read_instrument_latitude(void *user_data, harp_array data)
{
    return read_variable_double(user_data, "LATITUDE_INSTRUMENT", 1, data);
}

static int read_instrument_longitude(void *user_data, harp_array data)
{
    return read_variable_double(user_data, "LONGITUDE_INSTRUMENT", 1, data);
}

static int read_o3_nd_ad(void *user_data, harp_array data)
{
    return read_variable_double(user_data, "O3_NUMBER_DENSITY_ABSORPTION_DIFFERENTIAL",
                                ((ingest_info *)user_data)->num_vertical, data);
}

static int read_o3_nd_ad_stdev(void *user_data, harp_array data)
{
    return read_variable_double(user_data, "O3_NUMBER_DENSITY_ABSORPTION_DIFFERENTIAL_UNCERTAINTY_COMBINED_STANDARD",
                                ((ingest_info *)user_data)->num_vertical, data);
}

static int read_pressure_ind(void *user_data, harp_array data)
{
    return read_variable_double(user_data, "PRESSURE_INDEPENDENT", ((ingest_info *)user_data)->num_vertical, data);
}

static int read_temperature_ind(void *user_data, harp_array data)
{
    return read_variable_double(user_data, "TEMPERATURE_INDEPENDENT", ((ingest_info *)user_data)->num_vertical, data);
}

static void ingestion_done(void *user_data)
{
    ingest_info *info = (ingest_info *)user_data;

    free(info);
}

static int get_product_definition(const harp_ingestion_module *module, coda_product *product,
                                  harp_product_definition **definition)
{
    coda_cursor cursor;
    char template_name[100];
    int i;

    if (coda_cursor_set_product(&cursor, product) != 0)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, "@DATA_TEMPLATE") != 0)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, NULL);
        return -1;
    }
    if (coda_cursor_read_string(&cursor, template_name, 100) != 0)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, NULL);
        return -1;
    }

    for (i = 0; i < module->num_product_definitions; i++)
    {
        if (strcmp(template_name, module->product_definition[i]->name) == 0)
        {
            *definition = module->product_definition[i];
            return 0;
        }
    }

    harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, NULL);
    return -1;
}

static int get_dimensions(ingest_info *info)
{
    coda_cursor cursor;
    double values[2];

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        return -1;
    }
    if (coda_cursor_goto(&cursor, "/DATETIME") != 0)
    {
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &info->num_time) != 0)
    {
        return -1;
    }
    if (info->num_time > 1)
    {
        if (coda_cursor_read_double_partial_array(&cursor, 0, 2, values) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (values[1] < values[0])
        {
            harp_set_error(HARP_ERROR_PRODUCT, "time dimension should use a chronological ordering");
            return -1;
        }
    }

    if (coda_cursor_goto(&cursor, "/ALTITUDE") != 0)
    {
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &info->num_vertical) != 0)
    {
        return -1;
    }
    if (info->num_vertical > 1)
    {
        if (coda_cursor_read_double_partial_array(&cursor, 0, 2, values) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (values[1] < values[0])
        {
            harp_set_error(HARP_ERROR_PRODUCT, "vertical dimension should be ordered using increasing altitude");
            return -1;
        }
    }

    return 0;
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

    info->definition = NULL;
    info->product = product;

    if (get_product_definition(module, info->product, definition) != 0)
    {
        ingestion_done(info);
        return -1;
    }
    info->definition = *definition;

    if (get_dimensions(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }

    *user_data = info;
    return 0;
}

static int verify_template(const harp_ingestion_module *module, coda_product *product)
{
    harp_product_definition *definition;

    return get_product_definition(module, product, &definition);
}

int harp_ingestion_module_geoms_lidar_init(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2];
    const char *description;

    description = "GEOMS template for LIDAR ozone";
    module = harp_ingestion_register_module_coda("GEOMS-TE-LIDAR-O3", NULL, NULL, description, verify_template,
                                                 ingestion_init, ingestion_done);

    description = "GEOMS template for LIDAR ozone v003";
    product_definition = harp_ingestion_register_product(module, "GEOMS-TE-LIDAR-O3-003", description, read_dimensions);

    dimension_type[0] = harp_dimension_time;
    dimension_type[1] = harp_dimension_vertical;

    /* instrument_name */
    description = "name of the instrument";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "instrument_name",
                                                                     harp_type_string, 0, NULL, NULL, description, NULL,
                                                                     NULL, read_data_source);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/@DATA.SOURCE", NULL);

    /* site_name */
    description = "name of the site at which the instrument is located";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "site_name", harp_type_string,
                                                                     0, NULL, NULL, description, NULL, NULL,
                                                                     read_data_location);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/@DATA.LOCATION", NULL);

    /* instrument_latitude */
    description = "latitude of the instrument";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "instrument_latitude",
                                                                     harp_type_double, 0, NULL, NULL, description,
                                                                     "degree_north", NULL, read_instrument_latitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/LATITUDE.INSTRUMENT", NULL);

    /* instrument_longitude */
    description = "longitude of the measurement";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "instrument_longitude",
                                                                     harp_type_double, 0, NULL, NULL, description,
                                                                     "degree_east", NULL, read_instrument_longitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0, 180.0);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/LONGITUDE.INSTRUMENT", NULL);

    /* instrument_altitude */
    description = "altitude of the instrument";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "instrument_altitude",
                                                                     harp_type_double, 0, NULL, NULL, description, "m",
                                                                     NULL, read_instrument_altitude);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ALTITUDE.INSTRUMENT", NULL);

    /* datetime */
    description = "time of the measurement";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double,
                                                                     1, dimension_type, NULL, description,
                                                                     "days since 2000-01-01", NULL, read_datetime);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/DATETIME", NULL);

    /* datetime_start */
    description = "start time of the measurement";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "datetime_start",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, "days since 2000-01-01", NULL,
                                                                     read_datetime_start);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/DATETIME.START", NULL);

    /* datetime_stop */
    description = "stop time of the measurement";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "datetime_stop",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, "days since 2000-01-01", NULL,
                                                                     read_datetime_stop);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/DATETIME.STOP", NULL);

    /* O3_number_density */
    description = "absorption differential O3 number density";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "O3_number_density",
                                                                     harp_type_double, 2, dimension_type, NULL,
                                                                     description, "molec/m3", NULL, read_o3_nd_ad);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/O3.NUMBER.DENSITY_ABSORPTION.DIFFERENTIAL",
                                         NULL);

    /* O3_number_density_stdev */
    description = "standard deviation of the absorption differential O3 number density";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "O3_number_density_stdev",
                                                                     harp_type_double, 2, dimension_type, NULL,
                                                                     description, "molec/m3", NULL,
                                                                     read_o3_nd_ad_stdev);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/O3.NUMBER.DENSITY_ABSORPTION.DIFFERENTIAL_UNCERTAINTY.COMBINED.STANDARD",
                                         NULL);

    dimension_type[0] = harp_dimension_vertical;

    /* altitude */
    description = "altitude of the measurement";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "altitude", harp_type_double,
                                                                     1, dimension_type, NULL, description, "m", NULL,
                                                                     read_altitude);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ALTITUDE", NULL);

    /* pressure */
    description = "pressure profile to derive volume mixing ratio";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "pressure", harp_type_double,
                                                                     1, dimension_type, NULL, description, "hPa", NULL,
                                                                     read_pressure_ind);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/PRESSURE_INDEPENDENT", NULL);

    /* temperature */
    description = "temperature profile to derive volume mixing ratio";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "temperature",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, "K", NULL, read_temperature_ind);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/TEMPERATURE_INDEPENDENT", NULL);

    return 0;
}
