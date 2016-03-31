"""
HARP Python interface

This module implements the HARP Python interface. The interface consists of a
minimal set of methods to import and export HARP products, and to ingest
non-HARP products. It does not expose the full functionality of the HARP C
library. In virtually all cases this is an advantage, not a disadvantage.

Imported or ingested products are returned as pure Python objects, that can be
manipulated freely from within Python. It is also possible to build up a product
from scratch directly in Python. All data is represented as NumPy ndarrays.

The HARP Python interface offers two alternative ways to represent a HARP
product in Python: 'hierarchical' or 'flattened'.

The hierarchical representation uses a Product instance as the Python
representation of a HARP product. The Product instance contains a Variable
instance for each HARP variable. A Variable instance contains both the data as
well as the attributes (dimension types, unit, ...) of the corresponding HARP
variable.

Variables can be accessed by name using either the attribute access '.' syntax,
or the item access '[]' syntax. For example:

    # Alternative ways to access the variable 'HCHO_column_number_density'.
    product.HCHO_column_number_density
    product["HCHO_column_number_density"]

    # Iterate over all variables in the product. For imported or ingested
    # products, the order of the variables is the same as the order in the
    # source product.
    for name in product:
        print product[name].unit

Product attributes can be accessed in the same way as variables, but are not
included when iterating over the variables in a product. For example:

    # Print product attributes.
    print product.source_product
    print product.history

The names of product attributes are reserved and cannot be used for variables.

The flattened representation uses an OrderedDict instance as the Python
representation of a HARP product. The OrderedDict instance maps the name of each
variable directly to the associated data.

The unit attribute of a variable is represented by mapping the name of the
corresponding variable suffxed with '_unit' to a string containing the unit.
Other variable attributes not represented.

    # Accessing the variable 'HCHO_column_number_density'.
    product["HCHO_column_number_density"]

    # Iterate over all variables in the product. For imported or ingested
    # products, the order of the variables is the same as the order in the
    # source product.
    for name, value in product.iteritems():
        print name, value

Product attributes are represented in the same way as variables. Note that when
iterating over the OrderedDict instance, the product attributes *will* be
included (in constrast to the hierarchical representation).

    # Print product attributes.
    print product["source_product"]
    print product["history"]

Only products in the hierarchical representation can be exported. The flattened
representation does not contain enough information.

"""

from _harppy import *
