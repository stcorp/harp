ACLOCAL_AMFLAGS = -I coda/m4

SUBDIRS = coda

# *** general build settings ****

DOXYGEN = @DOXYGEN@
SPHINXBUILD = @SPHINXBUILD@
INDENT = @INDENT@

AM_CPPFLAGS = -Ilibharp -I$(srcdir)/libharp
AM_YFLAGS = -d -v

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

bin_PROGRAMS = harpcheck harpcollocate harpconvert harpfilter harpdump
noinst_PROGRAMS = findtypedef

# libraries (+ related files)

lib_LTLIBRARIES = libharp.la
noinst_LTLIBRARIES = libnetcdf.la libudunits2.la

# headers

include_HEADERS = libharp/harp.h

# *** build rules ***

# libharp

libharp_la_SOURCES = \
	libharp/harp-action.h \
	libharp/harp-action.c \
	libharp/harp-action-analyze.c \
	libharp/harp-action-lex.h \
	libharp/harp-action-lex.c \
	libharp/harp-action-list.c \
	libharp/harp-action-parse.h \
	libharp/harp-action-parse.c \
	libharp/harp-analysis.c \
	libharp/harp-area-mask.h \
	libharp/harp-area-mask.c \
	libharp/harp-aux-afgl86.c \
	libharp/harp-aux-usstd76.c \
	libharp/harp-chemistry.h \
	libharp/harp-chemistry.c \
	libharp/harp-collocation.h \
	libharp/harp-collocation.c \
	libharp/harp-constants.h \
	libharp/harp-derived-variable.c \
	libharp/harp-derived-variable-list.c \
	libharp/harp-dimension-mask.h \
	libharp/harp-dimension-mask.c \
	libharp/harp-errno.c \
	libharp/harp-filter.h \
	libharp/harp-filter.c \
	libharp/harp-filter-area.c \
	libharp/harp-filter-collocation.h \
	libharp/harp-filter-collocation.c \
	libharp/harp-filter-point.c \
	libharp/harp-filter-predicate.c \
	libharp/harp-geometry-sphere-circle.c \
	libharp/harp-geometry-sphere-euler.c \
	libharp/harp-geometry-sphere-line.c \
	libharp/harp-geometry-sphere-point.c \
	libharp/harp-geometry-sphere-polygon.c \
	libharp/harp-geometry-vector3d.c \
	libharp/harp-geometry-util.c \
	libharp/harp-geometry-wgs84.c \
	libharp/harp-geometry.h \
	libharp/harp-hdf4.c \
	libharp/harp-hdf5.c \
	libharp/harp-ingest-cci_l2_o3_lp.c \
	libharp/harp-ingest-cci_l2_o3_np.c \
	libharp/harp-ingest-cci_l2_o3_tc.c \
	libharp/harp-ingest-cci_l3_o3_lp.c \
	libharp/harp-ingest-cci_l3_o3_np.c \
	libharp/harp-ingest-cci_l3_o3_tc.c \
	libharp/harp-ingest-cci_l4_o3_np.c \
	libharp/harp-ingest-geoms-lidar.c \
	libharp/harp-ingest-geoms-mwr.c \
	libharp/harp-ingest-geoms-ftir.c \
	libharp/harp-ingest-geoms-uvvis-doas.c \
	libharp/harp-ingest-gome2_l2.c \
	libharp/harp-ingest-gosat_fts_l1b.c \
	libharp/harp-ingest-gosat_fts_l2.c \
	libharp/harp-ingest-hirdls_l2.c \
	libharp/harp-ingest-iasi_l2.c \
	libharp/harp-ingest-mip_nl__2p.c \
	libharp/harp-ingest-mls_l2.c \
	libharp/harp-ingest-omi_l2.c \
	libharp/harp-ingest-omi_l3.c \
	libharp/harp-ingest-s5p_l1b.c \
	libharp/harp-ingest-s5p_l2.c \
	libharp/harp-ingest-tes_l2.c \
	libharp/harp-ingestion.h \
	libharp/harp-ingestion.c \
	libharp/harp-ingestion-doc.c \
	libharp/harp-ingestion-module.c \
	libharp/harp-ingestion-options.c \
	libharp/harp-internal.h \
	libharp/harp-interpolation.c \
	libharp/harp-netcdf.c \
	libharp/harp-predicate.h \
	libharp/harp-predicate.c \
	libharp/harp-product.c \
	libharp/harp-sea-surface.c \
	libharp/harp-units.h \
	libharp/harp-units.c \
	libharp/harp-utils.c \
	libharp/harp-variable.c \
	libharp/harp-vertical-profiles.h \
	libharp/harp-vertical-profiles.c \
	libharp/harp.c \
	libharp/hashtable.h \
	libharp/hashtable.c \
	libharp/ipow.h
