harpconvert
===========

Convert a product from its original format to a HARP compliant netCDF/HDF4/HDF5
file that can be processed further with other HARP command line tools. See the
section :doc:`Ingestion definitions <ingestions/index>` for an overview of the
product types supported by HARP.

::

  Usage:
      harpconvert [options] <input product file> <output product file>
          Convert the input product to a HARP netCDF/HDF4/HDF5 product.

          Options:
              -a, --operations <operation list>
                  List of operations to apply to the product.
                  An operation list needs to be provided as a single expression.

              -o, --options <option list>
                  List of options to pass to the ingestion module.
                  Options are separated by semi-colons. Each option consists
                  of an <option name>=<value> pair. An option list needs to be
                  provided as a single expression.

              -f, --format <format>
                  Output format:
                      netcdf (default)
                      hdf4
                      hdf5

          Action list:
              Actions are separated by semi-colons. Each operation is either
              a comparison filter, a membership test filter, or a function
              call. Strings used in operations should be quoted with double
              quotes.

              Comparison filter:
                  variable operator value [unit]
                      Exclude measurements for which the comparison evaluates
                      to false.

                  Supported operators:
                      == !=
                      < <= >= > (for numerical variables only)

                  If a unit is specified, the comparison will be performed in
                  the specified unit. Otherwise, it will be performed in the
                  unit of the variable. Units can only be specified for
                  numerical variables.

              Membership test filter:
                  variable in (value, ...) [unit]
                  variable not in (value, ...) [unit]
                      Exclude measurements that do not occur in the specified
                      list.

                  If a unit is specified, the comparison will be performed in
                  the specified unit. Otherwise, it will be performed in the
                  unit of the variable. Units can only be specified for
                  numerical variables.

              Function call:
                  function(argument, ...)

              Supported functions:
                  collocate-left(collocation-result-file)
                      Apply the specified collocation result file as an index
                      filter assuming the product is part of dataset A.

                  collocate-right(collocation-result-file)
                      Apply the specified collocation result file as an index
                      filter assuming the product is part of dataset B.

                  valid(variable)
                      Exclude invalid values of the specified variable (values
                      outside the valid range of the variable, or NaN).

                  longitude-range(minimum [unit], maximum [unit])
                      Exclude measurements of which the longitude of the
                      measurement location falls outside the specified range.
                      This function correctly handles longitude ranges that
                      cross the international date line.

                  point-distance(longitude [unit], latitude [unit],
                                 distance [unit])
                      Exclude measurements situated further than the specified
                      distance from the specified location.

                  area-mask-covers-point(area-mask-file)
                      Exclude measurements for which no area from the area
                      mask file contains the measurement location.

                  area-mask-covers-area(area-mask-file)
                      Exclude measurements for which no area from the area
                      mask file covers the measurement area completely.

                  area-mask-intersects-area(area-mask-file,
                                            minimum-overlap-percentage)
                      Exclude measurements for which no area from the area
                      mask file overlaps at least the specified percentage of
                      the measurement area.

                  derive(variable {dimension-type, ...} [unit])
                      Derive the specified variable from other variables found
                      in the product. The --list-conversions option of
                      harpconvert can be used to list available variable
                      conversions.

                  keep(variable, ...)
                      Mark the specified variable(s) for inclusion in the
                      ingested product. All variables marked for inclusion
                      will be kept in the ingested product, all other
                      variables will be excluded.

                  exclude(variable, ...)
                      Mark the specified variable(s) for exclusion from the
                      ingested product. All variables marked for exclusion
                      will be excluded from the ingested product, all other
                      variables will be kept.

                  The unit qualifier is optional for all function arguments
                  that support it. If a unit is not specified, the unit of the
                  corresponding variable will be used.

              Examples:
                  -a 'derive(altitude {time} [km]); pressure > 3.0 [bar];'
                  -a 'point-distance(-52.5 [degree], 1.0 [rad], 1e3 [km])'
                  -a 'index in (0, 10, 20, 30, 40); valid(pressure)'

          If the ingested product is empty, a warning will be printed and the
          tool will return with exit code 2 (without writing a file).

      harpconvert --test <input product file> [input product file...]
          Perform an internal test for each product by ingesting the product
          using all possible combinations of ingestion options.

      harpconvert --list-conversions [options] [input product file]
          List all available variable conversions. If an input product file is
          specified, limit the list to variable conversions that are possible
          given the specified product.

          Options:
              -o, --options <option list>
                  List of options to pass to the ingestion module.
                  Options are separated by semi-colons. Each option consists
                  of an <option name>=<value> pair. An option list needs to be
                  provided as a single expression.

      harpconvert --generate-documentation [options] [output directory]
          Generate a series of documentation files in the specified output
          directory. The documentation describes the set of supported product
          types and the details of the HARP product(s) that can be produced
          from them.

          Options:
              -f, --format <format>
                  Output format:
                      html (default)
                      rst

      harpconvert -h, --help
          Show help (this text).

      harpconvert -v, --version
          Print the version number of HARP and exit.
