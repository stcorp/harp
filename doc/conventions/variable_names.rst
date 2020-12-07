Variable names
==============

To allow HARP to perform automatic operations on variables, it imposes a strict naming convention for variables. This
naming convention applies to the variable name itself and is therefore fully complementary to naming conventions that
apply to the value of a variable attribute, such as ``standard_name`` (as specified by netCDF-CF).

Note that it is possible to use variables inside HARP products with names that do not follow the convention, but then
these variables may not be handled correctly by operations that you perform on the product. The general rule is that if
you have a quantity that can be represented by the naming convention below then you should use the HARP variable name
for it.


HARP defines the following variable names:

============================================= =============== =============== ======= ==== ======= ===== ==================== =======================================================================
Name                                          Prefixes        Postfixes       Quality Vert Lat/Lon Spect Default unit         Comments
============================================= =============== =============== ======= ==== ======= ===== ==================== =======================================================================
absolute_vorticity                                                            X       X    X             [1/s]
absorbing_aerosol_index                                                       X            X             []
aerosol_base_height                                                           X            X             [m]
aerosol_base_pressure                                                         X            X             [Pa]
aerosol_extinction_coefficient                surface                         X       X    X       X     [1/m]
aerosol_height                                                                X            X             [m]
aerosol_number_density                                                        X       X    X             [1/m3]
aerosol_column_number_density                                                 X       X    X             [1/m2]
aerosol_optical_depth                         stratospheric,                  X       X    X       X     []                   this is equal to 'aerosol optical thickness'
                                              tropospheric
aerosol_pressure                                                              X            X             [Pa]
aerosol_top_height                                                            X            X             [m]
aerosol_top_pressure                                                          X            X             [Pa]
<aerosol_size>_aerosol_number_density                                         X       X    X             [1/m3]
<aerosol_size>_aerosol_column_number_density                                  X       X    X             [1/m2]
<aerosol_size>_aerosol_extinction_coefficient surface                         X       X    X       X     [1/m]
<aerosol_size>_aerosol_optical_depth          stratospheric,                  X       X    X       X     []                   this is equal to 'aerosol optical thickness'
                                              tropospheric
<aerosol_type>_aerosol_base_height                                            X            X             [m]
<aerosol_type>_aerosol_base_pressure                                          X            X             [Pa]
<aerosol_type>_aerosol_extinction_coefficient surface                         X       X    X       X     [1/m]
<aerosol_type>_aerosol_height                                                 X            X             [m]
<aerosol_type>_aerosol_optical_depth          stratospheric,                  X       X    X       X     []                   this is equal to 'aerosol optical thickness'
                                              tropospheric
<aerosol_type>_aerosol_pressure                                               X            X             [Pa]
<aerosol_type>_aerosol_top_height                                             X            X             [m]
<aerosol_type>_aerosol_top_pressure                                           X            X             [m]
altitude                                      sensor,                         X       X    X             [m]
                                              surface
altitude_bounds                                                               X       X    X             [m]
angstrom_exponent                                                             X       X    X             []
area                                                                          X                          [m2]                 the size of an area defined by latitude/longitude bounds
backscatter_coefficient                       surface                         X       X    X       X     [1/m/sr]
cloud_albedo                                                                  X            X             []
cloud_base_albedo                                                             X            X             []
cloud_base_height                                                             X            X             [m]
cloud_base_pressure                                                           X            X             [Pa]
cloud_base_temperature                                                        X            X             [K]
cloud_fraction                                                                X            X             []
cloud_height                                                                  X            X             [m]
cloud_optical_depth                                                           X            X             []                   this is equal to 'cloud optical thickness'
cloud_pressure                                                                X            X             [Pa]
cloud_temperature                                                             X            X             [K]
cloud_top_albedo                                                              X            X             []
cloud_top_height                                                              X            X             [m]
cloud_top_pressure                                                            X            X             [Pa]
cloud_top_temperature                                                         X            X             [K]
cloud_type                                                                    X            X
collocation_index                                                                                                             zero-based index as provided in the collocation result file
column_density                                stratospheric,  amf, apriori,   X       X    X             [kg/m2]
                                              tropospheric    avk, dfs, sic
