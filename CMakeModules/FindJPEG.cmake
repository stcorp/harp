# Find the Jpeg library
#
# This module defines
# JPEG_INCLUDE_DIR, where to find jpeglib.h
# JPEG_LIBRARIES, the libraries to link against to use JPEG.
# JPEG_FOUND, If false, do not try to use JPEG
#
# The user may specify JPEG_INCLUDE and JPEG_LIB environment
# variables to locate include files and library directories
#
include(CheckLibraryExists)
include(CheckIncludeFile)

find_path(JPEG_INCLUDE_DIR
  NAMES jpeglib.h
  PATHS ${JPEG_INCLUDE} ENV JPEG_INCLUDE)

set(JPEG_NAMES jpeg libjpeg)
find_library(JPEG_LIBRARY
  NAMES ${JPEG_NAMES}
  PATHS ${JPEG_LIB} ENV JPEG_LIB)
if (JPEG_LIBRARY)
  check_library_exists(${JPEG_LIBRARY} jpeg_start_compress "" HAVE_JPEG)
  if (HAVE_JPEG)
    set(JPEG_LIBRARIES ${JPEG_LIBRARY})
  endif(HAVE_JPEG)
endif (JPEG_LIBRARY)


# handle the QUIETLY and REQUIRED arguments and set JPEG_FOUND to
# TRUE if all listed variables are TRUE
#
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(JPEG DEFAULT_MSG JPEG_LIBRARIES JPEG_INCLUDE_DIR)
mark_as_advanced(JPEG_LIBRARY JPEG_INCLUDE_DIR)
