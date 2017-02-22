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
 * \param pressure_profile Pressure vertical profile [Pa]
 * \param temperature_profile Temperature vertical profile [K]
 * \param molar_mass_air Molar mass of total air [g/mol]
 * \param surface_pressure Surface pressure [Pa]
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
 * \param pressure_profile Pressure vertical profile [Pa]
 * \param temperature_profile Temperature vertical profile [K]
 * \param molar_mass_air Molar mass of total air [g/mol]
 * \param surface_pressure Surface pressure [Pa]
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
 * \param surface_pressure Surface pressure [Pa]
 * \param surface_height Surface height [m]
 * \param latitude Latitude [degree_north]
 * \param pressure_profile variable in which the vertical profile will be stored [Pa]
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
 * \param surface_pressure Surface pressure [Pa]
 * \param surface_height Surface height [m]
 * \param pressure_profile Variable in which the vertical profile will be stored [Pa]
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

int needs_interval_resample(harp_product *product)
{
    int i;

    for (i = 0; i < product->num_variables; i++)
    {
        if (get_profile_resample_type(product->variable[i]) == profile_resample_interval)
        {
            return 1;
        }
    }

    return 0;
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
        long collocated_product_index = pair->product_index_b;
        harp_product_metadata *product_metadata = collocation_result->dataset_b->metadata[collocated_product_index];

        if (!product_metadata)
        {
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "metadata unavailable for collocated product %s",
                           collocation_result->dataset_b->source_product[collocated_product_index]);
            return -1;
        }

        if (product_metadata->dimension[harp_dimension_vertical] > max)
        {
            max = product_metadata->dimension[harp_dimension_vertical];
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

    /* Get the collocation variable from the product */
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

    harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "couldn't locate collocation_index %li in product %s",
                   collocation_index, product->source_product);
    return -1;
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

/* var already needs to be on the same vertical grid as collocated_product
 * num_vertical_elements is the actual vertical profile length of var at time_index_a
 * (and also of the vars in collocated_product at time_index_b)
 */
static int vertical_profile_smooth(harp_variable *var, harp_product *collocated_product, long time_index_a,
                                   long time_index_b, long num_vertical_elements)
{
    harp_dimension_type dim_type[3] = { harp_dimension_time, harp_dimension_vertical, harp_dimension_vertical };
    long max_vertical_elements = collocated_product->dimension[harp_dimension_vertical];
    long num_blocks;
    long k;
    int has_apriori = 0;

    /* owned memory */
    harp_variable *apriori = NULL;
    harp_variable *avk = NULL;
    char *apriori_name = NULL;
    char *avk_name = NULL;
    double *vector = NULL;

    /* get the avk and a priori variables */
    avk_name = malloc(strlen(var->name) + 4 + 1);
    apriori_name = malloc(strlen(var->name) + 8 + 1);
    if (!avk_name || !apriori_name)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)",
                       __FILE__, __LINE__);
        goto error;
    }

    strcpy(apriori_name, var->name);
    strcat(apriori_name, "_apriori");
    strcpy(avk_name, var->name);
    strcat(avk_name, "_avk");

    if (harp_product_get_derived_variable(collocated_product, apriori_name, var->unit, 2, dim_type, &apriori) == 0)
    {
        has_apriori = 1;
    }

    if (harp_product_get_derived_variable(collocated_product, avk_name, "", 3, dim_type, &avk))
    {
        goto error;
    }

    /* allocate memory for the temporary vertical profile vector */
    vector = malloc(num_vertical_elements * sizeof(double));
    if (!vector)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_vertical_elements * sizeof(double), __FILE__, __LINE__);
        goto error;
    }

    /* calculate the number of blocks in this datetime slice of the variable */
    num_blocks = var->num_elements / var->dimension[0] / max_vertical_elements;

    for (k = 0; k < num_blocks; k++)
    {
        long blockoffset = (time_index_a * num_blocks + k) * max_vertical_elements;
        long i, j;

        /* store profile in temporary vector */
        for (i = 0; i < num_vertical_elements; i++)
        {
            vector[i] = var->data.double_data[blockoffset + i];
        }

        /* subtract a priori */
        if (has_apriori)
        {
            for (i = 0; i < num_vertical_elements; i++)
            {
                vector[i] -= apriori->data.double_data[time_index_b * max_vertical_elements + i];
            }
        }

        /* multiply by avk */
        for (i = 0; i < num_vertical_elements; i++)
        {
            long avk_offset = (time_index_b * max_vertical_elements + i) * max_vertical_elements;
            long num_valid = 0;

            var->data.double_data[blockoffset + i] = 0;
            for (j = 0; j < num_vertical_elements; j++)
            {
                if (!harp_isnan(vector[j]))
                {
                    var->data.double_data[blockoffset + i] += avk->data.double_data[avk_offset + j] * vector[j];
                    num_valid++;
                }
            }

            /* add the apriori again */
            if (has_apriori)
            {
                var->data.double_data[blockoffset + i] +=
                    apriori->data.double_data[time_index_b * max_vertical_elements + i];
            }
            else if (num_valid == 0)
            {
                var->data.double_data[blockoffset + i] = harp_nan();
            }
        }
    }

    /* cleanup */
    harp_variable_delete(apriori);
    harp_variable_delete(avk);
    free(apriori_name);
    free(avk_name);
    free(vector);

    return 0;

  error:

    harp_variable_delete(apriori);
    harp_variable_delete(avk);
    free(apriori_name);
    free(avk_name);
    free(vector);

    return -1;
}

