ACLOCAL_AMFLAGS = -I m4

# *** general build settings ****

DOXYGEN = @DOXYGEN@
SPHINXBUILD = @SPHINXBUILD@
INDENT = @INDENT@

AM_CPPFLAGS = -Ilibharp -I$(srcdir)/libharp
AM_YFLAGS = -d
AM_LFLAGS = -olex.yy.c

BUILT_SOURCES =
CLEANFILES =
EXTRA_DIST =
INDENTFILES =
MAINTAINERCLEANFILES =

INSTALL_DATA_HOOK_TARGETS =
UNINSTALL_HOOK_TARGETS =
install-data-hook: $(INSTALL_DATA_HOOK_TARGETS)
uninstall-hook: $(UNINSTALL_HOOK_TARGETS)

.PHONY: doc

# *** targets ***

# programs

bin_PROGRAMS = harpcheck harpcollocate harpconvert harpdump harpmerge
noinst_PROGRAMS = findtypedef

# libraries (+ related files)

lib_LTLIBRARIES = libharp.la
noinst_LTLIBRARIES = libnetcdf.la libudunits2.la

if BUILD_IDL
idldlmexec_LTLIBRARIES = harp_idl.la
endif

if BUILD_MATLAB
matlabmexexec_LTLIBRARIES = harp_matlab.la
endif

if BUILD_PYTHON
harppython_PYTHON = \
	$(srcdir)/python/__init__.py \
	$(srcdir)/python/_harppy.py \
	python/_harpc.py
endif

# headers

include_HEADERS = libharp/harp.h

# *** build rules ***

# parser generator

