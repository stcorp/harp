/*
 * Copyright (C) 2015-2021 S[&]T, The Netherlands.
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

#include <assert.h>
#include <stdlib.h>
#include <math.h>

/* Given arrays x[0..n-1] and y[0..n-1] containing a tabulated function, i.e., yi = f(xi), with
 * x1 < x2 < ... < xN , and given values d0 and dnmin1 for the first derivative of the interpolating
 * function at points 0 and n-1, respectively, this function returns an array second_derivatives[0..n-1] that contains
 * the second derivatives of the interpolating function at the tabulated points xi . If d0 and/or
 * dnmin1 are equal to 1.0e30 or larger, the function is signaled to set the corresponding boundary
 * condition for a natural spline, with zero second derivative on that boundary.
 */
static int get_second_derivatives(const double *x, const double *y, long n, double d0, double dnmin1,
                                  double *second_derivatives)
{
    double *u = NULL;
    double p;
    double qnmin1;
    double sig;
    double unmin1;
    long i;
    long k;

    if (second_derivatives == NULL)
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "'second_derivatives' is empty");
        return -1;
    }

    u = calloc((size_t)n, sizeof(double));
    if (u == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       n * sizeof(double), __FILE__, __LINE__);
        return -1;
    }

    if (d0 > 0.99e30)
    {
        /* The lower boundary condition is set either to be 'natural' ... */
        second_derivatives[1] = 0.0;
        u[1] = 0.0;
    }
    else
    {
        /* ... or else to have a specified first derivative. */
        second_derivatives[1] = -0.5;
        u[1] = (3.0 / (x[2] - x[1])) * ((y[2] - y[1]) / (x[2] - x[1]) - d0);
    }

    for (i = 1; i <= n - 2; i++)
    {
        /* This is the decomposition loop of the tridiagonal algorithm.
           second_derivatives and u are used for temporary storage of the decomposed factors. */
        sig = (x[i] - x[i - 1]) / (x[i + 1] - x[i - 1]);
        p = sig * second_derivatives[i - 1] + 2.0;
        second_derivatives[i] = (sig - 1.0) / p;

        u[i] = (y[i + 1] - y[i]) / (x[i + 1] - x[i]) - (y[i] - y[i - 1]) / (x[i] - x[i - 1]);
        u[i] = (6.0 * u[i] / (x[i + 1] - x[i - 1]) - sig * u[i - 1]) / p;
    }

    if (dnmin1 > 0.99e30)
    {
        /* The upper boundary condition is set either to be 'natural' ... */
        qnmin1 = 0.0;
        unmin1 = 0.0;
    }
    else
    {
        /* ... or else to have a specified first derivative. */
        qnmin1 = 0.5;
        unmin1 = (3.0 / (x[n] - x[n - 1])) * (dnmin1 - (y[n] - y[n - 1]) / (x[n] - x[n - 1]));
    }

    second_derivatives[n - 1] = (unmin1 - qnmin1 * u[n - 2]) / (qnmin1 * second_derivatives[n - 2] + 1.0);
    for (k = n - 2; k >= 0; k--)
    {
        /* This is the backsubstitution loop of the tridiagonal algorithm. */
        second_derivatives[k] = second_derivatives[k] * second_derivatives[k + 1] + u[k];
    }

    free(u);
    return 0;
}

/* Given an m by n tabulated function ya[1..m][1..n] , and tabulated independent variables
 * x2a[1..n], this function constructs one-dimensional natural cubic splines of the rows of ya
 * and returns the second-derivatives in the array y2a[0..m-1][0..n-1].  */
static int get_second_derivatives_matrix(const double *xx, const double **zz, long m, long n,
                                         double **second_derivatives_matrix)
{
    /* Set the first derivatives to 1.0e30 to obtain a natural spline */
    double d0 = 1.0e30; /* First derivative of the interpolating function at points 0. */
    double dnmin1 = 1.0e30;     /* First derivative of the interpolating function at points n-1. */
    long i;

    for (i = 0; i <= m - 1; i++)
    {
        if (get_second_derivatives(xx, zz[i], n, d0, dnmin1, second_derivatives_matrix[i]) != 0)
        {
            return -1;
        }
    }
    return 0;
}

