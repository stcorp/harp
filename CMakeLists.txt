cmake_minimum_required(VERSION 2.8)

project(HARP)

set(VERSION 0.1)
set(HARP_VERSION \"${VERSION}\")

# Set dynamic library version
set(LIBHARP_CURRENT 1)
set(LIBHARP_REVISION 0)
set(LIBHARP_AGE 0)
math(EXPR LIBHARP_MAJOR "${LIBHARP_CURRENT} - ${LIBHARP_AGE}")
set(LIBHARP_MINOR ${LIBHARP_AGE})

# HARP format version
set(HARP_FORMAT_VERSION_MAJOR 1)
set(HARP_FORMAT_VERSION_MINOR 0)
set(HARP_CONVENTION \"HARP-${HARP_FORMAT_VERSION_MAJOR}.${HARP_FORMAT_VERSION_MINOR}\")


set(CMAKE_MODULE_PATH
  ${HARP_SOURCE_DIR}/CMakeModules
  ${CMAKE_MODULE_PATH}
  CACHE INTERNAL "Local CMake modules")

include(CheckFunctionExists)
include(CheckLibraryExists)
include(CheckIncludeFile)
include(CheckSymbolExists)
include(CheckTypeSize)
include(TestBigEndian)

# Use RelWithDebInfo as default target 
#
IF(NOT CMAKE_BUILD_TYPE)
  SET(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING
      "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
      FORCE)
ENDIF(NOT CMAKE_BUILD_TYPE)

find_package(BISON)
find_package(FLEX)

find_package(HDF4)
if(NOT HDF4_FOUND)
  message(FATAL_ERROR "HDF4 libraries and/or header files are not found. Try setting the HDF4_LIBRARY_DIR and HDF4_INCLUDE_DIR cmake variables to the location of your HDF4 library and include files.")
endif(NOT HDF4_FOUND)

find_package(HDF5)
if(NOT HDF5_FOUND)
    message(FATAL_ERROR "HDF5 libraries and/or header files are not found. Try setting the HDF5_LIBRARY_DIR and HDF5_INCLUDE_DIR cmake variables to the location of your HDF5 library and include files.")
endif(NOT HDF5_FOUND)

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

include_directories(${CODA_INCLUDE_DIR} ${HDF4_INCLUDE_DIR} ${HDF5_INCLUDE_DIR})

if(MSVC)
# Suppress warnings from VC on deprecated POSIX names and unsafe functions
add_definitions(-D_CRT_NONSTDC_NO_DEPRECATE -D_CRT_SECURE_NO_WARNINGS)
endif(MSVC)

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

find_include(dlfcn.h HAVE_DLFCN_H)
find_include(inttypes.h HAVE_INTTYPES_H)
find_include(limits.h HAVE_LIMITS_H)
find_include(memory.h HAVE_MEMORY_H)
find_include(stdarg.h HAVE_STDARG_H)
find_include(stdint.h HAVE_STDINT_H)
find_include(stdlib.h HAVE_STDLIB_H)
find_include(string.h HAVE_STRING_H)
find_include(strings.h HAVE_STRINGS_H)
find_include(sys/stat.h HAVE_SYS_STAT_H)
find_include(sys/types.h HAVE_SYS_TYPES_H)
find_include(unistd.h HAVE_UNISTD_H)

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


include_directories(BEFORE ${HARP_BINARY_DIR} ${HARP_SOURCE_DIR}/libharp)

# Some autoconf variables that CMake does not use itself, but
# that are used in the config file substitutions.
#
set(PACKAGE_NAME \"${CMAKE_PROJECT_NAME}\")
string(TOLOWER ${PACKAGE_NAME} PACKAGE)
set(PACKAGE_VERSION ${HARP_VERSION})
string(REPLACE "\"" "" PACKAGE_STRING "${PACKAGE_NAME} ${PACKAGE_VERSION}")
set(PACKAGE_STRING \"${PACKAGE_STRING}\")
set(VERSION \"${VERSION}\")
set(PACKAGE_TARNAME ${PACKAGE})
set(PACKAGE_BUGREPORT \"\")

add_definitions(-DHAVE_CONFIG_H)

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/config.h)
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/libharp/harp.h.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/harp.h)

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
set(LIBHARP_SOURCES
  libharp/harp-action.h
  libharp/harp-action.c
  libharp/harp-action-analyze.c
  libharp/harp-action-lex.h
  libharp/harp-action-lex.c
  libharp/harp-action-list.c
  libharp/harp-action-parse.h
  libharp/harp-action-parse.c
  libharp/harp-analysis.c
  libharp/harp-area-mask.h
  libharp/harp-area-mask.c
  libharp/harp-aux-afgl86.c
  libharp/harp-aux-usstd76.c
  libharp/harp-chemistry.h
  libharp/harp-chemistry.c
  libharp/harp-collocation.h
  libharp/harp-collocation.c
  libharp/harp-constants.h
  libharp/harp-derived-variable.c
  libharp/harp-derived-variable-list.c
  libharp/harp-dimension-mask.h
  libharp/harp-dimension-mask.c
  libharp/harp-errno.c
  libharp/harp-filter.h
  libharp/harp-filter.c
  libharp/harp-filter-area.c
  libharp/harp-filter-collocation.h
  libharp/harp-filter-collocation.c
  libharp/harp-filter-point.c
  libharp/harp-filter-predicate.c
  libharp/harp-geometry-sphere-circle.c
  libharp/harp-geometry-sphere-euler.c
  libharp/harp-geometry-sphere-line.c
  libharp/harp-geometry-sphere-point.c
  libharp/harp-geometry-sphere-polygon.c
  libharp/harp-geometry-vector3d.c
  libharp/harp-geometry-util.c
  libharp/harp-geometry-wgs84.c
  libharp/harp-geometry.h
  libharp/harp-hdf4.c
  libharp/harp-hdf5.c
  libharp/harp-ingest-cci_l2_o3_lp.c
  libharp/harp-ingest-cci_l2_o3_np.c
  libharp/harp-ingest-cci_l2_o3_tc.c
  libharp/harp-ingest-cci_l3_o3_lp.c
  libharp/harp-ingest-cci_l3_o3_np.c
  libharp/harp-ingest-cci_l3_o3_tc.c
  libharp/harp-ingest-cci_l4_o3_np.c
  libharp/harp-ingest-geoms-lidar.c
  libharp/harp-ingest-geoms-mwr.c
  libharp/harp-ingest-geoms-ftir.c
  libharp/harp-ingest-geoms-uvvis-doas.c
  libharp/harp-ingest-gome2_l2.c
  libharp/harp-ingest-gosat_fts_l1b.c
  libharp/harp-ingest-gosat_fts_l2.c
  libharp/harp-ingest-hirdls_l2.c
  libharp/harp-ingest-iasi_l2.c
  libharp/harp-ingest-mip_nl__2p.c
  libharp/harp-ingest-mls_l2.c
  libharp/harp-ingest-omi_l2.c
  libharp/harp-ingest-omi_l3.c
  libharp/harp-ingest-s5p_l1b.c
  libharp/harp-ingest-s5p_l2.c
  libharp/harp-ingest-tes_l2.c
  libharp/harp-ingestion.h
  libharp/harp-ingestion.c
  libharp/harp-ingestion-doc.c
  libharp/harp-ingestion-module.c
  libharp/harp-ingestion-options.c
  libharp/harp-internal.h
  libharp/harp-interpolation.c
  libharp/harp-netcdf.c
  libharp/harp-predicate.h
  libharp/harp-predicate.c
  libharp/harp-product.c
  libharp/harp-sea-surface.c
  libharp/harp-units.h
  libharp/harp-units.c
  libharp/harp-utils.c
  libharp/harp-variable.c
  libharp/harp-vertical-profiles.h
  libharp/harp-vertical-profiles.c
  libharp/harp.c
  libharp/harp.h
  libharp/hashtable.c
  libharp/hashtable.h
  libharp/ipow.h)


file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/udunits2)
BISON_TARGET(utparser ${CMAKE_CURRENT_SOURCE_DIR}/udunits2/utparser.y ${CMAKE_CURRENT_BINARY_DIR}/udunits2/utparser.c)
FLEX_TARGET(utscanner ${CMAKE_CURRENT_SOURCE_DIR}/udunits2/utscanner.l ${CMAKE_CURRENT_BINARY_DIR}/lex.yy.c)
add_flex_bison_dependency(utscanner utparser)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/udunits2)

set(LIBUDUNITS2_SOURCES
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
  expat/utf8tab.h
  expat/xmlrole.h
  expat/xmltok.h
  expat/xmltok_impl.h
  udunits2/converter.c
  udunits2/converter.h
  udunits2/error.c
  udunits2/formatter.c
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

if(WIN32)
set(LIBUDUNITS2_SOURCES
  ${LIBUDUNITS2_SOURCES}
  udunits2/tsearch.c
  udunits2/tsearch.h)
endif(WIN32)

include_directories(BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/udunits2)

set(LIBNETCDF_SOURCES
  netcdf/attr.c
  netcdf/dim.c
  netcdf/error.c
  netcdf/fbits.h
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
  netcdf/t_nc.c
  netcdf/utf8proc.c
  netcdf/utf8proc.h
  netcdf/utf8proc_data.h
  netcdf/v1hpg.c
  netcdf/v2i.c
  netcdf/var.c)

include_directories(BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/expat ${CMAKE_CURRENT_SOURCE_DIR}/netcdf)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/harp.h DESTINATION include)

set(PKGDATADIR ${CMAKE_INSTALL_PREFIX}/share/harp)
add_definitions(-DDEFAULT_UDUNITS2_XML_PATH="${PKGDATADIR}/udunits2.xml")
add_library(harp SHARED ${LIBHARP_SOURCES} ${LIBUDUNITS2_SOURCES} ${LIBNETCDF_SOURCES})
target_link_libraries(harp ${CODA_LIBRARIES} ${HDF4_LIBRARIES} ${HDF5_LIBRARIES})
set_target_properties(harp PROPERTIES
  VERSION ${LIBHARP_MAJOR}.${LIBHARP_MINOR}.${LIBHARP_REVISION}
  SOVERSION ${LIBHARP_MAJOR})
if(WIN32)
  # Also set DLL compile flags
  set_target_properties(harp PROPERTIES COMPILE_FLAGS "-DLIBHARPDLL -DLIBHARPDLL_EXPORTS -DH5_BUILT_AS_DYNAMIC_LIB -DLIBCODADLL")
endif(WIN32)
install(TARGETS harp DESTINATION lib)

add_library(harp_static STATIC ${LIBHARP_SOURCES} ${LIBUDUNITS2_SOURCES} ${LIBNETCDF_SOURCES})

# On Windows, we want libharp.lib for static, harp.dll & harp.lib for shared.
# On Unix, we want libharp.a and libharp.so
if(WIN32)
  set_target_properties(harp_static PROPERTIES OUTPUT_NAME "libharp")
else(WIN32)
  set_target_properties(harp_static PROPERTIES OUTPUT_NAME "harp")
endif(WIN32)
install(TARGETS harp_static DESTINATION lib)

#  harpcheck
add_executable(harpcheck tools/harpcheck/harpcheck.c)
target_link_libraries(harpcheck harp ${CODA_LIBRARIES} ${HDF4_LIBRARIES} ${HDF5_LIBRARIES} ${MATHLIB})
if(WIN32)
  # Also set DLL compile flags
  set_target_properties(harpcheck PROPERTIES COMPILE_FLAGS "-DLIBHARPDLL")
endif(WIN32)
install(TARGETS harpcheck DESTINATION bin)

#  harpcollocate
set(HARPCOLLOCATE_SOURCES
  tools/harpcollocate/harpcollocate.c
  tools/harpcollocate/harpcollocate.h
  tools/harpcollocate/harpcollocate-matchup.c
  tools/harpcollocate/harpcollocate-parse.c
  tools/harpcollocate/harpcollocate-resample.c
  tools/harpcollocate/harpcollocate-result.c
  tools/harpcollocate/harpcollocate-update.c)
add_executable(harpcollocate ${HARPCOLLOCATE_SOURCES})
target_link_libraries(harpcollocate harp ${CODA_LIBRARIES} ${HDF4_LIBRARIES} ${HDF5_LIBRARIES} ${MATHLIB})
if(WIN32)
  # Also set DLL compile flags
  set_target_properties(harpcollocate PROPERTIES COMPILE_FLAGS "-DLIBHARPDLL")
endif(WIN32)
install(TARGETS harpcollocate DESTINATION bin)

#  harpconvert
add_executable(harpconvert tools/harpconvert/harpconvert.c)
target_link_libraries(harpconvert harp ${CODA_LIBRARIES} ${HDF4_LIBRARIES} ${HDF5_LIBRARIES} ${MATHLIB})
if(WIN32)
  # Also set DLL compile flags
  set_target_properties(harpconvert PROPERTIES COMPILE_FLAGS "-DLIBHARPDLL")
endif(WIN32)
install(TARGETS harpconvert DESTINATION bin)

#  harpdump
add_executable(harpdump tools/harpdump/harpdump.c)
target_link_libraries(harpdump harp ${CODA_LIBRARIES} ${HDF4_LIBRARIES} ${HDF5_LIBRARIES} ${MATHLIB})
if(WIN32)
  # Also set DLL compile flags
  set_target_properties(harpdump PROPERTIES COMPILE_FLAGS "-DLIBHARPDLL")
endif(WIN32)
install(TARGETS harpdump DESTINATION bin)

#  harpfilter
add_executable(harpfilter tools/harpfilter/harpfilter.c)
target_link_libraries(harpfilter harp ${CODA_LIBRARIES} ${HDF4_LIBRARIES} ${HDF5_LIBRARIES} ${MATHLIB})
if(WIN32)
  # Also set DLL compile flags
  set_target_properties(harpfilter PROPERTIES COMPILE_FLAGS "-DLIBHARPDLL")
endif(WIN32)
install(TARGETS harpfilter DESTINATION bin)


# product definition files
if(WIN32)
set(CODADEF_INSTALL_DIR definitions)
else(WIN32)
set(CODADEF_INSTALL_DIR share/harp/definitions)
endif(WIN32)

execute_process(COMMAND "${CMAKE_SOURCE_DIR}/codadef.sh" "--date" "${CMAKE_CURRENT_SOURCE_DIR}/definitions/ENVISAT_GOMOS"
  OUTPUT_VARIABLE "CODADEF_VERSION_ENVISAT_GOMOS"
  OUTPUT_STRIP_TRAILING_WHITESPACE)
set(CODADEF_ENVISAT_GOMOS ENVISAT_GOMOS-${CODADEF_VERSION_ENVISAT_GOMOS}.codadef)
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${CODADEF_ENVISAT_GOMOS}"
  COMMAND "${CMAKE_SOURCE_DIR}/codadef.sh" "${CMAKE_CURRENT_SOURCE_DIR}/definitions/ENVISAT_GOMOS"
  DEPENDS "${CMAKE_SOURCE_DIR}/codadef.sh")
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${CODADEF_ENVISAT_GOMOS} DESTINATION ${CODADEF_INSTALL_DIR})

