cmake_minimum_required(VERSION 3.12.4)
if(POLICY CMP0075)
  cmake_policy(SET CMP0075 NEW)
endif()

project(HARP)

set(VERSION 1.26)
set(HARP_VERSION \"${VERSION}\")
set(CPACK_PACKAGE_VERSION ${VERSION})

# Set dynamic library version
set(LIBHARP_CURRENT 15)
set(LIBHARP_REVISION 1)
set(LIBHARP_AGE 2)
math(EXPR LIBHARP_MAJOR "${LIBHARP_CURRENT} - ${LIBHARP_AGE}")
set(LIBHARP_MINOR ${LIBHARP_AGE})

# HARP format version
set(HARP_FORMAT_VERSION_MAJOR 1)
set(HARP_FORMAT_VERSION_MINOR 0)
set(HARP_CONVENTION \"HARP-${HARP_FORMAT_VERSION_MAJOR}.${HARP_FORMAT_VERSION_MINOR}\")


# General setup
set(CMAKE_INSTALL_DEFAULT_COMPONENT_NAME core)
set(CMAKE_MODULE_PATH
  ${HARP_SOURCE_DIR}/CMakeModules
  ${CMAKE_MODULE_PATH}
  CACHE INTERNAL "Local CMake modules")
set(CMAKE_MACOSX_RPATH ON)

include(CheckFunctionExists)
include(CheckLibraryExists)
include(CheckIncludeFile)
include(CheckSymbolExists)
include(CheckTypeSize)
include(CheckCSourceCompiles)
include(TestBigEndian)
include(CMakeDependentOption)

if(NOT MSVC AND NOT CMAKE_BUILD_TYPE AND NOT CMAKE_C_FLAGS)
  set(CMAKE_C_FLAGS "-O2 -g"
      CACHE STRING "Flags used by the C compiler during all build types." FORCE)
endif(NOT MSVC AND NOT CMAKE_BUILD_TYPE AND NOT CMAKE_C_FLAGS)

# Option handling
#
option(HARP_BUILD_IDL "build IDL interface" OFF)
option(HARP_BUILD_MATLAB "build MATLAB interface" OFF)
option(HARP_BUILD_PYTHON "build Python interface" OFF)
option(HARP_BUILD_R "build R interface" OFF)
option(HARP_WITH_HDF4 "use HDF4" ON)
option(HARP_WITH_HDF5 "use HDF5" ON)
option(HARP_ENABLE_CONDA_INSTALL OFF)
set(HARP_EXPAT_NAME_MANGLE 1)
set(HARP_NETCDF_NAME_MANGLE 1)
# Note that we also add an explicit -D option for HARP_UDUNITS2_NAME_MANGLE, since the udunits2 sources do not include config.h
set(HARP_UDUNITS2_NAME_MANGLE 1)

# settings for expat
set(XML_NS 1)
set(XML_DTD 1)
set(XML_LARGE_SIZE 1)
set(XML_CONTEXT_BYTES 1024)

if(MSVC)
  # Suppress warnings from VC on deprecated POSIX names and unsafe functions
  add_definitions(-D_CRT_NONSTDC_NO_DEPRECATE -D_CRT_SECURE_NO_WARNINGS)
endif(MSVC)

if(HARP_BUILD_PYTHON)
  find_package(Python COMPONENTS Interpreter)
  if(NOT Python_Interpreter_FOUND)
    message(FATAL_ERROR "Python interpreter not found.")
  endif(NOT Python_Interpreter_FOUND)
  set(HARP_PYTHON_INSTALL_DIR "${Python_SITEARCH}" CACHE STRING "Install location for HARP Python package")
endif()

set(BIN_PREFIX bin)
set(LIB_PREFIX lib)
set(INCLUDE_PREFIX include)
set(DOC_PREFIX share/harp/doc)
set(PYTHON_PREFIX ${HARP_PYTHON_INSTALL_DIR}/harp)
set(R_PREFIX lib/R/site-library/harp)
set(MATLAB_PREFIX lib/harp/matlab)
set(IDL_PREFIX lib/harp/idl)
set(CODADEF_PREFIX share/coda/definitions)
set(UDUNITS2_PREFIX share/harp)
if(WIN32)
  # we use a different installation directory structure on Windows
  if(HARP_ENABLE_CONDA_INSTALL)
    # all non-python/non-R components need to be installed in a Library sub-directory for Conda on Windows
    set(BIN_PREFIX Library/bin)
    set(LIB_PREFIX Library/lib)
    set(INCLUDE_PREFIX Library/include)
    set(DOC_PREFIX Library/share/harp/doc)
    set(MATLAB_PREFIX Library/lib/harp/matlab)
    set(IDL_PREFIX Library/lib/harp/idl)
    set(CODADEF_PREFIX Library/share/coda/definitions)
    set(UDUNITS2_PREFIX Library/share/harp)
  else(CODA_ENABLE_CONDA_INSTALL)
    set(DOC_PREFIX doc)
    set(PYTHON_PREFIX python/harp)
    set(R_PREFIX R/harp)
  endif(HARP_ENABLE_CONDA_INSTALL)
endif(WIN32)

SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${LIB_PREFIX}")

if(NOT MSVC)
  set(MATHLIB m)
endif(NOT MSVC)

set(INCLUDES "")

macro(find_include FILE VARIABLE)
  CHECK_INCLUDE_FILE(${FILE} ${VARIABLE})
  if(${VARIABLE})
    set(INCLUDES ${INCLUDES} ${FILE})
  endif(${VARIABLE})
endmacro(find_include)

find_include(dirent.h HAVE_DIRENT_H)
find_include(dlfcn.h HAVE_DLFCN_H)
find_include(inttypes.h HAVE_INTTYPES_H)
find_include(limits.h HAVE_LIMITS_H)
find_include(stdarg.h HAVE_STDARG_H)
find_include(stdio.h HAVE_STDIO_H)
find_include(stdint.h HAVE_STDINT_H)
find_include(stdlib.h HAVE_STDLIB_H)
find_include(string.h HAVE_STRING_H)
find_include(strings.h HAVE_STRINGS_H)
find_include(sys/stat.h HAVE_SYS_STAT_H)
find_include(sys/types.h HAVE_SYS_TYPES_H)
find_include(unistd.h HAVE_UNISTD_H)

set(CMAKE_EXTRA_INCLUDE_FILES ${INCLUDES})

check_function_exists(bcopy HAVE_BCOPY)
check_function_exists(floor HAVE_FLOOR)
check_function_exists(getpagesize HAVE_GETPAGESIZE)
check_function_exists(malloc HAVE_MALLOC)
check_function_exists(memmove HAVE_MEMMOVE)
check_function_exists(mmap HAVE_MMAP)
check_function_exists(pread HAVE_PREAD)
check_function_exists(realloc HAVE_REALLOC)
check_function_exists(stat HAVE_STAT)
check_function_exists(strcasecmp HAVE_STRCASECMP)
check_function_exists(strdup HAVE_STRDUP)
check_function_exists(strerror HAVE_STRERROR)
check_function_exists(strncasecmp HAVE_STRNCASECMP)
check_function_exists(vsnprintf HAVE_VSNPRINTF)

set(UCHAR "unsigned char")
check_symbol_exists(size_t "${INCLUDES}" HAVE_SIZE_T)
set(SIZE_T "long")
check_symbol_exists(ptrdiff_t "${INCLUDES}" HAVE_PTRDIFF_T)
set(PTRDIFF_T "int")

check_type_size(size_t SIZEOF_SIZE_T)
check_type_size(off_t SIZEOF_OFF_T)
check_type_size(double SIZEOF_DOUBLE)
check_type_size(float SIZEOF_FLOAT)
check_type_size(int SIZEOF_INT)
check_type_size(long SIZEOF_LONG)
check_type_size(short SIZEOF_SHORT)

check_type_size(_Bool SIZEOF__BOOL)
set(HAVE__BOOL ${SIZEOF__BOOL})

test_big_endian(WORDS_BIGENDIAN)

if(HAVE_STDLIB_H AND HAVE_STDDEF_H)
  set(STDC_HEADERS 1)
endif(HAVE_STDLIB_H AND HAVE_STDDEF_H)

if(MSVC AND MSVC_VERSION LESS 1600 AND NOT MINGW)
# For Visual Studio > 2010 and MinGW we can use stdint.h
# For earlier versions of Visual Studio we need to provide our own defines
set(int8_t "signed char")
set(int16_t "signed short")
set(int32_t "int")
set(int64_t "__int64")
set(uint8_t "unsigned char")
set(uint16_t "unsigned short")
set(uint32_t "unsigned int")
set(uint64_t "unsigned __int64")
endif(MSVC AND MSVC_VERSION LESS 1600 AND NOT MINGW)

find_package(BISON)
find_package(FLEX)

if(HARP_WITH_HDF4)
  find_package(HDF4)
  if(NOT HDF4_FOUND)
    message(FATAL_ERROR "HDF4 libraries and/or header files are not found. Try setting the HDF4_LIBRARY_DIR and HDF4_INCLUDE_DIR cmake variables to the location of your HDF4 library and include files.")
  else(NOT HDF4_FOUND)
    set(HAVE_HDF4 1)
    include_directories(${HDF4_INCLUDE_DIR})
  endif(NOT HDF4_FOUND)
endif(HARP_WITH_HDF4)

if(HARP_WITH_HDF5)
  find_package(HDF5)
  if(NOT HDF5_FOUND)
      message(FATAL_ERROR "HDF5 libraries and/or header files are not found. Try setting the HDF5_LIBRARY_DIR and HDF5_INCLUDE_DIR cmake variables to the location of your HDF5 library and include files.")
  else(NOT HDF5_FOUND)
    set(HAVE_HDF5 1)
    include_directories(${HDF5_INCLUDE_DIR})
  endif(NOT HDF5_FOUND)
endif(HARP_WITH_HDF5)

if(HARP_BUILD_R)
  find_package(R)
  if(NOT R_FOUND)
    message(FATAL_ERROR "R installation not found.")
  endif(NOT R_FOUND)
  include_directories(${R_INCLUDE_DIR})
endif()

# We allow HARP to be build as part of a larger CMake build that also includes
# CODA. It this is the case then CODA_SOURCE_DIR and CODA_BINARY_DIR will
# already be set and we don't have to look for an installed version of CODA.
if(DEFINED CODA_SOURCE_DIR AND DEFINED CODA_BINARY_DIR)
  set(CODA_INCLUDE_DIR ${CODA_BINARY_DIR} ${CODA_SOURCE_DIR}/libcoda)
  set(CODA_LIBRARIES coda)
else(DEFINED CODA_SOURCE_DIR AND DEFINED CODA_BINARY_DIR)
  find_package(CODA)
  if(NOT CODA_FOUND)
    message(FATAL_ERROR "CODA libraries and/or header files are not found. Try setting the CODA_LIBRARY_DIR and CODA_INCLUDE_DIR cmake variables to the location of your CODA library and include files.")
  endif(NOT CODA_FOUND)
endif(DEFINED CODA_SOURCE_DIR AND DEFINED CODA_BINARY_DIR)

include_directories(${CODA_INCLUDE_DIR})

include_directories(BEFORE ${HARP_BINARY_DIR} ${HARP_SOURCE_DIR}/libharp)

add_definitions(-DHAVE_CONFIG_H)

# The Doxyfile uses some autoconf variables CMake does not have.
# We set them here manually, so that we can use the same .in
# file for both autoconf and cmake
#
set(top_builddir ${CMAKE_BINARY_DIR})
set(top_srcdir ${CMAKE_SOURCE_DIR})
set(srcdir ${CMAKE_CURRENT_SOURCE_DIR}/doc)
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/doc/Doxyfile.in
  ${CMAKE_CURRENT_BINARY_DIR}/doc/Doxyfile)


# libharp sources
#

BISON_TARGET(harp-operation-parser ${CMAKE_CURRENT_SOURCE_DIR}/libharp/harp-operation-parser.y ${CMAKE_CURRENT_BINARY_DIR}/harp-operation-parser.c)
FLEX_TARGET(harp-operation-scanner ${CMAKE_CURRENT_SOURCE_DIR}/libharp/harp-operation-scanner.l ${CMAKE_CURRENT_BINARY_DIR}/harp-operation-scanner.c)
add_flex_bison_dependency(harp-operation-scanner harp-operation-parser)

set(LIBHARP_SOURCES
  libharp/harp-analysis.c
  libharp/harp-area-mask.h
  libharp/harp-area-mask.c
  libharp/harp-aux-afgl86.c
  libharp/harp-aux-usstd76.c
  libharp/harp-bin.c
  libharp/harp-bin-spatial.c
  libharp/harp-chemistry.h
  libharp/harp-chemistry.c
  libharp/harp-collocation.c
  libharp/harp-constants.h
  libharp/harp-dataset.c
  libharp/harp-derived-variable.c
  libharp/harp-derived-variable-list.c
  libharp/harp-dimension-mask.h
  libharp/harp-dimension-mask.c
  libharp/harp-csv.h
  libharp/harp-csv.c
  libharp/harp-errno.c
  libharp/harp-filter.h
  libharp/harp-filter.c
  libharp/harp-filter-collocation.h
  libharp/harp-filter-collocation.c
  libharp/harp-geometry-sphere-euler.c
  libharp/harp-geometry-sphere-line.c
  libharp/harp-geometry-sphere-point.c
  libharp/harp-geometry-sphere-polygon.c
  libharp/harp-geometry-vector3d.c
  libharp/harp-geometry-util.c
  libharp/harp-geometry.h
  libharp/harp-ingest-ace_fts_l2.c
  libharp/harp-ingest-aeolus_l1b.c
  libharp/harp-ingest-aeolus_l2a.c
  libharp/harp-ingest-aeolus_l2b.c
  libharp/harp-ingest-calipso_l2.c
  libharp/harp-ingest-cci_l2_aerosol.c
  libharp/harp-ingest-cci_l2_ghg.c
  libharp/harp-ingest-cci_l2_o3_lp.c
  libharp/harp-ingest-cci_l2_o3_np.c
  libharp/harp-ingest-cci_l2_o3_tc.c
  libharp/harp-ingest-cci_l3_aerosol.c
  libharp/harp-ingest-cci_l3_cloud.c
  libharp/harp-ingest-cci_l3_ghg.c
  libharp/harp-ingest-cci_l3_o3_lntoc.c
  libharp/harp-ingest-cci_l3_o3_lp.c
  libharp/harp-ingest-cci_l3_o3_np.c
  libharp/harp-ingest-cci_l3_o3_tc.c
  libharp/harp-ingest-cci_l3_o3_ttoc.c
  libharp/harp-ingest-cci_l4_o3_np.c
  libharp/harp-ingest-cloudnet.c
  libharp/harp-ingest-earlinet.c
  libharp/harp-ingest-earthcare_l1.c
  libharp/harp-ingest-earthcare_l2.c
  libharp/harp-ingest-ecmwf-grib.c
  libharp/harp-ingest-geoms-ftir.c
  libharp/harp-ingest-geoms-lidar.c
  libharp/harp-ingest-geoms-mwr.c
  libharp/harp-ingest-geoms-pandora.c
  libharp/harp-ingest-geoms-sonde.c
  libharp/harp-ingest-geoms-tccon.c
  libharp/harp-ingest-geoms-uvvis-brewer.c
  libharp/harp-ingest-geoms-uvvis-doas.c
  libharp/harp-ingest-geoms-uvvis-dobson.c
  libharp/harp-ingest-gome_l1.c
  libharp/harp-ingest-gome_l2.c
  libharp/harp-ingest-gome2_l1.c
  libharp/harp-ingest-gome2_l2.c
  libharp/harp-ingest-gomos_l1.c
  libharp/harp-ingest-gomos_l2.c
  libharp/harp-ingest-gosat_fts_l1b.c
  libharp/harp-ingest-gosat_fts_l2.c
  libharp/harp-ingest-hirdls_l2.c
  libharp/harp-ingest-iasi_l1.c
  libharp/harp-ingest-iasi_l2.c
  libharp/harp-ingest-mipas_l1.c
  libharp/harp-ingest-mipas_l2.c
  libharp/harp-ingest-mls_l2.c
  libharp/harp-ingest-npp-suomi-crimss_l2.c
  libharp/harp-ingest-npp-suomi-omps_profiles_l2.c
  libharp/harp-ingest-npp-suomi-omps_totals_l2.c
  libharp/harp-ingest-npp-suomi-viirs_l2.c
  libharp/harp-ingest-oco-l3co2.c
  libharp/harp-ingest-oco-ltco2.c
  libharp/harp-ingest-oco-ltsif.c
  libharp/harp-ingest-omi_l2.c
  libharp/harp-ingest-omi_l3.c
  libharp/harp-ingest-osiris_l2.c
  libharp/harp-ingest-pal_s5p_l2.c
  libharp/harp-ingest-qa4ecv.c
  libharp/harp-ingest-sciamachy_l1.c
  libharp/harp-ingest-sciamachy_l2.c
  libharp/harp-ingest-s5p_l1b.c
  libharp/harp-ingest-s5p_l2.c
  libharp/harp-ingest-s5_l2.c
  libharp/harp-ingest-smr_l2.c
  libharp/harp-ingest-temis.c
  libharp/harp-ingest-tes_l2.c
  libharp/harp-ingestion.h
  libharp/harp-ingestion.c
  libharp/harp-ingestion-doc.c
  libharp/harp-ingestion-module.c
  libharp/harp-ingestion-options.c
  libharp/harp-internal.h
  libharp/harp-interpolation.c
  libharp/harp-netcdf.c
  libharp/harp-operation.h
  libharp/harp-operation.c
  libharp/harp-product.c
  libharp/harp-product-metadata.c
  libharp/harp-program.h
  libharp/harp-program.c
  libharp/harp-sea-surface.c
  libharp/harp-rebin.c
  libharp/harp-regrid.c
  libharp/harp-units.c
  libharp/harp-utils.c
  libharp/harp-variable.c
  libharp/harp-vertical-profiles.h
  libharp/harp-vertical-profiles.c
  libharp/harp.c
  libharp/harp.h
  libharp/hashtable.c
  libharp/hashtable.h
  libharp/ipow.h
  ${BISON_harp-operation-parser_OUTPUTS}
  ${FLEX_harp-operation-scanner_OUTPUTS})

set(LIBHARP_HDF4_FILES libharp/harp-hdf4.c)
if(HARP_WITH_HDF4)
  set(LIBHARP_SOURCES ${LIBHARP_SOURCES} ${LIBHARP_HDF4_FILES})
endif(HARP_WITH_HDF4)

set(LIBHARP_HDF5_FILES libharp/harp-hdf5.c)
if(HARP_WITH_HDF5)
  set(LIBHARP_SOURCES ${LIBHARP_SOURCES} ${LIBHARP_HDF5_FILES})
endif(HARP_WITH_HDF5)

set(LIBEXPAT_SOURCES
  expat/xmlparse.c
  expat/xmlrole.c
  expat/xmltok.c
  expat/ascii.h
  expat/asciitab.h
  expat/expat.h
  expat/expat_external.h
  expat/harp_expat_mangle.h
  expat/iasciitab.h
  expat/internal.h
  expat/latin1tab.h
  expat/nametab.h
  expat/siphash.h
  expat/utf8tab.h
  expat/xmlrole.h
  expat/xmltok.h
  expat/xmltok_impl.h)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/udunits2)
