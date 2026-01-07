# Copyright (C) 2015-2026 S[&]T, The Netherlands.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
# this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution.
#
# 3. Neither the name of the copyright holder nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

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

"""

from harp._harppy import *
