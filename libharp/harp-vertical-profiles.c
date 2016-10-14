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

#include "harp-internal.h"
#include "harp-constants.h"
#include "harp-csv.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/** Convert geopotential height to geometric height (= altitude)
 * \param gph  Geopotential height [m]
 * \param latitude   Latitude [degree_north]
 * \return the altitude [m]
 */
double harp_altitude_from_gph_and_latitude(double gph, double latitude)
{
    double altitude;
    double g0 = (double)CONST_GRAV_ACCEL_45LAT_WGS84_SPHERE;    /* gravitational accel. [m s-2] at latitude 45o32'33'' */
    double gsurf;       /* gravitational acceleration at surface [m s-2] */
    double Rsurf;       /* local curvature radius [m] */

    gsurf = harp_gravity_at_surface_from_latitude(latitude);
    Rsurf = harp_local_curvature_radius_at_surface_from_latitude(latitude);

    altitude = g0 * Rsurf * gph / (gsurf * Rsurf - g0 * gph);
    return altitude;
}

/** Convert a pressure profile to an altitude profile
 * \param num_levels Length of vertical axis
 * \param pressure_profile Pressure vertical profile [hPa]
 * \param temperature_profile Temperature vertical profile [K]
 * \param molar_mass_air Molar mass of total air [g/mol]
 * \param surface_pressure Surface pressure [hPa]
 * \param surface_height Surface height [m]
 * \param latitude Latitude [degree_north]
 * \param altitude_profile variable in which the vertical profile will be stored [m]
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
void harp_profile_altitude_from_pressure(long num_levels, const double *pressure_profile,
                                         const double *temperature_profile, const double *molar_mass_air,
                                         double surface_pressure, double surface_height, double latitude,
                                         double *altitude_profile)
{
    double z, prev_z = 0, p, prev_p = 0, T, prev_T = 0, M, prev_M = 0, g;
    long i;

    for (i = 0; i < num_levels; i++)
    {
        long k = i;

        if (pressure_profile[0] < pressure_profile[num_levels - 1])
        {
            /* vertical axis is from TOA to surface -> invert the loop index */
            k = num_levels - 1 - i;
        }

        p = pressure_profile[k];
        M = molar_mass_air[k];
        T = temperature_profile[k];

        if (i == 0)
        {
            g = harp_gravity_at_surface_from_latitude(latitude);
            z = surface_height + 1e3 * (T / M) * (CONST_MOLAR_GAS / g) * log(surface_pressure / p);
        }
        else
        {
            g = harp_gravity_from_latitude_and_height(latitude, prev_z);
            z = prev_z + 1e3 * ((prev_T + T) / (prev_M + M)) * (CONST_MOLAR_GAS / g) * log(prev_p / p);
        }

        altitude_profile[k] = z;

        prev_p = p;
        prev_M = M;
        prev_T = T;
        prev_z = z;
    }
}

/** Convert geopotential height to geopotential
 * \param gph Geopotential height [m]
 * \return the geopotential [m2/s2]
 */
double harp_geopotential_from_gph(double gph)
{
    return CONST_GRAV_ACCEL_45LAT_WGS84_SPHERE * gph;
}

/** Convert geopotential to geopotential height
 * \param geopotential Geopotential [m2/s2]
 * \return the geopotential height [m]
 */
double harp_gph_from_geopotential(double geopotential)
{
    return geopotential / CONST_GRAV_ACCEL_45LAT_WGS84_SPHERE;
}

/** Convert geometric height (= altitude) to geopotential height
 * \param altitude  Altitude [m]
 * \param latitude   Latitude [degree_north]
 * \return the geopotential height [m]
 */
double harp_gph_from_altitude_and_latitude(double altitude, double latitude)
{
    double gsurf;       /* gravitational acceleration at surface [m] */
    double Rsurf;       /* local curvature radius [m] */

    gsurf = harp_gravity_at_surface_from_latitude(latitude);
    Rsurf = harp_local_curvature_radius_at_surface_from_latitude(latitude);

    return (gsurf / CONST_GRAV_ACCEL_45LAT_WGS84_SPHERE) * Rsurf * altitude / (altitude + Rsurf);
}

/** Convert a pressure profile to a geopotential height profile
 * \param num_levels Length of vertical axis
 * \param pressure_profile Pressure vertical profile [hPa]
 * \param temperature_profile Temperature vertical profile [K]
 * \param molar_mass_air Molar mass of total air [g/mol]
 * \param surface_pressure Surface pressure [hPa]
 * \param surface_height Surface height [m]
 * \param gph_profile Variable in which the vertical profile will be stored [m]
 */
void harp_profile_gph_from_pressure(long num_levels, const double *pressure_profile, const double *temperature_profile,
                                    const double *molar_mass_air, double surface_pressure, double surface_height,
                                    double *gph_profile)
{
    double z, prev_z = 0, p, prev_p = 0, T, prev_T = 0, M, prev_M = 0;
    long i;

    for (i = 0; i < num_levels; i++)
    {
        long k = i;

        if (pressure_profile[0] < pressure_profile[num_levels - 1])
        {
            /* vertical axis is from TOA to surface -> invert the loop index */
            k = num_levels - 1 - i;
        }

        p = pressure_profile[k];
        M = molar_mass_air[k];
        T = temperature_profile[k];

        if (i == 0)
        {
            z = surface_height + 1e3 * (T / M) * (CONST_MOLAR_GAS / CONST_GRAV_ACCEL_45LAT_WGS84_SPHERE) *
                log(surface_pressure / p);
        }
        else
        {
            z = prev_z + 1e3 * ((prev_T + T) / (prev_M + M)) * (CONST_MOLAR_GAS / CONST_GRAV_ACCEL_45LAT_WGS84_SPHERE) *
                log(prev_p / p);
        }

        gph_profile[k] = z;

        prev_p = p;
        prev_M = M;
        prev_T = T;
        prev_z = z;
    }
}

