/*
 * Copyright (C) 2015-2016 S[&]T, The Netherlands.
 *
 * This file is part of HARP.
 *
 * HARP is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * HARP is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with HARP; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "coda.h"
#include "harp-ingestion.h"
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifndef FALSE
#define FALSE    0
#define TRUE     1
#endif

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
    int upper;
    int corrected;
} ingest_info;

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
    if (coda_cursor_goto_root(&cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
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
    if (coda_cursor_goto_root(&cursor) != 0)
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
    if (coda_cursor_goto_root(&cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
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
    switch (condition)
    {
        case 0:
            data.string_data[0] = strdup("dark");
            break;
        case 1:
            data.string_data[0] = strdup("bright");
            break;
        case 2:
            data.string_data[0] = strdup("twilight");
            break;
        case 3:
            data.string_data[0] = strdup("straylight");
            break;
        case 4:
            data.string_data[0] = strdup("twilight/straylight");
            break;
        default:
            harp_set_error(HARP_ERROR_INGESTION, "invalid illumination condition value (%ld) in product",
                           (long)condition);
            return -1;
    }
    if (data.string_data[0] == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)", __FILE__,
                       __LINE__);
        return -1;
    }

    return 0;
}

static int spectral_conversion_factor(ingest_info *info, char *datasetname, char *num_fieldname,
                                      char *wavelength_fieldname, char *factor_fieldname, double wavelength,
                                      double *conversion_factor)
{
    static double *wavelengths_in_table = NULL, *conversion_factors_in_table = NULL;
    static uint8_t nr_wavelengths_in_table;
    coda_cursor cursor;
    uint8_t i;

    if (info == NULL)
    {
        /* Free the allocated memory */
        if (wavelengths_in_table != NULL)
        {
            free(wavelengths_in_table);
            wavelengths_in_table = NULL;
        }
        if (conversion_factors_in_table != NULL)
        {
            free(conversion_factors_in_table);
            conversion_factors_in_table = NULL;
        }
        return 0;
    }

    if (wavelengths_in_table == NULL)
    {
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

        /* Read number of wavelengths in the lookup table */
        if (coda_cursor_goto_record_field_by_name(&cursor, num_fieldname) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_read_uint8(&cursor, &nr_wavelengths_in_table) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        coda_cursor_goto_parent(&cursor);

        /* Read wavelengths in the lookup table */
        CHECKED_MALLOC(wavelengths_in_table, sizeof(double) * nr_wavelengths_in_table);
        if (coda_cursor_goto_record_field_by_name(&cursor, wavelength_fieldname) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_read_double_partial_array(&cursor, 0L, (long)nr_wavelengths_in_table, wavelengths_in_table) !=
            0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        coda_cursor_goto_parent(&cursor);

        /* Read conversion factors in the lookup table */
        CHECKED_MALLOC(conversion_factors_in_table, sizeof(double) * nr_wavelengths_in_table);
        if (coda_cursor_goto_record_field_by_name(&cursor, factor_fieldname) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_read_double_partial_array
            (&cursor, 0L, (long)nr_wavelengths_in_table, conversion_factors_in_table) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        coda_cursor_goto_root(&cursor);
    }

    if (wavelength < wavelengths_in_table[0])
    {
        *conversion_factor = conversion_factors_in_table[0];
    }
    else if (wavelength > wavelengths_in_table[nr_wavelengths_in_table - 1])
    {
        *conversion_factor = conversion_factors_in_table[nr_wavelengths_in_table - 1];
    }
    else
    {
        for (i = 1; i < nr_wavelengths_in_table; i++)
        {
            if ((wavelength > wavelengths_in_table[i - 1]) && (wavelength < wavelengths_in_table[i]))
            {
                *conversion_factor = conversion_factors_in_table[i - 1] +
                    ((conversion_factors_in_table[i] - conversion_factors_in_table[i - 1]) *
                     ((wavelength - wavelengths_in_table[i - 1]) /
                      (wavelengths_in_table[i] - wavelengths_in_table[i - 1])));
            }
        }
    }

    return 0;
}

static void ingestion_done(void *user_data)
{
    ingest_info *info = (ingest_info *)user_data;

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
    return get_main_data((ingest_info *)user_data, "lim_ads", "tangent_alt", USE_UPPER_FLAG_AS_INDEX,
                           data.double_data);
}

static int read_lim_spectral_photon_radiance(void *user_data, harp_array data)
{
    int retval;
    double background_in_electrons, wavelength, conversion_factor;
    double *background_code_values, *background_offsets, *background_gains;
    double *double_data;
    ingest_info *info;
    long profile_nr, wavelength_nr;

    info = (ingest_info *)user_data;

    CHECKED_MALLOC(background_code_values, sizeof(double) * info->elements_per_profile * info->num_wavelengths);
    if (info->corrected)
    {
        retval = get_spectral_data(info, "lim_mds", "up_low_back_corr", info->upper ? 0 : info->num_wavelengths,
                                   background_code_values);
    }
    else
    {
        retval = get_spectral_data(info, "lim_mds", "up_low_back_no_corr", info->upper ? 0 : info->num_wavelengths,
                                   background_code_values);
    }
    if (retval != 0)
    {
        free(background_code_values);
        return retval;
    }

    CHECKED_MALLOC(background_offsets, sizeof(double) * info->elements_per_profile);
    retval = get_main_data((ingest_info *)user_data, "lim_ads", "off_back", IS_NO_ARRAY, background_offsets);
    if (retval != 0)
    {
        free(background_offsets);
        free(background_code_values);
        return retval;
    }
    CHECKED_MALLOC(background_gains, sizeof(double) * info->elements_per_profile);
    retval = get_main_data((ingest_info *)user_data, "lim_ads", "gain_back", IS_NO_ARRAY, background_gains);
    if (retval != 0)
    {
        free(background_gains);
        free(background_offsets);
        free(background_code_values);
        return retval;
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
            wavelength = info->wavelengths[wavelength_nr];
            /* Convert the value in electrons to a physical unit according */
            /* to section 10.4.2.7.2 in the ENVISAT-GOMOS product          */
            /* specifications (ESA Doc Ref: PO-RS-MDA-GS-2009).            */
            if (spectral_conversion_factor(info, "lim_occultation_data", "size_rad_sens_curve_limb",
                                           "abs_rad_sens_curve_limb", "rad_sens_curve_limb", wavelength,
                                           &conversion_factor) == 0)
            {
                *double_data = background_in_electrons * conversion_factor;
            }
            else
            {
                *double_data = 0.0;
            }
            double_data++;
        }
    }

    spectral_conversion_factor(NULL, NULL, NULL, NULL, NULL, 0.0, NULL);
    free(background_gains);
    free(background_offsets);
    free(background_code_values);
    return 0;
}

static int read_lim_spectral_photon_radiance_error(void *user_data, harp_array data)
{
    ingest_info *info;

    info = (ingest_info *)user_data;
    return get_spectral_data(info, "lim_mds", "err_up_low_back_corr", info->upper ? 0 : info->num_wavelengths,
                             data.double_data);
}

static int read_lim_wavelength(void *user_data, harp_array data)
{
    int retval;

    retval = get_wavelength_data((ingest_info *)user_data, "lim_nom_wav_assignment", "nom_wl", data.double_data);
    return retval;
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

static int read_lim_illumination_condition(void *user_data, long index, harp_array data)
{
    (void)index;        /* prevent unused warning */
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

    info->upper = TRUE;
    if (harp_ingestion_options_has_option(options, "spectra"))
    {
        if (harp_ingestion_options_get_option(options, "spectra", &cp) == 0)
        {
            if (strcmp(cp, "lower") == 0)
            {
                info->upper = FALSE;
            }
        }
    }
    info->corrected = TRUE;
    if (harp_ingestion_options_has_option(options, "corrected"))
    {
        if (harp_ingestion_options_get_option(options, "corrected", &cp) == 0)
        {
            if (strcmp(cp, "false") == 0)
            {
                info->corrected = FALSE;
            }
        }
    }
    if (lim_init_dimensions(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }
    CHECKED_MALLOC(info->wavelengths, sizeof(double) * info->num_wavelengths);
    get_wavelength_data(info, "lim_nom_wav_assignment", "nom_wl", info->wavelengths);

    *definition = *module->product_definition;
    *user_data = info;

    return 0;
}

static void register_limb_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2];
    const char *description;
    const char *path;
    const char *upper_lower_options[] = { "upper", "lower" };
    const char *true_false_options[] = { "true", "false" };

    description = "GOMOS Level 1 Geolocated Calibrated Background Spectra (Limb)";
    module = harp_ingestion_register_module_coda("GOMOS_L1_LIMB", "GOMOS", "ENVISAT_GOMOS", "GOM_LIM_1P", description,
                                                 lim_ingestion_init, ingestion_done);

    harp_ingestion_register_option(module, "spectra", "retrieve the upper or lower background spectra; by default the "
                                   "upper spectra are retrieved", 2, upper_lower_options);
    harp_ingestion_register_option(module, "corrected", "retrieve the corrected or uncorrected background spectra; by "
                                   "default the corrected spectra are retrieved", 2, true_false_options);

    description = "limb data";
    product_definition = harp_ingestion_register_product(module, "GOMOS_L1_LIMB", description, read_lim_dimensions);
    description = "GOMOS Level 1 products only contain a single profile; all measured profile points will be provided "
        "in order from high altitude to low altitude in the profile";
    harp_product_definition_add_mapping(product_definition, description, NULL);

    dimension_type[0] = harp_dimension_time;
    dimension_type[1] = harp_dimension_spectral;

    /* datetime */
    description = "time of the measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double, 1, dimension_type,
                                                   NULL, description, "seconds since 2000-01-01", NULL,
                                                   read_lim_datetime);
    path = "/lim_mds/dsr_time";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

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
    description = "The background spectral photon radiance of each spectrum measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "wavelength_photon_radiance", harp_type_double, 2,
                                                   dimension_type, NULL, description, "count/s/cm2/nm/sr", NULL,
                                                   read_lim_spectral_photon_radiance);
    path = "/lim_mds[]/up_low_back_no_corr[0,]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "spectra=upper;corrected=false", path, NULL);
    path = "/lim_mds[]/up_low_back_no_corr[1,]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "spectra=lower;corrected=false", path, NULL);
    path = "/lim_mds[]/up_low_back_corr[0,]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "spectra=upper;corrected=true", path, NULL);
    path = "/lim_mds[]/up_low_back_corr[1,]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "spectra=lower;corrected=true", path, NULL);

    /* wavelength_photon_radiance_uncertainty */
    description = "error in the background spectral photon radiance of each spectrum measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "wavelength_photon_radiance_uncertainty",
                                                   harp_type_double, 2, dimension_type, NULL, description, "%", NULL,
                                                   read_lim_spectral_photon_radiance_error);
    path = "/lim_mds[]/err_up_low_back_corr[0,]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "spectra=upper", path, NULL);
    path = "/lim_mds[]/err_up_low_back_corr[1,]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "spectra=lower", path, NULL);

    /* wavelength */
    description = "nominal wavelength assignment for each of the detector pixels";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "wavelength", harp_type_double, 1,
                                                   &(dimension_type[1]), NULL, description, "nm", NULL,
                                                   read_lim_wavelength);
    path = "/lim_nom_wav_assignment[]/nom_wl[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, "Will be set to nm");

    /* datetime_length time */
    description = "integration time for a readout (in seconds)";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime_length", harp_type_double, 0,
                                                   dimension_type, NULL, description, "s", NULL,
                                                   read_lim_integration_time);
    path = "/sph/samp_duration";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

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

    /* illumination_condition */
    description = "The illumination condition for each profile: 'dark', 'bright', 'twilight', 'straylight' or "
        "'twilight/straylight'";
    variable_definition =
        harp_ingestion_register_variable_sample_read(product_definition, "flag_illumination_condition",
                                                     harp_type_string, 0, dimension_type, NULL, description, NULL, NULL,
                                                     read_lim_illumination_condition);
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

