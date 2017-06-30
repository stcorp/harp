Compatibility with other standards
==================================

Although the HARP data format aims to be compatible with (or orthogonal to) other data format standards, this may not
always be fully possible. Below we detail some of the known differences with existing standards.

netCDF-CF
---------
The HARP data format standard is to a large extent compatible with the netCDF-CF standard. This means that HARP
attributes either use the same definition as used for netCDF-CF or the attributes are not governed by netCDF-CF.
And vice versa, HARP allows (and ignores) attributes that are not HARP specific.

For the naming convention of variables, in HARP these apply to the actual variable names.
This is complementary to the naming convention for variables used in netCDF-CF which is performed by means of the
``standard_name`` variable attribute.

There are however a few differences between the conventions used in HARP and netCDF-CF. These are:

 - In HARP the vertical dimension in a variables always comes after the latitude and longitude dimensions, whereas in
   netCDF-CF the vertical dimension always comes before latitude and longitude.
 - In netCDF-CF a specific type of dimension can be used only once as dimension of a variable.
   This is not the case in HARP where a specific dimension may occur more than once.
   For instance, HARP allows you to use a ``{vertical,vertical}`` dimension for a vertical correlation matrix,
   whereas in netCDF-CF the second vertical dimension needs to be a differently named dimension.
 - In HARP coordinate variables can be 2-dimensional (e.g. ``{time,axis}``). This allows for e.g. a different altitude
   grid per time sample. It is unclear whether netCDF-CF allows such an incomplete multidimensional array representation
   for coordinate variables.
