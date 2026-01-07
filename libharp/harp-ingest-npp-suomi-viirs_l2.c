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
#include <stdio.h>

/* ------------------- Defines ------------------ */

#define CHECKED_MALLOC(v, s) v = malloc(s); if (v == NULL) { harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)", s, __FILE__, __LINE__); return -1;}

#define MICROSECONDS_IN_SECOND                 1000000
#define SECONDS_FROM_1958_TO_2000           1325376000

/* ------------------ Typedefs ------------------ */

typedef enum VIIRS_product_type_enum
{
    AEROSOL_OPTICAL_DEPTH,
    CLOUD_BASE_HEIGHT,
    CLOUD_FRACTION,
    CLOUD_EFFECTIVE_PARTICLE_SIZE,
    CLOUD_OPTICAL_DEPTH,
    CLOUD_TOP_HEIGHT,
    CLOUD_TOP_PRESSURE,
    CLOUD_TOP_TEMPERATURE,
    SUSPENDED_MATTER,
    CLOUD_MASK,
    NUM_VIIRS_PRODUCT_TYPES
} VIIRS_product_type;

typedef struct ingest_info_struct
{
    coda_product *product;
    const char *geo_swath_name;
    const char *viirs_swath_names[NUM_VIIRS_PRODUCT_TYPES];
    coda_cursor geo_cursor;
    coda_cursor viirs_cursors[NUM_VIIRS_PRODUCT_TYPES];
    long num_times;
    long num_measurements_alongtrack;
    long num_crosstracks;
} ingest_info;

/* -------------- Global variables --------------- */

static double nan;

static const char *viirs_swath_name_ends[NUM_VIIRS_PRODUCT_TYPES] = {
    "_Aeros_EDR_All",
    "_CBH_EDR_All",
    "_CCL_EDR_All",
    "_CEPS_EDR_All",
    "_COT_EDR_All",
    "_CTH_EDR_All",
    "_CTP_EDR_All",
    "_CTT_EDR_All",
    "_SusMat_EDR_All",
    "_CM_EDR_All"
};

static const short aerosol_optical_depth_wavelengths[] = {
    412,
    445,
    488,
    550,
    555,
    672,
    746,
    865,
    1240,
    1610,
    2250
};

/* -------------------- Code -------------------- */

static void ingestion_done(void *user_data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info != NULL)
    {
        free(info);
    }
}

static int read_variable(coda_cursor *cursor, const char *name, int num_dimensions, long dimension_0, long dimension_1,
                         double error_range_start, double error_range_end, harp_array data)
{
    double *double_data;
    long num_elements, i;
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
        harp_set_error(HARP_ERROR_INGESTION,
                       "product error detected in NPP Suomi L2 product (variable %s has %d dimensions, " "expected %d)",
                       name, num_coda_dimensions, num_dimensions);
        return -1;
    }
    if (dimension_0 != coda_dimension[0])
    {
        harp_set_error(HARP_ERROR_INGESTION,
                       "product error detected in NPP Suomi L2 product (first dimension for variable %s "
                       "has %ld elements, expected %ld", name, coda_dimension[0], dimension_0);
        return -1;
    }
    num_elements = coda_dimension[0];
    if (num_dimensions > 1)
    {
        if (dimension_1 != coda_dimension[1])
        {
            harp_set_error(HARP_ERROR_INGESTION, "product error detected in NPP Suomi L2 product (second dimension for "
                           "variable %s has %ld elements, expected %ld", name, coda_dimension[1], dimension_1);
            return -1;
        }
        num_elements *= coda_dimension[1];
    }
    if (coda_cursor_read_double_array(cursor, data.double_data, coda_array_ordering_c) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    if (error_range_start <= error_range_end)
    {
        double_data = data.double_data;
        for (i = 0; i < num_elements; i++)
        {
            if ((*double_data >= error_range_start) && (*double_data <= error_range_end))
            {
                *double_data = nan;
            }
            double_data++;
        }
    }

    coda_cursor_goto_parent(cursor);

    return 0;
}

