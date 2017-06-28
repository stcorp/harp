File naming
===========

HARP puts no specific restrictions on filenames.

There is however one constraint and that is that there should be a unique identifier to reference a product within
a dataset. HARP will first use the value of the ``source_product`` global attribute for this if that attribute is
present. And if the attribute is not present it will use the actual filename of the product as identifier.
