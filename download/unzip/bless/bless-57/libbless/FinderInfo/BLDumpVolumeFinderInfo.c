/*
 * Copyright (c) 2001-2005 Apple Computer, Inc. All rights reserved.
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
 *  BLDumpVolumeFinderInfo.c
 *  bless
 *
 *  Created by Shantonu Sen <ssen@apple.com> on Thu Apr 19 2001.
 *  Copyright (c) 2001-2005 Apple Computer, Inc. All rights reserved.
 *
 *  $Id: BLDumpVolumeFinderInfo.c,v 1.24 2006/02/20 22:49:54 ssen Exp $
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/param.h>

#include <CoreFoundation/CoreFoundation.h>

#include "bless.h"
#include "bless_private.h"

struct BootBlocks {
  short   id;
  long    entryPoint;
  short   version;
  short   pageFlags;
  Str15   system;
  Str15   shellApplication;
  Str15   debugger1;
  Str15   debugger2;
  Str15   startupScreen;
  Str15   startupApplication;
  char    otherStuff[1024 - (2+4+2+2+16+16+16+16+16+16)];
} __attribute__((packed));

typedef struct BootBlocks BootBlocks;

/*
 * 1. getattrlist on the mountpoint to get the volume id
 * 2. read in the finder words
 * 3. for the directories we're interested in, get the entries in /.vol
 */
int BLCreateVolumeInformationDictionary(BLContextPtr context, const char * mount,
					CFDictionaryRef *outDict) {
    uint32_t finderinfo[8];
    int err;
    uint32_t i;
    uint32_t dirID;
    CFMutableDictionaryRef dict = NULL;
    CFMutableArrayRef infarray = NULL;

    char blesspath[MAXPATHLEN];

    err = BLGetVolumeFinderInfo(context, mount, finderinfo);
    if(err) {
        return 1;
    }

    infarray = CFArrayCreateMutable(kCFAllocatorDefault,
				    8,
				    &kCFTypeArrayCallBacks);

    dict =  CFDictionaryCreateMutable(kCFAllocatorDefault, 3, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    for(i = 0; i< 8-2; i++) {
      CFMutableDictionaryRef word =
	CFDictionaryCreateMutable(kCFAllocatorDefault,6, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
      CFTypeRef val;
      
      dirID = finderinfo[i];
      blesspath[0] = '\0';
      
      err = BLLookupFileIDOnMount(context, mount, dirID, blesspath);
      
      val = CFNumberCreate(kCFAllocatorDefault, kCFNumberLongType, &dirID);
      CFDictionaryAddValue(word, CFSTR("Directory ID"), val);
      CFRelease(val); val = NULL;
      
      val = CFStringCreateWithCString(kCFAllocatorDefault, blesspath, kCFStringEncodingUTF8);
      CFDictionaryAddValue(word, CFSTR("Path"), val);
      CFRelease(val); val = NULL;

      if(strlen(blesspath) == 0 || 0 == strcmp(mount, "/")) {
	  val = CFStringCreateWithCString(kCFAllocatorDefault, blesspath, kCFStringEncodingUTF8);
      } else {
	  val = CFStringCreateWithCString(kCFAllocatorDefault, blesspath+strlen(mount), kCFStringEncodingUTF8);
      }
      CFDictionaryAddValue(word, CFSTR("Relative Path"), val);
      CFRelease(val); val = NULL;
      
      CFArrayAppendValue(infarray, word);
      CFRelease(word); word = NULL;
    }

    CFDictionaryAddValue(dict, CFSTR("Finder Info"),
			 infarray);

    CFRelease(infarray); infarray = NULL;

    {
        CFNumberRef vsdbref = NULL;
        uint64_t vsdb;
        vsdb = (*(uint64_t *)&finderinfo[8-2]);
        
        vsdbref = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &vsdb);
        CFDictionaryAddValue(dict, CFSTR("VSDB ID"), vsdbref);
        CFRelease(vsdbref); vsdbref = NULL;
    }

    
    {
        CFTypeRef val2;

      fbootstraptransfer_t        bbr;
      int                         fd;
      char                       bbPtr[kBootBlocksSize];
      
      CFMutableDictionaryRef bdict =
	CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFDataRef bdata = NULL;

      fd = open(mount, O_RDONLY);
      if (fd == -1) {
	contextprintf(context, kBLLogLevelError,  "Can't open volume mount point for %s\n", mount );
	return 2;
      }
  
      bbr.fbt_offset = 0;
      bbr.fbt_length = kBootBlocksSize;
      bbr.fbt_buffer = bbPtr;
  
      err = fcntl(fd, F_READBOOTSTRAP, &bbr);
      if (err) {
        contextprintf(context, kBLLogLevelError,  "Can't read boot blocks\n" );
        close(fd);
        return 3;
      } else {
        contextprintf(context, kBLLogLevelVerbose,  "Boot blocks read successfully\n" );
      }
      close(fd);

      if((uint16_t)CFSwapInt16BigToHost(*(uint16_t *)&bbPtr[0]) == kBootBlockTradOSSig) {
	  BootBlocks		*bb = (BootBlocks *)bbPtr;
    
	    val2 = CFStringCreateWithPascalString(kCFAllocatorDefault, bb->system, kCFStringEncodingMacRoman);
	    CFDictionaryAddValue(bdict, CFSTR("System"), val2);
	    CFRelease(val2); val2 = NULL;
    
	    val2 = CFStringCreateWithPascalString(kCFAllocatorDefault, bb->shellApplication, kCFStringEncodingMacRoman);
	    CFDictionaryAddValue(bdict, CFSTR("ShellApplication"), val2);
	    CFRelease(val2); val2 = NULL;
    
	    val2 = CFStringCreateWithPascalString(kCFAllocatorDefault, bb->debugger1, kCFStringEncodingMacRoman);
	    CFDictionaryAddValue(bdict, CFSTR("Debugger1"), val2);
	    CFRelease(val2); val2 = NULL;
    
	    val2 = CFStringCreateWithPascalString(kCFAllocatorDefault, bb->debugger2, kCFStringEncodingMacRoman);
	    CFDictionaryAddValue(bdict, CFSTR("Debugger2"), val2);
	    CFRelease(val2); val2 = NULL;
    
	    val2 = CFStringCreateWithPascalString(kCFAllocatorDefault, bb->startupScreen,  kCFStringEncodingMacRoman);
	    CFDictionaryAddValue(bdict, CFSTR("StartupScreen"), val2);
	    CFRelease(val2); val2 = NULL;
    
	    val2 = CFStringCreateWithPascalString(kCFAllocatorDefault, bb->startupApplication, kCFStringEncodingMacRoman);
	    CFDictionaryAddValue(bdict, CFSTR("StartupApplication"), val2);
	    CFRelease(val2); val2 = NULL;
    
      }

      bdata = CFDataCreate(kCFAllocatorDefault, (UInt8 *)bbPtr, 1024);
      CFDictionaryAddValue(bdict, CFSTR("Data"), bdata);
      CFRelease(bdata); bdata = NULL;

      CFDictionaryAddValue(dict, CFSTR("BootBlocks"),
			   bdict);
      CFRelease(bdict); bdict = NULL;
    }
    
    *outDict = dict;
    return 0;
}