/** Integrate the partial column profile to obtain the column
 * \param num_levels              Number of levels of the partial column profile
 * \param partial_column_profile  Partial column profile [molec/m2]
 * \return column the integrated column [molec/m2]
 */
double harp_profile_column_from_partial_column(long num_levels, const double *partial_column_profile)
{
    double column = 0;
    int empty = 1;
    long k;

    /* Integrate, but ignore NaN values */
    for (k = 0; k < num_levels; k++)
    {
        if (!harp_isnan(partial_column_profile[k]))
        {
            column += partial_column_profile[k];
            empty = 0;
        }
    }

    /* Set column to NaN if all contributions were NaN */
    if (empty)
    {
        return harp_nan();
    }

    return column;
}

/** Convert an altitude profile to a pressure profile
 * \param num_levels Length of vertical axis
 * \param altitude_profile Altitude profile [m]
 * \param temperature_profile Temperature vertical profile [K]
 * \param molar_mass_air Molar mass of total air [g/mol]
 * \param surface_pressure Surface pressure [hPa]
 * \param surface_height Surface height [m]
 * \param latitude Latitude [degree_north]
 * \param pressure_profile variable in which the vertical profile will be stored [hPa]
 */
void harp_profile_pressure_from_altitude(long num_levels, const double *altitude_profile,
                                         const double *temperature_profile, const double *molar_mass_air,
                                         double surface_pressure, double surface_height, double latitude,
                                         double *pressure_profile)
{
    double z, prev_z = 0, p, prev_p = 0, T, prev_T = 0, M, prev_M = 0, g;
    long i;

    for (i = 0; i < num_levels; i++)
    {
        long k = i;

        if (altitude_profile[0] > altitude_profile[num_levels - 1])
        {
            /* vertical axis is from TOA to surface -> invert the loop index */
            k = num_levels - 1 - i;
        }

        z = altitude_profile[k];
        M = molar_mass_air[k];
        T = temperature_profile[k];

        if (i == 0)
        {
            g = harp_gravity_from_latitude_and_height(latitude, (z + surface_height) / 2);
            p = surface_pressure * exp(-1e-3 * (M / T) * (g / CONST_MOLAR_GAS) * (z - surface_height));
        }
        else
        {
            g = harp_gravity_from_latitude_and_height(latitude, (prev_z + z) / 2);
            p = prev_p * exp(-1e-3 * ((prev_M + M) / (prev_T + T)) * (g / CONST_MOLAR_GAS) * (z - prev_z));
        }

        pressure_profile[k] = p;

        prev_p = p;
        prev_M = M;
        prev_T = T;
        prev_z = z;
    }
}

/** Convert a geopotential height profile to a pressure profile
 * \param num_levels Length of vertical axis
 * \param gph_profile Geopotential height profile [m]
 * \param temperature_profile Temperature vertical profile [K]
 * \param molar_mass_air Molar mass of total air [g/mol]
 * \param surface_pressure Surface pressure [hPa]
 * \param surface_height Surface height [m]
 * \param pressure_profile Variable in which the vertical profile will be stored [hPa]
 */
void harp_profile_pressure_from_gph(long num_levels, const double *gph_profile, const double *temperature_profile,
                                    const double *molar_mass_air, double surface_pressure, double surface_height,
                                    double *pressure_profile)
{
    double z, prev_z = 0, p, prev_p = 0, T, prev_T = 0, M, prev_M = 0;
    long i;

    for (i = 0; i < num_levels; i++)
    {
        long k = i;

        if (gph_profile[0] > gph_profile[num_levels - 1])
        {
            /* vertical axis is from TOA to surface -> invert the loop index */
            k = num_levels - 1 - i;
        }

        z = gph_profile[k];
        M = molar_mass_air[k];
        T = temperature_profile[k];

        if (i == 0)
        {
            p = surface_pressure * exp(-1e-3 * (M / T) * (CONST_GRAV_ACCEL_45LAT_WGS84_SPHERE / CONST_MOLAR_GAS) *
                                       (z - surface_height));
        }
        else
        {
            p = prev_p * exp(-1e-3 * ((prev_M + M) / (prev_T + T)) *
                             (CONST_GRAV_ACCEL_45LAT_WGS84_SPHERE / CONST_MOLAR_GAS) * (z - prev_z));
        }

        pressure_profile[k] = p;

        prev_p = p;
        prev_M = M;
        prev_T = T;
        prev_z = z;
    }
}

static profile_resample_type get_profile_resample_type(harp_variable *variable)
{
    int i, num_vertical_dims;

    /* Ensure that there is only 1 vertical dimension, that it's the fastest running one and has scalar values */
    for (i = 0, num_vertical_dims = 0; i < variable->num_dimensions; i++)
    {
        if (variable->dimension_type[i] == harp_dimension_vertical)
        {
            num_vertical_dims++;
        }
    }

    if (num_vertical_dims == 0)
    {
        /* if the variable has no vertical dimension, we should always skip */
        return profile_resample_skip;
    }
    else if (num_vertical_dims == 1 &&
             variable->dimension_type[variable->num_dimensions - 1] == harp_dimension_vertical)
    {
        /* exceptions that can't be resampled */
        if (variable->data_type == harp_type_string || strstr(variable->name, "_uncertainty") != NULL ||
            strstr(variable->name, "_avk") != NULL)
        {
            return profile_resample_remove;
        }
        /* exception that uses interval interpolation */
        if (strstr(variable->name, "_column_") != NULL)
        {
            return profile_resample_interval;
        }

        /* if one vertical dimension and the fastest running one, resample linearly */
        return profile_resample_linear;
    }

    /* remove all variables with more than one vertical dimension */
    return profile_resample_remove;
}

