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

 - In HARP the vertical dimension in a variable always comes after the latitude and longitude dimensions, whereas in
   netCDF-CF the recommendation (not a requirement) is to have the vertical dimension before latitude and longitude.
 - In netCDF-CF a specific type of dimension can be used only once as dimension of a variable.
   This is not the case in HARP where a specific dimension may occur more than once.
   For instance, HARP allows you to use a ``{vertical,vertical}`` dimension for a vertical correlation matrix,
   whereas in netCDF-CF the second vertical dimension needs to be a differently named dimension.
 - Axis variables in HARP can generally be considered 'auxiliary coordinate variables' (using netCDF-CF terminology)
   but rarely strict 'coordinate variables'. Note that HARP does not require a pre-identification of axis variables;
   none of the dimensions have a fixed axis variable associated with them (or require presence of an axis variable);
   different variables in a product can function as axis variable for a dimension dynamically, depending on the
   operation to be performed.
 - Due to the way axis variables and dimensions are defined in HARP, HARP products do not meet the netCDF-CF
   requirement that any used dimension (with length greater than one) that is latitude, longitude, vertical, or time
   needs to have a 'coordinate variable' (using same name and single dimension).
 - HARP axis variables can be 2-dimensional (e.g. ``{time,axis}``) to allow for e.g. a different altitude grid per
   time sample (where the actual ``axis`` length can also differ per time and will then contain fill values at the
   end to represent shorter lengths). NetCDF-CF supports this as 'incomplete multidimensional array representation'
   of 'auxiliary coordinate variables'. However, other multi-dimensional 'auxiliary coordinate variable' cases in
   netCDF-CF generally do not have a HARP axis variable equivalent. For instance, for the 2-dimensional lat/lon
   coordinate variables case (using ``lon(j,i)`` and ``lat(j,i)``) the coordinate variables would not be considered
   axis variables by HARP.