static int get_smoothed_total_column(harp_variable *column_variable, harp_variable *partcol_variable,
                                     harp_product *collocated_product, long time_index_a, long time_index_b,
                                     long num_vertical_elements)
{
    harp_dimension_type dim_type[2] = { harp_dimension_time, harp_dimension_vertical };
    long max_vertical_elements;
    long num_blocks;
    long i, k;
    int has_apriori = 0;

    /* owned memory */
    harp_variable *apriori = NULL;
    harp_variable *avk = NULL;
    char *apriori_name = NULL;
    char *avk_name = NULL;

    max_vertical_elements = partcol_variable->dimension[partcol_variable->num_dimensions - 1];

    /* get the avk and a priori variables */
    avk_name = malloc(strlen(column_variable->name) + 4 + 1);
    apriori_name = malloc(strlen(column_variable->name) + 8 + 1);
    if (!avk_name || !apriori_name)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string) (%s:%u)",
                       __FILE__, __LINE__);
        goto error;
    }

    strcpy(apriori_name, column_variable->name);
    strcat(apriori_name, "_apriori");
    strcpy(avk_name, column_variable->name);
    strcat(avk_name, "_avk");

    if (harp_product_get_derived_variable(collocated_product, apriori_name, column_variable->unit, 2, dim_type,
                                          &apriori) == 0)
    {
        has_apriori = 1;
    }

    if (harp_product_get_derived_variable(collocated_product, avk_name, "", 2, dim_type, &avk))
    {
        goto error;
    }

    /* calculate the number of blocks in this datetime slice of the variable */
    num_blocks = column_variable->num_elements / column_variable->dimension[0];

    for (k = 0; k < num_blocks; k++)
    {
        long blockoffset_col = time_index_a * num_blocks + k;
        long blockoffset_prof = blockoffset_col * max_vertical_elements;
        long is_valid = 0;

        /* multiply partial column profile with column averaging kernel */
        column_variable->data.double_data[blockoffset_col] = 0;
        for (i = 0; i < num_vertical_elements; i++)
        {
            if (!harp_isnan(partcol_variable->data.double_data[blockoffset_prof + i]))
            {
                column_variable->data.double_data[blockoffset_col] +=
                    partcol_variable->data.double_data[blockoffset_prof + i] *
                    avk->data.double_data[time_index_b * max_vertical_elements + i];
                is_valid = 1;
            }

            /* add the apriori */
            if (has_apriori)
            {
                column_variable->data.double_data[blockoffset_col] +=
                    (1 - avk->data.double_data[time_index_b * max_vertical_elements + i]) *
                    apriori->data.double_data[time_index_b * max_vertical_elements + i];
                is_valid = 1;
            }
        }
        if (!is_valid)
        {
            column_variable->data.double_data[blockoffset_col] = harp_nan();
        }
    }

    /* cleanup */
    harp_variable_delete(apriori);
    harp_variable_delete(avk);
    free(apriori_name);
    free(avk_name);

    return 0;

  error:

    harp_variable_delete(apriori);
    harp_variable_delete(avk);
    free(apriori_name);
    free(avk_name);

    return -1;
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
 * \param axis_variable Vertical grid to target.
 *
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_product_regrid_vertical_with_axis_variable(harp_product *product, harp_variable *axis_variable)
{
    harp_dimension_type grid_dim_type[2] = { harp_dimension_time, harp_dimension_vertical };
    harp_dimension_type bounds_dim_type[3] = { harp_dimension_time, harp_dimension_vertical,
        harp_dimension_independent
    };
    long num_source_max_vertical_elements;      /* actual elems + NaN padding */
    long num_source_grid_max_vertical_elements;
    long num_target_vertical_elements = axis_variable->dimension[axis_variable->num_dimensions - 1];
    long source_time_dim_length = 0;    /* 0 indicates that we do time-independent regridding */
    int source_grid_num_dims = 1;
    long i;

    /* owned memory */
    harp_variable *vertical_axis = NULL;
    harp_variable *source_grid = NULL;
    harp_variable *source_bounds = NULL;
    harp_variable *target_grid = NULL;
    harp_variable *target_bounds = NULL;
    double *interpolation_buffer = NULL;

    if (harp_variable_copy(axis_variable, &target_grid) != 0)
    {
        goto error;
    }

    /* Derive the source grid (will give doubles because unit is passed) */
    if (harp_product_get_derived_variable(product, target_grid->name, target_grid->unit, 1, &grid_dim_type[1],
                                          &source_grid) != 0)
    {
        /* Failed to derive 1D source grid. Try 2D */
        if (harp_product_get_derived_variable(product, target_grid->name, target_grid->unit, 2, grid_dim_type,
                                              &source_grid) != 0)
        {
            goto error;
        }
        source_grid_num_dims = 2;
        source_time_dim_length = source_grid->dimension[0];
    }
    num_source_grid_max_vertical_elements = source_grid->dimension[source_grid->num_dimensions - 1];
    num_source_max_vertical_elements = num_source_grid_max_vertical_elements;

    /* derive bounds variables if necessary for resampling */
    if (needs_interval_resample(product))
    {
        harp_product *target_grid_product = NULL;
        char *bounds_name = NULL;

        /* derive the name of the bounds variable for the vertical axis */
        bounds_name = malloc(strlen(target_grid->name) + 7 + 1);
        if (!bounds_name)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string)"
                           " (%s:%u)", __FILE__, __LINE__);
            goto error;
        }
        strcpy(bounds_name, target_grid->name);
        strcat(bounds_name, "_bounds");

        /* Create a dummy product to allow deriving the bounds for the target grid */
        if (harp_product_new(&target_grid_product) != 0)
        {
            free(bounds_name);
            goto error;
        }
        if (harp_product_add_variable(target_grid_product, target_grid) != 0)
        {
            harp_product_delete(target_grid_product);
            free(bounds_name);
            goto error;
        }
        if (harp_product_get_derived_variable(target_grid_product, bounds_name, target_grid->unit, 2,
                                              &bounds_dim_type[1], &target_bounds) != 0)
        {
            harp_product_delete(target_grid_product);
            target_grid = NULL;
            free(bounds_name);
            goto error;
        }
        if (harp_product_detach_variable(target_grid_product, target_grid) != 0)
        {
            harp_product_delete(target_grid_product);
            target_grid = NULL;
            free(bounds_name);
            goto error;
        }
        harp_product_delete(target_grid_product);

        if (source_grid_num_dims == 1)
        {
            if (harp_product_get_derived_variable(product, bounds_name, target_grid->unit, 2, &bounds_dim_type[1],
                                                  &source_bounds) != 0)
            {
                free(bounds_name);
                goto error;
            }
        }
        else
        {
            if (harp_product_get_derived_variable(product, bounds_name, target_grid->unit, 3, bounds_dim_type,
                                                  &source_bounds) != 0)
            {
                free(bounds_name);
                goto error;
            }
        }
        free(bounds_name);
    }

    /* remove axis variable if it exists (since we don't want to interpolate it) */
    if (harp_product_has_variable(product, target_grid->name))
    {
        if (harp_product_remove_variable_by_name(product, target_grid->name) != 0)
        {
            goto error;
        }
    }

    /* Remove variables that can't be resampled */
    if (product_filter_resamplable_variables(product) != 0)
    {
        goto error;
    }

    if (source_grid_num_dims > 1)
    {
        /* Expand time independent vertical profiles */
        if (expand_time_independent_vertical_variables(product) != 0)
        {
            goto error;
        }
    }

    /* Use loglin interpolation if pressure grid */
    if (strcmp(target_grid->name, "pressure") == 0)
    {
        for (i = 0; i < source_grid->num_elements; i++)
        {
            source_grid->data.double_data[i] = log(source_grid->data.double_data[i]);
        }
        for (i = 0; i < target_grid->num_elements; i++)
        {
            target_grid->data.double_data[i] = log(target_grid->data.double_data[i]);
        }
        if (source_bounds != NULL)
        {
            for (i = 0; i < source_bounds->num_elements; i++)
            {
                source_bounds->data.double_data[i] = log(source_bounds->data.double_data[i]);
            }
        }
        if (target_bounds != NULL)
        {
            for (i = 0; i < target_bounds->num_elements; i++)
            {
                target_bounds->data.double_data[i] = log(target_bounds->data.double_data[i]);
            }
        }
    }

    /* Resize the vertical dimension in the target product to make room for the resampled data */
    if (num_target_vertical_elements > num_source_max_vertical_elements)
    {
        if (resize_vertical_dimension(product, num_target_vertical_elements) != 0)
        {
            goto error;
        }
        num_source_max_vertical_elements = num_target_vertical_elements;
    }

    /* allocate the buffer for the interpolation */
    interpolation_buffer = (double *)malloc(num_target_vertical_elements * (size_t)sizeof(double));
    if (interpolation_buffer == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY,
                       "out of memory (could not allocate %lu bytes) (%s:%u)",
                       num_target_vertical_elements * (size_t)sizeof(double), __FILE__, __LINE__);
        goto error;
    }

    /* Resample all variables if we know how */
    for (i = product->num_variables - 1; i >= 0; i--)
    {
        harp_variable *variable = product->variable[i];
        profile_resample_type variable_type;
        long num_profiles = variable->num_elements / num_source_max_vertical_elements;
        long num_profiles_per_time = num_profiles;
        long num_source_vertical_elements = 0;
        long time_index;
        long j;

        /* Check if we can resample this kind of variable */
        variable_type = get_profile_resample_type(variable);

        if (variable_type == profile_resample_skip)
        {
            continue;
        }

        /* if variable is time dependent keep track of number of profiles per time */
        if (variable->dimension_type[0] == harp_dimension_time)
        {
            num_profiles_per_time /= variable->dimension[0];
        }

        /* Ensure that the variable data consists of doubles */
        if (variable->data_type != harp_type_double && harp_variable_convert_data_type(variable, harp_type_double) != 0)
        {
            goto error;
        }

        /* time independent variables with a time-dependent source grid are time-extended */
        if (variable->dimension_type[0] != harp_dimension_time && source_grid->dimension[0] == harp_dimension_time)
        {
            harp_variable_add_dimension(variable, 0, harp_dimension_time, source_time_dim_length);
        }

        /* Interpolate the data of the variable over the vertical axis */
        time_index = -1;
        for (j = 0; j < num_profiles; j++)
        {
            int l;

            /* keep track of time for time-dependent vertical grids */
            if (j % num_profiles_per_time == 0)
            {
                if (j == 0 || source_grid_num_dims == 2)
                {
                    time_index++;
                }
                /* find the source grid lengths */
                num_source_vertical_elements =
                    get_unpadded_vector_length
                    (&source_grid->data.double_data[time_index * num_source_grid_max_vertical_elements],
                     num_source_grid_max_vertical_elements);
            }

            if (variable_type == profile_resample_linear)
            {
                harp_interpolate_array_linear
                    (num_source_vertical_elements,
                     &source_grid->data.double_data[time_index * num_source_grid_max_vertical_elements],
                     &variable->data.double_data[j * num_source_max_vertical_elements], num_target_vertical_elements,
                     target_grid->data.double_data, 0, interpolation_buffer);
            }
            else if (variable_type == profile_resample_interval)
            {
                harp_interval_interpolate_array_linear
                    (num_source_vertical_elements,
                     &source_bounds->data.double_data[time_index * num_source_grid_max_vertical_elements * 2],
                     &variable->data.double_data[j * num_source_max_vertical_elements], num_target_vertical_elements,
                     target_bounds->data.double_data, interpolation_buffer);
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
                variable->data.double_data[j * num_source_max_vertical_elements + l] = interpolation_buffer[l];
            }
        }
    }

    /* Resize the vertical dimension in the target product to minimal size */
    if (num_target_vertical_elements < num_source_max_vertical_elements)
    {
        if (resize_vertical_dimension(product, num_target_vertical_elements) != 0)
        {
            goto error;
        }
    }

    /* ensure consistent axis variable in product */
    if (harp_variable_copy(axis_variable, &vertical_axis) != 0)
    {
        goto error;
    }
    if (harp_product_add_variable(product, vertical_axis) != 0)
    {
        goto error;
    }

    /* cleanup */
    harp_variable_delete(source_grid);
    harp_variable_delete(source_bounds);
    harp_variable_delete(target_grid);
    harp_variable_delete(target_bounds);
    free(interpolation_buffer);

    return 0;

  error:
    harp_variable_delete(source_grid);
    harp_variable_delete(source_bounds);
    harp_variable_delete(target_grid);
    harp_variable_delete(target_bounds);
    free(interpolation_buffer);

    return -1;
}