/** Iterates over the product metadata of all the products in column b of the collocation result and
 * determines the maximum vertical dimension size.
 */
static int get_maximum_vertical_dimension(harp_collocation_result *collocation_result, long *max_vertical)
{
    int i;
    long max = 0;

    for (i = 0; i < collocation_result->num_pairs; i++)
    {
        harp_collocation_pair *pair = collocation_result->pair[i];
        long matching_product_index = pair->product_index_b;
        long match_vertical_dim_size;
        harp_product_metadata *match_metadata = collocation_result->dataset_b->metadata[matching_product_index];

        if (!match_metadata)
        {
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "Metadata unavailable for match pair product %s.",
                           collocation_result->dataset_b->source_product[matching_product_index]);
            return -1;
        }

        match_vertical_dim_size = match_metadata->dimension[harp_dimension_vertical];

        if (match_vertical_dim_size > max)
        {
            max = match_vertical_dim_size;
        }
    }

    *max_vertical = max;

    return 0;
}

static int expand_time_independent_vertical_variables(harp_product *product)
{
    int i;
    harp_variable *datetime = NULL;

    if (harp_product_get_variable_by_name(product, "datetime", &datetime) != 0)
    {
        return -1;
    }

    for (i = 0; i < product->num_variables; i++)
    {
        harp_variable *var = product->variable[i];

        /* expand if variable has a vertical dimension and does not depend on time */
        if (var->num_dimensions > 0 && var->dimension_type[0] != harp_dimension_time
            && var->dimension_type[var->num_dimensions - 1] == harp_dimension_vertical)
        {
            harp_variable_add_dimension(var, 0, harp_dimension_time, datetime->dimension[0]);
        }
    }

    return 0;
}

static int resize_vertical_dimension(harp_product *product, long max_vertical_dim)
{
    int i;

    for (i = 0; i < product->num_variables; i++)
    {
        harp_variable *var = product->variable[i];
        int j;

        for (j = 0; j < var->num_dimensions; j++)
        {
            if (var->dimension_type[j] == harp_dimension_vertical)
            {
                if (harp_variable_resize_dimension(var, j, max_vertical_dim) != 0)
                {
                    return -1;
                }
            }
        }
    }

    product->dimension[harp_dimension_vertical] = max_vertical_dim;

    return 0;
}

static int get_time_index_by_collocation_index(harp_product *product, long collocation_index, long *index)
{
    int k;
    harp_variable *product_collocation_index = NULL;

    /* Get the collocation variable from the product product */
    if (harp_product_get_variable_by_name(product, "collocation_index", &product_collocation_index) != 0)
    {
        return -1;
    }

    /* Get the datetime index into product b using the collocation index */
    for (k = 0; k < product->dimension[harp_dimension_time]; k++)
    {
        if (product_collocation_index->data.int32_data[k] == collocation_index)
        {
            *index = k;
            return 0;
        }
    }

    harp_set_error(HARP_ERROR_INVALID_ARGUMENT,
                   "Couldn't locate collocation_index %li in product %s", collocation_index, product->source_product);
    return -1;
}

static void matrix_delete(double **matrix, long dim_vertical)
{
    if (matrix != NULL)
    {
        long k;

        for (k = 0; k < dim_vertical; k++)
        {
            if (matrix[k] != NULL)
            {
                free(matrix[k]);
            }
        }
        free(matrix);
    }
}

static int matrix_vector_product(double **matrix, const double *vector_in, long m, long n, double **new_vector_out)
{
    double *vector_out = NULL;
    long i;
    long j;
    double sum;

    vector_out = calloc((size_t)m, sizeof(double));
    if (vector_out == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       m * sizeof(double), __FILE__, __LINE__);
        return -1;
    }

    for (i = 0; i < m; i++)
    {
        sum = 0.0;
        for (j = 0; j < n; j++)
        {
            if (!harp_isnan(vector_in[j]))
            {
                sum += matrix[i][j] * vector_in[j];
            }
        }
        vector_out[i] = sum;
    }

    *new_vector_out = vector_out;

    return 0;
}

static int get_vector_from_variable(const harp_variable *variable, long measurement_id, double **new_vector)
{
    double *vector = NULL;
    long dim_vertical = variable->dimension[variable->num_dimensions - 1];
    long k, ii;

    vector = malloc((size_t)dim_vertical * sizeof(double));
    if (vector == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       (size_t)dim_vertical * sizeof(double), __FILE__, __LINE__);
        return -1;
    }

    for (k = 0; k < dim_vertical; k++)
    {
        ii = measurement_id * dim_vertical + k;

        if (ii < 0 || ii >= variable->num_elements)
        {
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "index %ld is not in the range [0,%ld) (%s:%u)",
                           ii, variable->num_elements, __FILE__, __LINE__);
            free(vector);
            return -1;
        }

        vector[k] = variable->data.double_data[ii];
    }

    *new_vector = vector;

    return 0;
}

static int get_matrix_from_avk_variable(const harp_variable *avk, long time_index, double ***new_matrix)
{
    long dim_vertical = avk->dimension[avk->num_dimensions - 1];
    double **matrix = NULL;
    long k, kk, ii;

    matrix = calloc((size_t)dim_vertical, sizeof(double *));
    if (matrix == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       dim_vertical * sizeof(double *), __FILE__, __LINE__);
        return -1;
    }

    for (k = 0; k < dim_vertical; k++)
    {
        matrix[k] = calloc((size_t)dim_vertical, sizeof(double));
        if (matrix[k] == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           dim_vertical * sizeof(double), __FILE__, __LINE__);
            matrix_delete(matrix, dim_vertical);
            return -1;
        }
    }

    for (k = 0; k < dim_vertical; k++)
    {
        for (kk = 0; kk < dim_vertical; kk++)
        {
            ii = time_index * dim_vertical * dim_vertical + k * dim_vertical + kk;
            assert(ii >= 0 && ii < avk->num_elements);
            matrix[k][kk] = avk->data.double_data[ii];
        }
    }

    *new_matrix = matrix;

    return 0;
}

