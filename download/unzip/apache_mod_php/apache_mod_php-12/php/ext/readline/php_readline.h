/*
   +----------------------------------------------------------------------+
   | PHP Version 4                                                        |
   +----------------------------------------------------------------------+
   | Copyright (c) 1997-2003 The PHP Group                                |
   +----------------------------------------------------------------------+
   | This source file is subject to version 2.02 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available at through the world-wide-web at                           |
   | http://www.php.net/license/2_02.txt.                                 |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Author: Thies C. Arntzen <thies@thieso.net>                          |
   +----------------------------------------------------------------------+
*/

/* $Id: php_readline.h,v 1.1.1.7 2003/07/18 18:07:41 zarzycki Exp $ */

#ifndef PHP_READLINE_H
#define PHP_READLINE_H

#if HAVE_LIBREADLINE || HAVE_LIBEDIT
#ifdef ZTS 
#warning Readline module will *NEVER* be thread-safe
#endif

extern zend_module_entry readline_module_entry;
#define phpext_readline_ptr &readline_module_entry

#else

#define phpext_readline_ptr NULL

#endif /* HAVE_LIBREADLINE */

#endif /* PHP_READLINE_H */

