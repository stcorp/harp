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

============================================= =============== =============== ======= ==== ======= ===== =======================================================================
Name                                          Prefixes        Postfixes       Quality Vert Lat/Lon Spect Comments
============================================= =============== =============== ======= ==== ======= ===== =======================================================================
absolute_vorticity                                                            X       X    X
absorbing_aerosol_index                                                       X            X
aerosol_base_height                                                           X            X
aerosol_base_pressure                                                         X            X
aerosol_extinction_coefficient                surface                         X       X    X       X
aerosol_height                                                                X            X
aerosol_optical_depth                         stratospheric,                  X       X    X       X     this is equal to 'aerosol optical thickness'
                                              tropospheric
aerosol_pressure                                                              X            X
aerosol_top_height                                                            X            X
aerosol_top_pressure                                                          X            X
<aerosol_type>_aerosol_base_height                                            X            X
<aerosol_type>_aerosol_base_pressure                                          X            X
<aerosol_type>_aerosol_extinction_coefficient surface                         X       X    X       X
<aerosol_type>_aerosol_height                                                 X            X
<aerosol_type>_aerosol_optical_depth          stratospheric,                  X       X    X       X     this is equal to 'aerosol optical thickness'
                                              tropospheric
<aerosol_type>_aerosol_pressure                                               X            X
<aerosol_type>_aerosol_top_height                                             X            X
<aerosol_type>_aerosol_top_pressure                                           X            X
altitude                                      sensor,                         X       X    X
                                              surface
altitude_bounds                                                               X       X    X
area                                                                          X                          the size of an area defined by latitude/longitude bounds
backscatter_coefficient                       surface                         X       X    X       X
cloud_albedo                                                                  X            X
cloud_base_albedo                                                             X            X
cloud_base_height                                                             X            X
cloud_base_pressure                                                           X            X
cloud_base_temperature                                                        X            X
cloud_fraction                                                                X            X
cloud_height                                                                  X            X
cloud_optical_depth                                                           X            X             this is equal to 'cloud optical thickness'
cloud_pressure                                                                X            X
cloud_temperature                                                             X            X
cloud_top_albedo                                                              X            X
cloud_top_height                                                              X            X
cloud_top_pressure                                                            X            X
cloud_top_temperature                                                         X            X
collocation_index                                                                                        zero-based index as provided in the collocation result file
column_density                                stratospheric,  amf, apriori,   X       X    X             this is the mass density
                                              tropospheric    avk, dfs
column_number_density                         stratospheric,  amf, apriori,   X       X    X
                                              tropospheric    avk, dfs
count
datetime
datetime_length                                                                       X
datetime_start
datetime_stop
density                                                                       X       X    X             this is the mass density
extinction_coefficient                        surface                         X       X    X       X
frequency                                                                     X
frequency_irradiance                                                          X                    X
frequency_photon_irradiance                                                   X                    X
frequency_photon_radiance                                                     X                    X
frequency_photon_transmittance                                                X                    X
frequency_radiance                                                            X                    X
frequency_transmittance                                                       X                    X
geoid_height                                                                  X            X
geopotential                                  surface                         X       X    X
geopotential_height                           surface                         X       X    X
hlos_wind_velocity                            surface                         X       X    X             hlos means 'horizontal line of sight'
index                                                                                                    zero-based index of the sample within the source product
integration_time                                                                      X    X       X     provides measurement specific integration time
                                                                                                         (at e.g. altitude or wavelength) compared to overal datetime_length;
                                                                                                         only use if integration time differs from datetime_length;
                                                                                                         integration_time longer than datetime_length that covers multiple
                                                                                                         datetime values means replication of measured value in time dimension
latitude                                      sensor                          X            (lat)
latitude_bounds                                                                            (lat)
longitude                                     sensor                          X            (lon)
longitude_bounds                                                                           (lon)
molar_mass                                                                    X       X    X             this is the molar mass of the total substance (it is defined by the
                                                                                                         relation between the variables 'density' and 'number_density')