column_number_density                         stratospheric,  amf, apriori,   X       X    X             [molec/m2]
                                              tropospheric    avk, dfs, sic
count                                                                                                                         number of samples per bin for binning/averaging
datetime                                                                                                 [s since 2000-01-01]
datetime_length                                                                                          [s]
datetime_start                                                                                           [s since 2000-01-01]
datetime_stop                                                                                            [s since 2000-01-01]
density                                                                       X       X    X             [kg/m3]
extinction_coefficient                        surface                         X       X    X       X     [1/m]
frequency                                                                     X                    X     [Hz]
frequency_bounds                                                              X                    X     [Hz]
frequency_irradiance                                                          X                    X     [W/m2/Hz]
frequency_photon_irradiance                                                   X                    X     [1/s/m2/Hz]
frequency_photon_radiance                                                     X                    X     [1/s/sr/m2/Hz]
frequency_photon_transmittance                                                X                    X     []
frequency_radiance                                                            X                    X     [W/sr/m2/Hz]
frequency_transmittance                                                       X                    X     []
geoid_height                                                                  X            X             [m]
geopotential                                  surface                         X       X    X             [m2/s2]
geopotential_height                           surface                         X       X    X             [m]
geopotential_height_bounds                                                    X       X    X             [m]
gravity                                       surface                         X       X    X             [m/s2]
hlos_wind_velocity                            surface                         X       X    X             [m/s]                hlos means 'horizontal line of sight'
index                                                                                                                         zero-based index of the sample within the source product
integration_time                                                                      X    X       X     [s]                  provides measurement specific integration time
                                                                                                                              (at e.g. altitude or wavelength) compared to overal datetime_length;
                                                                                                                              only use if integration time differs from datetime_length;
                                                                                                                              integration_time longer than datetime_length that covers multiple
                                                                                                                              datetime values means replication of measured value in time dimension
land_type                                                                     X            X
latitude                                      sensor                          X            (lat)         [degree_north]
latitude_bounds                                                                            (lat)         [degree_north]
longitude                                     sensor                          X            (lon)         [degree_east]
longitude_bounds                                                                           (lon)         [degree_east]
meridional_wind_velocity                      surface                         X       X    X             [m/s]
molar_mass                                                                    X       X    X             [g/mol]              this is the molar mass of the total substance (it is defined by the
                                                                                                                              relation between the variables 'density' and 'number_density')
month                                                                                                                         category variable for month of year ('January', ..., 'December')
number_density                                surface                         X       X    X             [molec/m3]
optical_depth                                                                 X       X    X       X     []                   this is equal to 'optical thickness'
orbit_index                                                                                                                   the absolute orbit number for data from polar orbiting satellites
planetary_boundary_layer_height                                               X            X             [m]
potential_temperature                         surface                         X       X    X             [K]
pressure                                      surface                         X       X    X             [Pa]
pressure_bounds                                                               X       X    X             [Pa]
radiance                                                                      X                    X     [W/sr/m2]
reflectance                                                                   X                    X     []
relative_azimuth_angle                                                        X                          [degree]             absolute difference between sensor and solar azimuth angles
relative_humidity                             surface                         X       X    X             []
relative_vorticity                                                            X       X    X             []
scan_direction_type
scan_subindex
scattering_angle                                                              X                          [degree]
scene_albedo                                                                  X            X             []
scene_pressure                                                                X            X             [Pa]
scene_type                                                                    X            X
sensor_azimuth_angle                                                          X                          [degree]
sensor_elevation_angle                                                        X                          [degree]
sensor_name                                                                                                                   used mainly for ground based networks to provide a unique sensor id
sensor_zenith_angle                                                           X                          [degree]
site_name                                                                                                                     used for data of a specific named geographical location
solar_azimuth_angle                           sensor,                         X                          [degree]
                                              surface, toa
solar_declination_angle                                                                                  [degree]
solar_elevation_angle                         sensor,                         X                          [degree]
                                              surface, toa
solar_hour_angle                                                                                         [degree]
solar_irradiance                                                              X                    X     [W/m2]
solar_zenith_angle                            sensor,                         X                          [degree]
                                              surface, toa,