BISON_TARGET(utparser ${CMAKE_CURRENT_SOURCE_DIR}/udunits2/utparser.y ${CMAKE_CURRENT_BINARY_DIR}/udunits2/utparser.c)
FLEX_TARGET(utscanner ${CMAKE_CURRENT_SOURCE_DIR}/udunits2/utscanner.l ${CMAKE_CURRENT_BINARY_DIR}/utscanner.c)
add_flex_bison_dependency(utscanner utparser)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/udunits2)

set(LIBUDUNITS2_SOURCES
  udunits2/converter.c
  udunits2/converter.h
  udunits2/error.c
  udunits2/formatter.c
  udunits2/harp_udunits2_mangle.h
  udunits2/idToUnitMap.c
  udunits2/idToUnitMap.h
  udunits2/prefix.c
  udunits2/prefix.h
  udunits2/status.c
  udunits2/systemMap.c
  udunits2/systemMap.h
  udunits2/udunits2.h
  udunits2/unitcore.c
  udunits2/unitAndId.c
  udunits2/unitAndId.h
  udunits2/unitToIdMap.c
  udunits2/unitToIdMap.h
  udunits2/ut_free_system.c
  udunits2/xml.c
  ${BISON_utparser_OUTPUTS}
  ${FLEX_utscanner_OUTPUTS})

