/*
 * Copyright (C) 2015-2021 S[&]T, The Netherlands.
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
#include <stdlib.h>
#include <string.h>

typedef struct ingest_info_struct
{
    coda_product *product;
    long num_time;
    coda_cursor *ddr_cursor;
    int format_version;
    int ozone_vcd;
} ingest_info;

static int init_ddr_cursor(ingest_info *info)
{
    coda_cursor cursor;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&cursor, "ddr") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &info->num_time) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    if (info->num_time > 0)
    {
        int i;

        info->ddr_cursor = malloc(info->num_time * sizeof(coda_cursor));
        if (info->ddr_cursor == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           info->num_time * sizeof(coda_cursor), __FILE__, __LINE__);
            return -1;
        }

        if (coda_cursor_goto_first_array_element(&cursor) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        for (i = 0; i < info->num_time; i++)
        {
            info->ddr_cursor[i] = cursor;
            if (i < info->num_time - 1)
            {
                if (coda_cursor_goto_next_array_element(&cursor) != 0)
                {
                    harp_set_error(HARP_ERROR_CODA, NULL);
                    return -1;
                }
            }
        }
    }

    return 0;
}

static int read_dimensions(void *user_data, long dimension[HARP_NUM_DIM_TYPES])
{
    dimension[harp_dimension_time] = ((ingest_info *)user_data)->num_time;
    return 0;
}

static int get_data(void *user_data, long index, const char *path, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    coda_cursor cursor;

    cursor = info->ddr_cursor[index];
    if (coda_cursor_goto(&cursor, path) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_double(&cursor, data.double_data) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    return 0;
}

static int get_relative_error(void *user_data, long index, const char *quantity_path, const char *error_path,
                              harp_array data)
{
    double quantity;

    if (get_data(user_data, index, quantity_path, data) != 0)
    {
        return -1;
    }
    quantity = *data.double_data;

    if (get_data(user_data, index, error_path, data) != 0)
    {
        return -1;
    }
    /* convert percentage to absolute error */
    (*data.double_data) *= 0.01 * quantity;

    return 0;
}

static int read_datetime(void *user_data, long index, harp_array data)
{
    return get_data(user_data, index, "glr/datetime", data);
}

static int read_integration_time(void *user_data, long index, harp_array data)
{
    (void)user_data;
    (void)index;

    *data.double_data = 1.5;

    return 0;
}

static int read_orbit_index(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    coda_cursor cursor;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, "/pir/start_orbit") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_int32(&cursor, data.int32_data) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    return 0;
}

static int read_latitude(void *user_data, long index, harp_array data)
{
    return get_data(user_data, index, "glr/corners[4]/lat", data);
}

static int read_longitude(void *user_data, long index, harp_array data)
{
    return get_data(user_data, index, "glr/corners[4]/lon", data);
}