# libharp
libharp_la_SOURCES = \
	libharp/harp-analysis.c \
	libharp/harp-area-mask.h \
	libharp/harp-area-mask.c \
	libharp/harp-aux-afgl86.c \
	libharp/harp-aux-usstd76.c \
	libharp/harp-binning.c \
	libharp/harp-chemistry.h \
	libharp/harp-chemistry.c \
	libharp/harp-collocation.c \
	libharp/harp-constants.h \
	libharp/harp-dataset.c \
	libharp/harp-derived-variable.c \
	libharp/harp-derived-variable-list.c \
	libharp/harp-dimension-mask.h \
	libharp/harp-dimension-mask.c \
	libharp/harp-csv.h \
	libharp/harp-csv.c \
	libharp/harp-errno.c \
	libharp/harp-filter.h \
	libharp/harp-filter.c \
	libharp/harp-filter-collocation.h \
	libharp/harp-filter-collocation.c \
	libharp/harp-geometry-sphere-circle.c \
	libharp/harp-geometry-sphere-euler.c \
	libharp/harp-geometry-sphere-line.c \
	libharp/harp-geometry-sphere-point.c \
	libharp/harp-geometry-sphere-polygon.c \
	libharp/harp-geometry-vector3d.c \
	libharp/harp-geometry-util.c \
	libharp/harp-geometry-wgs84.c \
	libharp/harp-geometry.h \
	libharp/harp-ingest-ace_fts_l2.c \
	libharp/harp-ingest-earlinet_l2.c \
	libharp/harp-ingest-aeolus_l1b.c \
	libharp/harp-ingest-aeolus_l2a.c \
	libharp/harp-ingest-aeolus_l2b.c \
	libharp/harp-ingest-calipso_l2.c \
	libharp/harp-ingest-cci_l2_aerosol.c \
	libharp/harp-ingest-cci_l2_ghg.c \
	libharp/harp-ingest-cci_l2_o3_lp.c \
	libharp/harp-ingest-cci_l2_o3_np.c \
	libharp/harp-ingest-cci_l2_o3_tc.c \
	libharp/harp-ingest-cci_l3_aerosol.c \
	libharp/harp-ingest-cci_l3_cloud.c \
	libharp/harp-ingest-cci_l3_ghg.c \
	libharp/harp-ingest-cci_l3_o3_lntoc.c \
	libharp/harp-ingest-cci_l3_o3_lp.c \
	libharp/harp-ingest-cci_l3_o3_np.c \
	libharp/harp-ingest-cci_l3_o3_tc.c \
	libharp/harp-ingest-cci_l3_o3_ttoc.c \
	libharp/harp-ingest-cci_l4_o3_np.c \
	libharp/harp-ingest-cloudnet_l2.c \
	libharp/harp-ingest-ecmwf-grib.c \
	libharp/harp-ingest-geoms-lidar.c \
	libharp/harp-ingest-geoms-mwr.c \
	libharp/harp-ingest-geoms-ftir.c \
	libharp/harp-ingest-geoms-uvvis-doas.c \
	libharp/harp-ingest-gome_l1.c \
	libharp/harp-ingest-gome_l2.c \
	libharp/harp-ingest-gome2_l1.c \
	libharp/harp-ingest-gome2_l2.c \
	libharp/harp-ingest-gomos_l1.c \
	libharp/harp-ingest-gomos_l2.c \
	libharp/harp-ingest-gosat_fts_l1b.c \
	libharp/harp-ingest-gosat_fts_l2.c \
	libharp/harp-ingest-hirdls_l2.c \
	libharp/harp-ingest-iasi_l1.c \
	libharp/harp-ingest-iasi_l2.c \
	libharp/harp-ingest-mipas_l1.c \
	libharp/harp-ingest-mipas_l2.c \
	libharp/harp-ingest-mls_l2.c \
	libharp/harp-ingest-npp-suomi-crimss_l2.c \
	libharp/harp-ingest-npp-suomi-omps_profiles_l2.c \
	libharp/harp-ingest-npp-suomi-omps_totals_l2.c \
	libharp/harp-ingest-npp-suomi-viirs_l2.c \
	libharp/harp-ingest-omi_l2.c \
	libharp/harp-ingest-omi_l3.c \
	libharp/harp-ingest-osiris_l2.c \
	libharp/harp-ingest-qa4ecv.c \
	libharp/harp-ingest-sciamachy_l1.c \
	libharp/harp-ingest-sciamachy_l2.c \
	libharp/harp-ingest-s5p_l1b.c \
	libharp/harp-ingest-s5p_l2.c \
	libharp/harp-ingest-smr_l2.c \
	libharp/harp-ingest-temis.c \
	libharp/harp-ingest-tes_l2.c \
	libharp/harp-ingestion.h \
	libharp/harp-ingestion.c \
	libharp/harp-ingestion-doc.c \
	libharp/harp-ingestion-module.c \
	libharp/harp-ingestion-options.c \
	libharp/harp-internal.h \
	libharp/harp-interpolation.c \
	libharp/harp-netcdf.c \
	libharp/harp-operation-parser.y \
	libharp/harp-operation-scanner.l \
	libharp/harp-operation.h \
	libharp/harp-operation.c \
	libharp/harp-product.c \
	libharp/harp-product-metadata.c \
	libharp/harp-program.h \
	libharp/harp-program.c \
	libharp/harp-regrid.c \
	libharp/harp-sea-surface.c \
	libharp/harp-units.c \
	libharp/harp-utils.c \
	libharp/harp-variable.c \
	libharp/harp-vertical-profiles.h \
	libharp/harp-vertical-profiles.c \
	libharp/harp.c \
	libharp/hashtable.h \
	libharp/hashtable.c \
	libharp/ipow.h
libharp_hdf4_files = libharp/harp-hdf4.c
libharp_hdf5_files = libharp/harp-hdf5.c
if WITH_HDF4
libharp_la_SOURCES += $(libharp_hdf4_files)
else
INDENTFILES += $(libharp_hdf4_files)
endif
if WITH_HDF5
libharp_la_SOURCES += $(libharp_hdf5_files)
else
INDENTFILES += $(libharp_hdf5_files)
endif
libharp_la_CPPFLAGS = -Inetcdf -I$(srcdir)/netcdf -Iudunits2 -I$(srcdir)/udunits2 $(AM_CPPFLAGS)
libharp_la_LDFLAGS = -no-undefined -version-info $(LIBHARP_CURRENT):$(LIBHARP_REVISION):$(LIBHARP_AGE)
libharp_la_LIBADD = @LTLIBOBJS@ libudunits2.la libnetcdf.la $(CODALIBS) $(HDF4LIBS) $(HDF5LIBS)
libharp_la_DEPENDENCIES = libudunits2.la libnetcdf.la
INDENTFILES += $(libharp_la_SOURCES) libharp/harp.h.in
BUILT_SOURCES += libharp/harp-operation-parser.h

# harpcheck

