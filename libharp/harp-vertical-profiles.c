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
#include "harp-filter-collocation.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define MAX_NAME_LENGTH 128

typedef enum profile_resample_type_enum
{
    profile_resample_skip,
    profile_resample_remove,
    profile_resample_linear,
    profile_resample_log,
    profile_resample_interval
} profile_resample_type;

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

/** \addtogroup harp_variable
 * @{
 */

/** Vertically smooth the variable using the given averaging kernel and a apriori.
 * The variable already needs to be on the same vertical grid as that of the averaging kernel (and a priori).
 * The apriori is optional. If provided, the apriori is first subtracted from the variable, then the smoothing is
 * performed, and finally the apriori is added again.
 * The averaging kernel needs to have dimensions {time,vertical,vertical} and the apriori {time,vertical}.
 * The variable to be smoothed needs to have dimensions {time, ..., vertical} (i.e. first dimension must be time and
 * the last the vertical dimension; number of dimensions needs to be 2 or higher).
 * The vertical axis variable is optional and, if provided, needs to have dimensions {time,vertical}.
 * The vertical axis variable will be used to determine the valid number of vertical elements per profile.
 * \param variable Variable to which the averaging kernel (and apriori) should be applied.
 * \param vertical_axis The variable containing the time dependent vertical grid (optional).
 * \param averaging_kernel The variable containing the averaging kernel.
 * \param apriori The variable containing the apriori (optional).
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_variable_smooth_vertical(harp_variable *variable, harp_variable *vertical_axis,
                                              harp_variable *averaging_kernel, harp_variable *apriori)
{
    double *vector = NULL;
    long max_vertical_elements;
    long num_blocks;
    long k, l;

    if (variable == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variable is NULL (%s:%u)", __FILE__, __LINE__);
        return -1;
    }
    if (averaging_kernel == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "avk is NULL (%s:%u)", __FILE__, __LINE__);
        return -1;
    }
    if (variable->num_dimensions < 2 || variable->dimension_type[0] != harp_dimension_time ||
        variable->dimension_type[variable->num_dimensions - 1] != harp_dimension_vertical)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variable should have dimensions {time,...,vertical}");
        return -1;
    }
    if (averaging_kernel->num_dimensions != 3 || averaging_kernel->dimension_type[0] != harp_dimension_time ||
        averaging_kernel->dimension_type[1] != harp_dimension_vertical ||
        averaging_kernel->dimension_type[2] != harp_dimension_vertical)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "averaging kernel should have dimensions {time,vertical,vertical}");
        return -1;
    }
    if (averaging_kernel->dimension[1] != averaging_kernel->dimension[2])
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "vertical dimensions of averaging kernel do not match");
        return -1;
    }
    if (variable->dimension[0] != averaging_kernel->dimension[0] ||
        variable->dimension[variable->num_dimensions - 1] != averaging_kernel->dimension[1])
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "variable and avk have inconsistent dimensions");
        return -1;
    }
    max_vertical_elements = averaging_kernel->dimension[1];

    if (apriori != NULL)
    {
        if (apriori->num_dimensions != 2 || apriori->dimension_type[0] != harp_dimension_time ||
            apriori->dimension_type[1] != harp_dimension_vertical)
        {
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "apriori should have dimensions {time,vertical}");
            return -1;
        }
        if (apriori->dimension[0] != averaging_kernel->dimension[0] ||
            apriori->dimension[1] != averaging_kernel->dimension[1])
        {
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "apriori and avk have inconsistent dimensions");
            return -1;
        }
    }

    if (vertical_axis != NULL)
    {
        if (vertical_axis->num_dimensions != 2 || vertical_axis->dimension_type[0] != harp_dimension_time ||
            vertical_axis->dimension_type[1] != harp_dimension_vertical)
        {
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "axis variable should have dimensions {time,vertical}");
            return -1;
        }
        if (vertical_axis->dimension[0] != averaging_kernel->dimension[0] ||
            vertical_axis->dimension[1] != averaging_kernel->dimension[1])
        {
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "axis variable and avk have inconsistent dimensions");
            return -1;
        }
    }

    /* allocate memory for the temporary vertical profile vector */
    vector = malloc(max_vertical_elements * sizeof(double));
    if (!vector)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       max_vertical_elements * sizeof(double), __FILE__, __LINE__);
        return -1;
    }

    /* calculate the number of blocks in this datetime slice of the variable */
    num_blocks = variable->num_elements / variable->dimension[0] / max_vertical_elements;

    for (k = 0; k < variable->dimension[0]; k++)
    {
        long num_vertical_elements = max_vertical_elements;

        if (vertical_axis != NULL)
        {
            num_vertical_elements =
                get_unpadded_vector_length(&vertical_axis->data.double_data[k * max_vertical_elements],
                                           max_vertical_elements);
        }

        for (l = 0; l < num_blocks; l++)
        {
            long blockoffset = (k * num_blocks + l) * max_vertical_elements;
            long i, j;

            /* store profile in temporary vector */
            for (i = 0; i < num_vertical_elements; i++)
            {
                vector[i] = variable->data.double_data[blockoffset + i];
            }

            /* subtract a priori */
            if (apriori != NULL)
            {
                for (i = 0; i < num_vertical_elements; i++)
                {
                    vector[i] -= apriori->data.double_data[k * max_vertical_elements + i];
                }
            }

            /* multiply by avk */
            for (i = 0; i < num_vertical_elements; i++)
            {
                long avk_offset = (k * max_vertical_elements + i) * max_vertical_elements;
                long num_valid = 0;

                variable->data.double_data[blockoffset + i] = 0;
                for (j = 0; j < num_vertical_elements; j++)
                {
                    if (!harp_isnan(vector[j]))
                    {
                        variable->data.double_data[blockoffset + i] +=
                            averaging_kernel->data.double_data[avk_offset + j] * vector[j];
                        num_valid++;
                    }
                }

                /* add the apriori again */
                if (apriori != NULL)
                {
                    variable->data.double_data[blockoffset + i] +=
                        apriori->data.double_data[k * max_vertical_elements + i];
                }
                else if (num_valid == 0)
                {
                    variable->data.double_data[blockoffset + i] = harp_nan();
                }
            }
        }
    }

    free(vector);

    return 0;
}

