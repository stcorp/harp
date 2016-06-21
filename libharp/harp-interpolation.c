/*
 * Copyright (C) 2015-2016 S[&]T, The Netherlands.
 *
 * This file is part of CODA.
 *
 * CODA is free software; you can redistribute it and/or modify
 * it unmin1der the terms of the GNU General Public License as published by
 * the Free Software Founmin1dation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * CODA is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with CODA; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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
 * returns 'index' such that target_grid_point is between source_grid[index] and source_grid[index+1].
 * source_grid[0...n-1] must be monotonic, either increasing or decreasing.
 * index = -1 or index = n-1 is returned to indicate that target_grid_point is out of range.
 * 'index' as input is taken as the initial guess for 'index' on output. */
void harp_interpolate_find_index(long source_length, const double *source_grid, double target_grid_point, long *index)
{
    long low;
    long high;
    long increment;
    int ascend;

    if (target_grid_point == source_grid[source_length - 1])
    {
        *index = source_length - 2;
        return;
    }

    if (target_grid_point == source_grid[0])
    {
        *index = 0;
        return;
    }

    low = *index;

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
        increment = 1;
        if ((target_grid_point >= source_grid[low]) == ascend)
        {
            if (low == source_length - 1)
            {
                *index = source_length - 1;
                return;
            }
            high = low + 1;
            while ((target_grid_point >= source_grid[high]) == ascend)
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
            while ((target_grid_point < source_grid[low]) == ascend)
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

        if ((target_grid_point >= source_grid[middle]) == ascend)
        {
            low = middle;
        }
        else
        {
            high = middle;
        }
    }

    *index = low;
}

/*  p4 +---+ p3
 *     |  +| <--- p
 *  p1 +---+ p2
 */
int harp_bilinear_interpolation(const double *source_grid_x, const double *source_grid_y, const double **source_value,
                                long m, long n, double target_x, double target_y, double *target_value)
{
    long i, j;

    /* Start hunt with with initial guess for ilo and jlo */
    i = 0;
    j = 0;

    harp_interpolate_find_index(n, source_grid_x, target_x, &i);
    harp_interpolate_find_index(m, source_grid_y, target_y, &j);

    if (i == -1 || i == m || j == -1 || j == n)
    {
        /* Do not use extrapolation  */
        *target_value = harp_nan();
    }
    else
    {
        double z1, z2, z3, z4;
        double x1, x2;
        double y1, y2;
        double dx, dy;

        x1 = source_grid_x[i];
        x2 = source_grid_x[i + 1];
        dx = x2 - x1;

        y1 = source_grid_y[j];
        y2 = source_grid_y[j + 1];
        dy = y2 - y1;

        z1 = source_value[i][j];
        z2 = source_value[i][j + 1];
        z3 = source_value[i + 1][j];
        z4 = source_value[i + 1][j + 1];

        /* Interpolate */
        *target_value = (z1 * (x2 - target_x) * (y2 - target_y) + z2 * (target_x - x1) * (y2 - target_y) +
                         z3 * (x2 - target_x) * (target_y - y1) + z4 * (target_x - x1) * (target_y - y1)) / (dx * dy);
    }

    return 0;
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
            *target_value = source_grid[0];
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
    else if (*pos == source_length - 1)
    {
        /* grid point is after source_grid[source_length - 1] */
        if (out_of_bound_flag == 1)
        {
            *target_value = source_grid[source_length - 1];
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
            *target_value = source_grid[0];
        }
        else if (out_of_bound_flag == 2)
        {
            v = (log(target_grid_point) - log(source_grid[0])) / (log(source_grid[0]) - log(source_grid[1]));
            *target_value = source_array[0] + v * (source_array[0] - source_array[1]);
        }
        else
        {
            *target_value = harp_nan();
        }
    }
    else if (*pos == source_length - 1)
    {
        /* grid point is after source_grid[source_length - 1] */
        if (out_of_bound_flag == 1)
        {
            *target_value = source_grid[source_length - 1];
        }
        else if (out_of_bound_flag == 2)
        {
            v = (log(target_grid_point) - log(source_grid[source_length - 1])) /
                (log(source_grid[source_length - 1]) - log(source_grid[source_length - 2]));
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
        v = (log(target_grid_point) - log(source_grid[*pos])) / (log(source_grid[(*pos) + 1]) - log(source_grid[*pos]));
        *target_value = (1 - v) * source_array[*pos] + v * source_array[(*pos) + 1];
    }
}

/* Interpolate single value from source grid to target point using log linear interpolation
 * source_grid needs to be strict monotonic.
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

/* Interpolate array from source grid to target grid using log linear interpolation
 * Both source_grid and target_grid need to be strict monotonic.
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

/* Interpolate array from source grid to target grid using linear interpolation
 * Both source_grid_boundaries and target_grid_boundaries need to be strict monotonic.
 */
int harp_interval_interpolate_array_linear(long source_length, const double *source_grid_boundaries,
                                           const double *source_array, long target_length,
                                           const double *target_grid_boundaries, double *target_array)
{
    long i, j;

    for (i = 0; i < target_length; i++)
    {
        target_array[i] = harp_nan();
    }

    for (i = 0; i < target_length; i++)
    {
        double sum = 0.0;
        long count_valid_contributions = 0;

        for (j = 0; j < source_length; j++)
        {
            double xmina = source_grid_boundaries[2 * j];
            double xmaxa = source_grid_boundaries[2 * j + 1];
            double xminb = target_grid_boundaries[2 * i];
            double xmaxb = target_grid_boundaries[2 * i + 1];

            if (!harp_isnan(source_array[i]))
            {
                harp_overlapping_scenario overlapping_scenario;
                double weight = 0.0;

                if (harp_determine_overlapping_scenario(xmina, xmaxa, xminb, xmaxb, &overlapping_scenario) != 0)
                {
                    return -1;
                }

                switch (overlapping_scenario)
                {
                    case harp_overlapping_scenario_no_overlap_b_a:
                    case harp_overlapping_scenario_no_overlap_a_b:
                        weight = 0.0;
                        break;
                    case harp_overlapping_scenario_overlap_a_equals_b:
                        weight = 1.0;
                        break;
                    case harp_overlapping_scenario_partial_overlap_a_b:
                        weight = (xmaxa - xminb) / (xmaxa - xmina);
                        break;
                    case harp_overlapping_scenario_partial_overlap_b_a:
                        weight = (xmaxb - xmina) / (xmaxa - xmina);
                        break;
                    case harp_overlapping_scenario_overlap_a_contains_b:
                        weight = (xmaxb - xminb) / (xmaxa - xmina);
                        break;
                    case harp_overlapping_scenario_overlap_b_contains_a:
                        weight = 1.0;
                        break;
                }

                sum += weight * source_array[i];
                count_valid_contributions++;
            }
        }

        if (count_valid_contributions != 0)
        {
            target_array[j] = sum;
        }
    }

    return 0;
}