static int read_dimensions(void *user_data, long dimension[HARP_NUM_DIM_TYPES])
{
    ingest_info *info = (ingest_info *)user_data;

    dimension[harp_dimension_time] = info->num_measurements_alongtrack * info->num_crosstracks;
    dimension[harp_dimension_spectral] = sizeof(aerosol_optical_depth_wavelengths) / sizeof(short);

    return 0;
}

static int read_datetime(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    harp_array start_times, middle_times;
    double *double_data, timestep, curtime;
    long num_timesteps, i, j;

    CHECKED_MALLOC(start_times.double_data, info->num_times * sizeof(double));
    if (read_variable(&info->geo_cursor, "StartTime", 1, info->num_times, 0, -999.5, -992.5, start_times) != 0)
    {
        free(start_times.double_data);
        return -1;
    }
    CHECKED_MALLOC(middle_times.double_data, info->num_times * sizeof(double));
    if (read_variable(&info->geo_cursor, "MidTime", 1, info->num_times, 0, -999.5, -992.5, middle_times) != 0)
    {
        free(middle_times.double_data);
        free(start_times.double_data);
        return -1;
    }

    num_timesteps = (info->num_measurements_alongtrack * info->num_crosstracks) / info->num_times;
    double_data = data.double_data;
    for (i = 0; i < info->num_times; i++)
    {
        if (!coda_isNaN(start_times.double_data[i]) && !coda_isNaN(middle_times.double_data[i]))
        {
            timestep =
                (2.0 * (middle_times.double_data[i] - start_times.double_data[i]) /
                 (num_timesteps * MICROSECONDS_IN_SECOND));
            curtime = (start_times.double_data[i] / MICROSECONDS_IN_SECOND) - SECONDS_FROM_1958_TO_2000;
            for (j = 0; j < num_timesteps; j++)
            {
                *double_data = curtime;
                double_data++;
                curtime += timestep;
            }
        }
        else
        {
            for (j = 0; j < num_timesteps; j++)
            {
                *double_data = nan;
                double_data++;
            }
        }
    }

    free(middle_times.double_data);
    free(start_times.double_data);
    return 0;
}

static int read_latitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable(&info->geo_cursor, "Latitude", 2, info->num_measurements_alongtrack, info->num_crosstracks,
                         -1000.0, -999.0, data);
}

static int read_longitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable(&info->geo_cursor, "Longitude", 2, info->num_measurements_alongtrack, info->num_crosstracks,
                         -1000.0, -999.0, data);
}

static int read_sensor_azimuth_angle(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable(&info->geo_cursor, "SatelliteAzimuthAngle", 2, info->num_measurements_alongtrack,
                         info->num_crosstracks, -1000.0, -999.0, data);
}

static int read_sensor_zenith_angle(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable(&info->geo_cursor, "SatelliteZenithAngle", 2, info->num_measurements_alongtrack,
                         info->num_crosstracks, -1000.0, -999.0, data);
}

static int read_solar_azimuth_angle(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable(&info->geo_cursor, "SolarAzimuthAngle", 2, info->num_measurements_alongtrack,
                         info->num_crosstracks, -1000.0, -999.0, data);
}

static int read_solar_zenith_angle(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable(&info->geo_cursor, "SolarZenithAngle", 2, info->num_measurements_alongtrack,
                         info->num_crosstracks, -1000.0, -999.0, data);
}

static int read_aerosol_optical_depth(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    double *src, *dest;
    harp_array aod_one_angle;
    long j;
    unsigned short i;
    char fieldname[81];

    CHECKED_MALLOC(aod_one_angle.double_data, info->num_measurements_alongtrack * info->num_crosstracks *
                   sizeof(double));

    for (i = 0; i < sizeof(aerosol_optical_depth_wavelengths) / sizeof(short); i++)
    {
        sprintf(fieldname, "AerosolOpticalDepth_at_%dnm", aerosol_optical_depth_wavelengths[i]);
        if (read_variable(&info->viirs_cursors[AEROSOL_OPTICAL_DEPTH], fieldname, 2,
                          info->num_measurements_alongtrack, info->num_crosstracks, 65527.5, 65535.5, aod_one_angle)
            != 0)
        {
            free(aod_one_angle.double_data);
            return -1;
        }
        src = aod_one_angle.double_data;
        dest = data.double_data + i;
        for (j = 0; j < (info->num_measurements_alongtrack * info->num_crosstracks); j++)
        {
            *dest = *src;
            dest += sizeof(aerosol_optical_depth_wavelengths) / sizeof(short);
            src++;
        }
    }

    free(aod_one_angle.double_data);
    return 0;
}

