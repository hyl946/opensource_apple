/*
 * Copyright (c) 2000-2004 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_OSREFERENCE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code 
 * as defined in and that are subject to the Apple Public Source License 
 * Version 2.0 (the 'License'). You may not use this file except in 
 * compliance with the License.  The rights granted to you under the 
 * License may not be used to create, or enable the creation or 
 * redistribution of, unlawful or unlicensed copies of an Apple operating 
 * system, or to circumvent, violate, or enable the circumvention or 
 * violation of, any terms of an Apple operating system software license 
 * agreement.
 *
 * Please obtain a copy of the License at 
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
 * @APPLE_LICENSE_OSREFERENCE_HEADER_END@
 */
/*
 * @OSF_COPYRIGHT@
 */

#ifdef	MACH_KERNEL_PRIVATE

#ifndef _KERN_LEDGER_H_
#define _KERN_LEDGER_H_

#include <mach/mach_types.h>

#include <kern/kern_types.h>
#include <kern/lock.h>
#include <ipc/ipc_types.h>

struct ledger {
        ipc_port_t	ledger_self;
        ipc_port_t	ledger_service_port;
        ledger_item_t	ledger_balance;
        ledger_item_t	ledger_limit;
        struct ledger	*ledger_ledger;
        struct ledger	*ledger_parent;
	decl_simple_lock_data(,lock)
};

typedef struct ledger ledger_data_t;

#define ledger_lock(ledger)	simple_lock(&(ledger)->lock)
#define ledger_unlock(ledger)	simple_unlock(&(ledger)->lock)
#define	ledger_lock_init(ledger) \
	simple_lock_init(&(ledger)->lock, 0)

extern ledger_t	root_wired_ledger;
extern ledger_t	root_paged_ledger;

#define root_wired_ledger_port root_wired_ledger->ledger_self
#define root_paged_ledger_port root_paged_ledger->ledger_self

extern void ledger_init(void);

extern ipc_port_t ledger_copy(ledger_t);

extern kern_return_t ledger_enter(ledger_t, ledger_item_t);

extern ledger_t convert_port_to_ledger(ipc_port_t);

extern ipc_port_t convert_ledger_to_port(ledger_t);

#endif	/* _KERN_LEDGER_H_ */

#endif /* MACH_KERNEL_PRIVATE */