execute_process(COMMAND "${CMAKE_SOURCE_DIR}/codadef.sh" "--date" "${CMAKE_CURRENT_SOURCE_DIR}/definitions/ENVISAT_MIPAS"
  OUTPUT_VARIABLE "CODADEF_VERSION_ENVISAT_MIPAS"
  OUTPUT_STRIP_TRAILING_WHITESPACE)
set(CODADEF_ENVISAT_MIPAS ENVISAT_MIPAS-${CODADEF_VERSION_ENVISAT_MIPAS}.codadef)
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${CODADEF_ENVISAT_MIPAS}"
  COMMAND "${CMAKE_SOURCE_DIR}/codadef.sh" "${CMAKE_CURRENT_SOURCE_DIR}/definitions/ENVISAT_MIPAS"
  DEPENDS "${CMAKE_SOURCE_DIR}/codadef.sh")
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${CODADEF_ENVISAT_MIPAS} DESTINATION ${CODADEF_INSTALL_DIR})

execute_process(COMMAND "${CMAKE_SOURCE_DIR}/codadef.sh" "--date" "${CMAKE_CURRENT_SOURCE_DIR}/definitions/ENVISAT_SCIAMACHY"
  OUTPUT_VARIABLE "CODADEF_VERSION_ENVISAT_SCIAMACHY"
  OUTPUT_STRIP_TRAILING_WHITESPACE)
