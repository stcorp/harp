Data formats
============

HARP data format
----------------
The HARP Data Format is a set of conventions for files in netCDF3, HDF4, or HDF5 format. For performance and sizing reasons it is recommended to use netCDF3. The netCDF4 format actually uses HDF5 underneath and is therefore supported as an HDF5 file by HARP.

The HARP conventions can generally be combined with other conventions that do not contradict it. For instance, a file can be compliant with both netCDF-CF and the HARP format convention at the same time.

In order for a product to be compliant with the HARP data format conventions it will need to have the ``HARP-1.0`` value in the ``Conventions`` global attribute and follow the restrictions as described in this section.

File naming
~~~~~~~~~~~
HARP puts no restrictions on the filename.

Global attributes
~~~~~~~~~~~~~~~~~
The HARP format has the following global attributes

``Conventions``
  This attribute should follow the netCDF convention and should contain ``HARP-1.0`` in its value to indicate that the file conforms to the HARP format convention.

``history`` (optional)
  This attribute is used by all HARP tools to keep a trace of the operations performed on a product. Each time a command is performed on a HARP file the full command line is appended to this attribute (using a newline separator between commands). This usage is in line with the general conventions for this attribute.

``source_product`` (optional)
  This attribute will hold the name of the original product in case the HARP file was converted using ``harpconvert``. This approach makes it possible to use your own file naming approach for HARP files without losing trace of which original product files the data came from.

``datetime_start`` (optional)
  This attribute is mandatory if the file is to be used with ``harpcollocate``. It will allow for a quick extraction of the time range of the product.
  The attribute should be a scalar double precision floating point value giving the ``datetime`` of the first measurement as ``days since 2000-01-01`` (using the fractional part to represent time-of-day).

``datetime_stop`` (optional)
  This attribute is mandatory if the file is to be used with ``harpcollocate``. It will allow for a quick extraction of the time range of the product.
  The attribute should be a scalar double precision floating point value giving the ``datetime`` of the last measurement as ``days since 2000-01-01`` (using the fractional part to represent time-of-day).


Dimensions
~~~~~~~~~~
HARP has strict rules regarding the dimensions of variables. Each dimension of a variable can either be associated with an axis (such as time, latitude, longitude, height/depth, etc.) or can be an independent dimension.
All dimensions need to be named. When using netCDF this is alread enforced by the netCDF format itself. For HDF4 and HDF5 the dimension names are stored per variable by means of the ``dims`` variable attribute.

Only dimensions that are supported by HARP can be used. These dimensions are:

 - ``time`` : temporal dimension; this is also the only appendable dimension
 - ``vertical`` : vertical axis, indicating height or depth
 - ``spectral`` : spectral axis, associated with wavelength, wavenumber, or frequency
 - ``latitude`` : latitude axis, only to be used for the latitude axis of a regular latitude x longitude grid
 - ``longitude`` : longitude axis, only to be used for the longitude axis of a regular latitude x longitude grid
 - ``independent_x`` : independent axis of length ``x`` (the length needs be provided as an integer number without leading zeroes);  all independent axis of the same length share the same dimension name; for instance, an independent dimension with length 16 will be named ``independent_16``.

A variable that has more than one dimension will have to use a fixed ordering of the dimensions. In the HARP documentation the ordering is always documented using the so-called 'C convention' for dimension ordering. With the C convention the last dimension (writing from left to right) is fastest running when enumerating all elements, compared to the Fortran convention, where the first dimension is fastest running. Note that different file access libraries may have different conventions with regard to how they deal with array ordering in their function interfaces.

The order in which dimensions need to be provided for a variable is defined by the following rules:
 - ``time``, if present, is always the first (i.e. slowest running) dimension
 - next are categorial dimensions used for grouping. For instance, this can be the ``spectral`` dimension when it is used to distinguish between retrievals done using different choices of wavelength, or to distinguish data from different spectral bands.
 - next are the spatial dimensions, ordered as ``latitude``, ``longitude``, ``vertical``
 - next is the ``spectral`` dimension if used as an actual axis (e.g. for L1 spectral data for instruments that measure along a spectral axis)
 - all independent dimensions come last (i.e. they will allways be the fastest running dimensions)

So, for a spectral axis used for grouping the overall ordering should be:

   ``time``, ``spectral``, ``latitude``, ``longitude``, ``vertical``, ``independent``

And for a spectral axis used for L1 data from spectral instruments the ordering should be:

   ``time``, ``latitude``, ``longitude``, ``vertical``, ``spectral``, ``independent``

