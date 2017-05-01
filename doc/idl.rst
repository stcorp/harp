IDL interface
================

The Interactive Data Language (IDL) interface consists of a Dynamic Loadable
Module (DLM) file named ``'harp_idl.dlm'`` that provides the interface of the
following functions `harp_ingest`, `harp_import`, `harp_export` and
`harp_unload` and a shared library named ``'harp_idl.so'`` that provides the
implementation of those functions.

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

   ; Ingest a file and convert it to a HARP product (the
   ; file that is used in this example is an ACE-FTS file).
   product1 = harp_ingest("ss13799.asc")

   ; Print information about the product.
   help,product1

   ; Print information about the variable 'temperature'.
   help,product1.temperature

   ; Print the contents of the variable 'temperature'.
   print,product1.temperature

   ; Export the updated product as an HDF4 file (the format must be
   ; HDF4, HDF5 or netCDF, if no format is specified netCDF is used).
   result = harp_export(product1, "ace_fts_ss13799.hdf", "hdf4")

   ; Print the result of the export.
   print,result

   ; Import the HDF4 file and perform an operation to exclude the variable
   ; temperature (variable name must be in uppercase).
   product2 = harp_import("ace_fts_ss13799.hdf", "exclude(TEMPERATURE)");

   ; Print information about the product.
   help,product2

API reference
-------------

This section describes the functions defined by the HARP IDL interface.

.. Note: The py:function does not mean that these are Python functions, it just
.. means that we use the python formatting in Sphinx.

.. py:function:: harp_ingest(filename, operations="", options="")

   Ingest a product of a type supported by HARP.

   :param str filename: Filename of the product to ingest
   :param str operations: Actions to apply as part of the ingestion; should be
                       specified as a semi-colon separated string of operations.
   :param str options: Ingestion module specific options; should be specified as
                       a semi-colon separated string of key=value pairs.
   :returns: Ingested product.

.. py:function:: harp_import(filename, operations="")

   Import a HARP compliant product.

   The file format (netCDF/HDF4/HDF5) of the product will be auto-detected.

   :param str filename: Filename of the product to import
   :param str operations: Actions to execute on the product after it has been
                       imported; should be specified as a semi-colon separated
                       string of operations.
   :returns: Imported product.

.. py:function:: harp_export(product, filename, file_format="netCDF")

   Export a HARP compliant product.

   :param str product: Product to export.
   :param str filename: Filename of the exported product.
   :param str file_format: File format to use; one of 'netCDF', 'HDF4', or
                           'HDF5'. If no format is specified, netCDF is used.

