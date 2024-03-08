/*
 * Copyright (C) 2015-2023 S[&]T, The Netherlands.
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

#define MAX_PATH_LENGTH 256

const char *BBR_DATASET_NAME_BM__RAD_2B[4] = { "Standard", "Small", "Full", "Assessment" };

const char *BBR_DATASET_NAME_BMA_FLX_2B[4] = {
    "StandardResolution", "SmallResolution", "FullResolution", "AssessmentResolution"
};

typedef struct ingest_info_struct
{
    coda_product *product;
    long num_time;
    long num_vertical;
    long num_along_track;
    long num_across_track;
    long num_spectral;
    coda_cursor science_data_cursor;
    int am_source;      /* 0: atlid, 1: msi */
    int angstrom_variant;       /* 0: 355/670, 1: 670/865 */
    int aot_variant;    /* 0: 670, 1: 865 */
    int atlid_resolution;       /* 0: default, 1: medium, 2: low */
    int bbr_combined_flux;      /* 0: false, 1: true */
    int bbr_direction;  /* 0: nadir, 1: fore, 2: aft */
    int bbr_edge_coordinate;    /* 0: zero weight, 1: one weight */
    int bbr_irradiance; /* 0: solar, 1: thermal */
    int bbr_radiance;   /* 0: SW, 1: SW MSI, 2: SW filtered, 3: LW, 4: LW filtered */
    int bbr_resolution; /* 0: standard, 1: small, 2: full, 3: assessment */

    /* dynamic choice of BBR dataset names */
    const char **bbr_dataset_name;

    /* geolocation buffers */
    double *latitude_edge;
    double *longitude_edge;
} ingest_info;


static int read_dimensions(void *user_data, long dimension[HARP_NUM_DIM_TYPES])
{
    dimension[harp_dimension_time] = ((ingest_info *)user_data)->num_time;
    dimension[harp_dimension_vertical] = ((ingest_info *)user_data)->num_vertical;
    dimension[harp_dimension_spectral] = ((ingest_info *)user_data)->num_spectral;
    return 0;
}

static int read_array(coda_cursor cursor, const char *path, harp_data_type data_type, long num_elements,
                      harp_array data)
{
    long coda_num_elements;

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
            if (coda_cursor_read_int8_array(&cursor, data.int8_data, coda_array_ordering_c) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            break;
        case harp_type_int32:
            if (coda_cursor_read_int32_array(&cursor, data.int32_data, coda_array_ordering_c) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
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

static int read_array_bbr(ingest_info *info, const char *path, harp_data_type data_type, harp_array data)
{
    coda_cursor cursor;

    cursor = info->science_data_cursor;
    if (coda_cursor_goto_record_field_by_name(&cursor, info->bbr_dataset_name[info->bbr_resolution]) != 0)
    {
        return -1;
    }

    return read_array(cursor, path, data_type, info->num_time, data);
}

static int read_array_bbr_directional(ingest_info *info, const char *path, harp_data_type data_type, harp_array data)
{
    coda_cursor cursor;
    harp_array array;
    long i;

    cursor = info->science_data_cursor;
    if (coda_cursor_goto_record_field_by_name(&cursor, info->bbr_dataset_name[info->bbr_resolution]) != 0)
    {
        return -1;
    }

    array.ptr = malloc(info->num_time * 3 * sizeof(double));
    if (array.ptr == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       info->num_time * 3 * sizeof(double), __FILE__, __LINE__);
        return -1;
    }
    if (read_array(cursor, path, data_type, info->num_time * 3, array) != 0)
    {
        free(array.ptr);
        return -1;
    }

    switch (data_type)
    {
        case harp_type_int8:
            for (i = 0; i < info->num_time; i++)
            {
                data.int8_data[i] = array.int8_data[i * 3 + info->bbr_direction];
            }
            break;
        case harp_type_double:
            for (i = 0; i < info->num_time; i++)
            {
                data.double_data[i] = array.double_data[i * 3 + info->bbr_direction];
            }
            break;
        default:
            assert(0);
            exit(1);
    }

    free(array.ptr);

    return 0;
}

static int init_cursors_and_dimensions(ingest_info *info)
{
    coda_cursor cursor;
    long dim[CODA_MAX_NUM_DIMS];
    int num_dims;
    long index;
    int is_bbr = 0;

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
    info->science_data_cursor = cursor;

    if (coda_cursor_get_record_field_index_from_name(&cursor, info->bbr_dataset_name[info->bbr_resolution], &index) ==
        0)
    {
        if (coda_cursor_goto_record_field_by_name(&cursor, info->bbr_dataset_name[info->bbr_resolution]) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        is_bbr = 1;
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
    if (num_dims > 1 && !is_bbr)
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
    else if (coda_cursor_get_record_field_index_from_name(&cursor, "max_layers", &index) == 0)
    {
        if (coda_cursor_goto_record_field_by_name(&cursor, "max_layers") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_get_num_elements(&cursor, &info->num_vertical) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
    }

    /* num_spectral */
    if (coda_cursor_get_record_field_index_from_name(&cursor, "aerosol_optical_thickness_dimension", &index) == 0)
    {
        if (coda_cursor_goto_record_field_by_name(&cursor, "aerosol_optical_thickness_dimension") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_get_num_elements(&cursor, &info->num_spectral) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
    }

    return 0;
}

static int init_geolocation_edge_grid(ingest_info *info)
{
    harp_array longitude;
    harp_array latitude;

    /* read latitude information */
    latitude.ptr = malloc(info->num_time * sizeof(double));
    if (latitude.ptr == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       info->num_time * sizeof(double), __FILE__, __LINE__);
        return -1;
    }
    if (read_array(info->science_data_cursor, "latitude", harp_type_double, info->num_time, latitude) != 0)
    {
        free(latitude.ptr);
        return -1;
    }

    /* read longitude information */
    longitude.ptr = malloc(info->num_time * sizeof(double));
    if (longitude.ptr == NULL)
    {
        free(latitude.ptr);
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       info->num_time * sizeof(double), __FILE__, __LINE__);
        return -1;
    }
    if (read_array(info->science_data_cursor, "longitude", harp_type_double, info->num_time, longitude) != 0)
    {
        free(latitude.ptr);
        free(longitude.ptr);
        return -1;
    }

    /* calculate corner coordinates */
    info->longitude_edge = malloc((info->num_across_track + 1) * (info->num_along_track + 1) * sizeof(double));
    if (info->longitude_edge == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       (info->num_across_track + 1) * (info->num_along_track + 1) * sizeof(double), __FILE__, __LINE__);
        free(latitude.ptr);
        free(longitude.ptr);
        return -1;
    }
    info->latitude_edge = malloc((info->num_across_track + 1) * (info->num_along_track + 1) * sizeof(double));
    if (info->latitude_edge == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       (info->num_across_track + 1) * (info->num_along_track + 1) * sizeof(double), __FILE__, __LINE__);
        free(latitude.ptr);
        free(longitude.ptr);
        return -1;
    }

    harp_get_grid_corner_coordinates(info->num_along_track, info->num_across_track, longitude.double_data,
                                     latitude.double_data, info->longitude_edge, info->latitude_edge);

    free(latitude.ptr);
    free(longitude.ptr);

    return 0;
}

static int read_355nm(void *user_data, harp_array data)
{
    (void)user_data;

    *data.float_data = 355;

    return 0;
}

static int read_355_670_865nm(void *user_data, harp_array data)
{
    (void)user_data;

    data.float_data[0] = 355;
    data.float_data[1] = 670;
    data.float_data[2] = 865;

    return 0;
}

static int read_aerosol_angstrom_exponent(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    harp_array angstrom;
    long i;

    angstrom.ptr = malloc(info->num_time * 2 * sizeof(double));
    if (angstrom.ptr == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       info->num_time * 2 * sizeof(double), __FILE__, __LINE__);
        return -1;
    }
    if (read_array(info->science_data_cursor, "aerosol_angstrom_exponent", harp_type_float, info->num_time * 2,
                   angstrom) != 0)
    {
        free(angstrom.ptr);
        return -1;
    }

    for (i = 0; i < info->num_time; i++)
    {
        data.float_data[i] = angstrom.float_data[i * 2 + info->angstrom_variant];
    }

    free(angstrom.ptr);

    return 0;
}

static int read_aerosol_angstrom_exponent_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    harp_array angstrom;
    long i;

    angstrom.ptr = malloc(info->num_time * 2 * sizeof(double));
    if (angstrom.ptr == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       info->num_time * 2 * sizeof(double), __FILE__, __LINE__);
        return -1;
    }
    if (read_array(info->science_data_cursor, "aerosol_angstrom_exponent_error", harp_type_float, info->num_time * 2,
                   angstrom) != 0)
    {
        free(angstrom.ptr);
        return -1;
    }

    for (i = 0; i < info->num_time; i++)
    {
        data.float_data[i] = angstrom.float_data[i * 2 + info->angstrom_variant];
    }

    free(angstrom.ptr);

    return 0;
}

static int read_aerosol_classification(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "aerosol_classification", harp_type_int8,
                      info->num_time * info->num_vertical, data);
}

static int read_aerosol_extinction(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "aerosol_extinction", harp_type_float,
                      info->num_time * info->num_vertical, data);
}

static int read_aerosol_mass_content(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "aerosol_mass_content", harp_type_float,
                      info->num_time * info->num_vertical, data);
}

static int read_aerosol_layer_base_top(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    harp_array buffer;
    long dimension[2];

    if (read_array(info->science_data_cursor, "aerosol_layer_base", harp_type_float,
                   info->num_time * info->num_vertical, data) != 0)
    {
        return -1;
    }

    buffer.float_data = &data.float_data[info->num_time * info->num_vertical];
    if (read_array(info->science_data_cursor, "aerosol_layer_top", harp_type_float,
                   info->num_time * info->num_vertical, buffer) != 0)
    {
        return -1;
    }

    /* change {2,N} dimension ordering to {N,2} */
    dimension[0] = 2;
    dimension[1] = info->num_time * info->num_vertical;
    return harp_array_transpose(harp_type_float, 2, dimension, NULL, data);
}

static int read_aerosol_layer_optical_thickness_355nm(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "aerosol_layer_optical_thickness_355nm", harp_type_float,
                      info->num_time * info->num_vertical, data);
}

static int read_aerosol_layer_optical_thickness_355nm_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "aerosol_layer_optical_thickness_355nm_error", harp_type_float,
                      info->num_time * info->num_vertical, data);
}

static int read_aerosol_layer_mean_extinction_355nm(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "aerosol_layer_mean_extinction_355nm", harp_type_float,
                      info->num_time * info->num_vertical, data);
}

static int read_aerosol_layer_mean_extinction_355nm_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "aerosol_layer_mean_extinction_355nm_error", harp_type_float,
                      info->num_time * info->num_vertical, data);
}

static int read_aerosol_layer_mean_backscatter_355nm(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "aerosol_layer_mean_backscatter_355nm", harp_type_float,
                      info->num_time * info->num_vertical, data);
}

static int read_aerosol_layer_mean_backscatter_355nm_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "aerosol_layer_mean_backscatter_355nm_error",
                      harp_type_float, info->num_time * info->num_vertical, data);
}

static int read_aerosol_layer_mean_lidar_ratio_355nm(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "aerosol_layer_mean_lidar_ratio_355nm", harp_type_float,
                      info->num_time * info->num_vertical, data);
}

static int read_aerosol_layer_mean_lidar_ratio_355nm_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "aerosol_layer_mean_lidar_ratio_355nm_error", harp_type_float,
                      info->num_time * info->num_vertical, data);
}

static int read_aerosol_layer_mean_depolarisation_355nm(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "aerosol_layer_mean_depolarisation_355nm", harp_type_float,
                      info->num_time * info->num_vertical, data);
}

static int read_aerosol_layer_mean_depolarisation_355nm_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "aerosol_layer_mean_depolarisation_355nm_error", harp_type_float,
                      info->num_time * info->num_vertical, data);
}

static int read_aerosol_number_concentration(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "aerosol_number_concentration", harp_type_float,
                      info->num_time * info->num_vertical, data);
}

static int read_aerosol_optical_depth(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "aerosol_optical_depth", harp_type_float, info->num_time, data);
}

static int read_aerosol_optical_thickness_spectral(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "aerosol_optical_thickness_spectral", harp_type_float,
                      info->num_time * info->num_spectral, data);
}

static int read_aerosol_optical_thickness_spectral_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "aerosol_optical_thickness_spectral_error", harp_type_float,
                      info->num_time * info->num_spectral, data);
}

static int read_aerosol_optical_thickness_MSI(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->aot_variant == 1)
    {
        return read_array(info->science_data_cursor, "aerosol_optical_thickness_865nm", harp_type_float, info->num_time,
                          data);
    }

    return read_array(info->science_data_cursor, "aerosol_optical_thickness_670nm", harp_type_float, info->num_time,
                      data);
}

static int read_aerosol_optical_thickness_error_MSI(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->aot_variant == 1)
    {
        return read_array(info->science_data_cursor, "aerosol_optical_thickness_865nm_error", harp_type_float,
                          info->num_time, data);
    }

    return read_array(info->science_data_cursor, "aerosol_optical_thickness_670nm_error", harp_type_float,
                      info->num_time, data);
}

static int read_aerosol_dominant_type_ATLID(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "aerosol_dominant_type_ATLID", harp_type_int8, info->num_time, data);
}

static int read_angstrom_parameter_MSI(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->angstrom_variant == 1)
    {
        return read_array(info->science_data_cursor, "angstrom_parameter_670nm_865nm", harp_type_float, info->num_time,
                          data);
    }

    return read_array(info->science_data_cursor, "angstrom_parameter_355nm_670nm", harp_type_float, info->num_time,
                      data);
}

static int read_atlid_cloud_top_height(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "ATLID_cloud_top_height", harp_type_float, info->num_time, data);
}

static int read_atlid_cloud_top_height_confidence(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "ATLID_cloud_top_height_confidence", harp_type_float, info->num_time,
                      data);
}

static int read_classification(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "classification", harp_type_int8, info->num_time * info->num_vertical,
                      data);
}

static int read_cloud_effective_radius(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "cloud_effective_radius", harp_type_float, info->num_time, data);
}

static int read_cloud_effective_radius_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "cloud_effective_radius_error", harp_type_float, info->num_time, data);
}

