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
#include "harp-geometry.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct ingest_info_struct
{
    coda_product *product;
    long num_time;
    long num_vertical;
    long num_along_track;
    long num_across_track;

    /* ingestion options */
    int bias_corrected;
} ingest_info;


static int read_dimensions(void *user_data, long dimension[HARP_NUM_DIM_TYPES])
{
    dimension[harp_dimension_time] = ((ingest_info *)user_data)->num_time;
    dimension[harp_dimension_vertical] = ((ingest_info *)user_data)->num_vertical;
    return 0;
}

static int read_array(ingest_info *info, const char *path, harp_data_type data_type, long num_elements, harp_array data)
{
    long coda_num_elements;
    coda_cursor cursor;

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
        harp_set_error(HARP_ERROR_INGESTION, "variable has %ld elements; expected %ld", coda_num_elements,
                       num_elements);
        harp_add_coda_cursor_path_to_error_message(&cursor);
        return -1;
    }

    switch (data_type)
    {
        case harp_type_int8:
            {
                coda_native_type read_type;

                if (coda_cursor_goto_first_array_element(&cursor) != 0)
                {
                    harp_set_error(HARP_ERROR_CODA, NULL);
                    return -1;
                }
                if (coda_cursor_get_read_type(&cursor, &read_type) != 0)
                {
                    harp_set_error(HARP_ERROR_CODA, NULL);
                    return -1;
                }
                coda_cursor_goto_parent(&cursor);
                if (read_type == coda_native_type_uint8)
                {
                    if (coda_cursor_read_uint8_array(&cursor, (uint8_t *)data.int8_data, coda_array_ordering_c) != 0)
                    {
                        harp_set_error(HARP_ERROR_CODA, NULL);
                        return -1;
                    }
                }
                else
                {
                    if (coda_cursor_read_int8_array(&cursor, data.int8_data, coda_array_ordering_c) != 0)
                    {
                        harp_set_error(HARP_ERROR_CODA, NULL);
                        return -1;
                    }
                }
            }
            break;
        case harp_type_int32:
            {
                coda_native_type read_type;

                if (coda_cursor_goto_first_array_element(&cursor) != 0)
                {
                    harp_set_error(HARP_ERROR_CODA, NULL);
                    return -1;
                }
                if (coda_cursor_get_read_type(&cursor, &read_type) != 0)
                {
                    harp_set_error(HARP_ERROR_CODA, NULL);
                    return -1;
                }
                coda_cursor_goto_parent(&cursor);
                if (read_type == coda_native_type_uint32)
                {
                    if (coda_cursor_read_uint32_array(&cursor, (uint32_t *)data.int32_data, coda_array_ordering_c) != 0)
                    {
                        harp_set_error(HARP_ERROR_CODA, NULL);
                        return -1;
                    }
                }
                else
                {
                    if (coda_cursor_read_int32_array(&cursor, data.int32_data, coda_array_ordering_c) != 0)
                    {
                        harp_set_error(HARP_ERROR_CODA, NULL);
                        return -1;
                    }
                }
            }
            break;
        case harp_type_float:
            if (coda_cursor_read_float_array(&cursor, data.float_data, coda_array_ordering_c) != 0)
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
            break;
        default:
            assert(0);
            exit(1);
    }

    return 0;
}

static int read_profile_array(ingest_info *info, const char *path, harp_data_type data_type, harp_array data)
{
    long dimension[2];

    if (read_array(info, path, data_type, info->num_time * info->num_vertical, data) != 0)
    {
        return -1;
    }

    /* invert axis */
    dimension[0] = info->num_time;
    dimension[1] = info->num_vertical;
    return harp_array_invert(data_type, 1, 2, dimension, data);

}

