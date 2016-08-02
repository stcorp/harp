harpprofile
==========

Apply various transformations to vertical profile data.

::

  Usage:

      harpprofile subcommand [options]
          Manipulate vertical profiles (resampling, filtering, etc.)

          Available subcommands:
            resample
            smooth

          Type 'harpprofile <subcommand> --help' for help on a specific subcommand.

      harpprofile -h, --help
          Show help (this text)

      harpprofile -v, --version
          Print the version number of the HARP Toolset and exit

Resampling
----------

::

  Usage:

      harpprofile resample -h, --help
          Show help for harpprofile resample (this text)

      harpprofile resample [options] <product file> [output product file]
          Regrid the vertical profiles in the file

          Options:
              -of, --output-format <format> :
                      Possible values for <format> (the output format) are:
                        netcdf (the default)
                        hdf4
                        hdf5

              One of the following:
              -a, --a-to-b <result_csv_file> <source_datasetdir_b> <vertical_axis>:
                      resample the vertical profiles of the input file (part of
                      dataset A) to the vertical grid of the vertical profiles
                      in dataset B
              -b, --b-to-a <result_csv_file> <source_datasetdir_a> <vertical_axis>:
                      resample the vertical profiles of the input file (part of
                      dataset B) to the <vertical_axis> grid of the vertical profiles
                      in dataset A
              -c, --common <input>
                      resample vertical profiles (in datasets A and B)
                      to a common grid before calculating the columns.
                      The common <vertical_axis> grid is defined in file C.
                      <input> denotes the filename

Smoothing
---------

::

  Usage:

      harpprofile smooth -h, --help
          Show help for harpprofile smooth (this text)

      harpprofile smooth [options] <varname> <vertical_axis> <product file> [output product file]
          Smooth the vertical profile <varname> in the <product file> with averaging kernel
          matrices and add a priori. Resampling is done beforehand against the specified vertical axis.

          Mandatory options:
              -o, --output <filename> :
                      write output to specified file
                      (by default the input file will be replaced)

              -of, --output-format <format> :
                      Possible values for <format> (the output format) are:
                        netcdf (the default)
                        hdf4
                        hdf5

              One of the following:
              -a, --a-with-b <result_csv_file> <source_datasetdir_b>:
                      resample and smooth the vertical profiles of the input file (part of
                      dataset A) with the <vertical_axis>, averaging kernel matrices and a priori
                      in dataset B
              -b, --b-with-a <result_csv_file> <source_datasetdir_a>:
                      resample and smooth the vertical profiles of the input file (part of
                      dataset B) with the <vertical_axis>, averaging kernel matrices and a priori
                      in dataset A

