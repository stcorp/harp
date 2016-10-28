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
#include "harp-geometry.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define SECONDS_FROM_1993_TO_2000 (220838400 + 5)

typedef enum omi_dim_type_enum
{
    omi_dim_time,
    omi_dim_xtrack,
    omi_dim_vertical,
    omi_dim_spectral
} omi_dim_type;

#define OMI_NUM_DIM_TYPES  (((int)omi_dim_spectral) + 1)

typedef struct variable_descriptor_struct
{
    coda_cursor cursor;
    long (*get_offset) (const long *dimension, long index);
    long length;
    double missing_value;
    double scale_factor;
    double offset;
} variable_descriptor;

typedef struct ingest_info_struct
{
    /* product */
    coda_product *product;

    /* product version */
    int product_version;        /* 2 for V2, 3 for V3, -1 for unknown */

    /* product dimensions */
    long dimension[OMI_NUM_DIM_TYPES];

    /* cursors */
    coda_cursor swath_cursor;
    coda_cursor geo_cursor;

    /* geolocation buffers */
    double *longitude_grid;
    double *latitude_grid;

    /* options */
    int clipped_cloud_fraction;
    int so2_column_level;       /* 0, 1, 2, 3 */
    int clear_sky;      /* for UVB */
    int wavelength;     /* 305, 308, 324, 380 for UVB */
    int destriped;
    int radiative_cloud_fraction;       /* use RadiativeCloudFraction */

    variable_descriptor omo3pr_pressure;
    variable_descriptor omo3pr_o3;
    variable_descriptor omo3pr_o3_precision;
    variable_descriptor omaeruv_aod;
    variable_descriptor omaeruv_aaod;
} ingest_info;

static void calculate_corner_coordinates(long num_time, long num_xtrack, const double *longitude,
                                         const double *latitude, double *longitude_grid, double *latitude_grid)
{
    double center_longitude[4]; /* the four center coordinates needed to calculate a corner coordinate */
    double center_latitude[4];
    long i;
    long j;

    /* corner coordinates lying at the outer edges are calculated by means of extrapolation. */

    /* enumerate all corner coordinates (num_xtrack + 1) x (num_time + 1) and calculate the coordinates */
    for (i = 0; i < num_time + 1; i++)
    {
        for (j = 0; j < num_xtrack + 1; j++)
        {
            long id1;   /* id of first center coordinate for extrapolation */
            long id2;   /* id of second center coordinate for extrapolation */

            if (i == 0)
            {
                /* extrapolate */
                id1 = i * num_xtrack + j - 1 + (j == 0);
                id2 = id1 + num_xtrack + (j == 0);
                harp_geographic_extrapolation(longitude[id1], latitude[id1], longitude[id2], latitude[id2],
                                              &center_longitude[0], &center_latitude[0]);

                id1 = i * num_xtrack + j - (j == num_xtrack);
                id2 = id1 + num_xtrack - (j == num_xtrack);
                harp_geographic_extrapolation(longitude[id1], latitude[id1], longitude[id2], latitude[id2],
                                              &center_longitude[1], &center_latitude[1]);
            }
            else
            {
                if (j == 0)
                {
                    /* extrapolate */
                    id1 = (i - 1) * num_xtrack + j;
                    id2 = id1 + 1;
                    harp_geographic_extrapolation(longitude[id1], latitude[id1], longitude[id2], latitude[id2],
                                                  &center_longitude[0], &center_latitude[0]);
                }
                else
                {
                    center_longitude[0] = longitude[(i - 1) * num_xtrack + j - 1];
                    center_latitude[0] = latitude[(i - 1) * num_xtrack + j - 1];
                }

                if (j == num_xtrack)
                {
                    /* extrapolate */
                    id1 = (i - 1) * num_xtrack + j - 1;
                    id2 = id1 - 1;
                    harp_geographic_extrapolation(longitude[id1], latitude[id1], longitude[id2], latitude[id2],
                                                  &center_longitude[1], &center_latitude[1]);
                }
                else
                {
                    center_longitude[1] = longitude[(i - 1) * num_xtrack + j];
                    center_latitude[1] = latitude[(i - 1) * num_xtrack + j];
                }
            }

            if (i == num_time)
            {
                /* extrapolate */
                id1 = (i - 1) * num_xtrack + j - (j == num_xtrack);
                id2 = id1 - num_xtrack - (j == num_xtrack);
                harp_geographic_extrapolation(longitude[id1], latitude[id1], longitude[id2], latitude[id2],
                                              &center_longitude[2], &center_latitude[2]);

                id1 = (i - 1) * num_xtrack + j - 1 + (j == 0);
                id2 = id1 - num_xtrack + (j == 0);
                harp_geographic_extrapolation(longitude[id1], latitude[id1], longitude[id2], latitude[id2],
                                              &center_longitude[3], &center_latitude[3]);
            }
            else
            {
                if (j == num_xtrack)
                {
                    /* extrapolate */
                    id1 = i * num_xtrack + j - 1;
                    id2 = id1 - 1;
                    harp_geographic_extrapolation(longitude[id1], latitude[id1], longitude[id2], latitude[id2],
                                                  &center_longitude[2], &center_latitude[2]);
                }
                else
                {
                    center_longitude[2] = longitude[i * num_xtrack + j];
                    center_latitude[2] = latitude[i * num_xtrack + j];
                }

                if (j == 0)
                {
                    /* extrapolate */
                    id1 = i * num_xtrack + j;
                    id2 = id1 + 1;
                    harp_geographic_extrapolation(longitude[id1], latitude[id1], longitude[id2], latitude[id2],
                                                  &center_longitude[3], &center_latitude[3]);
                }
                else
                {
                    center_longitude[3] = longitude[i * num_xtrack + j - 1];
                    center_latitude[3] = latitude[i * num_xtrack + j - 1];
                }
            }

            harp_geographic_intersection(center_longitude[0], center_latitude[0],
                                         center_longitude[2], center_latitude[2],
                                         center_longitude[1], center_latitude[1],
                                         center_longitude[3], center_latitude[3],
                                         &longitude_grid[i * (num_xtrack + 1) + j],
                                         &latitude_grid[i * (num_xtrack + 1) + j]);
        }
    }
}

static void transform_array_double(long num_elements, double *data, double missing_value, double scale_factor,
                                   double offset)
{
    double *data_end;

    for (data_end = data + num_elements; data != data_end; data++)
    {
        if (*data == missing_value)
        {
            *data = coda_NaN();
        }
        else
        {
            *data = offset + scale_factor * (*data);
        }
    }
}

static void broadcast_array_double(long num_time, long num_xtrack, double *data)
{
    long i;

    /* Repeat the value for each time for all across track samples. Iterate in reverse to avoid overwriting values. */
    for (i = num_time - 1; i >= 0; --i)
    {
        double *xtrack = data + i * num_xtrack;
        double *xtrack_end = xtrack + num_xtrack;
        const double value = data[i];

        for (; xtrack != xtrack_end; xtrack++)
        {
            *xtrack = value;
        }
    }
}

static long get_offset_pressure(const long *dimension, long index)
{
    /* OMI profile products store pressure per level instead of per layer, whereas the corresponding profiles are stored
     * per layer (where num_levels = num_layers + 1). The ingested HARP product uses num_layers as the length of the
     * vertical dimension, i.e. dimension[omi_dim_vertical] = num_layers in this case. The offset calculation below
     * accounts for this difference.
     */
    return index * (dimension[omi_dim_vertical] + 1);
}

