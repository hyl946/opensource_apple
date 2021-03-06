/*
 * Copyright (c) 2000-2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
 * @OSF_COPYRIGHT@
 */

#ifndef	_SYNC_POLICY_H_
#define _SYNC_POLICY_H_

typedef int sync_policy_t;

/*
 *	These options define the wait ordering of the synchronizers
 */
#define SYNC_POLICY_FIFO		0x0
#define SYNC_POLICY_FIXED_PRIORITY	0x1
#define SYNC_POLICY_REVERSED		0x2
#define SYNC_POLICY_ORDER_MASK		0x3
#define SYNC_POLICY_LIFO		(SYNC_POLICY_FIFO|SYNC_POLICY_REVERSED)

/*
 *	These options provide addition (kernel-private) behaviors
 */
#ifdef	KERNEL_PRIVATE
#include <sys/appleapiopts.h>

#ifdef  __APPLE_API_EVOLVING

#define SYNC_POLICY_PREPOST		0x4

#endif  /* __APPLE_API_EVOLVING */

#endif 	/* KERNEL_PRIVATE */

#define SYNC_POLICY_MAX			0x7

#endif 	/*_SYNC_POLICY_H_*/
