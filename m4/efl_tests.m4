dnl Copyright (C) 2008 Vincent Torri <vtorri at univ-evry dot fr>
dnl That code is public domain and can be freely used or copied.

dnl Macro that check if tests programs are wanted and if yes, if
dnl the Check library is available.

dnl Usage: EFL_CHECK_TESTS([ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
dnl Define the automake conditionnal EFL_ENABLE_TESTS

AC_DEFUN([EFL_CHECK_TESTS],
[

dnl configure option

AC_ARG_ENABLE([tests],
   [AC_HELP_STRING([--enable-tests], [enable tests @<:@default=disabled@:>@])],
   [
    if test "x${enableval}" = "xyes" ; then
       _efl_enable_tests="yes"
    else
       _efl_enable_tests="no"
    fi
   ],
   [_efl_enable_tests="no"])

AC_MSG_CHECKING([whether tests are built])
AC_MSG_RESULT([${_efl_enable_tests}])

AC_REQUIRE([PKG_PROG_PKG_CONFIG])

if test "x${_efl_enable_tests}" = "xyes" ; then
   PKG_CHECK_MODULES([CHECK],
      [check >= 0.9.5],
      [dummy="yes"],
      [_efl_enable_tests="no"])
fi
if test "x${_efl_enable_tests}" = "xyes" ; then
   AC_DEFINE(HAVE_TESTS, 1, [Set to 1 if we enabled unit testing.])
fi

AM_CONDITIONAL(EFL_ENABLE_TESTS, test "x${_efl_enable_tests}" = "xyes")

AS_IF([test "x$_efl_enable_tests" = "xyes"], [$1], [$2])
])

dnl End of efl_tests.m4