static int has_swath_variable(ingest_info *info, const char *name)
{
    long index;

    if (coda_cursor_get_record_field_index_from_name(&info->swath_cursor, name, &index) != 0)
    {
        return 0;
    }

    return 1;
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

static int get_variable_attributes(coda_cursor *cursor, double *missing_value, double *scale_factor, double *offset)
{
    if (coda_cursor_goto_attributes(cursor) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (missing_value != NULL)
    {
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
    }
    if (scale_factor != NULL)
    {
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
    }
    if (offset != NULL)
    {
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
    }
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
    if (get_variable_attributes(&descriptor->cursor, &descriptor->missing_value, &descriptor->scale_factor,
                                &descriptor->offset) != 0)
    {
        return -1;
    }

    return 0;
}

static int read_variable_int16(ingest_info *info, coda_cursor *cursor, const char *name, harp_array data)
{
    long dimension[2] = { info->dimension[omi_dim_time], info->dimension[omi_dim_xtrack] };

    if (coda_cursor_goto_record_field_by_name(cursor, name) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (verify_variable_dimensions(cursor, 2, dimension) != 0)
    {
        return -1;
    }
    if (coda_cursor_read_int16_array(cursor, data.int16_data, coda_array_ordering_c) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    coda_cursor_goto_parent(cursor);

    return 0;
}

static int read_variable_int32(ingest_info *info, coda_cursor *cursor, const char *name, harp_array data)
{
    long dimension[2] = { info->dimension[omi_dim_time], info->dimension[omi_dim_xtrack] };

    if (coda_cursor_goto_record_field_by_name(cursor, name) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (verify_variable_dimensions(cursor, 2, dimension) != 0)
    {
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

static int read_variable_double(ingest_info *info, coda_cursor *cursor, const char *name, int num_dimensions,
                                const long *dimension, harp_array data)
{
    long default_dimension[2] = { info->dimension[omi_dim_time], info->dimension[omi_dim_xtrack] };
    long num_elements;
    double missing_value;
    double scale_factor;
    double offset;

    if (coda_cursor_goto_record_field_by_name(cursor, name) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    assert(dimension != NULL || num_dimensions <= 2);
    if (verify_variable_dimensions(cursor, num_dimensions, (dimension == NULL ? default_dimension : dimension)) != 0)
    {
        return -1;
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
    coda_cursor_goto_parent(cursor);

    /* apply scaling and filter for NaN */
    num_elements = harp_get_num_elements(num_dimensions, (dimension == NULL ? default_dimension : dimension));
    transform_array_double(num_elements, data.double_data, missing_value, scale_factor, offset);

    return 0;
}

static int read_variable_partial_double(ingest_info *info, variable_descriptor *descriptor, long index, harp_array data)
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

    if (coda_cursor_read_double_partial_array(&descriptor->cursor, offset, descriptor->length, data.double_data) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }

    /* apply scaling and filter for NaN */
    transform_array_double(descriptor->length, data.double_data, descriptor->missing_value, descriptor->scale_factor,
                           descriptor->offset);

    return 0;
}

static int verify_dimensions(ingest_info *info)
{
    /* The time and xtrack dimensions should be >1 because we need to calculate corner coordinates. */
    if (info->dimension[omi_dim_time] == 1)
    {
        harp_set_error(HARP_ERROR_INGESTION, "product error detected ('time' dimension should be larger than 1)");
        return -1;
    }
    if (info->dimension[omi_dim_xtrack] == 1)
    {
        harp_set_error(HARP_ERROR_INGESTION, "product error detected ('xtrack' dimension should be larger than 1)");
        return -1;
    }

    return 0;
}

static int verify_dimensions_omaeruv(ingest_info *info)
{
    if (verify_dimensions(info) != 0)
    {
        return -1;
    }

    if (info->dimension[omi_dim_spectral] != 3)
    {
        harp_set_error(HARP_ERROR_INGESTION, "product error detected ('spectral' dimension has length %ld, expected 3)",
                       info->dimension[omi_dim_spectral]);
        return -1;
    }

    return 0;
}

static int verify_dimensions_omo3pr(ingest_info *info)
{
    if (verify_dimensions(info) != 0)
    {
        return -1;
    }

    if (info->dimension[omi_dim_vertical] <= 0)
    {
        harp_set_error(HARP_ERROR_INGESTION, "product error detected ('vertical' dimension should be larger than 0)");
        return -1;
    }

    return 0;
}

static int init_cursors(ingest_info *info)
{
    if (coda_cursor_set_product(&info->swath_cursor, info->product) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&info->swath_cursor, "HDFEOS") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_record_field_by_name(&info->swath_cursor, "SWATHS") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_goto_first_record_field(&info->swath_cursor) != 0)
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

static int init_dimensions(ingest_info *info)
{
    coda_cursor cursor;
    long dim[CODA_MAX_NUM_DIMS];
    int num_dims;

    cursor = info->geo_cursor;
    if (coda_cursor_goto_record_field_by_name(&cursor, "Latitude") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_array_dim(&cursor, &num_dims, dim) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (num_dims != 2)
    {
        harp_set_error(HARP_ERROR_INGESTION, "product error detected (product has %d dimensions, expected 2)",
                       num_dims);
        return -1;
    }

    info->dimension[omi_dim_time] = dim[0];
    info->dimension[omi_dim_xtrack] = dim[1];

    return 0;
}

static int init_dimensions_omaeruv(ingest_info *info)
{
    coda_cursor cursor;
    long dim[CODA_MAX_NUM_DIMS];
    int num_dims;

    cursor = info->swath_cursor;
    if (coda_cursor_goto_record_field_by_name(&cursor, "FinalAerosolOpticalDepth") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_array_dim(&cursor, &num_dims, dim) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (num_dims != 3)
    {
        harp_set_error(HARP_ERROR_INGESTION, "product error detected (product has %d dimensions, expected 3)",
                       num_dims);
        return -1;
    }

    info->dimension[omi_dim_time] = dim[0];
    info->dimension[omi_dim_xtrack] = dim[1];
    info->dimension[omi_dim_spectral] = dim[2];

    return 0;
}

static int init_dimensions_omo3pr(ingest_info *info)
{
    coda_cursor cursor;
    long dim[CODA_MAX_NUM_DIMS];
    int num_dims;

    cursor = info->geo_cursor;
    if (coda_cursor_goto_record_field_by_name(&cursor, "Pressure") != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (coda_cursor_get_array_dim(&cursor, &num_dims, dim) != 0)
    {
        harp_set_error(HARP_ERROR_CODA, NULL);
        return -1;
    }
    if (num_dims != 3)
    {
        harp_set_error(HARP_ERROR_INGESTION, "product error detected (product has %d dimensions, expected 3)",
                       num_dims);
        return -1;
    }

    info->dimension[omi_dim_time] = dim[0];
    info->dimension[omi_dim_xtrack] = dim[1];
    /* Pressure data is given per level and the amount of layers is the amount of levels minus 1. */
    info->dimension[omi_dim_vertical] = dim[2] - 1;

    return 0;
}

static int init_geolocation(ingest_info *info)
{
    long num_time = info->dimension[omi_dim_time];
    long num_xtrack = info->dimension[omi_dim_xtrack];
    harp_array longitude;
    harp_array latitude;

    /* read longitude information */
    longitude.ptr = malloc(num_xtrack * num_time * sizeof(double));
    if (longitude.ptr == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_xtrack * num_time * sizeof(double), __FILE__, __LINE__);
        return -1;
    }
    if (read_variable_double(info, &info->geo_cursor, "Longitude", 2, NULL, longitude) != 0)
    {
        free(longitude.ptr);
        return -1;
    }

    /* read latitude information */
    latitude.ptr = malloc(num_xtrack * num_time * sizeof(double));
    if (latitude.ptr == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_xtrack * num_time * sizeof(double), __FILE__, __LINE__);
        free(longitude.ptr);
        return -1;
    }
    if (read_variable_double(info, &info->geo_cursor, "Latitude", 2, NULL, latitude) != 0)
    {
        free(latitude.ptr);
        free(longitude.ptr);
        return -1;
    }

    /* calculate corner coordinates */
    info->longitude_grid = malloc((num_xtrack + 1) * (num_time + 1) * sizeof(double));
    if (info->longitude_grid == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       (num_xtrack + 1) * (num_time + 1) * sizeof(double), __FILE__, __LINE__);
        free(latitude.ptr);
        free(longitude.ptr);
        return -1;
    }
    info->latitude_grid = malloc((num_xtrack + 1) * (num_time + 1) * sizeof(double));
    if (info->latitude_grid == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       (num_xtrack + 1) * (num_time + 1) * sizeof(double), __FILE__, __LINE__);
        free(latitude.ptr);
        free(longitude.ptr);
        return -1;
    }

    calculate_corner_coordinates(num_time, num_xtrack, longitude.double_data, latitude.double_data,
                                 info->longitude_grid, info->latitude_grid);

    free(latitude.ptr);
    free(longitude.ptr);

    return 0;
}

static int ingest_info_new(ingest_info **new_info)
{
    ingest_info *info;

    info = malloc(sizeof(ingest_info));
    if (info == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(ingest_info), __FILE__, __LINE__);
        return -1;
    }
    info->product = NULL;
    info->product_version = -1;
    memset(info->dimension, 0, OMI_NUM_DIM_TYPES * sizeof(long));
    info->longitude_grid = NULL;
    info->latitude_grid = NULL;
    info->clipped_cloud_fraction = 1;
    info->so2_column_level = 0;
    info->destriped = 0;
    info->clear_sky = 0;
    info->wavelength = 0;
    info->radiative_cloud_fraction = 0;

    *new_info = info;

    return 0;
}

static void ingest_info_delete(ingest_info *info)
{
    if (info->longitude_grid != NULL)
    {
        free(info->longitude_grid);
    }
    if (info->latitude_grid != NULL)
    {
        free(info->latitude_grid);
    }

    free(info);
}

static int read_dimensions(void *user_data, long dimension[HARP_NUM_DIM_TYPES])
{
    ingest_info *info = (ingest_info *)user_data;

    dimension[harp_dimension_time] = info->dimension[omi_dim_time] * info->dimension[omi_dim_xtrack];

    return 0;
}

static int read_dimensions_omaeruv(void *user_data, long dimension[HARP_NUM_DIM_TYPES])
{
    ingest_info *info = (ingest_info *)user_data;

    dimension[harp_dimension_time] = info->dimension[omi_dim_time] * info->dimension[omi_dim_xtrack];
    dimension[harp_dimension_spectral] = info->dimension[omi_dim_spectral];

    return 0;
}

static int read_dimensions_omo3pr(void *user_data, long dimension[HARP_NUM_DIM_TYPES])
{
    ingest_info *info = (ingest_info *)user_data;

    dimension[harp_dimension_time] = info->dimension[omi_dim_time] * info->dimension[omi_dim_xtrack];
    dimension[harp_dimension_vertical] = info->dimension[omi_dim_vertical];

    return 0;
}

static int read_datetime(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long i;

    /* read datetime */
    if (read_variable_double(info, &info->geo_cursor, "Time", 1, NULL, data) != 0)
    {
        return -1;
    }

    /* convert datetime values from TAI93 to seconds since 2000-01-01 */
    for (i = 0; i < info->dimension[omi_dim_time]; i++)
    {
        data.double_data[i] -= SECONDS_FROM_1993_TO_2000;
    }

    /* broadcast the result along the xtrack dimension */
    broadcast_array_double(info->dimension[omi_dim_time], info->dimension[omi_dim_xtrack], data.double_data);

    return 0;
}

static int read_longitude_bounds(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long num_xtrack;
    int i;
    int j;

    if (info->longitude_grid == NULL)
    {
        if (init_geolocation(info) != 0)
        {
            return -1;
        }
    }

    num_xtrack = info->dimension[omi_dim_xtrack];
    i = index / num_xtrack;     /* 0 <= i < num_time */
    j = index - i * num_xtrack; /* 0 <= j < num_xtrack */

    data.double_data[0] = info->longitude_grid[i * (num_xtrack + 1) + j];
    data.double_data[1] = info->longitude_grid[i * (num_xtrack + 1) + j + 1];
    data.double_data[2] = info->longitude_grid[(i + 1) * (num_xtrack + 1) + j + 1];
    data.double_data[3] = info->longitude_grid[(i + 1) * (num_xtrack + 1) + j];

    return 0;
}

static int read_latitude_bounds(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long num_xtrack;
    int i;
    int j;

    if (info->latitude_grid == NULL)
    {
        if (init_geolocation(info) != 0)
        {
            return -1;
        }
    }

    num_xtrack = info->dimension[omi_dim_xtrack];
    i = index / num_xtrack;     /* 0 <= i < num_time */
    j = index - i * num_xtrack; /* 0 <= j < num_xtrack */

    data.double_data[0] = info->latitude_grid[i * (num_xtrack + 1) + j];
    data.double_data[1] = info->latitude_grid[i * (num_xtrack + 1) + j + 1];
    data.double_data[2] = info->latitude_grid[(i + 1) * (num_xtrack + 1) + j + 1];
    data.double_data[3] = info->latitude_grid[(i + 1) * (num_xtrack + 1) + j];

    return 0;
}

static int read_longitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_double(info, &info->geo_cursor, "Longitude", 2, NULL, data);
}

static int read_latitude(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_double(info, &info->geo_cursor, "Latitude", 2, NULL, data);
}

static int read_longitude_bounds_domino(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long dimension[3] = { 4, info->dimension[omi_dim_time], info->dimension[omi_dim_xtrack] };
    long dimension_transpose[2] = { 4, info->dimension[omi_dim_time] * info->dimension[omi_dim_xtrack] };
    long i;

    if (read_variable_double(info, &info->geo_cursor, "LongitudeCornerpoints", 3, dimension, data) != 0)
    {
        return -1;
    }

    /* reorder array dimensions from [4, num_time, num_xtrack] to [num_time, num_xtrack, 4] */
    if (harp_array_transpose(harp_type_double, 2, dimension_transpose, NULL, data) != 0)
    {
        return -1;
    }

    /* reorder corner coordinates from {a,b,c,d} to {d,b,a,c} */
    for (i = 0; i < info->dimension[omi_dim_time] * info->dimension[omi_dim_xtrack]; i++)
    {
        double temp = data.double_data[i * 4];

        data.double_data[i * 4] = data.double_data[i * 4 + 3];
        data.double_data[i * 4 + 3] = data.double_data[i * 4 + 2];
        data.double_data[i * 4 + 2] = temp;
    }

    return 0;
}

static int read_latitude_bounds_domino(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    long dimension[3] = { 4, info->dimension[omi_dim_time], info->dimension[omi_dim_xtrack] };
    long dimension_transpose[2] = { 4, info->dimension[omi_dim_time] * info->dimension[omi_dim_xtrack] };
    long i;

    if (read_variable_double(info, &info->geo_cursor, "LatitudeCornerpoints", 3, dimension, data) != 0)
    {
        return -1;
    }

    /* reorder array dimensions from [4, num_time, num_xtrack] to [num_time, num_xtrack, 4] */
    if (harp_array_transpose(harp_type_double, 2, dimension_transpose, NULL, data) != 0)
    {
        return -1;
    }

    /* reorder corner coordinates from {a,b,c,d} to {d,b,a,c} */
    for (i = 0; i < info->dimension[omi_dim_time] * info->dimension[omi_dim_xtrack]; i++)
    {
        double temp = data.double_data[i * 4];

        data.double_data[i * 4] = data.double_data[i * 4 + 3];
        data.double_data[i * 4 + 3] = data.double_data[i * 4 + 2];
        data.double_data[i * 4 + 2] = temp;
    }

    return 0;
}

static int read_processing_quality_flags(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_int32(info, &info->swath_cursor, "ProcessingQualityFlags", data);
}

static int read_quality_flags(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_int32(info, &info->swath_cursor, "QualityFlags", data);
}

static int read_pressure(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_partial_double(info, &info->omo3pr_pressure, index, data);
}

static int read_o3(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_partial_double(info, &info->omo3pr_o3, index, data);
}

static int read_o3_error(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_partial_double(info, &info->omo3pr_o3_precision, index, data);
}

static int read_o3_column(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_double(info, &info->swath_cursor, "ColumnAmountO3", 2, NULL, data);
}

static int read_o3_column_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_double(info, &info->swath_cursor, "ColumnAmountO3Precision", 2, NULL, data);
}

static int read_so2_column(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int status = -1;

    if (info->product_version == 2)
    {
        if (info->so2_column_level == 1)
        {
            status = read_variable_double(info, &info->swath_cursor, "SO2ColumnAmount05KM", 2, NULL, data);
        }
        else if (info->so2_column_level == 2)
        {
            status = read_variable_double(info, &info->swath_cursor, "SO2ColumnAmount15KM", 2, NULL, data);
        }
        else
        {
            assert(info->so2_column_level == 0);
            status = read_variable_double(info, &info->swath_cursor, "SO2ColumnAmountPBL", 2, NULL, data);
        }
    }
    else
    {
        if (info->so2_column_level == 1)
        {
            status = read_variable_double(info, &info->swath_cursor, "ColumnAmountSO2_TRL", 2, NULL, data);
        }
        else if (info->so2_column_level == 2)
        {
            status = read_variable_double(info, &info->swath_cursor, "ColumnAmountSO2_TRM", 2, NULL, data);
        }
        else if (info->so2_column_level == 3)
        {
            status = read_variable_double(info, &info->swath_cursor, "ColumnAmountSO2_STL", 2, NULL, data);
        }
        else
        {
            assert(info->so2_column_level == 0);
            status = read_variable_double(info, &info->swath_cursor, "ColumnAmountSO2_PBL", 2, NULL, data);
        }
    }

    return status;
}

static int read_no2_column(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_double(info, &info->swath_cursor, "ColumnAmountNO2", 2, NULL, data);
}

static int read_no2_column_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_double(info, &info->swath_cursor, "ColumnAmountNO2Std", 2, NULL, data);
}

static int read_no2_column_tropospheric(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_double(info, &info->swath_cursor, "ColumnAmountNO2Trop", 2, NULL, data);
}

static int read_no2_column_tropospheric_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_double(info, &info->swath_cursor, "ColumnAmountNO2TropStd", 2, NULL, data);
}

static int read_no2_column_domino(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_double(info, &info->swath_cursor, "TotalVerticalColumn", 2, NULL, data);
}

static int read_no2_column_error_domino(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_double(info, &info->swath_cursor, "TotalVerticalColumnError", 2, NULL, data);
}

static int read_no2_column_tropospheric_domino(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_double(info, &info->swath_cursor, "TroposphericVerticalColumn", 2, NULL, data);
}

static int read_no2_column_tropospheric_error_domino(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_double(info, &info->swath_cursor, "TroposphericVerticalColumnError", 2, NULL, data);
}

static int read_no2_column_tropospheric_validity_domino(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_int16(info, &info->swath_cursor, "TroposphericColumnFlag", data);
}

static int read_bro_column(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int status = -1;

    if (info->destriped)
    {
        status = read_variable_double(info, &info->swath_cursor, "ColumnAmountDestriped", 2, NULL, data);
    }
    else
    {
        status = read_variable_double(info, &info->swath_cursor, "ColumnAmount", 2, NULL, data);
    }

    return status;
}

static int read_bro_column_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_double(info, &info->swath_cursor, "ColumnUncertainty", 2, NULL, data);
}

static int read_chocho_column(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int status = -1;

    if (info->destriped)
    {
        status = read_variable_double(info, &info->swath_cursor, "ColumnAmountDestriped", 2, NULL, data);
    }
    else
    {
        status = read_variable_double(info, &info->swath_cursor, "ColumnAmount", 2, NULL, data);
    }

    return status;
}

static int read_chocho_column_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_double(info, &info->swath_cursor, "ColumnUncertainty", 2, NULL, data);
}

