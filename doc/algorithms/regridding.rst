Regridding operations
=====================

Introduction
------------

HARP makes a distinction between 'regridding' and 'rebinning'. With regridding you use interpolation to find values at
new grid points by using a weighted average based on distance between the values at neighbouring grid points.
With rebinning you consider intervals and assign each interval a value based on the (weighted) average of all values that intersect with that interval.

Note that regridding and rebinning is closely linked to the :doc:`HARP convention regarding axis variables <../conventions/axis_variables>`.

There are also special operations called 'binning' and 'spatial binning'.
With 'binning' all values in the time dimension are averaged together based on a time-dependent reference variable that defines the bin values.
With 'spatial binning' a product that does not depend on the 'latitude' and 'longitude' dimensions is gridded to a
specified lat/lon grid.


.. _regridding:

Regridding
----------

For regridding we define the source grid as :math:`x_{s}(i)` with :math:`i=1..N` and the target grid as
:math:`x_{t}(j)` with :math:`j=1..M`.
For each target grid point :math:`x_{t}(j)` the nearest points below and above that point in the source grid are
located (:math:`x_{s}(i)` and :math:`x_{s}(i+1)`) and then an interpolation is performed to convert the values.

If a target grid point lies outside the source grid then the behaviour depends on the `regrid_out_of_bounds` option
that can be set using the :ref:`set() operation <operation_set>`.

Both the source and target grid variables need to be strict monotic (either ascending or descending) for the
regridding to work.

Variables :math:`y_{s}(i)` that depend on the grid dimension are regridded to a target version :math:`y_{t}(j)` as
follows:

   .. math::
      :nowrap:

      \begin{eqnarray}
        y_{t}(j) & = & \begin{cases}
          y_{s}(0) + \frac{x_{t}(j) - x_{s}(0)}{x_{s}(0) - x_{s}(1)}\left(y_{s}(0) - y_{s}(1)\right), &
            x_{t}(j) < x_{s}(0) \wedge \textrm{regrid_out_of_bounds=extrapolate} \\
          y_{s}(0), & x_{t}(j) < x_{s}(0) \wedge \textrm{regrid_out_of_bounds=edge} \\
          \mathit{nan}, & x_{t}(j) < x_{s}(0) \wedge \textrm{regrid_out_of_bounds=nan} \\
          \left(1 - \frac{x_{t}(j) - x_{s}(i)}{x_{s}(i+1) - x_{s}(i)}\right)y_{s}(i) +
            \frac{x_{t}(j) - x_{s}(i)}{x_{s}(i+1) - x_{s}(i)}y_{s}(i+1) , &
             x_{s}(i) < x_{t}(j) < x_{s}(i+1) \\
          y_{s}(i), & x_{t}(j) = x_{s}(i) \\
          y_{s}(N) + \frac{x_{t}(j) - x_{s}(N)}{x_{s}(N) - x_{s}(N-1)}\left(y_{s}(N) - y_{s}(N-1)\right), &
            x_{t}(j) > x_{s}(N) \wedge \textrm{regrid_out_of_bounds=extrapolate} \\
          y_{s}(N), & x_{t}(j) > x_{s}(N) \wedge \textrm{regrid_out_of_bounds=edge} \\
          \mathit{nan}, & x_{t}(j) > x_{s}(N) \wedge \textrm{regrid_out_of_bounds=nan}
        \end{cases} \\
      \end{eqnarray}

Variables will be left unmodified if they do not depend on the to be regridded dimension.

Variables will be removed if they:

  - depend more than once on the given dimension (e.g. 2D averaging kernels when using the vertical dimension).

  - do not have a unit attribute (note that variables with an empty unit or unit=1 are considered to have a unit
    attribute).

  - have a 'string' data type.

  - are an uncertainty variable.

  - are a bounds axis variable for the given dimension

