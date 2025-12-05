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

const char *BBR_DATASET_NAME[3] = { "standard", "small", "full" };

typedef struct ingest_info_struct
{
    coda_product *product;
    long num_time;
    long num_vertical;
    long num_along_track;
    long num_across_track;
    long num_spectral;
    coda_cursor science_data_cursor;
    int atl_backscatter;        /* 0: rayleigh data, 1: mie data, 2: crosspolar data */
    int bbr_direction;  /* 0: aft, 1: nadir, 2: fore */
    int bbr_edge_coordinate;    /* 0: zero weight, 1: one weight */
    int bbr_band;       /* 0: SW, 1: LW */
    int bbr_resolution; /* 0: standard, 1: small, 2: full */
    int msi_band;       /* 0: VIS, 1: VNIR, 2: SWIR1, 3: SWIR2, 4: TIR1, 5: TIR2, 6: TIR3 */

    /* dynamic choice of BBR dataset names */
    const char **bbr_dataset_name;
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

static int read_array_partial(coda_cursor cursor, const char *path, harp_data_type data_type, long array_size,
                              long offset, long num_elements, harp_array data)
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
    if (coda_num_elements != array_size)
    {
        harp_set_error(HARP_ERROR_INGESTION, "variable has %ld elements; expected %ld", coda_num_elements, array_size);
        harp_add_coda_cursor_path_to_error_message(&cursor);
        return -1;
    }

    switch (data_type)
    {
        case harp_type_int8:
            if (coda_cursor_read_int8_partial_array(&cursor, offset, num_elements, data.int8_data) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            break;
        case harp_type_int32:
            if (coda_cursor_read_int32_partial_array(&cursor, offset, num_elements, data.int32_data) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            break;
        case harp_type_float:
            if (coda_cursor_read_float_partial_array(&cursor, offset, num_elements, data.float_data) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            break;
        case harp_type_double:
            if (coda_cursor_read_double_partial_array(&cursor, offset, num_elements, data.double_data) != 0)
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

static int read_profile_array(ingest_info *info, coda_cursor cursor, const char *path, harp_data_type data_type,
                              harp_array data)
{
    long dimension[2];

    if (read_array(cursor, path, data_type, info->num_time * info->num_vertical, data) != 0)
    {
        return -1;
    }

    /* invert axis */
    dimension[0] = info->num_time;
    dimension[1] = info->num_vertical;
    return harp_array_invert(data_type, 1, 2, dimension, data);

}

static int read_array_bbr_dirbnd(ingest_info *info, const char *path, harp_data_type data_type, harp_array data)
{
    return read_array_partial(info->science_data_cursor, path, data_type, 3 * 2 * info->num_time,
                              (info->bbr_direction * 2 + info->bbr_band) * info->num_time, info->num_time, data);
}

static int read_array_bbr_res(ingest_info *info, const char *path, harp_data_type data_type, harp_array data)
{
    coda_cursor cursor;

    cursor = info->science_data_cursor;
    if (coda_cursor_goto_record_field_by_name(&cursor, info->bbr_dataset_name[info->bbr_resolution]) != 0)
    {
        return -1;
    }

    return read_array(cursor, path, data_type, info->num_time, data);
}

static int read_array_bbr_resdir(ingest_info *info, const char *path, harp_data_type data_type, harp_array data)
{
    coda_cursor cursor = info->science_data_cursor;

    if (coda_cursor_goto_record_field_by_name(&cursor, info->bbr_dataset_name[info->bbr_resolution]) != 0)
    {
        return -1;
    }

    return read_array_partial(cursor, path, data_type, 3 * info->num_time, info->bbr_direction * info->num_time,
                              info->num_time, data);
}

static int read_array_bbr_resdirbnd(ingest_info *info, const char *path, harp_data_type data_type, harp_array data)
{
    coda_cursor cursor = info->science_data_cursor;

    if (coda_cursor_goto_record_field_by_name(&cursor, info->bbr_dataset_name[info->bbr_resolution]) != 0)
    {
        return -1;
    }

    return read_array_partial(cursor, path, data_type, 3 * 2 * info->num_time,
                              (info->bbr_direction * 2 + info->bbr_band) * info->num_time, info->num_time, data);
}

static int read_array_msi(ingest_info *info, const char *path, harp_data_type data_type, harp_array data)
{
    return read_array_partial(info->science_data_cursor, path, data_type, 7 * info->num_time,
                              info->msi_band * info->num_time, info->num_time, data);
}

static int init_cursors_and_dimensions(ingest_info *info)
{
    coda_cursor cursor;
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
    info->science_data_cursor = cursor;

    if (coda_cursor_goto_record_field_by_name(&cursor, "along_track") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &info->num_along_track) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    coda_cursor_goto_parent(&cursor);
    info->num_time = info->num_along_track;
    if (coda_cursor_get_record_field_index_from_name(&cursor, "across_track", &index) == 0)
    {
        if (coda_cursor_goto_record_field_by_name(&cursor, "across_track") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_get_num_elements(&cursor, &info->num_across_track) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        coda_cursor_goto_parent(&cursor);
        info->num_time *= info->num_across_track;
    }

    if (coda_cursor_get_record_field_index_from_name(&cursor, info->bbr_dataset_name[info->bbr_resolution], &index) ==
        0)
    {
        if (coda_cursor_goto_record_field_by_name(&cursor, info->bbr_dataset_name[info->bbr_resolution]) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
    }

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
    }

    return 0;
}

static int read_atlid_backscatter(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->atl_backscatter == 0)
    {
        return read_profile_array(info, info->science_data_cursor, "rayleigh_attenuated_backscatter", harp_type_float,
                                  data);
    }
    if (info->atl_backscatter == 1)
    {
        return read_profile_array(info, info->science_data_cursor, "mie_attenuated_backscatter", harp_type_float, data);
    }
    return read_profile_array(info, info->science_data_cursor, "crosspolar_attenuated_backscatter", harp_type_float,
                              data);
}

static int read_invalid_flag_bbr_2d(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    coda_cursor cursor;
    harp_array array;
    long source_offset;
    long i, j;

    cursor = info->science_data_cursor;

    array.ptr = malloc(info->num_along_track * 3 * 2 * sizeof(int8_t));
    if (array.ptr == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       info->num_along_track * 3 * 2 * sizeof(int8_t), __FILE__, __LINE__);
        return -1;
    }
    if (read_array(cursor, "invalid_flag", harp_type_int8, info->num_along_track * 3 * 2, array) != 0)
    {
        free(array.ptr);
        return -1;
    }

    source_offset = (info->bbr_direction * 2 + info->bbr_band) * info->num_along_track;

    /* replicate time value for all across elements */
    for (i = info->num_along_track - 1; i >= 0; i--)
    {
        long target_offset = i * info->num_across_track;
        int8_t value = array.int8_data[source_offset + i];

        for (j = 0; j < info->num_across_track; j++)
        {
            data.int8_data[target_offset + j] = value;
        }
    }

    free(array.ptr);

    return 0;
}

static int read_latitude_bbr_2d(void *user_data, harp_array data)
{
    return read_array_bbr_dirbnd((ingest_info *)user_data, "latitude", harp_type_double, data);
}

static int read_latitude_bbr_barycentre(void *user_data, harp_array data)
{
    return read_array_bbr_res((ingest_info *)user_data, "barycentre_latitude", harp_type_double, data);
}

static int read_latitude_bounds_bbr(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    const char *variable_name;
    coda_cursor cursor;

    if (info->bbr_edge_coordinate == 0)
    {
        variable_name = "zero_weight_edge_latitude";
    }
    else
    {
        variable_name = "one_weight_edge_latitude";
    }

    cursor = info->science_data_cursor;
    if (coda_cursor_goto_record_field_by_name(&cursor, info->bbr_dataset_name[info->bbr_resolution]) != 0)
    {
        return -1;
    }

    return read_array(cursor, variable_name, harp_type_double, info->num_time * 4, data);
}

static int read_latitude_msi(void *user_data, harp_array data)
{
    return read_array_msi((ingest_info *)user_data, "latitude", harp_type_double, data);
}

static int read_longitude_bbr_2d(void *user_data, harp_array data)
{
    return read_array_bbr_dirbnd((ingest_info *)user_data, "longitude", harp_type_double, data);
}

static int read_longitude_bbr_barycentre(void *user_data, harp_array data)
{
    return read_array_bbr_res((ingest_info *)user_data, "barycentre_longitude", harp_type_double, data);
}

static int read_longitude_bounds_bbr(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    const char *variable_name;
    coda_cursor cursor;

    if (info->bbr_edge_coordinate == 0)
    {
        variable_name = "zero_weight_edge_longitude";
    }
    else
    {
        variable_name = "one_weight_edge_longitude";
    }

    cursor = info->science_data_cursor;
    if (coda_cursor_goto_record_field_by_name(&cursor, info->bbr_dataset_name[info->bbr_resolution]) != 0)
    {
        return -1;
    }

    return read_array(cursor, variable_name, harp_type_double, info->num_time * 4, data);
}

static int read_longitude_msi(void *user_data, harp_array data)
{
    return read_array_msi((ingest_info *)user_data, "longitude", harp_type_double, data);
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

static int read_pixel_quality_status_msi(void *user_data, harp_array data)
{
    return read_array_msi((ingest_info *)user_data, "pixel_quality_status", harp_type_int8, data);
}

static int read_pixel_values_msi(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array_msi(info, "pixel_values", harp_type_double, data);
}

static int read_radiance_bbr(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array_bbr_resdirbnd(info, "radiance", harp_type_double, data);
}

static int read_radiance_bbr_2d(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array_bbr_dirbnd(info, "radiance", harp_type_double, data);
}

static int read_radiance_error_bbr(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array_bbr_resdirbnd(info, "radiance_error", harp_type_double, data);
}

static int read_radiance_error_bbr_2d(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array_bbr_dirbnd(info, "radiance_error", harp_type_double, data);
}

static int read_sample_altitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_profile_array(info, info->science_data_cursor, "sample_altitude", harp_type_float, data);
}

static int read_sample_latitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_profile_array(info, info->science_data_cursor, "sample_latitude", harp_type_double, data);
}

static int read_sample_longitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_profile_array(info, info->science_data_cursor, "sample_longitude", harp_type_double, data);
}

static int read_sensor_azimuth_angle_bbr(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array_bbr_resdir(info, "sensor_azimuth_angle", harp_type_double, data);
}

static int read_sensor_azimuth_angle_bbr_2d(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array_bbr_dirbnd(info, "sensor_azimuth_angle", harp_type_double, data);
}

static int read_sensor_azimuth_angle_msi(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array_msi(info, "sensor_azimuth_angle", harp_type_double, data);
}

static int read_sensor_elevation_angle_bbr(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array_bbr_resdir(info, "sensor_elevation_angle", harp_type_double, data);
}

static int read_sensor_elevation_angle_bbr_2d(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array_bbr_dirbnd(info, "sensor_elevation_angle", harp_type_double, data);
}

static int read_sensor_elevation_angle_msi(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array_msi(info, "sensor_elevation_angle", harp_type_double, data);
}

static int read_solar_azimuth_angle_bbr(void *user_data, harp_array data)
{
    return read_array_bbr_resdir((ingest_info *)user_data, "solar_azimuth_angle", harp_type_double, data);
}

static int read_solar_azimuth_angle_bbr_2d(void *user_data, harp_array data)
{
    return read_array_bbr_dirbnd((ingest_info *)user_data, "solar_azimuth_angle", harp_type_double, data);
}

static int read_solar_azimuth_angle_msi(void *user_data, harp_array data)
{
    return read_array_msi((ingest_info *)user_data, "solar_azimuth_angle", harp_type_double, data);
}

static int read_solar_elevation_angle_bbr(void *user_data, harp_array data)
{
    return read_array_bbr_resdir((ingest_info *)user_data, "solar_elevation_angle", harp_type_double, data);
}

static int read_solar_elevation_angle_bbr_2d(void *user_data, harp_array data)
{
    return read_array_bbr_dirbnd((ingest_info *)user_data, "solar_elevation_angle", harp_type_double, data);
}

static int read_solar_elevation_angle_msi(void *user_data, harp_array data)
{
    return read_array_msi((ingest_info *)user_data, "solar_elevation_angle", harp_type_double, data);
}

static int read_solar_spectral_irradiance_msi(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long i, j;

    if (read_array_partial(info->science_data_cursor, "solar_spectral_irradiance", harp_type_double,
                           4 * info->num_across_track, info->msi_band * info->num_across_track, info->num_across_track,
                           data) != 0)
    {
        return -1;
    }

    for (i = 1; i < info->num_along_track; i++)
    {
        for (j = 0; j < info->num_across_track; j++)
        {
            data.double_data[i * info->num_across_track + j] = data.double_data[j];
        }
    }

    return 0;
}

static int read_time(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array(info->science_data_cursor, "time", harp_type_double, info->num_time, data);
}

static int read_time_bbr_2d(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long i, j;

    if (read_array_partial(info->science_data_cursor, "time", harp_type_double, 3 * 2 * info->num_along_track,
                           (info->bbr_direction * 2 + info->bbr_band) * info->num_along_track, info->num_along_track,
                           data) != 0)
    {
        return -1;
    }

    /* replicate time value for all across elements */
    for (i = info->num_along_track - 1; i >= 0; i--)
    {
        for (j = 0; j < info->num_across_track; j++)
        {
            data.double_data[i * info->num_across_track + j] = data.double_data[i];
        }
    }

    return 0;
}

static int read_time_bbr_barycentre(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_array_bbr_resdirbnd(info, "time_barycentre", harp_type_double, data);
}

static int read_time_msi(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long i, j;

    if (read_array(info->science_data_cursor, "time", harp_type_double, info->num_along_track, data) != 0)
    {
        return -1;
    }

    /* replicate time value for all across elements */
    for (i = info->num_along_track - 1; i >= 0; i--)
    {
        for (j = 0; j < info->num_across_track; j++)
        {
            data.double_data[i * info->num_across_track + j] = data.double_data[i];
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
    info->atl_backscatter = 0;
    info->bbr_direction = 1;
    info->bbr_edge_coordinate = 0;
    info->bbr_resolution = 0;
    info->bbr_dataset_name = &BBR_DATASET_NAME[0];
    info->msi_band = 0;
    *definition = module->product_definition[0];

    if (harp_ingestion_options_has_option(options, "data"))
    {
        if (harp_ingestion_options_get_option(options, "data", &option_value) != 0)
        {
            ingestion_done(info);
            return -1;
        }
        if (strcmp(option_value, "rayleigh") == 0)
        {
            info->atl_backscatter = 0;
        }
        else if (strcmp(option_value, "mie") == 0)
        {
            info->atl_backscatter = 1;
        }
        else
        {
            /* crosspolar */
            info->atl_backscatter = 2;
        }
    }
    if (harp_ingestion_options_has_option(options, "direction"))
    {
        if (harp_ingestion_options_get_option(options, "direction", &option_value) != 0)
        {
            ingestion_done(info);
            return -1;
        }
        if (strcmp(option_value, "fore") == 0)
        {
            info->bbr_direction = 2;
        }
        else if (strcmp(option_value, "aft") == 0)
        {
            info->bbr_direction = 0;
        }
    }
    if (harp_ingestion_options_has_option(options, "edge_coordinate"))
    {
        info->bbr_resolution = 1;
    }
    if (harp_ingestion_options_has_option(options, "band"))
    {
        if (strncmp((*definition)->name, "ECA_B", 5) == 0)
        {
            info->bbr_band = 1;
        }
        else
        {
            if (harp_ingestion_options_get_option(options, "band", &option_value) != 0)
            {
                ingestion_done(info);
                return -1;
            }
            if (strcmp(option_value, "VNIR") == 0)
            {
                info->msi_band = 1;
            }
            else if (strcmp(option_value, "SWIR1") == 0)
            {
                info->msi_band = 2;
            }
            else if (strcmp(option_value, "SWIR2") == 0)
            {
                info->msi_band = 3;
            }
            else if (strcmp(option_value, "TIR1") == 0)
            {
                info->msi_band = 4;
            }
            else if (strcmp(option_value, "TIR2") == 0)
            {
                info->msi_band = 5;
            }
            else
            {
                /* TIR3 */
                info->msi_band = 6;
            }
        }
    }
    if (harp_ingestion_options_has_option(options, "resolution"))
    {
        if (harp_ingestion_options_get_option(options, "resolution", &option_value) != 0)
        {
            ingestion_done(info);
            return -1;
        }
        if (strcmp(option_value, "small") == 0)
        {
            info->bbr_resolution = 1;
        }
        else
        {
            /* option_value == "full" */
            info->bbr_resolution = 2;
        }
    }

    if (init_cursors_and_dimensions(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }

    *user_data = info;

    return 0;
}

static int include_radiance_msi(void *user_data)
{
    return ((ingest_info *)user_data)->msi_band < 4;
}

static int include_brightness_temperature_msi(void *user_data)
{
    return ((ingest_info *)user_data)->msi_band >= 4;
}

static void register_atl_nom_1b_product(void)
{
    const char *dataset_options[] = { "rayleigh", "mie", "crosspolar" };
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2];
    const char *description;
    const char *options;
    const char *path;

    description = "ATLID L1 Nominal product (ESA)";
    module = harp_ingestion_register_module("ECA_ATL_NOM_1B", "EarthCARE", "EARTHCARE", "ATL_NOM_1B", description,
                                            ingestion_init, ingestion_done);
    harp_ingestion_register_option(module, "data", "the type of backscatter profile to ingest; option values are "
                                   "'rayleigh' (default), 'mie', 'crosspolar'", 3, dataset_options);

    product_definition = harp_ingestion_register_product(module, "ECA_ATL_NOM_1B", NULL, read_dimensions);

    dimension_type[0] = harp_dimension_time;
    dimension_type[1] = harp_dimension_vertical;

    /* datetime */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double,
                                                                     1, dimension_type, NULL, "UTC time",
                                                                     "seconds since 2000-01-01", NULL, read_time);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/time", NULL);

    /* latitude */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_double,
                                                                     2, dimension_type, NULL, "latitude",
                                                                     "degree_north", NULL, read_sample_latitude);
    description = "the vertical grid is inverted to make it ascending";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/sample_latitude", description);

    /* longitude */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_double,
                                                                     2, dimension_type, NULL, "longitude",
                                                                     "degree_east", NULL, read_sample_longitude);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/sample_longitude", description);

    /* altitude */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "altitude", harp_type_float,
                                                                     2, dimension_type, NULL, "altitude", "m", NULL,
                                                                     read_sample_altitude);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/sample_altitude", description);

    /* orbit_index */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "orbit_index", harp_type_int32,
                                                                     0, NULL, NULL, "absolute orbit number", NULL, NULL,
                                                                     read_orbit_index);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/HeaderData/VariableProductHeader/MainProductHeader/orbitNumber", NULL);

    /* backscatter_coefficient */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "backscatter_coefficient",
                                                                     harp_type_float, 2, dimension_type, NULL,
                                                                     "attenuated backscatter",
                                                                     "1/m/sr", NULL, read_atlid_backscatter);
    path = "/ScienceData/rayleigh_attenuated_backscatter";
    options = "data=rayleigh or data unset";
    harp_variable_definition_add_mapping(variable_definition, NULL, options, path, description);
    path = "/ScienceData/mie_attenuated_backscatter";
    options = "data=mie";
    harp_variable_definition_add_mapping(variable_definition, NULL, options, path, description);
    path = "/ScienceData/crosspolar_attenuated_backscatter";
    options = "data=crosspolar";
    harp_variable_definition_add_mapping(variable_definition, NULL, options, path, description);

    /* backscatter_coefficient_uncertainty */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition,
                                                                     "backscatter_coefficient_uncertainty",
                                                                     harp_type_float, 2, dimension_type, NULL,
                                                                     "attenuated backscatter",
                                                                     "1/m/sr", NULL, read_atlid_backscatter);
    path = "/ScienceData/rayleigh_attenuated_backscatter_total_error";
    options = "data=rayleigh or data unset";
    harp_variable_definition_add_mapping(variable_definition, NULL, options, path, description);
    path = "/ScienceData/mie_attenuated_backscatter_total_error";
    options = "data=mie";
    harp_variable_definition_add_mapping(variable_definition, NULL, options, path, description);
    path = "/ScienceData/crosspolar_attenuated_backscatter_total_error";
    options = "data=crosspolar";
    harp_variable_definition_add_mapping(variable_definition, NULL, options, path, description);
}

