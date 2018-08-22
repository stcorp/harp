# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.


#.rst:
# FindIDL
# ----------
#
# Finds IDL installations and provides IDL executable and library to cmake.
#
# This package first intention is to find the library associated with IDL
# in order to be able to build IDL extensions (DLM files).
#
# The module supports the following components:
#
# * ``IDL_LIBRARY`` the IDL shared library
# * ``MAIN_PROGRAM`` the IDL binary program.
#
# .. note::
#
# The variable :variable:`IDL_ROOT_DIR` may be specified in order to give
# the path of the desired IDL version. Otherwise, the behaviour is platform
# specific:
#
# * Windows: The installed versions of IDL are retrieved from the
#   Windows registry.
# * macOS: The installed versions of IDL are given by the IDL
#   paths in ``/Applications``. If no such application is found, it falls
#   back to the one that might be accessible from the PATH.
# * Unix: The installed versions of IDL are given by the IDL
#   paths in ``/usr/local``. If no such application is found, it falls
#   back to the one that might be accessible from the PATH.
# IDL paths in ``/Applications`` and ``/usr/local`` are located by looking
# for subdirectories ``harris/idl``, ``exelis/idl``, or ``itt/idl``.
#
# Additional information is provided when :variable:`IDL_FIND_DEBUG` is set.
#
# Module Input Variables
# ^^^^^^^^^^^^^^^^^^^^^^
#
# Users or projects may set the following variables to configure the module
# behaviour:
#
# :variable:`IDL_ROOT_DIR`
#   the root of the IDL installation.
# :variable:`IDL_FIND_DEBUG`
#   outputs debug information
#
# Variables defined by the module
# ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
#
# Result variables
# """"""""""""""""
#
# ``IDL_FOUND``
#   ``TRUE`` if the IDL installation is found, ``FALSE``
#   otherwise. All variable below are defined if IDL is found.
# ``IDL_ROOT_DIR``
#   the final root of the IDL installation determined by the FindIDL
#   module.
# ``IDL_INCLUDE_DIRS``
#  the path of the IDL libraries headers
# ``IDL_LIBRARY``
#   library for idl.
# ``IDL_LIBRARIES``
#   the whole set of libraries of IDL
#
# Cached variables
# """"""""""""""""
#
# ``IDL_ROOT_DIR``
#   the location of the root of the IDL installation found. If this value
#   is changed by the user, the result variables are recomputed.
#
# Provided functions
# ^^^^^^^^^^^^^^^^^^
#
# :command:`IDL_extract_all_installed_versions_from_registry`
#   parses the registry for all IDL versions. Available on Windows only.
#   The part of the registry parsed is dependent on the host processor
# :command:`IDL_get_all_valid_IDL_roots_from_registry`
#   returns all the possible IDL paths, according to a previously
#   given list. Only the existing/accessible paths are kept. This is mainly
#   useful for the searching all possible IDL installation.
# :command:`IDL_get_version_from_root`
#   returns the version of IDL, given the IDL root directory.
#
# Reference
# ^^^^^^^^^
#
# .. variable:: IDL_ROOT_DIR
#
#    The root folder of the IDL installation. If set before the call to
#    :command:`find_package`, the module will look for the components in that
#    path. If not set, then an automatic search of IDL
#    will be performed. If set, it should point to a valid version of IDL.
#
# .. variable:: IDL_FIND_DEBUG
#
#    If set, the lookup of IDL and the intermediate configuration steps are
#    outputted to the console.
#

set(_FindIDL_SELF_DIR "${CMAKE_CURRENT_LIST_DIR}")

include(FindPackageHandleStandardArgs)
include(CheckCXXCompilerFlag)
include(CheckCCompilerFlag)


