# ST_CHECK_HDF4
# -------------
# Check for the availability of the HDF4 libraries and include files
AC_DEFUN([ST_CHECK_HDF4],
[AC_ARG_VAR([HDF4_LIB],[The HDF4 library directory. If not specified no extra LDFLAGS are set])
AC_ARG_VAR([HDF4_INCLUDE],[The HDF4 include directory. If not specified no extra CPPFLAGS are set])
AC_REQUIRE([ST_CHECK_LIBZ])
AC_REQUIRE([ST_CHECK_LIBJPEG])
AC_REQUIRE([ST_CHECK_LIBSZ])
old_CPPFLAGS=$CPPFLAGS
old_LDFLAGS=$LDFLAGS
if test "$HDF4_LIB" != "" ; then
  LDFLAGS="-L$HDF4_LIB $LDFLAGS"
fi
if test "$HDF4_INCLUDE" != "" ; then
  CPPFLAGS="-I$HDF4_INCLUDE $CPPFLAGS"
fi
AC_CHECK_HEADERS(hdf.h)
AC_CHECK_HEADERS(netcdf.h)
AC_CHECK_HEADERS(mfhdf.h)
AC_CHECK_LIB(df, Hopen, ac_cv_lib_df=yes, ac_cv_lib_df=no, [ $ZLIB $JPEGLIB $SZLIB])
AC_CHECK_LIB(mfhdf, SDstart, ac_cv_lib_mfhdf=yes, ac_cv_lib_mfhdf=no, [ -ldf $ZLIB $JPEGLIB $SZLIB])
if test $ac_cv_header_hdf_h = no || test $ac_cv_header_mfhdf_h = no ||
   test $ac_cv_lib_df = no || test $ac_cv_lib_mfhdf = no ; then
  st_cv_have_hdf4=no
  CPPFLAGS=$old_CPPFLAGS
  LDFLAGS=$old_LDFLAGS
else
  st_cv_have_hdf4=yes
  HDF4LIBS="-lmfhdf -ldf $ZLIB $JPEGLIB $SZLIB"
fi
AC_MSG_CHECKING(for HDF4 installation)
AC_MSG_RESULT($st_cv_have_hdf4)
if test $st_cv_have_hdf4 = yes ; then
  AC_DEFINE(HAVE_HDF4, 1, [Define to 1 if HDF4 is available.])
fi
])# ST_CHECK_HDF4