static void register_bbr_nom_1b_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_independent };
    long dimension[2] = { -1, 4 };
    const char *direction_option_values[2] = { "fore", "aft" };
    const char *edge_coordinate_option_values[1] = { "one_weight" };
    const char *band_option_values[1] = { "LW" };
    const char *resolution_option_values[2] = { "small", "full" };
    const char *resolution_description;
    const char *resdir_description;
    const char *resdirbnd_description;
    const char *description;
    const char *options;
    const char *path;

    description = "BBR L1 Nominal Product (ESA)";
    module = harp_ingestion_register_module("ECA_BBR_NOM_1B", "EarthCARE", "EARTHCARE", "BBR_NOM_1B", description,
                                            ingestion_init, ingestion_done);

    description = "viewing direction: nadir (default), fore (direction=fore), aft (direction=aft)";
    harp_ingestion_register_option(module, "direction", description, 2, direction_option_values);

    description = "edge coordinate: zero weight (default), one weight (edge_coordinate=one_weight)";
    harp_ingestion_register_option(module, "edge_coordinate", description, 1, edge_coordinate_option_values);

    description = "band: SW (default), LW (band=LW)";
    harp_ingestion_register_option(module, "band", description, 1, band_option_values);

    description = "resolution: standard (default), small (resolution=small), full (resolution=full)";
    harp_ingestion_register_option(module, "resolution", description, 2, resolution_option_values);

    product_definition = harp_ingestion_register_product(module, "ECA_BBR_NOM_1B", NULL, read_dimensions);

    /* predefined mapping descriptions */
    resolution_description = "<resolution> is Standard, Small, or Full based on resolution option value";
    resdir_description = "<resolution> is Standard, Small, or Full based on resolution option; "
        "<direction> is 0 (Aft), 1 (Nadir), or 2 (Fore) based on direction option";
    resdirbnd_description = "<resolution> is Standard, Small, or Full based on resolution option; "
        "<direction> is 0 (Aft), 1 (Nadir), or 2 (Fore) based on direction option; " "<band> is 0 (SW), or 1 (LW)";

    /* datetime */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double,
                                                                     1, dimension_type, NULL, "UTC time",
                                                                     "seconds since 2000-01-01", NULL,
                                                                     read_time_bbr_barycentre);
    path = "/ScienceData/<resolution>/time_barycentre[<direction>,<band>,*]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, resdirbnd_description);

    /* latitude */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_double,
                                                                     1, dimension_type, NULL, "Geodetic latitude",
                                                                     "degree_north", NULL,
                                                                     read_latitude_bbr_barycentre);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    path = "/ScienceData/<resolution>/barycentre_latitude";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, resolution_description);

    /* longitude */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_double,
                                                                     1, dimension_type, NULL, "Geodetic longitude",
                                                                     "degree_east", NULL,
                                                                     read_longitude_bbr_barycentre);
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0, 180.0);
    path = "/ScienceData/<resolution>/barycentre_longitude";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, resolution_description);

    /* latitude_bounds */
    description = "latitudes of the ground pixel corners (WGS84)";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "latitude_bounds",
                                                                     harp_type_double, 2, dimension_type,
                                                                     dimension, description, "degree_north", NULL,
                                                                     read_latitude_bounds_bbr);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    path = "/ScienceData/<resolution>/zero_weight_edge_latitude";
    options = "edge_coordinate unset";
    harp_variable_definition_add_mapping(variable_definition, NULL, options, path, resolution_description);
    path = "/ScienceData/<resolution>/one_weight_edge_latitude";
    options = "edge_coordinate=one_weigth";
    harp_variable_definition_add_mapping(variable_definition, NULL, options, path, resolution_description);

    /* longitude_bounds */
    description = "longitudes of the ground pixel corners (WGS84)";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "longitude_bounds",
                                                                     harp_type_double, 2, dimension_type,
                                                                     dimension, description, "degree_east", NULL,
                                                                     read_longitude_bounds_bbr);
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0, 180.0);
    path = "/ScienceData/<resolution>/zero_weight_edge_longitude";
    options = "direction unset, edge_coordinate unset";
    harp_variable_definition_add_mapping(variable_definition, NULL, options, path, resolution_description);
    path = "/ScienceData/<resolution>/one_weight_edge_longitude";
    options = "direction unset, edge_coordinate=one_weigth";
    harp_variable_definition_add_mapping(variable_definition, NULL, options, path, resolution_description);

    /* orbit_index */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "orbit_index", harp_type_int32,
                                                                     0, NULL, NULL, "absolute orbit number", NULL, NULL,
                                                                     read_orbit_index);
    path = "/HeaderData/VariableProductHeader/MainProductHeader/orbitNumber";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* solar_azimuth_angle */
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "solar_azimuth_angle", harp_type_double, 1,
                                                   dimension_type, NULL, "solar azimuth angle", "degree", NULL,
                                                   read_solar_azimuth_angle_bbr);
    path = "/ScienceData/<resolution>/solar_azimuth_angle[<direction>,*]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, resdir_description);

    /* solar_elevation_angle */
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "solar_elevation_angle", harp_type_double, 1,
                                                   dimension_type, NULL, "solar elevation angle", "degree", NULL,
                                                   read_solar_elevation_angle_bbr);
    path = "/ScienceData/<resolution>/solar_elevation_angle[<direction>,*]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, resdir_description);

    /* sensor_azimuth_angle */
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "sensor_azimuth_angle", harp_type_double, 1,
                                                   dimension_type, NULL, "sensor azimuth angle", "degree", NULL,
                                                   read_sensor_azimuth_angle_bbr);
    path = "/ScienceData/<resolution>/sensor_azimuth_angle[<direction>,*]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, resdir_description);

    /* sensor_elevation_angle */
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "sensor_elevation_angle", harp_type_double, 1,
                                                   dimension_type, NULL, "sensor elevation angle", "degree", NULL,
                                                   read_sensor_elevation_angle_bbr);
    path = "/ScienceData/<resolution>/sensor_elevation_angle[<direction>,*]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, resdir_description);

    /* radiance */
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "radiance", harp_type_double, 1,
                                                   dimension_type, NULL, "radiance", "W/m2/sr", NULL,
                                                   read_radiance_bbr);
    path = "/ScienceData/<resolution>/radiance[<direction>,<band>,*]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, resdirbnd_description);

    /* radiance_uncertainty */
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "radiance_uncertainty", harp_type_double, 1,
                                                   dimension_type, NULL, "radiance", "W/m2/sr", NULL,
                                                   read_radiance_error_bbr);
    path = "/ScienceData/<resolution>/radiance[<direction>,<band>,*]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, resdirbnd_description);
}

