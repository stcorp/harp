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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ------------------- Defines ------------------ */

#define FILL_VALUE_NO_DATA                   -999
#define FILL_VALUE_DATA_IS_ESTIMATE          -888

/* Conversion factor from atmosphere to hPa */
#define ATM_TO_HPA                        1013.25

/* conversion factor from ppv (parts per volume) to ppmv (parts per million volume) */
#define PPV_TO_PPMV                         1.0E6

#define CHECKED_MALLOC(v, s) v = malloc(s); if (v == NULL) { harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)", s, __FILE__, __LINE__); return -1;}

/* ------------------ Typedefs ------------------ */

typedef struct ingest_info_struct
{
    coda_product *product;
    long num_altitudes;
    short num_data_fields;
    char **field_names;
    double *field_values;
} ingest_info;

/* -------------- Global variables --------------- */

static double nan;

/* -------------------- Code -------------------- */

static void ingestion_done(void *user_data)
{
    ingest_info *info = (ingest_info *)user_data;
    short i;

    if (info != NULL)
    {
        if (info->field_names != NULL)
        {
            for (i = 0; i < info->num_data_fields; i++)
            {
                if ((info->field_names)[i] != NULL)
                {
                    free((info->field_names)[i]);
                }
            }
            free(info->field_names);
        }
        if (info->field_values != NULL)
        {
            free(info->field_values);
        }
        free(info);
    }
}

/* General read functions */

static int read_scalar_variable(ingest_info *info, const char *name, harp_array data)
{
    coda_cursor cursor;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&cursor, name) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_double(&cursor, data.double_data) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    /* filter for NaN */
    if (data.double_data[0] == FILL_VALUE_NO_DATA)
    {
        data.double_data[0] = nan;
    }

    return 0;
}

static int get_field_nr(ingest_info *info, const char *field_name)
{
    long field_nr;

    for (field_nr = 0; field_nr < info->num_data_fields; field_nr++)
    {
        if (strcmp(info->field_names[field_nr], field_name) == 0)
        {
            break;
        }
    }
    if (field_nr >= info->num_data_fields)
    {
        harp_set_error(HARP_ERROR_INGESTION, "field %s not found", field_name);
        return -1;
    }
    return field_nr;
}

static int read_data_field(void *user_data, const char *field_name, double scaling_factor, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    double value;
    long line_nr, field_nr;

    if ((field_nr = get_field_nr(info, field_name)) < 0)
    {
        return -1;
    }
    for (line_nr = 0; line_nr < info->num_altitudes; line_nr++)
    {
        value = info->field_values[line_nr * info->num_data_fields + field_nr];
        /* filter for NaN */
        if (value == FILL_VALUE_NO_DATA)
        {
            data.double_data[line_nr] = nan;
        }
        else if (value == FILL_VALUE_DATA_IS_ESTIMATE)
        {
            data.double_data[line_nr] = value;
        }
        else
        {
            data.double_data[line_nr] = value * scaling_factor;
        }

    }
    return 0;
}

/* Specific read functions */

static int read_string_from_header(ingest_info *info, char *name, char *expected_format, harp_array data)
{
    coda_cursor cursor;
    double datetime_in_seconds;
    char datetime_str[81];

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&cursor, name) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_string(&cursor, datetime_str, sizeof(datetime_str)) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_time_string_to_double(expected_format, datetime_str, &datetime_in_seconds) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    data.double_data[0] = datetime_in_seconds;
    return 0;
}

static int read_datetime(void *user_data, harp_array data)
{
    return read_string_from_header((ingest_info *)user_data, "date",
                                   "yyyy-MM-dd HH:mm:ss.SS+00|yyyy-MM-dd HH:mm:ss.SSSSSS+00:00", data);
}

static int read_datetime_start(void *user_data, harp_array data)
{
    return read_string_from_header((ingest_info *)user_data, "start_time",
                                   "yyyy-MM-dd HH:mm:ss+00|yyyy-MM-dd HH:mm:ss+00:00", data);
}

static int read_datetime_stop(void *user_data, harp_array data)
{
    return read_string_from_header((ingest_info *)user_data, "end_time",
                                   "yyyy-MM-dd HH:mm:ss+00|yyyy-MM-dd HH:mm:ss+00:00", data);
}

