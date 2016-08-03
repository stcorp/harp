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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SECONDS_FROM_1993_TO_2000 (220838400 + 5)

typedef struct ingest_info_struct
{
    coda_product *product;
    coda_cursor grid_cursor;
    int num_latitudes;
    int num_longitudes;
    long num_grid_elements;
    double granule_time;
    double latitude_origin;
    double latitude_step;
    double longitude_origin;
    double longitude_step;
} ingest_info;

static int get_dataset_attributes(coda_cursor *cursor, double *missing_value, double *scale_factor, double *offset)
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

static int read_data_set(ingest_info *info, const char *data_set_name, double *buffer)
{
    coda_cursor cursor;
    double missing_value;
    double scale_factor;
    double offset;
    long num_elements;
    long i;

    cursor = info->grid_cursor;

    if (coda_cursor_goto_record_field_by_name(&cursor, data_set_name) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &num_elements) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (num_elements != info->num_grid_elements)
    {
        harp_set_error(HARP_ERROR_INGESTION, "product error detected (inconsistent grid array size %ld != %ld)",
                       info->num_grid_elements, num_elements);
        return -1;
    }
    if (get_dataset_attributes(&cursor, &missing_value, &scale_factor, &offset) != 0)
    {
        return -1;
    }
    if (coda_cursor_read_double_array(&cursor, buffer, coda_array_ordering_c) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    /* apply scaling and filter for NaN */
    for (i = 0; i < num_elements; i++)
    {
        if (buffer[i] == missing_value)
        {
            buffer[i] = coda_NaN();
        }
        else
        {
            buffer[i] = offset + scale_factor * buffer[i];
        }
    }

    return 0;
}

static int read_dimensions(void *user_data, long dimension[HARP_NUM_DIM_TYPES])
{
    ingest_info *info = (ingest_info *)user_data;

    dimension[harp_dimension_time] = 1;
    dimension[harp_dimension_longitude] = info->num_longitudes;
    dimension[harp_dimension_latitude] = info->num_latitudes;

    return 0;
}

static int read_datetime(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    *data.double_data = info->granule_time;

    return 0;
}

static int read_longitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long i;

    for (i = 0; i < info->num_longitudes; i++)
    {
        data.double_data[i] = info->longitude_origin + info->longitude_step * i;
    }

    return 0;
}

static int read_latitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long i;

    for (i = 0; i < info->num_latitudes; i++)
    {
        data.double_data[i] = info->latitude_origin + info->latitude_step * i;
    }

    return 0;
}

static int read_cloud_fraction(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (read_data_set(info, "CloudFraction", data.double_data) != 0)
    {
        return -1;
    }

    return 0;
}

static int read_cloud_fraction_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (read_data_set(info, "CloudFractionPrecision", data.double_data) != 0)
    {
        return -1;
    }

    return 0;
}

static int read_radiative_cloud_fraction(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (read_data_set(info, "RadiativeCloudFraction", data.double_data) != 0)
    {
        return -1;
    }

    return 0;
}

static int read_cloud_pressure(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (read_data_set(info, "CloudPressure", data.double_data) != 0)
    {
        return -1;
    }

    return 0;
}

static int read_cloud_pressure_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (read_data_set(info, "CloudPressurePrecision", data.double_data) != 0)
    {
        return -1;
    }

    return 0;
}

static int read_column_amount_o3(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (read_data_set(info, "ColumnAmountO3", data.double_data) != 0)
    {
        return -1;
    }

    return 0;
}

static int read_column_amount_o3_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (read_data_set(info, "ColumnAmountO3Precision", data.double_data) != 0)
    {
        return -1;
    }

    return 0;
}

static int read_uv_aerosol_index(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (read_data_set(info, "UVAerosolIndex", data.double_data) != 0)
    {
        return -1;
    }

    return 0;
}

static int verify_product_type(coda_product *product, const char *grid_name, const char *process_level)
{
    coda_cursor cursor;
    char buffer[100];
    long string_length;
    long num_elements;

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
    if (strcmp(buffer, "OMI") != 0)
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
    if (strcmp(buffer, process_level) != 0)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, NULL);
        return -1;
    }

    if (coda_cursor_goto(&cursor, "/HDFEOS/GRIDS") != 0)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &num_elements) != 0)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, NULL);
        return -1;
    }
    if (num_elements != 1)
    {
        /* we only support products that have just 1 type of grid data */
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&cursor, grid_name) != 0)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, NULL);
        return -1;
    }

    return 0;
}

static int verify_omdoao3e(const harp_ingestion_module *module, coda_product *product)
{
    (void)module;
    return verify_product_type(product, "ColumnAmountO3", "3e");
}

static int verify_omto3d(const harp_ingestion_module *module, coda_product *product)
{
    (void)module;
    return verify_product_type(product, "OMI_Column_Amount_O3", "3d");
}

