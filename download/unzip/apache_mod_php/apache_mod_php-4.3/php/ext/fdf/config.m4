dnl $Id: config.m4,v 1.1.1.2 2001/01/25 04:59:15 wsanchez Exp $

PHP_ARG_WITH(fdftk, for fdftk support,
[  --with-fdftk[=DIR]      Include fdftk support])

if test "$PHP_FDFTK" != "no"; then
  if test -r $PHP_FDFTK/include/FdfTk.h; then
    FDFTK_DIR=$PHP_FDFTK
  else
    AC_MSG_CHECKING(for fdftk in default path)
    for i in /usr/local /usr; do
      if test -r $i/include/FdfTk.h; then
        FDFTK_DIR=$i
        AC_MSG_RESULT(found in $i)
      fi
    done
  fi

  if test -z "$FDFTK_DIR"; then
    AC_MSG_RESULT(not found)
    AC_MSG_ERROR(Please reinstall the fdftk distribution)
  fi

  AC_ADD_INCLUDE($FDFTK_DIR/include)
  
  old_LIBS=$LIBS
  LIBS="$LIBS -L$FDFTK_DIR/lib -lm"
  AC_CHECK_LIB(FdfTk, FDFOpen, [AC_DEFINE(HAVE_FDFLIB,1,[ ])],
     [AC_MSG_ERROR(fdftk module requires fdftk 2.0)])
  LIBS=$old_LIBS

  PHP_SUBST(FDFTK_SHARED_LIBADD)
  AC_ADD_LIBRARY_WITH_PATH(FdfTk, $FDFTK_DIR/lib, FDFTK_SHARED_LIBADD)

  PHP_EXTENSION(fdf, $ext_shared)
fi

