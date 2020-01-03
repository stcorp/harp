 /*
  * Copyright (C) 2015-2020 S[&]T, The Netherlands.
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

/* BAND_1A         0
   BAND_1B         1
   BAND_2A         2
   BAND_2B         3
   BAND_3          4
   BAND_4          5
   BLIND_1A        6
   STRAYLIGHT_1A   7
   STRAYLIGHT_1B   8
   STRAYLIGHT_2A   9 */
#define MAX_NR_BANDS                10

#define MAX_SUN_REFERENCE_CHANNELS   4

#define MAX_PIXELS                4096

#define MAX_SIZE_BANDNAME           14  /* maximum size of band name (based on longest band_name "Straylight 2a") */

#define CHECKED_MALLOC(v, s) v = malloc(s); if (v == NULL) { harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)", s, __FILE__, __LINE__); return -1;}

typedef struct ingest_info_struct
{
    coda_product *product;
    int format_version;
    int sun_reference;

    /* Measured radiance fields */
    coda_cursor *egp_cursors;
    long num_egp_records;
    long num_edr_records[MAX_NR_BANDS];
    long offset_of_band[MAX_NR_BANDS];
    long max_measurements_one_egp;
    int band_nr;        /* Which band (0-9) to ingest, -1 means all bands */

    /* Sun reference fields */
    long num_sdr_records[MAX_SUN_REFERENCE_CHANNELS];
    long offset_of_sun_reference_channel[MAX_SUN_REFERENCE_CHANNELS];
    long total_spectra_pixels;
} ingest_info;

typedef enum main_variable_type_enum
{
    IS_NO_ARRAY,
    USE_ARRAY_INDEX_4,
    USE_ARRAY_INDEX_0_3
} main_variable_type;

typedef enum spectral_variable_type_enum
{
    RADIANCE,
    WAVELENGTH,
    INTEGRATION_TIME
} spectral_variable_type;

static void ingestion_done(void *user_data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->egp_cursors != NULL)
    {
        free(info->egp_cursors);
    }
    free(info);
}

static int band_name_to_band_nr(const char *band_name)
{
    const char *name_in_file[] =
        { "Band 1a", "Band 1b", "Band 2a", "Band 2b", "Band 3", "Band 4", "Blind 1a", "Straylight 1a", "Straylight 1b",
        "Straylight 2a"
    };
    const char *name_as_option[] =
        { "band-1a", "band-1b", "band-2a", "band-2b", "band-3", "band-4", "blind-1a", "straylight-1a", "straylight-1b",
        "straylight-2a"
    };
    int i;

    for (i = 0; i < 10; i++)
    {
        if (strcmp(band_name, name_in_file[i]) == 0)
        {
            return i;
        }
        if (strcmp(band_name, name_as_option[i]) == 0)
        {
            return i;
        }
    }
    return -1;
}

