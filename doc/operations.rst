Operations
==========

HARP provides a simple expression syntax with which one can specify operations that need to be performed on a product.
A list of operations is always provided as a single string where individual operations are separated by a semi-colon (`;`).
Each operation can be either a comparison filter, a membership test filter, or a function call.
Strings used in operations should be quoted with double quotes.

Comparison filter
-----------------

    ``variable operator value [unit]``

Filter a dimension for all variables in the product such that items for which
the value of the provided variable does not match the expression get excluded.
The variable should be one dimensional or two dimensional (with the first
dimension being time). The dimension that gets filtered is the last dimension
of the referenced variable.

Supported operators are:

    | ``==`` ``!=``
    | ``<`` ``<=`` ``>=`` ``>`` (for numerical values only)
    | ``=&`` ``!&`` (bitfield operators, for integer values only)


Bitfield operators work such that ``a =& 5`` returns true if both bits 1 and 3 in ``a`` are set
and ``a !& 5`` returns true if neither bits 1 and 3 in ``a`` are set.

If a unit is specified, the comparison will be performed in the specified unit.
Otherwise, it will be performed in the unit that the variable currently has.
Units can only be specified for numerical values.


Membership test filter
----------------------

    | ``variable in (value, ...) [unit]``
    | ``variable not in (value, ...) [unit]``

Exclude measurements that occur or do not occur in the specified list.

If a unit is specified, the comparison will be performed in the specified unit.
Otherwise, it will be performed in the unit that the variable currently has.
Units can only be specified for numerical variables.

Function call
-------------

    ``function(argument, ...)``