if(MSVC)
  set(LIBUDUNITS2_SOURCES
    ${LIBUDUNITS2_SOURCES}
    udunits2/tsearch.c
    udunits2/tsearch.h
    udunits2/udunits_snprintf.c)
endif(MSVC)

include_directories(BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/udunits2)

set(LIBNETCDF_SOURCES
  netcdf/attr.c
  netcdf/dim.c
  netcdf/error.c
  netcdf/fbits.h
  netcdf/harp_netcdf_mangle.h
  netcdf/libvers.c
  netcdf/nc.c
  netcdf/nc.h
  netcdf/nc3convert.h
  netcdf/nc3local.h
  netcdf/ncio.h
  netcdf/ncx.c
  netcdf/ncx.h
  netcdf/netcdf.h
  netcdf/onstack.h
  netcdf/posixio.c
  netcdf/putget.c
  netcdf/rnd.h
  netcdf/string.c
  netcdf/utf8proc.c
  netcdf/utf8proc.h
  netcdf/utf8proc_data.h
  netcdf/v1hpg.c
  netcdf/v2i.c
  netcdf/var.c)

include_directories(BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/expat ${CMAKE_CURRENT_SOURCE_DIR}/netcdf)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/harp.h DESTINATION ${INCLUDE_PREFIX})

