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
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Default fill value taken from "Input/output data specification for the TROPOMI L-1b data processor",
 * S5P-KNMI-L01B-0012-SD.
 */
#define DEFAULT_FILL_VALUE_INT (-2147483647)

/* Macro to determine the number of elements in a one dimensional C array. */
#define ARRAY_SIZE(X) (sizeof((X))/sizeof((X)[0]))

/* Maximum length of a path string in generated mapping descriptions. */
#define MAX_PATH_LENGTH 256

typedef struct ingest_info_struct
{
    coda_product *product;
    int band;

    long num_scanlines;
    long num_pixels;
    long num_channels;

    coda_cursor sensor_mode_cursor;
    coda_cursor geo_data_cursor;
    coda_cursor observation_cursor;
    coda_cursor instrument_cursor;

    coda_cursor wavelength_cursor;
    harp_scalar wavelength_fill_value;
    coda_cursor observable_cursor;
    harp_scalar observable_fill_value;
} ingest_info;

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

static int get_product_group_availability(coda_product *product, const char *group_name, int *group_available)
{
    coda_type *root_type;
    long num_fields;
    long i;

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

        if (strcmp(field_name, group_name) == 0)
        {
            *group_available = 1;
            return 0;
        }
    }

    *group_available = 0;

    return 0;
}

static int read_dataset(coda_cursor cursor, const char *dataset_name, long num_elements, harp_array data)
{
    long coda_num_elements;
    harp_scalar fill_value;

    if (coda_cursor_goto_record_field_by_name(&cursor, dataset_name) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &coda_num_elements) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_num_elements != num_elements)
    {
        harp_set_error(HARP_ERROR_INGESTION, "dataset has %ld elements; expected %ld", coda_num_elements, num_elements);
        harp_add_coda_cursor_path_to_error_message(&cursor);
        return -1;
    }
    if (coda_cursor_read_float_array(&cursor, data.float_data, coda_array_ordering_c) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, "@FillValue[0]") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_float(&cursor, &fill_value.float_data) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    /* Replace values equal to the _FillValue variable attribute by NaN. */
    harp_array_replace_fill_value(harp_type_float, num_elements, data, fill_value);

    return 0;
}