A variable should only use dimensions on which it is dependent. This means that the radiance variable for L1 data of a nadir looking spectral instrument on a satellite will generally only have the dimensions ``time`` and ``spectral`` (and not ``latitude``, ``longitude``, ``vertical``, or ``independent``).

Note that you can only use a single grid for each type of dimension per time value.
This means that you can switch e.g. the vertical grid from measurement to measurement, but you cannot use two different vertical grids for a single measurement. To change your vertical grid per sample, your ``altitude`` variable can have dimensions ``[time,vertical]`` and the altitude grid for the first sample ``altitude[0,:]`` can differ from that of the second sample ``altitude[1,:]``. However, an averaging kernel for the first sample that has dimensions ``[time,vertical,vertical]`` will need to use the same altitude grid ``altitude[0,:]`` for both its vertical dimensions.
A grid that differs per sample can have a different effective length per sample. This is done by taking the maximum length for all samples as the real length of the dimension and filling out the dimension for each sample at the end with 'invalid' values (e.g. ``NaN``). For instance, you can have an ``altitude[time,vertical]`` where ``altitude[0,:]`` has 7 levels and equals ``[0, 5, 10, 15, 20, 25, 30]`` and ``altitude[1,:]`` has only 6 levels and equals ``[0, 6, 12, 18, 24, 30, NaN]``. Operations in HARP will determine the effective length of a dimension for a sample by discarding all trailing NaN values of the axis variable that is used for the operation (e.g. ``altitude``/``pressure`` variable for a vertical dimension or ``wavelength``/``wavenumber`` variable for a spectral dimension).

Note that even though the ``time`` dimension is considered conceptually `appendable`, this dimension is not stored as an actual appendable dimension in netCDF/HDF4/HDF5. Products are read/written from/to files in full and are only modified in memory. The `appendable` aspect is only something that  counts for e.g. tools such as plotting routines that combine the data from a series of HARP products in order to provide plots/statistics for a whole dataset (and thus, where data from different files will have to be concatenated). Furthermore, storing data in a netcdf file using an actual appendable dimension (using the netCDF definition of `appendable dimension`) will have a slightly lower read/write performance compared to having all dimensions fixed.

Variable attributes
~~~~~~~~~~~~~~~~~~~
description (optional)
  This attribute provides a human readable description of the content of the variable. It should make clear what the source of the data was (e.g. measured, climatology, derived, etc.)

units (optional)
  This attribute for data that has a physical unit. It should provide the unit in a form compatible with the ``udunits2`` software.

valid_min (optional)
  Provides the minimum value below which the data is to be considered invalid. Note that this attribute should only be used in case the variable actually contains values below this threshold that are to be interpreted as `missing` or `invalid` values.

valid_max (optional)
  Provides the maximum value above which the data is to be considered invalid. Note that this attribute should only be used in case the variable actually contains values above this threshold that are to be interpreted as `missing` or `invalid` values.

Note that ``_FillValue`` is not used by HARP. Wheter a measurement value is valid or not is purely determined by the ``valid_min`` and ``valid_max`` attributes.

Variables
~~~~~~~~~
In order for HARP to perform operations on the data it imposes a strict naming convention for variables. This naming convention is to be applied on the variable name itself and is therefore fully complementary to naming conventions for variable attribute values such as ``standard_name`` (as specified by netCDF-CF).

The list below provides the set of names currently defined by HARP.

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
- <variable>_stdev
- <variable>_stdev_random
- <variable>_stdev_systematic
- <variable>_validity

Collocation result file format
------------------------------
The collocation result file is a simple comma separated (csv) file, containing the following columns:

collocation_id
  Unique id of the collocation pair. This id will correspond with the ``collocation_index`` variable inside HARP products after they are filtered using a collocation result file.

filename_a
  The filename of the original input file (i.e. ``source_product`` global attribute value) from the primary dataset.

measurement_id_a
  A unique index number of the measurement within the file. This index number is based on the list of measurements from the original input file and corresponds to the ``index`` variable inside HARP products.

filename_b
  The filename of the original input file (i.e. ``source_product`` global attribute value) from the secondary dataset.

measurement_id_b
  A unique index number of the measurement within the file. This index number is based on the list of measurements from the original input file and corresponds to the ``index`` variable inside HARP products.

collocation criteria...
  The remaining columns cover the collocation criteria that were provided to harpcollocate. For each collocation criterium the column will provide the exact distance value for the given collocated measurement pair for that criterium.
