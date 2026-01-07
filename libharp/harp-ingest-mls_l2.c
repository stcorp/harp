/*
 * Copyright (C) 2015-2026 S[&]T, The Netherlands.
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

/* ------------------- Defines ------------------ */

#define SECONDS_FROM_1993_TO_2000 (220838400 + 5)

#define CHECKED_MALLOC(v, s) v = malloc(s); if (v == NULL) { harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)", s, __FILE__, __LINE__); return -1;}

/* ------------------ Typedefs ------------------ */

typedef struct ingest_info_struct
{
    const char *swath_name;
    coda_product *product;
    coda_cursor swath_cursor;
    coda_cursor geo_cursor;
    long num_times;
    long num_levels;
} ingest_info;

typedef struct product_limits_struct
{
    const char *product_name;
    double pressure_limit_high;
    double pressure_limit_low;
    double quality_threshold;
    double convergence_threshold;
} product_limits;

/* -------------- Global variables --------------- */

static product_limits check_limits[] = {
    {"BrO", 10.0, 3.2, 1.3, 1.05},
    {"CH3Cl", 147.0, 4.6, 1.3, 1.05},
    {"CH3CN", 46.0, 1.0, 1.4, 1.05},
    {"CH3OH", -1, -1, -1, -1},
    {"ClO", 147.0, 1.0, 1.3, 1.05},
    {"CO", 100.0, 0.0046, 1.5, 1.03},
    {"CO", 215.0, 146.0, 1.5, 1.03},
    {"GPH", 83.0, 0.001, 0.2, 1.03},
    {"GPH", 261.0, 100.0, 0.9, 1.03},
    {"H2O", 83.0, 0.002, 0.7, 2.0},
    {"H2O", 316.0, 100.0, 0.7, 2.0},
    {"HCl", 100.0, 0.32, 1.2, 1.05},
    {"HCN", 21.0, 0.1, 0.2, 2.0},
    {"HNO3", 215.0, 1.5, 0.8, 1.03},
    {"HO2", 22.0, 0.046, -1, 1.1},
    {"HOCl", 10.0, 2.2, 1.2, 1.05},
    {"IWC", 215.0, 83.0, -1, -1},
    {"IWP", -1, -1, -1, -1},
    {"N2O", 68.0, 0.46, 1.0, 2.0},
    {"O3", 100.0, 0.02, 1.0, 1.03},
    {"O3", 261.0, 121.0, 1.0, 1.03},
    {"OH", 32.0, 0.0032, -1, 1.1},
    {"RHI", 316.0, 100.0, 1.45, 2.0},
    {"RHI", 100.0, 83.0, -1, 2.0},
    {"RHI", 83.0, 0.002, 1.45, 2.0},
    {"SO2", 215.0, 10.0, 0.95, 1.03},
    {"Temperature", 83.0, 0.001, 0.2, 1.03},
    {"Temperature", 261.0, 100.0, 0.9, 1.03},
    {NULL, 0, 0, 0, 0}
};

static const char *quality_flag_description1 =
    "Bits 4 to 9 and 11 to 14 in the quality flag denote a specific error condition while bits 0 to 2 denote the "
    "severity (bit 0 error, bit 1 warning, bit 2 comment).\n\nBits 4 to 9 denote the MLS status (bit 4 HICLOUD, "
    "bit 5 LOWCLOUD, bit 6 NO_APRIORI_T, bit 7 NUM_ERROR, bit 8 TOO_FEW_RAD, bit 9 GLOB_FAILURE).\n\n"
    "Bits 11 to 14 denote specific checks as specified in the EOS MLS data quality document (the pressure range, ";
static const char *quality_flag_description2 =
    "quality and convergence thresholds come from table 1.1.1 in issue 4.x-4.0 of that document): "
    "Bit 11 denotes pressure out of range, bit 12 denotes quality below threshold, bit 13 denotes convergence above "
    "threshold and bit 14 denotes a negative precision. Also, if any of the bits 11 to 14 is set, "
    "bit 0 is automatically set.\n\n";

static const char *hno3_quality_flag_description =
    "Bits 15 and 16 denote the results of checks that are only performed for the HNO3 product. Bit 15 is set if the "
    "pressure is at most 68 hPa and the quality flag is not zero. Bit 16 is set if either the pressure is at least "
    "316 hPa and the volume mixing ratio is less than -2.0 or the pressure is between 68 hPa and 215 hPa and the "
    "volume mixing ratio is less than -1.2. Again, if any of the bits 15 and 16 is set, bit 0 is automatically set."
    "\n\n";

/* -------------------- Code -------------------- */

static int init_cursors(ingest_info *info)
{
    if (coda_cursor_set_product(&info->swath_cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&info->swath_cursor, "/HDFEOS/SWATHS") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&info->swath_cursor, info->swath_name) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    info->geo_cursor = info->swath_cursor;
    if (coda_cursor_goto_record_field_by_name(&info->swath_cursor, "Data_Fields") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&info->geo_cursor, "Geolocation_Fields") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    return 0;
}

