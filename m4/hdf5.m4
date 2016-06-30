# ST_CHECK_HDF5
# -------------
# Check for the availability of the HDF5 libraries and include files
AC_DEFUN([ST_CHECK_HDF5],
[AC_ARG_VAR([HDF5_LIB],[The HDF5 library directory. If not specified no extra LDFLAGS are set])
AC_ARG_VAR([HDF5_INCLUDE],[The HDF5 include directory. If not specified no extra CPPFLAGS are set])
AC_REQUIRE([ST_CHECK_LIBZ])
AC_REQUIRE([ST_CHECK_LIBSZ])
old_CPPFLAGS=$CPPFLAGS
old_LDFLAGS=$LDFLAGS
if test "$HDF5_LIB" != "" ; then
  LDFLAGS="-L$HDF5_LIB $LDFLAGS"
fi
if test "$HDF5_INCLUDE" != "" ; then
  CPPFLAGS="-I$HDF5_INCLUDE $CPPFLAGS"
fi
AC_CHECK_HEADERS(hdf5.h)
AC_CHECK_HEADERS(hdf5_hl.h)
AC_CHECK_LIB(hdf5, H5Fopen, ac_cv_lib_hdf5=yes, ac_cv_lib_hdf5=no, [ $ZLIB $SZLIB])
AC_CHECK_LIB(hdf5_hl, H5DSset_scale, ac_cv_lib_hdf5_hl=yes, ac_cv_lib_hdf5_hl=no, [ -lhdf5 $ZLIB $SZLIB])
if test $ac_cv_header_hdf5_h = no || test $ac_cv_header_hdf5_hl_h = no ||
   test $ac_cv_lib_hdf5 = no || test $ac_cv_lib_hdf5_hl = no ; then
  st_cv_have_hdf5=no
  CPPFLAGS=$old_CPPFLAGS
  LDFLAGS=$old_LDFLAGS
else
  st_cv_have_hdf5=yes
  HDF5LIBS="-lhdf5_hl -lhdf5 $ZLIB $SZLIB"
fi
AC_MSG_CHECKING(for HDF5 installation)
AC_MSG_RESULT($st_cv_have_hdf5)
if test $st_cv_have_hdf5 = yes ; then
  AC_DEFINE(HAVE_HDF5, 1, [Define to 1 if HDF5 is available.])
fi
])# ST_CHECK_HDF5
