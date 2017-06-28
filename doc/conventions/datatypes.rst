Data types
==========

HARP defines the following data types. See sections :doc:`netCDF-3 <netcdf3>`, :doc:`HDF4 <hdf4>`, and :doc:`HDF5 <hdf5>`
for a detailed description of how these data types are mapped to and from the data types supported by each file format.

============== =========== ==== ====== ========================= ==============================================================================
HARP data type C data type bits sign   range                     description
============== =========== ==== ====== ========================= ==============================================================================
int8           int8_t      8    signed [-128, 127]               signed 8-bit integer type (two's complement representation)
int16          int16_t     16   signed [-32768, 32767]           signed 16-bit integer type (two's complement representation)
int32          int32_t     32   signed [-2147483648, 2147483647] signed 32-bit integer type (two's complement representation)
float          float       32   signed [-3.4E38, 3.4E38]         IEEE754 single-precision binary floating-point format (on supported platforms)
double         double      64   signed [-1.7E308, 1.7E308]       IEEE754 double-precision binary floating-point format (on supported platforms)
string         char*                                             null-terminated ASCII string
============== =========== ==== ====== ========================= ==============================================================================
