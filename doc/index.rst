.. HARP documentation master file, created by
   sphinx-quickstart on Wed Oct 14 15:17:12 2015.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.

HARP Manual
===========

Contents
--------

.. toctree::
   :maxdepth: 2

   install
   dataformats
   ingestions/index
   libharp
   harpcheck
   harpcollocate
   harpconvert
   harpdump
   harpfilter

What is HARP?
-------------

HARP is a toolkit for ingesting, processing and inter-comparing satellite remote sensing data against correlative data. Correlative data can be either in-situ data, ground based remote sensing data or other satellite remote sensing data.
The toolkit is composed of:

- A set of command line tools
- A library of analysis functions

The main goal of HARP is to assist in the inter-comparison of data sets. By appropriately chaining calls to HARP command line tools one can preprocess satellite and correlative data such that the two datasets that need to be compared end up having the same temporal/spatial grid, same data format/structure, and same physical unit. At the end of the toolchain you will have a set of data files that can be directly compared in e.g. Python, IDL or MATLAB.

In order for the HARP command line tools to handle each others output the toolset uses a its own <a href="#dataformats">data format conventions</a> for intermediate files. The use of this harmonized formatting means that satellite products need to be converted first to HARP compliant files. Once in this format, further processing can take place.


Data Formats
------------
In order for HARP command line tools to handle each others output the toolset makes uses of its own <a href="dataformats/index.html">data format conventions</a>. There is one standard for storing measurement data, which only prescribes a specific set of constraints to the data format and is still flexible enough to allow storing of data in either netCDF, HDF4, or HDF5 and include metadata from other standards such as netCDF-CF. The other data format convention is a standard for storing information on collocations, the <a href="dataformats/index.html#crf">Collocation Result File</a>, which is based on csv. Both data formats are described in more detail in the <a href="dataformats/index.html">Data Formats</a> section of the documentation.

Overview of Command Line Tools
------------------------------

harpconvert
    Convert a product from its original format into a HARP compliant HDF4/HDF5/netCDF file that can be processed further with other HARP tools. An overview of supported products can be found in the <a href="ingestions/index.html">HARP Ingestion Definition Overview</a> section.

harpcheck
    Verify that a files conforms to the HARP data format conventions and can be read by the other HARP tools.

harpdump
    Inspect the contents of a HARP compliant HDF4/HDF5/netCDF file.

harpfilter
    Apply several kinds of filtering and conversions on a HARP compliant HDF4/HDF5/netCDF file.

harpcollocate
    Find pairs of measurements that match in time and geolocation for two sets of HARP compliant HDF4/HDF5/netCDF files.


HARP C Library
--------------
Common parts that are used by the various HARP command line tools are gathered in a single <a href="libharp/index.html">HARP C Library</a>.
This library can be used to build your own building blocks.

A Typical Intercomparison
-------------------------
Typical sequence when comparing two datasets of measurements:

harpconvert ...
    Convert all products that do not already follow the HARP format conventions to a common data format.

harpfilter ...
    Filter/convert the data in each file: remove unneeded parameters/measurements, add derived physical parameters, perform unit conversion, etc.

harpcollocate matchup ...
    Find the measurement pairs that match within the specified collocation criteria and save the result into a collocation result file.

harpcollocate resample ...
    Apply resampling to the collocation result file.

harpfilter -a 'collocate-left(<csvfile>)' ...
    Apply the collocation filter for each file in the primary dataset

harpfilter -a 'collocate-right(<csvfile>)' ...
    Apply the collocation filter for each file in the secondary dataset

Input files can be verified with

    harpcheck ...

and after each stage, the contents of each file can be inspected from the command line using:

    harpdump ...

Examples
--------
Examples of executions of a full comparison can be found in the <a href="examples/index.html">Examples</a> section.