sun_normalized_radiance                                                       X                    X     [degree]
surface_albedo                                                                X            X       X     []
temperature                                   surface                         X       X    X             [K]
tropopause_altitude                                                           X            X             [m]                  altitude of the troposphere/stratosphere boundary location
tropopause_pressure                                                           X            X             [K]                  pressure level of the troposphere/stratosphere boundary location
validity                                                                                                                      validity flag for each time sample or whole product;
                                                                                                                              only to be used if validity flag is for multiple variables combined
viewing_azimuth_angle                                                         X                          [degree]
viewing_elevation_angle                                                       X                          [degree]
viewing_zenith_angle                                                          X                          [degree]
virtual_temperature                                                           X       X    X             [K]
wavelength                                                                    X                    X     [m]
wavelength_bounds                                                             X                    X     [m]
wavelength_irradiance                                                         X                    X     [W/m2/m]
wavelength_photon_irradiance                                                  X                    X     [1/s/m2/m]
wavelength_photon_radiance                                                    X                    X     [1/s/sr/m2/m]
wavelength_photon_transmittance                                               X                    X     []
wavelength_radiance                                                           X                    X     [W/sr/m2/m]
wavelength_transmittance                                                      X                    X     []
wavenumber                                                                    X                    X     [1/m]
wavenumber_bounds                                                             X                    X     [1/m]
wavenumber_irradiance                                                         X                    X     [Wm/m2]
wavenumber_photon_irradiance                                                  X                    X     [m/s/m2]
wavenumber_photon_radiance                                                    X                    X     [m/s/sr/m2]
wavenumber_photon_transmittance                                               X                    X     []
wavenumber_radiance                                                           X                    X     [Wm/sr/m2]
wavenumber_transmittance                                                      X                    X     []
weight                                                                                     X                                  weighting factors used for binning/averaging
wind_speed                                    surface                         X       X    X             [m/s]
wind_direction                                surface                         X       X    X             [degree]
year                                                                                                                          integer value representing a year
zonal_wind_velocity                           surface                         X       X    X             [m/s]
<species>_column_density                      stratospheric,  amf, apriori,   X       X    X             [kg/m2]
                                              tropospheric    avk, dfs, sic
<species>_slant_column_density                                                X            X             [kg/m2]
<pm>_column_density                           stratospheric,                  X       X    X             [kg/m2]
                                              tropospheric
<species>_column_number_density               stratospheric,  amf, apriori,   X       X    X             [molec/m2]
                                              tropospheric    avk, dfs, sic
<species>_slant_column_number_density                                         X            X             [molec/m2]
<species>_column_mass_mixing_ratio            stratospheric,                  X            X             [kg/kg]
                                              tropospheric
<species>_column_mass_mixing_ratio_dry_air    stratospheric,                  X            X             [kg/kg]
                                              tropospheric
<species>_column_volume_mixing_ratio          stratospheric,                  X            X             [ppv]
                                              tropospheric
<species>_column_volume_mixing_ratio_dry_air  stratospheric,                  X            X             [ppv]
                                              tropospheric
<species>_density                             surface                         X       X    X             [kg/m3]
<pm>_density                                  surface                         X       X    X             [kg/m3]
O3_effective_temperature                                                      X            X             [K]
<species>_mass_mixing_ratio                   surface         apriori, avk,   X       X    X             [kg/kg]
                                                              dfs, sic
<species>_mass_mixing_ratio_dry_air           surface         apriori, avk,   X       X    X             [kg/kg]
                                                              dfs, sic
<species>_number_density                      surface         apriori, avk,   X       X    X             [molec/m3]
                                                              dfs, sic
<species>_partial_pressure                    surface                         X       X    X             [Pa]
<species>_partial_pressure_dry_air            surface                         X       X    X             [Pa]
<species>_volume_mixing_ratio                 surface         apriori, avk,   X       X    X             [ppv]                this is equal to 'number mixing ratio'
                                                              dfs, sic
<species>_volume_mixing_ratio_dry_air         surface         apriori, avk,   X       X    X             [ppv]
                                                              dfs, sic
============================================= =============== =============== ======= ==== ======= ===== ==================== =======================================================================

The supported aerosol sizes are:

============ ====================================================
Aerosol size Description
============ ====================================================
ultrafine    particles < 0.1 um
fine         particles < threshold, 0.5 um <= threshold <= 2.5 um
coarse       particles > threshold, 0.5 um <= threshold <= 2.5 um
============ ====================================================

The supported aerosol types are:

============== =================
Aerosol type   Description
============== =================
sea_salt       sea salt
dust           dust
organic_matter organic matter
black_carbon   black carbon
sulphate       sulphate
============== =================

The supported PM (particulate matter) types are:

===== ==================================
Name  Description
===== ==================================
PM1   particulate matter with d < 1 um
PM2p5 particulate matter with d < 2.5 um
PM10  particulate matter with d < 10 um
===== ==================================

The supported species are:

======== ============================= ===========================
Name     Description                   Aliases (not used by HARP)
======== ============================= ===========================
dry_air  dry air
BrO      bromine oxide
BrO2     bromine dioxide
CCl2F2   dichlorodifluoromethane       freon-12, CFC-12, R-12, F12
CCl3F    trichlorofluoromethane        freon-11, CFC-11, R-11, F11
CCl4     tetrachloromethane
CF4      tetrafluoromethane            CFC-14, F14
CHClF2   chlorodifluoromethane         HCFC-22, R-22, F22
CH3Cl    chloromethane,                HCC-40, R-40
         methyl chloride
CH3CN    acetonitrile,
         methyl cyanide
CH3OH    methanol
CH4      methane
CO       carbon monoxide
COF2     carbonyl fluoride
COS      carbonyl sulfide              OCS
CO2      carbon dioxide
C2H2     acetylene                     HCCH
C2H2O2   glyoxal                       OCHCHO, CHOCHO
C2H3NO5  peroxyacetyl nitrate          PAN
C2H6     ethane
C3H8     propane
C5H8     isoprene
ClNO3    chlorine nitrate
ClO      chlorine monoxide
HCHO     formaldehyde                  CH2O, H2CO
HCOOH    formic acid                   HCO2H
HCN      hydrogen cyanide
HCl      hydrogen chloride
HF       hydrogen fluoride
HNO2     nitrous acid
HNO3     nitric acid
HNO4     peroxynitric acid
HOCl     hypochlorous acid
HO2      hydroperoxyl
H2O      water
H2O_161  water (H1/O16/H1 isotopes)
H2O_162  water (H1/O16/H2 isotopes)    HDO
H2O_171  water (H1/O17/H1 isotopes)
H2O_181  water (H1/O18/H1 isotopes)
H2O2     hydrogen peroxide
IO       hypoiodite
IWC      ice water content;
         H2O in clouds in ice state
LWC      liquid water content;
         H2O in clouds in liquid state
NH3      ammonia
NO       nitric oxide
NOCl     nitrosyl chloride
NO2      nitrogen dioxide
NO3      nitrate
N2       nitrogen gas
N2O      nitrous oxide                 NOS
N2O5     dinitrogen pentoxide
OClO     chlorine dioxide              ClO2
OH       hydroxyl
O2       oxygen
O3       ozone
O3_666   ozone (O16/O16/O16 isotopes)
O3_667   ozone (O16/O16/O17 isotopes)
O3_668   ozone (O16/O16/O18 isotopes)
O3_686   ozone (O16/O18/O16 isotopes)
O4       tetraoxygen, oxozone
RWC      rain water content;
         H2O as rain
SF6      sulfur hexafluoride
SO2      sulfur dioxide
SWC      snow water content;
         H2O as snow/ice
======== ============================= ===========================

Variables for which a prefix and/or postfix is provided can have any of the given prefixes and/or any of the given
postfixes (separated by underscores). It is not allowed to provide more than one prefix or more than one postfix.
Variables having an 'X' in the Quality column can have any of the following additional versions of the variable
(where `<variable>` can include any of the allowed prefix and/or postfix combinations):

- <variable>_covariance
- <variable>_uncertainty
- <variable>_uncertainty_random
- <variable>_uncertainty_systematic
- <variable>_validity

Some examples of valid variable names are: ``tropospheric_O3_column_number_density``,
``tropospheric_O3_column_number_density_apriori``, ``O3_column_number_density_apriori``,
``tropospheric_O3_column_number_density_uncertainty``, ``O3_column_number_density_apriori_uncertainty``.