static int read_wavelength(void *user_data, harp_array data)
{
    unsigned short i;

    (void)user_data;    /* ignore */
    for (i = 0; i < sizeof(aerosol_optical_depth_wavelengths) / sizeof(short); i++)
    {
        data.double_data[i] = (double)aerosol_optical_depth_wavelengths[i];
    }

    return 0;
}

static int read_cloud_base_height(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    double *double_data, scale, offset;
    harp_array factors;
    long i;

    if (read_variable(&info->viirs_cursors[CLOUD_BASE_HEIGHT], "AverageCloudBaseHeight", 2,
                      info->num_measurements_alongtrack, info->num_crosstracks, 65527.5, 65535.5, data) != 0)
    {
        return -1;
    }
    CHECKED_MALLOC(factors.double_data, 8 * sizeof(double));
    if (read_variable(&info->viirs_cursors[CLOUD_BASE_HEIGHT], "CBHFactors", 1, 8, 0, 1.0, 0.0, factors) != 0)
    {
        free(factors.double_data);
        return -1;
    }
    scale = factors.double_data[0];
    offset = factors.double_data[1];
    free(factors.double_data);

    double_data = data.double_data;
    for (i = 0; i < (info->num_measurements_alongtrack * info->num_crosstracks); i++)
    {
        if (!coda_isNaN(*double_data))
        {
            *double_data = *double_data * scale + offset;
        }
        double_data++;
    }
    return 0;
}

static int read_cloud_top_height(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    double *double_data, scale, offset;
    harp_array factors;
    long i;

    if (read_variable(&info->viirs_cursors[CLOUD_TOP_HEIGHT], "AverageCloudTopHeight", 2,
                      info->num_measurements_alongtrack, info->num_crosstracks, 65527.5, 65535.5, data) != 0)
    {
        return -1;
    }
    CHECKED_MALLOC(factors.double_data, 8 * sizeof(double));
    if (read_variable(&info->viirs_cursors[CLOUD_TOP_HEIGHT], "CTHFactors", 1, 8, 0, 1.0, 0.0, factors) != 0)
    {
        free(factors.double_data);
        return -1;
    }
    scale = factors.double_data[0];
    offset = factors.double_data[1];
    free(factors.double_data);

    double_data = data.double_data;
    for (i = 0; i < (info->num_measurements_alongtrack * info->num_crosstracks); i++)
    {
        if (!coda_isNaN(*double_data))
        {
            *double_data = *double_data * scale + offset;
        }
        double_data++;
    }
    return 0;
}

static int read_cloud_top_pressure(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    double *double_data, scale, offset;
    harp_array factors;
    long i;

    if (read_variable(&info->viirs_cursors[CLOUD_TOP_PRESSURE], "AverageCloudTopPressure", 2,
                      info->num_measurements_alongtrack, info->num_crosstracks, 65527.5, 65535.5, data) != 0)
    {
        return -1;
    }
    CHECKED_MALLOC(factors.double_data, 8 * sizeof(double));
    if (read_variable(&info->viirs_cursors[CLOUD_TOP_PRESSURE], "CTPFactors", 1, 8, 0, 1.0, 0.0, factors) != 0)
    {
        free(factors.double_data);
        return -1;
    }
    scale = factors.double_data[0];
    offset = factors.double_data[1];
    free(factors.double_data);

    double_data = data.double_data;
    for (i = 0; i < (info->num_measurements_alongtrack * info->num_crosstracks); i++)
    {
        if (!coda_isNaN(*double_data))
        {
            *double_data = *double_data * scale + offset;
        }
        double_data++;
    }
    return 0;
}