static int get_vertical_unit(const char *name, char **new_unit)
{
    char *unit = NULL;

    if (strcmp(name, "altitude") == 0)
    {
        unit = strdup(HARP_UNIT_LENGTH);
        if (!unit)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string)"
                           " (%s:%u)", __FILE__, __LINE__);
        }
    }
    else if (strcmp(name, "pressure") == 0)
    {
        unit = strdup(HARP_UNIT_PRESSURE);
        if (!unit)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string)"
                           " (%s:%u)", __FILE__, __LINE__);
        }
    }
    else
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "Not a vertical axis variable: '%s'", name);
        return -1;
    }

    *new_unit = unit;

    return 0;
}

static int read_vertical_grid_line(FILE *file, const char *filename, double *new_value)
{
    char line[HARP_CSV_LINE_LENGTH];
    char *cursor = line;
    double value;

    if (fgets(line, HARP_CSV_LINE_LENGTH, file) == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "error reading line of '%s'", filename);
        return -1;
    }

    harp_csv_parse_double(&cursor, &value);

    *new_value = value;

    return 0;
}

static int read_vertical_grid_header(FILE *file, const char *filename, char **new_name, char **new_unit)
{
    char line[HARP_CSV_LINE_LENGTH];
    char *original_cursor, *cursor;
    int length;
    char *name = NULL;
    char *unit = NULL;

    rewind(file);

    if (fgets(line, HARP_CSV_LINE_LENGTH, file) == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "error reading line of file '%s'", filename);
        return -1;
    }

    /* remove trailing whitespace */
    harp_csv_rtrim(line);

    /* skip leading whitespace */
    original_cursor = cursor = harp_csv_ltrim(line);

    /* Grab name */
    length = 0;
    while (*cursor != '[' && *cursor != ',' && *cursor != '\0' && !isspace(*cursor))
    {
        cursor++;
        length++;
    }

    name = malloc((length + 1) * sizeof(char));
    if (name == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       (length + 1) * sizeof(char), __FILE__, __LINE__);

        return -1;
    }
    strncpy(name, original_cursor, length);
    name[length] = '\0';

    /* Skip white space between name and unit */
    cursor = harp_csv_ltrim(cursor);

    if (*cursor != '[')
    {
        /* No unit is found */
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "No unit in header of '%s'", filename);
        free(name);
        return -1;
    }
    else
    {
        cursor++;
    }

    /* find unit length */
    length = 0;
    original_cursor = cursor;
    while (*cursor != ']' && *cursor != '\0')
    {
        cursor++;
        length++;
    }

    /* copy unit */
    unit = malloc((length + 1) * sizeof(char));
    if (unit == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       (length + 1) * sizeof(char), __FILE__, __LINE__);
        free(name);
        free(unit);
        return -1;
    }
    strncpy(unit, original_cursor, length);
    unit[length] = '\0';

    /* done, return result */
    *new_name = name;
    *new_unit = unit;

    return 0;
}

/**
 * Import vertical grid (altitude/pressure) from specified CSV file into target harp_variable.
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
int harp_profile_import_grid(const char *filename, harp_variable **new_vertical_axis)
{
    FILE *file = NULL;
    long num_vertical;
    char *name = NULL;
    char *unit = NULL;
    double *values = NULL;
    double value;
    harp_variable *vertical_axis = NULL;
    harp_dimension_type vertical_1d_dim_type[1] = { harp_dimension_vertical };
    long vertical_1d_dim[1];
    int i;

    /* open the grid file */
    file = fopen(filename, "r+");
    if (file == NULL)
    {
        harp_set_error(HARP_ERROR_FILE_OPEN, "Error opening vertical grid file '%s'", filename);
        return -1;
    }

    /* Determine number of values */
    if (harp_csv_get_num_lines(file, filename, &num_vertical) != 0)
    {
        fclose(file);
        return -1;
    }

    /* Exclude the header line */
    num_vertical--;

    if (num_vertical < 1)
    {
        /* No lines to read */
        harp_set_error(HARP_ERROR_FILE_READ, "Vertical grid file '%s' has no values", filename);
        fclose(file);
        return -1;
    }

    /* Obtain the name and unit of the quantity */
    if (read_vertical_grid_header(file, filename, &name, &unit) != 0)
    {
        fclose(file);
        return -1;
    }

    /* Obtain the values */
    values = malloc((size_t)num_vertical * sizeof(double));
    if (values == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (%s:%u)", __FILE__, __LINE__);
        return -1;
    }

    for (i = 0; i < num_vertical; i++)
    {
        if (read_vertical_grid_line(file, filename, &value) != 0)
        {
            fclose(file);
            free(values);
            free(name);
            free(unit);
            return -1;
        }

        values[i] = value;
    }

    /* io cleanup */
    if (fclose(file) != 0)
    {
        harp_set_error(HARP_ERROR_FILE_READ, "Error closing vertical grid definition file '%s'", filename);
        free(values);
        free(name);
        free(unit);
        return -1;
    }

    /* validate the axis variable name */
    if ((strcmp(name, "altitude") == 0 || strcmp(name, "pressure") == 0) != 1)
    {
        harp_set_error(HARP_ERROR_INVALID_NAME,
                       "Invalid vertical axis name '%s' in header of csv file '%s'", name, filename);
        free(values);
        free(name);
        free(unit);
        return -1;
    }

    /* create the axis variable */
    vertical_1d_dim[0] = num_vertical;
    if (harp_variable_new(name, harp_type_double, 1, vertical_1d_dim_type, vertical_1d_dim, &vertical_axis) != 0)
    {
        free(values);
        free(name);
        free(unit);
        return -1;
    }

    /* Set the axis unit */
    vertical_axis->unit = strdup(unit);
    if (vertical_axis->unit == NULL)
    {
        harp_variable_delete(vertical_axis);
        free(values);
        free(name);
        free(unit);
        return -1;
    }

    /* Copy the axis data */
    for (i = 0; i < num_vertical; i++)
    {
        vertical_axis->data.double_data[i] = values[i];
    }

    *new_vertical_axis = vertical_axis;

    /* cleanup */
    free(values);
    free(name);
    free(unit);

    return 0;
}

