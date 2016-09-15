harpfilter
==========

Apply various types of filtering and conversions to a HARP compliant
netCDF/HDF4/HDF5 file.

::

  Usage:
      harpfilter [options] <input product file> [output product file]
          Filter a HARP compliant netCDF/HDF4/HDF5 product.

          Options:
              -a, --operations <operation list>
                  List of operations to apply to the product.
                  An operation list needs to be provided as a single expression.
                  See the 'operations' section of the HARP documentation for
                  more details.

              -f, --format <format>
                  Output format:
                      netcdf (default)
                      hdf4
                      hdf5

          If the filtered product is empty, a warning will be printed and the
          tool will return with exit code 2 (without writing a file).

      harpfilter --list-conversions [input product file]
          List all available variable conversions. If an input product file is
          specified, limit the list to variable conversions that are possible
          given the specified product.

      harpfilter -h, --help
          Show help (this text).

      harpfilter -v, --version
          Print the version number of HARP and exit.

