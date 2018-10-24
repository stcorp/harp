/*
 * Copyright (C) 2015-2018 S[&]T, The Netherlands.
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

#define DAYS_FROM_1950_TO_2000 (18262)

typedef enum species_type_enum
{
    species_type_bro,
    species_type_h2o,
    species_type_hcho,
    species_type_no2,
    species_type_o3,
    species_type_oclo,
    species_type_so2
} species_type;

typedef struct ingest_info_struct
{
    coda_product *product;
    int product_version;
    int window_for_species[7];
    int detailed_results_type;
    harp_array amf_buffer;
    harp_array amf_error_buffer;
    harp_array index_in_scan_buffer;
    harp_array quality_flags_buffer;
    long num_main;
    long num_windows;
    long num_vertical;
    int revision;
} ingest_info;

static int init_num_main(ingest_info *info)
{
    coda_cursor cursor;
    long dim[CODA_MAX_NUM_DIMS];
    int num_dims;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, "/GEOLOCATION/LatitudeCentre") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_array_dim(&cursor, &num_dims, dim) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (num_dims != 1)
    {
        harp_set_error(HARP_ERROR_INGESTION, "dataset '/GEOLOCATION/LatitudeCentre' has %d dimensions, expected 1)",
                       num_dims);
        return -1;
    }

    info->num_main = dim[0];

    return 0;
}

static int init_num_vertical(ingest_info *info)
{
    char *path;
    coda_cursor cursor;
    long dim[CODA_MAX_NUM_DIMS];
    int num_dims;

    if (info->detailed_results_type == species_type_hcho && info->product_version >= 3)
    {
        path = "/DETAILED_RESULTS/HCHO/AveragingKernelPressureLevel";
    }
    else if (info->detailed_results_type == species_type_no2 && info->product_version >= 3)
    {
        path = "/DETAILED_RESULTS/NO2/AveragingKernelPressureLevel";
    }
    else
    {
        info->num_vertical = 0;
        return 0;
    }

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
    if (coda_cursor_get_array_dim(&cursor, &num_dims, dim) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (num_dims != 2)
    {
        harp_set_error(HARP_ERROR_INGESTION, "dataset '%s' has %d dimensions, expected 2)", path, num_dims);
        return -1;
    }
    if (dim[0] != info->num_main)
    {
        harp_set_error(HARP_ERROR_INGESTION, "dataset '%s' has %ld elements for the first dimension, expected %ld)",
                       path, dim[0], info->num_main);
        return -1;
    }

    info->num_vertical = dim[1];

    return 0;
}

static int init_window_info(ingest_info *info)
{
    const char *species_name[] = { "BrO", "H2O", "HCHO", "NO2", "O3", "OClO", "SO2" };
    char product_contents[100];
    coda_cursor cursor;
    long dim[CODA_MAX_NUM_DIMS];
    int num_dims;
    int i;

    for (i = 0; i < 7; i++)
    {
        info->window_for_species[i] = -1;
    }

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, "/META_DATA/MainSpecies") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_array_dim(&cursor, &num_dims, dim) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (num_dims != 1)
    {
        harp_set_error(HARP_ERROR_INGESTION, "dataset '/META_DATA/MainSpecies' has %d dimensions, expected 1)",
                       num_dims);
        return -1;
    }

    info->num_windows = dim[0];
    if (info->num_windows > 0)
    {
        if (coda_cursor_goto_first_array_element(&cursor) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        for (i = 0; i < info->num_windows; i++)
        {
            char name[10];
            int j;

            if (coda_cursor_read_string(&cursor, name, 10) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            for (j = 0; j < 7; j++)
            {
                if (strcmp(name, species_name[j]) == 0)
                {
                    info->window_for_species[j] = i;
                    break;
                }
            }
            if (i < info->num_windows - 1)
            {
                if (coda_cursor_goto_next_array_element(&cursor) != 0)
                {
                    harp_set_error(HARP_ERROR_CODA, NULL);
                    return -1;
                }
            }
        }
    }

    if (coda_cursor_goto(&cursor, "/META_DATA@ProductContents[0]") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_string(&cursor, product_contents, 100) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    for (i = 0; i < 7; i++)
    {
        if (info->window_for_species[i] >= 0)
        {
            if (strstr(product_contents, species_name[i]) == NULL)
            {
                info->window_for_species[i] = -1;
            }
        }
    }

    return 0;
}

static int init_revision(ingest_info *info)
{
    char revision[3];
    coda_cursor cursor;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, "META_DATA@Revision[0]") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_string(&cursor, revision, 3) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (revision[0] < '0' || revision[0] > '9' || revision[1] < '0' || revision[1] > '9')
    {
        harp_set_error(HARP_ERROR_INGESTION, "attribute '/META_DATA@Revision' does not contain a valid revision value");
        return -1;
    }
    info->revision = (revision[0] - '0') * 10 + (revision[1] - '0');

    return 0;
}

static int read_dataset(ingest_info *info, const char *path, harp_data_type data_type, long num_elements,
                        harp_array data)
{
    coda_cursor cursor;
    long coda_num_elements;

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
        harp_set_error(HARP_ERROR_INGESTION, "dataset '%s' has %ld elements (expected %ld)", path, coda_num_elements,
                       num_elements);
        return -1;
    }
    switch (data_type)
    {
        case harp_type_int32:
            if (coda_cursor_read_int32_array(&cursor, data.int32_data, coda_array_ordering_c) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            break;
        case harp_type_double:
            {
                double fill_value;

                if (coda_cursor_read_double_array(&cursor, data.double_data, coda_array_ordering_c) != 0)
                {
                    harp_set_error(HARP_ERROR_CODA, NULL);
                    return -1;
                }
                if (coda_cursor_goto(&cursor, "@FillValue[0]") != 0)
                {
                    harp_set_error(HARP_ERROR_CODA, NULL);
                    return -1;
                }
                if (coda_cursor_read_double(&cursor, &fill_value) != 0)
                {
                    harp_set_error(HARP_ERROR_CODA, NULL);
                    return -1;
                }
                if (!coda_isNaN(fill_value))
                {
                    long i;

                    /* Replace fill values with NaN. */
                    for (i = 0; i < num_elements; i++)
                    {
                        if (data.double_data[i] == fill_value)
                        {
                            data.double_data[i] = coda_NaN();
                        }
                    }
                }
            }
            break;
        default:
            assert(0);
            exit(1);
    }

    return 0;
}

/* read relative uncertainty [%] and turn it into an absolute uncertainty */
static int read_relative_uncertainty(ingest_info *info, const char *path_quantity, const char *path_error,
                                     long num_elements, harp_array data)
{
    harp_array relerr;
    long i;

    if (read_dataset(info, path_quantity, harp_type_double, num_elements, data) != 0)
    {
        return -1;
    }

    relerr.ptr = malloc(num_elements * sizeof(double));
    if (relerr.ptr == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_elements * sizeof(double), __FILE__, __LINE__);
        return -1;
    }

    if (read_dataset(info, path_error, harp_type_double, num_elements, relerr) != 0)
    {
        free(relerr.ptr);
        return -1;
    }

    /* Convert relative error (in percent) to standard deviation (same unit as the associated quantity). */
    for (i = 0; i < num_elements; i++)
    {
        data.double_data[i] *= relerr.double_data[i] * 0.01;    /* relative error is a percentage */
    }

    free(relerr.ptr);

    return 0;
}

static int init_amf(ingest_info *info)
{
    if (info->amf_buffer.ptr == NULL)
    {
        long dimension[2];
        long num_elements;

        dimension[0] = info->num_main;
        dimension[1] = info->num_windows;
        num_elements = harp_get_num_elements(2, dimension);

        info->amf_buffer.ptr = malloc(num_elements * sizeof(double));
        if (info->amf_buffer.ptr == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           num_elements * sizeof(double), __FILE__, __LINE__);
            return -1;
        }

        if (read_dataset(info, "DETAILED_RESULTS/AMFTotal", harp_type_double, num_elements, info->amf_buffer) != 0)
        {
            return -1;
        }

        /* Transpose such that all values for each window are contiguous in memory. */
        if (harp_array_transpose(harp_type_double, 2, dimension, NULL, info->amf_buffer) != 0)
        {
            return -1;
        }
    }

    return 0;
}

static int init_amf_error(ingest_info *info)
{
    if (init_amf(info) != 0)
    {
        return -1;
    }

    if (info->amf_error_buffer.ptr == NULL)
    {
        long dimension[2];
        long num_elements;
        long i;

        dimension[0] = info->num_main;
        dimension[1] = info->num_windows;
        num_elements = harp_get_num_elements(2, dimension);

        info->amf_error_buffer.ptr = malloc(num_elements * sizeof(double));
        if (info->amf_error_buffer.ptr == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           num_elements * sizeof(double), __FILE__, __LINE__);
            return -1;
        }

        if (read_dataset(info, "DETAILED_RESULTS/AMFTotal_Error", harp_type_double, num_elements,
                         info->amf_error_buffer) != 0)
        {
            return -1;
        }

        /* Transpose such that all values for each window are contiguous in memory. */
        if (harp_array_transpose(harp_type_double, 2, dimension, NULL, info->amf_error_buffer) != 0)
        {
            return -1;
        }

        /* Convert relative error (in percent) to standard deviation (unitless). */
        for (i = 0; i < num_elements; i++)
        {
            info->amf_error_buffer.double_data[i] *= info->amf_buffer.double_data[i] * 0.01;
        }
    }

    return 0;
}

