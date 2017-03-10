Operations
==========

HARP provides a simple expression syntax with which one can specify operations that need to be performed on a product.
A list of operations is always provided as a single string where individual operations are separated by a semi-colon (`;`).
Each operation can be either a comparison filter, a membership test filter, or a function call.
Strings used in operations should be quoted with double quotes.

Comparison filter
-----------------

    ``variable operator value [unit]``

Exclude measurements for which the comparison evaluates to false.

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

    ``area_mask_covers_area(area-mask-file)``
       Exclude measurements for which no area from the area
       mask file covers the measurement area completely.

    ``area_mask_covers_point(area-mask-file)``
        Exclude measurements for which no area from the area
        mask file contains the measurement location.

    ``area_mask_intersects_area(area-mask-file, minimum-overlap-percentage)``
       Exclude measurements for which no area from the area
       mask file overlaps at least the specified percentage of
       the measurement area.

    ``collocate_left(collocation-result-file)``
        Apply the specified collocation result file as an index
        filter assuming the product is part of dataset A.

    ``collocate_right(collocation-result-file)``
        Apply the specified collocation result file as an index
        filter assuming the product is part of dataset B.

    ``derive(variable {dimension-type, ...} [unit])``
       Derive the specified variable from other variables found
       in the product. The ``--list-derivations`` option of
       harpconvert, harpdump, and harpfilter can be used to list
       available variable conversions.
       The algorithms behind all the conversions are described
       in the :doc:`Algorithms <algorithms/index>` section of the
       documentation.

    ``derive_smoothed_column(variable {dimension-type, ...} [unit], axis-variable unit, collocation-result-file, a|b, dataset-dir)``
       Derive the given intergrated column value by first deriving
       a partial column profile variant of the variable and then
       smoothing/integrating this partial column profile using the
       column avaraging kernel (and a-priori, if available) from a
       collocated dataset. The third parameter indicates which dataset
       contains the avaraging kernel.
       Before smoothing the partial column profile is regridded to
       the grid of the column averaging kernel using the given
       axis-variable (see also ``regrid()``).

       Example:

           ``derive_smoothed_column(O3_column_number_density {time} [molec/cm2], altitude [km], "collocation-result.csv", b, "./correlative_data/")``

    ``exclude(variable, ...)``
       Mark the specified variable(s) for exclusion from the
       ingested product. All variables marked for exclusion
       will be excluded from the ingested product, all other
       variables will be kept.

    ``flatten(dimension)``
       Flatten a product for a certain dimension by collapsing the
       given dimension into the time dimension. The time dimension
       will thus grow by a factor equal to the length of the given
       dimension and none of the variables in the product will
       depend on the given dimension anymore. Variables that depend
       more than once on the given dimension will be removed. The
       index and collocation_index variables will be removed if the
       length of the flattened dimension did not equal 1.
       Independent dimensions cannot be flattened.
       Example:

           ``flatten(latitude);flatten(longitude)``
           (turn a 2D lat/lon grid into a a series of individual points)

    ``keep(variable, ...)``
       Mark the specified variable(s) for inclusion in the
       ingested product. All variables marked for inclusion
       will be kept in the ingested product, all other
       variables will be excluded.

    ``longitude_range(minimum [unit], maximum [unit])``
        Exclude measurements of which the longitude of the
        measurement location falls outside the specified range.
        This function correctly handles longitude ranges that
        cross the international date line.

            ``longitude_range(179.0, -179.0)``
            (select a 2 degree range around the international dateline)

    ``point_distance(latitude [unit], longitude [unit], distance [unit])``
        Exclude measurements situated further than the specified
        distance from the specified location.
        Example:

            ``point_distance(52.012, 4.357, 3 [km])``

    ``point_in_area(latitude [unit], longitude [unit])``
        Exclude measurements for which the given location does not
        fall inside the measurement area.
        Example:

            ``point_in_area(52.012, 4.357)``

    ``regrid(dimension, axis-variable unit, (value, ...))``
        Regrid all variables in the product for the given dimension using
        the given axis variable as target grid. The operation will use a
        ``derive(axis-variable {[time,]dimension} unit)`` to determine
        the current grid. The target grid is specified as a list of values.
        Example:

            ``regrid(vertical, altitude [km], (1.0, 2.0, 5.0, 10.0, 15.0, 20.0, 30.0))``

    ``regrid(dimension, axis-variable unit, length, offset, step)``
        Regrid all variables in the product for the given dimension using
        the given axis variable as target grid. The operation will use a
        ``derive(axis-variable {[time,]dimension} unit)`` to determine
        the current grid. The target grid is specified as using a length,
        offset, and step parameters.
        Example:

            ``regrid(vertical, altitude [km], 10, 0.5, 1.0)``
            (indicating a grid of altitudes 0.5, 1.5, ..., 9.5)

    ``regrid(dimension, axis-variable unit, collocation-result-file, a|b, dataset-dir)``
        Regrid all variables in the product for the given dimension using
        the a target grid taken from a collocated dataset. The fourth
        parameter indicates which dataset contains the target grid. 
        Example:

            ``regrid(vertical, altitude [km], "collocation-result.csv", b, "./correlative_data/")``

    ``rename(variable, new_name)``
        Rename the variable to the new name.
        Note that this operation should be used with care since it will change the meaning of the
        data (potentially interpreting it incorrectly in further operations).
        It is primarilly meant to add/remove prefixes (such as surface/tropospheric/etc.) to allow
        the variable to be used in a more specific (with prefix) or generic (without prefix) way.
        Example:

            ``rename("surface_temperature", "temperature")``

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
        Same as above, but then providing a list of variables that need to be smoothed.
        For each variable an associated averaging kernel (and associated a-priori,
        if applicable) needs to be present in the collocated dataset.

    ``valid(variable)``
        Exclude invalid values of the specified variable (values
        outside the valid range of the variable, or NaN).

    ``wrap(variable [unit], minimum, maximum)``
        Wrap the values of the variable to the range given by minimum and maximum.
        The result is: min + (value - min) % (max - min)
        Example:

            ``wrap(longitude [degree_east], -180, 180)``