#.rst:
# .. command:: IDL_extract_all_installed_versions_from_registry
#
#   This function parses the registry and finds the IDL versions that are
#   installed. The found versions are returned in `IDL_versions`.
#   Set `win64` to `TRUE` if the 64 bit version of IDL should be looked for
#   The returned list contains all versions under
#   ``HKLM\\SOFTWARE\\ITT\\IDL`` or an empty list in case an error
#   occurred (or nothing found).
#
#   .. note::
#
#     Only the versions are provided. No check is made over the existence of the
#     installation referenced in the registry,
#
function(IDL_extract_all_installed_versions_from_registry win64 IDL_versions)

  if(NOT CMAKE_HOST_WIN32)
    message(FATAL_ERROR "This macro can only be called by a windows host (call to reg.exe")
  endif()


  if(${win64} AND ${CMAKE_HOST_SYSTEM_PROCESSOR} MATCHES "64")
    set(APPEND_REG "/reg:64")
  else()
    set(APPEND_REG "/reg:32")
  endif()

  # /reg:64 should be added on 64 bits capable OSs in order to enable the
  # redirection of 64 bits applications
  execute_process(
    COMMAND reg query HKEY_LOCAL_MACHINE\\SOFTWARE\\ITT\\IDL /f * /k ${APPEND_REG}
    RESULT_VARIABLE resultIDL
    OUTPUT_VARIABLE varIDL
    ERROR_VARIABLE errIDL
    INPUT_FILE NUL
    )

  set(IDLs_from_registry)
  if(${resultIDL} EQUAL 0)

    string(
      REGEX MATCHALL "IDL\\\\([0-9]+(\\.[0-9]+)?)"
      IDL_versions_regex ${varIDL})

    foreach(match IN LISTS IDL_versions_regex)
      string(
        REGEX MATCH "IDL\\\\(([0-9]+)(\\.([0-9]+))?)"
        current_match ${match})

      set(_IDL_current_version ${CMAKE_MATCH_1})
      set(current_IDL_version_major ${CMAKE_MATCH_2})
      set(current_IDL_version_minor ${CMAKE_MATCH_4})
      if(NOT current_IDL_version_minor)
        set(current_IDL_version_minor "0")
      endif()

      list(APPEND IDLs_from_registry ${_IDL_current_version})
      unset(_IDL_current_version)
    endforeach(match)

  endif()

  if(IDLs_from_registry)
    list(REMOVE_DUPLICATES IDLs_from_registry)
    list(SORT IDLs_from_registry)
    list(REVERSE IDLs_from_registry)
  endif()

  set(${IDL_versions} ${IDLs_from_registry} PARENT_SCOPE)

endfunction()

#.rst:
# .. command:: IDL_get_all_valid_IDL_roots_from_registry
#
#   Populates the IDL root with valid versions of IDL.
#   The returned IDL_roots is organized in pairs
#   ``(version_number,IDL_root_path)``.
#
#   ::
#
#     IDL_get_all_valid_IDL_roots_from_registry(
#         IDL_versions
#         IDL_roots)
#
#   ``IDL_versions``
#     the versions of each of the IDL installations
#   ``IDL_roots``
#     the location of each of the IDL installations
function(IDL_get_all_valid_IDL_roots_from_registry IDL_versions IDL_roots)

  set(_IDL_roots_list )
  foreach(_IDL_current_version ${IDL_versions})
    get_filename_component(
      current_IDL_ROOT
      "[HKEY_LOCAL_MACHINE\\SOFTWARE\\ITT\\IDL\\${_IDL_current_version};Installdir]"
      ABSOLUTE)

    # IDL root is "${Installdir}/IDLxy" with x.y the IDL version
    string(REPLACE "." "" _IDL_current_version_short ${_IDL_current_version})
    set(current_IDL_ROOT "${current_IDL_ROOT}/IDL${_IDL_current_version_short}")
    unset(_IDL_current_version_short)
    if(EXISTS ${current_IDL_ROOT})
      list(APPEND _IDL_roots_list ${_IDL_current_version} ${current_IDL_ROOT})
    endif()

  endforeach(_IDL_current_version)
  unset(_IDL_current_version)
  set(${IDL_roots} ${_IDL_roots_list} PARENT_SCOPE)
  unset(_IDL_roots_list)
endfunction()


# get IDL version for a given root path from the list of `(version_number,IDL_root_path)`` pairs
function(IDL_get_version_from_IDL_roots_from_registry IDL_roots IDL_root IDL_version)
  list(LENGTH IDL_roots _numbers_of_IDL_roots)
  foreach(i RANGE 0 (_numbers_of_IDL_roots-1) 2)
    list(GET IDL_roots 0 _IDL_version)
    list(GET IDL_roots 1 _IDL_root)
    if(_IDL_root STREQUAL IDL_root)
      set(IDL_version _IDL_version PARENT_SCOPE)
    endif()
    unset(_IDL_version)
    unset(_IDL_root)
  endforeach()
  set(_numbers_of_IDL_roots)
