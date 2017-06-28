HDF5/netCDF-4
=============

This section details a set of additional conventions that are specific to the HDF5 file format.

Note that this section also applies to the netCDF-4 format, since netCDF-4 actually uses HDF5 underneath.

The following table shows the mapping between HARP data types and HDF5 data types. The HDF5 backend uses this mapping
for writing only.

============== =================
HARP data type HDF5 data type
============== =================
int8           H5T_NATIVE_SCHAR
int16          H5T_NATIVE_SHORT
int32          H5T_NATIVE_INT
float          H5T_NATIVE_FLOAT
double         H5T_NATIVE_DOUBLE
string         H5T_C_S1
============== =================

The mapping used by the HDF5 backend for reading is shown below. The HDF5 data type interface (H5T) is used to
introspect the data type of the variable to be read.

=============== ============== ============== ==================== ==============
H5T_get_class() H5T_get_size() H5T_get_sign() H5Tget_native_type() HARP data type
=============== ============== ============== ==================== ==============
H5T_INTEGER     1              H5T_SGN_2                           int8
H5T_INTEGER     2              H5T_SGN_2                           int16
H5T_INTEGER     4              H5T_SGN_2                           int32
H5T_FLOAT                                     H5T_NATIVE_FLOAT     float
H5T_FLOAT                                     H5T_NATIVE_DOUBLE    double
H5T_STRING                                                         string
=============== ============== ============== ==================== ==============

HDF5 data types not covered in this table are not supported by HARP.

In the HDF5 data model there is no concept of shared dimensions (unlike netCDF). The shape of an HDF5 dataset is
specified as a list of dimension lengths. However, the netCDF-4 library uses HDF5 as its storage backend. It represents
shared dimensions using HDF5 *dimension scales*.

Dimension scales were introduced in HDF5 version 1.8.0. A dimension scale is a special dataset that can be attached to
one or more dimensions of other datasets. Multiple dimension scales can be attached to a single dimension, and the
length of the dimension scale does not have to be the same as the length of the dimension it is attached to. There are
no limitations on the shape or dimensionality of a dimension scale, since it is just a dataset with particular
attributes attached.

To represent shared dimensions, netCDF-4 creates dimension scales for each shared dimension and attaches these dimension
scales to the corresponding dimensions of all variables. If a product contains a variable with the same name as a shared
dimension, the dataset containing the values of the variable will be used as the dimension scale. Such a variable is
called a *coordinate variable* in netCDF-4. Note that in a HARP product, due to the variable naming convention, only
variables called ``latitude`` or ``longitude`` could possibly be coordinate variables. For shared dimensions for which a
variable with the same name does not exist, a stub dataset containing fill values is created and used as the dimension
scale. The optional ``NAME`` attribute of the dimension scale is set to ``This is a netCDF dimension but not a netCDF
variable.``, which causes the netCDF-4 library to hide the stub dataset from the user. For more information about the
netCDF-4 format, see the `NetCDF User's Guide`_.

.. _`NetCDF User's Guide`: http://www.unidata.ucar.edu/software/netcdf/docs/file_format_specifications.html#netcdf_4_spec

The HDF5 file format conventions used by HARP are designed to be compatible with netCDF-4. Like netCDF-4, HARP uses
dimension scales to represent shared dimensions. For independent dimensions, the same approach is used as for the
netCDF-3 backend. For each unique dimension length ``L``, a dimension scale named ``independent_L`` is created.

To summarize, HARP dimensions types are mapped to HDF5 dimension scales as follows:

=================== =====================
HARP dimension type HDF5 dimension scale
=================== =====================
time                time
latitude            latitude
longitude           longitude
vertical            vertical
spectral            spectral
independent         independent\_<length>
=================== =====================

The ``_nc3_strict`` attribute is attached to the root group of the HDF5 file such that it will be interpreted using the
netCDF classic data model by the netCDF-4 library. Enhanced features of netCDF-4 beyond the classic data model, such as
groups and user-defined types, are not supported by HARP.

HDF5 can represent strings in several ways. Both fixed and variable length strings are supported. The HDF5 backend
stores a HARP variable of type string as an HDF5 dataset of fixed length strings. The fixed string length equals the
length of the longest string, or 1 if the length of the longest string is zero. Shorter strings are padded with null-
termination characters.

HARP uses empty strings to represent the unit of dimensionless quantities (to distinguish them from non-quantities,
which will lack a unit attribute). However, HDF5 cannot store string attributes with length zero. For this reason
an empty unit string will be written as a ``units`` attribute with value ``"1"`` when writing data to HDF5.
When reading from HDF5 a unit string value ``"1"`` will be converted back again to an empty unit string.

Note that even though the ``time`` dimension is conceptually considered `appendable`, this dimension is not stored as an
actual appendable dimension in HDF5. Products are read/written from/to files in full and are only modified in memory.
The `appendable` aspect is only relevant for tools such as plotting routines that combine the data from a series of HARP
products in order to provide plots/statistics for a whole dataset (and thus, where data from different files will have
to be concatenated).