set(UDUNITS2_XML_DIR ${CMAKE_INSTALL_PREFIX}/${UDUNITS2_PREFIX})
add_definitions(-DDEFAULT_UDUNITS2_XML_PATH="${UDUNITS2_XML_DIR}/udunits2.xml" -DHARP_UDUNITS2_NAME_MANGLE)
add_library(harp SHARED ${LIBHARP_SOURCES} ${LIBUDUNITS2_SOURCES} ${LIBNETCDF_SOURCES} ${LIBEXPAT_SOURCES})
target_link_libraries(harp ${CODA_LIBRARIES} ${HDF4_LIBRARIES} ${HDF5_LIBRARIES})
set_target_properties(harp PROPERTIES
  VERSION ${LIBHARP_MAJOR}.${LIBHARP_MINOR}.${LIBHARP_REVISION}
  SOVERSION ${LIBHARP_MAJOR})
if(WIN32)
  # Also set DLL compile flags
  set_target_properties(harp PROPERTIES COMPILE_FLAGS "-DLIBHARPDLL -DLIBHARPDLL_EXPORTS -DH5_BUILT_AS_DYNAMIC_LIB -DLIBCODADLL -DYY_NO_UNISTD_H")
  install(TARGETS harp RUNTIME DESTINATION ${BIN_PREFIX} ARCHIVE DESTINATION ${LIB_PREFIX})