/** Smooth the product's variables (from dataset a in the collocation result) using the vertical grids,
 * avks and a apriori of collocated products in dataset b and smooth the variables specified.
 *
 * \param product Product to smooth.
 * \param num_smooth_variables length of smooth_variables.
 * \param smooth_variables The names of the variables to smooth.
 * \param vertical_axis The name of the variable to use as a vertical axis (pressure/altitude/etc).
 * \param vertical_unit The unit in which the vertical_axis will be brought for the regridding.
 * \param collocation_result The collocation result used to locate the matching vertical
 *   grids/avks/apriori.
 *   The collocation result is assumed to have the appropriate metadata available for all matches (dataset b).
 *
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_product_smooth_vertical(harp_product *product, int num_smooth_variables,
                                             const char **smooth_variables, const char *vertical_axis,
                                             const char *vertical_unit,
                                             const harp_collocation_result *collocation_result)
{
    long source_vertical_dim;   /* actual elems + NaN padding */
    long source_grid_vertical_dim;
    long max_target_vertical_dim;
    harp_variable *source_collocation_index = NULL;
    harp_dimension_type grid_dim_type[2] = { harp_dimension_time, harp_dimension_vertical };
    harp_dimension_type bounds_dim_type[3] = { harp_dimension_time, harp_dimension_vertical,
        harp_dimension_independent
    };
    long i;
    int pair_id;

    /* owned memory */
    harp_variable *source_grid = NULL;
    harp_variable *source_bounds = NULL;
    harp_product *collocated_product = NULL;
    harp_variable *target_grid = NULL;
    harp_variable *target_bounds = NULL;
    harp_collocation_result *filtered_collocation_result = NULL;
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

    /* raise warnings for any variables that were not present */
    for (i = 0; i < num_smooth_variables; i++)
    {
        if (!harp_product_has_variable(product, smooth_variables[i]))
        {
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "product has no variable named '%s'", smooth_variables[i]);
            goto error;
        }
    }

    /* copy the collocation result for filtering */
    if (harp_collocation_result_shallow_copy(collocation_result, &filtered_collocation_result) != 0)
    {
        goto error;
    }

    /* Get the source product's collocation index variable */
    if (harp_product_get_variable_by_name(product, "collocation_index", &source_collocation_index) != 0)
    {
        goto error;
    }

    /* Reduce the collocation result to only pairs that include the source product */
    if (harp_collocation_result_filter_for_collocation_indices(filtered_collocation_result,
                                                               source_collocation_index->num_elements,
                                                               source_collocation_index->data.int32_data) != 0)
    {
        goto error;
    }

    /* We sort by dataset b so we only have to read each file from dataset b once */
    if (harp_collocation_result_sort_by_b(filtered_collocation_result) != 0)
    {
        goto error;
    }

    /* Determine the maximum vertical dimensions size of database b */
    if (get_maximum_vertical_dimension(filtered_collocation_result, &max_target_vertical_dim) != 0)
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

    /* derive bounds variables if necessary for resampling */
    if (needs_interval_resample(product))
    {
        if (harp_product_get_derived_variable(product, bounds_name, vertical_unit, 3, bounds_dim_type, &source_bounds)
            != 0)
        {
            goto error;
        }
    }

    /* Remove variables that can't be resampled */
    if (product_filter_resamplable_variables(product) != 0)
    {
        goto error;
    }

    /* Use loglin interpolation if pressure grid */
    if (strcmp(source_grid->name, "pressure") == 0)
    {
        for (i = 0; i < source_grid->num_elements; i++)
        {
            source_grid->data.double_data[i] = log(source_grid->data.double_data[i]);
        }
        if (needs_interval_resample(product))
        {
            for (i = 0; i < source_bounds->num_elements; i++)
            {
                source_bounds->data.double_data[i] = log(source_bounds->data.double_data[i]);
            }
        }
    }

    /* Save the length of the original vertical dimension */
    source_grid_vertical_dim = product->dimension[harp_dimension_vertical];

    /* Resize the vertical dimension in the target product to make room for the resampled data */
    if (max_target_vertical_dim > product->dimension[harp_dimension_vertical])
    {
        if (resize_vertical_dimension(product, max_target_vertical_dim) != 0)
        {
            goto error;
        }
    }
    source_vertical_dim = product->dimension[harp_dimension_vertical];

    /* allocate the buffer for the interpolation */
    interpolation_buffer = (double *)malloc(max_target_vertical_dim * (size_t)sizeof(double));
    if (interpolation_buffer == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       max_target_vertical_dim * (size_t)sizeof(double), __FILE__, __LINE__);
        goto error;
    }

    for (pair_id = 0; pair_id < filtered_collocation_result->num_pairs; pair_id++)
    {
        harp_collocation_pair *pair = NULL;
        harp_product_metadata *product_metadata;
        long num_target_vertical_elements;
        long num_source_vertical_elements;
        long target_vertical_dim;
        long time_index_a;
        long time_index_b;
        int i, j;

        pair = filtered_collocation_result->pair[pair_id];
        if (get_time_index_by_collocation_index(product, pair->collocation_index, &time_index_a) != 0)
        {
            goto error;
        }

        if (strcmp(filtered_collocation_result->dataset_a->source_product[pair->product_index_a],
                   product->source_product) != 0)
        {
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "product reference mismatch for collocation index %d in "
                           "collocation result file", pair->collocation_index);
            goto error;
        }

        /* Get metadata of the collocated product */
        product_metadata = filtered_collocation_result->dataset_b->metadata[pair->product_index_b];
        if (product_metadata == NULL)
        {
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "missing product metadata for product %s",
                           filtered_collocation_result->dataset_b->source_product[pair->product_index_b]);
            goto error;
        }

        /* load the collocated product if necessary */
        if (collocated_product == NULL || strcmp(collocated_product->source_product, product_metadata->source_product)
            != 0)
        {
            /* cleanup previous collocated product */
            if (collocated_product != NULL)
            {
                harp_product_delete(collocated_product);
                collocated_product = NULL;
            }

            /* import new product */
            harp_import(product_metadata->filename, &collocated_product);
            if (!collocated_product)
            {
                harp_set_error(HARP_ERROR_IMPORT, "could not import file %s", product_metadata->filename);
                goto error;
            }

            /* Derive the target grid */
            harp_variable_delete(target_grid);
            if (harp_product_get_derived_variable(collocated_product, vertical_axis, vertical_unit, 2, grid_dim_type,
                                                  &target_grid) != 0)
            {
                goto error;
            }
            /* Use loglin interpolation if pressure grid */
            if (strcmp(target_grid->name, "pressure") == 0)
            {
                for (i = 0; i < target_grid->num_elements; i++)
                {
                    target_grid->data.double_data[i] = log(target_grid->data.double_data[i]);
                }
            }

            /* Cleanup the target bounds, they will get loaded again when necessary */
            harp_variable_delete(target_bounds);
            target_bounds = NULL;
        }

        if (get_time_index_by_collocation_index(collocated_product, pair->collocation_index, &time_index_b) != 0)
        {
            goto error;
        }

        /* find the source grid lengths */
        num_source_vertical_elements =
            get_unpadded_vector_length(&source_grid->data.double_data[time_index_a * source_grid_vertical_dim],
                                       source_grid_vertical_dim);


        /* figure out the target offset to use: i.e. the number of vertical profile elements that fall
         * below the first source profile elements
         */
        target_vertical_dim = target_grid->dimension[1];

        /* find the target grid length */
        num_target_vertical_elements =
            get_unpadded_vector_length(&target_grid->data.double_data[time_index_b * target_vertical_dim],
                                       target_vertical_dim);

        /* Resample & smooth variables */
        for (j = product->num_variables - 1; j >= 0; j--)
        {
            harp_variable *var = product->variable[j];
            long num_blocks;
            int k;

            /* Skip variables that don't need resampling */
            profile_resample_type var_type = get_profile_resample_type(var);

            if (var_type == profile_resample_skip)
            {
                continue;
            }

            /* do not interpolate the grid variable; this might produce nans at the bottom */
            if (strcmp(var->name, vertical_axis) == 0)
            {
                /* Ensure that the variable data to resample consists of doubles */
                if (var->data_type != harp_type_double && harp_variable_convert_data_type(var, harp_type_double) != 0)
                {
                    goto error;
                }

                /* copy the time slice to the target variable */
                memcpy(&var->data.double_data[time_index_a * source_vertical_dim],
                       &target_grid->data.double_data[time_index_b * target_vertical_dim],
                       target_vertical_dim * sizeof(double));

                /* make sure the data is using the unit associated with the variable */
                if (harp_convert_unit(vertical_unit, var->unit, target_vertical_dim,
                                      &var->data.double_data[time_index_a * source_vertical_dim]) != 0)
                {
                    goto error;
                }

                /* nan fill */
                for (k = target_vertical_dim; k < source_vertical_dim; k++)
                {
                    var->data.double_data[time_index_a * source_vertical_dim + k] = harp_nan();
                }
            }
            else
            {
                /* derive bounds variables if necessary for resampling */
                if (var_type == profile_resample_interval)
                {
                    if (target_bounds == NULL)
                    {
                        if (harp_product_get_derived_variable(collocated_product, bounds_name, vertical_unit, 3,
                                                              bounds_dim_type, &target_bounds) != 0)
                        {
                            goto error;
                        }
                        if (strcmp(target_grid->name, "pressure") == 0)
                        {
                            for (i = 0; i < target_bounds->num_elements; i++)
                            {
                                target_bounds->data.double_data[i] = log(target_bounds->data.double_data[i]);
                            }
                        }
                    }
                }

                /* Ensure that the variable data to resample consists of doubles */
                if (var->data_type != harp_type_double && harp_variable_convert_data_type(var, harp_type_double) != 0)
                {
                    goto error;
                }

                /* Interpolate variable data */
                num_blocks = var->num_elements / var->dimension[0] / source_vertical_dim;
                for (k = 0; k < num_blocks; k++)
                {
                    long blockoffset = (time_index_a * num_blocks + k) * source_vertical_dim;
                    int l;

                    if (var_type == profile_resample_linear)
                    {
                        harp_interpolate_array_linear
                            (num_source_vertical_elements,
                             &source_grid->data.double_data[time_index_a * source_grid_vertical_dim],
                             &var->data.double_data[blockoffset], num_target_vertical_elements,
                             &target_grid->data.double_data[time_index_b * target_vertical_dim], 0,
                             interpolation_buffer);
                    }
                    else if (var_type == profile_resample_interval)
                    {
                        harp_interval_interpolate_array_linear
                            (num_source_vertical_elements,
                             &source_bounds->data.double_data[time_index_a * source_grid_vertical_dim * 2],
                             &var->data.double_data[blockoffset], num_target_vertical_elements,
                             &target_bounds->data.double_data[time_index_b * target_vertical_dim * 2],
                             interpolation_buffer);
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
                        var->data.double_data[blockoffset + l] = interpolation_buffer[l];
                    }
                    for (l = num_target_vertical_elements; l < source_vertical_dim; l++)
                    {
                        var->data.double_data[blockoffset + l] = harp_nan();
                    }
                }
            }

            /* Smooth variable if it's index appears in smooth_variables */
            for (k = 0; k < num_smooth_variables; k++)
            {
                if (strcmp(smooth_variables[k], var->name) == 0)
                {
                    if (vertical_profile_smooth(var, collocated_product, time_index_a, time_index_b,
                                                num_target_vertical_elements) != 0)
                    {
                        goto error;
                    }
                }
            }
        }
    }

    /* Resize the vertical dimension in the target product to minimal size */
    if (max_target_vertical_dim < product->dimension[harp_dimension_vertical])
    {
        if (resize_vertical_dimension(product, max_target_vertical_dim) != 0)
        {
            goto error;
        }
    }

    /* cleanup */
    harp_variable_delete(source_grid);
    harp_variable_delete(source_bounds);
    harp_variable_delete(target_grid);
    harp_variable_delete(target_bounds);
    harp_product_delete(collocated_product);
    harp_collocation_result_shallow_delete(filtered_collocation_result);
    free(bounds_name);
    free(interpolation_buffer);

    return 0;

  error:
    harp_variable_delete(source_grid);
    harp_variable_delete(source_bounds);
    harp_variable_delete(target_grid);
    harp_variable_delete(target_bounds);
    harp_product_delete(collocated_product);
    harp_collocation_result_shallow_delete(filtered_collocation_result);
    free(bounds_name);
    free(interpolation_buffer);

    return -1;
}

