/*
 * Copyright (C) 2015 S[&]T, The Netherlands.
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Number of seconds between 2000/01/01 TAI and 2010/01/01 UTC. */
#define SECONDS_FROM_2000_TAI_TO_2010_UTC (315619200 + 34)

/* Default fill value taken from "Input/output data specification for the TROPOMI L-1b data processor",
 * S5P-KNMI-L01B-0012-SD.
 */
#define DEFAULT_FILL_VALUE_INT (-2147483647)

/* Macro to determine the number of elements in a one dimensional C array. */
#define ARRAY_SIZE(X) (sizeof((X))/sizeof((X)[0]))

typedef enum s5p_dim_type_enum
{
    s5p_dim_time,
    s5p_dim_scanline,
    s5p_dim_pixel,
    s5p_dim_channel
} s5p_dim_type;

#define S5P_NUM_DIM_TYPES  (((int)s5p_dim_channel) + 1)

typedef struct variable_descriptor_struct
{
    coda_cursor cursor;
    long (*get_offset) (const long *dimension, long index);
    long length;
    float fill_value;
} variable_descriptor;

typedef struct ingest_info_struct
{
    coda_product *product;
    long dimension[S5P_NUM_DIM_TYPES];
    int band;

    coda_cursor sensor_mode_cursor;
    coda_cursor geo_data_cursor;
    coda_cursor observation_cursor;
    coda_cursor instrument_cursor;

    variable_descriptor wavelength;
    variable_descriptor irradiance;
    variable_descriptor radiance;
} ingest_info;

static void filter_array_float(long num_elements, float *data, float fill_value)
{
    float *data_end;

    for (data_end = data + num_elements; data != data_end; ++data)
    {
        if (*data == fill_value)
        {
            *data = coda_NaN();
        }
    }
}

static void broadcast_array_float(long num_scanlines, long num_pixels, float *data)
{
    long i;

    /* Repeat the value for each scanline for all pixels in that scanline. Iterate in reverse to avoid overwriting
     * scanline values.
     */
    for (i = num_scanlines - 1; i >= 0; --i)
    {
        float *pixel = data + i * num_pixels;
        float *pixel_end = pixel + num_pixels;
        const float scanline_value = data[i];

        for (; pixel != pixel_end; ++pixel)
        {
            *pixel = scanline_value;
        }
    }
}

static void broadcast_array_double(long num_scanlines, long num_pixels, double *data)
{
    long i;

    /* Repeat the value for each scanline for all pixels in that scanline. Iterate in reverse to avoid overwriting
     * scanline values.
     */
    for (i = num_scanlines - 1; i >= 0; --i)
    {
        double *pixel = data + i * num_pixels;
        double *pixel_end = pixel + num_pixels;
        const double scanline_value = data[i];

        for (; pixel != pixel_end; ++pixel)
        {
            *pixel = scanline_value;
        }
    }
}

static long get_offset_wavelength(const long *dimension, long index)
{
    index = index - (index / dimension[s5p_dim_pixel]) * dimension[s5p_dim_pixel];
    return index * dimension[s5p_dim_channel];
}

static const char *get_variable_name_from_cursor(coda_cursor *cursor)
{
    coda_cursor parent_cursor;
    coda_type *parent_type;
    const char *variable_name;
    long index;

    variable_name = "<unknown variable name>";
    if (coda_cursor_get_index(cursor, &index) != 0)
    {
        return variable_name;
    }

    parent_cursor = *cursor;
    if (coda_cursor_goto_parent(&parent_cursor) != 0)
    {
        return variable_name;
    }
    if (coda_cursor_get_type(&parent_cursor, &parent_type) != 0)
    {
        return variable_name;
    }
    if (coda_type_get_record_field_real_name(parent_type, index, &variable_name) != 0)
    {
        return variable_name;
    }

    return variable_name;
}

