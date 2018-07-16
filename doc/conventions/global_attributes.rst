Global attributes
=================

The HARP format defines the following global attributes:

``Conventions`` string
  This attribute should follow the netCDF convention and should contain ``HARP-1.0`` in its value to indicate that the
  file conforms to the HARP data format conventions.

``history`` string (optional)
  This attribute is used by all HARP tools to keep a trace of the operations performed on a product. Each time a command
  is performed on a HARP file the full command line is appended to this attribute (using a newline separator between
  commands). The command line is prepended by the current UTC time and the version of HARP (using the format ``YYY-MM-DDThh:mm:ssZ [harp-x.y] <command line>``). This usage is in line with the general netCDF conventions for this attribute.

``source_product`` string (optional)
  This attribute will hold the name of the original product in case the HARP file was converted using ``harpconvert``.
  This approach makes it possible to use your own file naming approach for HARP files without losing trace of which
  original product files the data came from.
  Note that this attribute value is used as the main source to uniquely identify a product within a dataset.
  Only if the attribute is absent then the current filename of the product will be used as product id within a dataset.
  It is therefore important that the value of ``source_product`` (or, if absent, the current filename) is unique within
  a dataset to allow the product to be uniquely identified. Tools such as ``harpcollocate`` rely on this.

``datetime_start`` double (optional)
  This attribute is mandatory if the file is to be used with ``harpcollocate``. It allows for quick extraction of the
  time range of the product. The attribute should be a scalar double precision floating point value giving the
  ``datetime`` of the first measurement as ``days since 2000-01-01`` (using the fractional part to represent time-of-
  day). When exporting data, HARP will itself generate the value by looking at the minimum value of the available
  ``datetime_start`` (or, if absent, ``datetime``) variable.

``datetime_stop`` double (optional)
  This attribute is mandatory if the file is to be used with ``harpcollocate``. It allows for quick extraction of the
  time range of the product. The attribute should be a scalar double precision floating point value giving the
  ``datetime`` of the last measurement as ``days since 2000-01-01`` (using the fractional part to represent time-of-
  day). When exporting data, HARP will itself generate the value by looking at the maximum value of the available
  ``datetime_stop`` (or, if absent, ``datetime``) variable.


Note that the ``Conventions``, ``datetime_start``, and ``datetime_stop`` attributes are only used inside files.
For the in-memory representation (in C, Python, etc.) only the ``history`` and ``source_product`` attributes are present.

Note that files using the HARP data format can include global attributes in addition to the ones mentioned above.
However, such attributes will be ignored when the product is imported.
