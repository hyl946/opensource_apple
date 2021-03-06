/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * Modification History
 *
 * June 1, 2001			Allan Nathanson <ajn@apple.com>
 * - public API conversion
 *
 * March 24, 2000		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

#include <mach/mach.h>
#include <mach/mach_error.h>

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>
#include "SCDynamicStoreInternal.h"
#include "config.h"		/* MiG generated file */


CFDictionaryRef
SCDynamicStoreCopyMultiple(SCDynamicStoreRef	store,
			   CFArrayRef		keys,
			   CFArrayRef		patterns)
{
	SCDynamicStorePrivateRef	storePrivate = (SCDynamicStorePrivateRef)store;
	kern_return_t			status;
	CFDataRef			xmlKeys		= NULL;	/* keys (XML serialized) */
	xmlData_t			myKeysRef	= NULL;	/* keys (serialized) */
	CFIndex				myKeysLen	= 0;
	CFDataRef			xmlPatterns	= NULL;	/* patterns (XML serialized) */
	xmlData_t			myPatternsRef	= NULL;	/* patterns (serialized) */
	CFIndex				myPatternsLen	= 0;
	xmlDataOut_t			xmlDictRef;		/* dict (serialized) */
	CFIndex				xmlDictLen;
	CFDataRef			xmlDict;		/* dict (XML serialized) */
	CFDictionaryRef			dict;			/* dict (un-serialized) */
	int				sc_status;
	CFStringRef			xmlError;

	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("SCDynamicStoreCopyMultiple:"));
	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("  keys     = %@"), keys);
	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("  patterns = %@"), patterns);

	if (!store) {
		/* sorry, you must provide a session */
		_SCErrorSet(kSCStatusNoStoreSession);
		return NULL;
	}

	if (storePrivate->server == MACH_PORT_NULL) {
		_SCErrorSet(kSCStatusNoStoreServer);
		return NULL;	/* you must have an open session to play */
	}

	/* serialize the keys */
	if (keys) {
		xmlKeys = CFPropertyListCreateXMLData(NULL, keys);
		myKeysRef = (xmlData_t)CFDataGetBytePtr(xmlKeys);
		myKeysLen = CFDataGetLength(xmlKeys);
	}

	/* serialize the patterns */
	if (patterns) {
		xmlPatterns = CFPropertyListCreateXMLData(NULL, patterns);
		myPatternsRef = (xmlData_t)CFDataGetBytePtr(xmlPatterns);
		myPatternsLen = CFDataGetLength(xmlPatterns);
	}

	/* send the keys and patterns, fetch the associated result from the server */
	status = configget_m(storePrivate->server,
			     myKeysRef,
			     myKeysLen,
			     myPatternsRef,
			     myPatternsLen,
			     &xmlDictRef,
			     (int *)&xmlDictLen,
			     (int *)&sc_status);

	/* clean up */
	if (xmlKeys)		CFRelease(xmlKeys);
	if (xmlPatterns)	CFRelease(xmlPatterns);

	if (status != KERN_SUCCESS) {
		if (status != MACH_SEND_INVALID_DEST)
			SCLog(_sc_verbose, LOG_DEBUG, CFSTR("configget_m(): %s"), mach_error_string(status));
		(void) mach_port_destroy(mach_task_self(), storePrivate->server);
		storePrivate->server = MACH_PORT_NULL;
		_SCErrorSet(status);
		return NULL;
	}

	if (sc_status != kSCStatusOK) {
		status = vm_deallocate(mach_task_self(), (vm_address_t)xmlDictRef, xmlDictLen);
		if (status != KERN_SUCCESS) {
			SCLog(_sc_verbose, LOG_DEBUG, CFSTR("vm_deallocate(): %s"), mach_error_string(status));
			/* non-fatal???, proceed */
		}
		_SCErrorSet(sc_status);
		return NULL;
	}

	/* un-serialize the dict, return a value associated with the key */
	xmlDict = CFDataCreate(NULL, xmlDictRef, xmlDictLen);
	status = vm_deallocate(mach_task_self(), (vm_address_t)xmlDictRef, xmlDictLen);
	if (status != KERN_SUCCESS) {
		SCLog(_sc_verbose, LOG_DEBUG, CFSTR("vm_deallocate(): %s"), mach_error_string(status));
		/* non-fatal???, proceed */
	}
	dict = CFPropertyListCreateFromXMLData(NULL,
					       xmlDict,
					       kCFPropertyListImmutable,
					       &xmlError);
	CFRelease(xmlDict);
	if (!dict) {
		if (xmlError) {
			SCLog(_sc_verbose, LOG_DEBUG,
			       CFSTR("CFPropertyListCreateFromXMLData() dict: %@"),
			       xmlError);
			CFRelease(xmlError);
		}
		_SCErrorSet(kSCStatusFailed);
		return NULL;
	}

	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("  value    = %@"), dict);

	return dict;
}


