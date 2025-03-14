/*
 * Copyright (C) 2015-2025 S[&]T, The Netherlands.
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

#include <string.h>

static const double altitude[] = {
    0.0,
    1000.0,
    2000.0,
    3000.0,
    4000.0,
    5000.0,
    6000.0,
    7000.0,
    8000.0,
    9000.0,
    10000.0,
    11000.0,
    12000.0,
    13000.0,
    14000.0,
    15000.0,
    16000.0,
    17000.0,
    18000.0,
    19000.0,
    20000.0,
    21000.0,
    22000.0,
    23000.0,
    24000.0,
    25000.0,
    27500.0,
    30000.0,
    32500.0,
    35000.0,
    37500.0,
    40000.0,
    42500.0,
    45000.0,
    47500.0,
    50000.0,
    55000.0,
    60000.0,
    65000.0,
    70000.0,
    75000.0,
    80000.0,
    85000.0,
    90000.0,
    95000.0,
    100000.0,
    105000.0,
    110000.0,
    115000.0,
    120000.0
};

static const double pressure[] = {
    1013.00000,
    898.79999,
    795.00000,
    701.20001,
    616.59998,
    540.50000,
    472.20001,
    411.10001,
    356.50000,
    308.00000,
    265.00000,
    227.00000,
    194.00000,
    165.80000,
    141.70000,
    121.10000,
    103.50000,
    88.50000,
    75.65000,
    64.67000,
    55.29000,
    47.29000,
    40.47000,
    34.67000,
    29.72000,
    25.49000,
    17.43000,
    11.97000,
    8.01000,
    5.74600,
    4.15000,
    2.87100,
    2.06000,
    1.49100,
    1.09000,
    0.79780,
    0.42500,
    0.21900,
    0.10900,
    0.05220,
    0.02400,
    0.01050,
    0.00446,
    0.00184,
    0.00076,
    0.00032,
    0.00014,
    0.00007,
    0.00004,
    0.00003
};

static const double temperature[] = {
    288.200,
    281.700,
    275.200,
    268.700,
    262.200,
    255.700,
    249.200,
    242.700,
    236.200,
    229.700,
    223.300,
    216.800,
    216.700,
    216.700,
    216.700,
    216.700,
    216.700,
    216.700,
    216.700,
    216.700,
    216.700,
    217.600,
    218.600,
    219.600,
    220.600,
    221.600,
    224.000,
    226.500,
    230.000,
    236.500,
    242.900,
    250.400,
    257.300,
    264.200,
    270.600,
    270.700,
    260.800,
    247.000,
    233.300,
    219.600,
    208.400,
    198.600,
    188.900,
    186.900,
    188.400,
    195.100,
    208.800,
    240.000,
    300.000,
    360.000
};

static const double air_number_density[] = {
    2.545818E+19,
    2.310936E+19,
    2.092331E+19,
    1.890105E+19,
    1.703267E+19,
    1.531006E+19,
    1.372429E+19,
    1.226845E+19,
    1.093179E+19,
    9.711841E+18,
    8.595457E+18,
    7.583652E+18,
    6.484174E+18,
    5.541629E+18,
    4.736121E+18,
    4.047595E+18,
    3.459340E+18,
    2.957987E+18,
    2.528494E+18,
    2.161503E+18,
    1.847990E+18,
    1.574064E+18,
    1.340895E+18,
    1.143492E+18,
    9.757872E+17,
    8.331283E+17,
    5.635873E+17,
    3.827699E+17,
    2.522415E+17,
    1.759731E+17,
    1.237464E+17,
    8.304447E+16,
    5.798815E+16,
    4.087489E+16,
    2.917498E+16,
    2.134605E+16,
    1.180302E+16,
    6.421832E+15,
    3.383947E+15,
    1.721670E+15,
    8.341139E+14,
    3.829322E+14,
    1.710073E+14,
    7.130506E+13,
    2.921760E+13,
    1.187967E+13,
    5.029784E+12,
    2.142688E+12,
    9.681328E+11,
    5.110260E+11
};

static const double ch4_number_density[] = {
    4.327934e+13,
    3.928633e+13,
    3.557000e+13,
    3.213212e+13,
    2.895583e+13,
    2.602736e+13,
    2.333153e+13,
    2.084430e+13,
    1.855145e+13,
    1.644231e+13,
    1.448349e+13,
    1.270275e+13,
    1.077681e+13,
    9.116073e+12,
    7.701011e+12,
    6.496457e+12,
    5.472732e+12,
    4.593800e+12,
    3.845878e+12,
    3.199057e+12,
    2.631564e+12,
    2.132878e+12,
    1.705636e+12,
    1.361914e+12,
    1.090941e+12,
    8.789594e+11,
    5.562664e+11,
    3.497022e+11,
    2.093626e+11,
    1.312773e+11,
    8.189618e+10,
    4.682095e+10,
    2.675601e+10,
    1.484183e+10,
    8.090304e+09,
    4.482717e+09,
    1.947518e+09,
    9.632848e+08,
    5.075973e+08,
    2.582532e+08,
    1.251184e+08,
    5.744042e+07,
    2.565136e+07,
    9.982811e+06,
    3.798327e+06,
    1.425576e+06,
    5.532819e+05,
    2.035574e+05,
    5.808856e+04,
    1.533094e+04
};

static const double co_number_density[] = {
    4.327934e+13,
    3.928633e+13,
    3.557000e+13,
    3.213212e+13,
    2.895583e+13,
    2.602736e+13,
    2.333153e+13,
    2.084430e+13,
    1.855145e+13,
    1.644231e+13,
    1.448349e+13,
    1.270275e+13,
    1.077681e+13,
    9.116073e+12,
    7.701011e+12,
    6.496457e+12,
    5.472732e+12,
    4.593800e+12,
    3.845878e+12,
    3.199057e+12,
    2.631564e+12,
    2.132878e+12,
    1.705636e+12,
    1.361914e+12,
    1.090941e+12,
    8.789594e+11,
    5.562664e+11,
    3.497022e+11,
    2.093626e+11,
    1.312773e+11,
    8.189618e+10,
    4.682095e+10,
    2.675601e+10,
    1.484183e+10,
    8.090304e+09,
    4.482717e+09,
    1.947518e+09,
    9.632848e+08,
    5.075973e+08,
    2.582532e+08,
    1.251184e+08,
    5.744042e+07,
    2.565136e+07,
    9.982811e+06,
    3.798327e+06,
    1.425576e+06,
    5.532819e+05,
    2.035574e+05,
    5.808856e+04,
    1.533094e+04
};

static const double co2_number_density[] = {
    8.408400E+15,
    7.632900E+15,
    6.910200E+15,
    6.240300E+15,
    5.623200E+15,
    5.055600E+15,
    4.530900E+15,
    4.052400E+15,
    3.610200E+15,
    3.207270E+15,
    2.838660E+15,
    2.504370E+15,
    2.141370E+15,
    1.830180E+15,
    1.563870E+15,
    1.336500E+15,
    1.142460E+15,
    9.768000E+14,
    8.349000E+14,
    7.137900E+14,
    6.101700E+14,
    5.197500E+14,
    4.428600E+14,
    3.775200E+14,
    3.222450E+14,
    2.751210E+14,
    1.861200E+14,
    1.263900E+14,
    8.329200E+13,
    5.811300E+13,
    4.085400E+13,
    2.742300E+13,
    1.914990E+13,
    1.349700E+13,
    9.636000E+12,
    7.048799E+12,
    3.897300E+12,
    2.120580E+12,
    1.117380E+12,
    5.685899E+11,
    2.754510E+11,
    1.256896E+11,
    5.475200E+10,
    2.212160E+10,
    7.894800E+09,
    2.318550E+09,
    5.536300E+08,
    1.286400E+08,
    3.875200E+07,
    1.789900E+07
};

static const double h2o_number_density[] = {
    1.973426E+17,
    1.404222E+17,
    9.697315E+16,
    6.017162E+16,
    3.677232E+16,
    2.140204E+16,
    1.270574E+16,
    7.024160E+15,
    4.011698E+15,
    1.538518E+15,
    6.017959E+14,
    2.741906E+14,
    1.236803E+14,
    6.017410E+13,
    2.808805E+13,
    2.025000E+13,
    1.367490E+13,
    1.139600E+13,
    9.677249E+12,
    8.327550E+12,
    7.211100E+12,
    6.260625E+12,
    5.455230E+12,
    4.804800E+12,
    4.198950E+12,
    3.689123E+12,
    2.580300E+12,
    1.809675E+12,
    1.217830E+12,
    8.628900E+11,
    6.128100E+11,
    4.175775E+11,
    2.988545E+11,
    2.137025E+11,
    1.533000E+11,
    1.116060E+11,
    6.023100E+10,
    3.052350E+10,
    1.422120E+10,
    6.030500E+09,
    2.358028E+09,
    7.855600E+08,
    2.275630E+08,
    6.065600E+07,
    1.578960E+07,
    4.756001E+06,
    1.711220E+06,
    6.003200E+05,
    2.325120E+05,
    1.022800E+05
};

static const double n2o_number_density[] = {
    4.327934e+13,
    3.928633e+13,
    3.557000e+13,
    3.213212e+13,
    2.895583e+13,
    2.602736e+13,
    2.333153e+13,
    2.084430e+13,
    1.855145e+13,
    1.644231e+13,
    1.448349e+13,
    1.270275e+13,
    1.077681e+13,
    9.116073e+12,
    7.701011e+12,
    6.496457e+12,
    5.472732e+12,
    4.593800e+12,
    3.845878e+12,
    3.199057e+12,
    2.631564e+12,
    2.132878e+12,
    1.705636e+12,
    1.361914e+12,
    1.090941e+12,
    8.789594e+11,
    5.562664e+11,
    3.497022e+11,
    2.093626e+11,
    1.312773e+11,
    8.189618e+10,
    4.682095e+10,
    2.675601e+10,
    1.484183e+10,
    8.090304e+09,
    4.482717e+09,
    1.947518e+09,
    9.632848e+08,
    5.075973e+08,
    2.582532e+08,
    1.251184e+08,
    5.744042e+07,
    2.565136e+07,
    9.982811e+06,
    3.798327e+06,
    1.425576e+06,
    5.532819e+05,
    2.035574e+05,
    5.808856e+04,
    1.533094e+04
};

static const double no2_number_density[] = {
    5.860400E+08,
    5.319900E+08,
    4.816201E+08,
    4.349300E+08,
    3.919200E+08,
    3.523600E+08,
    3.157900E+08,
    2.824400E+08,
    2.516200E+08,
    2.254808E+08,
    2.047276E+08,
    1.988318E+08,
    2.044035E+08,
    2.467970E+08,
    3.544772E+08,
    6.925500E+08,
    1.104378E+09,
    1.536240E+09,
    1.950630E+09,
    2.292780E+09,
    2.570110E+09,
    2.772000E+09,
    2.898720E+09,
    2.951520E+09,
    2.988090E+09,
    3.118038E+09,
    2.712840E+09,
    2.359280E+09,
    1.819804E+09,
    1.282008E+09,
    7.749880E+08,
    3.348930E+08,
    1.259251E+08,
    4.703500E+07,
    1.944720E+07,
    9.462480E+06,
    4.003590E+06,
    1.831410E+06,
    8.566580E+05,
    3.980130E+05,
    1.794605E+05,
    7.740640E+04,
    3.285120E+04,
    1.305888E+04,
    5.146240E+03,
    2.021300E+03,
    8.254120E+02,
    3.408960E+02,
    1.501640E+02,
    7.722140E+01
};

static const double o2_number_density[] = {
    5.325320E+18,
    4.834170E+18,
    4.376460E+18,
    3.952190E+18,
    3.561360E+18,
    3.201880E+18,
    2.869570E+18,
    2.566520E+18,
    2.286460E+18,
    2.031271E+18,
    1.797818E+18,
    1.586101E+18,
    1.356201E+18,
    1.159114E+18,
    9.904510E+17,
    8.464500E+17,
    7.235580E+17,
    6.186399E+17,
    5.287700E+17,
    4.520670E+17,
    3.864410E+17,
    3.291750E+17,
    2.804780E+17,
    2.390960E+17,
    2.040885E+17,
    1.742433E+17,
    1.178760E+17,
    8.004700E+16,
    5.275160E+16,
    3.680490E+16,
    2.587420E+16,
    1.736790E+16,
    1.212827E+16,
    8.548099E+15,
    6.102800E+15,
    4.464240E+15,
    2.468290E+15,
    1.343034E+15,
    7.076740E+14,
    3.601070E+14,
    1.744523E+14,
    8.008879E+13,
    3.422000E+13,
    1.355840E+13,
    5.263200E+12,
    1.902400E+12,
    7.046200E+11,
    2.572800E+11,
    9.106720E+10,
    3.707650E+10
};

static const double o3_number_density[] = {
    6.777680E+11,
    6.779402E+11,
    6.778279E+11,
    6.274337E+11,
    5.771448E+11,
    5.772576E+11,
    5.645776E+11,
    6.151052E+11,
    6.526804E+11,
    8.910379E+11,
    1.129443E+12,
    1.630876E+12,
    2.008345E+12,
    2.132992E+12,
    2.383717E+12,
    2.634525E+12,
    3.012286E+12,
    3.513520E+12,
    4.015110E+12,
    4.390890E+12,
    4.768571E+12,
    4.769100E+12,
    4.894274E+12,
    4.768192E+12,
    4.518265E+12,
    4.266877E+12,
    3.272892E+12,
    2.509799E+12,
    1.860945E+12,
    1.380096E+12,
    9.656401E+11,
    6.066300E+11,
    3.597860E+11,
    2.147250E+11,
    1.197200E+11,
    6.621600E+10,
    2.125800E+10,
    7.068600E+09,
    2.370200E+09,
    5.169000E+08,
    2.086750E+08,
    1.149600E+08,
    8.555000E+07,
    4.995200E+07,
    2.046800E+07,
    4.756001E+06,
    1.006600E+06,
    1.072000E+05,
    4.844000E+03,
    2.557000E+02
};

int harp_aux_usstd76_get_profile(const char *name, int *num_vertical, const double **values)
{

    if (strcmp(name, "altitude") == 0)
    {
        *values = altitude;
    }
    else if (strcmp(name, "pressure") == 0)
    {
        *values = pressure;
    }
    else if (strcmp(name, "temperature") == 0)
    {
        *values = temperature;
    }
    else if (strcmp(name, "number_density") == 0)
    {
        *values = air_number_density;
    }
    else if (strcmp(name, "CH4_number_density") == 0)
    {
        *values = ch4_number_density;
    }
    else if (strcmp(name, "CO_number_density") == 0)
    {
        *values = co_number_density;
    }
    else if (strcmp(name, "CO2_number_density") == 0)
    {
        *values = co2_number_density;
    }
    else if (strcmp(name, "H2O_number_density") == 0)
    {
        *values = h2o_number_density;
    }
    else if (strcmp(name, "N2O_number_density") == 0)
    {
        *values = n2o_number_density;
    }
    else if (strcmp(name, "NO2_number_density") == 0)
    {
        *values = no2_number_density;
    }
    else if (strcmp(name, "O2_number_density") == 0)
    {
        *values = o2_number_density;
    }
    else if (strcmp(name, "O3_number_density") == 0)
    {
        *values = o3_number_density;
    }
    else
    {
        harp_set_error(HARP_ERROR_INVALID_ARGUMENT, "US standard 76 climatology does not have a profile for '%s'",
                       name);
        return -1;
    }

    *num_vertical = 50;

    return 0;
}
