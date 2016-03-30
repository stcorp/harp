# Find the CODA library
#
# This module defines
# CODA_INCLUDE_DIR, where to find coda.h
# CODA_LIBRARIES, the coda libraries to link against to use CODA.
# CODA_FOUND, If false, do not try to use CODA.
#
# The user may specify CODA_INCLUDE and CODA_LIB environment
# variables to locate include files and library
#
include(CheckLibraryExists)
include(CheckIncludeFile)

if (NOT CODA_INCLUDE)
  if ($ENV{CODA_INCLUDE} MATCHES ".+")
    file(TO_CMAKE_PATH $ENV{CODA_INCLUDE} CODA_INCLUDE)
    message(STATUS "Using CODA_INCLUDE environment variable: ${CODA_INCLUDE}")
  endif ($ENV{CODA_INCLUDE} MATCHES ".+")
endif (NOT CODA_INCLUDE)
set(CODA_INCLUDE ${CODA_INCLUDE} CACHE STRING "Location of CODA include files" FORCE)
set(CMAKE_REQUIRED_INCLUDES ${CODA_INCLUDE})

if (NOT CODA_LIB)
  if ($ENV{CODA_LIB} MATCHES ".+")
    file(TO_CMAKE_PATH $ENV{CODA_LIB} CODA_LIB)
    message(STATUS "Using CODA_LIB environment variable: ${CODA_LIB}")
  endif ($ENV{CODA_LIB} MATCHES ".+")
endif (NOT CODA_LIB)
set(CODA_LIB ${CODA_LIB} CACHE STRING "Location of CODA library" FORCE)

check_include_file(coda.h HAVE_CODA_H)

if (HAVE_CODA_H)
  set(CODA_INCLUDE_DIR ${CODA_INCLUDE} CACHE STRING "Location of CODA header file(s)")
endif (HAVE_CODA_H)

find_library(CODA_LIBRARY
  NAMES coda
  PATHS ${CODA_LIB} ENV CODA_LIB)
if (CODA_LIBRARY)
  check_library_exists(${CODA_LIBRARY} coda_init "" HAVE_CODA)
  if (HAVE_CODA)
    set(CODA_LIBRARIES ${CODA_LIBRARY})
  endif(HAVE_CODA)
endif (CODA_LIBRARY)

# handle the QUIETLY and REQUIRED arguments and set CODA_FOUND to
# TRUE if all listed variables are TRUE
#
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(CODA DEFAULT_MSG CODA_LIBRARIES CODA_INCLUDE_DIR)
mark_as_advanced(CODA_LIBRARIES CODA_INCLUDE_DIR)
