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
   netCDF-CF the recommendation (not a requirement) is to have the vertical dimension before latitude and longitude.
 - In netCDF-CF a specific type of dimension can be used only once as dimension of a variable.
   This is not the case in HARP where a specific dimension may occur more than once.
   For instance, HARP allows you to use a ``{vertical,vertical}`` dimension for a vertical correlation matrix,
   whereas in netCDF-CF the second vertical dimension needs to be a differently named dimension.
 - Axis variables in HARP can have different names from the dimension and are therefore generally considered
   'auxiliary coordinate variables' in netCDF-CF terminology.
 - In addition, HARP axis variables can be 2-dimensional (e.g. ``{time,axis}``). This allows for e.g. a different
   altitude grid per time sample (where the actual ``axis`` length can also differ per time and will then contain
   fill values at the end to represent shorter lengths). It is unclear whether netCDF-CF supports such a
   'variable length' multidimensional array representation of axis variables.
   Note that this is a different case from the 2-dimensional lat/lon coordinate variables that netCDF-CF covers (e.g.
   ``lon(j,i)`` and ``lat(j,i)``); what netCDF-CF considers coordinate variables in this case are not considered
   axis variables by HARP.
