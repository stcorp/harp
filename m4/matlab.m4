# ST_CHECK_MATLAB
# ---------------
# Check for the availability of MATLAB
AC_DEFUN([ST_CHECK_MATLAB],
[AC_ARG_VAR(MATLAB,[The MATLAB root directory. If not specified '/usr/local/matlab' is assumed])
if test "$MATLAB" = "" ; then
  MATLAB=/usr/local/matlab
fi
AC_PATH_PROG(matlab, matlab, no, [$MATLAB/bin])
old_CPPFLAGS=$CPPFLAGS
CPPFLAGS="-I$MATLAB/extern/include $CPPFLAGS"
AC_CHECK_HEADERS(mex.h)
ST_CHECK_MEX_EXTENSION
AC_MSG_CHECKING(for MATLAB installation)
if test $ac_cv_path_matlab = no || test $ac_cv_header_mex_h = no || test "$MEXEXT" = "" ; then
  st_cv_have_matlab=no
  CPPFLAGS=$old_CPPFLAGS
else
  st_cv_have_matlab=yes
fi
AC_MSG_RESULT($st_cv_have_matlab)
])# ST_CHECK_MATLAB


# ST_CHECK_MEX_EXTENSION
# ----------------------
# Determine the extension that is used for MATLAB MEX modules
AC_DEFUN([ST_CHECK_MEX_EXTENSION],
[AC_MSG_CHECKING([for mex extension])
AC_CANONICAL_HOST
case $host in
    x86_64-unknown-linux-gnu)
        MEXEXT=.mexa64
        ;;
    i*86-pc-linux-*)
        MEXEXT=.mexglx
        ;;
    i386-apple-darwin*)
        MEXEXT=.mexmaci
        ;;
    x86_64-apple-darwin*)
        MEXEXT=.mexmaci64
        ;;
    *-pc-mingw32)
        MEXEXT=.mexw32
        ;;
    *-pc-mingw64)
        MEXEXT=.mexw64
        ;;
    *)
    	MEXEXT=
esac
if test "$MEXEXT" != "" ; then
  AC_MSG_RESULT([$MEXEXT])
else
  AC_MSG_RESULT([not found])
fi 
AC_SUBST(MEXEXT)
])# ST_CHECK_MEX_EXTENSION
