harpcollocate
=============

The tool harpcollocate is used to derive a list of measurements that match in time, latitude and longitude for two sets
of HARP compliant HDF4/HDF5/netCDF files.

::

  Usage:
    harpcollocate sub-command [options]
      Determine the collocation filter for two sets of HARP files.

      Available sub-commands:
        matchup
        resample
        update

      Use 'harpcollocate <sub-command> --help' to get help on a specific
      sub-command.

    harpcollocate -h, --help
      Show help (this text).

    harpcollocate -v, --version
      Print the version number of HARP and exit.

Collocation
-----------

Collocation of two sets of files involves the following steps:
  1. ``harpcollocate matchup`` is used to derive the collocation result file.
  2. ``harpcollocate resample`` is optionally used to further process the collocation result file.
  3. ``harpconvert -fca ...`` is used together with the collocation result file to select the matching measurements in each file of the primary dataset
  4. ``harpconvert -fcb ...`` is used together with the collocation result file to select the matching measurements in each file of the secondary dataset

When needed, harpcollocate update can be used to refresh the collocation result file.

Obtaining collocation result file
---------------------------------

The tool harpcollocate matchup is used to derive a list of measurement pairs that match in time, latitude and longitude.

::

  Usage:
    harpcollocate matchup [options]
      Determine the collocation filter for two sets of HARP files,
      and optionally resample the collocation result

      Options:

        -h, --help
             Show matchup help (this text)
        -ia, --input-a <input>
             Specifies directory or names of input files of dataset A
        -ib, --input-b <input>
             Specifies directory or names of input files of dataset B
        -or, --output-result <output>
             Specifies collocation result file (comma separated values)

        Collocation options,
        set at least one of the following ([unit] is optional):
        -dt 'value [unit]'          : sets maximum allowed difference in time
        -dp 'value [unit]'          : sets maximum allowed point distance
        -dlat 'value [unit]'        : sets maximum allowed point difference
                                      in latitude
        -dlon 'value [unit]'        : sets maximum allowed point difference
                                      in longitude
        -da 'value [unit]'          : sets minimum allowed overlapping
                                      percentage of polygon areas
        -dsza 'value [unit]'        : sets allowed maximum difference
                                      in solar zenith angle
        -dsaa 'value [unit]'        : sets allowed maximum difference
                                      in solar azimuth angle
        -dvza 'value [unit]'        : sets allowed maximum difference
                                      in viewing zenith angle
        -dvaa 'value [unit]'        : sets allowed maximum difference
                                      in viewing azimuth angle
        -dtheta 'value [unit]'      : sets allowed maximum difference
                                      in scattering angle
        -overlap                    : sets that polygon areas must overlap
        -painab                     : sets that points of dataset A must fall
                                      in polygon areas of B
        -pbinaa                     : sets that points of dataset B must fall
                                      in polygon areas of A
        When '[unit]' is not specified, a default unit is used:
          Criteria; [default unit]
          -dt; [s]
          -dp; [m]
          -dlat; [degree_north]
          -dlon; [degree_east]
          -da; [%]
          -dsza, -dsaa, -dvza, -dvaa, -dvaa, -dtheta; [degree]

        Resampling options:
        -Rnna, --nearest-neighbour-a: keep only nearest neighbour,
                                      dataset A is the master dataset
        -Rnnb, --nearest-neighbour-b: keep only nearest neighbour,
                                      dataset B is the master dataset
        The nearest neighbour is the sample with which the squared sum
        of the weighted differences is minimal
        When resampling is set to 'Rnna' and/or 'Rnnb',
        the following parameters can be set:
        -wft 'value [unit]'         : sets the weighting factor for time
        -wfdp 'value [unit]'        : sets the weighting factor for
                                      point distance
        -wfa 'value [unit]'         : sets the weighting factor for
                                      overlapping percentage
        -wfsza 'value [unit]'       : sets the weighting factor
                                      for solar zenith angle
        -wfsaa 'value [unit]'       : sets the weighting factor
                                      for solar azimuth angle
        -wfvza 'value [unit]'       : sets the weighting factor
                                      for viewing zenith angle
        -wfvaa 'value [unit]'       : sets the weighting factor
                                      for viewing azimuth angle
        -wftheta 'value [unit]'     : sets the weighting factor
                                      for scattering angle
        When '[unit]' is not specified in the above, a default unit will be
        adopted:
          Weighting factors; [default unit]
          -wft; [1/s]
          -wfdp; [1/m]
          -wfa; [1/%]
          -wfsza, -wfsaa, -wfvza, -wfvaa, -wfvaa, -wftheta; [1/degree]
        When a weighting factor is not set, a default value of 1 and
        the default unit are adopted. Recommend value and unit for the
        weighting factors are the reciprocals of the corresponding
        collocation criteria value and unit that is used.

Resampling collocation result file
----------------------------------

The command ``harpcollocate resample`` is used to apply resampling on the collocation result file. For example, to limit
a series of matches to only the nearest neighbour.

::

  Usage:
    harpcollocate resample [options]
      Resample an existing collocation result file

      Options:

        -h, --help
             Show resample help (this text)
        -ir, --input-result <input>
             Input collocation result file (comma separated values)
        -or, --output-result <output>
             Create a new file, and do not overwrite the input
             collocation result file

        Resampling options:
        -Rnna, --nearest-neighbour-a: keep only nearest neighbour,
                                      dataset A is the master dataset
        -Rnnb, --nearest-neighbour-b: keep only nearest neighbour,
                                      dataset B is the master dataset
        The nearest neighbour is the sample with which the squared sum
        of the weighted differences is minimal
        When resampling is set to 'Rnna' and/or 'Rnnb',
        the following parameters can be set:
        -wft 'value [unit]'         : sets the weighting factor for time
        -wfdp 'value [unit]'        : sets the weighting factor for
                                      point distance
        -wfa 'value [unit]'         : sets the weighting factor for
                                      overlapping percentage
        -wfsza 'value [unit]'       : sets the weighting factor
                                      for solar zenith angle
        -wfsaa 'value [unit]'       : sets the weighting factor
                                      for solar azimuth angle
        -wfvza 'value [unit]'       : sets the weighting factor
                                      for viewing zenith angle
        -wfvaa 'value [unit]'       : sets the weighting factor
                                      for viewing azimuth angle
        -wftheta 'value [unit]'     : sets the weighting factor
                                      for scattering angle
        When '[unit]' is not specified in the above, a default unit will be
        adopted:
          Weighting factors; [default unit]
          -wft; [1/s]
          -wfdp; [1/m]
          -wfa; [1/%]
          -wfsza, -wfsaa, -wfvza, -wfvaa, -wfvaa, -wftheta; [1/degree]
        When a weighting factor is not set, a default value of 1 and
        the default unit are adopted. Recommend value and unit for the
        weighting factors are the reciprocals of the corresponding
        collocation criteria value and unit that is used.

Updating collocation result file
--------------------------------

The command ``harpcollocate update`` is used to:
  1. Determine which files in the collocation result file are still in existance
  2. Update the collocation result file accordingly

::

  Usage:
    harpcollocate update [options]
      Update an existing collocation result file by checking
      the measurements in two sets of HARP files that still exist

      Options:
        -ia, --input-a <input>
             Specifies directory or names of input files of dataset A
        -ib, --input-b <input>
             Specifies directory or names of input files of dataset B
        -ir, --input-result <input>
             Input collocation result file (comma separated values)
        -or, --output-result <output>
             Create a new file, and do not overwrite the input
             collocation result file