static int verify_omto3e(const harp_ingestion_module *module, coda_product *product)
{
    (void)module;
    return verify_product_type(product, "OMI_Column_Amount_O3", "3e");
}

static int init_cursors_and_grid(ingest_info *info, const char *data_group_name)
{
    coda_cursor cursor;
    long length;
    char str_buffer[64];
    static const double eps = 1.0e-10;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, "/HDFEOS/ADDITIONAL/FILE_ATTRIBUTES@TAI93At0zOfGranule") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &length) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (length != 1)
    {
        harp_set_error(HARP_ERROR_INGESTION, "product error detected (incorrect array length for TAI93At0zOfGranule)");
        return -1;
    }
    if (coda_cursor_read_double_array(&cursor, &(info->granule_time), coda_array_ordering_c) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    info->granule_time -= SECONDS_FROM_1993_TO_2000;

    if (coda_cursor_goto(&cursor, "/HDFEOS/GRIDS") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&cursor, data_group_name) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    info->grid_cursor = cursor;

    /* position the grid cursor */
    if (coda_cursor_goto_record_field_by_name(&info->grid_cursor, "Data_Fields") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    /* extract the grid dimension and scale information */
    if (coda_cursor_goto_attributes(&cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    /* grid spacing "(dlat,dlon)" */
    if (coda_cursor_goto_record_field_by_name(&cursor, "GridSpacing") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_string_length(&cursor, &length) != 0 || length >= (long)sizeof(str_buffer))
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_string(&cursor, str_buffer, length) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    str_buffer[length] = '\0';
    if (sscanf(str_buffer, "(%lf,%lf)", &(info->latitude_step), &(info->longitude_step)) != 2)
    {
        harp_set_error(HARP_ERROR_INGESTION, "product error detected (invalid format for GridSpacing attribute)");
        return -1;
    }
    coda_cursor_goto_parent(&cursor);

    /* number of longitudes */
    if (coda_cursor_goto_record_field_by_name(&cursor, "NumberOfLongitudesInGrid") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &length) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (length != 1)
    {
        harp_set_error(HARP_ERROR_INGESTION, "product error detected (incorrect array length for "
                       "NumberOfLongitudesInGrid)");
        return -1;
    }
    if (coda_cursor_read_int32_array(&cursor, &(info->num_longitudes), coda_array_ordering_c) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    coda_cursor_goto_parent(&cursor);

    /* number of latitudes */
    if (coda_cursor_goto_record_field_by_name(&cursor, "NumberOfLatitudesInGrid") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &length) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (length != 1)
    {
        harp_set_error(HARP_ERROR_INGESTION, "product error detected (incorrect array length for "
                       "NumberOfLatitudesInGrid)");
        return -1;
    }
    if (coda_cursor_read_int32_array(&cursor, &(info->num_latitudes), coda_array_ordering_c) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    coda_cursor_goto_parent(&cursor);

    /* check the grid is sensible (global) and set the origin */
    if (fabs(info->num_latitudes * info->latitude_step - 180.0) > eps ||
        fabs(info->num_longitudes * info->longitude_step - 360.0) > eps)
    {
        harp_set_error(HARP_ERROR_INGESTION, "product error detected (non-global grid coverage)");
        return -1;
    }
    info->latitude_origin = -90.0 + 0.5 * info->latitude_step;
    info->longitude_origin = -180.0 + 0.5 * info->longitude_step;

    info->num_grid_elements = info->num_latitudes * info->num_longitudes;

    return 0;
}

static void ingest_info_delete(ingest_info *info)
{
    if (info != NULL)
    {
        free(info);
    }
}

static int ingest_info_new(coda_product *product, ingest_info **new_info)
{
    ingest_info *info;

    info = malloc(sizeof(ingest_info));
    if (info == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(ingest_info), __FILE__, __LINE__);
        return -1;
    }

    info->product = product;
    info->num_latitudes = 0;
    info->num_longitudes = 0;
    info->num_grid_elements = 0;
    info->granule_time = 0.0;
    info->latitude_origin = 0.0;
    info->latitude_step = 0.0;
    info->longitude_origin = 0.0;
    info->longitude_step = 0.0;

    *new_info = info;

    return 0;
}

static void ingestion_done(void *user_data)
{
    ingest_info_delete((ingest_info *)user_data);
}

static int ingestion_init_omdoao3e(const harp_ingestion_module *module, coda_product *product,
                                   const harp_ingestion_options *options, harp_product_definition **definition,
                                   void **user_data)
{
    ingest_info *info;

    (void)options;
    if (ingest_info_new(product, &info) != 0)
    {
        return -1;
    }

    if (init_cursors_and_grid(info, "ColumnAmountO3") != 0)
    {
        ingest_info_delete(info);
        return -1;
    }

    *definition = *module->product_definition;
    *user_data = info;

    return 0;
}

static int ingestion_init_omto3(const harp_ingestion_module *module, coda_product *product,
                                const harp_ingestion_options *options, harp_product_definition **definition,
                                void **user_data)
{
    ingest_info *info;

    (void)options;
    if (ingest_info_new(product, &info) != 0)
    {
        return -1;
    }

    if (init_cursors_and_grid(info, "OMI_Column_Amount_O3") != 0)
    {
        ingest_info_delete(info);
        return -1;
    }

    *definition = *module->product_definition;
    *user_data = info;

    return 0;
}

static void register_datetime_variable(harp_product_definition *product_definition)
{
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };
    const char *description;
    const char *path;

    description = "Time of the measurement";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double,
                                                                     1, dimension_type, NULL, description,
                                                                     "seconds since 2000-01-01", NULL, read_datetime);

    path = "/HDFEOS/ADDITIONAL/FILE_ATTRIBUTES@TAI93At0zOfGranule";
    description = "The time of the measurement converted from TAI93 to seconds since 2000-01-01T00:00:00";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);
}

static void register_longitude_variable(harp_product_definition *product_definition, const char *path)
{
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_longitude };
    const char *description;

    description = "longitude of the grid cell mid-point (WGS84)";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "longitude",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, "degree_east", NULL, read_longitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0, 180.0);

    description = "A uniformly increasing sequence on the interval (-180, 180)";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);
}