static int read_hcho_column(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int status = -1;

    if (info->destriped)
    {
        status = read_variable_double(info, &info->swath_cursor, "ColumnAmountDestriped", 2, NULL, data);
    }
    else
    {
        status = read_variable_double(info, &info->swath_cursor, "ColumnAmount", 2, NULL, data);
    }

    return status;
}

static int read_hcho_column_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_double(info, &info->swath_cursor, "ColumnUncertainty", 2, NULL, data);
}

static int read_oclo_column(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int status = -1;

    if (info->destriped)
    {
        status = read_variable_double(info, &info->swath_cursor, "ColumnAmountDestriped", 2, NULL, data);
    }
    else
    {
        status = read_variable_double(info, &info->swath_cursor, "ColumnAmount", 2, NULL, data);
    }

    return status;
}

static int read_oclo_column_error(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_double(info, &info->swath_cursor, "ColumnUncertainty", 2, NULL, data);
}

static int read_cloud_fraction(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    int status = -1;

    if (info->radiative_cloud_fraction)
    {
        status = read_variable_double(info, &info->swath_cursor, "RadiativeCloudFraction", 2, NULL, data);
    }
    else if (info->clipped_cloud_fraction)
    {
        if (has_swath_variable(info, "fc"))
        {
            status = read_variable_double(info, &info->swath_cursor, "fc", 2, NULL, data);
        }
        else
        {
            status = read_variable_double(info, &info->swath_cursor, "CloudFraction", 2, NULL, data);
        }
    }
    else
    {
        status = read_variable_double(info, &info->swath_cursor, "CloudFractionNotClipped", 2, NULL, data);
    }

    return status;
}

static int read_cloud_fraction_for_o3(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_double(info, &info->swath_cursor, "CloudFractionforO3", 2, NULL, data);
}

static int read_cloud_fraction_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_double(info, &info->swath_cursor, "CloudFractionPrecision", 2, NULL, data);
}

static int read_cloud_fraction_std(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_double(info, &info->swath_cursor, "CloudFractionStd", 2, NULL, data);
}

static int read_pressure_cloud(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_double(info, &info->swath_cursor, "CloudPressure", 2, NULL, data);
}

static int read_pressure_cloud_for_o3(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_double(info, &info->swath_cursor, "CloudPressureforO3", 2, NULL, data);
}

static int read_pressure_cloud_top(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_double(info, &info->swath_cursor, "CloudTopPressure", 2, NULL, data);
}

static int read_pressure_cloud_precision(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_double(info, &info->swath_cursor, "CloudPressurePrecision", 2, NULL, data);
}

static int read_pressure_cloud_std(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_double(info, &info->swath_cursor, "CloudPressureStd", 2, NULL, data);
}

static int read_uv_irradiance_surface(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;
    const char *variable_name = NULL;

    switch (info->wavelength)
    {
        case 305:
            variable_name = info->clear_sky ? "CSIrradiance305" : "Irradiance305";
            break;
        case 310:
            variable_name = info->clear_sky ? "CSIrradiance310" : "Irradiance310";
            break;
        case 324:
            variable_name = info->clear_sky ? "CSIrradiance324" : "Irradiance324";
            break;
        case 380:
            variable_name = info->clear_sky ? "CSIrradiance380" : "Irradiance380";
            break;
        default:
            assert(0);
    }

    return read_variable_double(info, &info->swath_cursor, variable_name, 2, NULL, data);
}

static int read_aerosol_wavelength(void *user_data, long index, harp_array data)
{
    (void)user_data;
    (void)index;

    data.double_data[0] = 354.0;
    data.double_data[1] = 388.0;
    data.double_data[2] = 500.0;

    return 0;
}

static int read_aerosol_optical_depth(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_partial_double(info, &info->omaeruv_aod, index, data);
}

static int read_aerosol_absorbing_optical_depth(void *user_data, long index, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_partial_double(info, &info->omaeruv_aaod, index, data);
}

static int read_uv_aerosol_index(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_double(info, &info->swath_cursor, "UVAerosolIndex", 2, NULL, data);
}

static int read_vis_aerosol_index(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_double(info, &info->swath_cursor, "VISAerosolIndex", 2, NULL, data);
}

static int read_solar_zenith_angle_wgs84(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_double(info, &info->geo_cursor, "SolarZenithAngle", 2, NULL, data);
}

static int read_solar_azimuth_angle_wgs84(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_double(info, &info->geo_cursor, "SolarAzimuthAngle", 2, NULL, data);
}

static int read_viewing_zenith_angle_wgs84(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_double(info, &info->geo_cursor, "ViewingZenithAngle", 2, NULL, data);
}

static int read_viewing_azimuth_angle_wgs84(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_double(info, &info->geo_cursor, "ViewingAzimuthAngle", 2, NULL, data);
}

static int read_relative_azimuth_angle_wgs84(void *user_data, harp_array data)
{
    ingest_info *info = (ingest_info *)user_data;

    return read_variable_double(info, &info->geo_cursor, "RelativeAzimuthAngle", 2, NULL, data);
}

static int exclude_destriped(void *user_data)
{
    ingest_info *info = (ingest_info *)user_data;

    return info->destriped;
}

static int exclude_cloud_fraction(void *user_data)
{
    ingest_info *info = (ingest_info *)user_data;

    return (info->radiative_cloud_fraction && has_swath_variable(info, "RadiativeCloudFraction") == 0);
}

static int exclude_cloud_pressure(void *user_data)
{
    return (has_swath_variable((ingest_info *)user_data, "CloudPressure") == 0);
}

static int exclude_cloud_top_pressure(void *user_data)
{
    return (has_swath_variable((ingest_info *)user_data, "CloudTopPressure") == 0);
}

static int parse_option_clipped_cloud_fraction(ingest_info *info, const harp_ingestion_options *options)
{
    const char *value;

    if (harp_ingestion_options_get_option(options, "clipped_cloud_fraction", &value) == 0)
    {
        info->clipped_cloud_fraction = (strcmp(value, "true") == 0);
    }

    return 0;
}

static int parse_option_cloud_fraction_variant(ingest_info *info, const harp_ingestion_options *options)
{
    const char *value;

    if (harp_ingestion_options_get_option(options, "cloud_fraction_variant", &value) == 0)
    {
        info->radiative_cloud_fraction = (strcmp(value, "radiative") == 0);
    }

    return 0;
}

static int parse_option_so2_column_variant(ingest_info *info, const harp_ingestion_options *options)
{
    const char *value;

    if (harp_ingestion_options_get_option(options, "so2_column_variant", &value) == 0)
    {
        if (info->product_version == 2)
        {
            if (strcmp(value, "pbl") == 0)
            {
                info->so2_column_level = 0;
            }
            else if (strcmp(value, "5km") == 0)
            {
                info->so2_column_level = 1;
            }
            else if (strcmp(value, "15km") == 0)
            {
                info->so2_column_level = 2;
            }
            else
            {
                harp_set_error(HARP_ERROR_INVALID_INGESTION_OPTION_VALUE, "value '%s' for ingestion option "
                               "'so2_column_variant' not supported for product version %d", value,
                               info->product_version);
                return -1;
            }
        }
        else
        {
            if (strcmp(value, "pbl") == 0)
            {
                info->so2_column_level = 0;
            }
            else if (strcmp(value, "trl") == 0)
            {
                info->so2_column_level = 1;
            }
            else if (strcmp(value, "trm") == 0)
            {
                info->so2_column_level = 2;
            }
            else if (strcmp(value, "stl") == 0)
            {
                info->so2_column_level = 3;
            }
            else
            {
                harp_set_error(HARP_ERROR_INVALID_INGESTION_OPTION_VALUE, "value '%s' for ingestion option "
                               "'so2_column_variant' not supported for product version %d", value,
                               info->product_version);
                return -1;
            }
        }
    }

    return 0;
}

static int parse_option_destriped(ingest_info *info, const harp_ingestion_options *options)
{
    const char *value;

    if (harp_ingestion_options_get_option(options, "destriped", &value) == 0)
    {
        info->destriped = (strcmp(value, "true") == 0);
    }

    return 0;
}

static int parse_option_clear_sky(ingest_info *info, const harp_ingestion_options *options)
{
    const char *value;

    if (harp_ingestion_options_get_option(options, "clear_sky", &value) == 0)
    {
        info->clear_sky = (strcmp(value, "true") == 0);
    }

    return 0;
}

static int parse_option_wavelength_omuvb(ingest_info *info, const harp_ingestion_options *options)
{
    const char *value;

    if (harp_ingestion_options_get_option(options, "wavelength", &value) == 0)
    {
        if (strcmp(value, "305nm") == 0)
        {
            info->wavelength = 305;
        }
        else if (strcmp(value, "310nm") == 0)
        {
            info->wavelength = 310;
        }
        else if (strcmp(value, "324nm") == 0)
        {
            info->wavelength = 324;
        }
        else
        {
            /* Option values are guaranteed to be legal if present. */
            assert(strcmp(value, "380nm") == 0);
            info->wavelength = 380;
        }
    }

    return 0;
}

static void ingestion_done(void *user_data)
{
    ingest_info_delete((ingest_info *)user_data);
}

static int ingestion_init_helper(coda_product *product, ingest_info **new_info)
{
    ingest_info *info;

    if (ingest_info_new(&info) != 0)
    {
        return -1;
    }
    info->product = product;
    if (init_cursors(info) != 0)
    {
        ingest_info_delete(info);
        return -1;
    }
    if (init_dimensions(info) != 0)
    {
        ingest_info_delete(info);
        return -1;
    }
    if (verify_dimensions(info) != 0)
    {
        ingest_info_delete(info);
        return -1;
    }

    *new_info = info;

    return 0;
}

static int ingestion_init(const harp_ingestion_module *module, coda_product *product,
                          const harp_ingestion_options *options, harp_product_definition **definition, void **user_data)
{
    ingest_info *info;

    (void)options;

    if (ingestion_init_helper(product, &info) != 0)
    {
        return -1;
    }

    *definition = *module->product_definition;
    *user_data = info;

    return 0;
}

static int ingestion_init_omaeruv(const harp_ingestion_module *module, coda_product *product,
                                  const harp_ingestion_options *options, harp_product_definition **definition,
                                  void **user_data)
{
    ingest_info *info;
    long dimension[3];

    (void)options;

    if (ingest_info_new(&info) != 0)
    {
        return -1;
    }
    info->product = product;
    if (init_cursors(info) != 0)
    {
        ingest_info_delete(info);
        return -1;
    }
    if (init_dimensions_omaeruv(info) != 0)
    {
        ingest_info_delete(info);
        return -1;
    }
    if (verify_dimensions_omaeruv(info) != 0)
    {
        ingest_info_delete(info);
        return -1;
    }

    dimension[0] = info->dimension[omi_dim_time];
    dimension[1] = info->dimension[omi_dim_xtrack];
    dimension[2] = info->dimension[omi_dim_spectral];
    if (variable_descriptor_init(&info->swath_cursor, "FinalAerosolOpticalDepth", 3, dimension, NULL,
                                 info->dimension[omi_dim_spectral], &info->omaeruv_aod) != 0)
    {
        ingest_info_delete(info);
        return -1;
    }
    if (variable_descriptor_init(&info->swath_cursor, "FinalAerosolAbsOpticalDepth", 3, dimension, NULL,
                                 info->dimension[omi_dim_spectral], &info->omaeruv_aaod) != 0)
    {
        ingest_info_delete(info);
        return -1;
    }

    *definition = *module->product_definition;
    *user_data = info;

    return 0;
}