static int read_cloud_top_temperature(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    double *double_data, scale, offset;
    harp_array factors;
    long i;

    if (read_variable(&info->viirs_cursors[CLOUD_TOP_TEMPERATURE], "AverageCloudTopTemperature", 2,
                      info->num_measurements_alongtrack, info->num_crosstracks, 65527.5, 65535.5, data) != 0)
    {
        return -1;
    }
    CHECKED_MALLOC(factors.double_data, 8 * sizeof(double));
    if (read_variable(&info->viirs_cursors[CLOUD_TOP_TEMPERATURE], "CTTFactors", 1, 8, 0, 1.0, 0.0, factors) != 0)
    {
        free(factors.double_data);
        return -1;
    }
    scale = factors.double_data[0];
    offset = factors.double_data[1];
    free(factors.double_data);

    double_data = data.double_data;
    for (i = 0; i < (info->num_measurements_alongtrack * info->num_crosstracks); i++)
    {
        if (!coda_isNaN(*double_data))
        {
            *double_data = *double_data * scale + offset;
        }
        double_data++;
    }
    return 0;
}

static int read_cloud_fraction(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    double *double_data, scale, offset;
    harp_array factors;
    long i;

    if (read_variable(&info->viirs_cursors[CLOUD_FRACTION], "SummedCloudCover", 2,
                      info->num_measurements_alongtrack, info->num_crosstracks, 65527.5, 65535.5, data) != 0)
    {
        return -1;
    }
    CHECKED_MALLOC(factors.double_data, 8 * sizeof(double));
    if (read_variable(&info->viirs_cursors[CLOUD_FRACTION], "CCLFactors", 1, 8, 0, 1.0, 0.0, factors) != 0)
    {
        free(factors.double_data);
        return -1;
    }
    scale = factors.double_data[0];
    offset = factors.double_data[1];
    free(factors.double_data);

    double_data = data.double_data;
    for (i = 0; i < (info->num_measurements_alongtrack * info->num_crosstracks); i++)
    {
        if (!coda_isNaN(*double_data))
        {
            *double_data = *double_data * scale + offset;
        }
        double_data++;
    }
    return 0;
}

static int read_cloud_effective_particle_size(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    double *double_data, scale, offset;
    harp_array factors;
    long i;

    if (read_variable(&info->viirs_cursors[CLOUD_EFFECTIVE_PARTICLE_SIZE], "AverageCloudEffectiveParticleSize", 2,
                      info->num_measurements_alongtrack, info->num_crosstracks, 65527.5, 65535.5, data) != 0)
    {
        return -1;
    }
    CHECKED_MALLOC(factors.double_data, 8 * sizeof(double));
    if (read_variable
        (&info->viirs_cursors[CLOUD_EFFECTIVE_PARTICLE_SIZE], "CEPSFactors", 1, 8, 0, 1.0, 0.0, factors) != 0)
    {
        free(factors.double_data);
        return -1;
    }
    scale = factors.double_data[0];
    offset = factors.double_data[1];
    free(factors.double_data);

    double_data = data.double_data;
    for (i = 0; i < (info->num_measurements_alongtrack * info->num_crosstracks); i++)
    {
        if (!coda_isNaN(*double_data))
        {
            *double_data = *double_data * scale + offset;
        }
        double_data++;
    }
    return 0;
}

static int read_cloud_optical_depth(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    double *double_data, scale, offset;
    harp_array factors;
    long i;

    if (read_variable(&info->viirs_cursors[CLOUD_OPTICAL_DEPTH], "AverageCloudOpticalThickness", 2,
                      info->num_measurements_alongtrack, info->num_crosstracks, 65527.5, 65535.5, data) != 0)
    {
        return -1;
    }
    CHECKED_MALLOC(factors.double_data, 8 * sizeof(double));
    if (read_variable(&info->viirs_cursors[CLOUD_OPTICAL_DEPTH], "COTFactors", 1, 8, 0, 1.0, 0.0, factors) != 0)
    {
        free(factors.double_data);
        return -1;
    }
    scale = factors.double_data[0];
    offset = factors.double_data[1];
    free(factors.double_data);

    double_data = data.double_data;
    for (i = 0; i < (info->num_measurements_alongtrack * info->num_crosstracks); i++)
    {
        if (!coda_isNaN(*double_data))
        {
            *double_data = *double_data * scale + offset;
        }
        double_data++;
    }
    return 0;
}