else(WIN32)
  install(TARGETS harp DESTINATION ${LIB_PREFIX})
endif(WIN32)

add_library(harp_static STATIC ${LIBHARP_SOURCES} ${LIBUDUNITS2_SOURCES} ${LIBNETCDF_SOURCES})
# On Windows, we want libharp.lib for static, harp.dll & harp.lib for shared.
# On Unix, we want libharp.a and libharp.so
if(WIN32)
  set_target_properties(harp_static PROPERTIES OUTPUT_NAME "libharp")
  set_target_properties(harp_static PROPERTIES COMPILE_FLAGS "-DH5_BUILT_AS_DYNAMIC_LIB -DLIBCODADLL -DYY_NO_UNISTD_H")
  install(TARGETS harp_static DESTINATION ${LIB_PREFIX})
else(WIN32)
  set_target_properties(harp_static PROPERTIES OUTPUT_NAME "harp")
endif(WIN32)
install(TARGETS harp_static DESTINATION ${LIB_PREFIX})

#  harpcheck
add_executable(harpcheck tools/harpcheck/harpcheck.c)
target_link_libraries(harpcheck harp ${CODA_LIBRARIES} ${HDF4_LIBRARIES} ${HDF5_LIBRARIES} ${MATHLIB})
if(WIN32)
  set_target_properties(harpcheck PROPERTIES COMPILE_FLAGS "-DLIBHARPDLL")
