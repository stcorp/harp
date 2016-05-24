# Find the HDF5 library
#
# This module defines
# HDF5_INCLUDE_DIR, where to find hdf5.h, etc.
# HDF5_LIBRARIES, the hdf5 libraries to link against to use HDF5.
# HDF5_FOUND, If false, do not try to use HDF5.
#
# The user may specify HDF5_INCLUDE_DIR and HDF5_LIBRARY_DIR variables
# to locate include and library files
#
include(CheckLibraryExists)
include(CheckIncludeFile)

set(HDF5_INCLUDE_DIR CACHE STRING "Location of HDF5 include files")
set(HDF5_LIBRARY_DIR CACHE STRING "Location of HDF5 library files")

find_package(ZLIB)
find_package(SZIP)

if(HDF5_INCLUDE_DIR)
  set(CMAKE_REQUIRED_INCLUDES ${HDF5_INCLUDE_DIR})
endif(HDF5_INCLUDE_DIR)

check_include_file(hdf5.h HAVE_HDF5_H)

find_library(HDF5_LIBRARY NAMES hdf5 libhdf5 PATHS ${HDF5_LIBRARY_DIR})
if(HDF5_LIBRARY)
  set(CMAKE_REQUIRED_LIBRARIES ${ZLIB_LIBRARIES} ${SZIP_LIBRARIES})
  check_library_exists(${HDF5_LIBRARY} H5Fopen "" HAVE_HDF5_LIBRARY)
endif(HDF5_LIBRARY)
if(HAVE_HDF5_LIBRARY)
  set(HDF5_LIBRARIES ${HDF5_LIBRARY} ${ZLIB_LIBRARIES} ${SZIP_LIBRARIES})
endif(HAVE_HDF5_LIBRARY)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(HDF5 DEFAULT_MSG HAVE_HDF5_LIBRARY HAVE_HDF5_H)
