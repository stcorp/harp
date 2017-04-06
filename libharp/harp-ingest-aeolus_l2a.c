/*
 * Copyright (C) 2015-2017 S[&]T, The Netherlands.
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PATH_LENGTH 256

typedef struct ingest_info_struct
{
    coda_product *product;
    long num_properties;
    coda_cursor *properties_cursor;
    coda_cursor *pcd_cursor;
} ingest_info;


static int read_dimensions(void *user_data, long dimension[HARP_NUM_DIM_TYPES])
{
    dimension[harp_dimension_time] = ((ingest_info *)user_data)->num_properties;
    dimension[harp_dimension_vertical] = 24;
    return 0;
}

static int get_double_value(coda_cursor cursor, const char *path, harp_array data)
{
    if (coda_cursor_goto(&cursor, path) != 0)
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

static int get_int8_array(coda_cursor cursor, const char *field1, const char *field2, harp_array data)
{
    int i;

    if (coda_cursor_goto_record_field_by_name(&cursor, field1) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_first_array_element(&cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    for (i = 0; i < 24; i++)
    {
        if (coda_cursor_goto_record_field_by_name(&cursor, field2) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_read_int8(&cursor, &data.int8_data[i]) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        coda_cursor_goto_parent(&cursor);
        if (i < 23)
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

static int get_double_array(coda_cursor cursor, const char *field1, const char *field2, harp_array data)
{
    int i;

    if (coda_cursor_goto_record_field_by_name(&cursor, field1) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_first_array_element(&cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    for (i = 0; i < 24; i++)
    {
        if (coda_cursor_goto_record_field_by_name(&cursor, field2) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_read_double(&cursor, &data.double_data[i]) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        coda_cursor_goto_parent(&cursor);
        if (i < 23)
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

static int init_cursors(ingest_info *info)
{
    coda_cursor cursor;
    long num_elements;
    int i;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&cursor, "sca_optical_properties") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &info->num_properties) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    info->properties_cursor = malloc(info->num_properties * sizeof(coda_cursor));
    if (info->properties_cursor == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       info->num_properties * sizeof(coda_cursor), __FILE__, __LINE__);
        return -1;
    }

    if (coda_cursor_goto_first_array_element(&cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    for (i = 0; i < info->num_properties; i++)
    {
        info->properties_cursor[i] = cursor;
        if (i < info->num_properties - 1)
        {
            if (coda_cursor_goto_next_array_element(&cursor) != 0)
            {
                harp_set_error(HARP_ERROR_CODA, NULL);
                return -1;
            }
        }
    }

    if (coda_cursor_goto(&cursor, "/sca_pcd") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &num_elements) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (num_elements != info->num_properties)
    {
        harp_set_error(HARP_ERROR_INGESTION, "product error detected (pcd data set contains %ld records, "
                       "but expected %ld (= number of properties)", num_elements, info->num_properties);
        return -1;
    }

    info->pcd_cursor = malloc(info->num_properties * sizeof(coda_cursor));
    if (info->pcd_cursor == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       info->num_properties * sizeof(coda_cursor), __FILE__, __LINE__);
        return -1;
    }

    if (coda_cursor_goto_first_array_element(&cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    for (i = 0; i < info->num_properties; i++)
    {
        info->pcd_cursor[i] = cursor;
        if (i < info->num_properties - 1)
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

static int read_datetime(void *user_data, long index, harp_array data)
{
    return get_double_value(((ingest_info *)user_data)->properties_cursor[index], "starttime", data);
}

static int read_datetime_length(void *user_data, long index, harp_array data)
{
    (void)user_data;
    (void)index;

    *data.double_data = 12.0;

    return 0;
}

static int read_latitude(void *user_data, long index, harp_array data)
{
    return get_double_array(((ingest_info *)user_data)->properties_cursor[index], "geolocation_middle_bins", "latitude",
                            data);
}

static int read_longitude(void *user_data, long index, harp_array data)
{
    return get_double_array(((ingest_info *)user_data)->properties_cursor[index], "geolocation_middle_bins",
                            "longitude", data);
}

static int read_altitude(void *user_data, long index, harp_array data)
{
    return get_double_array(((ingest_info *)user_data)->properties_cursor[index], "geolocation_middle_bins",
                            "altitude", data);
}

static int read_extinction(void *user_data, long index, harp_array data)
{
    return get_double_array(((ingest_info *)user_data)->properties_cursor[index], "sca_optical_properties",
                            "extinction", data);
}

static int read_backscatter(void *user_data, long index, harp_array data)
{
    return get_double_array(((ingest_info *)user_data)->properties_cursor[index], "sca_optical_properties",
                            "backscatter", data);
}

static int read_lod(void *user_data, long index, harp_array data)
{
    return get_double_array(((ingest_info *)user_data)->properties_cursor[index], "sca_optical_properties",
                            "lod", data);
}

static int read_validity(void *user_data, long index, harp_array data)
{
    return get_int8_array(((ingest_info *)user_data)->pcd_cursor[index], "profile_pcd_bins", "processing_qc_flag",
                          data);
}

static void ingestion_done(void *user_data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->properties_cursor != NULL)
    {
        free(info->properties_cursor);
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
    info->properties_cursor = NULL;

    if (init_cursors(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }

    *definition = *module->product_definition;
    *user_data = info;

    return 0;
}

int harp_ingestion_module_aeolus_l2a_init(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2];
    const char *description;

    dimension_type[0] = harp_dimension_time;
    dimension_type[1] = harp_dimension_vertical;

    description = "AEOLUS Level 2A Optical Properties Product";
    module = harp_ingestion_register_module_coda("AEOLUS_L2A", "AEOLUS", "AEOLUS", "ALD_U_N_2A", description,
                                                 ingestion_init, ingestion_done);

    description = "AEOLUS Level 2A Standard Correct Algorithm (SCA) optical properties";
    product_definition = harp_ingestion_register_product(module, "AEOLUS_L2A_SCA", description, read_dimensions);

    /* datetime_start */
    description = "start time of observation";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition, "datetime",
                                                                       harp_type_double, 1, dimension_type, NULL,
                                                                       description, "seconds since 2000-01-01",
                                                                       NULL, read_datetime);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, "/sca_optical_properties[]/starttime", NULL);

    /* datetime_length */
    description = "duration of the observation";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition, "datetime_length",
                                                                       harp_type_double, 1, dimension_type, NULL,
                                                                       description, "s", NULL, read_datetime_length);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, NULL, "set to fixed value of 12 seconds");

    /* latitude */
    description = "latitude of the bin center";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition, "latitude",
                                                                       harp_type_double, 2, dimension_type, NULL,
                                                                       description, "degree_north", NULL,
                                                                       read_latitude);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/sca_optical_properties[]/geolocation_middle_bins[]/latitude", NULL);

    /* longitude */
    description = "longitude of the bin center";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition, "longitude",
                                                                       harp_type_double, 2, dimension_type, NULL,
                                                                       description, "degree_east", NULL,
                                                                       read_longitude);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/sca_optical_properties[]/geolocation_middle_bins[]/longitude", NULL);

    /* altitude */
    description = "altitude of the bin center";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition, "altitude",
                                                                       harp_type_double, 2, dimension_type, NULL,
                                                                       description, "m", NULL, read_altitude);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/sca_optical_properties[]/geolocation_middle_bins[]/altitude", NULL);

    /* extinction_coefficient */
    description = "particle extinction";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition, "extinction_coefficient",
                                                                       harp_type_double, 2, dimension_type, NULL,
                                                                       description, "10^-6 m^-1", NULL,
                                                                       read_extinction);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/sca_optical_properties[]/sca_optical_properties[]/extinction", NULL);

    /* backscatter_coefficient */
    description = "particle backscatter";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition, "backscatter_coefficient",
                                                                       harp_type_double, 2, dimension_type, NULL,
                                                                       description, "10^-6 m^-1 sr^-1", NULL,
                                                                       read_backscatter);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/sca_optical_properties[]/sca_optical_properties[]/backscatter", NULL);

    /* optical_depth */
    description = "particle local optical depth";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition, "optical_depth",
                                                                       harp_type_double, 2, dimension_type, NULL,
                                                                       description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                                       read_lod);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/sca_optical_properties[]/sca_optical_properties[]/lod", NULL);

    /* validity */
    description = "processing qc flag";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition, "validity", harp_type_int8,
                                                                       2, dimension_type, NULL, description, NULL, NULL,
                                                                       read_validity);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL,
                                         "/sca_pcd[]/profile_pcd_bins[]/processing_qc_flag", NULL);

    return 0;
}
