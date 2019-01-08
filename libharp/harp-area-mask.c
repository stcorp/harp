/*
 * Copyright (C) 2015-2019 S[&]T, The Netherlands.
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

#include "harp-area-mask.h"

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define AREA_MASK_BLOCK_SIZE 1024
#define AREA_MASK_MAX_LINE_SIZE 1024

int harp_area_mask_new(harp_area_mask **new_area_mask)
{
    harp_area_mask *area_mask;

    area_mask = (harp_area_mask *)malloc(sizeof(harp_area_mask));
    if (area_mask == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       sizeof(harp_area_mask), __FILE__, __LINE__);
        return -1;
    }

    area_mask->num_polygons = 0;
    area_mask->polygon = NULL;

    *new_area_mask = area_mask;
    return 0;
}

void harp_area_mask_delete(harp_area_mask *area_mask)
{
    if (area_mask != NULL)
    {
        if (area_mask->polygon != NULL)
        {
            long i;

            for (i = 0; i < area_mask->num_polygons; i++)
            {
                harp_spherical_polygon_delete(area_mask->polygon[i]);
            }

            free(area_mask->polygon);
        }

        free(area_mask);
    }
}

int harp_area_mask_add_polygon(harp_area_mask *area_mask, harp_spherical_polygon *polygon)
{
    if (harp_spherical_polygon_check(polygon) != 0)
    {
        return -1;
    }

    if (area_mask->num_polygons % AREA_MASK_BLOCK_SIZE == 0)
    {
        harp_spherical_polygon **new_polygon = NULL;

        new_polygon = realloc(area_mask->polygon, (area_mask->num_polygons + AREA_MASK_BLOCK_SIZE)
                              * sizeof(harp_spherical_polygon *));
        if (new_polygon == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           (area_mask->num_polygons + AREA_MASK_BLOCK_SIZE) * sizeof(harp_spherical_polygon *),
                           __FILE__, __LINE__);
            return -1;
        }

        area_mask->polygon = new_polygon;
    }

    area_mask->polygon[area_mask->num_polygons] = polygon;
    area_mask->num_polygons++;
    return 0;
}

/* returns true (1) if at least one polygon of the mask covers the given point */
int harp_area_mask_covers_point(const harp_area_mask *area_mask, const harp_spherical_point *point)
{
    long i;

    for (i = 0; i < area_mask->num_polygons; i++)
    {
        if (harp_spherical_polygon_contains_point(area_mask->polygon[i], point))
        {
            return 1;
        }
    }

    return 0;
}

/* returns true (1) if at least one polygon of the mask covers the given polygon */
int harp_area_mask_covers_area(const harp_area_mask *area_mask, const harp_spherical_polygon *area)
{
    long i;

    for (i = 0; i < area_mask->num_polygons; i++)
    {
        if (harp_spherical_polygon_spherical_polygon_relationship(area_mask->polygon[i], area, 0)
            == HARP_GEOMETRY_POLY_CONTAINS)
        {
            return 1;
        }
    }

    return 0;
}

/* returns true (1) if at least one polygon of the mask falls inside the given polygon */
int harp_area_mask_inside_area(const harp_area_mask *area_mask, const harp_spherical_polygon *area)
{
    long i;

    for (i = 0; i < area_mask->num_polygons; i++)
    {
        if (harp_spherical_polygon_spherical_polygon_relationship(area_mask->polygon[i], area, 0)
            == HARP_GEOMETRY_POLY_CONTAINED)
        {
            return 1;
        }
    }

    return 0;
}

/* returns true (1) if at least one polygon of the mask intersects the given polygon */
int harp_area_mask_intersects_area(const harp_area_mask *area_mask, const harp_spherical_polygon *area)
{
    long i;

    for (i = 0; i < area_mask->num_polygons; i++)
    {
        int has_overlap;

        if (harp_spherical_polygon_overlapping(area_mask->polygon[i], area, &has_overlap) != 0)
        {
            continue;
        }
        if (has_overlap)
        {
            return 1;
        }
    }

    return 0;
}

/* returns true (1) if at least one polygon of the mask intersects the given polygon for at least the given fraction */
int harp_area_mask_intersects_area_with_fraction(const harp_area_mask *area_mask, const harp_spherical_polygon *area,
                                                 double min_fraction)
{
    long i;

    for (i = 0; i < area_mask->num_polygons; i++)
    {
        int has_overlap;
        double fraction;

        if (harp_spherical_polygon_overlapping_fraction(area_mask->polygon[i], area, &has_overlap, &fraction) != 0)
        {
            continue;
        }

        if (has_overlap && fraction >= min_fraction)
        {
            return 1;
        }
    }

    return 0;
}

static int is_blank_line(const char *str)
{
    while (*str != '\0' && isspace(*str))
    {
        str++;
    }

    return (*str == '\0');
}