static void register_bbr_sng_1b_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };
    const char *direction_option_values[2] = { "fore", "aft" };
    const char *band_option_values[1] = { "LW" };
    const char *dirbnd_description;
    const char *description;
    const char *path;

    description = "BBR L1 Single Pixel Product (ESA)";
    module = harp_ingestion_register_module("ECA_BBR_SNG_1B", "EarthCARE", "EARTHCARE", "BBR_SNG_1B", description,
                                            ingestion_init, ingestion_done);

    description = "viewing direction: nadir (default), fore (direction=fore), aft (direction=aft)";
    harp_ingestion_register_option(module, "direction", description, 2, direction_option_values);

    description = "band: SW (default), LW (band=LW)";
    harp_ingestion_register_option(module, "band", description, 1, band_option_values);

    product_definition = harp_ingestion_register_product(module, "ECA_BBR_SNG_1B", NULL, read_dimensions);

    dirbnd_description = "<direction> is 0 (Aft), 1 (Nadir), or 2 (Fore) based on direction option; "
        "<band> is 0 (SW), or 1 (LW)";

    /* datetime */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double,
                                                                     1, dimension_type, NULL, "UTC time",
                                                                     "seconds since 2000-01-01", NULL,
                                                                     read_time_bbr_2d);
    path = "/ScienceData/time[<direction>,<band>,*]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, dirbnd_description);

    /* latitude */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_double,
                                                                     1, dimension_type, NULL, "Geodetic latitude",
                                                                     "degree_north", NULL, read_latitude_bbr_2d);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    path = "/ScienceData/latitude[<direction>,<band>,*,*]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, dirbnd_description);

    /* longitude */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_double,
                                                                     1, dimension_type, NULL, "Geodetic longitude",
                                                                     "degree_east", NULL, read_longitude_bbr_2d);
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0, 180.0);
    path = "/ScienceData/longitude[<direction>,<band>,*,*]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, dirbnd_description);

    /* orbit_index */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "orbit_index", harp_type_int32,
                                                                     0, NULL, NULL, "absolute orbit number", NULL, NULL,
                                                                     read_orbit_index);
    path = "/HeaderData/VariableProductHeader/MainProductHeader/orbitNumber";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* solar_azimuth_angle */
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "solar_azimuth_angle", harp_type_double, 1,
                                                   dimension_type, NULL, "solar azimuth angle", "degree", NULL,
                                                   read_solar_azimuth_angle_bbr_2d);
    path = "/ScienceData/solar_azimuth_angle[<direction>,<band>,*,*]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, dirbnd_description);

    /* solar_elevation_angle */
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "solar_elevation_angle", harp_type_double, 1,
                                                   dimension_type, NULL, "solar elevation angle", "degree", NULL,
                                                   read_solar_elevation_angle_bbr_2d);
    path = "/ScienceData/solar_elevation_angle[<direction>,<band>,*,*]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, dirbnd_description);

    /* sensor_azimuth_angle */
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "sensor_azimuth_angle", harp_type_double, 1,
                                                   dimension_type, NULL, "sensor azimuth angle", "degree", NULL,
                                                   read_sensor_azimuth_angle_bbr_2d);
    path = "/ScienceData/sensor_azimuth_angle[<direction>,<band>,*,*]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, dirbnd_description);

    /* sensor_elevation_angle */
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "sensor_elevation_angle", harp_type_double, 1,
                                                   dimension_type, NULL, "sensor elevation angle", "degree", NULL,
                                                   read_sensor_elevation_angle_bbr_2d);
    path = "/ScienceData/sensor_elevation_angle[<direction>,<band>,*,*]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, dirbnd_description);

    /* radiance */
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "radiance", harp_type_double, 1,
                                                   dimension_type, NULL, "radiance", "W/m2/sr", NULL,
                                                   read_radiance_bbr_2d);
    path = "/ScienceData/radiance[<direction>,<band>,*,*]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, dirbnd_description);

    /* radiance_uncertainty */
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "radiance_uncertainty", harp_type_double, 1,
                                                   dimension_type, NULL, "radiance", "W/m2/sr", NULL,
                                                   read_radiance_error_bbr_2d);
    path = "/ScienceData/radiance[<direction>,<band>,*,*]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, dirbnd_description);

    /* validity */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "validity", harp_type_int8, 1,
                                                                     dimension_type, NULL, "invalid data flag", NULL,
                                                                     NULL, read_invalid_flag_bbr_2d);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/ScienceData/invalid_flag[<direction>,<band>,*]", NULL);
}