/* Given the arrays xx[0..n-1] and yy[0..n-1], which tabulate a function (with the xai's in order),
 * and given the array second_derivatives[0..n-1], which is the output from spline above, and given a value of xp,
 * this function returns a cubic-spline interpolated value yp. */
static int execute_cubic_spline_interpolation(const double *xx, const double *yy, const double *second_derivatives,
                                              long n, double xp, double *new_yp)
{
    double yp;
    long klo;
    long khi;
    long k;
    double h;
    double b;
    double a;

    /* We will find the right place in the table by means of
     * bisection. This is optimal if sequential calls to this
     * function are at random values of x. If sequential calls
     * are in order, and closely spaced, one would do better
     * to store previous values of klo and khi and test if
     * they remain appropriate on the next call. */
    klo = 0;
    khi = n - 1;

    while (khi - klo > 1)
    {
        k = (khi + klo) >> 1;
        if (xx[k] > xp)
        {
            khi = k;
        }
        else
        {
            klo = k;
        }
    }

    /* klo and khi now bracket the input value of xp. */
    h = xx[khi] - xx[klo];
    if (h == 0.0)
    {
        /* The xa’s must be distinct. */
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "xx must be distinct (%s:%u)", __FILE__, __LINE__);
        return -1;
    }
    a = (xx[khi] - xp) / h;
    b = (xp - xx[klo]) / h;

    /* Cubic spline polynomial is now evaluated. */
    yp = a * yy[klo] + b * yy[khi] + ((a * a * a - a) * second_derivatives[klo] +
                                      (b * b * b - b) * second_derivatives[khi]) * (h * h) / 6.0;

    *new_yp = yp;
    return 0;
}

/* Given x1a, x2a, ya, m, n as described in 'get_second_derivatives_matrix' and second_derivatives_matrix as produced by that function;
 * and given a desired interpolating point x1,x2; this function returns an interpolated function value y
 * by bicubic spline interpolation. */
static int execute_bicubic_spline_interpolation(const double *xx, const double *yy, const double **zz,
                                                double **second_derivatives_matrix, long m, long n, double xp,
                                                double yp, double *new_zp)
{
    /* Set the first derivatives to 1.0e30 to obtain a natural spline */
    double d0 = 1.0e30; /* First derivative of the interpolating function at points 0. */
    double dnmin1 = 1.0e30;     /* First derivative of the interpolating function at points n-1. */
    double zp;
    double *temp = NULL;
    double *anothertemp = NULL;
    long j;

    temp = calloc((size_t)m, sizeof(double));
    if (temp == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       m * sizeof(double), __FILE__, __LINE__);
        return -1;
    }

    anothertemp = calloc((size_t)m, sizeof(double));
    if (anothertemp == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       m * sizeof(double), __FILE__, __LINE__);
        free(temp);
        return -1;
    }

    /* Perform m evaluations of the row splines constructed by 'execute_cubic_spline_interpolation',
     * using the 1-dimensional spline evaluator 'execute_cubic_spline_interpolation'. */
    for (j = 0; j < m; j++)
    {
        if (execute_cubic_spline_interpolation(yy, zz[j], second_derivatives_matrix[j], n, yp, &anothertemp[j]) != 0)
        {
            free(anothertemp);
            free(temp);
            return -1;
        }
    }

    if (get_second_derivatives(xx, anothertemp, m, d0, dnmin1, temp) != 0)
    {
        free(anothertemp);
        free(temp);
        return -1;
    }

    /* Construct the 1-dimensional column spline and evaluate it. */
    if (execute_cubic_spline_interpolation(xx, anothertemp, temp, m, xp, &zp) != 0)
    {
        free(anothertemp);
        free(temp);
        return -1;
    }

    free(anothertemp);
    free(temp);
    *new_zp = zp;
    return 0;
}