static int read_latitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long i;

    if (read_scalar_variable(info, "latitude", data) != 0)
    {
        return -1;
    }
    for (i = 1; i < info->num_altitudes; i++)
    {
        data.double_data[i] = data.double_data[0];
    }
    return 0;
}

static int read_orbit_index(void *user_data, harp_array data)
{
    const char *filename;

    if (coda_get_product_filename(((ingest_info *)user_data)->product, &filename) != 0)
    {
        return -1;
    }
    filename = harp_basename(filename);
    data.int32_data[0] = (int32_t)strtol(&filename[2], NULL, 10);

    return 0;
}

static int read_longitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long i;

    if (read_scalar_variable(info, "longitude", data) != 0)
    {
        return -1;
    }
    for (i = 1; i < info->num_altitudes; i++)
    {
        data.double_data[i] = data.double_data[0];
    }
    return 0;
}

static int read_altitude(void *user_data, harp_array data)
{
    return read_data_field(user_data, "z", 1.0, data);
}

static int read_temperature(void *user_data, harp_array data)
{
    return read_data_field(user_data, "T", 1.0, data);
}

static int read_pressure(void *user_data, harp_array data)
{
    return read_data_field(user_data, "P (atm)", ATM_TO_HPA, data);
}

static int read_density(void *user_data, harp_array data)
{
    return read_data_field(user_data, "dens", 1.0, data);
}

static int read_h2o_vmr(void *user_data, harp_array data)
{
    return read_data_field(user_data, "H2O", PPV_TO_PPMV, data);
}

static int read_h2o_vmr_uncertainty(void *user_data, harp_array data)
{
    return read_data_field(user_data, "H2O_err", PPV_TO_PPMV, data);
}

static int read_o3_vmr(void *user_data, harp_array data)
{
    return read_data_field(user_data, "O3", PPV_TO_PPMV, data);
}

static int read_o3_vmr_uncertainty(void *user_data, harp_array data)
{
    return read_data_field(user_data, "O3_err", PPV_TO_PPMV, data);
}

static int read_n2o_vmr(void *user_data, harp_array data)
{
    return read_data_field(user_data, "N2O", PPV_TO_PPMV, data);
}

static int read_n2o_vmr_uncertainty(void *user_data, harp_array data)
{
    return read_data_field(user_data, "N2O_err", PPV_TO_PPMV, data);
}

static int read_co_vmr(void *user_data, harp_array data)
{
    return read_data_field(user_data, "CO", PPV_TO_PPMV, data);
}

static int read_co_vmr_uncertainty(void *user_data, harp_array data)
{
    return read_data_field(user_data, "CO_err", PPV_TO_PPMV, data);
}

static int read_ch4_vmr(void *user_data, harp_array data)
{
    return read_data_field(user_data, "CH4", PPV_TO_PPMV, data);
}

static int read_ch4_vmr_uncertainty(void *user_data, harp_array data)
{
    return read_data_field(user_data, "CH4_err", PPV_TO_PPMV, data);
}

static int read_no_vmr(void *user_data, harp_array data)
{
    return read_data_field(user_data, "NO", PPV_TO_PPMV, data);
}

static int read_no_vmr_uncertainty(void *user_data, harp_array data)
{
    return read_data_field(user_data, "NO_err", PPV_TO_PPMV, data);
}

static int read_no2_vmr(void *user_data, harp_array data)
{
    return read_data_field(user_data, "NO2", PPV_TO_PPMV, data);
}

static int read_no2_vmr_uncertainty(void *user_data, harp_array data)
{
    return read_data_field(user_data, "NO2_err", PPV_TO_PPMV, data);
}

static int read_hno3_vmr(void *user_data, harp_array data)
{
    return read_data_field(user_data, "HNO3", PPV_TO_PPMV, data);
}

static int read_hno3_vmr_uncertainty(void *user_data, harp_array data)
{
    return read_data_field(user_data, "HNO3_err", PPV_TO_PPMV, data);
}

static int read_hf_vmr(void *user_data, harp_array data)
{
    return read_data_field(user_data, "HF", PPV_TO_PPMV, data);
}

static int read_hf_vmr_uncertainty(void *user_data, harp_array data)
{
    return read_data_field(user_data, "HF_err", PPV_TO_PPMV, data);
}

