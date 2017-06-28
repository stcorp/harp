HDF4
====

This section details a set of additional conventions that are specific to the HDF4 file format.

The following table shows the mapping between HARP data types and HDF4 data types. HDF4 data types not covered in this
table are not supported by HARP.

============== ==============
HARP data type HDF4 data type
============== ==============
int8           DFNT_INT8
int16          DFNT_INT16
int32          DFNT_INT32
float          DFNT_FLOAT32
double         DFNT_FLOAT64
string         DFNT_CHAR
============== ==============

In the HDF4 data model there is no concept of shared dimensions (unlike netCDF). The shape of an HDF4 dataset is
specified as a list of dimension lengths.

When a HARP variable is stored as an HDF4 dataset, dimension lengths are preserved, but dimension types are lost. A
dataset attribute named 'dims' is used to store the type of each dimension of the associated dataset as a comma-
separated list of dimension type names. The number of dimension types equals the number of dimensions of the HDF4
*dataset*. This number is equal to the number of dimensions of the HARP variable, *except* for scalar variables and
variables of type string (see below).

HDF4 does not support scalars (datasets with zero dimensions). The HDF4 backend stores a scalar HARP variable as an HDF4
dataset with a single dimension of length 1. To differentiate between scalars and proper 1-D variables, both of which
are stored as 1-D HDF4 datasets, the introduced dimension is included in the dimension type list using the dimension
type name 'scalar'.

HDF4 does not support strings. The HDF4 backend stores an N-dimensional HARP variable of type string as an
(N+1)-dimensional HDF4 dataset of type DFNT_CHAR. The length of the introduced dimension equals the length of the
longest string, or 1 if the length of the longest string is zero. Shorter strings are padded with null-termination
characters. The introduced dimension is included in the dimension type list using the dimension type name 'string'.

Thus, a scalar HARP variable of type string would be represented in HDF4 by a **2**-dimensional dataset of type
DFNT_CHAR. The length of the outer dimension would be 1, the length of the inner dimension would equal the length of the
string stored in the HARP variable. The 'dims' attribute associated with this HDF4 dataset would contain the string
'scalar,string'.

To summarize, HARP dimension types are mapped to dimension type names as follows:

=================== ===================
HARP dimension type dimension type name
=================== ===================
time                time
latitude            latitude
longitude           longitude
vertical            vertical
spectral            spectral
independent         independent
N/A                 scalar
N/A                 string
=================== ===================

HARP uses empty strings to represent the unit of dimensionless quantities (to distinguish them from non-quantities,
which will lack a unit attribute). However, HDF4 cannot store string attributes with length zero. For this reason
an empty unit string will be written as a ``units`` attribute with value ``"1"`` when writing data to HDF4.
When reading from HDF4 a unit string value ``"1"`` will be converted back again to an empty unit string.

Note that even though the ``time`` dimension is conceptually considered `appendable`, this dimension is not stored as an
actual appendable dimension in HDF4. Products are read/written from/to files in full and are only modified in memory.
The `appendable` aspect is only relevant for tools such as plotting routines that combine the data from a series of HARP
products in order to provide plots/statistics for a whole dataset (and thus, where data from different files will have
to be concatenated).