/* Given an array source_grid[0...n-1], with n=source_length, and given 'target_grid_point',
 * returns 'index' such that target_grid_point is inside the interval [source_grid[index],source_grid[index+1]).
 * source_grid[0...n-1] must be monotonic, either increasing or decreasing.
 * If the grid is increasing then return:
 *   index = -1 if target_grid_point < source_grid[0]
 *   index = i if source_grid[i] <= target_grid_point < source_grid[i+1] (0 <= i < n)
 *   index = n-1 if target_grid_point == source_grid[n-1]
 *   index = n if target_grid_point > source_grid[n-1]
 * If the grid is decreasing then return:
 *   index = -1 if target_grid_point > source_grid[0]
 *   index = i if source_grid[i] >= target_grid_point > source_grid[i+1] (0 <= i < n)
 *   index = n-1 if target_grid_point == source_grid[n-1]
 *   index = n if target_grid_point < source_grid[n-1]
 * 'index' as input is taken as the initial guess for 'index' on output. */
void harp_interpolate_find_index(long source_length, const double *source_grid, double target_grid_point, long *index)
{
    long low;
    long high;
    long increment;
    int ascend;

    if (target_grid_point == source_grid[source_length - 1])
    {
        *index = source_length - 1;
        return;
    }

    /* True if ascending order of table, false otherwise. */
    ascend = (source_grid[source_length - 1] >= source_grid[0]);

    if (*index < 0 || *index > source_length - 1)
    {
        /* Input guess not useful. Go immediately to bisection */
        low = -1;
        high = source_length;
    }
    else
    {
        low = *index;
        increment = 1;
        if (target_grid_point == source_grid[low] || (target_grid_point > source_grid[low]) == ascend)
        {
            if (low == source_length - 1)
            {
                *index = source_length;
                return;
            }
            high = low + 1;
            while (target_grid_point == source_grid[high] || (target_grid_point > source_grid[high]) == ascend)
            {
                low = high;
                high = low + increment;
                if (high > source_length - 1)
                {
                    high = source_length;
                    break;
                }
                increment += increment;
            }
        }
        else
        {
            if (low == 0)
            {
                *index = -1;
                return;
            }
            high = low;
            low -= 1;
            while (target_grid_point != source_grid[low] && (target_grid_point < source_grid[low]) == ascend)
            {
                high = low;
                if (increment >= high)
                {
                    low = -1;
                    break;
                }
                else
                {
                    low = high - increment;
                }
                increment += increment;
            }
        }
    }

    /* final bisection */
    while (high - low != 1)
    {
        long middle = (high + low) / 2;

        if (target_grid_point == source_grid[middle] || (target_grid_point > source_grid[middle]) == ascend)
        {
            low = middle;
        }
        else
        {
            high = middle;
        }
    }

    if (low == source_length - 1)
    {
        /* point is after source_grid[source_length - 1], because equality was already checked */
        *index = source_length;
    }
    else
    {
        *index = low;
    }
}

int harp_cubic_spline_interpolation(const double *xx, const double *yy, long n, const double xp, double *yp)
{
    double d0 = 1.0e30; /* First derivative of the interpolating function at points 0. */
    double dnmin1 = 1.0e30;     /* First derivative of the interpolating function at points n-1. */
    double *second_derivatives = NULL;  /* Second derivatives */

    /* Get the second derivatives of the interpolating function at the tabulated points */
    second_derivatives = calloc((size_t)n, sizeof(double));
    if (second_derivatives == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       n * sizeof(double), __FILE__, __LINE__);
        return -1;
    }
    if (get_second_derivatives(xx, yy, n, d0, dnmin1, second_derivatives) != 0)
    {
        free(second_derivatives);
        return -1;
    }

    /* Interpolate */
    if (execute_cubic_spline_interpolation(xx, yy, second_derivatives, n, xp, yp) != 0)
    {
        free(second_derivatives);
        return -1;
    }

    free(second_derivatives);
    return 0;
}