/**
 * @}
 */

/** \addtogroup harp_product
 * @{
 */

/** Smooth the product's variables using the vertical grids, avks and a apriori of the collocated product.
 *
 * The product is first fully regridded (using the vertical dimension) to the vertical grid of the averaging kernel
 * (and apriori). Then, the given list of variables is smoothed using the list of AVKs and apriori variables.
 *
 * \param product Product to smooth.
 * \param num_smooth_variables length of smooth_variables.
 * \param smooth_variables The names of the variables to smooth.
 * \param vertical_axis The name of the variable to use as a vertical axis (pressure/altitude/etc).
 * \param vertical_unit The unit in which the vertical_axis will be brought for the regridding.
 * \param collocated_product The product containing the collocated measurements and the averaging kernel and a-priori.
 *
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_product_smooth_vertical_with_collocated_product(harp_product *product, int num_smooth_variables,
                                                                     const char **smooth_variables,
                                                                     const char *vertical_axis,
                                                                     const char *vertical_unit,
                                                                     const harp_product *collocated_product)
{
    harp_dimension_type local_dimension_type[HARP_NUM_DIM_TYPES];
    harp_product *temp_product = NULL;
    char vertical_bounds_name[MAX_NAME_LENGTH];
    char avk_name[MAX_NAME_LENGTH];
    char apriori_name[MAX_NAME_LENGTH];
    harp_variable *collocation_index = NULL;
    harp_variable *vertical_grid = NULL;
    harp_variable *vertical_bounds = NULL;
    harp_variable *avk = NULL;
    harp_variable *apriori = NULL;
    harp_variable *variable = NULL;
    harp_variable *temp_variable = NULL;
    int i;

    if (product->dimension[harp_dimension_vertical] == 0)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "product has no vertical dimension");
        return -1;
    }

    /* raise warnings for any variables that were not present */
    for (i = 0; i < num_smooth_variables; i++)
    {
        if (!harp_product_has_variable(product, smooth_variables[i]))
        {
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "product has no variable named '%s'", smooth_variables[i]);
            return -1;
        }
    }

    snprintf(vertical_bounds_name, MAX_NAME_LENGTH, "%s_bounds", vertical_axis);
    if (harp_product_new(&temp_product) != 0)
    {
        return -1;
    }

    local_dimension_type[0] = harp_dimension_time;
    if (harp_product_get_derived_variable(collocated_product, "collocation_index", NULL, NULL, 1,
                                          local_dimension_type, &temp_variable) != 0)
    {
        harp_product_delete(temp_product);
        return -1;
    }
    if (harp_product_add_variable(temp_product, temp_variable) != 0)
    {
        harp_variable_delete(temp_variable);
        harp_product_delete(temp_product);
        return -1;
    }

    local_dimension_type[0] = harp_dimension_time;
    local_dimension_type[1] = harp_dimension_vertical;
    local_dimension_type[2] = harp_dimension_independent;

    /* vertical grid */
    if (harp_product_get_derived_variable(collocated_product, vertical_axis, NULL, vertical_unit, 2,
                                          local_dimension_type, &temp_variable) != 0)
    {
        harp_product_delete(temp_product);
        return -1;
    }
    if (harp_product_add_variable(temp_product, temp_variable) != 0)
    {
        harp_variable_delete(temp_variable);
        harp_product_delete(temp_product);
        return -1;
    }

    /* vertical grid bounds */
    if (harp_product_get_derived_variable(collocated_product, vertical_bounds_name, NULL, vertical_unit, 3,
                                          local_dimension_type, &temp_variable) != 0)
    {
        harp_product_delete(temp_product);
        return -1;
    }
    if (harp_product_add_variable(temp_product, temp_variable) != 0)
    {
        harp_variable_delete(temp_variable);
        harp_product_delete(temp_product);
        return -1;
    }

    local_dimension_type[2] = harp_dimension_vertical;
    for (i = 0; i < num_smooth_variables; i++)
    {
        snprintf(avk_name, MAX_NAME_LENGTH, "%s_avk", smooth_variables[i]);
        snprintf(apriori_name, MAX_NAME_LENGTH, "%s_apriori", smooth_variables[i]);

        harp_product_get_variable_by_name(product, smooth_variables[i], &variable);

        /* avk */
        if (harp_product_get_derived_variable(collocated_product, avk_name, NULL, "", 3, local_dimension_type,
                                              &temp_variable) != 0)
        {
            harp_product_delete(temp_product);
            return -1;
        }
        if (harp_product_add_variable(temp_product, temp_variable) != 0)
        {
            harp_variable_delete(temp_variable);
            harp_product_delete(temp_product);
            return -1;
        }

        /* apriori profile */
        if (harp_product_get_derived_variable(collocated_product, apriori_name, NULL, variable->unit, 2,
                                              local_dimension_type, &temp_variable) == 0)
        {
            if (harp_product_add_variable(temp_product, temp_variable) != 0)
            {
                harp_variable_delete(temp_variable);
                harp_product_delete(temp_product);
                return -1;
            }
        }
    }

    /* Get the source product's collocation index variable */
    if (harp_product_get_variable_by_name(product, "collocation_index", &collocation_index) != 0)
    {
        return -1;
    }

    /* sort/filter the reduced collocated product so the samples are in the same order as in 'product' */
    if (harp_product_filter_by_index(temp_product, "collocation_index", collocation_index->num_elements,
                                     collocation_index->data.int32_data) != 0)
    {
        harp_product_delete(temp_product);
        return -1;
    }

    harp_product_get_variable_by_name(temp_product, vertical_axis, &vertical_grid);
    harp_product_get_variable_by_name(temp_product, vertical_bounds_name, &vertical_bounds);
    if (harp_product_regrid_with_axis_variable(product, vertical_grid, vertical_bounds) != 0)
    {
        harp_product_delete(temp_product);
        return -1;
    }

    for (i = 0; i < num_smooth_variables; i++)
    {
        snprintf(avk_name, MAX_NAME_LENGTH, "%s_avk", smooth_variables[i]);
        snprintf(apriori_name, MAX_NAME_LENGTH, "%s_apriori", smooth_variables[i]);

        harp_product_get_variable_by_name(product, smooth_variables[i], &variable);
        harp_product_get_variable_by_name(temp_product, avk_name, &avk);
        apriori = NULL;
        if (harp_product_has_variable(temp_product, apriori_name))
        {
            harp_product_get_variable_by_name(temp_product, apriori_name, &apriori);
        }
        if (harp_variable_smooth_vertical(variable, vertical_grid, avk, apriori) != 0)
        {
            harp_product_delete(temp_product);
            return -1;
        }
    }

    harp_product_delete(temp_product);

    return 0;
}

