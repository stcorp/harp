Collocation result file
=======================

The collocation result file is a simple comma separated (csv) file that provides the list of matching pairs of
measurements from two datasets (dataset A and B). Both datasets can consist of multiple files.
Each measurement from dataset A can match with multiple measurements of dataset B and vice-versa.
This means that the combination of `filename_a` and `measurement_id_a` can occur multiple times in a collocation result
file and the same holds for a combination of `filename_b` and `measurement_id_b`.
Each pair is uniquely identified by a collocation index (`collocation_id`).


A collocation result file contains the following columns:

collocation_id
  Unique id of the collocation pair. This id will correspond with the ``collocation_index`` variable inside HARP
  products after the products are filtered using a collocation result file.

filename_a
  The filename of the original input file (i.e. ``source_product`` global attribute value) from the primary dataset.

measurement_id_a
  A unique index number of the measurement within the file. This index number is based on the list of measurements from
  the original input file and corresponds to the ``index`` variable inside HARP products.

filename_b
  The filename of the original input file (i.e. ``source_product`` global attribute value) from the secondary dataset.

measurement_id_b
  A unique index number of the measurement within the file. This index number is based on the list of measurements from
  the original input file and corresponds to the ``index`` variable inside HARP products.

collocation criteria...
  The remaining columns cover the collocation criteria that were provided to harpcollocate. For each collocation
  criterium the column will provide the exact distance value for the given collocated measurement pair for that
  criterium. The column label used for each criteria is the HARP variable name of the associate difference variable
  together with the unit (e.g. `datetime_diff [s]`)
