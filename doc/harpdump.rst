harpdump
========

Inspect the contents of a HARP compliant netCDF/HDF4/HDF5 file.

::

  Usage:
      harpdump [options] <input product file>
          Print the contents of a HARP compliant netCDF/HDF4/HDF5 product.

          Options:
              -a, --operations <operation list>
                  List of operations to apply to the product before printing.
                  An operation list needs to be provided as a single expression.
                  See the 'operations' section of the HARP documentation for
                  more details.
              -l, --list:
                  Only show list of variables (no attributes).
              -d, --data:
                  Show data values for each variable.

      harpdump -h, --help
          Show help (this text).

      harpdump -v, --version
          Print the version number of HARP and exit.