/** Smooth the product's variables (from dataset a in the collocation result) using the vertical grids,
 * avks and a apriori of collocated products in dataset b.
 *
 * The product is first fully regridded (using the vertical dimension) to the vertical grid of the averaging kernel
 * (and apriori). Then, the given list of variables is smoothed using the list of AVKs and apriori variables.
 *
 * \param product Product to smooth.
 * \param num_smooth_variables length of smooth_variables.
 * \param smooth_variables The names of the variables to smooth.
 * \param vertical_axis The name of the variable to use as a vertical axis (pressure/altitude/etc).
 * \param vertical_unit The unit in which the vertical_axis will be brought for the regridding.
 * \param collocation_result The collocation result used to locate the matching vertical grids/avks/apriori.
 *   The collocation result is assumed to have the appropriate metadata available for all matches (dataset b).
 *
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_product_smooth_vertical_with_collocated_dataset(harp_product *product, int num_smooth_variables,
                                                                     const char **smooth_variables,
                                                                     const char *vertical_axis,
                                                                     const char *vertical_unit,
                                                                     const harp_collocation_result *collocation_result)
{
    harp_collocation_result *filtered_collocation_result = NULL;
    harp_product *merged_product = NULL;
    char vertical_bounds_name[MAX_NAME_LENGTH];
    char avk_name[MAX_NAME_LENGTH];
    char apriori_name[MAX_NAME_LENGTH];
    harp_variable *variable = NULL;
    harp_variable *collocation_index = NULL;
    harp_variable *vertical_grid = NULL;
    harp_variable *vertical_bounds = NULL;
    harp_variable *avk = NULL;
    harp_variable *apriori = NULL;
    long i;

    if (product->dimension[harp_dimension_vertical] == 0)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "product has no vertical dimension");
        return -1;
    }

    /* raise warnings for any variables that were not present */
    for (i = 0; i < num_smooth_variables; i++)
    {
        if (!harp_product_has_variable(product, smooth_variables[i]))
        {
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "product has no variable named '%s'", smooth_variables[i]);
            return -1;
        }
    }

    /* Get the source product's collocation index variable */
    if (harp_product_get_variable_by_name(product, "collocation_index", &collocation_index) != 0)
    {
        return -1;
    }

    /* copy the collocation result for filtering */
    if (harp_collocation_result_shallow_copy(collocation_result, &filtered_collocation_result) != 0)
    {
        return -1;
    }

    /* Reduce the collocation result to only pairs that include the source product */
    if (harp_collocation_result_filter_for_collocation_indices(filtered_collocation_result,
                                                               collocation_index->num_elements,
                                                               collocation_index->data.int32_data) != 0)
    {
        harp_collocation_result_shallow_delete(filtered_collocation_result);
        return -1;
    }
    if (filtered_collocation_result->num_pairs != collocation_index->num_elements)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "product and collocation result are inconsistent");
        harp_collocation_result_shallow_delete(filtered_collocation_result);
        return -1;
    }

    snprintf(vertical_bounds_name, MAX_NAME_LENGTH, "%s_bounds", vertical_axis);

    for (i = 0; i < filtered_collocation_result->dataset_b->num_products; i++)
    {
        harp_dimension_type local_dimension_type[HARP_NUM_DIM_TYPES];
        harp_product *collocated_product;
        long j;

        if (harp_collocation_result_get_filtered_product_b(filtered_collocation_result,
                                                           filtered_collocation_result->dataset_b->source_product[i],
                                                           &collocated_product) != 0)
        {
            harp_product_delete(merged_product);
            harp_collocation_result_shallow_delete(filtered_collocation_result);
            return -1;
        }

        if (collocated_product == NULL || harp_product_is_empty(collocated_product))
        {
            continue;
        }

        local_dimension_type[0] = harp_dimension_time;
        local_dimension_type[1] = harp_dimension_vertical;
        local_dimension_type[2] = harp_dimension_independent;

        /* vertical grid */
        if (harp_product_add_derived_variable(collocated_product, vertical_axis, NULL, vertical_unit, 2,
                                              local_dimension_type) != 0)
        {
            harp_product_delete(collocated_product);
            harp_product_delete(merged_product);
            harp_collocation_result_shallow_delete(filtered_collocation_result);
            return -1;
        }

        /* vertical grid bounds */
        if (harp_product_add_derived_variable(collocated_product, vertical_bounds_name, NULL, vertical_unit, 3,
                                              local_dimension_type) != 0)
        {
            harp_product_delete(collocated_product);
            harp_product_delete(merged_product);
            harp_collocation_result_shallow_delete(filtered_collocation_result);
            return -1;
        }

        local_dimension_type[2] = harp_dimension_vertical;

        for (j = 0; j < num_smooth_variables; j++)
        {
            snprintf(avk_name, MAX_NAME_LENGTH, "%s_avk", smooth_variables[j]);
            snprintf(apriori_name, MAX_NAME_LENGTH, "%s_apriori", smooth_variables[j]);

            harp_product_get_variable_by_name(product, smooth_variables[j], &variable);

            /* avk */
            if (harp_product_add_derived_variable(collocated_product, avk_name, NULL, "", 3, local_dimension_type) != 0)
            {
                harp_product_delete(collocated_product);
                harp_product_delete(merged_product);
                harp_collocation_result_shallow_delete(filtered_collocation_result);
                return -1;
            }

            /* apriori profile */
            harp_product_add_derived_variable(collocated_product, apriori_name, NULL, variable->unit, 2,
                                              local_dimension_type);
            /* it is Ok if the apriori cannot be derived (we ignore the return value of the function) */
        }

        /* strip collocated product to the variables that we need */
        for (j = collocated_product->num_variables - 1; j >= 0; j--)
        {
            const char *name = collocated_product->variable[j]->name;

            if (strcmp(name, "collocation_index") != 0 && strcmp(name, vertical_axis) != 0 &&
                strcmp(name, vertical_bounds_name) != 0 && strstr(name, "_avk") == NULL &&
                strstr(name, "_apriori") == NULL)
            {
                if (harp_product_remove_variable(collocated_product, collocated_product->variable[j]) != 0)
                {
                    harp_product_delete(collocated_product);
                    harp_product_delete(merged_product);
                    harp_collocation_result_shallow_delete(filtered_collocation_result);
                    return -1;
                }
            }
        }

        if (merged_product == NULL)
        {
            merged_product = collocated_product;
        }
        else
        {
            if (harp_product_append(merged_product, collocated_product) != 0)
            {
                harp_product_delete(collocated_product);
                harp_product_delete(merged_product);
                harp_collocation_result_shallow_delete(filtered_collocation_result);
                return -1;
            }
            harp_product_delete(collocated_product);
        }
    }

    if (merged_product == NULL)
    {
        harp_collocation_result_shallow_delete(filtered_collocation_result);
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "collocated dataset does not contain any matching pairs");
        return -1;
    }

    /* sort/filter the merged product so the samples are in the same order as in 'product' */
    if (harp_product_filter_by_index(merged_product, "collocation_index", collocation_index->num_elements,
                                     collocation_index->data.int32_data) != 0)
    {
        harp_product_delete(merged_product);
        harp_collocation_result_shallow_delete(filtered_collocation_result);
        return -1;
    }

    harp_product_get_variable_by_name(merged_product, vertical_axis, &vertical_grid);
    harp_product_get_variable_by_name(merged_product, vertical_bounds_name, &vertical_bounds);
    if (harp_product_regrid_with_axis_variable(product, vertical_grid, vertical_bounds) != 0)
    {
        harp_product_delete(merged_product);
        harp_collocation_result_shallow_delete(filtered_collocation_result);
        return -1;
    }

    for (i = 0; i < num_smooth_variables; i++)
    {
        snprintf(avk_name, MAX_NAME_LENGTH, "%s_avk", smooth_variables[i]);
        snprintf(apriori_name, MAX_NAME_LENGTH, "%s_apriori", smooth_variables[i]);

        harp_product_get_variable_by_name(product, smooth_variables[i], &variable);
        harp_product_get_variable_by_name(merged_product, avk_name, &avk);
        apriori = NULL;
        if (harp_product_has_variable(merged_product, apriori_name))
        {
            harp_product_get_variable_by_name(merged_product, apriori_name, &apriori);
        }
        if (harp_variable_smooth_vertical(variable, vertical_grid, avk, apriori) != 0)
        {
            harp_product_delete(merged_product);
            harp_collocation_result_shallow_delete(filtered_collocation_result);
            return -1;
        }
    }

    /* cleanup */
    harp_product_delete(merged_product);
    harp_collocation_result_shallow_delete(filtered_collocation_result);

    return 0;
}

