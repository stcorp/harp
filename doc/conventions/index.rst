Conventions
===========

One of the most important harmonization aspects of HARP is that of the representation of data.
Having a well defined structure for data and a strict naming convention for quantities not only makes it possible to
define operations at a higher level (which will be able to automatically do the right thing for you) but also allows
operations to be defined in a modular approach, such that the output of one operation can be used as input to a next one.

The HARP conventions for data are strongly linked to the format in which the data can be written to files on disk.
This format is called the HARP data format and is a set of conventions for files stored in netCDF-3, HDF4, or HDF5 format.
The netCDF-4 format actually uses HDF5 underneath and is therefore supported as an HDF5 file by HARP.

A single file is called a `product` in HARP. A `product` can have one or more `variables` and `global attributes`.
Each `variable` contains a multi-dimensional array of values for a single quantity.
Each of the `dimensions` of a variable is of a specific type and `variables` can have `attributes` to provide
information such as a description, unit, etc.
A collection of products from the same source can be grouped into a `dataset`.

Each of the specific aspects of the HARP standard are described in the following sections:

.. toctree::
   :maxdepth: 2

   datatypes
   dimensions
   global_attributes
   variable_attributes
   variables
   variable_names
   datetime
   filenames
   netcdf3
   hdf4
   hdf5
   collocation_result
