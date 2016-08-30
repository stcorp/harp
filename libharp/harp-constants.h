/*
 * Copyright (C) 2015-2016 S[&]T, The Netherlands.
 *
 * This file is part of HARP.
 *
 * The HARP Toolset is free software you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation either version 2 of the
 * License, or (at your option) any later version.
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

/* SI units [1] dimensionless */

/* Use format in between square brackets
 * that is used in UDUNITS-2 C-API:
 *
 *  String Type  Using Names            Using Symbols  Comment
 * ------------------------------------------------------------
 *  Simple       meter 	                m
 *  Raised       meter^2 	            m2             higher precedence than multiplying or dividing
 *  Product      newton meter 	        N.m
 *  Quotient     meter per second 	    m/s
 *  Scaled       60 second 	            60 s
 *  Prefixed     kilometer 	            km
 *  Offset       kelvin from 273.15 	K @ 273.15     lower precedence than multiplying or dividing
 *  Logarithmic  lg(re milliwatt) 	    lg(re mW)      "lg" is base 10, "ln" is base e, and "lb" is base 2
 *  Grouped      (5 meter)/(30 second)  (5 m)/(30 s)
 *
 */

/* Prefixes */
#define CONST_YOTTA 1.0e24
#define CONST_ZETTA 1.0e21
#define CONST_EXA 1.0e18
#define CONST_PETA 1.0e15
#define CONST_TERA 1.0e12
#define CONST_GIGA 1.0e9
#define CONST_MEGA 1.0e6
#define CONST_KILO 1.0e3
#define CONST_MILLI 1.0e-3
#define CONST_MICRO 1.0e-6
#define CONST_NANO 1.0e-9
#define CONST_PICO 1.0e-12
#define CONST_FEMTO 1.0e-15
#define CONST_ATTO 1.0e-18
#define CONST_ZEPTO 1.0e-21
#define CONST_YOCTO 1.0e-24

/* Mathematical constants */
#define CONST_RAD2DEG 57.29577951308232311024   /* 360/(2*pi) */
#define CONST_DEG2RAD 0.01745329251994329547437 /* (2*pi)/360 */