static int read_uncertainty_profile_array(ingest_info *info, const char *var_path, const char *uncertainty_path,
                                          harp_array data)
{
    harp_array array;
    long i;

    array.ptr = malloc(info->num_time * info->num_vertical * sizeof(double));
    if (array.ptr == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       info->num_time * info->num_vertical * sizeof(double), __FILE__, __LINE__);
        return -1;
    }
    if (read_profile_array(info, var_path, harp_type_double, array) != 0)
    {
        free(array.ptr);
        return -1;
    }

    if (read_profile_array(info, uncertainty_path, harp_type_double, data) != 0)
    {
        free(array.ptr);
        return -1;
    }

    /* turn relative error [%] into absolute uncertainty */
    for (i = 0; i < info->num_time * info->num_vertical; i++)
    {
        data.double_data[i] *= array.double_data[i] / 100.0;
    }

    free(array.ptr);

    return 0;
}

static int init_dimensions(ingest_info *info)
{
    coda_cursor cursor;
    long dim[CODA_MAX_NUM_DIMS];
    int num_dims;
    long index;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&cursor, "ScienceData") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&cursor, "Geo") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&cursor, "latitude") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_array_dim(&cursor, &num_dims, dim) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    assert(num_dims > 0);
    info->num_along_track = dim[0];
    info->num_time = info->num_along_track;
    if (num_dims > 1)
    {
        assert(num_dims == 2);
        info->num_across_track = dim[1];
        info->num_time *= info->num_across_track;
    }
    coda_cursor_goto_parent(&cursor);

    /* num_vertical */
    if (coda_cursor_get_record_field_index_from_name(&cursor, "height", &index) == 0)
    {
        if (coda_cursor_goto_record_field_by_name(&cursor, "height") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_get_num_elements(&cursor, &info->num_vertical) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        info->num_vertical /= info->num_time;
    }
    else if (coda_cursor_get_record_field_index_from_name(&cursor, "bin_height", &index) == 0)
    {
        if (coda_cursor_goto_record_field_by_name(&cursor, "bin_height") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_get_num_elements(&cursor, &info->num_vertical) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        info->num_vertical /= info->num_time;
    }
    else if (coda_cursor_get_record_field_index_from_name(&cursor, "binHeight", &index) == 0)
    {
        if (coda_cursor_goto_record_field_by_name(&cursor, "binHeight") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_get_num_elements(&cursor, &info->num_vertical) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        info->num_vertical /= info->num_time;
    }

    return 0;
}

static int read_aerosol_backscatter_10km(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_profile_array(info, "/ScienceData/Data/aerosol_backscatter_10km", harp_type_double, data);
}

static int read_aerosol_extinction_10km(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_profile_array(info, "/ScienceData/Data/aerosol_extinction_10km", harp_type_double, data);
}

static int read_aerosol_lidar_ratio_10km(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_profile_array(info, "/ScienceData/Data/aerosol_lidar_ratio_10km", harp_type_double, data);
}

static int read_bin_height(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_profile_array(info, "/ScienceData/Geo/bin_height", harp_type_double, data);
}

static int read_binHeight(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_profile_array(info, "/ScienceData/Geo/binHeight", harp_type_double, data);
}

static int read_cloud_air_velocity_10km(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_profile_array(info, "/ScienceData/Data/cloud_air_velocity_10km", harp_type_double, data);
}

static int read_cloud_backscatter_10km(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_profile_array(info, "/ScienceData/Data/cloud_backscatter_10km", harp_type_double, data);
}

static int read_cloud_extinction_10km(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_profile_array(info, "/ScienceData/Data/cloud_extinction_10km", harp_type_double, data);
}

static int read_cloud_ice_content_10km(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_profile_array(info, "/ScienceData/Data/cloud_ice_content_10km", harp_type_double, data);
}

static int read_cloud_ice_content_10km_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_uncertainty_profile_array(info, "/ScienceData/Data/cloud_ice_content_10km",
                                          "/ScienceData/Data/cloud_ice_content_10km_uncertainty", data);
}

static int read_cloud_ice_effective_radius_10km(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_profile_array(info, "/ScienceData/Data/cloud_ice_effective_radius_10km", harp_type_double, data);
}

static int read_cloud_ice_effective_radius_10km_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_uncertainty_profile_array(info, "/ScienceData/Data/cloud_ice_effective_radius_10km",
                                          "/ScienceData/Data/cloud_ice_effective_radius_10km_uncertainty", data);
}