Collocation result file
-----------------------

The format of the collocation result file is described in the :ref:`data formats
<collocation\-result\-file\-format>` documentation.

Area mask file
--------------

A comma separated (csv) file is used as input for area filters.

It has the following format:

::

    lat0,lon0,lat1,lon1,lat2,lon2,lat3,lon3
    0.0,60.0,40.0,60.0,40.0,-60.0,0.0,-60.0
    ...

It starts with a header with latitude, longitude column headers (this header will be skipped by HARP).
Then, each further line defines a polygon. Each polygon consists of the vertices as defined on that line.

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

    dimension = 'time' | 'latitude' | 'longitude' | 'vertical' | 'spectral' | 'independent' ;

    dimensionlist =
       dimension |
       dimensionlist, ',', dimension ;

    dimensionspec = '{' dimensionlist '}' ;

    functioncall = 
       'area_mask_covers_area', '(', stringvalue, ')' |
       'area_mask_covers_point', '(', stringvalue, ')' |
       'area_mask_intersects_area', '(', stringvalue, ',', floatvalue, ')' |
       'collocate_left', '(', stringvalue, ')' |
       'collocate_right', '(', stringvalue, ')' |
       'derive', '(', variable, dimensionspec, [unit], ')' |
       'derive_smoothed_column', '(', variable, dimensionspec, [unit], ',', variable, unit, ',', stringvalue, ',', ( 'a' | 'b' ), ',', stringvalue, ')' |
       'exclude', '(', variablelist, ')' |
       'flatten', '(', dimension, ')' |
       'keep', '(', variablelist, ')' |
       'longitude_range', '(', floatvalue, [unit], ',', floatvalue, [unit], ')' |
       'point_distance', '(', floatvalue, [unit], ',', floatvalue, [unit], ',', floatvalue, [unit], ')' |
       'point_in_area', '(', floatvalue, [unit], ',', floatvalue, [unit], ')' |
       'regrid', '(', dimension, ',', variable, unit, ',', '(', floatvaluelist, ')', ')' |
       'regrid', '(', dimension, ',', variable, unit, ',', intvalue, ',', floatvalue, ',', floatvalue, ')' |
       'regrid', '(', dimension, ',', variable, unit, ',', stringvalue, ',', ( 'a' | 'b' ), ',', stringvalue, ')' |
       'smooth', '(', variable, ',' dimension, ',', variable, unit, ',', stringvalue, ',', ( 'a' | 'b' ), ',', stringvalue, ')' |
       'smooth', '(', '(', variablelist, ')', ',' dimension, ',', variable, unit, ',', stringvalue, ',', ( 'a' | 'b' ), ',', stringvalue, ')' |
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