/* Bicubic spline interpolation */
int harp_bicubic_spline_interpolation(const double *xx, const double *yy, const double **zz, long m, long n,
                                      double xp, double yp, double *new_zp)
{
    double **second_derivatives_matrix = NULL;  /* Matrix with second derivatives */
    double zp;
    long i;

    /* Get the second derivatives of the interpolating function at the tabulated points */
    second_derivatives_matrix = calloc((size_t)m, sizeof(double *));
    if (second_derivatives_matrix == NULL)
    {
        harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                       m * sizeof(double *), __FILE__, __LINE__);
        return -1;
    }
    for (i = 0; i < m; i++)
    {
        second_derivatives_matrix[i] = calloc((size_t)n, sizeof(double));
        if (second_derivatives_matrix[i] == NULL)
        {
            harp_set_error(HARP_ERROR_OUT_OF_MEMORY, "out of memory (could not allocate %lu bytes) (%s:%u)",
                           n * sizeof(double), __FILE__, __LINE__);
            free(second_derivatives_matrix);
            return -1;
        }
    }
    if (get_second_derivatives_matrix(xx, zz, m, n, second_derivatives_matrix) != 0)
    {
        for (i = 0; i < m; i++)
        {
            free(second_derivatives_matrix[i]);
        }
        free(second_derivatives_matrix);
        return -1;
    }

    /* Interpolate */
    if (execute_bicubic_spline_interpolation(xx, yy, zz, second_derivatives_matrix, m, n, xp, yp, &zp) != 0)
    {
        for (i = 0; i < m; i++)
        {
            free(second_derivatives_matrix[i]);
        }
        free(second_derivatives_matrix);
        return -1;
    }

    /* Done */
    for (i = 0; i < m; i++)
    {
        free(second_derivatives_matrix[i]);
    }
    free(second_derivatives_matrix);

    *new_zp = zp;
    return 0;
}

static void interpolate_linear(long source_length, const double *source_grid, const double *source_array,
                               double target_grid_point, int out_of_bound_flag, long *pos, double *target_value)
{
    double v;

    assert(source_length > 1);
    assert(out_of_bound_flag == 0 || out_of_bound_flag == 1 || out_of_bound_flag == 2);

    harp_interpolate_find_index(source_length, source_grid, target_grid_point, pos);

    if (*pos == -1)
    {
        /* grid point is before source_grid[0] */
        if (out_of_bound_flag == 1)
        {
            *target_value = source_array[0];
        }
        else if (out_of_bound_flag == 2)
        {
            v = (target_grid_point - source_grid[0]) / (source_grid[0] - source_grid[1]);
            *target_value = source_array[0] + v * (source_array[0] - source_array[1]);
        }
        else
        {
            *target_value = harp_nan();
        }
    }
    else if (*pos == source_length)
    {
        /* grid point is after source_grid[source_length - 1] */
        if (out_of_bound_flag == 1)
        {
            *target_value = source_array[source_length - 1];
        }
        else if (out_of_bound_flag == 2)
        {
            v = (target_grid_point - source_grid[source_length - 1]) /
                (source_grid[source_length - 1] - source_grid[source_length - 2]);
            *target_value = source_array[source_length - 1] +
                v * (source_array[source_length - 1] - source_array[source_length - 2]);
        }
        else
        {
            *target_value = harp_nan();
        }
    }
    else if (target_grid_point == source_grid[*pos])
    {
        /* don't interpolate, but take exact point */
        *target_value = source_array[*pos];
    }
    else if (target_grid_point == source_grid[*pos + 1])
    {
        /* don't interpolate, but take exact point */
        *target_value = source_array[*pos + 1];
    }
    else
    {
        /* grid point is between source_grid[pos] and source_grid[pos + 1] */
        v = (target_grid_point - source_grid[*pos]) / (source_grid[(*pos) + 1] - source_grid[*pos]);
        *target_value = (1 - v) * source_array[*pos] + v * source_array[(*pos) + 1];
    }
}

