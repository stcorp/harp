IDL interface
================

The Interactive Data Language (IDL) interface consists of a Dynamic Loadable
Module (DLM) that provides the `harp_import`, `harp_export` and `harp_unload`
functions.

Products are represented in IDL as structures which can be manipulated freely
from within IDL. A product structure contains a field for each variable
contained in the product.

Products can be exported as HARP compliant products in any of the file formats
supported by the HARP C library (netCDF/HDF4/HDF5). Such exported products can
subsequently be processed further using the :doc:`HARP command line tools <tools>`.

Dimension types
---------------

The HARP C library defines several dimension types (time, vertical, latitude
etc.) but this information is not available in IDL.

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

Unicode
-------

Zero-terminated C strings received from the HARP C library are always converted
to instances of type ``string`` in IDL which is an unicode string.

Examples
--------

.. code-block:: IDL

   ; Import a file as a HARP product.
   prod = harp_import("filename.ext")

   ; Print information about the product.
   help, prod, /struct

   ; Print information about the variable 'temperature'.
   help, prod.temperature

   ; Print the contents of the variable 'temperature'.
   print, prod.temperature

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