endfunction()


# Get the version of IDL from the version.txt file (Mac/Linux only).
function(IDL_get_version_from_root IDL_root IDL_final_version)
  if(WIN32)
    # determine based on entries in registry
    set(_IDL_versions_from_registry)
    IDL_extract_all_installed_versions_from_registry(CMAKE_CL_64 _IDL_versions_from_registry)
    if(_IDL_versions_from_registry)
      IDL_get_all_valid_IDL_roots_from_registry("${_IDL_versions_from_registry}" _IDL_available_roots)
      unset(_IDL_versions_from_registry)
      IDL_get_version_from_IDL_roots_from_registry(_IDL_available_roots IDL_root IDL_final_version)
      unset(_IDL_available_roots)
    endif()
  else()
    if(EXISTS ${IDL_root}/version.txt)
      file(READ ${IDL_root}/version.txt _IDL_version LIMIT 10)
      # change version 'xyz' to 'x.y.z'
      string(STRIP ${_IDL_version} _IDL_version)
      string(REGEX REPLACE "([0-9])" "\\1." _IDL_version ${_IDL_version})
      string(REGEX REPLACE "(.)$" "" _IDL_version ${_IDL_version})
      set(${IDL_final_version} ${_IDL_version} PARENT_SCOPE)
      unset(_IDL_version)
    else()
      message(WARNING "[IDL] the specified IDL_ROOT_DIR does not contain a version.txt file (${IDL_ROOT_DIR})")
    endif()
  endif()
endfunction()




# ###################################
# Exploring the possible IDL_ROOTS

# this variable will get all IDL installations found in the current system.
set(_IDL_possible_roots)



set(IDL_VERSION_STRING "NOTFOUND")

if(IDL_ROOT_DIR)
  # if the user specifies a possible root, we keep this one
  if(NOT EXISTS ${IDL_ROOT_DIR})
    # if IDL_ROOT_DIR specified but erroneous
    if(IDL_FIND_DEBUG)
      message(WARNING "[IDL] the specified path for IDL_ROOT_DIR does not exist (${IDL_ROOT_DIR})")
    endif()
  else()
    if(DEFINED IDL_VERSION_STRING_INTERNAL AND DEFINED IDL_ROOT_DIR_LAST_CACHED AND IDL_ROOT_DIR_LAST_CACHED STREQUAL IDL_ROOT_DIR)
      list(APPEND _IDL_possible_roots ${IDL_VERSION_STRING_INTERNAL} ${IDL_ROOT_DIR}) # cached version
    else()
      # determine the version again
      IDL_get_version_from_root("${IDL_ROOT_DIR}" _IDL_current_version)
      if (DEFINED _IDL_current_version)
        list(APPEND _IDL_possible_roots "${_IDL_current_version}" ${IDL_ROOT_DIR})
        unset(_IDL_current_version)
      endif()
    endif()
  endif()

else()

  # if the user does not specify the possible installation root, we look for
  # one installation using the appropriate heuristics

  if(WIN32)

    # On WIN32, we look for IDL installation in the registry
    # if unsuccessful, we look for all known revision and filter the existing
    # ones.

    # testing if we are able to extract the needed information from the registry
    set(_IDL_versions_from_registry)
    IDL_extract_all_installed_versions_from_registry(CMAKE_CL_64 _IDL_versions_from_registry)

    # the returned list is empty, doing the search on all known versions
    if(NOT _IDL_versions_from_registry)
      message(STATUS "[IDL] Search for IDL from the registry unsuccessful")
    endif()

    # filtering the results with the registry keys
    IDL_get_all_valid_IDL_roots_from_registry("${_IDL_versions_from_registry}" _IDL_possible_roots)
    unset(_IDL_versions_from_registry)

  else()

    if(APPLE)
      set(_IDL_search_root "/Applications")
    else()
      set(_IDL_search_root "/usr/local")
    endif()

    # on mac, we look for the /Application paths
    # this corresponds to the behaviour on Windows. On Linux, we do not have
    # any other guess.

    foreach(_IDL_company harris exelis itt)
      set(_IDL_full_string "${_IDL_search_root}/${_IDL_company}/idl")
      if(EXISTS ${_IDL_full_string})
        set(_IDL_current_version)
        IDL_get_version_from_root("${_IDL_full_string}" _IDL_current_version)
        if (DEFINED _IDL_current_version)
          if(IDL_FIND_DEBUG)
            message(STATUS "[IDL] Found version ${_IDL_current_version} in ${_IDL_full_string}")
          endif()
          list(APPEND _IDL_possible_roots ${_IDL_current_version} ${_IDL_full_string})
          unset(_IDL_current_version)
        endif()
        unset(_IDL_full_string)
      endif()

    endforeach(_IDL_company)

  endif()

