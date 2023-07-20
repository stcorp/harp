Regridding operations
=====================

Introduction
------------

HARP makes a distinction between 'regridding' and 'binning'. With regridding you use interpolation to find values at new
grid points by using a weighted average based on distance between the values at neighbouring grid points.
With binning you consider intervals and assign each interval a value based on the (weighted) average of all values that intersect with that interval.

Note that regridding and binning is closely linked to the :doc:`HARP convention regarding axis variables <../conventions/axis_variables>`.

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
quantity in the given dimension. These are, for example, partial column density profiles for the vertical dimension.
Other such vertical variables are column averaging kernels and degree-of-freedom profiles. Interval interpolation
requires boundary values for the source and target grid: :math:`x^{B}_{s}(i,l)` and :math:`x^{B}_{t}(j,l)` with
:math:`l=1..2`. The interval interpolation is then performed as follows:

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
           \sum_{i}{w(i,j)y_{s}(i)}, & \sum_{i}{w(i,j)} > 0 \\
           \mathit{nan}, & \sum_{i}{w(i,j)} = 0
         \end{cases} \\
      \end{eqnarray}


Binning
-------


Spatial binning
---------------