In most cases the axis variable is directly mapped to :math:`x` and the variables that will be regridded to :math:`y`.
The special cases are:

  - when regridding in the vertical dimension and the axis variable is `pressure` then :math:`x` is set to the
    logarithm of the pressure.

  - when regridding in the spectral dimension and the variable to be regridded is an `aerosol_optical_depth` or
    `aerosol_extinction_coefficient` variable then a log/log interpolation is performed. This means that the logarithm
    of the axis variable and the logarithm of the to be regridded variable is used for the interpolation.

A special version of interpolation, called interval interpolation is used for variables that provide an integrated
quantity in the given dimension. The algorithm for this is the same algorithm as for rebinning of integrated quantities as described in the next section.


.. _rebinning:

Rebinning
---------

Rebinning uses a weighted average of the overlapping intervals of the source grid with the interval of the target grid.
Each interval is defined by its upper and lower boundary. We define the source interval grid as :math:`x^{B}_{s}(i,l)`
with :math:`i=1..N` and :math:`l=1..2`, and the target interval grid as :math:`x^{B}_{t}(j,l)` with :math:`j=1..M` and
:math:`l=1..2`.

The rebinning is then performed as follows:

   .. math::
      :nowrap:

      \begin{eqnarray}
        x^{min}_{s}(i) & = & \min_{l}{x^{B}_{s}(i,l)} \\
        x^{max}_{s}(i) & = & \max_{l}{x^{B}_{s}(i,l)} \\
        x^{min}_{t}(j) & = & \min_{l}{x^{B}_{t}(j,l)} \\
        x^{max}_{t}(j) & = & \max_{l}{x^{B}_{t}(j,l)} \\
        w(i,j) & = & \frac{\max(\min(x^{max}_{s}(i), x^{max}_{t}(j)) - \max(x^{min}_{s}(i), x^{min}_{t}(j)), 0)}
                          {x^{max}_{s}(i) - x^{min}_{s}(i)} \\
        y_{t}(j) & = & \begin{cases}
          \frac{\sum_{i}{w(i,j)y_{s}(i)}}{\sum_{i}{w(i,j)}}, & \sum_{i}{w(i,j)} > 0 \\
          \mathit{nan}, & \sum_{i}{w(i,j)} = 0
        \end{cases} \\
      \end{eqnarray}


For variables that provide an integrated quantity in the given dimension, the end result is the sum of the weighted
contributions instead of the average. Such variables are, for example, partial column density profiles for the vertical
dimension. Other such vertical variables are column averaging kernels and degree-of-freedom profiles.

The rebinning operation for integrated variables uses the following revised calculation of :math:`y_{t}(j)`:

   .. math::
      :nowrap:

      \begin{eqnarray}
        y_{t}(j) & = & \begin{cases}
          \sum_{i}{w(i,j)y_{s}(i)}, & \sum_{i}{w(i,j)} > 0 \\
          \mathit{nan}, & \sum_{i}{w(i,j)} = 0
        \end{cases} \\
      \end{eqnarray}

If the product already contained any `count` or `weight` variables, then these are removed before a rebinning is performed.

.. _binning:

Binning
-------

Binning uses a variable that depends on the time dimension to define the bins. For each variable, all elements that
have the same value for the associated element in the binning variable are then averaged into a final value for the bin.

Even though the bins will be represented by the time dimension, this resulting time dimension may not end up in any
chronological order. This all depends on the variable that is used for the bins.

The algorithm for binning is somewhat similar to that of rebinning, except that each interval is represented by a
single value. The binning variable is defined as :math:`x_{s}(i)` with :math:`i=1..N` and the target bins as
:math:`x_{t}(j)` with :math:`j=1..M`. The value :math:`M` represents the number of unique values in :math:`x_{s}(i)`.