/* Physical constants */
#define CONST_NUM_AVOGADRO 6.022140857e23       /* [1/mol] (CODATA 2014) */
#define CONST_SPEED_OF_LIGHT 2.99792458e8       /* [m/s] */
#define CONST_GRAVITATIONAL_CONSTANT 6.67408e-11        /* [m3/kg s2] (CODATA 2014) */
#define CONST_PLANCKS_CONSTANT_H 6.626070040e-34        /* [kg.m2/s] (CODATA 2014) */
#define CONST_PLANCKS_CONSTANT_HBAR 1.054571800e-34     /* [kg.m2/s] (CODATA 2014) */
#define CONST_ELECTRON_VOLT 1.6021766208e-19    /* [kg.m2/s2] (CODATA 2014) */
#define CONST_MASS_ELECTRON 9.10938356e-31      /* [kg] (CODATA 2014) */
#define CONST_MASS_MUON 1.883531594e-28 /* [kg] (CODATA 2014) */
#define CONST_MASS_PROTON 1.672621898e-27       /* [kg] (CODATA 2014) */
#define CONST_MASS_NEUTRON 1.674927471e-27      /* [kg] (CODATA 2014) */
#define CONST_RYDBERG 2.179872325e-18   /* [kg.m2/s2] (CODATA 2014) */
#define CONST_BOLTZMANN 1.38064852e-23  /* [kg.m2/K.s2] (CODATA 2014) */
#define CONST_BOHR_MAGNETON 9.274009994e-24     /* [A.m2] (CODATA 2014) */
#define CONST_NUCLEAR_MAGNETON 5.050783699e-27  /* [A.m2] (CODATA 2014) */
#define CONST_ELECTRON_MAGNETIC_MOMENT 9.284764620e-24  /* [A.m2] (CODATA 2014) */
#define CONST_PROTON_MAGNETIC_MOMENT 1.4106067873e-26   /* [A.m2] (CODATA 2014) */
#define CONST_MOLAR_GAS 8.3144598       /* [kg.m2/K.mol.s2] = [J/K.mol] (CODATA 2014) */
#define CONST_MINUTE 6e1        /* [s] */
#define CONST_HOUR 3.6e3        /* [s] */
#define CONST_DAY 8.64e4        /* [s] */
#define CONST_WEEK 6.048e5      /* [s] */
#define CONST_INCH 2.54e-2      /* [m] */
#define CONST_FOOT 3.048e-1     /* [m] */
#define CONST_YARD 9.144e-1     /* [m] */
#define CONST_MILE 1.609344e3   /* [m] */
#define CONST_NAUTICAL_MILE 1.852e3     /* [m] */
#define CONST_FATHOM 1.8288e0   /* [m] */
#define CONST_MIL 2.54e-5       /* [m] */
#define CONST_POINT 3.52777777778e-4    /* [m] */
#define CONST_TEXPOINT 3.51459803515e-4 /* [m] */
#define CONST_MICRON 1e-6       /* [m] */
#define CONST_ANGSTROM 1e-10    /* [m] */
#define CONST_HECTARE 1e4       /* [m2] */
#define CONST_ACRE 4.04685642241e3      /* [m2] */
#define CONST_BARN 1e-28        /* [m2] */
#define CONST_LITER 1e-3        /* [m3] */
#define CONST_US_GALLON 3.78541178402e-3        /* [m3] */
#define CONST_QUART 9.46352946004e-4    /* [m3] */
#define CONST_PINT 4.73176473002e-4     /* [m3] */
#define CONST_CUP 2.36588236501e-4      /* [m3] */
#define CONST_FLUID_OUNCE 2.95735295626e-5      /* [m3] */
#define CONST_TABLESPOON 1.47867647813e-5       /* [m3] */
#define CONST_TEASPOON 4.92892159375e-6 /* [m3] */
#define CONST_CANADIAN_GALLON 4.54609e-3        /* [m3] */
#define CONST_UK_GALLON 4.546092e-3     /* [m3] */
#define CONST_MILES_PER_HOUR 4.4704e-1  /* [m/s] */
#define CONST_KILOMETERS_PER_HOUR 2.77777777778e-1      /* [m/s] */
#define CONST_KNOT 5.14444444444e-1     /* [m/s] */
#define CONST_POUND_MASS 4.5359237e-1   /* [kg] */
#define CONST_OUNCE_MASS 2.8349523125e-2        /* [kg] */
#define CONST_TON 9.0718474e2   /* [kg] */
#define CONST_METRIC_TON 1e3    /* [kg] */
#define CONST_UK_TON 1.0160469088e3     /* [kg] */
#define CONST_TROY_OUNCE 3.1103475e-2   /* [kg] */
#define CONST_CARAT 2e-4        /* [kg] */
#define CONST_UNIFIED_ATOMIC_MASS 1.660539040e-27       /* [kg] (CODATA 2014) */
#define CONST_GRAM_FORCE 9.80665e-3     /* [kg.m/s2] */
#define CONST_POUND_FORCE 4.44822161526e0       /* [kg.m/s2] */
#define CONST_KILOPOUND_FORCE 4.44822161526e3   /* [kg.m/s2] */
#define CONST_POUNDAL 1.38255e-1        /* [kg.m/s2] */
#define CONST_CALORIE 4.1868e0  /* [kg.m2/s2] */
#define CONST_BTU 1.05505585262e3       /* [kg.m2/s2] */
#define CONST_THERM 1.05506e8   /* [kg.m2/s2] */
#define CONST_HORSEPOWER 7.457e2        /* [kg.m2/s3] */
#define CONST_BAR 1e5   /* [kg/m.s2] */
#define CONST_PASCAL 1.0        /* [kg/m.s2] */
#define CONST_TORR 1.33322368421e2      /* [kg/m.s2] */
#define CONST_METER_OF_MERCURY 1.33322368421e5  /* [kg/m.s2] */
#define CONST_INCH_OF_MERCURY 3.38638815789e3   /* [kg/m.s2] */
#define CONST_INCH_OF_WATER 2.490889e2  /* [kg/m.s2] */
#define CONST_PSI 6.89475729317e3       /* [kg/m.s2] */
#define CONST_POISE 1e-1        /* [kg/m/s] */
#define CONST_STOKES 1e-4       /* [m2/s] */
#define CONST_FARADAY 96485.33289       /* [A.s/mol] (CODATA 2014) */
#define CONST_ELECTRON_CHARGE 1.6021766208e-19  /* [A.s] (CODATA 2014) */
#define CONST_GAUSS 1e-4        /* [kg/A.s2] */
#define CONST_STILB 1e4 /* [cd/m2] */
#define CONST_LUMEN 1e0 /* [cd.sr] */
#define CONST_LUX 1e0   /* [cd.sr/m2] */
#define CONST_PHOT 1e4  /* [cd.sr/m2] */
#define CONST_FOOTCANDLE 1.076e1        /* [cd.sr/m2] */
#define CONST_LAMBERT 1e4       /* [cd.sr/m2] */
#define CONST_FOOTLAMBERT 1.07639104e1  /* [cd.sr/m2] */
#define CONST_CURIE 3.7e10      /* [/s] */
#define CONST_ROENTGEN 2.58e-4  /* [A.s/kg] */
#define CONST_RAD 1e-2  /* [m2/s2] */
#define CONST_SOLAR_MASS 1.98892e30     /* [kg] */
#define CONST_BOHR_RADIUS 5.291772083e-11       /* [m] */
#define CONST_NEWTON 1e0        /* [kg.m/s2] */
#define CONST_DYNE 1e-5 /* [kg.m/s2] */
#define CONST_JOULE 1e0 /* [kg.m2/s2] */
#define CONST_ERG 1e-7  /* [kg.m2/s2] */
#define CONST_STEFAN_BOLTZMANN_CONSTANT 5.670367e-8     /* [W/m2.K4] (CODATA 2014) */
#define CONST_THOMSON_CROSS_SECTION 6.6524587158e-29    /* [m2] (CODATA 2014) */
#define CONST_VACUUM_PERMITTIVITY 8.854187817e-12       /* [A2.s4/kg.m3] */
#define CONST_VACUUM_PERMEABILITY 1.25663706144e-6      /* [kg.m/A2.s2] */
#define CONST_DEBYE 3.33564095198e-30   /* [A.s2/m2] */