static void register_latitude_variable(harp_product_definition *product_definition, const char *path)
{
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_latitude };
    const char *description;

    description = "Latitude of the grid cell mid-point (WGS84)";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_double,
                                                                     1, dimension_type, NULL, description,
                                                                     "degree_north", NULL, read_latitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);

    description = "A uniformly increasing sequence on the interval (-90, 90)";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);
}

static void register_omdoao3e_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[3] = { harp_dimension_time, harp_dimension_latitude, harp_dimension_longitude };
    const char *description;
    const char *path;

    module = harp_ingestion_register_module_coda("OMI_L3_OMDOAO3e", NULL, NULL, "OMI L3 daily O3 total column (DOAS) "
                                                 "on a global 0.25x0.25 degree grid", verify_omdoao3e,
                                                 ingestion_init_omdoao3e, ingestion_done);

    /* OMDOAO3e product */
    product_definition = harp_ingestion_register_product(module, "OMI_L3_OMDOAO3e", NULL, read_dimensions);

    /* datetime */
    register_datetime_variable(product_definition);

    /* longitude and latitude */
    path = "/HDFEOS/GRIDS/ColumnAmountO3@GridSpacing, /HDFEOS/GRIDS/ColumnAmountO3@NumberOfLongitudesInGrid";
    register_longitude_variable(product_definition, path);
    path = "/HDFEOS/GRIDS/ColumnAmountO3@GridSpacing, /HDFEOS/GRIDS/ColumnAmountO3@NumberOfLatitudesInGrid";
    register_latitude_variable(product_definition, path);

    /* cloud_fraction */
    description = "Cloud fraction";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "cloud_fraction",
                                                                     harp_type_double, 3, dimension_type, NULL,
                                                                     description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                                     read_cloud_fraction);
    path = "/HDFEOS/GRIDS/ColumnAmountO3/Data_Fields/CloudFraction[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_fraction_uncertainty */
    description = "Uncertainty of the cloud fraction";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "cloud_fraction_uncertainty",
                                                                     harp_type_double, 3, dimension_type, NULL,
                                                                     description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                                     read_cloud_fraction_precision);
    path = "/HDFEOS/GRIDS/ColumnAmountO3/Data_Fields/CloudFractionPrecision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_pressure */
    description = "Cloud pressure";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "cloud_pressure",
                                                                     harp_type_double, 3, dimension_type, NULL,
                                                                     description, "hPa", NULL, read_cloud_pressure);
    path = "/HDFEOS/GRIDS/ColumnAmountO3/Data_Fields/CloudPressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_pressure_uncertainty */
    description = "Uncertainty of the cloud pressure";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "cloud_pressure_uncertainty",
                                                                     harp_type_double, 3, dimension_type, NULL,
                                                                     description, "hPa", NULL,
                                                                     read_cloud_pressure_precision);
    path = "/HDFEOS/GRIDS/ColumnAmountO3/Data_Fields/CloudPressurePrecision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* O3_column_number_density */
    description = "O3 column number density";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "O3_column_number_density",
                                                                     harp_type_double, 3, dimension_type, NULL,
                                                                     description, "DU", NULL, read_column_amount_o3);
    path = "/HDFEOS/GRIDS/ColumnAmountO3/Data_Fields/ColumnAmountO3[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* O3_column_number_density_uncertainty */
    description = "Uncertainty of the O3 column number density";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition,
                                                                     "O3_column_number_density_uncertainty",
                                                                     harp_type_double, 3, dimension_type, NULL,
                                                                     description, "DU", NULL,
                                                                     read_column_amount_o3_precision);
    path = "/HDFEOS/GRIDS/ColumnAmountO3/Data_Fields/ColumnAmountO3Precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}


