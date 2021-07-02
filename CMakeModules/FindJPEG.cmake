# Find the Jpeg library
#
# This module defines
# JPEG_INCLUDE_DIR, where to find jpeglib.h
# JPEG_LIBRARIES, the libraries to link against to use JPEG.
# JPEG_FOUND, If false, do not try to use JPEG
#
# The user may specify JPEG_INCLUDE_DIR and JPEG_LIBRARY_DIR variables
# to locate include and library files
#
include(CheckLibraryExists)
include(CheckIncludeFiles)

set(JPEG_INCLUDE_DIR CACHE STRING "Location of JPEG include files")
set(JPEG_LIBRARY_DIR CACHE STRING "Location of JPEG library files")

if(JPEG_INCLUDE_DIR)
  set(CMAKE_REQUIRED_INCLUDES ${JPEG_INCLUDE_DIR})
endif(JPEG_INCLUDE_DIR)

check_include_files("stdio.h;jpeglib.h" HAVE_JPEGLIB_H)

find_library(JPEG_LIBRARY NAMES jpeg libjpeg PATHS ${JPEG_LIBRARY_DIR})
if(JPEG_LIBRARY)
  check_library_exists(${JPEG_LIBRARY} jpeg_start_compress "" HAVE_JPEG_LIBRARY)
endif(JPEG_LIBRARY)
if(HAVE_JPEG_LIBRARY)
  set(JPEG_LIBRARIES ${JPEG_LIBRARY})
endif(HAVE_JPEG_LIBRARY)

if(WIN32 AND HAVE_JPEG_LIBRARY)
get_filename_component(JPEG_LIBRARY_NAME ${JPEG_LIBRARY} NAME_WE)
find_file(JPEG_DLL NAMES ${JPEG_LIBRARY_NAME}.dll PATHS ${JPEG_LIBRARY_DIR} ${JPEG_LIBRARY_DIR}/../bin)
if(JPEG_DLL)
set(JPEG_DLLS ${JPEG_DLL})
endif(JPEG_DLL)
endif(WIN32 AND HAVE_JPEG_LIBRARY)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(JPEG DEFAULT_MSG HAVE_JPEG_LIBRARY HAVE_JPEGLIB_H)