The `Vert`, `Lat/Lon`, and `Spec` columns indicate whether a variable can be dependent on the ``vertical``,
``latitude`` & ``longitude``, and/or ``spectral`` dimensions (any variable can be dependent on the ``time`` dimension).


**surface quantities**

The 'surface' prefix should only be used when quantities are combined together with quantities that have a vertical dimension.
If a product just contains surface quantities then don't use a 'surface' prefix but just omit the vertical dimension and
indicate the vertical level (i.e. location of the surface) using a 'pressure', 'altitude', and/or 'geopotential_height' variable.

Surface wind velocity variables are actually near-surface wind velocities (usually at surface_altitude + 10m).


**azimuth angles**

All (horizontal) azimuth angles in HARP should follow the convention that 0 is North facing
and the angle is increasing when moving Eastwards (i.e. clockwise).
Wind direction follows the same rules as for azimuth angles (North = 0, East = 90 degrees),
but the direction indicates where the wind is coming *from*.


**differences**

In addition to the conventions above there can also be variables that describe a 'difference'.
These difference variables can only be used to describe differences of the same quantity between different datasets
('x' and 'y') and only for variables that have a unit.
All difference variables in a single product should apply to the same datasets 'x' and 'y'
(i.e. the difference variables should only reflect a single comparison of datasets;
you should not combine one difference variable for 'x-y' and another for 'x-z' (even for different quantities)
within the same product).
A difference variable is indicated by a postfix.
The 'difference postfix' can come before a 'quality postfix' if we are talking about the 'quality of the difference'.
If the 'difference postfix' comes after a 'quality postfix' then we are talking about the 'difference of the quality quantity'.
The supported differences are:

- <variable>_diff (:math:`x-y`)
- <variable>_diffrelx (:math:`\frac{x-y}{|x|}`)
- <variable>_diffrely (:math:`\frac{x-y}{|y|}`)
- <variable>_diffrelmin (:math:`\frac{x-y}{\min(|x|,|y|)}`)
- <variable>_diffrelmax (:math:`\frac{x-y}{\max(|x|,|y|)}`)
- <variable>_diffrelavg (:math:`\frac{2(x-y)}{|x|+|y|}`)
- <variable>_diffabs (:math:`|x-y|`)
- <variable>_diffabsrelx (:math:`\frac{|x-y|}{|x|}`)
- <variable>_diffabsrely (:math:`\frac{|x-y|}{|y|}`)
- <variable>_diffabsrelmin (:math:`\frac{|x-y|}{\min(|x|,|y|)}`)
- <variable>_diffabsrelmax (:math:`\frac{|x-y|}{\max(|x|,|y|)}`)
- <variable>_diffabsrelavg (:math:`\frac{2|x-y|}{|x|+|y|}`)


**statistics**

There are also 'postfix' variables available for statistics.
HARP only provides naming conventions for statistical quantities that can be propagated
(i.e. deriving statistics of a joined set of values based on statistics of disjoint subsets of those values).
Quantities like count, standard deviation, skewness, kurtosis, minimum, and maximum, can be propagated, but median and IQR cannot.
Variances should be stored as standard deviations.
For the mean of a value, the original variable name itself is used. Other quantities are indicated by a postfix:

- <variable>_count
- <variable>_weight
- <variable>_stddev (sample standard deviation)
- <variable>_skewness
- <variable>_kurtosis
- <variable>_min
- <variable>_max

The 'count' and 'weight' are also available as variables on their own.
The variable-specific postfix versions of 'count' and 'weight' should only be used
when filtering out invalid values of a variable during binning/averaging would result
in different count/weight values.


**vertical profiles**

The postfix 'avk' is used for averaging kernels of atmospheric vertical profiles.
An AVK that only depends once on the vertical dimension is a column averaging kernel,
and an AVK that depends twice on the vertical dimension is a profile averaging kernel.
The 'amf' postfix is used for air mass factors.
The 'dfs' postfix is used for the 'degree of freedom for signal' for vertical profiles which equals the trace or
diagonal of the two-dimensional AVK and provides information on the vertical resolution and information content of
profiles.
The 'sic' postfix is used for the 'Shannon information content' for vertical profiles which can be derived from the
two-dimensional AVK.