/** Regrid the product's variables (from dataset a in the collocation result) to the vertical grids,
 * of collocated products in dataset b and smooth the variables specified.
 *
 * \param product Product to regrid.
 * \param vertical_axis The name of the variable to use as a vertical axis (pressure/altitude/etc).
 * \param vertical_unit The unit in which the vertical_axis will be brought for the regridding.
 * \param collocation_result The collocation result used to find matching variables.
 *   The collocation result is assumed to have the appropriate metadata available for all matches (dataset b).
 *
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_product_regrid_vertical_with_collocated_dataset(harp_product *product, const char *vertical_axis,
                                                                     const char *vertical_unit,
                                                                     harp_collocation_result *collocation_result)
{
    return harp_product_smooth_vertical(product, 0, NULL, vertical_axis, vertical_unit, collocation_result);
}

/** Derive a vertical column smoothed with column averaging kernel and a-priori from collocated products in dataset b
 *
 * \param product Product to regrid.
 * \param name Name of the variable that should be created.
 * \param unit Unit (optional) of the variable that should be created.
 * \param num_dimensions Number of dimensions of the variable that should be created.
 * \param dimension_type Type of dimension for each of the dimensions of the variable that should be created.
 * \param vertical_axis The name of the variable to use as a vertical axis (pressure/altitude/etc).
 * \param vertical_unit The unit in which the vertical_axis will be brought for the regridding.
 * \param collocation_result The collocation result used to find matching variables.
 *   The collocation result is assumed to have the appropriate metadata available for all matches (dataset b).
 * \param variable Pointer to the C variable where the derived HARP variable will be stored.
 *
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_product_get_smoothed_column_using_collocated_dataset
    (harp_product *product, const char *name, const char *unit, int num_dimensions,
     const harp_dimension_type *dimension_type, const char *vertical_axis, const char *vertical_unit,
     harp_collocation_result *collocation_result, harp_variable **variable)
{
    harp_dimension_type partcol_dimension_type[HARP_NUM_DIM_TYPES];
    long source_vertical_dim;
    long source_grid_vertical_dim;
    long max_target_vertical_dim;
    harp_variable *source_collocation_index = NULL;

    harp_dimension_type bounds_dim_type[3] = { harp_dimension_time, harp_dimension_vertical,
        harp_dimension_independent
    };
    long i;
    int pair_id;

    /* owned memory */
    harp_variable *column_variable = NULL;
    harp_variable *partcol_variable = NULL;
    harp_variable *source_bounds = NULL;
    harp_product *collocated_product = NULL;
    harp_variable *target_bounds = NULL;
    harp_collocation_result *filtered_collocation_result = NULL;
    char *bounds_name = NULL;
    double *interpolation_buffer = NULL;

    if (num_dimensions == 0 || dimension_type[0] != harp_dimension_time)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT,
                       "first dimension of requested smoothed vertical column should be the time dimension");
        return -1;
    }
    if (num_dimensions >= HARP_NUM_DIM_TYPES)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "number of dimensions (%d) too large (%s:%u)", num_dimensions,
                       __FILE__, __LINE__);
        return -1;
    }
    if (product->dimension[harp_dimension_vertical] == 0)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "product has no vertical dimension");
        return -1;
    }
    source_grid_vertical_dim = product->dimension[harp_dimension_vertical];
    source_vertical_dim = source_grid_vertical_dim;

    /* derive the name of the bounds variable for the vertical axis */
    bounds_name = malloc(strlen(vertical_axis) + 7 + 1);
    if (bounds_name == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not duplicate string)"
                       " (%s:%u)", __FILE__, __LINE__);
        goto error;
    }
    strcpy(bounds_name, vertical_axis);
    strcat(bounds_name, "_bounds");

    /* determine partial column profile dimensions */
    for (i = 0; i < num_dimensions; i++)
    {
        partcol_dimension_type[i] = dimension_type[i];
    }
    partcol_dimension_type[num_dimensions] = harp_dimension_vertical;
    num_dimensions++;

    /* retrieve partial column profile from source product */
    if (harp_product_get_derived_variable(product, name, unit, num_dimensions, partcol_dimension_type,
                                          &partcol_variable) != 0)
    {
        goto error;
    }

    if (harp_variable_new(name, harp_type_double, num_dimensions - 1, dimension_type, partcol_variable->dimension,
                          &column_variable) != 0)
    {
        goto error;
    }

    /* copy the collocation result for filtering */
    if (harp_collocation_result_shallow_copy(collocation_result, &filtered_collocation_result) != 0)
    {
        goto error;
    }

    /* Get the source product's collocation index variable */
    if (harp_product_get_variable_by_name(product, "collocation_index", &source_collocation_index) != 0)
    {
        goto error;
    }

    /* Reduce the collocation result to only pairs that include the source product */
    if (harp_collocation_result_filter_for_collocation_indices(filtered_collocation_result,
                                                               source_collocation_index->num_elements,
                                                               source_collocation_index->data.int32_data) != 0)
    {
        goto error;
    }

    /* We sort by dataset b so we only have to read each file from dataset b once */
    if (harp_collocation_result_sort_by_b(filtered_collocation_result) != 0)
    {
        goto error;
    }

    /* Determine the maximum vertical dimensions size of database b */
    if (get_maximum_vertical_dimension(filtered_collocation_result, &max_target_vertical_dim) != 0)
    {
        goto error;
    }

    /* Derive the source grid */
    if (harp_product_get_derived_variable(product, bounds_name, vertical_unit, 3, bounds_dim_type, &source_bounds) != 0)
    {
        goto error;
    }

    /* Use loglin interpolation if pressure grid */
    if (strcmp(vertical_axis, "pressure") == 0)
    {
        for (i = 0; i < source_bounds->num_elements; i++)
        {
            source_bounds->data.double_data[i] = log(source_bounds->data.double_data[i]);
        }
    }

    /* Resize the partial column profile to make room for the resampled data */
    if (max_target_vertical_dim > source_vertical_dim)
    {
        if (harp_variable_resize_dimension(partcol_variable, num_dimensions - 1, max_target_vertical_dim) != 0)
        {
            goto error;
        }
        source_vertical_dim = max_target_vertical_dim;
    }

    /* allocate the buffer for the interpolation */
    interpolation_buffer = (double *)malloc(max_target_vertical_dim * (size_t)sizeof(double));
    if (interpolation_buffer == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       max_target_vertical_dim * (size_t)sizeof(double), __FILE__, __LINE__);
        goto error;
    }

    for (pair_id = 0; pair_id < filtered_collocation_result->num_pairs; pair_id++)
    {
        harp_collocation_pair *pair = NULL;
        harp_product_metadata *product_metadata;
        long num_target_vertical_elements;
        long num_source_vertical_elements;
        long target_vertical_dim;
        long time_index_a;
        long time_index_b;
        long num_blocks;
        int i, j;

        pair = filtered_collocation_result->pair[pair_id];
        if (get_time_index_by_collocation_index(product, pair->collocation_index, &time_index_a) != 0)
        {
            goto error;
        }

        if (strcmp(filtered_collocation_result->dataset_a->source_product[pair->product_index_a],
                   product->source_product) != 0)
        {
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "product reference mismatch for collocation index %d in "
                           "collocation result file", pair->collocation_index);
            goto error;
        }

        /* Get metadata of the collocated product */
        product_metadata = filtered_collocation_result->dataset_b->metadata[pair->product_index_b];
        if (product_metadata == NULL)
        {
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "missing product metadata for product %s",
                           filtered_collocation_result->dataset_b->source_product[pair->product_index_b]);
            goto error;
        }

        /* load the collocated product if necessary */
        if (collocated_product == NULL || strcmp(collocated_product->source_product, product_metadata->source_product)
            != 0)
        {
            /* cleanup previous collocated product */
            if (collocated_product != NULL)
            {
                harp_product_delete(collocated_product);
                collocated_product = NULL;
            }

            /* import new product */
            harp_import(product_metadata->filename, &collocated_product);
            if (!collocated_product)
            {
                harp_set_error(HARP_ERROR_IMPORT, "could not import file %s", product_metadata->filename);
                goto error;
            }

            /* Derive the target grid */
            harp_variable_delete(target_bounds);
            if (harp_product_get_derived_variable(collocated_product, bounds_name, vertical_unit, 3, bounds_dim_type,
                                                  &target_bounds) != 0)
            {
                goto error;
            }
            /* Use loglin interpolation if pressure grid */
            if (strcmp(vertical_axis, "pressure") == 0)
            {
                for (i = 0; i < target_bounds->num_elements; i++)
                {
                    target_bounds->data.double_data[i] = log(target_bounds->data.double_data[i]);
                }
            }
        }

        if (get_time_index_by_collocation_index(collocated_product, pair->collocation_index, &time_index_b) != 0)
        {
            goto error;
        }

        /* find the source grid lengths */
        num_source_vertical_elements =
            get_unpadded_vector_length(&source_bounds->data.double_data[time_index_a * source_grid_vertical_dim * 2],
                                       source_grid_vertical_dim * 2) / 2;


        /* find the target grid length */
        target_vertical_dim = target_bounds->dimension[1];
        num_target_vertical_elements =
            get_unpadded_vector_length(&target_bounds->data.double_data[time_index_b * target_vertical_dim * 2],
                                       target_vertical_dim * 2) / 2;

        /* Resample partial column */
        num_blocks = partcol_variable->num_elements / partcol_variable->dimension[0] / source_vertical_dim;
        for (i = 0; i < num_blocks; i++)
        {
            long blockoffset = (time_index_a * num_blocks + i) * source_vertical_dim;

            harp_interval_interpolate_array_linear
                (num_source_vertical_elements,
                 &source_bounds->data.double_data[time_index_a * source_grid_vertical_dim * 2],
                 &partcol_variable->data.double_data[blockoffset], num_target_vertical_elements,
                 &target_bounds->data.double_data[time_index_b * target_vertical_dim * 2], interpolation_buffer);

            /* copy the buffer to the target var */
            for (j = 0; j < num_target_vertical_elements; j++)
            {
                partcol_variable->data.double_data[blockoffset + j] = interpolation_buffer[j];
            }
            for (j = num_target_vertical_elements; j < source_vertical_dim; j++)
            {
                partcol_variable->data.double_data[blockoffset + j] = harp_nan();
            }
        }

        if (get_smoothed_total_column(column_variable, partcol_variable, collocated_product, time_index_a,
                                      time_index_b, num_target_vertical_elements) != 0)
        {
            goto error;
        }
    }

    /* regrid partial column profile to target grid */
    /* apply column averaging kernel + a-priori to regridded partial column profile */

    *variable = column_variable;

    /* cleanup */
    harp_variable_delete(partcol_variable);
    harp_variable_delete(source_bounds);
    harp_variable_delete(target_bounds);
    harp_product_delete(collocated_product);
    harp_collocation_result_shallow_delete(filtered_collocation_result);
    free(bounds_name);
    free(interpolation_buffer);

    return 0;

  error:
    harp_variable_delete(column_variable);
    harp_variable_delete(partcol_variable);
    harp_variable_delete(source_bounds);
    harp_variable_delete(target_bounds);
    harp_product_delete(collocated_product);
    harp_collocation_result_shallow_delete(filtered_collocation_result);
    free(bounds_name);
    free(interpolation_buffer);

    return -1;
}

/**
 * @}
 */
