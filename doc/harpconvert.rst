harpconvert
===========

Convert a product from its original format to a HARP compliant netCDF/HDF4/HDF5
file that can be processed further with other HARP command line tools. See the
section :doc:`Ingestion definitions <ingestions/index>` for an overview of the
product types supported by HARP.

::

  Usage:
      harpconvert [options] <input product file> <output product file>
          Import a product that is stored in HARP format or in one of the
          supported external formats, perform operations on it (if provided),
          and save the results to a HARP netCDF/HDF4/HDF5 product.

          Options:
              -a, --operations <operation list>
                  List of operations to apply to the product.
                  An operation list needs to be provided as a single expression.
                  See the 'operations' section of the HARP documentation for
                  more details.

              -o, --options <option list>
                  List of options to pass to the ingestion module.
                  Only applicable if the input product is not in HARP format.
                  Options are separated by semi-colons. Each option consists
                  of an <option name>=<value> pair. An option list needs to be
                  provided as a single expression.

              -f, --format <format>
                  Output format:
                      netcdf (default)
                      hdf4
                      hdf5

              --hdf5-compression <level>
                  Set data compression level for storing in HDF5 format.
                  0=disabled, 1=low, ..., 9=high.

              --no-history
                  Do not update the global history attribute.

          If the ingested product is empty, a warning will be printed and the
          tool will return with exit code 2 (without writing a file).

      harpconvert --generate-documentation [options] [output directory]
          Generate a series of documentation files in the specified output
          directory. The documentation describes the set of supported foreign
          product types and the details of the HARP product(s) that are
          produced by an ingestion.

      harpconvert -h, --help
          Show help (this text).

      harpconvert -v, --version
          Print the version number of HARP and exit.
