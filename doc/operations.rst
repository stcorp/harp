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

    ``area-mask-covers-area(area-mask-file)``
       Exclude measurements for which no area from the area
       mask file covers the measurement area completely.

    ``area-mask-covers-point(area-mask-file)``
        Exclude measurements for which no area from the area
        mask file contains the measurement location.

    ``area-mask-intersects-area(area-mask-file, minimum-overlap-percentage)``
       Exclude measurements for which no area from the area
       mask file overlaps at least the specified percentage of
       the measurement area.

    ``collocate-left(collocation-result-file)``
        Apply the specified collocation result file as an index
        filter assuming the product is part of dataset A.

    ``collocate-right(collocation-result-file)``
        Apply the specified collocation result file as an index
        filter assuming the product is part of dataset B.

    ``derive(variable {dimension-type, ...} [unit])``
       Derive the specified variable from other variables found
       in the product. The ``--list-conversions`` option of
       harpconvert and harpfilter can be used to list available
       variable conversions.
       The algorithms behind all the conversions are described
       in the :doc:`Algorithms <algorithms>` section of the
       documentation.

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
       index and collocation_index variables will also be removed.
       Independent dimensions cannot be flattened.
       Example:

           ``flatten(longitude);flatten(latitude)``
           (turn a 2D lat/lon grid into a a series of individual points)

    ``keep(variable, ...)``
       Mark the specified variable(s) for inclusion in the
       ingested product. All variables marked for inclusion
       will be kept in the ingested product, all other
       variables will be excluded.

    ``longitude-range(minimum [unit], maximum [unit])``
        Exclude measurements of which the longitude of the
        measurement location falls outside the specified range.
        This function correctly handles longitude ranges that
        cross the international date line.

            ``longitude-range(179.0, -179.0)``
            (select a 2 degree range around the international dateline)

    ``point-distance(longitude [unit], latitude [unit], distance [unit])``
        Exclude measurements situated further than the specified
        distance from the specified location.
        Example:

            ``point-distance(4.357, 52.012, 3 [km])`` 

    ``regrid(dimension, axisvariable unit, (value, ...))``
        Regrid all variables in the product for the given dimension using
        the given axis variable as target grid. The operation will use a
        ``derive(axisvariable {[time,]dimension} unit)`` to determine
        the current grid. The target grid is specified as a list of values.
        Example:

            ``regrid(vertical, altitude [km], (1.0, 2.0, 5.0, 10.0, 15.0, 20.0, 30.0))``

    ``regrid(dimension, axisvariable unit, length, offset, step)``
        Regrid all variables in the product for the given dimension using
        the given axis variable as target grid. The operation will use a
        ``derive(axisvariable {[time,]dimension} unit)`` to determine
        the current grid. The target grid is specified as using a length,
        offset, and step parameters.
        Example:

            ``regrid(vertical, altitude [km], 10, 0.5, 1.0)``
            (indicating a grid of altitudes 0.5, 1.5, ..., 9.5)

    ``valid(variable)``
        Exclude invalid values of the specified variable (values
        outside the valid range of the variable, or NaN).


Collocation result file
-----------------------

The format of the collocation result file is described in the :ref:`data formats
<collocation\-result\-file\-format>` documentation.

Area mask file
--------------

A comma separated (csv) file is used as input for area filters.

It has the following format:

::

    lon0,lat0,lon1,lat1,lon2,lat2,lon3,lat3
    60.0,0.0,60.0,40.0,-60.0,40.0,-60.0,0.0
    ...

It starts with a header with longitude, latitude column headers (this header will be skipped by HARP).
Then, each further line defines a polygon. Each polygon consists of the vertices as defined on that line.

Examples
--------

    | ``derive(altitude {time} [km]); pressure > 3.0 [bar]``
    | ``point-distance(-52.5 [degree], 1.0 [rad], 1e3 [km])``
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
       'area-mask-covers-area', '(', stringvalue, ')' |
       'area-mask-covers-point', '(', stringvalue, ')' |
       'area-mask-intersects-area', '(', stringvalue, ',', floatvalue, ')' |
       'collocate-left', '(', stringvalue, ')' |
       'collocate-right', '(', stringvalue, ')' |
       'derive', '(', variable, dimensionspec, [unit], ')' |
       'exclude', '(', variablelist, ')' |
       'flatten', '(', dimension, ')' ;
       'keep', '(', variablelist, ')' |
       'longitude-range', '(', floatvalue, [unit], ',', floatvalue, [unit], ')' |
       'point-distance', '(', floatvalue, [unit], ',', floatvalue, [unit], ',', floatvalue, [unit], ')' |
       'regrid', '(', dimension, ',', variable, [unit], ',', '(', floatvaluelist, ')', ')' |
       'regrid', '(', dimension, ',', variable, [unit], ',', intvalue, ',', floatvalue, ',', floatvalue, ')' |
       'valid', '(', variable, ')' |

    operationexpr = 
       variable, operator, value, [unit] |
       variable, ['not'], 'in', '(', valuelist, ')', [unit] |
       functioncall |
       operationexpr, ';', operationexpr ;

    operations =
       operationexpr ';' |
       operationexpr ;
