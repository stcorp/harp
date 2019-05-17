harpdump
========

Inspect the contents of a file that can be imported by HARP.

::

  Usage:
      harpdump [options] <input product file>
          Print the contents of a HARP imported product.

          Options:
              -a, --operations <operation list>
                  List of operations to apply to the product before printing.
                  An operation list needs to be provided as a single expression.
                  See the 'operations' section of the HARP documentation for
                  more details.

              -o, --options <option list>
                  List of options to pass to the ingestion module.
                  Only applicable if the input product is not in HARP format.
                  Options are separated by semi-colons. Each option consists
                  of an <option name>=<value> pair. An option list needs to be
                  provided as a single expression.

              -l, --list
                  Only show list of variables (no attributes).

              -d, --data
                  Show data values for each variable.

              --no-history
                  Do not show the global history attribute.

      harpdump --dataset [options] <file|dir> [<file|dir> ...]
          Print metadata for all files in the dataset in csv format.

              -o, --options <option list>
                  List of options to pass to the ingestion module.
                  Only applicable if the input product is not in HARP format.
                  Options are separated by semi-colons. Each option consists
                  of an <option name>=<value> pair. An option list needs to be
                  provided as a single expression.

      harpdump --list-derivations [options] [input product file]
          List all available variable conversions. If an input product file is
          specified, limit the list to variable conversions that are possible
          given the specified product.

          Options:
              -a, --operations <operation list>
                  List of operations to apply to the product before determining
                  the possible derivations.
                  An operation list needs to be provided as a single expression.
                  See the 'operations' section of the HARP documentation for
                  more details.

              -o, --options <option list>
                  List of options to pass to the ingestion module.
                  Only applicable if the input product is not in HARP format.
                  Options are separated by semi-colons. Each option consists
                  of an <option name>=<value> pair. An option list needs to be
                  provided as a single expression.

               -t, --target <variable_name>
                  Only show derivations that produce the given variable.

      harpdump -h, --help
          Show help (this text).

      harpdump -v, --version
          Print the version number of HARP and exit.