month                                                                                                    category variable for month of year ('January', ..., 'December')
number_density                                surface                         X       X    X
optical_depth                                                                 X       X    X       X     this is equal to 'optical thickness'
orbit_index                                                                                              the absolute orbit number for data from polar orbiting satellites
pressure                                      surface                         X       X    X
pressure_bounds                                                               X       X    X
radiance                                                                      X                    X
reflectance                                                                   X                    X
relative_azimuth_angle                                                        X                          absolute difference between sensor and solar azimuth angles
relative_humidity                                                             X       X    X
relative_vorticity                                                            X       X    X
scan_direction_type
scan_subindex
scattering_angle                                                              X
sensor_azimuth_angle                                                          X
sensor_elevation_angle                                                        X
sensor_name                                                                                              used mainly for ground based networks to provide a unique sensor id
sensor_zenith_angle                                                           X
site_name                                                                                                used for data of a specific named geographical location
solar_azimuth_angle                           sensor,                         X
                                              surface, toa
solar_declination_angle
solar_elevation_angle                         sensor,                         X
                                              surface, toa
solar_hour_angle
solar_irradiance                                                              X                    X
solar_zenith_angle                            sensor,                         X
                                              surface, toa,
sun_normalized_radiance                                                       X                    X
surface_albedo                                                                X            X       X
temperature                                   surface                         X       X    X
tropopause_altitude                                                           X            X             altitude of the troposphere/stratosphere boundary location
tropopause_pressure                                                           X            X             pressure level of the troposphere/stratosphere boundary location
validity                                                                                                 validity flag for each time sample or whole product;
                                                                                                         only to be used if validity flag is for multiple variables combined
viewing_azimuth_angle                                                         X
viewing_elevation_angle                                                       X
viewing_zenith_angle                                                          X
virtual_temperature                                                           X       X    X
wavelength                                                                    X                    X
wavelength_irradiance                                                         X                    X
wavelength_photon_irradiance                                                  X                    X
wavelength_photon_radiance                                                    X                    X
wavelength_photon_transmittance                                               X                    X
wavelength_radiance                                                           X                    X
wavelength_transmittance                                                      X                    X
wavenumber                                                                    X                    X
wavenumber_irradiance                                                         X                    X
wavenumber_photon_irradiance                                                  X                    X
wavenumber_photon_radiance                                                    X                    X
wavenumber_photon_transmittance                                               X                    X
wavenumber_radiance                                                           X                    X
wavenumber_transmittance                                                      X                    X
wind_speed                                    surface                         X       X    X
wind_direction                                surface                         X       X    X
year                                                                                                     integer value representing a year
<species>_column_density                      stratospheric,  amf, apriori,   X       X    X             this is the mass density
                                              tropospheric    avk, dfs
<pm>_column_density                           stratospheric,                  X       X    X             this is the mass density
                                              tropospheric
<species>_column_number_density               stratospheric,  amf, apriori,   X       X    X
                                              tropospheric    avk, dfs
<species>_column_mass_mixing_ratio            stratospheric,                  X            X
                                              tropospheric
<species>_column_mass_mixing_ratio_dry_air    stratospheric,                  X            X
                                              tropospheric
<species>_column_volume_mixing_ratio          stratospheric,                  X            X
                                              tropospheric
<species>_column_volume_mixing_ratio_dry_air  stratospheric,                  X            X
                                              tropospheric
<species>_density                             surface                         X       X    X             this is the mass density
<pm>_density                                  surface                         X       X    X             this is the mass density
<species>_mass_mixing_ratio                   surface         apriori, avk,   X       X    X
                                                              dfs
<species>_mass_mixing_ratio_dry_air           surface         apriori, avk,   X       X    X
                                                              dfs
<species>_number_density                      surface         apriori, avk,   X       X    X
                                                              dfs
<species>_partial_pressure                    surface                         X       X    X
<species>_partial_pressure_dry_air            surface                         X       X    X
<species>_volume_mixing_ratio                 surface         apriori, avk,   X       X    X             this is equal to 'number mixing ratio'
                                                              dfs
<species>_volume_mixing_ratio_dry_air         surface         apriori, avk,   X       X    X
                                                              dfs
============================================= =============== =============== ======= ==== ======= ===== =======================================================================

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

