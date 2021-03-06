# quotearg.m4 serial 2
dnl Copyright (C) 2002, 2004 Free Software Foundation, Inc.
dnl This file is free software, distributed under the terms of the GNU
dnl General Public License.  As a special exception to the GNU General
dnl Public License, this file may be distributed as part of a program
dnl that contains a configuration script generated by Autoconf, under
dnl the same distribution terms as the rest of that program.

AC_DEFUN([gl_QUOTEARG],
[
  dnl Prerequisites of lib/quotearg.c.
  AC_CHECK_HEADERS_ONCE(wchar.h wctype.h)
  AC_CHECK_FUNCS_ONCE(iswprint mbsinit)
  AC_TYPE_MBSTATE_T
  gl_FUNC_MBRTOWC
])