static void register_omto3d_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[3] = { harp_dimension_time, harp_dimension_latitude, harp_dimension_longitude };
    const char *description;
    const char *path;

    module = harp_ingestion_register_module_coda("OMI_L3_OMTO3d", NULL, NULL, "OMI L3 daily O3, aerosol index, and "
                                                 "radiative cloud fraction on a global 1x1 degree grid", verify_omto3d,
                                                 ingestion_init_omto3, ingestion_done);

    /* OMTO3d product */
    product_definition = harp_ingestion_register_product(module, "OMI_L3_OMTO3d", NULL, read_dimensions);

    /* datetime */
    register_datetime_variable(product_definition);

    /* longitude and latitude */
    path = "/HDFEOS/GRIDS/OMI_Column_Amount_O3@GridSpacing, "
        "/HDFEOS/GRIDS/OMI_Column_Amount_O3@NumberOfLongitudesInGrid";
    register_longitude_variable(product_definition, path);
    path = "/HDFEOS/GRIDS/OMI_Column_Amount_O3@GridSpacing, "
        "/HDFEOS/GRIDS/OMI_Column_Amount_O3@NumberOfLatitudesInGrid";
    register_latitude_variable(product_definition, path);

    /* O3_column_number_density */
    description = "O3 column number density";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "O3_column_number_density",
                                                                     harp_type_double, 3, dimension_type, NULL,
                                                                     description, "DU", NULL, read_column_amount_o3);
    path = "/HDFEOS/GRIDS/OMI_Column_Amount_O3/Data_Fields/ColumnAmountO3[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_fraction */
    description = "Cloud fraction";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "cloud_fraction",
                                                                     harp_type_double, 3, dimension_type, NULL,
                                                                     description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                                     read_radiative_cloud_fraction);
    path = "/HDFEOS/GRIDS/OMI_Column_Amount_O3/Data_Fields/RadiativeCloudFraction[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* uv_aerosol_index */
    description = "UV aerosol index";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "uv_aerosol_index",
                                                                     harp_type_double, 3, dimension_type, NULL,
                                                                     description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                                     read_uv_aerosol_index);
    path = "/HDFEOS/GRIDS/OMI_Column_Amount_O3/Data_Fields/UVAerosolIndex[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

static void register_omto3e_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[3] = { harp_dimension_time, harp_dimension_latitude, harp_dimension_longitude };
    const char *description;
    const char *path;

    module = harp_ingestion_register_module_coda("OMI_L3_OMTO3e", NULL, NULL, "OMI L3 daily O3 and radiative cloud  "
                                                 "fraction on a global 0.25x0.25 degree grid", verify_omto3e,
                                                 ingestion_init_omto3, ingestion_done);

    /* OMTO3e product */
    product_definition = harp_ingestion_register_product(module, "OMI_L3_OMTO3e", NULL, read_dimensions);

    /* datetime */
    register_datetime_variable(product_definition);

    /* longitude and latitude */
    path = "/HDFEOS/GRIDS/OMI_Column_Amount_O3@GridSpacing, "
        "/HDFEOS/GRIDS/OMI_Column_Amount_O3@NumberOfLongitudesInGrid";
    register_longitude_variable(product_definition, path);
    path = "/HDFEOS/GRIDS/OMI_Column_Amount_O3@GridSpacing, "
        "/HDFEOS/GRIDS/OMI_Column_Amount_O3@NumberOfLatitudesInGrid";
    register_latitude_variable(product_definition, path);

    /* O3_column_number_density */
    description = "O3 column number density";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "O3_column_number_density",
                                                                     harp_type_double, 3, dimension_type, NULL,
                                                                     description, "DU", NULL, read_column_amount_o3);
    path = "/HDFEOS/GRIDS/OMI_Column_Amount_O3/Data_Fields/ColumnAmountO3[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_fraction */
    description = "Cloud fraction";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "cloud_fraction",
                                                                     harp_type_double, 3, dimension_type, NULL,
                                                                     description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                                     read_radiative_cloud_fraction);
    path = "/HDFEOS/GRIDS/OMI_Column_Amount_O3/Data_Fields/RadiativeCloudFraction[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

int harp_ingestion_module_omi_l3_init(void)
{
    register_omdoao3e_product();
    register_omto3d_product();
    register_omto3e_product();

    return 0;
}