/** Derive vertical column smoothed with column averaging kernel and optional a-priori
 * First a partial column profile will be derived from the product.
 * This partial column profile will be regridded to the column averaging kernel grid.
 * The regridded column profile will then be combined with the column averaging kernel and optional apriori profile
 * to create an integrated smoothed vertical column.
 * \param product Product from which to derive a smoothed integrated vertical column.
 * \param name Name of the variable that should be created.
 * \param unit Unit (optional) of the variable that should be created.
 * \param vertical_grid Variable containing the vertical grid of the column avk.
 * \param vertical_bounds Variable containig the grid boundaries of the column avk (optional).
 * \param column_avk Column averaging kernel variable.
 * \param apriori Apriori profile (optional).
 * \param variable Pointer to the C variable where the derived HARP variable will be stored.
 *
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_product_get_smoothed_column(harp_product *product, const char *name, const char *unit,
                                                 harp_variable *vertical_grid, harp_variable *vertical_bounds,
                                                 harp_variable *column_avk, harp_variable *apriori,
                                                 harp_variable **variable)
{
    harp_dimension_type grid_dim_type[2];
    harp_product *regrid_product;
    harp_variable *column_variable;
    harp_variable *partcol_variable;
    harp_variable *source_grid;
    harp_variable *source_bounds;
    long num_vertical_elements;
    long i, j;

    if (product->dimension[harp_dimension_vertical] == 0)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "product has no vertical dimension");
        return -1;
    }
    if (vertical_grid->num_dimensions < 1 ||
        vertical_grid->dimension_type[vertical_grid->num_dimensions - 1] != harp_dimension_vertical)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "vertical grid has invalid dimensions");
        return -1;
    }
    /* vertical_bounds are checked by harp_product_regrid_with_axis_variable() */
    if (column_avk->num_dimensions < 1 ||
        column_avk->dimension_type[column_avk->num_dimensions - 1] != harp_dimension_vertical)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "column avk has invalid dimensions");
        return -1;
    }
    num_vertical_elements = vertical_grid->dimension[vertical_grid->num_dimensions - 1];
    if (column_avk->dimension[column_avk->num_dimensions - 1] != num_vertical_elements)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "column avk and vertical grid have inconsistent dimensions");
        return -1;
    }
    if (apriori != NULL)
    {
        if (apriori->num_dimensions != column_avk->num_dimensions)
        {
            harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "apriori profile and column avk have inconsistent dimensions");
            return -1;
        }
        for (i = 0; i < apriori->num_dimensions; i++)
        {
            if (apriori->dimension_type[i] != column_avk->dimension_type[i] ||
                apriori->dimension[i] != column_avk->dimension[i])
            {
                harp_set_error(HARP_ERROR_INVALID_ARGUMENT,
                               "apriori profile and column avk have inconsistent dimensions");
                return -1;
            }
        }
    }

    if (harp_product_new(&regrid_product) != 0)
    {
        return -1;
    }

    /* retrieve partial column profile from source product */
    if (harp_product_get_derived_variable(product, name, NULL, unit, column_avk->num_dimensions,
                                          column_avk->dimension_type, &partcol_variable) != 0)
    {
        harp_product_delete(regrid_product);
        return -1;
    }
    if (harp_product_add_variable(regrid_product, partcol_variable) != 0)
    {
        harp_variable_delete(partcol_variable);
        harp_product_delete(regrid_product);
        return -1;
    }

    grid_dim_type[0] = harp_dimension_time;
    grid_dim_type[1] = harp_dimension_vertical;

    /* Add axis variables for the source grid to the temporary product */
    if (harp_product_get_derived_variable(product, vertical_grid->name, NULL, vertical_grid->unit, 1,
                                          &grid_dim_type[1], &source_grid) != 0)
    {
        /* Failed to derive time independent. Try time dependent. */
        if (harp_product_get_derived_variable(product, vertical_grid->name, NULL, vertical_grid->unit, 2,
                                              grid_dim_type, &source_grid) != 0)
        {
            harp_product_delete(regrid_product);
            return -1;
        }
    }
    if (harp_product_add_variable(regrid_product, source_grid) != 0)
    {
        harp_variable_delete(source_grid);
        harp_product_delete(regrid_product);
        return -1;
    }
    if (harp_product_get_derived_bounds_for_grid(product, source_grid, &source_bounds) != 0)
    {
        harp_product_delete(regrid_product);
        return -1;
    }
    if (harp_product_add_variable(regrid_product, source_bounds) != 0)
    {
        harp_variable_delete(source_bounds);
        harp_product_delete(regrid_product);
        return -1;
    }

    if (harp_product_regrid_with_axis_variable(regrid_product, vertical_grid, vertical_bounds) != 0)
    {
        harp_product_delete(regrid_product);
        return -1;
    }

    if (harp_variable_new(name, harp_type_double, column_avk->num_dimensions - 1, column_avk->dimension_type,
                          column_avk->dimension, &column_variable) != 0)
    {
        harp_product_delete(regrid_product);
        return -1;
    }
    if (harp_variable_set_unit(column_variable, unit) != 0)
    {
        harp_variable_delete(column_variable);
        harp_product_delete(regrid_product);
        return -1;
    }

    for (i = 0; i < column_variable->num_elements; i++)
    {
        int is_valid = 0;

        /* multiply partial column profile with column averaging kernel */
        column_variable->data.double_data[i] = 0;
        for (j = 0; j < num_vertical_elements; j++)
        {
            if (!harp_isnan(partcol_variable->data.double_data[i * num_vertical_elements + j]))
            {
                column_variable->data.double_data[i] +=
                    partcol_variable->data.double_data[i * num_vertical_elements + j] *
                    column_avk->data.double_data[i * num_vertical_elements + j];
                is_valid = 1;
                /* subtract the apriori */
                if (apriori != NULL && !harp_isnan(apriori->data.double_data[i * num_vertical_elements + j]))
                {
                    column_variable->data.double_data[i] -=
                        column_avk->data.double_data[i * num_vertical_elements + j] *
                        apriori->data.double_data[i * num_vertical_elements + j];
                }
            }

            /* add the apriori */
            if (apriori != NULL && !harp_isnan(apriori->data.double_data[i * num_vertical_elements + j]))
            {
                column_variable->data.double_data[i] += apriori->data.double_data[i * num_vertical_elements + j];
                is_valid = 1;
            }
        }
        if (!is_valid)
        {
            column_variable->data.double_data[i] = harp_nan();
        }
    }

    harp_product_delete(regrid_product);

    *variable = column_variable;

    return 0;
}