static int read_partial_dataset(const coda_cursor *cursor, long offset, long length, harp_array data,
                                harp_scalar fill_value)
{
    if (coda_cursor_read_float_partial_array(cursor, offset, length, data.float_data) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    /* Replace values equal to the _FillValue variable attribute by NaN. */
    harp_array_replace_fill_value(harp_type_float, length, data, fill_value);

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

    info->geo_data_cursor = cursor;
    if (coda_cursor_goto_record_field_by_name(&info->geo_data_cursor, "GEODATA") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    info->observation_cursor = cursor;
    if (coda_cursor_goto(&info->observation_cursor, "OBSERVATIONS") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    info->instrument_cursor = cursor;
    if (coda_cursor_goto(&info->instrument_cursor, "INSTRUMENT") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    return 0;
}

static int init_dimensions(ingest_info *info, coda_cursor cursor, const char *name)
{
    long coda_dim[CODA_MAX_NUM_DIMS];
    int num_coda_dims;

    if (coda_cursor_goto_record_field_by_name(&cursor, name) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_array_dim(&cursor, &num_coda_dims, coda_dim) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (num_coda_dims != 4)
    {
        harp_set_error(HARP_ERROR_INGESTION, "dataset has %d dimensions; expected 4", num_coda_dims);
        harp_add_coda_cursor_path_to_error_message(&cursor);
        return -1;
    }
    if (coda_dim[0] != 1)
    {
        harp_set_error(HARP_ERROR_INGESTION, "outermost dimension of dataset has length %ld; expected 1", coda_dim[0]);
        harp_add_coda_cursor_path_to_error_message(&cursor);
        return -1;
    }

    info->num_scanlines = coda_dim[1];
    info->num_pixels = coda_dim[2];
    info->num_channels = coda_dim[3];

    return 0;
}

static int init_dataset(coda_cursor cursor, const char *name, long num_elements, coda_cursor *new_cursor,
                        harp_scalar *fill_value)
{
    long coda_num_elements;

    if (coda_cursor_goto_record_field_by_name(&cursor, name) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &coda_num_elements) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_num_elements != num_elements)
    {
        harp_set_error(HARP_ERROR_INGESTION, "dataset has %ld elements; expected %ld", coda_num_elements, num_elements);
        harp_add_coda_cursor_path_to_error_message(&cursor);
        return -1;
    }
    if (coda_cursor_goto(&cursor, "@FillValue[0]") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_float(&cursor, &fill_value->float_data) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    coda_cursor_goto_parent(&cursor);
    coda_cursor_goto_parent(&cursor);
    coda_cursor_goto_parent(&cursor);

    *new_cursor = cursor;

    return 0;
}

static int parse_option_band(ingest_info *info, const harp_ingestion_options *options)
{
    const char *value;

    if (harp_ingestion_options_get_option(options, "band", &value) != 0)
    {
        harp_set_error(HARP_ERROR_INVALID_INGESTION_OPTION, "ingestion option 'band' not specified");
        return -1;
    }

    info->band = *value - '0';

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
    char product_group_name[17];
    int band_available;

    info = malloc(sizeof(ingest_info));
    if (info == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(ingest_info), __FILE__, __LINE__);
        return -1;
    }
    info->product = product;
    info->band = -1;

    if (parse_option_band(info, options) != 0)
    {
        ingestion_done(info);
        return -1;
    }

    snprintf(product_group_name, ARRAY_SIZE(product_group_name), "BAND%d_IRRADIANCE", info->band);
    if (get_product_group_availability(info->product, product_group_name, &band_available) != 0)
    {
        ingestion_done(info);
        return -1;
    }
    if (!band_available)
    {
        harp_set_error(HARP_ERROR_INGESTION, "no data for band '%d'", info->band);
        ingestion_done(info);
        return -1;
    }

    if (init_cursors(info, product_group_name) != 0)
    {
        ingestion_done(info);
        return -1;
    }
    if (init_dimensions(info, info->observation_cursor, "irradiance") != 0)
    {
        ingestion_done(info);
        return -1;
    }

    /* Initialize cursors and fill values for datasets which will be read using partial reads. */
    if (init_dataset
        (info->instrument_cursor, "calibrated_wavelength", info->num_pixels * info->num_channels,
         &info->wavelength_cursor, &info->wavelength_fill_value) != 0)
    {
        ingestion_done(info);
        return -1;
    }

    if (init_dataset
        (info->observation_cursor, "irradiance", info->num_scanlines * info->num_pixels * info->num_channels,
         &info->observable_cursor, &info->observable_fill_value) != 0)
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

    (void)options;

    info = malloc(sizeof(ingest_info));
    if (info == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(ingest_info), __FILE__, __LINE__);
        return -1;
    }
    info->product = product;
    info->band = -1;

    if (init_cursors(info, NULL) != 0)
    {
        ingestion_done(info);
        return -1;
    }
    if (init_dimensions(info, info->observation_cursor, "radiance") != 0)
    {
        ingestion_done(info);
        return -1;
    }

    /* Initialize cursors and fill values for datasets which will be read using partial reads. */
    if (init_dataset
        (info->instrument_cursor, "nominal_wavelength", info->num_pixels * info->num_channels, &info->wavelength_cursor,
         &info->wavelength_fill_value) != 0)
    {
        ingestion_done(info);
        return -1;
    }

    if (init_dataset
        (info->observation_cursor, "radiance", info->num_scanlines * info->num_pixels * info->num_channels,
         &info->observable_cursor, &info->observable_fill_value) != 0)
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
    ingest_info *info = (ingest_info *)user_data;

    dimension[harp_dimension_time] = info->num_scanlines * info->num_pixels;
    dimension[harp_dimension_spectral] = info->num_channels;

    return 0;
}

static int read_scanline_pixel_index(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    index = index - (index / info->num_pixels) * info->num_pixels;
    *data.int16_data = (int16_t)index;

    return 0;
}

static int read_datetime(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    coda_cursor cursor;
    harp_scalar fill_value;
    double epoch;
    double time_reference;
    long coda_num_elements;
    long i;

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

    /* Convert the epoch 2010-01-01 00:00:00 UTC to seconds since 2000-01-01 00:00:00 TAI. */
    if (coda_time_parts_to_double_utc(2010, 1, 1, 0, 0, 0, 0, &epoch) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    /* Read reference time in seconds since 2010-01-01 00:00:00 UTC (probably wrong, see above). */
    cursor = info->observation_cursor;
    if (coda_cursor_goto_record_field_by_name(&cursor, "time") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &coda_num_elements) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_num_elements != 1)
    {
        harp_set_error(HARP_ERROR_INGESTION, "dataset has %ld elements; expected 1", coda_num_elements);
        harp_add_coda_cursor_path_to_error_message(&cursor);
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
    if (time_reference == DEFAULT_FILL_VALUE_INT)
    {
        time_reference = coda_NaN();
    }

    /* Read difference in milliseconds (ms) between the time reference and the start of the observation. */
    cursor = info->observation_cursor;
    if (coda_cursor_goto_record_field_by_name(&cursor, "delta_time") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_num_elements(&cursor, &coda_num_elements) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_num_elements != info->num_scanlines)
    {
        harp_set_error(HARP_ERROR_INGESTION, "dataset has %ld elements; expected %ld", coda_num_elements,
                       info->num_scanlines);
        harp_add_coda_cursor_path_to_error_message(&cursor);
        return -1;
    }
    if (coda_cursor_read_double_array(&cursor, data.double_data, coda_array_ordering_c) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto(&cursor, "@FillValue[0]") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_read_double(&cursor, &fill_value.double_data) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    /* Replace values equal to the _FillValue variable attribute by NaN. */
    harp_array_replace_fill_value(harp_type_double, info->num_scanlines, data, fill_value);

    /* Convert observation start time to seconds since 2000-01-01 00:00:00 TAI. */
    for (i = 0; i < info->num_scanlines; i++)
    {
        data.double_data[i] = epoch + time_reference + data.double_data[i] / 1e3;
    }

    /* Broadcast the result along the pixel dimension. */
    broadcast_array_double(info->num_scanlines, info->num_pixels, data.double_data);

    return 0;
}

static int read_longitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->geo_data_cursor, "longitude", info->num_scanlines * info->num_pixels, data);
}

static int read_longitude_bounds(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->geo_data_cursor, "longitude_bounds", info->num_scanlines * info->num_pixels * 4, data);
}

static int read_latitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->geo_data_cursor, "latitude", info->num_scanlines * info->num_pixels, data);
}

