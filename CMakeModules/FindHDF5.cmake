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
check_include_file(hdf5_hl.h HAVE_HDF5_HL_H)

find_library(HDF5_LIBRARY NAMES hdf5 libhdf5 PATHS ${HDF5_LIBRARY_DIR})
if(HDF5_LIBRARY)
  set(CMAKE_REQUIRED_LIBRARIES ${ZLIB_LIBRARIES} ${SZIP_LIBRARIES})
  check_library_exists(${HDF5_LIBRARY} H5Fopen "" HAVE_HDF5_LIBRARY)
endif(HDF5_LIBRARY)

find_library(HDF5_HL_LIBRARY NAMES hdf5_hl libhdf5_hl PATHS ${HDF5_LIBRARY_DIR})
if(HDF5_HL_LIBRARY)
  set(CMAKE_REQUIRED_LIBRARIES ${HDF5_LIBRARY} ${ZLIB_LIBRARIES} ${SZIP_LIBRARIES})
  check_library_exists(${HDF5_HL_LIBRARY} H5DSset_scale "" HAVE_HDF5_HL_LIBRARY)
endif(HDF5_HL_LIBRARY)

if(HAVE_HDF5_LIBRARY AND HAVE_HDF5_HL_LIBRARY)
  set(HDF5_LIBRARIES ${HDF5_HL_LIBRARY} ${HDF5_LIBRARY} ${ZLIB_LIBRARIES} ${SZIP_LIBRARIES})
endif(HAVE_HDF5_LIBRARY AND HAVE_HDF5_HL_LIBRARY)

if(WIN32 AND HAVE_HDF5_LIBRARY AND HAVE_HDF5_HL_LIBRARY)
get_filename_component(HDF5_LIBRARY_NAME ${HDF5_LIBRARY} NAME_WE)
find_file(HDF5_DLL NAMES ${HDF5_LIBRARY_NAME}.dll PATHS ${HDF5_LIBRARY_DIR} ${HDF5_LIBRARY_DIR}/../bin)
get_filename_component(HDF5_HL_LIBRARY_NAME ${HDF5_HL_LIBRARY} NAME_WE)
find_file(HDF5_HL_DLL NAMES ${HDF5_HL_LIBRARY_NAME}.dll PATHS ${HDF5_LIBRARY_DIR} ${HDF5_LIBRARY_DIR}/../bin)
if(HDF5_DLL AND HDF5_HL_DLL)
set(HDF5_DLLS ${HDF5_HL_DLL} ${HDF5_DLL} ${ZLIB_DLLS} ${SZIP_DLLS})
endif(HDF5_DLL AND HDF5_HL_DLL)
endif(WIN32 AND HAVE_HDF5_LIBRARY AND HAVE_HDF5_HL_LIBRARY)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(HDF5 DEFAULT_MSG HAVE_HDF5_LIBRARY HAVE_HDF5_HL_LIBRARY HAVE_HDF5_H HAVE_HDF5_HL_H)