static int get_dimensions(ingest_info *info)
{
    coda_cursor cursor;
    long coda_dimension[CODA_MAX_NUM_DIMS];
    int num_coda_dimensions;

    cursor = info->swath_cursor;
    if (coda_cursor_goto_record_field_by_name(&cursor, "L2gpValue") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_array_dim(&cursor, &num_coda_dimensions, coda_dimension) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    info->num_times = coda_dimension[0];
    info->num_levels = coda_dimension[1];

    return 0;
}

static int get_variable_attributes(coda_cursor *cursor, double *missing_value, double *scale_factor, double *offset)
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

static int read_variable(coda_cursor *cursor, const char *name, int num_dimensions, long dimension_0, long dimension_1,
                         harp_array data)
{
    double missing_value;
    double scale_factor;
    double offset;
    long num_elements;
    long coda_dimension[CODA_MAX_NUM_DIMS];
    int num_coda_dimensions;
    long i;

    if (coda_cursor_goto_record_field_by_name(cursor, name) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_array_dim(cursor, &num_coda_dimensions, coda_dimension) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (num_coda_dimensions != num_dimensions)
    {
        harp_set_error(HARP_ERROR_INGESTION, "product error detected in MLS L2 product (variable %s has %d dimensions, "
                       "expected %d)", name, num_coda_dimensions, num_dimensions);
        return -1;
    }
    if (dimension_0 != coda_dimension[0])
    {
        harp_set_error(HARP_ERROR_INGESTION,
                       "product error detected in MLS L2 product (first dimension for variable %s "
                       "has %ld elements, expected %ld", name, coda_dimension[0], dimension_0);
        return -1;
    }
    num_elements = coda_dimension[0];
    if (num_dimensions > 1)
    {
        if (dimension_1 != coda_dimension[1])
        {
            harp_set_error(HARP_ERROR_INGESTION, "product error detected in MLS L2 product (second dimension for "
                           "variable %s has %ld elements, expected %ld", name, coda_dimension[1], dimension_1);
            return -1;
        }
        num_elements *= coda_dimension[1];
    }
    if (get_variable_attributes(cursor, &missing_value, &scale_factor, &offset) != 0)
    {
        return -1;
    }
    if (coda_cursor_read_double_array(cursor, data.double_data, coda_array_ordering_c) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    /* apply scaling and filter for NaN */
    for (i = 0; i < num_elements; i++)
    {
        if (data.double_data[i] == missing_value)
        {
            data.double_data[i] = coda_NaN();
        }
        else
        {
            data.double_data[i] = offset + scale_factor * data.double_data[i];
        }
    }

    coda_cursor_goto_parent(cursor);

    return 0;
}

static int read_int32_variable(coda_cursor *cursor, const char *name, int num_dimensions, long dimension_0,
                               harp_array data)
{
    long coda_dimension[CODA_MAX_NUM_DIMS];
    int num_coda_dimensions;

    if (coda_cursor_goto_record_field_by_name(cursor, name) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_array_dim(cursor, &num_coda_dimensions, coda_dimension) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (num_coda_dimensions != num_dimensions)
    {
        harp_set_error(HARP_ERROR_INGESTION, "product error detected in MLS L2 product (variable %s has %d dimensions, "
                       "expected %d)", name, num_coda_dimensions, num_dimensions);
        return -1;
    }
    if (dimension_0 != coda_dimension[0])
    {
        harp_set_error(HARP_ERROR_INGESTION,
                       "product error detected in MLS L2 product (first dimension for variable %s "
                       "has %ld elements, expected %ld", name, coda_dimension[0], dimension_0);
        return -1;
    }
    if (coda_cursor_read_int32_array(cursor, data.int32_data, coda_array_ordering_c) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    coda_cursor_goto_parent(cursor);

    return 0;
}

static int read_dimensions(void *user_data, long dimension[HARP_NUM_DIM_TYPES])
{
    ingest_info *info = (ingest_info *)user_data;

    dimension[harp_dimension_time] = info->num_times;
    dimension[harp_dimension_vertical] = info->num_levels;

    return 0;
}

static int read_time(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long i;

    if (read_variable(&info->geo_cursor, "Time", 1, info->num_times, 0, data) != 0)
    {
        return -1;
    }

    /* convert time values from TAI93 to seconds since 2000-01-01 */
    for (i = 0; i < info->num_times; i++)
    {
        data.double_data[i] -= SECONDS_FROM_1993_TO_2000;
    }

    return 0;
}

static int read_longitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable(&info->geo_cursor, "Longitude", 1, info->num_times, 0, data);
}

static int read_latitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable(&info->geo_cursor, "Latitude", 1, info->num_times, 0, data);
}

static int read_pressure(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable(&info->geo_cursor, "Pressure", 1, info->num_levels, 0, data);
}

static int read_value(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable(&info->swath_cursor, "L2gpValue", 2, info->num_times, info->num_levels, data);
}

static int read_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable(&info->swath_cursor, "L2gpPrecision", 2, info->num_times, info->num_levels, data);
}

static int perform_pressure_quality_convergence_precision_checks(ingest_info *info, int32_t *status_data,
                                                                 const char *product_name)
{
    double pressure, quality, convergence, precision;
    long i, j;
    short pressure_is_within_limits;
    short quality_is_high_enough;
    short convergence_is_low_enough;
    harp_array pressure_data, quality_data, convergence_data, precision_data;
    product_limits *limits;

    CHECKED_MALLOC(pressure_data.double_data, info->num_levels * sizeof(double));
    quality_data.double_data = malloc(info->num_times * sizeof(double));
    if (quality_data.double_data == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       info->num_times * sizeof(double), __FILE__, __LINE__);
        free(pressure_data.double_data);
        return -1;
    }
    convergence_data.double_data = malloc(info->num_times * sizeof(double));
    if (convergence_data.double_data == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       info->num_times * sizeof(double), __FILE__, __LINE__);
        free(quality_data.double_data);
        free(pressure_data.double_data);
        return -1;
    }
    precision_data.double_data = malloc(info->num_times * info->num_levels * sizeof(double));
    if (precision_data.double_data == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       info->num_times * info->num_levels * sizeof(double), __FILE__, __LINE__);
        free(convergence_data.double_data);
        free(quality_data.double_data);
        free(pressure_data.double_data);
        return -1;
    }

    if (read_variable(&info->geo_cursor, "Pressure", 1, info->num_levels, 0, pressure_data) != 0)
    {
        free(precision_data.double_data);
        free(convergence_data.double_data);
        free(quality_data.double_data);
        free(pressure_data.double_data);
        return -1;
    }
    if (read_variable(&info->swath_cursor, "Quality", 1, info->num_times, 0, quality_data) != 0)
    {
        free(precision_data.double_data);
        free(convergence_data.double_data);
        free(quality_data.double_data);
        free(pressure_data.double_data);
        return -1;
    }
    if (read_variable(&info->swath_cursor, "Convergence", 1, info->num_times, 0, convergence_data) != 0)
    {
        /* In old datafiles the field Convergence was not yet present so   */
        /* we fill the Convergence values with values far below the limit. */
        memset(convergence_data.double_data, '\0', info->num_times * sizeof(double));
    }
    if (read_variable(&info->swath_cursor, "L2gpPrecision", 2, info->num_times, info->num_levels, precision_data) != 0)
    {
        free(precision_data.double_data);
        free(convergence_data.double_data);
        free(quality_data.double_data);
        free(pressure_data.double_data);
        return -1;
    }
    for (i = 0; i < info->num_times; i++)
    {
        for (j = 0; j < info->num_levels; j++)
        {
            pressure = pressure_data.double_data[j];
            quality = quality_data.double_data[i];
            convergence = convergence_data.double_data[i];
            precision = precision_data.double_data[i * info->num_levels + j];
            pressure_is_within_limits = 0;
            quality_is_high_enough = 0;
            convergence_is_low_enough = 0;
            limits = check_limits;
            while (limits->product_name != NULL)
            {
                if (strcmp(limits->product_name, product_name) == 0)
                {
                    if ((limits->pressure_limit_low < 0.0) ||
                        ((pressure >= limits->pressure_limit_low) && (pressure <= limits->pressure_limit_high)))
                    {
                        pressure_is_within_limits = 1;
                        if ((limits->quality_threshold < 0.0) || (quality >= limits->quality_threshold))
                        {
                            quality_is_high_enough = 1;
                        }
                        if ((limits->convergence_threshold < 0.0) || (convergence <= limits->convergence_threshold))
                        {
                            convergence_is_low_enough = 1;
                        }
                    }
                }
                limits++;
            }
            if (!pressure_is_within_limits)
            {
                /* Set bit 11 (OUTSIDE_PRESS_RANGE) + bit 0 (MLS_STATUS_BAD) */
                status_data[i * info->num_levels + j] |= 0x801;
            }
            if (!quality_is_high_enough)
            {
                /* Set bit 12 (QUALITY_TOO_LOW) + bit 0 (MLS_STATUS_BAD) */
                status_data[i * info->num_levels + j] |= 0x1001;
            }
            if (!convergence_is_low_enough)
            {
                /* Set bit 13 (CONVERGENCE_TOO_HIGH) + bit 0 (MLS_STATUS_BAD) */
                status_data[i * info->num_levels + j] |= 0x2001;
            }
            if (precision <= 0.0)
            {
                /* Set bit 14 (NEGATIVE_PRECISION) + bit 0 (MLS_STATUS_BAD) */
                status_data[i * info->num_levels + j] |= 0x4001;
            }
        }
    }
    free(precision_data.double_data);
    free(convergence_data.double_data);
    free(quality_data.double_data);
    free(pressure_data.double_data);
    return 0;
}

static int perform_hno3_checks(ingest_info *info, int32_t *status_data)
{
    double pressure, vmr;
    long i, j;
    harp_array pressure_data, vmr_data;

    CHECKED_MALLOC(pressure_data.double_data, info->num_levels * sizeof(double));
    vmr_data.double_data = malloc(info->num_times * info->num_levels * sizeof(double));
    if (vmr_data.double_data == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       info->num_times * info->num_levels * sizeof(double), __FILE__, __LINE__);
        free(pressure_data.double_data);
        return -1;
    }
    if (read_variable(&info->geo_cursor, "Pressure", 1, info->num_levels, 0, pressure_data) != 0)
    {
        free(vmr_data.double_data);
        free(pressure_data.double_data);
        return -1;
    }
    if (read_variable(&info->swath_cursor, "L2gpValue", 2, info->num_times, info->num_levels, vmr_data) != 0)
    {
        free(vmr_data.double_data);
        free(pressure_data.double_data);
        return -1;
    }
    for (i = 0; i < info->num_times; i++)
    {
        for (j = 0; j < info->num_levels; j++)
        {
            pressure = pressure_data.double_data[j];
            vmr = vmr_data.double_data[i * info->num_levels + j];
            /* Check on cloud contamination as specified on page 90 of the DQD issue 4.x-4.0 */
            if ((pressure <= 68) && (status_data[i * info->num_levels + j] != 0))
            {
                /* Set bit 15 (CLOUD_CONTAMINATION) + bit 0 (MLS_STATUS_BAD) */
                status_data[i * info->num_levels + j] |= 0x8001;
            }
            /* Check on HNO3 outlier as specified on page 91 of the DQD issue 4.x-4.0 */
            if ((pressure >= 316.0) && (vmr < -2.0))
            {
                /* Set bit 16 (HNO3_OUTLIER) + bit 0 (MLS_STATUS_BAD) */
                status_data[i * info->num_levels + j] |= 0x10001;
            }
            else if ((pressure >= 68.0) && (pressure <= 215.0) && (vmr < -1.6))
            {
                /* Set bit 16 (HNO3_OUTLIER) + bit 0 (MLS_STATUS_BAD) */
                status_data[i * info->num_levels + j] |= 0x10001;
            }
        }
    }
    free(vmr_data.double_data);
    free(pressure_data.double_data);
    return 0;
}