static int ingestion_init_ombro(const harp_ingestion_module *module, coda_product *product,
                                const harp_ingestion_options *options, harp_product_definition **definition,
                                void **user_data)
{
    ingest_info *info;

    if (ingestion_init_helper(product, &info) != 0)
    {
        return -1;
    }

    if (parse_option_destriped(info, options) != 0)
    {
        ingest_info_delete(info);
        return -1;
    }

    *definition = *module->product_definition;
    *user_data = info;

    return 0;
}

static int ingestion_init_omchocho(const harp_ingestion_module *module, coda_product *product,
                                   const harp_ingestion_options *options, harp_product_definition **definition,
                                   void **user_data)
{
    ingest_info *info;

    if (ingestion_init_helper(product, &info) != 0)
    {
        return -1;
    }

    if (parse_option_destriped(info, options) != 0)
    {
        ingest_info_delete(info);
        return -1;
    }

    *definition = *module->product_definition;
    *user_data = info;

    return 0;
}

static int ingestion_init_omcldo2(const harp_ingestion_module *module, coda_product *product,
                                  const harp_ingestion_options *options, harp_product_definition **definition,
                                  void **user_data)
{
    ingest_info *info;

    if (ingestion_init_helper(product, &info) != 0)
    {
        return -1;
    }

    if (parse_option_clipped_cloud_fraction(info, options) != 0)
    {
        ingest_info_delete(info);
        return -1;
    }

    *definition = *module->product_definition;
    *user_data = info;

    return 0;
}

static int ingestion_init_omdomino(const harp_ingestion_module *module, coda_product *product,
                                   const harp_ingestion_options *options, harp_product_definition **definition,
                                   void **user_data)
{
    ingest_info *info;

    (void)options;

    if (ingest_info_new(&info) != 0)
    {
        return -1;
    }
    info->product = product;
    if (init_cursors(info) != 0)
    {
        ingest_info_delete(info);
        return -1;
    }
    if (init_dimensions(info) != 0)
    {
        ingest_info_delete(info);
        return -1;
    }
    if (verify_dimensions(info) != 0)
    {
        ingest_info_delete(info);
        return -1;
    }

    *definition = *module->product_definition;
    *user_data = info;

    return 0;
}

static int ingestion_init_omhcho(const harp_ingestion_module *module, coda_product *product,
                                 const harp_ingestion_options *options, harp_product_definition **definition,
                                 void **user_data)
{
    ingest_info *info;

    if (ingestion_init_helper(product, &info) != 0)
    {
        return -1;
    }

    if (parse_option_destriped(info, options) != 0)
    {
        ingest_info_delete(info);
        return -1;
    }

    *definition = *module->product_definition;
    *user_data = info;

    return 0;
}

static int ingestion_init_omo3pr(const harp_ingestion_module *module, coda_product *product,
                                 const harp_ingestion_options *options, harp_product_definition **definition,
                                 void **user_data)
{
    ingest_info *info;
    long dimension[3];

    (void)options;

    if (ingest_info_new(&info) != 0)
    {
        return -1;
    }
    info->product = product;
    if (init_cursors(info) != 0)
    {
        ingest_info_delete(info);
        return -1;
    }
    if (init_dimensions_omo3pr(info) != 0)
    {
        ingest_info_delete(info);
        return -1;
    }
    if (verify_dimensions_omo3pr(info) != 0)
    {
        ingest_info_delete(info);
        return -1;
    }

    /* OMI profile products store pressure per level instead of per layer, whereas the corresponding profiles are stored
     * per layer (where num_levels = num_layers + 1). The ingested HARP product uses num_layers as the length of the
     * vertical dimension, i.e. info->dimension[omi_dim_vertical] = num_layers in this case.
     */
    dimension[0] = info->dimension[omi_dim_time];
    dimension[1] = info->dimension[omi_dim_xtrack];
    dimension[2] = info->dimension[omi_dim_vertical] + 1;
    if (variable_descriptor_init(&info->geo_cursor, "Pressure", 3, dimension, get_offset_pressure,
                                 info->dimension[omi_dim_vertical], &info->omo3pr_pressure) != 0)
    {
        ingest_info_delete(info);
        return -1;
    }

    dimension[0] = info->dimension[omi_dim_time];
    dimension[1] = info->dimension[omi_dim_xtrack];
    dimension[2] = info->dimension[omi_dim_vertical];
    if (variable_descriptor_init(&info->swath_cursor, "O3", 3, dimension, NULL, info->dimension[omi_dim_vertical],
                                 &info->omo3pr_o3) != 0)
    {
        ingest_info_delete(info);
        return -1;
    }
    if (variable_descriptor_init(&info->swath_cursor, "O3Precision", 3, dimension, NULL,
                                 info->dimension[omi_dim_vertical], &info->omo3pr_o3_precision) != 0)
    {
        ingest_info_delete(info);
        return -1;
    }

    *definition = *module->product_definition;
    *user_data = info;

    return 0;
}

static int ingestion_init_omoclo(const harp_ingestion_module *module, coda_product *product,
                                 const harp_ingestion_options *options, harp_product_definition **definition,
                                 void **user_data)
{
    ingest_info *info;

    if (ingestion_init_helper(product, &info) != 0)
    {
        return -1;
    }

    if (parse_option_destriped(info, options) != 0)
    {
        ingest_info_delete(info);
        return -1;
    }

    *definition = *module->product_definition;
    *user_data = info;

    return 0;
}

static int ingestion_init_omso2(const harp_ingestion_module *module, coda_product *product,
                                const harp_ingestion_options *options, harp_product_definition **definition,
                                void **user_data)
{
    ingest_info *info;

    if (ingestion_init_helper(product, &info) != 0)
    {
        return -1;
    }

    if (has_swath_variable(info, "SO2ColumnAmountPBL"))
    {
        info->product_version = 2;
    }

    /* Requires product version to be set. */
    if (parse_option_so2_column_variant(info, options) != 0)
    {
        ingest_info_delete(info);
        return -1;
    }

    *definition = *module->product_definition;
    *user_data = info;

    return 0;
}

static int ingestion_init_omto3(const harp_ingestion_module *module, coda_product *product,
                                const harp_ingestion_options *options, harp_product_definition **definition,
                                void **user_data)
{
    ingest_info *info;

    if (ingestion_init_helper(product, &info) != 0)
    {
        return -1;
    }

    if (parse_option_cloud_fraction_variant(info, options) != 0)
    {
        ingest_info_delete(info);
        return -1;
    }

    *definition = *module->product_definition;
    *user_data = info;

    return 0;
}

static int ingestion_init_omuvb(const harp_ingestion_module *module, coda_product *product,
                                const harp_ingestion_options *options, harp_product_definition **definition,
                                void **user_data)
{
    ingest_info *info;

    if (ingestion_init_helper(product, &info) != 0)
    {
        return -1;
    }

    if (parse_option_clear_sky(info, options) != 0)
    {
        ingest_info_delete(info);
        return -1;
    }

    info->wavelength = 305;

    if (parse_option_wavelength_omuvb(info, options) != 0)
    {
        ingest_info_delete(info);
        return -1;
    }

    *definition = *module->product_definition;
    *user_data = info;

    return 0;
}

static void register_datetime_variable(harp_product_definition *product_definition, const char *path)
{
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };
    const char *description;

    description = "time of the measurement";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "datetime", harp_type_double,
                                                                     1, dimension_type, NULL, description,
                                                                     "seconds since 2000-01-01", NULL, read_datetime);

    description = "the time of the measurement converted from TAI93 to seconds since 2000-01-01T00:00:00";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);
}

static void register_longitude_variable(harp_product_definition *product_definition, const char *path)
{
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };
    const char *description;

    description = "longitude of the ground pixel center (WGS84)";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "longitude",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, "degree_east", NULL, read_longitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0, 180.0);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

static void register_latitude_variable(harp_product_definition *product_definition, const char *path)
{
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };
    const char *description;

    description = "latitude of the ground pixel center (WGS84)";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_double,
                                                                     1, dimension_type, NULL, description,
                                                                     "degree_north", NULL, read_latitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

static void register_footprint_variables(harp_product_definition *product_definition)
{
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_independent };
    long dimension[2] = { -1, 4 };
    const char *description;
    const char *mapping_description;

    mapping_description = "The shape and size of each ground pixel is not included in the product. HARP therefore "
        "provides its own approximation. The calculation is based on interpolation of the available center coordinates "
        "for each of the ground pixels. Each corner coordinate is determined by its four surrounding center "
        "coordinates. The corner coordinate is exactly at the intersection of the cross that can be made with these "
        "four points (each line of the cross is the minimal distance along the earth surface from one center "
        "coordinate to the other). In situations where a corner coordinate is not surrounded by four center "
        "coordinates (i.e. at the boundaries) virtual center coordinates are created by means of extrapolation. The "
        "virtual center coordinate is placed such that the distance to its nearest real center coordinate equals the "
        "distance between that nearest real center coordinate and the next center coordinate going further inwards. In "
        "mathematical notation: when c(i,m+1) is the virtual center coordinate and c(i,m) and c(i,m-1) are real center "
        "coordinates, then ||c(i,m+1) - c(i,m)|| = ||c(i,m) - c(i,m-1)|| and all three coordinates should lie on the "
        "same great circle. The four virtual coordinates that lie in the utmost corners of the boundaries are "
        "calculated by extrapolating in a diagonal direction (e.g. c(n+1,m+1) is calculated from c(n,m) and "
        "c(n-1,m-1))";

    description = "longitudes of the ground pixel corners (WGS84)";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition, "longitude_bounds",
                                                                       harp_type_double, 2, dimension_type, dimension,
                                                                       description, "degree_east", NULL,
                                                                       read_longitude_bounds);
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0, 180.0);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, NULL, mapping_description);

    description = "latitudes of the ground pixel corners (WGS84)";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition, "latitude_bounds",
                                                                       harp_type_double, 2, dimension_type, dimension,
                                                                       description, "degree_north", NULL,
                                                                       read_latitude_bounds);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, NULL, mapping_description);
}

static void register_solar_zenith_angle_variable(harp_product_definition *product_definition, const char *path)
{
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };
    const char *description;

    description = "solar zenith angle at WGS84 ellipsoid for center co-ordinate of the ground pixel";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "solar_zenith_angle",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, "degree", NULL,
                                                                     read_solar_zenith_angle_wgs84);
    harp_variable_definition_set_valid_range_double(variable_definition, 0.0, 180.0);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

static void register_solar_azimuth_angle_variable(harp_product_definition *product_definition, const char *path)
{
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };
    const char *description;

    description = "solar azimuth angle at WGS84 ellipsoid for center co-ordinate of the ground pixel, defined East-of"
        "-North";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "solar_azimuth_angle",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, "degree", NULL,
                                                                     read_solar_azimuth_angle_wgs84);
    harp_variable_definition_set_valid_range_double(variable_definition, 0.0, 360.0);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

static void register_viewing_zenith_angle_variable(harp_product_definition *product_definition, const char *path)
{
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };
    const char *description;

    description = "viewing zenith angle at WGS84 ellipsoid for center co-ordinate of the ground pixel";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "viewing_zenith_angle",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, "degree", NULL,
                                                                     read_viewing_zenith_angle_wgs84);
    harp_variable_definition_set_valid_range_double(variable_definition, 0.0, 180.0);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

static void register_viewing_azimuth_angle_variable(harp_product_definition *product_definition, const char *path)
{
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };
    const char *description;

    description = "viewing azimuth angle at WGS84 ellipsoid for center co-ordinate of the ground pixel, defined East-of"
        "-North";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "viewing_azimuth_angle",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, "degree", NULL,
                                                                     read_viewing_azimuth_angle_wgs84);
    harp_variable_definition_set_valid_range_double(variable_definition, 0.0, 360.0);
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