static int read_hcl_vmr(void *user_data, harp_array data)
{
    return read_data_field(user_data, "HCl", PPV_TO_PPMV, data);
}

static int read_hcl_vmr_uncertainty(void *user_data, harp_array data)
{
    return read_data_field(user_data, "HCl_err", PPV_TO_PPMV, data);
}

static int read_ocs_vmr(void *user_data, harp_array data)
{
    return read_data_field(user_data, "OCS", PPV_TO_PPMV, data);
}

static int read_ocs_vmr_uncertainty(void *user_data, harp_array data)
{
    return read_data_field(user_data, "OCS_err", PPV_TO_PPMV, data);
}

static int read_n2o5_vmr(void *user_data, harp_array data)
{
    return read_data_field(user_data, "N2O5", PPV_TO_PPMV, data);
}

static int read_n2o5_vmr_uncertainty(void *user_data, harp_array data)
{
    return read_data_field(user_data, "N2O5_err", PPV_TO_PPMV, data);
}

static int read_clono2_vmr(void *user_data, harp_array data)
{
    return read_data_field(user_data, "ClONO2", PPV_TO_PPMV, data);
}

static int read_clono2_vmr_uncertainty(void *user_data, harp_array data)
{
    return read_data_field(user_data, "ClONO2_err", PPV_TO_PPMV, data);
}

static int read_hcn_vmr(void *user_data, harp_array data)
{
    return read_data_field(user_data, "HCN", PPV_TO_PPMV, data);
}

static int read_hcn_vmr_uncertainty(void *user_data, harp_array data)
{
    return read_data_field(user_data, "HCN_err", PPV_TO_PPMV, data);
}

static int read_ch3cl_vmr(void *user_data, harp_array data)
{
    return read_data_field(user_data, "CH3Cl", PPV_TO_PPMV, data);
}

static int read_ch3cl_vmr_uncertainty(void *user_data, harp_array data)
{
    return read_data_field(user_data, "CH3Cl_err", PPV_TO_PPMV, data);
}

static int read_cf4_vmr(void *user_data, harp_array data)
{
    return read_data_field(user_data, "CF4", PPV_TO_PPMV, data);
}

static int read_cf4_vmr_uncertainty(void *user_data, harp_array data)
{
    return read_data_field(user_data, "CF4_err", PPV_TO_PPMV, data);
}

static int read_ccl2f2_vmr(void *user_data, harp_array data)
{
    return read_data_field(user_data, "CCl2F2", PPV_TO_PPMV, data);
}

static int read_ccl2f2_vmr_uncertainty(void *user_data, harp_array data)
{
    return read_data_field(user_data, "CCl2F2_err", PPV_TO_PPMV, data);
}

static int read_ccl3f_vmr(void *user_data, harp_array data)
{
    return read_data_field(user_data, "CCl3F", PPV_TO_PPMV, data);
}

static int read_ccl3f_vmr_uncertainty(void *user_data, harp_array data)
{
    return read_data_field(user_data, "CCl3F_err", PPV_TO_PPMV, data);
}

static int read_cof2_vmr(void *user_data, harp_array data)
{
    return read_data_field(user_data, "COF2", PPV_TO_PPMV, data);
}

static int read_cof2_vmr_uncertainty(void *user_data, harp_array data)
{
    return read_data_field(user_data, "COF2_err", PPV_TO_PPMV, data);
}

static int read_c2h6_vmr(void *user_data, harp_array data)
{
    return read_data_field(user_data, "C2H6", PPV_TO_PPMV, data);
}

static int read_c2h6_vmr_uncertainty(void *user_data, harp_array data)
{
    return read_data_field(user_data, "C2H6_err", PPV_TO_PPMV, data);
}

static int read_c2h2_vmr(void *user_data, harp_array data)
{
    return read_data_field(user_data, "C2H2", PPV_TO_PPMV, data);
}

static int read_c2h2_vmr_uncertainty(void *user_data, harp_array data)
{
    return read_data_field(user_data, "C2H2_err", PPV_TO_PPMV, data);
}

static int read_chf2cl_vmr(void *user_data, harp_array data)
{
    return read_data_field(user_data, "CHF2Cl", PPV_TO_PPMV, data);
}