static int init_swath_names_and_cursors(ingest_info *info)
{
    const char *swath_name;
    coda_cursor cursor;
    coda_type *type;
    long num_swaths, swath_index, i;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&cursor, "All_Data") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_type(&cursor, &type) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_type_get_num_record_fields(type, &num_swaths) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    for (swath_index = 0; swath_index < num_swaths; swath_index++)
    {
        if (coda_type_get_record_field_name(type, swath_index, &swath_name) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_goto_record_field_by_index(&cursor, swath_index) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if ((strcmp(swath_name + strlen(swath_name) - 7, "GEO_All") == 0) ||
            (strcmp(swath_name + strlen(swath_name) - 10, "GEO_TC_All") == 0))
        {
            info->geo_swath_name = swath_name;
            info->geo_cursor = cursor;
        }
        for (i = 0; i < NUM_VIIRS_PRODUCT_TYPES; i++)
        {
            if (strcmp(swath_name + strlen(swath_name) - strlen(viirs_swath_name_ends[i]), viirs_swath_name_ends[i]) ==
                0)
            {
                info->viirs_swath_names[i] = swath_name;
                info->viirs_cursors[i] = cursor;
            }
        }
        if (coda_cursor_goto_parent(&cursor) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
    }

    return 0;
}

static int get_dimensions(ingest_info *info)
{
    coda_cursor cursor;
    long coda_dimension[CODA_MAX_NUM_DIMS];
    int num_coda_dimensions;

    cursor = info->geo_cursor;
    if (coda_cursor_goto_record_field_by_name(&cursor, "Latitude") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_array_dim(&cursor, &num_coda_dimensions, coda_dimension) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    info->num_measurements_alongtrack = coda_dimension[0];
    info->num_crosstracks = coda_dimension[1];

    cursor = info->geo_cursor;
    if (coda_cursor_goto_record_field_by_name(&cursor, "StartTime") != 0)
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

    return 0;
}

static int ingestion_init(const harp_ingestion_module *module, coda_product *product,
                          const harp_ingestion_options *options, harp_product_definition **definition, void **user_data)
{
    ingest_info *info;

    (void)options;

    CHECKED_MALLOC(info, sizeof(ingest_info));
    memset(info, '\0', sizeof(ingest_info));
    info->product = product;

    if (init_swath_names_and_cursors(info) != 0)
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

    nan = coda_NaN();

    return 0;
}

static int include_non_cloud_base_height(void *user_data)
{
    return ((ingest_info *)user_data)->viirs_swath_names[CLOUD_BASE_HEIGHT] != NULL;
}

static int include_non_cloud_top_height(void *user_data)
{
    return ((ingest_info *)user_data)->viirs_swath_names[CLOUD_TOP_HEIGHT] != NULL;
}

static int include_non_cloud_top_pressure(void *user_data)
{
    return ((ingest_info *)user_data)->viirs_swath_names[CLOUD_TOP_PRESSURE] != NULL;
}

static int include_non_cloud_top_temperature(void *user_data)
{
    return ((ingest_info *)user_data)->viirs_swath_names[CLOUD_TOP_TEMPERATURE] != NULL;
}

static int include_non_cloud_fraction(void *user_data)
{
    return ((ingest_info *)user_data)->viirs_swath_names[CLOUD_FRACTION] != NULL;
}

static int include_non_cloud_effective_particle_size(void *user_data)
{
    return ((ingest_info *)user_data)->viirs_swath_names[CLOUD_EFFECTIVE_PARTICLE_SIZE] != NULL;
}

static int include_non_cloud_optical_depth(void *user_data)
{
    return ((ingest_info *)user_data)->viirs_swath_names[CLOUD_OPTICAL_DEPTH] != NULL;
}

static void register_aeros_product_type(const char *product_type)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_spectral };
    const char *description;
    const char *path;

    module = harp_ingestion_register_module("NPP_SUOMI_L2_VIIRS_EDR_VAOO", "NPP", "NPP_SUOMI", product_type,
                                            "NPP Suomi VIIRS EDR Aerosol Optical Thickness", ingestion_init,
                                            ingestion_done);

    product_definition = harp_ingestion_register_product(module, "NPP_SUOMI_L2_VIIRS_EDR_VAOO", NULL, read_dimensions);

    /* datetime */
    description = "time of the measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double, 1, dimension_type,
                                                   NULL, description, "seconds since 2000-01-01", NULL, read_datetime);
    description = "the time converted from seconds since 1958-01-01 to seconds since 2000-01-01T00:00:00";
    path = "/All_Data/VIIRS-Aeros-EDR-GEO_All/StartTime, /All_Data/VIIRS-Aeros-EDR-GEO_All/MidTime";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* latitude */
    description = "tangent latitude";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_double, 1, dimension_type,
                                                   NULL, description, "degree_north", NULL, read_latitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    path = "/All_Data/VIIRS-Aeros-EDR-GEO_All/Latitude";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* longitude */
    description = "tangent longitude";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_double, 1, dimension_type,
                                                   NULL, description, "degree_east", NULL, read_longitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0, 180.0);
    path = "/All_Data/VIIRS-Aeros-EDR-GEO_All/Longitude";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* The Height-field contains the difference between the geoid (avery sea level of the globe) and the ellipsoid */
    /* (against which GPS coordinates are specified). This is not the altitude-field we use in HARP so we will not */
    /* ingest the Height-field.                                                                                    */

    /* sensor_azimuth_angle */
    description = "azimuth angle (measured clockwise positive from North) to Satellite at each retrieval position";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "sensor_azimuth_angle",
                                                   harp_type_double, 1, dimension_type, NULL, description, "degree",
                                                   NULL, read_sensor_azimuth_angle);
    harp_variable_definition_set_valid_range_double(variable_definition, 0.0, 180.0);
    path = "/All_Data/VIIRS-Aeros-EDR-GEO_All/SatelliteAzimuthAngle";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* sensor_zenith_angle */
    description = "zenith angle to Satellite at each retrieval position";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "sensor_zenith_angle",
                                                   harp_type_double, 1, dimension_type, NULL, description, "degree",
                                                   NULL, read_sensor_zenith_angle);
    harp_variable_definition_set_valid_range_double(variable_definition, 0.0, 180.0);
    path = "/All_Data/VIIRS-Aeros-EDR-GEO_All/SatelliteZenithAngle";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* solar_azimuth_angle */
    description = "azimuth angle of sun (measured clockwise positive from North) at each retrieval position";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "solar_azimuth_angle",
                                                   harp_type_double, 1, dimension_type, NULL, description, "degree",
                                                   NULL, read_solar_azimuth_angle);
    harp_variable_definition_set_valid_range_double(variable_definition, 0.0, 180.0);
    path = "/All_Data/VIIRS-Aeros-EDR-GEO_All/SatelliteAzimuthAngle";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* solar_zenith_angle */
    description = "zenith angle of sun at each retrieval position";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "solar_zenith_angle",
                                                   harp_type_double, 1, dimension_type, NULL, description, "degree",
                                                   NULL, read_solar_zenith_angle);
    harp_variable_definition_set_valid_range_double(variable_definition, 0.0, 180.0);
    path = "/All_Data/VIIRS-Aeros-EDR-GEO_All/SatelliteZenithAngle";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* aerosol_optical_depth */
    description = "aerosol_optical_depth";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "aerosol_optical_depth",
                                                   harp_type_double, 2, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS, NULL, read_aerosol_optical_depth);
    path = "/All_Data/VIIRS-Aeros-EDR_All/AerosolOpticalDepth";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* wavelength */
    description = "wavelength";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "wavelength",
                                                   harp_type_double, 1, &(dimension_type[1]), NULL, description, "nm",
                                                   NULL, read_wavelength);
    description = "fixed values";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, NULL, description);
}