libharp_la_CPPFLAGS = -Icoda -I$(srcdir)/coda -Icoda/libcoda -I$(srcdir)/coda/libcoda -Inetcdf -I$(srcdir)/netcdf -Iudunits2 -I$(srcdir)/udunits2
libharp_la_LDFLAGS = -no-undefined -version-info $(LIBHARP_CURRENT):$(LIBHARP_REVISION):$(LIBHARP_AGE)
libharp_la_LIBADD = @LTLIBOBJS@ coda/libcoda_internal.la libudunits2.la libnetcdf.la $(HDF4LIBS) $(HDF5LIBS)
libharp_la_DEPENDENCIES = coda/libcoda_internal.la libudunits2.la libnetcdf.la coda/coda.h
INDENTFILES += $(libharp_la_SOURCES) libharp/harp.h.in

# harpcheck

harpcheck_SOURCES = tools/harpcheck/harpcheck.c
harpcheck_LDFLAGS = -static
harpcheck_LDADD = libharp.la
harpcheck_DEPENDENCIES = libharp.la
INDENTFILES += $(harpcheck_SOURCES)

# harpcollocate

harpcollocate_SOURCES = \
	tools/harpcollocate/harpcollocate.c \
	tools/harpcollocate/harpcollocate.h \
	tools/harpcollocate/harpcollocate-matchup.c \
	tools/harpcollocate/harpcollocate-parse.c \
	tools/harpcollocate/harpcollocate-resample.c \
	tools/harpcollocate/harpcollocate-result.c \
	tools/harpcollocate/harpcollocate-update.c
harpcollocate_CPPFLAGS = -Icoda -I$(srcdir)/coda $(AM_CPPFLAGS)
harpcollocate_LDFLAGS = -static
harpcollocate_LDADD = libharp.la
INDENTFILES += $(harpcollocate_SOURCES)

# harpconvert

harpconvert_SOURCES = tools/harpconvert/harpconvert.c
harpconvert_CPPFLAGS = -Icoda -I$(srcdir)/coda $(AM_CPPFLAGS)
harpconvert_LDFLAGS = -static
harpconvert_LDADD = libharp.la
INDENTFILES += $(harpconvert_SOURCES)

# harpdump

harpdump_SOURCES = tools/harpdump/harpdump.c
harpdump_CPPFLAGS = -Icoda -I$(srcdir)/coda $(AM_CPPFLAGS)
harpdump_LDFLAGS = -static
harpdump_LDADD = libharp.la
INDENTFILES += $(harpdump_SOURCES)

# harpfilter

harpfilter_SOURCES = tools/harpfilter/harpfilter.c
harpfilter_CPPFLAGS = -Icoda -I$(srcdir)/coda $(AM_CPPFLAGS)
harpfilter_LDFLAGS = -static
harpfilter_LDADD = libharp.la
INDENTFILES += $(harpfilter_SOURCES)

# libnetcdf

libnetcdf_la_SOURCES = \
	netcdf/attr.c \
	netcdf/dim.c \
	netcdf/error.c \
	netcdf/fbits.h \
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
	netcdf/t_nc.c \
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
	udunits2/idToUnitMap.c \
	udunits2/idToUnitMap.h \
	udunits2/utparser.y \
	udunits2/prefix.c \
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
libudunits2_la_CPPFLAGS = -Iudunits2 -I$(srcdir)/udunits2 -Iexpat -I$(srcdir)/expat -DDEFAULT_UDUNITS2_XML_PATH='"$(pkgdatadir)/udunits2.xml"'
libudunits2_la_LDFLAGS = -no-undefined
BUILT_SOURCES += udunits2/utparser.h
pkgdata_DATA = \
	udunits2.xml \
	udunits2-harp.xml \
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
	udunits2/LICENSE \
	udunits2/README