static int get_main_data(ingest_info *info, const char *datasetname, const char *fieldname, main_variable_type var_type,
                         double *double_data_array)
{
    coda_cursor cursor;
    double *double_data, corners[4];
    long i, j;

    double_data = double_data_array;
    for (i = 0; i < info->num_egp_records; i++)
    {
        cursor = info->egp_cursors[i];
        if (datasetname != NULL)
        {
            if (coda_cursor_goto(&cursor, datasetname) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
        }
        switch (var_type)
        {
            case IS_NO_ARRAY:
                if (coda_cursor_goto_record_field_by_name(&cursor, fieldname) != 0)
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
                break;

            case USE_ARRAY_INDEX_4:
                if (coda_cursor_goto_array_element_by_index(&cursor, 4) != 0)
                {
                    harp_set_error(HARP_ERROR_CODA, NULL);
                    return -1;
                }
                if (coda_cursor_goto_record_field_by_name(&cursor, fieldname) != 0)
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
                break;

            case USE_ARRAY_INDEX_0_3:
                for (j = 0; j <= 3; j++)
                {
                    if (coda_cursor_goto_array_element_by_index(&cursor, j) != 0)
                    {
                        harp_set_error(HARP_ERROR_CODA, NULL);
                        return -1;
                    }
                    if (coda_cursor_goto_record_field_by_name(&cursor, fieldname) != 0)
                    {
                        harp_set_error(HARP_ERROR_CODA, NULL);
                        return -1;
                    }
                    if (coda_cursor_read_double(&cursor, &corners[j]) != 0)
                    {
                        harp_set_error(HARP_ERROR_CODA, NULL);
                        return -1;
                    }
                    coda_cursor_goto_parent(&cursor);
                    coda_cursor_goto_parent(&cursor);
                }
                double_data[0] = corners[1];
                double_data[1] = corners[3];
                double_data[2] = corners[2];
                double_data[3] = corners[0];
                double_data += 4;
                break;
        }
    }
    return 0;
}

static int get_spectral_data_per_band(coda_cursor cursor_start_of_band, ingest_info *info, const char *fieldname,
                                      spectral_variable_type var_type, long egp_record_nr, int band_nr,
                                      double *data_startposition)
{
    coda_cursor cursor, save_cursor_edr;
    double *double_data, integration_time;
    long k, l, num_edr, copy_previous_values;
    int32_t flag;

    cursor = cursor_start_of_band;
    if (coda_cursor_goto_record_field_by_name(&cursor, "integration_time") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_double(&cursor, &integration_time) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    /* Calculate how many previous records need to be filled with the */
    /* value from this record. Since we know the integration time is  */
    /* a multiple of 1.5 we use a margin of 0.01 to prevent rounding  */
    /* problems.                                                      */
    copy_previous_values = 0;
    if (integration_time > 1.51)
    {
        copy_previous_values = (long)((integration_time - 1.49) / 1.5);
    }

    cursor = cursor_start_of_band;
    if (coda_cursor_goto_record_field_by_name(&cursor, "edr") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &num_edr) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_first_array_element(&cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    double_data = data_startposition;
    for (k = 0; k < info->num_edr_records[band_nr]; k++)
    {
        save_cursor_edr = cursor;
        switch (var_type)
        {
            case RADIANCE:
                if (coda_cursor_goto_record_field_by_name(&cursor, "flag") != 0)
                {
                    harp_set_error(HARP_ERROR_CODA, NULL);
                    return -1;
                }
                if (coda_cursor_read_int32(&cursor, &flag) != 0)
                {
                    harp_set_error(HARP_ERROR_CODA, NULL);
                    return -1;
                }
                if (flag != 0)
                {
                    /* Skip these radiance values */
                    break;
                }
                cursor = save_cursor_edr;
                /* Fall through */
            case WAVELENGTH:
                if (coda_cursor_goto_record_field_by_name(&cursor, fieldname) != 0)
                {
                    harp_set_error(HARP_ERROR_CODA, NULL);
                    return -1;
                }
                if (coda_cursor_read_double(&cursor, double_data) != 0)
                {
                    harp_set_error(HARP_ERROR_CODA, NULL);
                    return -1;
                }
                if (copy_previous_values > 0)
                {
                    for (l = 1; (l <= copy_previous_values) && (l <= egp_record_nr); l++)
                    {
                        *(double_data - (l * info->max_measurements_one_egp)) = *double_data;
                    }
                }
                break;

            case INTEGRATION_TIME:
                for (l = 0; (l <= copy_previous_values) && (l <= egp_record_nr); l++)
                {
                    *(double_data - (l * info->max_measurements_one_egp)) = integration_time;
                }
                break;
        }
        double_data++;

        cursor = save_cursor_edr;
        if (k < (info->num_edr_records[band_nr] - 1))
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

static int get_spectral_data(ingest_info *info, const char *fieldname, spectral_variable_type var_type,
                             double *double_data_array)
{
    coda_cursor cursor;
    double *double_data, nan;
    long i, j, num_brda_elements;
    int band_nr;
    char band_name[MAX_SIZE_BANDNAME];

    /* set all values to NaN */
    nan = coda_NaN();
    double_data = double_data_array;
    for (i = 0; i < info->num_egp_records; i++)
    {
        for (j = 0; j < info->max_measurements_one_egp; j++)
        {
            *double_data = nan;
            double_data++;
        }
    }

    double_data = double_data_array;
    for (i = 0; i < info->num_egp_records; i++)
    {
        cursor = info->egp_cursors[i];

        if (coda_cursor_goto_record_field_by_name(&cursor, "brda") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_get_num_elements(&cursor, &num_brda_elements) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_goto_first_array_element(&cursor) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        for (j = 0; j < num_brda_elements; j++)
        {
            if (coda_cursor_goto_record_field_by_name(&cursor, "band_id") != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            if (coda_cursor_read_string(&cursor, band_name, MAX_SIZE_BANDNAME) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            coda_cursor_goto_parent(&cursor);
            band_nr = band_name_to_band_nr(band_name);
            /* Check if this is a valid band name */
            if (band_nr >= 0)
            {
                if (info->band_nr < 0)
                {
                    /* Ingest all bands */
                    if (get_spectral_data_per_band
                        (cursor, info, fieldname, var_type, i, band_nr,
                         double_data + info->offset_of_band[band_nr]) != 0)
                    {
                        return -1;
                    }
                }
                else if (info->band_nr == band_nr)
                {
                    /* Ingest only this band */
                    if (get_spectral_data_per_band(cursor, info, fieldname, var_type, i, band_nr, double_data) != 0)
                    {
                        return -1;
                    }
                    break;
                }
            }
            if (j < (num_brda_elements - 1))
            {
                if (coda_cursor_goto_next_array_element(&cursor) != 0)
                {
                    harp_set_error(HARP_ERROR_CODA, NULL);
                    return -1;
                }
            }
        }
        double_data += info->max_measurements_one_egp;
    }
    return 0;
}

static int get_sun_reference_spectral_data_per_channel(coda_cursor cursor_start_of_channel, ingest_info *info,
                                                       const char *fieldname, long channel_nr,
                                                       double *data_startposition)
{
    coda_cursor cursor, save_cursor_sdr;
    double *double_data;
    long sdr_nr, num_sdr;

    cursor = cursor_start_of_channel;
    if (coda_cursor_goto_record_field_by_name(&cursor, "sdr") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &num_sdr) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_first_array_element(&cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    double_data = data_startposition;
    for (sdr_nr = 0; sdr_nr < info->num_sdr_records[channel_nr]; sdr_nr++)
    {
        save_cursor_sdr = cursor;
        if (coda_cursor_goto_record_field_by_name(&cursor, fieldname) != 0)
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

        cursor = save_cursor_sdr;
        if (sdr_nr < (info->num_sdr_records[channel_nr] - 1))
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

static int get_sun_reference_spectral_data(ingest_info *info, const char *fieldname, double *double_data_array)
{
    coda_cursor cursor;
    double *double_data, nan;
    long channel_nr, num_cdr_elements, i;

    /* set all values to NaN */
    nan = coda_NaN();
    double_data = double_data_array;
    for (i = 0; i < info->total_spectra_pixels; i++)
    {
        *double_data = nan;
        double_data++;
    }

    double_data = double_data_array;
    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&cursor, "cdr") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &num_cdr_elements) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_first_array_element(&cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    for (channel_nr = 0; channel_nr < num_cdr_elements; channel_nr++)
    {
        if (get_sun_reference_spectral_data_per_channel
            (cursor, info, fieldname, channel_nr, double_data + info->offset_of_sun_reference_channel[channel_nr]) != 0)
        {
            return -1;
        }
        if (channel_nr < (num_cdr_elements - 1))
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

static int read_datetime_stop(void *user_data, harp_array data)
{
    return get_main_data((ingest_info *)user_data, "agi", "groundpixel_end", IS_NO_ARRAY, data.double_data);
}

static int read_datetime_length(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long i;

    for (i = 0; i < info->num_egp_records; i++)
    {
        data.double_data[i] = 1.5;
    }

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
    if (coda_cursor_goto(&cursor, "/pir/start_orbit") != 0)
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
    return get_main_data((ingest_info *)user_data, "agi/coords", "latitude", USE_ARRAY_INDEX_4, data.double_data);
}

static int read_longitude(void *user_data, harp_array data)
{
    return get_main_data((ingest_info *)user_data, "agi/coords", "longitude", USE_ARRAY_INDEX_4, data.double_data);
}

static int read_latitude_bounds(void *user_data, harp_array data)
{
    return get_main_data((ingest_info *)user_data, "agi/coords", "latitude", USE_ARRAY_INDEX_0_3, data.double_data);
}

static int read_longitude_bounds(void *user_data, harp_array data)
{
    return get_main_data((ingest_info *)user_data, "agi/coords", "longitude", USE_ARRAY_INDEX_0_3, data.double_data);
}

static int read_wavelength_photon_radiance(void *user_data, harp_array data)
{
    return get_spectral_data((ingest_info *)user_data, "abs_radiance", RADIANCE, data.double_data);
}

static int read_wavelength_photon_radiance_uncertainty(void *user_data, harp_array data)
{
    return get_spectral_data((ingest_info *)user_data, "abs_rad_err", RADIANCE, data.double_data);
}

static int read_wavelength(void *user_data, harp_array data)
{
    return get_spectral_data((ingest_info *)user_data, "wavelength", WAVELENGTH, data.double_data);
}

static int read_integration_time(void *user_data, harp_array data)
{
    return get_spectral_data((ingest_info *)user_data, NULL, INTEGRATION_TIME, data.double_data);
}

static int read_scan_subindex(void *user_data, harp_array data)
{
    coda_cursor cursor;
    ingest_info *info;
    int8_t *int_data;
    long i;
    int32_t sub_counter;

    info = (ingest_info *)user_data;
    int_data = data.int8_data;
    for (i = 0; i < info->num_egp_records; i++)
    {
        cursor = info->egp_cursors[i];
        if (coda_cursor_goto_record_field_by_name(&cursor, "sub_counter") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_read_int32(&cursor, &sub_counter) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        *int_data = (int8_t)sub_counter;
        int_data++;
    }
    return 0;
}

static int read_scan_direction_type(void *user_data, long index, harp_array data)
{
    coda_cursor cursor;
    ingest_info *info;
    int32_t sub_counter;

    info = (ingest_info *)user_data;
    cursor = info->egp_cursors[index];
    if (coda_cursor_goto_record_field_by_name(&cursor, "sub_counter") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_int32(&cursor, &sub_counter) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    /* Note: In previous versions of this source, we had a value 'mixed' */
    /* that was meant for a measurement that consisted of both a forward */
    /* and a backward scan. Since in HARP we always store GOME_L1 data   */
    /* with the maximum resolution (one main-record every 1.5 seconds),  */
    /* this 'mixed' value is no longer used.                             */
    if (sub_counter < 3)
    {
        *data.int8_data = 0;
    }
    else
    {
        *data.int8_data = 1;
    }
    return 0;
}

static int read_solar_zenith_angle(void *user_data, harp_array data)
{
    return get_main_data((ingest_info *)user_data, "agi/solar_angles_spacecraft", "zenith_b", IS_NO_ARRAY,
                         data.double_data);
}

static int read_solar_azimuth_angle(void *user_data, harp_array data)
{
    return get_main_data((ingest_info *)user_data, "agi/solar_angles_spacecraft", "azimuth_b", IS_NO_ARRAY,
                         data.double_data);
}

static int read_viewing_zenith_angle(void *user_data, harp_array data)
{
    return get_main_data((ingest_info *)user_data, "agi/los_spacecraft", "zenith_b", IS_NO_ARRAY, data.double_data);
}

static int read_viewing_azimuth_angle(void *user_data, harp_array data)
{
    return get_main_data((ingest_info *)user_data, "agi/los_spacecraft", "azimuth_b", IS_NO_ARRAY, data.double_data);
}

static int read_sun_reference_datetime(void *user_data, harp_array data)
{
    coda_cursor cursor;
    ingest_info *info = (ingest_info *)user_data;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, "sfs/utc_solar_spectrum") != 0)
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

static int read_sun_reference_wavelength_photon_irradiance(void *user_data, harp_array data)
{
    return get_sun_reference_spectral_data((ingest_info *)user_data, "abs_irr", data.double_data);
}

static int read_sun_reference_wavelength_photon_irradiance_uncertainty(void *user_data, harp_array data)
{
    return get_sun_reference_spectral_data((ingest_info *)user_data, "abs_irr_err", data.double_data);
}

static int read_sun_reference_wavelength(void *user_data, harp_array data)
{
    return get_sun_reference_spectral_data((ingest_info *)user_data, "wavelength", data.double_data);
}

static int read_dimensions(void *user_data, long dimension[HARP_NUM_DIM_TYPES])
{
    ingest_info *info = (ingest_info *)user_data;

    dimension[harp_dimension_time] = info->num_egp_records;
    dimension[harp_dimension_spectral] = info->max_measurements_one_egp;
    return 0;
}

static int read_sun_reference_dimensions(void *user_data, long dimension[HARP_NUM_DIM_TYPES])
{
    ingest_info *info = (ingest_info *)user_data;

    dimension[harp_dimension_time] = 1;
    dimension[harp_dimension_spectral] = info->total_spectra_pixels;
    return 0;
}

static int init_dimensions(ingest_info *info)
{
    coda_cursor cursor, save_cursor_egp, save_cursor_brda;
    long num_brda_elements, num_edr_records, i, j, offset;
    int band_nr;
    char band_name[MAX_SIZE_BANDNAME];

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    /* Count the number of EGP records */
    if (coda_cursor_goto_record_field_by_name(&cursor, "egp") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &info->num_egp_records) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    CHECKED_MALLOC(info->egp_cursors, info->num_egp_records * sizeof(coda_cursor));

    /* Count the number of spectra per band */
    if (coda_cursor_goto_first_array_element(&cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    for (i = 0; i < info->num_egp_records; i++)
    {
        save_cursor_egp = cursor;
        info->egp_cursors[i] = cursor;
        if (coda_cursor_goto_record_field_by_name(&cursor, "brda") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_get_num_elements(&cursor, &num_brda_elements) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_goto_first_array_element(&cursor) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }

        for (j = 0; j < num_brda_elements; j++)
        {
            save_cursor_brda = cursor;

            if (coda_cursor_goto_record_field_by_name(&cursor, "band_id") != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            if (coda_cursor_read_string(&cursor, band_name, MAX_SIZE_BANDNAME) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            coda_cursor_goto_parent(&cursor);
            band_nr = band_name_to_band_nr(band_name);
            if (band_nr < 0)
            {
                cursor = save_cursor_brda;
                if (j < (num_brda_elements - 1))
                {
                    if (coda_cursor_goto_next_array_element(&cursor) != 0)
                    {
                        harp_set_error(HARP_ERROR_CODA, NULL);
                        return -1;
                    }
                }
                continue;
            }

            if (coda_cursor_goto_record_field_by_name(&cursor, "edr") != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            if (coda_cursor_get_num_elements(&cursor, &num_edr_records) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            if (info->num_edr_records[band_nr] == 0)
            {
                info->num_edr_records[band_nr] = num_edr_records;
            }
            else if (info->num_edr_records[band_nr] != num_edr_records)
            {
                harp_set_error(HARP_ERROR_INGESTION, "Number of EDR records for band %s is changed from %ld to %ld",
                               band_name, info->num_edr_records[band_nr], num_edr_records);
                info->num_edr_records[band_nr] = num_edr_records;
            }

            cursor = save_cursor_brda;
            if (j < (num_brda_elements - 1))
            {
                if (coda_cursor_goto_next_array_element(&cursor) != 0)
                {
                    harp_set_error(HARP_ERROR_CODA, NULL);
                    return -1;
                }
            }
        }

        cursor = save_cursor_egp;
        if (i < (info->num_egp_records - 1))
        {
            if (coda_cursor_goto_next_array_element(&cursor) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
        }
    }

    coda_cursor_goto_root(&cursor);

    offset = 0L;
    for (i = 0; i < MAX_NR_BANDS; i++)
    {
        info->offset_of_band[i] = offset;
        offset += info->num_edr_records[i];
    }
    if (info->band_nr >= 0)
    {
        info->max_measurements_one_egp = info->num_edr_records[info->band_nr];
    }
    else
    {
        info->max_measurements_one_egp = MAX_PIXELS;
    }
    return 0;
}

static int init_sun_reference_dimensions(ingest_info *info)
{
    coda_cursor cursor, save_cursor_cdr;
    long num_cdr_elements, num_sdr_records, i, offset;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    /* Count the number of spectra per channel */
    if (coda_cursor_goto_record_field_by_name(&cursor, "cdr") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &num_cdr_elements) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_first_array_element(&cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    for (i = 0; i < num_cdr_elements; i++)
    {
        save_cursor_cdr = cursor;
        if (coda_cursor_goto_record_field_by_name(&cursor, "sdr") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_get_num_elements(&cursor, &num_sdr_records) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (info->num_sdr_records[i] == 0)
        {
            info->num_sdr_records[i] = num_sdr_records;
        }
        cursor = save_cursor_cdr;
        if (i < (num_cdr_elements - 1))
        {
            if (coda_cursor_goto_next_array_element(&cursor) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
        }
    }

    coda_cursor_goto_root(&cursor);

    offset = 0L;
    for (i = 0; i < MAX_SUN_REFERENCE_CHANNELS; i++)
    {
        info->offset_of_sun_reference_channel[i] = offset;
        offset += info->num_sdr_records[i];
    }
    info->total_spectra_pixels = MAX_PIXELS;
    return 0;
}

static int ingestion_init(const harp_ingestion_module *module, coda_product *product,
                          const harp_ingestion_options *options, harp_product_definition **definition, void **user_data)
{
    int format_version;
    ingest_info *info;
    const char *cp;

    if (coda_get_product_version(product, &format_version) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    CHECKED_MALLOC(info, sizeof(ingest_info));
    memset(info, '\0', sizeof(ingest_info));
    info->product = product;
    info->format_version = format_version;

    info->band_nr = -1;
    if (harp_ingestion_options_has_option(options, "band"))
    {
        if (harp_ingestion_options_get_option(options, "band", &cp) == 0)
        {
            info->band_nr = band_name_to_band_nr(cp);
        }
    }
    info->sun_reference = 0;
    if (harp_ingestion_options_has_option(options, "data"))
    {
        if (harp_ingestion_options_get_option(options, "data", &cp) == 0)
        {
            info->sun_reference = (strcmp(cp, "sun_reference") == 0);
        }
    }

    if (info->sun_reference)
    {
        if (init_sun_reference_dimensions(info) != 0)
        {
            ingestion_done(info);
            return -1;
        }
        *definition = module->product_definition[1];
    }
    else
    {
        if (init_dimensions(info) != 0)
        {
            ingestion_done(info);
            return -1;
        }
        *definition = module->product_definition[0];
    }

    *user_data = info;

    return 0;
}

static void register_nominal_product(harp_ingestion_module *module)
{
    const char *scan_direction_type_values[] = { "forward", "backward" };
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2];
    harp_dimension_type bounds_dimension_type[2];
    long bounds_dimension[2] = { -1, 4 };
    const char *description;
    const char *path;

    description = "GOME Level 1 Extracted Spectra product";
    product_definition = harp_ingestion_register_product(module, "GOME_L1_EXTRACTED", description, read_dimensions);
    description = "GOME Level 1 Extracted Spectra";
    harp_product_definition_add_mapping(product_definition, description, "data unset");

    dimension_type[0] = harp_dimension_time;
    dimension_type[1] = harp_dimension_spectral;

    bounds_dimension_type[0] = harp_dimension_time;
    bounds_dimension_type[1] = harp_dimension_independent;

    /* datetime_stop */
    description = "time of the measurement at the end of the integration time";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime_stop", harp_type_double, 1,
                                                   dimension_type, NULL, description, "seconds since 2000-01-01", NULL,
                                                   read_datetime_stop);
    path = "/egp[]/agi/groundpixel_end";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* datetime_length */
    description = "length of each measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime_length", harp_type_double, 1,
                                                   dimension_type, NULL, description, "s", NULL, read_datetime_length);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, NULL, "set to fixed value of 1.5 [s]");

    /* orbit_index */
    description = "absolute orbit number";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "orbit_index", harp_type_int32, 0, NULL, NULL,
                                                   description, NULL, NULL, read_orbit_index);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/pir/start_orbit", NULL);

    /* latitude */
    description = "tangent latitude of the measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_double, 1, dimension_type,
                                                   NULL, description, "degree_north", NULL, read_latitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    path = "/egp[]/agi/coords[4]/latitude";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* longitude */
    description = "tangent longitude of the measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_double, 1, dimension_type,
                                                   NULL, description, "degree_east", NULL, read_longitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0, 180.0);
    path = "/egp[]/agi/coords[4]/longitude";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* latitude_bounds */
    description = "corner latitudes for the ground pixel of the measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "latitude_bounds", harp_type_double, 2,
                                                   bounds_dimension_type, bounds_dimension, description, "degree_north",
                                                   NULL, read_latitude_bounds);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    path = "/egp[]/agi/coords[0:3]/latitude";
    description = "The corners are rearranged in the following way: 1,3,2,0";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* longitude_bounds */
    description = "corner longitudes for the ground pixel of the measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "longitude_bounds", harp_type_double, 2,
                                                   bounds_dimension_type, bounds_dimension, description, "degree_east",
                                                   NULL, read_longitude_bounds);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    path = "/egp[]/agi/coords[0:3]/longitude";
    description = "The corners are rearranged in the following way: 1,3,2,0";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* wavelength_photon_radiance */
    description = "measured radiances";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "wavelength_photon_radiance", harp_type_double,
                                                   2, dimension_type, NULL, description, "count/s/cm2/sr/nm",
                                                   NULL, read_wavelength_photon_radiance);
    path = "/egp[]/brda[]/edr[]/abs_radiance";
    description = "will be set to NaN is brda record is not available or if egp[]/brda[]/edr[]/flag != 0";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* wavelength_photon_radiance_uncertainty */
    description = "absolute radiance measurement error";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "wavelength_photon_radiance_uncertainty",
                                                   harp_type_double, 2, dimension_type, NULL, description,
                                                   "count/s/cm2/sr/nm", NULL,
                                                   read_wavelength_photon_radiance_uncertainty);
    path = "/egp[]/brda[]/edr[]/abs_rad_err";
    description = "will be set to NaN is brda record is not available or if egp[]/brda[]/edr[]/flag != 0";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* wavelength */
    description = "nominal wavelength assignment for each of the detector pixels";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "wavelength", harp_type_double, 2,
                                                   dimension_type, NULL, description, "nm", NULL, read_wavelength);
    path = "/egp[]/brda[]/edr[]/wavelength";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* integration_time */
    description = "integration time for each pixel";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "integration_time", harp_type_double, 2,
                                                   dimension_type, NULL, description, "s", NULL, read_integration_time);
    path = "/egp[]/brda[]/integration_time";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* scan_subindex */
    description = "relative index (0-3) of this measurement within a scan (forward+backward)";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "scan_subindex", harp_type_int8, 1,
                                                   dimension_type, NULL, description, NULL, NULL, read_scan_subindex);
    harp_variable_definition_set_valid_range_int8(variable_definition, 0, 3);
    path = "/egp[]/sub_counter";
    description = "if a measurement consisted of multiple ground pixels, the subset counter of the last pixel is taken";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* scan_direction_type */
    description = "scan direction for each measurement";
    variable_definition =
        harp_ingestion_register_variable_block_read(product_definition, "scan_direction_type", harp_type_int8, 1,
                                                    dimension_type, NULL, description, NULL, NULL,
                                                    read_scan_direction_type);
    harp_variable_definition_set_enumeration_values(variable_definition, 2, scan_direction_type_values);
    path = "/egp[]/sub_counter";
    description =
        "the scan direction is based on the subset counter of the measurement; 0-2: forward (0), 3: backward (1)";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* solar_zenith_angle */
    description = "solar zenith angle at instrument";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "solar_zenith_angle", harp_type_double, 1,
                                                   dimension_type, NULL, description, "degree", NULL,
                                                   read_solar_zenith_angle);
    path = "/egp[]/agi/solar_angles_spacecraft/zenith_b";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* solar_azimuth_angle */
    description = "solar azimuth angle at instrument";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "solar_azimuth_angle", harp_type_double, 1,
                                                   dimension_type, NULL, description, "degree", NULL,
                                                   read_solar_azimuth_angle);
    path = "/egp[]/agi/solar_angles_spacecraft/azimuth_b";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* viewing_zenith_angle */
    description = "line of sight zenith angle at instrument";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "viewing_zenith_angle", harp_type_double, 1,
                                                   dimension_type, NULL, description, "degree", NULL,
                                                   read_viewing_zenith_angle);
    path = "/egp[]/agi/los_spacecraft/zenith_b";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* viewing_azimuth_angle */
    description = "line of sight azimuth angle at instrument";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "viewing_azimuth_angle", harp_type_double, 1,
                                                   dimension_type, NULL, description, "degree", NULL,
                                                   read_viewing_azimuth_angle);
    path = "/egp[]/agi/los_spacecraft/azimuth_b";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

static void register_sun_reference_product(harp_ingestion_module *module)
{
    harp_product_definition *product_definition_sun_reference;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2];
    const char *description;
    const char *path;

    description = "GOME Level 1 Extracted Spectra Sun Reference product";
    product_definition_sun_reference =
        harp_ingestion_register_product(module, "GOME_L1_EXTRACTED_sun_reference", description,
                                        read_sun_reference_dimensions);
    description = "GOME Level 1 Extracted Spectra Sun Reference";
    harp_product_definition_add_mapping(product_definition_sun_reference, description, "data=sun_reference");

    dimension_type[0] = harp_dimension_time;
    dimension_type[1] = harp_dimension_spectral;

    /* time_of_the_measurement */
    description = "time of the sun reference measurement at the end of the integration time";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition_sun_reference, "datetime", harp_type_double, 1,
                                                   dimension_type, NULL, description, "seconds since 2000-01-01", NULL,
                                                   read_sun_reference_datetime);
    path = "/sfs/utc_solar_spectrum";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* orbit_index */
    description = "absolute orbit number";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition_sun_reference, "orbit_index", harp_type_int32, 0,
                                                   NULL, NULL, description, NULL, NULL, read_orbit_index);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/pir/start_orbit", NULL);

    /* wavelength_photon_irradiance */
    description = "sun spectrum spectral irradiance";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition_sun_reference, "wavelength_photon_irradiance",
                                                   harp_type_double, 2, dimension_type, NULL, description,
                                                   "count/s/cm2/nm", NULL,
                                                   read_sun_reference_wavelength_photon_irradiance);
    path = "/cdr[]/sdr[]/abs_irr";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* wavelength_photon_irradiance_uncertainty */
    description = "relative radiometric precision of the sun reference spectrum";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition_sun_reference,
                                                   "wavelength_photon_irradiance_uncertainty", harp_type_double, 2,
                                                   dimension_type, NULL, description, "count/s/cm2/nm", NULL,
                                                   read_sun_reference_wavelength_photon_irradiance_uncertainty);
    path = "/cdr[]/sdr[]/abs_irr_err";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* wavelength */
    description = "nominal wavelength assignment for each of the detector pixels";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition_sun_reference, "wavelength", harp_type_double, 2,
                                                   dimension_type, NULL, description, "nm", NULL,
                                                   read_sun_reference_wavelength);
    path = "/cdr[]/sdr[]/wavelength";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

int harp_ingestion_module_gome_l1_init(void)
{
    harp_ingestion_module *module;
    const char *description;

    const char *band_options[] =
        { "band-1a", "band-1b", "band-2a", "band-2b", "band-3", "band-4", "blind-1a", "straylight-1a", "straylight-1b",
        "straylight-2a"
    };
    const char *sun_reference_options[] = { "sun_reference" };

    description = "GOME Level 1 Extracted data";
    module = harp_ingestion_register_module_coda("GOME_L1_EXTRACTED", "GOME", "ERS_GOME", "GOM.LVL13_EXTRACTED",
                                                 description, ingestion_init, ingestion_done);
    harp_ingestion_register_option(module, "band", "only include data from the specified band ('band-1a', 'band-1b', "
                                   "'band-2a', 'band-2b', 'band-3', 'band-4', 'blind-1a', 'straylight-1a', "
                                   "'straylight-1b', 'straylight-2a'); by default data from all bands is retrieved", 10,
                                   band_options);
    harp_ingestion_register_option(module, "data", "retrieve the measured radiances (default) or the sun spectra "
                                   "(data=sun_reference)", 1, sun_reference_options);

    register_nominal_product(module);
    register_sun_reference_product(module);
    return 0;
}