/* Interpolate single value from source grid to target point using linear interpolation
 * source_grid needs to be strict monotonic.
 * out_of_bound_flag:
 *  0: set value outside source_grid to NaN
 *  1: set value outside source_grid to edge value (i.e. source_array[0] or source_array[source_length - 1])
 *  2: extrapolate based on nearest two edge values
 */
void harp_interpolate_value_linear(long source_length, const double *source_grid, const double *source_array,
                                   double target_grid_point, int out_of_bound_flag, double *target_value)
{
    long pos = 0;

    interpolate_linear(source_length, source_grid, source_array, target_grid_point, out_of_bound_flag, &pos,
                       target_value);
}

/* Interpolate array from source grid to target grid using linear interpolation
 * Both source_grid and target_grid need to be strict monotonic.
 * This function will do 'the right thing' depending on whether a grid is increasing or decreasing.
 * out_of_bound_flag:
 *  0: set values outside source_grid to NaN
 *  1: set values outside source_grid to edge value (i.e. source_array[0] or source_array[source_length - 1])
 *  2: extrapolate based on nearest two edge values
 */
void harp_interpolate_array_linear(long source_length, const double *source_grid, const double *source_array,
                                   long target_length, const double *target_grid, int out_of_bound_flag,
                                   double *target_array)
{
    long pos = 0;
    long i;

    for (i = 0; i < target_length; i++)
    {
        interpolate_linear(source_length, source_grid, source_array, target_grid[i], out_of_bound_flag, &pos,
                           &target_array[i]);
    }
}

static void interpolate_loglinear(long source_length, const double *source_grid, const double *source_array,
                                  double target_grid_point, int out_of_bound_flag, long *pos, double *target_value)
{
    double v;

    assert(source_length > 1);
    assert(out_of_bound_flag == 0 || out_of_bound_flag == 1 || out_of_bound_flag == 2);

    harp_interpolate_find_index(source_length, source_grid, target_grid_point, pos);

    if (*pos == -1)
    {
        /* grid point is before source_grid[0] */
        if (out_of_bound_flag == 1)
        {
            *target_value = source_array[0];
        }
        else if (out_of_bound_flag == 2)
        {
            v = (log(target_grid_point / source_grid[0])) / (log(source_grid[0] / source_grid[1]));
            *target_value = source_array[0] + v * (source_array[0] - source_array[1]);
        }
        else
        {
            *target_value = harp_nan();
        }
    }
    else if (*pos == source_length)
    {
        /* grid point is after source_grid[source_length - 1] */
        if (out_of_bound_flag == 1)
        {
            *target_value = source_array[source_length - 1];
        }
        else if (out_of_bound_flag == 2)
        {
            v = (log(target_grid_point / source_grid[source_length - 1])) /
                (log(source_grid[source_length - 1] / source_grid[source_length - 2]));
            *target_value = source_array[source_length - 1] +
                v * (source_array[source_length - 1] - source_array[source_length - 2]);
        }
        else
        {
            *target_value = harp_nan();
        }
    }
    else if (target_grid_point == source_grid[*pos])
    {
        /* don't interpolate, but take exact point */
        *target_value = source_array[*pos];
    }
    else if (target_grid_point == source_grid[*pos + 1])
    {
        /* don't interpolate, but take exact point */
        *target_value = source_array[*pos + 1];
    }
    else
    {
        /* grid point is between source_grid[pos] and source_grid[pos + 1] */
        v = (log(target_grid_point / source_grid[*pos])) / (log(source_grid[(*pos) + 1] / source_grid[*pos]));
        *target_value = (1 - v) * source_array[*pos] + v * source_array[(*pos) + 1];
    }
}

/* Interpolate single value from source grid to target point using log linear interpolation of the axis.
 * A log linear interpolation is a linear interpolation using log(source_grid) and log(target_grid_point).
 * source_grid needs to be strict monotonic with values > 0. target_grid_point needs to be > 0.
 * out_of_bound_flag:
 *  0: set value outside source_grid to NaN
 *  1: set value outside source_grid to edge value (i.e. source_array[0] or source_array[source_length - 1])
 *  2: extrapolate based on nearest two edge values
 */
