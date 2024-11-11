harpcollocate
=============

Find pairs of measurements that match in time and geolocation for two sets of
HARP compliant netCDF files.

::

  Usage:
      harpcollocate [options] <path-a> <path-b> <outputpath>
          Find matching sample pairs between two datasets of HARP files.
          The path for a dataset can be either a single file or a directory
          containing files. The results will be written as a comma separated
          value (csv) file to the provided output path.
          If a directory is specified then all files (recursively) from that
          directory are used for a dataset.
          If a file is a .pth file then the file paths from that text file
          (one per line) are used. These file paths can be absolute or
          relative and can point to files, directories, or other .pth files.
          Note that the 'source_product' attribute of products in a .pth file
          needs to be unique; if a .pth file references multiple products with
          the same 'source_product' value then only the last product in the
          list will be kept.

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
                  Criteria on azimuth angles, longitude, and wind direction
                  will be automatically mapped to [0..180] degrees.
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
              -oa, --options-a <option list>
                  List of options to pass to the ingestion module for ingesting
                  products from the first dataset.
                  Only applicable if the input product is not in HARP format.
                  Options are separated by semi-colons. Each option consists
                  of an <option name>=<value> pair. An option list needs to be
                  provided as a single expression.
              -ob, --options-b <option list>
                  List of options to pass to the ingestion module for ingesting
                  products from the second dataset (see above).
              -aa, --operations-a <operation list>
                  List of operations to apply to each product of the first
                  dataset before collocating.
                  An operation list needs to be provided as a single expression.
                  See the 'operations' section of the HARP documentation for
                  more details.
              -ab, --operations-b <operation list>
                  List of operations to apply to each product of the second
                  dataset before collocating (see above).
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

      harpcollocate --update <inputpath> <datasetpath> [<outputpath>]
          Update an existing collocation result file by checking the
          measurements in the given dataset and only keeping pairs
          for which measurements still exist

      harpcollocate -h, --help
          Show help (this text).

      harpcollocate -v, --version
          Print the version number of HARP and exit.
