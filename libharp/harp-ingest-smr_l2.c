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

#define MAX_PATH_LENGTH  256

enum
{
    ClO_SPECIES,
    CO_SPECIES,
    H2O_SPECIES,
    H2O_161_SPECIES,
    H2O_162_SPECIES,
    H2O_181_SPECIES,
    HNO3_SPECIES,
    HO2_SPECIES,
    N2O_SPECIES,
    NO_SPECIES,
    O3_SPECIES,
    O3_666_SPECIES,
    O3_667_SPECIES,
    O3_668_SPECIES,
    O3_686_SPECIES,
    BrO2_SPECIES,
    TEMP_SPECIES,
    PRES_SPECIES,
    NR_POSSIBLE_SPECIES
};

#ifndef FALSE
#define FALSE    0
#define TRUE     1
#endif

typedef struct ingest_info_struct
{
    coda_product *product;
    int format_version;
    long num_profiles;  // The number of profiles (each profile is a series of measurements at a certain latitude, longitude and time)
    long max_num_altitudes;     // The maximum number of altitudes in a profile
    long num_species;
    long species_nr_in_file[NR_POSSIBLE_SPECIES];
    long current_species_nr;
    short *num_altitudes;
    short *sum_prev_altitudes;
} ingest_info;

static int read_dimensions(void *user_data, long dimension[HARP_NUM_DIM_TYPES])
{
    dimension[harp_dimension_time] = ((ingest_info *)user_data)->num_profiles;
    dimension[harp_dimension_vertical] = ((ingest_info *)user_data)->max_num_altitudes;
    return 0;
}