static long get_unpadded_vector_length(double *vector, long vector_length)
{
    long i;

    for (i = vector_length - 1; i >= 0; i--)
    {
        if (!harp_isnan(vector[i]))
        {
            return i + 1;
        }
    }

    return vector_length;
}

static int vertical_profile_smooth(harp_variable *var, harp_product *match, long time_index_a, long time_index_b)
{
    double *vector_in = NULL;
    double *vector_a_priori = NULL;
    double *vector_out = NULL;
    double **matrix = NULL;

    char *apriori_name, *avk_name;
    harp_variable *apriori, *avk = NULL;

    int has_apriori = 0;
    int i;
    long block, blocks;
    long max_vertical_elements = match->dimension[harp_dimension_vertical];

    /* get the avk and a priori variables */
    avk_name = malloc(strlen(var->name) + 4 + 1);
    apriori_name = malloc(strlen(var->name) + 8 + 1);
    if (!avk_name || !apriori_name)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)",
                       __FILE__, __LINE__);
        return -1;
    }

    strcpy(apriori_name, var->name);
    strcat(apriori_name, "_apriori");
    strcpy(avk_name, var->name);
    strcat(avk_name, "_avk");

    if (harp_product_has_variable(match, apriori_name))
    {
        has_apriori = 1;

        if (harp_product_get_variable_by_name(match, apriori_name, &apriori) != 0)
        {
            return -1;
        }
    }

    if (harp_product_get_variable_by_name(match, avk_name, &avk))
    {
        return -1;
    }

    /* check unit and data type */
    if (has_apriori && strcmp(apriori->unit, var->unit) != 0)
    {
        if (harp_variable_convert_unit(apriori, var->unit) != 0)
        {
            return -1;
        }
    }
    if (has_apriori && apriori->data_type != harp_type_double)
    {
        if (harp_variable_convert_data_type(var, harp_type_double) != 0)
        {
            return -1;
        }
    }

    /* collect avk and a priori data vectors */
    if (has_apriori && get_vector_from_variable(apriori, time_index_b, &vector_a_priori) != 0)
    {
        free(avk_name);
        free(apriori_name);
        return -1;
    }
    if (get_matrix_from_avk_variable(avk, time_index_b, &matrix) != 0)
    {
        free(avk_name);
        free(apriori_name);
        free(vector_a_priori);
        return -1;
    }

    /* allocate memory for the vertical profile input vector */
    vector_in = malloc((size_t)max_vertical_elements * sizeof(double));
    if (!vector_in)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       max_vertical_elements * sizeof(double), __FILE__, __LINE__);
        return -1;
    }

    /* calculate the number of blocks in this datetime slice of the variable */
    blocks = var->num_elements / var->dimension[0] / max_vertical_elements;

    for (block = 0; block < blocks; block++)
    {
        long blockoffset = (time_index_a * blocks + block) * max_vertical_elements;

        /* figure out the actual unpadded length of the input vector. */
        long vertical_elements = get_unpadded_vector_length(&var->data.double_data[blockoffset], max_vertical_elements);

        /* collect profile vector */
        for (i = 0; i < vertical_elements; i++)
        {
            vector_in[i] = var->data.double_data[blockoffset + i];
        }

        /* subtract a priori */
        if (has_apriori)
        {
            for (i = 0; i < vertical_elements; i++)
            {
                vector_in[i] -= vector_a_priori[i];
            }
        }

        /* premultiply avk */
        if (matrix_vector_product(matrix, vector_in, vertical_elements, vertical_elements, &vector_out) != 0)
        {
            free(avk_name);
            free(apriori_name);
            free(vector_a_priori);
            free(vector_in);
            return -1;
        }

        /* add the apriori */
        if (has_apriori)
        {
            for (i = 0; i < vertical_elements; i++)
            {
                vector_out[i] += vector_a_priori[i];
            }
        }

        /* update the variable */
        for (i = 0; i < vertical_elements; i++)
        {
            var->data.double_data[blockoffset + i] = vector_out[i];
        }
    }

    /* cleanup */
    free(avk_name);
    free(apriori_name);
    free(vector_in);
    free(vector_a_priori);
    matrix_delete(matrix, max_vertical_elements);

    return 0;
}

static int product_filter_resamplable_variables(harp_product *product)
{
    int i;

    for (i = product->num_variables - 1; i >= 0; i--)
    {
        harp_variable *var = product->variable[i];

        int var_type = get_profile_resample_type(var);

        if (var_type == profile_resample_remove)
        {
            if (harp_product_remove_variable(product, var) != 0)
            {
                return -1;
            }

            continue;
        }
    }

    return 0;
}

/** \addtogroup harp_product
 * @{
 */

