Python interface
================

The Python interface consists of a Python package ``'harp'`` that provides a set
of functions to :py:func:`import <harp.import_product>` and :py:func:`export
<harp.export_product>` HARP products, and to :py:func:`ingest
<harp.ingest_product>` non-HARP products of a type :doc:`supported by HARP
<ingestions/index>`. The Python interface depends on the ``_cffi_backend``
module, which is part of the C foreign function interface (cffi) package. This
package must be installed in order to be able to use the Python interface. See
the `cffi documentation`_ for details on how to install the cffi package.

Products are represented in Python by instances of :py:class:`harp.Product`,
which can be manipulated freely from within Python. A :py:class:`harp.Product`
instance contains a :py:class:`harp.Variable` instance for each variable
contained in the product.

Products can be :py:func:`exported <harp.export_product>` as HARP compliant
products in any of the file formats supported by the HARP C library
(NetCDF/HDF4/HDF5). Such exported products can subsequently be processed further
using the :doc:`HARP command line tools <tools>`. Products can also be
:py:func:`converted <harp.to_dict>` to an ``OrderedDict``. This can be
convenient when there is a need to interface with existing code such as plotting
libraries, or when the additional information provided by the
:py:class:`harp.Product` representation is not needed.

.. _cffi documentation: http://cffi.readthedocs.org/en/latest/installation.html

Dimension types
---------------

The HARP C library defines several dimension types. Each dimension of a variable
is associated with one of these dimension types. The number of dimension types
should match the number of dimensions of the data array.

In Python, all dimension types are referred to by name, except the
``independent`` dimension type. Dimension type names are case-sensitive. The
``independent`` dimension type is special because variable dimensions associated
with this dimension type need not be of the same length (in contrast to all
other dimension types). The ``independent`` dimension type is represented in
Python by ``None``.

Each :py:class:`harp.Variable` instance contains an attribute ``dimension``,
which is a list of dimension types. For each dimension of a variable, the
``dimension`` attribute indicates the dimension type it is associated with.

The dimension types supported by HARP are:

time
    Temporal dimension; this is the only appendable dimension.

vertical
    Vertical dimension, indicating height or depth.

spectral
    Spectral dimension, associated with wavelength, wavenumber, or frequency.

latitude
    Latitude dimension, only to be used for the latitude axis of a regular
    (latitude, longitude) grid.

longitude
    Longitude dimension, only to be used for the longitude axis of a regular
    (latitude, longitude) grid.

independent
    Independent dimension, used to index other quantities, such as the corner
    coordinates of ground pixel polygons.

Data types
----------

The HARP Python interface takes care of the conversion of product and variables
from the C domain to the Python domain and back. This section describes the
relation between types in the C domain and types in the Python domain.

The table below shows the type map that is used to convert the high level
concepts product and variable.

+---------------+------------------+
| C type        | Python type      |
+===============+==================+
| harp_product  | harp.Product     |
+---------------+------------------+
| harp_variable | harp.Variable    |
+---------------+------------------+

The table below shows the type map that is used when importing or ingesting a
product, i.e. when translating from the C domain to the Python domain.

Variable data arrays are converted to NumPy arrays. The NumPy data type used for
the converted array is determined from the HARP data type of the variable
according to the type map shown below. Zero-dimensional arrays of length 1 are
converted to Python scalars using the ``numpy.asscalar()`` function. The
resulting Python type is also shown in the type map.

Product and variable attributes, being scalars, are converted directly to Python
scalars. The Python type is determined from the HARP data type according to the
type map.

Zero-terminated C strings are always converted to instances of type ``str`` in
Python. See section :ref:`Unicode <unicode-details>` for details on unicode
decoding in Python 3.

+------------------+----------------+-------------+------------------+
| HARP data type   | NumPy dtype    | Python type | unicode decoding |
+==================+================+=============+==================+
| harp_type_int8   | numpy.int8     | int         |                  |
+------------------+----------------+-------------+------------------+
| harp_type_int16  | numpy.int16    | int         |                  |
+------------------+----------------+-------------+------------------+
| harp_type_int32  | numpy.int32    | int         |                  |
+------------------+----------------+-------------+------------------+
| harp_type_float  | numpy.float32  | float       |                  |
+------------------+----------------+-------------+------------------+
| harp_type_double | numpy.float64  | float       |                  |
+------------------+----------------+-------------+------------------+
| harp_type_string | numpy.object\_ | str         | Python 3         |
+------------------+----------------+-------------+------------------+