static int read_latitude_bounds(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->geo_data_cursor, "latitude_bounds", info->num_scanlines * info->num_pixels * 4, data);
}

static int read_instrument_longitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (read_dataset(info->geo_data_cursor, "satellite_longitude", info->num_scanlines, data) != 0)
    {
        return -1;
    }

    broadcast_array_float(info->num_scanlines, info->num_pixels, data.float_data);

    return 0;
}

static int read_instrument_latitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (read_dataset(info->geo_data_cursor, "satellite_latitude", info->num_scanlines, data) != 0)
    {
        return -1;
    }

    broadcast_array_float(info->num_scanlines, info->num_pixels, data.float_data);

    return 0;
}

static int read_instrument_altitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    if (read_dataset(info->geo_data_cursor, "satellite_altitude", info->num_scanlines, data) != 0)
    {
        return -1;
    }

    broadcast_array_float(info->num_scanlines, info->num_pixels, data.float_data);

    return 0;
}

static int read_solar_azimuth_angle(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->geo_data_cursor, "solar_azimuth_angle", info->num_scanlines * info->num_pixels, data);
}

static int read_solar_zenith_angle(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->geo_data_cursor, "solar_zenith_angle", info->num_scanlines * info->num_pixels, data);
}

static int read_viewing_azimuth_angle(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->geo_data_cursor, "viewing_azimuth_angle", info->num_scanlines * info->num_pixels, data);
}

static int read_viewing_zenith_angle(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_dataset(info->geo_data_cursor, "viewing_zenith_angle", info->num_scanlines * info->num_pixels, data);
}

static int read_wavelength(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long offset;

    /* Compute the offset into an array of dimensions #pixels x #channels (either calibrated_wavelength or
     * nominal_wavelength) using a pixel index derived from the provided index on the time dimension (of length
     * #scanlines x #pixels).
     */
    offset = (index - (index / info->num_pixels) * info->num_pixels) * info->num_channels;

    return read_partial_dataset(&info->wavelength_cursor, offset, info->num_channels, data,
                                info->wavelength_fill_value);
}

