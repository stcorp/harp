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

    ``collocate-left(collocation-result-file)``
        Apply the specified collocation result file as an index
        filter assuming the product is part of dataset A.

    ``collocate-right(collocation-result-file)``
        Apply the specified collocation result file as an index
        filter assuming the product is part of dataset B.

    ``valid(variable)``
        Exclude invalid values of the specified variable (values
        outside the valid range of the variable, or NaN).

    ``longitude-range(minimum [unit], maximum [unit])``
        Exclude measurements of which the longitude of the
        measurement location falls outside the specified range.
        This function correctly handles longitude ranges that
        cross the international date line.
        
    ``point-distance(longitude [unit], latitude [unit], distance [unit])``
        Exclude measurements situated further than the specified
        distance from the specified location.
        
    ``area-mask-covers-point(area-mask-file)``
        Exclude measurements for which no area from the area
        mask file contains the measurement location.
        
    ``area-mask-covers-area(area-mask-file)``
       Exclude measurements for which no area from the area
       mask file covers the measurement area completely.
        
    ``area-mask-intersects-area(area-mask-file, minimum-overlap-percentage)``
       Exclude measurements for which no area from the area
       mask file overlaps at least the specified percentage of
       the measurement area.
        
    ``derive(variable {dimension-type, ...} [unit])``
       Derive the specified variable from other variables found
       in the product. The ``--list-conversions`` option of
       harpconvert and harpfilter can be used to list available
       variable conversions.
        
    ``keep(variable, ...)``
       Mark the specified variable(s) for inclusion in the
       ingested product. All variables marked for inclusion
       will be kept in the ingested product, all other
       variables will be excluded.
        
    ``exclude(variable, ...)``
       Mark the specified variable(s) for exclusion from the
       ingested product. All variables marked for exclusion
       will be excluded from the ingested product, all other
       variables will be kept.

Collocation result file
-----------------------

The format of the collocation result file is described in the :ref:`data formats
<collocation\-result\-file\-format>` documentation.


Area mask file
--------------

A comma separated (csv) file is used as input for area filters.

It has the following format:

::
    longitude0[degree_north],latitude0[degree_north],longitude1[degree_north],latitude1[degree_north],longitude2[degree_north],latitude2[degree_north],longitude3[degree_north],latitude3[degree_north],longitude4[degree_north],latitude4[degree_north]
    -60.0,40.0,60.0,40.0,60.0,0.0,-60.0,0.0,-60.0,40.0
    ...

It starts with a header with longitude, latitude for each polygon point with the units in square brackets. This examples show the coordinates of four vertices plus a duplicate point connecting the first and the last point of the polygon area. Thus, each line in the comma separated file specifies one polygon area.

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
       [sign], ('N' | 'n'), ('A', 'a'), ('N', 'n') |
       [sign], ('I' | 'i'), ('N', 'n'), ('F', 'f') |
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
       'collocate-left', '(', stringvalue, ')' |
       'collocate-right', '(', stringvalue, ')' |
       'valid', '(', variable, ')' |
       'longitude-range', '(', floatvalue, [unit], ',', floatvalue, [unit], ')' |
       'point-distance', '(', floatvalue, [unit], ',', floatvalue, [unit], ',', floatvalue, [unit], ')' |
       'area-mask-covers-point', '(', stringvalue, ')' |
       'area-mask-covers-area', '(', stringvalue, ')' |
       'area-mask-intersects-area', '(', stringvalue, ',', floatvalue, ')' |
       'derive', '(', variable, dimensionspec, [unit], ')' |
       'keep', '(', variablelist, ')' |
       'exclude', '(', variablelist, ')' ;

    operationexpr = 
       variable, operator, value, [unit] |
       variable, ['not'], 'in', '(', valuelist, ')', [unit] |
       functioncall |
       operationexpr, ';', operationexpr ;