static int read_cloud_lidar_ratio_10km(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_profile_array(info, "/ScienceData/Data/cloud_lidar_ratio_10km", harp_type_double, data);
}

static int read_cloud_optical_thickness(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info, "/ScienceData/Data/cloud_optical_thickness", harp_type_double, info->num_time, data);
}

static int read_cloud_top_height(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info, "/ScienceData/Data/cloud_top_height", harp_type_double, info->num_time, data);
}

static int read_cloud_top_pressure(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info, "/ScienceData/Data/cloud_top_pressure", harp_type_double, info->num_time, data);
}

static int read_cloud_top_temperature(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info, "/ScienceData/Data/cloud_top_temperature", harp_type_double, info->num_time, data);
}

static int read_cloud_water_content_10km(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_profile_array(info, "/ScienceData/Data/cloud_water_content_10km", harp_type_double, data);
}

static int read_cloud_water_content_10km_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_uncertainty_profile_array(info, "/ScienceData/Data/cloud_water_content_10km",
                                          "/ScienceData/Data/cloud_water_content_10km_uncertainty", data);
}

static int read_cloud_water_effective_radius_10km(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_profile_array(info, "/ScienceData/Data/cloud_water_effective_radius_10km", harp_type_double, data);
}

static int read_cloud_water_effective_radius_10km_uncertainty(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_uncertainty_profile_array(info, "/ScienceData/Data/cloud_water_effective_radius_10km",
                                          "/ScienceData/Data/cloud_water_effective_radius_10km_uncertainty", data);
}

static int read_doppler_velocity(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_profile_array(info, "/ScienceData/Data/dopplerVelocity", harp_type_double, data);
}

static int read_doppler_velocity_eco(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    const char *path = "/ScienceData/Data/integrated_doppler_velocity_10km";

    if (info->bias_corrected)
    {
        path = "/ScienceData/Data/integrated_doppler_velocity_10km_bias_corr";
    }

    return read_profile_array(info, path, harp_type_double, data);
}

static int read_doppler_velocity_quality_flag_eco(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    const char *path = "/ScienceData/Data/doppler_velocity_quality_flag_10km";

    if (info->bias_corrected)
    {
        path = "/ScienceData/Data/doppler_velocity_quality_flag_10km_bias_corr";
    }

    return read_profile_array(info, path, harp_type_int32, data);
}

static int read_height(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_profile_array(info, "/ScienceData/Geo/height", harp_type_double, data);
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
    if (coda_cursor_goto(&cursor, "/HeaderData/VariableProductHeader/MainProductHeader/orbitNumber[0]") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_uint32(&cursor, (uint32_t *)data.int32_data) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    return 0;
}

static int read_latitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info, "/ScienceData/Geo/latitude", harp_type_double, info->num_time, data);
}

static int read_longitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info, "/ScienceData/Geo/longitude", harp_type_double, info->num_time, data);
}

static int read_optical_thickness_10km(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info, "/ScienceData/Data/optical_thickness_10km", harp_type_double, info->num_time, data);
}

static int read_profile_time(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info, "/ScienceData/Geo/profileTime", harp_type_double, info->num_time, data);
}

static int read_quality_flag(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info, "/ScienceData/Data/quality_flag", harp_type_int8, info->num_time, data);
}

static int read_quality_flag_10km(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_profile_array(info, "/ScienceData/Data/quality_flag_10km", harp_type_int8, data);
}

static int read_radar_reflectivity_factor(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_profile_array(info, "/ScienceData/Data/radarReflectivityFactor", harp_type_double, data);
}

static int read_radar_reflectivity_eco(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_profile_array(info, "/ScienceData/Data/integrated_radar_reflectivity_10km", harp_type_double, data);
}

