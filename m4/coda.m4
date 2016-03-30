# ST_CHECK_CODA
# -------------
# Check for the availability of the CODA library and include file
AC_DEFUN([ST_CHECK_CODA],
[AC_ARG_VAR([CODA_LIB],[The CODA library directory. If not specified no extra LDFLAGS are set])
AC_ARG_VAR([CODA_INCLUDE],[The CODA include directory. If not specified no extra CPPFLAGS are set])
old_CPPFLAGS=$CPPFLAGS
old_LDFLAGS=$LDFLAGS
if test "$CODA_LIB" != "" ; then
  LDFLAGS="-L$CODA_LIB $LDFLAGS"
fi
if test "$CODA_INCLUDE" != "" ; then
  CPPFLAGS="-I$CODA_INCLUDE $CPPFLAGS"
fi
AC_CHECK_HEADERS(coda.h)
AC_CHECK_LIB(coda, coda_init, ac_cv_lib_coda=yes, ac_cv_lib_coda=no, [ $HDF4LIBS $HDF5LIBS])
if test $ac_cv_header_coda_h = no || test $ac_cv_lib_coda = no ; then
  st_cv_have_coda=no
  CPPFLAGS=$old_CPPFLAGS
  LDFLAGS=$old_LDFLAGS
else
  st_cv_have_coda=yes
  CODALIBS="-lcoda"
fi
AC_MSG_CHECKING(for CODA installation)
AC_MSG_RESULT($st_cv_have_coda)
if test $st_cv_have_coda = yes ; then
  AC_DEFINE(HAVE_CODA, 1, [Define to 1 if CODA is available.])
fi
])# ST_CHECK_CODA