CFPropertyListRef
SCDynamicStoreCopyValue(SCDynamicStoreRef store, CFStringRef key)
{
	SCDynamicStorePrivateRef	storePrivate = (SCDynamicStorePrivateRef)store;
	kern_return_t			status;
	CFDataRef			xmlKey;		/* key (XML serialized) */
	xmlData_t			myKeyRef;	/* key (serialized) */
	CFIndex				myKeyLen;
	xmlDataOut_t			xmlDataRef;	/* data (serialized) */
	CFIndex				xmlDataLen;
	CFDataRef			xmlData;	/* data (XML serialized) */
	CFPropertyListRef		data;		/* data (un-serialized) */
	int				newInstance;
	int				sc_status;
	CFStringRef			xmlError;

	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("SCDynamicStoreCopyValue:"));
	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("  key      = %@"), key);

	if (!store) {
		/* sorry, you must provide a session */
		_SCErrorSet(kSCStatusNoStoreSession);
		return NULL;
	}

	if (storePrivate->server == MACH_PORT_NULL) {
		_SCErrorSet(kSCStatusNoStoreServer);
		return NULL;	/* you must have an open session to play */
	}

	/* serialize the key */
	xmlKey = CFPropertyListCreateXMLData(NULL, key);
	myKeyRef = (xmlData_t)CFDataGetBytePtr(xmlKey);
	myKeyLen = CFDataGetLength(xmlKey);

	/* send the key & fetch the associated data from the server */
	status = configget(storePrivate->server,
			   myKeyRef,
			   myKeyLen,
			   &xmlDataRef,
			   (int *)&xmlDataLen,
			   &newInstance,
			   (int *)&sc_status);

	/* clean up */
	CFRelease(xmlKey);

	if (status != KERN_SUCCESS) {
		if (status != MACH_SEND_INVALID_DEST)
			SCLog(_sc_verbose, LOG_DEBUG, CFSTR("configget(): %s"), mach_error_string(status));
		(void) mach_port_destroy(mach_task_self(), storePrivate->server);
		storePrivate->server = MACH_PORT_NULL;
		_SCErrorSet(status);
		return NULL;
	}

	if (sc_status != kSCStatusOK) {
		status = vm_deallocate(mach_task_self(), (vm_address_t)xmlDataRef, xmlDataLen);
		if (status != KERN_SUCCESS) {
			SCLog(_sc_verbose, LOG_DEBUG, CFSTR("vm_deallocate(): %s"), mach_error_string(status));
			/* non-fatal???, proceed */
		}
		_SCErrorSet(sc_status);
		return NULL;
	}

	/* un-serialize the data, return a value associated with the key */
	xmlData = CFDataCreate(NULL, xmlDataRef, xmlDataLen);
	status = vm_deallocate(mach_task_self(), (vm_address_t)xmlDataRef, xmlDataLen);
	if (status != KERN_SUCCESS) {
		SCLog(_sc_verbose, LOG_DEBUG, CFSTR("vm_deallocate(): %s"), mach_error_string(status));
		/* non-fatal???, proceed */
	}
	data = CFPropertyListCreateFromXMLData(NULL,
					       xmlData,
					       kCFPropertyListImmutable,
					       &xmlError);
	CFRelease(xmlData);
	if (!data) {
		if (xmlError) {
			SCLog(_sc_verbose, LOG_DEBUG,
			       CFSTR("CFPropertyListCreateFromXMLData() data: %@"),
			       xmlError);
			CFRelease(xmlError);
		}
		_SCErrorSet(kSCStatusFailed);
		return NULL;
	}

	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("  value    = %@"), data);

	return data;
}