static int read_solar_azimuth_angle(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info, "/ScienceData/Geo/solarAzimuthAngle", harp_type_double, info->num_time, data);
}

static int read_solar_elevation_angle(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info, "/ScienceData/Geo/solarElevationAngle", harp_type_double, info->num_time, data);
}

static int read_surface_elevation(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info, "/ScienceData/Geo/surfaceElevation", harp_type_double, info->num_time, data);
}

static int read_time(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (read_array(info, "/ScienceData/Geo/time", harp_type_double, info->num_along_track, data) != 0)
    {
        return -1;
    }

    /* replicate time value for all across elements */
    if (info->num_across_track > 1)
    {
        long i, j;

        for (i = info->num_along_track - 1; i >= 0; i--)
        {
            long offset = i * info->num_across_track;

            for (j = 0; j < info->num_across_track; j++)
            {
                data.double_data[offset + j] = data.double_data[i];
            }
        }
    }

    return 0;
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

    info = malloc(sizeof(ingest_info));
    if (info == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(ingest_info), __FILE__, __LINE__);
        return -1;
    }
    info->product = product;
    info->num_time = 0;
    info->num_vertical = 0;
    info->num_along_track = 0;
    info->num_across_track = 0;
    info->bias_corrected = 0;
    *definition = module->product_definition[0];

    if (harp_ingestion_options_has_option(options, "bias_corrected"))
    {
        info->bias_corrected = 1;
    }

    if (init_dimensions(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }

    *user_data = info;

    return 0;
}

static void register_cpr_nom_1b_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2];
    const char *description;

    description = "CPR L1 Nominal product (JAXA)";
    module = harp_ingestion_register_module("ECA_CPR_NOM_1B", "EarthCARE", "EARTHCARE", "CPR_NOM_1B", description,
                                            ingestion_init, ingestion_done);

    product_definition = harp_ingestion_register_product(module, "ECA_CPR_NOM_1B", NULL, read_dimensions);

    dimension_type[0] = harp_dimension_time;
    dimension_type[1] = harp_dimension_vertical;

    /* datetime */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double,
                                                                     1, dimension_type, NULL, "UTC time",
                                                                     "seconds since 2000-01-01", NULL,
                                                                     read_profile_time);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/Geo/profileTime", NULL);

    /* latitude */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_double,
                                                                     1, dimension_type, NULL, "latitude",
                                                                     "degree_north", NULL, read_latitude);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/Geo/latitude", NULL);

    /* longitude */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_double,
                                                                     1, dimension_type, NULL, "longitude",
                                                                     "degree_east", NULL, read_longitude);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/Geo/longitude", NULL);

    /* altitude */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "altitude", harp_type_double,
                                                                     2, dimension_type, NULL, "altitude", "m", NULL,
                                                                     read_binHeight);
    description = "the vertical grid is inverted to make it ascending";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/Geo/binHeight", description);

    /* solar_azimuth_angle */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "solar_azimuth_angle",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     "solar azimuth angle", HARP_UNIT_ANGLE, NULL,
                                                                     read_solar_azimuth_angle);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/Geo/solarAzimuthAngle", NULL);

    /* solar_elevation_angle */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "solar_elevation_angle",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     "solar elevation angle", HARP_UNIT_ANGLE, NULL,
                                                                     read_solar_elevation_angle);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/Geo/solarElevationAngle", NULL);

    /* surface_altitude */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "surface_altitude",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     "solar elevation", "m", NULL,
                                                                     read_surface_elevation);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/Geo/surfaceElevation", NULL);

    /* orbit_index */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "orbit_index", harp_type_int32,
                                                                     0, NULL, NULL, "absolute orbit number", NULL, NULL,
                                                                     read_orbit_index);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/HeaderData/VariableProductHeader/MainProductHeader/orbitNumber", NULL);

    /* doppler_velocity */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "doppler_velocity",
                                                                     harp_type_double, 2, dimension_type, NULL,
                                                                     "doppler velocity", "m/s", NULL,
                                                                     read_doppler_velocity);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/Data/dopplerVelocity",
                                         description);

    /* radar_reflectivity_factor */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "radar_reflectivity_factor",
                                                                     harp_type_double, 2, dimension_type, NULL,
                                                                     "radar reflectivity factor", "mm6/m3", NULL,
                                                                     read_radar_reflectivity_factor);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/Data/radarReflectivityFactor",
                                         description);
}