endif()



list(LENGTH _IDL_possible_roots _numbers_of_IDL_roots)
if(_numbers_of_IDL_roots EQUAL 0)
  if(NOT WIN32)
    # if we have not found anything, we fall back on the PATH

    # At this point, we have no other choice than trying to find it from PATH.
    # If set by the user, this will not change
    find_program(_IDL_main_tmp NAMES idl)

    if(_IDL_main_tmp)
      # we then populate the list of roots, with empty version
      if(IDL_FIND_DEBUG)
        message(STATUS "[IDL] IDL found from PATH: ${_IDL_main_tmp}")
      endif()

      # resolve symlinks
      get_filename_component(_IDL_current_location "${_IDL_main_tmp}" REALPATH)

      # get the directory (the command below has to be run twice)
      # this will be the IDL root
      get_filename_component(_IDL_current_location "${_IDL_current_location}" DIRECTORY)
      get_filename_component(_IDL_current_location "${_IDL_current_location}" DIRECTORY) # IDL should be in bin

      IDL_get_version_from_root("${_IDL_current_location}" _IDL_current_version)
      if (DEFINED _IDL_current_version)
        list(APPEND _IDL_possible_roots ${_IDL_current_version} ${_IDL_current_location})
        unset(_IDL_current_version)
      endif()

      unset(_IDL_current_location)

    endif()
    unset(_IDL_main_tmp CACHE)
  endif()
endif()


if(IDL_FIND_DEBUG)
  message(STATUS "[IDL] IDL root folders are ${_IDL_possible_roots}")
endif()


# take the first possible IDL root
list(LENGTH _IDL_possible_roots _numbers_of_IDL_roots)
if(_numbers_of_IDL_roots GREATER 0)
  list(GET _IDL_possible_roots 0 IDL_VERSION_STRING)
  list(GET _IDL_possible_roots 1 IDL_ROOT_DIR)

  # adding a warning in case of ambiguity
  if(_numbers_of_IDL_roots GREATER 2 AND IDL_FIND_DEBUG)
    message(WARNING "[IDL] Found several distributions of IDL. Setting the current version to ${IDL_VERSION_STRING} (located ${IDL_ROOT_DIR})."
                    " If this is not the desired behaviour, provide the -DIDL_ROOT_DIR=... on the command line")
  endif()
endif()


# check if the root changed wrt. the previous defined one, if so
# clear all the cached variables for being able to reconfigure properly
if(DEFINED IDL_ROOT_DIR_LAST_CACHED)

  if(NOT IDL_ROOT_DIR_LAST_CACHED STREQUAL IDL_ROOT_DIR)
    set(_IDL_cached_vars
        IDL_INCLUDE_DIRS
        IDL_MAIN_PROGRAM
        IDL_LIBRARY

        # internal
        IDL_ROOT_DIR_LAST_CACHED
        IDL_VERSION_STRING_INTERNAL
        )
    foreach(_var IN LISTS _IDL_cached_vars)
      if(DEFINED ${_var})
        unset(${_var} CACHE)
      endif()
    endforeach()
  endif()
endif()

set(IDL_ROOT_DIR_LAST_CACHED ${IDL_ROOT_DIR} CACHE INTERNAL "last IDL root dir location")
set(IDL_ROOT_DIR ${IDL_ROOT_DIR} CACHE PATH "IDL installation root path" FORCE)
set(IDL_VERSION_STRING_INTERNAL ${IDL_VERSION_STRING} CACHE INTERNAL "IDL version (automatically determined)" FORCE)

if(IDL_FIND_DEBUG)
  message(STATUS "[IDL] Current version is ${IDL_VERSION_STRING} located ${IDL_ROOT_DIR}")
endif()


if(IDL_ROOT_DIR)
  file(TO_CMAKE_PATH ${IDL_ROOT_DIR} IDL_ROOT_DIR)
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 4)
  set(_IDL_64Build FALSE)