static void register_omaeruv_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_spectral };
    harp_dimension_type dimension_type_wavelength[1] = { harp_dimension_spectral };
    const char *description;
    const char *path;

    module = harp_ingestion_register_module_coda("OMI_L2_OMAERUV", "OMI", "AURA_OMI", "OMAERUV",
                                                 "OMI L2 aerosol product (AOD and AAOD)", ingestion_init_omaeruv,
                                                 ingestion_done);

    /* OMAERUV product */
    product_definition = harp_ingestion_register_product(module, "OMI_L2_OMAERUV", NULL, read_dimensions_omaeruv);

    /* datetime */
    path = "/HDFEOS/SWATHS/Aerosol_NearUV_Swath/Geolocation_Fields/Time[]";
    register_datetime_variable(product_definition, path);

    /* longitude and latitude */
    path = "/HDFEOS/SWATHS/Aerosol_NearUV_Swath/Geolocation_Fields/Longitude[]";
    register_longitude_variable(product_definition, path);
    path = "/HDFEOS/SWATHS/Aerosol_NearUV_Swath/Geolocation_Fields/Latitude[]";
    register_latitude_variable(product_definition, path);

    /* longitude_bounds and latitude_bounds */
    register_footprint_variables(product_definition);

    /* wavelength */
    description = "wavelength";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition, "wavelength",
                                                                       harp_type_double, 1, dimension_type_wavelength,
                                                                       NULL, description, "nm", NULL,
                                                                       read_aerosol_wavelength);
    description = "wavelength information is not included in the product; however, the product specification for OMI"
        " OMAERUV products defines a set of three fixed wavelengths: 354, 388, and 500 nm; these wavelengths"
        " are made available as a variable that only depends on the spectral dimension (of size 3)";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, NULL, description);

    /* aerosol_optical_depth */
    description = "aerosol optical depth";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition, "aerosol_optical_depth",
                                                                       harp_type_double, 2, dimension_type, NULL,
                                                                       description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                                       read_aerosol_optical_depth);
    path = "/HDFEOS/SWATHS/Aerosol_NearUV_Swath/Data_Fields/FinalAerosolOpticalDepth[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* aerosol_absorbing_optical_depth */
    description = "aerosol absorbing optical depth";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition,
                                                                       "aerosol_absorbing_optical_depth",
                                                                       harp_type_double, 2, dimension_type, NULL,
                                                                       description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                                       read_aerosol_absorbing_optical_depth);

    path = "/HDFEOS/SWATHS/Aerosol_NearUV_Swath/Data_Fields/FinalAerosolAbsOpticalDepth[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* uv_aerosol_index */
    description = "UV aerosol index";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "uv_aerosol_index",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                                     read_uv_aerosol_index);
    path = "/HDFEOS/SWATHS/Aerosol_NearUV_Swath/Data_Fields/UVAerosolIndex[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* vis_aerosol_index */
    description = "VIS aerosol index";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "vis_aerosol_index",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                                     read_vis_aerosol_index);
    path = "/HDFEOS/SWATHS/Aerosol_NearUV_Swath/Data_Fields/VISAerosolIndex[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

static void register_ombro_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };
    const char *destriped_option_values[] = { "false", "true" };
    const char *description;
    const char *path;

    module = harp_ingestion_register_module_coda("OMI_L2_OMBRO", "OMI", "AURA_OMI", "OMBRO", "OMI L2 BrO total column",
                                                 ingestion_init_ombro, ingestion_done);

    /* destriped ingestion option */
    description = "ingest column densities with destriping correction";
    harp_ingestion_register_option(module, "destriped", description, 2, destriped_option_values);

    /* OMBRO product */
    product_definition = harp_ingestion_register_product(module, "OMI_L2_OMBRO", NULL, read_dimensions);

    /* datetime */
    path = "/HDFEOS/SWATHS/OMI_Total_Column_Amount_BRO/Geolocation_Fields/Time[]";
    register_datetime_variable(product_definition, path);

    /* longitude and latitude */
    path = "/HDFEOS/SWATHS/OMI_Total_Column_Amount_BRO/Geolocation_Fields/Longitude[]";
    register_longitude_variable(product_definition, path);
    path = "/HDFEOS/SWATHS/OMI_Total_Column_Amount_BRO/Geolocation_Fields/Latitude[]";
    register_latitude_variable(product_definition, path);

    /* longitude_bounds and latitude_bounds */
    register_footprint_variables(product_definition);

    /* BrO_column_number_density */
    description = "BrO vertical column density";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "BrO_column_number_density",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, "molec/cm^2", NULL, read_bro_column);
    path = "/HDFEOS/SWATHS/OMI_Total_Column_Amount_BRO/Data_Fields/ColumnAmount[]";
    harp_variable_definition_add_mapping(variable_definition, "destriped=false", NULL, path, NULL);
    path = "/HDFEOS/SWATHS/OMI_Total_Column_Amount_BRO/Data_Fields/ColumnAmountDestriped[]";
    harp_variable_definition_add_mapping(variable_definition, "destriped=true", NULL, path, NULL);

    /* BrO_column_number_density_uncertainty */
    description = "uncertainty of the BrO vertical column density";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition,
                                                                     "BrO_column_number_density_uncertainty",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, "molec/cm^2", exclude_destriped,
                                                                     read_bro_column_error);
    description = "will only be ingested if destriped=false (default)";
    path = "/HDFEOS/SWATHS/OMI_Total_Column_Amount_BRO/Data_Fields/ColumnUncertainty[]";
    harp_variable_definition_add_mapping(variable_definition, "destriped=false", NULL, path, description);
}

static void register_omchocho_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };
    const char *destriped_option_values[] = { "false", "true" };
    const char *description;
    const char *path;

    module =
        harp_ingestion_register_module_coda("OMI_L2_OMCHOCHO", "OMI", "AURA_OMI", "OMCHOCHO",
                                            "OMI L2 Glyoxal total column", ingestion_init_omchocho, ingestion_done);

    /* destriped ingestion option */
    description = "ingest column densities with destriping correction";
    harp_ingestion_register_option(module, "destriped", description, 2, destriped_option_values);

    /* OMCHOCHO product */
    product_definition = harp_ingestion_register_product(module, "OMI_L2_OMCHOCHO", NULL, read_dimensions);

    /* datetime */
    path = "/HDFEOS/SWATHS/OMI_Total_Column_Amount_CHOCHO/Geolocation_Fields/Time[]";
    register_datetime_variable(product_definition, path);

    /* longitude and latitude */
    path = "/HDFEOS/SWATHS/OMI_Total_Column_Amount_CHOCHO/Geolocation_Fields/Longitude[]";
    register_longitude_variable(product_definition, path);
    path = "/HDFEOS/SWATHS/OMI_Total_Column_Amount_CHOCHO/Geolocation_Fields/Latitude[]";
    register_latitude_variable(product_definition, path);

    /* longitude_bounds and latitude_bounds */
    register_footprint_variables(product_definition);

    /* C2H2O2_column_number_density */
    description = "CHOCHO vertical column density";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "C2H2O2_column_number_density",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, "molec/cm^2", NULL,
                                                                     read_chocho_column);
    path = "/HDFEOS/SWATHS/OMI_Total_Column_Amount_CHOCHO/Data_Fields/ColumnAmount[]";
    harp_variable_definition_add_mapping(variable_definition, "destriped=false", NULL, path, NULL);
    path = "/HDFEOS/SWATHS/OMI_Total_Column_Amount_CHOCHO/Data_Fields/ColumnAmountDestriped[]";
    harp_variable_definition_add_mapping(variable_definition, "destriped=true", NULL, path, NULL);

    /* BrO_column_number_density_uncertainty */
    description = "uncertainty of the CHOCHO vertical column density";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition,
                                                                     "C2H2O2_column_number_density_uncertainty",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, "molec/cm^2", exclude_destriped,
                                                                     read_chocho_column_error);
    description = "will only be ingested if destriped=false (default)";
    path = "/HDFEOS/SWATHS/OMI_Total_Column_Amount_CHOCHO/Data_Fields/ColumnUncertainty[]";
    harp_variable_definition_add_mapping(variable_definition, "destriped=false", NULL, path, description);
}

static void register_omcldo2_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };
    const char *clipped_cloud_fraction_option_values[] = { "true", "false" };
    const char *description;
    const char *path;

    module = harp_ingestion_register_module_coda("OMI_L2_OMCLDO2", "OMI", "AURA_OMI", "OMCLDO2",
                                                 "OMI L2 cloud pressure and cloud fraction (O2-O2 absorbtion)",
                                                 ingestion_init_omcldo2, ingestion_done);

    /* clipped_cloud_fraction ingestion option */
    description = "ingest clipped (to the range [0.0, 1.0]) cloud fractions";
    harp_ingestion_register_option(module, "clipped_cloud_fraction", description, 2,
                                   clipped_cloud_fraction_option_values);

    /* OMCLDO2 product */
    product_definition = harp_ingestion_register_product(module, "OMI_L2_OMCLDO2", NULL, read_dimensions);

    /* datetime */
    path = "/HDFEOS/SWATHS/CloudFractionAndPressure/Geolocation_Fields/Time[]";
    register_datetime_variable(product_definition, path);

    /* longitude and latitude */
    path = "/HDFEOS/SWATHS/CloudFractionAndPressure/Geolocation_Fields/Longitude[]";
    register_longitude_variable(product_definition, path);
    path = "/HDFEOS/SWATHS/CloudFractionAndPressure/Geolocation_Fields/Latitude[]";
    register_latitude_variable(product_definition, path);

    /* longitude_bounds and latitude_bounds */
    register_footprint_variables(product_definition);

    /* solar_zenith_angle */
    path = "/HDFEOS/SWATHS/CloudFractionAndPressure/Geolocation_Fields/SolarZenithAngle[]";
    register_solar_zenith_angle_variable(product_definition, path);

    /* solar_azimuth_angle */
    path = "/HDFEOS/SWATHS/CloudFractionAndPressure/Geolocation_Fields/SolarAzimuthAngle[]";
    register_solar_azimuth_angle_variable(product_definition, path);

    /* viewing_zenith_angle */
    path = "/HDFEOS/SWATHS/CloudFractionAndPressure/Geolocation_Fields/ViewingZenithAngle[]";
    register_viewing_zenith_angle_variable(product_definition, path);

    /* viewing_azimuth_angle */
    path = "/HDFEOS/SWATHS/CloudFractionAndPressure/Geolocation_Fields/ViewingAzimuthAngle[]";
    register_viewing_azimuth_angle_variable(product_definition, path);

    /* cloud_fraction */
    description = "effective cloud fraction";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "cloud_fraction",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                                     read_cloud_fraction);
    path = "/HDFEOS/SWATHS/CloudFractionAndPressure/Data_Fields/CloudFraction[]";
    harp_variable_definition_add_mapping(variable_definition, "clipped_cloud_fraction=true", NULL, path, NULL);
    path = "/HDFEOS/SWATHS/CloudFractionAndPressure/Data_Fields/CloudFractionNotClipped[]";
    harp_variable_definition_add_mapping(variable_definition, "clipped_cloud_fraction=false", NULL, path, NULL);

    /* cloud_fraction_uncertainty */
    description = "uncertainty of the effective cloud fraction";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "cloud_fraction_uncertainty",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                                     read_cloud_fraction_precision);
    path = "/HDFEOS/SWATHS/CloudFractionAndPressure/Data_Fields/CloudFractionPrecision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_pressure */
    description = "effective cloud pressure";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "cloud_pressure",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, "hPa", NULL, read_pressure_cloud);
    path = "/HDFEOS/SWATHS/CloudFractionAndPressure/Data_Fields/CloudPressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_pressure_uncertainty */
    description = "uncertainty of the effective cloud pressure";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "cloud_pressure_uncertainty",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, "hPa", NULL,
                                                                     read_pressure_cloud_precision);
    path = "/HDFEOS/SWATHS/CloudFractionAndPressure/Data_Fields/CloudPressurePrecision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

static void register_omcldrr_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };
    const char *description;
    const char *path;

    module = harp_ingestion_register_module_coda("OMI_L2_OMCLDRR", "OMI", "AURA-OMI", "OMCLDRR",
                                                 "OMI L2 cloud pressure and cloud fraction (Raman scattering)",
                                                 ingestion_init, ingestion_done);

    /* OMCLDRR product */
    product_definition = harp_ingestion_register_product(module, "OMI_L2_OMCLDRR", NULL, read_dimensions);

    /* datetime */
    path = "/HDFEOS/SWATHS/Cloud_Product/Geolocation_Fields/Time[]";
    register_datetime_variable(product_definition, path);

    /* longitude and latitude */
    path = "/HDFEOS/SWATHS/Cloud_Product/Geolocation_Fields/Longitude[]";
    register_longitude_variable(product_definition, path);
    path = "/HDFEOS/SWATHS/Cloud_Product/Geolocation_Fields/Latitude[]";
    register_latitude_variable(product_definition, path);

    /* longitude_bounds and latitude_bounds */
    register_footprint_variables(product_definition);

    /* solar_zenith_angle */
    path = "/HDFEOS/SWATHS/Cloud_Product/Geolocation_Fields/SolarZenithAngle[]";
    register_solar_zenith_angle_variable(product_definition, path);

    /* viewing_zenith_angle */
    path = "/HDFEOS/SWATHS/Cloud_Product/Geolocation_Fields/ViewingZenithAngle[]";
    register_viewing_zenith_angle_variable(product_definition, path);

    /* relative_azimuth_angle */
    description = "relative (sun + 180 - view) azimuth angle at WGS84 ellipsoid for center co-ordinate of the ground"
        " pixel";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "relative_azimuth_angle",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, "degree", NULL,
                                                                     read_relative_azimuth_angle_wgs84);
    harp_variable_definition_set_valid_range_double(variable_definition, 0.0, 360.0);
    path = "/HDFEOS/SWATHS/Cloud_Product/Geolocation_Fields/RelativeAzimuthAngle[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_fraction */
    description = "effective cloud fraction";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "cloud_fraction",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                                     read_cloud_fraction_for_o3);
    path = "/HDFEOS/SWATHS/Cloud_Product/Data_Fields/CloudFractionforO3[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_pressure */
    description = "effective cloud pressure";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "cloud_pressure",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, "hPa", NULL,
                                                                     read_pressure_cloud_for_o3);
    path = "/HDFEOS/SWATHS/Cloud_Product/Data_Fields/CloudPressureforO3[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

