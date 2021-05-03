Variable attributes
===================

``description`` string (optional)
  This attribute provides a human readable description of the content of the variable. It should make clear what the
  source of the data was (e.g. measured, climatology, derived, etcetera).

``dims`` string (optional)
  This attribute is only applicable for :doc:`HDF4 <hdf4>` files (:doc:`netCDF-3 <netcdf3>` uses named dimensions and
  :doc:`HDF5 <hdf5>` uses dimension scales).
  This attribute stores the type of each dimension of the associated variable as a comma-separated list of
  dimension type names. The number of dimension types should equal the number of dimensions of the variable.

``units`` string (optional)
  This attribute is used for data that has a physical unit. It should provide the unit in a form compatible with the
  ``udunits2`` software. A ``units`` attribute is expected to be available for any variable defining a quantity.
  If a variable represents a dimensionless quantity the ``units`` string should be an empty string (or have the value
  ``1`` in case empty strings are not supported). The plural attribute name ``units`` is only used when storing HARP
  data in a file (for compatibility reasons with other standards such as netCDF-CF); inside data structures of HARP
  variables within C, Python, etc. the (more accurate) singular term ``unit`` is used.

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

``enum_name``/``flag_meanings`` [string] (optional)
  Provides the list of string values of the enumeration labels of a categorical variable. For the in-memory
  representation (C, Python, etc.) this attribute is called ``enum_name`` and consists of a list of strings.
  Within a :doc:`netCDF-3 <netcdf3>`, :doc:`HDF4 <hdf4>`, or :doc:`HDF5 <hdf5>` file this attribute is stored as a
  single string attribute named ``flag_meanings`` which contains a space separated list of the label names.

Note that ``_FillValue`` is not used by HARP. Whether a value is valid is purely determined by the ``valid_min`` and
``valid_max`` attributes.

Note that files using the HARP data format can include variable attributes in addition to the ones mentioned above
(such as e.g. a ``_FillValue`` attribute). However, such attributes will be ignored when the product is imported.
