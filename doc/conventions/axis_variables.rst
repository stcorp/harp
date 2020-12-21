Axis variables
==============

HARP does not impose a fixed link between dimensions and its axis variables.
A product can contain multiple variables that can function as an axis variable for a dimension.
For instance, for the vertical dimension, a product can contain both a ``pressure`` and an ``altitude`` variable
where either can function as an axis variable.
The same applies for e.g. ``wavelength`` and ``wavenumber`` for the spectral axis.

In order for a variable to function as an axis variable it should have either strict ascending or strict descending values.

Intervals
---------
HARP has strict rules when it comes to dealing with the specification of an applicable interval along a dimension.

Especially for the vertical dimension there is often some degree of confusion about the concept of layers vs. levels
and whether levels are the centers of layers or the edges of layers.

Values in HARP are always provided at the *center* of an interval.
In HARP, levels are *always* the center point of layers and values are *always* provided at levels (never at layer edges).
If you have data that is provided at the edges of layers then you will have to redefine the layer grid when bringing
the data into HARP such that edges become centers (and centers become edges).

When using intervals, the axis variables should always indicate the center of these intervals.
A separate ``<axis_variable>_bounds`` variable can be used to define the edges of the intervals.
This bounds variable has one extra dimension, which is an independent dimension of length 2.
This independent dimension should contain the edge values of the interval.
The ordering of these values needs to be exactly the same as the ordering of the axis itself
(i.e. if the values of the axis are ascending, then the values defining the interval edges should also be ascending).

Intervals are usually connected. So, if for instance we have an altitude grid of center points ``latitude = [5, 10, 15, 20]``
then the corresponding intervals could be ``latitude_bounds = [[2.5, 7.5], [7.5, 12.5], [12.5, 17.5], [17.5, 22.5]]``.

There are two special cases for intervals, which are described below.

Time intervals
~~~~~~~~~~~~~~
One special case is the time dimension.
In addition to a ``datetime_bounds`` variable, HARP supports separate ``datetime_start`` and ``datetime_stop`` variables
to define the edges of a time interval.
Be aware that in HARP the ``datetime`` variable always needs to represent the center of a time interval.

If you provide two out of ``datetime``, ``datetime_start``, ``datetime_stop``, ``datetime_length`` then you can use
the HARP derivation operations to automatically have the other two variables calculated.
In addition any of these two can be converted into a ``datetime_bounds`` variable and vice-versa.

Spatial extent
~~~~~~~~~~~~~~
The other special case is the spatial dimensions.

When data is gridded spatially using a ``latitude`` and ``longitude`` dimension then the normal rules apply and you
would have a ``latitude_bounds {latitude,2}`` and ``longitude_bounds {longitude,2}`` to define the edges of the grid cells.

However, when ``latitude`` and ``longitude`` are not explicit dimensions, which is the case when e.g. you have a time
series of areas, then ``latitude_bounds`` and ``longitude_bounds`` can still be used, but with a different convention.

- if the independent dimension in ``latitude_bounds {time,independent}`` and ``longitude_bounds {time,independent}``
  has a length of 2 then the two values define the corner of a bounding rectangle of a spatial area.
- if the independent dimension has length 3 or higher, then the points define the vertices of a polygon that defines the spatial area.

A bounding rectangle can always be represented by a 4 point polygon with the exact same meaning.
For instance, ``latitude_bounds = [[3.3, 7.1]]`` and ``longitude_bounds = [[50.8, 53.6]]`` as a bounding rectangle definition
is the same as ``latitude_bounds = [[3.3, 3.3, 7.1, 7.1]]`` and ``longitude_bounds = [[50.8, 53.6, 53.6, 50.8]]``
in terms of a polygon definition (mind the counter clockwise ordering of the polygon points).