static int init_index_in_scan(ingest_info *info)
{
    if (info->index_in_scan_buffer.ptr == NULL)
    {
        info->index_in_scan_buffer.ptr = malloc(info->num_main * sizeof(int32_t));
        if (info->index_in_scan_buffer.ptr == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           info->num_main * sizeof(int32_t), __FILE__, __LINE__);
            return -1;
        }

        if (read_dataset(info, "GEOLOCATION/IndexInScan", harp_type_int32, info->num_main, info->index_in_scan_buffer)
            != 0)
        {
            return -1;
        }
    }

    return 0;
}

static int init_quality_flags(ingest_info *info)
{
    if (info->quality_flags_buffer.ptr == NULL)
    {
        long dimension[2];
        long num_elements;

        dimension[0] = info->num_main;
        dimension[1] = info->num_windows;
        num_elements = harp_get_num_elements(2, dimension);

        info->quality_flags_buffer.ptr = malloc(num_elements * sizeof(int32_t));
        if (info->quality_flags_buffer.ptr == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           num_elements * sizeof(int32_t), __FILE__, __LINE__);
            return -1;
        }

        if (read_dataset(info, "DETAILED_RESULTS/QualityFlags", harp_type_int32, num_elements,
                         info->quality_flags_buffer) != 0)
        {
            return -1;
        }

        /* Transpose such that all values for each window are contiguous in memory. */
        if (harp_array_transpose(harp_type_int32, 2, dimension, NULL, info->quality_flags_buffer) != 0)
        {
            return -1;
        }
    }

    return 0;
}

static int read_amf(ingest_info *info, species_type species, harp_array data)
{
    long offset;

    /* This function cannot be called for unavailable species (because of species specific include() functions). */
    assert(info->window_for_species[species] >= 0);

    if (init_amf(info) != 0)
    {
        return -1;
    }

    offset = info->window_for_species[species] * info->num_main;
    memcpy(data.double_data, &info->amf_buffer.double_data[offset], info->num_main * sizeof(double));

    return 0;
}

static int read_amf_error(ingest_info *info, species_type species, harp_array data)
{
    long offset;

    /* This function cannot be called for unavailable species (because of species specific include() functions). */
    assert(info->window_for_species[species] >= 0);

    if (init_amf_error(info) != 0)
    {
        return -1;
    }

    offset = info->window_for_species[species] * info->num_main;
    memcpy(data.double_data, &info->amf_error_buffer.double_data[offset], info->num_main * sizeof(double));

    return 0;
}

static int read_quality_flags(ingest_info *info, species_type species, harp_array data)
{
    long offset;
    long i;

    /* This function cannot be called for unavailable species (because of species specific include() functions). */
    assert(info->window_for_species[species] >= 0);

    if (init_quality_flags(info) != 0)
    {
        return -1;
    }

    offset = info->window_for_species[species] * info->num_main;
    for (i = 0; i < info->num_main; i++)
    {
        data.int8_data[i] = (int8_t)info->quality_flags_buffer.int32_data[offset + i];
    }

    return 0;
}

static int read_dimensions(void *user_data, long dimension[HARP_NUM_DIM_TYPES])
{
    ingest_info *info = (ingest_info *)user_data;

    dimension[harp_dimension_time] = info->num_main;
    if (info->num_vertical > 0)
    {
        dimension[harp_dimension_vertical] = info->num_vertical;
    }

    return 0;
}

static int read_time(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    coda_cursor cursor;
    long dim[CODA_MAX_NUM_DIMS];
    int num_dims;
    long i;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, "GEOLOCATION/Time") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_array_dim(&cursor, &num_dims, dim) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (num_dims != 1)
    {
        harp_set_error(HARP_ERROR_INGESTION, "dataset '/GEOLOCATION/Time' has %d dimensions, expected 1", num_dims);
        return -1;
    }
    if (dim[0] != info->num_main)
    {
        harp_set_error(HARP_ERROR_INGESTION, "dataset '/GEOLOCATION/Time' has %ld elements, expected %ld", dim[0],
                       info->num_main);
        return -1;
    }
    if (coda_cursor_goto_first_array_element(&cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    for (i = 0; i < info->num_main; i++)
    {
        int32_t Day;
        int32_t MillisecondOfDay;

        if (coda_cursor_goto_first_record_field(&cursor) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_read_int32(&cursor, &Day) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_goto_next_record_field(&cursor) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_read_int32(&cursor, &MillisecondOfDay) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        coda_cursor_goto_parent(&cursor);
        if (Day == 0 && MillisecondOfDay == 0)
        {
            data.double_data[i] = coda_NaN();
        }
        else
        {
            data.double_data[i] = (Day - DAYS_FROM_1950_TO_2000) * 86400.0 + MillisecondOfDay / 1000.0;
        }
        if (i < info->num_main - 1)
        {
            if (coda_cursor_goto_next_array_element(&cursor) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
        }
    }

    return 0;
}

static int read_longitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "GEOLOCATION/LongitudeCentre", harp_type_double, info->num_main, data);
}

static int read_latitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "GEOLOCATION/LatitudeCentre", harp_type_double, info->num_main, data);
}

static int read_longitude_bounds(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long dimension[2];
    harp_array tmp;

    dimension[0] = 4;
    dimension[1] = info->num_main;

    tmp = data;
    if (read_dataset(info, "GEOLOCATION/LongitudeB", harp_type_double, info->num_main, data) != 0)
    {
        return -1;
    }
    data.double_data += info->num_main;
    if (read_dataset(info, "GEOLOCATION/LongitudeD", harp_type_double, info->num_main, data) != 0)
    {
        return -1;
    }
    data.double_data += info->num_main;
    if (read_dataset(info, "GEOLOCATION/LongitudeC", harp_type_double, info->num_main, data) != 0)
    {
        return -1;
    }
    data.double_data += info->num_main;
    if (read_dataset(info, "GEOLOCATION/LongitudeA", harp_type_double, info->num_main, data) != 0)
    {
        return -1;
    }

    /* Transpose such that the four corner coordinates for each sample are contiguous in memory. */
    if (harp_array_transpose(harp_type_double, 2, dimension, NULL, tmp) != 0)
    {
        return -1;
    }

    return 0;
}

static int read_latitude_bounds(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long dimension[2];
    harp_array tmp;

    dimension[0] = 4;
    dimension[1] = info->num_main;

    tmp = data;
    if (read_dataset(info, "GEOLOCATION/LatitudeB", harp_type_double, info->num_main, data) != 0)
    {
        return -1;
    }
    data.double_data += info->num_main;
    if (read_dataset(info, "GEOLOCATION/LatitudeD", harp_type_double, info->num_main, data) != 0)
    {
        return -1;
    }
    data.double_data += info->num_main;
    if (read_dataset(info, "GEOLOCATION/LatitudeC", harp_type_double, info->num_main, data) != 0)
    {
        return -1;
    }
    data.double_data += info->num_main;
    if (read_dataset(info, "GEOLOCATION/LatitudeA", harp_type_double, info->num_main, data) != 0)
    {
        return -1;
    }

    /* Transpose such that the four corner coordinates for each sample are contiguous in memory. */
    if (harp_array_transpose(harp_type_double, 2, dimension, NULL, tmp) != 0)
    {
        return -1;
    }

    return 0;
}

static int read_solar_zenith_angle_sensor(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "GEOLOCATION/SolarZenithAngleSatCentre", harp_type_double, info->num_main, data);
}

static int read_solar_zenith_angle(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "GEOLOCATION/SolarZenithAngleCentre", harp_type_double, info->num_main, data);
}

static int read_viewing_zenith_angle(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "GEOLOCATION/LineOfSightZenithAngleCentre", harp_type_double, info->num_main, data);
}

static int read_relative_azimuth_angle(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "GEOLOCATION/RelativeAzimuthCentre", harp_type_double, info->num_main, data);
}

static int read_bro_column(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "TOTAL_COLUMNS/BrO", harp_type_double, info->num_main, data);
}

static int read_bro_column_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->product_version < 3)
    {
        return read_relative_uncertainty(info, "TOTAL_COLUMNS/BrO", "TOTAL_COLUMNS/BrO_Error", info->num_main, data);
    }

    return read_dataset(info, "TOTAL_COLUMNS/BrO_Error", harp_type_double, info->num_main, data);
}

static int read_h2o_column(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "TOTAL_COLUMNS/H2O", harp_type_double, info->num_main, data);
}

static int read_h2o_column_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_relative_uncertainty(info, "TOTAL_COLUMNS/H2O", "TOTAL_COLUMNS/H2O_Error", info->num_main, data);
}

static int read_hcho_column(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "TOTAL_COLUMNS/HCHO", harp_type_double, info->num_main, data);
}

static int read_hcho_column_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->product_version < 3)
    {
        return read_relative_uncertainty(info, "TOTAL_COLUMNS/HCHO", "TOTAL_COLUMNS/HCHO_Error", info->num_main, data);
    }

    return read_dataset(info, "TOTAL_COLUMNS/HCHO_Error", harp_type_double, info->num_main, data);
}

