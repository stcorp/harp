# Find the CODA library
#
# This module defines
# CODA_INCLUDE_DIR, where to find coda.h
# CODA_LIBRARIES, the coda libraries to link against to use CODA.
# CODA_FOUND, If false, do not try to use CODA.
#
# The user may specify CODA_INCLUDE_DIR and CODA_LIBRARY_DIR variables
# to locate include and library files
#
include(CheckLibraryExists)
include(CheckIncludeFile)

set(CODA_INCLUDE_DIR CACHE STRING "Location of CODA include files")
set(CODA_LIBRARY_DIR CACHE STRING "Location of CODA library files")

if(CODA_INCLUDE_DIR)
  set(CMAKE_REQUIRED_INCLUDES ${CODA_INCLUDE_DIR})
endif(CODA_INCLUDE_DIR)

check_include_file(coda.h HAVE_CODA_H)

find_library(CODA_LIBRARY NAMES coda libcoda PATHS ${CODA_LIBRARY_DIR})
if(CODA_LIBRARY)
  check_library_exists(${CODA_LIBRARY} coda_init "" HAVE_CODA_LIBRARY)
endif(CODA_LIBRARY)
if(HAVE_CODA_LIBRARY)
  set(CODA_LIBRARIES ${CODA_LIBRARY})
endif(HAVE_CODA_LIBRARY)

if(WIN32 AND HAVE_CODA_LIBRARY)
get_filename_component(CODA_LIBRARY_NAME ${CODA_LIBRARY} NAME_WE)
find_file(CODA_DLL NAMES ${CODA_LIBRARY_NAME}.dll PATHS ${CODA_LIBRARY_DIR} ${CODA_LIBRARY_DIR}/../bin)
set(CODA_DLLS ${CODA_DLL})
endif(WIN32 AND HAVE_CODA_LIBRARY)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(CODA DEFAULT_MSG HAVE_CODA_LIBRARY HAVE_CODA_H)