endif(WIN32)
install(TARGETS harpcheck DESTINATION ${BIN_PREFIX})

#  harpcollocate
set(HARPCOLLOCATE_SOURCES
  tools/harpcollocate/harpcollocate.c
  tools/harpcollocate/harpcollocate-matchup.c
  tools/harpcollocate/harpcollocate-resample.c
  tools/harpcollocate/harpcollocate-update.c)
add_executable(harpcollocate ${HARPCOLLOCATE_SOURCES})
target_link_libraries(harpcollocate harp ${CODA_LIBRARIES} ${HDF4_LIBRARIES} ${HDF5_LIBRARIES} ${MATHLIB})
if(WIN32)
  set_target_properties(harpcollocate PROPERTIES COMPILE_FLAGS "-DLIBHARPDLL")
endif(WIN32)
install(TARGETS harpcollocate DESTINATION ${BIN_PREFIX})

#  harpconvert
add_executable(harpconvert tools/harpconvert/harpconvert.c)
target_link_libraries(harpconvert harp ${CODA_LIBRARIES} ${HDF4_LIBRARIES} ${HDF5_LIBRARIES} ${MATHLIB})
if(WIN32)
  set_target_properties(harpconvert PROPERTIES COMPILE_FLAGS "-DLIBHARPDLL")
endif(WIN32)
install(TARGETS harpconvert DESTINATION ${BIN_PREFIX})

#  harpdump
add_executable(harpdump tools/harpdump/harpdump.c)
target_link_libraries(harpdump harp ${CODA_LIBRARIES} ${HDF4_LIBRARIES} ${HDF5_LIBRARIES} ${MATHLIB})
if(WIN32)
  set_target_properties(harpdump PROPERTIES COMPILE_FLAGS "-DLIBHARPDLL")
endif(WIN32)
install(TARGETS harpdump DESTINATION ${BIN_PREFIX})

#  harpmerge
add_executable(harpmerge tools/harpmerge/harpmerge.c)
target_link_libraries(harpmerge harp ${CODA_LIBRARIES} ${HDF4_LIBRARIES} ${HDF5_LIBRARIES} ${MATHLIB})
if(WIN32)
  set_target_properties(harpmerge PROPERTIES COMPILE_FLAGS "-DLIBHARPDLL")
endif(WIN32)
install(TARGETS harpmerge DESTINATION ${BIN_PREFIX})

# idl
if(HARP_BUILD_IDL)
  find_package(IDL)
  if(NOT IDL_FOUND)
    message(FATAL_ERROR "IDL installation not found.")
  endif(NOT IDL_FOUND)
  include_directories(AFTER ${IDL_INCLUDE_DIRS})
  set(HARP_IDL_SOURCES idl/harp_idl.c idl/harp_idl.def)
  add_library(harp_idl MODULE ${HARP_IDL_SOURCES})
  set_target_properties(harp_idl PROPERTIES PREFIX "")
  target_link_libraries(harp_idl harp ${IDL_LIBRARIES})
  if(WIN32)
    set_target_properties(harp_idl PROPERTIES COMPILE_FLAGS "-DLIBHARPDLL")
  endif(WIN32)
  configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/idl/harp_idl.dlm.in
    ${CMAKE_CURRENT_BINARY_DIR}/idl/harp_idl.dlm)
  install(TARGETS harp_idl DESTINATION ${IDL_PREFIX} COMPONENT idl)
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/idl/harp_idl.dlm DESTINATION ${IDL_PREFIX} COMPONENT idl)
endif(HARP_BUILD_IDL)