void harp_interpolate_value_loglinear(long source_length, const double *source_grid, const double *source_array,
                                      double target_grid_point, int out_of_bound_flag, double *target_value)
{
    long pos = 0;

    interpolate_loglinear(source_length, source_grid, source_array, target_grid_point, out_of_bound_flag, &pos,
                          target_value);
}

/* Interpolate array from source grid to target grid using log linear interpolation of the axis.
 * A log linear interpolation is a linear interpolation using log(source_grid) and log(target_grid).
 * Both source_grid and target_grid need to be strict monotonic with values > 0.
 * This function will do 'the right thing' depending on whether a grid is increasing or decreasing.
 * out_of_bound_flag:
 *  0: set values outside source_grid to NaN
 *  1: set values outside source_grid to edge value (i.e. source_array[0] or source_array[source_length - 1])
 *  2: extrapolate based on nearest two edge values
 */
void harp_interpolate_array_loglinear(long source_length, const double *source_grid, const double *source_array,
                                      long target_length, const double *target_grid, int out_of_bound_flag,
                                      double *target_array)
{
    long pos = 0;
    long i;

    for (i = 0; i < target_length; i++)
    {
        interpolate_loglinear(source_length, source_grid, source_array, target_grid[i], out_of_bound_flag, &pos,
                              &target_array[i]);
    }
}

static void interpolate_logloglinear(long source_length, const double *source_grid, const double *source_array,
                                     double target_grid_point, int out_of_bound_flag, long *pos, double *target_value)
{
    double v;

    assert(source_length > 1);
    assert(out_of_bound_flag == 0 || out_of_bound_flag == 1 || out_of_bound_flag == 2);

    harp_interpolate_find_index(source_length, source_grid, target_grid_point, pos);

    if (*pos == -1)
    {
        /* grid point is before source_grid[0] */
        if (out_of_bound_flag == 1)
        {
            *target_value = source_array[0];
        }
        else if (out_of_bound_flag == 2)
        {
            v = log(target_grid_point / source_grid[0]) / log(source_grid[0] / source_grid[1]);
            *target_value = exp((1 + v) * log(source_array[0]) - v * log(source_array[1]));
        }
        else
        {
            *target_value = harp_nan();
        }
    }
    else if (*pos == source_length)
    {
        /* grid point is after source_grid[source_length - 1] */
        if (out_of_bound_flag == 1)
        {
            *target_value = source_array[source_length - 1];
        }
        else if (out_of_bound_flag == 2)
        {
            v = log(target_grid_point / source_grid[source_length - 1]) /
                log(source_grid[source_length - 1] / source_grid[source_length - 2]);
            *target_value = exp((1 + v) * log(source_array[source_length - 1]) -
                                v * log(source_array[source_length - 2]));
        }
        else
        {
            *target_value = harp_nan();
        }
    }
    else if (target_grid_point == source_grid[*pos])
    {
        /* don't interpolate, but take exact point */
        *target_value = source_array[*pos];
    }
    else if (target_grid_point == source_grid[*pos + 1])
    {
        /* don't interpolate, but take exact point */
        *target_value = source_array[*pos + 1];
    }
    else
    {
        /* grid point is between source_grid[pos] and source_grid[pos + 1] */
        v = log(target_grid_point / source_grid[*pos]) / log(source_grid[(*pos) + 1] / source_grid[*pos]);
        *target_value = exp((1 - v) * log(source_array[*pos]) + v * log(source_array[(*pos) + 1]));
    }
}

/* Interpolate single value from source grid to target point using log/log linear interpolation of the axis.
 * A log/log linear interpolation is a linear interpolation using log(source_grid), log(target_grid_point) and
 * log(source_array).
 * source_grid needs to be strict monotonic with values > 0. source_array and target_grid_point need to be > 0.
 * out_of_bound_flag:
 *  0: set value outside source_grid to NaN
 *  1: set value outside source_grid to edge value (i.e. source_array[0] or source_array[source_length - 1])
 *  2: extrapolate based on nearest two edge values
 */
