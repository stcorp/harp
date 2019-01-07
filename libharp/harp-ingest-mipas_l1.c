/*
 * Copyright (C) 2015-2019 S[&]T, The Netherlands.
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
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* ----- defines ----- */

#define NR_BANDS 5

#define CHECKED_MALLOC(v, s) v = malloc(s); if (v == NULL) { harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)", s, __FILE__, __LINE__); return -1;}

/* ----- typedefs ----- */

typedef struct ingest_info_struct
{
    coda_product *product;
    int format_version;
    short selected_band;

    /* Measured radiance fields */
    coda_cursor *mds_cursors;
    long num_mds_records;
    double first_wavenum[NR_BANDS];
    double last_wavenum[NR_BANDS];
    int measurements_in_band[NR_BANDS];
    long offset_in_band[NR_BANDS];
    long total_measurements_all_bands;

} ingest_info;

/* ----- global variables ----- */

static const char *band_name_in_file[] = { "band_a", "band_ab", "band_b", "band_c", "band_d" };
static const char *band_name_as_option[] = { "A", "AB", "B", "C", "D" };

/* ----- code ----- */

static void ingestion_done(void *user_data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->mds_cursors != NULL)
    {
        free(info->mds_cursors);
    }
    free(info);
}

static int get_main_data(ingest_info *info, const char *fieldname, double *double_data_array)
{
    coda_cursor cursor;
    double *double_data;
    long i;

    double_data = double_data_array;
    for (i = 0; i < info->num_mds_records; i++)
    {
        cursor = info->mds_cursors[i];
        if (coda_cursor_goto(&cursor, fieldname) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_read_double(&cursor, double_data) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        double_data++;
    }
    return 0;
}

static int read_datetime(void *user_data, harp_array data)
{
    return get_main_data((ingest_info *)user_data, "dsr_time", data.double_data);
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
    if (coda_cursor_goto(&cursor, "/mph/abs_orbit") != 0)
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

static int read_latitude(void *user_data, harp_array data)
{
    return get_main_data((ingest_info *)user_data, "loc_2/latitude", data.double_data);
}

static int read_longitude(void *user_data, harp_array data)
{
    return get_main_data((ingest_info *)user_data, "loc_2/longitude", data.double_data);
}

static int read_altitude(void *user_data, harp_array data)
{
    return get_main_data((ingest_info *)user_data, "loc_1[0]", data.double_data);
}

static int read_wavenumber_radiance(void *user_data, harp_array data)
{
    coda_cursor cursor;
    ingest_info *info = (ingest_info *)user_data;
    float *float_data;
    long i;
    short start_band, end_band, band_nr;

    float_data = data.float_data;
    if (info->selected_band >= 0)
    {
        start_band = end_band = info->selected_band;
    }
    else
    {
        start_band = 0;
        end_band = NR_BANDS - 1;
    }
    for (i = 0; i < info->num_mds_records; i++)
    {
        cursor = info->mds_cursors[i];
        for (band_nr = start_band; band_nr <= end_band; band_nr++)
        {
            if (coda_cursor_goto(&cursor, band_name_in_file[band_nr]) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            if (coda_cursor_read_float_array(&cursor, float_data, coda_array_ordering_c) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            float_data += info->measurements_in_band[band_nr];
            coda_cursor_goto_parent(&cursor);
        }
    }
    return 0;
}

static int read_wavenumber(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    double *double_data = data.double_data;
    long i, j;
    short start_band, end_band, band_nr;

    if (info->selected_band >= 0)
    {
        start_band = end_band = info->selected_band;
    }
    else
    {
        start_band = 0;
        end_band = NR_BANDS - 1;
    }
    for (i = 0; i < info->num_mds_records; i++)
    {
        for (band_nr = start_band; band_nr <= end_band; band_nr++)
        {
            for (j = 0; j < info->measurements_in_band[band_nr]; j++)
            {
                *double_data =
                    info->first_wavenum[band_nr] +
                    (j *
                     ((info->last_wavenum[band_nr] -
                       info->first_wavenum[band_nr]) / info->measurements_in_band[band_nr]));
                double_data++;
            }
        }
    }
    return 0;
}

static int read_dimensions(void *user_data, long dimension[HARP_NUM_DIM_TYPES])
{
    ingest_info *info = (ingest_info *)user_data;

    dimension[harp_dimension_time] = info->num_mds_records;
    dimension[harp_dimension_spectral] = info->total_measurements_all_bands;
    return 0;
}

static int init_dimensions(ingest_info *info)
{
    coda_cursor cursor;
    long i;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    /* Count the number of MDS records */
    if (coda_cursor_goto_record_field_by_name(&cursor, "mipas_level_1b_mds") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &info->num_mds_records) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    CHECKED_MALLOC(info->mds_cursors, info->num_mds_records * sizeof(coda_cursor));

    if (coda_cursor_goto_first_array_element(&cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    for (i = 0; i < info->num_mds_records; i++)
    {
        info->mds_cursors[i] = cursor;
        if (i < (info->num_mds_records - 1))
        {
            if (coda_cursor_goto_next_array_element(&cursor) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
        }
    }

    if (coda_cursor_goto(&cursor, "/sph/first_wavenum") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_double_array(&cursor, info->first_wavenum, coda_array_ordering_c) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, "/sph/last_wavenum") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_double_array(&cursor, info->last_wavenum, coda_array_ordering_c) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, "/sph/num_points_per_band") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_int32_array(&cursor, info->measurements_in_band, coda_array_ordering_c) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (info->selected_band >= 0)
    {
        info->offset_in_band[0] = 0L;
        info->total_measurements_all_bands = info->measurements_in_band[info->selected_band];
    }
    else
    {
        info->offset_in_band[0] = 0L;
        for (i = 1; i < NR_BANDS; i++)
        {
            info->offset_in_band[i] = info->offset_in_band[i - 1] + info->measurements_in_band[i - 1];
        }
        info->total_measurements_all_bands =
            info->offset_in_band[NR_BANDS - 1] + info->measurements_in_band[NR_BANDS - 1];
    }

    coda_cursor_goto_root(&cursor);

    return 0;
}

static int ingestion_init(const harp_ingestion_module *module, coda_product *product,
                          const harp_ingestion_options *options, harp_product_definition **definition, void **user_data)
{
    int format_version;
    ingest_info *info;
    const char *cp;
    short i;

    if (coda_get_product_version(product, &format_version) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    CHECKED_MALLOC(info, sizeof(ingest_info));
    memset(info, '\0', sizeof(ingest_info));
    info->product = product;
    info->format_version = format_version;

    info->selected_band = -1;
    if (harp_ingestion_options_has_option(options, "band"))
    {
        if (harp_ingestion_options_get_option(options, "band", &cp) == 0)
        {
            for (i = 0; i < NR_BANDS; i++)
            {
                if (strcmp(cp, band_name_as_option[i]) == 0)
                {
                    info->selected_band = i;
                    break;
                }
            }
        }
    }

    if (init_dimensions(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }
    *definition = module->product_definition[0];

    *user_data = info;

    return 0;
}

int harp_ingestion_module_mipas_l1_init(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2];
    const char *description;
    const char *path;

    description = "MIPAS Level 1 data";
    module = harp_ingestion_register_module_coda("MIPAS_L1", "MIPAS", "ENVISAT_MIPAS", "MIP_NL__1P",
                                                 description, ingestion_init, ingestion_done);

    harp_ingestion_register_option(module, "band", "only include data from the specified band ('A', 'AB', 'B', 'C',"
                                   " 'D'); by default data from all bands is retrieved", 5, band_name_as_option);

    description = "MIPAS Level 1 Spectra product";
    product_definition = harp_ingestion_register_product(module, "MIPAS_L1", description, read_dimensions);
    description = "MIPAS Level 1 Spectra";
    harp_product_definition_add_mapping(product_definition, description, NULL);

    dimension_type[0] = harp_dimension_time;
    dimension_type[1] = harp_dimension_spectral;

    /* time_of_the_measurement */
    description = "time of the measurement at the end of the integration time";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double, 1, dimension_type,
                                                   NULL, description, "seconds since 2000-01-01", NULL, read_datetime);
    path = "/mipas_level_1b_mds[]/dsr_time";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* orbit_index */
    description = "absolute orbit number";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "orbit_index", harp_type_int32, 0, NULL, NULL,
                                                   description, NULL, NULL, read_orbit_index);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/mph/abs_orbit", NULL);

    /* latitude_of_the_measurement */
    description = "tangent latitude of the measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_double, 1, dimension_type,
                                                   NULL, description, "degree_north", NULL, read_latitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    path = "/mipas_level_1b_mds[]/loc_2/latitude";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* longitude_of_the_measurement */
    description = "tangent longitude of the measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_double, 1, dimension_type,
                                                   NULL, description, "degree_east", NULL, read_longitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0, 180.0);
    path = "/mipas_level_1b_mds[]/loc_2/longitude";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* altitude_of_the_measurement */
    description = "tangent altitude of the measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "altitude", harp_type_double, 1, dimension_type,
                                                   NULL, description, "km", NULL, read_altitude);
    path = "/mipas_level_1b_mds[]/loc_1[0]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* wavenumber_radiance */
    description = "measured radiances";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "wavenumber_radiance", harp_type_double,
                                                   2, dimension_type, NULL, description, "W/cm^2/sr/cm",
                                                   NULL, read_wavenumber_radiance);
    path =
        "/mipas_level_1b_mds[]/band_a[], /mipas_level_1b_mds[]/band_ab[], /mipas_level_1b_mds[]/band_b[], /mipas_level_1b_mds[]/band_c[], /mipas_level_1b_mds[]/band_d[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* wavenumber */
    description = "nominal wavenumber assignment for each of the detector pixels";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "wavenumber", harp_type_double, 2,
                                                   dimension_type, NULL, description, "cm", NULL, read_wavenumber);
    path = "/sph/first_wavenum], /sph/last_wavenum[], /sph/num_points_per_band[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    return 0;
}