# matlab
if(HARP_BUILD_MATLAB)
  set(Matlab_FIND_COMPONENTS MX_LIBRARY)
  find_package(Matlab)
  if(NOT MATLAB_FOUND)
    message(FATAL_ERROR "MATLAB installation not found.")
  endif(NOT MATLAB_FOUND)
  include_directories(AFTER ${Matlab_INCLUDE_DIRS})
  set(HARP_MATLAB_SOURCES
    matlab/harp-matlab-product.c
    matlab/harp-matlab.h
    matlab/harp_matlab.c
    matlab/harp_matlab.def)
  add_library(harp_matlab MODULE ${HARP_MATLAB_SOURCES})
  set_target_properties(harp_matlab PROPERTIES
    PREFIX ""
    SUFFIX ".${Matlab_MEX_EXTENSION}"
    COMPILE_DEFINITIONS MATLAB_MEX_FILE
    COMPILE_FLAGS "-DLIBHARPDLL")
  target_link_libraries(harp_matlab harp ${Matlab_LIBRARIES})
  if(WIN32)
    set_target_properties(harp_matlab PROPERTIES COMPILE_FLAGS "-DLIBHARPDLL")
  endif(WIN32)
  set(HARP_MATLAB_M_FILES
    matlab/harp_export.m
    matlab/harp_import.m
    matlab/harp_version.m)
  install(TARGETS harp_matlab DESTINATION ${MATLAB_PREFIX} COMPONENT matlab)
  install(FILES ${HARP_MATLAB_M_FILES} DESTINATION ${MATLAB_PREFIX} COMPONENT matlab)
endif(HARP_BUILD_MATLAB)

# python
if(HARP_BUILD_PYTHON)
  install(FILES
    python/__init__.py
    python/_harpc.py
    python/_harppy.py
    DESTINATION ${PYTHON_PREFIX} COMPONENT python)
endif(HARP_BUILD_PYTHON)

# R
if(HARP_BUILD_R)
  configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/R/DESCRIPTION.in
    ${CMAKE_CURRENT_BINARY_DIR}/R/DESCRIPTION)
  configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/R/create_rds.r.in
    ${CMAKE_CURRENT_BINARY_DIR}/R/create_rds.r)
  add_library(Rharp MODULE R/harp.c R/Rharp.def)
  set_target_properties(Rharp PROPERTIES PREFIX "")
  if(WIN32)
    add_custom_command(OUTPUT R/R.lib
      COMMAND lib /def:${CMAKE_CURRENT_SOURCE_DIR}/R/R.def /MACHINE:x64 /OUT:${CMAKE_CURRENT_BINARY_DIR}/R/R.lib
      DEPENDS R/R.def)
    add_custom_target(Rlib ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/R/R.lib)
    target_link_libraries(Rharp harp ${CMAKE_CURRENT_BINARY_DIR}/R/R.lib)
    add_dependencies(Rharp Rlib)
    install(TARGETS Rharp DESTINATION ${R_PREFIX}/libs/x64 COMPONENT r)
    set_target_properties(Rharp PROPERTIES COMPILE_FLAGS "-DLIBHARPDLL -DR_LEGACY_RCOMPLEX")
    string(REGEX REPLACE "\\.exe$" "script.exe" RSCRIPT_EXECUTABLE ${R_EXECUTABLE})
  else()
    target_link_libraries(Rharp harp ${R_LIBRARIES})
    install(TARGETS Rharp DESTINATION ${R_PREFIX}/libs COMPONENT r)
    set(RSCRIPT_EXECUTABLE "${R_EXECUTABLE}script")
  endif()
  add_custom_command(OUTPUT features.rds package.rds
    COMMAND "${RSCRIPT_EXECUTABLE}" --silent --vanilla R/create_rds.r
    DEPENDS R/create_rds.r)
  install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/features.rds
    ${CMAKE_CURRENT_BINARY_DIR}/package.rds
    DESTINATION ${R_PREFIX}/Meta COMPONENT r)
  add_custom_target(rds_files ALL
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/features.rds ${CMAKE_CURRENT_BINARY_DIR}/package.rds)
  install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/R/DESCRIPTION
    R/NAMESPACE
    DESTINATION ${R_PREFIX} COMPONENT r)
  install(FILES R/harp.r DESTINATION ${R_PREFIX}/R RENAME harp COMPONENT r)
  file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/R/help)
  file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/R/html)
  install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/R/help DESTINATION ${R_PREFIX} COMPONENT r)
  install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/R/html DESTINATION ${R_PREFIX} COMPONENT r)
endif(HARP_BUILD_R)