harpcheck_SOURCES = tools/harpcheck/harpcheck.c
harpcheck_LDADD = libharp.la
harpcheck_DEPENDENCIES = libharp.la
INDENTFILES += $(harpcheck_SOURCES)

# harpcollocate

harpcollocate_SOURCES = \
	tools/harpcollocate/harpcollocate.c \
	tools/harpcollocate/harpcollocate-matchup.c \
	tools/harpcollocate/harpcollocate-resample.c \
	tools/harpcollocate/harpcollocate-update.c
harpcollocate_LDADD = libharp.la
INDENTFILES += $(harpcollocate_SOURCES)

# harpconvert

harpconvert_SOURCES = tools/harpconvert/harpconvert.c
harpconvert_LDADD = libharp.la
INDENTFILES += $(harpconvert_SOURCES)

# harpdump

harpdump_SOURCES = tools/harpdump/harpdump.c
harpdump_LDADD = libharp.la
INDENTFILES += $(harpdump_SOURCES)

# harpmerge

harpmerge_SOURCES = tools/harpmerge/harpmerge.c
harpmerge_LDADD = libharp.la
INDENTFILES += $(harpmerge_SOURCES)

# libnetcdf

libnetcdf_la_SOURCES = \
	netcdf/attr.c \
	netcdf/dim.c \
	netcdf/error.c \
	netcdf/fbits.h \
	netcdf/harp_netcdf_mangle.h \
	netcdf/libvers.c \
	netcdf/nc.c \
	netcdf/nc.h \
	netcdf/nc3convert.h \
	netcdf/nc3local.h \
	netcdf/ncio.h \
	netcdf/ncx.c \
	netcdf/ncx.h \
	netcdf/netcdf.h \
	netcdf/onstack.h \
	netcdf/posixio.c \
	netcdf/putget.c \
	netcdf/rnd.h \
	netcdf/string.c \
	netcdf/utf8proc.c \
	netcdf/utf8proc.h \
	netcdf/utf8proc_data.h \
	netcdf/v1hpg.c \
	netcdf/v2i.c \
	netcdf/var.c
libnetcdf_la_CPPFLAGS = -Inetcdf -I$(srcdir)/netcdf
libnetcdf_la_LDFLAGS = -no-undefined

# libudunits2

libudunits2_la_SOURCES = \
	expat/xmlparse.c \
	expat/xmlrole.c \
	expat/xmltok.c \
	expat/ascii.h \
	expat/asciitab.h \
	expat/expat.h \
	expat/expat_external.h \
	expat/harp_expat_mangle.h \
	expat/iasciitab.h \
	expat/internal.h \
	expat/latin1tab.h \
	expat/nametab.h \
	expat/utf8tab.h \
	expat/xmlrole.h \
	expat/xmltok.h \
	expat/xmltok_impl.h \
	udunits2/converter.c \
	udunits2/converter.h \
	udunits2/error.c \
	udunits2/formatter.c \
	udunits2/harp_udunits2_mangle.h \
	udunits2/idToUnitMap.c \
	udunits2/idToUnitMap.h \
	udunits2/utparser.y \
	udunits2/prefix.c \
	udunits2/prefix.h \
	udunits2/utscanner.l \
	udunits2/status.c \
	udunits2/systemMap.c \
	udunits2/systemMap.h \
	udunits2/udunits2.h \
	udunits2/unitcore.c \
	udunits2/unitAndId.c \
	udunits2/unitAndId.h \
	udunits2/unitToIdMap.c \
	udunits2/unitToIdMap.h \
	udunits2/ut_free_system.c \
	udunits2/xml.c
libudunits2_la_CPPFLAGS = -Iudunits2 -I$(srcdir)/udunits2 -Iexpat -I$(srcdir)/expat -DDEFAULT_UDUNITS2_XML_PATH='"$(pkgdatadir)/udunits2.xml"' -DHARP_UDUNITS2_NAME_MANGLE
libudunits2_la_LDFLAGS = -no-undefined
BUILT_SOURCES += udunits2/utparser.h
pkgdata_DATA = \
	udunits2/udunits2.xml \
	udunits2/udunits2-accepted.xml \
	udunits2/udunits2-base.xml \
	udunits2/udunits2-common.xml \
	udunits2/udunits2-derived.xml \
	udunits2/udunits2-prefixes.xml