static int read_validity(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long i, j;

    /* The Status-field in the ingested file is 1-dimensional with size info->num_times but the       */
    /* validity-field in the HARP data is 2-dimensional with size info->num_times * info->num_levels. */
    if (read_int32_variable(&info->swath_cursor, "Status", 1, info->num_times, data) != 0)
    {
        return -1;
    }
    for (i = info->num_times - 1; i >= 0; i--)
    {
        for (j = 0; j < info->num_levels; j++)
        {
            data.int32_data[i * info->num_levels + j] = data.int32_data[i];
        }
    }

    if (perform_pressure_quality_convergence_precision_checks(info, data.int32_data, info->swath_name) != 0)
    {
        return -1;
    }

    if (strcmp(info->swath_name, "HNO3") == 0)
    {
        if (perform_hno3_checks(info, data.int32_data) != 0)
        {
            return -1;
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
                          const harp_ingestion_options *options, harp_product_definition **definition, void **user_data,
                          const char *swath_name)
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
    info->swath_name = swath_name;

    if (init_cursors(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }
    if (get_dimensions(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }

    *definition = *module->product_definition;
    *user_data = info;

    return 0;
}

static int ingestion_init_bro(const harp_ingestion_module *module, coda_product *product,
                              const harp_ingestion_options *options, harp_product_definition **definition,
                              void **user_data)
{
    return ingestion_init(module, product, options, definition, user_data, "BrO");
}

static int ingestion_init_ch3cl(const harp_ingestion_module *module, coda_product *product,
                                const harp_ingestion_options *options, harp_product_definition **definition,
                                void **user_data)
{
    return ingestion_init(module, product, options, definition, user_data, "CH3Cl");
}

static int ingestion_init_ch3cn(const harp_ingestion_module *module, coda_product *product,
                                const harp_ingestion_options *options, harp_product_definition **definition,
                                void **user_data)
{
    return ingestion_init(module, product, options, definition, user_data, "CH3CN");
}

static int ingestion_init_ch3oh(const harp_ingestion_module *module, coda_product *product,
                                const harp_ingestion_options *options, harp_product_definition **definition,
                                void **user_data)
{
    return ingestion_init(module, product, options, definition, user_data, "CH3OH");
}

static int ingestion_init_clo(const harp_ingestion_module *module, coda_product *product,
                              const harp_ingestion_options *options, harp_product_definition **definition,
                              void **user_data)
{
    return ingestion_init(module, product, options, definition, user_data, "ClO");
}

static int ingestion_init_co(const harp_ingestion_module *module, coda_product *product,
                             const harp_ingestion_options *options, harp_product_definition **definition,
                             void **user_data)
{
    return ingestion_init(module, product, options, definition, user_data, "CO");
}

static int ingestion_init_gph(const harp_ingestion_module *module, coda_product *product,
                              const harp_ingestion_options *options, harp_product_definition **definition,
                              void **user_data)
{
    return ingestion_init(module, product, options, definition, user_data, "GPH");
}

static int ingestion_init_h2o(const harp_ingestion_module *module, coda_product *product,
                              const harp_ingestion_options *options, harp_product_definition **definition,
                              void **user_data)
{
    return ingestion_init(module, product, options, definition, user_data, "H2O");
}

static int ingestion_init_hcl(const harp_ingestion_module *module, coda_product *product,
                              const harp_ingestion_options *options, harp_product_definition **definition,
                              void **user_data)
{
    return ingestion_init(module, product, options, definition, user_data, "HCl");
}

static int ingestion_init_hcn(const harp_ingestion_module *module, coda_product *product,
                              const harp_ingestion_options *options, harp_product_definition **definition,
                              void **user_data)
{
    return ingestion_init(module, product, options, definition, user_data, "HCN");
}

static int ingestion_init_hno3(const harp_ingestion_module *module, coda_product *product,
                               const harp_ingestion_options *options, harp_product_definition **definition,
                               void **user_data)
{
    return ingestion_init(module, product, options, definition, user_data, "HNO3");
}

static int ingestion_init_ho2(const harp_ingestion_module *module, coda_product *product,
                              const harp_ingestion_options *options, harp_product_definition **definition,
                              void **user_data)
{
    return ingestion_init(module, product, options, definition, user_data, "HO2");
}

static int ingestion_init_hocl(const harp_ingestion_module *module, coda_product *product,
                               const harp_ingestion_options *options, harp_product_definition **definition,
                               void **user_data)
{
    return ingestion_init(module, product, options, definition, user_data, "HOCl");
}

static int ingestion_init_iwc(const harp_ingestion_module *module, coda_product *product,
                              const harp_ingestion_options *options, harp_product_definition **definition,
                              void **user_data)
{
    return ingestion_init(module, product, options, definition, user_data, "IWC");
}

static int ingestion_init_n2o(const harp_ingestion_module *module, coda_product *product,
                              const harp_ingestion_options *options, harp_product_definition **definition,
                              void **user_data)
{
    return ingestion_init(module, product, options, definition, user_data, "N2O");
}

static int ingestion_init_o3(const harp_ingestion_module *module, coda_product *product,
                             const harp_ingestion_options *options, harp_product_definition **definition,
                             void **user_data)
{
    return ingestion_init(module, product, options, definition, user_data, "O3");
}

static int ingestion_init_oh(const harp_ingestion_module *module, coda_product *product,
                             const harp_ingestion_options *options, harp_product_definition **definition,
                             void **user_data)
{
    return ingestion_init(module, product, options, definition, user_data, "OH");
}

static int ingestion_init_rhi(const harp_ingestion_module *module, coda_product *product,
                              const harp_ingestion_options *options, harp_product_definition **definition,
                              void **user_data)
{
    return ingestion_init(module, product, options, definition, user_data, "RHI");
}

static int ingestion_init_so2(const harp_ingestion_module *module, coda_product *product,
                              const harp_ingestion_options *options, harp_product_definition **definition,
                              void **user_data)
{
    return ingestion_init(module, product, options, definition, user_data, "SO2");
}

static int ingestion_init_t(const harp_ingestion_module *module, coda_product *product,
                            const harp_ingestion_options *options, harp_product_definition **definition,
                            void **user_data)
{
    return ingestion_init(module, product, options, definition, user_data, "Temperature");
}

static void register_datetime_variable(harp_product_definition *product_definition, const char *path)
{
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };
    const char *description;

    description = "time of the measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double, 1, dimension_type,
                                                   NULL, description, "seconds since 2000-01-01", NULL, read_time);

    description = "the time converted from TAI93 to seconds since 2000-01-01T00:00:00";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);
}

static void register_longitude_variable(harp_product_definition *product_definition, const char *path)
{
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };
    const char *description;

    description = "tangent longitude";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_double, 1, dimension_type,
                                                   NULL, description, "degree_east", NULL, read_longitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0, 180.0);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

static void register_latitude_variable(harp_product_definition *product_definition, const char *path)
{
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };
    const char *description;

    description = "tangent latitude";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_double, 1, dimension_type,
                                                   NULL, description, "degree_north", NULL, read_latitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

static void register_pressure_variable(harp_product_definition *product_definition, const char *path)
{
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_vertical };
    const char *description;

    description = "pressure per profile level";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "pressure", harp_type_double, 1, dimension_type,
                                                   NULL, description, "hPa", NULL, read_pressure);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

static void register_bro_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_vertical };
    const char *description;
    const char *path;

    module = harp_ingestion_register_module("MLS_L2_BRO", "MLS", "AURA_MLS", "ML2BRO", "MLS BrO profile",
                                            ingestion_init_bro, ingestion_done);

    /* BRO product */
    product_definition = harp_ingestion_register_product(module, "MLS_L2_BRO", NULL, read_dimensions);
    harp_product_definition_add_mapping(product_definition, quality_flag_description1, NULL);
    harp_product_definition_add_mapping(product_definition, quality_flag_description2, NULL);

    /* datetime */
    path = "/HDFEOS/SWATHS/BrO/Geolocation_Fields/Time[]";
    register_datetime_variable(product_definition, path);

    /* longitude and latitude */
    path = "/HDFEOS/SWATHS/BrO/Geolocation_Fields/Longitude[]";
    register_longitude_variable(product_definition, path);
    path = "/HDFEOS/SWATHS/BrO/Geolocation_Fields/Latitude[]";
    register_latitude_variable(product_definition, path);

    /* pressure */
    path = "/HDFEOS/SWATHS/BrO/Geolocation_Fields/Pressure[]";
    register_pressure_variable(product_definition, path);

    /* BrO_volume_mixing_ratio */
    description = "BrO volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "BrO_volume_mixing_ratio", harp_type_double, 2,
                                                   dimension_type, NULL, description, "ppv", NULL, read_value);
    path = "/HDFEOS/SWATHS/BrO/Data_Fields/L2gpValue[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* BrO_volume_mixing_ratio_uncertainty */
    description = "uncertainty of the BrO volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "BrO_volume_mixing_ratio_uncertainty",
                                                   harp_type_double, 2, dimension_type, NULL, description, "ppv", NULL,
                                                   read_error);
    path = "/HDFEOS/SWATHS/BrO/Data_Fields/L2gpPrecision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* BrO_volume_mixing_ratio_validity */
    description = "quality flag for the BrO volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "BrO_volume_mixing_ratio_validity",
                                                   harp_type_int32, 2, dimension_type, NULL, description, NULL, NULL,
                                                   read_validity);
    path = "/HDFEOS/SWATHS/BrO/Data_Fields/Status[], /HDFEOS/SWATHS/BrO/Data_Fields/Quality[], "
        "/HDFEOS/SWATHS/BrO/Data_Fields/Convergence[], /HDFEOS/SWATHS/BrO/Data_Fields/L2gpPrecision[], "
        "/HDFEOS/SWATHS/BrO/Geolocation_Fields/Pressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, "see generic mapping description");
}

static void register_ch3cl_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_vertical };
    const char *description;
    const char *path;

    module = harp_ingestion_register_module("MLS_L2_CH3Cl", "MLS", "AURA_MLS", "ML2CH3CL", "MLS CH3Cl profile",
                                            ingestion_init_ch3cl, ingestion_done);

    /* CH3Cl product */
    product_definition = harp_ingestion_register_product(module, "MLS_L2_CH3Cl", NULL, read_dimensions);
    harp_product_definition_add_mapping(product_definition, quality_flag_description1, NULL);
    harp_product_definition_add_mapping(product_definition, quality_flag_description2, NULL);

    /* datetime */
    path = "/HDFEOS/SWATHS/CH3Cl/Geolocation_Fields/Time[]";
    register_datetime_variable(product_definition, path);

    /* longitude and latitude */
    path = "/HDFEOS/SWATHS/CH3Cl/Geolocation_Fields/Longitude[]";
    register_longitude_variable(product_definition, path);
    path = "/HDFEOS/SWATHS/CH3Cl/Geolocation_Fields/Latitude[]";
    register_latitude_variable(product_definition, path);

    /* pressure */
    path = "/HDFEOS/SWATHS/CH3Cl/Geolocation_Fields/Pressure[]";
    register_pressure_variable(product_definition, path);

    /* CH3Cl_volume_mixing_ratio */
    description = "CH3Cl volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "CH3Cl_volume_mixing_ratio", harp_type_double, 2,
                                                   dimension_type, NULL, description, "ppv", NULL, read_value);
    path = "/HDFEOS/SWATHS/CH3Cl/Data_Fields/L2gpValue[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* CH3Cl_volume_mixing_ratio_uncertainty */
    description = "uncertainty of the CH3Cl volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "CH3Cl_volume_mixing_ratio_uncertainty",
                                                   harp_type_double, 2, dimension_type, NULL, description, "ppv", NULL,
                                                   read_error);
    path = "/HDFEOS/SWATHS/CH3Cl/Data_Fields/L2gpPrecision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* CH3Cl_volume_mixing_ratio_validity */
    description = "quality flag for the CH3Cl volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "CH3Cl_volume_mixing_ratio_validity",
                                                   harp_type_int32, 2, dimension_type, NULL, description, NULL, NULL,
                                                   read_validity);
    path = "/HDFEOS/SWATHS/CH3Cl/Data_Fields/Status[], /HDFEOS/SWATHS/CH3Cl/Data_Fields/Quality[], "
        "/HDFEOS/SWATHS/CH3Cl/Data_Fields/Convergence[], /HDFEOS/SWATHS/CH3Cl/Data_Fields/L2gpPrecision[], "
        "/HDFEOS/SWATHS/CH3Cl/Geolocation_Fields/Pressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, "see generic mapping description");
}