static int read_cloud_fraction(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "cloud_fraction", harp_type_float, info->num_time, data);
}

static int read_cloud_mask(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "cloud_mask", harp_type_int8, info->num_time, data);
}

static int read_cloud_optical_thickness(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "cloud_optical_thickness", harp_type_float, info->num_time, data);
}

static int read_cloud_optical_thickness_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "cloud_optical_thickness_error", harp_type_float, info->num_time,
                      data);
}

static int read_cloud_phase(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long i;

    if (read_array(info->science_data_cursor, "cloud_phase", harp_type_int8, info->num_time, data) != 0)
    {
        return -1;
    }

    /* change values 1-4 to 0-3 */
    for (i = 0; i < info->num_time; i++)
    {
        if (data.int8_data[i] > 0)
        {
            data.int8_data[i]--;
        }
    }

    return 0;
}

static int read_cloud_phase_quality_status(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "cloud_phase_quality_status", harp_type_int8, info->num_time, data);
}

static int read_cloud_mask_quality_status(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "cloud_mask_quality_status", harp_type_int8, info->num_time, data);
}

static int read_cloud_top_height(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "cloud_top_height", harp_type_float, info->num_time, data);
}

static int read_cloud_top_height_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "cloud_top_height_error", harp_type_float, info->num_time, data);
}

static int read_cloud_top_height_AM(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (read_array(info->science_data_cursor, "cloud_top_height_MSI", harp_type_float, info->num_time, data) != 0)
    {
        return -1;
    }

    if (info->am_source == 0)
    {
        harp_array buffer;
        long i;

        buffer.ptr = malloc(info->num_time * sizeof(float));
        if (buffer.ptr == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           info->num_time * sizeof(float), __FILE__, __LINE__);
            return -1;
        }
        if (read_array(info->science_data_cursor, "cloud_top_height_difference_ATLID_MSI", harp_type_float,
                       info->num_time, buffer) != 0)
        {
            free(buffer.ptr);
            return -1;
        }

        for (i = 0; i < info->num_time; i++)
        {
            data.float_data[i] += buffer.float_data[i];
        }

        free(buffer.ptr);
    }

    return 0;
}

static int read_cloud_top_pressure(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "cloud_top_pressure", harp_type_float, info->num_time, data);
}

static int read_cloud_top_pressure_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "cloud_top_pressure_error", harp_type_float, info->num_time, data);
}

static int read_cloud_top_temperature(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "cloud_top_temperature", harp_type_float, info->num_time, data);
}

static int read_cloud_top_temperature_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "cloud_top_temperature_error", harp_type_float, info->num_time, data);
}

static int read_cloud_type(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "cloud_type", harp_type_int8, info->num_time, data);
}

static int read_cloud_type_quality_status(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "cloud_type_quality_status", harp_type_int8, info->num_time, data);
}

static int read_cloud_water_path(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "cloud_water_path", harp_type_float, info->num_time, data);
}

static int read_cloud_water_path_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "cloud_water_path_error", harp_type_float, info->num_time, data);
}

static int read_elevation(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "elevation", harp_type_float, info->num_time, data);
}

static int read_height(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "height", harp_type_float, info->num_time * info->num_vertical, data);
}

static int read_ice_effective_radius(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "ice_effective_radius", harp_type_float,
                      info->num_time * info->num_vertical, data);
}

static int read_ice_effective_radius_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "ice_effective_radius_error", harp_type_float,
                      info->num_time * info->num_vertical, data);
}

static int read_ice_mass_flux(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "ice_mass_flux", harp_type_float, info->num_time * info->num_vertical,
                      data);
}

static int read_ice_water_content(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "ice_water_content", harp_type_float,
                      info->num_time * info->num_vertical, data);
}

static int read_ice_water_content_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "ice_water_content_error", harp_type_float,
                      info->num_time * info->num_vertical, data);
}

static int read_ice_water_path(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "ice_water_path", harp_type_float, info->num_time, data);
}

static int read_ice_water_path_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "ice_water_path_error", harp_type_float, info->num_time, data);
}

static int read_irradiance_bbr(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->bbr_irradiance == 0)
    {
        /* solar */
        if (info->bbr_combined_flux)
        {
            return read_array_bbr(info, "solar_combined_top_of_atmosphere_flux", harp_type_double, data);
        }
        return read_array_bbr_directional(info, "solar_top_of_atmosphere_flux", harp_type_double, data);
    }
    else
    {
        /* thermal */
        if (info->bbr_combined_flux)
        {
            return read_array_bbr(info, "thermal_combined_top_of_atmosphere_flux", harp_type_double, data);
        }
        return read_array_bbr_directional(info, "thermal_top_of_atmosphere_flux", harp_type_double, data);
    }
}

static int read_irradiance_error_bbr(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->bbr_irradiance == 0)
    {
        /* solar */
        if (info->bbr_combined_flux)
        {
            return read_array_bbr(info, "solar_combined_top_of_atmosphere_flux_error", harp_type_double, data);
        }
        return read_array_bbr_directional(info, "solar_top_of_atmosphere_flux_error", harp_type_double, data);
    }
    else
    {
        /* thermal */
        if (info->bbr_combined_flux)
        {
            return read_array_bbr(info, "thermal_combined_top_of_atmosphere_flux_error", harp_type_double, data);
        }
        return read_array_bbr_directional(info, "thermal_top_of_atmosphere_flux_error", harp_type_double, data);
    }
}

static int read_irradiance_quality_status_bbr(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->bbr_irradiance == 0)
    {
        /* solar */
        if (info->bbr_combined_flux)
        {
            return read_array_bbr(info, "solar_combined_top_of_atmosphere_flux_quality_status", harp_type_int8, data);
        }
        return read_array_bbr_directional(info, "solar_top_of_atmosphere_flux_quality_status", harp_type_int8, data);
    }
    else
    {
        /* thermal */
        if (info->bbr_combined_flux)
        {
            return read_array_bbr(info, "thermal_combined_top_of_atmosphere_flux_quality_status", harp_type_int8, data);
        }
        return read_array_bbr_directional(info, "thermal_top_of_atmosphere_flux_quality_status", harp_type_int8, data);
    }
}

static int read_land_flag(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "land_flag", harp_type_int8, info->num_time, data);
}

static int read_latitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "latitude", harp_type_double, info->num_time, data);
}

static int read_latitude_bbr(void *user_data, harp_array data)
{
    return read_array_bbr((ingest_info *)user_data, "latitude", harp_type_double, data);
}

static int read_latitude_bbr_directional(void *user_data, harp_array data)
{
    return read_array_bbr_directional((ingest_info *)user_data, "latitude", harp_type_double, data);
}

static int read_latitude_bounds(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long num_xtrack = info->num_across_track;
    long i, j;

    if (info->latitude_edge == NULL)
    {
        if (init_geolocation_edge_grid(info) != 0)
        {
            return -1;
        }
    }

    i = index / num_xtrack;     /* 0 <= i < num_along_track */
    j = index - i * num_xtrack; /* 0 <= j < num_across_track */

    data.double_data[0] = info->latitude_edge[i * (num_xtrack + 1) + j];
    data.double_data[1] = info->latitude_edge[i * (num_xtrack + 1) + j + 1];
    data.double_data[2] = info->latitude_edge[(i + 1) * (num_xtrack + 1) + j + 1];
    data.double_data[3] = info->latitude_edge[(i + 1) * (num_xtrack + 1) + j];

    return 0;
}

static int read_latitude_bounds_bbr(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    coda_cursor cursor;
    const char *variable_name;
    long coda_num_elements;
    long dimension[2] = { 4, info->num_time };

    if (info->bbr_edge_coordinate == 0)
    {
        variable_name = info->bbr_edge_coordinate == 0 ? "zero_weight_edge_coordinate_nadir" :
            "one_weight_edge_coordinate_nadir";
    }
    else if (info->bbr_edge_coordinate == 1)
    {
        variable_name = info->bbr_edge_coordinate == 0 ? "zero_weight_edge_coordinate_fore" :
            "one_weight_edge_coordinate_fore";
    }
    else
    {
        variable_name = info->bbr_edge_coordinate == 0 ? "zero_weight_edge_coordinate_aft" :
            "one_weight_edge_coordinate_aft";
    }

    cursor = info->science_data_cursor;
    if (coda_cursor_goto_record_field_by_name(&cursor, info->bbr_dataset_name[info->bbr_resolution]) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&cursor, variable_name) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &coda_num_elements) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_num_elements != info->num_time * 8)
    {
        harp_set_error(HARP_ERROR_INGESTION, "variable has %ld elements; expected %ld", coda_num_elements,
                       info->num_time * 8);
        harp_add_coda_cursor_path_to_error_message(&cursor);
        return -1;
    }
    if (coda_cursor_read_double_partial_array(&cursor, 0, info->num_time * 4, data.double_data) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    return harp_array_transpose(harp_type_double, 2, dimension, NULL, data);
}

static int read_lidar_ratio_355nm(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    const char *name = "lidar_ratio_355nm";

    if (info->atlid_resolution == 1)
    {
        name = "lidar_ratio_355nm_medium_resolution";
    }
    else if (info->atlid_resolution == 2)
    {
        name = "lidar_ratio_355nm_low_resolution";
    }

    return read_array(info->science_data_cursor, name, harp_type_float, info->num_time * info->num_vertical, data);
}

static int read_lidar_ratio_355nm_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    const char *name = "lidar_ratio_355nm_error";

    if (info->atlid_resolution == 1)
    {
        name = "lidar_ratio_355nm_medium_resolution_error";
    }
    else if (info->atlid_resolution == 2)
    {
        name = "lidar_ratio_355nm_low_resolution_error";
    }

    return read_array(info->science_data_cursor, name, harp_type_float, info->num_time * info->num_vertical, data);
}

static int read_liquid_effective_radius(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "liquid_effective_radius", harp_type_float,
                      info->num_time * info->num_vertical, data);
}

static int read_liquid_effective_radius_relative_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    harp_array buffer;
    long i;

    if (read_array(info->science_data_cursor, "liquid_effective_radius_relative_error", harp_type_float,
                   info->num_time * info->num_vertical, data) != 0)
    {
        return -1;
    }

    buffer.ptr = malloc(info->num_time * info->num_vertical * sizeof(float));
    if (buffer.ptr == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       info->num_time * info->num_vertical * sizeof(float), __FILE__, __LINE__);
        return -1;

    }
    if (read_liquid_effective_radius(user_data, buffer) != 0)
    {
        free(buffer.ptr);
        return -1;
    }

    for (i = 0; i < info->num_time * info->num_vertical; i++)
    {
        data.float_data[i] *= buffer.float_data[i];
    }

    free(buffer.ptr);

    return 0;
}

static int read_liquid_extinction(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "liquid_extinction", harp_type_float,
                      info->num_time * info->num_vertical, data);
}

static int read_liquid_water_content(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "liquid_water_content", harp_type_float,
                      info->num_time * info->num_vertical, data);
}

static int read_liquid_water_content_relative_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    harp_array buffer;
    long i;

    if (read_array(info->science_data_cursor, "liquid_water_content_relative_error", harp_type_float,
                   info->num_time * info->num_vertical, data) != 0)
    {
        return -1;
    }

    buffer.ptr = malloc(info->num_time * info->num_vertical * sizeof(float));
    if (buffer.ptr == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       info->num_time * info->num_vertical * sizeof(float), __FILE__, __LINE__);
        return -1;
    }
    if (read_liquid_water_content(user_data, buffer) != 0)
    {
        free(buffer.ptr);
        return -1;
    }

    for (i = 0; i < info->num_time * info->num_vertical; i++)
    {
        data.float_data[i] *= buffer.float_data[i];
    }

    free(buffer.ptr);

    return 0;
}

static int read_liquid_water_path(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "liquid_water_path", harp_type_float, info->num_time, data);
}

static int read_liquid_water_path_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "liquid_water_path_error", harp_type_float, info->num_time,
                      data);
}

static int read_longitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "longitude", harp_type_double, info->num_time, data);
}

static int read_longitude_bbr(void *user_data, harp_array data)
{
    return read_array_bbr((ingest_info *)user_data, "longitude", harp_type_double, data);
}

static int read_longitude_bbr_directional(void *user_data, harp_array data)
{
    return read_array_bbr_directional((ingest_info *)user_data, "longitude", harp_type_double, data);
}

static int read_longitude_bounds(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long num_xtrack = info->num_across_track;
    long i, j;

    if (info->longitude_edge == NULL)
    {
        if (init_geolocation_edge_grid(info) != 0)
        {
            return -1;
        }
    }

    i = index / num_xtrack;     /* 0 <= i < num_along_track */
    j = index - i * num_xtrack; /* 0 <= j < num_across_track */

    data.double_data[0] = info->longitude_edge[i * (num_xtrack + 1) + j];
    data.double_data[1] = info->longitude_edge[i * (num_xtrack + 1) + j + 1];
    data.double_data[2] = info->longitude_edge[(i + 1) * (num_xtrack + 1) + j + 1];
    data.double_data[3] = info->longitude_edge[(i + 1) * (num_xtrack + 1) + j];

    /* wrap longitude to [-180,180] */
    for (i = 0; i < 4; i++)
    {
        if (data.double_data[i] > 180)
        {
            data.double_data[i] -= 360;
        }
        if (data.double_data[i] < -180)
        {
            data.double_data[i] += 360;
        }
    }

    return 0;
}