/**
 * Resamples all variables in product against a specified grid.
 * Target_grid is expected to be a variable of dimensions {vertical}.
 * The source grid is determined by derivation of a matching vertical quantity on the specified product.
 *
 * \param product Product to resample.
 * \param target_grid Vertical grid to target.
 *
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_product_regrid_vertical_with_axis_variable(harp_product *product, harp_variable *target_grid)
{
    harp_variable *source_grid = NULL;
    harp_variable *vertical_axis = NULL;
    long target_vertical_elements = target_grid->dimension[target_grid->num_dimensions - 1];
    long source_time_dim_length = 0;    /* 0 indicates that we do time-independent regridding */
    long source_vertical_elements;
    int i;

    harp_dimension_type vertical_1d_dim_type[1] = { harp_dimension_vertical };
    harp_dimension_type vertical_2d_dim_type[2] = { harp_dimension_time, harp_dimension_vertical };

    /* Derive the source grid (will give doubles because unit is passed) */
    if (harp_product_add_derived_variable(product, target_grid->name, target_grid->unit, 1, vertical_1d_dim_type) != 0)
    {
        /* Failed to derive 1D source grid. Try 2D */
        if (harp_product_add_derived_variable(product,
                                              target_grid->name, target_grid->unit, 2, vertical_2d_dim_type) != 0)
        {
            return -1;
        }
    }

    /* Retrieve basic info about the source grid */
    harp_product_get_variable_by_name(product, target_grid->name, &source_grid);
    if (source_grid->num_dimensions > 1)
    {
        source_time_dim_length = source_grid->dimension[0];
    }
    source_vertical_elements = source_grid->dimension[source_grid->num_dimensions - 1];

    /* Resample all variables if we know how */
    for (i = product->num_variables - 1; i >= 0; i--)
    {
        harp_variable *variable = product->variable[i];
        harp_array old_data;

        long new_data_num_elements = variable->num_elements / source_vertical_elements * target_vertical_elements;
        double *new_data = NULL;
        long num_blocks = variable->num_elements / source_vertical_elements;
        long time_blocks = num_blocks;
        profile_resample_type variable_type;

        long time;
        long block_id;

        /* Calculate the number of num_blocks for which time is constant for time-dependent resampling */
        if (source_time_dim_length != 0)
        {
            time_blocks = num_blocks / source_time_dim_length;
        }

        /* Check if we can resample this kind of variable */
        variable_type = get_profile_resample_type(variable);

        /* skip the source grid variable, we'll set that afterwards */
        if (variable == source_grid)
        {
            variable_type = profile_resample_skip;
        }

        if (variable_type == profile_resample_skip)
        {
            continue;
        }
        else if (variable_type == profile_resample_remove)
        {
            harp_report_warning("Removing variable %s; unresamplable dimensions\n", variable->name);
            harp_product_remove_variable(product, variable);
            continue;
        }

        /* Ensure that the variable data consists of doubles */
        if (variable->data_type != harp_type_double && harp_variable_convert_data_type(variable, harp_type_double) != 0)
        {
            harp_variable_delete(target_grid);
            return -1;
        }

        /* time independent variables with a time-dependent source grid are time-extended */
        if (variable->dimension_type[0] != harp_dimension_time && source_grid->dimension[0] == harp_dimension_time)
        {
            harp_variable_add_dimension(variable, 0, harp_dimension_time, source_time_dim_length);
        }

        /* Setup target array */
        new_data = (double *)malloc((size_t)new_data_num_elements * sizeof(double));
        if (new_data == NULL)
        {
            harp_variable_delete(target_grid);
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)",
                           __FILE__, __LINE__);
            return -1;
        }

        /* Interpolate the data of the variable over the vertical axis */
        time = -1;
        for (block_id = 0; block_id < num_blocks; block_id++)
        {
            /* keep track of time for time-dependent vertical grids */
            if (block_id % time_blocks == 0)
            {
                time++;
            }

            harp_interpolate_array_linear(source_vertical_elements,
                                          source_grid->data.double_data + time * source_vertical_elements,
                                          variable->data.double_data + block_id * source_vertical_elements,
                                          target_vertical_elements, target_grid->data.double_data, 0,
                                          new_data + block_id * target_vertical_elements);
        }

        /* Update the vertical dimension length */
        variable->dimension[variable->num_dimensions - 1] = target_vertical_elements;

        /* Set the new variable data */
        old_data = variable->data;

        variable->data.double_data = new_data;
        variable->num_elements = new_data_num_elements;

        /* Clean up the old data */
        free(old_data.double_data);
    }

    /* ensure consistent axis variable in product */
    product->dimension[harp_dimension_vertical] = target_vertical_elements;
    harp_variable_copy(target_grid, &vertical_axis);
    if (harp_product_replace_variable(product, vertical_axis) != 0)
    {
        return -1;
    }

    return 0;
}

