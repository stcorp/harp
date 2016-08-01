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
endif(DF_LIBRARY)

find_library(MFHDF_LIBRARY NAMES mfhdf libmfhdf PATHS ${HDF4_LIBRARY_DIR})
if(MFHDF_LIBRARY)
  set(CMAKE_REQUIRED_LIBRARIES ${DF_LIBRARY} ${ZLIB_LIBRARIES} ${JPEG_LIBRARIES} ${SZIP_LIBRARIES})
  check_library_exists(${MFHDF_LIBRARY} SDstart "" HAVE_MFHDF_LIBRARY)
endif(MFHDF_LIBRARY)

if(HAVE_DF_LIBRARY AND HAVE_MFHDF_LIBRARY)
  set(HDF4_LIBRARIES ${MFHDF_LIBRARY} ${DF_LIBRARY} ${ZLIB_LIBRARIES} ${JPEG_LIBRARIES} ${SZIP_LIBRARIES})
endif(HAVE_DF_LIBRARY AND HAVE_MFHDF_LIBRARY)

if(WIN32 AND HAVE_DF_LIBRARY AND HAVE_MFHDF_LIBRARY)
get_filename_component(DF_LIBRARY_NAME ${DF_LIBRARY} NAME_WE)
find_file(DF_DLL NAMES ${DF_LIBRARY_NAME}.dll PATHS ${HDF4_LIBRARY_DIR} ${HDF4_LIBRARY_DIR}/../bin)
get_filename_component(MFHDF_LIBRARY_NAME ${MFHDF_LIBRARY} NAME_WE)
find_file(MFHDF_DLL NAMES ${MFHDF_LIBRARY_NAME}.dll PATHS ${HDF4_LIBRARY_DIR} ${HDF4_LIBRARY_DIR}/../bin)
find_file(XDR_DLL NAMES xdr.dll PATHS ${HDF4_LIBRARY_DIR} ${HDF4_LIBRARY_DIR}/../bin)
if(DF_DLL AND MFHDF_DLL)
set(HDF4_DLLS ${DF_DLL} ${MFHDF_DLL} ${XDR_DLL} ${ZLIB_DLLS} ${JPEG_DLLS} ${SZIP_DLLS})
endif(DF_DLL AND MFHDF_DLL)
endif(WIN32 AND HAVE_DF_LIBRARY AND HAVE_MFHDF_LIBRARY)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(HDF4 DEFAULT_MSG HAVE_DF_LIBRARY HAVE_MFHDF_LIBRARY HAVE_HDF_H HAVE_MFHDF_H)
