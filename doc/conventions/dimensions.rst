Dimensions
==========

HARP has strict rules regarding the dimensions of variables. Each dimension of a variable may be used to represent a
physical dimension (such as time, latitude, longitude, height, etcetera), or it may be used as an independent dimension.

Only dimension types supported by HARP can be used. These types are:

``time``
  Temporal dimension; this is also the only appendable dimension.

``vertical``
  Vertical dimension, indicating height or depth.

``spectral``
  Spectral dimension, associated with wavelength, wavenumber, or frequency.

``latitude``
  Latitude dimension, only to be used for the latitude axis of a regular latitude x longitude grid.

``longitude``
  Longitude dimension, only to be used for the longitude axis of a regular latitude x longitude grid.

``independent``
  Independent dimension, used to index other quantities, such as the corner coordinates of ground pixel polygons.

Within a HARP product, all dimensions of the same type should have the same length, *except* independent dimensions. For
example, it is an error to have two variables within the same product that both have a time dimension, yet of a
different length.

A variable with more than one dimension has to use a fixed ordering of the dimensions. In the HARP documentation the
ordering is always documented using the so-called 'C convention' for dimension ordering. Using the C convention, the
last dimension (writing from left to right) is the fastest running dimension when enumerating all elements, compared to
the Fortran convention, where the first dimension is the fastest running dimension. Note that different file access
libraries may have different conventions with regard to how they deal with array ordering in their function interfaces.

The order in which dimensions need to be provided for a variable is defined by the following rules:

 - If present, the ``time`` dimension is always the first (i.e. slowest running) dimension.
 - Next are categorical dimensions used for grouping. For instance, this can be the ``spectral`` dimension when it is
   used to distinguish between retrievals performed using different choices of wavelength, or to distinguish data from
   different spectral bands.
 - Next are the spatial dimensions, ordered as ``latitude``, ``longitude``, ``vertical``.
 - Next is the ``spectral`` dimension when it is used as an actual axis (e.g. for L1 spectral data for instruments that
   measure along a spectral axis).
 - Any independent dimensions come last (i.e. they will always be the fastest running dimensions).

So, for a spectral axis used for grouping, the ordering should be:

   ``time``, ``spectral``, ``latitude``, ``longitude``, ``vertical``, ``independent``

And, for a spectral axis used for L1 data from spectral instruments, the ordering should be:

   ``time``, ``latitude``, ``longitude``, ``vertical``, ``spectral``, ``independent``

A variable should only use dimensions on which it is dependent. This means that the radiance variable for L1 data of a
nadir looking spectral instrument on a satellite will generally only have the dimensions ``time`` and ``spectral`` (and
not ``latitude``, ``longitude``, ``vertical``, or ``independent``).

Note that only a single grid can be used for each type of dimension per time value. This means that, for example, it is
possible to change the vertical grid from sample to sample, but it is not possible to use different vertical grids
for the *same* sample.

To allow a different vertical grid from sample to sample, the ``altitude`` variable should have dimensions
``{time,vertical}`` (instead of ``{vertical}``). This way, the altitude values for the first sample, ``altitude[0,:]``,
may differ from the altitude values for the second sample, ``altitude[1,:]``, and so on. However, for an averaging
kernel, which has dimensions ``{time,vertical,vertical}``, the altitude values for both vertical dimensions are
necessarily the same for each single sample.

A grid that differs from sample to sample could have a different effective length per sample. This is implemented by
taking the maximum length over all samples as the length of the dimension and padding the dimension for each sample at
the end with fill values (``NaN`` for floating point values, ``0`` for integers, and empty strings for string values).
For instance, you can have an ``altitude{time,vertical}`` variable where ``altitude[0,:]`` has 7 levels and equals
``[0.0, 5.0, 10.0, 15.0, 20.0, 25.0, 30.0]`` and ``altitude[1,:]`` has only 6 levels and equals
``[0.0, 6.0, 12.0, 18.0, 24.0, 30.0, NaN]``.

Operations performed by HARP will determine the effective length of a dimension for each sample by ignoring all trailing
``NaN`` values of the axis variable that is used for the operation (e.g. the ``altitude`` or ``pressure`` variable for a
vertical dimension or the ``wavelength`` or ``wavenumber`` variable for a spectral dimension). Axis variables should
therefore always use floating point values.
