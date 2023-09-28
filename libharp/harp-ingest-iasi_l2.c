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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct ingest_info_struct
{
    coda_product *product;
    int num_main;
    int num_mdr;
    coda_cursor *mdr_cursor;    /* pointers to earthshine records */
    int buffered_scan_id;       /* we buffer the calculated corner coordinates for each 2x2 scan */
    double corner_latitude[4 * 4];
    double corner_longitude[4 * 4];
} ingest_info;

static int init_mdr_cursor(ingest_info *info)
{
    coda_cursor cursor;
    long num_applicable_mdr;
    long num_mdr;
    long i;

    /* There are max 32 measurements per scan.
     * Even though the lowest integration time of a measurement is 93.75ms and the total scan takes 6s there will never
     * be 64 measurements in a scan! For measurements with 93.75ms integration time only half the measurements will
     * ever be stored and the associated ground pixel is half that of the 187.5ms ground pixel (with gaps for the other
     * half). The available 93.75ms measurement covers the latter half of a 187.5ms ground pixel.
     *
     * For GOME2 L1b records the scans and mdrs do not correspond 1-to-1. An MDR is slightly shifted with regard to a
     * scan in the sense that the last measurement of a scan can be found as first measurement in the next MDR.
     */

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&cursor, "MDR") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &num_mdr) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    if (num_mdr == 0)
    {
        /* no data */
        harp_set_error(HARP_ERROR_NO_DATA, NULL);
        return -1;
    }

    info->mdr_cursor = malloc(num_mdr * sizeof(coda_cursor));
    if (info->mdr_cursor == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_mdr * sizeof(coda_cursor), __FILE__, __LINE__);
        return -1;
    }

    num_applicable_mdr = 0;     /* we only count real MDRs (i.e. excluding Dummy records) with the appropriate data */
    if (coda_cursor_goto_first_array_element(&cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    for (i = 0; i < num_mdr; i++)
    {
        int is_mdr;

        if (coda_cursor_get_record_field_available_status(&cursor, 0, &is_mdr) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (is_mdr)
        {
            if (coda_cursor_goto_record_field_by_index(&cursor, 0) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            info->mdr_cursor[num_applicable_mdr] = cursor;
            coda_cursor_goto_parent(&cursor);
            num_applicable_mdr++;
        }
        if (i < num_mdr - 1)
        {
            if (coda_cursor_goto_next_array_element(&cursor) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
        }
    }

    if (num_applicable_mdr == 0)
    {
        /* no data */
        harp_set_error(HARP_ERROR_NO_DATA, NULL);
        return -1;
    }

    info->num_mdr = num_applicable_mdr;
    /* there are 120 measurements per MDR */
    info->num_main = 120 * num_applicable_mdr;

    return 0;
}

static int read_dimensions(void *user_data, long dimension[HARP_NUM_DIM_TYPES])
{
    ingest_info *info = (ingest_info *)user_data;

    dimension[harp_dimension_time] = info->num_main;

    return 0;
}

static int read_time(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    coda_cursor cursor;

    cursor = info->mdr_cursor[index / 120];

    if (coda_cursor_goto(&cursor, "RECORD_HEADER/RECORD_START_TIME") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_double(&cursor, data.double_data) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    *data.double_data += +((int)((index % 120) / 4)) * 8 / 37;

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
    if (coda_cursor_goto(&cursor, "/MPHR/ORBIT_START") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    /* we read the data as uint32, but since it is only 5 ascii digits, we can directly cast it to int32 */
    if (coda_cursor_read_uint32(&cursor, (uint32_t *)data.int32_data) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    return 0;
}

static int read_latitude(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    coda_cursor cursor;

    cursor = info->mdr_cursor[index / 120];
    if (coda_cursor_goto_record_field_by_name(&cursor, "EARTH_LOCATION") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    /* flat index in [120,2] array */
    if (coda_cursor_goto_array_element_by_index(&cursor, (index % 120) * 2) != 0)
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

static int read_longitude(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    coda_cursor cursor;

    cursor = info->mdr_cursor[index / 120];
    if (coda_cursor_goto_record_field_by_name(&cursor, "EARTH_LOCATION") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    /* flat index in [120,2] array */
    if (coda_cursor_goto_array_element_by_index(&cursor, (index % 120) * 2 + 1) != 0)
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

static int get_corner_coordinates(ingest_info *info, long scan_id)
{
    double latlong[4 * 2];
    double center_latitude;
    double center_longitude;
    double outer_latitude[4];
    double outer_longitude[4];
    coda_cursor cursor;
    int i;

    cursor = info->mdr_cursor[scan_id / 30];
    if (coda_cursor_goto_record_field_by_name(&cursor, "EARTH_LOCATION") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    /* read 4 lat/long pairs (using flat index) from [120,2] array */
    if (coda_cursor_goto_array_element_by_index(&cursor, (scan_id % 30) * 4 * 2) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    for (i = 0; i < 8; i++)
    {
        if (coda_cursor_read_double(&cursor, &latlong[i]) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (i < 8 - 1)
        {
            if (coda_cursor_goto_next_array_element(&cursor) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
        }
    }

    /* The 2x2 elements in a scan are stored in the product in the order:
     *  - bottom right
     *  - top right
     *  - top left
     *  - bottom left
     * The scans within a scan line go from left to right with increasing time.
     * The bottom is defined as 'first in flight direction' and the top as 'last in flight direction'.
     */

    /* calculate the center point of the scan */
    harp_geographic_intersection(latlong[6], latlong[7], latlong[2], latlong[3], latlong[0], latlong[1], latlong[4],
                                 latlong[5], &center_latitude, &center_longitude);

    /* extrapolate the center point outwards to each of the four corners
     * i.e. the outer latitude/longitude points are twice as far from the center point as the mid points of the four
     * elements.
     */
    harp_geographic_extrapolation(latlong[0], latlong[1], center_latitude, center_longitude,
                                  &(outer_latitude[0]), &(outer_longitude[0]));
    harp_geographic_extrapolation(latlong[2], latlong[3], center_latitude, center_longitude,
                                  &(outer_latitude[1]), &(outer_longitude[1]));
    harp_geographic_extrapolation(latlong[4], latlong[5], center_latitude, center_longitude,
                                  &(outer_latitude[2]), &(outer_longitude[2]));
    harp_geographic_extrapolation(latlong[6], latlong[7], center_latitude, center_longitude,
                                  &(outer_latitude[3]), &(outer_longitude[3]));

    /* the inner corner coordinate (i.e. the one nearest to the center point of the scan) for each of the elements
     * is chosen as the interpolation between the center point of the opposite element and the outer point of the
     * current element:
     *
     *  outer_2
     *     \
     *  outer_corner_2
     *        \
     *      center_2
     *          \
     *       inner_corner_2
     *             \
     *          center_scan
     *                \
     *             inner_corner_0
     *                   \
     *                  center_0
     *                      \
     *                  outer_corner_0
     *                         \
     *                        outer_0
     *
     * In this case inner_corner_0 is the interpolation of outer_0 and center_2 and inner_corner_2 is the interpolation
     * of outer_2 and center_0.
     * The distance (center_scan, inner_corner_element) will then be half the distance (center_scan, center_element)
     * and the distance (center_scan, outer_corner_element) will be 1.5 the distance (center_scan, center_element)
     */
    harp_geographic_average(outer_latitude[0], outer_longitude[0], latlong[4], latlong[5],
                            &info->corner_latitude[0 + 3], &info->corner_longitude[0 + 3]);
    harp_geographic_average(outer_latitude[1], outer_longitude[1], latlong[6], latlong[7],
                            &info->corner_latitude[4 + 0], &info->corner_longitude[4 + 0]);
    harp_geographic_average(outer_latitude[2], outer_longitude[2], latlong[0], latlong[1],
                            &info->corner_latitude[8 + 1], &info->corner_longitude[8 + 1]);
    harp_geographic_average(outer_latitude[3], outer_longitude[3], latlong[2], latlong[3],
                            &info->corner_latitude[12 + 2], &info->corner_longitude[12 + 2]);

    /* The outer corner coordinate is the interpolation of the outer coordinate of an element with its center
     * coordinate.
     */
    harp_geographic_average(outer_latitude[0], outer_longitude[0], latlong[0], latlong[1],
                            &info->corner_latitude[0 + 1], &info->corner_longitude[0 + 1]);
    harp_geographic_average(outer_latitude[1], outer_longitude[1], latlong[2], latlong[3],
                            &info->corner_latitude[4 + 2], &info->corner_longitude[4 + 2]);
    harp_geographic_average(outer_latitude[2], outer_longitude[2], latlong[4], latlong[5],
                            &info->corner_latitude[8 + 3], &info->corner_longitude[8 + 3]);
    harp_geographic_average(outer_latitude[3], outer_longitude[3], latlong[6], latlong[7],
                            &info->corner_latitude[12 + 0], &info->corner_longitude[12 + 0]);

    /* the other corner coordinates are calculated by finding the intersection of the greatcircle through two
     * innner corner coordinates and the greatcircle through two outer corner coordinates.
     * Mind that the 4 elements of a scan are ordered according to:
     *
     *   2 - 1
     *   |   |
     *   3 - 0
     *
     * while the corner coordinates of each element are ordered according to (using the first in time / first in flight
     * convention):
     *
     *   3 - 2
     *   |   |
     *   0 - 1
     *
     */
    harp_geographic_intersection(info->corner_latitude[12 + 2], info->corner_longitude[12 + 2],
                                 info->corner_latitude[0 + 3], info->corner_longitude[0 + 3],
                                 info->corner_latitude[0 + 1], info->corner_longitude[0 + 1],
                                 info->corner_latitude[4 + 2], info->corner_longitude[4 + 2],
                                 &info->corner_latitude[0 + 2], &info->corner_longitude[0 + 2]);
    harp_geographic_intersection(info->corner_latitude[12 + 0], info->corner_longitude[12 + 0],
                                 info->corner_latitude[0 + 1], info->corner_longitude[0 + 1],
                                 info->corner_latitude[0 + 3], info->corner_longitude[0 + 3],
                                 info->corner_latitude[4 + 0], info->corner_longitude[4 + 0],
                                 &info->corner_latitude[0 + 0], &info->corner_longitude[0 + 0]);
    harp_geographic_intersection(info->corner_latitude[0 + 3], info->corner_longitude[0 + 3],
                                 info->corner_latitude[4 + 0], info->corner_longitude[4 + 0],
                                 info->corner_latitude[4 + 2], info->corner_longitude[4 + 2],
                                 info->corner_latitude[8 + 3], info->corner_longitude[8 + 3],
                                 &info->corner_latitude[4 + 3], &info->corner_longitude[4 + 3]);
    harp_geographic_intersection(info->corner_latitude[0 + 1], info->corner_longitude[0 + 1],
                                 info->corner_latitude[4 + 2], info->corner_longitude[4 + 2],
                                 info->corner_latitude[4 + 0], info->corner_longitude[4 + 0],
                                 info->corner_latitude[8 + 1], info->corner_longitude[8 + 1],
                                 &info->corner_latitude[4 + 1], &info->corner_longitude[4 + 1]);
    harp_geographic_intersection(info->corner_latitude[4 + 0], info->corner_longitude[4 + 0],
                                 info->corner_latitude[8 + 1], info->corner_longitude[8 + 1],
                                 info->corner_latitude[8 + 3], info->corner_longitude[8 + 3],
                                 info->corner_latitude[12 + 0], info->corner_longitude[12 + 0],
                                 &info->corner_latitude[8 + 0], &info->corner_longitude[8 + 0]);
    harp_geographic_intersection(info->corner_latitude[4 + 2], info->corner_longitude[4 + 2],
                                 info->corner_latitude[8 + 3], info->corner_longitude[8 + 3],
                                 info->corner_latitude[8 + 1], info->corner_longitude[8 + 1],
                                 info->corner_latitude[12 + 2], info->corner_longitude[12 + 2],
                                 &info->corner_latitude[8 + 2], &info->corner_longitude[8 + 2]);
    harp_geographic_intersection(info->corner_latitude[8 + 1], info->corner_longitude[8 + 1],
                                 info->corner_latitude[12 + 2], info->corner_longitude[12 + 2],
                                 info->corner_latitude[12 + 0], info->corner_longitude[12 + 0],
                                 info->corner_latitude[0 + 1], info->corner_longitude[0 + 1],
                                 &info->corner_latitude[12 + 1], &info->corner_longitude[12 + 1]);
    harp_geographic_intersection(info->corner_latitude[8 + 3], info->corner_longitude[8 + 3],
                                 info->corner_latitude[12 + 0], info->corner_longitude[12 + 0],
                                 info->corner_latitude[12 + 2], info->corner_longitude[12 + 2],
                                 info->corner_latitude[0 + 3], info->corner_longitude[0 + 3],
                                 &info->corner_latitude[12 + 3], &info->corner_longitude[12 + 3]);

    return 0;
}

static int read_corner_latitude(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long scan_id;

    scan_id = index / 4;
    if (info->buffered_scan_id != scan_id)
    {
        if (get_corner_coordinates(info, scan_id) != 0)
        {
            return -1;
        }
        info->buffered_scan_id = scan_id;
    }

    data.double_data[0] = info->corner_latitude[(index % 4) * 4 + 0];
    data.double_data[1] = info->corner_latitude[(index % 4) * 4 + 1];
    data.double_data[2] = info->corner_latitude[(index % 4) * 4 + 2];
    data.double_data[3] = info->corner_latitude[(index % 4) * 4 + 3];

    return 0;
}

static int read_corner_longitude(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long scan_id;

    scan_id = index / 4;
    if (info->buffered_scan_id != scan_id)
    {
        if (get_corner_coordinates(info, scan_id) != 0)
        {
            return -1;
        }
        info->buffered_scan_id = scan_id;
    }

    data.double_data[0] = info->corner_longitude[(index % 4) * 4 + 0];
    data.double_data[1] = info->corner_longitude[(index % 4) * 4 + 1];
    data.double_data[2] = info->corner_longitude[(index % 4) * 4 + 2];
    data.double_data[3] = info->corner_longitude[(index % 4) * 4 + 3];

    return 0;
}

static int get_angle_data(void *user_data, long index, int angle_id, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    coda_cursor cursor;

    cursor = info->mdr_cursor[index / 120];
    if (coda_cursor_goto_record_field_by_name(&cursor, "ANGULAR_RELATION") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    /* flat index in [120,4] array */
    if (coda_cursor_goto_array_element_by_index(&cursor, (index % 120) * 4 + angle_id) != 0)
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

static int read_solar_zenith_angle(void *user_data, long index, harp_array data)
{
    return get_angle_data(user_data, index, 0, data);
}

static int read_sensor_zenith_angle(void *user_data, long index, harp_array data)
{
    return get_angle_data(user_data, index, 1, data);
}

static int read_solar_azimuth_angle(void *user_data, long index, harp_array data)
{
    return get_angle_data(user_data, index, 2, data);
}

static int read_sensor_azimuth_angle(void *user_data, long index, harp_array data)
{
    return get_angle_data(user_data, index, 3, data);
}

static int get_species_data(void *user_data, long index, const char *fieldname, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    coda_cursor cursor;

    cursor = info->mdr_cursor[index / 120];
    if (coda_cursor_goto_record_field_by_name(&cursor, fieldname) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_array_element_by_index(&cursor, (index % 120)) != 0)
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

static int read_o3_column(void *user_data, long index, harp_array data)
{
    return get_species_data(user_data, index, "INTEGRATED_OZONE", data);
}

static int read_n2o_column(void *user_data, long index, harp_array data)
{
    return get_species_data(user_data, index, "INTEGRATED_N2O", data);
}

static int read_co_column(void *user_data, long index, harp_array data)
{
    return get_species_data(user_data, index, "INTEGRATED_CO", data);
}

static int read_ch4_column(void *user_data, long index, harp_array data)
{
    return get_species_data(user_data, index, "INTEGRATED_CH4", data);
}

static int read_co2_column(void *user_data, long index, harp_array data)
{
    return get_species_data(user_data, index, "INTEGRATED_CO2", data);
}

static int read_scan_subindex(void *user_data, long index, harp_array data)
{
    (void)user_data;
    *data.int8_data = (int8_t)(index % 120);

    return 0;
}

static void ingestion_done(void *user_data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->mdr_cursor != NULL)
    {
        free(info->mdr_cursor);
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
    info->mdr_cursor = NULL;
    info->buffered_scan_id = -1;

    if (init_mdr_cursor(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }

    *definition = *module->product_definition;
    *user_data = info;

    return 0;
}

int harp_ingestion_module_iasi_l2_init(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };
    harp_dimension_type dimension_type_bounds[2] = { harp_dimension_time, harp_dimension_independent };
    long dimension_bounds[2] = { -1, 4 };
    const char *description;
    const char *path;

    module = harp_ingestion_register_module("IASI_L2", "IASI", "EPS", "IASI_SND_02", "IASI L2 total column densities",
                                            ingestion_init, ingestion_done);
    product_definition =
        harp_ingestion_register_product(module, "IASI_L2", "IASI L2 total column densities", read_dimensions);

    /* datetime */
    description = "The time of the measurement at end of integration time";
    variable_definition =
        harp_ingestion_register_variable_block_read(product_definition, "datetime", harp_type_double, 1,
                                                    dimension_type, NULL, description, "seconds since 2000-01-01",
                                                    NULL, read_time);
    path = "/MDR[]/MDR/RECORD_HEADER/RECORD_START_TIME";
    description = "The time for a scan is the MDR start time + the scan id (0..29) times 8 / 37. Each part of the 2x2 "
        "matrix of a scan will get assigned the same measurement time (i.e. there are 30 unique time values "
        "per mdr)";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* orbit_index */
    description = "absolute orbit number";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "orbit_index", harp_type_int32, 0, NULL, NULL,
                                                   description, NULL, NULL, read_orbit_index);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/MPHR/ORBIT_START", NULL);

    /* longitude */
    description = "center longitude of the measurement";
    variable_definition =
        harp_ingestion_register_variable_block_read(product_definition, "longitude", harp_type_double, 1,
                                                    dimension_type, NULL, description, "degree_east", NULL,
                                                    read_longitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0, 180.0);
    path = "/MDR[]/MDR/EARTH_LOCATION[,1]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* latitude */
    description = "center latitude of the measurement";
    variable_definition =
        harp_ingestion_register_variable_block_read(product_definition, "latitude", harp_type_double, 1,
                                                    dimension_type, NULL, description, "degree_north", NULL,
                                                    read_latitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    path = "/MDR[]/MDR/EARTH_LOCATION[,0]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* longitude_bounds */
    description = "corner longitudes of the measurement";
    variable_definition =
        harp_ingestion_register_variable_block_read(product_definition, "longitude_bounds", harp_type_double, 2,
                                                    dimension_type_bounds, dimension_bounds, description,
                                                    "degree_east", NULL, read_corner_longitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0, 180.0);
    path = "/MDR[]/MDR/EARTH_LOCATION[]";
    description = "the corner coordinates are rough estimates of the circle areas for the scan elements; the size of "
        "a scan element (in a certain direction) is taken to be half the distance, from center to center, "
        "from a scan element to its nearest neighboring scan element";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* latitude_bounds */
    description = "corner latitudes of the measurement";
    variable_definition =
        harp_ingestion_register_variable_block_read(product_definition, "latitude_bounds", harp_type_double, 2,
                                                    dimension_type_bounds, dimension_bounds, description,
                                                    "degree_north", NULL, read_corner_latitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    path = "/MDR[]/MDR/EARTH_LOCATION[]";
    description = "the corner coordinates are rough estimates of the circle areas for the scan elements; the size of "
        "a scan element (in a certain direction) is taken to be half the distance, from center to center, "
        "from a scan element to its nearest neighboring scan element";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* solar_azimuth_angle */
    description = "solar azimuth angle at the surface";
    variable_definition =
        harp_ingestion_register_variable_block_read(product_definition, "solar_azimuth_angle", harp_type_double, 1,
                                                    dimension_type, NULL, description, "degree", NULL,
                                                    read_solar_azimuth_angle);
    harp_variable_definition_set_valid_range_double(variable_definition, 0.0, 360.0);
    path = "/MDR[]/MDR/ANGULAR_RELATION[,2]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* solar_zenith_angle */
    description = "solar zenith angle at the surface";
    variable_definition =
        harp_ingestion_register_variable_block_read(product_definition, "solar_zenith_angle", harp_type_double,
                                                    1, dimension_type, NULL, description, "degree", NULL,
                                                    read_solar_zenith_angle);
    harp_variable_definition_set_valid_range_double(variable_definition, 0.0, 180.0);
    path = "/MDR[]/MDR/ANGULAR_RELATION[,0]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* sensor_azimuth_angle */
    description = "sensor azimuth angle at the surface";
    variable_definition =
        harp_ingestion_register_variable_block_read(product_definition, "sensor_azimuth_angle", harp_type_double, 1,
                                                    dimension_type, NULL, description, "degree", NULL,
                                                    read_sensor_azimuth_angle);
    harp_variable_definition_set_valid_range_double(variable_definition, 0.0, 360.0);
    path = "/MDR[]/MDR/ANGULAR_RELATION[,3]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* sensor_zenith_angle */
    description = "sensor zenith angle at the surface";
    variable_definition =
        harp_ingestion_register_variable_block_read(product_definition, "sensor_zenith_angle", harp_type_double, 1,
                                                    dimension_type, NULL, description, "degree", NULL,
                                                    read_sensor_zenith_angle);
    harp_variable_definition_set_valid_range_double(variable_definition, 0.0, 180.0);
    path = "/MDR[]/MDR/ANGULAR_RELATION[,1]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* CH4_column_density */
    description = "CH4 column mass density";
    variable_definition =
        harp_ingestion_register_variable_block_read(product_definition, "CH4_column_density", harp_type_double, 1,
                                                    dimension_type, NULL, description, "kg/m^2", NULL, read_ch4_column);
    path = "/MDR[]/MDR/INTEGRATED_CH4[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* CO_column_density */
    description = "CO column mass density";
    variable_definition =
        harp_ingestion_register_variable_block_read(product_definition, "CO_column_density", harp_type_double, 1,
                                                    dimension_type, NULL, description, "kg/m^2", NULL, read_co_column);
    path = "/MDR[]/MDR/INTEGRATED_CO[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* CO2_column_density */
    description = "CO2 column mass density";
    variable_definition =
        harp_ingestion_register_variable_block_read(product_definition, "CO2_column_density", harp_type_double, 1,
                                                    dimension_type, NULL, description, "kg/m^2", NULL, read_co2_column);
    path = "/MDR[]/MDR/INTEGRATED_CO2[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* O3_column_density */
    description = "O3 column mass density";
    variable_definition =
        harp_ingestion_register_variable_block_read(product_definition, "O3_column_density", harp_type_double, 1,
                                                    dimension_type, NULL, description, "kg/m^2", NULL, read_o3_column);
    path = "/MDR[]/MDR/INTEGRATED_OZONE[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* N2O_column_density */
    description = "N2O column mass density";
    variable_definition =
        harp_ingestion_register_variable_block_read(product_definition, "N2O_column_density", harp_type_double, 1,
                                                    dimension_type, NULL, description, "kg/m^2", NULL, read_n2o_column);
    path = "/MDR[]/MDR/INTEGRATED_N2O[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* scan_subindex */
    description = "the relative index (0-119) of this measurement within an MDR";
    variable_definition =
        harp_ingestion_register_variable_block_read(product_definition, "scan_subindex", harp_type_int8, 1,
                                                    dimension_type, NULL, description, NULL, NULL, read_scan_subindex);

    return 0;
}