static int read_longitude_bounds_bbr(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    coda_cursor cursor;
    const char *variable_name;
    long coda_num_elements;
    long dimension[2] = { 4, info->num_time };

    if (info->bbr_edge_coordinate == 0)
    {
        variable_name = info->bbr_edge_coordinate == 0 ? "zero_weight_edge_coordinate_nadir" :
            "one_weight_edge_coordinate_nadir";
    }
    else if (info->bbr_edge_coordinate == 1)
    {
        variable_name = info->bbr_edge_coordinate == 0 ? "zero_weight_edge_coordinate_fore" :
            "one_weight_edge_coordinate_fore";
    }
    else
    {
        variable_name = info->bbr_edge_coordinate == 0 ? "zero_weight_edge_coordinate_aft" :
            "one_weight_edge_coordinate_aft";
    }

    cursor = info->science_data_cursor;
    if (coda_cursor_goto_record_field_by_name(&cursor, info->bbr_dataset_name[info->bbr_resolution]) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&cursor, variable_name) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &coda_num_elements) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_num_elements != info->num_time * 8)
    {
        harp_set_error(HARP_ERROR_INGESTION, "variable has %ld elements; expected %ld", coda_num_elements,
                       info->num_time * 8);
        harp_add_coda_cursor_path_to_error_message(&cursor);
        return -1;
    }
    if (coda_cursor_read_double_partial_array(&cursor, info->num_time * 4, info->num_time * 4, data.double_data) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    return harp_array_transpose(harp_type_double, 2, dimension, NULL, data);
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

static int read_particle_backscatter_coefficient_355nm(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    const char *name = "particle_backscatter_coefficient_355nm";

    if (info->atlid_resolution == 1)
    {
        name = "particle_backscatter_coefficient_355nm_medium_resolution";
    }
    else if (info->atlid_resolution == 2)
    {
        name = "particle_backscatter_coefficient_355nm_low_resolution";
    }

    return read_array(info->science_data_cursor, name, harp_type_float, info->num_time * info->num_vertical, data);
}

static int read_particle_backscatter_coefficient_355nm_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    const char *name = "particle_backscatter_coefficient_355nm_error";

    if (info->atlid_resolution == 1)
    {
        name = "particle_backscatter_coefficient_355nm_medium_resolution_error";
    }
    else if (info->atlid_resolution == 2)
    {
        name = "particle_backscatter_coefficient_355nm_low_resolution_error";
    }

    return read_array(info->science_data_cursor, name, harp_type_float, info->num_time * info->num_vertical, data);
}

static int read_particle_effective_area_radius(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "particle_effective_area_radius", harp_type_float,
                      info->num_time * info->num_vertical, data);
}

static int read_particle_effective_area_radius_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "particle_effective_area_radius_error", harp_type_float,
                      info->num_time * info->num_vertical, data);
}

static int read_particle_extinction_coefficient_355nm(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    const char *name = "particle_extinction_coefficient_355nm";

    if (info->atlid_resolution == 1)
    {
        name = "particle_extinction_coefficient_355nm_medium_resolution";
    }
    else if (info->atlid_resolution == 2)
    {
        name = "particle_extinction_coefficient_355nm_low_resolution";
    }

    return read_array(info->science_data_cursor, name, harp_type_float, info->num_time * info->num_vertical, data);
}

static int read_particle_extinction_coefficient_355nm_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    const char *name = "particle_extinction_coefficient_355nm_error";

    if (info->atlid_resolution == 1)
    {
        name = "particle_extinction_coefficient_355nm_medium_resolution_error";
    }
    else if (info->atlid_resolution == 2)
    {
        name = "particle_extinction_coefficient_355nm_low_resolution_error";
    }

    return read_array(info->science_data_cursor, name, harp_type_float, info->num_time * info->num_vertical, data);
}

static int read_particle_linear_depolarization_ratio_355nm(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    const char *name = "particle_linear_depolarization_ratio_355nm";

    if (info->atlid_resolution == 1)
    {
        name = "particle_linear_depolarization_ratio_355nm_medium_resolution";
    }
    else if (info->atlid_resolution == 2)
    {
        name = "particle_linear_depolarization_ratio_355nm_low_resolution";
    }

    return read_array(info->science_data_cursor, name, harp_type_float, info->num_time * info->num_vertical, data);
}

static int read_particle_linear_depolarization_ratio_355nm_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    const char *name = "particle_linear_depolarization_ratio_355nm_error";

    if (info->atlid_resolution == 1)
    {
        name = "particle_linear_depolarization_ratio_355nm_medium_resolution_error";
    }
    else if (info->atlid_resolution == 2)
    {
        name = "particle_linear_depolarization_ratio_355nm_low_resolution_error";
    }

    return read_array(info->science_data_cursor, name, harp_type_float, info->num_time * info->num_vertical, data);
}

static int read_particle_optical_depth_355nm(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    const char *name = "particle_optical_depth_355nm";

    if (info->atlid_resolution == 1)
    {
        name = "particle_optical_depth_355nm_medium_resolution";
    }
    else if (info->atlid_resolution == 2)
    {
        name = "particle_optical_depth_355nm_low_resolution";
    }

    return read_array(info->science_data_cursor, name, harp_type_float, info->num_time * info->num_vertical, data);
}

static int read_particle_optical_depth_355nm_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    const char *name = "particle_optical_depth_355nm_error";

    if (info->atlid_resolution == 1)
    {
        name = "particle_optical_depth_355nm_medium_resolution_error";
    }
    else if (info->atlid_resolution == 2)
    {
        name = "particle_optical_depth_355nm_low_resolution_error";
    }

    return read_array(info->science_data_cursor, name, harp_type_float, info->num_time * info->num_vertical, data);
}

static int read_quality_status(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "quality_status", harp_type_int8, info->num_time, data);
}

static int read_quality_status_2d(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "quality_status", harp_type_int8, info->num_time * info->num_vertical,
                      data);
}

static int read_quality_status_bbr(void *user_data, harp_array data)
{
    return read_array_bbr((ingest_info *)user_data, "quality_status", harp_type_int8, data);
}

static int read_quality_status_bbr_directional(void *user_data, harp_array data)
{
    return read_array_bbr_directional((ingest_info *)user_data, "quality_status", harp_type_int8, data);
}

static int read_radiance_bbr(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    const char *variable_name;

    switch (info->bbr_radiance)
    {
        case 0:
            variable_name = "solar_radiance";
            break;
        case 1:
            variable_name = "solar_radiance_MSI";
            break;
        case 2:
            variable_name = "shortwave_filtered_radiance";
            break;
        case 3:
            variable_name = "thermal_radiance";
            break;
        case 4:
            variable_name = "longwave_filtered_radiance";
            break;
        default:
            assert(0);
            exit(1);
    }

    return read_array_bbr_directional(info, variable_name, harp_type_double, data);
}

static int read_radiance_error_bbr(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    const char *variable_name;

    switch (info->bbr_radiance)
    {
        case 0:
            variable_name = "solar_radiance_error";
            break;
        case 1:
            variable_name = "solar_radiance_MSI_error";
            break;
        case 3:
            variable_name = "thermal_radiance_error";
            break;
        default:
            assert(0);
            exit(1);
    }

    return read_array_bbr_directional(info, variable_name, harp_type_double, data);
}

static int read_radiance_quality_status_bbr(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    const char *variable_name;

    switch (info->bbr_radiance)
    {
        case 0:
            variable_name = "solar_radiance_quality_status";
            break;
        case 1:
            variable_name = "solar_radiance_MSI_quality_status";
            break;
        case 3:
            variable_name = "thermal_radiance_quality_status";
            break;
        default:
            assert(0);
            exit(1);
    }

    return read_array_bbr_directional(info, variable_name, harp_type_int8, data);
}

static int read_rain_rate(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "rain_rate", harp_type_float, info->num_time * info->num_vertical,
                      data);
}

static int read_rain_water_content(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "rain_water_content", harp_type_float,
                      info->num_time * info->num_vertical, data);
}

static int read_rain_water_path(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "rain_water_path", harp_type_float, info->num_time, data);
}

static int read_rain_water_path_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "rain_water_path_error", harp_type_float, info->num_time, data);
}

static int read_retrieval_status(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "retrieval_status", harp_type_int8,
                      info->num_time * info->num_vertical, data);
}

static int read_solar_azimuth_angle_bbr(void *user_data, harp_array data)
{
    return read_array_bbr_directional((ingest_info *)user_data, "solar_azimuth_angle", harp_type_double, data);
}

static int read_solar_zenith_angle_bbr(void *user_data, harp_array data)
{
    return read_array_bbr_directional((ingest_info *)user_data, "solar_zenith_angle", harp_type_double, data);
}

static int read_simple_classification(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "simple_classification", harp_type_int8,
                      info->num_time * info->num_vertical, data);
}

static int read_simplified_uppermost_cloud_classification(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "simplified_uppermost_cloud_classification", harp_type_int8,
                      info->num_time, data);
}

static int read_surface_elevation(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "surface_elevation", harp_type_float, info->num_time, data);
}

static int read_surface_elevation_bbr(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array_bbr_directional(info, "surface_elevation", harp_type_double, data);
}

static int read_surface_reflectance_670(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "surface_reflectance_670nm", harp_type_float, info->num_time, data);
}

static int read_surface_reflectance_670_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "surface_reflectance_670nm_error", harp_type_float, info->num_time,
                      data);
}

static int read_synergetic_target_classification(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    const char *name = "synergetic_target_classification";

    if (info->atlid_resolution == 1)
    {
        name = "synergetic_target_classification_medium_resolution";
    }
    else if (info->atlid_resolution == 2)
    {
        name = "synergetic_target_classification_low_resolution";
    }

    return read_array(info->science_data_cursor, name, harp_type_int8, info->num_time * info->num_vertical, data);
}

static int read_time(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (read_array(info->science_data_cursor, "time", harp_type_double, info->num_along_track, data) != 0)
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

static int read_time_bbr(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array_bbr(info, "time", harp_type_double, data);
}

static int read_time_bbr_directional(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array_bbr_directional(info, "time", harp_type_double, data);
}

static int read_tropopause_height(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "tropopause_height", harp_type_float, info->num_time, data);
}

static int read_viewing_azimuth_angle_bbr(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array_bbr_directional(info, "viewing_azimuth_angle", harp_type_double, data);
}

static int read_viewing_zenith_angle_bbr(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array_bbr_directional(info, "viewing_zenith_angle", harp_type_double, data);
}

static int read_viewing_elevation_angle(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "viewing_elevation_angle", harp_type_float, info->num_time, data);
}

static int include_aot_670(void *user_data)
{
    return ((ingest_info *)user_data)->aot_variant == 0;
}

static int include_bbr_not_combined(void *user_data)
{
    return !((ingest_info *)user_data)->bbr_combined_flux;
}

static int include_bbr_unfiltered_radiance(void *user_data)
{
    return ((ingest_info *)user_data)->bbr_radiance != 2 && ((ingest_info *)user_data)->bbr_radiance != 4;
}

static void ingestion_done(void *user_data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info != NULL)
    {
        if (info->latitude_edge != NULL)
        {
            free(info->latitude_edge);
        }
        if (info->longitude_edge != NULL)
        {
            free(info->longitude_edge);
        }
        free(info);
    }
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
    info->num_time = 0;
    info->num_vertical = 0;
    info->num_along_track = 0;
    info->num_across_track = 0;
    info->num_spectral = 0;
    info->am_source = 1;
    info->angstrom_variant = 0;
    info->aot_variant = 0;
    info->atlid_resolution = 0;
    info->bbr_combined_flux = 1;
    info->bbr_direction = 0;
    info->bbr_edge_coordinate = 0;
    info->bbr_irradiance = 0;
    info->bbr_radiance = 0;
    info->bbr_resolution = 0;
    info->bbr_dataset_name = BBR_DATASET_NAME_BM__RAD_2B;
    info->latitude_edge = NULL;
    info->longitude_edge = NULL;
    *definition = module->product_definition[0];

    if (harp_ingestion_options_has_option(options, "angstrom"))
    {
        info->angstrom_variant = 1;
    }
    if (harp_ingestion_options_has_option(options, "aot"))
    {
        info->aot_variant = 1;
    }
    if (harp_ingestion_options_has_option(options, "direction"))
    {
        if (strcmp(option_value, "fore") == 0)
        {
            info->bbr_direction = 1;
        }
        else if (strcmp(option_value, "aft") == 0)
        {
            info->bbr_direction = 2;
        }
        if (strncmp((*definition)->name, "ECA_BMA_FLX_2B", 13) == 0)
        {
            /* just leave bbr_direction = 0 when value option is "nadir" for BMA_FLX_2B */
            /* but disable the ingestion of the combined flux if a direction option was provided */
            info->bbr_combined_flux = 0;
        }
    }
    if (harp_ingestion_options_has_option(options, "edge_coordinate"))
    {
        /* option_value == "aft" */
        info->bbr_resolution = 2;
    }
    if (harp_ingestion_options_has_option(options, "irradiance"))
    {
        info->bbr_irradiance = 1;
    }
    if (harp_ingestion_options_has_option(options, "radiance"))
    {
        if (strcmp(option_value, "SW_MSI") == 0)
        {
            info->bbr_radiance = 1;
        }
        else if (strcmp(option_value, "SW_filtered") == 0)
        {
            info->bbr_radiance = 2;
        }
        else if (strcmp(option_value, "LW") == 0)
        {
            info->bbr_radiance = 3;
        }
        else
        {
            /* option_value == "LW_filtered" */
            info->bbr_radiance = 4;
        }
    }
    if (harp_ingestion_options_has_option(options, "resolution"))
    {
        if (harp_ingestion_options_get_option(options, "resolution", &option_value) != 0)
        {
            ingestion_done(info);
            return -1;
        }
        if (strncmp((*definition)->name, "ECA_A", 5) == 0)
        {
            /* atlid */
            if (strcmp(option_value, "medium") == 0)
            {
                info->atlid_resolution = 1;
            }
            else
            {
                /* option_value == "low" */
                info->atlid_resolution = 2;
            }
        }
        else
        {
            /* bbr */
            assert(strncmp((*definition)->name, "ECA_B", 5) == 0);
            if (strcmp(option_value, "small") == 0)
            {
                info->bbr_resolution = 1;
            }
            if (strcmp(option_value, "full") == 0)
            {
                info->bbr_resolution = 2;
            }
            else
            {
                /* option_value == "assessment" */
                info->bbr_resolution = 3;
            }
        }
    }
    if (harp_ingestion_options_has_option(options, "source"))
    {
        /* currently only applicable for ECA_AM products */
        /* note that the ingestion option value is the inverted value of am_source */
        info->am_source = 0;
    }

    if (strncmp((*definition)->name, "ECA_BMA_FLX_2B", 13) == 0)
    {
        info->bbr_dataset_name = BBR_DATASET_NAME_BMA_FLX_2B;
    }

    if (init_cursors_and_dimensions(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }

    *user_data = info;

    return 0;
}