static int read_latitude_bounds(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    coda_cursor cursor;
    double latitude[4];
    int i;

    cursor = info->ddr_cursor[index];
    if (coda_cursor_goto(&cursor, "glr/corners[0]") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    for (i = 0; i < 4; i++)
    {
        if (coda_cursor_goto_first_record_field(&cursor) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_read_double(&cursor, &latitude[i]) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        coda_cursor_goto_parent(&cursor);
        if (i < 4 - 1)
        {
            if (coda_cursor_goto_next_array_element(&cursor) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
        }
    }

    data.double_data[0] = latitude[1];
    data.double_data[1] = latitude[3];
    data.double_data[2] = latitude[2];
    data.double_data[3] = latitude[0];

    return 0;
}

static int read_longitude_bounds(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    coda_cursor cursor;
    double longitude[4];
    int i;

    cursor = info->ddr_cursor[index];
    if (coda_cursor_goto(&cursor, "glr/corners[0]") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    for (i = 0; i < 4; i++)
    {
        if (coda_cursor_goto_record_field_by_index(&cursor, 1) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_read_double(&cursor, &longitude[i]) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        coda_cursor_goto_parent(&cursor);
        if (i < 4 - 1)
        {
            if (coda_cursor_goto_next_array_element(&cursor) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
        }
    }

    data.double_data[0] = longitude[1];
    data.double_data[1] = longitude[3];
    data.double_data[2] = longitude[2];
    data.double_data[3] = longitude[0];

    for (i = 0; i < 4; i++)
    {
        if (data.double_data[i] > 180)
        {
            data.double_data[i] -= 360;
        }
    }

    return 0;
}

static int read_o3(void *user_data, long index, harp_array data)
{
    if (((ingest_info *)user_data)->ozone_vcd == 1)
    {
        return get_data(user_data, index, "irr/vcd/total[1]", data);
    }
    return get_data(user_data, index, "irr/vcd/total[0]", data);
}

static int read_o3_error(void *user_data, long index, harp_array data)
{
    if (((ingest_info *)user_data)->ozone_vcd == 1)
    {
        return get_relative_error(user_data, index, "irr/vcd/total[1]", "irr/vcd/error[1]", data);
    }
    return get_relative_error(user_data, index, "irr/vcd/total[0]", "irr/vcd/error[0]", data);
}

static int read_no2(void *user_data, long index, harp_array data)
{
    return get_data(user_data, index, "irr/vcd/total[2]", data);
}

static int read_no2_error(void *user_data, long index, harp_array data)
{
    return get_relative_error(user_data, index, "irr/vcd/total[2]", "irr/vcd/error[2]", data);
}

static int read_cloud_fraction(void *user_data, long index, harp_array data)
{
    if (((ingest_info *)user_data)->format_version == 1)
    {
        return get_data(user_data, index, "irr/icfa/frac", data);
    }
    return get_data(user_data, index, "irr/ocra/cloud_frac", data);
}

static int read_cloud_fraction_error(void *user_data, long index, harp_array data)
{
    if (((ingest_info *)user_data)->format_version == 1)
    {
        return get_relative_error(user_data, index, "irr/icfa/frac", "irr/icfa/err_frac", data);
    }
    return get_relative_error(user_data, index, "irr/ocra/cloud_frac", "irr/ocra/cloud_frac_error", data);
}

static int read_cloud_top_height(void *user_data, long index, harp_array data)
{
    return get_data(user_data, index, "irr/rocinn/height", data);
}

static int read_cloud_top_height_error(void *user_data, long index, harp_array data)
{
    return get_relative_error(user_data, index, "irr/rocinn/height", "irr/rocinn/height_error", data);
}

static int read_cloud_top_pressure(void *user_data, long index, harp_array data)
{
    if (((ingest_info *)user_data)->format_version == 1)
    {
        return get_data(user_data, index, "irr/icfa/press", data);
    }
    return get_data(user_data, index, "irr/rocinn/pressure", data);
}

static int read_cloud_top_pressure_error(void *user_data, long index, harp_array data)
{
    if (((ingest_info *)user_data)->format_version == 1)
    {
        return get_relative_error(user_data, index, "irr/icfa/press", "irr/icfa/err_press", data);
    }
    return get_relative_error(user_data, index, "irr/rocinn/pressure", "irr/rocinn/pressure_error", data);
}

static int read_cloud_top_albedo(void *user_data, long index, harp_array data)
{
    return get_data(user_data, index, "irr/rocinn/albedo", data);
}

static int read_cloud_top_albedo_error(void *user_data, long index, harp_array data)
{
    return get_relative_error(user_data, index, "irr/rocinn/albedo", "irr/rocinn/albedo_error", data);
}

static int read_surface_pressure(void *user_data, long index, harp_array data)
{
    if (((ingest_info *)user_data)->format_version == 1)
    {
        return get_data(user_data, index, "irr/icfa/surf_press", data);
    }
    return get_data(user_data, index, "irr/surface_pressure", data);
}

static int read_surface_height(void *user_data, long index, harp_array data)
{
    return get_data(user_data, index, "irr/surface_height", data);
}

static int read_surface_albedo(void *user_data, long index, harp_array data)
{
    return get_data(user_data, index, "irr/surface_albedo", data);
}

static int read_solar_zenith_angle(void *user_data, long index, harp_array data)
{
    return get_data(user_data, index, "glr/sza_toa[1]", data);
}

static int read_los_zenith_angle(void *user_data, long index, harp_array data)
{
    return get_data(user_data, index, "glr/line_sight_toa[1]", data);
}

static int read_rel_azimuth_angle(void *user_data, long index, harp_array data)
{
    return get_data(user_data, index, "glr/rel_azi_toa[1]", data);
}

static int read_scan_subindex(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    coda_cursor cursor;
    int32_t counter;

    cursor = info->ddr_cursor[index];
    if (coda_cursor_goto(&cursor, "glr/subset_counter") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_int32(&cursor, &counter) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    *data.int8_data = (int8_t)counter;

    return 0;
}

static int read_scan_direction_type(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    coda_cursor cursor;
    int32_t counter;

    cursor = info->ddr_cursor[index];
    if (coda_cursor_goto(&cursor, "glr/subset_counter") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_int32(&cursor, &counter) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (counter < 3)
    {
        *data.int8_data = 0;
    }
    else
    {
        *data.int8_data = 1;
    }

    return 0;
}

static void ingestion_done(void *user_data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->ddr_cursor != NULL)
    {
        free(info->ddr_cursor);
    }

    free(info);
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
    info->num_time = 0;
    info->ddr_cursor = NULL;
    info->format_version = -1;
    info->ozone_vcd = 0;

    if (coda_get_product_version(product, &info->format_version) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        ingestion_done(info);
        return -1;
    }

    info->ozone_vcd = 0;
    if (harp_ingestion_options_has_option(options, "ozone"))
    {
        info->ozone_vcd = 1;
    }

    if (init_ddr_cursor(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }

    *definition = *module->product_definition;
    *user_data = info;

    return 0;
}

static int include_v2(void *user_data)
{
    return ((ingest_info *)user_data)->format_version >= 2;
}

int harp_ingestion_module_gome_l2_init(void)
{
    const char *scan_direction_type_values[] = { "forward", "backward" };
    const char *ozone_options[] = { "vcd1" };
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2];
    long bounds_dimension[2] = { -1, 4 };
    const char *error_mapping;
    const char *description;

    dimension_type[0] = harp_dimension_time;
    dimension_type[1] = harp_dimension_independent;

    error_mapping = "relative error is converted to absolute error by multiplying with measured value";

    description = "GOME Level-2 Data";
    module = harp_ingestion_register_module("GOME_L2", "GOME", "ERS_GOME", "GOM.LVL21",
                                            description, ingestion_init, ingestion_done);

    harp_ingestion_register_option(module, "ozone", "the fitting window choice for ozone to ingest; either window 0 "
                                   "(default) or window 1 (ozone=vcd1)", 1, ozone_options);

    product_definition = harp_ingestion_register_product(module, "GOME_L2", "total column data", read_dimensions);

    /* datetime_stop */
    description = "time of the measurement at end of integration time";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "datetime_stop",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, "seconds since 2000-01-01", NULL,
                                                                      read_datetime);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ddr[]/glr/datetime", NULL);

    /* datetime_length */
    description = "measurement integration time";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "datetime_length",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, "s", NULL, read_integration_time);
    description = "set to a fixed value of 1.5s for all pixels";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, NULL, description);

    /* orbit_index */
    description = "absolute orbit number";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "orbit_index", harp_type_int32, 0, NULL, NULL,
                                                   description, NULL, NULL, read_orbit_index);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/pir/start_orbit", NULL);

    /* latitude */
    description = "tangent latitude of the measurement";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "latitude", harp_type_double,
                                                                      1, dimension_type, NULL, description,
                                                                      "degree_north", NULL, read_latitude);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ddr[]/glr/corners[4]/lat", NULL);

    /* longitude */
    description = "tangent longitude of the measurement";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "longitude",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, "degree_east", NULL, read_longitude);
    description = "each longitude will be transformed from a value in the range 0 - 360 to a value in the range -180 "
        "- 180";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ddr[]/glr/corners[4]/lon", description);

    /* latitude_bounds */
    description = "corner latitudes for the ground pixel of the measurement";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "latitude_bounds",
                                                                      harp_type_double, 2, dimension_type,
                                                                      bounds_dimension, description,
                                                                      "degree_north", NULL, read_latitude_bounds);
    description = "the corners are rearranged in the following way: 1,3,2,0";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ddr[]/glr/corners[0:3]/lat", description);

    /* longitude_bounds */
    description = "corner longitudes for the ground pixel of the measurement";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "longitude_bounds",
                                                                      harp_type_double, 2, dimension_type,
                                                                      bounds_dimension, description,
                                                                      "degree_east", NULL, read_longitude_bounds);
    description = "the corners are rearranged in the following way: 1,3,2,0";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ddr[]/glr/corners[0:3]/lon", description);

    /* O3_column_number_density */
    description = "ozone total column";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "O3_column_number_density",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, "molec/cm^2", NULL, read_o3);
    harp_variable_definition_add_mapping(variable_definition, "ozone unset", NULL, "/ddr[]/irr/vcd[0]/total", NULL);
    harp_variable_definition_add_mapping(variable_definition, "ozone=vcd1", NULL, "/ddr[]/irr/vcd[1]/total", NULL);

    /* O3_column_number_density_uncertainty */
    description = "error on the ozone total column";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "O3_column_number_density_uncertainty",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, "molec/cm^2", NULL, read_o3_error);
    harp_variable_definition_add_mapping(variable_definition, "ozone unset", NULL,
                                         "/ddr[]/irr/vcd[0]/total, /ddr[]/irr/vcd[0]/error", error_mapping);
    harp_variable_definition_add_mapping(variable_definition, "ozone=vcd1", NULL,
                                         "/ddr[]/irr/vcd[1]/total, /ddr[]/irr/vcd[1]/error", error_mapping);

    /* NO2_column_number_density */
    description = "NO2 total column";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "NO2_column_number_density",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, "molec/cm^2", NULL, read_no2);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ddr[]/irr/vcd[2]/total", NULL);

    /* NO2_column_number_density_uncertainty */
    description = "error on the NO2 total column";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "NO2_column_number_density_uncertainty",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, "molec/cm^2", NULL, read_no2_error);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/ddr[]/irr/vcd[2]/total, /ddr[]/irr/vcd[2]/error", error_mapping);

    /* cloud_fraction */
    description = "cloud fraction";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "cloud_fraction",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                                      read_cloud_fraction);
    harp_variable_definition_add_mapping(variable_definition, NULL, "version=1", "/ddr[]/irr/icfa/frac", NULL);
    harp_variable_definition_add_mapping(variable_definition, NULL, "version>1", "/ddr[]/irr/ocra/cloud_frac", NULL);

    /* cloud_fraction_uncertainty */
    description = "cloud fraction error";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "cloud_fraction_uncertainty",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                                      read_cloud_fraction_error);
    harp_variable_definition_add_mapping(variable_definition, NULL, "version=1",
                                         "/ddr[]/irr/icfa/frac, /ddr[]/irr/icfa/err_frac", error_mapping);
    harp_variable_definition_add_mapping(variable_definition, NULL, "version>1",
                                         "/ddr[]/irr/ocra/cloud_frac, /ddr[]/irr/ocra/cloud_frac_error", error_mapping);

    /* cloud_top_height */
    description = "cloud top height";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "cloud_top_height",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, "km", include_v2,
                                                                      read_cloud_top_height);
    harp_variable_definition_add_mapping(variable_definition, NULL, "version>1", "/ddr[]/irr/rocinn/height", NULL);

    /* cloud_top_height_uncertainty */
    description = "cloud top height uncertainty";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "cloud_top_height_uncertainty", harp_type_double,
                                                                      1, dimension_type, NULL, description, "km",
                                                                      include_v2, read_cloud_top_height_error);
    harp_variable_definition_add_mapping(variable_definition, NULL, "version>1",
                                         "/ddr[]/irr/rocinn/height, /ddr[]/irr/rocinn/height_error", error_mapping);

    /* cloud_top_pressure */
    description = "cloud top pressure";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "cloud_top_pressure",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, "hPa", NULL,
                                                                      read_cloud_top_pressure);
    harp_variable_definition_add_mapping(variable_definition, NULL, "version=1", "/ddr[]/irr/icfa/press", NULL);
    harp_variable_definition_add_mapping(variable_definition, NULL, "version>1", "/ddr[]/irr/rocinn/pressure", NULL);

    /* cloud_top_pressure_uncertainty */
    description = "cloud top pressure uncertainty";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "cloud_top_pressure_uncertainty",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, "hPa", NULL,
                                                                      read_cloud_top_pressure_error);
    harp_variable_definition_add_mapping(variable_definition, NULL, "version=1",
                                         "/ddr[]/irr/icfa/press, /ddr[]/irr/icfa/err_press", error_mapping);
    harp_variable_definition_add_mapping(variable_definition, NULL, "version>1",
                                         "/ddr[]/irr/rocinn/pressure, /ddr[]/irr/rocinn/pressure_error", error_mapping);

    /* cloud_top_albedo */
    description = "cloud top albedo";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "cloud_top_albedo",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, HARP_UNIT_DIMENSIONLESS, include_v2,
                                                                      read_cloud_top_albedo);
    harp_variable_definition_add_mapping(variable_definition, NULL, "version>1", "/ddr[]/irr/rocinn/albedo", NULL);

    /* cloud_top_albedo_uncertainty */
    description = "cloud top albedo uncertainty";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition,
                                                                      "cloud_top_albedo_uncertainty",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, HARP_UNIT_DIMENSIONLESS, include_v2,
                                                                      read_cloud_top_albedo_error);
    harp_variable_definition_add_mapping(variable_definition, NULL, "version>1",
                                         "/ddr[]/irr/rocinn/albedo, /ddr[]/irr/rocinn/albedo_error", error_mapping);

    /* surface_pressure */
    description = "surface pressure";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "surface_pressure",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, "hPa", NULL, read_surface_pressure);
    harp_variable_definition_add_mapping(variable_definition, NULL, "version=1", "/ddr[]/irr/icfa/surf_press", NULL);
    harp_variable_definition_add_mapping(variable_definition, NULL, "version>1", "/ddr[]/irr/surface_pressure", NULL);

    /* surface_height */
    description = "surface height";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "surface_height",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, "km", include_v2,
                                                                      read_surface_height);
    harp_variable_definition_add_mapping(variable_definition, NULL, "version>1", "/ddr[]/irr/surface_height", NULL);

    /* surface_albedo */
    description = "surface albedo";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "surface_albedo",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, HARP_UNIT_DIMENSIONLESS, include_v2,
                                                                      read_surface_albedo);
    harp_variable_definition_add_mapping(variable_definition, NULL, "version>1", "/ddr[]/irr/surface_albedo", NULL);

    /* solar_zenith_angle */
    description = "solar zenith angle at top of atmosphere";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "solar_zenith_angle",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, "degree", NULL,
                                                                      read_solar_zenith_angle);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ddr[]/glr/sza_toa[1]", NULL);

    /* viewing_zenith_angle */
    description = "line of sight zenith angle at top of atmosphere";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "viewing_zenith_angle",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, "degree", NULL,
                                                                      read_los_zenith_angle);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ddr[]/glr/line_sight_toa[1]", NULL);

    /* relative_azimuth_angle */
    description = "relative azimuth angle at top of atmosphere";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "relative_azimuth_angle",
                                                                      harp_type_double, 1, dimension_type, NULL,
                                                                      description, "degree", NULL,
                                                                      read_rel_azimuth_angle);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ddr[]/glr/rel_azi_toa[1]", NULL);

    /* scan_subindex */
    description = "relative index (0-3) of this measurement within a scan (forward + backward)";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "scan_subindex",
                                                                      harp_type_int8, 1, dimension_type, NULL,
                                                                      description, NULL, NULL, read_scan_subindex);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ddr[]/glr/subset_counter", description);

    /* scan_direction_type */
    description = "scan direction for each measurement";
    variable_definition = harp_ingestion_register_variable_block_read(product_definition, "scan_direction_type",
                                                                      harp_type_int8, 1, dimension_type, NULL,
                                                                      description, NULL, NULL,
                                                                      read_scan_direction_type);
    harp_variable_definition_set_enumeration_values(variable_definition, 2, scan_direction_type_values);
    description =
        "the scan direction is based on the subset_counter of the measurement; 0-2: forward (0), 3: backward (1)";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ddr[]/glr/subset_counter", description);

    return 0;
}