static void register_atl_cla_2a_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2];
    const char *description;

    description = "ATLID One-sensor Cloud and Aerosol Product (JAXA)";
    module = harp_ingestion_register_module("ECA_ATL_CLA_2A", "EarthCARE", "EARTHCARE", "ATL_CLA_2A", description,
                                            ingestion_init, ingestion_done);

    product_definition = harp_ingestion_register_product(module, "ECA_ATL_CLA_2A", NULL, read_dimensions);

    dimension_type[0] = harp_dimension_time;
    dimension_type[1] = harp_dimension_vertical;

    /* datetime */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double,
                                                                     1, dimension_type, NULL, "UTC time",
                                                                     "seconds since 2000-01-01", NULL, read_time);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/Geo/time", NULL);

    /* latitude */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_double,
                                                                     1, dimension_type, NULL, "latitude",
                                                                     "degree_north", NULL, read_latitude);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/Geo/latitude", NULL);

    /* longitude */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_double,
                                                                     1, dimension_type, NULL, "longitude",
                                                                     "degree_east", NULL, read_longitude);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/Geo/longitude", NULL);

    /* altitude */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "altitude", harp_type_double,
                                                                     2, dimension_type, NULL, "altitude", "m", NULL,
                                                                     read_height);
    description = "the vertical grid is inverted to make it ascending";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/Geo/height", description);

    /* orbit_index */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "orbit_index", harp_type_int32,
                                                                     0, NULL, NULL, "absolute orbit number", NULL, NULL,
                                                                     read_orbit_index);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/HeaderData/VariableProductHeader/MainProductHeader/orbitNumber", NULL);

    /* validity */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "validity", harp_type_int8,
                                                                     2, dimension_type, NULL, "quality flag 10km", NULL,
                                                                     NULL, read_quality_flag_10km);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/Data/quality_flag_10km", NULL);

    /* aerosol_backscatter_coefficient */
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "aerosol_backscatter_coefficient",
                                                   harp_type_double, 2, dimension_type, NULL,
                                                   "aerosol backscatter 10km", "1/m/sr", NULL,
                                                   read_aerosol_backscatter_10km);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/ScienceData/Data/aerosol_backscatter_10km", description);

    /* aerosol_extinction_coefficient */
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "aerosol_extinction_coefficient",
                                                   harp_type_double, 2, dimension_type, NULL,
                                                   "aerosol extinction 10km", "1/m/sr", NULL,
                                                   read_aerosol_extinction_10km);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/ScienceData/Data/aerosol_extinction_10km", description);

    /* aerosol_lidar_ratio */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "aerosol_lidar_ratio",
                                                                     harp_type_double, 2, dimension_type, NULL,
                                                                     "aerosol lidar ratio 10km", "sr",
                                                                     NULL, read_aerosol_lidar_ratio_10km);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/ScienceData/Data/aerosol_lidar_ratio_10km", description);

    /* cloud_backscatter_coefficient */
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_backscatter_coefficient",
                                                   harp_type_double, 2, dimension_type, NULL,
                                                   "cloud backscatter 10km", "1/m/sr", NULL,
                                                   read_cloud_backscatter_10km);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/ScienceData/Data/cloud_backscatter_10km", description);

    /* cloud_extinction_coefficient */
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_extinction_coefficient",
                                                   harp_type_double, 2, dimension_type, NULL,
                                                   "cloud extinction 10km", "1/m/sr", NULL, read_cloud_extinction_10km);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/ScienceData/Data/cloud_extinction_10km", description);

    /* cloud_lidar_ratio */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "cloud_lidar_ratio",
                                                                     harp_type_double, 2, dimension_type, NULL,
                                                                     "cloud lidar ratio 10km", "sr",
                                                                     NULL, read_cloud_lidar_ratio_10km);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/ScienceData/Data/cloud_lidar_ratio_10km", description);
}

