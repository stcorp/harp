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


include(CheckFunctionExists)
include(CheckLibraryExists)
include(CheckIncludeFile)
include(CheckSymbolExists)
include(CheckTypeSize)
include(TestBigEndian)

set(CMAKE_MODULE_PATH
  ${HARP_SOURCE_DIR}/CMakeModules
  ${CMAKE_MODULE_PATH}
  CACHE INTERNAL "Local CMake modules")

find_package(BISON)
find_package(FLEX)

find_package(HDF4)
find_package(HDF5)
find_package(CODA)

include_directories(${CODA_INCLUDE_DIR} ${HDF4_INCLUDE_DIR} ${HDF5_INCLUDE_DIR})

if(NOT MSVC)
set(MATHLIB m)
endif()


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


include_directories(${HARP_BINARY_DIR} ${HARP_SOURCE_DIR}/libharp)

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

include_directories(BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/netcdf)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/harp.h DESTINATION include)

set(PKGDATADIR ${CMAKE_INSTALL_PREFIX}/share/harp)
add_definitions(-DDEFAULT_UDUNITS2_XML_PATH="${PKGDATADIR}/udunits2.xml")
add_library(harp SHARED ${LIBHARP_SOURCES} ${LIBUDUNITS2_SOURCES} ${LIBNETCDF_SOURCES})
target_link_libraries(harp ${CODA_LIBRARIES} ${HDF4_LIBRARIES} ${HDF5_LIBRARIES})
set_target_properties(harp PROPERTIES
  VERSION ${LIBHARP_MAJOR}.${LIBHARP_MINOR}.${LIBHARP_REVISION}
  SOVERSION ${LIBHARP_MAJOR})
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

#  harpconvert
add_executable(harpconvert tools/harpconvert/harpconvert.c)
target_link_libraries(harpconvert harp_static ${CODA_LIBRARIES} ${HDF4_LIBRARIES} ${HDF5_LIBRARIES} ${MATHLIB})
install(TARGETS harpconvert DESTINATION bin)
