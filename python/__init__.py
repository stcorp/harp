"""HARP Python interface

This package implements the HARP Python interface. The interface consists of a
minimal set of methods to import and export HARP products, and to ingest
non-HARP products of a type supported by HARP.

Imported and ingested products are returned as pure Python objects, that can be
manipulated freely from within Python. It is also possible to build up a product
from scratch directly in Python. Data is represented as NumPy ndarrays.

HARP products are represented in Python by instances of class Product. A Product
contains an instance of class Variable for each variable contained in the HARP
product. A Variable contains both data and attributes (dimensions, unit, valid
range, description).

Variables can be accessed by name using either the attribute access '.' syntax,
or the item access '[]' syntax. For example:

    # Alternative ways to access the variable 'HCHO_column_number_density'.
    density = product.HCHO_column_number_density
    density = product["HCHO_column_number_density"]

    # Iterate over all variables in the product. For imported or ingested
    # products, the order of the variables is the same as the order in the
    # source product.
    for name in product:
        print(product[name].unit)

Product attributes can be accessed in the same way as variables, but are *not*
included when iterating over the variables in a product. For example:

    # Print product attributes.
    print(product.source_product)
    print(product.history)

The names of product attributes are reserved and cannot be used for variables.

A Product can be converted to an OrderedDict using the to_dict() package level
method. The OrderedDict representation provides direct access to the data
associated with each variable. All product attributes and all variable
attributes except the unit attribute are discarded as part of the conversion.
The unit attribute of a variable is represented by adding a scalar variable
of type string with the name of the corresponding variable suffixed with
'_unit' as name and the unit as value.

    # Convert input product to an OrderedDict.
    product = to_dict(input_product)

    # Accessing the variable 'HCHO_column_number_density'.
    product["HCHO_column_number_density"]

    # Accessing the unit attribute of the variable 'HCHO_column_number_density'.
    product["HCHO_column_number_density_unit"]

    # Iterate over all variables in the product. For imported or ingested
    # products, the order of the variables is the same as the order in the
    # source product.
    for name, value in product.items():
        print name, value

The OrderedDict representation can be convenient when there is a need to
interface with existing code such as plotting libraries, or when the additional
information provided by the Product representation is not needed.

Note that only Product instances can be exported. The OrderedDict representation
does not contain enough information.

Further information is available in the HARP documentation.

Copyright (C) 2015-2016 S[&]T, The Netherlands.

This file is part of HARP.

HARP is free software; you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation;
either version 2 of the License, or (at your option) any later version.

HARP is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
HARP; if not, write to the Free Software Foundation, Inc., 59 Temple Place,
Suite 330, Boston, MA  02111-1307  USA

"""

from harp._harppy import *