static void register_msi_nom_1b_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_independent };
    const char *band_option_values[6] = { "VNIR", "SWIR1", "SWIR2", "TIR1", "TIR2", "TIR3" };
    const char *band_description;
    const char *description;
    const char *path;

    description = "MSI L1b Nominal Product (ESA)";
    module = harp_ingestion_register_module("ECA_MSI_NOM_1B", "EarthCARE", "EARTHCARE", "MSI_NOM_1B", description,
                                            ingestion_init, ingestion_done);

    description = "band: VIS (default), VNIR (band=VNIR), SWIR1 (band=SWIR1), SWIR2 (band=SWIR2), TIR1 (band=TIR1), "
        "TIR2 (band=TIR2), TIR3 (band=TIR3)";
    harp_ingestion_register_option(module, "band", description, 6, band_option_values);

    product_definition = harp_ingestion_register_product(module, "ECA_MSI_NOM_1B", NULL, read_dimensions);

    band_description = "<band> is 0 (VIS), 1 (VNIR), 2 (SWIR1), 3 (SWIR2), 4 (TIR1), 5 (TIR2), or 6 (TIR3)";

    /* datetime */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double,
                                                                     1, dimension_type, NULL, "UTC time",
                                                                     "seconds since 2000-01-01", NULL, read_time_msi);
    description = "time is replicated in the across track dimension";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/time", description);

    /* latitude */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_double,
                                                                     1, dimension_type, NULL, "Geodetic latitude",
                                                                     "degree_north", NULL, read_latitude_msi);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/latitude", NULL);

    /* longitude */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_double,
                                                                     1, dimension_type, NULL, "Geodetic longitude",
                                                                     "degree_east", NULL, read_longitude_msi);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/ScienceData/longitude", NULL);

    /* orbit_index */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "orbit_index", harp_type_int32,
                                                                     0, NULL, NULL, "absolute orbit number", NULL, NULL,
                                                                     read_orbit_index);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/HeaderData/VariableProductHeader/MainProductHeader/orbitNumber", NULL);

    /* solar_azimuth_angle */
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "solar_azimuth_angle", harp_type_double, 1,
                                                   dimension_type, NULL, "solar azimuth angle", "degree", NULL,
                                                   read_solar_azimuth_angle_msi);
    path = "/ScienceData/solar_azimuth_angle[<band>,*,*]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, band_description);

    /* solar_elevation_angle */
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "solar_elevation_angle", harp_type_double, 1,
                                                   dimension_type, NULL, "solar elevation angle", "degree", NULL,
                                                   read_solar_elevation_angle_msi);
    path = "/ScienceData/solar_elevation_angle[<band>,*,*]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, band_description);

    /* sensor_azimuth_angle */
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "sensor_azimuth_angle", harp_type_double, 1,
                                                   dimension_type, NULL, "sensor azimuth angle", "degree", NULL,
                                                   read_sensor_azimuth_angle_msi);
    path = "/ScienceData/sensor_azimuth_angle[<band>,*,*]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, band_description);

    /* sensor_elevation_angle */
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "sensor_elevation_angle", harp_type_double, 1,
                                                   dimension_type, NULL, "sensor elevation angle", "degree", NULL,
                                                   read_sensor_elevation_angle_msi);
    path = "/ScienceData/sensor_elevation_angle[<band>,*,*]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, band_description);

    /* wavelength_radiance */
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "wavelength_radiance", harp_type_double, 1,
                                                   dimension_type, NULL, "radiance", "W/m2/sr/um", include_radiance_msi,
                                                   read_pixel_values_msi);
    path = "/ScienceData/pixel_values[<band>,*,*]";
    description = "<band> is 0 (VIS), 1 (VNIR), 2 (SWIR1), or 3 (SWIR2)";
    harp_variable_definition_add_mapping(variable_definition, NULL, "band=VIS, band=VNIR, band=SWIR1, band=SWIR2", path,
                                         description);

    /* wavelength_irradiance */
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "wavelength_irradiance", harp_type_double, 1,
                                                   dimension_type, NULL, "solar spectral irradiance", "W/m2/um",
                                                   include_radiance_msi, read_solar_spectral_irradiance_msi);
    path = "/ScienceData/solar_spectral_irradiance[<band>,*]";
    description = "<band> is 0 (VIS), 1 (VNIR), 2 (SWIR1), or 3 (SWIR2); data is replicated for all scanlines";
    harp_variable_definition_add_mapping(variable_definition, NULL, "band=VIS, band=VNIR, band=SWIR1, band=SWIR2", path,
                                         description);

    /* brightness_temperature */
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "brightness_temperature", harp_type_double, 1,
                                                   dimension_type, NULL, "brightness temperature", "K",
                                                   include_brightness_temperature_msi, read_pixel_values_msi);
    path = "/ScienceData/pixel_values[<band>,*,*]";
    description = "<band> is 4 (TIR1), 5 (TIR2), or 6 (TIR3)";
    harp_variable_definition_add_mapping(variable_definition, NULL, "band=TIR1, band=TIR2, band=TIR3", path,
                                         description);

    /* validity */
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "validity", harp_type_int8, 1,
                                                                     dimension_type, NULL, "pixel quality status", NULL,
                                                                     NULL, read_pixel_quality_status_msi);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/ScienceData/pixel_quality_status[<band>,*,*]", NULL);
}


int harp_ingestion_module_earthcare_l1_init(void)
{
    register_atl_nom_1b_product();
    register_bbr_nom_1b_product();
    register_bbr_sng_1b_product();
    register_msi_nom_1b_product();

    return 0;
}
