# VL_PROG_CC_WARNINGS
# -------------------
# Determine common warning flags for the C compiler
AC_DEFUN([VL_PROG_CC_WARNINGS],
[ansi=$1
if test -z "$ansi"; then
  msg="for C compiler warning flags"
else
  msg="for C compiler warning and ANSI conformance flags"
fi
AC_CACHE_CHECK([$msg], vl_cv_prog_cc_warnings,
[if test -n "$CC"; then
  cat > conftest.c <<EOF
int main(int argc, char **argv) { return 0; }
EOF

  # GCC
  if test "$GCC" = "yes"; then
    if test -z "$ansi"; then
      vl_cv_prog_cc_warnings="-W -Wall"
    else
      vl_cv_prog_cc_warnings="-W -Wall -ansi -pedantic"
    fi

  # Most compilers print some kind of a version string with some command
  # line options (often "-V").  The version string should be checked
  # before doing a test compilation run with compiler-specific flags.
  # This is because some compilers (like the Cray compiler) only
  # produce a warning message for unknown flags instead of returning
  # an error, resulting in a false positive. Also, compilers may do
  # erratic things when invoked with flags meant for a different
  # compiler.

  # Solaris C compiler
  elif $CC -V 2>&1 | grep -i "WorkShop" > /dev/null 2>&1 &&
       $CC -c -v -Xc conftest.c > /dev/null 2>&1 &&
       test -f conftest.o; then
    if test -z "$ansi"; then
      vl_cv_prog_cc_warnings="-v"
    else
      vl_cv_prog_cc_warnings="-v -Xc"
    fi

  # Digital Unix C compiler
  elif $CC -V 2>&1 | grep -i "Digital UNIX Compiler" > /dev/null 2>&1 &&
       $CC -c -verbose -w0 -warnprotos -std1 conftest.c > /dev/null 2>&1 &&
       test -f conftest.o; then
    if test -z "$ansi"; then
      vl_cv_prog_cc_warnings="-verbose -w0 -warnprotos"
    else
      vl_cv_prog_cc_warnings="-verbose -w0 -warnprotos -std1"
    fi

  # C for AIX Compiler
  elif $CC 2>&1 | grep -i "C for AIX Compiler" > /dev/null 2>&1 &&
       $CC -c -qlanglvl=ansi -qinfo=all conftest.c > /dev/null 2>&1 &&
       test -f conftest.o; then
    if test -z "$ansi"; then
       vl_cv_prog_cc_warnings="-qsrcmsg -qinfo=all:noppt:noppc:noobs:nocnd"
    else
       vl_cv_prog_cc_warnings="-qsrcmsg -qinfo=all:noppt:noppc:noobs:nocnd -qlanglvl=ansi"
    fi

  # IRIX C compiler
  elif $CC -version 2>&1 | grep -i "MIPSpro Compilers" > /dev/null 2>&1 &&
       $CC -c -fullwarn -ansi -ansiE conftest.c > /dev/null 2>&1 &&
       test -f conftest.o; then
    if test -z "$ansi"; then
      vl_cv_prog_cc_warnings="-fullwarn"
    else
      vl_cv_prog_cc_warnings="-fullwarn -ansi -ansiE"
    fi

  # HP-UX C compiler
  elif what $CC 2>&1 | grep -i "HP C Compiler" > /dev/null 2>&1 &&
       $CC -c -Aa +w1 conftest.c > /dev/null 2>&1 &&
       test -f conftest.o; then
    if test -z "$ansi"; then
      vl_cv_prog_cc_warnings="+w1"
    else
      vl_cv_prog_cc_warnings="+w1 -Aa"
    fi

  # The NEC SX-5 (Super-UX 10) C compiler
  elif $CC -V 2>&1 | grep "/SX" > /dev/null 2>&1 &&
       $CC -c -pvctl[,]fullmsg -Xc conftest.c > /dev/null 2>&1 &&
       test -f conftest.o; then
    if test -z "$ansi"; then
      vl_cv_prog_cc_warnings="-pvctl[,]fullmsg"
    else
      vl_cv_prog_cc_warnings="-pvctl[,]fullmsg -Xc"
    fi

  # The Cray C compiler (Unicos)
  elif $CC -V 2>&1 | grep -i "Cray" > /dev/null 2>&1 &&
       $CC -c -h msglevel 2 conftest.c > /dev/null 2>&1 &&
       test -f conftest.o; then
    if test -z "$ansi"; then
      vl_cv_prog_cc_warnings="-h msglevel 2"
    else
      vl_cv_prog_cc_warnings="-h msglevel 2 -h conform"
    fi
  
  fi
  rm -f conftest.*
fi
if test -n "$vl_cv_prog_cc_warnings"; then
  CFLAGS="$vl_cv_prog_cc_warnings $CFLAGS"
else
  vl_cv_prog_cc_warnings="unknown"
fi])
])# VL_PROG_CC_WARNINGS
