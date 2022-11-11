AC_PREREQ(2.62)
AC_INIT(yaddns, 1.1, avd@patatrac.info)
AM_INIT_AUTOMAKE([foreign])

sinclude(m4/ax_cflags_gcc_option.m4)

# workaround autoconf setting default flags which we don't want
test ".$CFLAGS" = "." && CFLAGS=" "

# programs needed to build/install
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_RANLIB

# libs needed
AC_SEARCH_LIBS(clock_gettime, rt)

# config options
AC_ARG_ENABLE(debug,
        [  --enable-debug  compile yaddns with -g to easily debug])

AC_ARG_ENABLE(log-debug,
        [  --enable-log-debug  enable log debug message])

AC_ARG_ENABLE(log-color,
        [  --enable-log-color  enable log color message])

# pimp CFLAGS
if test "x$GCC" = "xyes"; then
   # gcc specific options
   CFLAGS="$CFLAGS -Wall -Wextra"
   AX_CFLAGS_GCC_OPTION(-Wfloat-equal)
   AX_CFLAGS_GCC_OPTION(-Wundef)
   AX_CFLAGS_GCC_OPTION(-Wshadow)
   AX_CFLAGS_GCC_OPTION(-Wpointer-arith)
#   AX_CFLAGS_GCC_OPTION(-Wcast-align)
   AX_CFLAGS_GCC_OPTION(-Wstrict-prototypes)
   AX_CFLAGS_GCC_OPTION(-Wstrict-overflow=5)
   AX_CFLAGS_GCC_OPTION(-Wwrite-strings)
   AX_CFLAGS_GCC_OPTION(-Waggregate-return)
   AX_CFLAGS_GCC_OPTION(-Wcast-qual)
   AX_CFLAGS_GCC_OPTION(-Wswitch-default)
#   AX_CFLAGS_GCC_OPTION(-Wswitch-enum)
   AX_CFLAGS_GCC_OPTION(-Wconversion)
   AX_CFLAGS_GCC_OPTION(-Wunreachable-code)
   AX_CFLAGS_GCC_OPTION(-Wformat=2)
   AX_CFLAGS_GCC_OPTION(-Wformat-y2k)
   AX_CFLAGS_GCC_OPTION(-Wno-format-extra-args)
   AX_CFLAGS_GCC_OPTION(-Wno-format-zero-length)
   AX_CFLAGS_GCC_OPTION(-Wformat-nonliteral)
   AX_CFLAGS_GCC_OPTION(-Wformat-security)
fi

if test "x$enable_debug" = "xyes"; then
   AC_MSG_RESULT(> enable debug)
   CFLAGS="$CFLAGS -g -DDEBUG"
fi

if test "x$enable_log_debug" = "xyes"; then
   AC_MSG_RESULT(> enable log debug)
   CFLAGS="$CFLAGS -DENABLE_LOG_DEBUG"
fi

if test "x$enable_log_color" = "xyes"; then
   AC_MSG_RESULT(> enable log color)
   CFLAGS="$CFLAGS -DENABLE_LOG_COLOR"
fi

# the generated files
AC_CONFIG_FILES([
Makefile
src/Makefile
src/services/Makefile
tests/Makefile
])

AC_OUTPUT