static void register_common_variables(harp_product_definition *product_definition, int is_2d)
{
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_independent };
    long dimension[2] = { -1, 4 };
    const char *mapping_description;
    const char *description;

    /* datetime */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double,
                                                                     1, dimension_type, NULL, "UTC time",
                                                                     "seconds since 2000-01-01", NULL, read_time);
    description = is_2d ? "time is replicated in the across track dimension" : NULL;
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/time", description);

    /* latitude */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_double,
                                                                     1, dimension_type, NULL, "Geodetic latitude",
                                                                     "degree_north", NULL, read_latitude);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/latitude", NULL);

    /* longitude */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_double,
                                                                     1, dimension_type, NULL, "Geodetic longitude",
                                                                     "degree_east", NULL, read_longitude);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/longitude", NULL);

    if (is_2d)
    {
        /* latitude_bounds */
        description = "latitudes of the ground pixel corners (WGS84)";
        variable_definition = harp_ingestion_register_variable_block_read(product_definition, "latitude_bounds",
                                                                          harp_type_double, 2, dimension_type,
                                                                          dimension, description, "degree_north", NULL,
                                                                          read_latitude_bounds);
        harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
        mapping_description = "interpolated from the center coordinates for each of the ground pixels";
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, NULL, mapping_description);

        /* longitude_bounds */
        description = "longitudes of the ground pixel corners (WGS84)";
        variable_definition = harp_ingestion_register_variable_block_read(product_definition, "longitude_bounds",
                                                                          harp_type_double, 2, dimension_type,
                                                                          dimension, description, "degree_east", NULL,
                                                                          read_longitude_bounds);
        harp_variable_definition_set_valid_range_double(variable_definition, -180.0, 180.0);
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, NULL, mapping_description);

    }

    /* orbit_index */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "orbit_index", harp_type_int32,
                                                                     0, NULL, NULL, "absolute orbit number", NULL, NULL,
                                                                     read_orbit_index);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/HeaderData/VariableProductHeader/MainProductHeader/orbitNumber", NULL);
}

static void register_ac__tc__2b_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2];
    const char *resolution_option_values[2] = { "medium", "low" };
    const char *description;

    description = "ATLID/CPR synergetic lidar/radar classification";
    module = harp_ingestion_register_module("ECA_AC__TC__2B", "EarthCARE", "EARTHCARE", "AC__TC__2B", description,
                                            ingestion_init, ingestion_done);

    description = "classification resolution: normal (default), medium (resolution=medium), or low (resolution=low)";
    harp_ingestion_register_option(module, "resolution", description, 2, resolution_option_values);

    product_definition = harp_ingestion_register_product(module, "ECA_AC__TC__2B", NULL, read_dimensions);

    register_common_variables(product_definition, 0);

    dimension_type[0] = harp_dimension_time;
    dimension_type[1] = harp_dimension_vertical;

    /* altitude */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "altitude", harp_type_float,
                                                                     2, dimension_type, NULL,
                                                                     "joint standard grid height", "m", NULL,
                                                                     read_height);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/height", NULL);

    /* surface_height */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "surface_height",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     "elevation ", "m", NULL, read_elevation);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/elevation", NULL);

    /* scene_type */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "scene_type",
                                                                     harp_type_int8, 2, dimension_type, NULL,
                                                                     "synergetic target classification", NULL, NULL,
                                                                     read_synergetic_target_classification);
    harp_variable_definition_add_mapping(variable_definition, "resolution unset", NULL,
                                         "/ScienceData/synergetic_target_classification", NULL);
    harp_variable_definition_add_mapping(variable_definition, "resolution=medium", NULL,
                                         "/ScienceData/synergetic_target_classification_medium_resolution", NULL);
    harp_variable_definition_add_mapping(variable_definition, "resolution=low", NULL,
                                         "/ScienceData/synergetic_target_classification_low_resolution", NULL);

}

static void register_acm_cap_2b_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2];
    const char *description;

    description = "ATLID/CPR/MSI cloud and aerosol properties";
    module = harp_ingestion_register_module("ECA_ACM_CAP_2B", "EarthCARE", "EARTHCARE", "ACM_CAP_2B", description,
                                            ingestion_init, ingestion_done);

    product_definition = harp_ingestion_register_product(module, "ECA_ACM_CAP_2B", NULL, read_dimensions);

    register_common_variables(product_definition, 0);

    dimension_type[0] = harp_dimension_time;
    dimension_type[1] = harp_dimension_vertical;

    /* altitude */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "altitude", harp_type_float,
                                                                     2, dimension_type, NULL,
                                                                     "joint standard grid height", "m", NULL,
                                                                     read_height);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/height", NULL);

    /* liquid_water_density */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "liquid_water_density",
                                                                     harp_type_float, 2, dimension_type, NULL,
                                                                     "liquid water content", "kg/m3", NULL,
                                                                     read_liquid_water_content);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/liquid_water_content", NULL);

    /* liquid_water_extinction_coefficient */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition,
                                                                     "liquid_water_extinction_coefficient",
                                                                     harp_type_float, 2, dimension_type, NULL,
                                                                     "liquid extinction", "1/m", NULL,
                                                                     read_liquid_extinction);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/liquid_extinction", NULL);

    /* liquid_particle_effective_radius */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition,
                                                                     "liquid_particle_effective_radius",
                                                                     harp_type_float, 2, dimension_type, NULL,
                                                                     "liquid effective radius", "m",
                                                                     NULL, read_liquid_effective_radius);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/liquid_effective_radius", NULL);

    /* ice_water_density */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "ice_water_density",
                                                                     harp_type_float, 2, dimension_type, NULL,
                                                                     "ice water content", "kg/m3", NULL,
                                                                     read_ice_water_content);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/ice_water_content", NULL);

    /* ice_particle_effective_radius */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition,
                                                                     "ice_particle_effective_radius",
                                                                     harp_type_float, 2, dimension_type, NULL,
                                                                     "ice effective radius", "m", NULL,
                                                                     read_ice_effective_radius);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/ice_effective_radius", NULL);

    /* ice_water_mass_flux */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "ice_water_mass_flux",
                                                                     harp_type_float, 2, dimension_type, NULL,
                                                                     "ice mass flux", "kg/m2/s", NULL,
                                                                     read_ice_mass_flux);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/ice_mass_flux", NULL);

    /* ice_water_column_density */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "ice_water_column_density",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     "ice water path", "kg/m2", NULL,
                                                                     read_ice_water_path);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/ice_water_path", NULL);

    /* rain_rate */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "rain_rate", harp_type_float,
                                                                     2, dimension_type, NULL, "rain rate", "mm/h", NULL,
                                                                     read_rain_rate);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/rain_rate", NULL);

    /* rain_water_density */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "rain_water_density",
                                                                     harp_type_float, 2, dimension_type, NULL,
                                                                     "rain water content", "kg/m3", NULL,
                                                                     read_rain_water_content);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/rain_water_content", NULL);

    /* aerosol_number_density */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "aerosol_number_density",
                                                                     harp_type_float, 2, dimension_type, NULL,
                                                                     "aerosol number concentration", "1/m3", NULL,
                                                                     read_aerosol_number_concentration);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/aerosol_number_concentration",
                                         NULL);

    /* aerosol_extinction_coefficient */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition,
                                                                     "aerosol_extinction_coefficient", harp_type_float,
                                                                     2, dimension_type, NULL, "aerosol extinction",
                                                                     "1/m", NULL, read_aerosol_extinction);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/aerosol_extinction", NULL);

    /* aerosol_optical_depth */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "aerosol_optical_depth",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     "aerosol optical depth", HARP_UNIT_DIMENSIONLESS,
                                                                     NULL, read_aerosol_optical_depth);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/aerosol_optical_depth", NULL);

    /* aerosol_density */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "aerosol_density",
                                                                     harp_type_float, 2, dimension_type, NULL,
                                                                     "aerosol mass content", "kg/m3", NULL,
                                                                     read_aerosol_mass_content);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/aerosol_mass_content", NULL);

    /* validity */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "validity", harp_type_int8, 1,
                                                                     dimension_type, NULL, "quality status", NULL, NULL,
                                                                     read_quality_status);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/quality_status", NULL);
}

static void register_am__acd_2b_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2];
    const char *angstrom_option_values[1] = { "670/865" };
    const char *description;

    description = "ATLID-MSI aerosol column descriptor";
    module = harp_ingestion_register_module("ECA_AM__ACD_2B", "EarthCARE", "EARTHCARE", "AM__ACD_2B", description,
                                            ingestion_init, ingestion_done);

    description = "wavelength combination for which the angstrom exponent is extracted: 355/670 (default), or 670/865 "
        "(angstrom=670/865)";
    harp_ingestion_register_option(module, "angstrom", description, 1, angstrom_option_values);

    product_definition = harp_ingestion_register_product(module, "ECA_AM__ACD_2B", NULL, read_dimensions);

    register_common_variables(product_definition, 1);

    dimension_type[0] = harp_dimension_time;
    dimension_type[1] = harp_dimension_spectral;

    /* aerosol_optical_depth */
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "aerosol_optical_depth", harp_type_float, 2,
                                                   dimension_type, NULL, "aerosol layer optical thickness 355nm",
                                                   HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_aerosol_optical_thickness_spectral);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/ScienceData/aerosol_optical_thickness_spectral", NULL);

    /* aerosol_optical_depth_uncertainty */
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "aerosol_optical_depth_uncertainty",
                                                   harp_type_float, 2, dimension_type, NULL,
                                                   "aerosol layer optical thickness error", HARP_UNIT_DIMENSIONLESS,
                                                   NULL, read_aerosol_optical_thickness_spectral_error);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/ScienceData/aerosol_optical_thickness_spectral_error", NULL);

    /* angstrom_exponent */
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "angstrom_exponent", harp_type_float, 1,
                                                   dimension_type, NULL, "angstrom exponent", HARP_UNIT_DIMENSIONLESS,
                                                   NULL, read_aerosol_angstrom_exponent);
    harp_variable_definition_add_mapping(variable_definition, NULL, "angstrom unset",
                                         "/ScienceData/aerosol_angstrom_exponent[*,*,0]", NULL);
    harp_variable_definition_add_mapping(variable_definition, NULL, "angstrom=670/865",
                                         "/ScienceData/aerosol_angstrom_exponent[*,*,1]", NULL);

    /* angstrom_exponent_uncertainty */
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "angstrom_exponent_uncertainty", harp_type_float,
                                                   1, dimension_type, NULL, "angstrom exponent error",
                                                   HARP_UNIT_DIMENSIONLESS, NULL, read_aerosol_angstrom_exponent_error);
    harp_variable_definition_add_mapping(variable_definition, NULL, "angstrom unset",
                                         "/ScienceData/aerosol_angstrom_exponent_error[*,*,0]", NULL);
    harp_variable_definition_add_mapping(variable_definition, NULL, "angstrom=670/865",
                                         "/ScienceData/aerosol_angstrom_exponent_error[*,*,1]", NULL);

    /* aerosol_type */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "aerosol_type", harp_type_int8,
                                                                     1, dimension_type, NULL, "aerosol type", NULL,
                                                                     NULL, read_aerosol_dominant_type_ATLID);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/aerosol_dominant_type_ATLID",
                                         NULL);

    /* validity */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "validity", harp_type_int8, 1,
                                                                     dimension_type, NULL, "quality status", NULL, NULL,
                                                                     read_quality_status);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/quality_status", NULL);

    /* wavelength */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "wavelength", harp_type_float,
                                                                     1, &dimension_type[1], NULL, "wavelength", "nm",
                                                                     NULL, read_355_670_865nm);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, NULL,
                                         "set to fixed values of 355nm, 670nm, and 865nm");

}

static void register_am__cth_2b_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1];
    const char *source_option_values[1] = { "atlid" };
    const char *description;
    const char *path;

    description = "ATLID-MSI cloud top height";
    module = harp_ingestion_register_module("ECA_AM__CTH_2B", "EarthCARE", "EARTHCARE", "AM__CTH_2B", description,
                                            ingestion_init, ingestion_done);

    description = "whether to ingest the cloud top height from MSI (default) or ATLID (data=atlid)";
    harp_ingestion_register_option(module, "source", description, 1, source_option_values);

    product_definition = harp_ingestion_register_product(module, "ECA_AM__CTH_2B", NULL, read_dimensions);

    register_common_variables(product_definition, 1);

    dimension_type[0] = harp_dimension_time;

    /* cloud_fraction */
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_fraction", harp_type_float, 1,
                                                   dimension_type, NULL, "cloud fraction", HARP_UNIT_DIMENSIONLESS,
                                                   NULL, read_cloud_fraction);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/cloud_fraction", NULL);

    /* cloud_top_height */
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_top_height", harp_type_float, 1,
                                                   dimension_type, NULL, "cloud top height", "m", NULL,
                                                   read_cloud_top_height_AM);
    path = "/ScienceData/cloud_top_height_MSI";
    harp_variable_definition_add_mapping(variable_definition, NULL, "source unset", path, NULL);
    path = "/ScienceData/cloud_top_height_MSI, /ScienceData/cloud_top_height_difference_ATLID_MSI";
    description = "cloud_top_height_MSI + cloud_top_height_difference_ATLID_MSI";
    harp_variable_definition_add_mapping(variable_definition, NULL, "source=atlid", path, description);

    /* validity */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "validity", harp_type_int8, 1,
                                                                     dimension_type, NULL, "quality status", NULL, NULL,
                                                                     read_quality_status);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/quality_status", NULL);
}