EXTRA_DIST += \
	$(pkgdata_DATA) \
	expat/xmltok_impl.c \
	expat/xmltok_ns.c \
	expat/COPYING \
	expat/README \
	udunits2/tsearch.c \
	udunits2/tsearch.h \
	udunits2/LICENSE \
	udunits2/README

# harp-python

python/_harpc.py: $(srcdir)/libharp/harp.h.in $(srcdir)/python/build.py
	@$(MKDIR_P) python
	$(PYTHON) $(srcdir)/python/build.py $(srcdir)/libharp/harp.h.in python/_harpc.py && touch python/_harpc.py # explicitly touch the file to make sure the timestamp gets updated

EXTRA_DIST += \
	python/_harpc.py \
	python/build.py

# harp-idl

HARP_IDL_SOURCE_FILES = idl/harp_idl.c
INDENTFILES += $(HARP_IDL_SOURCE_FILES)

if BUILD_IDL
harp_idl_la_SOURCES = $(HARP_IDL_SOURCE_FILES)
harp_idl_la_LDFLAGS = -module -shared -avoid-version
harp_idl_la_LIBADD = libharp.la
harp_idl_la_DEPENDENCIES = libharp.la
endif

HARP_IDL_DLMFILES = \
	idl/harp_idl.dlm

EXTRA_DIST += $(HARP_IDL_DLMFILES) \
	idl/harp_idl.def \
	idl/harp_idl.dlm.in

if BUILD_IDL
idldlmexec_DATA = $(HARP_IDL_DLMFILES)
endif

# harp-matlab

HARP_MATLAB_SOURCE_FILES = \
	matlab/harp-matlab.h \
	matlab/harp-matlab-product.c \
	matlab/harp_matlab.c
INDENTFILES += $(HARP_MATLAB_SOURCE_FILES)

if BUILD_MATLAB
harp_matlab_la_SOURCES = $(HARP_MATLAB_SOURCE_FILES)
harp_matlab_la_CPPFLAGS = -DMATLAB_MEX_FILE $(AM_CPPFLAGS)
harp_matlab_la_LDFLAGS = -module -Wno-uninitialized -shared -avoid-version -export-symbols-regex mexFunction -shrext @MEXEXT@
harp_matlab_la_LIBADD = libharp.la
harp_matlab_la_DEPENDENCIES = libharp.la
endif

HARP_MATLAB_MFILES = \
	matlab/harp_import.m \
	matlab/harp_export.m \
	matlab/harp_version.m

EXTRA_DIST += $(HARP_MATLAB_MFILES) \
	matlab/harp_matlab.def

if BUILD_MATLAB
matlabmexexec_DATA = $(HARP_MATLAB_MFILES)
endif

# *** product definition files ***

DEFINITION_FILES = \
	definitions/$(CODADEF_FILE_ACE_FTS) \
	definitions/$(CODADEF_FILE_ACSAF) \
	definitions/$(CODADEF_FILE_AEOLUS) \
	definitions/$(CODADEF_FILE_AURA_HIRDLS) \
	definitions/$(CODADEF_FILE_AURA_MLS) \
	definitions/$(CODADEF_FILE_AURA_OMI) \
	definitions/$(CODADEF_FILE_AURA_TES) \
	definitions/$(CODADEF_FILE_CALIPSO) \
	definitions/$(CODADEF_FILE_CLOUDNET) \
	definitions/$(CODADEF_FILE_EARLINET) \
	definitions/$(CODADEF_FILE_ECMWF) \
	definitions/$(CODADEF_FILE_ENVISAT_GOMOS) \
	definitions/$(CODADEF_FILE_ENVISAT_MIPAS) \
	definitions/$(CODADEF_FILE_ENVISAT_SCIAMACHY) \
	definitions/$(CODADEF_FILE_EPS) \
	definitions/$(CODADEF_FILE_ERS_GOME) \
	definitions/$(CODADEF_FILE_ESACCI_AEROSOL) \
	definitions/$(CODADEF_FILE_ESACCI_CLOUD) \
	definitions/$(CODADEF_FILE_ESACCI_GHG) \
	definitions/$(CODADEF_FILE_ESACCI_OZONE) \
	definitions/$(CODADEF_FILE_GEOMS) \
	definitions/$(CODADEF_FILE_GOSAT) \
	definitions/$(CODADEF_FILE_NPP_SUOMI) \
	definitions/$(CODADEF_FILE_ODIN_OSIRIS) \
	definitions/$(CODADEF_FILE_ODIN_SMR) \
	definitions/$(CODADEF_FILE_QA4ECV) \
	definitions/$(CODADEF_FILE_Sentinel5P) \
	definitions/$(CODADEF_FILE_TEMIS)