static void register_ch3cn_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_vertical };
    const char *description;
    const char *path;

    module = harp_ingestion_register_module("MLS_L2_CH3CN", "MLS", "AURA_MLS", "ML2CH3CN", "MLS CH3CN profile",
                                            ingestion_init_ch3cn, ingestion_done);

    /* CH3CN product */
    product_definition = harp_ingestion_register_product(module, "MLS_L2_CH3CN", NULL, read_dimensions);
    harp_product_definition_add_mapping(product_definition, quality_flag_description1, NULL);
    harp_product_definition_add_mapping(product_definition, quality_flag_description2, NULL);

    /* datetime */
    path = "/HDFEOS/SWATHS/CH3CN/Geolocation_Fields/Time[]";
    register_datetime_variable(product_definition, path);

    /* longitude and latitude */
    path = "/HDFEOS/SWATHS/CH3CN/Geolocation_Fields/Longitude[]";
    register_longitude_variable(product_definition, path);
    path = "/HDFEOS/SWATHS/CH3CN/Geolocation_Fields/Latitude[]";
    register_latitude_variable(product_definition, path);

    /* pressure */
    path = "/HDFEOS/SWATHS/CH3CN/Geolocation_Fields/Pressure[]";
    register_pressure_variable(product_definition, path);

    /* CH3CN_volume_mixing_ratio */
    description = "CH3CN volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "CH3CN_volume_mixing_ratio", harp_type_double, 2,
                                                   dimension_type, NULL, description, "ppv", NULL, read_value);
    path = "/HDFEOS/SWATHS/CH3CN/Data_Fields/L2gpValue[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* CH3Cl_volume_mixing_ratio_uncertainty */
    description = "uncertainty of the CH3CN volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "CH3CN_volume_mixing_ratio_uncertainty",
                                                   harp_type_double, 2, dimension_type, NULL, description, "ppv",
                                                   NULL, read_error);
    path = "/HDFEOS/SWATHS/CH3CN/Data_Fields/L2gpPrecision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* CH3CN_volume_mixing_ratio_validity */
    description = "quality flag for the CH3CN volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "CH3CN_volume_mixing_ratio_validity",
                                                   harp_type_int32, 2, dimension_type, NULL, description, NULL, NULL,
                                                   read_validity);
    path = "/HDFEOS/SWATHS/CH3CN/Data_Fields/Status[], /HDFEOS/SWATHS/CH3CN/Data_Fields/Quality[], "
        "/HDFEOS/SWATHS/CH3CN/Data_Fields/Convergence[], /HDFEOS/SWATHS/CH3CN/Data_Fields/L2gpPrecision[], "
        "/HDFEOS/SWATHS/CH3CN/Geolocation_Fields/Pressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, "see generic mapping description");
}

static void register_ch3oh_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_vertical };
    const char *description;
    const char *path;

    module = harp_ingestion_register_module("MLS_L2_CH3OH", "MLS", "AURA_MLS", "ML2CH3OH", "MLS CH3OH profile",
                                            ingestion_init_ch3oh, ingestion_done);

    /* CH3OH product */
    product_definition = harp_ingestion_register_product(module, "MLS_L2_CH3OH", NULL, read_dimensions);
    harp_product_definition_add_mapping(product_definition, quality_flag_description1, NULL);
    harp_product_definition_add_mapping(product_definition, quality_flag_description2, NULL);

    /* datetime */
    path = "/HDFEOS/SWATHS/CH3OH/Geolocation_Fields/Time[]";
    register_datetime_variable(product_definition, path);

    /* longitude and latitude */
    path = "/HDFEOS/SWATHS/CH3OH/Geolocation_Fields/Longitude[]";
    register_longitude_variable(product_definition, path);
    path = "/HDFEOS/SWATHS/CH3OH/Geolocation_Fields/Latitude[]";
    register_latitude_variable(product_definition, path);

    /* pressure */
    path = "/HDFEOS/SWATHS/CH3OH/Geolocation_Fields/Pressure[]";
    register_pressure_variable(product_definition, path);

    /* CH3OH_volume_mixing_ratio */
    description = "CH3OH volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "CH3OH_volume_mixing_ratio", harp_type_double, 2,
                                                   dimension_type, NULL, description, "ppv", NULL, read_value);
    path = "/HDFEOS/SWATHS/CH3OH/Data_Fields/L2gpValue[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* CH3OH_volume_mixing_ratio_uncertainty */
    description = "uncertainty of the CH3OH volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "CH3OH_volume_mixing_ratio_uncertainty",
                                                   harp_type_double, 2, dimension_type, NULL, description, "ppv", NULL,
                                                   read_error);
    path = "/HDFEOS/SWATHS/CH3OH/Data_Fields/L2gpPrecision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* CH3OH_volume_mixing_ratio_validity */
    description = "quality flag for the CH3OH volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "CH3OH_volume_mixing_ratio_validity",
                                                   harp_type_int32, 2, dimension_type, NULL, description, NULL, NULL,
                                                   read_validity);
    path = "/HDFEOS/SWATHS/CH3OH/Data_Fields/Status[], /HDFEOS/SWATHS/CH3OH/Data_Fields/Quality[], "
        "/HDFEOS/SWATHS/CH3OH/Data_Fields/Convergence[], /HDFEOS/SWATHS/CH3OH/Data_Fields/L2gpPrecision[], "
        "/HDFEOS/SWATHS/CH3OH/Geolocation_Fields/Pressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, "see generic mapping description");
}

static void register_clo_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_vertical };
    const char *description;
    const char *path;

    module = harp_ingestion_register_module("MLS_L2_CLO", "MLS", "AURA_MLS", "ML2CLO", "MLS ClO profile",
                                            ingestion_init_clo, ingestion_done);

    /* CLO product */
    product_definition = harp_ingestion_register_product(module, "MLS_L2_CLO", NULL, read_dimensions);
    harp_product_definition_add_mapping(product_definition, quality_flag_description1, NULL);
    harp_product_definition_add_mapping(product_definition, quality_flag_description2, NULL);

    /* datetime */
    path = "/HDFEOS/SWATHS/ClO/Geolocation_Fields/Time[]";
    register_datetime_variable(product_definition, path);

    /* longitude and latitude */
    path = "/HDFEOS/SWATHS/ClO/Geolocation_Fields/Longitude[]";
    register_longitude_variable(product_definition, path);
    path = "/HDFEOS/SWATHS/ClO/Geolocation_Fields/Latitude[]";
    register_latitude_variable(product_definition, path);

    /* pressure */
    path = "/HDFEOS/SWATHS/ClO/Geolocation_Fields/Pressure[]";
    register_pressure_variable(product_definition, path);

    /* ClO_volume_mixing_ratio */
    description = "ClO volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "ClO_volume_mixing_ratio", harp_type_double, 2,
                                                   dimension_type, NULL, description, "ppv", NULL, read_value);
    path = "/HDFEOS/SWATHS/ClO/Data_Fields/L2gpValue[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* ClO_volume_mixing_ratio_uncertainty */
    description = "uncertainty of the ClO volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "ClO_volume_mixing_ratio_uncertainty",
                                                   harp_type_double, 2, dimension_type, NULL, description, "ppv", NULL,
                                                   read_error);
    path = "/HDFEOS/SWATHS/ClO/Data_Fields/L2gpPrecision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* ClO_volume_mixing_ratio_validity */
    description = "quality flag for the ClO volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "ClO_volume_mixing_ratio_validity",
                                                   harp_type_int32, 2, dimension_type, NULL, description, NULL, NULL,
                                                   read_validity);
    path = "/HDFEOS/SWATHS/ClO/Data_Fields/Status[], /HDFEOS/SWATHS/ClO/Data_Fields/Quality[], "
        "/HDFEOS/SWATHS/ClO/Data_Fields/Convergence[], /HDFEOS/SWATHS/ClO/Data_Fields/L2gpPrecision[], "
        "/HDFEOS/SWATHS/ClO/Geolocation_Fields/Pressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, "see generic mapping description");
}