void harp_interpolate_value_logloglinear(long source_length, const double *source_grid, const double *source_array,
                                         double target_grid_point, int out_of_bound_flag, double *target_value)
{
    long pos = 0;

    interpolate_logloglinear(source_length, source_grid, source_array, target_grid_point, out_of_bound_flag, &pos,
                             target_value);
}

/* Interpolate single value from source grid to target point using log/log linear interpolation of the axis.
 * A log/log linear interpolation is a linear interpolation on the arrays log(source_grid), log(target_grid) and
 * log(source_array).
 * Both source_grid and target_grid need to be strict monotonic with values > 0. source_array needs to be > 0.
 * This function will do 'the right thing' depending on whether a grid is increasing or decreasing.
 * out_of_bound_flag:
 *  0: set values outside source_grid to NaN
 *  1: set values outside source_grid to edge value (i.e. source_array[0] or source_array[source_length - 1])
 *  2: extrapolate based on nearest two edge values
 */
void harp_interpolate_array_logloglinear(long source_length, const double *source_grid, const double *source_array,
                                         long target_length, const double *target_grid, int out_of_bound_flag,
                                         double *target_array)
{
    long pos = 0;
    long i;

    for (i = 0; i < target_length; i++)
    {
        interpolate_logloglinear(source_length, source_grid, source_array, target_grid[i], out_of_bound_flag, &pos,
                                 &target_array[i]);
    }
}

/* Interpolate array from source grid to target grid using linear interpolation
 * Both source_grid_boundaries and target_grid_boundaries need to be strict monotonic.
 */
void harp_interval_interpolate_array_linear(long source_length, const double *source_grid_boundaries,
                                            const double *source_array, long target_length,
                                            const double *target_grid_boundaries, double *target_array)
{
    long i, j;

    for (i = 0; i < target_length; i++)
    {
        long num_valid_contributions = 0;
        double sum = 0.0;
        double xminb, xmaxb;

        if (target_grid_boundaries[2 * i] < target_grid_boundaries[2 * i + 1])
        {
            xminb = target_grid_boundaries[2 * i];
            xmaxb = target_grid_boundaries[2 * i + 1];
        }
        else
        {
            xminb = target_grid_boundaries[2 * i + 1];
            xmaxb = target_grid_boundaries[2 * i];
        }

        for (j = 0; j < source_length; j++)
        {
            double xmina, xmaxa;

            if (source_grid_boundaries[2 * j] < source_grid_boundaries[2 * j + 1])
            {
                xmina = source_grid_boundaries[2 * j];
                xmaxa = source_grid_boundaries[2 * j + 1];
            }
            else
            {
                xmina = source_grid_boundaries[2 * j + 1];
                xmaxa = source_grid_boundaries[2 * j];
            }

            if (!(xmina >= xmaxb || xminb >= xmaxa || harp_isnan(source_array[j])))
            {
                double xminc, xmaxc, weight;

                /* there is overlap, interval A is not empty, and interval A has a valid value */

                /* calculate intersection interval C of intervals A and B */
                xminc = xmina < xminb ? xminb : xmina;
                xmaxc = xmaxa > xmaxb ? xmaxb : xmaxa;

                weight = (xmaxc - xminc) / (xmaxa - xmina);
                sum += weight * source_array[j];
                num_valid_contributions++;
            }
        }

        if (num_valid_contributions != 0)
        {
            target_array[i] = sum;
        }
        else
        {
            target_array[i] = harp_nan();
        }
    }
}