static int read_no2_column(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "TOTAL_COLUMNS/NO2", harp_type_double, info->num_main, data);
}

static int read_no2_column_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->product_version < 3)
    {
        return read_relative_uncertainty(info, "TOTAL_COLUMNS/NO2", "TOTAL_COLUMNS/NO2_Error", info->num_main, data);
    }

    return read_dataset(info, "TOTAL_COLUMNS/NO2_Error", harp_type_double, info->num_main, data);
}

static int read_no2_column_tropospheric(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->product_version < 2)
    {
        return read_dataset(info, "TOTAL_COLUMNS/NO2_Trop", harp_type_double, info->num_main, data);
    }

    return read_dataset(info, "TOTAL_COLUMNS/NO2Tropo", harp_type_double, info->num_main, data);
}

static int read_no2_column_tropospheric_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->product_version < 3)
    {
        return read_relative_uncertainty(info, "TOTAL_COLUMNS/NO2Tropo", "TOTAL_COLUMNS/NO2Tropo_Error", info->num_main,
                                         data);
    }

    return read_dataset(info, "TOTAL_COLUMNS/NO2Tropo_Error", harp_type_double, info->num_main, data);
}

static int read_o3_column(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "TOTAL_COLUMNS/O3", harp_type_double, info->num_main, data);
}

static int read_o3_column_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->product_version < 3)
    {
        return read_relative_uncertainty(info, "TOTAL_COLUMNS/O3", "TOTAL_COLUMNS/O3_Error", info->num_main, data);
    }

    return read_dataset(info, "TOTAL_COLUMNS/O3_Error", harp_type_double, info->num_main, data);
}

static int read_oclo_column(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "TOTAL_COLUMNS/OClO", harp_type_double, info->num_main, data);
}

static int read_oclo_column_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->product_version < 3)
    {
        return read_relative_uncertainty(info, "TOTAL_COLUMNS/OClO", "TOTAL_COLUMNS/OClO_Error", info->num_main, data);
    }

    return read_dataset(info, "TOTAL_COLUMNS/OClO_Error", harp_type_double, info->num_main, data);
}

static int read_so2_column(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "TOTAL_COLUMNS/SO2", harp_type_double, info->num_main, data);
}

static int read_so2_column_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->product_version < 3)
    {
        return read_relative_uncertainty(info, "TOTAL_COLUMNS/SO2", "TOTAL_COLUMNS/SO2_Error", info->num_main, data);
    }

    return read_dataset(info, "TOTAL_COLUMNS/SO2_Error", harp_type_double, info->num_main, data);
}

static int read_amf_bro(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_amf(info, species_type_bro, data);
}

static int read_amf_bro_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_amf_error(info, species_type_bro, data);
}

static int read_amf_h2o(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_amf(info, species_type_h2o, data);
}

static int read_amf_h2o_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_amf_error(info, species_type_h2o, data);
}

static int read_amf_hcho(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_amf(info, species_type_hcho, data);
}

static int read_amf_hcho_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_amf_error(info, species_type_hcho, data);
}

static int read_amf_no2(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_amf(info, species_type_no2, data);
}

static int read_amf_no2_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_amf_error(info, species_type_no2, data);
}

static int read_amf_no2_tropospheric(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "DETAILED_RESULTS/NO2/AMFTropo", harp_type_double, info->num_main, data);
}

static int read_amf_no2_tropospheric_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_relative_uncertainty(info, "DETAILED_RESULTS/NO2/AMFTropo", "DETAILED_RESULTS/NO2/AMFTropo_Error",
                                     info->num_main, data);
}

static int read_amf_o3(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_amf(info, species_type_o3, data);
}

static int read_amf_o3_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_amf_error(info, species_type_o3, data);
}

static int read_amf_oclo(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_amf(info, species_type_oclo, data);
}

static int read_amf_oclo_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_amf_error(info, species_type_oclo, data);
}

static int read_amf_so2(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_amf(info, species_type_so2, data);
}

static int read_amf_so2_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_amf_error(info, species_type_so2, data);
}

static int read_quality_flags_bro(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_quality_flags(info, species_type_bro, data);
}

static int read_quality_flags_h2o(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_quality_flags(info, species_type_h2o, data);
}

static int read_quality_flags_hcho(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_quality_flags(info, species_type_hcho, data);
}

static int read_quality_flags_no2(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_quality_flags(info, species_type_no2, data);
}

static int read_quality_flags_o3(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_quality_flags(info, species_type_o3, data);
}

static int read_quality_flags_oclo(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_quality_flags(info, species_type_oclo, data);
}

static int read_quality_flags_so2(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_quality_flags(info, species_type_so2, data);
}

static int read_o3_temperature(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "DETAILED_RESULTS/O3/O3Temperature", harp_type_double, info->num_main, data);
}

static int read_pressure(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long dimension[2];

    if (info->detailed_results_type == species_type_hcho)
    {
        if (read_dataset(info, "DETAILED_RESULTS/HCHO/AveragingKernelPressureLevel", harp_type_double,
                         info->num_main * info->num_vertical, data) != 0)
        {
            return -1;
        }
    }
    else if (info->detailed_results_type == species_type_no2)
    {
        if (read_dataset(info, "DETAILED_RESULTS/NO2/AveragingKernelPressureLevel", harp_type_double,
                         info->num_main * info->num_vertical, data) != 0)
        {
            return -1;
        }
    }
    else
    {
        assert(0);
        exit(1);
    }

    dimension[0] = info->num_main;
    dimension[1] = info->num_vertical;
    return harp_array_invert(harp_type_double, 1, 2, dimension, data);
}

static int read_hcho_apriori(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long dimension[2];

    if (read_dataset(info, "DETAILED_RESULTS/HCHO/AprioriHCHOProfile", harp_type_double,
                     info->num_main * info->num_vertical, data) != 0)
    {
        return -1;
    }

    dimension[0] = info->num_main;
    dimension[1] = info->num_vertical;
    return harp_array_invert(harp_type_double, 1, 2, dimension, data);
}

static int read_hcho_avk(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long dimension[2];

    if (read_dataset(info, "DETAILED_RESULTS/HCHO/AveragingKernel", harp_type_double,
                     info->num_main * info->num_vertical, data) != 0)
    {
        return -1;
    }

    dimension[0] = info->num_main;
    dimension[1] = info->num_vertical;
    return harp_array_invert(harp_type_double, 1, 2, dimension, data);
}

static int read_no2_apriori(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long dimension[2];

    if (read_dataset(info, "DETAILED_RESULTS/NO2/AprioriNO2Profile", harp_type_double,
                     info->num_main * info->num_vertical, data) != 0)
    {
        return -1;
    }

    dimension[0] = info->num_main;
    dimension[1] = info->num_vertical;
    return harp_array_invert(harp_type_double, 1, 2, dimension, data);
}

static int read_no2_avk(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long dimension[2];

    if (read_dataset(info, "DETAILED_RESULTS/NO2/AveragingKernel", harp_type_double,
                     info->num_main * info->num_vertical, data) != 0)
    {
        return -1;
    }

    dimension[0] = info->num_main;
    dimension[1] = info->num_vertical;
    return harp_array_invert(harp_type_double, 1, 2, dimension, data);
}

static int read_surface_albedo(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    harp_array buffer;
    long dimension[2];
    long num_elements;
    long offset;
    long i;

    assert(info->detailed_results_type >= 0);
    offset = info->window_for_species[info->detailed_results_type];

    dimension[0] = info->num_main;
    dimension[1] = info->num_windows;
    num_elements = harp_get_num_elements(2, dimension);

    buffer.ptr = malloc(num_elements * sizeof(double));
    if (buffer.ptr == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_elements * sizeof(double), __FILE__, __LINE__);
        return -1;
    }

    if (read_dataset(info, "DETAILED_RESULTS/SurfaceAlbedo", harp_type_double, num_elements, buffer) != 0)
    {
        free(buffer.ptr);
        return -1;
    }

    for (i = 0; i < info->num_main; i++)
    {
        data.double_data[i] = buffer.double_data[i * info->num_windows + offset];
    }

    free(buffer.ptr);

    return 0;
}

static int read_cloud_fraction(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->product_version < 2)
    {
        return read_dataset(info, "DETAILED_RESULTS/CloudFraction", harp_type_double, info->num_main, data);
    }

    return read_dataset(info, "CLOUD_PROPERTIES/CloudFraction", harp_type_double, info->num_main, data);
}

static int read_cloud_fraction_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->product_version < 2)
    {
        return read_relative_uncertainty(info, "DETAILED_RESULTS/CloudFraction", "DETAILED_RESULTS/CloudFraction_Error",
                                         info->num_main, data);
    }

    return read_relative_uncertainty(info, "CLOUD_PROPERTIES/CloudFraction", "CLOUD_PROPERTIES/CloudFraction_Error",
                                     info->num_main, data);
}

static int read_pressure_cloud_top(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->product_version < 2)
    {
        return read_dataset(info, "DETAILED_RESULTS/CloudTopPressure", harp_type_double, info->num_main, data);
    }

    return read_dataset(info, "CLOUD_PROPERTIES/CloudTopPressure", harp_type_double, info->num_main, data);
}

