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
physical dimension (such as time, latitude, longitude, height, et cetera), or it may be used as an indepedent dimension.

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
 - Next are categorial dimensions used for grouping. For instance, this can be the ``spectral`` dimension when it is
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
  source of the data was (e.g. measured, climatology, derived, et cetera).

``dims`` string (optional)
  This attribute stores the type of each dimension of the associated variable as a comma-separated list of dimension
  type names. The number of dimension types should equal the number of dimensions of the variable. This attribute is
  only present in HDF4 and HDF5 files, *not* in netCDF-3 files. See also sections `HDF4`_ and `HDF5`_.

``units`` string (optional)
  This attribute is used for data that has a physical unit. It should provide the unit in a form compatible with the
  ``udunits2`` software.

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

The core variables are:

- absorbing_aerosol_index
- aerosol_extinction_coefficient
- aerosol_optical_depth
- altitude
- altitude_bounds
- cloud_fraction
- cloud_optical_thickness
- cloud_top_albedo
- cloud_top_height
- cloud_top_pressure
- surface_albedo
- surface_pressure
- collocation_index
- datetime
- datetime_start
- datetime_stop
- datetime_length
- flag_am_pm
- flag_day_twilight_night
- frequency
- geopotential_height
- index
- instrument_altitude
- instrument_latitude
- instrument_longitude
- instrument_name
- latitude
- latitude_bounds
- longitude
- longitude_bounds
- normalized_radiance
- number_density
- pressure
- radiance
- reflectance
- relative_humidity
- relative_azimuth_angle
- scan_direction
- scan_subset_counter
- scanline_pixel_index
- scattering_angle
- site_name
- solar_azimuth_angle
- solar_elevation_angle
- solar_irradiance
- solar_zenith_angle
- temperature
- viewing_azimuth_angle
- viewing_zenith_angle
- virtual_temperature
- wavelength
- wavenumber
- <species>_column_number_density
- <species>_density
- <species>_mass_mixing_ratio
- <species>_mass_mixing_ratio_wet
- <species>_number_density
- <species>_partial_pressure
- <species>_volume_mixing_ratio

with supported species:

- BrO
- C2H2
- C2H6
- CCl2F2
- CCl3F
- CF4
- CH2O
- CH3Cl
- CH4
- CHF2Cl
- ClNO
- ClONO2
- ClO
- CO2
- COF2
- CO
- H2O_161
- H2O_162
- H2O_171
- H2O_181
- H2O2
- H2O
- HCl
- HCN
- HCOOH
- HF
- HO2NO2
- HO2
- HOCl
- HNO3
- N2O
- N2O5
- N2
- NO2
- NO3
- NO
- O2
- O3_666
- O3_667
- O3_668
- O3_686
- O3
- O4
- OBrO
- OClO
- OCS
- OH
- SF6
- SO2

Specific height variants of the above variables:

- instrument_<variable>
- stratospheric_<variable>
- surface_<variable>
- toa_<variable>
- tropospheric_<variable>

Specific ancillary variables for the atmospheric variables are:

- <variable>_apriori
- <variable>_amf
- <variable>_avk

Generic ancillary variables for the above variables are:

- <variable>_cov
- <variable>_cov_random
- <variable>_cov_systematic
- <variable>_uncertainty
- <variable>_uncertainty_random
- <variable>_uncertainty_systematic
- <variable>_validity

Be aware that there are still several topics under discussion that may change the above naming convention:
- 'cov' may be renamed to 'covariance'
- split of random vs. systematic for covariance will likely be removed since any non-zero off-diagonal covariance element is by definition a systematic effect
- (some) covariance aspects may be captured by correlation variables
- split of random vs. systematic for uncertainty may be captured by a naming convention that captures the actual semantics, which is uncertainty with zero auto-correlation and uncertainty with full auto-correlation

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

The netCDF-3 data model defines the concept of a dimension. A netCDF-3 dimension has a name and a length. The name of a
dimension should be unique. The shape of a netCDF-3 variable is specified as a list of dimensions (instead of a list of
dimension *lengths*). This implies that to store a HARP product in netCDF-3 format, a netCDF-3 dimension should be
defined for each dimension present in the product. This is straight-forward for all dimension types supported by HARP,
except for independent dimensions (since HARP does not require that all independent dimensions have the same length).

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

The HDF4 data model does not define the concept of a dimension (unlike netCDF-3). The shape of an HDF4 dataset is
specified as a list of dimensions lengths.

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

The HDF5 data model does not define the concept of a dimension (unlike netCDF-3). The shape of an HDF5 dataset is
specified as a list of dimensions lengths.

When a HARP variable is stored as an HDF5 dataset, dimension lengths are preserved, but dimension types are lost. A
dataset attribute named 'dims' is used to store the type of each dimension of the associated dataset as a comma-
separated list of dimension type names. The number of dimension types equals the number of dimensions of the HARP
variable.

HDF5 can represent strings in several ways. Both fixed and variable length strings are supported. The HDF5 backend
stores a HARP variable of type string as an HDF5 dataset of fixed length strings. The fixed string length equals the
length of the longest string, or 1 if the length of the longest string is zero. Shorter strings are padded with null-
termination characters.

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
