/*
 * Copyright (C) 2015-2024 S[&]T, The Netherlands.
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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct ingest_info_struct
{
    coda_product *product;
    long num_time;
    int sif_wavelength;
    int use_daily_correction;
} ingest_info;

static void ingestion_done(void *user_data)
{
    ingest_info *info = (ingest_info *)user_data;

    free(info);
}

static int read_dataset(ingest_info *info, const char *path, harp_data_type data_type, long num_elements,
                        harp_array data)
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
    switch (data_type)
    {
        case harp_type_int16:
            if (coda_cursor_read_int16_array(&cursor, data.int16_data, coda_array_ordering_c) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            break;
        case harp_type_double:
            if (coda_cursor_read_double_array(&cursor, data.double_data, coda_array_ordering_c) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            if (coda_cursor_goto(&cursor, "@missing_value[0]") != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            if (coda_cursor_read_double(&cursor, &fill_value.double_data) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            /* Replace values equal to the missing_value variable attribute by NaN. */
            harp_array_replace_fill_value(data_type, num_elements, data, fill_value);
            break;
        default:
            assert(0);
            exit(1);
    }

    return 0;
}

static int read_datetime(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "/Delta_Time", harp_type_double, info->num_time, data);
}

static int read_latitude(void *user_data, harp_array data)
{
    return read_dataset((ingest_info *)user_data, "Latitude", harp_type_double,
                        ((ingest_info *)user_data)->num_time, data);
}

static int read_latitude_corners(void *user_data, harp_array data)
{
    return read_dataset((ingest_info *)user_data, "Latitude_Corners", harp_type_double,
                        ((ingest_info *)user_data)->num_time * 4, data);
}

static int read_longitude(void *user_data, harp_array data)
{
    return read_dataset((ingest_info *)user_data, "Longitude", harp_type_double,
                        ((ingest_info *)user_data)->num_time, data);
}

static int read_longitude_corners(void *user_data, harp_array data)
{
    return read_dataset((ingest_info *)user_data, "Longitude_Corners", harp_type_double,
                        ((ingest_info *)user_data)->num_time * 4, data);
}

static int read_saz(void *user_data, harp_array data)
{
    return read_dataset((ingest_info *)user_data, "SAz", harp_type_double, ((ingest_info *)user_data)->num_time, data);
}

static int read_sza(void *user_data, harp_array data)
{
    return read_dataset((ingest_info *)user_data, "SZA", harp_type_double, ((ingest_info *)user_data)->num_time, data);
}

static int read_vaz(void *user_data, harp_array data)
{
    return read_dataset((ingest_info *)user_data, "VAz", harp_type_double, ((ingest_info *)user_data)->num_time, data);
}

static int read_vza(void *user_data, harp_array data)
{
    return read_dataset((ingest_info *)user_data, "VZA", harp_type_double, ((ingest_info *)user_data)->num_time, data);
}

static int read_sif(void *user_data, harp_array data)
{
    const char *variable_path = NULL;
    ingest_info *info = (ingest_info *)user_data;

    if (info->use_daily_correction)
    {
        switch (info->sif_wavelength)
        {
            case 757:
                variable_path = "Science/SIF_757nm";
                break;
            case 771:
                variable_path = "Science/SIF_771nm";
                break;
            default:
                variable_path = "SIF_740nm";
        }
    }
    else
    {
        switch (info->sif_wavelength)
        {
            case 757:
                variable_path = "Daily_SIF_757nm";
                break;
            case 771:
                variable_path = "Daily_SIF_771nm";
                break;
            default:
                variable_path = "Daily_SIF_740nm";
        }
    }
    return read_dataset(info, variable_path, harp_type_double, info->num_time, data);
}

static int read_sif_uncertainty(void *user_data, harp_array data)
{
    const char *variable_path = NULL;
    ingest_info *info = (ingest_info *)user_data;

    switch (info->sif_wavelength)
    {
        case 757:
            variable_path = "Science/SIF_Uncertainty_757nm";
            break;
        case 771:
            variable_path = "Science/SIF_Uncertainty_771nm";
            break;
        default:
            variable_path = "SIF_Uncertainty_740nm";
    }

    return read_dataset(info, variable_path, harp_type_double, info->num_time, data);
}