static int read_pressure_cloud_top_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->product_version < 2)
    {
        return read_relative_uncertainty(info, "DETAILED_RESULTS/CloudTopPressure",
                                         "DETAILED_RESULTS/CloudTopPressure_Error", info->num_main, data);
    }

    return read_relative_uncertainty(info, "CLOUD_PROPERTIES/CloudTopPressure",
                                     "CLOUD_PROPERTIES/CloudTopPressure_Error", info->num_main, data);
}

static int read_height_cloud_top(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->product_version < 2)
    {
        return read_dataset(info, "DETAILED_RESULTS/CloudTopHeight", harp_type_double, info->num_main, data);
    }

    return read_dataset(info, "CLOUD_PROPERTIES/CloudTopHeight", harp_type_double, info->num_main, data);
}

static int read_height_cloud_top_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->product_version < 2)
    {
        return read_relative_uncertainty(info, "DETAILED_RESULTS/CloudTopHeight",
                                         "DETAILED_RESULTS/CloudTopHeight_Error", info->num_main, data);
    }

    return read_relative_uncertainty(info, "CLOUD_PROPERTIES/CloudTopHeight", "CLOUD_PROPERTIES/CloudTopHeight_Error",
                                     info->num_main, data);
}

static int read_albedo_cloud_top(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->product_version < 2)
    {
        return read_dataset(info, "DETAILED_RESULTS/CloudTopAlbedo", harp_type_double, info->num_main, data);
    }

    return read_dataset(info, "CLOUD_PROPERTIES/CloudTopAlbedo", harp_type_double, info->num_main, data);
}

static int read_albedo_cloud_top_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->product_version < 2)
    {
        return read_relative_uncertainty(info, "DETAILED_RESULTS/CloudTopAlbedo",
                                         "DETAILED_RESULTS/CloudTopAlbedo_Error", info->num_main, data);
    }

    return read_relative_uncertainty(info, "CLOUD_PROPERTIES/CloudTopAlbedo", "CLOUD_PROPERTIES/CloudTopAlbedo_Error",
                                     info->num_main, data);
}

static int read_cloud_optical_thickness(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->product_version < 2)
    {
        return read_dataset(info, "DETAILED_RESULTS/CloudOpticalThickness", harp_type_double, info->num_main, data);
    }

    return read_dataset(info, "CLOUD_PROPERTIES/CloudOpticalThickness", harp_type_double, info->num_main, data);
}

static int read_cloud_optical_thickness_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->product_version < 2)
    {
        return read_relative_uncertainty(info, "DETAILED_RESULTS/CloudOpticalThickness",
                                         "DETAILED_RESULTS/CloudOpticalThickness_Error", info->num_main, data);
    }

    return read_relative_uncertainty(info, "CLOUD_PROPERTIES/CloudOpticalThickness",
                                     "CLOUD_PROPERTIES/CloudOpticalThickness_Error", info->num_main, data);
}

static int read_absorbing_aerosol_index(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "DETAILED_RESULTS/AAI", harp_type_double, info->num_main, data);
}

static int read_surface_height(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "DETAILED_RESULTS/SurfaceHeight", harp_type_double, info->num_main, data);
}

static int read_surface_pressure(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info, "DETAILED_RESULTS/SurfacePressure", harp_type_double, info->num_main, data);
}

static int read_index_in_scan(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long i;

    if (init_index_in_scan(info) != 0)
    {
        return -1;
    }

    for (i = 0; i < info->num_main; i++)
    {
        assert(info->index_in_scan_buffer.int32_data[i] >= 0 && info->index_in_scan_buffer.int32_data[i] <= 127);
        data.int8_data[i] = (int8_t)info->index_in_scan_buffer.int32_data[i];
    }

    return 0;
}

static int read_sub_pixel_in_scan(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    harp_array buffer;
    long i;

    buffer.ptr = malloc(info->num_main * sizeof(int32_t));
    if (buffer.ptr == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       info->num_main * sizeof(int32_t), __FILE__, __LINE__);
        return -1;
    }

    if (read_dataset(info, "GEOLOCATION/SubpixelInScan", harp_type_int32, info->num_main, buffer) != 0)
    {
        free(buffer.ptr);
        return -1;
    }

    if (info->revision == 0)
    {
        for (i = 0; i < info->num_main; i++)
        {
            /* Perform shift to go from MDR pixel id to scan pixel id. */
            int32_t scan_pixel_id = (buffer.int32_data[i] + 31) % 32;

            assert(scan_pixel_id >= 0 && scan_pixel_id <= 127);
            data.int8_data[i] = (int8_t)scan_pixel_id;
        }
    }
    else
    {
        for (i = 0; i < info->num_main; i++)
        {
            assert(buffer.int32_data[i] >= 0 && buffer.int32_data[i] <= 127);
            data.int8_data[i] = (int8_t)buffer.int32_data[i];
        }
    }

    free(buffer.ptr);

    return 0;
}

static int read_scan_direction_type(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (init_index_in_scan(info) != 0)
    {
        return -1;
    }

    if (info->index_in_scan_buffer.int32_data[index] < 3)
    {
        *data.int8_data = 0;
    }
    else
    {
        *data.int8_data = 1;
    }

    return 0;
}