The table below shows the type map that is used when exporting a product, i.e.
when translating from the Python domain to the C domain.

NumPy object arrays (that is, NumPy arrays with data type ``numpy.object_``)
will be converted to arrays of zero-terminated C strings. The elements of a
NumPy object array must be all ``str`` or all ``bytes``. (Note that on Python 2,
``bytes`` is an alias of ``str``.) NumPy arrays with data type ``numpy.str_`` or
``numpy.bytes_`` will be converted to arrays of zero-terminated C strings as
well.

NumPy scalars with data type ``numpy.object_``, ``numpy.str_``, or
``numpy.bytes_`` are converted following the same rules as for NumPy arrays.
NumPy scalars are treated as NumPy arrays of length 1 in this respect. Python
scalars of type ``str`` or ``bytes`` will also be converted to zero-terminated C
strings.

Unicode encoding is only performed for array elements or scalars of type ``str``
or ``numpy.str_``, and only on Python 3. See section :ref:`Unicode
<unicode\-details>` for details on unicode encoding in Python 3.

Any NumPy array, NumPy scalar, or Python scalar that cannot be converted
according to the rules described above is assumed to be numeric. An attempt will
be made to determine the minimal HARP data type that it, or its elements, can be
safely cast to (according to the function ``numpy.can_cast()`` using the
``'safe'`` casting option). See the type map for details.

+-----------------+----------------+------------------+--------------------+-------------------------+-------------------+------------------+
| Python type     | NumPy dtype    | type test        | array element type | array element type test | HARP data type    | unicode encoding |
+=================+================+==================+====================+=========================+===================+==================+
| numpy.ndarray   | numpy.object\_ | numpy.issubdtype | str                | isinstance              | harp_type_string  | Python 3         |
| numpy.generic   |                |                  +--------------------+-------------------------+-------------------+------------------+
|                 |                |                  | bytes              | isinstance              | harp_type_string  | no               |
|                 +----------------+------------------+--------------------+-------------------------+-------------------+------------------+
|                 | numpy.str\_    | numpy.issubdtype |                    |                         | harp_type_string  | Python 3         |
|                 +----------------+------------------+--------------------+-------------------------+-------------------+------------------+
|                 | numpy.bytes\_  | numpy.issubdtype |                    |                         | harp_type_string  | no               |
|                 +----------------+------------------+--------------------+-------------------------+-------------------+------------------+
|                 | numpy.int8     | numpy.can_cast   |                    |                         | harp_type_int8    |                  |
|                 +----------------+------------------+--------------------+-------------------------+-------------------+------------------+
|                 | numpy.int16    | numpy.can_cast   |                    |                         | harp_type_int16   |                  |
|                 +----------------+------------------+--------------------+-------------------------+-------------------+------------------+
|                 | numpy.int32    | numpy.can_cast   |                    |                         | harp_type_int32   |                  |
|                 +----------------+------------------+--------------------+-------------------------+-------------------+------------------+
|                 | numpy.float32  | numpy.can_cast   |                    |                         | harp_type_float32 |                  |
|                 +----------------+------------------+--------------------+-------------------------+-------------------+------------------+
|                 | numpy.float64  | numpy.can_cast   |                    |                         | harp_type_float64 |                  |
+-----------------+----------------+------------------+--------------------+-------------------------+-------------------+------------------+
| str             |                | isinstance       |                    |                         | harp_type_string  | Python 3         |
+-----------------+----------------+------------------+--------------------+-------------------------+-------------------+------------------+
| bytes           |                | isinstance       |                    |                         | harp_type_string  | no               |
+-----------------+----------------+------------------+--------------------+-------------------------+-------------------+------------------+
| any other type  | numpy.int8     | numpy.can_cast   |                    |                         | harp_type_int8    |                  |
|                 +----------------+------------------+--------------------+-------------------------+-------------------+------------------+
|                 | numpy.int16    | numpy.can_cast   |                    |                         | harp_type_int16   |                  |
|                 +----------------+------------------+--------------------+-------------------------+-------------------+------------------+
|                 | numpy.int32    | numpy.can_cast   |                    |                         | harp_type_int32   |                  |
|                 +----------------+------------------+--------------------+-------------------------+-------------------+------------------+
|                 | numpy.float32  | numpy.can_cast   |                    |                         | harp_type_float32 |                  |
|                 +----------------+------------------+--------------------+-------------------------+-------------------+------------------+
|                 | numpy.float64  | numpy.can_cast   |                    |                         | harp_type_float64 |                  |
+-----------------+----------------+------------------+--------------------+-------------------------+-------------------+------------------+