# coda

coda/coda.h:
	cd coda && $(MAKE) coda.h

coda/libcoda_internal.la:
	cd coda && $(MAKE) libcoda_internal.la


# *** product definition files ***

ENVISAT_GOMOS_DEFINITION_FILE = ENVISAT_GOMOS-@CODADEF_VERSION_ENVISAT_GOMOS@.codadef
ENVISAT_MIPAS_DEFINITION_FILE = ENVISAT_MIPAS-@CODADEF_VERSION_ENVISAT_MIPAS@.codadef
ENVISAT_SCIAMACHY_DEFINITION_FILE = ENVISAT_SCIAMACHY-@CODADEF_VERSION_ENVISAT_SCIAMACHY@.codadef
EPS_DEFINITION_FILE = EPS-@CODADEF_VERSION_EPS@.codadef
ERS_GOME_DEFINITION_FILE = ERS_GOME-@CODADEF_VERSION_ERS_GOME@.codadef

DEFINITION_FILES = \
	definitions/$(ENVISAT_GOMOS_DEFINITION_FILE) \
	definitions/$(ENVISAT_MIPAS_DEFINITION_FILE) \
	definitions/$(ENVISAT_SCIAMACHY_DEFINITION_FILE) \
	definitions/$(EPS_DEFINITION_FILE) \
	definitions/$(ERS_GOME_DEFINITION_FILE)

EXTRA_DIST += $(DEFINITION_FILES)
MAINTAINERCLEANFILES += $(DEFINITION_FILES)

nobase_pkgdata_DATA = $(DEFINITION_FILES)

definitions/$(ENVISAT_GOMOS_DEFINITION_FILE):
	test -d definitions || $(mkinstalldirs) definitions
	@$(srcdir)/coda/codadef.sh $(srcdir)/definitions/ENVISAT_GOMOS definitions
	@if test ! -f definitions/$(ENVISAT_GOMOS_DEFINITION_FILE) ; then \
	  echo definitions/$(ENVISAT_GOMOS_DEFINITION_FILE) not created. ; \
	  echo run './config.status --recheck' to update the .codadef version reference. ; \
	  exit 1 ; \
	 fi

definitions/$(ENVISAT_MIPAS_DEFINITION_FILE):
	test -d definitions || $(mkinstalldirs) definitions
	@$(srcdir)/coda/codadef.sh $(srcdir)/definitions/ENVISAT_MIPAS definitions
	@if test ! -f definitions/$(ENVISAT_MIPAS_DEFINITION_FILE) ; then \
	  echo definitions/$(ENVISAT_MIPAS_DEFINITION_FILE) not created. ; \
	  echo run './config.status --recheck' to update the .codadef version reference. ; \
	  exit 1 ; \
	 fi

definitions/$(ENVISAT_SCIAMACHY_DEFINITION_FILE):
	test -d definitions || $(mkinstalldirs) definitions
	@$(srcdir)/coda/codadef.sh $(srcdir)/definitions/ENVISAT_SCIAMACHY definitions
	@if test ! -f definitions/$(ENVISAT_SCIAMACHY_DEFINITION_FILE) ; then \
	  echo definitions/$(ENVISAT_SCIAMACHY_DEFINITION_FILE) not created. ; \
	  echo run './config.status --recheck' to update the .codadef version reference. ; \
	  exit 1 ; \
	 fi

definitions/$(EPS_DEFINITION_FILE):
	test -d definitions || $(mkinstalldirs) definitions
	@$(srcdir)/coda/codadef.sh $(srcdir)/definitions/EPS definitions
	@if test ! -f definitions/$(EPS_DEFINITION_FILE) ; then \
	  echo definitions/$(EPS_DEFINITION_FILE) not created. ; \
	  echo run './config.status --recheck' to update the .codadef version reference. ; \
	  exit 1 ; \
	 fi