static void register_cloud_product_type(const char *product_type, const char *product_name, VIIRS_product_type type_nr)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_spectral };
    const char *description;
    const char *path;

    module = harp_ingestion_register_module(product_name, "NPP", "NPP_SUOMI", product_type,
                                            "NPP Suomi VIIRS EDR", ingestion_init, ingestion_done);

    product_definition = harp_ingestion_register_product(module, product_name, NULL, read_dimensions);

    /* datetime */
    description = "time of the measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double, 1, dimension_type,
                                                   NULL, description, "seconds since 2000-01-01", NULL, read_datetime);
    description = "the time converted from seconds since 1958-01-01 to seconds since 2000-01-01T00:00:00";
    path = "/All_Data/VIIRS-CLD-AGG-GEO_All/StartTime, /All_Data/VIIRS-CLD-AGG-GEO_All/MidTime";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* latitude */
    description = "tangent latitude";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_double, 1, dimension_type,
                                                   NULL, description, "degree_north", NULL, read_latitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    path = "/All_Data/VIIRS-CLD-AGG-GEO_All/Latitude";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* longitude */
    description = "tangent longitude";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_double, 1, dimension_type,
                                                   NULL, description, "degree_east", NULL, read_longitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0, 180.0);
    path = "/All_Data/VIIRS-CLD-AGG-GEO_All/Longitude";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* The Height-field contains the difference between the geoid (avery sea level of the globe) and the ellipsoid */
    /* (against which GPS coordinates are specified). This is not the altitude-field we use in HARP so we will not */
    /* ingest the Height-field.                                                                                    */

    /* sensor_azimuth_angle */
    description = "azimuth angle (measured clockwise positive from North) to Satellite at each retrieval position";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "sensor_azimuth_angle",
                                                   harp_type_double, 1, dimension_type, NULL, description, "degree",
                                                   NULL, read_sensor_azimuth_angle);
    harp_variable_definition_set_valid_range_double(variable_definition, 0.0, 180.0);
    path = "/All_Data/VIIRS-CLD-AGG-GEO_All/SatelliteAzimuthAngle";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* sensor_zenith_angle */
    description = "zenith angle to Satellite at each retrieval position";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "sensor_zenith_angle",
                                                   harp_type_double, 1, dimension_type, NULL, description, "degree",
                                                   NULL, read_sensor_zenith_angle);
    harp_variable_definition_set_valid_range_double(variable_definition, 0.0, 180.0);
    path = "/All_Data/VIIRS-CLD-AGG-GEO_All/SatelliteZenithAngle";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* solar_azimuth_angle */
    description = "azimuth angle of sun (measured clockwise positive from North) at each retrieval position";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "solar_azimuth_angle",
                                                   harp_type_double, 1, dimension_type, NULL, description, "degree",
                                                   NULL, read_solar_azimuth_angle);
    harp_variable_definition_set_valid_range_double(variable_definition, 0.0, 180.0);
    path = "/All_Data/VIIRS-CLD-AGG-GEO_All/SatelliteAzimuthAngle";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* solar_zenith_angle */
    description = "zenith angle of sun at each retrieval position";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "solar_zenith_angle",
                                                   harp_type_double, 1, dimension_type, NULL, description, "degree",
                                                   NULL, read_solar_zenith_angle);
    harp_variable_definition_set_valid_range_double(variable_definition, 0.0, 180.0);
    path = "/All_Data/VIIRS-CLD-AGG-GEO_All/SatelliteZenithAngle";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_base_height */
    description = "cloud_base_height";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_base_height",
                                                   harp_type_double, 1, dimension_type, NULL, description, "km",
                                                   (type_nr ==
                                                    CLOUD_BASE_HEIGHT) ? NULL : include_non_cloud_base_height,
                                                   read_cloud_base_height);
    path = "/All_Data/VIIRS-CBH-EDR_All/AverageCloudBaseHeight";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_top_height */
    description = "cloud_top_height";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_top_height",
                                                   harp_type_double, 1, dimension_type, NULL, description, "km",
                                                   (type_nr == CLOUD_TOP_HEIGHT) ? NULL : include_non_cloud_top_height,
                                                   read_cloud_top_height);
    path = "/All_Data/VIIRS-CTH-EDR_All/AverageCloudTopHeight";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_top_pressure */
    description = "cloud_top_pressure";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_top_pressure",
                                                   harp_type_double, 1, dimension_type, NULL, description, "hPa",
                                                   (type_nr ==
                                                    CLOUD_TOP_PRESSURE) ? NULL : include_non_cloud_top_pressure,
                                                   read_cloud_top_pressure);
    path = "/All_Data/VIIRS-CTP-EDR_All/AverageCloudTopPressure";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_top_temperature */
    description = "cloud_top_temperature";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_top_temperature",
                                                   harp_type_double, 1, dimension_type, NULL, description, "K",
                                                   (type_nr ==
                                                    CLOUD_TOP_TEMPERATURE) ? NULL : include_non_cloud_top_temperature,
                                                   read_cloud_top_temperature);
    path = "/All_Data/VIIRS-CTT-EDR_All/AverageCloudTopTemperature";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_fraction */
    description = "cloud_fraction";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_fraction",
                                                   harp_type_double, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS,
                                                   (type_nr == CLOUD_FRACTION) ? NULL : include_non_cloud_fraction,
                                                   read_cloud_fraction);
    path = "/All_Data/VIIRS-CCL-EDR_All/SummedCloudCover";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_effective_particle_size */
    description = "cloud_effective_particle_size";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_effective_particle_size",
                                                   harp_type_double, 1, dimension_type, NULL, description, "um",
                                                   (type_nr ==
                                                    CLOUD_EFFECTIVE_PARTICLE_SIZE) ? NULL :
                                                   include_non_cloud_effective_particle_size,
                                                   read_cloud_effective_particle_size);
    path = "/All_Data/VIIRS-CEPS-EDR_All/AverageCloudEffectiveParticleSize";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_optical_depth */
    description = "cloud_optical_depth";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "cloud_optical_depth",
                                                   harp_type_double, 1, dimension_type, NULL, description,
                                                   HARP_UNIT_DIMENSIONLESS,
                                                   (type_nr ==
                                                    CLOUD_OPTICAL_DEPTH) ? NULL : include_non_cloud_optical_depth,
                                                   read_cloud_optical_depth);
    path = "/All_Data/VIIRS-COT-EDR_All/AverageCloudOpticalThickness";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