/* Molecular constants [g/mol] */
#define CONST_MOLAR_MASS_DRY_AIR 28.9644
#define CONST_MOLAR_MASS_BrO 95.9034
#define CONST_MOLAR_MASS_BrO2 111.9028
#define CONST_MOLAR_MASS_CCl2F2 120.9135
#define CONST_MOLAR_MASS_CCl3F 137.3681
#define CONST_MOLAR_MASS_CF4 88.00431
#define CONST_MOLAR_MASS_CHClF2 86.4684
#define CONST_MOLAR_MASS_CH3Cl 50.4875
#define CONST_MOLAR_MASS_CH4 16.0425
#define CONST_MOLAR_MASS_CO 28.0101
#define CONST_MOLAR_MASS_COF2 66.0069
#define CONST_MOLAR_MASS_COS 60.0751
#define CONST_MOLAR_MASS_CO2 44.0095
#define CONST_MOLAR_MASS_C2H2 26.0373
#define CONST_MOLAR_MASS_C2H2O2 58.036163
#define CONST_MOLAR_MASS_C2H6 30.0690
#define CONST_MOLAR_MASS_C2H3NO5 121.04892
#define CONST_MOLAR_MASS_C3H8 44.09562
#define CONST_MOLAR_MASS_C5H8 68.11702
#define CONST_MOLAR_MASS_ClNO3 97.4579
#define CONST_MOLAR_MASS_ClO 51.4524
#define CONST_MOLAR_MASS_HCHO 30.026
#define CONST_MOLAR_MASS_HCOOH 46.0254
#define CONST_MOLAR_MASS_HCN 27.0253
#define CONST_MOLAR_MASS_HCl 36.4609
#define CONST_MOLAR_MASS_HF 20.006343
#define CONST_MOLAR_MASS_HNO2 47.013494
#define CONST_MOLAR_MASS_HNO3 63.0129
#define CONST_MOLAR_MASS_HNO4 79.0122
#define CONST_MOLAR_MASS_HOCl 52.4603
#define CONST_MOLAR_MASS_HO2 33.00674
#define CONST_MOLAR_MASS_H2O 18.0153
#define CONST_MOLAR_MASS_H2O_161 (1.00782503207 + 15.99491461956 + 1.00782503207)
#define CONST_MOLAR_MASS_H2O_162 (1.00782503207 + 15.99491461956 + 2.0141017778)
#define CONST_MOLAR_MASS_H2O_171 (1.00782503207 + 16.99913170 + 1.00782503207)
#define CONST_MOLAR_MASS_H2O_181 (1.00782503207 + 17.9991610 + 1.00782503207)
#define CONST_MOLAR_MASS_H2O2 34.01468
#define CONST_MOLAR_MASS_IO 142.903873
#define CONST_MOLAR_MASS_NO 30.00610
#define CONST_MOLAR_MASS_NOCl 65.4591
#define CONST_MOLAR_MASS_NO2 46.00550
#define CONST_MOLAR_MASS_NO3 62.0049
#define CONST_MOLAR_MASS_N2 28.01340
#define CONST_MOLAR_MASS_N2O 44.0129
#define CONST_MOLAR_MASS_N2O5 108.0104
#define CONST_MOLAR_MASS_OClO 67.4518
#define CONST_MOLAR_MASS_OH 17.00734
#define CONST_MOLAR_MASS_O2 32.000
#define CONST_MOLAR_MASS_O3 47.99820
#define CONST_MOLAR_MASS_O3_666 (15.99491461956 + 15.99491461956 + 15.99491461956)
#define CONST_MOLAR_MASS_O3_667 (15.99491461956 + 15.99491461956 + 16.99913170)
#define CONST_MOLAR_MASS_O3_668 (15.99491461956 + 15.99491461956 + 17.9991610)
#define CONST_MOLAR_MASS_O3_686 (15.99491461956 + 17.9991610 + 15.99491461956)
#define CONST_MOLAR_MASS_O4 63.9976
#define CONST_MOLAR_MASS_SF6 146.0554
#define CONST_MOLAR_MASS_SO2 64.0638
#define CONST_MEAN_MOLAR_MASS_WET_AIR 28.940