static void register_co_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_vertical };
    const char *description;
    const char *path;

    module = harp_ingestion_register_module("MLS_L2_CO", "MLS", "AURA_MLS", "ML2CO", "MLS CO profile",
                                            ingestion_init_co, ingestion_done);

    /* CO product */
    product_definition = harp_ingestion_register_product(module, "MLS_L2_CO", NULL, read_dimensions);
    harp_product_definition_add_mapping(product_definition, quality_flag_description1, NULL);
    harp_product_definition_add_mapping(product_definition, quality_flag_description2, NULL);

    /* datetime */
    path = "/HDFEOS/SWATHS/CO/Geolocation_Fields/Time[]";
    register_datetime_variable(product_definition, path);

    /* longitude and latitude */
    path = "/HDFEOS/SWATHS/CO/Geolocation_Fields/Longitude[]";
    register_longitude_variable(product_definition, path);
    path = "/HDFEOS/SWATHS/CO/Geolocation_Fields/Latitude[]";
    register_latitude_variable(product_definition, path);

    /* pressure */
    path = "/HDFEOS/SWATHS/CO/Geolocation_Fields/Pressure[]";
    register_pressure_variable(product_definition, path);

    /* CO_volume_mixing_ratio */
    description = "CO volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "CO_volume_mixing_ratio", harp_type_double, 2,
                                                   dimension_type, NULL, description, "ppv", NULL, read_value);
    path = "/HDFEOS/SWATHS/CO/Data_Fields/L2gpValue[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* CO_volume_mixing_ratio_uncertainty */
    description = "uncertainty of the CO volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "CO_volume_mixing_ratio_uncertainty",
                                                   harp_type_double, 2, dimension_type, NULL, description, "ppv", NULL,
                                                   read_error);
    path = "/HDFEOS/SWATHS/CO/Data_Fields/L2gpPrecision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* CO_volume_mixing_ratio_validity */
    description = "quality flag for the CO volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "CO_volume_mixing_ratio_validity",
                                                   harp_type_int32, 2, dimension_type, NULL, description, NULL, NULL,
                                                   read_validity);
    path = "/HDFEOS/SWATHS/CO/Data_Fields/Status[], /HDFEOS/SWATHS/CO/Data_Fields/Quality[], "
        "/HDFEOS/SWATHS/CO/Data_Fields/Convergence[], /HDFEOS/SWATHS/CO/Data_Fields/L2gpPrecision[], "
        "/HDFEOS/SWATHS/CO/Geolocation_Fields/Pressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, "see generic mapping description");
}

static void register_gph_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_vertical };
    const char *description;
    const char *path;

    module = harp_ingestion_register_module("MLS_L2_GPH", "MLS", "AURA_MLS", "ML2GPH", "MLS GPH profile",
                                            ingestion_init_gph, ingestion_done);

    /* GPH product */
    product_definition = harp_ingestion_register_product(module, "MLS_L2_GPH", NULL, read_dimensions);
    harp_product_definition_add_mapping(product_definition, quality_flag_description1, NULL);
    harp_product_definition_add_mapping(product_definition, quality_flag_description2, NULL);

    /* datetime */
    path = "/HDFEOS/SWATHS/GPH/Geolocation_Fields/Time[]";
    register_datetime_variable(product_definition, path);

    /* longitude and latitude */
    path = "/HDFEOS/SWATHS/GPH/Geolocation_Fields/Longitude[]";
    register_longitude_variable(product_definition, path);
    path = "/HDFEOS/SWATHS/GPH/Geolocation_Fields/Latitude[]";
    register_latitude_variable(product_definition, path);

    /* pressure */
    path = "/HDFEOS/SWATHS/GPH/Geolocation_Fields/Pressure[]";
    register_pressure_variable(product_definition, path);

    /* geopotential_height */
    description = "retrieved geopotential height";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "geopotential_height", harp_type_double, 2,
                                                   dimension_type, NULL, description, "m", NULL, read_value);
    path = "/HDFEOS/SWATHS/GPH/Data_Fields/L2gpValue[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* geopotential_height_uncertainty */
    description = "uncertainty of the retrieved geopotential height";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "geopotential_height_uncertainty",
                                                   harp_type_double, 2, dimension_type, NULL, description, "m", NULL,
                                                   read_error);
    path = "/HDFEOS/SWATHS/GPH/Data_Fields/L2gpPrecision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* geopotential_height_validity */
    description = "quality flag for the retrieved geopotential height";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "geopotential_height_validity",
                                                   harp_type_int32, 2, dimension_type, NULL, description, NULL, NULL,
                                                   read_validity);
    path = "/HDFEOS/SWATHS/GPH/Data_Fields/Status[], /HDFEOS/SWATHS/GPH/Data_Fields/Quality[], "
        "/HDFEOS/SWATHS/GPH/Data_Fields/Convergence[], /HDFEOS/SWATHS/GPH/Data_Fields/L2gpPrecision[], "
        "/HDFEOS/SWATHS/GPH/Geolocation_Fields/Pressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, "see generic mapping description");
}

static void register_h2o_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_vertical };
    const char *description;
    const char *path;

    module = harp_ingestion_register_module("MLS_L2_H2O", "MLS", "AURA_MLS", "ML2H2O", "MLS H2O profile",
                                            ingestion_init_h2o, ingestion_done);

    /* H2O product */
    product_definition = harp_ingestion_register_product(module, "MLS_L2_H2O", NULL, read_dimensions);
    harp_product_definition_add_mapping(product_definition, quality_flag_description1, NULL);
    harp_product_definition_add_mapping(product_definition, quality_flag_description2, NULL);

    /* datetime */
    path = "/HDFEOS/SWATHS/H2O/Geolocation_Fields/Time[]";
    register_datetime_variable(product_definition, path);

    /* longitude and latitude */
    path = "/HDFEOS/SWATHS/H2O/Geolocation_Fields/Longitude[]";
    register_longitude_variable(product_definition, path);
    path = "/HDFEOS/SWATHS/H2O/Geolocation_Fields/Latitude[]";
    register_latitude_variable(product_definition, path);

    /* pressure */
    path = "/HDFEOS/SWATHS/H2O/Geolocation_Fields/Pressure[]";
    register_pressure_variable(product_definition, path);

    /* H2O_volume_mixing_ratio */
    description = "H2O volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "H2O_volume_mixing_ratio", harp_type_double, 2,
                                                   dimension_type, NULL, description, "ppv", NULL, read_value);
    path = "/HDFEOS/SWATHS/H2O/Data_Fields/L2gpValue[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* H2O_volume_mixing_ratio_uncertainty */
    description = "uncertainty of the H2O volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "H2O_volume_mixing_ratio_uncertainty",
                                                   harp_type_double, 2, dimension_type, NULL, description, "ppv", NULL,
                                                   read_error);
    path = "/HDFEOS/SWATHS/H2O/Data_Fields/L2gpPrecision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* H2O_volume_mixing_ratio_validity */
    description = "quality flag for the H2O volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "H2O_volume_mixing_ratio_validity",
                                                   harp_type_int32, 2, dimension_type, NULL, description, NULL, NULL,
                                                   read_validity);
    path = "/HDFEOS/SWATHS/H2O/Data_Fields/Status[], /HDFEOS/SWATHS/H2O/Data_Fields/Quality[], "
        "/HDFEOS/SWATHS/H2O/Data_Fields/Convergence[], /HDFEOS/SWATHS/H2O/Data_Fields/L2gpPrecision[], "
        "/HDFEOS/SWATHS/H2O/Geolocation_Fields/Pressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, "see generic mapping description");
}

static void register_hcl_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_vertical };
    const char *description;
    const char *path;

    module = harp_ingestion_register_module("MLS_L2_HCL", "MLS", "AURA_MLS", "ML2HCL", "MLS HCl profile",
                                            ingestion_init_hcl, ingestion_done);

    /* HCL product */
    product_definition = harp_ingestion_register_product(module, "MLS_L2_HCL", NULL, read_dimensions);
    harp_product_definition_add_mapping(product_definition, quality_flag_description1, NULL);
    harp_product_definition_add_mapping(product_definition, quality_flag_description2, NULL);

    /* datetime */
    path = "/HDFEOS/SWATHS/HCL/Geolocation_Fields/Time[]";
    register_datetime_variable(product_definition, path);

    /* longitude and latitude */
    path = "/HDFEOS/SWATHS/HCL/Geolocation_Fields/Longitude[]";
    register_longitude_variable(product_definition, path);
    path = "/HDFEOS/SWATHS/HCL/Geolocation_Fields/Latitude[]";
    register_latitude_variable(product_definition, path);

    /* pressure */
    path = "/HDFEOS/SWATHS/HCL/Geolocation_Fields/Pressure[]";
    register_pressure_variable(product_definition, path);

    /* HCl_volume_mixing_ratio */
    description = "HCl volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "HCl_volume_mixing_ratio", harp_type_double, 2,
                                                   dimension_type, NULL, description, "ppv", NULL, read_value);
    path = "/HDFEOS/SWATHS/HCL/Data_Fields/L2gpValue[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* HCl_volume_mixing_ratio_uncertainty */
    description = "uncertainty of the HCl volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "HCl_volume_mixing_ratio_uncertainty",
                                                   harp_type_double, 2, dimension_type, NULL, description, "ppv", NULL,
                                                   read_error);
    path = "/HDFEOS/SWATHS/HCL/Data_Fields/L2gpPrecision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* HCl_volume_mixing_ratio_validity */
    description = "quality flag for the HCl volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "HCl_volume_mixing_ratio_validity",
                                                   harp_type_int32, 2, dimension_type, NULL, description, NULL, NULL,
                                                   read_validity);
    path = "/HDFEOS/SWATHS/HCl/Data_Fields/Status[], /HDFEOS/SWATHS/HCl/Data_Fields/Quality[], "
        "/HDFEOS/SWATHS/HCl/Data_Fields/Convergence[], /HDFEOS/SWATHS/HCl/Data_Fields/L2gpPrecision[], "
        "/HDFEOS/SWATHS/HCl/Geolocation_Fields/Pressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, "see generic mapping description");
}