static void register_omdoao3_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };
    const char *description;
    const char *path;

    module = harp_ingestion_register_module_coda("OMI_L2_OMDOAO3", "OMI", "AURA_OMI", "OMDOAO3",
                                                 "OMI L2 O3 total column (DOAS)", ingestion_init, ingestion_done);

    /* OMDOAO3 product */
    product_definition = harp_ingestion_register_product(module, "OMI_L2_OMDOAO3", NULL, read_dimensions);

    /* datetime */
    path = "/HDFEOS/SWATHS/ColumnAmountO3/Geolocation_Fields/Time[]";
    register_datetime_variable(product_definition, path);

    /* longitude and latitude */
    path = "/HDFEOS/SWATHS/ColumnAmountO3/Geolocation_Fields/Longitude[]";
    register_longitude_variable(product_definition, path);
    path = "/HDFEOS/SWATHS/ColumnAmountO3/Geolocation_Fields/Latitude[]";
    register_latitude_variable(product_definition, path);

    /* longitude_bounds and latitude_bounds */
    register_footprint_variables(product_definition);

    /* solar_zenith_angle */
    path = "/HDFEOS/SWATHS/ColumnAmountO3/Geolocation_Fields/SolarZenithAngle[]";
    register_solar_zenith_angle_variable(product_definition, path);

    /* solar_azimuth_angle */
    path = "/HDFEOS/SWATHS/ColumnAmountO3/Geolocation_Fields/SolarAzimuthAngle[]";
    register_solar_azimuth_angle_variable(product_definition, path);

    /* viewing_zenith_angle */
    path = "/HDFEOS/SWATHS/ColumnAmountO3/Geolocation_Fields/ViewingZenithAngle[]";
    register_viewing_zenith_angle_variable(product_definition, path);

    /* viewing_azimuth_angle */
    path = "/HDFEOS/SWATHS/ColumnAmountO3/Geolocation_Fields/ViewingAzimuthAngle[]";
    register_viewing_azimuth_angle_variable(product_definition, path);

    /* O3_column_number_density */
    description = "O3 vertical column density";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "O3_column_number_density",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, "DU", NULL, read_o3_column);
    path = "/HDFEOS/SWATHS/ColumnAmountO3/Data_Fields/ColumnAmountO3[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* O3_column_number_density_uncertainty */
    description = "uncertainty of the O3 vertical column density";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition,
                                                                     "O3_column_number_density_uncertainty",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, "DU", NULL, read_o3_column_error);
    path = "/HDFEOS/SWATHS/ColumnAmountO3/Data_Fields/ColumnAmountO3Precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);


    /* O3_column_number_density_validity */
    description = "flags describing the O3 vertical column processing quality";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition,
                                                                     "O3_column_number_density_validity",
                                                                     harp_type_int32, 1, dimension_type, NULL,
                                                                     description, NULL, NULL,
                                                                     read_processing_quality_flags);
    path = "/HDFEOS/SWATHS/ColumnAmountO3/Data_Fields/ProcessingQualityFlags[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_fraction */
    description = "effective cloud fraction";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "cloud_fraction",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                                     read_cloud_fraction);
    path = "/HDFEOS/SWATHS/ColumnAmountO3/Data_Fields/CloudFraction[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_pressure */
    description = "effective cloud pressure";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "cloud_pressure",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, "hPa", NULL, read_pressure_cloud);
    path = "/HDFEOS/SWATHS/ColumnAmountO3/Data_Fields/CloudPressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_pressure_uncertainty */
    description = "uncertainty of the effective cloud pressure";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "cloud_pressure_uncertainty",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, "hPa", NULL,
                                                                     read_pressure_cloud_precision);
    path = "/HDFEOS/SWATHS/ColumnAmountO3/Data_Fields/CloudPressurePrecision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

static void register_omdomino_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };
    harp_dimension_type dimension_type_bounds[2] = { harp_dimension_time, harp_dimension_independent };
    long dimension_bounds[2] = { -1, 4 };
    const char *description;
    const char *path;

    module = harp_ingestion_register_module_coda("OMI_L2_OMDOMINO", "OMI", "AURA_OMI", "OMDOMINO",
                                                 "OMI L2 DOMINO NO2 product", ingestion_init_omdomino, ingestion_done);

    /* OMDOMINO product */
    product_definition = harp_ingestion_register_product(module, "OMI_L2_OMDOMINO", NULL, read_dimensions);

    /* datetime */
    path = "/HDFEOS/SWATHS/DominoNO2/Geolocation_Fields/Time[]";
    register_datetime_variable(product_definition, path);

    /* longitude */
    description = "longitude of the ground pixel center";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "longitude", harp_type_double,
                                                                     1, dimension_type, NULL, description,
                                                                     "degree_east", NULL, read_longitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0, 180.0);
    path = "/HDFEOS/SWATHS/DominoNO2/Geolocation_Fields/Longitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* latitude */
    description = "latitude of the ground pixel center";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "latitude", harp_type_double,
                                                                     1, dimension_type, NULL, description,
                                                                     "degree_north", NULL, read_latitude);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    path = "/HDFEOS/SWATHS/DominoNO2/Geolocation_Fields/Latitude[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* longitude_bounds */
    description = "longitudes of the ground pixel corners";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "longitude_bounds",
                                                                     harp_type_double, 2, dimension_type_bounds,
                                                                     dimension_bounds, description, "degree_east",
                                                                     NULL, read_longitude_bounds_domino);
    harp_variable_definition_set_valid_range_double(variable_definition, -180.0, 180.0);
    path = "/HDFEOS/SWATHS/DominoNO2/Geolocation_Fields/LongitudeCornerpoints[]";
    description = "coorners are reordered from {a,b,c,d} to {d,b,a,c}";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* latitude_bounds */
    description = "latitudes of the ground pixel corners";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "latitude_bounds",
                                                                     harp_type_double, 2, dimension_type_bounds,
                                                                     dimension_bounds, description, "degree_north",
                                                                     NULL, read_latitude_bounds_domino);
    harp_variable_definition_set_valid_range_double(variable_definition, -90.0, 90.0);
    path = "/HDFEOS/SWATHS/DominoNO2/Geolocation_Fields/LatitudeCornerpoints[]";
    description = "coorners are reordered from {a,b,c,d} to {d,b,a,c}";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, description);

    /* solar_zenith_angle */
    path = "/HDFEOS/SWATHS/DominoNO2/Geolocation_Fields/SolarZenithAngle[]";
    register_solar_zenith_angle_variable(product_definition, path);

    /* solar_azimuth_angle */
    path = "/HDFEOS/SWATHS/DominoNO2/Geolocation_Fields/SolarAzimuthAngle[]";
    register_solar_azimuth_angle_variable(product_definition, path);

    /* viewing_zenith_angle */
    path = "/HDFEOS/SWATHS/DominoNO2/Geolocation_Fields/ViewingZenithAngle[]";
    register_viewing_zenith_angle_variable(product_definition, path);

    /* viewing_azimuth_angle */
    path = "/HDFEOS/SWATHS/DominoNO2/Geolocation_Fields/ViewingAzimuthAngle[]";
    register_viewing_azimuth_angle_variable(product_definition, path);

    /* NO2_column_number_density */
    description = "NO2 vertical column density";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "NO2_column_number_density",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, "molec/cm^2", NULL,
                                                                     read_no2_column_domino);
    path = "/HDFEOS/SWATHS/DominoNO2/Data_Fields/TotalVerticalColumn[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* NO2_column_number_density_uncertainty */
    description = "uncertainty of the NO2 vertical column density";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition,
                                                                     "NO2_column_number_density_uncertainty",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, "molec/cm^2", NULL,
                                                                     read_no2_column_error_domino);
    path = "/HDFEOS/SWATHS/DominoNO2/Data_Fields/TotalVerticalColumnError[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* tropospheric_NO2_column_number_density */
    description = "NO2 tropospheric column density";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition,
                                                                     "tropospheric_NO2_column_number_density",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, "molec/cm^2", NULL,
                                                                     read_no2_column_tropospheric_domino);
    path = "/HDFEOS/SWATHS/DominoNO2/Data_Fields/TroposphericVerticalColumn[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* tropospheric_NO2_column_number_density_uncertainty */
    description = "uncertainty of the NO2 tropospheric column density";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition,
                                                                     "tropospheric_NO2_column_number_density_uncertainty",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, "molec/cm^2", NULL,
                                                                     read_no2_column_tropospheric_error_domino);
    path = "/HDFEOS/SWATHS/DominoNO2/Data_Fields/TroposphericVerticalColumnError[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* tropospheric_NO2_column_number_density_validity */
    description = "flags describing the NO2 tropospheric column processing quality";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition,
                                                                     "tropospheric_NO2_column_number_density_validity",
                                                                     harp_type_int16, 1, dimension_type, NULL,
                                                                     description, NULL, NULL,
                                                                     read_no2_column_tropospheric_validity_domino);
    path = "/HDFEOS/SWATHS/DominoNO2/Data_Fields/TroposphericColumnFlag[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path,
                                         "data is converted from uint8 to int16");

    /* cloud_fraction */
    description = "effective cloud fraction";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "cloud_fraction",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                                     read_cloud_fraction);
    path = "/HDFEOS/SWATHS/DominoNO2/Data_Fields/CloudFraction[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_fraction_uncertainty */
    description = "uncertainty of the effective cloud fraction";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "cloud_fraction_uncertainty",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                                     read_cloud_fraction_std);
    path = "/HDFEOS/SWATHS/DominoNO2/Data_Fields/CloudFractionStd[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_pressure */
    description = "effective cloud pressure";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "cloud_pressure",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, "hPa", NULL, read_pressure_cloud);
    path = "/HDFEOS/SWATHS/DominoNO2/Data_Fields/CloudPressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_pressure_uncertainty */
    description = "uncertainty of the effective cloud pressure";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "cloud_pressure_uncertainty",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, "hPa", NULL, read_pressure_cloud_std);
    path = "/HDFEOS/SWATHS/DominoNO2/Data_Fields/CloudPressureStd[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

static void register_omhcho_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };
    const char *destriped_option_values[] = { "false", "true" };
    const char *description;
    const char *path;

    module = harp_ingestion_register_module_coda("OMI_L2_OMHCHO", "OMI", "AURA_OMI", "OMHCHO",
                                                 "OMI L2 HCHO total column", ingestion_init_omhcho, ingestion_done);

    /* destriped ingestion option */
    description = "ingest column densities with destriping correction";
    harp_ingestion_register_option(module, "destriped", description, 2, destriped_option_values);

    /* OMHCHO product */
    product_definition = harp_ingestion_register_product(module, "OMI_L2_OMHCHO", NULL, read_dimensions);

    /* datetime */
    path = "/HDFEOS/SWATHS/OMI_Total_Column_Amount_HCHO/Geolocation_Fields/Time[]";
    register_datetime_variable(product_definition, path);

    /* longitude and latitude */
    path = "/HDFEOS/SWATHS/OMI_Total_Column_Amount_HCHO/Geolocation_Fields/Longitude[]";
    register_longitude_variable(product_definition, path);
    path = "/HDFEOS/SWATHS/OMI_Total_Column_Amount_HCHO/Geolocation_Fields/Latitude[]";
    register_latitude_variable(product_definition, path);

    /* longitude_bounds and latitude_bounds */
    register_footprint_variables(product_definition);

    /* HCHO_column_number_density */
    description = "HCHO vertical column density";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "HCHO_column_number_density",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, "molec/cm^2", NULL, read_hcho_column);
    path = "/HDFEOS/SWATHS/OMI_Total_Column_Amount_HCHO/Data_Fields/ColumnAmount[]";
    harp_variable_definition_add_mapping(variable_definition, "destriped=false", NULL, path, NULL);
    path = "/HDFEOS/SWATHS/OMI_Total_Column_Amount_HCHO/Data_Fields/ColumnAmountDestriped[]";
    harp_variable_definition_add_mapping(variable_definition, "destriped=true", NULL, path, NULL);

    /* HCHO_column_number_density_uncertainty */
    description = "uncertainty of the HCHO vertical column density";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition,
                                                                     "HCHO_column_number_density_uncertainty",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, "molec/cm^2", exclude_destriped,
                                                                     read_hcho_column_error);
    description = "will only be ingested if destriped=false (default)";
    path = "/HDFEOS/SWATHS/OMI_Total_Column_Amount_HCHO/Data_Fields/ColumnUncertainty[]";
    harp_variable_definition_add_mapping(variable_definition, "destriped=false", NULL, path, description);
}