/* Atmospheric physics constants */
#define CONST_DOBSON_UNIT 2.6868e20     /* [molec/m2] */
#define CONST_STD_AIR_DENSITY 2.6867811e25      /* [molec/m3] Loschmidt constant = air density at standard T and p (CODATA 2014) */
#define CONST_STD_PRESSURE 1.01325e3    /* [hPa] (standard pressure consistent with Loschmidt constant) */
#define CONST_STD_TEMPERATURE 273.15    /* [K] (standard temperature consistent with Loschmidt constant)  */
#define CONST_STANDARD_GAS_VOLUME 22.413962e-3  /* [m3/mol] (for standard temperature and pressure) (CODATA 2014) */
#define CONST_GAS_SPECIFIC_WET_AIR 287.30       /* [J/kg/K] (= 1e3 * CONST_MOLAR_GAS / CONST_MEAN_MOLAR_MASS_WET_AIR) */
#define CONST_GAS_SPECIFIC_DRY_AIR 287.058      /* [J/kg/K] (= 1e3 * CONST_MOLAR_GAS / CONST_MOLAR_MASS_DRY_AIR) */
#define CONST_TOA_ALTITUDE 100.0e3      /* [m] altitude of the top of the atmosphere */

/* Astronomical constants */
#define CONST_ASTRONOMICAL_UNIT 1.49597870691e11        /* [m] */
#define CONST_LIGHT_YEAR 9.46053620707e15       /* [m] */
#define CONST_PARSEC 3.08567758135e16   /* [m] */

/* Sphere with WGS-84 radius */
#define CONST_GRAV_ACCEL 9.80665e0      /*  [m/s2] */
#define CONST_EARTH_RADIUS_WGS84_SPHERE 6371.0e3        /* Rearth [m] */
#define CONST_GRAV_ACCEL_45LAT_WGS84_SPHERE 9.80665e0   /* g0 [m/s2] */

/* WGS-84 Earth ellipsoid */
#define CONST_GRAVITATIONAL_CONSTANT_WGS84_ELLIPSOID 3986004.418e8      /* GM_earth, including atmosphere [m3/s2] */
#define CONST_ANGULAR_VELOCITY_WGS84_ELLIPSOID 7292115.0e-11    /* omega [rad/s] */
#define CONST_SEMI_MAJOR_AXIS_WGS84_ELLIPSOID 6378.1370e3       /* a [m] */
#define CONST_SEMI_MINOR_AXIS_WGS84_ELLIPSOID 6356.7523142e3    /* b [m] */
#define CONST_FLATTENING_WGS84_ELLIPSOID 0.003352811e0  /* f (a-b)/a [1] */
#define CONST_LINEAR_ECCENTRICITY_WGS84_ELLIPSOID 521.854008974e3       /* E a^2 - b^2 [m] */
#define CONST_ECCENTRICITY_WGS84_ELLIPSOID 0.081819e0   /* e E/a [1] */
#define CONST_GRAV_ACCEL_POLAR_WGS84_ELLIPSOID 9.8321849378e0   /* gp [m/s2] */
#define CONST_GRAV_ACCEL_EQUATOR_WGS84_ELLIPSOID 9.7803253359e0 /* ge [m/s2] */
#define CONST_SOMIGLIANA_WGS84_ELLIPSOID 1.93853e-3     /* ks (b/a) * (gp/ge)-1 [1] */
#define CONST_GRAV_RATIO_WGS84_ELLIPSOID 0.003449787e0  /* Gravity ratio mr w^2 a^2 b/ GM_earth [1] */