static void register_hcn_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_vertical };
    const char *description;
    const char *path;

    module = harp_ingestion_register_module("MLS_L2_HCN", "MLS", "AURA_MLS", "ML2HCN", "MLS HCN profile",
                                            ingestion_init_hcn, ingestion_done);

    /* HCN product */
    product_definition = harp_ingestion_register_product(module, "MLS_L2_HCN", NULL, read_dimensions);
    harp_product_definition_add_mapping(product_definition, quality_flag_description1, NULL);
    harp_product_definition_add_mapping(product_definition, quality_flag_description2, NULL);

    /* datetime */
    path = "/HDFEOS/SWATHS/HCN/Geolocation_Fields/Time[]";
    register_datetime_variable(product_definition, path);

    /* longitude and latitude */
    path = "/HDFEOS/SWATHS/HCN/Geolocation_Fields/Longitude[]";
    register_longitude_variable(product_definition, path);
    path = "/HDFEOS/SWATHS/HCN/Geolocation_Fields/Latitude[]";
    register_latitude_variable(product_definition, path);

    /* pressure */
    path = "/HDFEOS/SWATHS/HCN/Geolocation_Fields/Pressure[]";
    register_pressure_variable(product_definition, path);

    /* HCN_volume_mixing_ratio */
    description = "HCN volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "HCN_volume_mixing_ratio", harp_type_double, 2,
                                                   dimension_type, NULL, description, "ppv", NULL, read_value);
    path = "/HDFEOS/SWATHS/HCN/Data_Fields/L2gpValue[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* HCN_volume_mixing_ratio_uncertainty */
    description = "uncertainty of the HCN volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "HCN_volume_mixing_ratio_uncertainty",
                                                   harp_type_double, 2, dimension_type, NULL, description, "ppv", NULL,
                                                   read_error);
    path = "/HDFEOS/SWATHS/HCN/Data_Fields/L2gpPrecision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* HCN_volume_mixing_ratio_validity */
    description = "quality flag for the HCN volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "HCN_volume_mixing_ratio_validity",
                                                   harp_type_int32, 2, dimension_type, NULL, description, NULL, NULL,
                                                   read_validity);
    path = "/HDFEOS/SWATHS/HCN/Data_Fields/Status[], /HDFEOS/SWATHS/HCN/Data_Fields/Quality[], "
        "/HDFEOS/SWATHS/HCN/Data_Fields/Convergence[], /HDFEOS/SWATHS/HCN/Data_Fields/L2gpPrecision[], "
        "/HDFEOS/SWATHS/HCN/Geolocation_Fields/Pressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, "see generic mapping description");
}

static void register_hno3_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_vertical };
    const char *description;
    const char *path;

    module = harp_ingestion_register_module("MLS_L2_HNO3", "MLS", "AURA_MLS", "ML2HNO3", "MLS HNO3 profile",
                                            ingestion_init_hno3, ingestion_done);

    /* HNO3 product */
    product_definition = harp_ingestion_register_product(module, "MLS_L2_HNO3", NULL, read_dimensions);
    harp_product_definition_add_mapping(product_definition, quality_flag_description1, NULL);
    harp_product_definition_add_mapping(product_definition, quality_flag_description2, NULL);
    harp_product_definition_add_mapping(product_definition, hno3_quality_flag_description, NULL);

    /* datetime */
    path = "/HDFEOS/SWATHS/HNO3/Geolocation_Fields/Time[]";
    register_datetime_variable(product_definition, path);

    /* longitude and latitude */
    path = "/HDFEOS/SWATHS/HNO3/Geolocation_Fields/Longitude[]";
    register_longitude_variable(product_definition, path);
    path = "/HDFEOS/SWATHS/HNO3/Geolocation_Fields/Latitude[]";
    register_latitude_variable(product_definition, path);

    /* pressure */
    path = "/HDFEOS/SWATHS/HNO3/Geolocation_Fields/Pressure[]";
    register_pressure_variable(product_definition, path);

    /* HNO3_volume_mixing_ratio */
    description = "HNO3 volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "HNO3_volume_mixing_ratio", harp_type_double, 2,
                                                   dimension_type, NULL, description, "ppv", NULL, read_value);
    path = "/HDFEOS/SWATHS/HNO3/Data_Fields/L2gpValue[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* HNO3_volume_mixing_ratio_uncertainty */
    description = "uncertainty of the HNO3 volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "HNO3_volume_mixing_ratio_uncertainty",
                                                   harp_type_double, 2, dimension_type, NULL, description, "ppv", NULL,
                                                   read_error);
    path = "/HDFEOS/SWATHS/HNO3/Data_Fields/L2gpPrecision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* HNO3_volume_mixing_ratio_validity */
    description = "quality flag for the HNO3 volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "HNO3_volume_mixing_ratio_validity",
                                                   harp_type_int32, 2, dimension_type, NULL, description, NULL, NULL,
                                                   read_validity);
    path = "/HDFEOS/SWATHS/HNO3/Data_Fields/Status[], /HDFEOS/SWATHS/HNO3/Data_Fields/Quality[], "
        "/HDFEOS/SWATHS/HNO3/Data_Fields/Convergence[], /HDFEOS/SWATHS/HNO3/Data_Fields/L2gpPrecision[], "
        "/HDFEOS/SWATHS/HNO3/Geolocation_Fields/Pressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, "see generic mapping description");
}

static void register_ho2_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_vertical };
    const char *description;
    const char *path;

    module = harp_ingestion_register_module("MLS_L2_HO2", "MLS", "AURA_MLS", "ML2HO2", "MLS HO2 profile",
                                            ingestion_init_ho2, ingestion_done);

    /* HO2 product */
    product_definition = harp_ingestion_register_product(module, "MLS_L2_HO2", NULL, read_dimensions);
    harp_product_definition_add_mapping(product_definition, quality_flag_description1, NULL);
    harp_product_definition_add_mapping(product_definition, quality_flag_description2, NULL);

    /* datetime */
    path = "/HDFEOS/SWATHS/HO2/Geolocation_Fields/Time[]";
    register_datetime_variable(product_definition, path);

    /* longitude and latitude */
    path = "/HDFEOS/SWATHS/HO2/Geolocation_Fields/Longitude[]";
    register_longitude_variable(product_definition, path);
    path = "/HDFEOS/SWATHS/HO2/Geolocation_Fields/Latitude[]";
    register_latitude_variable(product_definition, path);

    /* pressure */
    path = "/HDFEOS/SWATHS/HO2/Geolocation_Fields/Pressure[]";
    register_pressure_variable(product_definition, path);

    /* HO2_volume_mixing_ratio */
    description = "HO2 volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "HO2_volume_mixing_ratio", harp_type_double, 2,
                                                   dimension_type, NULL, description, "ppv", NULL, read_value);
    path = "/HDFEOS/SWATHS/HO2/Data_Fields/L2gpValue[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* HO2_volume_mixing_ratio_uncertainty */
    description = "uncertainty of the HO2 volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "HO2_volume_mixing_ratio_uncertainty",
                                                   harp_type_double, 2, dimension_type, NULL, description, "ppv", NULL,
                                                   read_error);
    path = "/HDFEOS/SWATHS/HO2/Data_Fields/L2gpPrecision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* HO2_volume_mixing_ratio_validity */
    description = "quality flag for the HO2 volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "HO2_volume_mixing_ratio_validity",
                                                   harp_type_int32, 2, dimension_type, NULL, description, NULL, NULL,
                                                   read_validity);
    path = "/HDFEOS/SWATHS/HO2/Data_Fields/Status[], /HDFEOS/SWATHS/HO2/Data_Fields/Quality[], "
        "/HDFEOS/SWATHS/HO2/Data_Fields/Convergence[], /HDFEOS/SWATHS/HO2/Data_Fields/L2gpPrecision[], "
        "/HDFEOS/SWATHS/HO2/Geolocation_Fields/Pressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, "see generic mapping description");
}

static void register_hocl_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_vertical };
    const char *description;
    const char *path;

    module = harp_ingestion_register_module("MLS_L2_HOCL", "MLS", "AURA_MLS", "ML2HOCL", "MLS HOCl profile",
                                            ingestion_init_hocl, ingestion_done);

    /* HOCL product */
    product_definition = harp_ingestion_register_product(module, "MLS_L2_HOCL", NULL, read_dimensions);
    harp_product_definition_add_mapping(product_definition, quality_flag_description1, NULL);
    harp_product_definition_add_mapping(product_definition, quality_flag_description2, NULL);

    /* datetime */
    path = "/HDFEOS/SWATHS/HOCL/Geolocation_Fields/Time[]";
    register_datetime_variable(product_definition, path);

    /* longitude and latitude */
    path = "/HDFEOS/SWATHS/HOCL/Geolocation_Fields/Longitude[]";
    register_longitude_variable(product_definition, path);
    path = "/HDFEOS/SWATHS/HOCL/Geolocation_Fields/Latitude[]";
    register_latitude_variable(product_definition, path);

    /* pressure */
    path = "/HDFEOS/SWATHS/HOCL/Geolocation_Fields/Pressure[]";
    register_pressure_variable(product_definition, path);

    /* HOCl_volume_mixing_ratio */
    description = "HOCl volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "HOCl_volume_mixing_ratio", harp_type_double, 2,
                                                   dimension_type, NULL, description, "ppv", NULL, read_value);
    path = "/HDFEOS/SWATHS/HOCL/Data_Fields/L2gpValue[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* HOCl_volume_mixing_ratio_uncertainty */
    description = "uncertainty of the HOCl volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "HOCl_volume_mixing_ratio_uncertainty",
                                                   harp_type_double, 2, dimension_type, NULL, description, "ppv", NULL,
                                                   read_error);
    path = "/HDFEOS/SWATHS/HOCL/Data_Fields/L2gpPrecision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* HOCl_volume_mixing_ratio_validity */
    description = "quality flag for the HOCl volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "HOCl_volume_mixing_ratio_validity",
                                                   harp_type_int32, 2, dimension_type, NULL, description, NULL, NULL,
                                                   read_validity);
    path = "/HDFEOS/SWATHS/HOCl/Data_Fields/Status[], /HDFEOS/SWATHS/HOCl/Data_Fields/Quality[], "
        "/HDFEOS/SWATHS/HOCl/Data_Fields/Convergence[], /HDFEOS/SWATHS/HOCl/Data_Fields/L2gpPrecision[], "
        "/HDFEOS/SWATHS/HOCl/Geolocation_Fields/Pressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, "see generic mapping description");
}

