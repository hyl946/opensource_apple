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
   | Author: Marcus Boerger <helly@php.net>                               |
   +----------------------------------------------------------------------+
 */

/* $Id: uint32.h,v 1.1.2.2 2002/12/31 16:34:20 sebastian Exp $ */

/* incorporated from D.J.Bernstein's cdb-0.75 (http://cr.yp.to/cdb.html)*/

#ifndef UINT32_H
#define UINT32_H

#if SIZEOF_INT == 4
/* Most 32-bit and 64-bit systems have 32-bit ints */
typedef unsigned int uint32;
#elif SIZEOF_LONG == 4
/* 16-bit systems? */
typedef unsigned long uint32;
#else
#error Need type which holds 32 bits
#endif

void uint32_pack(char *out, uint32 in);
void uint32_unpack(const char *in, uint32 *out);

#endif
