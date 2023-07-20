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
contributions instead of the average. Such variables are, for example, partial column density profiles for the vertical dimension. Other such vertical variables are column averaging kernels and degree-of-freedom profiles.

The rebinning operation for integrated variables uses the following revised calculation of :math:`y_{t}(j)`:

   .. math::
      :nowrap:

      \begin{eqnarray}
        y_{t}(j) & = & \begin{cases}
          \sum_{i}{w(i,j)y_{s}(i)}, & \sum_{i}{w(i,j)} > 0 \\
          \mathit{nan}, & \sum_{i}{w(i,j)} = 0
        \end{cases} \\
      \end{eqnarray}


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
        y_{t}(j) & = & \frac{
          \sum_{i}{\begin{cases}
            y_{s}(i), & x_{s}(i) = x_{s}(j) \\
            0, & x_{s}(i) \ne x_{s}(j) \\
          \end{cases}}
        }{
          \sum_{i}{\begin{cases}
            1, & x_{s}(i) = x_{s}(j) \\
            0, & x_{s}(i) \ne x_{s}(j) \\
          \end{cases}}
        }
      \end{eqnarray}


Spatial binning
---------------