EXTRA_DIST += $(DEFINITION_FILES) codadef.sh
MAINTAINERCLEANFILES += $(DEFINITION_FILES)

definitions_DATA = $(DEFINITION_FILES)

@CODADEF_RULE_ACE_FTS@
@CODADEF_RULE_ACSAF@
@CODADEF_RULE_AEOLUS@
@CODADEF_RULE_AURA_HIRDLS@
@CODADEF_RULE_AURA_MLS@
@CODADEF_RULE_AURA_OMI@
@CODADEF_RULE_AURA_TES@
@CODADEF_RULE_CALIPSO@
@CODADEF_RULE_CLOUDNET@
@CODADEF_RULE_EARLINET@
@CODADEF_RULE_ECMWF@
@CODADEF_RULE_ENVISAT_GOMOS@
@CODADEF_RULE_ENVISAT_MIPAS@
@CODADEF_RULE_ENVISAT_SCIAMACHY@
@CODADEF_RULE_EPS@
@CODADEF_RULE_ERS_GOME@
@CODADEF_RULE_ESACCI_AEROSOL@
@CODADEF_RULE_ESACCI_CLOUD@
@CODADEF_RULE_ESACCI_GHG@
@CODADEF_RULE_ESACCI_OZONE@
@CODADEF_RULE_GEOMS@
@CODADEF_RULE_GOSAT@
@CODADEF_RULE_NPP_SUOMI@
@CODADEF_RULE_ODIN_OSIRIS@
@CODADEF_RULE_ODIN_SMR@
@CODADEF_RULE_QA4ECV@
@CODADEF_RULE_Sentinel5P@
@CODADEF_RULE_TEMIS@

# *** misc distribution files ***

EXTRA_DIST += \
	CHANGES \
	COPYING \
	DEVELOPER \
	README \
	INSTALL \
	bootstrap

# *** documentation ***

include harpdoc.mk

DOCFILES = $(HARP_DOCFILES)