static void register_atl_aer_2a_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2];
    const char *description;

    description = "ATLID aerosol inversion";
    module = harp_ingestion_register_module("ECA_ATL_AER_2A", "EarthCARE", "EARTHCARE", "ATL_AER_2A", description,
                                            ingestion_init, ingestion_done);

    product_definition = harp_ingestion_register_product(module, "ECA_ATL_AER_2A", NULL, read_dimensions);

    register_common_variables(product_definition, 0);

    dimension_type[0] = harp_dimension_time;
    dimension_type[1] = harp_dimension_vertical;

    /* altitude */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "altitude", harp_type_float,
                                                                     2, dimension_type, NULL,
                                                                     "joint standard grid height", "m", NULL,
                                                                     read_height);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/height", NULL);

    /* surface_height */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "surface_height",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     "elevation ", "m", NULL, read_elevation);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/elevation", NULL);

    /* aerosol_extinction_coefficient */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition,
                                                                     "aerosol_extinction_coefficient",
                                                                     harp_type_float, 2, dimension_type, NULL,
                                                                     "particle extinction coefficient 355nm", "1/m",
                                                                     NULL, read_particle_extinction_coefficient_355nm);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/ScienceData/particle_extinction_coefficient_355nm", NULL);

    /* aerosol_extinction_coefficient_uncertainty */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition,
                                                                     "aerosol_extinction_coefficient_uncertainty",
                                                                     harp_type_float, 2, dimension_type, NULL,
                                                                     "particle extinction coefficient 355nm error",
                                                                     "1/m", NULL,
                                                                     read_particle_extinction_coefficient_355nm_error);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/ScienceData/particle_extinction_coefficient_355nm_error", NULL);

    /* aerosol_backscatter_coefficient */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition,
                                                                     "aerosol_backscatter_coefficient",
                                                                     harp_type_float, 2, dimension_type, NULL,
                                                                     "particle backscatter coefficient 355nm", "1/m/sr",
                                                                     NULL, read_particle_backscatter_coefficient_355nm);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/ScienceData/particle_backscatter_coefficient_355nm", NULL);

    /* aerosol_backscatter_coefficient_uncertainty */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition,
                                                                     "aerosol_backscatter_coefficient_uncertainty",
                                                                     harp_type_float, 2, dimension_type, NULL,
                                                                     "particle backscatter coefficient 355nm error",
                                                                     "1/m/sr", NULL,
                                                                     read_particle_backscatter_coefficient_355nm_error);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/ScienceData/particle_backscatter_coefficient_355nm_error", NULL);

    /* linear_depolarization_ratio */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "linear_depolarization_ratio",
                                                                     harp_type_float, 2, dimension_type, NULL,
                                                                     "particle linear depolarization ratio 355nm",
                                                                     HARP_UNIT_DIMENSIONLESS, NULL,
                                                                     read_particle_linear_depolarization_ratio_355nm);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/ScienceData/particle_linear_depolarization_ratio_355nm", NULL);

    /* linear_depolarization_ratio_uncertainty */
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "linear_depolarization_ratio_uncertainty",
                                                   harp_type_float, 2, dimension_type, NULL,
                                                   "particle linear depolarization ratio 355nm error",
                                                   HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_particle_linear_depolarization_ratio_355nm_error);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/ScienceData/particle_linear_depolarization_ratio_355nm_error", NULL);

    /* lidar_ratio */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "lidar_ratio", harp_type_float,
                                                                     2, dimension_type, NULL, "lidar ratio 355nm", "sr",
                                                                     NULL, read_lidar_ratio_355nm);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/lidar_ratio_355nm", NULL);

    /* lidar_ratio_uncertainty */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "lidar_ratio_uncertainty",
                                                                     harp_type_float, 2, dimension_type, NULL,
                                                                     "lidar ratio 355nm error", "sr",
                                                                     NULL, read_lidar_ratio_355nm_error);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/lidar_ratio_355nm_error", NULL);

    /* tropopause_height */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "tropopause_height",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     "tropopause height", "m", NULL,
                                                                     read_tropopause_height);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/tropopause_height", NULL);

    /* aerosol_type */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "aerosol_type", harp_type_int8,
                                                                     2, dimension_type, NULL, "aerosol classification",
                                                                     NULL, NULL, read_aerosol_classification);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/aerosol_classification", NULL);

    /* scene_type */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "scene_type", harp_type_int8,
                                                                     2, dimension_type, NULL, "classification", NULL,
                                                                     NULL, read_classification);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/classification", NULL);

    /* validity */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "validity", harp_type_int8, 2,
                                                                     dimension_type, NULL, "quality status", NULL, NULL,
                                                                     read_quality_status_2d);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/quality_status", NULL);

    /* wavelength */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "wavelength", harp_type_float,
                                                                     0, NULL, NULL, "lidar wavelength", "nm", NULL,
                                                                     read_355nm);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, NULL, "set to fixed value of 355nm");

}

static void register_atl_ald_2a_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[3];
    const char *description;
    long dimension[3];

    description = "ATLID aerosol layers in cloud-free observations";
    module = harp_ingestion_register_module("ECA_ATL_ALD_2A", "EarthCARE", "EARTHCARE", "ATL_ALD_2A", description,
                                            ingestion_init, ingestion_done);

    product_definition = harp_ingestion_register_product(module, "ECA_ATL_ALD_2A", NULL, read_dimensions);

    register_common_variables(product_definition, 0);

    dimension_type[0] = harp_dimension_time;
    dimension_type[1] = harp_dimension_vertical;
    dimension_type[2] = harp_dimension_independent;

    /* for altitude bounds */
    dimension[0] = -1;
    dimension[1] = -1;
    dimension[2] = 2;

    /* altitude_bounds */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "altitude_bounds",
                                                                     harp_type_float, 3, dimension_type, dimension,
                                                                     "aerorosl layer base and top", "m", NULL,
                                                                     read_aerosol_layer_base_top);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/ScienceData/aerosol_layer_base, /ScienceData/aerosol_layer_top", NULL);

    /* aerosol_optical_depth */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "aerosol_optical_depth",
                                                                     harp_type_float, 2, dimension_type, NULL,
                                                                     "aerosol layer optical thickness 355nm",
                                                                     HARP_UNIT_DIMENSIONLESS, NULL,
                                                                     read_aerosol_layer_optical_thickness_355nm);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/ScienceData/aerosol_layer_optical_thickness_355nm", NULL);


    /* aerosol_optical_depth_uncertainty */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition,
                                                                     "aerosol_optical_depth_uncertainty",
                                                                     harp_type_float, 2, dimension_type, NULL,
                                                                     "aerosol layer optical thickness 355nm error",
                                                                     HARP_UNIT_DIMENSIONLESS, NULL,
                                                                     read_aerosol_layer_optical_thickness_355nm_error);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/ScienceData/aerosol_layer_optical_thickness_355nm_error", NULL);



    /* aerosol_extinction_coefficient */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition,
                                                                     "aerosol_extinction_coefficient",
                                                                     harp_type_float, 2, dimension_type, NULL,
                                                                     "aerosol layer mean extinction 355nm",
                                                                     "1/m", NULL,
                                                                     read_aerosol_layer_mean_extinction_355nm);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/ScienceData/aerosol_layer_mean_extinction_355nm", NULL);

    /* aerosol_extinction_coefficient_uncertainty */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition,
                                                                     "aerosol_extinction_coefficient_uncertainty",
                                                                     harp_type_float, 2, dimension_type, NULL,
                                                                     "aerosol layer mean extinction 355nm error",
                                                                     "1/m", NULL,
                                                                     read_aerosol_layer_mean_extinction_355nm_error);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/ScienceData/aerosol_layer_mean_extinction_355nm_error", NULL);

    /* aerosol_backscatter_coefficient */
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "aerosol_backscatter_coefficient",
                                                   harp_type_float, 2, dimension_type, NULL,
                                                   "aerosol layer mean backscatter 355nm", "1/m/sr", NULL,
                                                   read_aerosol_layer_mean_backscatter_355nm);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/ScienceData/aerosol_layer_mean_backscatter_355nm", NULL);

    /* aerosol_backscatter_coefficient_uncertainty */
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "aerosol_backscatter_coefficient_uncertainty",
                                                   harp_type_float, 2, dimension_type, NULL,
                                                   "aerosol layer mean backscatter 355nm error", "1/m/sr", NULL,
                                                   read_aerosol_layer_mean_backscatter_355nm_error);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/ScienceData/aerosol_layer_mean_backscatter_355nm_error", NULL);

    /* lidar_ratio */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "lidar_ratio", harp_type_float,
                                                                     2, dimension_type, NULL,
                                                                     "aerosol layer mean lidar ratio 355nm", "sr",
                                                                     NULL, read_aerosol_layer_mean_lidar_ratio_355nm);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/ScienceData/aerosol_layer_mean_lidar_ratio_355nm", NULL);

    /* lidar_ratio_uncertainty */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "lidar_ratio_uncertainty",
                                                                     harp_type_float, 2, dimension_type, NULL,
                                                                     "aerosol layer mean lidar ratio 355nm error", "sr",
                                                                     NULL,
                                                                     read_aerosol_layer_mean_lidar_ratio_355nm_error);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/ScienceData/aerosol_layer_mean_lidar_ratio_355nm_error", NULL);

    /* linear_depolarization_ratio */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "linear_depolarization_ratio",
                                                                     harp_type_float, 2, dimension_type, NULL,
                                                                     "aerosol layer mean depolarization ratio 355nm",
                                                                     HARP_UNIT_DIMENSIONLESS, NULL,
                                                                     read_aerosol_layer_mean_depolarisation_355nm);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/ScienceData/aerosol_layer_mean_depolarisation_355nm", NULL);

    /* linear_depolarization_ratio_uncertainty */
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "linear_depolarization_ratio_uncertainty",
                                                   harp_type_float, 2, dimension_type, NULL,
                                                   "aerosol layer mean depolarization ratio 355nm error",
                                                   HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_aerosol_layer_mean_depolarisation_355nm_error);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/ScienceData/aerosol_layer_mean_depolarisation_355nm_error", NULL);

    /* validity */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "validity", harp_type_int8, 1,
                                                                     dimension_type, NULL, "quality status", NULL, NULL,
                                                                     read_quality_status);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/quality_status", NULL);

    /* wavelength */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "wavelength", harp_type_float,
                                                                     0, NULL, NULL, "lidar wavelength", "nm", NULL,
                                                                     read_355nm);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, NULL, "set to fixed value of 355nm");

}

static void register_atl_cth_2a_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1];
    const char *description;

    description = "ATLID uppermost cloud top height";
    module = harp_ingestion_register_module("ECA_ATL_CTH_2A", "EarthCARE", "EARTHCARE", "ATL_CTH_2A", description,
                                            ingestion_init, ingestion_done);

    product_definition = harp_ingestion_register_product(module, "ECA_ATL_CTH_2A", NULL, read_dimensions);

    register_common_variables(product_definition, 0);

    dimension_type[0] = harp_dimension_time;

    /* cloud_top_height */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "cloud_top_height",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     "ATLID cloud top height", "m", NULL,
                                                                     read_atlid_cloud_top_height);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/ATLID_cloud_top_height", NULL);

    /* cloud_top_height_uncertainty */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "cloud_top_height_uncertainty",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     "ATLID cloud top height confidence", "m", NULL,
                                                                     read_atlid_cloud_top_height_confidence);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/ScienceData/ATLID_cloud_top_height_confidence", NULL);

    /* cloud_type */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "cloud_type", harp_type_int8,
                                                                     1, dimension_type, NULL,
                                                                     "simplified uppermost cloud classification", NULL,
                                                                     NULL,
                                                                     read_simplified_uppermost_cloud_classification);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/ScienceData/simplified_uppermost_cloud_classification", NULL);

    /* validity */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "validity", harp_type_int8, 1,
                                                                     dimension_type, NULL, "quality status", NULL, NULL,
                                                                     read_quality_status);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/quality_status", NULL);
}