static int parse_option_detailed_results(ingest_info *info, const harp_ingestion_options *options)
{
    const char *value;

    if (harp_ingestion_options_get_option(options, "detailed_results", &value) == 0)
    {
        if (strcmp(value, "BrO") == 0)
        {
            if (info->window_for_species[species_type_bro] >= 0)
            {
                info->detailed_results_type = species_type_bro;
            }
        }
        else if (strcmp(value, "H2O") == 0)
        {
            if (info->window_for_species[species_type_h2o] >= 0)
            {
                info->detailed_results_type = species_type_h2o;
            }
        }
        else if (strcmp(value, "HCHO") == 0)
        {
            if (info->window_for_species[species_type_hcho] >= 0)
            {
                info->detailed_results_type = species_type_hcho;
            }
        }
        else if (strcmp(value, "NO2") == 0)
        {
            if (info->window_for_species[species_type_no2] >= 0)
            {
                info->detailed_results_type = species_type_no2;
            }
        }
        else if (strcmp(value, "O3") == 0)
        {
            if (info->window_for_species[species_type_o3] >= 0)
            {
                info->detailed_results_type = species_type_o3;
            }
        }
        else if (strcmp(value, "OClO") == 0)
        {
            if (info->window_for_species[species_type_oclo] >= 0)
            {
                info->detailed_results_type = species_type_oclo;
            }
        }
        else if (strcmp(value, "SO2") == 0)
        {
            if (info->window_for_species[species_type_so2] >= 0)
            {
                info->detailed_results_type = species_type_so2;
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
        if (info->amf_buffer.ptr != NULL)
        {
            free(info->amf_buffer.ptr);
        }

        if (info->amf_error_buffer.ptr != NULL)
        {
            free(info->amf_error_buffer.ptr);
        }

        if (info->index_in_scan_buffer.ptr != NULL)
        {
            free(info->index_in_scan_buffer.ptr);
        }

        if (info->quality_flags_buffer.ptr != NULL)
        {
            free(info->quality_flags_buffer.ptr);
        }

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
    info->product_version = -1;
    info->detailed_results_type = -1;
    info->amf_buffer.ptr = NULL;
    info->amf_error_buffer.ptr = NULL;
    info->index_in_scan_buffer.ptr = NULL;
    info->quality_flags_buffer.ptr = NULL;
    info->revision = 0;

    if (coda_get_product_version(info->product, &info->product_version) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        ingestion_done(info);
        return -1;
    }

    if (init_num_main(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }
    if (init_window_info(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }
    if (init_revision(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }
    if (parse_option_detailed_results(info, options) != 0)
    {
        ingestion_done(info);
        return -1;
    }
    if (init_num_vertical(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }

    *definition = *module->product_definition;
    *user_data = info;

    return 0;
}

static int dataset_available(ingest_info *info, const char *path)
{
    coda_cursor cursor;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        return 0;
    }
    if (coda_cursor_goto(&cursor, path) != 0)
    {
        return 0;
    }

    return 1;
}

static int include_no2_column_tropospheric(void *user_data)
{
    if (((ingest_info *)user_data)->product_version < 2)
    {
        return dataset_available((ingest_info *)user_data, "TOTAL_COLUMNS/NO2_Trop");
    }
    if (((ingest_info *)user_data)->product_version < 3)
    {
        return dataset_available((ingest_info *)user_data, "TOTAL_COLUMNS/NO2Tropo");
    }

    return 1;
}

static int include_no2_column_tropospheric_error(void *user_data)
{
    if (((ingest_info *)user_data)->product_version < 2)
    {
        return 0;
    }
    if (((ingest_info *)user_data)->product_version < 3)
    {
        return dataset_available((ingest_info *)user_data, "TOTAL_COLUMNS/NO2Tropo_Error");
    }
    return 1;
}

static int include_bro(void *user_data)
{
    return ((ingest_info *)user_data)->window_for_species[species_type_bro] >= 0;
}

static int include_h2o(void *user_data)
{
    return ((ingest_info *)user_data)->window_for_species[species_type_h2o] >= 0;
}

static int include_hcho(void *user_data)
{
    return ((ingest_info *)user_data)->window_for_species[species_type_hcho] >= 0;
}

static int include_no2(void *user_data)
{
    return ((ingest_info *)user_data)->window_for_species[species_type_no2] >= 0;
}

static int include_no2_v2(void *user_data)
{
    ingest_info *info = (ingest_info *)user_data;

    return info->product_version >= 2 && info->window_for_species[species_type_no2] >= 0;
}

static int include_o3(void *user_data)
{
    return ((ingest_info *)user_data)->window_for_species[species_type_o3] >= 0;
}

static int include_oclo(void *user_data)
{
    return ((ingest_info *)user_data)->window_for_species[species_type_oclo] >= 0;
}

static int include_so2(void *user_data)
{
    return ((ingest_info *)user_data)->window_for_species[species_type_so2] >= 0;
}

static int include_hcho_details(void *user_data)
{
    ingest_info *info = (ingest_info *)user_data;

    return info->product_version >= 3 && info->detailed_results_type == species_type_hcho;
}

static int include_no2_details(void *user_data)
{
    ingest_info *info = (ingest_info *)user_data;

    return info->product_version >= 3 && info->detailed_results_type == species_type_no2;
}

static int include_o3_details(void *user_data)
{
    ingest_info *info = (ingest_info *)user_data;

    return info->product_version >= 2 && info->detailed_results_type == species_type_o3;
}

static int include_pressure(void *user_data)
{
    ingest_info *info = (ingest_info *)user_data;

    return info->product_version >= 3 &&
        (info->detailed_results_type == species_type_no2 || info->detailed_results_type == species_type_hcho);
}

static int include_surface_albedo(void *user_data)
{
    ingest_info *info = (ingest_info *)user_data;

    return info->product_version >= 2 && info->detailed_results_type >= 0;
}

static void register_common_variables(harp_product_definition *product_definition)
{
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_independent };
    long dimension_bounds[2] = { -1, 4 };
    const char *description;
    const char *path;

    /* datetime */
    description = "time of the measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double, 1, dimension_type,
                                                   NULL, description, "seconds since 2000-01-01", NULL, read_time);
    path = "/GEOLOCATION/Time[]/Day, /GEOLOCATION/Time[]/MillisecondOfDay";
    description = "the time values are converted to seconds since 2000-01-01 00:00:00 using time = (Day - 1577836800) "
        "* 86400 + MillisecondOfDay / 1000";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* longitude */
    description = "longitude of the measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_double, 1, dimension_type,
                                                   NULL, description, "degree_east", NULL, read_longitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0, 180.0);
    path = "/GEOLOCATION/LongitudeCentre[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* latitude */
    description = "latitude of the measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_double, 1, dimension_type,
                                                   NULL, description, "degree_north", NULL, read_latitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    path = "/GEOLOCATION/LatitudeCentre[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* longitude_bounds */
    description = "corner longitudes of the measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "longitude_bounds", harp_type_double, 2,
                                                   dimension_type, dimension_bounds, description, "degree_east", NULL,
                                                   read_longitude_bounds);
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0, 180.0);
    path = "/GEOLOCATION/LongitudeA[], /GEOLOCATION/LongitudeB[], /GEOLOCATION/LongitudeC[], /GEOLOCATION/LongitudeD[]";
    description = "the corner coordinates are re-arranged in the order B-D-C-A";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* latitude_bounds */
    description = "corner latitudes of the measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "latitude_bounds", harp_type_double, 2,
                                                   dimension_type, dimension_bounds, description, "degree_north", NULL,
                                                   read_latitude_bounds);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    path = "/GEOLOCATION/LatitudeA[], /GEOLOCATION/LatitudeB[], /GEOLOCATION/LatitudeC[], /GEOLOCATION/LatitudeD[]";
    description = "the corner coordinates are re-arranged in the order B-D-C-A";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* sensor_solar_zenith_angle */
    description = "solar zenith angle at the sensor";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "sensor_solar_zenith_angle",
                                                   harp_type_double, 1, dimension_type, NULL, description, "degree",
                                                   NULL, read_solar_zenith_angle_sensor);
    harp_variable_definition_set_valid_range_double(variable_definition, 0.0, 180.0);
    path = "/GEOLOCATION/SolarZenithAngleSatCentre[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* solar_zenith_angle */
    description = "solar zenith angle at top of atmosphere";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "solar_zenith_angle", harp_type_double, 1,
                                                   dimension_type, NULL, description, "degree", NULL,
                                                   read_solar_zenith_angle);
    harp_variable_definition_set_valid_range_double(variable_definition, 0.0, 180.0);
    path = "/GEOLOCATION/SolarZenithAngleCentre[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* viewing_zenith_angle */
    description = "viewing zenith angle at top of atmosphere";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "viewing_zenith_angle", harp_type_double, 1,
                                                   dimension_type, NULL, description, "degree", NULL,
                                                   read_viewing_zenith_angle);
    harp_variable_definition_set_valid_range_double(variable_definition, 0.0, 180.0);
    path = "/GEOLOCATION/LineOfSightZenithAngleCentre[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* relative_azimuth_angle */
    description = "relative azimuth angle at top of atmosphere";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "relative_azimuth_angle", harp_type_double,
                                                   1, dimension_type, NULL, description, "degree", NULL,
                                                   read_relative_azimuth_angle);
    harp_variable_definition_set_valid_range_double(variable_definition, 0.0, 360.0);
    path = "/GEOLOCATION/RelativeAzimuthCentre[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* BrO_column_number_density */
    description = "BrO column number density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "BrO_column_number_density", harp_type_double, 1,
                                                   dimension_type, NULL, description, "molec/cm^2", include_bro,
                                                   read_bro_column);
    path = "/TOTAL_COLUMNS/BrO[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* BrO_column_number_density_uncertainty */
    description = "uncertainty of the BrO column number density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "BrO_column_number_density_uncertainty",
                                                   harp_type_double, 1, dimension_type, NULL, description,
                                                   "molec/cm^2", include_bro, read_bro_column_error);
    path = "/TOTAL_COLUMNS/BrO_Error[], /TOTAL_COLUMNS/BrO[]";
    description = "derived from the relative error in percent as: BrO_Error[] * 0.01 * BrO[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA product version < 3", path, description);
    path = "/TOTAL_COLUMNS/BrO_Error[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA product version >= 3", path, NULL);

    /* BrO_column_number_density_validity */
    description = "quality flags for BrO retrieval";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "BrO_column_number_density_validity",
                                                   harp_type_int8, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, include_bro, read_quality_flags_bro);
    path = "/DETAILED_RESULTS/QualityFlags[,window], /META_DATA/MainSpecies[]";
    description = "window is the index in MainSpecies[] that has the value 'BrO'";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* H2O_column_density */
    description = "H2O column mass density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "H2O_column_density", harp_type_double, 1,
                                                   dimension_type, NULL, description, "kg/m^2", include_h2o,
                                                   read_h2o_column);
    path = "/TOTAL_COLUMNS/H2O[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* H2O_column_density_uncertainty */
    description = "uncertainty of the H2O column mass density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "H2O_column_density_uncertainty",
                                                   harp_type_double, 1, dimension_type, NULL, description, "kg/m^2",
                                                   include_h2o, read_h2o_column_error);
    path = "/TOTAL_COLUMNS/H2O_Error[], /TOTAL_COLUMNS/H2O[]";
    description = "derived from the relative error in percent as: H2O_Error[] * 0.01 * H2O[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA product version < 3", path, description);
    path = "/TOTAL_COLUMNS/H2O_Error[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA product version >= 3", path, NULL);

    /* H2O_column_number_density_validity */
    description = "quality flags for H2O retrieval";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "H2O_column_number_density_validity",
                                                   harp_type_int8, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, include_h2o, read_quality_flags_h2o);
    path = "/DETAILED_RESULTS/QualityFlags[,window], /META_DATA/MainSpecies[]";
    description = "window is the index in MainSpecies[] that has the value 'H2O'";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* HCHO_column_number_density */
    description = "HCHO column number density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "HCHO_column_number_density", harp_type_double,
                                                   1, dimension_type, NULL, description, "molec/cm^2", include_hcho,
                                                   read_hcho_column);
    path = "/TOTAL_COLUMNS/HCHO[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* HCHO_column_number_density_uncertainty */
    description = "uncertainty of the HCHO column number density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "HCHO_column_number_density_uncertainty",
                                                   harp_type_double, 1, dimension_type, NULL, description,
                                                   "molec/cm^2", include_hcho, read_hcho_column_error);
    path = "/TOTAL_COLUMNS/HCHO_Error[], /TOTAL_COLUMNS/HCHO[]";
    description = "derived from the relative error in percent as: HCHO_Error[] * 0.01 * HCHO[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA product version < 3", path, description);
    path = "/TOTAL_COLUMNS/HCHO_Error[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA product version >= 3", path, NULL);

    /* HCHO_column_number_density_validity */
    description = "quality flags for HCHO retrieval";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "HCHO_column_number_density_validity",
                                                   harp_type_int8, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, include_hcho, read_quality_flags_hcho);
    path = "/DETAILED_RESULTS/QualityFlags[,window], /META_DATA/MainSpecies[]";
    description = "window is the index in MainSpecies[] that has the value 'HCHO'";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* NO2_column_number_density */
    description = "NO2 column number density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "NO2_column_number_density", harp_type_double, 1,
                                                   dimension_type, NULL, description, "molec/cm^2", include_no2,
                                                   read_no2_column);
    path = "/TOTAL_COLUMNS/NO2[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* NO2_column_number_density_uncertainty */
    description = "uncertainty of the NO2 column number density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "NO2_column_number_density_uncertainty",
                                                   harp_type_double, 1, dimension_type, NULL, description,
                                                   "molec/cm^2", include_no2, read_no2_column_error);
    path = "/TOTAL_COLUMNS/NO2_Error[], /TOTAL_COLUMNS/NO2[]";
    description = "derived from the relative error in percent as: NO2_Error[] * 0.01 * NO2[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA product version < 3", path, description);
    path = "/TOTAL_COLUMNS/NO2_Error[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA product version >= 3", path, NULL);

    /* NO2_column_number_density_validity */
    description = "quality flags for NO2 retrieval";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "NO2_column_number_density_validity",
                                                   harp_type_int8, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, include_no2, read_quality_flags_no2);
    path = "/DETAILED_RESULTS/QualityFlags[,window], /META_DATA/MainSpecies[]";
    description = "window is the index in MainSpecies[] that has the value 'NO2'";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* tropospheric_NO2_column_number_density */
    description = "tropospheric NO2 column number density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "tropospheric_NO2_column_number_density",
                                                   harp_type_double, 1, dimension_type, NULL, description, "molec/cm^2",
                                                   include_no2_column_tropospheric, read_no2_column_tropospheric);
    path = "/TOTAL_COLUMNS/NO2_Trop[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA product version < 2", path, NULL);
    path = "/TOTAL_COLUMNS/NO2Tropo[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA product version >= 2", path, NULL);

    /* tropospheric_NO2_column_number_density_uncertainty */
    description = "uncertainty of the tropospheric NO2 column number density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "tropospheric_NO2_column_number_density_uncertainty",
                                                   harp_type_double, 1, dimension_type, NULL, description, "molec/cm^2",
                                                   include_no2_column_tropospheric_error,
                                                   read_no2_column_tropospheric_error);
    path = "/TOTAL_COLUMNS/NO2Tropo_Error[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA product version >= 2", path, NULL);

    /* O3_column_number_density */
    description = "O3 column number density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_column_number_density", harp_type_double, 1,
                                                   dimension_type, NULL, description, "DU", include_o3, read_o3_column);
    path = "/TOTAL_COLUMNS/O3[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* O3_column_number_density_uncertainty */
    description = "uncertainty of the O3 column number density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_column_number_density_uncertainty",
                                                   harp_type_double, 1, dimension_type, NULL, description, "DU",
                                                   include_o3, read_o3_column_error);
    path = "/TOTAL_COLUMNS/O3_Error[], /TOTAL_COLUMNS/O3[]";
    description = "derived from the relative error in percent as: O3_Error[] * 0.01 * O3[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA product version < 3", path, description);
    path = "/TOTAL_COLUMNS/O3_Error[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA product version >= 3", path, NULL);

    /* O3_column_number_density_validity */
    description = "quality flags for O3 retrieval";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_column_number_density_validity",
                                                   harp_type_int8, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, include_o3, read_quality_flags_o3);
    path = "/DETAILED_RESULTS/QualityFlags[,window], /META_DATA/MainSpecies[]";
    description = "window is the index in MainSpecies[] that has the value 'O3'";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* OClO_column_number_density */
    description = "OClO column number density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "OClO_column_number_density", harp_type_double,
                                                   1, dimension_type, NULL, description, "molec/cm^2", include_oclo,
                                                   read_oclo_column);
    path = "/TOTAL_COLUMNS/OClO[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* OClO_column_number_density_uncertainty */
    description = "uncertainty of the OClO column number density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "OClO_column_number_density_uncertainty",
                                                   harp_type_double, 1, dimension_type, NULL, description, "molec/cm^2",
                                                   include_oclo, read_oclo_column_error);
    path = "/TOTAL_COLUMNS/OClO_Error[], /TOTAL_COLUMNS/OClO[]";
    description = "derived from the relative error in percent as: OClO_Error[] * 0.01 * OClO[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA product version < 3", path, description);
    path = "/TOTAL_COLUMNS/OClO_Error[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA product version >= 3", path, NULL);

    /* OClO_column_number_density_validity */
    description = "quality flags for OClO retrieval";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "OClO_column_number_density_validity",
                                                   harp_type_int8, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, include_oclo, read_quality_flags_oclo);
    path = "/DETAILED_RESULTS/QualityFlags[,window], /META_DATA/MainSpecies[]";
    description = "window is the index in MainSpecies[] that has the value 'OClO'";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* SO2_column_number_density */
    description = "SO2 column number density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "SO2_column_number_density", harp_type_double, 1,
                                                   dimension_type, NULL, description, "DU", include_so2,
                                                   read_so2_column);
    path = "/TOTAL_COLUMNS/SO2[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* SO2_column_number_density_uncertainty */
    description = "uncertainty of the SO2 column number density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "SO2_column_number_density_uncertainty",
                                                   harp_type_double, 1, dimension_type, NULL, description, "DU",
                                                   include_so2, read_so2_column_error);
    path = "/TOTAL_COLUMNS/SO2_Error[], /TOTAL_COLUMNS/SO2[]";
    description = "derived from the relative error in percent as: SO2_Error[] * 0.01 * SO2[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA product version < 3", path, description);
    path = "/TOTAL_COLUMNS/SO2_Error[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA product version >= 3", path, NULL);

    /* SO2_column_number_density_validity */
    description = "quality flags for SO2 retrieval";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "SO2_column_number_density_validity",
                                                   harp_type_int8, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, include_so2, read_quality_flags_so2);
    path = "/DETAILED_RESULTS/QualityFlags[,window], /META_DATA/MainSpecies[]";
    description = "window is the index in MainSpecies[] that has the value 'SO2'";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* BrO_column_number_density_amf */
    description = "BrO air mass factor";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "BrO_column_number_density_amf",
                                                   harp_type_double, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, include_bro, read_amf_bro);
    path = "/DETAILED_RESULTS/AMFTotal[,window], /META_DATA/MainSpecies[]";
    description = "window is the index in MainSpecies[] that has the value 'BrO'";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* BrO_column_number_density_amf_uncertainty */
    description = "uncertainty of the BrO air mass factor";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "BrO_column_number_density_amf_uncertainty",
                                                   harp_type_double, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, include_bro, read_amf_bro_error);
    path = "/DETAILED_RESULTS/AMFTotal_Error[,window], /DETAILED_RESULTS/AMFTotal[,window], /META_DATA/MainSpecies[]";
    description = "derived from the relative error in percent as: AMFTotal_Error[,window] * 0.01 * AMFTotal[,window]; "
        "window is the index in MainSpecies[] that has the value 'BrO'";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* H2O_column_number_density_amf */
    description = "H2O air mass factor";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "H2O_column_number_density_amf",
                                                   harp_type_double, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, include_h2o, read_amf_h2o);
    path = "/DETAILED_RESULTS/AMFTotal[,window], /META_DATA/MainSpecies[]";
    description = "window is the index in MainSpecies[] that has the value 'H2O'";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* H2O_column_number_density_amf_uncertainty */
    description = "uncertainty of the H2O air mass factor";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "H2O_column_number_density_amf_uncertainty",
                                                   harp_type_double, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, include_h2o, read_amf_h2o_error);
    path = "/DETAILED_RESULTS/AMFTotal_Error[,window], /DETAILED_RESULTS/AMFTotal[,window], /META_DATA/MainSpecies[]";
    description = "derived from the relative error in percent as: AMFTotal_Error[,window] * 0.01 * AMFTotal[,window]; "
        "window is the index in MainSpecies[] that has the value 'H2O'";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* HCHO_column_number_density_amf */
    description = "HCHO air mass factor";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "HCHO_column_number_density_amf",
                                                   harp_type_double, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, include_hcho, read_amf_hcho);
    path = "/DETAILED_RESULTS/AMFTotal[,window], /META_DATA/MainSpecies[]";
    description = "window is the index in MainSpecies[] that has the value 'HCHO'";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* HCHO_column_number_density_amf_uncertainty */
    description = "uncertainty of the HCHO air mass factor";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "HCHO_column_number_density_amf_uncertainty",
                                                   harp_type_double, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, include_hcho, read_amf_hcho_error);
    path = "/DETAILED_RESULTS/AMFTotal_Error[,window], /DETAILED_RESULTS/AMFTotal[,window], /META_DATA/MainSpecies[]";
    description = "derived from the relative error in percent as: AMFTotal_Error[,window] * 0.01 * AMFTotal[,window]; "
        "window is the index in MainSpecies[] that has the value 'HCHO'";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* NO2_column_number_density_amf */
    description = "NO2 air mass factor";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "NO2_column_number_density_amf",
                                                   harp_type_double, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, include_no2, read_amf_no2);
    path = "/DETAILED_RESULTS/AMFTotal[,window], /META_DATA/MainSpecies[]";
    description = "window is the index in MainSpecies[] that has the value 'NO2'";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* NO2_column_number_density_amf_uncertainty */
    description = "uncertainty of the NO2 air mass factor";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "NO2_column_number_density_amf_uncertainty",
                                                   harp_type_double, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, include_no2, read_amf_no2_error);
    path = "/DETAILED_RESULTS/AMFTotal_Error[,window], /DETAILED_RESULTS/AMFTotal[,window], /META_DATA/MainSpecies[]";
    description = "derived from the relative error in percent as: AMFTotal_Error[,window] * 0.01 * AMFTotal[,window]; "
        "window is the index in MainSpecies[] that has the value 'NO2'";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* tropospheric_NO2_column_number_density_amf */
    description = "tropospheric NO2 air mass factor";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "tropospheric_NO2_column_number_density_amf",
                                                   harp_type_double, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, include_no2_v2, read_amf_no2_tropospheric);
    path = "/DETAILED_RESULTS/NO2/AMFTropo[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA product version >= 2", path, NULL);

    /* tropospheric_NO2_column_number_density_amf_uncertainty */
    description = "uncertainty of the tropospheric NO2 air mass factor";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition,
                                                   "tropospheric_NO2_column_number_density_amf_uncertainty",
                                                   harp_type_double, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, include_no2_v2,
                                                   read_amf_no2_tropospheric_error);
    path = "/DETAILED_RESULTS/NO2/AMFTropo_Error[], /DETAILED_RESULTS/NO2/AMFTropo[]";
    description = "derived from the relative error in percent as: AMFTropo_Error[] * 0.01 * AMFTropo[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "CODA product version >= 2", description);

    /* O3_column_number_density_amf */
    description = "O3 air mass factor";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_column_number_density_amf", harp_type_double,
                                                   1, dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS,
                                                   include_o3, read_amf_o3);
    path = "/DETAILED_RESULTS/AMFTotal[,window], /META_DATA/MainSpecies[]";
    description = "window is the index in MainSpecies[] that has the value 'O3'";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* O3_column_number_density_amf_uncertainty */
    description = "uncertainty of the O3 air mass factor";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_column_number_density_amf_uncertainty",
                                                   harp_type_double, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, include_o3, read_amf_o3_error);
    path = "/DETAILED_RESULTS/AMFTotal_Error[,window], /DETAILED_RESULTS/AMFTotal[,window], /META_DATA/MainSpecies[]";
    description = "derived from the relative error in percent as: AMFTotal_Error[,window] * 0.01 * AMFTotal[,window]; "
        "window is the index in MainSpecies[] that has the value 'O3'";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* OClO_column_number_density_amf */
    description = "OClO air mass factor";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "OClO_column_number_density_amf",
                                                   harp_type_double, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, include_oclo, read_amf_oclo);
    path = "/DETAILED_RESULTS/AMFTotal[,window], /META_DATA/MainSpecies[]";
    description = "window is the index in MainSpecies[] that has the value 'OClO'";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* OClO_column_number_density_amf_uncertainty */
    description = "uncertainty of the OClO air mass factor";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "OClO_column_number_density_amf_uncertainty",
                                                   harp_type_double, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, include_oclo, read_amf_oclo_error);
    path = "/DETAILED_RESULTS/AMFTotal_Error[,window], /DETAILED_RESULTS/AMFTotal[,window], /META_DATA/MainSpecies[]";
    description = "derived from the relative error in percent as: AMFTotal_Error[,window] * 0.01 * AMFTotal[,window]; "
        "window is the index in MainSpecies[] that has the value 'OClO'";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* SO2_column_number_density_amf */
    description = "SO2 air mass factor";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "SO2_column_number_density_amf",
                                                   harp_type_double, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, include_so2, read_amf_so2);
    path = "/DETAILED_RESULTS/AMFTotal[,window], /META_DATA/MainSpecies[]";
    description = "window is the index in MainSpecies[] that has the value 'SO2'";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* SO2_column_number_density_amf_uncertainty */
    description = "uncertainty of the SO2 air mass factor";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "SO2_column_number_density_amf_uncertainty",
                                                   harp_type_double, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, include_so2, read_amf_so2_error);
    path = "/DETAILED_RESULTS/AMFTotal_Error[,window], /DETAILED_RESULTS/AMFTotal[,window], /META_DATA/MainSpecies[]";
    description = "derived from the relative error in percent as: AMFTotal_Error[,window] * 0.01 * AMFTotal[,window]; "
        "window is the index in MainSpecies[] that has the value 'SO2'";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* O3_effective_temperature */
    description = "fitted ozone temperature";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_effective_temperature",
                                                   harp_type_double, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, include_o3_details, read_o3_temperature);
    path = "/DETAILED_RESULTS/O3/O3Temperature";
    harp_variable_definition_add_mapping(variable_definition, "detailed_results=O3", "CODA product version >= 2", path,
                                         NULL);

    /* pressure */
    dimension_type[1] = harp_dimension_vertical;
    description = "pressure levels";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "pressure", harp_type_double, 2, dimension_type,
                                                   NULL, description, "hPa", include_pressure, read_pressure);
    path = "/DETAILED_RESULTS/HCHO/AveragingKernelPressureLevel";
    description = "the vertical grid is inverted to make it ascending";
    harp_variable_definition_add_mapping(variable_definition, "detailed_results=HCHO", "CODA product version >= 3",
                                         path, description);
    path = "/DETAILED_RESULTS/NO2/AveragingKernelPressureLevel";
    harp_variable_definition_add_mapping(variable_definition, "detailed_results=NO2", "CODA product version >= 3", path,
                                         description);

    /* HCHO_volume_mixing_ratio_dry_air_apriori */
    description = "a priori HCHO volume mixing ratio profile";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "HCHO_volume_mixing_ratio_dry_air_apriori",
                                                   harp_type_double, 2, dimension_type, NULL, description,
                                                   HARP_UNIT_VOLUME_MIXING_RATIO, include_hcho_details,
                                                   read_hcho_apriori);
    path = "/DETAILED_RESULTS/HCHO/AprioriHCHOProfile";
    description = "the vertical grid is inverted to make it ascending";
    harp_variable_definition_add_mapping(variable_definition, "detailed_results=HCHO", "CODA product version >= 3",
                                         path, description);

    /* HCHO_column_number_density_avk */
    description = "HCHO column averaging kernel";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "HCHO_column_number_density_avk",
                                                   harp_type_double, 2, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, include_hcho_details, read_hcho_avk);
    path = "/DETAILED_RESULTS/HCHO/AveragingKernel";
    description = "the vertical grid is inverted to make it ascending";
    harp_variable_definition_add_mapping(variable_definition, "detailed_results=HCHO", "CODA product version >= 3",
                                         path, description);

    /* NO2_volume_mixing_ratio_dry_air_apriori */
    description = "a priori NO2 volume mixing ratio profile";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "NO2_volume_mixing_ratio_dry_air_apriori",
                                                   harp_type_double, 2, dimension_type, NULL, description,
                                                   HARP_UNIT_VOLUME_MIXING_RATIO, include_no2_details,
                                                   read_no2_apriori);
    path = "/DETAILED_RESULTS/HCHO/AprioriNO2Profile";
    description = "the vertical grid is inverted to make it ascending";
    harp_variable_definition_add_mapping(variable_definition, "detailed_results=NO2", "CODA product version >= 3",
                                         path, description);

    /* NO2_column_number_density_avk */
    description = "NO2 column averaging kernel";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "NO2_column_number_density_avk",
                                                   harp_type_double, 2, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, include_no2_details, read_no2_avk);
    path = "/DETAILED_RESULTS/NO2/AveragingKernel";
    description = "the vertical grid is inverted to make it ascending";
    harp_variable_definition_add_mapping(variable_definition, "detailed_results=NO2", "CODA product version >= 3",
                                         path, description);

    /* surface_albedo */
    description = "surface albedo";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "surface_albedo", harp_type_double, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS,
                                                   include_surface_albedo, read_surface_albedo);
    path = "/DETAILED_RESULTS/SurfaceAlbedo[,window], /META_DATA/MainSpecies[]";
    description =
        "window is the index in MainSpecies[] that has the value for which the detailed_restults opion is set";
    harp_variable_definition_add_mapping(variable_definition, "detailed_results set", "CODA product version >= 2", path,
                                         description);

    /* cloud_fraction */
    description = "cloud fraction";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_fraction", harp_type_double, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_cloud_fraction);
    path = "/DETAILED_RESULTS/CloudFraction[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA product version < 2", path, NULL);
    path = "/CLOUD_PROPERTIES/CloudFraction[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA product version >= 2", path, NULL);

    /* cloud_fraction_uncertainty */
    description = "uncertainty of the cloud fraction";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_fraction_uncertainty", harp_type_double,
                                                   1, dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_cloud_fraction_error);
    description = "derived from the relative error in percent as: CloudFraction_Error[] * 0.01 * CloudFraction[]";
    path = "/DETAILED_RESULTS/CloudFraction_Error[], /DETAILED_RESULTS/CloudFraction[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA product version < 2", path, description);
    path = "/CLOUD_PROPERTIES/CloudFraction_Error[], /CLOUD_PROPERTIES/CloudFraction[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA product version >= 2", path, description);

    /* cloud_top_pressure */
    description = "cloud top pressure";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_top_pressure", harp_type_double, 1,
                                                   dimension_type, NULL, description, "hPa", NULL,
                                                   read_pressure_cloud_top);
    path = "/DETAILED_RESULTS/CloudTopPressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA product version < 2", path, NULL);
    path = "/CLOUD_PROPERTIES/CloudTopPressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA product version >= 2", path, NULL);

    /* cloud_top_pressure_uncertainty */
    description = "uncertainty of the cloud top pressure";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_top_pressure_uncertainty",
                                                   harp_type_double, 1, dimension_type, NULL, description, "hPa",
                                                   NULL, read_pressure_cloud_top_error);
    description = "derived from the relative error in percent as: CloudTopPressure_Error[] * 0.01 * CloudTopPressure[]";
    path = "/DETAILED_RESULTS/CloudTopPressure_Error[], /DETAILED_RESULTS/CloudTopPressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA product version < 2", path, description);
    path = "/CLOUD_PROPERTIES/CloudTopPressure_Error[], /CLOUD_PROPERTIES/CloudTopPressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA product version >= 2", path, description);

    /* cloud_top_height */
    description = "cloud top height";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_top_height", harp_type_double, 1,
                                                   dimension_type, NULL, description, "km", NULL,
                                                   read_height_cloud_top);
    path = "/DETAILED_RESULTS/CloudTopHeight[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA product version < 2", path, NULL);
    path = "/CLOUD_PROPERTIES/CloudTopHeight[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA product version >= 2", path, NULL);

    /* cloud_top_height_uncertainty */
    description = "uncertainty of the cloud top height";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_top_height_uncertainty", harp_type_double,
                                                   1, dimension_type, NULL, description, "km", NULL,
                                                   read_height_cloud_top_error);
    description = "derived from the relative error in percent as: CloudTopHeight_Error[] * 0.01 * CloudTopHeight[]";
    path = "/DETAILED_RESULTS/CloudTopHeight_Error[], /DETAILED_RESULTS/CloudTopHeight[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA product version < 2", path, description);
    path = "/CLOUD_PROPERTIES/CloudTopHeight_Error[], /CLOUD_PROPERTIES/CloudTopHeight[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA product version >= 2", path, description);

    /* cloud_top_albedo */
    description = "cloud top albedo";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_top_albedo", harp_type_double, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_albedo_cloud_top);
    path = "/DETAILED_RESULTS/CloudTopAlbedo[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA product version < 2", path, NULL);
    path = "/CLOUD_PROPERTIES/CloudTopAlbedo[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA product version >= 2", path, NULL);

    /* cloud_top_albedo_uncertainty */
    description = "uncertainty of the cloud top albedo";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_top_albedo_uncertainty", harp_type_double,
                                                   1, dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS,
                                                   NULL, read_albedo_cloud_top_error);
    description = "derived from the relative error in percent as: CloudTopAlbedo_Error[] * 0.01 * CloudTopAlbedo[]";
    path = "/DETAILED_RESULTS/CloudTopAlbedo_Error[], /DETAILED_RESULTS/CloudTopAlbedo[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA product version < 2", path, description);
    path = "/CLOUD_PROPERTIES/CloudTopAlbedo_Error[], /CLOUD_PROPERTIES/CloudTopAlbedo[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA product version >= 2", path, description);

    /* cloud_optical_depth */
    description = "cloud optical depth";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_optical_depth", harp_type_double, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_cloud_optical_thickness);
    path = "/DETAILED_RESULTS/CloudOpticalThickness[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA product version < 2", path, NULL);
    path = "/CLOUD_PROPERTIES/CloudOpticalThickness[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA product version >= 2", path, NULL);

    /* cloud_optical_depth_uncertainty */
    description = "uncertainty of the cloud optical depth";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_optical_depth_uncertainty",
                                                   harp_type_double, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL, read_cloud_optical_thickness_error);
    description = "derived from the relative error in percent as: CloudOpticalThickness_Error[] * 0.01 * "
        "CloudOpticalThickness[]";
    path = "/DETAILED_RESULTS/CloudOpticalThickness_Error[], /DETAILED_RESULTS/CloudOpticalThickness[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA product version < 2", path, description);
    path = "/CLOUD_PROPERTIES/CloudOpticalThickness_Error[], /CLOUD_PROPERTIES/CloudOpticalThickness[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA product version >= 2", path, description);

    /* absorbing_aerosol_index */
    description = "absorbing aerosol index";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "absorbing_aerosol_index", harp_type_double, 1,
                                                   dimension_type, NULL, description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                   read_absorbing_aerosol_index);
    path = "/DETAILED_RESULTS/AAI[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* surface_height */
    description = "surface height";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "surface_heigth", harp_type_double, 1,
                                                   dimension_type, NULL, description, "km", NULL, read_surface_height);
    path = "/DETAILED_RESULTS/SurfaceHeight[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* surface_pressure */
    description = "surface pressure";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "surface_pressure", harp_type_double, 1,
                                                   dimension_type, NULL, description, "hPa", NULL,
                                                   read_surface_pressure);
    path = "/DETAILED_RESULTS/SurfacePressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

