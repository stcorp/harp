# ST_CHECK_LIB_M
# --------------
# Check whether we need to explicitly link with the math library (-lm)
AC_DEFUN([ST_CHECK_LIB_M],
         [AC_MSG_CHECKING([whether explicit linking with math library is needed])
AC_TRY_LINK([char sqrt();], [sqrt();], st_need_math_lib=no, st_need_math_lib=yes)
AC_MSG_RESULT([$st_need_math_lib])
if test $st_need_math_lib = yes ; then
  AC_CHECK_LIB([m], [sqrt])
fi
])# ST_CHECK_LIB
