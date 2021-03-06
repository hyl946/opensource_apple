/*
 * Copyright (c) 2003-2005 Apple Computer, Inc. All rights reserved.
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
 *  BLGetRAIDBootData.c
 *  bless
 *
 *  Created by Shantonu Sen on 1/13/05.
 *  Copyright 2005-2005 Apple Computer, Inc. All rights reserved.
 *
 *
 *  $Id: BLGetRAIDBootDataForDevice.c,v 1.8 2006/02/20 22:49:58 ssen Exp $
 *
 */

#include <stdlib.h>
#include <unistd.h>

#include <mach/mach_error.h>

#include <IOKit/IOKitLib.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/storage/RAID/AppleRAIDUserLib.h>

#include <CoreFoundation/CoreFoundation.h>

#include "bless.h"
#include "bless_private.h"

int BLGetRAIDBootDataForDevice(BLContextPtr context, const char * device,
							   CFTypeRef *bootData)
{
	const char *name = NULL;
	kern_return_t           kret;
	mach_port_t             ourIOKitPort;
	io_service_t			service;
	io_iterator_t			serviter;
	CFTypeRef				isRAID = NULL, data = NULL;
	
	*bootData = NULL;
	
	if(!device || 0 != strncmp(device, "/dev/", 5)) return 1;

	name = device + 5;
	
	// Obtain the I/O Kit communication handle.
        if((kret = IOMasterPort(bootstrap_port, &ourIOKitPort)) != KERN_SUCCESS) {
            return 2;
        }	
	
	kret =  IOServiceGetMatchingServices(ourIOKitPort,
                                             IOBSDNameMatching(ourIOKitPort,
                                                               0, name),
                                             &serviter);
	if (kret != KERN_SUCCESS) {
            return 3;
        }
	
        service = IOIteratorNext(serviter);
        if (!service) {
            IOObjectRelease(serviter);
            return 3;
        }
        
	IOObjectRelease(serviter);
	
	isRAID = IORegistryEntrySearchCFProperty( service,
											  kIOServicePlane,
											  CFSTR(kAppleRAIDIsRAIDKey),
											  kCFAllocatorDefault,
											  kIORegistryIterateRecursively|
												kIORegistryIterateParents);

	if(isRAID == NULL
	   || CFGetTypeID(isRAID) != CFBooleanGetTypeID()
	   || !CFEqual(isRAID, kCFBooleanTrue)) {
		// no error, but the lack of boot data means it's not a raid
		if(isRAID) CFRelease(isRAID);
		IOObjectRelease(service);
		
		return 0;
	}

	CFRelease(isRAID);
	contextprintf(context, kBLLogLevelVerbose,  "%s is part of a RAID\n", device );
	
	// we know this IOService is a RAID member. Now we need to get the boot data
	data = IORegistryEntrySearchCFProperty( service,
											  kIOServicePlane,
											  CFSTR(kIOBootDeviceKey),
											  kCFAllocatorDefault,
											  kIORegistryIterateRecursively|
											  kIORegistryIterateParents);
	if(data == NULL) {
		// it's an error for a RAID not to have this information
		IOObjectRelease(service);
		return 4;
	}
	
	IOObjectRelease(service);
	
	if(CFGetTypeID(data) == CFArrayGetTypeID()) {

	} else if(CFGetTypeID(bootData) == CFDictionaryGetTypeID()) {
		
	} else {
		contextprintf(context, kBLLogLevelError,  "Invalid RAID boot data\n" );
		return 3;                
	}
	
	*bootData = data;
	
	return 0;
}

