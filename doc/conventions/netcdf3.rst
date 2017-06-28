netCDF-3
========
This section details a set of additional conventions that are specific to the netCDF-3 file format.

The following table shows the mapping between HARP data types and netCDF-3 data types. NetCDF-3 data types not covered
in this table are not supported by HARP.

============== ==================
HARP data type netCDF-3 data type
============== ==================
int8           NC_BYTE
int16          NC_SHORT
int32          NC_INT
float          NC_FLOAT
double         NC_DOUBLE
string         NC_CHAR
============== ==================

The netCDF-3 data model defines the concept of shared dimensions. A netCDF-3 dimension has a name and a length. The name
of a dimension should be unique. The shape of a netCDF-3 variable is specified as a list of dimensions (instead of a
list of dimension *lengths*). This implies that to store a HARP product in netCDF-3 format, a netCDF-3 dimension should
be defined for each dimension present in the product. This is straight-forward for all dimension types supported by
HARP, except for independent dimensions (since HARP does not require that all independent dimensions have the same
length).

For example, a variable may have an independent dimension of length 2, while another variable may have an independent
dimension of length 4. However, it is not possible to create two netCDF-3 dimensions called 'independent' with different
lengths, because netCDF-3 requires dimension names to be unique. Instead, the netCDF-3 backend defines a netCDF-3
dimension for each independent dimension by appending the length to the name of the dimension to make it unique. The
name and the length are separated by an underscore. In the case of the example, two netCDF-3 dimensions would be
defined, one named 'independent_2' and another named 'independent_4'. Independent dimensions of the same length will be
represented by the same netCDF-3 dimension.

NetCDF-3 does not support strings. The netCDF-3 backend stores an N-dimensional HARP variable of type string as an
(N+1)-dimensional netCDF-3 variable of type NC_CHAR. The length of the introduced dimension equals the length of the
longest string, or 1 if the length of the longest string is zero. Shorter strings are padded will null-termination
characters.

Of course, netCDF-3 dimensions need to be defined for the introduced dimensions. This is handled in the same way as for
independent dimensions, by appending the length to the name of the dimension. The name and the length are separated by
an underscore. The name used for these introduced dimensions is 'string'. For example, if the longest string has a
length of 10, a netCDF-3 dimension named 'string_10' would be defined. String dimensions of the same length will be
represented by the same netCDF-3 dimension.

To summarize, HARP dimensions are mapped to netCDF-3 dimensions as follows:

=================== =======================
HARP dimension type netCDF-3 dimension name
=================== =======================
time                time
latitude            latitude
longitude           longitude
vertical            vertical
spectral            spectral
independent         independent\_<length>
N/A                 string\_<length>
=================== =======================

Note that even though the ``time`` dimension is conceptually considered `appendable`, this dimension is not stored as an
actual appendable dimension in netCDF-3. Products are read/written from/to files in full and are only modified in memory.
The `appendable` aspect is only relevant for tools such as plotting routines that combine the data from a series of HARP
products in order to provide plots/statistics for a whole dataset (and thus, where data from different files will have
to be concatenated). Storing data in a netCDF-3 file using an actual appendable dimension (using the netCDF-3 definition
of `appendable dimension`) will have a slightly lower read/write performance compared to having all dimensions fixed.
