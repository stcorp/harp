# Find the R installation
#
# This module defines
# R_EXECUTABLE, the R executable
# R_INCLUDE_DIR, where to find R.h
# R_LIBRARIES, the R libraries to link against to use R.
# R_FOUND, If false, do not try to use R.
#
# The user may specify the R_EXECUTABLE variable to locate
# the R executable
#
include(CheckLibraryExists)
include(CheckIncludeFile)

if(NOT R_EXECUTABLE)
  find_program(R_EXECUTABLE NAMES R)
endif(NOT R_EXECUTABLE)

if(R_EXECUTABLE)
  execute_process(COMMAND ${R_EXECUTABLE} RHOME OUTPUT_VARIABLE RHOME)
  string(STRIP ${RHOME} RHOME)
  set(R_INCLUDE_DIR "${RHOME}/include")
  set(R_LIBRARY_DIR "${RHOME}/lib")
  set(CMAKE_REQUIRED_INCLUDES ${R_INCLUDE_DIR})
endif(R_EXECUTABLE)

check_include_file(R.h HAVE_R_H)

find_library(R_LIBRARY NAMES R libR PATHS ${R_LIBRARY_DIR})
if(R_LIBRARY)
  check_library_exists(${R_LIBRARY} Rprintf "" HAVE_R_LIBRARY)
endif(R_LIBRARY)
if(HAVE_R_LIBRARY)
  set(R_LIBRARIES ${R_LIBRARY})
endif(HAVE_R_LIBRARY)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(R DEFAULT_MSG R_EXECUTABLE HAVE_R_LIBRARY HAVE_R_H)
