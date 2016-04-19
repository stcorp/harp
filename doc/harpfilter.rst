harpfilter
==========

Apply various types of filtering and conversions to a HARP compliant
netCDF/HDF4/HDF5 file.

::

  Usage:
      harpfilter [options] <input product file> [output product file]
          Filter a HARP compliant netCDF/HDF4/HDF5 product.

          Options:
              -a, --actions <action list>
                  List of actions to apply to the product.
                  An action list needs to be provided as a single expression.

              -f, --format <format>
                  Output format:
                      netcdf (default)
                      hdf4
                      hdf5

          Action list:
              Actions are separated by semi-colons. Each action is either
              a comparison filter, a membership test filter, or a function
              call. Strings used in actions should be quoted with double
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
                      harpfilter can be used to list available variable
                      conversions.

                  include(variable, ...)
                      Mark the specified variable(s) for inclusion in the
                      filtered product. All variables marked for inclusion
                      will be included in the filtered product, all other
                      variables will be excluded. By default, all variables
                      will be included.

                  exclude(variable, ...)
                      Mark the specified variable(s) for exclusion from the
                      filtered product. All variables marked for exclusion
                      will be excluded from the filtered product, all other
                      variables will be included. Variable exclusions will be
                      evaluated after evaluating all variable inclusions (if
                      any).

                  The unit qualifier is optional for all function arguments
                  that support it. If a unit is not specified, the unit of the
                  corresponding variable will be used.

              Examples:
                  -a 'derive(altitude {time} [km]); pressure > 3.0 [bar];'
                  -a 'point-distance(-52.5 [degree], 1.0 [rad], 1e3 [km])'
                  -a 'index in (0, 10, 20, 30, 40); valid(pressure)'

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