set(CODADEF_ENVISAT_SCIAMACHY ENVISAT_SCIAMACHY-${CODADEF_VERSION_ENVISAT_SCIAMACHY}.codadef)
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${CODADEF_ENVISAT_SCIAMACHY}"
  COMMAND "${CMAKE_SOURCE_DIR}/codadef.sh" "${CMAKE_CURRENT_SOURCE_DIR}/definitions/ENVISAT_SCIAMACHY"
  DEPENDS "${CMAKE_SOURCE_DIR}/codadef.sh")
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${CODADEF_ENVISAT_SCIAMACHY} DESTINATION ${CODADEF_INSTALL_DIR})

execute_process(COMMAND "${CMAKE_SOURCE_DIR}/codadef.sh" "--date" "${CMAKE_CURRENT_SOURCE_DIR}/definitions//EPS"
  OUTPUT_VARIABLE "CODADEF_VERSION_EPS"
  OUTPUT_STRIP_TRAILING_WHITESPACE)
set(CODADEF_EPS EPS-${CODADEF_VERSION_EPS}.codadef)
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${CODADEF_EPS}"
  COMMAND "${CMAKE_SOURCE_DIR}/codadef.sh" "${CMAKE_CURRENT_SOURCE_DIR}/definitions/EPS"
  DEPENDS "${CMAKE_SOURCE_DIR}/codadef.sh")
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${CODADEF_EPS} DESTINATION ${CODADEF_INSTALL_DIR})