static void register_cpr_clp_2a_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2];
    const char *description;
    const char *uncertainty_description;

    description = "CPR One-sensor Cloud Product (JAXA)";
    module = harp_ingestion_register_module("ECA_CPR_CLP_2A", "EarthCARE", "EARTHCARE", "CPR_CLP_2A", description,
                                            ingestion_init, ingestion_done);

    product_definition = harp_ingestion_register_product(module, "ECA_CPR_CLP_2A", NULL, read_dimensions);

    dimension_type[0] = harp_dimension_time;
    dimension_type[1] = harp_dimension_vertical;

    /* datetime */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double,
                                                                     1, dimension_type, NULL, "UTC time",
                                                                     "seconds since 2000-01-01", NULL, read_time);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/Geo/time", NULL);

    /* latitude */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_double,
                                                                     1, dimension_type, NULL, "latitude",
                                                                     "degree_north", NULL, read_latitude);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/Geo/latitude", NULL);

    /* longitude */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_double,
                                                                     1, dimension_type, NULL, "longitude",
                                                                     "degree_east", NULL, read_longitude);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/Geo/longitude", NULL);

    /* altitude */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "altitude", harp_type_double,
                                                                     2, dimension_type, NULL, "altitude", "m", NULL,
                                                                     read_height);
    description = "the vertical grid is inverted to make it ascending";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/Geo/height", description);

    /* orbit_index */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "orbit_index", harp_type_int32,
                                                                     0, NULL, NULL, "absolute orbit number", NULL, NULL,
                                                                     read_orbit_index);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/HeaderData/VariableProductHeader/MainProductHeader/orbitNumber", NULL);

    /* vertical_wind_velocity */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "vertical_wind_velocity",
                                                                     harp_type_double, 2, dimension_type, NULL,
                                                                     "virtical air velocity", "m/s",
                                                                     NULL, read_cloud_air_velocity_10km);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/Data/cloud_air_velocity_10km",
                                         description);

    /* ice_water_density */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "ice_water_density",
                                                                     harp_type_double, 2, dimension_type, NULL,
                                                                     "ice water content", "g/m3",
                                                                     NULL, read_cloud_ice_content_10km);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/Data/cloud_ice_content_10km",
                                         description);

    /* ice_water_density_uncertainty */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition,
                                                                     "ice_water_density_uncertainty", harp_type_double,
                                                                     2, dimension_type, NULL,
                                                                     "uncertainty in ice water content", "g/m3",
                                                                     NULL, read_cloud_ice_content_10km_uncertainty);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/ScienceData/Data/cloud_ice_content_10km_uncertainty", description);

    /* ice_water_effective_radius */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "ice_water_effective_radius",
                                                                     harp_type_double, 2, dimension_type, NULL,
                                                                     "effective radius of ice cloud", "um",
                                                                     NULL, read_cloud_ice_effective_radius_10km);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/ScienceData/Data/cloud_ice_effective_radius_10km", description);

    /* ice_water_effective_radius_uncertainty */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition,
                                                                     "ice_water_effective_radius_uncertainty",
                                                                     harp_type_double, 2, dimension_type, NULL,
                                                                     "uncertainty in effective radius of ice cloud",
                                                                     "um", NULL,
                                                                     read_cloud_ice_effective_radius_10km_uncertainty);
    uncertainty_description = "the relative error in % is turned into an absolute error; "
        "the vertical grid is inverted to make it ascending";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/ScienceData/Data/cloud_ice_effective_radius_10km, "
                                         "/ScienceData/Data/cloud_ice_effective_radius_10km_uncertainty",
                                         uncertainty_description);

    /* liquid_water_density */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "liquid_water_density",
                                                                     harp_type_double, 2, dimension_type, NULL,
                                                                     "liquid water content", "g/m3",
                                                                     NULL, read_cloud_water_content_10km);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/Data/cloud_water_content_10km",
                                         description);

    /* liquid_water_density_uncertainty */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition,
                                                                     "liquid_water_density_uncertainty",
                                                                     harp_type_double, 2, dimension_type, NULL,
                                                                     "uncertainty in liquid water content", "g/m3",
                                                                     NULL, read_cloud_water_content_10km_uncertainty);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/ScienceData/Data/cloud_water_content_10km, "
                                         "/ScienceData/Data/cloud_water_content_10km_uncertainty",
                                         uncertainty_description);

    /* cloud_water_effective_radius */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "cloud_water_effective_radius",
                                                                     harp_type_double, 2, dimension_type, NULL,
                                                                     "effective radius of liquid water cloud", "um",
                                                                     NULL, read_cloud_water_effective_radius_10km);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/ScienceData/Data/cloud_water_effective_radius_10km", description);

    /* cloud_water_effective_radius_uncertainty */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition,
                                                                     "cloud_water_effective_radius_uncertainty",
                                                                     harp_type_double, 2, dimension_type, NULL,
                                                                     "uncertainty in effective radius of liquid water "
                                                                     "cloud", "um", NULL,
                                                                     read_cloud_water_effective_radius_10km_uncertainty);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/ScienceData/Data/cloud_water_effective_radius_10km, "
                                         "/ScienceData/Data/cloud_water_effective_radius_10km_uncertainty",
                                         uncertainty_description);

    /* optical_depth */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "optical_depth",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     "optical thickness", HARP_UNIT_DIMENSIONLESS,
                                                                     NULL, read_optical_thickness_10km);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/Data/optical_thickness_10km",
                                         NULL);
}