static int read_tra_spectral_photon_irradiance(void *user_data, harp_array data)
{
    int retval;
    double transmission_in_electrons, wavelength, conversion_factor;
    double *transmission_code_values, *transmission_offsets, *transmission_gains;
    double *double_data;
    ingest_info *info;
    long profile_nr, wavelength_nr;

    info = (ingest_info *)user_data;

    CHECKED_MALLOC(transmission_code_values, sizeof(double) * info->elements_per_profile * info->num_wavelengths);
    retval = get_spectral_data(info, "tra_transmission", "trans_spectra", 0, transmission_code_values);
    if (retval != 0)
    {
        free(transmission_code_values);
        return retval;
    }

    CHECKED_MALLOC(transmission_offsets, sizeof(double) * info->elements_per_profile);
    retval = get_main_data((ingest_info *)user_data, "tra_auxiliary_data", "off_back", IS_NO_ARRAY,
                           transmission_offsets);
    if (retval != 0)
    {
        free(transmission_offsets);
        free(transmission_code_values);
        return retval;
    }
    CHECKED_MALLOC(transmission_gains, sizeof(double) * info->elements_per_profile);
    retval = get_main_data((ingest_info *)user_data, "tra_auxiliary_data", "gain_back", IS_NO_ARRAY,
                           transmission_gains);
    if (retval != 0)
    {
        free(transmission_gains);
        free(transmission_offsets);
        free(transmission_code_values);
        return retval;
    }
    double_data = data.double_data;
    for (profile_nr = 0; profile_nr < info->elements_per_profile; profile_nr++)
    {
        for (wavelength_nr = 0; wavelength_nr < info->num_wavelengths; wavelength_nr++)
        {
            /* Convert the value to electrons by using the offsets and gains */
            /* from the auxiliary data according to section 10.4.1.7.6 in    */
            /* the ENVISAT-GOMOS product specifications (PO-RS-MDA-GS-2009). */
            transmission_in_electrons = transmission_offsets[profile_nr] +
                transmission_code_values[profile_nr * info->num_wavelengths + wavelength_nr] /
                transmission_gains[profile_nr];
            wavelength = info->wavelengths[wavelength_nr];
            /* Convert the value in electrons to a physical unit according */
            /* to section 10.4.1.7.2 in the ENVISAT-GOMOS product          */
            /* specifications (ESA Doc Ref: PO-RS-MDA-GS-2009).            */
            if (spectral_conversion_factor(info, "tra_occultation_data", "size_rad_sens_curve_star",
                                           "abs_rad_sens_curve_star", "rad_sens_curve_star", wavelength,
                                           &conversion_factor) == 0)
            {
                *double_data = transmission_in_electrons * conversion_factor;
            }
            else
            {
                *double_data = 0.0;
            }
            double_data++;
        }
    }

    spectral_conversion_factor(NULL, NULL, NULL, NULL, NULL, 0.0, NULL);
    free(transmission_gains);
    free(transmission_offsets);
    free(transmission_code_values);
    return 0;
}