static void register_atl_ebd_2a_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2];
    const char *resolution_option_values[3] = { "medium", "low" };
    const char *description;

    description = "ATLID extinction, backscatter, and depolarization";
    module = harp_ingestion_register_module("ECA_ATL_EBD_2A", "EarthCARE", "EARTHCARE", "ATL_EBD_2A", description,
                                            ingestion_init, ingestion_done);

    description = "classification resolution: normal (default), medium (resolution=medium), or low (resolution=low)";
    harp_ingestion_register_option(module, "resolution", description, 2, resolution_option_values);

    product_definition = harp_ingestion_register_product(module, "ECA_ATL_EBD_2A", NULL, read_dimensions);

    register_common_variables(product_definition, 0);

    dimension_type[0] = harp_dimension_time;
    dimension_type[1] = harp_dimension_vertical;

    /* altitude */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "altitude", harp_type_float,
                                                                     2, dimension_type, NULL,
                                                                     "joint standard grid height", "m", NULL,
                                                                     read_height);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/height", NULL);

    /* surface_height */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "surface_height",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     "elevation ", "m", NULL, read_elevation);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/elevation", NULL);

    /* viewing_elevation_angle */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "viewing_elevation_angle",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     "viewing elevation angle", "degree", NULL,
                                                                     read_viewing_elevation_angle);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/viewing_elevation_angle", NULL);


    /* tropopause_height */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "tropopause_height",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     "tropopause height", "m", NULL,
                                                                     read_tropopause_height);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/tropopause_height", NULL);

    /* extinction_coefficient */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "extinction_coefficient",
                                                                     harp_type_float, 2, dimension_type, NULL,
                                                                     "particle extinction coefficient 355nm", "1/m",
                                                                     NULL, read_particle_extinction_coefficient_355nm);
    harp_variable_definition_add_mapping(variable_definition, "resolution unset", NULL,
                                         "/ScienceData/particle_extinction_coefficient_355nm", NULL);
    harp_variable_definition_add_mapping(variable_definition, "resolution=medium", NULL,
                                         "/ScienceData/particle_extinction_coefficient_355nm_medium_resolution", NULL);
    harp_variable_definition_add_mapping(variable_definition, "resolution=low", NULL,
                                         "/ScienceData/particle_extinction_coefficient_355nm_low_resolution", NULL);

    /* extinction_coefficient_uncertainty */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition,
                                                                     "extinction_coefficient_uncertainty",
                                                                     harp_type_float, 2, dimension_type, NULL,
                                                                     "particle extinction coefficient 355nm error",
                                                                     "1/m", NULL,
                                                                     read_particle_extinction_coefficient_355nm_error);
    harp_variable_definition_add_mapping(variable_definition, "resolution unset", NULL,
                                         "/ScienceData/particle_extinction_coefficient_355nm_error", NULL);
    harp_variable_definition_add_mapping(variable_definition, "resolution=medium", NULL,
                                         "/ScienceData/particle_extinction_coefficient_355nm_medium_resolution_error",
                                         NULL);
    harp_variable_definition_add_mapping(variable_definition, "resolution=low", NULL,
                                         "/ScienceData/particle_extinction_coefficient_355nm_low_resolution_error",
                                         NULL);

    /* backscatter_coefficient */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "backscatter_coefficient",
                                                                     harp_type_float, 2, dimension_type, NULL,
                                                                     "particle backscatter coefficient 355nm", "1/m/sr",
                                                                     NULL, read_particle_backscatter_coefficient_355nm);
    harp_variable_definition_add_mapping(variable_definition, "resolution unset", NULL,
                                         "/ScienceData/particle_backscatter_coefficient_355nm", NULL);
    harp_variable_definition_add_mapping(variable_definition, "resolution=medium", NULL,
                                         "/ScienceData/particle_backscatter_coefficient_355nm_medium_resolution", NULL);
    harp_variable_definition_add_mapping(variable_definition, "resolution=low", NULL,
                                         "/ScienceData/particle_backscatter_coefficient_355nm_low_resolution", NULL);

    /* backscatter_coefficient_uncertainty */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition,
                                                                     "backscatter_coefficient_uncertainty",
                                                                     harp_type_float, 2, dimension_type, NULL,
                                                                     "particle backscatter coefficient 355nm error",
                                                                     "1/m/sr", NULL,
                                                                     read_particle_backscatter_coefficient_355nm_error);
    harp_variable_definition_add_mapping(variable_definition, "resolution unset", NULL,
                                         "/ScienceData/particle_backscatter_coefficient_355nm_error", NULL);
    harp_variable_definition_add_mapping(variable_definition, "resolution=medium", NULL,
                                         "/ScienceData/particle_backscatter_coefficient_355nm_medium_resolution_error",
                                         NULL);
    harp_variable_definition_add_mapping(variable_definition, "resolution=low", NULL,
                                         "/ScienceData/particle_backscatter_coefficient_355nm_low_resolution_error",
                                         NULL);

    /* lidar_ratio */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "lidar_ratio", harp_type_float,
                                                                     2, dimension_type, NULL, "lidar ratio 355nm", "sr",
                                                                     NULL, read_lidar_ratio_355nm);
    harp_variable_definition_add_mapping(variable_definition, "resolution unset", NULL,
                                         "/ScienceData/lidar_ratio_355nm", NULL);
    harp_variable_definition_add_mapping(variable_definition, "resolution=medium", NULL,
                                         "/ScienceData/lidar_ratio_355nm_medium_resolution", NULL);
    harp_variable_definition_add_mapping(variable_definition, "resolution=low", NULL,
                                         "/ScienceData/lidar_ratio_355nm_low_resolution", NULL);

    /* lidar_ratio_uncertainty */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "lidar_ratio_uncertainty",
                                                                     harp_type_float, 2, dimension_type, NULL,
                                                                     "lidar ratio 355nm error", "sr",
                                                                     NULL, read_lidar_ratio_355nm_error);
    harp_variable_definition_add_mapping(variable_definition, "resolution unset", NULL,
                                         "/ScienceData/lidar_ratio_355nm_error", NULL);
    harp_variable_definition_add_mapping(variable_definition, "resolution=medium", NULL,
                                         "/ScienceData/lidar_ratio_355nm_medium_resolution_error", NULL);
    harp_variable_definition_add_mapping(variable_definition, "resolution=low", NULL,
                                         "/ScienceData/lidar_ratio_355nm_low_resolution_error", NULL);


    /* linear_depolarization_ratio */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "linear_depolarization_ratio",
                                                                     harp_type_float, 2, dimension_type, NULL,
                                                                     "particle linear depolarization ratio 355nm",
                                                                     HARP_UNIT_DIMENSIONLESS, NULL,
                                                                     read_particle_linear_depolarization_ratio_355nm);
    harp_variable_definition_add_mapping(variable_definition, "resolution unset", NULL,
                                         "/ScienceData/particle_linear_depolarization_ratio_355nm", NULL);
    harp_variable_definition_add_mapping(variable_definition, "resolution=medium", NULL,
                                         "/ScienceData/particle_linear_depolarization_ratio_355nm_medium_resolution",
                                         NULL);
    harp_variable_definition_add_mapping(variable_definition, "resolution=low", NULL,
                                         "/ScienceData/particle_linear_depolarization_ratio_355nm_low_resolution",
                                         NULL);

    /* linear_depolarization_ratio_uncertainty */
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "linear_depolarization_ratio_uncertainty",
                                                   harp_type_float, 2, dimension_type, NULL,
                                                   "particle linear depolarization ratio 355nm error",
                                                   HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_particle_linear_depolarization_ratio_355nm_error);
    harp_variable_definition_add_mapping(variable_definition, "resolution unset", NULL,
                                         "/ScienceData/particle_linear_depolarization_ratio_355nm_error", NULL);
    harp_variable_definition_add_mapping(variable_definition, "resolution=medium", NULL, "/ScienceData/particle_"
                                         "linear_depolarization_ratio_355nm_medium_resolution_error", NULL);
    harp_variable_definition_add_mapping(variable_definition, "resolution=low", NULL,
                                         "/ScienceData/particle_linear_depolarization_ratio_355nm_low_resolution_error",
                                         NULL);

    /* optical_depth */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "optical_depth",
                                                                     harp_type_float, 2, dimension_type, NULL,
                                                                     "partical optical depth", HARP_UNIT_DIMENSIONLESS,
                                                                     NULL, read_particle_optical_depth_355nm);
    harp_variable_definition_add_mapping(variable_definition, "resolution unset", NULL,
                                         "/ScienceData/particle_optical_depth_355nm", NULL);
    harp_variable_definition_add_mapping(variable_definition, "resolution=medium", NULL,
                                         "/ScienceData/particle_optical_depth_355nm_medium_resolution", NULL);
    harp_variable_definition_add_mapping(variable_definition, "resolution=low", NULL,
                                         "/ScienceData/particle_optical_depth_355nm_low_resolution", NULL);


    /* optical_depth_uncertainty */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "optical_depth_uncertainty",
                                                                     harp_type_float, 2, dimension_type, NULL,
                                                                     "partical optical depth error",
                                                                     HARP_UNIT_DIMENSIONLESS, NULL,
                                                                     read_particle_optical_depth_355nm_error);
    harp_variable_definition_add_mapping(variable_definition, "resolution unset", NULL,
                                         "/ScienceData/particle_optical_depth_355nm_error", NULL);
    harp_variable_definition_add_mapping(variable_definition, "resolution=medium", NULL,
                                         "/ScienceData/particle_optical_depth_355nm_medium_resolution_error", NULL);
    harp_variable_definition_add_mapping(variable_definition, "resolution=low", NULL,
                                         "/ScienceData/particle_optical_depth_355nm_low_resolution_error", NULL);

    /* particle_effective_radius */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "particle_effective_radius",
                                                                     harp_type_float, 2, dimension_type, NULL,
                                                                     "particle effective area radius", "m",
                                                                     NULL, read_particle_effective_area_radius);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/particle_effective_area_radius",
                                         NULL);

    /* particle_effective_radius_uncertainty */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition,
                                                                     "particle_effective_radius_uncertainty",
                                                                     harp_type_float, 2, dimension_type, NULL,
                                                                     "particle effective area radius error", "m",
                                                                     NULL, read_particle_effective_area_radius_error);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/ScienceData/particle_effective_area_radius_error", NULL);

    /* particle_type */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "particle_type",
                                                                     harp_type_int8, 2, dimension_type, NULL,
                                                                     "simple classification", NULL, NULL,
                                                                     read_simple_classification);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/simple_classification", NULL);

    /* validity */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "validity", harp_type_int8, 2,
                                                                     dimension_type, NULL, "quality status", NULL, NULL,
                                                                     read_quality_status_2d);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/quality_status", NULL);

    /* wavelength */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "wavelength", harp_type_float,
                                                                     0, NULL, NULL, "lidar wavelength", "nm", NULL,
                                                                     read_355nm);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, NULL, "set to fixed value of 355nm");

}

static void register_atl_ice_2a_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2];
    const char *description;

    description = "ATLID ice water content and effictive radius";
    module = harp_ingestion_register_module("ECA_ATL_ICE_2A", "EarthCARE", "EARTHCARE", "ATL_ICE_2A", description,
                                            ingestion_init, ingestion_done);

    product_definition = harp_ingestion_register_product(module, "ECA_ATL_ICE_2A", NULL, read_dimensions);

    register_common_variables(product_definition, 0);

    dimension_type[0] = harp_dimension_time;
    dimension_type[1] = harp_dimension_vertical;

    /* altitude */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "altitude", harp_type_float,
                                                                     2, dimension_type, NULL,
                                                                     "joint standard grid height", "m", NULL,
                                                                     read_height);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/height", NULL);

    /* surface_altitude */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "surface_altitude",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     "surface altitude ", "m", NULL, read_elevation);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/elevation", NULL);

    /* viewing_elevation_angle */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "viewing_elevation_angle",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     "viewing elevation angle", "degree", NULL,
                                                                     read_viewing_elevation_angle);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/viewing_elevation_angle", NULL);


    /* tropopause_height */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "tropopause_height",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     "tropopause height", "m", NULL,
                                                                     read_tropopause_height);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/tropopause_height", NULL);

    /* ice_water_density */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "ice_water_density",
                                                                     harp_type_float, 2, dimension_type, NULL,
                                                                     "ice water content", "kg/m3", NULL,
                                                                     read_ice_water_content);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/ice_water_content", NULL);

    /* ice_water_density_uncertainty */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition,
                                                                     "ice_water_density_uncertainty",
                                                                     harp_type_float, 2, dimension_type, NULL,
                                                                     "ice water content error", "kg/m3", NULL,
                                                                     read_ice_water_content_error);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/ice_water_content_error", NULL);

    /* ice_particle_effective_radius */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition,
                                                                     "ice_particle_effective_radius",
                                                                     harp_type_float, 2, dimension_type, NULL,
                                                                     "ice effective radius", "m",
                                                                     NULL, read_ice_effective_radius);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/ice_effective_radius", NULL);

    /* ice_particle_effective_radius_uncertainty */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition,
                                                                     "ice_particle_effective_radius_uncertainty",
                                                                     harp_type_float, 2, dimension_type, NULL,
                                                                     "ice effective radius error", "m",
                                                                     NULL, read_ice_effective_radius_error);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/ice_effective_radius_error",
                                         NULL);

    /* validity */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "validity", harp_type_int8, 2,
                                                                     dimension_type, NULL, "quality status", NULL, NULL,
                                                                     read_quality_status_2d);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/quality_status", NULL);
}