static void register_cpr_eco_2a_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2];
    const char *bias_corrected_option_values[1] = { "true" };
    const char *description;

    description = "CPR One-sensor Echo Product (JAXA)";
    module = harp_ingestion_register_module("ECA_CPR_ECO_2A", "EarthCARE", "EARTHCARE", "CPR_ECO_2A", description,
                                            ingestion_init, ingestion_done);

    description = "whether to ingest the bias corrected data: false (default), true (bias_corrected=true)";
    harp_ingestion_register_option(module, "bias_corrected", description, 1, bias_corrected_option_values);

    product_definition = harp_ingestion_register_product(module, "ECA_CPR_ECO_2A", NULL, read_dimensions);

    dimension_type[0] = harp_dimension_time;
    dimension_type[1] = harp_dimension_vertical;

    /* datetime */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double,
                                                                     1, dimension_type, NULL, "UTC time",
                                                                     "seconds since 2000-01-01", NULL, read_time);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/Geo/time", NULL);

    /* latitude */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_double,
                                                                     1, dimension_type, NULL, "latitude",
                                                                     "degree_north", NULL, read_latitude);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/Geo/latitude", NULL);

    /* longitude */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_double,
                                                                     1, dimension_type, NULL, "longitude",
                                                                     "degree_east", NULL, read_longitude);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/Geo/longitude", NULL);

    /* altitude */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "altitude", harp_type_double,
                                                                     2, dimension_type, NULL, "altitude", "m", NULL,
                                                                     read_bin_height);
    description = "the vertical grid is inverted to make it ascending";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/Geo/bin_height", description);

    /* orbit_index */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "orbit_index", harp_type_int32,
                                                                     0, NULL, NULL, "absolute orbit number", NULL, NULL,
                                                                     read_orbit_index);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/HeaderData/VariableProductHeader/MainProductHeader/orbitNumber", NULL);

    /* doppler_velocity */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "doppler_velocity",
                                                                     harp_type_double, 2, dimension_type, NULL,
                                                                     "doppler velocity 10km", "m/s",
                                                                     NULL, read_doppler_velocity_eco);
    harp_variable_definition_add_mapping(variable_definition, "bias_corrected unset", NULL,
                                         "/ScienceData/Data/integrated_doppler_velocity_10km", description);
    harp_variable_definition_add_mapping(variable_definition, "bias_corrected=true", NULL,
                                         "/ScienceData/Data/integrated_doppler_velocity_10km_bias_corr", description);

    /* doppler_velocity_validity */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "doppler_velocity_validity",
                                                                     harp_type_int32, 2, dimension_type, NULL,
                                                                     "quality flag 10km", NULL, NULL,
                                                                     read_doppler_velocity_quality_flag_eco);
    harp_variable_definition_add_mapping(variable_definition, "bias_corrected unset", NULL,
                                         "/ScienceData/Data/doppler_velocity_quality_flag_10km", description);
    harp_variable_definition_add_mapping(variable_definition, "bias_corrected=true", NULL,
                                         "/ScienceData/Data/doppler_velocity_quality_flag_10km_bias_corr", description);

    /* radar_reflectivity_factor */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "radar_reflectivity_factor",
                                                                     harp_type_double, 2, dimension_type, NULL,
                                                                     "radar reflectivity 10km", "mm6/m3",
                                                                     NULL, read_radar_reflectivity_eco);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/ScienceData/Data/integrated_radar_reflectivity_10km", description);
}