static int read_tra_spectral_photon_irradiance_error(void *user_data, harp_array data)
{
    ingest_info *info;

    info = (ingest_info *)user_data;
    return get_spectral_data(info, "tra_transmission", "cov", 0, data.double_data);
}

static int read_tra_wavelength(void *user_data, harp_array data)
{
    int retval;

    retval = get_wavelength_data((ingest_info *)user_data, "tra_nom_wav_assignment", "nom_wl", data.double_data);
    return retval;
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

static int read_tra_illumination_condition(void *user_data, long index, harp_array data)
{
    (void)index;        /* prevent unused warning */
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
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2];
    const char *description;
    const char *path;

    description = "GOMOS Level 1 Geolocated Calibrated Transmission Spectra";
    module = harp_ingestion_register_module_coda("GOMOS_L1_TRANSMISSION", "GOMOS", "ENVISAT_GOMOS", "GOM_TRA_1P",
                                                 description, tra_ingestion_init, ingestion_done);

    description = "transmission data";
    product_definition = harp_ingestion_register_product(module, "GOMOS_L1_TRANSMISSION", description,
                                                         read_tra_dimensions);
    description = "GOMOS Level 1 products only contain a single profile; all measured transmission data will be "
        "provided in order from high altitude to low altitude in the profile";
    harp_product_definition_add_mapping(product_definition, description, NULL);

    dimension_type[0] = harp_dimension_time;
    dimension_type[1] = harp_dimension_spectral;

    /* datetime */
    description = "time of the measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double, 1, dimension_type,
                                                   NULL, description, "seconds since 2000-01-01", NULL,
                                                   read_tra_datetime);
    path = "/tra_transmission/dsr_time";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

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

    /* wavelength_photon_irradiance */
    description = "spectral photon irradiance of each spectrum measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "wavelength_photon_irradiance", harp_type_double,
                                                   2, dimension_type, NULL, description, "count/s/cm2/nm", NULL,
                                                   read_tra_spectral_photon_irradiance);
    path = "/tra_transmission[]/trans_spectra[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* According to section 10.4.1.7.6 in the ENVISAT-GOMOS product      */
    /* specifications (PO-RS-MDA-GS-2009) the cov[] field contains the   */
    /* covariance function of the full transmission. For now, this is    */
    /* interpreted as a standard deviation of the transmission.          */
    description = "error in the spectral photon irradiance of each spectrum measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "wavelength_photon_irradiance_uncertainty",
                                                   harp_type_double, 2, dimension_type, NULL, description,
                                                   "count/s/cm2/nm", NULL, read_tra_spectral_photon_irradiance_error);
    path = "/tra_transmission[]/cov[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* wavelength_of_each_spectrum_measurement */
    description = "nominal wavelength assignment for each of the detector pixels";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "wavelength", harp_type_double, 1,
                                                   &(dimension_type[1]), NULL, description, "nm", NULL,
                                                   read_tra_wavelength);
    path = "/tra_nom_wav_assignment[]/nom_wl[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* integration time */
    description = "integration time for a readout (in seconds)";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime_length", harp_type_double, 0,
                                                   dimension_type, NULL, description, "s", NULL,
                                                   read_tra_integration_time);
    path = "/sph/samp_duration";
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

    /* illumination_condition */
    description = "The illumination condition for each profile: 'dark', 'bright', 'twilight', 'straylight' or "
        "'twilight/straylight'";
    variable_definition =
        harp_ingestion_register_variable_sample_read(product_definition, "flag_illumination_condition",
                                                     harp_type_string, 0, dimension_type, NULL, description, NULL, NULL,
                                                     read_tra_illumination_condition);
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