static int get_main_data(ingest_info *info, const char *datasetname, const char *fieldname, harp_array data)
{
    coda_cursor cursor;
    int altitude_index, profile_nr, i;
    double *current_double_data;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, datasetname) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&cursor, fieldname) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    current_double_data = data.double_data;
    for (profile_nr = 0; profile_nr < info->num_profiles; profile_nr++)
    {
        altitude_index = profile_nr * info->num_species;
        if ((info->current_species_nr > 0) && (info->species_nr_in_file[info->current_species_nr] > 0))
        {
            altitude_index += info->species_nr_in_file[info->current_species_nr] - 1;
        }
        if (coda_cursor_goto_array_element_by_index(&cursor, info->sum_prev_altitudes[altitude_index]) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        /* Read all doubles for one profile and the current species. We   */
        /* can not use read_double_partial_array because that function is */
        /* not supported in CODA for the format of the SMR data (HDF4).   */
        for (i = 0; i < info->num_altitudes[altitude_index]; i++)
        {
            if (coda_cursor_read_double(&cursor, current_double_data) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            if (coda_cursor_goto_next_array_element(&cursor) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
            current_double_data++;
        }
        if (coda_cursor_goto_parent(&cursor) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (profile_nr < (info->num_profiles - 1))
        {
            current_double_data += (info->max_num_altitudes - info->num_altitudes[altitude_index]);
        }
    }
    if (coda_cursor_goto_root(&cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    return 0;
}

static int get_profile_data(ingest_info *info, const char *datasetname, const char *fieldname, harp_array data)
{
    coda_cursor cursor;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, datasetname) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&cursor, fieldname) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_double_array(&cursor, data.double_data, coda_array_ordering_c) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_parent(&cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    return 0;
}

static int read_datetime(void *user_data, harp_array data)
{
    return get_profile_data((ingest_info *)user_data, "GHz/Data_Vgroup/Geolocation", "Time", data);
}

static int read_latitude(void *user_data, harp_array data)
{
    return get_profile_data((ingest_info *)user_data, "GHz/Data_Vgroup/Geolocation", "Latitude", data);
}

static int read_longitude(void *user_data, harp_array data)
{
    return get_profile_data((ingest_info *)user_data, "GHz/Data_Vgroup/Geolocation", "Longitude", data);
}

static int read_solar_zenith_angle(void *user_data, harp_array data)
{
    return get_profile_data((ingest_info *)user_data, "GHz/Data_Vgroup/Geolocation", "SunZD", data);
}

static int read_altitude(void *user_data, harp_array data)
{
    ((ingest_info *)user_data)->current_species_nr = -1;
    return get_main_data((ingest_info *)user_data, "GHz/Data_Vgroup/Data", "Altitudes", data);
}

static int read_profile_value(void *user_data, harp_array data)
{
    return get_main_data((ingest_info *)user_data, "GHz/Data_Vgroup/Data", "Profiles", data);
}

static int read_totalerror_value(void *user_data, harp_array data)
{
    return get_main_data((ingest_info *)user_data, "GHz/Data_Vgroup/Data", "TotalError", data);
}

static int disable_exclude_for_species_in_file(ingest_info *info)
{
    coda_cursor cursor;
    int num_dims;
    long dims[2], species_nr, i;
    char speciesname[MAX_PATH_LENGTH];

    /* Read the species names and include only the registered variables */
    /* that match those species.                                        */
    for (i = 0; i < NR_POSSIBLE_SPECIES; i++)
    {
        info->species_nr_in_file[i] = 0L;       /* By default all species are not used */
    }
    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, "GHz/Data_Vgroup/Retrieval/SpeciesNames") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    /* We can't read the speciesname with with coda_cursor_read_string  */
    /* because that yields a 1-char string and cursor_read_array fails  */
    /* because the type is text and not an array. Also, the size of the */
    /* name varies. Therefor we first get the size and read size chars. */
    if (coda_cursor_get_array_dim(&cursor, &num_dims, dims) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    if (coda_cursor_goto_first_array_element(&cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    for (species_nr = 1; species_nr <= info->num_species; species_nr++)
    {
        for (i = 0; (i < dims[1]) && (i < MAX_PATH_LENGTH); i++)
        {
            if (coda_cursor_read_char(&cursor, &speciesname[i]) != 0)
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
        if (memcmp(speciesname, "ClO_", 4) == 0)
        {
            info->species_nr_in_file[ClO_SPECIES] = species_nr;
        }
        if (memcmp(speciesname, "CO_", 3) == 0)
        {
            info->species_nr_in_file[CO_SPECIES] = species_nr;
        }
        if (memcmp(speciesname, "H2O_", 4) == 0)
        {
            info->species_nr_in_file[H2O_SPECIES] = species_nr;
        }
        if (memcmp(speciesname, "H2O-161_", 8) == 0)
        {
            info->species_nr_in_file[H2O_161_SPECIES] = species_nr;
        }
        if (memcmp(speciesname, "H2O-162_", 8) == 0)
        {
            info->species_nr_in_file[H2O_162_SPECIES] = species_nr;
        }
        if (memcmp(speciesname, "H2O-181_", 8) == 0)
        {
            info->species_nr_in_file[H2O_181_SPECIES] = species_nr;
        }
        if (memcmp(speciesname, "HNO3_", 5) == 0)
        {
            info->species_nr_in_file[HNO3_SPECIES] = species_nr;
        }
        if (memcmp(speciesname, "HO2_", 4) == 0)
        {
            info->species_nr_in_file[HO2_SPECIES] = species_nr;
        }
        if (memcmp(speciesname, "N2O_", 4) == 0)
        {
            info->species_nr_in_file[N2O_SPECIES] = species_nr;
        }
        if (memcmp(speciesname, "NO_", 3) == 0)
        {
            info->species_nr_in_file[NO_SPECIES] = species_nr;
        }
        if (memcmp(speciesname, "O3_", 3) == 0)
        {
            info->species_nr_in_file[O3_SPECIES] = species_nr;
        }
        if (memcmp(speciesname, "O3-666_", 7) == 0)
        {
            info->species_nr_in_file[O3_666_SPECIES] = species_nr;
        }
        if (memcmp(speciesname, "O3-667_", 7) == 0)
        {
            info->species_nr_in_file[O3_667_SPECIES] = species_nr;
        }
        if (memcmp(speciesname, "O3-668_", 7) == 0)
        {
            info->species_nr_in_file[O3_668_SPECIES] = species_nr;
        }
        if (memcmp(speciesname, "O3-686_", 7) == 0)
        {
            info->species_nr_in_file[O3_686_SPECIES] = species_nr;
        }
        if (memcmp(speciesname, "OBrO_", 5) == 0)
        {
            /* In the ODIN SMR L2 documentation this gas is called OBrO */
            /* but in HARP it is called BrO2.                           */
            info->species_nr_in_file[BrO2_SPECIES] = species_nr;
        }
        if (memcmp(speciesname, "TEMP_", 5) == 0)
        {
            info->species_nr_in_file[TEMP_SPECIES] = species_nr;
        }
        if (memcmp(speciesname, "PRES_", 5) == 0)
        {
            info->species_nr_in_file[PRES_SPECIES] = species_nr;
        }
    }
    coda_cursor_goto_root(&cursor);
    return 0;
}

static int init_dimensions(ingest_info *info)
{
    coda_cursor cursor;
    long num_retrieval_records, l;
    short sum;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    /* Count the number of profiles */
    if (coda_cursor_goto(&cursor, "GHz/Data_Vgroup/Geolocation/Latitude") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &info->num_profiles) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    coda_cursor_goto_root(&cursor);

    /* Count the maximum number of altitudes per profile */
    if (coda_cursor_goto(&cursor, "GHz/Data_Vgroup/Retrieval/Naltitudes") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &num_retrieval_records) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    info->num_species = num_retrieval_records / info->num_profiles;
    if (coda_cursor_goto_first_array_element(&cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    info->max_num_altitudes = 0;
    info->num_altitudes = malloc(info->num_profiles * info->num_species * sizeof(short));
    if (info->num_altitudes == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(info->num_profiles * info->num_species * sizeof(short)), __FILE__, __LINE__);
        return -1;
    }
    info->sum_prev_altitudes = malloc(info->num_profiles * info->num_species * sizeof(short));
    if (info->sum_prev_altitudes == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(info->num_profiles * info->num_species * sizeof(short)), __FILE__, __LINE__);
        return -1;
    }
    sum = 0;
    for (l = 0; l < num_retrieval_records; l++)
    {
        if (coda_cursor_read_int16(&cursor, &info->num_altitudes[l]) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        info->sum_prev_altitudes[l] = sum;
        sum += info->num_altitudes[l];
        if (info->num_altitudes[l] > info->max_num_altitudes)
        {
            info->max_num_altitudes = info->num_altitudes[l];
        }
        if (l < (num_retrieval_records - 1L))
        {
            if (coda_cursor_goto_next_array_element(&cursor) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
        }
    }
    coda_cursor_goto_root(&cursor);
    return 0;
}

static void ingestion_done(void *user_data)
{
    ingest_info *info = (ingest_info *)user_data;

    free(info);
}

static int ingestion_init(const harp_ingestion_module *module, coda_product *product,
                          const harp_ingestion_options *options, harp_product_definition **definition, void **user_data)
{
    int format_version;
    ingest_info *info;

    (void)options;

    if (coda_get_product_version(product, &format_version) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    info = malloc(sizeof(ingest_info));
    if (info == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(ingest_info), __FILE__, __LINE__);
        return -1;
    }
    info->product = product;
    info->format_version = format_version;
    info->num_profiles = 0;

    if (init_dimensions(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }
    if (disable_exclude_for_species_in_file(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }

    *definition = *module->product_definition;
    *user_data = info;

    return 0;
}

static int exclude_ClO(void *user_data)
{
    ((ingest_info *)user_data)->current_species_nr = ClO_SPECIES;
    return ((ingest_info *)user_data)->species_nr_in_file[ClO_SPECIES] == 0L;
}

static int exclude_CO(void *user_data)
{
    ((ingest_info *)user_data)->current_species_nr = CO_SPECIES;
    return ((ingest_info *)user_data)->species_nr_in_file[CO_SPECIES] == 0L;
}

static int exclude_H2O(void *user_data)
{
    ((ingest_info *)user_data)->current_species_nr = H2O_SPECIES;
    return ((ingest_info *)user_data)->species_nr_in_file[H2O_SPECIES] == 0L;
}

static int exclude_H2O_161(void *user_data)
{
    ((ingest_info *)user_data)->current_species_nr = H2O_161_SPECIES;
    return ((ingest_info *)user_data)->species_nr_in_file[H2O_161_SPECIES] == 0L;
}

static int exclude_H2O_162(void *user_data)
{
    ((ingest_info *)user_data)->current_species_nr = H2O_162_SPECIES;
    return ((ingest_info *)user_data)->species_nr_in_file[H2O_162_SPECIES] == 0L;
}

static int exclude_H2O_181(void *user_data)
{
    ((ingest_info *)user_data)->current_species_nr = H2O_181_SPECIES;
    return ((ingest_info *)user_data)->species_nr_in_file[H2O_181_SPECIES] == 0L;
}

static int exclude_HNO3(void *user_data)
{
    ((ingest_info *)user_data)->current_species_nr = HNO3_SPECIES;
    return ((ingest_info *)user_data)->species_nr_in_file[HNO3_SPECIES] == 0L;
}

static int exclude_HO2(void *user_data)
{
    ((ingest_info *)user_data)->current_species_nr = HO2_SPECIES;
    return ((ingest_info *)user_data)->species_nr_in_file[HO2_SPECIES] == 0L;
}

static int exclude_N2O(void *user_data)
{
    ((ingest_info *)user_data)->current_species_nr = N2O_SPECIES;
    return ((ingest_info *)user_data)->species_nr_in_file[N2O_SPECIES] == 0L;
}

static int exclude_NO(void *user_data)
{
    ((ingest_info *)user_data)->current_species_nr = NO_SPECIES;
    return ((ingest_info *)user_data)->species_nr_in_file[NO_SPECIES] == 0L;
}

static int exclude_O3(void *user_data)
{
    ((ingest_info *)user_data)->current_species_nr = O3_SPECIES;
    return ((ingest_info *)user_data)->species_nr_in_file[O3_SPECIES] == 0L;
}

static int exclude_O3_666(void *user_data)
{
    ((ingest_info *)user_data)->current_species_nr = O3_666_SPECIES;
    return ((ingest_info *)user_data)->species_nr_in_file[O3_666_SPECIES] == 0L;
}

static int exclude_O3_667(void *user_data)
{
    ((ingest_info *)user_data)->current_species_nr = O3_667_SPECIES;
    return ((ingest_info *)user_data)->species_nr_in_file[O3_667_SPECIES] == 0L;
}

static int exclude_O3_668(void *user_data)
{
    ((ingest_info *)user_data)->current_species_nr = O3_668_SPECIES;
    return ((ingest_info *)user_data)->species_nr_in_file[O3_668_SPECIES] == 0L;
}

static int exclude_O3_686(void *user_data)
{
    ((ingest_info *)user_data)->current_species_nr = O3_686_SPECIES;
    return ((ingest_info *)user_data)->species_nr_in_file[O3_686_SPECIES] == 0L;
}

static int exclude_BrO2(void *user_data)
{
    ((ingest_info *)user_data)->current_species_nr = BrO2_SPECIES;
    return ((ingest_info *)user_data)->species_nr_in_file[BrO2_SPECIES] == 0L;
}

static int exclude_temperature(void *user_data)
{
    ((ingest_info *)user_data)->current_species_nr = TEMP_SPECIES;
    return ((ingest_info *)user_data)->species_nr_in_file[TEMP_SPECIES] == 0L;
}

static int exclude_pressure(void *user_data)
{
    ((ingest_info *)user_data)->current_species_nr = PRES_SPECIES;
    return ((ingest_info *)user_data)->species_nr_in_file[PRES_SPECIES] == 0L;
}

static void add_ingestion_VMR_variables(harp_product_definition *product_definition,
                                        harp_dimension_type *dimension_type, char *species, int exclude_function())
{
    harp_variable_definition *variable_definition;
    const char *path;
    char vmr_description[MAX_PATH_LENGTH], vmr_name[MAX_PATH_LENGTH];
    char precision_description[MAX_PATH_LENGTH], precision_name[MAX_PATH_LENGTH];

    /* volume_mixing_ratio variable */
    snprintf(vmr_description, MAX_PATH_LENGTH, "%s volume mixing ratio", species);
    snprintf(vmr_name, MAX_PATH_LENGTH, "%s_volume_mixing_ratio", species);
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, vmr_name, harp_type_double, 2, dimension_type,
                                                   NULL, vmr_description, NULL, exclude_function, read_profile_value);
    path = "/GHz/Data_Vgroup/Data/Profiles[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path,
                                         "The VMR is converted from ppv to ppmv");

    /* volume_mixing_ratio_uncertainty variable */
    snprintf(precision_description, MAX_PATH_LENGTH, "Precision of the %s volume mixing ratio", species);
    snprintf(precision_name, MAX_PATH_LENGTH, "%s_volume_mixing_ratio_uncertainty", species);
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, precision_name, harp_type_double, 2,
                                                   dimension_type, NULL, precision_description, NULL, exclude_function,
                                                   read_totalerror_value);
    path = "/GHz/Data_Vgroup/Data/TotalError[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path,
                                         "The VMR error is converted from ppv to ppmv");
}

int harp_ingestion_module_smr_l2_init(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2];
    const char *description;
    const char *path;

    description = "SMR Level 2";
    module =
        harp_ingestion_register_module_coda("SMR_L2", "SMR", "ODIN_SMR", "L2", description, ingestion_init,
                                            ingestion_done);

    description = "profile data";
    product_definition = harp_ingestion_register_product(module, "SMR_L2", description, read_dimensions);
    description = "SMR Level 2 products only contain a single profile; all measured profile points will be provided "
        "in reverse order (from low altitude to high altitude) in the profile";
    harp_product_definition_add_mapping(product_definition, description, NULL);

    dimension_type[0] = harp_dimension_time;
    dimension_type[1] = harp_dimension_vertical;

    /* time_per_profile */
    description = "The time of the measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double, 1, dimension_type,
                                                   NULL, description, "seconds since 2000-01-01", NULL, read_datetime);
    path = "/GHz/Data_Vgroup/Geolocation/Time[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path,
                                         "The time converted from TAI93 to seconds since 2000-01-01");

    /* latitude_per_profile */
    description = "The center latitude for a profile";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "latitude_per_profile", harp_type_double, 1,
                                                   dimension_type, NULL, description, "degree_north", NULL,
                                                   read_latitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    path = "/GHz/Data_Vgroup/Geolocation/Latitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* longitude_per_profile */
    description = "The center longitude for a profile";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "longitude_per_profile", harp_type_double, 1,
                                                   dimension_type, NULL, description, "degree_east", NULL,
                                                   read_longitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0, 180.0);
    path = "/GHz/Data_Vgroup/Geolocation/Longitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* solar_zenith_angle_per_profile */
    description = "Average solar zenith angle for the scan";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "solar_zenith_angle_per_profile",
                                                   harp_type_double, 1, dimension_type, NULL, description, NULL, NULL,
                                                   read_solar_zenith_angle);
    path = "/GHz/Data_Vgroup/Geolocation/SunZD[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* altitude */
    description = "The altitude in km for each profile element";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "altitude", harp_type_double, 2, dimension_type,
                                                   NULL, description, "km", NULL, read_altitude);
    path = "/GHz/Data_Vgroup/Data/Altitudes[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    add_ingestion_VMR_variables(product_definition, dimension_type, "ClO", exclude_ClO);
    add_ingestion_VMR_variables(product_definition, dimension_type, "CO", exclude_CO);
    add_ingestion_VMR_variables(product_definition, dimension_type, "H2O", exclude_H2O);
    add_ingestion_VMR_variables(product_definition, dimension_type, "H2O_161", exclude_H2O_161);
    add_ingestion_VMR_variables(product_definition, dimension_type, "H2O_162", exclude_H2O_162);
    add_ingestion_VMR_variables(product_definition, dimension_type, "H2O_181", exclude_H2O_181);
    add_ingestion_VMR_variables(product_definition, dimension_type, "HNO3", exclude_HNO3);
    add_ingestion_VMR_variables(product_definition, dimension_type, "HO2", exclude_HO2);
    add_ingestion_VMR_variables(product_definition, dimension_type, "N2O", exclude_N2O);
    add_ingestion_VMR_variables(product_definition, dimension_type, "NO", exclude_NO);
    add_ingestion_VMR_variables(product_definition, dimension_type, "O3", exclude_O3);
    add_ingestion_VMR_variables(product_definition, dimension_type, "O3_666", exclude_O3_666);
    add_ingestion_VMR_variables(product_definition, dimension_type, "O3_667", exclude_O3_667);
    add_ingestion_VMR_variables(product_definition, dimension_type, "O3_668", exclude_O3_668);
    add_ingestion_VMR_variables(product_definition, dimension_type, "O3_686", exclude_O3_686);
    add_ingestion_VMR_variables(product_definition, dimension_type, "BrO2", exclude_BrO2);

    /* temperature */
    description = "Temperature";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "temperature", harp_type_double, 2,
                                                   dimension_type, NULL, description, "K", exclude_temperature,
                                                   read_profile_value);
    path = "/GHz/Data_Vgroup/Data/Profiles[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* temperature_uncertainty */
    description = "Precision of the temperature";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "temperature_uncertainty", harp_type_double, 2,
                                                   dimension_type, NULL, description, NULL, exclude_temperature,
                                                   read_totalerror_value);
    path = "/GHz/Data_Vgroup/Data/TotalError[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* pressure */
    description = "Pressure";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "pressure", harp_type_double, 2, dimension_type,
                                                   NULL, description, "K", exclude_pressure, read_profile_value);
    path = "/GHz/Data_Vgroup/Data/Profiles[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* pressure_uncertainty */
    description = "Precision of the pressure";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "pressure_uncertainty", harp_type_double, 2,
                                                   dimension_type, NULL, description, NULL, exclude_pressure,
                                                   read_totalerror_value);
    path = "/GHz/Data_Vgroup/Data/TotalError[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    return 0;
}
