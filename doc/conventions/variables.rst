Variables
=========
A HARP variable is a named multi-dimensional array with associated attributes
(see section :doc:`Variable attributes <variable_attributes>`).
The base type of a variable can be any of the data types supported by HARP (see section :doc:`Data types <datatypes>`).
A variable can have zero or more dimensions. A variable with zero dimensions is a *scalar*.
The maximum number of dimensions is 8. Each dimension of a variable has a type that refers to one of the dimension types
supported by HARP (see section :doc:`Dimensions <dimensions>`).
Dimensions of the same type should have the same length, *except* independent dimensions.

Variables have a name that should follow the :doc:`Variable naming convention <variable_names>`.
