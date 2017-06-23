MATLAB interface
================

The Matrix Laboratory (MATLAB) interface consists of a series of M-files
that provide the `harp_import`, `harp_export` and `harp_version` functions.

Products are represented in MATLAB as structures which can be manipulated freely
from within MATLAB. A product structure contains an array field for each variable
contained in the product.

Products can be exported as HARP compliant products in any of the file formats
supported by the HARP C library (netCDF/HDF4/HDF5). Such exported products can
subsequently be processed further using the :doc:`HARP command line tools <tools>`.

Dimension types
---------------

The HARP C library defines several dimension types (time, vertical, latitude
etc.). This information is stored in the dimension_type field of each variable
in MATLAB. If a variable is a scalar (which has no dimensions), the dimension_type
field is not present.

Data types
----------

The HARP MATLAB interface takes care of the conversion of product and variables
from the C domain to the MATLAB domain and back. This section describes the
relation between types in the C domain and types in the MATLAB domain.

The table below shows the type map that is used when importing or ingesting a
product, i.e. when translating from the C domain to the MATLAB domain.

Variable data arrays are converted to arrays in the product structure. The data
type used for the converted array is determined from the HARP data type of the
variable according to the type map shown below. Zero-dimensional arrays of
length 1 are converted to MATLAB arrays of 1x1. The resulting MATLAB type is also
shown in the type map.

+------------------+-------------+
| HARP data type   | MATLAB type |
+==================+=============+
| harp_type_int8   | int8        |
+------------------+-------------+
| harp_type_int16  | int16       |
+------------------+-------------+
| harp_type_int32  | int32       |
+------------------+-------------+
| harp_type_float  | single      |
+------------------+-------------+
| harp_type_double | double      |
+------------------+-------------+
| harp_type_string | char        |
+------------------+-------------+

Unicode
-------

Zero-terminated C strings received from the HARP C library are always converted
to arrays of mxCHAR in MATLAB which is an unicode string.

Examples
--------

.. code-block:: MATLAB

   ; Ingest a file and convert it to a HARP product (the
   ; file that is used in this example is an ACE-FTS file).
   product1 = harp_import("ss13799.asc")

   ; Print information about the product.
   disp(product1)

   ; Print information about the variable 'temperature'.
   disp(product1.temperature)

   ; Print the contents of the variable 'temperature'.
   product1.temperature.value

   ; Export the updated product as an HDF4 file (the format must be
   ; HDF4, HDF5 or netCDF, if no format is specified netCDF is used).
   result = harp_export(product1, "ace_fts_ss13799.hdf", "hdf4")

   ; Print the result of the export.
   disp(result)

   ; Import the HDF4 file and perform an operation to exclude the variable
   ; temperature (variable name must be in uppercase).
   product2 = harp_import("ace_fts_ss13799.hdf", "exclude(TEMPERATURE)");

   ; Print information about the product.
   disp(product2)

API reference
-------------

This section describes the functions defined by the HARP MATLAB interface.

.. Note: The py:function does not mean that these are Python functions, it just
.. means that we use the python formatting in Sphinx.

.. py:function:: harp_import(filename, operations='', options='')
   :noindex:

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
   :returns: Ingested product.

.. py:function:: harp_export(product, filename, file_format='netcdf')
   :noindex:

   Export a HARP compliant product.

   :param str product: Product to export.
   :param str filename: Filename of the exported product.
   :param str file_format: File format to use; one of 'netcdf', 'hdf4', or
                           'hdf5'. If no format is specified, netcdf is used.