static int read_quality_flag(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "quality_flag", harp_type_int16, info->num_time, data);
}

static int read_wavelength(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    switch (info->sif_wavelength)
    {
        case 757:
            data.double_data[0] = 757;
            break;
        case 771:
            data.double_data[0] = 771;
            break;
        default:
            data.double_data[0] = 740;
    }

    return 0;
}

static int read_dimensions(void *user_data, long dimension[HARP_NUM_DIM_TYPES])
{
    ingest_info *info = (ingest_info *)user_data;

    dimension[harp_dimension_time] = info->num_time;

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
    if (coda_cursor_goto(&cursor, "/Delta_Time") != 0)
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

    return 0;
}

static int ingestion_init(const harp_ingestion_module *module, coda_product *product,
                          const harp_ingestion_options *options, harp_product_definition **definition, void **user_data)
{
    const char *option_value;
    ingest_info *info;

    info = malloc(sizeof(ingest_info));
    if (info == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(ingest_info), __FILE__, __LINE__);
        return -1;
    }
    info->product = product;
    info->sif_wavelength = 740;
    info->use_daily_correction = 0;

    if (harp_ingestion_options_has_option(options, "daily_correction"))
    {
        info->use_daily_correction = 1;
    }
    if (harp_ingestion_options_has_option(options, "sif_wavelength"))
    {
        if (harp_ingestion_options_get_option(options, "sif_wavelength", &option_value) != 0)
        {
            ingestion_done(info);
            return -1;
        }
        if (strcmp(option_value, "757") == 0)
        {
            info->sif_wavelength = 757;
        }
        else if (strcmp(option_value, "771") == 0)
        {
            info->sif_wavelength = 771;
        }
    }

    if (init_dimensions(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }
    *definition = *module->product_definition;
    *user_data = info;

    return 0;
}

static int include_sif_uncertainty(void *user_data)
{
    return !((ingest_info *)user_data)->use_daily_correction;
}