/* Determine boundary intervals based on linear inter-/extrapolation of mid points.
 * Any trailing NaN values in the mid point array will be ignored (and corresponding bounds values will be set to NaN).
 * The bounds array will be treated as a [num_midpoints,2] array and should thus be allocated
 * to hold '2 * num_midpoints' values.
 * If num_midpoints equals 1, the two bounds values will be set equal to the midpoint value.
 * If extrapolate is 1 then the values of the first bound of the first midpoint (bound[0]) and the last bound of the
 * last midpoint (bound[2 * num_midpoints - 1]) will be set based on extrapolation of the nearest two midpoint values.
 * If extrapolate is 0 then the midpoint values will be used (i.e. bound[0] = midpoint[0] and
 * bound[2*num_midpoints-1] = midpoint[num_midpoints-1])
 */
void harp_bounds_from_midpoints_linear(long num_midpoints, const double *midpoints, int extrapolate, double *bounds)
{
    long i;

    while (num_midpoints > 0 && harp_isnan(midpoints[num_midpoints - 1]))
    {
        num_midpoints--;
        bounds[num_midpoints * 2] = harp_nan();
        bounds[num_midpoints * 2 + 1] = harp_nan();
    }

    if (num_midpoints < 1)
    {
        return;
    }
    if (num_midpoints == 1)
    {
        bounds[0] = midpoints[0];
        bounds[1] = midpoints[0];
        return;
    }

    for (i = 0; i < num_midpoints - 1; i++)
    {
        double average = 0.5 * (midpoints[i] + midpoints[i + 1]);

        bounds[2 * i + 1] = average;
        bounds[2 * (i + 1)] = average;
    }
    if (extrapolate)
    {
        bounds[0] = 0.5 * (3.0 * midpoints[0] - midpoints[1]);
        bounds[2 * (num_midpoints - 1) + 1] = 0.5 * (3.0 * midpoints[num_midpoints - 1] - midpoints[num_midpoints - 2]);
    }
    else
    {
        bounds[0] = midpoints[0];
        bounds[2 * (num_midpoints - 1) + 1] = midpoints[num_midpoints - 1];
    }
}

/* Determine boundary intervals based on loglinear inter-/extrapolation of mid points.
 * Any trailing NaN values in the mid point array will be ignored (and corresponding bounds values will be set to NaN).
 * The bounds array will be treated as a [num_midpoints,2] array and should thus be allocated
 * to hold '2 * num_midpoints' values.
 * If num_midpoints equals 1, the two bounds values will be set equal to the midpoint value.
 * If extrapolate is 1 then the values of the first bound of the first midpoint (bound[0]) and the last bound of the
 * last midpoint (bound[2 * num_midpoints - 1]) will be set based on extrapolation of the nearest two midpoint values.
 * If extrapolate is 0 then the midpoint values will be used (i.e. bound[0] = midpoint[0] and
 * bound[2*num_midpoints-1] = midpoint[num_midpoints-1])
 */
void harp_bounds_from_midpoints_loglinear(long num_midpoints, const double *midpoints, int extrapolate, double *bounds)
{
    long i;

    while (num_midpoints > 0 && harp_isnan(midpoints[num_midpoints - 1]))
    {
        num_midpoints--;
        bounds[num_midpoints * 2] = harp_nan();
        bounds[num_midpoints * 2 + 1] = harp_nan();
    }

    if (num_midpoints < 1)
    {
        return;
    }
    if (num_midpoints == 1)
    {
        bounds[0] = midpoints[0];
        bounds[1] = midpoints[0];
        return;
    }

    for (i = 0; i < num_midpoints - 1; i++)
    {
        double average = exp(0.5 * (log(midpoints[i]) + log(midpoints[i + 1])));

        bounds[2 * i + 1] = average;
        bounds[2 * (i + 1)] = average;
    }
    if (extrapolate)
    {
        bounds[0] = exp(0.5 * (3.0 * log(midpoints[0]) - log(midpoints[1])));
        bounds[2 * (num_midpoints - 1) + 1] =
            exp(0.5 * (3.0 * log(midpoints[num_midpoints - 1]) - log(midpoints[num_midpoints - 2])));
    }
    else
    {
        bounds[0] = midpoints[0];
        bounds[2 * (num_midpoints - 1) + 1] = midpoints[num_midpoints - 1];
    }
}
