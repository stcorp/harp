# ST_CHECK_LIBZ
# -------------
# Check for the availability of the libz library
AC_DEFUN([ST_CHECK_LIBZ],
[ZLIB=
AC_CHECK_LIB(z, compress, ac_cv_lib_z=yes, ac_cv_lib_z=no)
if test $ac_cv_lib_z = yes ; then
  ZLIB="-lz"
fi
])# ST_CHECK_LIBZ