RSTFILES = \
	doc/algorithms/common.rst \
	doc/algorithms/definitions.rst \
	doc/algorithms/derivations/aerosol_extinction_coefficient.rst \
	doc/algorithms/derivations/aerosol_optical_depth.rst \
	doc/algorithms/derivations/altitude.rst \
	doc/algorithms/derivations/altitude_bounds.rst \
	doc/algorithms/derivations/column_mass_density.rst \
	doc/algorithms/derivations/column_mass_mixing_ratio.rst \
	doc/algorithms/derivations/column_number_density.rst \
	doc/algorithms/derivations/column_volume_mixing_ratio.rst \
	doc/algorithms/derivations/datetime.rst \
	doc/algorithms/derivations/datetime_length.rst \
	doc/algorithms/derivations/datetime_start.rst \
	doc/algorithms/derivations/datetime_stop.rst \
	doc/algorithms/derivations/frequency.rst \
	doc/algorithms/derivations/geopotential.rst \
	doc/algorithms/derivations/geopotential_height.rst \
	doc/algorithms/derivations/latitude.rst \
	doc/algorithms/derivations/latitude_bounds.rst \
	doc/algorithms/derivations/longitude.rst \
	doc/algorithms/derivations/longitude_bounds.rst \
	doc/algorithms/derivations/mass_density.rst \
	doc/algorithms/derivations/mass_mixing_ratio.rst \
	doc/algorithms/derivations/molar_mass.rst \
	doc/algorithms/derivations/number_density.rst \
	doc/algorithms/derivations/partial_pressure.rst \
	doc/algorithms/derivations/pressure.rst \
	doc/algorithms/derivations/pressure_bounds.rst \
	doc/algorithms/derivations/relative_azimuth_angle.rst \
	doc/algorithms/derivations/relative_humidity.rst \
	doc/algorithms/derivations/scattering_angle.rst \
	doc/algorithms/derivations/sensor_azimuth_angle.rst \
	doc/algorithms/derivations/sensor_elevation_angle.rst \
	doc/algorithms/derivations/sensor_zenith_angle.rst \
	doc/algorithms/derivations/solar_azimuth_angle.rst \
	doc/algorithms/derivations/solar_elevation_angle.rst \
	doc/algorithms/derivations/solar_zenith_angle.rst \
	doc/algorithms/derivations/temperature.rst \
	doc/algorithms/derivations/viewing_azimuth_angle.rst \
	doc/algorithms/derivations/viewing_elevation_angle.rst \
	doc/algorithms/derivations/viewing_zenith_angle.rst \
	doc/algorithms/derivations/virtual_temperature.rst \
	doc/algorithms/derivations/volume_mixing_ratio.rst \
	doc/algorithms/derivations/wavelength.rst \
	doc/algorithms/derivations/wavenumber.rst \
	doc/algorithms/formula.rst \
	doc/algorithms/index.rst \
	doc/conventions/collocation_result.rst \
	doc/conventions/compatibility.rst \
	doc/conventions/datatypes.rst \
	doc/conventions/datetime.rst \
	doc/conventions/dimensions.rst \
	doc/conventions/filenames.rst \
	doc/conventions/global_attributes.rst \
	doc/conventions/hdf4.rst \
	doc/conventions/hdf5.rst \
	doc/conventions/index.rst \
	doc/conventions/netcdf3.rst \
	doc/conventions/variable_attributes.rst \
	doc/conventions/variable_names.rst \
	doc/conventions/variables.rst \
	doc/harpcheck.rst \
	doc/harpcollocate.rst \
	doc/harpconvert.rst \
	doc/harpdump.rst \
	doc/harpmerge.rst \
	doc/idl.rst \
	doc/index.rst \
	doc/libharp.rst \
	doc/libharp_collocation.rst \
	doc/libharp_dataset.rst \
	doc/libharp_documentation.rst \
	doc/libharp_error.rst \
	doc/libharp_general.rst \
	doc/libharp_product.rst \
	doc/libharp_product_metadata.rst \
	doc/libharp_variable.rst \
	doc/matlab.rst \
	doc/operations.rst \
	doc/python.rst \
	doc/tools.rst

EXTRA_DIST += \
	$(RSTFILES) \
	doc/conf.py \
	doc/static/custom.css \
	$(DOCFILES)

nobase_pkgdata_DATA = $(DOCFILES)

doc: harp_doc

harp_doc: harpconvert libharp/harp.h doc/Doxyfile
	@if test "x$(SPHINXBUILD)" = x ; then \
	  echo "Sphinx not available. HARP documentation was not (re)created." ; \
	elif test "x$(DOXYGEN)" = x ; then \
	  echo "Doxygen not available. HARP documentation was not (re)created." ; \
	else \
	  $(MAKE) docclean ; \
	  echo "creating HARP documentation" ; \
	  cd doc ; \
	  $(DOXYGEN) Doxyfile ; \
	  cd .. ; \
	  $(mkinstalldirs) doc/ingestions ; \
	  UDUNITS2_XML_PATH=$(srcdir)/udunits2/udunits2.xml ./harpconvert --generate-documentation doc/ingestions ; \
	  cp $(srcdir)/INSTALL doc/install.rst ; \
	  if test "x$(srcdir)" != "x$(builddir)" ; then \
	    cp $(srcdir)/doc/conf.py doc ; \
	    cp -r $(srcdir)/doc/static doc ; \
	    $(mkinstalldirs) doc/algorithms ; \
	    $(mkinstalldirs) doc/algorithms/derivations ; \
	    for file in $(RSTFILES) ; do \
	      cp $(srcdir)/$$file $$file ; \
	    done ; \
	  fi ; \
	  $(SPHINXBUILD) -a -b html -d doctrees doc $(srcdir)/doc/html ; \
	  echo "creating harpdoc.mk" ; \
	  $(MAKE) harpdoc_include ; \
	fi

docclean:
	@echo "removing HARP documentation"
	@rm -rf doctrees
	@rm -f doc/install.rst
	@rm -rf doc/ingestions
	@rm -rf doc/xml
	@if test "x$(srcdir)" != "x$(builddir)" ; then \
	  rm -rf doc/static ; \
	  rm -f doc/conf.py ; \
	  for file in $(RSTFILES) ; do \
	    rm -f $$file ; \
	  done ; \
	fi
	@rm -rf $(srcdir)/doc/html