static void register_bm__rad_2b_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_independent };
    long dimension[2] = { -1, 4 };
    const char *direction_option_values[2] = { "fore", "aft" };
    const char *edge_coordinate_option_values[1] = { "one_weight" };
    const char *radiance_option_values[4] = { "SW_MSI", "SW_filtered", "LW", "LW_filtered" };
    const char *resolution_option_values[3] = { "small", "full", "assessment" };
    const char *resolution_description;
    const char *resdir_description;
    const char *description;
    const char *options;
    const char *path;

    description = "BBR TOA radiances";
    module = harp_ingestion_register_module("ECA_BM__RAD_2B", "EarthCARE", "EARTHCARE", "BM__RAD_2B", description,
                                            ingestion_init, ingestion_done);

    description = "viewing direction: nadir (default), fore (direction=fore), aft (direction=aft)";
    harp_ingestion_register_option(module, "direction", description, 2, direction_option_values);

    description = "edge coordinate: zero weight (default), one weight (edge_coordinate=one_weight)";
    harp_ingestion_register_option(module, "edge_coordinate", description, 1, edge_coordinate_option_values);

    description = "radiance: SW (default), SW from MSI (radiance=SW_MSI), SW filtered (radiance=SW_filtered), "
        "LW (radiance=LW), LW filtered (radiance=LW_filtered)";
    harp_ingestion_register_option(module, "radiance", description, 4, radiance_option_values);

    description = "resolution: standard (default), small (resolution=small), full (resolution=full), or assessment "
        "(resolution=assessment)";
    harp_ingestion_register_option(module, "resolution", description, 3, resolution_option_values);

    product_definition = harp_ingestion_register_product(module, "ECA_BM__RAD_2B", NULL, read_dimensions);

    /* predefined mapping descriptions */
    resolution_description = "<resolution> is Standard, Small, Full, or Assessment based on resolution option value";
    resdir_description = "<resolution> is Standard, Small, Full, or Assessment based on resolution option; "
        "<direction> is 0 (Fore), 1 (Nadir), or 2 (Aft) based on direction option";

    /* datetime */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double,
                                                                     1, dimension_type, NULL, "UTC time",
                                                                     "seconds since 2000-01-01", NULL,
                                                                     read_time_bbr_directional);
    path = "/ScienceData/<resolution>/time";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, resolution_description);

    /* latitude */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_double,
                                                                     1, dimension_type, NULL, "Geodetic latitude",
                                                                     "degree_north", NULL,
                                                                     read_latitude_bbr_directional);
    path = "/ScienceData/<resolution>/latitude";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, resolution_description);

    /* longitude */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_double,
                                                                     1, dimension_type, NULL, "Geodetic longitude",
                                                                     "degree_east", NULL,
                                                                     read_longitude_bbr_directional);
    path = "/ScienceData/<resolution>/longitude";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, resolution_description);

    /* latitude_bounds */
    description = "latitudes of the ground pixel corners (WGS84)";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "latitude_bounds",
                                                                     harp_type_double, 2, dimension_type,
                                                                     dimension, description, "degree_north", NULL,
                                                                     read_latitude_bounds_bbr);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    path = "/ScienceData/<resolution>/zero_weight_coordinate_nadir[0,*,*]";
    options = "direction unset, edge_coordinate unset";
    harp_variable_definition_add_mapping(variable_definition, NULL, options, path, resolution_description);
    path = "/ScienceData/<resolution>/one_weight_coordinate_nadir[0,*,*]";
    options = "direction unset, edge_coordinate=one_weigth";
    harp_variable_definition_add_mapping(variable_definition, NULL, options, path, resolution_description);
    path = "/ScienceData/<resolution>/zero_weight_coordinate_fore[0,*,*]";
    options = "direction=fore, edge_coordinate unset";
    harp_variable_definition_add_mapping(variable_definition, NULL, options, path, resolution_description);
    path = "/ScienceData/<resolution>/one_weight_coordinate_fore[0,*,*]";
    options = "direction=fore, edge_coordinate=one_weigth";
    harp_variable_definition_add_mapping(variable_definition, NULL, options, path, resolution_description);
    path = "/ScienceData/<resolution>/zero_weight_coordinate_aft[0,*,*]";
    options = "direction=aft, edge_coordinate unset";
    harp_variable_definition_add_mapping(variable_definition, NULL, options, path, resolution_description);
    path = "/ScienceData/<resolution>/one_weight_coordinate_aft[0,*,*]";
    options = "direction=aft, edge_coordinate=one_weigth";
    harp_variable_definition_add_mapping(variable_definition, NULL, options, path, resolution_description);

    /* longitude_bounds */
    description = "longitudes of the ground pixel corners (WGS84)";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "longitude_bounds",
                                                                     harp_type_double, 2, dimension_type,
                                                                     dimension, description, "degree_east", NULL,
                                                                     read_longitude_bounds_bbr);
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0, 180.0);
    path = "/ScienceData/<resolution>/zero_weight_coordinate_nadir[1,*,*]";
    options = "direction unset, edge_coordinate unset";
    harp_variable_definition_add_mapping(variable_definition, NULL, options, path, resolution_description);
    path = "/ScienceData/<resolution>/one_weight_coordinate_nadir[1,*,*]";
    options = "direction unset, edge_coordinate=one_weigth";
    harp_variable_definition_add_mapping(variable_definition, NULL, options, path, resolution_description);
    path = "/ScienceData/<resolution>/zero_weight_coordinate_fore[1,*,*]";
    options = "direction=fore, edge_coordinate unset";
    harp_variable_definition_add_mapping(variable_definition, NULL, options, path, resolution_description);
    path = "/ScienceData/<resolution>/one_weight_coordinate_fore[1,*,*]";
    options = "direction=fore, edge_coordinate=one_weigth";
    harp_variable_definition_add_mapping(variable_definition, NULL, options, path, resolution_description);
    path = "/ScienceData/<resolution>/zero_weight_coordinate_aft[1,*,*]";
    options = "direction=aft, edge_coordinate unset";
    harp_variable_definition_add_mapping(variable_definition, NULL, options, path, resolution_description);
    path = "/ScienceData/<resolution>/one_weight_coordinate_aft[1,*,*]";
    options = "direction=aft, edge_coordinate=one_weigth";
    harp_variable_definition_add_mapping(variable_definition, NULL, options, path, resolution_description);

    /* orbit_index */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "orbit_index", harp_type_int32,
                                                                     0, NULL, NULL, "absolute orbit number", NULL, NULL,
                                                                     read_orbit_index);
    path = "/HeaderData/VariableProductHeader/MainProductHeader/orbitNumber";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* surface_altitude */
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "surface_altitude", harp_type_double, 1,
                                                   dimension_type, NULL, "altitude of the surface", "m", NULL,
                                                   read_surface_elevation_bbr);
    path = "/ScienceData/<resolution>/surface_elevation[*,<direction>]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, resdir_description);

    /* solar_azimuth_angle */
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "solar_azimuth_angle", harp_type_double, 1,
                                                   dimension_type, NULL, "solar azimuth angle", "degree", NULL,
                                                   read_solar_azimuth_angle_bbr);
    path = "/ScienceData/<resolution>/solar_azimuth_angle[*,<direction>]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, resdir_description);

    /* solar_zenith_angle */
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "solar_zenith_angle", harp_type_double, 1,
                                                   dimension_type, NULL, "solar zenith angle", "degree", NULL,
                                                   read_solar_zenith_angle_bbr);
    path = "/ScienceData/<resolution>/solar_zenith_angle[*,<direction>]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, resdir_description);

    /* viewing_azimuth_angle */
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "viewing_azimuth_angle", harp_type_double, 1,
                                                   dimension_type, NULL, "viewing azimuth angle", "degree", NULL,
                                                   read_viewing_azimuth_angle_bbr);
    path = "/ScienceData/<resolution>/viewing_azimuth_angle[*,<direction>]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, resdir_description);

    /* viewing_zenith_angle */
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "viewing_zenith_angle", harp_type_double, 1,
                                                   dimension_type, NULL, "viewing zenith angle", "degree", NULL,
                                                   read_viewing_zenith_angle_bbr);
    path = "/ScienceData/<resolution>/viewing_zenith_angle[*,<direction>]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, resdir_description);

    /* radiance */
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "radiance", harp_type_double, 1,
                                                   dimension_type, NULL, "TOA radiance", "W/m2/sr", NULL,
                                                   read_radiance_bbr);
    path = "/ScienceData/<resolution>/solar_radiance[*,<direction>]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "radiance unset", path, resdir_description);
    path = "/ScienceData/<resolution>/solar_radiance_MSI[*,<direction>]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "radiance=SW_MSI", path, resdir_description);
    path = "/ScienceData/<resolution>/shortwave_filtered_radiance[*,<direction>]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "radiance=SW_filtered", path, resdir_description);
    path = "/ScienceData/<resolution>/thermal_radiance[*,<direction>]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "radiance=LW", path, resdir_description);
    path = "/ScienceData/<resolution>/longwave_filtered_radiance[*,<direction>]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "radiance=LW_filtered", path, resdir_description);

    /* radiance_uncertainty */
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "radiance_uncertainty", harp_type_double, 1,
                                                   dimension_type, NULL, "TOA radiance error", "W/m2/sr",
                                                   include_bbr_unfiltered_radiance, read_radiance_error_bbr);
    path = "/ScienceData/<resolution>/solar_radiance_error[*,<direction>]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "radiance unset", path, resdir_description);
    path = "/ScienceData/<resolution>/solar_radiance_MSI_error[*,<direction>]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "radiance=SW_MSI", path, resdir_description);
    path = "/ScienceData/<resolution>/thermal_radiance_error[*,<direction>]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "radiance=LW", path, resdir_description);

    /* radiance_validity */
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "radiance_validity", harp_type_int8, 1,
                                                   dimension_type, NULL, "radiance quality status", NULL,
                                                   include_bbr_unfiltered_radiance, read_radiance_quality_status_bbr);
    path = "/ScienceData/<resolution>/solar_radiance_quality_status[*,<direction>]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "radiance unset", path, resdir_description);
    path = "/ScienceData/<resolution>/solar_radiance_MSI_quality_status[*,<direction>]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "radiance=SW_MSI", path, resdir_description);
    path = "/ScienceData/<resolution>/thermal_radiance_quality_status[*,<direction>]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "radiance=LW", path, resdir_description);

    /* validity */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "validity", harp_type_int8, 1,
                                                                     dimension_type, NULL, "quality status", NULL, NULL,
                                                                     read_quality_status_bbr_directional);
    path = "/ScienceData/<resolution>/quality_status[*,<direction>]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, resdir_description);
}

static void register_bma_flx_2b_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_independent };
    const char *direction_option_values[3] = { "nadir", "fore", "aft" };
    const char *irradiance_option_values[1] = { "thermal" };
    const char *resolution_option_values[3] = { "small", "full", "assessment" };
    const char *resolution_description;
    const char *resdir_description;
    const char *description;
    const char *path;

    description = "BBR TOA solar and thermal fluxes";
    module = harp_ingestion_register_module("ECA_BMA_FLX_2B", "EarthCARE", "EARTHCARE", "BMA_FLX_2B", description,
                                            ingestion_init, ingestion_done);

    description = "viewing direction: combined (default), nadir (direction=nadir), fore (direction=fore), "
        "aft (direction=aft)";
    harp_ingestion_register_option(module, "direction", description, 3, direction_option_values);

    description = "irradiance: solar (default), thermal (irradiance=thermal)";
    harp_ingestion_register_option(module, "irradiance", description, 1, irradiance_option_values);

    description = "resolution: standard (default), small (resolution=small), full (resolution=full), or assessment "
        "(resolution=assessment)";
    harp_ingestion_register_option(module, "resolution", description, 3, resolution_option_values);

    product_definition = harp_ingestion_register_product(module, "ECA_BMA_FLX_2B", NULL, read_dimensions);

    /* predefined mapping descriptions */
    resolution_description = "<resolution> is StandardResolution, SmallResolution, FullResolution, or "
        "AssessmentResolution based on resolution option value";
    resdir_description = "<resolution> is StandardResolution, SmallResolution, FullResolution, or "
        "AssessmentResolution based on resolution option; "
        "<direction> is 0 (Fore), 1 (Nadir), or 2 (Aft) based on direction option";

    /* datetime */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double,
                                                                     1, dimension_type, NULL, "UTC time",
                                                                     "seconds since 2000-01-01", NULL, read_time_bbr);
    path = "/ScienceData/<resolution>/time";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, resolution_description);

    /* latitude */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_double,
                                                                     1, dimension_type, NULL, "Geodetic latitude",
                                                                     "degree_north", NULL, read_latitude_bbr);
    path = "/ScienceData/<resolution>/latitude";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, resolution_description);

    /* longitude */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_double,
                                                                     1, dimension_type, NULL, "Geodetic longitude",
                                                                     "degree_east", NULL, read_longitude_bbr);
    path = "/ScienceData/<resolution>/longitude";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, resolution_description);

    /* orbit_index */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "orbit_index", harp_type_int32,
                                                                     0, NULL, NULL, "absolute orbit number", NULL, NULL,
                                                                     read_orbit_index);
    path = "/HeaderData/VariableProductHeader/MainProductHeader/orbitNumber";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* solar_azimuth_angle */
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "solar_azimuth_angle", harp_type_double, 1,
                                                   dimension_type, NULL, "solar azimuth angle", "degree",
                                                   include_bbr_not_combined, read_solar_azimuth_angle_bbr);
    path = "/ScienceData/<resolution>/solar_azimuth_angle[*,<direction>]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "direction set", path, resdir_description);

    /* solar_zenith_angle */
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "solar_zenith_angle", harp_type_double, 1,
                                                   dimension_type, NULL, "solar zenith angle", "degree",
                                                   include_bbr_not_combined, read_solar_zenith_angle_bbr);
    path = "/ScienceData/<resolution>/solar_zenith_angle[*,<direction>]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "direction set", path, resdir_description);

    /* viewing_azimuth_angle */
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "viewing_azimuth_angle", harp_type_double, 1,
                                                   dimension_type, NULL, "viewing azimuth angle", "degree",
                                                   include_bbr_not_combined, read_viewing_azimuth_angle_bbr);
    path = "/ScienceData/<resolution>/viewing_azimuth_angle[*,<direction>]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "direction set", path, resdir_description);

    /* viewing_zenith_angle */
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "viewing_zenith_angle", harp_type_double, 1,
                                                   dimension_type, NULL, "viewing zenith angle", "degree",
                                                   include_bbr_not_combined, read_viewing_zenith_angle_bbr);
    path = "/ScienceData/<resolution>/viewing_zenith_angle[*,<direction>]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "direction set", path, resdir_description);

    /* irradiance */
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "irradiance", harp_type_double, 1,
                                                   dimension_type, NULL, "TOA flux", "W/m2", NULL, read_irradiance_bbr);
    path = "/ScienceData/<resolution>/solar_combined_top_of_atmosphere_flux[*]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "irradiance unset, direction unset", path,
                                         resolution_description);
    path = "/ScienceData/<resolution>/solar_top_of_atmosphere_flux[*,<direction>]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "irradiance unset, direction set", path,
                                         resdir_description);
    path = "/ScienceData/<resolution>/thermal_combined_top_of_atmosphere_flux[*]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "irradiance=thermal, direction unset", path,
                                         resolution_description);
    path = "/ScienceData/<resolution>/thermal_top_of_atmosphere_flux[*,<direction>]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "irradiance=thermal, direction set", path,
                                         resdir_description);

    /* irradiance_uncertainty */
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "irradiance_uncertainty", harp_type_double, 1,
                                                   dimension_type, NULL, "TOA flux error", "W/m2", NULL,
                                                   read_irradiance_error_bbr);
    path = "/ScienceData/<resolution>/solar_combined_top_of_atmosphere_flux_error[*]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "irradiance unset, direction unset", path,
                                         resolution_description);
    path = "/ScienceData/<resolution>/solar_top_of_atmosphere_flux_error[*,<direction>]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "irradiance unset, direction set", path,
                                         resdir_description);
    path = "/ScienceData/<resolution>/thermal_combined_top_of_atmosphere_flux_error[*]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "irradiance=thermal, direction unset", path,
                                         resolution_description);
    path = "/ScienceData/<resolution>/thermal_top_of_atmosphere_flux_error[*,<direction>]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "irradiance=thermal, direction set", path,
                                         resdir_description);

    /* irradiance_validity */
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "irradiance_validity", harp_type_int8, 1,
                                                   dimension_type, NULL, "TOA flux quality status", NULL, NULL,
                                                   read_irradiance_quality_status_bbr);
    path = "/ScienceData/<resolution>/solar_combined_top_of_atmosphere_flux_quality_status[*]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "irradiance unset, direction unset", path,
                                         resolution_description);
    path = "/ScienceData/<resolution>/solar_top_of_atmosphere_flux_quality_status[*,<direction>]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "irradiance unset, direction set", path,
                                         resdir_description);
    path = "/ScienceData/<resolution>/thermal_combined_top_of_atmosphere_flux_quality_status[*]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "irradiance=thermal, direction unset", path,
                                         resolution_description);
    path = "/ScienceData/<resolution>/thermal_top_of_atmosphere_flux_quality_status[*,<direction>]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "irradiance=thermal, direction set", path,
                                         resdir_description);

    /* validity */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "validity", harp_type_int8, 1,
                                                                     dimension_type, NULL, "quality status", NULL, NULL,
                                                                     read_quality_status_bbr);
    path = "/ScienceData/<resolution>/quality_status[*>]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, resolution_description);
}