static int read_chf2cl_vmr_uncertainty(void *user_data, harp_array data)
{
    return read_data_field(user_data, "CHF2Cl_err", PPV_TO_PPMV, data);
}

static int read_sf6_vmr(void *user_data, harp_array data)
{
    return read_data_field(user_data, "SF6", PPV_TO_PPMV, data);
}

static int read_sf6_vmr_uncertainty(void *user_data, harp_array data)
{
    return read_data_field(user_data, "SF6_err", PPV_TO_PPMV, data);
}

static int read_clo_vmr(void *user_data, harp_array data)
{
    return read_data_field(user_data, "ClO", PPV_TO_PPMV, data);
}

static int read_clo_vmr_uncertainty(void *user_data, harp_array data)
{
    return read_data_field(user_data, "ClO_err", PPV_TO_PPMV, data);
}

static int read_ho2no2_vmr(void *user_data, harp_array data)
{
    return read_data_field(user_data, "HO2NO2", PPV_TO_PPMV, data);
}

static int read_ho2no2_vmr_uncertainty(void *user_data, harp_array data)
{
    return read_data_field(user_data, "HO2NO2_err", PPV_TO_PPMV, data);
}

static int read_h2o2_vmr(void *user_data, harp_array data)
{
    return read_data_field(user_data, "H2O2", PPV_TO_PPMV, data);
}

static int read_h2o2_vmr_uncertainty(void *user_data, harp_array data)
{
    return read_data_field(user_data, "H2O2_err", PPV_TO_PPMV, data);
}

static int read_hocl_vmr(void *user_data, harp_array data)
{
    return read_data_field(user_data, "HOCl", PPV_TO_PPMV, data);
}

static int read_hocl_vmr_uncertainty(void *user_data, harp_array data)
{
    return read_data_field(user_data, "HOCl_err", PPV_TO_PPMV, data);
}

static int read_n2_vmr(void *user_data, harp_array data)
{
    return read_data_field(user_data, "N2", PPV_TO_PPMV, data);
}

static int read_n2_vmr_uncertainty(void *user_data, harp_array data)
{
    return read_data_field(user_data, "N2_err", PPV_TO_PPMV, data);
}

static int read_h2o_181_vmr(void *user_data, harp_array data)
{
    return read_data_field(user_data, "H2O (181)", PPV_TO_PPMV, data);
}

static int read_h2o_181_vmr_uncertainty(void *user_data, harp_array data)
{
    return read_data_field(user_data, "181_err", PPV_TO_PPMV, data);
}

static int read_h2o_171_vmr(void *user_data, harp_array data)
{
    return read_data_field(user_data, "H2O (171)", PPV_TO_PPMV, data);
}

static int read_h2o_171_vmr_uncertainty(void *user_data, harp_array data)
{
    return read_data_field(user_data, "171_err", PPV_TO_PPMV, data);
}

static int read_h2o_162_vmr(void *user_data, harp_array data)
{
    return read_data_field(user_data, "H2O (162)", PPV_TO_PPMV, data);
}

static int read_h2o_162_vmr_uncertainty(void *user_data, harp_array data)
{
    return read_data_field(user_data, "162_err", PPV_TO_PPMV, data);
}

static int read_ch4_311_vmr(void *user_data, harp_array data)
{
    return read_data_field(user_data, "CH4 (311)", PPV_TO_PPMV, data);
}

static int read_ch4_311_vmr_uncertainty(void *user_data, harp_array data)
{
    return read_data_field(user_data, "311_err", PPV_TO_PPMV, data);
}

static int read_ch4_212_vmr(void *user_data, harp_array data)
{
    return read_data_field(user_data, "CH4 (212)", PPV_TO_PPMV, data);
}

static int read_ch4_212_vmr_uncertainty(void *user_data, harp_array data)
{
    return read_data_field(user_data, "212_err", PPV_TO_PPMV, data);
}

static int read_dimensions(void *user_data, long dimension[HARP_NUM_DIM_TYPES])
{
    ingest_info *info = (ingest_info *)user_data;

    dimension[harp_dimension_time] = 1;
    dimension[harp_dimension_vertical] = info->num_altitudes;

    return 0;
}