harpdoc_include:
	@cd $(srcdir) && find doc/html -type f | LANG= sort | $(AWK) 'BEGIN {printf("HARP_DOCFILES =")}; {printf(" \\\n\t%s", $$0)}' > harpdoc.mk2
	@echo "" >> $(srcdir)/harpdoc.mk2
	@cd $(srcdir) && find doc/html -type f | LANG= sort | $(AWK) '{printf("%s:\n\t$$(MAKE) harp_doc\n", $$0)}' >> harpdoc.mk2
	@if diff $(srcdir)/harpdoc.mk $(srcdir)/harpdoc.mk2 >/dev/null 2>&1 ; then \
	  echo "harpdoc.mk is unchanged" ; \
	  rm -f $(srcdir)/harpdoc.mk2 ; \
	else \
	  mv $(srcdir)/harpdoc.mk2 $(srcdir)/harpdoc.mk ; \
	fi

# *** indent ***

findtypedef_SOURCES = findtypedef.l
findtypedef_LDADD = @LIBOBJS@

.indent.pro: $(srcdir)/.indent.pro.in findtypedef$(EXEEXT) $(INDENTFILES)
	@echo creating .indent.pro
	@$(RM) indent.types
	@touch indent.types
	@for file in $(INDENTFILES) ; do \
	  ./findtypedef `test -f $$file || echo '$(srcdir)/'`$$file ; \
	done >> indent.types
	@cp $(srcdir)/.indent.pro.in .indent.pro
	@sort -u indent.types | sed "s/^/-T /" >> .indent.pro
	@$(RM) indent.types

indent:
	@$(RM) .indent.pro
	@$(MAKE) .indent.pro
	@echo Indenting
	@test "x$(INDENTFILES)" = x || for file in $(INDENTFILES) ; do \
	  $(INDENT) `test -f $$file || echo '$(srcdir)/'`$$file ; \
	done

# *** CMake-specific ***

EXTRA_DIST += \
	CMakeLists.txt \
	CMakeModules/FindCODA.cmake \
	CMakeModules/FindHDF4.cmake \
	CMakeModules/FindHDF5.cmake \
	CMakeModules/FindJPEG.cmake \
	CMakeModules/FindSZIP.cmake \
	CMakeModules/FindZLIB.cmake \
	config.h.cmake.in \
	libharp/harp.h.cmake.in

config.h.cmake.in: config.h.in Makefile.am
	cat $(srcdir)/config.h.in | sed -e 's/#undef HARP_FORMAT_VERSION_MAJOR/#define HARP_FORMAT_VERSION_MAJOR @HARP_FORMAT_VERSION_MAJOR@/' -e 's/#undef HARP_FORMAT_VERSION_MINOR/#define HARP_FORMAT_VERSION_MINOR @HARP_FORMAT_VERSION_MINOR@/' -e 's/#[ ]*undef \(.*\)/#cmakedefine \1 \$${\1}/' > config.h.cmake.in2
	@if diff $(srcdir)/config.h.cmake.in config.h.cmake.in2 >/dev/null 2>&1 ; then \
	  echo "config.h.cmake.in is unchanged" ; \
	  rm -f config.h.cmake.in2 ; \
	else \
	  mv config.h.cmake.in2 $(srcdir)/config.h.cmake.in ; \
	fi

libharp/harp.h.cmake.in: libharp/harp.h.in Makefile.am
	cat $(srcdir)/libharp/harp.h.in | sed 's/#[ ]*undef \(.*\)/#cmakedefine \1 \$${\1}/' > libharp/harp.h.cmake.in2
	@if diff $(srcdir)/libharp/harp.h.cmake.in libharp/harp.h.cmake.in2 >/dev/null 2>&1 ; then \
	  echo "libharp/harp.h.cmake.in is unchanged" ; \
	  rm -f libharp/harp.h.cmake.in2 ; \
	else \
	  mv libharp/harp.h.cmake.in2 $(srcdir)/libharp/harp.h.cmake.in ; \
	fi

# *** cppcheck ***

cppcheck:
	cppcheck -q $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(libharp_la_CPPFLAGS) $(CPPFLAGS) $(srcdir)/libharp $(srcdir)/tools