static int verify_variable_dimensions(coda_cursor *cursor, int num_dimensions, const long *dimension)
{
    long coda_dimension[CODA_MAX_NUM_DIMS];
    int num_coda_dimensions;
    int i;

    if (coda_cursor_get_array_dim(cursor, &num_coda_dimensions, coda_dimension) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    if (num_coda_dimensions != num_dimensions)
    {
        harp_set_error(HARP_ERROR_INGESTION, "product error detected (variable '%s' has %d dimensions, expected %d)",
                       get_variable_name_from_cursor(cursor), num_coda_dimensions, num_dimensions);
        return -1;
    }

    for (i = 0; i < num_dimensions; i++)
    {
        if (coda_dimension[i] != dimension[i])
        {
            harp_set_error(HARP_ERROR_INGESTION,
                           "product error detected (dimension %d of variable '%s' has %ld elements," " expected %ld)",
                           i, get_variable_name_from_cursor(cursor), coda_dimension[i], dimension[i]);
            return -1;
        }
    }

    return 0;
}

static int get_variable_attributes(coda_cursor *cursor, float *fill_value)
{
    if (coda_cursor_goto_attributes(cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(cursor, "FillValue") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_first_array_element(cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_float(cursor, fill_value) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    coda_cursor_goto_parent(cursor);
    coda_cursor_goto_parent(cursor);
    coda_cursor_goto_parent(cursor);

    return 0;
}

static int variable_descriptor_init(coda_cursor *cursor, const char *name, int num_dimensions, const long *dimension,
                                    long (*get_offset) (const long *dimension, long index), long length,
                                    variable_descriptor *descriptor)
{
    descriptor->cursor = *cursor;
    descriptor->get_offset = get_offset;
    descriptor->length = length;

    if (coda_cursor_goto(&descriptor->cursor, name) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (verify_variable_dimensions(&descriptor->cursor, num_dimensions, dimension) != 0)
    {
        return -1;
    }
    if (get_variable_attributes(&descriptor->cursor, &descriptor->fill_value) != 0)
    {
        return -1;
    }

    return 0;
}

static int get_data_availability(coda_product *product, int band, int *product_has_data)
{
    coda_type *root_type;
    char product_group_name[17];
    long num_fields;
    long i;

    snprintf(product_group_name, ARRAY_SIZE(product_group_name), "BAND%u_IRRADIANCE", band);
    if (coda_get_product_root_type(product, &root_type) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    if (coda_type_get_num_record_fields(root_type, &num_fields) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    for (i = 0; i < num_fields; i++)
    {
        const char *field_name;

        if (coda_type_get_record_field_real_name(root_type, i, &field_name) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }

        if (strcmp(field_name, product_group_name) == 0)
        {
            *product_has_data = 1;
            return 0;
        }
    }

    *product_has_data = 0;
    return 0;
}

static int get_dimension_length(ingest_info *info, const char *dimension_name, long *dimension_length)
{
    coda_cursor cursor;
    long dim[CODA_MAX_NUM_DIMS];
    int num_dims;

    cursor = info->sensor_mode_cursor;
    if (coda_cursor_goto_record_field_by_name(&cursor, dimension_name) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    if (coda_cursor_get_array_dim(&cursor, &num_dims, dim) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    if (num_dims != 1)
    {
        harp_set_error(HARP_ERROR_INGESTION, "product error detected (cannot determine length of dimension '%s')",
                       dimension_name);
        return -1;
    }

    *dimension_length = dim[0];
    return 0;
}

static int read_variable_float(ingest_info *info, coda_cursor *cursor, const char *name, int num_dimensions,
                               const long *dimension, harp_array data)
{
    long default_dimension[4] = { 1, info->dimension[s5p_dim_scanline], info->dimension[s5p_dim_pixel], 4 };
    long num_elements;
    float fill_value;

    if (coda_cursor_goto_record_field_by_name(cursor, name) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    assert(dimension != NULL || num_dimensions <= 4);
    if (verify_variable_dimensions(cursor, num_dimensions, (dimension == NULL ? default_dimension : dimension)) != 0)
    {
        return -1;
    }
    if (get_variable_attributes(cursor, &fill_value) != 0)
    {
        return -1;
    }
    if (coda_cursor_read_float_array(cursor, data.float_data, coda_array_ordering_c) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    coda_cursor_goto_parent(cursor);

    /* Replace values equal to the variable specific _FillValue attribute by NaN. */
    num_elements = harp_get_num_elements(num_dimensions, (dimension == NULL ? default_dimension : dimension));
    filter_array_float(num_elements, data.float_data, fill_value);

    return 0;
}

static int read_variable_partial_float(ingest_info *info, variable_descriptor *descriptor, long index, harp_array data)
{
    long offset;

    if (descriptor->get_offset == NULL)
    {
        offset = index * descriptor->length;
    }
    else
    {
        offset = descriptor->get_offset(info->dimension, index);
    }

    if (coda_cursor_read_float_partial_array(&descriptor->cursor, offset, descriptor->length, data.float_data) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    /* Replace values equal to the variable specific _FillValue attribute by NaN. */
    filter_array_float(descriptor->length, data.float_data, descriptor->fill_value);

    return 0;
}

static int init_cursors(ingest_info *info, const char *product_group_name)
{
    coda_cursor cursor;

    if (coda_cursor_set_product(&cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (product_group_name == NULL)
    {
        if (coda_cursor_goto_first_record_field(&cursor) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
    }
    else
    {
        if (coda_cursor_goto_record_field_by_name(&cursor, product_group_name) != 0)
        {
            harp_set_error(HARP_ERROR_CODA, NULL);
            return -1;
        }
    }

    if (coda_cursor_goto_record_field_by_name(&cursor, "STANDARD_MODE") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    info->sensor_mode_cursor = cursor;

    if (coda_cursor_goto_record_field_by_name(&cursor, "GEODATA") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    info->geo_data_cursor = cursor;

    if (coda_cursor_goto(&cursor, "../OBSERVATIONS") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    info->observation_cursor = cursor;

    if (coda_cursor_goto(&cursor, "../INSTRUMENT") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    info->instrument_cursor = cursor;

    return 0;
}

static int init_dimensions(ingest_info *info, const char **dimension_names)
{
    int i;

    for (i = 0; i < S5P_NUM_DIM_TYPES; i++)
    {
        if (get_dimension_length(info, dimension_names[i], &info->dimension[i]) != 0)
        {
            return -1;
        }
    }

    return 0;
}

static int parse_option_band(ingest_info *info, const harp_ingestion_options *options)
{
    const char *value;

    if (harp_ingestion_options_get_option(options, "band", &value) == 0)
    {
        info->band = *value - '0';
    }

    return 0;
}

static void ingestion_done(void *user_data)
{
    free(user_data);
}

static int ingestion_init_s5p_l1b_ir(const harp_ingestion_module *module, coda_product *product,
                                     const harp_ingestion_options *options, harp_product_definition **definition,
                                     void **user_data)
{
    ingest_info *info;
    const char *dimension_names[4] = { "time", "scanline", "pixel", "spectral_channel" };
    long dimension_wavelength[3];
    char product_group_name[17];
    int product_has_data;

    info = malloc(sizeof(ingest_info));
    if (info == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(ingest_info), __FILE__, __LINE__);
        return -1;
    }

    info->product = product;
    info->band = 1;
    memset(info->dimension, 0, S5P_NUM_DIM_TYPES * sizeof(long));

    if (parse_option_band(info, options) != 0)
    {
        ingestion_done(info);
        return -1;
    }

    if (get_data_availability(info->product, info->band, &product_has_data) != 0)
    {
        ingestion_done(info);
        return -1;
    }

    if (!product_has_data)
    {
        harp_set_error(HARP_ERROR_NO_DATA, NULL);
        ingestion_done(info);
        return -1;
    }

    snprintf(product_group_name, ARRAY_SIZE(product_group_name), "BAND%d_IRRADIANCE", info->band);
    if (init_cursors(info, product_group_name) != 0)
    {
        ingestion_done(info);
        return -1;
    }

    if (init_dimensions(info, dimension_names) != 0)
    {
        ingestion_done(info);
        return -1;
    }

    dimension_wavelength[0] = 1;
    dimension_wavelength[1] = info->dimension[s5p_dim_pixel];
    dimension_wavelength[2] = info->dimension[s5p_dim_channel];
    if (variable_descriptor_init(&info->instrument_cursor, "nominal_wavelength", 3, dimension_wavelength,
                                 get_offset_wavelength, info->dimension[s5p_dim_channel], &info->wavelength) != 0)
    {
        ingestion_done(info);
        return -1;
    }

    if (variable_descriptor_init(&info->observation_cursor, "irradiance", 4, info->dimension, NULL,
                                 info->dimension[s5p_dim_channel], &info->irradiance) != 0)
    {
        ingestion_done(info);
        return -1;
    }

    assert(info->band >= 1 && info->band <= 8);
    *definition = module->product_definition[info->band - 1];
    *user_data = info;

    return 0;
}

static int ingestion_init_s5p_l1b_ra(const harp_ingestion_module *module, coda_product *product,
                                     const harp_ingestion_options *options, harp_product_definition **definition,
                                     void **user_data)
{
    ingest_info *info;
    const char *dimension_names[4] = { "time", "scanline", "ground_pixel", "spectral_channel" };
    long dimension_wavelength[3];

    (void)options;

    info = malloc(sizeof(ingest_info));
    if (info == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(ingest_info), __FILE__, __LINE__);
        return -1;
    }

    info->product = product;
    info->band = 1;
    memset(info->dimension, 0, S5P_NUM_DIM_TYPES * sizeof(long));

    if (init_cursors(info, NULL) != 0)
    {
        ingestion_done(info);
        return -1;
    }

    if (init_dimensions(info, dimension_names) != 0)
    {
        ingestion_done(info);
        return -1;
    }

    dimension_wavelength[0] = 1;
    dimension_wavelength[1] = info->dimension[s5p_dim_pixel];
    dimension_wavelength[2] = info->dimension[s5p_dim_channel];
    if (variable_descriptor_init(&info->instrument_cursor, "nominal_wavelength", 3, dimension_wavelength,
                                 get_offset_wavelength, info->dimension[s5p_dim_channel], &info->wavelength) != 0)
    {
        ingestion_done(info);
        return -1;
    }

    if (variable_descriptor_init(&info->observation_cursor, "radiance", 4, info->dimension, NULL,
                                 info->dimension[s5p_dim_channel], &info->radiance) != 0)
    {
        ingestion_done(info);
        return -1;
    }

    *definition = *module->product_definition;
    *user_data = info;

    return 0;
}

static int read_dimensions(void *user_data, long dimension[HARP_NUM_DIM_TYPES])
{
    ingest_info *info;

    info = (ingest_info *)user_data;
    dimension[harp_dimension_time] = info->dimension[s5p_dim_scanline] * info->dimension[s5p_dim_pixel];
    dimension[harp_dimension_spectral] = info->dimension[s5p_dim_channel];

    return 0;
}

static int read_scanline_pixel_index(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    index = index - (index / info->dimension[s5p_dim_pixel]) * info->dimension[s5p_dim_pixel];
    *data.int16_data = (int16_t)index;

    return 0;
}

static int read_datetime(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    double *datetime;
    double *datetime_end;
    coda_cursor cursor;
    double time_reference;

    /* NB. It seems that leap seconds are not handled properly in the product. The product specification "Input/output
     * data specification for the TROPOMI L01b data processor" [S5P-KNMI-L01B-0012-SD], issue 4.0.0, date 2014-12-09,
     * page 38, section 8.5 "Variable: time" states that the UTC time defined by the variable 'time' (stored as a number
     * of seconds since 2010-01-01) corresponds to the UTC time defined by the global attribute 'time_reference' (stored
     * as text).
     *
     * The sample product S5P_TEST_L1B_IR_SIR_20140827T114200_20140827T115800_53811_01_000800_20141209T120000.nc,
     * however, yields the following:
     *
     *     time_reference = 2014-08-27T00:00:00Z
     *     time = 146793600
     *
     * Yet, the number of seconds since 2010-01-01 00:00:00 UTC for 2014-08-27 00:00:00 UTC computed with proper
     * handling of leap seconds is: 146793601 (due to the leap second introduced on January 30, 2012).
     */

    /* Read reference time in seconds since 2010-01-01 00:00:00 UTC (probably wrong, see above). */
    cursor = info->observation_cursor;
    if (coda_cursor_goto_record_field_by_name(&cursor, "time") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (verify_variable_dimensions(&cursor, 1, info->dimension) != 0)
    {
        return -1;
    }
    if (coda_cursor_goto_first_array_element(&cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_double(&cursor, &time_reference) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    /* Read difference in milliseconds (ms) between the time reference and the start of the observation. */
    cursor = info->observation_cursor;
    if (coda_cursor_goto_record_field_by_name(&cursor, "delta_time") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (verify_variable_dimensions(&cursor, 2, info->dimension) != 0)
    {
        return -1;
    }
    if (coda_cursor_read_double_array(&cursor, data.double_data, coda_array_ordering_c) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    /* Convert observation start time to seconds since 2000-01-01 00:00:00 TAI. */
    for (datetime = data.double_data, datetime_end = datetime + info->dimension[s5p_dim_scanline];
         datetime != datetime_end; ++datetime)
    {
        if (time_reference == DEFAULT_FILL_VALUE_INT || *datetime == DEFAULT_FILL_VALUE_INT)
        {
            *datetime = coda_NaN();
        }
        else
        {
            *datetime = SECONDS_FROM_2000_TAI_TO_2010_UTC + time_reference + *datetime / 1e3;
        }
    }

    /* Broadcast the result along the pixel dimension. */
    broadcast_array_double(info->dimension[s5p_dim_scanline], info->dimension[s5p_dim_pixel], data.double_data);

    return 0;
}

static int read_longitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(info, &info->geo_data_cursor, "longitude", 3, NULL, data);
}

static int read_longitude_bounds(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(info, &info->geo_data_cursor, "longitude_bounds", 4, NULL, data);
}

static int read_latitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(info, &info->geo_data_cursor, "latitude", 3, NULL, data);
}

static int read_latitude_bounds(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(info, &info->geo_data_cursor, "latitude_bounds", 4, NULL, data);
}

static int read_instrument_longitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (read_variable_float(info, &info->geo_data_cursor, "satellite_longitude", 2, NULL, data) != 0)
    {
        return -1;
    }

    broadcast_array_float(info->dimension[s5p_dim_scanline], info->dimension[s5p_dim_pixel], data.float_data);

    return 0;
}

static int read_instrument_latitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (read_variable_float(info, &info->geo_data_cursor, "satellite_latitude", 2, NULL, data) != 0)
    {
        return -1;
    }

    broadcast_array_float(info->dimension[s5p_dim_scanline], info->dimension[s5p_dim_pixel], data.float_data);

    return 0;
}

static int read_instrument_altitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (read_variable_float(info, &info->geo_data_cursor, "satellite_altitude", 2, NULL, data) != 0)
    {
        return -1;
    }

    broadcast_array_float(info->dimension[s5p_dim_scanline], info->dimension[s5p_dim_pixel], data.float_data);

    return 0;
}

static int read_solar_azimuth_angle(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(info, &info->geo_data_cursor, "solar_azimuth_angle", 3, NULL, data);
}

static int read_solar_zenith_angle(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(info, &info->geo_data_cursor, "solar_zenith_angle", 3, NULL, data);
}

static int read_viewing_azimuth_angle(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(info, &info->geo_data_cursor, "viewing_azimuth_angle", 3, NULL, data);
}

static int read_viewing_zenith_angle(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_float(info, &info->geo_data_cursor, "viewing_zenith_angle", 3, NULL, data);
}

static int read_wavelength(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_partial_float(info, &info->wavelength, index, data);
}

static int read_photon_irradiance(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_partial_float(info, &info->irradiance, index, data);
}

static int read_photon_radiance(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_partial_float(info, &info->radiance, index, data);
}

static int verify_s5p_l1b_ir(const harp_ingestion_module *module, coda_product *product)
{
    coda_type *root_type;
    long num_fields;
    long i;

    (void)module;

    /* TODO: Use more information from the product itself (e.g. from the METADATA section?) to make this check more
     * strict.
     */
    if (coda_get_product_root_type(product, &root_type) != 0)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, NULL);
        return -1;
    }
    if (coda_type_get_num_record_fields(root_type, &num_fields) != 0)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, NULL);
        return -1;
    }

    for (i = 0; i < num_fields; i++)
    {
        const char *field_name;

        if (coda_type_get_record_field_real_name(root_type, i, &field_name) != 0)
        {
            harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, NULL);
            return -1;
        }
        if (strlen(field_name) != 16)
        {
            continue;
        }
        if (strncmp(field_name, "BAND", 4) != 0 || strncmp(&field_name[5], "_IRRADIANCE", 9) != 0)
        {
            continue;
        }
        if (isdigit(field_name[4]) && field_name[4] != '0' && field_name[4] != '9')
        {
            break;
        }
    }

    if (i == num_fields)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, NULL);
        return -1;
    }

    return 0;
}

static int verify_s5p_l1b_ra(coda_product *product, const char *band_name)
{
    coda_cursor cursor;

    if (coda_cursor_set_product(&cursor, product) != 0)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&cursor, band_name) != 0)
    {
        harp_set_error(HARP_ERROR_UNSUPPORTED_PRODUCT, NULL);
        return -1;
    }

    return 0;
}

static int verify_s5p_l1b_ra_bd1(const harp_ingestion_module *module, coda_product *product)
{
    (void)module;

    return verify_s5p_l1b_ra(product, "BAND1_RADIANCE");
}

static int verify_s5p_l1b_ra_bd2(const harp_ingestion_module *module, coda_product *product)
{
    (void)module;

    return verify_s5p_l1b_ra(product, "BAND2_RADIANCE");
}

static int verify_s5p_l1b_ra_bd3(const harp_ingestion_module *module, coda_product *product)
{
    (void)module;

    return verify_s5p_l1b_ra(product, "BAND3_RADIANCE");
}

static int verify_s5p_l1b_ra_bd4(const harp_ingestion_module *module, coda_product *product)
{
    (void)module;

    return verify_s5p_l1b_ra(product, "BAND4_RADIANCE");
}

static int verify_s5p_l1b_ra_bd5(const harp_ingestion_module *module, coda_product *product)
{
    (void)module;

    return verify_s5p_l1b_ra(product, "BAND5_RADIANCE");
}

static int verify_s5p_l1b_ra_bd6(const harp_ingestion_module *module, coda_product *product)
{
    (void)module;

    return verify_s5p_l1b_ra(product, "BAND6_RADIANCE");
}

static int verify_s5p_l1b_ra_bd7(const harp_ingestion_module *module, coda_product *product)
{
    (void)module;

    return verify_s5p_l1b_ra(product, "BAND7_RADIANCE");
}

static int verify_s5p_l1b_ra_bd8(const harp_ingestion_module *module, coda_product *product)
{
    (void)module;

    return verify_s5p_l1b_ra(product, "BAND8_RADIANCE");
}

static void register_irradiance_product_variables(harp_product_definition *product_definition)
{
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_spectral };
    const char *description;


    description = "zero-based index of the pixel within the scanline";
    harp_ingestion_register_variable_sample_read(product_definition, "scanline_pixel_index", harp_type_int16, 1,
                                                 dimension_type, NULL, description, NULL, NULL,
                                                 read_scanline_pixel_index);

    description = "start time of the measurement";
    harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double, 1, dimension_type,
                                               NULL, description, "seconds since 2000-01-01", NULL, read_datetime);

    /* Geographic. */
    description = "latitude of the sub-instrument point (WGS84)";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "instrument_latitude",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     description, "degree_north", NULL,
                                                                     read_instrument_latitude);
    harp_variable_definition_set_valid_range_float(variable_definition, -90.0f, 90.0f);

    description = "longitude of the sub-instrument point (WGS84)";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "instrument_longitude",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     description, "degree_east", NULL,
                                                                     read_instrument_longitude);
    harp_variable_definition_set_valid_range_float(variable_definition, -180.0f, 180.0f);

    description = "altitude of the instrument (WGS84)";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "instrument_altitude",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     description, "m", NULL, read_instrument_altitude);

    /* Irradiance. */
    description = "nominal wavelength";
    harp_ingestion_register_variable_sample_read(product_definition, "wavelength", harp_type_float, 2, dimension_type,
                                                 NULL, description, "nm", NULL, read_wavelength);

    description = "spectral photon irradiance";
    harp_ingestion_register_variable_sample_read(product_definition, "photon_irradiance", harp_type_float, 2,
                                                 dimension_type, NULL, description, "mol/(s.m^2.nm.sr)", NULL,
                                                 read_photon_irradiance);
}

static void register_radiance_product_variables(harp_product_definition *product_definition)
{
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_spectral };
    harp_dimension_type bounds_dimension_type[2] = { harp_dimension_time, harp_dimension_independent };
    long bounds_dimension[2] = { -1, 4 };
    const char *description;

    description = "zero-based index of the pixel within the scanline";
    harp_ingestion_register_variable_sample_read(product_definition, "scanline_pixel_index", harp_type_int16, 1,
                                                 dimension_type, NULL, description, NULL, NULL,
                                                 read_scanline_pixel_index);

    description = "start time of the measurement";
    harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double, 1, dimension_type,
                                               NULL, description, "seconds since 2000-01-01", NULL, read_datetime);

    /* Geographic. */
    description = "latitude of the ground pixel center (WGS84)";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_float, 1,
                                                                     dimension_type, NULL, description, "degree_north",
                                                                     NULL, read_latitude);
    harp_variable_definition_set_valid_range_float(variable_definition, -90.0f, 90.0f);

    description = "longitude of the ground pixel center (WGS84)";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_float,
                                                                     1, dimension_type, NULL, description,
                                                                     "degree_east", NULL, read_longitude);
    harp_variable_definition_set_valid_range_float(variable_definition, -180.0f, 180.0f);

    description = "latitudes of the ground pixel corners (WGS84)";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "latitude_bounds",
                                                                     harp_type_float, 2, bounds_dimension_type,
                                                                     bounds_dimension, description, "degree_north",
                                                                     NULL, read_latitude_bounds);
    harp_variable_definition_set_valid_range_float(variable_definition, -90.0f, 90.0f);

    description = "longitudes of the ground pixel corners (WGS84)";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "longitude_bounds",
                                                                     harp_type_float, 2, bounds_dimension_type,
                                                                     bounds_dimension, description, "degree_east",
                                                                     NULL, read_longitude_bounds);
    harp_variable_definition_set_valid_range_float(variable_definition, -180.0f, 180.0f);

    description = "latitude of the sub-instrument point (WGS84)";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "instrument_latitude",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     description, "degree_north", NULL,
                                                                     read_instrument_latitude);
    harp_variable_definition_set_valid_range_float(variable_definition, -90.0f, 90.0f);

    description = "longitude of the sub-instrument point (WGS84)";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "instrument_longitude",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     description, "degree_east", NULL,
                                                                     read_instrument_longitude);
    harp_variable_definition_set_valid_range_float(variable_definition, -180.0f, 180.0f);

    description = "altitude of the instrument (WGS84)";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "instrument_altitude",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     description, "m", NULL, read_instrument_altitude);

    /* Angles. */
    description = "zenith angle of the Sun at the ground pixel location (WGS84)";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "solar_zenith_angle",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     description, "degree", NULL,
                                                                     read_solar_zenith_angle);
    harp_variable_definition_set_valid_range_float(variable_definition, 0.0f, 180.0f);

    description = "azimuth angle of the Sun at the ground pixel location (WGS84), measured East-of-North";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "solar_azimuth_angle",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     description, "degree", NULL,
                                                                     read_solar_azimuth_angle);
    harp_variable_definition_set_valid_range_float(variable_definition, 0.0f, 360.0f);

    description = "zenith angle of the instrument at the ground pixel location (WGS84)";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "viewing_zenith_angle",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     description, "degree", NULL,
                                                                     read_viewing_zenith_angle);
    harp_variable_definition_set_valid_range_float(variable_definition, 0.0f, 180.0f);

    description = "azimuth angle of the instrument at the ground pixel location (WGS84), measured East-of-North";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "viewing_azimuth_angle",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     description, "degree", NULL,
                                                                     read_viewing_azimuth_angle);
    harp_variable_definition_set_valid_range_float(variable_definition, 0.0f, 360.0f);

    /* Radiance. */
    description = "nominal wavelength";
    harp_ingestion_register_variable_sample_read(product_definition, "wavelength", harp_type_float, 2, dimension_type,
                                                 NULL, description, "nm", NULL, read_wavelength);

    description = "spectral photon radiance";
    harp_ingestion_register_variable_sample_read(product_definition, "photon_radiance", harp_type_float, 2,
                                                 dimension_type, NULL, description, "mol/(s.m^2.nm.sr)", NULL,
                                                 read_photon_radiance);
}