static void register_iwc_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_vertical };
    const char *description;
    const char *path;

    module = harp_ingestion_register_module("MLS_L2_IWC", "MLS", "AURA_MLS", "ML2IWC",
                                            "MLS ice water content profile", ingestion_init_iwc, ingestion_done);

    /* IWC product */
    product_definition = harp_ingestion_register_product(module, "MLS_L2_IWC", NULL, read_dimensions);
    harp_product_definition_add_mapping(product_definition, quality_flag_description1, NULL);
    harp_product_definition_add_mapping(product_definition, quality_flag_description2, NULL);

    /* datetime */
    path = "/HDFEOS/SWATHS/IWC/Geolocation_Fields/Time[]";
    register_datetime_variable(product_definition, path);

    /* longitude and latitude */
    path = "/HDFEOS/SWATHS/IWC/Geolocation_Fields/Longitude[]";
    register_longitude_variable(product_definition, path);
    path = "/HDFEOS/SWATHS/IWC/Geolocation_Fields/Latitude[]";
    register_latitude_variable(product_definition, path);

    /* pressure */
    path = "/HDFEOS/SWATHS/IWC/Geolocation_Fields/Pressure[]";
    register_pressure_variable(product_definition, path);

    /* ice_water_density */
    description = "Ice water content";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "ice_water_density", harp_type_double, 2,
                                                   dimension_type, NULL, description, "g/m^3", NULL, read_value);
    path = "/HDFEOS/SWATHS/IWC/Data_Fields/L2gpValue[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* ice_water_density_uncertainty */
    description = "uncertainty of the ice water content";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "ice_water_density_uncertainty",
                                                   harp_type_double, 2, dimension_type, NULL, description, "g/m^3",
                                                   NULL, read_value);
    path = "/HDFEOS/SWATHS/IWC/Data_Fields/L2gpPrecision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* ice_water_density_validity */
    description = "quality flag for the ice water content";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "ice_water_density_validity",
                                                   harp_type_int32, 2, dimension_type, NULL, description, NULL, NULL,
                                                   read_validity);
    path = "/HDFEOS/SWATHS/IWC/Data_Fields/Status[], /HDFEOS/SWATHS/IWC/Data_Fields/Quality[], "
        "/HDFEOS/SWATHS/IWC/Data_Fields/Convergence[], /HDFEOS/SWATHS/IWC/Data_Fields/L2gpPrecision[], "
        "/HDFEOS/SWATHS/IWC/Geolocation_Fields/Pressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, "see generic mapping description");
}

static void register_n2o_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_vertical };
    const char *description;
    const char *path;

    module = harp_ingestion_register_module("MLS_L2_N2O", "MLS", "AURA_MLS", "ML2N2O", "MLS N2O profile",
                                            ingestion_init_n2o, ingestion_done);

    /* N2O product */
    product_definition = harp_ingestion_register_product(module, "MLS_L2_N2O", NULL, read_dimensions);
    harp_product_definition_add_mapping(product_definition, quality_flag_description1, NULL);
    harp_product_definition_add_mapping(product_definition, quality_flag_description2, NULL);

    /* datetime */
    path = "/HDFEOS/SWATHS/N2O/Geolocation_Fields/Time[]";
    register_datetime_variable(product_definition, path);

    /* longitude and latitude */
    path = "/HDFEOS/SWATHS/N2O/Geolocation_Fields/Longitude[]";
    register_longitude_variable(product_definition, path);
    path = "/HDFEOS/SWATHS/N2O/Geolocation_Fields/Latitude[]";
    register_latitude_variable(product_definition, path);

    /* pressure */
    path = "/HDFEOS/SWATHS/N2O/Geolocation_Fields/Pressure[]";
    register_pressure_variable(product_definition, path);

    /* N2O_volume_mixing_ratio */
    description = "N2O volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "N2O_volume_mixing_ratio", harp_type_double, 2,
                                                   dimension_type, NULL, description, "ppv", NULL, read_value);
    path = "/HDFEOS/SWATHS/N2O/Data_Fields/L2gpValue[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* N2O_volume_mixing_ratio_uncertainty */
    description = "uncertainty of the N2O volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "N2O_volume_mixing_ratio_uncertainty",
                                                   harp_type_double, 2, dimension_type, NULL, description, "ppv", NULL,
                                                   read_error);
    path = "/HDFEOS/SWATHS/N2O/Data_Fields/L2gpPrecision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* N2O_volume_mixing_ratio_validity */
    description = "quality flag for the N2O volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "N2O_volume_mixing_ratio_validity",
                                                   harp_type_int32, 2, dimension_type, NULL, description, NULL, NULL,
                                                   read_validity);
    path = "/HDFEOS/SWATHS/N2O/Data_Fields/Status[], /HDFEOS/SWATHS/N2O/Data_Fields/Quality[], "
        "/HDFEOS/SWATHS/N2O/Data_Fields/Convergence[], /HDFEOS/SWATHS/N2O/Data_Fields/L2gpPrecision[], "
        "/HDFEOS/SWATHS/N2O/Geolocation_Fields/Pressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, "see generic mapping description");
}

static void register_o3_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_vertical };
    const char *description;
    const char *path;

    module = harp_ingestion_register_module("MLS_L2_O3", "MLS", "AURA_MLS", "ML2O3", "MLS O3 profile",
                                            ingestion_init_o3, ingestion_done);

    /* O3 product */
    product_definition = harp_ingestion_register_product(module, "MLS_L2_O3", NULL, read_dimensions);
    harp_product_definition_add_mapping(product_definition, quality_flag_description1, NULL);
    harp_product_definition_add_mapping(product_definition, quality_flag_description2, NULL);

    /* datetime */
    path = "/HDFEOS/SWATHS/O3/Geolocation_Fields/Time[]";
    register_datetime_variable(product_definition, path);

    /* longitude and latitude */
    path = "/HDFEOS/SWATHS/O3/Geolocation_Fields/Longitude[]";
    register_longitude_variable(product_definition, path);
    path = "/HDFEOS/SWATHS/O3/Geolocation_Fields/Latitude[]";
    register_latitude_variable(product_definition, path);

    /* pressure */
    path = "/HDFEOS/SWATHS/O3/Geolocation_Fields/Pressure[]";
    register_pressure_variable(product_definition, path);

    /* O3_volume_mixing_ratio */
    description = "O3 volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_volume_mixing_ratio", harp_type_double, 2,
                                                   dimension_type, NULL, description, "ppv", NULL, read_value);
    path = "/HDFEOS/SWATHS/O3/Data_Fields/L2gpValue[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* O3_volume_mixing_ratio_uncertainty */
    description = "uncertainty of the O3 volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_volume_mixing_ratio_uncertainty",
                                                   harp_type_double, 2, dimension_type, NULL, description, "ppv", NULL,
                                                   read_error);
    path = "/HDFEOS/SWATHS/O3/Data_Fields/L2gpPrecision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* O3_volume_mixing_ratio_validity */
    description = "quality flag for the O3 volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "O3_volume_mixing_ratio_validity",
                                                   harp_type_int32, 2, dimension_type, NULL, description, NULL, NULL,
                                                   read_validity);
    path = "/HDFEOS/SWATHS/O3/Data_Fields/Status[], /HDFEOS/SWATHS/O3/Data_Fields/Quality[], "
        "/HDFEOS/SWATHS/O3/Data_Fields/Convergence[], /HDFEOS/SWATHS/O3/Data_Fields/L2gpPrecision[], "
        "/HDFEOS/SWATHS/O3/Geolocation_Fields/Pressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, "see generic mapping description");
}