.. _unicode-details:

Unicode
-------

Zero-terminated C strings received from the HARP C library are always converted
to instances of type ``str`` in Python. Type ``str`` is a byte string in Python
2, but a unicode string in Python 3.

In Python 2, no unicode encoding or decoding is performed by the HARP Python
interface.

In Python 3, byte strings received from the HARP C library are decoded using a
configurable encoding. Unicode strings (instances of type ``str``) are encoded
using the same encoding into byte strings, which are sent to the HARP C library.
Byte strings (instances of type ``bytes``) are passed through without encoding.

The encoding used can be configured by the user, see the
:py:func:`harp.set_encoding` and :py:func:`harp.get_encoding` methods. The
default encoding is ``'ascii'``.

Examples
--------

.. code-block:: python

   from __future__ import print_function

   import harp
   import numpy

   # Create a product in Python and export it as a NetCDF file.
   product = harp.Product()
   harp.export_product(product, "empty.nc")

   # Add some variables to the product.
   product.foo = harp.Variable("foo")
   product.strings = harp.Variable(numpy.array(("foo", "bar", "baz")), ["time"])
   product.temperature = harp.Variable(numpy.ones((3, 5), dtype=numpy.float32),
                                       ["time", None])
   product.temperature.unit = "K"
   product.temperature.description = "temperature"

   # Pretty print information about the product.
   print(product)

   # Pretty print information about the variable 'temperature'.
   print(product.temperature)

   # Set valid minimum value of the variable 'temperature'. Note the use of item
   # access syntax instead of attribute access syntax.
   product["temperature"].valid_min = 0.0
   print(product.temperature)

   # Export the updated product as an HDF4 file.
   harp.export_product(product, "non-empty.hdf", file_format="hdf4")

   # Convert the product to an OrderedDict.
   dict_product = harp.to_dict(product)

   # Ingest an S5P L2 HCHO product.
   hcho_product = harp.ingest_product("S5P_NRTI_L2__HCHO___20080808T224727_20080808T234211_21635_01_021797_00000000T000000.nc",
                                      "solar_zenith_angle < 60 [degree]; latitude > 30 [degree_north]; latitude < 60 [degree_north]")

   # Pretty print information about the product.
   print(hcho_product)

   # Export the product as a HARP compliant data product.
   harp.export_product(hcho_product, "hcho.h5", file_format='hdf5')

API reference
-------------

This section describes the types, functions, and exceptions defined by the HARP
Python interface.

Types
^^^^^

This section describes the types defined by the HARP Python interface.

.. py:class:: harp.Product(source_product="", history="")

   Python representation of a HARP product.

   A product consists of product attributes and variables. Any attribute of a
   Product instance of which the name does not start with an underscore is
   either a variable or a product attribute. Product attribute names are
   reserved and cannot be used for variables.

   The list of names reserved for product attributes is:

   source_product
       Name of the original product this product is derived from.
   history
       New-line separated list of invocations of HARP command line tools that
       have been performed on the product.

   Variables can be accessed by name using either the attribute access ``'.'``
   syntax, or the item access ``'[]'`` syntax. For example:

   .. code-block:: python

      from __future__ import print_function

      # Alternative ways to access the variable 'HCHO_column_number_density'.
      density = product.HCHO_column_number_density
      density = product["HCHO_column_number_density"]

      # Iterate over all variables in the product. For imported or ingested
      # products, the order of the variables is the same as the order in the
      # source product.
      for name in product:
          print(product[name].unit)

   Product attributes can be accessed in the same way as variables, but are
   *not* included when iterating over the variables in a product. For example:

   .. code-block:: python

      from __future__ import print_function

      # Print product attributes.
      print(product.source_product)
      print(product.history)

   :param str source_product: Name of the original product this product is
                              derived from.
   :param str history: New-line separated list of invocations of HARP command
                       line tools that have been performed on the product.

.. py:class:: harp.Variable(data, dimension=[], unit=None, valid_min=None, \
                            valid_max=None, description=None)

   Python representation of a HARP variable.

   A variable consists of data (either a scalar or NumPy array), a list of
   dimension types that describe the dimensions of the data, and a number of
   optional attributes: physical unit, minimum valid value, maximum valid value,
   and a human-readable description.

   :param data: Value(s) associated with the variable; can be either a scalar or
                a NumPy array.
   :param list dimension: List of strings indicating the dimensions the variable
                          depends on.
   :param str unit: Physical unit the values associated with the variable are
                    expressed in.
   :param valid_min: Minimum valid value; any value below this threshold is
                     considered to be invalid.
   :param valid_max: Maximum valid value; any value above this threshold is
                     considered to be invalid.
   :param str description: Humand-readble description of the variable.