/** Smooth the product's variables (from dataset a in the collocation result) using the vertical grids,
 * avks and a apriori of matching products in dataset b and smooth the variables specified.
 *
 * \param product Product to smooth.
 * \param num_smooth_variables length of smooth_variables.
 * \param smooth_variables The names of the variables to smooth.
 * \param vertical_axis The name of the variable to use as a vertical axis (pressure/altitude/etc).
 * \param original_collocation_result The collocation result used to locate the matching vertical
 *   grids/avks/apriori.
 *   The collocation result is assumed to have the appropriate metadata available for all matches (dataset b).
 *
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_product_smooth_vertical(harp_product *product, int num_smooth_variables,
                                             const char **smooth_variables, const char *vertical_axis,
                                             const harp_collocation_result *original_collocation_result)
{
    int time_index_a, pair_id;
    long num_source_max_vertical_elements;      /* actual elems + NaN padding */
    harp_variable *source_collocation_index = NULL;
    harp_dimension_type grid_dim_type[2] = { harp_dimension_time, harp_dimension_vertical };
    harp_dimension_type bounds_dim_type[3] = { harp_dimension_time, harp_dimension_vertical,
        harp_dimension_independent
    };
    long max_vertical_dim;

    /* owned memory */
    harp_variable *source_grid = NULL;
    harp_variable *source_bounds = NULL;
    harp_product *match = NULL;
    harp_variable *target_grid = NULL;
    harp_variable *target_bounds = NULL;
    harp_collocation_result *collocation_result = NULL;
    char *vertical_unit = NULL;
    char *bounds_name = NULL;
    double *interpolation_buffer = NULL;

    /* derive the name of the bounds variable for the vertical axis */
    bounds_name = malloc(strlen(vertical_axis) + 7 + 1);
    if (!bounds_name)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string)"
                       " (%s:%u)", __FILE__, __LINE__);
        goto error;
    }
    strcpy(bounds_name, vertical_axis);
    strcat(bounds_name, "_bounds");

    /* copy the collocation result for filtering */
    if (harp_collocation_result_shallow_copy(original_collocation_result, &collocation_result) != 0)
    {
        goto error;
    }

    /* get the default unit for the chosen vertical axis type */
    if (get_vertical_unit(vertical_axis, &vertical_unit) != 0)
    {
        goto error;
    }

    /* Get the source product's collocation index variable */
    if (harp_product_get_variable_by_name(product, "collocation_index", &source_collocation_index) != 0)
    {
        goto error;
    }

    /* Prepare the collocation result for efficient iteration over the pairs */
    if (harp_collocation_result_filter_for_source_product_a(collocation_result, product->source_product) != 0)
    {
        goto error;
    }
    if (harp_collocation_result_sort_by_collocation_index(collocation_result) != 0)
    {
        goto error;
    }

    /* Determine the maximum vertical dimensions size */
    if (get_maximum_vertical_dimension(collocation_result, &max_vertical_dim) != 0)
    {
        goto error;
    }

    /* Remove variables that can't be resampled */
    if (product_filter_resamplable_variables(product) != 0)
    {
        goto error;
    }

    /* Expand time independent vertical profiles */
    if (expand_time_independent_vertical_variables(product) != 0)
    {
        goto error;
    }

    /* Derive the source grid */
    if (harp_product_get_derived_variable(product, vertical_axis, vertical_unit, 2, grid_dim_type, &source_grid) != 0)
    {
        goto error;
    }

    /* Use loglin interpolation if pressure grid */
    if (strcmp(source_grid->name, "pressure") == 0)
    {
        int i;

        for (i = 0; i < source_grid->num_elements; i++)
        {
            source_grid->data.double_data[i] = log(source_grid->data.double_data[i]);
        }
    }

    /* Save the length of the original vertical dimension */
    num_source_max_vertical_elements = product->dimension[harp_dimension_vertical];

    /* Resize the vertical dimension in the target product to make room for the resampled data */
    if (max_vertical_dim > product->dimension[harp_dimension_vertical])
    {
        if (resize_vertical_dimension(product, max_vertical_dim) != 0)
        {
            goto error;
        }
    }

    /* allocate the buffer for the interpolation */
    interpolation_buffer = (double *)malloc(max_vertical_dim * (size_t)sizeof(double));
    if (interpolation_buffer == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY,
                       "out of memory (could not allocate %lu bytes) (%s:%u)",
                       max_vertical_dim * (size_t)sizeof(double), __FILE__, __LINE__);
        goto error;
    }

    for (pair_id = 0, time_index_a = 0; time_index_a < product->dimension[harp_dimension_time]; time_index_a++)
    {
        harp_collocation_pair *pair = NULL;
        harp_product_metadata *match_metadata;
        long time_index_b = -1;
        int j;
        long coll_index;
        long num_target_vertical_elements;
        long num_target_max_vertical_elements;
        long num_target_offset;
        long num_source_vertical_elements;

        /* Get the collocation index */
        coll_index = source_collocation_index->data.int32_data[time_index_a];

        /* Get the match-pair for said collocation index */
        for (pair_id = 0; pair_id < collocation_result->num_pairs; pair_id++)
        {
            if (collocation_result->pair[pair_id]->collocation_index == coll_index)
            {
                pair = collocation_result->pair[pair_id];
                break;
            }
        }

        /* Error if no collocation pair exists for this index */
        if (!pair)
        {
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "No collocation pair for collocation index %li.", coll_index);
            goto error;
        }

        /* Get metadata of the matching product */
        match_metadata = collocation_result->dataset_b->metadata[pair->product_index_b];
        if (match_metadata == NULL)
        {
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "Missing product metadata for product %s.",
                           collocation_result->dataset_b->source_product[pair->product_index_b]);
            goto error;
        }

        /* load the matching product if necessary */
        if (match == NULL || strcmp(match->source_product, match_metadata->source_product) != 0)
        {
            /* cleanup previous match product */
            if (match != NULL)
            {
                harp_product_delete(match);
                match = NULL;
            }

            /* import new product */
            harp_import(match_metadata->filename, &match);
            if (!match)
            {
                harp_set_error(HARP_ERROR_IMPORT, "Could not import file %s.", match_metadata->filename);
                goto error;
            }

            /* Derive the target grid */
            harp_variable_delete(target_grid);
            if (harp_product_get_derived_variable(match, vertical_axis, vertical_unit, 2, grid_dim_type, &target_grid)
                != 0)
            {
                goto error;
            }
            /* Use loglin interpolation if pressure grid */
            if (strcmp(target_grid->name, "pressure") == 0)
            {
                int i;

                for (i = 0; i < target_grid->num_elements; i++)
                {
                    target_grid->data.double_data[i] = log(target_grid->data.double_data[i]);
                }
            }

            /* Cleanup the target bounds, they will get loaded again when necessary */
            harp_variable_delete(target_bounds);
            target_bounds = NULL;
        }

        if (get_time_index_by_collocation_index(match, pair->collocation_index, &time_index_b) != 0)
        {
            goto error;
        }

        /* find the source grid lengths */
        num_source_vertical_elements =
            get_unpadded_vector_length(&source_grid->data.double_data[time_index_a * num_source_max_vertical_elements],
                                       num_source_max_vertical_elements);


        /* figure out the target offset to use: i.e. the number of vertical profile elements that fall
         * below the first source profile elements
         */
        num_target_max_vertical_elements = target_grid->dimension[1];
        for (num_target_offset = 0; num_target_offset < num_target_max_vertical_elements; num_target_offset++)
        {
            long block_offset = time_index_a * num_target_max_vertical_elements;

            if (target_grid->data.double_data[block_offset + num_target_offset] >=
                source_grid->data.double_data[time_index_a * num_source_max_vertical_elements])
            {
                break;
            }
        }

        /* find the target grid length */
        num_target_vertical_elements =
            get_unpadded_vector_length(&target_grid->data.double_data[time_index_a * num_target_max_vertical_elements],
                                       num_target_max_vertical_elements) - num_target_offset;

        /* Resample & smooth variables */
        for (j = product->num_variables - 1; j >= 0; j--)
        {
            harp_variable *var = product->variable[j];

            long block, blocks;
            int k;

            /* Skip variables that don't need resampling */
            profile_resample_type var_type = get_profile_resample_type(var);

            if (var_type == profile_resample_skip)
            {
                continue;
            }

            /* derive bounds variables if necessary for resampling */
            if (var_type == profile_resample_interval)
            {
                if (target_bounds == NULL)
                {
                    if (harp_product_get_derived_variable(match, bounds_name, vertical_unit, 3, bounds_dim_type,
                                                          &target_bounds) != 0)
                    {
                        goto error;
                    }
                }
                if (source_bounds == NULL)
                {
                    if (harp_product_get_derived_variable(product, bounds_name, vertical_unit, 3, bounds_dim_type,
                                                          &source_bounds) != 0)
                    {
                        goto error;
                    }
                }
            }

            /* Ensure that the variable data to resample consists of doubles */
            if (var->data_type != harp_type_double && harp_variable_convert_data_type(var, harp_type_double) != 0)
            {
                goto error;
            }

            /* Interpolate variable data */
            blocks = var->num_elements / var->dimension[0] / num_source_max_vertical_elements;
            for (block = 0; block < blocks; block++)
            {
                int l;
                long target_block_index = (time_index_a * blocks + block) * num_target_max_vertical_elements;
                long source_block_index = (time_index_a * blocks + block) * num_source_max_vertical_elements;

                if (var_type == profile_resample_linear)
                {
                    harp_interpolate_array_linear
                        (num_source_vertical_elements,
                         &source_grid->data.double_data[time_index_a * num_source_max_vertical_elements],
                         &var->data.double_data[source_block_index], num_target_vertical_elements,
                         &target_grid->data.double_data[time_index_b * num_target_max_vertical_elements +
                                                        num_target_offset], 0, interpolation_buffer);
                }
                else if (var_type == profile_resample_interval)
                {
                    harp_interval_interpolate_array_linear
                        (num_source_vertical_elements,
                         &source_bounds->data.double_data[time_index_a * num_source_max_vertical_elements * 2],
                         &var->data.double_data[(time_index_a * blocks + block) * num_source_max_vertical_elements],
                         num_target_vertical_elements,
                         &target_bounds->data.double_data[(time_index_b * num_target_max_vertical_elements +
                                                           num_target_offset) * 2], interpolation_buffer);
                }
                else
                {
                    /* other resampling methods are not supported, but should also never be set */
                    assert(0);
                    exit(1);
                }

                /* copy the buffer to the target var */
                for (l = 0; l < num_target_vertical_elements; l++)
                {
                    var->data.double_data[target_block_index + l] = interpolation_buffer[l];
                }
            }

            /* Smooth variable if it's index appears in smooth_variables */
            for (k = 0; k < num_smooth_variables; k++)
            {
                if (strcmp(smooth_variables[k], var->name) == 0)
                {
                    if (vertical_profile_smooth(var, match, time_index_a, time_index_b) != 0)
                    {
                        goto error;
                    }
                }
            }
        }
    }

    /* Resize the vertical dimension in the target product to minimal size */
    if (max_vertical_dim < product->dimension[harp_dimension_vertical])
    {
        if (resize_vertical_dimension(product, max_vertical_dim) != 0)
        {
            goto error;
        }
    }

    /* cleanup */
    harp_variable_delete(source_grid);
    harp_variable_delete(source_bounds);
    harp_variable_delete(target_grid);
    harp_variable_delete(target_bounds);
    harp_product_delete(match);
    harp_collocation_result_shallow_delete(collocation_result);
    free(vertical_unit);
    free(bounds_name);
    free(interpolation_buffer);

    return 0;

  error:
    harp_variable_delete(source_grid);
    harp_variable_delete(source_bounds);
    harp_variable_delete(target_grid);
    harp_variable_delete(target_bounds);
    harp_product_delete(match);
    harp_collocation_result_shallow_delete(collocation_result);
    free(vertical_unit);
    free(bounds_name);
    free(interpolation_buffer);

    return -1;
}

/** Regrid the product's variables (from dataset a in the collocation result) to the vertical grids,
 * of matching products in dataset b and smooth the variables specified.
 *
 * \param product Product to regrid.
 * \param vertical_axis The name of the variable to use as a vertical axis (pressure/altitude/etc).
 * \param collocation_result The collocation result used to find matching variables.
 *   The collocation result is assumed to have the appropriate metadata available for all matches (dataset b).
 *
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_product_regrid_vertical_with_collocated_dataset(harp_product *product, const char *vertical_axis,
                                                                     harp_collocation_result *collocation_result)
{
    return harp_product_smooth_vertical(product, 0, NULL, vertical_axis, collocation_result);
}

/**
 * @}
 */
