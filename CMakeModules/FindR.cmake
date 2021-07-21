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
include(FindPackageHandleStandardArgs)

if(NOT R_EXECUTABLE)
  set(TEMP_CMAKE_FIND_APPBUNDLE ${CMAKE_FIND_APPBUNDLE})
  set(CMAKE_FIND_APPBUNDLE "NEVER")
  find_program(R_EXECUTABLE NAMES R)
  set(CMAKE_FIND_APPBUNDLE ${TEMP_CMAKE_FIND_APPBUNDLE})
endif(NOT R_EXECUTABLE)

if(R_EXECUTABLE)
  message(STATUS "Found R executable: ${R_EXECUTABLE}")
  execute_process(COMMAND ${R_EXECUTABLE} RHOME OUTPUT_VARIABLE RHOME)
  string(STRIP ${RHOME} RHOME)
  message(STATUS "Found R home: ${RHOME}")
  set(R_INCLUDE_DIR "${RHOME}/include")
  set(R_LIBRARY_DIR "${RHOME}/lib")
  set(CMAKE_REQUIRED_INCLUDES ${R_INCLUDE_DIR})
endif(R_EXECUTABLE)

check_include_file(R.h HAVE_R_H)

if(NOT WIN32)
find_library(R_LIBRARY NAMES R libR PATHS ${R_LIBRARY_DIR} NO_DEFAULT_PATH)
if(R_LIBRARY)
  message(STATUS "Checking R library: ${R_LIBRARY}")
  check_library_exists(${R_LIBRARY} Rprintf "" HAVE_R_LIBRARY)
endif(R_LIBRARY)
if(HAVE_R_LIBRARY)
  message(STATUS "Found R library: ${R_LIBRARY}")
  set(R_LIBRARIES ${R_LIBRARY})
endif(HAVE_R_LIBRARY)
find_package_handle_standard_args(R DEFAULT_MSG R_EXECUTABLE HAVE_R_LIBRARY HAVE_R_H)
else()
# On Windows there is no global R library to link against
find_package_handle_standard_args(R DEFAULT_MSG R_EXECUTABLE HAVE_R_H)
endif()