static int read_field_names(ingest_info *info, char *field_name_str)
{
    char *s;
    int bytes_read, field_nr;
    char name[81], name_plus_extra[81];

    info->num_data_fields = 0;
    s = field_name_str;
    while (sscanf(s, " %s%n", name, &bytes_read) > 0)
    {
        if (name[0] != '(')
        {
            info->num_data_fields++;
        }
        s += bytes_read;
    }
    CHECKED_MALLOC(info->field_names, info->num_data_fields * sizeof(char *));

    s = field_name_str;
    field_nr = 0;
    while (sscanf(s, " %s%n", name, &bytes_read) > 0)
    {
        if (name[0] != '(')
        {
            info->field_names[field_nr] = strdup(name);
            field_nr++;
        }
        else
        {
            snprintf(name_plus_extra, 81, "%s %s", info->field_names[field_nr - 1], name);
            free(info->field_names[field_nr - 1]);
            info->field_names[field_nr - 1] = strdup(name_plus_extra);
        }
        s += bytes_read;
    }
    return 0;
}

static int read_field_values(ingest_info *info, coda_cursor *cursor)
{
    char *cp;
    double value;
    long line_nr, length;
    int bytes_read, field_nr;
    char line[2048];

    if (coda_cursor_goto_first_array_element(cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    CHECKED_MALLOC(info->field_values, info->num_altitudes * info->num_data_fields * sizeof(double));
    for (line_nr = 0; line_nr < info->num_altitudes; line_nr++)
    {
        if (coda_cursor_goto_record_field_by_index(cursor, 0) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_get_string_length(cursor, &length) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (length >= 2047)
        {
            harp_set_error(HARP_ERROR_INGESTION, "line too long (%ld)", length);
            return -1;
        }
        if (coda_cursor_read_string(cursor, line, length + 1) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        coda_cursor_goto_parent(cursor);

        cp = line;
        field_nr = 0;
        while (sscanf(cp, " %lf%n", &value, &bytes_read) > 0)
        {
            cp += bytes_read;
            info->field_values[line_nr * info->num_data_fields + field_nr] = value;
            field_nr++;
        }
        if (line_nr < info->num_altitudes - 1)
        {
            if (coda_cursor_goto_next_array_element(cursor) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }

        }
    }
    return 0;
}

static int get_dimensions(ingest_info *info)
{
    coda_cursor cursor;
    long coda_dimension[CODA_MAX_NUM_DIMS];
    int num_coda_dimensions;
    char line[2048];

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, "field_names") != 0)
    {
        /* This productfile does not contain data */
        info->num_altitudes = 0;
        return 0;
    }
    if (coda_cursor_read_string(&cursor, line, sizeof(line)) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (read_field_names(info, line) != 0)
    {
        return -1;
    }

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, "field_data") != 0)
    {
        /* This productfile does not contain data */
        info->num_altitudes = 0;
        return 0;
    }
    if (coda_cursor_get_array_dim(&cursor, &num_coda_dimensions, coda_dimension) != 0)
    {
        /* This productfile does not contain data */
        info->num_altitudes = 0;
        return 0;
    }
    info->num_altitudes = coda_dimension[0];
    if (read_field_values(info, &cursor) != 0)
    {
        return -1;
    }

    return 0;
}

static int ingestion_init(const harp_ingestion_module *module, coda_product *product,
                          const harp_ingestion_options *options, harp_product_definition **definition, void **user_data)
{
    ingest_info *info;

    (void)options;

    nan = coda_NaN();
    info = malloc(sizeof(ingest_info));
    if (info == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(ingest_info), __FILE__, __LINE__);
        return -1;
    }
    memset(info, '\0', sizeof(ingest_info));
    info->product = product;

    if (get_dimensions(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }

    *definition = *module->product_definition;
    *user_data = info;

    return 0;
}

static void register_general_fields(harp_product_definition *product_definition)
{
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_vertical };
    harp_dimension_type datetime_dimension_type[1] = { harp_dimension_time };
    const char *description;
    const char *path;

    /* datetime */
    description = "date and time of occultation 30 km geometric tangent point";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double, 1,
                                                   datetime_dimension_type, NULL, description,
                                                   "seconds since 2000-01-01", NULL, read_datetime);
    path = "/date";
    description = "date field from header section converted to seconds since 2000-01-01";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* datetime_start */
    description = "date and time of start of measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime_start", harp_type_double, 1,
                                                   datetime_dimension_type, NULL, description,
                                                   "seconds since 2000-01-01", NULL, read_datetime_start);
    path = "/start_time";
    description = "start_time field from header section converted to seconds since 2000-01-01";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* datetime_stop */
    description = "date and time of end of measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime_stop", harp_type_double, 1,
                                                   datetime_dimension_type, NULL, description,
                                                   "seconds since 2000-01-01", NULL, read_datetime_stop);
    path = "/end_time";
    description = "end_time field from header section converted to seconds since 2000-01-01";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* orbit_index */
    description = "absolute orbit number";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "orbit_index", harp_type_int32, 0, NULL, NULL,
                                                   description, NULL, NULL, read_orbit_index);
    description = "extracted from the filename (assuming ssXXXXX... or srXXXXX... format); "
        "set to 0 if extraction of the value was not possible";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, NULL, description);

    /* latitude */
    description = "latitude of 30 km geometric tangent point for occultation";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_double, 1, dimension_type,
                                                   NULL, description, "degree_north", NULL, read_latitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    path = "/latitude";
    description = "latitude field from header section";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* longitude */
    description = "longitude of 30 km geometric tangent point for occultation";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_double, 1, dimension_type,
                                                   NULL, description, "degree_east", NULL, read_longitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0, 180.0);
    path = "/longitude";
    description = "longitude field from header section";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* altitude */
    description = "tangent altitude grid for retrieved parameters and species";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "altitude", harp_type_double, 1, dimension_type,
                                                   NULL, description, "km", NULL, read_altitude);
    path = "/data section/z";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* temperature */
    description = "temperature";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "temperature", harp_type_double, 1,
                                                   dimension_type, NULL, description, "K", NULL, read_temperature);
    path = "/data section/T";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* pressure */
    description = "pressure";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "pressure", harp_type_double, 1, dimension_type,
                                                   NULL, description, "hPa", NULL, read_pressure);
    path = "/data section/P (atm)";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* number_density */
    description = "atmospheric density";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "number_density", harp_type_double, 1,
                                                   dimension_type, NULL, description, "cm^-3", NULL, read_density);
    path = "/data section/dens";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