static void register_scan_variables(harp_product_definition *product_definition, int is_ers_product)
{
    const char *scan_direction_type_values[] = { "forward", "backward" };
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };
    const char *description;
    const char *path;

    /* scan_subindex */
    if (is_ers_product)
    {
        description = "the relative index (0-3) of this measurement within a scan (forward + backward)";
        variable_definition =
            harp_ingestion_register_variable_full_read(product_definition, "scan_subindex", harp_type_int8, 1,
                                                       dimension_type, NULL, description, NULL, NULL,
                                                       read_index_in_scan);
        path = "/GEOLOCATION/IndexInScan[]";
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
    }
    else
    {
        description = "the relative index (0-31) of this measurement within a scan (forward + backward)";
        variable_definition =
            harp_ingestion_register_variable_full_read(product_definition, "scan_subindex", harp_type_int8, 1,
                                                       dimension_type, NULL, description, NULL, NULL,
                                                       read_sub_pixel_in_scan);
        path = "/GEOLOCATION/SubPixelInScan[]";
        description = "the pixel id is actually the pixel id relative to the L1b MDR, which is off by one with regard "
            "to the scan; the MDR pixel id is therefore converted to a real scan pixel id by subtracting one "
            "and performing a modulo 32";
        harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);
    }

    /* scan_direction_type */
    description = "scan direction for each measurement";
    variable_definition =
        harp_ingestion_register_variable_block_read(product_definition, "scan_direction_type", harp_type_int8, 1,
                                                    dimension_type, NULL, description, NULL, NULL,
                                                    read_scan_direction_type);
    harp_variable_definition_set_enumeration_values(variable_definition, 2, scan_direction_type_values);
    path = "/GEOLOCATION/IndexInScan[]";
    description = "the scan direction is based on IndexInScan[]; 0-2: forward (0), 3: backward (1)";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);
}