int harp_ingestion_module_s5p_l1b_init(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    const char *band_option_values[] = { "1", "2", "3", "4", "5", "6", "7", "8" };
    const char *description;

    /* S5P_L1B_IR products. */
    description = "Sentinel 5P L1b irradiance spectra";
    module = harp_ingestion_register_module_coda("S5P_L1B_IR", NULL, NULL, description, verify_s5p_l1b_ir,
                                                 ingestion_init_s5p_l1b_ir, ingestion_done);
    harp_ingestion_register_option(module, "band", "spectral band to ingest", 8, band_option_values);

    product_definition =
        harp_ingestion_register_product(module, "S5P_L1B_IR_UVN_BD1", "irradiance spectra (band 1, UVN module)",
                                        read_dimensions);
    harp_product_definition_add_mapping(product_definition, NULL, "band=1");
    register_irradiance_product_variables(product_definition);

    product_definition =
        harp_ingestion_register_product(module, "S5P_L1B_IR_UVN_BD2", "irradiance spectra (band 2, UVN module)",
                                        read_dimensions);
    harp_product_definition_add_mapping(product_definition, NULL, "band=2");
    register_irradiance_product_variables(product_definition);

    product_definition =
        harp_ingestion_register_product(module, "S5P_L1B_IR_UVN_BD3", "irradiance spectra (band 3, UVN module)",
                                        read_dimensions);
    harp_product_definition_add_mapping(product_definition, NULL, "band=3");
    register_irradiance_product_variables(product_definition);

    product_definition =
        harp_ingestion_register_product(module, "S5P_L1B_IR_UVN_BD4", "irradiance spectra (band 4, UVN module)",
                                        read_dimensions);
    harp_product_definition_add_mapping(product_definition, NULL, "band=4");
    register_irradiance_product_variables(product_definition);

    product_definition =
        harp_ingestion_register_product(module, "S5P_L1B_IR_UVN_BD5", "irradiance spectra (band 5, UVN module)",
                                        read_dimensions);
    harp_product_definition_add_mapping(product_definition, NULL, "band=5");
    register_irradiance_product_variables(product_definition);

    product_definition =
        harp_ingestion_register_product(module, "S5P_L1B_IR_UVN_BD6", "irradiance spectra (band 6, UVN module)",
                                        read_dimensions);
    harp_product_definition_add_mapping(product_definition, NULL, "band=6");
    register_irradiance_product_variables(product_definition);

    product_definition =
        harp_ingestion_register_product(module, "S5P_L1B_IR_SIR_BD7", "irradiance spectra (band 7, SWIR module)",
                                        read_dimensions);
    harp_product_definition_add_mapping(product_definition, NULL, "band=7");
    register_irradiance_product_variables(product_definition);

    product_definition =
        harp_ingestion_register_product(module, "S5P_L1B_IR_SIR_BD8", "irradiance spectra (band 8, SWIR module)",
                                        read_dimensions);
    harp_product_definition_add_mapping(product_definition, NULL, "band=8");
    register_irradiance_product_variables(product_definition);

    /* S5P_L1B_RA products. */
    description = "Sentinel 5P L1b photon radiance spectra (band 1, UV detector)";
    module = harp_ingestion_register_module_coda("S5P_L1B_RA_BD1", NULL, NULL, description, verify_s5p_l1b_ra_bd1,
                                                 ingestion_init_s5p_l1b_ra, ingestion_done);

    product_definition = harp_ingestion_register_product(module, "S5P_L1B_RA_BD1", NULL, read_dimensions);
    register_radiance_product_variables(product_definition);

    description = "Sentinel 5P L1b photon radiance spectra (band 2, UV detector)";
    module = harp_ingestion_register_module_coda("S5P_L1B_RA_BD2", NULL, NULL, description, verify_s5p_l1b_ra_bd2,
                                                 ingestion_init_s5p_l1b_ra, ingestion_done);

    product_definition = harp_ingestion_register_product(module, "S5P_L1B_RA_BD2", NULL, read_dimensions);
    register_radiance_product_variables(product_definition);

    description = "Sentinel 5P L1b photon radiance spectra (band 3, UVIS detector)";
    module = harp_ingestion_register_module_coda("S5P_L1B_RA_BD3", NULL, NULL, description, verify_s5p_l1b_ra_bd3,
                                                 ingestion_init_s5p_l1b_ra, ingestion_done);

    product_definition = harp_ingestion_register_product(module, "S5P_L1B_RA_BD3", NULL, read_dimensions);
    register_radiance_product_variables(product_definition);

    description = "Sentinel 5P L1b photon radiance spectra (band 4, UVIS detector)";
    module = harp_ingestion_register_module_coda("S5P_L1B_RA_BD4", NULL, NULL, description, verify_s5p_l1b_ra_bd4,
                                                 ingestion_init_s5p_l1b_ra, ingestion_done);

    product_definition = harp_ingestion_register_product(module, "S5P_L1B_RA_BD4", NULL, read_dimensions);
    register_radiance_product_variables(product_definition);

    description = "Sentinel 5P L1b photon radiance spectra (band 5, NIR detector)";
    module = harp_ingestion_register_module_coda("S5P_L1B_RA_BD5", NULL, NULL, description, verify_s5p_l1b_ra_bd5,
                                                 ingestion_init_s5p_l1b_ra, ingestion_done);

    product_definition = harp_ingestion_register_product(module, "S5P_L1B_RA_BD5", NULL, read_dimensions);
    register_radiance_product_variables(product_definition);

    description = "Sentinel 5P L1b photon radiance spectra (band 6, NIR detector)";
    module = harp_ingestion_register_module_coda("S5P_L1B_RA_BD6", NULL, NULL, description, verify_s5p_l1b_ra_bd6,
                                                 ingestion_init_s5p_l1b_ra, ingestion_done);

    product_definition = harp_ingestion_register_product(module, "S5P_L1B_RA_BD6", NULL, read_dimensions);
    register_radiance_product_variables(product_definition);

    description = "Sentinel 5P L1b photon radiance spectra (band 7, SWIR detector)";
    module = harp_ingestion_register_module_coda("S5P_L1B_RA_BD7", NULL, NULL, description, verify_s5p_l1b_ra_bd7,
                                                 ingestion_init_s5p_l1b_ra, ingestion_done);

    product_definition = harp_ingestion_register_product(module, "S5P_L1B_RA_BD7", NULL, read_dimensions);
    register_radiance_product_variables(product_definition);

    description = "Sentinel 5P L1b photon radiance spectra (band 8, SWIR detector)";
    module = harp_ingestion_register_module_coda("S5P_L1B_RA_BD8", NULL, NULL, description, verify_s5p_l1b_ra_bd8,
                                                 ingestion_init_s5p_l1b_ra, ingestion_done);

    product_definition = harp_ingestion_register_product(module, "S5P_L1B_RA_BD8", NULL, read_dimensions);
    register_radiance_product_variables(product_definition);

    return 0;
}