static void register_oh_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_vertical };
    const char *description;
    const char *path;

    module = harp_ingestion_register_module("MLS_L2_OH", "MLS", "AURA_MLS", "ML2OH", "MLS OH profile",
                                            ingestion_init_oh, ingestion_done);

    /* OH product */
    product_definition = harp_ingestion_register_product(module, "MLS_L2_OH", NULL, read_dimensions);
    harp_product_definition_add_mapping(product_definition, quality_flag_description1, NULL);
    harp_product_definition_add_mapping(product_definition, quality_flag_description2, NULL);

    /* datetime */
    path = "/HDFEOS/SWATHS/OH/Geolocation_Fields/Time[]";
    register_datetime_variable(product_definition, path);

    /* longitude and latitude */
    path = "/HDFEOS/SWATHS/OH/Geolocation_Fields/Longitude[]";
    register_longitude_variable(product_definition, path);
    path = "/HDFEOS/SWATHS/OH/Geolocation_Fields/Latitude[]";
    register_latitude_variable(product_definition, path);

    /* pressure */
    path = "/HDFEOS/SWATHS/OH/Geolocation_Fields/Pressure[]";
    register_pressure_variable(product_definition, path);

    /* OH_volume_mixing_ratio */
    description = "OH volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "OH_volume_mixing_ratio", harp_type_double, 2,
                                                   dimension_type, NULL, description, "ppv", NULL, read_value);
    path = "/HDFEOS/SWATHS/OH/Data_Fields/L2gpValue[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* OH_volume_mixing_ratio_uncertainty */
    description = "uncertainty of the OH volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "OH_volume_mixing_ratio_uncertainty",
                                                   harp_type_double, 2, dimension_type, NULL, description, "ppv", NULL,
                                                   read_error);
    path = "/HDFEOS/SWATHS/OH/Data_Fields/L2gpPrecision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* OH_volume_mixing_ratio_validity */
    description = "quality flag for the OH volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "OH_volume_mixing_ratio_validity",
                                                   harp_type_int32, 2, dimension_type, NULL, description, NULL, NULL,
                                                   read_validity);
    path = "/HDFEOS/SWATHS/OH/Data_Fields/Status[], /HDFEOS/SWATHS/OH/Data_Fields/Quality[], "
        "/HDFEOS/SWATHS/OH/Data_Fields/Convergence[], /HDFEOS/SWATHS/OH/Data_Fields/L2gpPrecision[], "
        "/HDFEOS/SWATHS/OH/Geolocation_Fields/Pressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, "see generic mapping description");
}

static void register_rhi_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_vertical };
    const char *description;
    const char *path;

    module = harp_ingestion_register_module("MLS_L2_RHI", "MLS", "AURA_MLS", "ML2RHI",
                                            "MLS relative humidity with respect to ice profile",
                                            ingestion_init_rhi, ingestion_done);

    /* RHI product */
    product_definition = harp_ingestion_register_product(module, "MLS_L2_RHI", NULL, read_dimensions);
    harp_product_definition_add_mapping(product_definition, quality_flag_description1, NULL);
    harp_product_definition_add_mapping(product_definition, quality_flag_description2, NULL);

    /* datetime */
    path = "/HDFEOS/SWATHS/RHI/Geolocation_Fields/Time[]";
    register_datetime_variable(product_definition, path);

    /* longitude and latitude */
    path = "/HDFEOS/SWATHS/RHI/Geolocation_Fields/Longitude[]";
    register_longitude_variable(product_definition, path);
    path = "/HDFEOS/SWATHS/RHI/Geolocation_Fields/Latitude[]";
    register_latitude_variable(product_definition, path);

    /* pressure */
    path = "/HDFEOS/SWATHS/RHI/Geolocation_Fields/Pressure[]";
    register_pressure_variable(product_definition, path);

    /* relative_humidity_ice */
    description = "relative humidity with respect to ice";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "relative_humidity_ice", harp_type_double, 2,
                                                   dimension_type, NULL, description, "%", NULL, read_value);
    path = "/HDFEOS/SWATHS/RHI/Data_Fields/L2gpValue[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* relative_humidity_ice_uncertainty */
    description = "uncertainty of the relative humidity with respect to ice";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "relative_humidity_ice_uncertainty",
                                                   harp_type_double, 2, dimension_type, NULL, description, "%", NULL,
                                                   read_error);
    path = "/HDFEOS/SWATHS/RHI/Data_Fields/L2gpPrecision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* relative_humidity_ice_validity */
    description = "quality flag for the relative humidity with respect to ice";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "relative_humidity_ice_validity",
                                                   harp_type_int32, 2, dimension_type, NULL, description, NULL, NULL,
                                                   read_validity);
    path = "/HDFEOS/SWATHS/RHI/Data_Fields/Status[], /HDFEOS/SWATHS/RHI/Data_Fields/Quality[], "
        "/HDFEOS/SWATHS/RHI/Data_Fields/Convergence[], /HDFEOS/SWATHS/RHI/Data_Fields/L2gpPrecision[], "
        "/HDFEOS/SWATHS/RHI/Geolocation_Fields/Pressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, "see generic mapping description");
}

static void register_so2_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_vertical };
    const char *description;
    const char *path;

    module = harp_ingestion_register_module("MLS_L2_SO2", "MLS", "AURA_MLS", "ML2SO2", "MLS SO2 profile",
                                            ingestion_init_so2, ingestion_done);

    /* SO2 product */
    product_definition = harp_ingestion_register_product(module, "MLS_L2_SO2", NULL, read_dimensions);
    harp_product_definition_add_mapping(product_definition, quality_flag_description1, NULL);
    harp_product_definition_add_mapping(product_definition, quality_flag_description2, NULL);

    /* datetime */
    path = "/HDFEOS/SWATHS/SO2/Geolocation_Fields/Time[]";
    register_datetime_variable(product_definition, path);

    /* longitude and latitude */
    path = "/HDFEOS/SWATHS/SO2/Geolocation_Fields/Longitude[]";
    register_longitude_variable(product_definition, path);
    path = "/HDFEOS/SWATHS/SO2/Geolocation_Fields/Latitude[]";
    register_latitude_variable(product_definition, path);

    /* pressure */
    path = "/HDFEOS/SWATHS/SO2/Geolocation_Fields/Pressure[]";
    register_pressure_variable(product_definition, path);

    /* SO2_volume_mixing_ratio */
    description = "SO2 volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "SO2_volume_mixing_ratio", harp_type_double, 2,
                                                   dimension_type, NULL, description, "ppv", NULL, read_value);
    path = "/HDFEOS/SWATHS/SO2/Data_Fields/L2gpValue[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* SO2_volume_mixing_ratio_uncertainty */
    description = "uncertainty of the SO2 volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "SO2_volume_mixing_ratio_uncertainty",
                                                   harp_type_double, 2, dimension_type, NULL, description, "ppv", NULL,
                                                   read_error);
    path = "/HDFEOS/SWATHS/SO2/Data_Fields/L2gpPrecision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* SO2_volume_mixing_ratio_validity */
    description = "quality flag for the SO2 volume mixing ratio";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "SO2_volume_mixing_ratio_validity",
                                                   harp_type_int32, 2, dimension_type, NULL, description, NULL, NULL,
                                                   read_validity);
    path = "/HDFEOS/SWATHS/SO2/Data_Fields/Status[], /HDFEOS/SWATHS/SO2/Data_Fields/Quality[], "
        "/HDFEOS/SWATHS/SO2/Data_Fields/Convergence[], /HDFEOS/SWATHS/SO2/Data_Fields/L2gpPrecision[], "
        "/HDFEOS/SWATHS/SO2/Geolocation_Fields/Pressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, "see generic mapping description");
}

static void register_t_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_vertical };
    const char *description;
    const char *path;

    module = harp_ingestion_register_module("MLS_L2_T", "MLS", "AURA_MLS", "ML2T", "MLS temperature profile",
                                            ingestion_init_t, ingestion_done);

    /* T product */
    product_definition = harp_ingestion_register_product(module, "MLS_L2_T", NULL, read_dimensions);
    harp_product_definition_add_mapping(product_definition, quality_flag_description1, NULL);
    harp_product_definition_add_mapping(product_definition, quality_flag_description2, NULL);

    /* datetime */
    path = "/HDFEOS/SWATHS/Temperature/Geolocation_Fields/Time[]";
    register_datetime_variable(product_definition, path);

    /* longitude and latitude */
    path = "/HDFEOS/SWATHS/Temperature/Geolocation_Fields/Longitude[]";
    register_longitude_variable(product_definition, path);
    path = "/HDFEOS/SWATHS/Temperature/Geolocation_Fields/Latitude[]";
    register_latitude_variable(product_definition, path);

    /* pressure */
    path = "/HDFEOS/SWATHS/Temperature/Geolocation_Fields/Pressure[]";
    register_pressure_variable(product_definition, path);

    /* temperature */
    description = "temperature";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "temperature", harp_type_double, 2,
                                                   dimension_type, NULL, description, "K", NULL, read_value);
    path = "/HDFEOS/SWATHS/Temperature/Data_Fields/L2gpValue[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* temperature_uncertainty */
    description = "uncertainty of the temperature";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "temperature_uncertainty", harp_type_double, 2,
                                                   dimension_type, NULL, description, "K", NULL, read_error);
    path = "/HDFEOS/SWATHS/Temperature/Data_Fields/L2gpPrecision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* temperature_validity */
    description = "quality flag for the temperature";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "temperature_validity",
                                                   harp_type_int32, 2, dimension_type, NULL, description, NULL, NULL,
                                                   read_validity);
    path = "/HDFEOS/SWATHS/Temperature/Data_Fields/Status[], /HDFEOS/SWATHS/Temperature/Data_Fields/Quality[], "
        "/HDFEOS/SWATHS/Temperature/Data_Fields/Convergence[], /HDFEOS/SWATHS/Temperature/Data_Fields/L2gpPrecision[], "
        "/HDFEOS/SWATHS/Temperature/Geolocation_Fields/Pressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, "see generic mapping description");
}

int harp_ingestion_module_mls_l2_init(void)
{
    register_bro_product();
    register_ch3cl_product();
    register_ch3cn_product();
    register_ch3oh_product();
    register_clo_product();
    register_co_product();
    register_gph_product();
    register_h2o_product();
    register_hcl_product();
    register_hcn_product();
    register_hno3_product();
    register_ho2_product();
    register_hocl_product();
    register_iwc_product();
    register_n2o_product();
    register_o3_product();
    register_oh_product();
    register_rhi_product();
    register_so2_product();
    register_t_product();

    return 0;
}
