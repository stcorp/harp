/*
 * Copyright (C) 2015-2021 S[&]T, The Netherlands.
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

typedef enum variable_type_enum
{
    IS_NO_ARRAY,
    USE_ARRAY_INDEX_0,
    USE_ARRAY_INDEX_1,
    USE_UPPER_FLAG_AS_INDEX
} variable_type;

#define CHECKED_MALLOC(v, s) v = malloc(s); if (v == NULL) { harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)", s, __FILE__, __LINE__); return -1; }

typedef struct ingest_info_struct
{
    coda_product *product;
    int format_version;
    long elements_per_profile;  /* Each profile is a series of elements, each element is a series of measurements for different wavelengths */
    long num_wavelengths;       /* The number of different spectra in 1 element */
    double *wavelengths;
    uint8_t sensitivity_curve_size;
    double *sensitivity_curve;
    double *sensitivity_curve_wavelengths;
    int upper;
    int corrected;
} ingest_info;

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

static int get_scalar_value(ingest_info *info, const char *datasetname, const char *fieldname,
                            variable_type dataset_type, double *double_data)
{
    coda_cursor cursor;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&cursor, datasetname) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (dataset_type != IS_NO_ARRAY)
    {
        if (coda_cursor_goto_first_array_element(&cursor) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
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
    if (coda_cursor_goto_root(&cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    return 0;
}

static int get_main_data(ingest_info *info, const char *datasetname, const char *fieldname, variable_type var_type,
                         double *double_data_array)
{
    coda_cursor cursor;
    double *double_data;
    long l;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&cursor, datasetname) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_first_array_element(&cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    double_data = double_data_array;
    for (l = 0; l < info->elements_per_profile; l++)
    {
        if (coda_cursor_goto_record_field_by_name(&cursor, fieldname) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        switch (var_type)
        {
            case IS_NO_ARRAY:
                break;
            case USE_ARRAY_INDEX_0:
                if (coda_cursor_goto_array_element_by_index(&cursor, 0) != 0)
                {
                    harp_set_error(HARP_ERROR_CODA, NULL);
                    return -1;
                }
                break;
            case USE_ARRAY_INDEX_1:
                if (coda_cursor_goto_array_element_by_index(&cursor, 1) != 0)
                {
                    harp_set_error(HARP_ERROR_CODA, NULL);
                    return -1;
                }
                break;
            case USE_UPPER_FLAG_AS_INDEX:
                if (coda_cursor_goto_array_element_by_index(&cursor, info->upper ? 1 : 0) != 0)
                {
                    harp_set_error(HARP_ERROR_CODA, NULL);
                    return -1;
                }
                break;
        }
        if (coda_cursor_read_double(&cursor, double_data) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (var_type != IS_NO_ARRAY)
        {
            if (coda_cursor_goto_parent(&cursor) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
        }
        if (coda_cursor_goto_parent(&cursor) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (l < (info->elements_per_profile - 1))
        {
            if (coda_cursor_goto_next_array_element(&cursor) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
        }
        double_data++;
    }

    return 0;
}

static int get_wavelength_data(ingest_info *info, const char *datasetname, const char *fieldname,
                               double *double_data_array)
{
    coda_cursor cursor;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&cursor, datasetname) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_first_array_element(&cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&cursor, fieldname) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_double_array(&cursor, double_data_array, coda_array_ordering_c) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    return 0;
}

static int get_spectral_data(ingest_info *info, const char *datasetname, const char *fieldname, int upper_lower_index,
                             double *double_data_array)
{
    coda_cursor cursor;
    long element_nr, wavelength_nr;
    double *double_data;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&cursor, datasetname) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_first_array_element(&cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    double_data = double_data_array;
    for (element_nr = 0; element_nr < info->elements_per_profile; element_nr++)
    {
        if (coda_cursor_goto_record_field_by_name(&cursor, fieldname) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_goto_array_element_by_index(&cursor, upper_lower_index) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        for (wavelength_nr = 0; wavelength_nr < info->num_wavelengths; wavelength_nr++)
        {
            if (coda_cursor_read_double(&cursor, double_data) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            double_data++;
            if (coda_cursor_goto_next_array_element(&cursor) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
        }
        if (coda_cursor_goto_parent(&cursor) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_goto_parent(&cursor) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_goto_next_array_element(&cursor) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
    }

    return 0;
}

static int get_illumination_condition(ingest_info *info, const char *datasetname, harp_array data)
{
    coda_cursor cursor;
    int32_t condition;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&cursor, datasetname) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_first_array_element(&cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (info->format_version == 0)
    {
        if (coda_cursor_goto_record_field_by_name(&cursor, "limb_flag") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
    }
    else
    {
        if (coda_cursor_goto_record_field_by_name(&cursor, "obs_illum_cond") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
    }
    if (coda_cursor_read_int32(&cursor, &condition) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    *data.int8_data = (int8_t)condition;

    return 0;
}


static int read_sensitivity_curve(ingest_info *info)
{
    coda_cursor cursor;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&cursor, "lim_occultation_data") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_first_array_element(&cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    /* Read number of wavelengths in the lookup table */
    if (coda_cursor_goto_record_field_by_name(&cursor, "size_rad_sens_curve_limb") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_uint8(&cursor, &info->sensitivity_curve_size) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    coda_cursor_goto_parent(&cursor);

    /* Read wavelengths in the lookup table */
    CHECKED_MALLOC(info->sensitivity_curve_wavelengths, sizeof(double) * info->sensitivity_curve_size);
    if (coda_cursor_goto_record_field_by_name(&cursor, "abs_rad_sens_curve_limb") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_double_partial_array(&cursor, 0L, (long)(info->sensitivity_curve_size),
                                              info->sensitivity_curve_wavelengths) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    coda_cursor_goto_parent(&cursor);

    /* Read conversion factors in the lookup table */
    CHECKED_MALLOC(info->sensitivity_curve, sizeof(double) * info->sensitivity_curve_size);
    if (coda_cursor_goto_record_field_by_name(&cursor, "rad_sens_curve_limb") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_double_partial_array(&cursor, 0L, (long)(info->sensitivity_curve_size),
                                              info->sensitivity_curve) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    return 0;
}

static double spectral_conversion_factor(ingest_info *info, double wavelength)
{
    uint8_t i;

    if (wavelength < info->sensitivity_curve_wavelengths[0])
    {
        return info->sensitivity_curve[0];
    }
    else if (wavelength > info->sensitivity_curve_wavelengths[info->sensitivity_curve_size - 1])
    {
        return info->sensitivity_curve[info->sensitivity_curve_size - 1];
    }
    else
    {
        for (i = 1; i < info->sensitivity_curve_size; i++)
        {
            if ((wavelength > info->sensitivity_curve_wavelengths[i - 1]) &&
                (wavelength < info->sensitivity_curve_wavelengths[i]))
            {
                return info->sensitivity_curve[i - 1] +
                    ((info->sensitivity_curve[i] - info->sensitivity_curve[i - 1]) *
                     ((wavelength - info->sensitivity_curve_wavelengths[i - 1]) /
                      (info->sensitivity_curve_wavelengths[i] - info->sensitivity_curve_wavelengths[i - 1])));
            }
        }
    }

    return harp_nan();
}

static void ingestion_done(void *user_data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->wavelengths != NULL)
    {
        free(info->wavelengths);
    }
    if (info->sensitivity_curve_wavelengths != NULL)
    {
        free(info->sensitivity_curve_wavelengths);
    }
    if (info->sensitivity_curve != NULL)
    {
        free(info->sensitivity_curve);
    }
    free(info);
}

/* Start of the GOMOS_LIM specific code */

static int read_lim_dimensions(void *user_data, long dimension[HARP_NUM_DIM_TYPES])
{
    dimension[harp_dimension_time] = ((ingest_info *)user_data)->elements_per_profile;
    dimension[harp_dimension_spectral] = ((ingest_info *)user_data)->num_wavelengths;
    return 0;
}

static int read_lim_datetime(void *user_data, harp_array data)
{
    return get_main_data((ingest_info *)user_data, "lim_mds", "dsr_time", IS_NO_ARRAY, data.double_data);
}

static int read_lim_latitude(void *user_data, harp_array data)
{
    return get_main_data((ingest_info *)user_data, "lim_ads", "tangent_lat", USE_UPPER_FLAG_AS_INDEX, data.double_data);
}

static int read_lim_longitude(void *user_data, harp_array data)
{
    return get_main_data((ingest_info *)user_data, "lim_ads", "tangent_long", USE_UPPER_FLAG_AS_INDEX,
                         data.double_data);
}

static int read_lim_altitude(void *user_data, harp_array data)
{
    return get_main_data((ingest_info *)user_data, "lim_ads", "tangent_alt", USE_UPPER_FLAG_AS_INDEX, data.double_data);
}

static int read_lim_spectral_photon_radiance(void *user_data, harp_array data)
{
    double background_in_electrons;
    double *background_code_values, *background_offsets, *background_gains;
    double *double_data;
    ingest_info *info;
    long profile_nr, wavelength_nr;

    info = (ingest_info *)user_data;

    CHECKED_MALLOC(background_code_values, sizeof(double) * info->elements_per_profile * info->num_wavelengths);
    if (get_spectral_data(info, "lim_mds", info->corrected ? "up_low_back_corr" : "up_low_back_no_corr",
                          info->upper ? 0 : info->num_wavelengths, background_code_values) != 0)
    {
        free(background_code_values);
        return -1;
    }

    CHECKED_MALLOC(background_offsets, sizeof(double) * info->elements_per_profile);
    if (get_main_data((ingest_info *)user_data, "lim_ads", "off_back", IS_NO_ARRAY, background_offsets) != 0)
    {
        free(background_offsets);
        free(background_code_values);
        return -1;
    }
    CHECKED_MALLOC(background_gains, sizeof(double) * info->elements_per_profile);
    if (get_main_data((ingest_info *)user_data, "lim_ads", "gain_back", IS_NO_ARRAY, background_gains) != 0)
    {
        free(background_gains);
        free(background_offsets);
        free(background_code_values);
        return -1;
    }

    double_data = data.double_data;
    for (profile_nr = 0; profile_nr < info->elements_per_profile; profile_nr++)
    {
        for (wavelength_nr = 0; wavelength_nr < info->num_wavelengths; wavelength_nr++)
        {
            /* Convert the value to electrons by using the offsets and gains */
            /* from the lim_ads according to section 10.4.2.7.4 in the       */
            /* ENVISAT-GOMOS product specifications (PO-RS-MDA-GS-2009).     */
            background_in_electrons = background_offsets[profile_nr] +
                background_code_values[profile_nr * info->num_wavelengths + wavelength_nr] /
                background_gains[profile_nr];
            /* Convert the value in electrons to a physical unit according */
            /* to section 10.4.2.7.2 in the ENVISAT-GOMOS product          */
            /* specifications (ESA Doc Ref: PO-RS-MDA-GS-2009).            */
            *double_data = background_in_electrons * spectral_conversion_factor(info, info->wavelengths[wavelength_nr]);
            double_data++;
        }
    }

    free(background_gains);
    free(background_offsets);
    free(background_code_values);

    return 0;
}

static int read_lim_spectral_photon_radiance_error(void *user_data, harp_array data)
{
    ingest_info *info;
    harp_array measured_data;
    long i;

    info = (ingest_info *)user_data;

    if (get_spectral_data(info, "lim_mds", "err_up_low_back_corr", info->upper ? 0 : info->num_wavelengths,
                          data.double_data) != 0)
    {
        return -1;
    }

    CHECKED_MALLOC(measured_data.double_data, sizeof(double) * info->elements_per_profile * info->num_wavelengths);
    if (read_lim_spectral_photon_radiance(user_data, measured_data) != 0)
    {
        free(measured_data.double_data);
        return -1;
    }

    for (i = 0; i < info->elements_per_profile * info->num_wavelengths; i++)
    {
        /* From percentage and measured datavalue, calculate uncertainty */
        data.double_data[i] = fabs(0.01 * data.double_data[i] * measured_data.double_data[i]);
    }

    free(measured_data.double_data);

    return 0;
}

static int read_lim_wavelength(void *user_data, harp_array data)
{
    return get_wavelength_data((ingest_info *)user_data, "lim_nom_wav_assignment", "nom_wl", data.double_data);
}

static int read_lim_integration_time(void *user_data, harp_array data)
{
    return get_scalar_value((ingest_info *)user_data, "sph", "samp_duration", IS_NO_ARRAY, data.double_data);
}

static int read_lim_sensor_latitude(void *user_data, harp_array data)
{
    return get_main_data((ingest_info *)user_data, "lim_ads", "lat", IS_NO_ARRAY, data.double_data);
}

static int read_lim_sensor_longitude(void *user_data, harp_array data)
{
    return get_main_data((ingest_info *)user_data, "lim_ads", "longit", IS_NO_ARRAY, data.double_data);
}

static int read_lim_sensor_altitude(void *user_data, harp_array data)
{
    return get_main_data((ingest_info *)user_data, "lim_ads", "alt", IS_NO_ARRAY, data.double_data);
}

static int read_lim_illumination_condition(void *user_data, harp_array data)
{
    return get_illumination_condition((ingest_info *)user_data, "lim_summary_quality", data);
}

static int lim_init_dimensions(ingest_info *info)
{
    coda_cursor cursor;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    /* Count the number of elements per profile */
    if (coda_cursor_goto_record_field_by_name(&cursor, "lim_mds") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &info->elements_per_profile) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    coda_cursor_goto_parent(&cursor);

    /* Count the number of spectra per element */
    if (coda_cursor_goto_record_field_by_name(&cursor, "lim_nom_wav_assignment") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    /* lim_nom_wav_assignment is an array of 1 element */
    if (coda_cursor_goto_first_array_element(&cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&cursor, "nom_wl") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &info->num_wavelengths) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    coda_cursor_goto_root(&cursor);
    return 0;
}

static int lim_ingestion_init(const harp_ingestion_module *module, coda_product *product,
                              const harp_ingestion_options *options, harp_product_definition **definition,
                              void **user_data)
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
    info->product = product;
    info->format_version = format_version;
    info->elements_per_profile = 0;
    info->num_wavelengths = 0;
    info->wavelengths = NULL;
    info->sensitivity_curve_size = 0;
    info->sensitivity_curve = NULL;
    info->sensitivity_curve_wavelengths = NULL;

    info->upper = 1;
    if (harp_ingestion_options_has_option(options, "spectra"))
    {
        if (harp_ingestion_options_get_option(options, "spectra", &cp) == 0)
        {
            if (strcmp(cp, "lower") == 0)
            {
                info->upper = 0;
            }
        }
    }
    info->corrected = 1;
    if (harp_ingestion_options_has_option(options, "corrected"))
    {
        if (harp_ingestion_options_get_option(options, "corrected", &cp) == 0)
        {
            if (strcmp(cp, "false") == 0)
            {
                info->corrected = 0;
            }
        }
    }
    if (lim_init_dimensions(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }
    CHECKED_MALLOC(info->wavelengths, sizeof(double) * info->num_wavelengths);
    if (get_wavelength_data(info, "lim_nom_wav_assignment", "nom_wl", info->wavelengths) != 0)
    {
        ingestion_done(info);
        return -1;
    }
    if (read_sensitivity_curve(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }

    *definition = *module->product_definition;
    *user_data = info;

    return 0;
}

static void register_limb_product(void)
{
    const char *scene_type_values[] = { "dark", "bright", "twilight", "straylight", "twilight_straylight" };
    const char *upper_lower_options[] = { "upper", "lower" };
    const char *true_false_options[] = { "false" };
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2];
    const char *description;
    const char *path;

    description = "GOMOS Level 1 Geolocated Calibrated Background Spectra (Limb)";
    module = harp_ingestion_register_module("GOMOS_L1_LIMB", "GOMOS", "ENVISAT_GOMOS", "GOM_LIM_1P", description,
                                            lim_ingestion_init, ingestion_done);

    harp_ingestion_register_option(module, "spectra", "retrieve the upper (default, spectra=upper) or lower "
                                   "(spectra=lower) background spectra", 2, upper_lower_options);
    harp_ingestion_register_option(module, "corrected", "retrieve the corrected (default) or uncorrected "
                                   "(corrected=false) background spectra", 1, true_false_options);

    description = "limb data";
    product_definition = harp_ingestion_register_product(module, "GOMOS_L1_LIMB", description, read_lim_dimensions);
    description = "GOMOS Level 1 products only contain a single profile; all measured profile points will be provided "
        "in order from high altitude to low altitude in the profile";
    harp_product_definition_add_mapping(product_definition, description, NULL);

    dimension_type[0] = harp_dimension_time;
    dimension_type[1] = harp_dimension_spectral;

    /* datetime_start */
    description = "start time of the measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime_start", harp_type_double, 1,
                                                   dimension_type, NULL, description, "seconds since 2000-01-01", NULL,
                                                   read_lim_datetime);
    path = "/lim_mds/dsr_time";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* datetime_length */
    description = "integration time for a readout";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime_length", harp_type_double, 0,
                                                   dimension_type, NULL, description, "s", NULL,
                                                   read_lim_integration_time);
    path = "/sph/samp_duration";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* orbit_index */
    description = "absolute orbit number";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "orbit_index", harp_type_int32, 0, NULL, NULL,
                                                   description, NULL, NULL, read_orbit_index);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/mph/abs_orbit", NULL);

    /* latitude */
    description = "latitude of the apparent tangent point";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_double, 1, dimension_type,
                                                   NULL, description, "degree_north", NULL, read_lim_latitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    path = "/lim_ads/tangent_lat[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* longitude */
    description = "longitude of the apparent tangent point";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_double, 1, dimension_type,
                                                   NULL, description, "degree_east", NULL, read_lim_longitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0, 180.0);
    path = "/lim_ads/tangent_long[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* altitude */
    description = "altitude of the apparent tangent point";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "altitude", harp_type_double, 1, dimension_type,
                                                   NULL, description, "m", NULL, read_lim_altitude);
    path = "/lim_ads/tangent_alt[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* wavelength_photon_radiance */
    description = "background spectral photon radiance of each spectrum measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "wavelength_photon_radiance", harp_type_double,
                                                   2, dimension_type, NULL, description, "count/s/cm2/nm/nsr", NULL,
                                                   read_lim_spectral_photon_radiance);
    path = "/lim_mds[]/up_low_back_no_corr[0,], /lim_occultation_data[0]/abs_rad_sens_curve_limb[], "
        "/lim_occultation_data[0]/rad_sens_curve_limb[], /lim_ads[]/off_back, /lim_ads[]/gain_back";
    description = "radiance = (off_back + up_low_back_no_corr / gain_back) * "
        "interp(abs_rad_sens_curve_limb, rad_sens_curve_limb, nom_wl)";
    harp_variable_definition_add_mapping(variable_definition, NULL, "spectra=upper and corrected=false", path,
                                         description);
    path = "/lim_mds[]/up_low_back_no_corr[1,], /lim_occultation_data[0]/abs_rad_sens_curve_limb[], "
        "/lim_occultation_data[0]/rad_sens_curve_limb[], /lim_ads[]/off_back, /lim_ads[]/gain_back";
    harp_variable_definition_add_mapping(variable_definition, NULL, "spectra=lower and corrected=false", path,
                                         description);
    path = "/lim_mds[]/up_low_back_corr[0,], /lim_occultation_data[0]/abs_rad_sens_curve_limb[], "
        "/lim_occultation_data[0]/rad_sens_curve_limb[], /lim_ads[]/off_back, /lim_ads[]/gain_back";
    description = "radiance = (off_back + up_low_back_corr / gain_back) * "
        "interp(abs_rad_sens_curve_limb, rad_sens_curve_limb, nom_wl)";
    harp_variable_definition_add_mapping(variable_definition, NULL, "spectra=upper and corrected=true", path,
                                         description);
    path = "/lim_mds[]/up_low_back_corr[1,], /lim_occultation_data[0]/abs_rad_sens_curve_limb[], "
        "/lim_occultation_data[0]/rad_sens_curve_limb[], /lim_ads[]/off_back, /lim_ads[]/gain_back";
    harp_variable_definition_add_mapping(variable_definition, NULL, "spectra=lower and corrected=true", path,
                                         description);

    /* wavelength_photon_radiance_uncertainty */
    description = "error in the background spectral photon radiance of each spectrum measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "wavelength_photon_radiance_uncertainty",
                                                   harp_type_double, 2, dimension_type, NULL, description,
                                                   "count/s/cm2/nm/nsr", NULL, read_lim_spectral_photon_radiance_error);
    path = "/lim_mds[]/err_up_low_back_corr[0,]";
    description = "uncertainty = (err_up_low_back_corr / 100) * wavelength_photon_radiance";
    harp_variable_definition_add_mapping(variable_definition, NULL, "spectra=upper", path, description);
    path = "/lim_mds[]/err_up_low_back_corr[1,]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "spectra=lower", path, description);

    /* wavelength */
    description = "nominal wavelength assignment for each of the detector pixels";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "wavelength", harp_type_double, 1,
                                                   &(dimension_type[1]), NULL, description, "nm", NULL,
                                                   read_lim_wavelength);
    path = "/lim_nom_wav_assignment[]/nom_wl[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, "Will be set to nm");

    /* sensor_latitude */
    description = "latitude of the satellite";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "sensor_latitude", harp_type_double, 1,
                                                   dimension_type, NULL, description, "degree_north", NULL,
                                                   read_lim_sensor_latitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    path = "/lim_ads/lat[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* sensor_longitude */
    description = "longitude of the satellite";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "sensor_longitude", harp_type_double, 1,
                                                   dimension_type, NULL, description, "degree_east", NULL,
                                                   read_lim_sensor_longitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0, 180.0);
    path = "/lim_ads/longit[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* sensor_altitude */
    description = "altitude of satellite";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "sensor_altitude", harp_type_double, 1,
                                                   dimension_type, NULL, description, "m", NULL,
                                                   read_lim_sensor_altitude);
    path = "/lim_ads/alt[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* scene_type */
    description = "illumination condition for each profile";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "scene_type", harp_type_int8, 0,
                                                   dimension_type, NULL, description, NULL, NULL,
                                                   read_lim_illumination_condition);
    harp_variable_definition_set_enumeration_values(variable_definition, 5, scene_type_values);
    path = "/lim_summary_quality[0]/limb_flag";
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA product version 0", path, NULL);
    path = "/lim_summary_quality[0]/obs_illum_cond";
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA product version 1 and higher", path, NULL);
}

/* Start of the GOMOS_TRA specific code */

static int read_tra_dimensions(void *user_data, long dimension[HARP_NUM_DIM_TYPES])
{
    dimension[harp_dimension_time] = ((ingest_info *)user_data)->elements_per_profile;
    dimension[harp_dimension_spectral] = ((ingest_info *)user_data)->num_wavelengths;
    return 0;
}

static int read_tra_datetime(void *user_data, harp_array data)
{
    return get_main_data((ingest_info *)user_data, "tra_transmission", "dsr_time", IS_NO_ARRAY, data.double_data);
}

static int read_tra_latitude(void *user_data, harp_array data)
{
    return get_main_data((ingest_info *)user_data, "tra_geolocation", "tangent_lat", USE_ARRAY_INDEX_1,
                         data.double_data);
}

static int read_tra_longitude(void *user_data, harp_array data)
{
    return get_main_data((ingest_info *)user_data, "tra_geolocation", "tangent_long", USE_ARRAY_INDEX_1,
                         data.double_data);
}

static int read_tra_altitude(void *user_data, harp_array data)
{
    return get_main_data((ingest_info *)user_data, "tra_geolocation", "tangent_alt", USE_ARRAY_INDEX_1,
                         data.double_data);
}

static int read_tra_wavelength_photon_transmittance(void *user_data, harp_array data)
{
    ingest_info *info;

    info = (ingest_info *)user_data;
    return get_spectral_data(info, "tra_transmission", "trans_spectra", 0, data.double_data);
}

static int read_tra_wavelength_photon_transmittance_error(void *user_data, harp_array data)
{
    ingest_info *info;
    long i;

    info = (ingest_info *)user_data;
    if (get_spectral_data(info, "tra_transmission", "cov", 0, data.double_data) != 0)
    {
        return -1;
    }
    for (i = 0; i < info->elements_per_profile * info->num_wavelengths; i++)
    {
        data.double_data[i] = sqrt(data.double_data[i]);
    }
    return 0;
}

static int read_tra_wavelength(void *user_data, harp_array data)
{
    return get_wavelength_data((ingest_info *)user_data, "tra_nom_wav_assignment", "nom_wl", data.double_data);
}

static int read_tra_integration_time(void *user_data, harp_array data)
{
    return get_scalar_value((ingest_info *)user_data, "sph", "samp_duration", IS_NO_ARRAY, data.double_data);
}

static int read_tra_sensor_latitude(void *user_data, harp_array data)
{
    return get_main_data((ingest_info *)user_data, "tra_geolocation", "lat", USE_ARRAY_INDEX_1, data.double_data);
}

static int read_tra_sensor_longitude(void *user_data, harp_array data)
{
    return get_main_data((ingest_info *)user_data, "tra_geolocation", "longit", USE_ARRAY_INDEX_1, data.double_data);
}

static int read_tra_sensor_altitude(void *user_data, harp_array data)
{
    return get_main_data((ingest_info *)user_data, "tra_geolocation", "alt", USE_ARRAY_INDEX_1, data.double_data);
}

static int read_tra_illumination_condition(void *user_data, harp_array data)
{
    return get_illumination_condition((ingest_info *)user_data, "tra_summary_quality", data);
}

static int tra_init_dimensions(ingest_info *info)
{
    coda_cursor cursor;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    /* Count the number of elements per profile */
    if (coda_cursor_goto_record_field_by_name(&cursor, "tra_transmission") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &info->elements_per_profile) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    coda_cursor_goto_parent(&cursor);

    /* Count the number of spectra per element */
    if (coda_cursor_goto_record_field_by_name(&cursor, "tra_nom_wav_assignment") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    /* tra_nom_wav_assignment is an array of 1 element */
    if (coda_cursor_goto_first_array_element(&cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&cursor, "nom_wl") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &info->num_wavelengths) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    coda_cursor_goto_root(&cursor);

    return 0;
}

static int tra_ingestion_init(const harp_ingestion_module *module, coda_product *product,
                              const harp_ingestion_options *options, harp_product_definition **definition,
                              void **user_data)
{
    int format_version;
    ingest_info *info;

    (void)options;      /* ignore */

    if (coda_get_product_version(product, &format_version) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    CHECKED_MALLOC(info, sizeof(ingest_info));
    info->product = product;
    info->format_version = format_version;
    info->elements_per_profile = 0;
    info->num_wavelengths = 0;
    info->wavelengths = NULL;
    info->sensitivity_curve_size = 0;
    info->sensitivity_curve = NULL;
    info->sensitivity_curve_wavelengths = NULL;

    if (tra_init_dimensions(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }
    CHECKED_MALLOC(info->wavelengths, sizeof(double) * info->num_wavelengths);
    get_wavelength_data(info, "tra_nom_wav_assignment", "nom_wl", info->wavelengths);

    *definition = *module->product_definition;
    *user_data = info;

    return 0;
}

static void register_tra_product(void)
{
    const char *scene_type_values[] = { "dark", "bright", "twilight", "straylight", "twilight_straylight" };
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2];
    const char *description;
    const char *path;

    description = "GOMOS Level 1 Geolocated Calibrated Transmission Spectra";
    module = harp_ingestion_register_module("GOMOS_L1_TRANSMISSION", "GOMOS", "ENVISAT_GOMOS", "GOM_TRA_1P",
                                            description, tra_ingestion_init, ingestion_done);

    description = "transmission data";
    product_definition = harp_ingestion_register_product(module, "GOMOS_L1_TRANSMISSION", description,
                                                         read_tra_dimensions);
    description = "GOMOS Level 1 products only contain a single profile; all measured transmission data will be "
        "provided in order from high altitude to low altitude in the profile";
    harp_product_definition_add_mapping(product_definition, description, NULL);

    dimension_type[0] = harp_dimension_time;
    dimension_type[1] = harp_dimension_spectral;

    /* datetime_start */
    description = "start time of the measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime_start", harp_type_double, 1,
                                                   dimension_type, NULL, description, "seconds since 2000-01-01", NULL,
                                                   read_tra_datetime);
    path = "/tra_transmission/dsr_time";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* datetime_length */
    description = "integration time for a readout";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime_length", harp_type_double, 0,
                                                   dimension_type, NULL, description, "s", NULL,
                                                   read_tra_integration_time);
    path = "/sph/samp_duration";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* orbit_index */
    description = "absolute orbit number";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "orbit_index", harp_type_int32, 0, NULL, NULL,
                                                   description, NULL, NULL, read_orbit_index);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/mph/abs_orbit", NULL);

    /* latitude */
    description = "latitude of the apparent tangent point";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_double, 1, dimension_type,
                                                   NULL, description, "degree_north", NULL, read_tra_latitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    path = "/tra_geolocation/tangent_lat[1]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* longitude */
    description = "longitude of the apparent tangent point";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_double, 1, dimension_type,
                                                   NULL, description, "degree_east", NULL, read_tra_longitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0, 180.0);
    path = "/tra_geolocation/tangent_long[1]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* altitude */
    description = "altitude of the apparent tangent point";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "altitude", harp_type_double, 1, dimension_type,
                                                   NULL, description, "m", NULL, read_tra_altitude);
    path = "/tra_geolocation/tangent_alt[1]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* wavelength_photon_transmittance */
    description = "wavelength photon transmittance of each spectrum measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "wavelength_photon_transmittance",
                                                   harp_type_double, 2, dimension_type, NULL, description,
                                                   "(count/s/cm2/nm)/(count/s/cm2/nm)", NULL,
                                                   read_tra_wavelength_photon_transmittance);
    path = "/tra_transmission[]/trans_spectra[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* According to section 10.4.1.7.6 in the ENVISAT-GOMOS product      */
    /* specifications (PO-RS-MDA-GS-2009) the cov[] field contains the   */
    /* covariance function of the full transmission. For now, this is    */
    /* interpreted as a standard deviation of the transmission.          */
    description = "error in the wavelength photon transmittance of each spectrum measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "wavelength_photon_transmittance_uncertainty",
                                                   harp_type_double, 2, dimension_type, NULL, description,
                                                   "(count/s/cm2/nm)/(count/s/cm2/nm)", NULL,
                                                   read_tra_wavelength_photon_transmittance_error);
    path = "/tra_transmission[]/cov[]";
    description = "the square root of the variance values are taken to provide the standard uncertainty";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* wavelength_of_each_spectrum_measurement */
    description = "nominal wavelength assignment for each of the detector pixels";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "wavelength", harp_type_double, 1,
                                                   &(dimension_type[1]), NULL, description, "nm", NULL,
                                                   read_tra_wavelength);
    path = "/tra_nom_wav_assignment[]/nom_wl[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* sensor_latitude */
    description = "latitude of the satellite position at half-measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "sensor_latitude", harp_type_double, 1,
                                                   dimension_type, NULL, description, "degree_north", NULL,
                                                   read_tra_sensor_latitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    path = "/tra_geolocation/lat[1]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* sensor_longitude */
    description = "longitude of the satellite position at half-measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "sensor_longitude", harp_type_double, 1,
                                                   dimension_type, NULL, description, "degree_east", NULL,
                                                   read_tra_sensor_longitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0, 180.0);
    path = "/tra_geolocation/longit[1]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* sensor_altitude */
    description = "altitude of the satellite at half-measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "sensor_altitude", harp_type_double, 1,
                                                   dimension_type, NULL, description, "m", NULL,
                                                   read_tra_sensor_altitude);
    path = "/tra_geolocation/alt[1]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* scene_type */
    description = "illumination condition for each profile";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "scene_type", harp_type_int8, 0,
                                                   dimension_type, NULL, description, NULL, NULL,
                                                   read_tra_illumination_condition);
    harp_variable_definition_set_enumeration_values(variable_definition, 5, scene_type_values);
    path = "/tra_summary_quality[0]/limb_flag";
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA product version 0", path, NULL);
    path = "/tra_summary_quality[0]/obs_illum_cond";
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA product version 1 and higher", path, NULL);
}

int harp_ingestion_module_gomos_l1_init(void)
{
    register_limb_product();
    register_tra_product();

    return 0;
}