static int parse_polygon(const char *str, harp_spherical_polygon **polygon)
{
    harp_spherical_point *point_array = NULL;
    long num_points = 0;

    while (1)
    {
        harp_spherical_point point;
        const char *mark;
        int length;

        while (*str != '\0' && isspace(*str))
        {
            str++;
        }

        if (*str == '\0')
        {
            break;
        }

        mark = str;
        while (*str != ',' && !isspace(*str) && *str != '\0')
        {
            str++;
        }

        length = (int)(str - mark);
        if (harp_parse_double(mark, length, &point.lat, 0) != length || !harp_isfinite(point.lat))
        {
            harp_set_error(HARP_ERROR_INVALID_FORMAT, "invalid latitude '%.*s' (%s:%u)", length, mark, __FILE__,
                           __LINE__);
            free(point_array);
            return -1;
        }

        while (*str != '\0' && isspace(*str))
        {
            str++;
        }

        if (*str != ',')
        {
            free(point_array);
            return -1;
        }

        /* Skip the comma. */
        str++;

        while (*str != '\0' && isspace(*str))
        {
            str++;
        }

        mark = str;
        while (*str != ',' && !isspace(*str) && *str != '\0')
        {
            str++;
        }

        length = (int)(str - mark);
        if (harp_parse_double(mark, length, &point.lon, 0) != length || !harp_isfinite(point.lon))
        {
            harp_set_error(HARP_ERROR_INVALID_FORMAT, "invalid longitude '%.*s' (%s:%u)", length, mark, __FILE__,
                           __LINE__);
            free(point_array);
            return -1;
        }

        /* Skip the comma, if there is one. */
        if (*str == ',')
        {
            str++;
        }

        harp_spherical_point_rad_from_deg(&point);
        harp_spherical_point_check(&point);

        if (num_points % BLOCK_SIZE == 0)
        {
            harp_spherical_point *new_point_array;

            /* grow the source_product array by one block */
            new_point_array = realloc(point_array, (num_points + BLOCK_SIZE) * sizeof(harp_spherical_point));
            if (new_point_array == NULL)
            {
                harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                               (num_points + BLOCK_SIZE) * sizeof(harp_spherical_point), __FILE__, __LINE__);
                free(point_array);
                return -1;
            }
            point_array = new_point_array;
        }
        point_array[num_points] = point;
        num_points++;
    }

    /* Discard the last point of the polygon if it is equal to the first. */
    if (num_points > 1)
    {
        if (harp_spherical_point_equal(&point_array[0], &point_array[num_points - 1]))
        {
            num_points--;
        }
    }

    if (harp_spherical_polygon_new(num_points, polygon) != 0)
    {
        free(point_array);
        return -1;
    }
    memcpy(&(*polygon)->point, point_array, num_points * sizeof(harp_spherical_point));

    free(point_array);

    /* Check the polygon */
    if (harp_spherical_polygon_check(*polygon) != 0)
    {
        harp_spherical_polygon_delete(*polygon);
        *polygon = NULL;
        return -1;
    }

    return 0;
}

static int read_area_mask(FILE *stream, harp_area_mask **new_area_mask)
{
    harp_area_mask *area_mask;
    char line[AREA_MASK_MAX_LINE_SIZE];
    int read_header;
    long i;

    if (harp_area_mask_new(&area_mask) != 0)
    {
        return -1;
    }

    i = 1;
    read_header = 0;
    while (fgets(line, AREA_MASK_MAX_LINE_SIZE, stream) != NULL)
    {
        harp_spherical_polygon *polygon;

        /* Skip blank lines. */
        if (is_blank_line(line))
        {
            i++;
            continue;
        }

        /* Skip header. */
        if (!read_header)
        {
            read_header = 1;
            i++;
            continue;
        }

        if (parse_polygon(line, &polygon) != 0)
        {
            harp_add_error_message(" (line %lu)", i);
            harp_area_mask_delete(area_mask);
            return -1;
        }

        if (harp_area_mask_add_polygon(area_mask, polygon) != 0)
        {
            harp_spherical_polygon_delete(polygon);
            harp_area_mask_delete(area_mask);
            return -1;
        }

        i++;
    }

    if (ferror(stream) || !feof(stream))
    {
        harp_set_error(HARP_ERROR_FILE_READ, "read error");
        harp_area_mask_delete(area_mask);
        return -1;
    }

    *new_area_mask = area_mask;
    return 0;
}

int harp_area_mask_read(const char *filename, harp_area_mask **new_area_mask)
{
    FILE *stream;
    harp_area_mask *area_mask;

    if (filename == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "filename is NULL");
        return -1;
    }

    stream = fopen(filename, "r");
    if (stream == NULL)
    {
        harp_set_error(HARP_ERROR_FILE_OPEN, "cannot open area mask file '%s'", filename);
        return -1;
    }

    if (read_area_mask(stream, &area_mask) != 0)
    {
        harp_add_error_message(" (while reading area mask file '%s')", filename);
        fclose(stream);
        return -1;
    }

    if (fclose(stream) != 0)
    {
        harp_set_error(HARP_ERROR_FILE_CLOSE, "cannot close area mask file '%s'", filename);
        harp_area_mask_delete(area_mask);
        return -1;
    }

    *new_area_mask = area_mask;
    return 0;
}