static void register_omno2_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };
    const char *description;
    const char *path;

    module = harp_ingestion_register_module_coda("OMI_L2_OMNO2", "OMI", "AURA_OMI", "OMNO2",
                                                 "OMI L2 NO2 total and tropospheric column", ingestion_init,
                                                 ingestion_done);

    /* OMNO2 product */
    product_definition = harp_ingestion_register_product(module, "OMI_L2_OMNO2", NULL, read_dimensions);

    /* datetime */
    path = "/HDFEOS/SWATHS/ColumnAmountNO2/Geolocation_Fields/Time[]";
    register_datetime_variable(product_definition, path);

    /* longitude and latitude */
    path = "/HDFEOS/SWATHS/ColumnAmountNO2/Geolocation_Fields/Longitude[]";
    register_longitude_variable(product_definition, path);
    path = "/HDFEOS/SWATHS/ColumnAmountNO2/Geolocation_Fields/Latitude[]";
    register_latitude_variable(product_definition, path);

    /* longitude_bounds and latitude_bounds */
    register_footprint_variables(product_definition);

    /* solar_zenith_angle */
    path = "/HDFEOS/SWATHS/ColumnAmountNO2/Geolocation_Fields/SolarZenithAngle[]";
    register_solar_zenith_angle_variable(product_definition, path);

    /* solar_azimuth_angle */
    path = "/HDFEOS/SWATHS/ColumnAmountNO2/Geolocation_Fields/SolarAzimuthAngle[]";
    register_solar_azimuth_angle_variable(product_definition, path);

    /* viewing_zenith_angle */
    path = "/HDFEOS/SWATHS/ColumnAmountNO2/Geolocation_Fields/ViewingZenithAngle[]";
    register_viewing_zenith_angle_variable(product_definition, path);

    /* viewing_azimuth_angle */
    path = "/HDFEOS/SWATHS/ColumnAmountNO2/Geolocation_Fields/ViewingAzimuthAngle[]";
    register_viewing_azimuth_angle_variable(product_definition, path);

    /* NO2_column_number_density */
    description = "NO2 vertical column density";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "NO2_column_number_density",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, "molec/cm^2", NULL, read_no2_column);
    path = "/HDFEOS/SWATHS/ColumnAmountNO2/Data_Fields/ColumnAmountNO2[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* NO2_column_number_density_uncertainty */
    description = "uncertainty of the NO2 vertical column density";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition,
                                                                     "NO2_column_number_density_uncertainty",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, "molec/cm^2", NULL,
                                                                     read_no2_column_error);
    path = "/HDFEOS/SWATHS/ColumnAmountNO2/Data_Fields/ColumnAmountNO2Std[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* tropospheric_NO2_column_number_density */
    description = "NO2 tropospheric column density";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition,
                                                                     "tropospheric_NO2_column_number_density",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, "molec/cm^2", NULL,
                                                                     read_no2_column_tropospheric);
    path = "/HDFEOS/SWATHS/ColumnAmountNO2/Data_Fields/ColumnAmountNO2Trop[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* tropospheric_NO2_column_number_density_uncertainty */
    description = "uncertainty of the NO2 tropospheric column density";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition,
                                                                     "tropospheric_NO2_column_number_density_uncertainty",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, "molec/cm^2", NULL,
                                                                     read_no2_column_tropospheric_error);
    path = "/HDFEOS/SWATHS/ColumnAmountNO2/Data_Fields/ColumnAmountNO2TropStd[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_fraction */
    description = "effective cloud fraction";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "cloud_fraction",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                                     read_cloud_fraction);
    path = "/HDFEOS/SWATHS/ColumnAmountNO2/Data_Fields/CloudFraction[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_fraction_uncertainty */
    description = "uncertainty of the effective cloud fraction";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "cloud_fraction_uncertainty",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                                     read_cloud_fraction_std);
    path = "/HDFEOS/SWATHS/ColumnAmountNO2/Data_Fields/CloudFractionStd[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_pressure */
    description = "effective cloud pressure";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "cloud_pressure",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, "hPa", NULL, read_pressure_cloud);
    path = "/HDFEOS/SWATHS/ColumnAmountNO2/Data_Fields/CloudPressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_pressure_uncertainty */
    description = "uncertainty of the effective cloud pressure";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "cloud_pressure_uncertainty",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, "hPa", NULL, read_pressure_cloud_std);
    path = "/HDFEOS/SWATHS/ColumnAmountNO2/Data_Fields/CloudPressureStd[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

static void register_omo3pr_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[2] = { harp_dimension_time, harp_dimension_vertical };
    const char *description;
    const char *path;

    module = harp_ingestion_register_module_coda("OMI_L2_OMO3PR", "OMI", "AURA_OMI", "OMO3PR", "OMI L2 O3 profile",
                                                 ingestion_init_omo3pr, ingestion_done);

    /* OMO3PR product */
    product_definition = harp_ingestion_register_product(module, "OMI_L2_OMO3PR", NULL, read_dimensions_omo3pr);

    /* datetime */
    path = "/HDFEOS/SWATHS/O3Profile/Geolocation_Fields/Time[]";
    register_datetime_variable(product_definition, path);

    /* longitude and latitude */
    path = "/HDFEOS/SWATHS/O3Profile/Geolocation_Fields/Longitude[]";
    register_longitude_variable(product_definition, path);
    path = "/HDFEOS/SWATHS/O3Profile/Geolocation_Fields/Latitude[]";
    register_latitude_variable(product_definition, path);

    /* longitude_bounds and latitude_bounds */
    register_footprint_variables(product_definition);

    /* solar_zenith_angle */
    path = "/HDFEOS/SWATHS/O3Profile/Geolocation_Fields/SolarZenithAngle[]";
    register_solar_zenith_angle_variable(product_definition, path);

    /* solar_azimuth_angle */
    path = "/HDFEOS/SWATHS/O3Profile/Geolocation_Fields/SolarAzimuthAngle[]";
    register_solar_azimuth_angle_variable(product_definition, path);

    /* viewing_zenith_angle */
    path = "/HDFEOS/SWATHS/O3Profile/Geolocation_Fields/ViewingZenithAngle[]";
    register_viewing_zenith_angle_variable(product_definition, path);

    /* viewing_azimuth_angle */
    path = "/HDFEOS/SWATHS/O3Profile/Geolocation_Fields/ViewingAzimuthAngle[]";
    register_viewing_azimuth_angle_variable(product_definition, path);

    /* pressure */
    description = "the pressure level for each profile element";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition, "pressure", harp_type_double,
                                                                       2, dimension_type, NULL, description, "hPa",
                                                                       NULL, read_pressure);
    path = "/HDFEOS/SWATHS/O3Profile/Geolocation_Fields/Pressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* O3_number_density */
    description = "O3 concentration";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition, "O3_number_density",
                                                                       harp_type_double, 2, dimension_type, NULL,
                                                                       description, "DU", NULL, read_o3);
    path = "/HDFEOS/SWATHS/O3Profile/Data_Fields/O3[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* O3_number_density_uncertainty */
    description = "uncertainty of the O3 concentration";
    variable_definition = harp_ingestion_register_variable_sample_read(product_definition,
                                                                       "O3_number_density_uncertainty",
                                                                       harp_type_double, 2, dimension_type, NULL,
                                                                       description, "DU", NULL, read_o3_error);
    path = "/HDFEOS/SWATHS/O3Profile/Data_Fields/O3Precision[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* O3_number_density_validity */
    description = "flags describing the O3 profile processing quality";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "O3_number_density_validity",
                                                                     harp_type_int32, 1, dimension_type, NULL,
                                                                     description, NULL, NULL,
                                                                     read_processing_quality_flags);
    path = "/HDFEOS/SWATHS/O3Profile/Data_Fields/ProcessingQualityFlags[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);
}

static void register_omoclo_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };
    const char *destriped_option_values[] = { "false", "true" };
    const char *description;
    const char *path;

    module = harp_ingestion_register_module_coda("OMI_L2_OMOCLO", "OMI", "AURA_OMI", "OMOCLO",
                                                 "OMI L2 OClO slant column", ingestion_init_omoclo, ingestion_done);

    /* destriped ingestion option */
    description = "ingest column densities with destriping correction";
    harp_ingestion_register_option(module, "destriped", description, 2, destriped_option_values);

    /* OMOCLO product */
    product_definition = harp_ingestion_register_product(module, "OMI_L2_OMOCLO", NULL, read_dimensions);

    /* datetime */
    path = "/HDFEOS/SWATHS/OMI_Total_Column_Amount_OClO/Geolocation_Fields/Time[]";
    register_datetime_variable(product_definition, path);

    /* longitude and latitude */
    path = "/HDFEOS/SWATHS/OMI_Total_Column_Amount_OClO/Geolocation_Fields/Longitude[]";
    register_longitude_variable(product_definition, path);
    path = "/HDFEOS/SWATHS/OMI_Total_Column_Amount_OClO/Geolocation_Fields/Latitude[]";
    register_latitude_variable(product_definition, path);

    /* longitude_bounds and latitude_bounds */
    register_footprint_variables(product_definition);

    /* OClO_column_number_density */
    description = "OClO vertical column density";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "OClO_column_number_density",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, "molec/cm^2", NULL, read_oclo_column);
    path = "/HDFEOS/SWATHS/OMI_Total_Column_Amount_OClO/Data_Fields/ColumnAmount[]";
    harp_variable_definition_add_mapping(variable_definition, "destriped=false", NULL, path, NULL);
    path = "/HDFEOS/SWATHS/OMI_Total_Column_Amount_OClO/Data_Fields/ColumnAmountDestriped[]";
    harp_variable_definition_add_mapping(variable_definition, "destriped=true", NULL, path, NULL);

    /* OClO_column_number_density_uncertainty */
    description = "uncertainty of the OClO vertical column density";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition,
                                                                     "OClO_column_number_density_uncertainty",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, "molec/cm^2", exclude_destriped,
                                                                     read_oclo_column_error);
    description = "will only be ingested if destriped=false (default)";
    path = "/HDFEOS/SWATHS/OMI_Total_Column_Amount_OClO/Data_Fields/ColumnUncertainty[]";
    harp_variable_definition_add_mapping(variable_definition, "destriped=false", NULL, path, description);
}