Supported functions:

    ``area_covers_area((lat, ...) [unit], (lon, ...) [unit])``
        Exclude measurements whose area does not fully cover the given polygon.
        Example:

            | ``area_covers_area((51.98,51.98,52.02,52.02),(4.33,4.39,4.39,4.33))``

    ``area_covers_area(area-mask-file)``
        Exclude measurements whose area does not fully cover one of the
        areas of the area mask file.

        Example:

            | ``area_covers_area("areas.csv")``

    ``area_covers_point(latitude [unit], longitude [unit])``
        Exclude measurements whose area does not cover the given point.
        Example:

            | ``area_covers_point(52.012, 4.357)``

    ``area_inside_area((lat, ...) [unit], (lon, ...) [unit])``
        Exclude measurements whose area is not inside the given polygon.
        Example:

            | ``area_inside_area((50,50,54,54),(3,8,8,3))``

    ``area_inside_area(area-mask-file)``
        Exclude measurements whose area is not inside one of the areas of
        the area mask file.

    ``area_intersects_area((lat, ...) [unit], (lon, ...) [unit], minimum-overlap-fraction)``
        Exclude measurements whose area does not overlap at least the
        specified fraction with the given polygon.
        The fraction is calculated as area(intersection)/min(area(x),area(y))

    ``area_intersects_area(area-mask-file, minimum-overlap-fraction)``
        Exclude measurements whose area does not overlap at least the
        specified fraction with one the areas of the area mask file.

    ``area_intersects_area((lat, ...) [unit], (lon, ...) [unit])``
        Exclude measurements whose area does not overlap with the given
        polygon.

    ``area_intersects_area(area-mask-file)``
        Exclude measurements whose area does not overlap with one of the
        areas of the area mask file.

    ``bin()``
        For all variables in a product perform an averaging in the time
        dimension such that all samples end up in a single bin.

    ``bin(variable)``
        For all variables in a product perform an averaging in the time
        dimension such that all samples in the same bin get averaged.
        A bin is defined by all samples of the given variable that have
        the same value.
        Example:

            | ``bin(index)``
            | ``bin(validity)``

    ``bin(collocation-result-file, a|b)``
        For all variables in a product perform an averaging in the time
        dimension such that all samples in the same bin get averaged.
        A bin is defined by all samples having the same collocated sample
        from the dataset that is indicated by the second parameter.
        Example:

            | ``bin("collocation-result.csv", b)``
            | (the product is part of dataset A and the collocated
              sample that defines the bin is part of dataset B)

    ``bin_spatial((lat_edge, lat_edge, ...), (lon_edge, lon_edge, ...))``
        For all variables in a product map all time samples onto a
        spatial latitude/longitude grid. The latitude/longitude grid is
        defined by the list of edge values.
        Example:

            | ``bin_spatial((-90,-60,-30,0,30,60,90),(-180,0,180))``
            | (bin data onto latitude bands, separated into an
            |  eastern and western hemisphere)

    ``bin_spatial(lat_edge_length, lat_edge_offset, lat_edge_step, lon_edge_length, lon_edge_offset, lon_edge_step)``
        For all variables in a product map all time samples onto a
        spatial latitude/longitude grid. The latitude/longitude grid is
        defined by the list of edge values.
        Example:

            | ``bin_spatial(7, -90, 30, 3, -180, 180)``
            | (this is the same as ``bin_spatial((-90,-60,-30,0,30,60,90),(-180,0,180))``)

    ``collocate_left(collocation-result-file)``
        Apply the specified collocation result file as an index
        filter assuming the product is part of dataset A.

    ``collocate_right(collocation-result-file)``
        Apply the specified collocation result file as an index
        filter assuming the product is part of dataset B.

    ``derive(variable [datatype] [unit])``
        The derive operation *without* a dimension specification can be
        used to change the data type or unit of an already existing
        variable. A variable with the given name should therefore already
        be in the product (with any kind of dimensions).
        If a unit conversion is performed and no data type is specified
        the variable will be converted to ``double`` values.

        Example:

            | ``derive(altitude [km])``
            | ``derive(latitude float)``

    ``derive(variable [datatype] {dimension-type, ...} [unit])``
        The derive operation *with* a dimension specification is used
        to derive the specified variable from other variables found in
        the product (i.e. a variable with that name and dimension does
        not have to exist yet). The ``--list-derivations`` option of
        harpdump can be used to list available variable conversions.
        The algorithms behind all the conversions are described
        in the :doc:`Algorithms <algorithms/index>` section of the
        documentation.
        If the datatype is not provided then the default result data
        type for a conversion will be used (usually ``double``).
        If a variable with the given name and dimension specification
        already exists then this operation will just perform a data
        type and/or unit conversion on that variable.

        Example:

            | ``derive(number_density {time,vertical} [molec/m3])``
            | ``derive(latitude float {time})``

    ``derive_smoothed_column(variable {dimension-type, ...} [unit], axis-variable unit, collocation-result-file, a|b, dataset-dir)``
        Derive the given integrated column value by first deriving
        a partial column profile variant of the variable and then
        smoothing/integrating this partial column profile using the
        column avaraging kernel (and a-priori, if available) from a
        collocated dataset. The fourth parameter indicates which dataset
        contains the avaraging kernel.
        Before smoothing the partial column profile is regridded to
        the grid of the column averaging kernel using the given
        axis-variable (see also ``regrid()``).

        Example:

            ``derive_smoothed_column(O3_column_number_density {time} [molec/cm2], altitude [km], "collocation-result.csv", b, "./correlative_data/")``

    ``derive_smoothed_column(variable {dimension-type, ...} [unit], axis-variable unit, collocated-file)``
        Derive the given integrated column value by first deriving
        a partial column profile variant of the variable and then
        smoothing/integrating this partial column profile using the
        column avaraging kernel (and a-priori, if available) from a
        single merged collocated product. Both the product and the
        collocated product need to have a ``collocation_index``
        variable that will be used to associate the right collocated
        grid/avk/apriori to each sample.
        Before smoothing the partial column profile is regridded to
        the grid of the column averaging kernel using the given
        axis-variable (see also ``regrid()``).

        Example:

            ``derive_smoothed_column(O3_column_number_density {time} [molec/cm2], altitude [km], "./collocated_file.nc")``

    ``exclude(variable, ...)``
        Mark the specified variable(s) for exclusion from the
        ingested product. All variables marked for exclusion
        will be excluded from the ingested product, all other
        variables will be kept.
        Variables that do not exist will be ignored.

    ``flatten(dimension)``
        Flatten a product for a certain dimension by collapsing the
        given dimension into the time dimension. The time dimension
        will thus grow by a factor equal to the length of the given
        dimension and none of the variables in the product will
        depend on the given dimension anymore. If the length of the
        flattened dimension does not equal 1 then: variables that
        depend more than once on the given dimension will be removed,
        the index and collocation_index variables will be removed,
        and time independent variables are made time dependent.
        Independent dimensions and the time dimension cannot be flattened.
        Example:

            | ``flatten(latitude);flatten(longitude)``
            | (turn a 2D lat/lon grid into a a series of individual points)

    ``keep(variable, ...)``
        Mark the specified variable(s) for inclusion in the
        ingested product. All variables marked for inclusion
        will be kept in the ingested product, all other
        variables will be excluded.
        Trying to keep a variable that does not exist will
        result in an error.

    ``longitude_range(minimum [unit], maximum [unit])``
        Exclude measurements of which the longitude of the
        measurement location falls outside the specified range.
        This function correctly handles longitude ranges that
        cross the international date line.

            | ``longitude_range(179.0, -179.0)``
            | (select a 2 degree range around the international dateline)

    ``point_distance(latitude [unit], longitude [unit], distance [unit])``
        Exclude measurements whose point location is situated further than
        the specified distance from the given location.
        Example:

            ``point_distance(52.012, 4.357, 3 [km])``

    ``point_in_area((lat, ...) [unit], (lon, ...) [unit])``
        Exclude measurements whose point location does not fall inside the
        measurement area.
        Example:

            ``point_in_area((50,50,54,54) [degN],(3,8,8,3) [degE])``

    ``point_in_area(area-mask-file)``
        Exclude measurements whose point location does not fall inside one of
        the areas from the area mask file.

    ``regrid(dimension, axis-variable unit, (value, ...))``
        Regrid all variables in the product for the given dimension using
        the given axis variable as target grid. The operation will use a
        ``derive(axis-variable {[time,]dimension} unit)`` to determine
        the current grid. The target grid is specified as a list of values.
        Example:

            ``regrid(vertical, altitude [km], (1.0, 2.0, 5.0, 10.0, 15.0, 20.0, 30.0))``

    ``regrid(dimension, axis-variable unit, (value, ...), (value, ...))``
        Regrid all variables in the product for the given dimension using
        the given axis variable as target grid. The operation will use a
        ``derive(axis-variable {[time,]dimension} unit)`` and
        ``derive(<axis-variable>_bounds {[time,]dimension} unit)`` to
        determine the current grid and boundaries. The target grid mid points
        are specified by the first list of values and the target grid
        boundaries by the second list of values. If there are N mid points,
        then the list of boundary values can either contain N+1 points if the
        boundaries are adjacent or 2N points to define each boundary pair
        separately.
        Example:

            | ``regrid(vertical, altitude [km], (1.0, 2.0, 5.0), (0.0, 1.5, 3.0, 7.0))``
            | ``regrid(vertical, altitude [km], (1.0, 2.0, 5.0), (0.5, 1.5, 1.5, 2.5, 4.0, 6.0))``

    ``regrid(dimension, axis-variable unit, length, offset, step)``
        Regrid all variables in the product for the given dimension using
        the given axis variable as target grid. The operation will use a
        ``derive(axis-variable {[time,]dimension} unit)`` to determine
        the current grid. The target grid is specified as using a length,
        offset, and step parameters.
        Example:

            | ``regrid(vertical, altitude [km], 10, 0.5, 1.0)``
            | (indicating a grid of altitudes 0.5, 1.5, ..., 9.5)
            | ``regrid(time, datetime [hours since 2017-04-01], 23, 0.5, 1.0)``

    ``regrid(dimension, axis-variable unit, collocation-result-file, a|b, dataset-dir)``
        Regrid all variables in the product for the given dimension using the
        target grid taken from a collocated dataset. The fourth parameter
        indicates which dataset contains the target grid.
        Example:

            ``regrid(vertical, altitude [km], "collocation-result.csv", b, "./correlative_data/")``

    ``regrid(dimension, axis-variable unit, collocated-file)``
        Regrid all variables in the product for the given dimension using the
        target grid taken from a single merged collocated product. Both the
        product and the collocated product need to have a ``collocation_index``
        variable that will be used to associate the right collocated grid to
        each sample.
        Example:

            ``regrid(vertical, altitude [km], "./collocated_file.nc")``

    ``rename(variable, new_name)``
        Rename the variable to the new name.
        Note that this operation should be used with care since it will
        change the meaning of the data (potentially interpreting it
        incorrectly in further operations). It is primarilly meant to
        add/remove prefixes (such as surface/tropospheric/etc.) to allow
        the variable to be used in a more specific (with prefix) or
        generic (without prefix) way.
        Example:

            ``rename("surface_temperature", "temperature")``

    ``set(option, value)``
        Set a specific option in HARP.
        Both the option and value parameters need to be provided as string
        values (using double quotes).
        Options will be set 'globally' in HARP and will persists for all
        further operations in the list. After termination of the list of
        operations, all HARP options will be reverted back to their initial
        values.
        Available options are:

        ``afgl86``
            Possible values are:

            - ``disabled`` (default) disable the use of AFGL86 climatology
              in variable conversions
            - ``enabled`` enable the use of AFGL86 climatology in variable
              conversions (using seasonal and latitude band dependence)
            - ``usstd76`` enable AFGL86 using US Standard profiles

        ``regrid_out_of_bounds``
            Determine how to deal with interpolation of target grid values
            that fall outside the source grid range.
            Possible values are:

            - ``nan`` (default) to set values outside the range to NaN
            - ``edge`` to use the nearest edge value
            - ``extrapolate`` to perform extrapolation

        Example:

            | ``set("afgl86", "enabled")``
            | ``set("regrid_out_of_bounds", "extrapolate")``

    ``smooth(variable, dimension, axis-variable unit, collocation-result-file, a|b, dataset-dir)``
        Smooth the given variable in the product for the given dimension
        using the avaraging kernel (and a-priori profile, if available)
        from a collocated dataset. The fifth parameter indicates which
        dataset contains the avaraging kernel. Before smoothing the
        product is regridded to the grid of the averaging kernel using
        the given axis-variable (see also ``regrid()``).
        Example:

            ``smooth(O3_number_density, vertical, altitude [km], "collocation-result.csv", b, "./correlative_data/")``

    ``smooth((variable, variable, ...), dimension, axis-variable unit, collocation-result-file, a|b, dataset-dir)``
        Same as above, but then providing a list of variables that need
        to be smoothed. For each variable an associated averaging kernel
        (and associated a-priori, if applicable) needs to be present in
        the collocated dataset.

    ``smooth(variable, dimension, axis-variable unit, collocated-file)``
        Smooth the given variable in the product for the given dimension
        using the avaraging kernel (and a-priori profile, if available)
        from a single merged collocated product. Both the product and the
        collocated product need to have a ``collocation_index`` variable
        that will be used to associate the right collocated grid/avk/apriori
        to each sample.
        Before smoothing the product is regridded to the grid of the
        averaging kernel using the given axis-variable (see also ``regrid()``).
        Example:

            ``smooth(O3_number_density, vertical, altitude [km], "./collocated_file.nc")``

    ``smooth((variable, variable, ...), dimension, axis-variable unit, collocated-file)``
        Same as above, but then providing a list of variables that need
        to be smoothed. For each variable an associated averaging kernel
        (and associated a-priori, if applicable) needs to be present in
        the merged collocated file.

    ``sort(variable)``
        Reorder a dimension for all variables in the product such that the
    	variable provided as parameter ends up being sorted. The variable
    	should be one dimensional and the dimension that gets reordered is
    	this dimension of the referenced variable.

    ``valid(variable)``
        Filter a dimension for all variables in the product such that
        invalid values for the variable provided as parameter get excluded
        (values outside the valid range of the variable, or NaN).
        This operation is executed similar to a comparison filter.

    ``wrap(variable [unit], minimum, maximum)``
        Wrap the values of the variable to the range given by minimum
        and maximum. The result is: min + (value - min) % (max - min)
        Example:

            ``wrap(longitude [degree_east], -180, 180)``


