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
#include <string.h>


typedef struct ingest_info_struct
{
    coda_product *product;
    int format_version;
    long num_vertical;
    int model_temperature;
    int model_air;
    int has_model_air;
    int has_absolute_error;     /* as of PFS 3/K error values are stored as scaled log10() absolute values */
} ingest_info;

static int read_dimensions(void *user_data, long dimension[HARP_NUM_DIM_TYPES])
{
    dimension[harp_dimension_time] = 1;
    dimension[harp_dimension_vertical] = ((ingest_info *)user_data)->num_vertical;
    return 0;
}

static int get_profile_point(ingest_info *info, const char *datasetname, long index, const char *fieldname,
                             harp_array data)
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
    if (coda_cursor_goto_array_element_by_index(&cursor, index) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&cursor, fieldname) != 0)
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

static int get_profile(ingest_info *info, const char *datasetname, const char *fieldname, harp_array data)
{
    coda_cursor cursor;
    int i;

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
    for (i = 0; i < info->num_vertical; i++)
    {
        if (coda_cursor_goto_record_field_by_name(&cursor, fieldname) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_read_double(&cursor, &data.double_data[info->num_vertical - i - 1]) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        coda_cursor_goto_parent(&cursor);

        if (i < info->num_vertical - 1)
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

static int get_std_profile(ingest_info *info, const char *datasetname, const char *fieldname,
                           const char *std_fieldname, harp_array data)
{
    coda_cursor cursor;
    int i;

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
    for (i = 0; i < info->num_vertical; i++)
    {
        long index = info->num_vertical - i - 1;

        if (coda_cursor_goto_record_field_by_name(&cursor, std_fieldname) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_read_double(&cursor, &data.double_data[index]) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        coda_cursor_goto_parent(&cursor);
        if (info->has_absolute_error && strcmp(datasetname, "nl_local_species_density") == 0)
        {
            if (data.double_data[index] == 6554)
            {
                /* set invalid values to NaN */
                data.double_data[index] = harp_nan();
            }
            else
            {
                /* perform exponent scaling */
                data.double_data[index] = pow(10, 0.005 * data.double_data[index]);
            }
        }
        else
        {
            if (data.double_data[index] == 6553.5)
            {
                /* set invalid values to NaN */
                data.double_data[index] = harp_nan();
            }
            else
            {
                double value;

                if (coda_cursor_goto_record_field_by_name(&cursor, fieldname) != 0)
                {
                    harp_set_error(HARP_ERROR_CODA, NULL);
                    return -1;
                }
                if (coda_cursor_read_double(&cursor, &value) != 0)
                {
                    harp_set_error(HARP_ERROR_CODA, NULL);
                    return -1;
                }
                coda_cursor_goto_parent(&cursor);

                /* scale the relative error in '%' to an absolute error */
                data.double_data[index] = fabs(data.double_data[index] * 0.01 * value);
            }
        }

        if (i < info->num_vertical - 1)
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

static int get_pcd_profile(ingest_info *info, const char *datasetname, long pcd_index, harp_array data)
{
    coda_cursor cursor;
    int i;

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
    for (i = 0; i < info->num_vertical; i++)
    {
        if (coda_cursor_goto_record_field_by_name(&cursor, "pcd") != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_goto_array_element_by_index(&cursor, pcd_index) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        if (coda_cursor_read_int16(&cursor, &data.int16_data[info->num_vertical - i - 1]) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
        coda_cursor_goto_parent(&cursor);
        coda_cursor_goto_parent(&cursor);

        if (i < info->num_vertical - 1)
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

static int read_illumination_condition(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    coda_cursor cursor;
    int32_t condition;

    (void)index;    /* prevent unused warning */

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&cursor, "nl_summary_quality") != 0)
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

static int read_datetime(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return get_profile_point(info, "nl_geolocation", info->num_vertical / 2, "dsr_time", data);
}

static int read_datetime_start(void *user_data, harp_array data)
{
    return get_profile_point((ingest_info *)user_data, "nl_geolocation", 0, "dsr_time", data);
}

static int read_datetime_stop(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return get_profile_point(info, "nl_geolocation", info->num_vertical - 1, "dsr_time", data);
}

static int read_altitude(void *user_data, harp_array data)
{
    return get_profile((ingest_info *)user_data, "nl_geolocation", "tangent_alt", data);
}

static int read_latitude(void *user_data, harp_array data)
{
    return get_profile((ingest_info *)user_data, "nl_geolocation", "tangent_lat", data);
}

static int read_longitude(void *user_data, harp_array data)
{
    return get_profile((ingest_info *)user_data, "nl_geolocation", "tangent_long", data);
}

static int read_o3(void *user_data, harp_array data)
{
    return get_profile((ingest_info *)user_data, "nl_local_species_density", "o3", data);
}

static int read_o3_std(void *user_data, harp_array data)
{
    return get_std_profile((ingest_info *)user_data, "nl_local_species_density", "o3", "o3_std", data);
}

static int read_o3_validity(void *user_data, harp_array data)
{
    return get_pcd_profile((ingest_info *)user_data, "nl_local_species_density", 0, data);
}

static int read_no2(void *user_data, harp_array data)
{
    return get_profile((ingest_info *)user_data, "nl_local_species_density", "no2", data);
}

static int read_no2_std(void *user_data, harp_array data)
{
    return get_std_profile((ingest_info *)user_data, "nl_local_species_density", "no2", "no2_std", data);
}

static int read_no2_validity(void *user_data, harp_array data)
{
    return get_pcd_profile((ingest_info *)user_data, "nl_local_species_density", 1, data);
}

static int read_no3(void *user_data, harp_array data)
{
    return get_profile((ingest_info *)user_data, "nl_local_species_density", "no3", data);
}

static int read_no3_std(void *user_data, harp_array data)
{
    return get_std_profile((ingest_info *)user_data, "nl_local_species_density", "no3", "no3_std", data);
}

static int read_no3_validity(void *user_data, harp_array data)
{
    return get_pcd_profile((ingest_info *)user_data, "nl_local_species_density", 2, data);
}

static int read_air(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->model_air)
    {
        return get_profile(info, "nl_geolocation", "tangent_density", data);
    }
    return get_profile(info, "nl_local_species_density", "air", data);
}

static int read_air_std(void *user_data, harp_array data)
{
    return get_std_profile((ingest_info *)user_data, "nl_local_species_density", "air", "air_std", data);
}

static int read_air_validity(void *user_data, harp_array data)
{
    return get_pcd_profile((ingest_info *)user_data, "nl_local_species_density", 3, data);
}

static int read_o2(void *user_data, harp_array data)
{
    return get_profile((ingest_info *)user_data, "nl_local_species_density", "o2", data);
}

static int read_o2_std(void *user_data, harp_array data)
{
    return get_std_profile((ingest_info *)user_data, "nl_local_species_density", "o2", "o2_std", data);
}

static int read_o2_validity(void *user_data, harp_array data)
{
    return get_pcd_profile((ingest_info *)user_data, "nl_local_species_density", 4, data);
}

static int read_h2o(void *user_data, harp_array data)
{
    return get_profile((ingest_info *)user_data, "nl_local_species_density", "h2o", data);
}

static int read_h2o_std(void *user_data, harp_array data)
{
    return get_std_profile((ingest_info *)user_data, "nl_local_species_density", "h2o", "h2o_std", data);
}

static int read_h2o_validity(void *user_data, harp_array data)
{
    return get_pcd_profile((ingest_info *)user_data, "nl_local_species_density", 5, data);
}

static int read_oclo(void *user_data, harp_array data)
{
    return get_profile((ingest_info *)user_data, "nl_local_species_density", "oclo", data);
}

static int read_oclo_std(void *user_data, harp_array data)
{
    return get_std_profile((ingest_info *)user_data, "nl_local_species_density", "oclo", "oclo_std", data);
}

static int read_oclo_validity(void *user_data, harp_array data)
{
    return get_pcd_profile((ingest_info *)user_data, "nl_local_species_density", 6, data);
}

static int read_extinction_coefficient(void *user_data, harp_array data)
{
    return get_profile((ingest_info *)user_data, "nl_aerosols", "local_ext", data);
}

static int read_extinction_coefficient_std(void *user_data, harp_array data)
{
    return get_std_profile((ingest_info *)user_data, "nl_aerosols", "local_ext", "local_ext_std", data);
}

static int read_pressure(void *user_data, harp_array data)
{
    return get_profile((ingest_info *)user_data, "nl_geolocation", "tangent_atm_p", data);
}

static int read_temperature(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (info->model_temperature)
    {
        return get_profile(info, "nl_geolocation", "tangent_temp", data);
    }
    return get_profile(info, "nl_geolocation", "local_temp", data);
}

static int read_temperature_std(void *user_data, harp_array data)
{
    return get_std_profile((ingest_info *)user_data, "nl_geolocation", "local_temp", "local_temp_std", data);
}

static int read_instrument_altitude(void *user_data, harp_array data)
{
    return get_profile((ingest_info *)user_data, "nl_geolocation", "alt", data);
}

static int read_instrument_latitude(void *user_data, harp_array data)
{
    return get_profile((ingest_info *)user_data, "nl_geolocation", "lat", data);
}

static int read_instrument_longitude(void *user_data, harp_array data)
{
    return get_profile((ingest_info *)user_data, "nl_geolocation", "longit", data);
}

static int init_dimensions(ingest_info *info)
{
    coda_cursor cursor;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&cursor, "nl_geolocation") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &info->num_vertical) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

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
    const char *option_value;
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
    info->num_vertical = 0;
    info->model_temperature = 0;
    info->model_air = 0;
    info->has_model_air = 0;
    info->has_absolute_error = 0;

    if (format_version > 0)
    {
        info->has_model_air = 1;
    }
    if (format_version >= 2)
    {
        info->has_absolute_error = 1;
    }

    if (init_dimensions(info) != 0)
    {
        ingestion_done(info);
        return -1;
    }

    if (harp_ingestion_options_get_option(options, "temperature", &option_value) == 0)
    {
        if (strcmp(option_value, "model") == 0)
        {
            info->model_temperature = 1;
        }
    }

    if (harp_ingestion_options_get_option(options, "air", &option_value) == 0)
    {
        if (strcmp(option_value, "model") == 0)
        {
            info->model_air = 1;
        }
    }

    *definition = *module->product_definition;
    *user_data = info;

    return 0;
}

static int exclude_air(void *user_data)
{
    return ((ingest_info *)user_data)->model_air && !((ingest_info *)user_data)->has_model_air;
}

static int exclude_air_std(void *user_data)
{
    return ((ingest_info *)user_data)->model_air;
}

static int exclude_temperature_std(void *user_data)
{
    return ((ingest_info *)user_data)->model_temperature;
}

int harp_ingestion_module_gomos_l2_init(void)
{
    const char *model_options[] = {"local", "model"};
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2];
    const char *description_std_rel;
    const char *description_std_abs;
    const char *description;
    const char *path;

    description = "GOMOS Temperature and Atmospheric Constituents Profiles";
    module = harp_ingestion_register_module_coda("GOMOS_L2", "ENVISAT_GOMOS", "GOM_NL__2P", description, NULL,
                                                 ingestion_init, ingestion_done);

    harp_ingestion_register_option(module, "temperature", "can be either 'local' or 'model'; 'local' (the default) "
                                   "will retrieve the locally measured temperature and 'model' will return the "
                                   "temperature from the external model", 2, model_options);

    harp_ingestion_register_option(module, "air", "can be either 'local' or 'model'; 'local' (the default) will "
                                   "retrieve the locally measured air density and 'model' will return the air density "
                                   "from the external model", 2, model_options);

    description = "profile data";
    product_definition = harp_ingestion_register_product(module, "GOMOS_NL_L2", description, read_dimensions);
    description = "GOMOS Level 2 products only contain a single profile; all measured profile points will be provided "
        "in reverse order (from low altitude to high altitude) in the profile";
    harp_product_definition_add_mapping(product_definition, description, NULL);

    dimension_type[0] = harp_dimension_time;
    dimension_type[1] = harp_dimension_vertical;

    description_std_rel = "values equal to 6553.5% will be set to NaN; value will be converted to an uncertainty by "
        "multiplying with the absolute value of the measured concentration";
    description_std_abs = "values equal to 6554 will be set to NaN; value will be converted to an uncertainty by "
        "using the log10(v)/0.005 conversion";

    /* datetime */
    description = "time of the profile";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double,
                                                                     1, dimension_type, NULL, description,
                                                                     "seconds since 2000-01-01", NULL, read_datetime);
    path = "/nl_geolocation[]/dsr_time";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, "time of mid record");

    /* datetime_start */
    description = "start time of the profile";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "datetime_start",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, "seconds since 2000-01-01", NULL,
                                                                     read_datetime_start);
    path = "/nl_geolocation[]/dsr_time";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, "time of first record");

    /* datetime_stop */
    description = "stop time of the profile";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "datetime_stop",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, "seconds since 2000-01-01", NULL,
                                                                     read_datetime_stop);
    path = "/nl_geolocation[]/dsr_time";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, "time of last record");

    /* altitude */
    description = "altitude";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "altitude", harp_type_double,
                                                                     2, dimension_type, NULL, description, "m", NULL,
                                                                     read_altitude);
    path = "/nl_geolocation[]/tangent_alt";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* latitude */
    description = "latitude";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_double,
                                                                     2, dimension_type, NULL, description,
                                                                     "degree_north", NULL, read_latitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    path = "/nl_geolocation[]/tangent_lat";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* longitude */
    description = "longitude";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "longitude",
                                                                     harp_type_double, 2, dimension_type, NULL,
                                                                     description, "degree_east", NULL,
                                                                     read_longitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0, 180.0);
    path = "/nl_geolocation[]/tangent_long";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* O3_number_density */
    description = "Ozone local density";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "O3_number_density",
                                                                     harp_type_double, 2, dimension_type, NULL,
                                                                     description, "molec/cm3", NULL,
                                                                     read_o3);
    path = "/nl_local_species_density[]/o3";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* O3_number_density_uncertainty */
    description = "standard deviation for the ozone local density";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition,
                                                                     "O3_number_density_uncertainty", harp_type_double,
                                                                     2, dimension_type, NULL, description, "molec/cm3",
                                                                     NULL, read_o3_std);
    path = "/nl_local_species_density[]/o3_std";
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA product version < 2", path,
                                         description_std_rel);
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA product version >= 2", path,
                                         description_std_abs);

    /* O3_number_density_validity */
    description = "PCD (product confidence data) value for the ozone local density";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition,
                                                                     "O3_number_density_validity", harp_type_int16,
                                                                     2, dimension_type, NULL, description, NULL,
                                                                     NULL, read_o3_validity);
    path = "/nl_local_species_density[]/pcd[0]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* NO2_number_density */
    description = "NO2 local density";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "NO2_number_density",
                                                                     harp_type_double, 2, dimension_type, NULL,
                                                                     description, "molec/cm3", NULL,
                                                                     read_no2);
    path = "/nl_local_species_density[]/no2";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* NO2_number_density_uncertainty */
    description = "standard deviation for the NO2 local density";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition,
                                                                     "NO2_number_density_uncertainty", harp_type_double,
                                                                     2, dimension_type, NULL, description, "molec/cm3",
                                                                     NULL, read_no2_std);
    path = "/nl_local_species_density[]/no2_std";
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA product version < 2", path,
                                         description_std_rel);
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA product version >= 2", path,
                                         description_std_abs);

    /* NO2_number_density_validity */
    description = "PCD (product confidence data) value for the NO2 local density";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition,
                                                                     "NO2_number_density_validity", harp_type_int16,
                                                                     2, dimension_type, NULL, description, NULL,
                                                                     NULL, read_no2_validity);
    path = "/nl_local_species_density[]/pcd[1]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* NO3_number_density */
    description = "NO3 local density";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "NO3_number_density",
                                                                     harp_type_double, 2, dimension_type, NULL,
                                                                     description, "molec/cm3", NULL,
                                                                     read_no3);
    path = "/nl_local_species_density[]/no3";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* NO3_number_density_uncertainty */
    description = "standard deviation for the NO3 local density";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition,
                                                                     "NO3_number_density_uncertainty", harp_type_double,
                                                                     2, dimension_type, NULL, description, "molec/cm3",
                                                                     NULL, read_no3_std);
    path = "/nl_local_species_density[]/no3_std";
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA product version < 2", path,
                                         description_std_rel);
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA product version >= 2", path,
                                         description_std_abs);

    /* NO3_number_density_validity */
    description = "PCD (product confidence data) value for the NO3 local density";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition,
                                                                     "NO3_number_density_validity", harp_type_int16,
                                                                     2, dimension_type, NULL, description, NULL,
                                                                     NULL, read_no3_validity);
    path = "/nl_local_species_density[]/pcd[2]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* O2_number_density */
    description = "O2 local density";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "O2_number_density",
                                                                     harp_type_double, 2, dimension_type, NULL,
                                                                     description, "molec/cm3", NULL,
                                                                     read_o2);
    path = "/nl_local_species_density[]/o2";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* O2_number_density_uncertainty */
    description = "standard deviation for the O2 local density";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition,
                                                                     "O2_number_density_uncertainty", harp_type_double,
                                                                     2, dimension_type, NULL, description, "molec/cm3",
                                                                     NULL, read_o2_std);
    path = "/nl_local_species_density[]/o2_std";
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA product version < 2", path,
                                         description_std_rel);
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA product version >= 2", path,
                                         description_std_abs);

    /* O2_number_density_validity */
    description = "PCD (product confidence data) value for the O2 local density";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition,
                                                                     "O2_number_density_validity", harp_type_int16,
                                                                     2, dimension_type, NULL, description, NULL,
                                                                     NULL, read_o2_validity);
    path = "/nl_local_species_density[]/pcd[4]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* H2O_number_density */
    description = "H2O local density";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "H2O_number_density",
                                                                     harp_type_double, 2, dimension_type, NULL,
                                                                     description, "molec/cm3", NULL,
                                                                     read_h2o);
    path = "/nl_local_species_density[]/h2o";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* H2O_number_density_uncertainty */
    description = "standard deviation for the H2O local density";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition,
                                                                     "H2O_number_density_uncertainty", harp_type_double,
                                                                     2, dimension_type, NULL, description, "molec/cm3",
                                                                     NULL, read_h2o_std);
    path = "/nl_local_species_density[]/h2o_std";
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA product version < 2", path,
                                         description_std_rel);
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA product version >= 2", path,
                                         description_std_abs);

    /* H2O_number_density_validity */
    description = "PCD (product confidence data) value for the H2O local density";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition,
                                                                     "H2O_number_density_validity", harp_type_int16,
                                                                     2, dimension_type, NULL, description, NULL,
                                                                     NULL, read_h2o_validity);
    path = "/nl_local_species_density[]/pcd[5]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* OClO_number_density */
    description = "OClO local density";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "OClO_number_density",
                                                                     harp_type_double, 2, dimension_type, NULL,
                                                                     description, "molec/cm3", NULL,
                                                                     read_oclo);
    path = "/nl_local_species_density[]/oclo";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* OClO_number_density_uncertainty */
    description = "standard deviation for the OClO local density";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition,
                                                                     "OClO_number_density_uncertainty",
                                                                     harp_type_double, 2, dimension_type, NULL,
                                                                     description, "molec/cm3", NULL, read_oclo_std);
    path = "/nl_local_species_density[]/oclo_std";
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA product version < 2", path,
                                         description_std_rel);
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA product version >= 2", path,
                                         description_std_abs);

    /* OClO_number_density_validity */
    description = "PCD (product confidence data) value for the OClO local density";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition,
                                                                     "OClO_number_density_validity", harp_type_int16,
                                                                     2, dimension_type, NULL, description, NULL,
                                                                     NULL, read_oclo_validity);
    path = "/nl_local_species_density[]/pcd[6]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* aerosol_extinction_coefficient */
    description = "aerosol extinction coefficient";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition,
                                                                     "aerosol_extinction_coefficient", harp_type_double,
                                                                     2, dimension_type, NULL, description, "1/km", NULL,
                                                                     read_extinction_coefficient);
    path = "/nl_aerosols[]/local_ext";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* aerosol_extinction_coefficient_uncertainty */
    description = "standard deviation for the aerosol extinction coefficient";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition,
                                                                     "aerosol_extinction_coefficient_uncertainty",
                                                                     harp_type_double, 2, dimension_type, NULL,
                                                                     description, "1/km", NULL,
                                                                     read_extinction_coefficient_std);
    path = "/nl_aerosols[]/local_ext_std";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description_std_rel);

    /* pressure */
    description = "atmospheric pressure from external model";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "pressure",
                                                                     harp_type_double, 2, dimension_type, NULL,
                                                                     description, "Pa", NULL,
                                                                     read_pressure);
    path = "/nl_geolocation[]/tangent_atm_p";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* temperature */
    description = "temperature";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "temperature",
                                                                     harp_type_double, 2, dimension_type, NULL,
                                                                     description, "K", NULL,
                                                                     read_temperature);
    path = "/nl_geolocation[]/local_temp";
    harp_variable_definition_add_mapping(variable_definition, "temperature=local", NULL, path, NULL);
    path = "/nl_geolocation[]/tangent_temp";
    harp_variable_definition_add_mapping(variable_definition, "temperature=model", NULL, path, NULL);

    /* temperature_uncertainty */
    description = "standard deviation for the local temperature";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "temperature_uncertainty",
                                                                     harp_type_double, 2, dimension_type, NULL,
                                                                     description, "K", exclude_temperature_std,
                                                                     read_temperature_std);
    path = "/nl_geolocation[]/local_temp_std";
    harp_variable_definition_add_mapping(variable_definition, "temperature=local", NULL, path, NULL);

    /* number_density */
    description = "air density";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "number_density",
                                                                     harp_type_double, 2, dimension_type, NULL,
                                                                     description, "molec/cm3", exclude_air,
                                                                     read_air);
    path = "/nl_local_species_density[]/air";
    harp_variable_definition_add_mapping(variable_definition, "air=local", NULL, path, NULL);
    path = "/nl_geolocation[]/tangent_density";
    harp_variable_definition_add_mapping(variable_definition, "air=model", "CODA product version > 0", path, NULL);

    /* number_density_uncertainty */
    description = "standard deviation for the local air density";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition,
                                                                     "number_density_uncertainty", harp_type_double,
                                                                     2, dimension_type, NULL, description, "molec/cm3",
                                                                     exclude_air_std, read_air_std);
    path = "/nl_local_species_density[]/air_std";
    harp_variable_definition_add_mapping(variable_definition, "air=local", "CODA product version < 2", path,
                                         description_std_rel);
    harp_variable_definition_add_mapping(variable_definition, "air=local", "CODA product version >= 2", path,
                                         description_std_abs);

    /* number_density_validity */
    description = "PCD (product confidence data) value for the local air density";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition,
                                                                     "number_density_validity", harp_type_int16,
                                                                     2, dimension_type, NULL, description, NULL,
                                                                     exclude_air_std, read_air_validity);
    path = "/nl_local_species_density[]/pcd[3]";
    harp_variable_definition_add_mapping(variable_definition, "air=local", NULL, path, NULL);

    /* instrument_altitude */
    description = "altitude of the satellite";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "instrument_altitude",
                                                                     harp_type_double, 2, dimension_type, NULL,
                                                                     description, "m", NULL, read_instrument_altitude);
    path = "/nl_geolocation[]/tangent_alt";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* instrument_latitude */
    description = "latitude of the satellite position";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "instrument_latitude",
                                                                     harp_type_double, 2, dimension_type, NULL,
                                                                     description, "degree_north", NULL,
                                                                     read_instrument_latitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    path = "/nl_geolocation[]/lat";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* instrument_longitude */
    description = "longitude of the satellite position";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "instrument_longitude",
                                                                     harp_type_double, 2, dimension_type, NULL,
                                                                     description, "degree_east", NULL,
                                                                     read_instrument_longitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0, 180.0);
    path = "/nl_geolocation[]/longit";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* flag_illumination_condition */
    description = "illumination condition for the profile: 'dark', 'bright', 'twilight', 'strailight', or "
        "'twilight/strailight'";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition,
                                                                       "flag_illumination_condition", harp_type_string,
                                                                       1, dimension_type, NULL, description, NULL, NULL,
                                                                       read_illumination_condition);
    path = "/nl_summary_quality[0]/limb_flag";
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA product version 0", path, NULL);
    path = "/nl_summary_quality[0]/obs_illum_cond";
    harp_variable_definition_add_mapping(variable_definition, NULL, "CODA product version 1 and higher", path, NULL);


    return 0;
}