static void register_omso2_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };
    const char *so2_column_variant_option_values[] = { "pbl", "5km", "15km", "trl", "trm", "stl" };
    const char *description;
    const char *path;

    module = harp_ingestion_register_module_coda("OMI_L2_OMSO2", "OMI", "AURA_OMI", "OMSO2", "OMI L2 SO2 total column",
                                                 ingestion_init_omso2, ingestion_done);

    /* so2_column_variant ingestion option */
    description = "for V2 products: 'pbl' (anthropogenic SO2 pollution at the planet boundary layer), '5km' (showing"
        " passive degassing at 5km altitude), or '15km' (showing explosive eruptions at 15km); for V3"
        " products: 'pbl' (planet boundary layer - 0.9km), 'trl' (lower troposhere - 2.5km), 'trm' (middle"
        " troposphere - 7.5km), 'stl' (upper tropospheric and stratospheric - 17km)";
    harp_ingestion_register_option(module, "so2_column_variant", description, 6, so2_column_variant_option_values);

    /* OMSO2 product */
    product_definition = harp_ingestion_register_product(module, "OMI_L2_OMSO2", NULL, read_dimensions);

    /* datetime */
    path = "/HDFEOS/SWATHS/OMI_Total_Column_Amount_SO2/Geolocation_Fields/Time[]";
    register_datetime_variable(product_definition, path);

    /* longitude and latitude */
    path = "/HDFEOS/SWATHS/OMI_Total_Column_Amount_SO2/Geolocation_Fields/Longitude[]";
    register_longitude_variable(product_definition, path);
    path = "/HDFEOS/SWATHS/OMI_Total_Column_Amount_SO2/Geolocation_Fields/Latitude[]";
    register_latitude_variable(product_definition, path);

    /* longitude_bounds and latitude_bounds */
    register_footprint_variables(product_definition);

    /* solar_zenith_angle */
    path = "/HDFEOS/SWATHS/OMI_Total_Column_Amount_SO2/Geolocation_Fields/SolarZenithAngle[]";
    register_solar_zenith_angle_variable(product_definition, path);

    /* solar_azimuth_angle */
    path = "/HDFEOS/SWATHS/OMI_Total_Column_Amount_SO2/Geolocation_Fields/SolarAzimuthAngle[]";
    register_solar_azimuth_angle_variable(product_definition, path);

    /* viewing_zenith_angle */
    path = "/HDFEOS/SWATHS/OMI_Total_Column_Amount_SO2/Geolocation_Fields/ViewingZenithAngle[]";
    register_viewing_zenith_angle_variable(product_definition, path);

    /* viewing_azimuth_angle */
    path = "/HDFEOS/SWATHS/OMI_Total_Column_Amount_SO2/Geolocation_Fields/ViewingAzimuthAngle[]";
    register_viewing_azimuth_angle_variable(product_definition, path);

    /* SO2_column_number_density */
    description = "SO2 vertical column density";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "SO2_column_number_density",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, "DU", NULL, read_so2_column);
    path = "/HDFEOS/SWATHS/OMI_Total_Column_Amount_SO2/Data_Fields/SO2ColumnAmountPBL[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column_variant=pbl", "V2 product", path, "default");
    path = "/HDFEOS/SWATHS/OMI_Total_Column_Amount_SO2/Data_Fields/SO2ColumnAmount05KM[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column_variant=5km", "V2 product", path, NULL);
    path = "/HDFEOS/SWATHS/OMI_Total_Column_Amount_SO2/Data_Fields/SO2ColumnAmount15KM[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column_variant=15km", "V2 product", path, NULL);
    path = "/HDFEOS/SWATHS/OMI_Total_Column_Amount_SO2/Data_Fields/ColumnAmountSO2_PBL[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column_variant=pbl", "V3 product", path, "default");
    path = "/HDFEOS/SWATHS/OMI_Total_Column_Amount_SO2/Data_Fields/ColumnAmountSO2_TRL[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column_variant=trl", "V3 product", path, NULL);
    path = "/HDFEOS/SWATHS/OMI_Total_Column_Amount_SO2/Data_Fields/ColumnAmountSO2_TRM[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column_variant=trm", "V3 product", path, NULL);
    path = "/HDFEOS/SWATHS/OMI_Total_Column_Amount_SO2/Data_Fields/ColumnAmountSO2_STL[]";
    harp_variable_definition_add_mapping(variable_definition, "so2_column_variant=stl", "V3 product", path, NULL);

    /* cloud_fraction */
    description = "effective cloud fraction";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "cloud_fraction",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, HARP_UNIT_DIMENSIONLESS, NULL,
                                                                     read_cloud_fraction);
    path = "/HDFEOS/SWATHS/OMI_Total_Column_Amount_SO2/Data_Fields/CloudFraction[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_pressure */
    description = "effective cloud pressure";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "cloud_pressure",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, "hPa", exclude_cloud_pressure,
                                                                     read_pressure_cloud);
    path = "/HDFEOS/SWATHS/OMI_Total_Column_Amount_SO2/Data_Fields/CloudPressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "V3 product", path, NULL);


    /* cloud_top_pressure */
    description = "cloud top pressure";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "cloud_top_pressure",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, "hPa", exclude_cloud_top_pressure,
                                                                     read_pressure_cloud_top);
    path = "/HDFEOS/SWATHS/OMI_Total_Column_Amount_SO2/Data_Fields/CloudTopPressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "V2 product", path, NULL);
}

static void register_omto3_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };
    const char *cloud_fraction_variant_option_values[] = { "effective", "radiative" };
    const char *description;
    const char *path;

    module = harp_ingestion_register_module_coda("OMI_L2_OMTO3", "OMI", "AURA_OMI", "OMTO3",
                                                 "OMI L2 O3 total column (TOMS)", ingestion_init_omto3, ingestion_done);

    /* cloud_fraction_variant ingestion option */
    description = "ingest effective or radiative cloud fraction (only applicable for V3 products)";
    harp_ingestion_register_option(module, "cloud_fraction_variant", description, 2,
                                   cloud_fraction_variant_option_values);

    /* OMTO3 product */
    product_definition = harp_ingestion_register_product(module, "OMI_L2_OMTO3", NULL, read_dimensions);

    /* datetime */
    path = "/HDFEOS/SWATHS/OMI_Column_Amount_O3/Geolocation_Fields/Time[]";
    register_datetime_variable(product_definition, path);

    /* longitude and latitude */
    path = "/HDFEOS/SWATHS/OMI_Column_Amount_O3/Geolocation_Fields/Longitude[]";
    register_longitude_variable(product_definition, path);
    path = "/HDFEOS/SWATHS/OMI_Column_Amount_O3/Geolocation_Fields/Latitude[]";
    register_latitude_variable(product_definition, path);

    /* longitude_bounds and latitude_bounds */
    register_footprint_variables(product_definition);

    /* solar_zenith_angle */
    path = "/HDFEOS/SWATHS/OMI_Column_Amount_O3/Geolocation_Fields/SolarZenithAngle[]";
    register_solar_zenith_angle_variable(product_definition, path);

    /* solar_azimuth_angle */
    path = "/HDFEOS/SWATHS/OMI_Column_Amount_O3/Geolocation_Fields/SolarAzimuthAngle[]";
    register_solar_azimuth_angle_variable(product_definition, path);

    /* viewing_zenith_angle */
    path = "/HDFEOS/SWATHS/OMI_Column_Amount_O3/Geolocation_Fields/ViewingZenithAngle[]";
    register_viewing_zenith_angle_variable(product_definition, path);

    /* viewing_azimuth_angle */
    path = "/HDFEOS/SWATHS/OMI_Column_Amount_O3/Geolocation_Fields/ViewingAzimuthAngle[]";
    register_viewing_azimuth_angle_variable(product_definition, path);

    /* O3_column_number_density */
    description = "ozone vertical column density";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "O3_column_number_density",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, "DU", NULL, read_o3_column);
    path = "/HDFEOS/SWATHS/OMI_Column_Amount_O3/Data_Fields/ColumnAmountO3[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* O3_column_number_density_validity */
    description = "flags describing the O3 vertical column processing quality";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition,
                                                                     "O3_column_number_density_validity",
                                                                     harp_type_int32, 1, dimension_type, NULL,
                                                                     description, NULL, NULL, read_quality_flags);
    path = "/HDFEOS/SWATHS/OMI_Column_Amount_O3/Data_Fields/QualityFlags[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, NULL, path, NULL);

    /* cloud_fraction */
    description = "effective or radiative cloud fraction";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "cloud_fraction",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, HARP_UNIT_DIMENSIONLESS,
                                                                     exclude_cloud_fraction, read_cloud_fraction);
    path = "/HDFEOS/SWATHS/OMI_Column_Amount_O3/Data_Fields/CloudFraction[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "V2 product", path, NULL);
    path = "/HDFEOS/SWATHS/OMI_Column_Amount_O3/Data_Fields/fc[]";
    harp_variable_definition_add_mapping(variable_definition, "cloud_fraction_variant=effective", "V3 product", path,
                                         NULL);
    path = "/HDFEOS/SWATHS/OMI_Column_Amount_O3/Data_Fields/RadiativeCloudFraction[]";
    harp_variable_definition_add_mapping(variable_definition, "cloud_fraction_variant=radiative", "V3 product", path,
                                         NULL);

    /* cloud_pressure */
    description = "effective cloud pressure";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "cloud_pressure",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, "hPa", exclude_cloud_pressure,
                                                                     read_pressure_cloud);
    path = "/HDFEOS/SWATHS/OMI_Column_Amount_O3/Data_Fields/CloudPressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "V3 product", path, NULL);

    /* cloud_top_pressure */
    description = "cloud top pressure";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "cloud_top_pressure",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, "hPa", exclude_cloud_top_pressure,
                                                                     read_pressure_cloud_top);
    path = "/HDFEOS/SWATHS/OMI_Column_Amount_O3/Data_Fields/CloudTopPressure[]";
    harp_variable_definition_add_mapping(variable_definition, NULL, "V2 product", path, NULL);
}

static void register_omuvb_product(void)
{
    harp_ingestion_module *module;
    harp_product_definition *product_definition;
    harp_variable_definition *variable_definition;
    harp_dimension_type dimension_type[1] = { harp_dimension_time };
    const char *clear_sky_option_values[] = { "false", "true" };
    const char *wavelength_option_values[] = { "305nm", "310nm", "324nm", "380nm" };
    const char *description;
    const char *path;

    module = harp_ingestion_register_module_coda("OMI_L2_OMUVB", "OMI", "AURA_OMI", "OMUVB",
                                                 "OMI L2 UV-B surface irradiance and erythemal dose rate",
                                                 ingestion_init_omuvb, ingestion_done);

    /* clear_sky ingestion option */
    description = "ingest clear sky surface UV irradiance";
    harp_ingestion_register_option(module, "clear_sky", description, 2, clear_sky_option_values);

    /* wavelength ingestion option */
    description = "wavelength for which to ingest the surface UV irradiance";
    harp_ingestion_register_option(module, "wavelength", description, 4, wavelength_option_values);

    /* OMUVB product */
    product_definition = harp_ingestion_register_product(module, "OMI_L2_OMUVB", NULL, read_dimensions);

    /* datetime */
    path = "/HDFEOS/SWATHS/UVB/Geolocation_Fields/Time[]";
    register_datetime_variable(product_definition, path);

    /* longitude and latitude */
    path = "/HDFEOS/SWATHS/UVB/Geolocation_Fields/Longitude[]";
    register_longitude_variable(product_definition, path);
    path = "/HDFEOS/SWATHS/UVB/Geolocation_Fields/Latitude[]";
    register_latitude_variable(product_definition, path);

    /* longitude_bounds and latitude_bounds */
    register_footprint_variables(product_definition);

    /* solar_zenith_angle */
    path = "/HDFEOS/SWATHS/UVB/Geolocation_Fields/SolarZenithAngle[]";
    register_solar_zenith_angle_variable(product_definition, path);

    /* surface_irradiance */
    description = "surface irradiance";
    variable_definition = harp_ingestion_register_variable_full_read(product_definition, "surface_irradiance",
                                                                     harp_type_double, 1, dimension_type, NULL,
                                                                     description, "W/(m^2.nm)", NULL,
                                                                     read_uv_irradiance_surface);
    path = "/HDFEOS/SWATHS/UVB/Data_Fields/Irradiance305[]";
    harp_variable_definition_add_mapping(variable_definition, "clear_sky=false and wavelength=305nm", NULL, path, NULL);
    path = "/HDFEOS/SWATHS/UVB/Data_Fields/Irradiance310[]";
    harp_variable_definition_add_mapping(variable_definition, "clear_sky=false and wavelength=310nm", NULL, path, NULL);
    path = "/HDFEOS/SWATHS/UVB/Data_Fields/Irradiance324[]";
    harp_variable_definition_add_mapping(variable_definition, "clear_sky=false and wavelength=324nm", NULL, path, NULL);
    path = "/HDFEOS/SWATHS/UVB/Data_Fields/Irradiance380[]";
    harp_variable_definition_add_mapping(variable_definition, "clear_sky=false and wavelength=380nm", NULL, path, NULL);
    path = "/HDFEOS/SWATHS/UVB/Data_Fields/CSIrradiance305[]";
    harp_variable_definition_add_mapping(variable_definition, "clear_sky=true and wavelength=305nm", NULL, path, NULL);
    path = "/HDFEOS/SWATHS/UVB/Data_Fields/CSIrradiance310[]";
    harp_variable_definition_add_mapping(variable_definition, "clear_sky=true and wavelength=310nm", NULL, path, NULL);
    path = "/HDFEOS/SWATHS/UVB/Data_Fields/CSIrradiance324[]";
    harp_variable_definition_add_mapping(variable_definition, "clear_sky=true and wavelength=324nm", NULL, path, NULL);
    path = "/HDFEOS/SWATHS/UVB/Data_Fields/CSIrradiance380[]";
    harp_variable_definition_add_mapping(variable_definition, "clear_sky=true and wavelength=380nm", NULL, path, NULL);
    path = "/HDFEOS/SWATHS/UVB/Geolocation_Fields/SolarZenithAngle[]";
}

int harp_ingestion_module_omi_l2_init(void)
{
    register_omaeruv_product();
    register_ombro_product();
    register_omchocho_product();
    register_omcldo2_product();
    register_omcldrr_product();
    register_omdoao3_product();
    register_omdomino_product();
    register_omhcho_product();
    register_omno2_product();
    register_omo3pr_product();
    register_omoclo_product();
    register_omso2_product();
    register_omto3_product();
    register_omuvb_product();

    return 0;
}