Collocation result file
-----------------------

The format of the collocation result file is described in the
:doc:`conventions <conventions/collocation_result>` section of the HARP documentation.

Area mask file
--------------

A comma separated (csv) file is used as input for area filters.

It has the following format:

::

    lat0,lon0,lat1,lon1,lat2,lon2,lat3,lon3
    0.0,60.0,40.0,60.0,40.0,-60.0,0.0,-60.0
    ...

It starts with a header with latitude, longitude column headers (this
header will be skipped by HARP). Then, each further line defines a polygon.
Each polygon consists of the vertices as defined on that line.

Examples
--------

    | ``derive(altitude {time} [km]); pressure > 3.0 [bar]``
    | ``point_distance(-52.5 [degree], 1.0 [rad], 1e3 [km])``
    | ``index in (0, 10, 20, 30, 40); valid(pressure)``

Formal definition
-----------------

::

    digit = '0'|'1'|'2'|'3'|'4'|'5'|'6'|'7'|'8'|'9' ;
    sign = '+'|'-' ;

    alpha =
       'a'|'b'|'c'|'d'|'e'|'f'|'g'|'h'|'i'|'j'|'k'|'l'|'m'|
       'n'|'o'|'p'|'q'|'r'|'s'|'t'|'u'|'v'|'w'|'x'|'y'|'z'|
       'A'|'B'|'C'|'D'|'E'|'F'|'G'|'H'|'I'|'J'|'K'|'L'|'M'|
       'N'|'O'|'P'|'Q'|'R'|'S'|'T'|'U'|'V'|'W'|'X'|'Y'|'Z' ;

    character = alpha | digit |
       ' '|'!'|'"'|'#'|'$'|'%'|'&'|"'"|'('|')'|'*'|'+'|','|
       '-'|'.'|'/'|':'|';'|'<'|'='|'>'|'?'|'@'|'['|'\'|']'|
       '^'|'_'|'`'|'{'|'|'|'}'|'~' ;

    identifier = alpha, [{alpha | digit | '_'}] ;

    variable = identifier ;

    variablelist =
       variable |
       variablelist, ',', variable ;

    intvalue = [sign], {digit} ;

    floatvalue =
       [sign], ('N' | 'n'), ('A' | 'a'), ('N' | 'n') |
       [sign], ('I' | 'i'), ('N' | 'n'), ('F' | 'f') |
       (intvalue, '.', [{digit}] | '.', {digit}), [('D' | 'd' | 'E' | 'e'), intvalue] ;

    stringvalue = '"', [{character-('\', '"') | '\' character}], '"' ;

    value = intvalue | floatvalue | stringvalue ;

    intvaluelist =
       intvalue |
       intvaluelist, ',', intvalue;

    floatvaluelist =
       floatvalue |
       floatvaluelist, ',', floatvalue;

    stringvaluelist =
       stringvalue |
       stringvaluelist, ',', stringvalue;

    valuelist = intvaluelist | floatvaluelist | stringvaluelist ;

    unit = '[', [{character-(']')}], ']' ;

    datatype = 'int8' | 'int16' | 'int32' | 'float' | 'double' | 'string' ;

    dimension = 'time' | 'latitude' | 'longitude' | 'vertical' | 'spectral' | 'independent' ;

    dimensionlist =
       dimension |
       dimensionlist, ',', dimension ;

    dimensionspec = '{' dimensionlist '}' ;

    functioncall =
       'area_covers_area', '(', '(', floatvaluelist, ')', [unit], '(', floatvaluelist, ')', [unit], ')' |
       'area_covers_area', '(', stringvalue, ')' |
       'area_covers_point', '(', floatvalue, [unit], ',', floatvalue, [unit], ')' |
       'area_inside_area', '(', '(', floatvaluelist, ')', [unit], '(', floatvaluelist, ')', [unit], ')' |
       'area_inside_area', '(', stringvalue, ')' |
       'area_intersects_area', '(', '(', floatvaluelist, ')', [unit], '(', floatvaluelist, ')', [unit], ',', floatvalue, ')' |
       'area_intersects_area', '(', stringvalue, ',', floatvalue, ')' |
       'area_intersects_area', '(', '(', floatvaluelist, ')', [unit], '(', floatvaluelist, ')', [unit], ')' |
       'area_intersects_area', '(', stringvalue, ')' |
       'bin', '(', [variable], ')' |
       'bin', '(', stringvalue, ',', ( 'a' | 'b' ), ')' |
       'collocate_left', '(', stringvalue, ')' |
       'collocate_right', '(', stringvalue, ')' |
       'derive', '(', variable, [datatype], [dimensionspec], [unit], ')' |
       'derive_smoothed_column', '(', variable, dimensionspec, [unit], ',', variable, unit, ',', stringvalue, ',', ( 'a' | 'b' ), ',', stringvalue, ')' |
       'derive_smoothed_column', '(', variable, dimensionspec, [unit], ',', variable, unit, ',', stringvalue, ')' |
       'exclude', '(', variablelist, ')' |
       'flatten', '(', dimension, ')' |
       'keep', '(', variablelist, ')' |
       'longitude_range', '(', floatvalue, [unit], ',', floatvalue, [unit], ')' |
       'point_distance', '(', floatvalue, [unit], ',', floatvalue, [unit], ',', floatvalue, [unit], ')' |
       'point_in_area', '(', '(', floatvaluelist, ')', [unit], '(', floatvaluelist, ')', [unit], ')' |
       'point_in_area', '(', stringvalue, ')' |
       'regrid', '(', dimension, ',', variable, unit, ',', '(', floatvaluelist, ')', ')' |
       'regrid', '(', dimension, ',', variable, unit, ',', '(', floatvaluelist, ')', ',', '(', floatvaluelist, ')', ')' |
       'regrid', '(', dimension, ',', variable, unit, ',', intvalue, ',', floatvalue, ',', floatvalue, ')' |
       'regrid', '(', dimension, ',', variable, unit, ',', stringvalue, ',', ( 'a' | 'b' ), ',', stringvalue, ')' |
       'regrid', '(', dimension, ',', variable, unit, ',', stringvalue, ')' |
       'rename', '(', variable, ',', variable, ')' |
       'set', '(', stringvalue, ',', stringvalue, ')' |
       'smooth', '(', variable, ',', dimension, ',', variable, unit, ',', stringvalue, ',', ( 'a' | 'b' ), ',', stringvalue, ')' |
       'smooth', '(', '(', variablelist, ')', ',', dimension, ',', variable, unit, ',', stringvalue, ',', ( 'a' | 'b' ), ',', stringvalue, ')' |
       'smooth', '(', variable, ',', dimension, ',', variable, unit, ',', stringvalue, ')' |
       'smooth', '(', '(', variablelist, ')', ',', dimension, ',', variable, unit, ',', stringvalue, ')' |
       'sort', '(', variable, ')' |
       'valid', '(', variable, ')' |
       'wrap', '(', variable, [unit], ',', floatvalue, ',', floatvalue, ')' ;

    operationexpr =
       variable, operator, value, [unit] |
       variable, ['not'], 'in', '(', valuelist, ')', [unit] |
       functioncall |
       operationexpr, ';', operationexpr ;

    operations =
       operationexpr ';' |
       operationexpr ;