Functions
^^^^^^^^^

This section describes the functions defined by the HARP Python library.

.. py:function:: harp.ingest_product(filename, operations="", options="")

   Ingest a product of a type supported by HARP.

   :param str filename: Filename of the product to ingest.
   :param str operations: Actions to apply as part of the ingestion; should be
                       specified as a semi-colon separated string of operations.
   :param str options: Ingestion module specific options; should be specified as
                       a semi-colon separated string of key=value pairs.
   :returns: Ingested product.
   :rtype: harp.Product

.. py:function:: harp.import_product(filename, operations="")

   Import a HARP compliant product.

   The file format (NetCDF/HDF4/HDF5) of the product will be auto-detected.

   :param str filename: Filename of the product to import.
   :param str operations: Actions to execute on the product after it has been
                       imported; should be specified as a semi-colon separated
                       string of operations.
   :returns: Imported product.
   :rtype: harp.Product

.. py:function:: harp.export_product(product, filename, file_format="netcdf")

   Export a HARP compliant product.

   :param str product: Product to export.
   :param str filename: Filename of the exported product.
   :param str file_format: File format to use; one of 'netcdf', 'hdf4', or
                           'hdf5'.

.. py:function:: harp.to_dict(product)

   Convert a :py:class:`harp.Product` instance to an ``OrderedDict``.

   The ``OrderedDict`` representation provides direct access to the data
   associated with each variable. All product attributes and all variable
   attributes except the unit attribute are discarded as part of the conversion.

   The unit attribute of a variable is represented by adding a scalar variable
   of type string with the name of the corresponding variable suffixed with
   ``'_unit'`` as name and the unit as value.

   The ``OrderedDict`` representation can be convenient when there is a need to
   interface with existing code such as plotting libraries, or when the
   additional information provided by the Product representation is not needed.

   Note that only :py:class:`harp.Product` instances can be exported as a HARP
   product. The ``OrderedDict`` representation does not contain enough
   information.

   For example:

   .. code-block:: python

      from __future__ import print_function

      # Convert input product to an OrderedDict.
      product = to_dict(input_product)

      # Accessing the variable 'HCHO_column_number_density'.
      product["HCHO_column_number_density"]

      # Accessing the unit attribute of the variable
      # 'HCHO_column_number_density'.
      product["HCHO_column_number_density_unit"]

      # Iterate over all variables in the product. For imported or ingested
      # products, the order of the variables is the same as the order in the
      # source product.
      for name, value in product.items():
          print name, value

   :param harp.Product product: Product to convert.
   :returns: Converted product.
   :rtype: collections.OrderedDict

.. py:function:: harp.get_encoding()

   Return the encoding used to convert between unicode strings and C strings
   (only relevant when using Python 3).

   :returns: Encoding currently in use.
   :rtype: str

.. py:function:: harp.set_encoding(encoding)

   Set the encoding used to convert between unicode strings and C strings
   (only relevant when using Python 3).

   :param str encoding: Encoding to use.

.. py:function:: harp.version()

   Return the version of the HARP C library.

   :returns: HARP C library version.
   :rtype: str

Exceptions
^^^^^^^^^^

This sections describes the exceptions defined by the HARP Python interface.

.. py:exception:: harp.Error(*args)

   Exception base class for all HARP Python interface errors.

   :param tuple args: Tuple of arguments passed to the constructor; usually a
                      single string containing an error message.

.. py:exception:: harp.CLibraryError(errno=None, strerror=None)

   Exception raised when an error occurs inside the HARP C library.

   :param str errno: error code; if None, the error code will be retrieved from
                     the HARP C library.
   :param str strerror: error message; if None, the error message will be
                        retrieved from the HARP C library.

.. py:exception:: harp.UnsupportedTypeError(*args)

   Exception raised when unsupported types are encountered, either on the Python
   or on the C side of the interface.

   :param tuple args: Tuple of arguments passed to the constructor; usually a
                      single string containing an error message.

.. py:exception:: harp.UnsupportedDimensionError(*args)

   Exception raised when unsupported dimensions are encountered, either on the
   Python or on the C side of the interface.

   :param tuple args: Tuple of arguments passed to the constructor; usually a
                      single string containing an error message.

.. py:exception:: harp.NoDataError()

   Exception raised when the product returned from an ingestion or import
   contains no variables, or variables without data.