# product definition files
function(add_codadef name)
  file(GLOB CODADEF_DIR ${CMAKE_CURRENT_SOURCE_DIR}/definitions/${name}/index.xml)
  if(CODADEF_DIR)
    if(WIN32)
      message(FATAL_ERROR "Cannot create codadef files on Windows, use the official source package of HARP")
    else(WIN32)
      execute_process(COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/codadef.sh" "--date" "${CMAKE_CURRENT_SOURCE_DIR}/definitions/${name}"
        OUTPUT_VARIABLE "CODADEF_VERSION"
        OUTPUT_STRIP_TRAILING_WHITESPACE)
      set(CODADEF ${name}-${CODADEF_VERSION}.codadef)
      add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${CODADEF}"
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/codadef.sh" "${CMAKE_CURRENT_SOURCE_DIR}/definitions/${name}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/codadef.sh")
      install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${CODADEF} DESTINATION ${CODADEF_PREFIX})
      add_custom_target(codadef_${name} ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/${CODADEF}")
    endif(WIN32)
  else(CODADEF_DIR)
    file(GLOB CODADEF RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/definitions ${CMAKE_CURRENT_SOURCE_DIR}/definitions/${name}-*.codadef)
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/definitions/${CODADEF}" DESTINATION ${CODADEF_PREFIX})
  endif(CODADEF_DIR)
endfunction(add_codadef)

add_codadef(ACE_FTS)
add_codadef(ACSAF)
add_codadef(AEOLUS)
add_codadef(AURA_HIRDLS)
add_codadef(AURA_MLS)
add_codadef(AURA_OMI)
add_codadef(AURA_TES)
add_codadef(CALIPSO)
add_codadef(CLOUDNET)
add_codadef(EARLINET)
add_codadef(EARTHCARE)
add_codadef(ECMWF)
add_codadef(ENVISAT_GOMOS)
add_codadef(ENVISAT_MIPAS)
add_codadef(ENVISAT_SCIAMACHY)
add_codadef(EPS)
add_codadef(EPS_SG)
add_codadef(ERS_GOME)
add_codadef(ESACCI_AEROSOL)
add_codadef(ESACCI_CLOUD)
add_codadef(ESACCI_GHG)
add_codadef(ESACCI_OZONE)
add_codadef(GEOMS)
add_codadef(GOSAT)
add_codadef(NPP_SUOMI)
add_codadef(OCO)
add_codadef(ODIN_OSIRIS)
add_codadef(ODIN_SMR)
add_codadef(QA4ECV)
add_codadef(S5P_PAL)
add_codadef(Sentinel5P)
add_codadef(TEMIS)

# documentation
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/doc/html DESTINATION ${DOC_PREFIX})

# udunits xml files
install(FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/udunits2/udunits2-accepted.xml
  ${CMAKE_CURRENT_SOURCE_DIR}/udunits2/udunits2-base.xml
  ${CMAKE_CURRENT_SOURCE_DIR}/udunits2/udunits2-common.xml
  ${CMAKE_CURRENT_SOURCE_DIR}/udunits2/udunits2-derived.xml
  ${CMAKE_CURRENT_SOURCE_DIR}/udunits2/udunits2-prefixes.xml
  ${CMAKE_CURRENT_SOURCE_DIR}/udunits2/udunits2.xml
  DESTINATION ${UDUNITS2_PREFIX})

# installer
set(CPACK_PACKAGE_NAME "harp")
set(CPACK_PACKAGE_VENDOR "S&T")
set(CPACK_COMPONENTS_GROUPING ALL_COMPONENTS_IN_ONE)
if(WIN32)
  install(FILES ${CODA_DLLS} ${HDF4_DLLS} ${HDF5_DLLS} DESTINATION ${BIN_PREFIX})
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/Release/harp.exp DESTINATION ${LIB_PREFIX})
  set(CPACK_GENERATOR "WIX")
  configure_file(COPYING copying.txt COPYONLY)
  configure_file(README readme.txt COPYONLY)
  set(CPACK_RESOURCE_FILE_LICENSE copying.txt)
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/copying.txt" "${CMAKE_CURRENT_BINARY_DIR}/readme.txt" DESTINATION .)
  set(CPACK_PACKAGE_INSTALL_DIRECTORY "HARP")
  set(CPACK_WIX_UPGRADE_GUID "9CF2DA32-201D-4557-BC59-B655216EAD6D")
  set(CPACK_WIX_PRODUCT_GUID "FA7592A4-D91C-4401-863D-CE52DA95EB11")
  set(CPACK_WIX_PATCH_FILE "${CMAKE_CURRENT_SOURCE_DIR}/cpack_wix_patch.xml")
endif(WIN32)

include(CPack)

# Some autoconf variables that CMake does not use itself, but that are used in the config file substitutions.
# (leave this section at the end so other rules will use the unquoted VERSION value)
#
set(PACKAGE_NAME \"${CMAKE_PROJECT_NAME}\")
string(TOLOWER ${PACKAGE_NAME} PACKAGE)
set(PACKAGE_VERSION ${HARP_VERSION})
string(REPLACE "\"" "" PACKAGE_STRING "${PACKAGE_NAME} ${PACKAGE_VERSION}")
set(PACKAGE_STRING \"${PACKAGE_STRING}\")
set(VERSION \"${VERSION}\")
set(PACKAGE_TARNAME ${PACKAGE})
set(PACKAGE_BUGREPORT \"\")

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/config.h)
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/libharp/harp.h.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/harp.h)