static int read_observable(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_partial_dataset(&info->observable_cursor, index * info->num_channels, info->num_channels, data,
                                info->observable_fill_value);
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

static void register_irradiance_product_variables(harp_product_definition *product_definition,
                                                  const char *product_group_name)
{
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_spectral };
    const char *description;
    char path[MAX_PATH_LENGTH];

    description = "zero-based index of the pixel within the scanline";
    variable_definition =
        harp_ingestion_register_variable_sample_read(product_definition, "scanline_pixel_index", harp_type_int16, 1,
                                                     dimension_type, NULL, description, NULL, NULL,
                                                     read_scanline_pixel_index);
    description =
        "the scanline and pixel dimensions are collapsed into a temporal dimension; the index of the pixel within the "
        "scanline is computed as the index on the temporal dimension modulo the number of scanlines";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, NULL, description);

    description = "time of the measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double, 1, dimension_type,
                                                   NULL, description, "seconds since 2000-01-01", NULL, read_datetime);
    snprintf(path, MAX_PATH_LENGTH, "/%s/STANDARD_MODE/OBSERVATIONS/time, /%s/STANDARD_MODE/OBSERVATIONS/delta_time[]",
             product_group_name, product_group_name);
    description =
        "time converted from milliseconds since a reference time (given as seconds since 2010-01-01 UTC) to seconds "
        "since 2000-01-01 TAI; the time associated with a scanline is repeated for each pixel in the scanline";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* Irradiance. */
    description = "calibrated wavelength";
    variable_definition =
        harp_ingestion_register_variable_sample_read(product_definition, "wavelength", harp_type_float, 2,
                                                     dimension_type, NULL, description, "nm", NULL, read_wavelength);
    snprintf(path, MAX_PATH_LENGTH, "/%s/STANDARD_MODE/INSTRUMENT/calibrated_wavelength[]", product_group_name);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "spectral photon irradiance";
    variable_definition =
        harp_ingestion_register_variable_sample_read(product_definition, "photon_irradiance", harp_type_float, 2,
                                                     dimension_type, NULL, description, "mol/(s.m^2.nm)", NULL,
                                                     read_observable);
    snprintf(path, MAX_PATH_LENGTH, "/%s/STANDARD_MODE/OBSERVATIONS/irradiance[]", product_group_name);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