static void register_fields(harp_product_definition *product_definition)
{
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_independent };
    long bounds_dimension[2] = { -1, 4 };
    const char *description;
    const char *path;

    /* datetime */
    description = "time";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double, 1, dimension_type,
                                                   NULL, description, "seconds since 1990-01-01", NULL, read_datetime);
    path = "/Delta_Time[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* latitude */
    description = "center latitude";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_double, 1, dimension_type,
                                                   NULL, description, "degree_north", NULL, read_latitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    path = "/Latitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* longitude */
    description = "center longitude";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_double, 1, dimension_type,
                                                   NULL, description, "degree_east", NULL, read_longitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0, 180.0);
    path = "/Longitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* latitude_bounds */
    description = "corner latitude";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "latitude_bounds", harp_type_double, 2,
                                                   dimension_type, bounds_dimension, description, "degree_north",
                                                   NULL, read_latitude_corners);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    path = "/Latitude_Corners[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* longitude */
    description = "corner longitude";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "longitude_bounds", harp_type_double, 2,
                                                   dimension_type, bounds_dimension, description, "degree_east",
                                                   NULL, read_longitude_corners);
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0, 180.0);
    path = "/Longitude_Corners[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* solar_azimuth_angle */
    description = "solar azimuth angle";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "solar_azimuth_angle", harp_type_double, 1,
                                                   dimension_type, NULL, description, "degree", NULL, read_saz);
    path = "/SZa[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* solar_zenith_angle */
    description = "solar zenith angle";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "solar_zenith_angle", harp_type_double, 1,
                                                   dimension_type, NULL, description, "degree", NULL, read_sza);
    path = "/SZA[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* viewing_azimuth_angle */
    description = "viewing azimuth angle";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "viewing_azimuth_angle", harp_type_double, 1,
                                                   dimension_type, NULL, description, "degree", NULL, read_vaz);
    path = "/VAz[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* viewing_zenith_angle */
    description = "viewing zenith angle";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "viewing_zenith_angle", harp_type_double, 1,
                                                   dimension_type, NULL, description, "degree", NULL, read_vza);
    path = "/VZA[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* solar_induced_fluorescence */
    description = "Solar Induced Fluorescence";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "solar_induced_fluorescence",
                                                   harp_type_double, 1, dimension_type, NULL, description, "", NULL,
                                                   read_sif);
    path = "/SIF_740nm[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "sif_wavelength unset, daily_correction unset",
                                         path, NULL);
    path = "/Science/SIF_757nm[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "sif_wavelength=757, daily_correction unset", path,
                                         NULL);
    path = "/Science/SIF_771nm[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "sif_wavelength=771, daily_correction unset", path,
                                         NULL);
    path = "/Daily_SIF_740nm[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "sif_wavelength unset, daily_correction=applied",
                                         path, NULL);
    path = "/Daily_SIF_757nm[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "sif_wavelength=757, daily_correction=applied",
                                         path, NULL);
    path = "/Daily_SIF_771nm[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "sif_wavelength=771, daily_correction=applied",
                                         path, NULL);

    /* solar_induced_fluorescence_uncertainty */
    description = "Estimated 1-Sigma Uncertainty of Solar Induced Fluorescence";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "solar_induced_fluorescence_uncertainty",
                                                   harp_type_double, 1, dimension_type, NULL, description, "",
                                                   include_sif_uncertainty, read_sif_uncertainty);
    path = "/SIF_Uncertainty_740nm[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "sif_wavelength unset, daily_correction unset",
                                         path, NULL);
    path = "/Science/SIF_Uncertainty_757nm[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "sif_wavelength=757, daily_correction unset", path,
                                         NULL);
    path = "/Science/SIF_Uncertainty_771nm[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "sif_wavelength=771, daily_correction unset", path,
                                         NULL);

    /* wavelength */
    description = "SIF wavelength";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "wavelength", harp_type_double,
                                                                     0, NULL, NULL, description, "nm", NULL,
                                                                     read_wavelength);
    harp_variable_definition_add_mapping(variable_definition, NULL, "sif_wavelength unset", NULL, "741");
    harp_variable_definition_add_mapping(variable_definition, NULL, "sif_wavelength=757", NULL, "757");
    harp_variable_definition_add_mapping(variable_definition, NULL, "sif_wavelength=771", NULL, "771");

    /* validity */
    description = "quality flag";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "validity", harp_type_int16, 1, dimension_type,
                                                   NULL, description, NULL, NULL, read_quality_flag);
    path = "/Quality_Flag[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

static void register_options(harp_ingestion_module *module)
{
    const char *sif_wavelength_options[] = { "757", "771" };
    const char *sif_daily_correction[] = { "applied" };

    harp_ingestion_register_option(module, "sif_wavelength", "whether to ingest SIF at 740nm (default) or the "
                                   "one at 757nm (sif_wavelength=757) or the one at 771nm (sif_wavelength=771)", 2,
                                   sif_wavelength_options);

    harp_ingestion_register_option(module, "daily_correction", "whether to ingest the instantaneous SIF (default) or "
                                   "the daily averaged SIF based on geometric correction (daily_correction=applied)", 1,
                                   sif_daily_correction);
}

static void register_module_oco2_LtSIF(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;

    module = harp_ingestion_register_module("OCO_OCO2_LtSIF", "OCO", "OCO", "oco2_LtSIF", "OCO-2 L2 Lite SIF",
                                            ingestion_init, ingestion_done);
    register_options(module);
    product_definition = harp_ingestion_register_product(module, "OCO_OCO2_LtSIF", NULL, read_dimensions);

    register_fields(product_definition);
}

static void register_module_oco3_LtSIF(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;

    module = harp_ingestion_register_module("OCO_OCO3_LtSIF", "OCO", "OCO", "oco3_LtSIF", "OCO-3 L2 Lite SIF",
                                            ingestion_init, ingestion_done);
    register_options(module);
    product_definition = harp_ingestion_register_product(module, "OCO_OCO3_LtSIF", NULL, read_dimensions);

    register_fields(product_definition);
}

int harp_ingestion_module_oco_ltsif_init(void)
{
    register_module_oco2_LtSIF();
    register_module_oco3_LtSIF();
    return 0;
}
