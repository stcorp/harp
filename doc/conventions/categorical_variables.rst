Categorical Variables
=====================

HARP has special support for categorical variables, which are variables that can be represented by an enumeration type.

Categorical variables are represented in HARP using integer data types that provide an index into a fixed list of
enumeration labels. These enumeration labels are stored as metadata together with a variables. For ``int8`` data the
labels are stored as a ``flag_meanings`` attribute (together with a ``flag_values`` attribute) in netCDF3, HDF4, and
HDF5 files.

HARP does not impose a rule on which integer value maps to a specific enumeration label. The integer values are
just plain indices into the enumeration label list, and the list can be sorted in any way.

Any integer value in a categorical variable that is outside the range of the available enumeration labels
(``value < 0`` or ``value >= N``) will be treated as an invalid value (``valid_min`` and ``valid_max`` should therefore
always be set to ``0`` and ``N-1`` respectively for these variables).


Operations in HARP on categorical variables will need to primarily use the enumeration label `names`
(and not the `integer values`). Any standardization (such as for the variable names) on the categories for
categorical variables in HARP will thus also only be on enumeration names and not on numbers.

In HARP we distinguish category types and category values. The name of the category type ``<cattype>`` is used in the
variable name and the names of the category values ``<catval>`` are the enumeration labels. Variables for category
types should generally be called ``<cattype>_type``. For instance, if we want to classify clouds we would have
``<cattype>=cloud`` and a variable name ``cloud_type``, and for land use classification we would have ``land_type``.

The enumeration labels for a variable should ideally be fully distinct categories. This means that if you have land
types ``A``, ``B``, and ``C``, you should try to avoid also having land types for ``A_and_B``, ``B_and_C``,
``A_and_B_and_C``, etc.
A ``<cattype>_type`` variable should ideally indicate just a single dominant applicable category.
To deal with mixed presence of multiple categories one can use variables such as ``<catval>_flag`` and
``<catval>_fraction``.

For instance, for a ``land_type`` variable we could have ``forest`` as enumeration label, but then also have variables
``forest_flag`` and ``forest_fraction``.

A ``<catval>_flag`` variable in HARP should always be a binary/dichotomous variable (it should contain true/false, 1/0
values). A flag variable denotes whether something is present/applicable/true or not. A flag variable will therefore
generally always have an ``int8`` data type. Important to note is that ``<*>_flag`` variables in HARP should not be used
as bitfield variables (unlike the ``<*>_validity`` variables). 

A ``<catval>_fraction`` variable can be used to define the fraction of the area that has the given ``<catval>``
classification. The fraction is a floating point value between 0 and 1.

Note that many other data product formats use bitfields to indicate applicability of multiple classification categories
in order to save storage space. However, the aim of HARP is not to provide a compact data format, but to have data
variables that can be readily used by algorithms. For this reason, combining multiple information elements in a single 
variable is often avoided within the HARP conventions.
