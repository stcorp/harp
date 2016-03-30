# ST_CHECK_LIBSZ
# --------------
# Check for the availability of the libsz library
AC_DEFUN([ST_CHECK_LIBSZ],
[SZLIB=
AC_CHECK_LIB(sz, SZ_Compress, ac_cv_lib_sz=yes, ac_cv_lib_sz=no)
if test $ac_cv_lib_sz = yes ; then
  SZLIB="-lsz"
fi
])# ST_CHECK_LIBSZ
