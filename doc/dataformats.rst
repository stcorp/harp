Data formats
============

HARP data format
----------------
The HARP data format is a set of conventions for files stored in netCDF-3, HDF4, or HDF5 format. For performance and
sizing reasons it is recommended to use netCDF-3. The netCDF-4 format actually uses HDF5 underneath and is therefore
supported as an HDF5 file by HARP.

The HARP conventions can generally be combined with other conventions that do not contradict it. For instance, a file
can be compliant with both netCDF-CF and the HARP data format conventions at the same time.

In order for a product to be compliant with the HARP data format conventions it will need to have the ``HARP-1.0`` value
in the ``Conventions`` global attribute and follow the restrictions described in this section.

Note that the representation of HARP data in memory by the HARP software may differ from the file format described here
(although there should always be a straightforward mapping between the two). Check the documentation for the HARP
software interfaces for a description of the differences.

File naming
~~~~~~~~~~~
HARP puts no restrictions on the filename.

Data types
~~~~~~~~~~
HARP defines the following data types. See sections `netCDF-3`_, `HDF4`_, and `HDF5`_ for a detailed description of how
these data types are mapped to and from the data types supported by each file format.

============== =========== ==== ====== ========================= ==============================================================================
HARP data type C data type bits sign   range                     description
============== =========== ==== ====== ========================= ==============================================================================
int8           int8_t      8    signed [-128, 127]               signed 8-bit integer type (two's complement representation)
int16          int16_t     16   signed [-32768, 32767]           signed 16-bit integer type (two's complement representation)
int32          int32_t     32   signed [-2147483648, 2147483647] signed 32-bit integer type (two's complement representation)
float          float       32   signed [-3.4E38, 3.4E38]         IEEE754 single-precision binary floating-point format (on supported platforms)
double         double      64   signed [-1.7E308, 1.7E308]       IEEE754 double-precision binary floating-point format (on supported platforms)
string         char*                                             null-terminated ASCII string
============== =========== ==== ====== ========================= ==============================================================================

Dimensions
~~~~~~~~~~
HARP has strict rules regarding the dimensions of variables. Each dimension of a variable may be used to represent a
physical dimension (such as time, latitude, longitude, height, etcetera), or it may be used as an independent dimension.

Only dimension types supported by HARP can be used. These types are:

``time``
  Temporal dimension; this is also the only appendable dimension.

``vertical``
  Vertical dimension, indicating height or depth.

``spectral``
  Spectral dimension, associated with wavelength, wavenumber, or frequency.

``latitude``
  Latitude dimension, only to be used for the latitude axis of a regular latitude x longitude grid.

``longitude``
  Longitude dimension, only to be used for the longitude axis of a regular latitude x longitude grid.

``independent``
  Independent dimension, used to index other quantities, such as the corner coordinates of ground pixel polygons.

Within a HARP product, all dimensions of the same type should have the same length, *except* independent dimensions. For
example, it is an error to have two variables within the same product that both have a time dimension, yet of a
different length.

A variable with more than one dimension has to use a fixed ordering of the dimensions. In the HARP documentation the
ordering is always documented using the so-called 'C convention' for dimension ordering. Using the C convention, the
last dimension (writing from left to right) is the fastest running dimension when enumerating all elements, compared to
the Fortran convention, where the first dimension is the fastest running dimension. Note that different file access
libraries may have different conventions with regard to how they deal with array ordering in their function interfaces.

The order in which dimensions need to be provided for a variable is defined by the following rules:

 - If present, the ``time`` dimension is always the first (i.e. slowest running) dimension.
 - Next are categorical dimensions used for grouping. For instance, this can be the ``spectral`` dimension when it is
   used to distinguish between retrievals performed using different choices of wavelength, or to distinguish data from
   different spectral bands.
 - Next are the spatial dimensions, ordered as ``latitude``, ``longitude``, ``vertical``.
 - Next is the ``spectral`` dimension when it is used as an actual axis (e.g. for L1 spectral data for instruments that
   measure along a spectral axis).
 - Any independent dimensions come last (i.e. they will always be the fastest running dimensions).

So, for a spectral axis used for grouping, the ordering should be:

   ``time``, ``spectral``, ``latitude``, ``longitude``, ``vertical``, ``independent``

And, for a spectral axis used for L1 data from spectral instruments, the ordering should be:

   ``time``, ``latitude``, ``longitude``, ``vertical``, ``spectral``, ``independent``

A variable should only use dimensions on which it is dependent. This means that the radiance variable for L1 data of a
nadir looking spectral instrument on a satellite will generally only have the dimensions ``time`` and ``spectral`` (and
not ``latitude``, ``longitude``, ``vertical``, or ``independent``).

Note that only a single grid can be used for each type of dimension per time value. This means that, for example, it is
possible to change the vertical grid from sample to sample. However, it is not possible to use different vertical grids
for the *same* sample.

To allow a different vertical grid from sample to sample, the ``altitude`` variable should have dimensions
``{time,vertical}`` (instead of ``{vertical}``). This way, the altitude values for the first sample, ``altitude[0,:]``,
may differ from the altitude values for the second sample, ``altitude[1,:]``, and so on. However, for an averaging
kernel, which has dimensions ``{time,vertical,vertical}``, the altitude values for both vertical dimensions are
necessarily the same for each sample.

A grid that differs from sample to sample could have a different effective length per sample. This is implemented by
taking the maximum length over all samples as the length of the dimension and padding the dimension for each sample at
the end with 'invalid' values (e.g. ``NaN``). For instance, you can have an ``altitude{time,vertical}`` variable where
``altitude[0,:]`` has 7 levels and equals ``[0, 5, 10, 15, 20, 25, 30]`` and ``altitude[1,:]`` has only 6 levels and
equals ``[0, 6, 12, 18, 24, 30, NaN]``.

Operations performed by HARP will determine the effective length of a dimension for each sample by ignoring all trailing
``NaN`` values of the axis variable that is used for the operation (e.g. the ``altitude`` or ``pressure`` variable for a
vertical dimension or the ``wavelength`` or ``wavenumber`` variable for a spectral dimension).

Note that even though the ``time`` dimension is conceptually considered `appendable`, this dimension is not stored as an
actual appendable dimension in netCDF-3/HDF4/HDF5. Products are read/written from/to files in full and are only modified
in memory. The `appendable` aspect is only relevant for tools such as plotting routines that combine the data from a
series of HARP products in order to provide plots/statistics for a whole dataset (and thus, where data from different
files will have to be concatenated). Furthermore, storing data in a netCDF-3 file using an actual appendable dimension
(using the netCDF-3 definition of `appendable dimension`) will have a slightly lower read/write performance compared to
having all dimensions fixed.

Global attributes
~~~~~~~~~~~~~~~~~
The HARP format defines the following global attributes:

``Conventions`` string
  This attribute should follow the netCDF convention and should contain ``HARP-1.0`` in its value to indicate that the
  file conforms to the HARP data format conventions.

``history`` string (optional)
  This attribute is used by all HARP tools to keep a trace of the operations performed on a product. Each time a command
  is performed on a HARP file the full command line is appended to this attribute (using a newline separator between
  commands). This usage is in line with the general conventions for this attribute.

``source_product`` string (optional)
  This attribute will hold the name of the original product in case the HARP file was converted using ``harpconvert``.
  This approach makes it possible to use your own file naming approach for HARP files without losing trace of which
  original product files the data came from.

``datetime_start`` double (optional)
  This attribute is mandatory if the file is to be used with ``harpcollocate``. It allows for quick extraction of the
  time range of the product. The attribute should be a scalar double precision floating point value giving the
  ``datetime`` of the first measurement as ``days since 2000-01-01`` (using the fractional part to represent time-of-
  day).

``datetime_stop`` double (optional)
  This attribute is mandatory if the file is to be used with ``harpcollocate``. It allows for quick extraction of the
  time range of the product. The attribute should be a scalar double precision floating point value giving the
  ``datetime`` of the last measurement as ``days since 2000-01-01`` (using the fractional part to represent time-of-
  day).

Variable attributes
~~~~~~~~~~~~~~~~~~~
``description`` string (optional)
  This attribute provides a human readable description of the content of the variable. It should make clear what the
  source of the data was (e.g. measured, climatology, derived, etcetera).

``dims`` string (optional)
  This attribute is only applicable for `HDF4`_ files (`netCDF-3`_ uses named dimensions and `HDF5`_ uses dimension
  scales). This attribute stores the type of each dimension of the associated variable as a comma-separated list of
  dimension type names. The number of dimension types should equal the number of dimensions of the variable.

``units`` string (optional)
  This attribute is used for data that has a physical unit. It should provide the unit in a form compatible with the
  ``udunits2`` software. A ``units`` attribute is expected to be available for any variable defining a quantity.
  If a variable represents a dimensionless quantity the ``units`` string should be an empty string (or have the value
  ``1`` in case empty strings are not supported).

``valid_min`` [int8, int16, int32, float, double] (optional)
  Provides the minimum value below which the data is to be considered invalid. The data type of this attribute should
  match the data type of the associated variable. This attribute is *not* allowed to be present for variables of type
  *string*. For variables of numeric type, this attribute should only be present if the variable actually contains
  values below this threshold that are to be interpreted as `missing` or `invalid` values.

``valid_max`` [int8, int16, int32, float, double] (optional)
  Provides the maximum value above which the data is to be considered invalid. The data type of this attribute should
  match the data type of the associated variable. This attribute is *not* allowed to be present for variables of type
  *string*. For variables of numeric type, this attribute should only be present if the variable actually contains
  values above this threshold that are to be interpreted as `missing` or `invalid` values.

Note that ``_FillValue`` is not used by HARP. Whether a value is valid is purely determined by the ``valid_min`` and
``valid_max`` attributes.

Variables
~~~~~~~~~
A HARP variable is a named multi-dimensional array with associated attributes (see section `Variable attributes`_). The
base type of a variable can be any of the data types supported by HARP (see section `Data types`_). A variable can have
zero or more dimensions. A variable with zero dimensions is a *scalar*. The maximum number of dimensions is 8. Each
dimension of a variable has a type that refers to one of the dimension types supported by HARP (see section
`Dimensions`_). Dimensions of the same type should have the same length, *except* independent dimensions.

To allow HARP to perform automatic operations on variables, it imposes a strict naming convention for variables. This
naming convention applies to the variable name itself and is therefore fully complementary to naming conventions that
apply to the value of a variable attribute, such as ``standard_name`` (as specified by netCDF-CF).

HARP defines the following variable names:

============================================= =============== =============== ======= ==== ======= ===== =======================================================================
Name                                          Prefixes        Postfixes       Quality Vert Lat/Lon Spect Comments
============================================= =============== =============== ======= ==== ======= ===== =======================================================================
absorbing_aerosol_index                                                       X            X
aerosol_extinction_coefficient                surface                         X       X    X       X
aerosol_optical_depth                         stratospheric,                  X       X    X       X     this is equal to 'aerosol optical thickness'
                                              tropospheric
<aerosol_type>_aerosol_extinction_coefficient surface                         X       X    X       X
<aerosol_type>_aerosol_optical_depth          stratospheric,                  X       X    X       X     this is equal to 'aerosol optical thickness'
                                              tropospheric
altitude                                      sensor,                         X       X    X
                                              surface
altitude_bounds                                                                       X    X
backscatter_coefficient                       surface                         X       X    X       X
cloud_albedo                                                                  X            X
cloud_base_albedo                                                             X            X
cloud_base_height                                                             X            X
cloud_base_pressure                                                           X            X
cloud_fraction                                                                X            X
cloud_optical_depth                                                           X            X             this is equal to 'cloud optical thickness'
cloud_pressure                                                                X            X
cloud_height                                                                  X            X
cloud_top_albedo                                                              X            X
cloud_top_height                                                              X            X
cloud_top_pressure                                                            X            X
collocation_index                                                                                        zero-based index as provided in the collocation result file
column_density                                stratospheric,  amf, apriori,   X       X    X             this is the mass density
                                              tropospheric    avk
column_number_density                         stratospheric,  amf, apriori,   X       X    X
                                              tropospheric    avk
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
geopotential                                  surface                         X       X    X
geopotential_height                           surface                         X       X    X
hlos_wind_velocity                            surface                         X       X    X             hlos means 'horizontal line of sight'
index                                                                                                    zero-based index of the sample within the source product
integration_time                                                                      X    X       X     provides measurement specific integration time
                                                                                                         (at e.g. altitude or wavelength) compared to overal datetime_length;
                                                                                                         only use if integration time differs from datetime_length;
                                                                                                         integration_time longer than datetime_length that covers multiple
                                                                                                         datetime values means replication of measured value in time dimension.
latitude                                      sensor                          X            (lat)
latitude_bounds                                                                            (lat)
longitude                                     sensor                          X            (lon)
longitude_bounds                                                                           (lon)
molar_mass                                                                    X       X    X             this is the molar mass of the total substance (it is defined by the
                                                                                                         relation between the variables 'density' and 'number_density')
number_density                                surface                         X       X    X
optical_depth                                                                 X       X    X       X     this is equal to 'optical thickness'
pressure                                      surface                         X       X    X
pressure_bounds                                                               X       X    X
radiance                                                                      X                    X
reflectance                                                                   X                    X
relative_azimuth_angle                                                        X                          absolute difference between sensor and solar azimuth angles
relative_humidity                                                             X       X    X
scan_direction
scan_subset_counter
scanline_pixel_index
scattering_angle                                                              X
sensor_azimuth_angle                                                          X
sensor_elevation_angle                                                        X
sensor_name                                                                                              used mainly for ground based networks to provide a unique sensor id
sensor_zenith_angle                                                           X
site_name                                                                                                used for data of a specific named geographical location
solar_azimuth_angle                           sensor,                         X
                                              surface, toa
solar_elevation_angle                         sensor,                         X
                                              surface, toa
solar_irradiance                                                              X                    X
solar_zenith_angle                            sensor,                         X
                                              surface, toa,
sun_normalized_radiance                                                       X                    X
surface_albedo                                                                X            X       X
temperature                                   surface                         X       X    X
tropopause_altitude                                                           X            X             altitude of the troposphere/stratosphere boundary location
tropopause_pressure                                                           X            X             pressure level of the troposphere/stratosphere boundary location
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
<species>_column_density                      stratospheric,  amf, apriori,   X       X    X             this is the mass density
                                              tropospheric    avk
<pm>_column_density                           stratospheric,                  X       X    X             this is the mass density
                                              tropospheric
<species>_column_number_density               stratospheric,  amf, apriori,   X       X    X
                                              tropospheric    avk
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
<species>_mass_mixing_ratio                   surface         apriori, avk    X       X    X
<species>_mass_mixing_ratio_dry_air           surface         apriori, avk    X       X    X
<species>_number_density                      surface         apriori, avk    X       X    X
<species>_partial_pressure                    surface                         X       X    X
<species>_partial_pressure_dry_air            surface                         X       X    X
<species>_volume_mixing_ratio                 surface         apriori, avk    X       X    X             this is equal to 'number mixing ratio'
<species>_volume_mixing_ratio_dry_air         surface         apriori, avk    X       X    X
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

======== ============================ ==========================
Name     Description                  Aliases (not used by HARP)
======== ============================ ==========================
dry_air  dry air
BrO      bromine oxide
BrO2     bromine dioxide
CCl2F2   dichlorodifluoromethane      freon-12, CFC-12, R-12
CCl3F    trichlorofluoromethane       freon-11, CFC-11, R-11
CF4      tetrafluoromethane
CHClF2   chlorodifluoromethane        HCFC-22, R-22
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
======== ============================ ==========================

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

Be aware that there are still several topics under discussion that may change the above naming convention.
See the HARP issues list on the GitHub website for more details.

Conventions
~~~~~~~~~~~
In addition to the conventions above, there are a few more general conventions in HARP for quantities.

Datetime values
"""""""""""""""
Datetime values are always represented as a number of days or seconds since a reference time. This is also reflected
by the ``unit`` attribute for datetime values (e.g. ``days since 2000-01-01``). The reference time that is mentioned in
these units should always use UTC as timezone (i.e none of the datetime values should reference a local time in HARP).

In addition, HARP does not deal explicitly with leap seconds in its time calculations. Each day is just treated as
having 24 * 60 * 60 = 86400 seconds (the udunits2 library, which HARP uses internally, has the same behaviour).
In practice, datetime values should be chosen such that they end up being accurate with regard to the UTC epoch that
they represent when using the 86400 seconds per day convention (and will introduce an error when calculating time
differences between epochs if there were leap seconds introduced between those epochs). For instance when representing
``2010-01-01T00:00:00`` as an amount of seconds since 2000, then this is best represented with
``315619200 [s since 2000-01-01]`` and not with ``315619202 [s since 2000-01-01]``.
For cases where it is needed to be interoperable with software that can properly deal with leap seconds, the
recommended approach is to use a reference epoch in the unit such that the represented value is not impacted by leap
seconds. This can, for instance, be achieved by using the start of the day as reference epoch (i.e. represent
``2001-02-03T04:05:06`` as ``14706 [s since 2001-02-03]``).

netCDF-3
~~~~~~~~
This section details a set of additional conventions that are specific to the netCDF-3 file format.

The following table shows the mapping between HARP data types and netCDF-3 data types. NetCDF-3 data types not covered
in this table are not supported by HARP.

============== ==================
HARP data type netCDF-3 data type
============== ==================
int8           NC_BYTE
int16          NC_SHORT
int32          NC_INT
float          NC_FLOAT
double         NC_DOUBLE
string         NC_CHAR
============== ==================

The netCDF-3 data model defines the concept of shared dimensions. A netCDF-3 dimension has a name and a length. The name
of a dimension should be unique. The shape of a netCDF-3 variable is specified as a list of dimensions (instead of a
list of dimension *lengths*). This implies that to store a HARP product in netCDF-3 format, a netCDF-3 dimension should
be defined for each dimension present in the product. This is straight-forward for all dimension types supported by
HARP, except for independent dimensions (since HARP does not require that all independent dimensions have the same
length).

For example, a variable may have an independent dimension of length 2, while another variable may have an independent
dimension of length 4. However, it is not possible to create two netCDF-3 dimensions called 'independent' with different
lengths, because netCDF-3 requires dimension names to be unique. Instead, the netCDF-3 backend defines a netCDF-3
dimension for each independent dimension by appending the length to the name of the dimension to make it unique. The
name and the length are separated by an underscore. In the case of the example, two netCDF-3 dimensions would be
defined, one named 'independent_2' and another named 'independent_4'. Independent dimensions of the same length will be
represented by the same netCDF-3 dimension.

NetCDF-3 does not support strings. The netCDF-3 backend stores an N-dimensional HARP variable of type string as an
(N+1)-dimensional netCDF-3 variable of type NC_CHAR. The length of the introduced dimension equals the length of the
longest string, or 1 if the length of the longest string is zero. Shorter strings are padded will null-termination
characters.

Of course, netCDF-3 dimensions need to be defined for the introduced dimensions. This is handled in the same way as for
independent dimensions, by appending the length to the name of the dimension. The name and the length are separated by
an underscore. The name used for these introduced dimensions is 'string'. For example, if the longest string has a
length of 10, a netCDF-3 dimension named 'string_10' would be defined. String dimensions of the same length will be
represented by the same netCDF-3 dimension.

To summarize, HARP dimensions are mapped to netCDF-3 dimensions as follows:

=================== =======================
HARP dimension type netCDF-3 dimension name
=================== =======================
time                time
latitude            latitude
longitude           longitude
vertical            vertical
spectral            spectral
independent         independent\_<length>
N/A                 string\_<length>
=================== =======================

HDF4
~~~~
This section details a set of additional conventions that are specific to the HDF4 file format.

The following table shows the mapping between HARP data types and HDF4 data types. HDF4 data types not covered in this
table are not supported by HARP.

============== ==============
HARP data type HDF4 data type
============== ==============
int8           DFNT_INT8
int16          DFNT_INT16
int32          DFNT_INT32
float          DFNT_FLOAT32
double         DFNT_FLOAT64
string         DFNT_CHAR
============== ==============

In the HDF4 data model there is no concept of shared dimensions (unlike netCDF). The shape of an HDF4 dataset is
specified as a list of dimension lengths.

When a HARP variable is stored as an HDF4 dataset, dimension lengths are preserved, but dimension types are lost. A
dataset attribute named 'dims' is used to store the type of each dimension of the associated dataset as a comma-
separated list of dimension type names. The number of dimension types equals the number of dimensions of the HDF4
*dataset*. This number is equal to the number of dimensions of the HARP variable, *except* for scalar variables and
variables of type string (see below).

HDF4 does not support scalars (datasets with zero dimensions). The HDF4 backend stores a scalar HARP variable as an HDF4
dataset with a single dimension of length 1. To differentiate between scalars and proper 1-D variables, both of which
are stored as 1-D HDF4 datasets, the introduced dimension is included in the dimension type list using the dimension
type name 'scalar'.

HDF4 does not support strings. The HDF4 backend stores an N-dimensional HARP variable of type string as an
(N+1)-dimensional HDF4 dataset of type DFNT_CHAR. The length of the introduced dimension equals the length of the
longest string, or 1 if the length of the longest string is zero. Shorter strings are padded with null-termination
characters. The introduced dimension is included in the dimension type list using the dimension type name 'string'.

Thus, a scalar HARP variable of type string would be represented in HDF4 by a **2**-dimensional dataset of type
DFNT_CHAR. The length of the outer dimension would be 1, the length of the inner dimension would equal the length of the
string stored in the HARP variable. The 'dims' attribute associated with this HDF4 dataset would contain the string
'scalar,string'.

To summarize, HARP dimensions types are mapped to dimension type names as follows:

=================== ===================
HARP dimension type dimension type name
=================== ===================
time                time
latitude            latitude
longitude           longitude
vertical            vertical
spectral            spectral
independent         independent
N/A                 scalar
N/A                 string
=================== ===================

HARP uses empty strings to represent the unit of dimensionless quantities (to distinguish them from non-quantities,
which will lack a unit attribute). However, HDF4 cannot store string attributes with length zero. For this reason
an empty unit string will be written as a ``units`` attribute with value ``"1"`` when writing data to HDF4.
When reading from HDF4 a unit string value ``"1"`` will be converted back again to an empty unit string.

HDF5
~~~~
This section details a set of additional conventions that are specific to the HDF5 file format.

The following table shows the mapping between HARP data types and HDF5 data types. The HDF5 backend uses this mapping
for writing only.

============== =================
HARP data type HDF5 data type
============== =================
int8           H5T_NATIVE_SCHAR
int16          H5T_NATIVE_SHORT
int32          H5T_NATIVE_INT
float          H5T_NATIVE_FLOAT
double         H5T_NATIVE_DOUBLE
string         H5T_C_S1
============== =================

The mapping used by the HDF5 backend for reading is shown below. The HDF5 data type interface (H5T) is used to
introspect the data type of the variable to be read.

=============== ============== ============== ==================== ==============
H5T_get_class() H5T_get_size() H5T_get_sign() H5Tget_native_type() HARP data type
=============== ============== ============== ==================== ==============
H5T_INTEGER     1              H5T_SGN_2                           int8
H5T_INTEGER     2              H5T_SGN_2                           int16
H5T_INTEGER     4              H5T_SGN_2                           int32
H5T_FLOAT                                     H5T_NATIVE_FLOAT     float
H5T_FLOAT                                     H5T_NATIVE_DOUBLE    double
H5T_STRING                                                         string
=============== ============== ============== ==================== ==============

HDF5 data types not covered in this table are not supported by HARP.

In the HDF5 data model there is no concept of shared dimensions (unlike netCDF). The shape of an HDF5 dataset is
specified as a list of dimension lengths. However, the netCDF-4 library uses HDF5 as its storage backend. It represents
shared dimensions using HDF5 *dimension scales*.

Dimension scales were introduced in HDF5 version 1.8.0. A dimension scale is a special dataset that can be attached to
one or more dimensions of other datasets. Multiple dimension scales can be attached to a single dimension, and the
length of the dimension scale does not have to be the same as the length of the dimension it is attached to. There are
no limitations on the shape or dimensionality of a dimension scale, since it is just a dataset with particular
attributes attached.

To represent shared dimensions, netCDF-4 creates dimension scales for each shared dimension and attaches these dimension
scales to the corresponding dimensions of all variables. If a product contains a variable with the same name as a shared
dimension, the dataset containing the values of the variable will be used as the dimension scale. Such a variable is
called a *coordinate variable* in netCDF-4. Note that in a HARP product, due to the variable naming convention, only
variables called ``latitude`` or ``longitude`` could possibly be coordinate variables. For shared dimensions for which a
variable with the same name does not exist, a stub dataset containing fill values is created and used as the dimension
scale. The optional ``NAME`` attribute of the dimension scale is set to ``This is a netCDF dimension but not a netCDF
variable.``, which causes the netCDF-4 library to hide the stub dataset from the user. For more information about the
netCDF-4 format, see the `NetCDF User's Guide`_.

.. _`NetCDF User's Guide`: http://www.unidata.ucar.edu/software/netcdf/docs/file_format_specifications.html#netcdf_4_spec

The HDF5 file format conventions used by HARP are designed to be compatible with netCDF-4. Like netCDF-4, HARP uses
dimension scales to represent shared dimensions. For independent dimensions, the same approach is used as for the
netCDF-3 backend. For each unique dimension length ``L``, a dimension scale named ``independent_L`` is created.

To summarize, HARP dimensions types are mapped to HDF5 dimension scales as follows:

=================== =====================
HARP dimension type HDF5 dimension scale
=================== =====================
time                time
latitude            latitude
longitude           longitude
vertical            vertical
spectral            spectral
independent         independent\_<length>
=================== =====================

The ``_nc3_strict`` attribute is attached to the root group of the HDF5 file such that it will be interpreted using the
netCDF classic data model by the netCDF-4 library. Enhanced features of netCDF-4 beyond the classic data model, such as
groups and user-defined types, are not supported by HARP.

HDF5 can represent strings in several ways. Both fixed and variable length strings are supported. The HDF5 backend
stores a HARP variable of type string as an HDF5 dataset of fixed length strings. The fixed string length equals the
length of the longest string, or 1 if the length of the longest string is zero. Shorter strings are padded with null-
termination characters.

HARP uses empty strings to represent the unit of dimensionless quantities (to distinguish them from non-quantities,
which will lack a unit attribute). However, HDF5 cannot store string attributes with length zero. For this reason
an empty unit string will be written as a ``units`` attribute with value ``"1"`` when writing data to HDF5.
When reading from HDF5 a unit string value ``"1"`` will be converted back again to an empty unit string.

.. _collocation-result-file-format:

Collocation result file format
------------------------------
The collocation result file is a simple comma separated (csv) file, containing the following columns:

collocation_id
  Unique id of the collocation pair. This id will correspond with the ``collocation_index`` variable inside HARP
  products after they are filtered using a collocation result file.

filename_a
  The filename of the original input file (i.e. ``source_product`` global attribute value) from the primary dataset.

measurement_id_a
  A unique index number of the measurement within the file. This index number is based on the list of measurements from
  the original input file and corresponds to the ``index`` variable inside HARP products.

filename_b
  The filename of the original input file (i.e. ``source_product`` global attribute value) from the secondary dataset.

measurement_id_b
  A unique index number of the measurement within the file. This index number is based on the list of measurements from
  the original input file and corresponds to the ``index`` variable inside HARP products.

collocation criteria...
  The remaining columns cover the collocation criteria that were provided to harpcollocate. For each collocation
  criterium the column will provide the exact distance value for the given collocated measurement pair for that
  criterium.