The value :math:`y_{t}(j)` for each bin :math:`j` is then determined using:

   .. math::
      :nowrap:

      \begin{eqnarray}
        x_{t}(j) & = & x_{s}(\arg \min_{i}{x_{s}(i) \ne x_{t}(k) \forall k < j}) \\
        N_{j} & = & \sum_{i}{\begin{cases}
            w(i), & x_{s}(i) = x_{s}(j) \\
            0, & x_{s}(i) \ne x_{s}(j) \\
          \end{cases}} \\
        y_{t}(j) & = & \begin{cases}
          \frac{
            \sum_{i}{\begin{cases}
              w(i)y_{s}(i), & x_{s}(i) = x_{s}(j) \\
              0, & x_{s}(i) \ne x_{s}(j) \\
            \end{cases}}
          }{N_{j}}, & N_{j} > 0 \\
          \mathit{nan}, & N_{j} = 0
        \end{cases}
      \end{eqnarray}

The weight :math:`w(i)` is taken from an existing `weight` variable if it exists, otherwise from an existing `count`
variable if it exists, and set to 1 if there was no existing `weight` or `count` variable.

In most cases, each variable is directly mapped to :math:`y`. The special cases are:

  - random uncertainty variables are averaged using the square of each value. The final value is given by:
    :math:`y_{t}(j) = \frac{\sqrt{\sum_{i,x_{s}(i) = x_{s}(j)}{w(i)^{2}y_{s}(i)^{2}}}}{N_{j}}`.

  - total uncertainty variables are averaged based on the `propagate_uncertainty` option that can be set using the
    :ref:`set() operation <operation_set>`. If it is set to `uncorrelated` then the variable is averaged as a random
    uncertainty variable (using its square), otherwise a regular average is taken.

  - variables that define an angle (such as `latitude`, `longitude`, `angle` and `direction`) are averaged using their
    unit vector representation (:math:`\textbf{y}_{s} = (\textrm{cos}(y_{s}) , \textrm{sin}(y_{s}))`. The final average
    is converted back into an angle using :math:`\textrm{atan2}(\textbf{y}_{t})`. The norm :math:`\|\textbf{y}_{t}\|`
    is stored as a weight variable.

.. _spatial_binning:

Spatial binning
---------------

Spatial binning grids the data to a rectilinear lat/lon grid. Depending on what latitude/longitude variables are
available the gridding either uses a point average or an area weighted average.

If the product contains `latitude_bounds` and `longitude_bounds` variables (that only depend on the `time` dimension)
then an area weighted average is performed. Otherwise, if the product contains `latitude` and `longitude` variables
(that only depend on the `time` dimension) then a point average is performed.

Spatial binning can only be performed on a product that does not already depend on the `latitude` and `longitude`
dimensions. Regridding an existing lat/lon grid can be done by individually :ref:`rebinning <rebinning>` the
existing `latitude` and `longitude` dimensions.

If the product already contained `count` or `weight` variables, then these are removed before the spatial binning is
performed.

The target grid is defined by the lat/lon positions of the cell edge corners. This edge grid is represented as
:math:`\phi^{E}_{t}(j)` with :math:`j=1..(M_{\phi}+1)` for latitude and :math:`\lambda^{E}_{t}(k)` with
:math:`j=k..(M_{\lambda}+1)` for longitude.

In the resulting HARP product the edge grid is stored as `latitude_bounds` and `longitude_bounds` variables
:math:`\phi^{B}_{t}(j,l)` and :math:`\lambda^{B}_{t}(k,l)` with :math:`j=1..M_{\phi}`, :math:`k=1..M_{\lambda}`, and
:math:`l=1..2` using the relation:

   .. math::
      :nowrap:

      \begin{eqnarray}
        \phi^{B}_{t}(j,1) & = & \phi^{E}_{t}(j) \\
        \phi^{B}_{t}(j,2) & = & \phi^{E}_{t}(j + 1) \\
        \lambda^{B}_{t}(k,1) & = & \lambda^{E}_{t}(k) \\
        \lambda^{B}_{t}(k,2) & = & \lambda^{E}_{t}(k + 1) \\
      \end{eqnarray}

The spatial binning maps each source variable :math:`y_{s}(i)` with :math:`i=1..N_{t}` to a gridded target variable
:math:`y_{t}(j,k)`. Each target grid cell is represented by :math:`(j,k)` with :math:`j=1..M_{\phi}` and :math:`k=1..M_{\lambda}` providing the latitude and longitude indices within the spatial grid.

The source coordinates can be :math:`\phi_{s}(i)` and :math:`\lambda_{s}(i)` for latitude and longitude in case of
points, and :math:`\phi^{B}_{s}(i,l)` and :math:`\lambda^{B}_{s}(i,l)` for the latitude and longitude boundaries in
case of areas (with :math:`l=1..N_{V}` being the number of vertices for the area polygon).

The weight :math:`w(i,j,k)` determines the contribution of the point/polygon :math:`i` to the target grid cell
:math:`(j,k)`.

In case of point averages each weight is determined by:

   .. math::
      :nowrap:

      \begin{eqnarray}
        w(i,j,k) & = & \sum_{i}{\begin{cases}
          1, & \left( \phi^{E}_{t}(j) \le \phi_{s}(i) < \phi^{E}_{t}(j+1) \vee
              \phi_{s}(i) = \phi^{E}_{t}(M_{\phi}+1) \right) \wedge
            \left( \lambda^{E}_{t}(k) \le \lambda_{s}(i) < \lambda^{E}_{t}(k+1) \vee
              \lambda_{s}(i) = \lambda^{E}_{t}(M_{\lambda}+1) \right) \wedge
            x_{s}(i) \ne \mathit{nan} \\
          0, \textrm{otherwise} \\
        \end{cases}}
      \end{eqnarray}


In case of area weighted averages we consider :math:`\textbf{P}_{t}(j,k)` as the polygon that represents the target
grid cell at position :math:`(j,k)`, and :math:`\textbf{P}_{s}(i)` as the polygon that is defined by the boundary
coordinates :math:`\phi^{B}_{s}(i,l)` and :math:`\lambda^{B}_{s}(i,l)`. The weights are then determined using:

   .. math::
      :nowrap:

      \begin{eqnarray}
        w(i,j,k) & = & \sum_{i}{\begin{cases}
          \frac{\textrm{area}(\textbf{P}_{t}(j,k) \wedge \textbf{P}_{s}(i))}{\textrm{area}(\textbf{P}_{t}(j,k))}, &
            x_{s}(i) \ne \mathit{nan} \\
          0, & x_{s}(i) = \mathit{nan} \\
        \end{cases}}
      \end{eqnarray}

The algorithms for the polygon area calculation :math:`\textrm{area}(\textbf{P})` and polygon intersection :math:`\textbf{P}_{a} \wedge \textbf{P}_{b}` are those for polygons in a 2D Cartesian plane (i.e. these calculations
are not performed using polygons on a sphere).

With the calculated weights each variable is then regridded using:

   .. math::
      :nowrap:

      \begin{eqnarray}
        y_{t}(j,k) & = & \frac{\sum_{i}{w(i,j,k)y_{s}(i)}}{\sum_{i}{w(i,j,k)}}
      \end{eqnarray}

In most cases, each variable is directly mapped to :math:`y`. The special cases are:

  - random uncertainty variables are averaged using the square of each value. The final value is given by:
    :math:`y_{t}(j,k) = \frac{\sqrt{\sum_{i}{\left(w(i,j,k)y_{s}(i)\right)^{2}}}}{\sum_{i}{w(i,j,k)}}`.

  - total uncertainty variables are always averaged as correlated (i.e. using a regular average).

  - variables that define an angle (such as `latitude`, `longitude`, `angle` and `direction`) are averaged using their
    unit vector representation (:math:`\textbf{y}_{s} = (\textrm{cos}(y_{s}) , \textrm{sin}(y_{s}))`. The final average
    is converted back into an angle using :math:`\textrm{atan2}(\textbf{y}_{t})`. The norm :math:`\|\textbf{y}_{t}\|`
    is stored as the weight for this variable.
