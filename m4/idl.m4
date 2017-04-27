# ST_CHECK_IDL
# -------------
# Check for the availability of IDL
AC_DEFUN([ST_CHECK_IDL],
[AC_ARG_VAR(IDL, [The IDL root directory. If not specified '/usr/local/itt/idl/idl' is assumed])
if test "$IDL" = "" ; then
  IDL=/usr/local/itt/idl/idl
fi
AC_PATH_PROG(idl, idl, no, [$IDL/bin])
old_CPPFLAGS=$CPPFLAGS
CPPFLAGS="-I$IDL/external $CPPFLAGS"
AC_CHECK_HEADERS(export.h)
AC_MSG_CHECKING(for IDL installation)
if test $ac_cv_path_idl = no || test $ac_cv_header_export_h = no ; then
   st_cv_have_idl=no
   CPPFLAGS=$old_CPPFLAGS
else
   st_cv_have_idl=yes
fi
 AC_MSG_RESULT($st_cv_have_idl)
])# ST_CHECK_IDL