/** Derive a vertical column smoothed with column averaging kernel and a-priori from the collocated product
 *
 * \param product Product to regrid.
 * \param name Name of the variable that should be created.
 * \param unit Unit (optional) of the variable that should be created.
 * \param num_dimensions Number of dimensions of the variable that should be created.
 * \param dimension_type Type of dimension for each of the dimensions of the variable that should be created.
 * \param vertical_axis The name of the variable to use as a vertical axis (pressure/altitude/etc).
 * \param vertical_unit The unit in which the vertical_axis will be brought for the regridding.
 * \param collocated_product The product containing the collocated measurements and the averaging kernel and a-priori.
 * \param variable Pointer to the C variable where the derived HARP variable will be stored.
 *
 * \return
 *   \arg \c 0, Success.
 *   \arg \c -1, Error occurred (check #harp_errno).
 */
LIBHARP_API int harp_product_get_smoothed_column_using_collocated_product(harp_product *product, const char *name,
                                                                          const char *unit, int num_dimensions,
                                                                          const harp_dimension_type *dimension_type,
                                                                          const char *vertical_axis,
                                                                          const char *vertical_unit,
                                                                          const harp_product *collocated_product,
                                                                          harp_variable **variable)
{
    harp_dimension_type local_dimension_type[HARP_NUM_DIM_TYPES];
    harp_product *temp_product = NULL;
    char vertical_bounds_name[MAX_NAME_LENGTH];
    char column_avk_name[MAX_NAME_LENGTH];
    char apriori_name[MAX_NAME_LENGTH];
    harp_variable *collocation_index = NULL;
    harp_variable *vertical_grid = NULL;
    harp_variable *vertical_bounds = NULL;
    harp_variable *column_avk = NULL;
    harp_variable *apriori = NULL;
    harp_variable *temp_variable = NULL;
    int i;

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

    snprintf(vertical_bounds_name, MAX_NAME_LENGTH, "%s_bounds", vertical_axis);
    snprintf(column_avk_name, MAX_NAME_LENGTH, "%s_avk", name);
    snprintf(apriori_name, MAX_NAME_LENGTH, "%s_apriori", name);

    if (harp_product_new(&temp_product) != 0)
    {
        return -1;
    }

    local_dimension_type[0] = harp_dimension_time;
    if (harp_product_get_derived_variable(collocated_product, "collocation_index", NULL, NULL, 1,
                                          local_dimension_type, &temp_variable) != 0)
    {
        harp_product_delete(temp_product);
        return -1;
    }
    if (harp_product_add_variable(temp_product, temp_variable) != 0)
    {
        harp_variable_delete(temp_variable);
        harp_product_delete(temp_product);
        return -1;
    }

    local_dimension_type[0] = harp_dimension_time;
    local_dimension_type[1] = harp_dimension_vertical;
    local_dimension_type[2] = harp_dimension_independent;

    /* vertical grid */
    if (harp_product_get_derived_variable(collocated_product, vertical_axis, NULL, vertical_unit, 2,
                                          local_dimension_type, &temp_variable) != 0)
    {
        harp_product_delete(temp_product);
        return -1;
    }
    if (harp_product_add_variable(temp_product, temp_variable) != 0)
    {
        harp_variable_delete(temp_variable);
        harp_product_delete(temp_product);
        return -1;
    }

    /* vertical grid bounds */
    if (harp_product_get_derived_variable(collocated_product, vertical_bounds_name, NULL, vertical_unit, 3,
                                          local_dimension_type, &temp_variable) != 0)
    {
        harp_product_delete(temp_product);
        return -1;
    }
    if (harp_product_add_variable(temp_product, temp_variable) != 0)
    {
        harp_variable_delete(temp_variable);
        harp_product_delete(temp_product);
        return -1;
    }

    for (i = 0; i < num_dimensions; i++)
    {
        local_dimension_type[i] = dimension_type[i];
    }
    local_dimension_type[num_dimensions] = harp_dimension_vertical;

    /* column avk */
    if (harp_product_get_derived_variable(collocated_product, column_avk_name, NULL, "", num_dimensions + 1,
                                          local_dimension_type, &temp_variable) != 0)
    {
        harp_product_delete(temp_product);
        return -1;
    }
    if (harp_product_add_variable(temp_product, temp_variable) != 0)
    {
        harp_variable_delete(temp_variable);
        harp_product_delete(temp_product);
        return -1;
    }

    /* apriori profile */
    if (harp_product_get_derived_variable(collocated_product, apriori_name, NULL, unit, num_dimensions + 1,
                                          local_dimension_type, &temp_variable) == 0)
    {
        if (harp_product_add_variable(temp_product, temp_variable) != 0)
        {
            harp_variable_delete(temp_variable);
            harp_product_delete(temp_product);
            return -1;
        }
    }

    /* Get the source product's collocation index variable */
    if (harp_product_get_variable_by_name(product, "collocation_index", &collocation_index) != 0)
    {
        return -1;
    }

    /* sort/filter the reduced collocated product so the samples are in the same order as in 'product' */
    if (harp_product_filter_by_index(temp_product, "collocation_index", collocation_index->num_elements,
                                     collocation_index->data.int32_data) != 0)
    {
        harp_product_delete(temp_product);
        return -1;
    }

    harp_product_get_variable_by_name(temp_product, vertical_axis, &vertical_grid);
    harp_product_get_variable_by_name(temp_product, vertical_bounds_name, &vertical_bounds);
    harp_product_get_variable_by_name(temp_product, column_avk_name, &column_avk);
    if (harp_product_has_variable(temp_product, apriori_name))
    {
        harp_product_get_variable_by_name(temp_product, apriori_name, &apriori);
    }
    if (harp_product_get_smoothed_column(product, name, unit, vertical_grid, vertical_bounds, column_avk, apriori,
                                         variable) != 0)
    {
        harp_product_delete(temp_product);
        return -1;
    }

    harp_product_delete(temp_product);

    return 0;
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
LIBHARP_API int harp_product_get_smoothed_column_using_collocated_dataset(harp_product *product, const char *name,
                                                                          const char *unit, int num_dimensions,
                                                                          const harp_dimension_type *dimension_type,
                                                                          const char *vertical_axis,
                                                                          const char *vertical_unit,
                                                                          const harp_collocation_result
                                                                          *collocation_result, harp_variable **variable)
{
    harp_collocation_result *filtered_collocation_result = NULL;
    harp_product *merged_product = NULL;
    char vertical_bounds_name[MAX_NAME_LENGTH];
    char column_avk_name[MAX_NAME_LENGTH];
    char apriori_name[MAX_NAME_LENGTH];
    harp_variable *collocation_index = NULL;
    harp_variable *vertical_grid = NULL;
    harp_variable *vertical_bounds = NULL;
    harp_variable *column_avk = NULL;
    harp_variable *apriori = NULL;
    long i;

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

    /* Get the source product's collocation index variable */
    if (harp_product_get_variable_by_name(product, "collocation_index", &collocation_index) != 0)
    {
        return -1;
    }

    /* copy the collocation result for filtering */
    if (harp_collocation_result_shallow_copy(collocation_result, &filtered_collocation_result) != 0)
    {
        return -1;
    }

    /* Reduce the collocation result to only pairs that include the source product */
    if (harp_collocation_result_filter_for_collocation_indices(filtered_collocation_result,
                                                               collocation_index->num_elements,
                                                               collocation_index->data.int32_data) != 0)
    {
        harp_collocation_result_shallow_delete(filtered_collocation_result);
        return -1;
    }
    if (filtered_collocation_result->num_pairs != collocation_index->num_elements)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "product and collocation result are inconsistent");
        harp_collocation_result_shallow_delete(filtered_collocation_result);
        return -1;
    }

    snprintf(vertical_bounds_name, MAX_NAME_LENGTH, "%s_bounds", vertical_axis);
    snprintf(column_avk_name, MAX_NAME_LENGTH, "%s_avk", name);
    snprintf(apriori_name, MAX_NAME_LENGTH, "%s_apriori", name);

    for (i = 0; i < filtered_collocation_result->dataset_b->num_products; i++)
    {
        harp_dimension_type local_dimension_type[HARP_NUM_DIM_TYPES];
        harp_product *collocated_product;
        long j;

        if (harp_collocation_result_get_filtered_product_b(filtered_collocation_result,
                                                           filtered_collocation_result->dataset_b->source_product[i],
                                                           &collocated_product) != 0)
        {
            harp_product_delete(merged_product);
            harp_collocation_result_shallow_delete(filtered_collocation_result);
            return -1;
        }

        if (collocated_product == NULL || harp_product_is_empty(collocated_product))
        {
            continue;
        }

        local_dimension_type[0] = harp_dimension_time;
        local_dimension_type[1] = harp_dimension_vertical;
        local_dimension_type[2] = harp_dimension_independent;

        /* vertical grid */
        if (harp_product_add_derived_variable(collocated_product, vertical_axis, NULL, vertical_unit, 2,
                                              local_dimension_type) != 0)
        {
            harp_product_delete(collocated_product);
            harp_product_delete(merged_product);
            harp_collocation_result_shallow_delete(filtered_collocation_result);
            return -1;
        }

        /* vertical grid bounds */
        if (harp_product_add_derived_variable(collocated_product, vertical_bounds_name, NULL, vertical_unit, 3,
                                              local_dimension_type) != 0)
        {
            harp_product_delete(collocated_product);
            harp_product_delete(merged_product);
            harp_collocation_result_shallow_delete(filtered_collocation_result);
            return -1;
        }

        for (j = 0; j < num_dimensions; j++)
        {
            local_dimension_type[j] = dimension_type[j];
        }
        local_dimension_type[num_dimensions] = harp_dimension_vertical;

        /* column avk */
        if (harp_product_add_derived_variable(collocated_product, column_avk_name, NULL, "", num_dimensions + 1,
                                              local_dimension_type) != 0)
        {
            harp_product_delete(collocated_product);
            harp_product_delete(merged_product);
            harp_collocation_result_shallow_delete(filtered_collocation_result);
            return -1;
        }

        /* apriori profile */
        harp_product_add_derived_variable(collocated_product, apriori_name, NULL, unit, num_dimensions + 1,
                                          local_dimension_type);
        /* it is Ok if the apriori cannot be derived (we ignore the return value of the function) */

        /* strip collocated product to just the variables that we need */
        for (j = collocated_product->num_variables - 1; j >= 0; j--)
        {
            const char *name = collocated_product->variable[j]->name;

            if (strcmp(name, "collocation_index") != 0 && strcmp(name, vertical_axis) != 0 &&
                strcmp(name, vertical_bounds_name) != 0 && strcmp(name, column_avk_name) != 0 &&
                strcmp(name, apriori_name) != 0)
            {
                if (harp_product_remove_variable(collocated_product, collocated_product->variable[j]) != 0)
                {
                    harp_product_delete(collocated_product);
                    harp_product_delete(merged_product);
                    harp_collocation_result_shallow_delete(filtered_collocation_result);
                    return -1;
                }
            }
        }

        if (merged_product == NULL)
        {
            merged_product = collocated_product;
        }
        else
        {
            if (harp_product_append(merged_product, collocated_product) != 0)
            {
                harp_product_delete(collocated_product);
                harp_product_delete(merged_product);
                harp_collocation_result_shallow_delete(filtered_collocation_result);
                return -1;
            }
            harp_product_delete(collocated_product);
        }
    }

    if (merged_product == NULL)
    {
        harp_collocation_result_shallow_delete(filtered_collocation_result);
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "collocated dataset does not contain any matching pairs");
        return -1;
    }

    /* sort/filter the merged product so the samples are in the same order as in 'product' */
    if (harp_product_filter_by_index(merged_product, "collocation_index", collocation_index->num_elements,
                                     collocation_index->data.int32_data) != 0)
    {
        harp_product_delete(merged_product);
        harp_collocation_result_shallow_delete(filtered_collocation_result);
        return -1;
    }

    harp_product_get_variable_by_name(merged_product, vertical_axis, &vertical_grid);
    harp_product_get_variable_by_name(merged_product, vertical_bounds_name, &vertical_bounds);
    harp_product_get_variable_by_name(merged_product, column_avk_name, &column_avk);
    if (harp_product_has_variable(merged_product, apriori_name))
    {
        harp_product_get_variable_by_name(merged_product, apriori_name, &apriori);
    }
    if (harp_product_get_smoothed_column(product, name, unit, vertical_grid, vertical_bounds, column_avk, apriori,
                                         variable) != 0)
    {
        harp_product_delete(merged_product);
        harp_collocation_result_shallow_delete(filtered_collocation_result);
        return -1;
    }

    /* cleanup */
    harp_product_delete(merged_product);
    harp_collocation_result_shallow_delete(filtered_collocation_result);

    return 0;
}

/**
 * @}
 */