======== ============================ ===========================
Name     Description                  Aliases (not used by HARP)
======== ============================ ===========================
dry_air  dry air
BrO      bromine oxide
BrO2     bromine dioxide
CCl2F2   dichlorodifluoromethane      freon-12, CFC-12, R-12, F12
CCl3F    trichlorofluoromethane       freon-11, CFC-11, R-11, F11
CCl4     tetrachloromethane
CF4      tetrafluoromethane           CFC-14, F14
CHClF2   chlorodifluoromethane        HCFC-22, R-22, F22
CH3Cl    chloromethane,               HCC-40, R-40
         methyl chloride
CH3CN    acetonitrile,
         methyl cyanide
CH3OH    methanol
CH4      methane
CO       carbon monoxide
COF2     carbonyl fluoride
COS      carbonyl sulfide             OCS
CO2      carbon dioxide
C2H2     acetylene                    HCCH
C2H2O2   glyoxal                      OCHCHO, CHOCHO
C2H3NO5  peroxyacetyl nitrate         PAN
C2H6     ethane
C3H8     propane
C5H8     isoprene
ClNO3    chlorine nitrate
ClO      chlorine monoxide
HCHO     formaldehyde                 CH2O, H2CO
HCOOH    formic acid                  HCO2H
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
H2O_162  water (H1/O16/H2 isotopes)   HDO
H2O_171  water (H1/O17/H1 isotopes)
H2O_181  water (H1/O18/H1 isotopes)
H2O2     hydrogen peroxide
IO       hypoiodite
IWC      ice water content;
         H2O in ice state
LWC      liquid water content;
         H2O in liquid state
NO       nitric oxide
NOCl     nitrosyl chloride
NO2      nitrogen dioxide
NO3      nitrate
N2       nitrogen gas
N2O      nitrous oxide                NOS
N2O5     dinitrogen pentoxide
OClO     chlorine dioxide             ClO2
OH       hydroxyl
O2       oxygen
O3       ozone
O3_666   ozone (O16/O16/O16 isotopes)
O3_667   ozone (O16/O16/O17 isotopes)
O3_668   ozone (O16/O16/O18 isotopes)
O3_686   ozone (O16/O18/O16 isotopes)
O4       tetraoxygen, oxozone
SF6      sulfur hexafluoride
SO2      sulfur dioxide
======== ============================ ===========================

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

The 'surface' prefix should only be used when quantities are combined together with quantities that have a vertical dimension.
If a product just contains surface quantities then don't use a 'surface' prefix but just omit the vertical dimension and
indicate the vertical level (i.e. location of the surface) using a 'pressure', 'altitude', and/or 'geopotential_height' variable.

All (horizontal) azimuth angles in HARP should follow the convention that 0 is North facing
and the angle is increasing when moving Eastwards (i.e. clockwise).
Wind direction follows the same rules as for azimuth angles (North = 0, East = 90 degrees),
but the direction indicates where the wind is coming *from*.

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
- <variable>_diffrelx (:math:`\frac{x-y}{x}`)
- <variable>_diffrely (:math:`\frac{x-y}{y}`)
- <variable>_diffrelmin (:math:`\frac{x-y}{\min(x,y)}`)
- <variable>_diffrelmax (:math:`\frac{x-y}{\max(x,y)}`)
- <variable>_diffrelavg (:math:`\frac{2(x-y)}{x+y}`)
- <variable>_diffabs (:math:`|x-y|`)
- <variable>_diffabsrelx (:math:`\frac{|x-y|}{|x|}`)
- <variable>_diffabsrely (:math:`\frac{|x-y|}{|y|}`)
- <variable>_diffabsrelmin (:math:`\frac{|x-y|}{\min(|x|,|y|)}`)
- <variable>_diffabsrelmax (:math:`\frac{|x-y|}{\max(|x|,|y|)}`)
- <variable>_diffabsrelavg (:math:`\frac{2|x-y|}{|x+y|}`)

The postfix 'avk' is used for averaging kernels of atmospheric vertical profiles.
An AVK that only depends once on the vertical dimension is a column averaging kernel,
and an AVK that depends twice on the vertical dimension is a profile averaging kernel.
The 'amf' postfix is used for air mass factors.
The 'dfs' postfix is used for the 'degree of freedom for signal' for vertical profiles which equals the trace of
the two dimensional AVK and provides information on the vertical resolution and information content of profiles.
