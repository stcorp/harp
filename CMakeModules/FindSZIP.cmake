# Find the SZIP library
#
# This module defines
# SZIP_INCLUDE_DIR, where to find szip.h
# SZIP_LIBRARIES, the libraries to link against to use SZIP.
# SZIP_FOUND, If false, do not try to use SZIP
#
# The user may specify SZIP_INCLUDE and SZIP_LIB environment
# variables to locate include files and library directories
#
include(CheckLibraryExists)
include(CheckIncludeFile)

find_path(SZIP_INCLUDE_DIR
  NAMES szlib.h 
  PATHS ${SZIP_INCLUDE} ENV SZIP_INCLUDE)

set(SZIP_NAMES sz szlib)
find_library(SZIP_LIBRARY NAMES 
  NAMES ${SZIP_NAMES}
  PATHS ${SZIP_LIB} ENV SZIP_LIB)
if (SZIP_LIBRARY)
  CHECK_LIBRARY_EXISTS(${SZIP_LIBRARY} SZ_Compress "" HAVE_SZIP)
  if (HAVE_SZIP)
    set(SZIP_LIBRARIES ${SZIP_LIBRARY})
  endif(HAVE_SZIP)
endif (SZIP_LIBRARY)

# handle the QUIETLY and REQUIRED arguments and set SZIP_FOUND to
# TRUE if all listed variables are TRUE
#
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SZIP DEFAULT_MSG SZIP_LIBRARIES SZIP_INCLUDE_DIR)
mark_as_advanced(SZIP_LIBRARIES SZIP_INCLUDE_DIR)