definitions/$(ERS_GOME_DEFINITION_FILE):
	test -d definitions || $(mkinstalldirs) definitions
	@$(srcdir)/coda/codadef.sh $(srcdir)/definitions/ERS_GOME definitions
	@if test ! -f definitions/$(ERS_GOME_DEFINITION_FILE) ; then \
	  echo definitions/$(ERS_GOME_DEFINITION_FILE) not created. ; \
	  echo run './config.status --recheck' to update the .codadef version reference. ; \
	  exit 1 ; \
	 fi

# *** misc distribution files ***

EXTRA_DIST += \
	CHANGES \
	COPYING \
	README \
	INSTALL \
	bootstrap

# *** documentation ***

include harpdoc.mk

DOCFILES = $(HARP_DOCFILES)

RSTFILES = \
	doc/dataformats.rst \
	doc/harpcheck.rst \
	doc/harpcollocate.rst \
	doc/harpconvert.rst \
	doc/harpdump.rst \
	doc/harpfilter.rst \
	doc/index.rst \
	doc/libharp.rst \
	doc/libharp_algorithm.rst \
	doc/libharp_error.rst \
	doc/libharp_general.rst \
	doc/libharp_product.rst \
	doc/libharp_variable.rst

EXTRA_DIST += \
	$(RSTFILES) \
	doc/conf.py \
	doc/static/custom.css \
	$(DOCFILES)

nobase_pkgdata_DATA += $(DOCFILES)

doc: harp_doc

harp_doc: harpconvert libharp/harp.h
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
	  ./harpconvert --generate-documentation -f rst doc/ingestions ; \
	  cp $(srcdir)/INSTALL doc/install.rst ; \
	  if test "x$(srcdir)" != "x$(builddir)" ; then \
	    cp $(srcdir)/doc/conf.py doc ; \
	    cp -r $(srcdir)/doc/static doc ; \
	    for file in $(RSTFILES) ; do \
	      cp $(srcdir)/$$file doc ; \
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
	config.h.cmake.in \
	libharp/harp.h.cmake.in

config.h.cmake.in: config.h.in coda/cmakeify-dot-infiles.pl
	cat $(srcdir)/config.h.in | sed -e 's/#undef HARP_FORMAT_VERSION_MAJOR/#define HARP_FORMAT_VERSION_MAJOR @HARP_FORMAT_VERSION_MAJOR@/' -e 's/#undef HARP_FORMAT_VERSION_MINOR/#define HARP_FORMAT_VERSION_MINOR @HARP_FORMAT_VERSION_MINOR@/' -e 's/#undef \(.*\)/#cmakedefine \1 \$${\1}/' > config.h.cmake.in2
	@if diff $(srcdir)/config.h.cmake.in config.h.cmake.in2 >/dev/null 2>&1 ; then \
	  echo "config.h.cmake.in is unchanged" ; \
	  rm -f config.h.cmake.in2 ; \
	else \
	  mv config.h.cmake.in2 $(srcdir)/config.h.cmake.in ; \
	fi

libharp/harp.h.cmake.in: libharp/harp.h.in coda/cmakeify-dot-infiles.pl
	cat $(srcdir)/libharp/harp.h.in | sed 's/#undef \(.*\)/#cmakedefine \1 \$${\1}/' > libharp/harp.h.cmake.in2
	@if diff $(srcdir)/libharp/harp.h.cmake.in libharp/harp.h.cmake.in2 >/dev/null 2>&1 ; then \
	  echo "libharp/harp.h.cmake.in is unchanged" ; \
	  rm -f libharp/harp.h.cmake.in2 ; \
	else \
	  mv libharp/harp.h.cmake.in2 $(srcdir)/libharp/harp.h.cmake.in ; \
	fi

# *** cppcheck ***

cppcheck:
	cppcheck -q $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(libharp_la_CPPFLAGS) $(CPPFLAGS) libharp tools
