.. HARP documentation master file, created by
   sphinx-quickstart on Wed Oct 14 15:17:12 2015.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.

HARP manual
===========

Contents
--------

.. toctree::
   :maxdepth: 2

   install
   conventions/index
   algorithms/index
   operations
   ingestions/index
   libharp
   idl
   matlab 
   python
   tools

What is HARP?
-------------

HARP is a toolkit for reading, processing and inter-comparing satellite remote
sensing data, model data, in-situ data, and ground based remote sensing data.
The toolkit is composed of:

- A set of :ref:`command line tools <command-line-tools>`
- A library of analysis functions

The main goal of HARP is to assist in the inter-comparison of data sets. By
appropriately chaining calls to HARP command line tools one can pre-process
data sets such that two datasets that need to be compared end up having the
same temporal/spatial grid, same data format/structure, and same physical unit.
At the end of the toolchain you will have a set of data files that can be
directly compared in e.g. Python, IDL or MATLAB.

In order for the HARP command line tools to handle each others output the
toolkit uses its own :ref:`data format conventions <data-formats>` for
intermediate files. The use of this harmonized formatting means that satellite
products need to be converted first to HARP compliant files. Once in this
format, further processing can be performed.

.. _data-formats:

Data formats
------------

In order for HARP command line tools to handle each others output the toolkit
makes uses of its own :doc:`data format conventions <conventions/index>`.

There is one standard for representing and storing measurement data, which
only prescribes a specific set of constraints to the data format and is still
flexible enough to allow data storage using either netCDF, HDF4, or HDF5 and
inclusion of metadata from other standards such as netCDF-CF.

The other data format convention is a standard for storing information on
collocations, the :doc:`Collocation Result File <conventions/collocation_result>`,
which is based on CSV.

Both data formats are described in more detail in the :doc:`Data formats
<conventions/index>` section of the documentation.

Algorithms
----------

HARP can perform a wide variety of operations on the data. A description of
the algorithms as used in HARP can be found in the :doc:`Algorithms
<algorithms/index>` section of the documentation.

Operations
----------

Several HARP functions and tools can take a sequence of operations, such as
filters or regridding operations, which are then performed on a product.
A description of the operations expressions is provided in the :doc:`Operations
<operations>` section of the documentation.

C library
---------

Common parts that are used by the various HARP command line tools are gathered
in a single :doc:`HARP C library <libharp>`. This library can be used to build
custom applications or libraries that work with HARP compliant data products.

IDL interface
----------------

The :doc:`HARP IDL interface <idl>` provides a set of functions to import
and export HARP products, and to ingest non-HARP products of a type supported by
HARP from IDL.

MATLAB interface
----------------

The :doc:`HARP MATLAB interface <matlab>` provides a set of functions to import
and export HARP products, and to ingest non-HARP products of a type supported by
HARP from MATLAB.

Python interface
----------------

The :doc:`HARP Python interface <python>` provides a set of functions to import
and export HARP products, and to ingest non-HARP products of a type supported by
HARP from Python. The Python interface depends on the ``_cffi_backend`` module,
which is part of the C foreign function interface (cffi) package. This package
must be installed in order to be able to use the Python interface. See the `cffi
documentation`_ for details on how to install the cffi package.

.. _cffi documentation: http://cffi.readthedocs.org/en/latest/installation.html

.. _command-line-tools:

Command line tools
------------------

The HARP command line tools perform various operations on HARP compliant data
products. All command line tools except ``harpconvert`` expect one or more HARP
data products as input. The ``harpconvert`` tool can be used to convert non-HARP
products into HARP products.

The available command line tools are described in more detail in the
:doc:`Command line tools <tools>` section of the documentation.

A typical inter-comparison
--------------------------
Typical sequence when comparing two datasets of measurements:

harpconvert ...
    Convert all products that do not already follow the HARP format conventions
    to a common data format.
    Filter/convert the data in each file: remove unneeded
    parameters/measurements, add derived physical parameters, perform unit
    conversion, regrid dimensions, etc.

harpcollocate matchup ...
    Find the measurement pairs that match within the specified collocation
    criteria and save the result into a collocation result file.

harpcollocate resample ...
    Apply resampling to the collocation result file.

harpconvert -a 'collocate-left(<csvfile>)' ...
    Apply the collocation filter for each file in the primary dataset

harpconvert -a 'collocate-right(<csvfile>)' ...
    Apply the collocation filter for each file in the secondary dataset

Input files can be verified with

    harpcheck ...

and after each stage, the contents of each file can be inspected from the
command line using:

    harpdump ...