int harp_ingestion_module_npp_suomi_viirs_l2_init(void)
{
    register_aeros_product_type("VIIRS_EDR_VAOO_L2");
    register_cloud_product_type("VIIRS_EDR_VCBH_L2", "NPP_SUOMI_L2_VIIRS_EDR_VCBH", CLOUD_BASE_HEIGHT);
    register_cloud_product_type("VIIRS_EDR_VCCL_L2", "NPP_SUOMI_L2_VIIRS_EDR_VCCL", CLOUD_FRACTION);
    register_cloud_product_type("VIIRS_EDR_VCEP_L2", "NPP_SUOMI_L2_VIIRS_EDR_VCEP", CLOUD_EFFECTIVE_PARTICLE_SIZE);
    register_cloud_product_type("VIIRS_EDR_VCDT_L2", "NPP_SUOMI_L2_VIIRS_EDR_VCDT", CLOUD_OPTICAL_DEPTH);
    register_cloud_product_type("VIIRS_EDR_VCTH_L2", "NPP_SUOMI_L2_VIIRS_EDR_VCTH", CLOUD_TOP_HEIGHT);
    register_cloud_product_type("VIIRS_EDR_VCTP_L2", "NPP_SUOMI_L2_VIIRS_EDR_VCTP", CLOUD_TOP_PRESSURE);
    register_cloud_product_type("VIIRS_EDR_VCTT_L2", "NPP_SUOMI_L2_VIIRS_EDR_VCTT", CLOUD_TOP_TEMPERATURE);

    /* Note: The VICM (Cloud Mask) en VSUM (suspended matter) types are not ingested. */
    /* They do not contain data that is valid for HARP.                               */

    return 0;
}
