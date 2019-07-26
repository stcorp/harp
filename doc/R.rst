R interface
===========

The R interface consists of an R package that provides the `import`, `export` and
`version` functions.

Products are represented in R as lists which can be manipulated freely
from within R. A product structure contains a sub-list field for each variable
contained in the product.

Products can be exported as HARP compliant products in any of the file formats
supported by the HARP C library (netCDF/HDF4/HDF5). Such exported products can
subsequently be processed further using the :doc:`HARP command line tools <tools>`.

Dimension types
---------------

The HARP C library defines several dimension types (time, vertical, latitude
etc.). This information is stored in the `dimension` field of each variable
in R. If a variable is a scalar (which has no dimensions), the `dimension`
field is `NULL`.

Because of a mismatch in storage between the C and R side (row-major versus
column-major), the dimension order is reversed on the R side.

Data types
----------

The HARP R interface takes care of the conversion of product and variables
from the C domain to the R domain and back. This section describes the
relation between types in the C domain and types in the R domain.

The table below shows the type map that is used when importing or ingesting a
product, i.e. when translating from the C domain to the R domain.

Variable data arrays are converted to arrays in the product structure. The data
type used for the converted array is determined from the HARP data type of the
variable according to the type map shown below. Zero-dimensional arrays of
length 1 are converted to R arrays of 1x1. The resulting R type is also
shown in the type map.

+------------------+-------------+
| HARP data type   | R type      |
+==================+=============+
| harp_type_int8   | integer     |
+------------------+-------------+
| harp_type_int16  | integer     |
+------------------+-------------+
| harp_type_int32  | integer     |
+------------------+-------------+
| harp_type_float  | real        |
+------------------+-------------+
| harp_type_double | real        |
+------------------+-------------+
| harp_type_string | string      |
+------------------+-------------+

Unicode
-------

Zero-terminated C strings received from the HARP C library are always converted
to arrays of mxCHAR in R which is a unicode string.

Examples
--------

.. code-block:: R

   # Ingest a file and convert it to a HARP product (the
   # file that is used in this example is an S5P L2 HCHO product).
   hcho_product = harp::import("S5P_NRTI_L2__HCHO___20080808T224727_20080808T234211_21635_01_021797_00000000T000000.nc",
                               "solar_zenith_angle < 60 [degree]; latitude > 30 [degree_north]; latitude < 60 [degree_north]")

   # Print product
   print(hcho_product)

   # Print variable 'tropospheric_HCHO_column_number_density'.
   print(hcho_product$tropospheric_HCHO_column_number_density)

   # Export the updated product as an HDF5 file (the format must be
   # HDF4, HDF5 or netCDF, if no format is specified netCDF is used).
   harp::export(hcho_product, "product.h5", "hdf5")

   # Import the HDF5 file and perform an operation to exclude the variable
   # solar_azimuth_angle.
   hcho_product2 = harp::import("product.h5", "exclude(solar_azimuth_angle)");

API reference
-------------

This section describes the functions defined by the HARP R interface.

.. Note: The py:function does not mean that these are Python functions, it just
.. means that we use the python formatting in Sphinx.

.. py:function:: import(filename, operations="", options="")
   :noindex:

   Import a product from a file.

   This will first try to import the file as an HDF4, HDF5, or netCDF file that
   complies to the HARP Data Format. If the file is not stored using the HARP
   format then it will try to import it using one of the available ingestion
   modules.

   :param str filename: Filename of the product to ingest
   :param str operations: Actions to apply as part of the import; should be
                       specified as a semi-colon separated string of operations.
   :param str options: Ingestion module specific options; should be specified as
                       a semi-colon separated string of key=value pairs; only
                       used if the file is not in HARP format.
   :returns: Ingested product.

.. py:function:: export(product, filename, file_format="netcdf")
   :noindex:

   Export a HARP compliant product.

   :param product: Product to export.
   :param str filename: Filename of the exported product.
   :param str file_format: File format to use; one of 'netcdf', 'hdf4', or
                           'hdf5'. If no format is specified, netcdf is used.

.. py:function:: version()
   :noindex:

   Returns the version number of HARP.
