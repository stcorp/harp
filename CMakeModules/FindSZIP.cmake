# Find the SZIP library
#
# This module defines
# SZIP_INCLUDE_DIR, where to find szip.h
# SZIP_LIBRARIES, the libraries to link against to use SZIP.
# SZIP_FOUND, If false, do not try to use SZIP
#
# The user may specify SZIP_INCLUDE_DIR and SZIP_LIBRARY_DIR variables
# to locate include and library files
#
include(CheckLibraryExists)
include(CheckIncludeFile)

set(SZIP_INCLUDE_DIR CACHE STRING "Location of SZIP include files")
set(SZIP_LIBRARY_DIR CACHE STRING "Location of SZIP library files")

if(SZIP_INCLUDE_DIR)
  set(CMAKE_REQUIRED_INCLUDES ${SZIP_INCLUDE_DIR})
endif(SZIP_INCLUDE_DIR)

check_include_file(szlib.h HAVE_SZIP_H)

find_library(SZIP_LIBRARY NAMES sz szip libszip PATHS ${SZIP_LIBRARY_DIR})
if(SZIP_LIBRARY)
  check_library_exists(${SZIP_LIBRARY} SZ_Compress "" HAVE_SZIP_LIBRARY)
endif(SZIP_LIBRARY)
if(HAVE_SZIP_LIBRARY)
  set(SZIP_LIBRARIES ${SZIP_LIBRARY})
endif(HAVE_SZIP_LIBRARY)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SZIP DEFAULT_MSG HAVE_SZIP_LIBRARY HAVE_SZIP_H)
