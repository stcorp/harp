IDL interface
================

The Interactive Data Language (IDL) interface consists of a Dynamic Loadable
Module (DLM) that provides the `harp_import`, `harp_export`, `harp_version`,
and 'harp_unload' functions.

Products are represented in IDL as structures which can be manipulated freely
from within IDL. A product structure contains a field for each variable
contained in the product and contains the global attributes `source_product`
and `history` (if available). Each variable itself is again a structure containing
the variable attributes (`unit`, `description`, etc.) and a field `data` that
contains the data of the variable. The structure of a variable also contains a
field `name` that contains the case-sensitive name of the variable and (if the
variable is not a scalar) a field `dimension` that contains a list of dimension
names for each dimension of the variable.

Products can be exported as HARP compliant products in any of the file formats
supported by the HARP C library (netCDF/HDF4/HDF5). Such exported products can
subsequently be processed further using the :doc:`HARP command line tools <tools>`.

Dimension types
---------------

Each non-scalar variable will have a `dimension` field in its structure, which
is a list of strings representing the dimension types (e.g. `time`, `vertical`,
`latitude`, etc.).

Data types
----------

The HARP IDL interface takes care of the conversion of product and variables
from the C domain to the IDL domain and back. This section describes the
relation between types in the C domain and types in the IDL domain.

The table below shows the type map that is used when importing or ingesting a
product, i.e. when translating from the C domain to the IDL domain.

Variable data arrays are converted to arrays in the product structure. The data
type used for the converted array is determined from the HARP data type of the
variable according to the type map shown below. Zero-dimensional arrays of
length 1 are converted to IDL scalars. The resulting IDL type is also shown in
the type map.

+------------------+----------+
| HARP data type   | IDL type |
+==================+==========+
| harp_type_int8   | byte     |
+------------------+----------+
| harp_type_int16  | int      |
+------------------+----------+
| harp_type_int32  | long     |
+------------------+----------+
| harp_type_float  | float    |
+------------------+----------+
| harp_type_double | double   |
+------------------+----------+
| harp_type_string | string   |
+------------------+----------+

Note that the IDL `byte` type is an unsigned type (since IDL does not have any
8-bit signed type).
The HARP IDL interface will just hard cast signed int8 values to unsigned uint8
values (e.g. -1 will become 255). Make sure that in your operations within IDL
on these 8-bit integers you als treat them as mapped signed integers.
Note also that this holds for the `valid_min` and `valid_max` attributes
(e.g. `valid_min` may end up being higher than `valid_max` in IDL).

Unicode
-------

Zero-terminated C strings received from the HARP C library are always converted
to instances of type ``string`` in IDL which are unicode strings.

Examples
--------

.. code-block:: IDL

   ; Import a file as a HARP product.
   prod = harp_import("filename.ext")

   ; Print information about the product.
   help, prod, /struct

   ; Print information about the variable 'temperature'.
   help, prod.temperature, /struct

   ; Print the contents of the variable 'temperature'.
   print, prod.temperature.data

   ; Export the updated product as an HDF4 file (the format must be
   ; HDF4, HDF5 or netCDF, if no format is specified netCDF is used).
   result = harp_export(prod, "filename.hdf", "hdf4")

   ; Print the result of the export.
   print, result

   ; Import the HDF4 file and perform an operation to exclude the variable
   ; temperature (variable name must be in uppercase).
   prod2 = harp_import("filename.hdf", "exclude(temperature)");

   ; Print information about the product.
   help, prod2, /struct

API reference
-------------

This section describes the functions defined by the HARP IDL interface.

.. Note: The py:function does not mean that these are Python functions, it just
.. means that we use the python formatting in Sphinx.

.. py:function:: harp_import(filename, operations="", options="")

   Import a product from a file.
 
   This will first try to import the file as an HDF4, HDF5, or netCDF file that
   complies to the HARP Data Format. If the file is not stored using the HARP
   format then it will try to import it using one of the available ingestion
   modules.

   If the filename argument is a list of filenames or a globbing (glob.glob())
   pattern then the harp.import_product() function will be called on each
   individual file and the result of harp.concatenate() on the imported products
   will be returned.

   :param str filename: Filename of the product to ingest
   :param str operations: Actions to apply as part of the import; should be
                       specified as a semi-colon separated string of operations.
   :param str options: Ingestion module specific options; should be specified as
                       a semi-colon separated string of key=value pairs; only
                       used if the file is not in HARP format.
   :returns: Ingested product or error structure.

.. py:function:: harp_export(product, filename, file_format="netcdf")

   Export a HARP compliant product.

   :param str product: Product to export.
   :param str filename: Filename of the exported product.
   :param str file_format: File format to use; one of 'netcdf', 'hdf4', or
                           'hdf5'. If no format is specified, netcdf is used.
   :returns: Error structure with result code.

.. py:function:: harp_version()

   The harp_version function returns a string containing the current version
   number of HARP. The version number is always of the format 'x.y.z', i.e.,
   major, minor, and revision numbers, separated by dots.

   :returns: HARP version number.

.. py:function:: harp_unload()

   The harp_unload procedure will clean up any HARP resources. At the first
   call to a HARP IDL function the HARP C Library will be initialized which
   will require some memory.
   A call to harp_unload can then be used to clean up these HARP resources.
   After a clean up, the first call to a HARP IDL function will initialize
   the HARP C Library again.

   This function may be (slightly) useful on systems with little memory.