execute_process(COMMAND "${CMAKE_SOURCE_DIR}/codadef.sh" "--date" "${CMAKE_CURRENT_SOURCE_DIR}/definitions/ERS_GOME"
  OUTPUT_VARIABLE "CODADEF_VERSION_ERS_GOME"
  OUTPUT_STRIP_TRAILING_WHITESPACE)
set(CODADEF_ERS_GOME ERS_GOME-${CODADEF_VERSION_ERS_GOME}.codadef)
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${CODADEF_ERS_GOME}"
  COMMAND "${CMAKE_SOURCE_DIR}/codadef.sh" "${CMAKE_CURRENT_SOURCE_DIR}/definitions/ERS_GOME"
  DEPENDS "${CMAKE_SOURCE_DIR}/codadef.sh")
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${CODADEF_ERS_GOME} DESTINATION ${CODADEF_INSTALL_DIR})

add_custom_target(codadef ALL DEPENDS
  "${CMAKE_CURRENT_BINARY_DIR}/${CODADEF_ENVISAT_GOMOS}"
  "${CMAKE_CURRENT_BINARY_DIR}/${CODADEF_ENVISAT_MIPAS}"
  "${CMAKE_CURRENT_BINARY_DIR}/${CODADEF_ENVISAT_SCIAMACHY}"
  "${CMAKE_CURRENT_BINARY_DIR}/${CODADEF_EPS}"
  "${CMAKE_CURRENT_BINARY_DIR}/${CODADEF_ERS_GOME}")
