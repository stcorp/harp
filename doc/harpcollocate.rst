harpcollocate
=============

Find pairs of measurements that match in time and geolocation for two sets of
HARP compliant netCDF files.

::

  Usage:
      harpcollocate [options] <path_a> <path_b> <outputpath>
          Find matching sample pairs between two datasets of HARP files.
          The path for a dataset can be either a single file or a directory
          containing files. The result will be write as a comma separate value
          (csv) file to the provided output path

          Options:
              -d '<diffvariable> <value> [unit]'
                  Specifies a collocation criterium.
                  Only include pairs where the absolute difference between the
                  values of the given variable for dataset A and B are
                  less/equal than the given value.
                  There is a special variable name 'point_distance' to indicate
                  the earth surface distance between lat/lon points of A and B.
                  Examples:
                      -d 'datetime 3 [h]'
                      -d 'point_distance 10 [km]'
              --area-intersects
                  Specifies that latitude/longitude polygon areas of A and B
                  must overlap
              --point-in-area-xy
                  Specifies that latitude/longitude points from dataset A must
                  fall in polygon areas of dataset B
              --point-in-area-yx
                  Specifies that latitude/longitude points from dataset B must
                  fall in polygon areas of dataset A
              -nx <diffvariable>
                  Filter collocation pairs such that for each sample from
                  dataset A only the nearest sample from dataset B (using the
                  given variable as difference) is kept
              -ny <diffvariable>
                  Filter collocation pairs such that for each sample from
                  dataset B only the neareset sample from dataset A is kept.
          The order in which -nx and -ny are provided determines the order in
          which the nearest filters are executed.
          When '[unit]' is not specified, the unit of the variable of the
          first file from dataset A will be used.

      harpcollocate --resample [options] <inputpath> [<outputpath>]
          Filter an existing collocation result file by selecting only nearest
          samples.

          Options:
              -nx <diffvariable>
                  Filter collocation pairs such that for each sample from
                  dataset A only the nearest sample from dataset B (using the
                  given variable as difference) is kept
              -ny <diffvariable>
                  Filter collocation pairs such that for each sample from
                  dataset B only the neareset sample from dataset A is kept.
          The order in which -nx and -ny are provided determines the order in
          which the nearest filters are executed.

      harpcollocate --update <inputpath> <path_a> <path_b> [<outputpath>]
          Update an existing collocation result file by checking the
          measurements in two sets of HARP files and only keeping pairs
          for which measurements still exist

      harpcollocate -h, --help
          Show help (this text).

      harpcollocate -v, --version
          Print the version number of HARP and exit.