static void register_radiance_product_variables(harp_product_definition *product_definition,
                                                const char *product_group_name)
{
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_spectral };
    harp_dimension_type bounds_dimension_type[2] = { harp_dimension_time, harp_dimension_independent };
    long bounds_dimension[2] = { -1, 4 };
    const char *description;
    char path[MAX_PATH_LENGTH];

    description = "zero-based index of the pixel within the scanline";
    variable_definition =
        harp_ingestion_register_variable_sample_read(product_definition, "scanline_pixel_index", harp_type_int16, 1,
                                                     dimension_type, NULL, description, NULL, NULL,
                                                     read_scanline_pixel_index);
    description =
        "the scanline and ground pixel dimensions are collapsed into a single temporal dimension; the index "
        "of the pixel within the scanline is computed as the index on this temporal dimension modulo the "
        "number of scanlines";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, NULL, description);

    description = "time of the measurement";
    variable_definition =
        harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double, 1, dimension_type,
                                                   NULL, description, "seconds since 2000-01-01", NULL, read_datetime);
    snprintf(path, MAX_PATH_LENGTH, "/%s/STANDARD_MODE/OBSERVATIONS/time, /%s/STANDARD_MODE/OBSERVATIONS/delta_time[]",
             product_group_name, product_group_name);
    description =
        "time converted from milliseconds since a reference time (given as seconds since 2010-01-01 UTC) to seconds "
        "since 2000-01-01 TAI; the time associated with a scanline is repeated for each pixel in the scanline";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* Geographic. */
    description = "latitude of the ground pixel center (WGS84)";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_float, 1,
                                                                     dimension_type, NULL, description, "degree_north",
                                                                     NULL, read_latitude);
    harp_variable_definition_set_valid_range_float(variable_definition, -90.0f, 90.0f);
    snprintf(path, MAX_PATH_LENGTH, "/%s/STANDARD_MODE/GEODATA/latitude[]", product_group_name);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "longitude of the ground pixel center (WGS84)";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_float,
                                                                     1, dimension_type, NULL, description,
                                                                     "degree_east", NULL, read_longitude);
    harp_variable_definition_set_valid_range_float(variable_definition, -180.0f, 180.0f);
    snprintf(path, MAX_PATH_LENGTH, "/%s/STANDARD_MODE/GEODATA/longitude[]", product_group_name);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "latitudes of the ground pixel corners (WGS84)";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "latitude_bounds",
                                                                     harp_type_float, 2, bounds_dimension_type,
                                                                     bounds_dimension, description, "degree_north",
                                                                     NULL, read_latitude_bounds);
    harp_variable_definition_set_valid_range_float(variable_definition, -90.0f, 90.0f);
    snprintf(path, MAX_PATH_LENGTH, "/%s/STANDARD_MODE/GEODATA/latitude_bounds[]", product_group_name);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "longitudes of the ground pixel corners (WGS84)";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "longitude_bounds",
                                                                     harp_type_float, 2, bounds_dimension_type,
                                                                     bounds_dimension, description, "degree_east",
                                                                     NULL, read_longitude_bounds);
    harp_variable_definition_set_valid_range_float(variable_definition, -180.0f, 180.0f);
    snprintf(path, MAX_PATH_LENGTH, "/%s/STANDARD_MODE/GEODATA/longitude_bounds[]", product_group_name);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "latitude of the sub-instrument point (WGS84)";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "instrument_latitude",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     description, "degree_north", NULL,
                                                                     read_instrument_latitude);
    harp_variable_definition_set_valid_range_float(variable_definition, -90.0f, 90.0f);
    snprintf(path, MAX_PATH_LENGTH, "/%s/STANDARD_MODE/GEODATA/satellite_latitude[]", product_group_name);
    description = "the satellite latitude associated with a scanline is repeated for each pixel in the scanline";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    description = "longitude of the sub-instrument point (WGS84)";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "instrument_longitude",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     description, "degree_east", NULL,
                                                                     read_instrument_longitude);
    harp_variable_definition_set_valid_range_float(variable_definition, -180.0f, 180.0f);
    snprintf(path, MAX_PATH_LENGTH, "/%s/STANDARD_MODE/GEODATA/satellite_longitude[]", product_group_name);
    description = "the satellite longitude associated with a scanline is repeated for each pixel in the scanline";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    description = "altitude of the instrument (WGS84)";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "instrument_altitude",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     description, "m", NULL, read_instrument_altitude);
    snprintf(path, MAX_PATH_LENGTH, "/%s/STANDARD_MODE/GEODATA/satellite_altitude[]", product_group_name);
    description = "the satellite altitude associated with a scanline is repeated for each pixel in the scanline";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* Angles. */
    description = "zenith angle of the Sun at the ground pixel location (WGS84)";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "solar_zenith_angle",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     description, "degree", NULL,
                                                                     read_solar_zenith_angle);
    harp_variable_definition_set_valid_range_float(variable_definition, 0.0f, 180.0f);
    snprintf(path, MAX_PATH_LENGTH, "/%s/STANDARD_MODE/GEODATA/solar_zenith_angle[]", product_group_name);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "azimuth angle of the Sun at the ground pixel location (WGS84), measured East-of-North";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "solar_azimuth_angle",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     description, "degree", NULL,
                                                                     read_solar_azimuth_angle);
    harp_variable_definition_set_valid_range_float(variable_definition, 0.0f, 360.0f);
    snprintf(path, MAX_PATH_LENGTH, "/%s/STANDARD_MODE/GEODATA/solar_azimuth_angle[]", product_group_name);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "zenith angle of the instrument at the ground pixel location (WGS84)";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "viewing_zenith_angle",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     description, "degree", NULL,
                                                                     read_viewing_zenith_angle);
    harp_variable_definition_set_valid_range_float(variable_definition, 0.0f, 180.0f);
    snprintf(path, MAX_PATH_LENGTH, "/%s/STANDARD_MODE/GEODATA/viewing_zenith_angle[]", product_group_name);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "azimuth angle of the instrument at the ground pixel location (WGS84), measured East-of-North";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "viewing_azimuth_angle",
                                                                     harp_type_float, 1, dimension_type, NULL,
                                                                     description, "degree", NULL,
                                                                     read_viewing_azimuth_angle);
    harp_variable_definition_set_valid_range_float(variable_definition, 0.0f, 360.0f);
    snprintf(path, MAX_PATH_LENGTH, "/%s/STANDARD_MODE/GEODATA/viewing_azimuth_angle[]", product_group_name);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* Radiance. */
    description = "nominal wavelength";
    variable_definition =
        harp_ingestion_register_variable_sample_read(product_definition, "wavelength", harp_type_float, 2,
                                                     dimension_type, NULL, description, "nm", NULL, read_wavelength);
    snprintf(path, MAX_PATH_LENGTH, "/%s/STANDARD_MODE/INSTRUMENT/nominal_wavelength[]", product_group_name);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    description = "spectral photon radiance";
    variable_definition =
        harp_ingestion_register_variable_sample_read(product_definition, "photon_radiance", harp_type_float, 2,
                                                     dimension_type, NULL, description, "mol/(s.m^2.nm.sr)", NULL,
                                                     read_observable);
    snprintf(path, MAX_PATH_LENGTH, "/%s/STANDARD_MODE/OBSERVATIONS/radiance[]", product_group_name);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

