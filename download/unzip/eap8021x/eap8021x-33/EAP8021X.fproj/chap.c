
/*
 * Copyright (c) 2001-2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/* 
 * Modification History
 *
 * December 10, 2001	Dieter Siegmund (dieter@apple.com)
 * - created
 */

/*
 * Function: CHAP_md5_hash
 * Purpose:
 *   Compute the CHAP MD5 hash using the method described in
 *   RFC 1994.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <openssl/md5.h>
#include "chap.h"

void
chap_md5(char identifier, const char * password, int password_len,
	 const void * challenge, int challenge_len,
	 char * hash)
{
    int				len;
    char *			offset;
    char *			value = NULL;

    /* MD5 hash over the identifier + password + challenge */
    len = 1 + password_len + challenge_len;
    value = malloc(len);
    if (value == NULL) {
	return;
    }
    offset = value;
    *offset = identifier;
    offset += 1;
    bcopy(password, offset, password_len);
    offset += password_len;
    bcopy(challenge, offset, challenge_len);
    MD5(value, len, hash);
    free(value);
    return;
}
