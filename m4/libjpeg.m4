# ST_CHECK_LIBJPEG
# ----------------
# Check for the availability of the libjpeg library
AC_DEFUN([ST_CHECK_LIBJPEG],
[JPEGLIB=
AC_CHECK_LIB(jpeg, jpeg_start_compress, ac_cv_lib_jpeg=yes, ac_cv_lib_jpeg=no)
if test $ac_cv_lib_jpeg = yes ; then
  JPEGLIB="-ljpeg"
fi
])# ST_CHECK_LIBJPEG