static void register_msi_clp_2a_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2];
    const char *description;

    description = "MSI One-sensor Cloud Product (JAXA)";
    module = harp_ingestion_register_module("ECA_MSI_CLP_2A", "EarthCARE", "EARTHCARE", "MSI_CLP_2A", description,
                                            ingestion_init, ingestion_done);

    product_definition = harp_ingestion_register_product(module, "ECA_MSI_CLP_2A", NULL, read_dimensions);

    dimension_type[0] = harp_dimension_time;

    /* datetime */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double,
                                                                     1, dimension_type, NULL, "UTC time",
                                                                     "seconds since 2000-01-01", NULL, read_time);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/Geo/time", NULL);

    /* latitude */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_double,
                                                                     1, dimension_type, NULL, "latitude",
                                                                     "degree_north", NULL, read_latitude);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/Geo/latitude", NULL);

    /* longitude */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_double,
                                                                     1, dimension_type, NULL, "longitude",
                                                                     "degree_east", NULL, read_longitude);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/Geo/longitude", NULL);

    /* orbit_index */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "orbit_index", harp_type_int32,
                                                                     0, NULL, NULL, "absolute orbit number", NULL, NULL,
                                                                     read_orbit_index);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/HeaderData/VariableProductHeader/MainProductHeader/orbitNumber", NULL);

    /* validity */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "validity", harp_type_int8,
                                                                     1, dimension_type, NULL, "quality flag", NULL,
                                                                     NULL, read_quality_flag);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/Data/quality_flag", NULL);

    /* cloud_optical_depth */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "cloud_optical_depth",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     "cloud optical thickness", HARP_UNIT_DIMENSIONLESS,
                                                                     NULL, read_cloud_optical_thickness);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/Data/cloud_optical_thickness",
                                         NULL);

    /* cloud_top_height */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "cloud_top_height",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     "cloud top height", "m", NULL,
                                                                     read_cloud_top_height);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/Data/cloud_top_height", NULL);

    /* cloud_top_pressure */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "cloud_top_pressure",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     "cloud top pressure", "hPa", NULL,
                                                                     read_cloud_top_pressure);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/Data/cloud_top_pressure", NULL);

    /* cloud_top_temperature */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "cloud_top_temperature",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     "cloud top temperature", "K", NULL,
                                                                     read_cloud_top_temperature);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/Data/cloud_top_temperature",
                                         NULL);
}

int harp_ingestion_module_earthcare_jaxa_init(void)
{
    register_cpr_nom_1b_product();
    register_atl_cla_2a_product();
    register_cpr_clp_2a_product();
    register_cpr_eco_2a_product();
    register_msi_clp_2a_product();

    return 0;
}