int harp_ingestion_module_s5p_l1b_init(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    const char *band_option_values[] = { "1", "2", "3", "4", "5", "6", "7", "8" };
    const char *description;

    /* S5P_L1B_IR products. */
    description = "Sentinel-5P L1b irradiance spectra";
    module =
        harp_ingestion_register_module_coda("S5P_L1B_IR", "Sentinel-5P", NULL, NULL, description, verify_s5p_l1b_ir,
                                            ingestion_init_s5p_l1b_ir, ingestion_done);
    harp_ingestion_register_option(module, "band", "spectral band to ingest", 8, band_option_values);

    product_definition =
        harp_ingestion_register_product(module, "S5P_L1B_IR_UVN_BD1", "irradiance spectra (band 1, UVN module)",
                                        read_dimensions);
    harp_product_definition_add_mapping(product_definition, NULL, "band=1");
    register_irradiance_product_variables(product_definition, "BAND1_IRRADIANCE");

    product_definition =
        harp_ingestion_register_product(module, "S5P_L1B_IR_UVN_BD2", "irradiance spectra (band 2, UVN module)",
                                        read_dimensions);
    harp_product_definition_add_mapping(product_definition, NULL, "band=2");
    register_irradiance_product_variables(product_definition, "BAND2_IRRADIANCE");

    product_definition =
        harp_ingestion_register_product(module, "S5P_L1B_IR_UVN_BD3", "irradiance spectra (band 3, UVN module)",
                                        read_dimensions);
    harp_product_definition_add_mapping(product_definition, NULL, "band=3");
    register_irradiance_product_variables(product_definition, "BAND3_IRRADIANCE");

    product_definition =
        harp_ingestion_register_product(module, "S5P_L1B_IR_UVN_BD4", "irradiance spectra (band 4, UVN module)",
                                        read_dimensions);
    harp_product_definition_add_mapping(product_definition, NULL, "band=4");
    register_irradiance_product_variables(product_definition, "BAND4_IRRADIANCE");

    product_definition =
        harp_ingestion_register_product(module, "S5P_L1B_IR_UVN_BD5", "irradiance spectra (band 5, UVN module)",
                                        read_dimensions);
    harp_product_definition_add_mapping(product_definition, NULL, "band=5");
    register_irradiance_product_variables(product_definition, "BAND5_IRRADIANCE");

    product_definition =
        harp_ingestion_register_product(module, "S5P_L1B_IR_UVN_BD6", "irradiance spectra (band 6, UVN module)",
                                        read_dimensions);
    harp_product_definition_add_mapping(product_definition, NULL, "band=6");
    register_irradiance_product_variables(product_definition, "BAND6_IRRADIANCE");

    product_definition =
        harp_ingestion_register_product(module, "S5P_L1B_IR_SIR_BD7", "irradiance spectra (band 7, SWIR module)",
                                        read_dimensions);
    harp_product_definition_add_mapping(product_definition, NULL, "band=7");
    register_irradiance_product_variables(product_definition, "BAND7_IRRADIANCE");

    product_definition =
        harp_ingestion_register_product(module, "S5P_L1B_IR_SIR_BD8", "irradiance spectra (band 8, SWIR module)",
                                        read_dimensions);
    harp_product_definition_add_mapping(product_definition, NULL, "band=8");
    register_irradiance_product_variables(product_definition, "BAND8_IRRADIANCE");

    /* S5P_L1B_RA products. */
    description = "Sentinel-5P L1b photon radiance spectra (band 1, UV detector)";
    module =
        harp_ingestion_register_module_coda("S5P_L1B_RA_BD1", "Sentinel-5P", NULL, NULL, description,
                                            verify_s5p_l1b_ra_bd1, ingestion_init_s5p_l1b_ra, ingestion_done);

    product_definition = harp_ingestion_register_product(module, "S5P_L1B_RA_BD1", NULL, read_dimensions);
    register_radiance_product_variables(product_definition, "BAND1_RADIANCE");

    description = "Sentinel-5P L1b photon radiance spectra (band 2, UV detector)";
    module =
        harp_ingestion_register_module_coda("S5P_L1B_RA_BD2", "Sentinel-5P", NULL, NULL, description,
                                            verify_s5p_l1b_ra_bd2, ingestion_init_s5p_l1b_ra, ingestion_done);

    product_definition = harp_ingestion_register_product(module, "S5P_L1B_RA_BD2", NULL, read_dimensions);
    register_radiance_product_variables(product_definition, "BAND2_RADIANCE");

    description = "Sentinel-5P L1b photon radiance spectra (band 3, UVIS detector)";
    module =
        harp_ingestion_register_module_coda("S5P_L1B_RA_BD3", "Sentinel-5P", NULL, NULL, description,
                                            verify_s5p_l1b_ra_bd3, ingestion_init_s5p_l1b_ra, ingestion_done);

    product_definition = harp_ingestion_register_product(module, "S5P_L1B_RA_BD3", NULL, read_dimensions);
    register_radiance_product_variables(product_definition, "BAND3_RADIANCE");

    description = "Sentinel-5P L1b photon radiance spectra (band 4, UVIS detector)";
    module =
        harp_ingestion_register_module_coda("S5P_L1B_RA_BD4", "Sentinel-5P", NULL, NULL, description,
                                            verify_s5p_l1b_ra_bd4, ingestion_init_s5p_l1b_ra, ingestion_done);

    product_definition = harp_ingestion_register_product(module, "S5P_L1B_RA_BD4", NULL, read_dimensions);
    register_radiance_product_variables(product_definition, "BAND4_RADIANCE");

    description = "Sentinel-5P L1b photon radiance spectra (band 5, NIR detector)";
    module =
        harp_ingestion_register_module_coda("S5P_L1B_RA_BD5", "Sentinel-5P", NULL, NULL, description,
                                            verify_s5p_l1b_ra_bd5, ingestion_init_s5p_l1b_ra, ingestion_done);

    product_definition = harp_ingestion_register_product(module, "S5P_L1B_RA_BD5", NULL, read_dimensions);
    register_radiance_product_variables(product_definition, "BAND5_RADIANCE");

    description = "Sentinel-5P L1b photon radiance spectra (band 6, NIR detector)";
    module =
        harp_ingestion_register_module_coda("S5P_L1B_RA_BD6", "Sentinel-5P", NULL, NULL, description,
                                            verify_s5p_l1b_ra_bd6, ingestion_init_s5p_l1b_ra, ingestion_done);

    product_definition = harp_ingestion_register_product(module, "S5P_L1B_RA_BD6", NULL, read_dimensions);
    register_radiance_product_variables(product_definition, "BAND6_RADIANCE");

    description = "Sentinel-5P L1b photon radiance spectra (band 7, SWIR detector)";
    module =
        harp_ingestion_register_module_coda("S5P_L1B_RA_BD7", "Sentinel-5P", NULL, NULL, description,
                                            verify_s5p_l1b_ra_bd7, ingestion_init_s5p_l1b_ra, ingestion_done);

    product_definition = harp_ingestion_register_product(module, "S5P_L1B_RA_BD7", NULL, read_dimensions);
    register_radiance_product_variables(product_definition, "BAND7_RADIANCE");

    description = "Sentinel-5P L1b photon radiance spectra (band 8, SWIR detector)";
    module =
        harp_ingestion_register_module_coda("S5P_L1B_RA_BD8", "Sentinel-5P", NULL, NULL, description,
                                            verify_s5p_l1b_ra_bd8, ingestion_init_s5p_l1b_ra, ingestion_done);

    product_definition = harp_ingestion_register_product(module, "S5P_L1B_RA_BD8", NULL, read_dimensions);
    register_radiance_product_variables(product_definition, "BAND8_RADIANCE");

    return 0;
}
