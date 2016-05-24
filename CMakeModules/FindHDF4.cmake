# Find the HDF4 library
#
# This module defines
# HDF4_INCLUDE_DIR, where to find hdfi.h, etc.
# HDF4_LIBRARIES, the hdf libraries to link against to use HDF4.
# HDF4_FOUND, If false, do not try to use HDF4.
#
# The user may specify HDF4_INCLUDE_DIR and HDF4_LIBRARY_DIR variables
# to locate include and library files
#
include(CheckLibraryExists)
include(CheckIncludeFile)

set(HDF4_INCLUDE_DIR CACHE STRING "Location of HDF4 include files")
set(HDF4_LIBRARY_DIR CACHE STRING "Location of HDF4 library files")

find_package(JPEG)
find_package(ZLIB)
find_package(SZIP)

if(HDF4_INCLUDE_DIR)
  set(CMAKE_REQUIRED_INCLUDES ${HDF4_INCLUDE_DIR})
endif(HDF4_INCLUDE_DIR)

check_include_file(hdf.h HAVE_HDF_H)
check_include_file(mfhdf.h HAVE_MFHDF_H)

find_library(DF_LIBRARY NAMES hdf libhdf df PATHS ${HDF4_LIBRARY_DIR})
if(DF_LIBRARY)
  set(CMAKE_REQUIRED_LIBRARIES ${ZLIB_LIBRARIES} ${JPEG_LIBRARIES} ${SZIP_LIBRARIES})
  check_library_exists(${DF_LIBRARY} Hopen "" HAVE_DF_LIBRARY)
endif (DF_LIBRARY)

find_library(MFHDF_LIBRARY NAMES mfhdf libmfhdf PATHS ${HDF4_LIBRARY_DIR})
if(MFHDF_LIBRARY)
  set(CMAKE_REQUIRED_LIBRARIES ${DF_LIBRARIES} ${ZLIB_LIBRARIES} ${JPEG_LIBRARIES} ${SZIP_LIBRARIES})
  check_library_exists(${MFHDF_LIBRARY} SDstart "" HAVE_MFHDF_LIBRARY)
endif(MFHDF_LIBRARY)

if(HAVE_DF_LIBRARY AND HAVE_MFHDF_LIBRARY)
  set(HDF4_LIBRARIES ${MFHDF_LIBRARY} ${DF_LIBRARY} ${ZLIB_LIBRARIES} ${JPEG_LIBRARIES} ${SZIP_LIBRARIES})
  if(MSVC)
    set(HDF4_LIBRARIES ${HDF4_LIBRARIES} wsock32)
  endif(MSVC)
endif(HAVE_DF_LIBRARY AND HAVE_MFHDF_LIBRARY)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(HDF4 DEFAULT_MSG HAVE_DF_LIBRARY HAVE_MFHDF_LIBRARY HAVE_HDF_H HAVE_MFHDF_H)