else()
  set(_IDL_64Build TRUE)
endif()

if(APPLE)
  set(_IDL_bin_suffix_32bits ".darwin.x86")
  set(_IDL_bin_suffix_64bits ".darwin.x86_64")
elseif(UNIX)
  set(_IDL_bin_suffix_32bits ".linux.x86")
  set(_IDL_bin_suffix_64bits ".linux.x86_64")
else()
  set(_IDL_bin_suffix_32bits ".x86")
  set(_IDL_bin_suffix_64bits ".x86_64")
endif()



set(IDL_INCLUDE_DIR_TO_LOOK ${IDL_ROOT_DIR}/external/include)
if(_IDL_64Build)
  set(_IDL_current_suffix ${_IDL_bin_suffix_64bits})
else()
  set(_IDL_current_suffix ${_IDL_bin_suffix_32bits})
endif()

set(IDL_BINARIES_DIR ${IDL_ROOT_DIR}/bin/bin${_IDL_current_suffix})

set(_IDL_lib_dir_for_search ${IDL_BINARIES_DIR})
if(WIN32)
  set(_IDL_lib_prefix_for_search "")
else()
  set(_IDL_lib_prefix_for_search "lib")
endif()

unset(_IDL_64Build)


if(IDL_FIND_DEBUG)
  message(STATUS "[IDL] [DEBUG]_IDL_lib_prefix_for_search = ${_IDL_lib_prefix_for_search} | _IDL_lib_dir_for_search = ${_IDL_lib_dir_for_search}")
endif()



# internal
# This small stub around find_library is to prevent any pollution of CMAKE_FIND_LIBRARY_PREFIXES in the global scope.
# This is the function to be used below instead of the find_library directives.
function(_IDL_find_library _IDL_library_prefix)
  set(CMAKE_FIND_LIBRARY_PREFIXES ${CMAKE_FIND_LIBRARY_PREFIXES} "${_IDL_library_prefix}")
  find_library(${ARGN})
endfunction()


set(_IDL_required_variables)

# the IDL root is required
list(APPEND _IDL_required_variables IDL_ROOT_DIR)

# the idl library and export.h header file are required
find_path(
  IDL_INCLUDE_DIRS
  idl_export.h
  PATHS ${IDL_INCLUDE_DIR_TO_LOOK}
  NO_DEFAULT_PATH
  )
list(APPEND _IDL_required_variables IDL_INCLUDE_DIRS)

_IDL_find_library(
  "${_IDL_lib_prefix_for_search}"
  IDL_LIBRARY
  idl
  PATHS "${_IDL_lib_dir_for_search}"
  NO_DEFAULT_PATH
)

list(APPEND _IDL_required_variables IDL_LIBRARY)

# component IDL program
list(FIND IDL_FIND_COMPONENTS MAIN_PROGRAM _IDL_find_IDL_program)
if(_IDL_find_IDL_program GREATER -1)
  find_program(
    IDL_MAIN_PROGRAM
    idl
    PATHS ${IDL_ROOT_DIR}/bin
    DOC "IDL main program"
    NO_DEFAULT_PATH
  )
  if(IDL_MAIN_PROGRAM)
    set(IDL_MAIN_PROGRAM_FOUND TRUE)
  endif()
endif()
unset(_IDL_find_IDL_program)

unset(_IDL_lib_dir_for_search)

set(IDL_LIBRARIES ${IDL_LIBRARY})

# the IDL version is required
list(APPEND _IDL_required_variables IDL_VERSION_STRING)

find_package_handle_standard_args(
  IDL
  FOUND_VAR IDL_FOUND
  REQUIRED_VARS ${_IDL_required_variables}
  VERSION_VAR IDL_VERSION_STRING
  HANDLE_COMPONENTS)

unset(_IDL_required_variables)
unset(_IDL_bin_prefix)
unset(_IDL_bin_suffix_32bits)
unset(_IDL_bin_suffix_64bits)
unset(_IDL_current_suffix)
unset(_IDL_lib_dir_for_search)
unset(_IDL_lib_prefix_for_search)

if(IDL_INCLUDE_DIRS AND IDL_LIBRARIES)
  mark_as_advanced(
    IDL_LIBRARY
    IDL_INCLUDE_DIRS
    IDL_FOUND
    IDL_MAIN_PROGRAM
  )
endif()