static void register_species_fields(harp_product_definition *product_definition, const char *species_name,
                                    int (*read_species_vmr)(void *user_data, harp_array data),
                                    int (*read_species_vmr_uncertainty)(void *user_data, harp_array data))
{
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_vertical };
    char description[255], field_name[81], path[255];

    /* <species>_volume_mixing_ratio */
    sprintf(description, "volume mixing ratio for %s", species_name);
    sprintf(field_name, "%s_volume_mixing_ratio", species_name);
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, field_name, harp_type_double, 1, dimension_type,
                                                   NULL, description, "ppmv", NULL, read_species_vmr);
    sprintf(path, "/data section/%s", species_name);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* <species>_volume_mixing_ratio_uncertainty */
    sprintf(description,
            "volume mixing ratio uncertainty for %s. If this value is -888 the vmr was not retrieved but obtained by scaling the a priori value",
            species_name);
    sprintf(field_name, "%s_volume_mixing_ratio_uncertainty", species_name);
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, field_name, harp_type_double, 1, dimension_type,
                                                   NULL, description, "ppmv", NULL, read_species_vmr_uncertainty);
    sprintf(path, "/data section/%s_err", species_name);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

static void register_ace_fts_main(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;

    module = harp_ingestion_register_module("ACE_FTS_L2_main", "ACE", "ACE_FTS", "L2_ASCII_main",
                                            "ACE_FTS_L2_ASCII_main", ingestion_init, ingestion_done);

    product_definition = harp_ingestion_register_product(module, "ACE_FTS_L2_main", NULL, read_dimensions);
    register_general_fields(product_definition);
    register_species_fields(product_definition, "H2O", read_h2o_vmr, read_h2o_vmr_uncertainty);
    register_species_fields(product_definition, "O3", read_o3_vmr, read_o3_vmr_uncertainty);
    register_species_fields(product_definition, "N2O", read_n2o_vmr, read_n2o_vmr_uncertainty);
    register_species_fields(product_definition, "CO", read_co_vmr, read_co_vmr_uncertainty);
    register_species_fields(product_definition, "CH4", read_ch4_vmr, read_ch4_vmr_uncertainty);
    register_species_fields(product_definition, "NO", read_no_vmr, read_no_vmr_uncertainty);
    register_species_fields(product_definition, "NO2", read_no2_vmr, read_no2_vmr_uncertainty);
    register_species_fields(product_definition, "HNO3", read_hno3_vmr, read_hno3_vmr_uncertainty);
    register_species_fields(product_definition, "HF", read_hf_vmr, read_hf_vmr_uncertainty);
    register_species_fields(product_definition, "HCl", read_hcl_vmr, read_hcl_vmr_uncertainty);
    register_species_fields(product_definition, "OCS", read_ocs_vmr, read_ocs_vmr_uncertainty);
    register_species_fields(product_definition, "N2O5", read_n2o5_vmr, read_n2o5_vmr_uncertainty);
    register_species_fields(product_definition, "ClONO2", read_clono2_vmr, read_clono2_vmr_uncertainty);
    register_species_fields(product_definition, "HCN", read_hcn_vmr, read_hcn_vmr_uncertainty);
    register_species_fields(product_definition, "CH3Cl", read_ch3cl_vmr, read_ch3cl_vmr_uncertainty);
    register_species_fields(product_definition, "CF4", read_cf4_vmr, read_cf4_vmr_uncertainty);
    register_species_fields(product_definition, "CCl2F2", read_ccl2f2_vmr, read_ccl2f2_vmr_uncertainty);
    register_species_fields(product_definition, "CCl3F", read_ccl3f_vmr, read_ccl3f_vmr_uncertainty);
    register_species_fields(product_definition, "COF2", read_cof2_vmr, read_cof2_vmr_uncertainty);
    register_species_fields(product_definition, "C2H6", read_c2h6_vmr, read_c2h6_vmr_uncertainty);
    register_species_fields(product_definition, "C2H2", read_c2h2_vmr, read_c2h2_vmr_uncertainty);
    register_species_fields(product_definition, "CHF2Cl", read_chf2cl_vmr, read_chf2cl_vmr_uncertainty);
    register_species_fields(product_definition, "SF6", read_sf6_vmr, read_sf6_vmr_uncertainty);
    register_species_fields(product_definition, "ClO", read_clo_vmr, read_clo_vmr_uncertainty);
    register_species_fields(product_definition, "HO2NO2", read_ho2no2_vmr, read_ho2no2_vmr_uncertainty);
    register_species_fields(product_definition, "H2O2", read_h2o2_vmr, read_h2o2_vmr_uncertainty);
    register_species_fields(product_definition, "HOCl", read_hocl_vmr, read_hocl_vmr_uncertainty);
    register_species_fields(product_definition, "N2", read_n2_vmr, read_n2_vmr_uncertainty);
}

static void register_ace_fts_iso(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;

    module = harp_ingestion_register_module("ACE_FTS_L2_iso", "ACE", "ACE_FTS", "L2_ASCII_iso",
                                            "ACE_FTS_L2_ASCII_iso", ingestion_init, ingestion_done);

    product_definition = harp_ingestion_register_product(module, "ACE_FTS_L2_iso", NULL, read_dimensions);
    register_general_fields(product_definition);
    register_species_fields(product_definition, "H2O_181", read_h2o_181_vmr, read_h2o_181_vmr_uncertainty);
    register_species_fields(product_definition, "H2O_171", read_h2o_171_vmr, read_h2o_171_vmr_uncertainty);
    register_species_fields(product_definition, "H2O_162", read_h2o_162_vmr, read_h2o_162_vmr_uncertainty);
    register_species_fields(product_definition, "CH4_311", read_ch4_311_vmr, read_ch4_311_vmr_uncertainty);
    register_species_fields(product_definition, "CH4_212", read_ch4_212_vmr, read_ch4_212_vmr_uncertainty);
}

int harp_ingestion_module_ace_fts_l2_init(void)
{
    register_ace_fts_main();
    register_ace_fts_iso();

    return 0;
}
