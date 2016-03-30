# Find the Zlib library
#
# This module defines
# ZLIB_INCLUDE_DIR, where to find zlib.h
# ZLIB_LIBRARIES, the libraries to link against to use Zlib.
# ZLIB_FOUND, If false, do not try to use Zlib
#
# The user may specify ZLIB_INCLUDE and ZLIB_LIB environment
# variables to locate include files and library directories
#
include(CheckLibraryExists)
include(CheckIncludeFile)

find_path(ZLIB_INCLUDE_DIR
  NAMES zlib.h
  PATHS ${ZLIB_INCLUDE} ENV ZLIB_INCLUDE)

set(ZLIB_NAMES z zlib1 zlib zdll)
find_library(ZLIB_LIBRARY
  NAMES ${ZLIB_NAMES}
  PATHS ${ZLIB_LIB} ENV ZLIB_LIB)
if (ZLIB_LIBRARY)
  check_library_exists(${ZLIB_LIBRARY} deflate "" HAVE_ZLIB)
  if (HAVE_ZLIB)
    set(ZLIB_LIBRARIES ${ZLIB_LIBRARY})
  endif(HAVE_ZLIB)
endif (ZLIB_LIBRARY)

# handle the QUIETLY and REQUIRED arguments and set ZLIB_FOUND to TRUE if
# all listed variables are TRUE
#
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ZLIB DEFAULT_MSG ZLIB_LIBRARIES ZLIB_INCLUDE_DIR)
mark_as_advanced(ZLIB_LIBRARY ZLIB_INCLUDE_DIR)