static void register_cpr_cld_2a_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2];
    const char *description;

    description = "CPR cloud profiles";
    module = harp_ingestion_register_module("ECA_CPR_CLD_2A", "EarthCARE", "EARTHCARE", "CPR_CLD_2A", description,
                                            ingestion_init, ingestion_done);

    product_definition = harp_ingestion_register_product(module, "ECA_CPR_CLD_2A", NULL, read_dimensions);

    register_common_variables(product_definition, 0);

    dimension_type[0] = harp_dimension_time;
    dimension_type[1] = harp_dimension_vertical;

    /* altitude */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "altitude", harp_type_float,
                                                                     2, dimension_type, NULL,
                                                                     "joint standard grid height", "m", NULL,
                                                                     read_height);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/height", NULL);

    /* surface_altitude */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "surface_altitude",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     "surface altitude ", "m", NULL,
                                                                     read_surface_elevation);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/surface_elevation", NULL);

    /* surface_type */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "surface_type",
                                                                     harp_type_int8, 1, dimension_type, NULL,
                                                                     "land flag", NULL, NULL, read_land_flag);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/land_flag", NULL);


    /* ice_water_column_density */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "ice_water_column_density",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     "ice water path", "kg/m2", NULL,
                                                                     read_ice_water_path);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/ice_water_path", NULL);

    /* ice_water_column_density_uncertainty */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition,
                                                                     "ice_water_column_density_uncertainty",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     "ice water path error", "kg/m2", NULL,
                                                                     read_ice_water_path_error);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/ice_water_path_error", NULL);

    /* rain_water_column_density */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "rain_water_column_density",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     "rain water path", "kg/m2", NULL,
                                                                     read_rain_water_path);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/rain_water_path", NULL);

    /* rain_water_column_density_uncertainty */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition,
                                                                     "rain_water_column_density_uncertainty",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     "rain water path error", "kg/m2", NULL,
                                                                     read_rain_water_path_error);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/rain_water_path_error", NULL);

    /* liquid_water_density */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "liquid_water_density",
                                                                     harp_type_float, 2, dimension_type, NULL,
                                                                     "liquid water content", "kg/m3", NULL,
                                                                     read_liquid_water_content);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/liquid_water_content", NULL);

    /* liquid_water_density_uncertainty */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition,
                                                                     "liquid_water_density_uncertainty",
                                                                     harp_type_float, 2, dimension_type, NULL,
                                                                     "liquid water content error", "kg/m3", NULL,
                                                                     read_liquid_water_content_relative_error);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/liquid_water_content, "
                                         "/ScienceData/liquid_water_content_relative_error", NULL);

    /* liquid_particle_effective_radius */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition,
                                                                     "liquid_particle_effective_radius",
                                                                     harp_type_float, 2, dimension_type, NULL,
                                                                     "liquid effective radius", "m",
                                                                     NULL, read_liquid_effective_radius);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/liquid_effective_radius", NULL);

    /* liquid_particle_effective_radius_uncertainty */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition,
                                                                     "liquid_particle_effective_radius_uncertainty",
                                                                     harp_type_float, 2, dimension_type, NULL,
                                                                     "liquid effective radius error", "m",
                                                                     NULL, read_liquid_effective_radius_relative_error);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/liquid_effective_radius, "
                                         "/ScienceData/liquid_effective_radius_relative_error", NULL);

    /* liquid_water_column_density */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "liquid_water_column_density",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     "liquid cloud water path", "kg/m2", NULL,
                                                                     read_liquid_water_path);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/liquid_water_path", NULL);

    /* liquid_water_column_density_uncertainty */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition,
                                                                     "liquid_water_column_density_uncertainty",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     "liquid cloud water path error", "kg/m2", NULL,
                                                                     read_liquid_water_path_error);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/liquid_water_path_error",
                                         NULL);

    /* validity */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "validity", harp_type_int8, 2,
                                                                     dimension_type, NULL, "retrieval status", NULL,
                                                                     NULL, read_retrieval_status);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/retrieval_status", NULL);
}

static void register_msi_aot_2a_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1];
    const char *aot_option_values[1] = { "865" };
    const char *angstrom_option_values[1] = { "670/865" };
    const char *description;
    const char *path;

    description = "MSI aerosol optical thickness";
    module = harp_ingestion_register_module("ECA_MSI_AOT_2A", "EarthCARE", "EARTHCARE", "MSI_AOT_2A", description,
                                            ingestion_init, ingestion_done);

    description = "wavelength combination for which the angstrom exponent is extracted: 355/670 (default), or 670/865 "
        "(angstrom=670/865)";
    harp_ingestion_register_option(module, "angstrom", description, 1, angstrom_option_values);

    description = "wavelength for which to ingest the aerosol optical thickness: 670nm (default) or 865nm (aot=865)";
    harp_ingestion_register_option(module, "aot", description, 1, aot_option_values);

    product_definition = harp_ingestion_register_product(module, "ECA_MSI_AOT_2A", NULL, read_dimensions);

    register_common_variables(product_definition, 1);

    dimension_type[0] = harp_dimension_time;

    /* aorosol_optical_depth */
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "aorosol_optical_depth", harp_type_float, 1,
                                                   dimension_type, NULL, "aorosol optical thickness",
                                                   HARP_UNIT_DIMENSIONLESS, NULL, read_aerosol_optical_thickness_MSI);
    path = "/ScienceData/aerosol_optical_thickness_670nm";
    harp_variable_definition_add_mapping(variable_definition, NULL, "aot unset", path, NULL);
    path = "/ScienceData/aerosol_optical_thickness_865nm";
    harp_variable_definition_add_mapping(variable_definition, NULL, "aot=865", path, NULL);

    /* aorosol_optical_depth_uncertainty */
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "aorosol_optical_depth_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL,
                                                   "aorosol optical thickness error", HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_aerosol_optical_thickness_error_MSI);
    path = "/ScienceData/aerosol_optical_thickness_670nm_error";
    harp_variable_definition_add_mapping(variable_definition, NULL, "aot unset", path, NULL);
    path = "/ScienceData/aerosol_optical_thickness_865nm_error";
    harp_variable_definition_add_mapping(variable_definition, NULL, "aot=865", path, NULL);

    /* angstrom_exponent */
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "angstrom_exponent", harp_type_float, 1,
                                                   dimension_type, NULL, "angstrom parameter", HARP_UNIT_DIMENSIONLESS,
                                                   NULL, read_angstrom_parameter_MSI);
    path = "/ScienceData/angstrom_parameter_355nm_670nm";
    harp_variable_definition_add_mapping(variable_definition, NULL, "angstrom unset", path, NULL);
    path = "/ScienceData/angstrom_parameter_670nm_865nm";
    harp_variable_definition_add_mapping(variable_definition, NULL, "angstrom=670/865", path, NULL);

    /* surface_reflectance */
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "surface_reflectance", harp_type_float, 1,
                                                   dimension_type, NULL, "surface reflectance",
                                                   HARP_UNIT_DIMENSIONLESS, include_aot_670,
                                                   read_surface_reflectance_670);
    path = "/ScienceData/surface_reflectance_670nm";
    harp_variable_definition_add_mapping(variable_definition, NULL, "aot unset", path, NULL);

    /* surface_reflectance_uncertainty */
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "surface_reflectance_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL,
                                                   "surface reflectance error", HARP_UNIT_DIMENSIONLESS,
                                                   include_aot_670, read_surface_reflectance_670_error);
    path = "/ScienceData/surface_reflectance_670nm_error";
    harp_variable_definition_add_mapping(variable_definition, NULL, "aot unset", path, NULL);


    /* validity */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "validity", harp_type_int8, 1,
                                                                     dimension_type, NULL, "quality status", NULL,
                                                                     NULL, read_quality_status);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/quality_status", NULL);
}

static void register_msi_cm__2a_product(void)
{
    const char *cloud_type_values[] = {
        "clear", "cumulus", "altocumulus", "cirrus", "stratocumulus", "altostratus", "cirrostratus", "stratus",
        "nimbostratus", "deep_convection"
    };
    const char *cloud_phase_type_values[] = { "water", "ice", "supercooled", "overlap" };
    const char *cloud_mask_values[] = { "confident_clear", "probably_clear", "probably_cloudy", "confident_cloudy" };
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2];
    const char *description;

    description = "MSI cloud mask, type and phase";
    module = harp_ingestion_register_module("ECA_MSI_CM__2A", "EarthCARE", "EARTHCARE", "MSI_CM__2A", description,
                                            ingestion_init, ingestion_done);

    product_definition = harp_ingestion_register_product(module, "ECA_MSI_CM__2A", NULL, read_dimensions);

    register_common_variables(product_definition, 1);

    dimension_type[0] = harp_dimension_time;
    dimension_type[1] = harp_dimension_vertical;

    /* cloud_type */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "cloud_type",
                                                                     harp_type_int8, 1, dimension_type, NULL,
                                                                     "cloud type", NULL, NULL, read_cloud_type);
    harp_variable_definition_set_enumeration_values(variable_definition, 10, cloud_type_values);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/cloud_type", NULL);

    /* cloud_type_validity */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "cloud_type_validity",
                                                                     harp_type_int8, 1, dimension_type, NULL,
                                                                     "cloud type quality status", NULL, NULL,
                                                                     read_cloud_type_quality_status);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/cloud_type_quality_status",
                                         NULL);

    /* cloud_phase_type */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "cloud_phase_type",
                                                                     harp_type_int8, 1, dimension_type, NULL,
                                                                     "cloud phase", NULL, NULL, read_cloud_phase);
    harp_variable_definition_set_enumeration_values(variable_definition, 4, cloud_phase_type_values);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/cloud_phase", NULL);

    /* cloud_phase_validity */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "cloud_phase_type_validity",
                                                                     harp_type_int8, 1, dimension_type, NULL,
                                                                     "cloud phase quality status", NULL, NULL,
                                                                     read_cloud_phase_quality_status);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/cloud_phase_quality_status",
                                         NULL);

    /* scene_type */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "scene_type",
                                                                     harp_type_int8, 1, dimension_type, NULL,
                                                                     "cloud mask", NULL, NULL, read_cloud_mask);
    harp_variable_definition_set_enumeration_values(variable_definition, 4, cloud_mask_values);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/cloud_mask", NULL);

    /* scene_type_validity */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "scene_type_validity",
                                                                     harp_type_int8, 1, dimension_type, NULL,
                                                                     "cloud mask quality status", NULL, NULL,
                                                                     read_cloud_mask_quality_status);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/cloud_mask_quality_status",
                                         NULL);

    /* validity */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "validity", harp_type_int8, 1,
                                                                     dimension_type, NULL, "quality status", NULL,
                                                                     NULL, read_quality_status);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/quality_status", NULL);
}

static void register_msi_cop_2a_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1];
    const char *description;

    description = "MSI cloud optical thickness, cloud effective radius, ice crystal diameter, cloud water path, "
        "and cloud top temperature, pressure and height";
    module = harp_ingestion_register_module("ECA_MSI_COP_2A", "EarthCARE", "EARTHCARE", "MSI_COP_2A", description,
                                            ingestion_init, ingestion_done);

    product_definition = harp_ingestion_register_product(module, "ECA_MSI_COP_2A", NULL, read_dimensions);

    register_common_variables(product_definition, 1);

    dimension_type[0] = harp_dimension_time;

    /* cloud_particle_effective_radius */
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_particle_effective_radius",
                                                   harp_type_float, 1, dimension_type, NULL, "cloud effective radius",
                                                   "m", NULL, read_cloud_effective_radius);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/cloud_effective_radius", NULL);

    /* cloud_particle_effective_radius_uncertainty */
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_particle_effective_radius_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL,
                                                   "cloud effective radius error", "m", NULL,
                                                   read_cloud_effective_radius_error);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/cloud_effective_radius_error",
                                         NULL);

    /* cloud_optical_depth */
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_optical_depth", harp_type_float, 1,
                                                   dimension_type, NULL, "cloud optical thickness",
                                                   HARP_UNIT_DIMENSIONLESS, NULL, read_cloud_optical_thickness);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/cloud_optical_thickness", NULL);

    /* cloud_optical_depth_uncertainty */
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_optical_depth_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL,
                                                   "cloud optical thickness error", HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_cloud_optical_thickness_error);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/cloud_optical_thickness_error",
                                         NULL);

    /* cloud_top_height */
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_top_height", harp_type_float, 1,
                                                   dimension_type, NULL, "cloud top height", "m", NULL,
                                                   read_cloud_top_height);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/cloud_top_height", NULL);

    /* cloud_top_height_uncertainty */
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_top_height_uncertainty", harp_type_float,
                                                   1, dimension_type, NULL, "cloud top height error", "m", NULL,
                                                   read_cloud_top_height_error);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/cloud_top_height_error", NULL);

    /* cloud_top_pressure */
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_top_pressure", harp_type_float, 1,
                                                   dimension_type, NULL, "cloud top pressure", "Pa", NULL,
                                                   read_cloud_top_pressure);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/cloud_top_pressure", NULL);

    /* cloud_top_pressure_uncertainty */
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_top_pressure_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL,
                                                   "cloud top pressure error", "Pa", NULL,
                                                   read_cloud_top_pressure_error);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/cloud_top_pressure_error",
                                         NULL);

    /* cloud_top_temperature */
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_top_temperature", harp_type_float, 1,
                                                   dimension_type, NULL, "cloud top temperature", "K", NULL,
                                                   read_cloud_top_temperature);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/cloud_top_temperature", NULL);

    /* cloud_top_temperature_uncertainty */
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_top_temperature_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL,
                                                   "cloud top temperature error", "K", NULL,
                                                   read_cloud_top_temperature_error);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/cloud_top_temperature_error",
                                         NULL);

    /* liquid_water_column_density */
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "liquid_water_column_density", harp_type_float,
                                                   1, dimension_type, NULL, "cloud water path", "kg/m2", NULL,
                                                   read_cloud_water_path);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/cloud_water_path", NULL);

    /* liquid_water_column_density_uncertainty */
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "liquid_water_column_density_uncertainty",
                                                   harp_type_float, 1, dimension_type, NULL, "cloud water path error",
                                                   "kg/m2", NULL, read_cloud_water_path_error);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/cloud_water_path_error", NULL);

    /* validity */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "validity", harp_type_int8, 1,
                                                                     dimension_type, NULL, "quality status", NULL,
                                                                     NULL, read_quality_status);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/quality_status", NULL);
}

int harp_ingestion_module_earthcare_l2_init(void)
{
    register_ac__tc__2b_product();
    register_acm_cap_2b_product();
    register_am__acd_2b_product();
    register_am__cth_2b_product();
    register_atl_aer_2a_product();
    register_atl_ald_2a_product();
    register_atl_cth_2a_product();
    register_atl_ebd_2a_product();
    register_atl_ice_2a_product();
    register_bm__rad_2b_product();
    register_bma_flx_2b_product();
    register_cpr_cld_2a_product();
    register_msi_aot_2a_product();
    register_msi_cm__2a_product();
    register_msi_cop_2a_product();

    return 0;
}