static void register_common_options(harp_ingestion_module *module)
{
    const char *detailed_results_option_values[7] = { "BrO", "H2O", "HCHO", "NO2", "O3", "OClO", "SO2" };
    const char *description;

    /* detailed results ingestion option */
    description = "include additional detailed results for the given species";
    harp_ingestion_register_option(module, "detailed_results", description, 7, detailed_results_option_values);
}

static void register_o3mnto_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;

    module = harp_ingestion_register_module_coda("GOME2_L2_O3MNTO", "GOME-2", "ACSAF", "O3MNTO",
                                                 "GOME2 near-real-time total column trace gas product", ingestion_init,
                                                 ingestion_done);
    register_common_options(module);

    /* O3MNTO product */
    product_definition = harp_ingestion_register_product(module, "GOME2_L2_O3MNTO", NULL, read_dimensions);
    register_common_variables(product_definition);
    register_scan_variables(product_definition, 0);
}

static void register_o3moto_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;

    module = harp_ingestion_register_module_coda("GOME2_L2_O3MOTO", "GOME-2", "ACSAF", "O3MOTO",
                                                 "GOME2 offline total column trace gas product", ingestion_init,
                                                 ingestion_done);
    register_common_options(module);

    /* O3MOTO product */
    product_definition = harp_ingestion_register_product(module, "GOME2_L2_O3MOTO", NULL, read_dimensions);
    register_common_variables(product_definition);
    register_scan_variables(product_definition, 0);
}

static void register_ersnto_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;

    module = harp_ingestion_register_module_coda("GOME_L2_ERSNTO", "GOME", "ACSAF", "ERSNTO",
                                                 "GOME near-real-time total column trace gas product", ingestion_init,
                                                 ingestion_done);
    register_common_options(module);

    /* ERSNTO product */
    product_definition = harp_ingestion_register_product(module, "GOME_L2_ERSNTO", NULL, read_dimensions);
    register_common_variables(product_definition);
    register_scan_variables(product_definition, 1);
}

static void register_ersoto_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;

    module = harp_ingestion_register_module_coda("GOME_L2_ERSOTO", "GOME", "ACSAF", "ERSOTO",
                                                 "GOME offline total column trace gas product", ingestion_init,
                                                 ingestion_done);
    register_common_options(module);

    /* ERSOTO product */
    product_definition = harp_ingestion_register_product(module, "GOME_L2_ERSOTO", NULL, read_dimensions);
    register_common_variables(product_definition);
    register_scan_variables(product_definition, 1);
}


int harp_ingestion_module_gome2_l2_init(void)
{
    register_o3mnto_product();
    register_o3moto_product();
    register_ersnto_product();
    register_ersoto_product();

    return 0;
}
