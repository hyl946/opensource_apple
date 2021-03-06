/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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

#include <mach/mach.h>
#include <mach/thread_switch.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFBundlePriv.h>

#include <IOKit/IOKitLib.h>
#include <libkern/OSByteOrder.h>
#include <IOKit/graphics/IOGraphicsLib.h>
#include <IOKit/graphics/IOGraphicsLibPrivate.h>
#include <IOKit/graphics/IOGraphicsTypesPrivate.h>
#include <IOKit/graphics/IOGraphicsEngine.h>
#include <IOKit/iokitmig.h>

#include "IOGraphicsLibInternal.h"

#define DEBUGPARAMS		0


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

Boolean _IOReadBytesFromFile(CFAllocatorRef alloc, const char *path, void **bytes,
				CFIndex *length, CFIndex maxLength);
Boolean _IOWriteBytesToFile(const char *path, const void *bytes, CFIndex length);

__private_extern__ IOReturn
readFile(const char *path, vm_offset_t * objAddr, vm_size_t * objSize)
{
    int fd;
    int err;
    struct stat stat_buf;

    *objAddr = 0;
    *objSize = 0;

    if((fd = open(path, O_RDONLY)) == -1)
	return errno;

    do {
	if(fstat(fd, &stat_buf) == -1) {
	    err = errno;
	    continue;
	}
        if (0 == (stat_buf.st_mode & S_IFREG)) 
        {
            *objAddr = 0;
            *objSize = 0;
            err = kIOReturnNotReadable;
            continue;
        }
	*objSize = stat_buf.st_size;

	if( KERN_SUCCESS != map_fd(fd, 0, objAddr, TRUE, *objSize)) {
            *objAddr = 0;
            *objSize = 0;
	    err = errno;
	    continue;
	}

	err = kIOReturnSuccess;

    } while( false );

    close(fd);

    return( err );
}


__private_extern__ CFMutableDictionaryRef
readPlist( const char * path, UInt32 key )
{
    IOReturn			err;
    vm_offset_t 		bytes;
    vm_size_t			byteLen;
    CFDataRef			data;
    CFMutableDictionaryRef	obj = 0;

    err = readFile( path, &bytes, &byteLen );

    if( kIOReturnSuccess != err) 
	return (0);
    
    data = CFDataCreateWithBytesNoCopy( kCFAllocatorDefault,
				(const UInt8 *) bytes, byteLen, kCFAllocatorNull );
    if( data) {
	obj = (CFMutableDictionaryRef) CFPropertyListCreateFromXMLData( kCFAllocatorDefault, data,
					    kCFPropertyListMutableContainers,
					    (CFStringRef *) NULL );
	CFRelease( data );
    }
    vm_deallocate( mach_task_self(), bytes, byteLen );

    return (obj);
}

__private_extern__ Boolean
writePlist( const char * path, CFMutableDictionaryRef dict, UInt32 key )
{
    Boolean   result = false;
    CFDataRef data;
    CFIndex   length;
    int       fd = -1;

    data = CFPropertyListCreateXMLData(kCFAllocatorDefault, dict);

    if (data)
    {
	fd = open(path, O_WRONLY|O_CREAT|O_TRUNC, (S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH));
	result = (fd >= 0);
	if (result)
	{
	    if ((length = CFDataGetLength(data)))
		result = (length == write(fd, CFDataGetBytePtr(data), length));
	    close(fd);
	}
	CFRelease(data);
    }

    return (result);
}

static CFMutableDictionaryRef
IODisplayCreateOverrides( IOOptionBits options, 
                            IODisplayVendorID vendor, IODisplayProductID product,
                            UInt32 serialNumber, CFAbsoluteTime manufactureDate,
			    Boolean isDigital )
{

    char			path[256];
    CFTypeRef			obj = 0;
    CFMutableDictionaryRef	dict = 0;

    if( 0 == (options & kIODisplayMatchingInfo)) {

        sprintf( path, "/System/Library/Displays/Overrides"
                        "/" kDisplayVendorID "-%lx"
                        "/" kDisplayProductID "-%lx",
                        vendor, product );
    
        obj = readPlist( path, ((vendor & 0xffff) << 16) | (product & 0xffff) );
        if( obj) {
            if( CFDictionaryGetTypeID() == CFGetTypeID( obj ))
	    {
                dict = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, obj);
            }
	    else if( CFArrayGetTypeID() == CFGetTypeID( obj ))
	    {
		CFArrayRef      array;
		CFIndex         count, idx;
		CFTypeRef       obj2;
		CFDictionaryRef matching, candidate;

                // look for a matching override
		array = obj;
		candidate = 0;
		count = CFArrayGetCount(array);
		for (idx = 0; idx < count; idx++, candidate = 0)
		{
		    obj2 = CFArrayGetValueAtIndex(array, idx);
		    if (CFDictionaryGetTypeID() != CFGetTypeID(obj2))
			continue;
		    candidate = obj2;
		    matching = CFDictionaryGetValue(candidate, CFSTR(kIODisplayOverrideMatchingKey));
		    if (!matching)
			break;
		    if (CFDictionaryGetTypeID() != CFGetTypeID(matching))
			continue;

		    obj2 = CFDictionaryGetValue(matching, CFSTR(kIODisplayIsDigitalKey));
		    if ((obj2 == kCFBooleanTrue) && !isDigital)
			continue;
		    if ((obj2 == kCFBooleanFalse) && isDigital)
			continue;

		    break;
		}
		if (candidate)
		    dict = CFDictionaryCreateMutableCopy( kCFAllocatorDefault, 0, candidate);
            }
	    CFRelease( obj );
        }
    }
    if( !dict)
        dict = CFDictionaryCreateMutable( kCFAllocatorDefault, 0,
                    &kCFTypeDictionaryKeyCallBacks,
                    &kCFTypeDictionaryValueCallBacks);

    if( dict) do {
        CFStringRef string;
        CFURLRef   url;
        CFBundleRef bdl;

	if((kIODisplayMatchingInfo | kIODisplayNoProductName) & options)
	    continue;

        sprintf( path, "/System/Library/Displays/Overrides");
//                            "/" kDisplayVendorID "-%lx", vendor );
    
        string = CFStringCreateWithCString( kCFAllocatorDefault, path,
                                            kCFStringEncodingMacRoman );
        if( !string)
            continue;
        url = CFURLCreateWithString( kCFAllocatorDefault, string, NULL);
	CFRelease(string);
        if( !url)
            continue;
        bdl = CFBundleCreate( kCFAllocatorDefault, url);
        if( bdl) {
            CFDictionarySetValue( dict, CFSTR(kDisplayBundleKey), bdl);
            CFRelease(bdl);
        }
	CFRelease(url);
    
    } while( false );

    return( dict );
}

static void
EDIDInfo( struct EDID * edid,
            IODisplayVendorID * vendor, IODisplayProductID * product,
            UInt32 * serialNumber, CFAbsoluteTime * manufactureDate,
	    Boolean * isDigital )
{
    SInt32		sint;

    if (vendor)
        *vendor = (edid->vendorProduct[0] << 8) | edid->vendorProduct[1];
    if (product)
        *product = (edid->vendorProduct[3] << 8) | edid->vendorProduct[2];
    if (isDigital)
	*isDigital = (0 != (0x80 & edid->displayParams[0]));

    if( serialNumber) {
        sint = (edid->serialNumber[3] << 24)
             | (edid->serialNumber[2] << 16)
             | (edid->serialNumber[1] << 8)
             | (edid->serialNumber[0]);
        if( sint == 0x01010101)
            sint = 0;
        *serialNumber = sint;
    }

    if( false && manufactureDate ) {

        CFGregorianDate gDate;
        CFTimeZoneRef	tz;

        gDate.year	= edid->yearOfManufacture + 1990;
        gDate.month	= 0;
        gDate.day	= edid->weekOfManufacture * 7;
        gDate.hour	= 0;
        gDate.minute	= 0;
        gDate.second	= 0.0;

        tz = CFTimeZoneCopySystem();
        *manufactureDate = CFGregorianDateGetAbsoluteTime( gDate, tz);
        CFRelease(tz);
    }
}

static Boolean
EDIDName( EDID * edid, char * name )
{
    char *	oname = name;
    EDIDDesc *	desc;
    int		i,j;
    Boolean	ok;
    char	c;

    if( !edid || (edid->version < 1) || (edid->revision < 1))
        return( false );

    desc = edid->descriptors;
    for( i = 0; i < 4; i++, desc++) {
        if( desc->general.flag1 || desc->general.flag2 || desc->general.flag3)
            continue;
        if( 0xfc != desc->general.type)
            continue;

        for( j = 0; j < sizeof(desc->general.data); j++) {
            c = desc->general.data[j];
            if( c != 0x0a)
                *oname++ = c;
            else
                break;
        }
    }
    ok = (oname != name);
    if( ok)
        *oname++ = 0;

    return( ok );
}

struct MakeOneLocalContext {
    CFBundleRef		   bdl;
    CFMutableDictionaryRef dict;
    CFStringRef		   key;
};

static void MakeOneLocalization( const void * item, void * context )
{
    struct MakeOneLocalContext * ctx = (struct MakeOneLocalContext *) context;
    CFStringRef		value = NULL;
    CFDictionaryRef	stringTable = NULL;
    CFURLRef		url;
    CFDataRef		tableData = NULL;
    CFStringRef		errStr;
    SInt32		errCode;

    url = CFBundleCopyResourceURLForLocalization( ctx->bdl,
                                CFSTR("Localizable"), CFSTR("strings"), NULL, item );
    if (url && CFURLCreateDataAndPropertiesFromResource( kCFAllocatorDefault,
                                url, &tableData, NULL, NULL, &errCode)) {
        stringTable = CFPropertyListCreateFromXMLData( kCFAllocatorDefault,
                                tableData, kCFPropertyListImmutable, &errStr);
        if (errStr)
            CFRelease( errStr);
        CFRelease( tableData);
    }
    if( url)
        CFRelease(url);
    if( stringTable)
        value = CFDictionaryGetValue(stringTable, ctx->key);
    if (!value)
        value = ctx->key;

    {
        SInt32		 languageCode, regionCode, scriptCode;
        CFStringEncoding stringEncoding;
        if( CFBundleGetLocalizationInfoForLocalization( item, &languageCode, &regionCode,
                                                        &scriptCode, &stringEncoding )) {
            item = CFBundleCopyLocalizationForLocalizationInfo( languageCode, regionCode,
                                                        scriptCode, stringEncoding );
        } else
            item = CFRetain(item);
    }

    CFDictionarySetValue( ctx->dict, item, value );
    CFRelease( item );

    if( stringTable)
        CFRelease( stringTable );
}

static void GenerateProductName( CFMutableDictionaryRef dict,
                                        EDID * edid, SInt32 displayType, IOOptionBits options )
{
    CFStringRef		key;
    CFBundleRef		bdl;
    CFArrayRef		localizations;
    struct MakeOneLocalContext ctx;
    static const char * type2Name[] = {
        NULL,				// 000 kUnknownConnect
        NULL,				// 001 kUnknownConnect
        "Color LCD",			// 002 kPanelTFTConnect
        NULL,				// 003 kFixedModeCRTConnect
        "Multiple Scan Display",	// 004 kMultiModeCRT1Connect
        "Multiple Scan Display",	// 005 kMultiModeCRT2Connect
        "Multiple Scan Display",	// 006 kMultiModeCRT3Connect
        "Multiple Scan Display",	// 007 kMultiModeCRT4Connect
        NULL,				// 008 kModelessConnect
        "Full-Page Display",		// 009 kFullPageConnect
        "VGA Display",			// 010 kVGAConnect
        "Television",			// 011 kNTSCConnect
        "Television",			// 012 kPALConnect
        NULL,				// 013 kHRConnect
        "Color LCD",			// 014 kPanelFSTNConnect
        "Two-Page Display",		// 015 kMonoTwoPageConnect
        "Two-Page Display",		// 016 kColorTwoPageConnect
        NULL,				// 017 kColor16Connect
        NULL,				// 018 kColor19Connect
        NULL,				// 019 kGenericCRT
        "Color LCD",			// 020 kGenericLCD
        NULL,				// 021 kDDCConnect
        NULL				// 022 kNoConnect
    };

    key = CFDictionaryGetValue( dict, CFSTR(kDisplayProductName));
    if( key) {
        if( CFStringGetTypeID() != CFGetTypeID( key ))
            return;
        CFRetain(key);
    }
    bdl = (CFBundleRef) CFDictionaryGetValue( dict, CFSTR(kDisplayBundleKey));

    if( !key) {
        char sbuf[ 128 ];
        const char * name = NULL;

        if( EDIDName(edid, sbuf))
            name = sbuf;
	else if (edid)
	    name = "Unknown Display";
        else {

            if( displayType < (sizeof( type2Name) / sizeof(type2Name[0])))
                name = type2Name[displayType];
            if( !name)
                name = "Unknown Display";
        }

        key = CFStringCreateWithCString( kCFAllocatorDefault, name,
                                            kCFStringEncodingMacRoman );
        if( !key)
            return;
    }

    if( bdl) {
        ctx.bdl = bdl;
        ctx.dict = CFDictionaryCreateMutable( kCFAllocatorDefault, 0,
                        &kCFTypeDictionaryKeyCallBacks,
                        &kCFTypeDictionaryValueCallBacks);
        ctx.key = key;
    
	localizations = CFBundleCopyBundleLocalizations( bdl);
	if( kIODisplayOnlyPreferredName & options) {
	    CFArrayRef temp = localizations;
	    localizations = CFBundleCopyPreferredLocalizationsFromArray( temp );
	    CFRelease( temp );
	}

	CFArrayApplyFunction( localizations,
			    CFRangeMake(0, CFArrayGetCount(localizations)),
			    &MakeOneLocalization,
			    &ctx);
        CFDictionarySetValue( dict, CFSTR(kDisplayProductName), ctx.dict);
    
        CFRelease( localizations );
        CFRelease( ctx.dict );
    }
    CFRelease( key );
}

static void
MaxTimingRangeRec( IODisplayTimingRange * range )
{
    bzero( range, sizeof( IODisplayTimingRange) );

    range->supportedSyncFlags			= 0xffffffff;
    range->supportedSignalLevels		= 0xffffffff;
    range->supportedSignalConfigs		= 0xffffffff;

    range->maxFrameRate				= 0xffffffff;
    range->maxLineRate				= 0xffffffff;
    range->maxPixelClock			= 0xffffffff;
    range->maxPixelError			= 0xffffffff;

    range->maxHorizontalTotal			= 0xffffffff;
    range->maxVerticalTotal			= 0xffffffff;
    range->maxHorizontalActiveClocks		= 0xffffffff;
    range->maxHorizontalBlankingClocks		= 0xffffffff;
    range->maxHorizontalSyncOffsetClocks	= 0xffffffff;
    range->maxHorizontalPulseWidthClocks	= 0xffffffff;
    range->maxVerticalActiveClocks		= 0xffffffff;
    range->maxVerticalBlankingClocks		= 0xffffffff;
    range->maxVerticalSyncOffsetClocks		= 0xffffffff;
    range->maxVerticalPulseWidthClocks		= 0xffffffff;
    range->maxHorizontalBorderLeft		= 0xffffffff;
    range->maxHorizontalBorderRight		= 0xffffffff;
    range->maxVerticalBorderTop			= 0xffffffff;
    range->maxVerticalBorderBottom		= 0xffffffff;

    range->charSizeHorizontalActive		= 1;
    range->charSizeHorizontalBlanking		= 1;
    range->charSizeHorizontalSyncOffset		= 1;
    range->charSizeHorizontalSyncPulse		= 1;
    range->charSizeVerticalBlanking		= 1;
    range->charSizeVerticalSyncOffset		= 1;
    range->charSizeVerticalSyncPulse		= 1;
    range->charSizeHorizontalBorderLeft		= 1;
    range->charSizeHorizontalBorderRight	= 1;
    range->charSizeVerticalBorderTop		= 1;
    range->charSizeVerticalBorderBottom		= 1;
    range->charSizeHorizontalTotal		= 1;
    range->charSizeVerticalTotal		= 1;
}

static Boolean
EDIDDescToDisplayTimingRangeRec( EDID * edid, EDIDGeneralDesc * desc,
                                 IODisplayTimingRange * range )
{
    UInt8 byte;

    if( !edid || (edid->version < 1) || (edid->revision < 1))
        return( false );

    if( desc->flag1 || desc->flag2 || desc->flag3)
        return( false );
    if( 0xfd != desc->type)
        return( false );

    MaxTimingRangeRec( range );

    byte = edid->displayParams[0];
    range->supportedSignalLevels  = 1 << ((byte >> 5) & 3);
    range->supportedSyncFlags	  = ((byte & 1) ? kIORangeSupportsVSyncSerration : 0)
                                  | ((byte & 2) ? kIORangeSupportsSyncOnGreen : 0)
                                  | ((byte & 4) ? kIORangeSupportsCompositeSync : 0)
                                  | ((byte & 8) ? kIORangeSupportsVSyncSerration : 0);

    range->supportedSignalConfigs = kIORangeSupportsInterlacedCEATiming;

    range->minVerticalPulseWidthClocks   = 1;
    range->minHorizontalPulseWidthClocks = 1;

    range->minFrameRate  = desc->data[0];
    range->maxFrameRate  = desc->data[1];
    range->minLineRate   = desc->data[2] * 1000;
    range->maxLineRate   = desc->data[3] * 1000;
    range->maxPixelClock = desc->data[4] * 10000000ULL;

    range->minHorizontalActiveClocks = 640;
    range->minVerticalActiveClocks   = 480;

    if( range->minLineRate)
        range->maxHorizontalActiveClocks = range->maxPixelClock / range->minLineRate;
    if( range->minFrameRate)
        range->maxVerticalActiveClocks   = range->maxPixelClock
                                            / (range->minHorizontalActiveClocks * range->minFrameRate);

    return( true );
}

static kern_return_t
DecodeStandardTiming( EDID * edid, UInt16 standardTiming, 
		      UInt32 * oWidth, UInt32 * height, float * refreshRate)
{
    UInt32 width;

    if( 0x0101 == standardTiming)
	return (kIOReturnBadArgument);

    width = ((standardTiming >> 8) + 31) << 3;
    *oWidth = width;
    switch( (standardTiming >> 6) & 3) {
	case 0:
	    if ((edid->version > 1) || (edid->revision >= 3))
		*height = (10 * width) / 16;
	    else
		*height = width;
	    break;
	case 2:
	    *height = (4 * width) / 5;
	    break;
	case 3:
	    *height = (9 * width) / 16;
	    break;
	default:
	case 1:
	    *height = (3 * width) / 4;
	    break;
    }

    if (refreshRate)
	*refreshRate = (float) ((standardTiming & 63) + 60);

    return (kIOReturnSuccess);
}

static void
AdjustTimingForInterlace( IODetailedTimingInformation * timing )
{
    timing->signalConfig 	  |= kIOInterlacedCEATiming;
    timing->verticalActive         = (timing->verticalActive << 1);
    timing->verticalBlanking       = (timing->verticalBlanking << 1) | 1;
    timing->verticalSyncPulseWidth = (timing->verticalSyncPulseWidth << 1);
    timing->verticalSyncOffset     = (timing->verticalSyncOffset << 1) | 1;
    timing->verticalBorderTop      = (timing->verticalBorderTop << 1);
    timing->verticalBorderBottom   = (timing->verticalBorderBottom << 1);
}

static IOReturn
EDIDDescToDetailedTiming( EDID * edid, EDIDDetailedTimingDesc * desc,
                            IODetailedTimingInformation * timing )
{
    bool interlaced;

    bzero( timing, sizeof( IODetailedTimingInformation) );

    if( !desc->clock)
        return( kIOReturnBadArgument );

    timing->signalConfig		= (edid->displayParams[0] & 16)
                                        ? kIOAnalogSetupExpected : 0;
    interlaced = (0 != (desc->flags & 0x80));

    timing->signalLevels		= (edid->displayParams[0] >> 5) & 3;

    timing->pixelClock 			= ((UInt64) OSReadLittleInt16(&desc->clock, 0))
                                        * 10000ULL;
    timing->maxPixelClock 		= timing->pixelClock;
    timing->minPixelClock 		= timing->pixelClock;

    timing->horizontalActive		= desc->horizActive
                                        | ((desc->horizHigh & 0xf0) << 4);
    timing->horizontalBlanking		= desc->horizBlanking
                                        | ((desc->horizHigh & 0x0f) << 8);

    timing->verticalActive 		= desc->verticalActive
                                        | ((desc->verticalHigh & 0xf0) << 4);
    timing->verticalBlanking  		= desc->verticalBlanking
                                        | ((desc->verticalHigh & 0x0f) << 8);

    timing->horizontalSyncOffset	= desc->horizSyncOffset
                                        | ((desc->syncHigh & 0xc0) << 2);
    timing->horizontalSyncPulseWidth	= desc->horizSyncWidth
                                        | ((desc->syncHigh & 0x30) << 4);

    timing->verticalSyncOffset		= ((desc->verticalSyncOffsetWidth & 0xf0) >> 4)
                                        | ((desc->syncHigh & 0x0c) << 2);
    timing->verticalSyncPulseWidth	= ((desc->verticalSyncOffsetWidth & 0x0f) >> 0)
                                        | ((desc->syncHigh & 0x03) << 4);

    timing->horizontalBorderLeft	= desc->horizBorder;
    timing->horizontalBorderRight	= desc->horizBorder;
    timing->verticalBorderTop		= desc->verticalBorder;
    timing->verticalBorderBottom	= desc->verticalBorder;

    timing->horizontalSyncConfig	= (desc->flags & 2)
                                        ? kIOSyncPositivePolarity : 0;
    timing->horizontalSyncLevel		= 0;
    timing->verticalSyncConfig		= (desc->flags & 4)
                                        ? kIOSyncPositivePolarity : 0;
    timing->verticalSyncLevel		= 0;

    if (interlaced)
	AdjustTimingForInterlace(timing);

    return( kIOReturnSuccess );
}

static void
TimingToHost( const IODetailedTimingInformationV2 * _t1, IODetailedTimingInformationV2 * t2 )
{
    IODetailedTimingInformationV2 * t1 = (IODetailedTimingInformationV2 *) _t1;

    bcopy(t1, t2, sizeof(IODetailedTimingInformationV2));

    t2->scalerFlags      	 = OSReadBigInt32(&t1->scalerFlags, 0);
    t2->horizontalScaled 	 = OSReadBigInt32(&t1->horizontalScaled, 0);
    t2->verticalScaled   	 = OSReadBigInt32(&t1->verticalScaled, 0);
    t2->signalConfig     	 = OSReadBigInt32(&t1->signalConfig, 0);
    t2->signalLevels     	 = OSReadBigInt32(&t1->signalLevels, 0);

    t2->pixelClock    		 = OSReadBigInt64(&t1->pixelClock, 0);
    t2->minPixelClock		 = OSReadBigInt64(&t1->minPixelClock, 0);
    t2->maxPixelClock		 = OSReadBigInt64(&t1->maxPixelClock, 0);

    t2->horizontalActive	 = OSReadBigInt32(&t1->horizontalActive, 0);
    t2->horizontalBlanking	 = OSReadBigInt32(&t1->horizontalBlanking, 0);
    t2->horizontalSyncOffset	 = OSReadBigInt32(&t1->horizontalSyncOffset, 0);
    t2->horizontalSyncPulseWidth = OSReadBigInt32(&t1->horizontalSyncPulseWidth, 0);

    t2->verticalActive		 = OSReadBigInt32(&t1->verticalActive, 0);
    t2->verticalBlanking	 = OSReadBigInt32(&t1->verticalBlanking, 0);
    t2->verticalSyncOffset	 = OSReadBigInt32(&t1->verticalSyncOffset, 0);
    t2->verticalSyncPulseWidth	 = OSReadBigInt32(&t1->verticalSyncPulseWidth, 0);

    t2->horizontalBorderLeft 	 = OSReadBigInt32(&t1->horizontalBorderLeft, 0);
    t2->horizontalBorderRight 	 = OSReadBigInt32(&t1->horizontalBorderRight, 0);
    t2->verticalBorderTop 	 = OSReadBigInt32(&t1->verticalBorderTop, 0);
    t2->verticalBorderBottom 	 = OSReadBigInt32(&t1->verticalBorderBottom, 0);
    t2->horizontalSyncConfig 	 = OSReadBigInt32(&t1->horizontalSyncConfig, 0);
    t2->horizontalSyncLevel 	 = OSReadBigInt32(&t1->horizontalSyncLevel, 0);
    t2->verticalSyncConfig 	 = OSReadBigInt32(&t1->verticalSyncConfig, 0);
    t2->verticalSyncLevel 	 = OSReadBigInt32(&t1->verticalSyncLevel, 0);
}

static IOReturn
StandardResolutionToDetailedTiming( IOFBConnectRef connectRef, EDID * edid, 
				    IOFBResolutionSpec * spec,
				    IOTimingInformation * timing )
{
    CFDictionaryRef stdModes, timingIDs, dict;
    const void *    key;
    CFDataRef	    data;

    if (kResSpecNeedInterlace & spec->flags)
	return (kIOReturnUnsupportedMode);

    stdModes = CFDictionaryGetValue(connectRef->iographicsProperties, CFSTR("std-modes"));
    if (!stdModes)
	return (kIOReturnUnsupportedMode);

    key = (const void *) spec->timingID;
    if (!key)
    {
	CFStringRef timingKey;
	if (kResSpecInternalReducedBlank & spec->flags)
	    timingKey = CFSTR("irb-timing-ids");
	else
	    timingKey = CFSTR("timing-ids");
	timingIDs = CFDictionaryGetValue(connectRef->iographicsProperties, timingKey);
	if (!timingIDs)
	    return (kIOReturnUnsupportedMode);
	key = (const void *)((spec->width << 20) | (spec->height << 8) | ((UInt32)(spec->refreshRate + 0.5)));
	key = CFDictionaryGetValue(timingIDs, key);
    }
    dict = CFDictionaryGetValue(stdModes, key);

    if (!dict)
	return (kIOReturnUnsupportedMode);
    data = CFDictionaryGetValue(dict, CFSTR(kIOFBModeTMKey));
    if (!data)
	return (kIOReturnUnsupportedMode);

    TimingToHost( (const IODetailedTimingInformationV2 *) CFDataGetBytePtr(data), &timing->detailedInfo.v2 );

    timing->appleTimingID = (UInt32) key;

    return (kIOReturnSuccess);
}

static UInt32
IODisplayGetCVTSyncWidth( UInt32 horizontalActive, UInt32 verticalActive )
{
    // CVT Table 2
    enum {
	kCVTAspect4By3    = 4,
	kCVTAspect16By9   = 5,
	kCVTAspect16By10  = 6,
	kCVTAspect5By4    = 7,
	kCVTAspect15By9   = 7,
	kCVTAspectUnknown = 10
    };

    float ratio = ((float) horizontalActive) / ((float) verticalActive);

    if (ratioOver(ratio, 4.0 / 3.0) <= 1.03125)
	return (kCVTAspect4By3);

    if (ratioOver(ratio, 16.0 / 9.0) <= 1.03125)
	return (kCVTAspect16By9);

    if (ratioOver(ratio, 16.0 / 10.0) <= 1.03125)
	return (kCVTAspect16By10);

    if (ratioOver(ratio, 5.0 / 4.0) <= 1.03125)
	return (kCVTAspect5By4);

    if (ratioOver(ratio, 15.0 / 9.0) <= 1.03125)
	return (kCVTAspect15By9);

    return (kCVTAspectUnknown);
}

static IOReturn
GTFToDetailedTiming( IOFBConnectRef connectRef, EDID * edid, 
		     IOFBResolutionSpec * spec, UInt32 characterSize,
		     IODetailedTimingInformation * timing )
{
    float	interlace	= (kResSpecNeedInterlace & spec->flags) ? 0.5 : 0.0;
    float	interlaceFactor = (kResSpecNeedInterlace & spec->flags) ? 2.0 : 1.0;
    float	fieldRate;						// V_FIELD_RATE_RQD
    float	pixelFrequency;						// ACT_PIXEL_FREQ
    int		horizontalTotal;					// TOTAL_PIXELS
    int		horizontalActive;					// TOTAL_ACTIVE_PIXELS 
    int		horizontalBlanking;					// H_BLANK
    int 	horizontalSyncWidth;					// H_SYNC_PIXELS
    int		verticalActive;						// V_LINES_RND
    int		verticalBlanking;					// VBI_LINES
    int		verticalSyncWidth = 3; 					// V_SYNC_RND
    int		verticalSyncAndBackPorch;    				// V_SYNC_BP
    int		verticalSyncFrontPorch;
    // 4,5,15,16.
    int		topMargin	= 0;
    int		bottomMargin	= 0;
    int		leftMargin	= 0;
    int		rightMargin	= 0;

    UInt32	horizontalSyncConfig;
    UInt32	verticalSyncConfig;

    enum { kGTF, kCVT, kCVTRB } genType;

    // 1.
    fieldRate = spec->refreshRate; // * interlaceFactor;
    // 4.
    horizontalActive = roundf(spec->width / characterSize) * characterSize
				    + leftMargin + rightMargin;
    // 5.
    verticalActive = roundf(spec->height / interlaceFactor);

    if (kResSpecReducedBlank & spec->flags)
	genType = kCVTRB;
    else if (connectRef->gtfDisplay && !connectRef->cvtDisplay)
	genType = kGTF;
    else
	genType = kCVT;

    if (kGTF != genType)
	verticalSyncWidth = IODisplayGetCVTSyncWidth(horizontalActive, verticalActive * interlaceFactor);

    if (kGTF == genType)
    {
	/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * GTF */

	float horizontalSyncPercent        = 8.0/100.0;			// H_SYNC_PER
	float verticalSyncAndBackPorchTime = 550e-6; 			// MIN_VSYNC_BP
	int   minVerticalFrontPorch        = 1;  			// MIN_V_PORCH_RND
	float estimatedHorizontalPeriod;				// H_PERIOD_EST 
	float verticalFieldTotal;					// TOTAL_V_LINES 
	float estimatedFieldRate;					// V_FIELD_RATE_EST
	SInt32 curve;

	// 7.
	estimatedHorizontalPeriod = 
	    ((1 / fieldRate) - verticalSyncAndBackPorchTime) 
	    / (verticalActive + (2 * topMargin) + minVerticalFrontPorch + interlace);

	// 8.
	verticalSyncAndBackPorch = roundf(verticalSyncAndBackPorchTime / estimatedHorizontalPeriod);
	verticalSyncFrontPorch   = minVerticalFrontPorch;
	verticalBlanking	 = verticalSyncFrontPorch + verticalSyncAndBackPorch;

	// 10.
	verticalFieldTotal = verticalActive + topMargin + bottomMargin 
			    + verticalBlanking + interlace;
	// 11.
	estimatedFieldRate = 1.0 / estimatedHorizontalPeriod / verticalFieldTotal;
    
	// 12.
	float hPeriod = estimatedHorizontalPeriod / (fieldRate / estimatedFieldRate);
    
	for (curve = (connectRef->numGTFCurves - 1); curve >= 0; curve--)
	{
	    if ((1 / hPeriod) > connectRef->gtfCurves[curve].startHFrequency)
		break;
	}
    
	float cPrime = ((((float) connectRef->gtfCurves[curve].c) - ((float) connectRef->gtfCurves[curve].j)) 
				    * ((float) connectRef->gtfCurves[curve].k) / 256.0) 
				    + ((float) connectRef->gtfCurves[curve].j);
	float mPrime = ((float) connectRef->gtfCurves[curve].k) / 256.0 * ((float) connectRef->gtfCurves[curve].m);

	// 18.
	float idealDutyCycle = cPrime - (mPrime * hPeriod * 1e6 / 1000.0);
	// 19.
	horizontalBlanking = 2 * characterSize * roundf(
				(horizontalActive * idealDutyCycle / (100.0 - idealDutyCycle) 
				/ (2 * characterSize)));

	// 20.
	horizontalTotal = horizontalActive + horizontalBlanking;
	// 21.
	pixelFrequency  = horizontalTotal / hPeriod;
    
	// gtf 2.17.
	horizontalSyncWidth = characterSize *
		    roundf(horizontalSyncPercent * horizontalTotal / characterSize);

	horizontalSyncConfig	= (curve == 0) ? 0 : kIOSyncPositivePolarity;
	verticalSyncConfig	= (curve == 0) ? kIOSyncPositivePolarity : 0;
    }
    else if (kCVT == genType)
    {
	/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * CVT */

	float horizontalSyncPercent        = 8.0/100.0; 		// H_SYNC_PER
	float verticalSyncAndBackPorchTime = 550e-6; 			// MIN_VSYNC_BP
        int   minVerticalBackPorch         = 6;				// MIN_VBPORCH
	int   minVerticalFrontPorch        = 3; 			// MIN_V_PORCH_RND
	float estimatedHorizontalPeriod;				// H_PERIOD_EST 
	float verticalFieldTotal;					// TOTAL_V_LINES 
	SInt32 curve = 0;

	float cPrime = ((((float) connectRef->gtfCurves[curve].c) - ((float) connectRef->gtfCurves[curve].j)) 
				    * ((float) connectRef->gtfCurves[curve].k) / 256.0) 
				    + ((float) connectRef->gtfCurves[curve].j);
	float mPrime = ((float) connectRef->gtfCurves[curve].k) / 256.0 * ((float) connectRef->gtfCurves[curve].m);

	// 8.
	estimatedHorizontalPeriod = 
	    ((1 / fieldRate) - verticalSyncAndBackPorchTime) 
	    / (verticalActive + (topMargin + bottomMargin) + minVerticalFrontPorch + interlace);
	// 9.

	verticalSyncAndBackPorch = 1 + truncf(verticalSyncAndBackPorchTime / estimatedHorizontalPeriod);
    
	if (verticalSyncAndBackPorch < (verticalSyncWidth + minVerticalBackPorch))
	    verticalSyncAndBackPorch = verticalSyncWidth + minVerticalBackPorch;
    
	// 10.

	verticalSyncFrontPorch = minVerticalFrontPorch;
	verticalBlanking       = verticalSyncFrontPorch + verticalSyncAndBackPorch;
    
	// 11.
	verticalFieldTotal = verticalActive + topMargin + bottomMargin 
			    + verticalBlanking + interlace;
	// 12.
	float idealDutyCycle = cPrime - (mPrime * estimatedHorizontalPeriod * 1e6 / 1000.0);
    
	// 13.
	if (idealDutyCycle < 20.0)
	    idealDutyCycle = 20.0;

	horizontalBlanking = 2 * characterSize * truncf(
			    (horizontalActive * idealDutyCycle / (100.0 - idealDutyCycle) 
			    / (2 * characterSize)));
    	// 14.
	horizontalTotal = horizontalActive + horizontalBlanking;

	// 15.
	float frequencyStep = 0.25e6;					// CLOCK_STEP
	pixelFrequency = frequencyStep * truncf(
			(horizontalTotal / estimatedHorizontalPeriod) / frequencyStep);

	// gtf 2.17.
	horizontalSyncWidth = characterSize * truncf(
	    horizontalSyncPercent * horizontalTotal / characterSize);

	horizontalSyncConfig	= 0 * kIOSyncPositivePolarity;
	verticalSyncConfig	= 1 * kIOSyncPositivePolarity;
    }
    else
    {
	/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * CVT reduced blank */

	float minVerticalBlankTime      = 460e-6;			// RB_MIN_V_BLANK 
        int   minVerticalBackPorch      = 6;				// MIN_VBPORCH
	float estimatedHorizontalPeriod;				// H_PERIOD_EST 
	verticalSyncFrontPorch          = 3;				// RB_V_FPORCH
	horizontalBlanking              = 160;				// RB_H_BLANK
	horizontalSyncWidth             = 32;				// RB_H_SYNC

    
	// 8.
	estimatedHorizontalPeriod = ((1 / fieldRate) - minVerticalBlankTime) 
				    / (verticalActive + topMargin + bottomMargin);
	// 9.
	verticalBlanking = truncf(minVerticalBlankTime / estimatedHorizontalPeriod) + 1; // VBI_LINES

	// 10.
	if (verticalBlanking < (verticalSyncFrontPorch + verticalSyncWidth + minVerticalBackPorch))
	    verticalBlanking = (verticalSyncFrontPorch + verticalSyncWidth + minVerticalBackPorch);

	verticalSyncAndBackPorch = verticalBlanking - verticalSyncFrontPorch;

	// 11.
	int verticalFieldTotal = verticalBlanking  + verticalActive 
				+ topMargin + bottomMargin + interlace;

	// 12.
	horizontalTotal = horizontalActive + horizontalBlanking;

	// 13.
	float frequencyStep = 0.25e6;					// CLOCK_STEP
    	pixelFrequency = frequencyStep * truncf(
			    (horizontalTotal * verticalFieldTotal * fieldRate) / frequencyStep);

	horizontalSyncConfig	= 1 * kIOSyncPositivePolarity;
	verticalSyncConfig	= 0 * kIOSyncPositivePolarity;
    }
    /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

    int horizontalSyncOffset = (horizontalBlanking / 2) - horizontalSyncWidth;

    // --
    bzero( timing, sizeof(IODetailedTimingInformation) );

    if (edid)
    {
        timing->signalConfig		= (edid->displayParams[0] & 16)
                                        ? kIOAnalogSetupExpected : 0;
        timing->signalLevels		= (edid->displayParams[0] >> 5) & 3;
    }
    else
    {
        timing->signalConfig		= kIOAnalogSetupExpected;
        timing->signalLevels		= kIOAnalogSignalLevel_0700_0300;
    }

    timing->pixelClock 			= pixelFrequency;

    timing->horizontalActive		= horizontalActive;
    timing->horizontalBlanking		= horizontalBlanking;

    timing->verticalActive 		= verticalActive;
    timing->verticalBlanking  		= verticalBlanking;

    timing->horizontalSyncOffset	= horizontalSyncOffset;
    timing->horizontalSyncPulseWidth	= horizontalSyncWidth;

    timing->verticalSyncOffset		= verticalSyncFrontPorch;
    timing->verticalSyncPulseWidth	= verticalSyncWidth;

    timing->horizontalBorderLeft	= leftMargin;
    timing->horizontalBorderRight	= rightMargin;
    timing->verticalBorderTop		= topMargin;
    timing->verticalBorderBottom	= bottomMargin;

    timing->horizontalSyncConfig	= horizontalSyncConfig;
    timing->horizontalSyncLevel		= 0;
    timing->verticalSyncConfig		= verticalSyncConfig;
    timing->verticalSyncLevel		= 0;

    if (kResSpecNeedInterlace & spec->flags)
	AdjustTimingForInterlace(timing);

    return( kIOReturnSuccess );
}

#if RLOG
__private_extern__ void
IOFBLogRange(IOFBConnectRef connectRef, const IODisplayTimingRange * range)
{
    if (!connectRef->logfile || !range)
	return;

    fprintf(connectRef->logfile, "  minPixelClock                 %qd\n", range->minPixelClock);
    fprintf(connectRef->logfile, "  maxPixelClock                 %qd\n", range->maxPixelClock);
    
    fprintf(connectRef->logfile, "  maxPixelError                 %ld\n", range->maxPixelError);
    fprintf(connectRef->logfile, "  supportedSyncFlags            %lx\n", range->supportedSyncFlags);
    fprintf(connectRef->logfile, "  supportedSignalLevels         %lx\n", range->supportedSignalLevels);
    fprintf(connectRef->logfile, "  supportedSignalConfigs        %lx\n", range->supportedSignalConfigs);
    fprintf(connectRef->logfile, "  minFrameRate                  %ld\n", range->minFrameRate);
    fprintf(connectRef->logfile, "  maxFrameRate                  %ld\n", range->maxFrameRate);
    fprintf(connectRef->logfile, "  minLineRate                   %ld\n", range->minLineRate);
    fprintf(connectRef->logfile, "  maxLineRate                   %ld\n", range->maxLineRate);

    fprintf(connectRef->logfile, "  maxHorizontalTotal            %ld\n", range->maxHorizontalTotal);
    fprintf(connectRef->logfile, "  maxVerticalTotal              %ld\n", range->maxVerticalTotal);
    fprintf(connectRef->logfile, "  charSizeHorizontalActive      %d\n", range->charSizeHorizontalActive);
    fprintf(connectRef->logfile, "  charSizeHorizontalBlanking    %d\n", range->charSizeHorizontalBlanking);
    fprintf(connectRef->logfile, "  charSizeHorizontalSyncOffset  %d\n", range->charSizeHorizontalSyncOffset);
    fprintf(connectRef->logfile, "  charSizeHorizontalSyncPulse   %d\n", range->charSizeHorizontalSyncPulse);
    fprintf(connectRef->logfile, "  charSizeVerticalActive        %d\n", range->charSizeVerticalActive);
    fprintf(connectRef->logfile, "  charSizeVerticalBlanking      %d\n", range->charSizeVerticalBlanking);
    fprintf(connectRef->logfile, "  charSizeVerticalSyncOffset    %d\n", range->charSizeVerticalSyncOffset);
    fprintf(connectRef->logfile, "  charSizeVerticalSyncPulse     %d\n", range->charSizeVerticalSyncPulse);
    fprintf(connectRef->logfile, "  charSizeHorizontalBorderLeft  %d\n", range->charSizeHorizontalBorderLeft);
    fprintf(connectRef->logfile, "  charSizeHorizontalBorderRight %d\n", range->charSizeHorizontalBorderRight);
    fprintf(connectRef->logfile, "  charSizeVerticalBorderTop     %d\n", range->charSizeVerticalBorderTop);
    fprintf(connectRef->logfile, "  charSizeVerticalBorderBottom  %d\n", range->charSizeVerticalBorderBottom);
    fprintf(connectRef->logfile, "  charSizeHorizontalTotal       %d\n", range->charSizeHorizontalTotal);
    fprintf(connectRef->logfile, "  charSizeVerticalTotal         %d\n", range->charSizeVerticalTotal);

    fprintf(connectRef->logfile, "  minHorizontalActiveClocks     %ld\n", range->minHorizontalActiveClocks);
    fprintf(connectRef->logfile, "  maxHorizontalActiveClocks     %ld\n", range->maxHorizontalActiveClocks);
    fprintf(connectRef->logfile, "  minHorizontalBlankingClocks   %ld\n", range->minHorizontalBlankingClocks);
    fprintf(connectRef->logfile, "  maxHorizontalBlankingClocks   %ld\n", range->maxHorizontalBlankingClocks);
    fprintf(connectRef->logfile, "  minHorizontalSyncOffsetClocks %ld\n", range->minHorizontalSyncOffsetClocks);
    fprintf(connectRef->logfile, "  maxHorizontalSyncOffsetClocks %ld\n", range->maxHorizontalSyncOffsetClocks);
    fprintf(connectRef->logfile, "  minHorizontalPulseWidthClocks %ld\n", range->minHorizontalPulseWidthClocks);
    fprintf(connectRef->logfile, "  maxHorizontalPulseWidthClocks %ld\n", range->maxHorizontalPulseWidthClocks);

    fprintf(connectRef->logfile, "  minVerticalActiveClocks       %ld\n", range->minVerticalActiveClocks);
    fprintf(connectRef->logfile, "  maxVerticalActiveClocks       %ld\n", range->maxVerticalActiveClocks);
    fprintf(connectRef->logfile, "  minVerticalBlankingClocks     %ld\n", range->minVerticalBlankingClocks);
    fprintf(connectRef->logfile, "  maxVerticalBlankingClocks     %ld\n", range->maxVerticalBlankingClocks);

    fprintf(connectRef->logfile, "  minVerticalSyncOffsetClocks   %ld\n", range->minVerticalSyncOffsetClocks);
    fprintf(connectRef->logfile, "  maxVerticalSyncOffsetClocks   %ld\n", range->maxVerticalSyncOffsetClocks);
    fprintf(connectRef->logfile, "  minVerticalPulseWidthClocks   %ld\n", range->minVerticalPulseWidthClocks);
    fprintf(connectRef->logfile, "  maxVerticalPulseWidthClocks   %ld\n", range->maxVerticalPulseWidthClocks);

    fprintf(connectRef->logfile, "  minHorizontalBorderLeft       %ld\n", range->minHorizontalBorderLeft);
    fprintf(connectRef->logfile, "  maxHorizontalBorderLeft       %ld\n", range->maxHorizontalBorderLeft);
    fprintf(connectRef->logfile, "  minHorizontalBorderRight      %ld\n", range->minHorizontalBorderRight);
    fprintf(connectRef->logfile, "  maxHorizontalBorderRight      %ld\n", range->maxHorizontalBorderRight);

    fprintf(connectRef->logfile, "  minVerticalBorderTop          %ld\n", range->minVerticalBorderTop);
    fprintf(connectRef->logfile, "  maxVerticalBorderTop          %ld\n", range->maxVerticalBorderTop);
    fprintf(connectRef->logfile, "  minVerticalBorderBottom       %ld\n", range->minVerticalBorderBottom);
    fprintf(connectRef->logfile, "  maxVerticalBorderBottom       %ld\n", range->maxVerticalBorderBottom);

    fprintf(connectRef->logfile, "  maxNumLinks                   %ld\n", range->maxNumLinks);
    fprintf(connectRef->logfile, "  minLink0PixelClock            %ld\n", range->minLink0PixelClock);
    fprintf(connectRef->logfile, "  maxLink0PixelClock            %ld\n", range->maxLink0PixelClock);
    fprintf(connectRef->logfile, "  minLink1PixelClock            %ld\n", range->minLink1PixelClock);
    fprintf(connectRef->logfile, "  maxLink1PixelClock            %ld\n", range->maxLink1PixelClock);

    fflush(connectRef->logfile);
}

__private_extern__ void
IOFBLogTiming(IOFBConnectRef connectRef, const IOTimingInformation * timing)
{
    if (!connectRef->logfile || !timing)
	return;

    fprintf(connectRef->logfile, "  pixelClock                    %qd\n", timing->detailedInfo.v2.pixelClock);
    fprintf(connectRef->logfile, "  minPixelClock                 %qd\n", timing->detailedInfo.v2.minPixelClock);
    fprintf(connectRef->logfile, "  maxPixelClock                 %qd\n", timing->detailedInfo.v2.maxPixelClock);
    fprintf(connectRef->logfile, "  signalConfig                  %lx\n", timing->detailedInfo.v2.signalConfig);
    fprintf(connectRef->logfile, "  signalLevels                  %lx\n", timing->detailedInfo.v2.signalLevels);
    fprintf(connectRef->logfile, "  horizontalActive              %ld\n", timing->detailedInfo.v2.horizontalActive);
    fprintf(connectRef->logfile, "  horizontalBlanking            %ld\n", timing->detailedInfo.v2.horizontalBlanking);
    fprintf(connectRef->logfile, "  horizontalSyncOffset          %ld\n", timing->detailedInfo.v2.horizontalSyncOffset);
    fprintf(connectRef->logfile, "  horizontalSyncPulseWidth      %ld\n", timing->detailedInfo.v2.horizontalSyncPulseWidth);
    fprintf(connectRef->logfile, "  verticalActive                %ld\n", timing->detailedInfo.v2.verticalActive);
    fprintf(connectRef->logfile, "  verticalBlanking              %ld\n", timing->detailedInfo.v2.verticalBlanking);
    fprintf(connectRef->logfile, "  verticalSyncOffset            %ld\n", timing->detailedInfo.v2.verticalSyncOffset);
    fprintf(connectRef->logfile, "  verticalSyncPulseWidth        %ld\n", timing->detailedInfo.v2.verticalSyncPulseWidth);
    fprintf(connectRef->logfile, "  horizontalSyncConfig          %ld\n", timing->detailedInfo.v2.horizontalSyncConfig);
    fprintf(connectRef->logfile, "  horizontalSyncLevel           %ld\n", timing->detailedInfo.v2.horizontalSyncLevel);
    fprintf(connectRef->logfile, "  verticalSyncConfig            %ld\n", timing->detailedInfo.v2.verticalSyncConfig);
    fprintf(connectRef->logfile, "  verticalSyncLevel             %ld\n", timing->detailedInfo.v2.verticalSyncLevel);
    fprintf(connectRef->logfile, "  numLinks                      %ld\n", timing->detailedInfo.v2.numLinks);

    fprintf(connectRef->logfile, "  scalerFlags                   %lx\n", timing->detailedInfo.v2.scalerFlags);
    fprintf(connectRef->logfile, "  horizontalScaled              %ld\n", timing->detailedInfo.v2.horizontalScaled);
    fprintf(connectRef->logfile, "  verticalScaled                %ld\n", timing->detailedInfo.v2.verticalScaled);
    fprintf(connectRef->logfile, "  horizontalScaledInset         %ld\n", timing->detailedInfo.v2.horizontalScaledInset);
    fprintf(connectRef->logfile, "  verticalScaledInset           %ld\n", timing->detailedInfo.v2.verticalScaledInset);

    fflush(connectRef->logfile);
}
#endif

static void
AdjustTimingForRange( IODisplayTimingRange * range,
                        IODetailedTimingInformation * timing )
{
    if( timing->horizontalSyncPulseWidth % range->charSizeHorizontalSyncPulse)
    // digital only?
#if 0
	// round
        timing->horizontalSyncPulseWidth += range->charSizeHorizontalSyncPulse 
			    - (timing->horizontalSyncPulseWidth % range->charSizeHorizontalSyncPulse);
#else
	// trunc
        timing->horizontalSyncPulseWidth -= 
			    (timing->horizontalSyncPulseWidth % range->charSizeHorizontalSyncPulse);
#endif
}

static UInt32
GetAssumedPixelRepetition( IODetailedTimingInformation * timing )
{
    if (!(kIOInterlacedCEATiming & timing->signalConfig))
	return (0);

    if ((1440 == timing->horizontalActive)
     && ((480 == timing->verticalActive) || (576 == timing->verticalActive)))
	return (2);

    if ((2880 == timing->horizontalActive)
     && ((480 == timing->verticalActive) || (576 == timing->verticalActive)))
	return (4);	// 1 - 10

    return (0);
}

static int
CheckTimingWithRange( IOFBConnectRef connectRef,
		      IODisplayTimingRange * range, IODetailedTimingInformation * timing )
{
    UInt64	pixelClock;
    UInt64	rate;
    UInt64	hTotal, vTotal;

    if( kIODigitalSignal & timing->signalConfig)
        return(1);

    if ((kIOInterlacedCEATiming & timing->signalConfig)
     && !((kIORangeSupportsInterlacedCEATiming | kIORangeSupportsInterlacedCEATimingWithConfirm) 
	    & range->supportedSignalConfigs))
        return(34);

    if ((timing->numLinks > 1) 
	&& range->maxNumLinks
	&& (timing->numLinks > range->maxNumLinks))
        return(35);

//    if( 0 == (range->supportedSyncFlags & (1 << (timing->signalLevels))))
//        return(2);
//    if( 0 == (range->supportedSignalLevels & (1 << (timing->signalLevels))))
//        return(3);

    pixelClock = timing->pixelClock;
    hTotal  = timing->horizontalActive;
    hTotal += timing->horizontalBlanking;
    vTotal  = timing->verticalActive;
    vTotal += timing->verticalBlanking;

    if (!hTotal || !vTotal)
        return(36);

    if( (pixelClock > range->maxPixelClock)
     || (pixelClock < range->minPixelClock))
        return(4);

    // line rate
    rate = pixelClock / hTotal;
    if( (rate > range->maxLineRate)
     || (rate < range->minLineRate))
        return(5);

    // frame rate
    rate = pixelClock;
    if (kIOInterlacedCEATiming & timing->signalConfig)
	rate *= 2;
    rate /= (hTotal * vTotal);
    if( (rate > range->maxFrameRate)
     || (rate < range->minFrameRate))
        return(6);

    if( hTotal > range->maxHorizontalTotal)
        return(7);
    if( vTotal > range->maxVerticalTotal)
        return(8);

    if( (timing->horizontalActive > range->maxHorizontalActiveClocks)
     || (timing->horizontalActive < range->minHorizontalActiveClocks))
        return(9);
    if( (timing->verticalActive > range->maxVerticalActiveClocks)
     || (timing->verticalActive < range->minVerticalActiveClocks))
        return(10);

/*
    if( (timing->horizontalBlanking > range->maxHorizontalBlankingClocks)
     || (timing->horizontalBlanking < range->minHorizontalBlankingClocks))
        return(11);
    if( (timing->verticalBlanking > range->maxVerticalBlankingClocks)
     || (timing->verticalBlanking < range->minVerticalBlankingClocks))
        return(12);
*/
    if( (timing->horizontalSyncOffset > range->maxHorizontalSyncOffsetClocks)
     || (timing->horizontalSyncOffset < range->minHorizontalSyncOffsetClocks))
        return(13);
    if( (timing->horizontalSyncPulseWidth > range->maxHorizontalPulseWidthClocks)
     || (timing->horizontalSyncPulseWidth < range->minHorizontalPulseWidthClocks))
        return(14);

    if( (timing->verticalSyncOffset > range->maxVerticalSyncOffsetClocks)
     || (timing->verticalSyncOffset < range->minVerticalSyncOffsetClocks))
        return(15);
    if( (timing->verticalSyncPulseWidth > range->maxVerticalPulseWidthClocks)
     || (timing->verticalSyncPulseWidth < range->minVerticalPulseWidthClocks))
        return(16);

    if( (timing->horizontalBorderLeft > range->maxHorizontalBorderLeft)
     || (timing->horizontalBorderLeft < range->minHorizontalBorderLeft))
        return(17);
    if( (timing->horizontalBorderRight > range->maxHorizontalBorderRight)
     || (timing->horizontalBorderRight < range->minHorizontalBorderRight))
        return(18);
    if( (timing->verticalBorderTop > range->maxVerticalBorderTop)
     || (timing->verticalBorderTop < range->minVerticalBorderTop))
        return(19);
    if( (timing->verticalBorderBottom > range->maxVerticalBorderBottom)
     || (timing->verticalBorderBottom < range->minVerticalBorderBottom))
        return(20);

    if( timing->horizontalActive % range->charSizeHorizontalActive)
        return(21);
    if( timing->horizontalBlanking % range->charSizeHorizontalBlanking)
        return(22);
    if( timing->horizontalSyncOffset % range->charSizeHorizontalSyncOffset)
        return(23);
    if( timing->horizontalSyncPulseWidth % range->charSizeHorizontalSyncPulse)
        return(24);
    if( timing->verticalBlanking % range->charSizeVerticalBlanking)
        return(25);
    if( timing->verticalSyncOffset % range->charSizeVerticalSyncOffset)
        return(26);
    if( timing->verticalSyncPulseWidth % range->charSizeVerticalSyncPulse)
        return(27);
    if( timing->horizontalBorderLeft % range->charSizeHorizontalBorderLeft)
        return(28);
    if( timing->horizontalBorderRight % range->charSizeHorizontalBorderRight)
        return(29);
    if( timing->verticalBorderTop % range->charSizeVerticalBorderTop)
        return(30);
    if( timing->verticalBorderBottom % range->charSizeVerticalBorderBottom)
        return(31);
    if( hTotal % range->charSizeHorizontalTotal)
        return(32);
    if( vTotal % range->charSizeVerticalTotal)
        return(33);

    return (0);
}

static Boolean
HasEstablishedTiming( IOFBConnectRef connectRef, UInt32 appleTimingID )
{
    CFDataRef data;
    UInt32 *  establishedIDs;
    UInt32    i;

    if (kIOTimingIDInvalid == appleTimingID)
	return (false);

    data = CFDictionaryGetValue(connectRef->iographicsProperties, CFSTR("established-ids"));
    if (data)
	establishedIDs = (UInt32 *) CFDataGetBytePtr(data);
    else
	establishedIDs = 0;

    for( i = 0; 
	 establishedIDs && (i < 24) && (appleTimingID != OSReadBigInt32(&establishedIDs[i], 0));
	 i++ )	{}

    return( i < 24 );
}

__private_extern__ IOReturn
IOCheckTimingWithDisplay( IOFBConnectRef connectRef,
			  IOFBDisplayModeDescription * desc,
			  IOOptionBits modeGenFlags )
{
    IOTimingInformation * timing = &desc->timingInfo;
    IOReturn		  result;
    CFDataRef		  edidData;
    CFDataRef		  data;
    int                   diag;

    do 
    {
        if (connectRef->fbRange && !(kIOFBDriverMode & modeGenFlags))
	{
	    AdjustTimingForRange(connectRef->fbRange,  &timing->detailedInfo.v2);

	    if ((diag = CheckTimingWithRange(connectRef, connectRef->fbRange, &timing->detailedInfo.v2)))
	    {
#if RLOG
		DEBG(connectRef, "out of cards range(%d) %ld x %ld\n", diag,
				timing->detailedInfo.v2.horizontalActive, 
				timing->detailedInfo.v2.verticalActive );
		IOFBLogTiming(connectRef, timing);
#endif
		result = kIOReturnUnsupportedMode;
		continue;
	    }

	    result = IOFBDriverPreflight(connectRef, desc);
	    if (kIOReturnSuccess != result)
		continue;
	}

	result = kIOReturnNotFound;
	if(!connectRef->overrides)
	    continue;

	if (!(kIOFBEDIDStdEstMode & modeGenFlags))
	{
	    edidData = CFDictionaryGetValue(connectRef->overrides, CFSTR(kIODisplayEDIDKey));
	    if (edidData && HasEstablishedTiming(connectRef, timing->appleTimingID))
	    {
		result = kIOReturnUnsupportedMode;
		continue;
	    }
	}

	if (connectRef->hasCEAExt 
	    && !((kIOFBScaledMode | kIOFBEDIDDetailedMode | kIOFBEDIDStdEstMode) & modeGenFlags))
	{
	    result = kIOReturnUnsupportedMode;
	    continue;
	}

	if (kIODetailedTimingValid & timing->flags)
	{
	    CFNumberRef num;

	    num = CFDictionaryGetValue( connectRef->overrides, CFSTR("sync") );
	    if( num)
	    {
		UInt32 hSyncMask, hSyncValue, vSyncMask, vSyncValue;
    
		CFNumberGetValue( num, kCFNumberSInt32Type, &vSyncValue );
		hSyncMask  = 0xff & (vSyncValue >> 24);
		hSyncValue = 0xff & (vSyncValue >> 16);
		vSyncMask  = 0xff & (vSyncValue >> 8);
		vSyncValue = 0xff & (vSyncValue >> 0);
		if ((hSyncValue != (timing->detailedInfo.v2.horizontalSyncConfig & hSyncMask))
		 || (vSyncValue != (timing->detailedInfo.v2.verticalSyncConfig   & vSyncMask)))
		{
		    result = kIOReturnUnsupportedMode;
		    continue;
		}
	    }

	    if ((timing->detailedInfo.v2.numLinks > 1) 
		&& connectRef->maxDisplayLinks
		&& (timing->detailedInfo.v2.numLinks > connectRef->maxDisplayLinks))
	    {
#if RLOG
		DEBG(connectRef, "out display maxDisplayLinks\n");
		IOFBLogTiming(connectRef, timing);
#endif
		result = kIOReturnUnsupportedMode;
		continue;
	    }

	    data = CFDictionaryGetValue(connectRef->overrides, CFSTR("trng"));
	    if (data && ((kIOFBGTFMode | kIOFBStdMode | kIOFBDriverMode) & modeGenFlags))
	    {
		if ((diag = CheckTimingWithRange(connectRef, (IODisplayTimingRange *) CFDataGetBytePtr(data),
					      (IODetailedTimingInformation *) &timing->detailedInfo.v2)))
		{
#if RLOG
		    DEBG(connectRef, "out display range(%d)\n", diag);
		    IOFBLogTiming(connectRef, timing);
#endif
		    result = kIOReturnUnsupportedMode;
		    continue;
		}
		if (kIOFBDriverMode & modeGenFlags)
		{
		    if (ratioOver(
			((float) timing->detailedInfo.v2.horizontalActive) 
				/ ((float) timing->detailedInfo.v2.verticalActive),
			connectRef->nativeAspect) > 1.2)
		    {
#if RLOG
                        DEBG(connectRef, "aspect too different\n");
                        IOFBLogTiming(connectRef, timing);
#endif
			continue;
		    }
		}
		result = kIOReturnSuccess;
	    }
	}
    }
    while (false);

    return (result);
}

static kern_return_t
InstallTiming( IOFBConnectRef                connectRef, 
		IOFBDisplayModeDescription * desc,
		IOOptionBits                 dmFlags,
		IOOptionBits                 modeGenFlags )
{
    IOReturn			err;
    IOTimingInformation	*	timing = &desc->timingInfo;

    bzero(&desc->info, sizeof(desc->info));

    if (connectRef->dualLinkCrossover)
    {
	if (timing->detailedInfo.v2.pixelClock >= connectRef->dualLinkCrossover)
	    timing->detailedInfo.v2.numLinks = 2;
	else
	    timing->detailedInfo.v2.numLinks = 1;
    }
    else
	timing->detailedInfo.v2.numLinks = 0;

    UpdateTimingInfoForTransform(connectRef, desc, 0);

    if (InvalidTiming(connectRef, timing))
	return (kIOReturnUnsupportedMode);

    err = IOCheckTimingWithDisplay( connectRef, desc, modeGenFlags );
    if (kIOReturnUnsupportedMode == err)
        return( err );

    if ((kIOFBEDIDStdEstMode | kIOFBEDIDDetailedMode) & modeGenFlags)
    {
	if ((0xffffffff == connectRef->dimensions.width)
	 || (timing->detailedInfo.v2.horizontalActive > connectRef->dimensions.width))
	    connectRef->dimensions.width = timing->detailedInfo.v2.horizontalActive;
	if ((0xffffffff == connectRef->dimensions.height)
	 || (timing->detailedInfo.v2.verticalActive > connectRef->dimensions.height))
	    connectRef->dimensions.height = timing->detailedInfo.v2.verticalActive;
    }
    else
    {
	if( (timing->detailedInfo.v2.horizontalActive > connectRef->dimensions.width)
	    || (timing->detailedInfo.v2.verticalActive > connectRef->dimensions.height)) {
	    dmFlags |= connectRef->dimensions.setFlags;
	    dmFlags &= ~connectRef->dimensions.clearFlags;
	}
	if (kIOReturnSuccess != err)
	    dmFlags &= ~kDisplayModeSafeFlag;
    }

    if (timing->detailedInfo.v2.signalConfig & kIOInterlacedCEATiming)
    {
	dmFlags |= (kDisplayModeInterlacedFlag /*| kDisplayModeTelevisionFlag*/);
	if (connectRef->fbRange && (kIORangeSupportsInterlacedCEATimingWithConfirm & connectRef->fbRange->supportedSignalConfigs))
	    dmFlags &= ~kDisplayModeSafeFlag;
    }

    desc->info.flags = dmFlags;

    err = IOFBInstallMode( connectRef, 0xffffffff, desc, 0, modeGenFlags );

#if RLOG
    if (timing->detailedInfo.v2.signalConfig & kIOInterlacedCEATiming)
    {
	DEBG(connectRef, "interlaced: %lx\n", modeGenFlags);
	IOFBLogTiming(connectRef, timing);
    }
#endif

    return( err );
}

static kern_return_t
InstallFromEDIDDesc( IOFBConnectRef connectRef, 
		     EDID * edid, EDIDDetailedTimingDesc * desc,
		     IOOptionBits dmFlags)
{
    IOReturn		err;
    UInt32		pixelRep;
    IOFBDisplayModeDescription modeDesc;
    IOTimingInformation	*      timing = &modeDesc.timingInfo;

    bzero( &modeDesc, sizeof( modeDesc ));
    timing->flags = kIODetailedTimingValid;

    err = EDIDDescToDetailedTiming( edid, desc, (IODetailedTimingInformation *) &timing->detailedInfo.v2 );
    if( kIOReturnSuccess != err)
        return (err);

    if (kIOInterlacedCEATiming & timing->detailedInfo.v2.signalConfig)
    {
	connectRef->hasInterlaced = true;
	DEBG(connectRef, "hasInterlaced\n");
    }
#if 1
    // internal reduced blank timing switch
    else if (
	(kDisplayModeDefaultFlag & dmFlags)
	&& connectRef->fbRange
	&& CFDictionaryGetValue(connectRef->overrides, CFSTR(kIODisplayIsDigitalKey))
	&& ((timing->detailedInfo.v2.pixelClock > connectRef->fbRange->maxPixelClock)
	 || (connectRef->fbRange->maxLink0PixelClock 
		&& (timing->detailedInfo.v2.pixelClock > (connectRef->fbRange->maxLink0PixelClock * 1000ULL)))))
    {
	IOTimingInformation   _newTiming;
	IOTimingInformation * newTiming = &_newTiming;
	IOFBResolutionSpec    spec;
	
	spec.timingID    = kIOTimingIDInvalid;
	spec.width       = timing->detailedInfo.v2.horizontalActive;
	spec.height      = timing->detailedInfo.v2.verticalActive;
	spec.refreshRate = RefreshRateFromDetailedTiming( &timing->detailedInfo.v2 );
	spec.flags       = kResSpecInternalReducedBlank;

	err = StandardResolutionToDetailedTiming( connectRef, edid, &spec, newTiming );
	if (kIOReturnSuccess == err)
	{
#if RLOG
	    DEBG(connectRef, "switching:\n");
	    IOFBLogTiming(connectRef, timing);
#endif
	    timing->detailedInfo.v2.pixelClock		     = newTiming->detailedInfo.v2.pixelClock;
	    timing->detailedInfo.v2.maxPixelClock 	     = newTiming->detailedInfo.v2.pixelClock;
	    timing->detailedInfo.v2.minPixelClock 	     = newTiming->detailedInfo.v2.pixelClock;
	    timing->detailedInfo.v2.horizontalActive	     = newTiming->detailedInfo.v2.horizontalActive;
	    timing->detailedInfo.v2.horizontalBlanking       = newTiming->detailedInfo.v2.horizontalBlanking;
	    timing->detailedInfo.v2.verticalActive           = newTiming->detailedInfo.v2.verticalActive;
	    timing->detailedInfo.v2.verticalBlanking         = newTiming->detailedInfo.v2.verticalBlanking;
	    timing->detailedInfo.v2.horizontalSyncOffset     = newTiming->detailedInfo.v2.horizontalSyncOffset;
	    timing->detailedInfo.v2.horizontalSyncPulseWidth = newTiming->detailedInfo.v2.horizontalSyncPulseWidth;
	    timing->detailedInfo.v2.verticalSyncOffset       = newTiming->detailedInfo.v2.verticalSyncOffset;
	    timing->detailedInfo.v2.verticalSyncPulseWidth   = newTiming->detailedInfo.v2.verticalSyncPulseWidth;
	    timing->detailedInfo.v2.horizontalBorderLeft     = newTiming->detailedInfo.v2.horizontalBorderLeft;
	    timing->detailedInfo.v2.horizontalBorderRight    = newTiming->detailedInfo.v2.horizontalBorderRight;
	    timing->detailedInfo.v2.verticalBorderTop        = newTiming->detailedInfo.v2.verticalBorderTop;
	    timing->detailedInfo.v2.verticalBorderBottom     = newTiming->detailedInfo.v2.verticalBorderBottom;
#if RLOG
	    DEBG(connectRef, "switching to:\n");
	    IOFBLogTiming(connectRef, timing);
#endif
	}
    }
#endif

    if ((pixelRep = GetAssumedPixelRepetition((IODetailedTimingInformation *) &timing->detailedInfo.v2)))
    {
	enum { kNeedFeatures = (kIOScaleCanUpSamplePixels | kIOScaleCanScaleInterlaced) };

	if (2 != pixelRep || !connectRef->scalerInfo)
	    return (kIOReturnUnsupported);

	if (kNeedFeatures != (kNeedFeatures & connectRef->scalerInfo->scalerFeatures)) 
	    return (kIOReturnUnsupported);

	if (timing->detailedInfo.v2.verticalActive > connectRef->scalerInfo->maxVerticalPixels)
	    return (kIOReturnUnsupported);
	if ((timing->detailedInfo.v2.horizontalActive >> 1) > connectRef->scalerInfo->maxHorizontalPixels)
	    return (kIOReturnUnsupported);

	timing->detailedInfo.v2.scalerFlags      = kIOScaleStretchToFit;
	timing->detailedInfo.v2.horizontalScaled = timing->detailedInfo.v2.horizontalActive >> 1;
	timing->detailedInfo.v2.verticalScaled   = timing->detailedInfo.v2.verticalActive;
#if RLOG
	DEBG(connectRef, "pixel rep: %ld\n", pixelRep);
	IOFBLogTiming(connectRef, timing);
#endif
    }

    if (!connectRef->defaultWidth)
    {
	connectRef->defaultWidth       = timing->detailedInfo.v2.horizontalActive;
	connectRef->defaultHeight      = timing->detailedInfo.v2.verticalActive;
	connectRef->defaultImageWidth  = desc->horizImageSize    | ((desc->imageSizeHigh & 0xf0) << 4);
	connectRef->defaultImageHeight = desc->verticalImageSize | ((desc->imageSizeHigh & 0x0f) << 8);
    }

    err = InstallTiming( connectRef, &modeDesc,
			    dmFlags, kIOFBEDIDDetailedMode );

    return( err );
}

static kern_return_t
InstallFromTimingOverride( IOFBConnectRef connectRef, 
			   IODetailedTimingInformation * desc,
		           IOOptionBits dmFlags)
{
    IOReturn		err;
    IOFBDisplayModeDescription modeDesc;
    IOTimingInformation	*      timing = &modeDesc.timingInfo;

    bzero( &modeDesc, sizeof( modeDesc ));
    timing->flags = kIODetailedTimingValid;

    TimingToHost( desc, &timing->detailedInfo.v2 );

    if (!connectRef->defaultWidth)
    {
	connectRef->defaultWidth       = timing->detailedInfo.v2.horizontalActive;
	connectRef->defaultHeight      = timing->detailedInfo.v2.verticalActive;
	// doh!:
	connectRef->defaultImageWidth  = timing->detailedInfo.v2.horizontalActive;
	connectRef->defaultImageHeight = timing->detailedInfo.v2.verticalActive;
    }
    err = InstallTiming( connectRef, &modeDesc,
			    dmFlags,
			    kIOFBEDIDDetailedMode );

    return( err );
}

static IOReturn
InstallTimingForResolution( IOFBConnectRef connectRef, EDID * edid,
			    IOFBResolutionSpec * spec,
			    IOOptionBits dmFlags, IOOptionBits modeGenFlags )
{
    IOReturn		err;
    CFNumberRef		num;
    IOFBDisplayModeDescription modeDesc;
    IOTimingInformation	*      timing = &modeDesc.timingInfo;

    bzero( &modeDesc, sizeof( modeDesc ));
    timing->flags = kIODetailedTimingValid;

    do
    {
	err = StandardResolutionToDetailedTiming( connectRef, edid, spec, timing );

	DEBG(connectRef, "%s std-mode for %ldx%ld@%ld, id %ld, genFlags 0x%lx\n", 
		(kIOReturnSuccess == err) ? "Have" : "No",
		spec->width, spec->height, (UInt32)(spec->refreshRate + 0.5),
		timing->appleTimingID, modeGenFlags);

	if (kIOReturnSuccess == err)
	{
	    if (kIOFBGTFMode & modeGenFlags)
	    {
		modeGenFlags = kIOFBStdMode;
		dmFlags      = kDisplayModeValidFlag | kDisplayModeSafeFlag;
	    }
	}
	else
	{
	    if (!(kIOFBCVTEstMode & modeGenFlags)
#if GTF_ON_TV
		&& !(connectRef->hasInterlaced)
#endif
		&& (connectRef->overrides 
		&& CFDictionaryGetValue(connectRef->overrides, CFSTR(kIODisplayIsDigitalKey))))
	    {
		err = kIOReturnUnsupportedMode;
		continue;
	    }
	    err = GTFToDetailedTiming( connectRef, edid, spec, 8,
					  (IODetailedTimingInformation *) &timing->detailedInfo.v2 );

	    if( kIOReturnSuccess != err)
		continue;
	}

	if( connectRef->overrides
	 && (num = CFDictionaryGetValue( connectRef->overrides, CFSTR("sync") )))
	 {
	    UInt32 hSyncMask, hSyncValue, vSyncMask, vSyncValue;

	    CFNumberGetValue( num, kCFNumberSInt32Type, &vSyncValue );
	    hSyncMask  = 0xff & (vSyncValue >> 24);
	    hSyncValue = 0xff & (vSyncValue >> 16);
	    vSyncMask  = 0xff & (vSyncValue >> 8);
	    vSyncValue = 0xff & (vSyncValue >> 0);
	    if ((hSyncValue != (timing->detailedInfo.v2.horizontalSyncConfig & hSyncMask))
	     || (vSyncValue != (timing->detailedInfo.v2.verticalSyncConfig   & vSyncMask)))
	    {
		err = kIOReturnUnsupportedMode;
		continue;
	    }
	}
    
	err = InstallTiming( connectRef, &modeDesc, dmFlags, modeGenFlags );
    }
    while (false);

    DEBG(connectRef, "%x\n", err);
    
    return (err);
}

static IOReturn
InstallGTFResolution( IOFBConnectRef connectRef, EDID * edid,
                      float h, float v )
{
    IOReturn	 	err = kIOReturnSuccess;
    CFArrayRef	 	array;
    CFTypeRef	 	obj;
    CFIndex	 	count, i;
    IOOptionBits	dmFlags;
    IOFBResolutionSpec	spec = { 0 };
    UInt32       	width  = (UInt32) h;
    UInt32       	height = (UInt32) v;		    // rounding?

    if (width > connectRef->dimensions.width)
        return (kIOReturnNoSpace);
    if (height > connectRef->dimensions.height)
        return (kIOReturnNoSpace);

    array = CFDictionaryGetValue( connectRef->iographicsProperties, CFSTR("gtf-refresh-rates") );
    count = array ? CFArrayGetCount(array) : 0;

    dmFlags = kDisplayModeValidFlag;
    if (connectRef->gtfDisplay)
	dmFlags |= kDisplayModeSafeFlag;
    if( !connectRef->gtfDisplay || (ratioOver(connectRef->nativeAspect, h / v) > 1.03125))
        dmFlags |= kDisplayModeNotPresetFlag;

    spec.width         = width;
    spec.height        = height;
    spec.flags	       = 0;
    if (connectRef->supportsReducedBlank)
	spec.flags |= kResSpecReducedBlank;

    for (i = 0; i < count; i++)
    {
        obj = CFArrayGetValueAtIndex(array, i);
        if( CFNumberGetTypeID() != CFGetTypeID(obj))
            continue;
        CFNumberGetValue( (CFNumberRef) obj, kCFNumberFloatType, &spec.refreshRate );

	err = InstallTimingForResolution( connectRef, edid,
                                          &spec, dmFlags, kIOFBGTFMode );

	if ((kIOReturnSuccess != err) 
	  && connectRef->hasInterlaced)
	{
	    spec.flags |= kResSpecNeedInterlace;
	    err = InstallTimingForResolution( connectRef, edid,
					      &spec, dmFlags, kIOFBGTFMode );
	    spec.flags &= ~kResSpecNeedInterlace;
	}
    }
    
    return (err);
}

static void
InstallStandardEstablishedTiming(
		    IOFBConnectRef connectRef, EDID * edid,
		    IOFBResolutionSpec * spec )

{
    InstallTimingForResolution( connectRef, edid, spec,
                                kDisplayModeValidFlag | kDisplayModeSafeFlag, kIOFBEDIDStdEstMode );
}

static void
InstallStandardEstablishedTimings( IOFBConnectRef connectRef, EDID * edid  )
{
    CFDataRef		data;
    IOFBResolutionSpec	spec = { 0 };
    UInt32 *		establishedIDs;
    UInt32		i;

    data = CFDictionaryGetValue(connectRef->iographicsProperties, CFSTR("established-ids"));
    if (data)
	establishedIDs = (UInt32 *) CFDataGetBytePtr(data);
    else
	establishedIDs = 0;

    spec.flags = 0;

    for( i = 7; establishedIDs && (i < 24); i++ )
    {
        if (0 != (edid->establishedTimings[ 2 - (i / 8) ] & (1 << (i % 8))))
	{
	    spec.timingID = OSReadBigInt32(&establishedIDs[i], 0);
	    if (spec.timingID != kIOTimingIDInvalid)
		InstallStandardEstablishedTiming(connectRef, edid, &spec);
	}
    }

    for( i = 0; i < 8; i++ )
    {
	spec.timingID = kIOTimingIDInvalid;
	if (kIOReturnSuccess != DecodeStandardTiming(edid, OSReadBigInt16(&edid->standardTimings[i], 0),
							&spec.width, &spec.height, &spec.refreshRate))
	    continue;
	InstallStandardEstablishedTiming(connectRef, edid, &spec);
    }
}

static void
InstallCVTStandardTimings( IOFBConnectRef connectRef, EDID * edid  )
{
    EDIDGeneralDesc *	desc;
    CFDataRef		data;
    CFArrayRef		array;
    CFIndex		i, count;
    IOFBResolutionSpec	spec = { 0 };
    UInt8 *		bits;

    connectRef->gtfDisplay = (0 != (edid->featureSupport & 1));

    data = CFDictionaryGetValue(connectRef->overrides, CFSTR("drng"));
    if (!data)
	return;
    desc = (EDIDGeneralDesc *) CFDataGetBytePtr(data);

    if (0x04 == desc->data[5])
    {
	connectRef->cvtDisplay = true;
	connectRef->gtfDisplay = true;
    }

    if (!connectRef->cvtDisplay)
	return;

    array = CFDictionaryGetValue(connectRef->iographicsProperties, CFSTR("cvt-established-ids"));
    if (!array)
	return;

    count = CFArrayGetCount(array);
    if (count > (7 * 8))
	count = 7 * 8;
    bits = &desc->data[6];
    for (i = 0; i < count; i++)
    {
        if (0 != (bits[i / 8] & (0x80 >> (i % 8))))
	{
	    UInt32 * rec;
	    CFIndex  num;
	    
	    data = CFArrayGetValueAtIndex(array, i);
	    rec = (UInt32 *) CFDataGetBytePtr(data);
	    num = CFDataGetLength(data) / sizeof(UInt32);
	    if (num < 4)
		continue;

	    spec.width         = OSReadBigInt32(rec, 0);
	    spec.height        = OSReadBigInt32(rec, 4);
	    spec.refreshRate   = OSReadBigInt32(rec, 8);
	    spec.flags         = OSReadBigInt32(rec, 12);
	    if (num >= 5)
		spec.timingID  = OSReadBigInt32(rec, 16);
	    else
		spec.timingID  = kIOTimingIDInvalid;

	    connectRef->supportsReducedBlank |= (0 != (kResSpecReducedBlank & spec.flags));

	    InstallTimingForResolution(connectRef, edid, &spec,
					kDisplayModeValidFlag | kDisplayModeSafeFlag, kIOFBCVTEstMode);
	}
    }
}

static Boolean
IODisplayConsiderAspect( float w, float h, float * aspectWidth, float * aspectHeight )
{
    float ratio;

    if (!w || !h)
	return (false);

    ratio = w / h;

    if ((ratio > 1.85) || (ratio < 1.2))
    {
	*aspectWidth = w;
	*aspectHeight = h;
	return (true);
    }

    if (ratio > 1.65)
    {
	*aspectWidth = 16.0;
	*aspectHeight = 9.0;
	return (true);
    }

    if (ratio > 1.55)
    {
	*aspectWidth = 16.0;
	*aspectHeight = 10.0;
	return (true);
    }

    if (ratio > 1.45)
    {
	*aspectWidth = 3.0;
	*aspectHeight = 2.0;
	return (true);
    }

    return (false);
}

static void
IODisplayGetAspect( IOFBConnectRef connectRef )
{
    CFDictionaryRef ovr;
    CFNumberRef     imageH, imageV;
    float	    aspectWidth, aspectHeight;
    float	    w, h;

    aspectWidth    = 4.0;
    aspectHeight   = 3.0;

    ovr = connectRef->overrides;
    do
    {
	if (IODisplayConsiderAspect(connectRef->defaultWidth, connectRef->defaultHeight,
				    &aspectWidth, &aspectHeight))
	    break;

	if (IODisplayConsiderAspect(connectRef->defaultImageWidth, connectRef->defaultImageHeight,
				    &aspectWidth, &aspectHeight))
	    break;

	if( ovr)
	{
	    imageH = CFDictionaryGetValue( ovr, CFSTR(kDisplayHorizontalImageSize) );
	    imageV = CFDictionaryGetValue( ovr, CFSTR(kDisplayVerticalImageSize) );
	    if (imageH && imageV)
	    {
		CFNumberGetValue( imageH, kCFNumberFloatType, &w );
		CFNumberGetValue( imageV, kCFNumberFloatType, &h );
		if (IODisplayConsiderAspect(w, h, &aspectWidth, &aspectHeight))
		    break;
	    } 
	}
    }
    while (false);

    connectRef->nativeAspect = aspectWidth / aspectHeight;
}


static kern_return_t
InstallGTFTimings( IOFBConnectRef connectRef, EDID * edid )
{
    IOReturn		err = kIOReturnSuccess;
    UInt32		i;
    CFArrayRef		array;
    CFIndex		count;
    CFStringRef		key;
    float		h, v, nativeAspect;
    Boolean		wide, displayNot4By3;

    // arb timings

    nativeAspect = connectRef->nativeAspect;
    wide = (nativeAspect > 1.45);
    displayNot4By3 = (ratioOver(nativeAspect, 4.0 / 3.0) > 1.03125);

    key = wide ? CFSTR("gtf-resolutions-wide") : CFSTR("gtf-resolutions");
    array = CFDictionaryGetValue( connectRef->iographicsProperties, key );
    count = array ? CFArrayGetCount(array) : 0;

    for( i = 0; i < count; i++)
    {
        CFTypeRef obj;

        obj = CFArrayGetValueAtIndex(array, i);
        if( CFNumberGetTypeID() == CFGetTypeID(obj)) {
            SInt32	value;
            CFNumberGetValue( (CFNumberRef) obj, kCFNumberSInt32Type, &value );
            h = (float) (value & 0xffff);
            v = (float) (value >> 16);

        } else
            continue;

	if (v)
        {
            err = InstallGTFResolution( connectRef, edid, h, v );
        }
        else
        {
            if (displayNot4By3)
	    {
                err = InstallGTFResolution( connectRef, edid, h, (h * 3.0) / 4.0 );
	    }
            err = InstallGTFResolution( connectRef, edid, h, h / nativeAspect );
        }
    }

    return( err );
}

static void
InstallDIEXT( IOFBConnectRef connectRef, EDID * edid, DIEXT * ext )
{
    UInt16 minPixelClockPerLink, maxPixelClockPerLink, crossover;
    
    minPixelClockPerLink = ext->minPixelClockPerLink;
    maxPixelClockPerLink = (ext->maxPixelClockPerLink[1] << 16) | ext->maxPixelClockPerLink[0];
    crossover = (ext->crossoverFreq[1] << 16) | ext->crossoverFreq[0];

    if ((3 == ext->standardSupported) || (4 == ext->standardSupported))
    {
	connectRef->dualLinkCrossover = ((UInt64) crossover) * 1000000ULL;
	connectRef->maxDisplayLinks = 2;
    }
    else
	connectRef->maxDisplayLinks = 1;

    DEBG(connectRef, "(%d) minPixelClockPerLink %d, maxPixelClockPerLink %d, cross %qd\n", 
	    ext->standardSupported,
	    minPixelClockPerLink, maxPixelClockPerLink, connectRef->dualLinkCrossover);
}

static void
InstallVTBEXT( IOFBConnectRef connectRef, EDID * edid, VTBEXT * ext )
{
    IOByteCount        offset = 0;
    IOItemCount        idx, count;
    IOFBResolutionSpec spec;
    Boolean            malformed = false;

    do
    {
	count = ext->numDetailedTimings;
	for (idx = 0; idx < count; idx++)
	{
	    malformed = (offset > (sizeof(ext->data) - sizeof(EDIDDetailedTimingDesc)));
	    if (malformed)
		break;
	    InstallFromEDIDDesc( connectRef,
				    edid, (EDIDDetailedTimingDesc *) &ext->data[offset],
				    kDisplayModeValidFlag | kDisplayModeSafeFlag );
	    offset += sizeof(EDIDDetailedTimingDesc);
	}
	if (malformed)
	    break;

	// --
	count = ext->numCVTTimings;
	for (idx = 0; idx < count; idx++)
	{
	    SInt32              refreshBit;
	    UInt32              vsize;
	    VTBCVTTimingDesc *  desc;
	    static const UInt32 cvtRefreshRates[] = { 60, 85, 75, 60, 50 };

	    malformed = (offset > (sizeof(ext->data) - sizeof(VTBCVTTimingDesc)));
	    if (malformed)
		break;

	    desc = (VTBCVTTimingDesc *) &ext->data[offset];
	    vsize = ((desc->verticalSizeHigh & 0xf0) << 4) | desc->verticalSize;
	    vsize = 2 * (vsize + 1);
	    spec.height = vsize;
	    switch (kVTBCVTAspectRatioMask & desc->verticalSizeHigh)
	    {
		case kVTBCVTAspectRatio16By10:
		    spec.width = (vsize * 16) / 10;
		    break;
		case kVTBCVTAspectRatio16By9:
		    spec.width = (vsize * 16) / 9;
		    break;
		case kVTBCVTAspectRatio4By3:
		default:
		    spec.width = (vsize * 4) / 3;
		    break;
	    }
	    spec.timingID      = kIOTimingIDInvalid;
	    spec.width &= ~7;
	    for (refreshBit = 4; refreshBit >= 0; refreshBit--)
	    {
		if ((1 << refreshBit) & desc->refreshRates)
		{
		    spec.refreshRate   = cvtRefreshRates[refreshBit];
		    spec.flags = (kVTBCVTRefresh60RBlank == (1 << refreshBit)) ? kResSpecReducedBlank : kNilOptions;
		    InstallTimingForResolution(connectRef, edid, &spec,
					    kDisplayModeValidFlag | kDisplayModeSafeFlag, kIOFBCVTEstMode);
		    DEBG(connectRef, "VTB cvt: %ldx%ld @ %f (%lx)\n", 
			    spec.width, spec.height, spec.refreshRate, spec.flags);
		}
	    }
	    offset += sizeof(VTBCVTTimingDesc);
	}

	// --
	count = ext->numStandardTimings;
	for (idx = 0; idx < count; idx++)
	{
	    UInt16 stdTiming;

	    malformed = (offset > (sizeof(ext->data) - sizeof(UInt16)));
	    if (malformed)
		break;

	    spec.flags    = 0;
	    spec.timingID = kIOTimingIDInvalid;
	    stdTiming = (ext->data[offset] << 8) | ext->data[offset + 1];

	    DEBG(connectRef, "VTB st: %04x\n", stdTiming);
	    if (kIOReturnSuccess != DecodeStandardTiming(edid, stdTiming,
							    &spec.width, &spec.height, &spec.refreshRate))
		continue;
	    DEBG(connectRef, "VTB st: %ldx%ld @ %f (%lx)\n", 
		    spec.width, spec.height, spec.refreshRate, spec.flags);
	    InstallStandardEstablishedTiming(connectRef, edid, &spec);
	    offset += sizeof(UInt16);
	}
    }
    while (false);
}

static void
InstallCEA861EXT( IOFBConnectRef connectRef, EDID * edid, CEA861EXT * ext )
{
    IOByteCount offset = ext->detailedTimingsOffset;

    if (offset < 4)
	return;
    offset -= 4;
    while (offset <= (sizeof(ext->data) - sizeof(EDIDDetailedTimingDesc)))
    {
	DEBG(connectRef, "FromCEA:\n");
	InstallFromEDIDDesc( connectRef,
 				 edid, (EDIDDetailedTimingDesc *) &ext->data[offset],
				 kDisplayModeValidFlag | kDisplayModeSafeFlag );
	offset += sizeof(EDIDDetailedTimingDesc);
    }
}

static void
LookExtensions( IOFBConnectRef connectRef, EDID * edid, UInt8 * blocks, IOByteCount length)
{
    UInt8 tag;
    
    while (length >= 128)
    {

	tag = blocks[0];
	switch (tag)
	{
	    case kExtTagDI:
		InstallDIEXT( connectRef, edid, (DIEXT *) blocks);
		connectRef->hasDIEXT = true;
		break;
	    case kExtTagVTB:
		InstallVTBEXT( connectRef, edid, (VTBEXT *) blocks);
		break;
	    case kExtTagCEA:
		connectRef->hasCEAExt = true;
                if (connectRef->scalerInfo && (kIOScaleCanSupportInset & connectRef->scalerInfo->scalerFeatures))
                    connectRef->useScalerUnderscan = connectRef->hasCEAExt;
		InstallCEA861EXT( connectRef, edid, (CEA861EXT *) blocks);
		break;
	}
	length -= 128;
    }
}

__private_extern__ void
IODisplayInstallTimings( IOFBConnectRef connectRef )
{
    int				i, defaultIndex;
    io_service_t		service = connectRef->framebuffer;
    EDID *			edid = 0;
    CFDataRef			fbRange;
    CFDataRef			data;
    CFArrayRef			array;
    CFIndex			count;

    static const GTFTimingCurve defaultGTFCurves[] = {
	{ 0,          40, 600, 128, 20 },
	{ 0xffffffff,  0,   0,   0,  0 }
    };

    // controller timing range
    fbRange = (CFDataRef) IORegistryEntryCreateCFProperty( service, 
							    CFSTR(kIOFBTimingRangeKey),
							    kCFAllocatorDefault, kNilOptions);
    if (fbRange && CFDataGetLength(fbRange) >= sizeof(IODisplayTimingRange))
	connectRef->fbRange = (IODisplayTimingRange *) CFDataGetBytePtr(fbRange);


    connectRef->gtfDisplay	     = false;
    connectRef->cvtDisplay	     = false;
    connectRef->supportsReducedBlank = false;
    connectRef->hasInterlaced	     = false;
    connectRef->hasDIEXT	     = false;
    connectRef->hasCEAExt	     = false;
    connectRef->numGTFCurves         = 1;
    bcopy(&defaultGTFCurves, &connectRef->gtfCurves, sizeof(connectRef->gtfCurves));

    // defaults for no DIEXT
    if (CFDictionaryGetValue(connectRef->overrides, CFSTR(kIODisplayIsDigitalKey)))
    {
	connectRef->dualLinkCrossover = 165000000ULL;
	connectRef->maxDisplayLinks   = 2;
    }
    else
    {
	connectRef->dualLinkCrossover = 0;
	connectRef->maxDisplayLinks   = 0;
    }

#if RLOG
    DEBG(connectRef, "FB range:\n");
    IOFBLogRange(connectRef, connectRef->fbRange);

    DEBG(connectRef, "Display range:\n");
    data = CFDictionaryGetValue(connectRef->overrides, CFSTR("trng"));
    if (data && CFDataGetLength(data) >= sizeof(IODisplayTimingRange))
	IOFBLogRange(connectRef, (IODisplayTimingRange *) CFDataGetBytePtr(data));
#endif

    do 
    {
        // EDID timings

        data = CFDictionaryGetValue( connectRef->overrides, CFSTR(kIODisplayEDIDKey) );
        if( !data || (CFDataGetLength(data) < sizeof( EDID)) )
            continue;
#if RLOG
	if (connectRef->logfile)
	{
	    DEBG(connectRef, "EDID\n");
	    fprintf(connectRef->logfile, "/*          0    1    2    3    4    5    6    7    8    9    A    B    C    D    E    F */");
	    for (i = 0; i < CFDataGetLength(data); i++)
	    {
		if( 0 == (i & 15))
		    fprintf(connectRef->logfile, "\n/* %02X */ ", i);
		fprintf(connectRef->logfile, "0x%02x,", CFDataGetBytePtr( data )[i]);
	    }
	    fprintf(connectRef->logfile, "\n");
	    fflush(connectRef->logfile);
	}
#endif
        edid = (EDID *) CFDataGetBytePtr( data );

        static const uint8_t checkHeader[] = { 0x00,0xff,0xff,0xff,0xff,0xff,0xff,0x00 };
        if (bcmp(&checkHeader[0], &edid->header[0], sizeof(checkHeader)))
	{
	    edid = 0;
            continue;
	}
	count = CFDataGetLength(data);
	if (count > sizeof(EDID))
	    LookExtensions(connectRef, edid, (UInt8 *)(edid + 1), count - sizeof(EDID));

#if RLOG
        if (connectRef->scalerInfo && (kIOScaleCanSupportInset & connectRef->scalerInfo->scalerFeatures))
            connectRef->useScalerUnderscan |= (connectRef->displayVendor == 4268);
#endif

	DEBG(connectRef, "Display maxLinks %ld, crossover %qd\n", 
			connectRef->maxDisplayLinks, connectRef->dualLinkCrossover);

        // max dimensions
        connectRef->dimensions.setFlags = kDisplayModeNotPresetFlag;
        if( CFDictionaryGetValue( connectRef->overrides, CFSTR(kIODisplayIsDigitalKey)))
            connectRef->dimensions.clearFlags = 
		kDisplayModeValidFlag | kDisplayModeSafeFlag | kDisplayModeDefaultFlag;
        else
            connectRef->dimensions.clearFlags =
		kDisplayModeSafeFlag | kDisplayModeDefaultFlag;

        // override timing recs (18-byte)
        array = (CFArrayRef) CFDictionaryGetValue( connectRef->overrides, CFSTR("dspc"));
        if( array)
            count = CFArrayGetCount(array);
        else
            count = 0;
        for( i = 0; i < count; i++ ) {
            data = CFArrayGetValueAtIndex(array, i);
            if( !data || (sizeof(EDIDDetailedTimingDesc) != CFDataGetLength(data)))
                continue;

            InstallFromEDIDDesc( connectRef,
 				 edid, (EDIDDetailedTimingDesc *) CFDataGetBytePtr(data),
				 kDisplayModeValidFlag | kDisplayModeSafeFlag );
        }

	if ((edid->version > 1) || (edid->revision >= 3))
	{
	    if (2 & edid->featureSupport)
		defaultIndex = 0;
	    else if (connectRef->hasDIEXT)
		defaultIndex = 1;
	    else
		defaultIndex = 0;
	}
	else if (2 & edid->featureSupport)
	    defaultIndex = 0;
	else
	    defaultIndex = -1;

	DEBG(connectRef, "EDID default idx %d\n", defaultIndex);

	if ((1 == defaultIndex) 
	 && !CFDictionaryGetValue(connectRef->overrides, CFSTR("trng")))
	{
	    // just install the default for scaling, if it works
	    IOReturn
	    err = InstallFromEDIDDesc(connectRef,
                                 edid,
                                 &edid->descriptors[1].timing,
				 kDisplayModeValidFlag | kDisplayModeSafeFlag | kDisplayModeDefaultFlag);
	    if ((kIOReturnSuccess == err) || (kIOReturnPortExists == err))
		continue;
	}

        // EDID timing recs
	for( i = 0; i < 4; i++ )
	{
            if( i && (0 == bcmp( &edid->descriptors[0].timing,
                                 &edid->descriptors[i].timing,
                                 sizeof( EDIDDetailedTimingDesc))))
                continue;

            InstallFromEDIDDesc( connectRef,
                                 edid,
                                 &edid->descriptors[i].timing,
				 kDisplayModeValidFlag | kDisplayModeSafeFlag
				 | ((i == defaultIndex) ? kDisplayModeDefaultFlag : 0));
        }

        InstallCVTStandardTimings( connectRef, edid );
        InstallStandardEstablishedTimings( connectRef, edid );
    }
    while( false );

    // override timing recs
    array = (CFArrayRef) CFDictionaryGetValue( connectRef->overrides, CFSTR("tspc"));
    if (array)
	count = CFArrayGetCount(array);
    else
	count = 0;
    for (i = 0; i < count; i++ ) {
	data = CFArrayGetValueAtIndex(array, i);
	if( !data || (sizeof(IODetailedTimingInformation) != CFDataGetLength(data)))
	    continue;

	InstallFromTimingOverride(connectRef, 
		(IODetailedTimingInformation *) CFDataGetBytePtr(data),
		kDisplayModeValidFlag | kDisplayModeSafeFlag);
    }

    IODisplayGetAspect( connectRef );

    if (CFDictionaryGetValue(connectRef->overrides, CFSTR("trng"))
    || ((!CFDictionaryGetValue(connectRef->overrides, CFSTR(kIODisplayIsDigitalKey)))
	&& ((connectRef->displayVendor != kDisplayVendorIDUnknown)
	 || (connectRef->displayProduct == kDisplayProductIDGeneric))
	&& (!edid || ((0xffffffff != connectRef->dimensions.width)
		    && (0xffffffff != connectRef->dimensions.height)))))
    {
	// have range limits, or analog-VGA/nonsensed
	InstallGTFTimings( connectRef, edid );
    }

    connectRef->fbRange = 0;
    if (fbRange)
	CFRelease(fbRange);
}

SInt32
IODisplayMatchDictionaries(
        CFDictionaryRef		matching1,
        CFDictionaryRef		matching2,
	IOOptionBits		options )
{
    CFNumberRef		num1, num2;
    CFStringRef		str1, str2;
    SInt32		matches = 0;

    if( !matching1 || !matching2)
        return( -1 );

    do {
        num1 = CFDictionaryGetValue( matching1, CFSTR(kDisplayVendorID) );
        num2 = CFDictionaryGetValue( matching2, CFSTR(kDisplayVendorID) );
        if( !num1 || !num2)
            continue;
        if( !CFEqual( num1, num2))
            continue;

        num1 = CFDictionaryGetValue( matching1, CFSTR(kDisplayProductID) );
        num2 = CFDictionaryGetValue( matching2, CFSTR(kDisplayProductID) );
        if( !num1 || !num2)
            continue;
        if( !CFEqual( num1, num2))
            continue;

        num1 = CFDictionaryGetValue( matching1, CFSTR(kDisplaySerialNumber) );
        num2 = CFDictionaryGetValue( matching2, CFSTR(kDisplaySerialNumber) );
        if( num1 && num2 && (!CFEqual( num1, num2)))
            continue;

        str1 = CFDictionaryGetValue( matching1, CFSTR(kIODisplayLocationKey) );
        str2 = CFDictionaryGetValue( matching2, CFSTR(kIODisplayLocationKey) );
        if( str1 && str2 && (!CFEqual( str1, str2)))
            continue;

        matches = 1000;

    } while( false );

    return( matches );
}

io_service_t
IODisplayForFramebuffer(
	io_service_t		framebuffer,
	IOOptionBits		options )
{
    IOReturn		kr;
    io_iterator_t	iter;
    io_service_t	service = 0;

    if( IOObjectConformsTo( framebuffer, "IODisplay"))
        return( framebuffer );

    kr = IORegistryEntryCreateIterator( framebuffer, kIOServicePlane,
					kIORegistryIterateRecursively, &iter);
    if( kr != kIOReturnSuccess )
	return( 0 );

    for( ;
	(service = IOIteratorNext( iter));
	IOObjectRelease(service)) {

        if( IOObjectConformsTo( service, "IODisplay"))
            break;
    }
    IOObjectRelease( iter );

    return( service );
}

enum {
    /* Used by default calibrator (should we show brightness panel) */
    kDisplayGestaltBrightnessAffectsGammaMask	= (1 << 0),
    kDisplayGestaltViewAngleAffectsGammaMask	= (1 << 1)
};

CFDictionaryRef
_IODisplayCreateInfoDictionary(
    IOFBConnectRef		connectRef,
    io_service_t		framebuffer,
    IOOptionBits		options )
{
    IOReturn			kr;
    io_service_t		service = 0;
    Boolean			isDigital = false;
    CFDataRef			data = 0;
    CFNumberRef			num;
    CFMutableDictionaryRef	dict = 0;
    CFMutableDictionaryRef	regDict;
    CFMutableDictionaryRef	fbRegDict;
    CFTypeRef			obj;
    SInt32			sint;
    UInt8			low;
    float			fnum;
    EDID *			edid = 0;
    IODisplayVendorID		vendor = 0;
    IODisplayProductID		product = 0;
    SInt32			displayType = 0;
    UInt32			serialNumber = 0;
    CFAbsoluteTime		manufactureDate;
    io_string_t			path;
    int				i;
    IODisplayTimingRange	displayRange;

    bzero( &manufactureDate, sizeof(manufactureDate) );

    if( !(service = IODisplayForFramebuffer( framebuffer, options))) {
	dict = CFDictionaryCreateMutable( kCFAllocatorDefault, 0,
                                            &kCFTypeDictionaryKeyCallBacks,
                                            &kCFTypeDictionaryValueCallBacks );
        if( dict)
            CFDictionarySetValue( dict, CFSTR(kIODisplayLocationKey), CFSTR("unknown"));
        return( dict );
    }

    do {

        regDict = 0;
        kr = IORegistryEntryCreateCFProperties( service, &regDict,
                                                kCFAllocatorDefault, kNilOptions );
        if( kr != kIOReturnSuccess)
            continue;

        num = CFDictionaryGetValue( regDict, CFSTR(kDisplayVendorID) );
        if( num)
            CFNumberGetValue( num, kCFNumberSInt32Type, &vendor );
        num = CFDictionaryGetValue( regDict, CFSTR(kDisplayProductID) );
        if( num)
            CFNumberGetValue( num, kCFNumberSInt32Type, &product );

        num = CFDictionaryGetValue( regDict, CFSTR(kAppleDisplayTypeKey) );
        if( num) {
            CFNumberGetValue( num, kCFNumberSInt32Type, &displayType );
            if( (vendor == kDisplayVendorIDUnknown) && (displayType == 10))
                product = kDisplayProductIDGeneric;
        }

        data = CFDictionaryGetValue( regDict, CFSTR(kIODisplayEDIDKey) );

#if SPOOF_EDID
#warning             ****************
#warning             ** SPOOF_EDID **
#warning             ****************

//	if (!connectRef || !connectRef->dependentID || connectRef->dependentIndex)
	{
	    vm_offset_t 		bytes;
	    vm_size_t			byteLen;
    
	    if (kIOReturnSuccess == readFile( "/testedid", &bytes, &byteLen ))
	    {
		data = CFDataCreateWithBytesNoCopy( kCFAllocatorDefault,
						    (const void *) bytes, byteLen, kCFAllocatorNull );
//		vm_deallocate( mach_task_self(), bytes, byteLen );
	    }
	    else
		data = CFDataCreateWithBytesNoCopy( kCFAllocatorDefault,
						    spoofEDID, 128, kCFAllocatorNull );

	}

	vendor = product = 0;
#endif
        if( !data)
            continue;
        edid = (EDID *) CFDataGetBytePtr( data );
        if( vendor && product)
            EDIDInfo( edid, 0, 0, &serialNumber, &manufactureDate, &isDigital );
        else
            EDIDInfo( edid, &vendor, &product, &serialNumber, &manufactureDate, &isDigital );

    } while( false );

    // <hack>
    if( !vendor && !product) {
        vendor = kDisplayVendorIDUnknown;
        product = kDisplayProductIDGeneric;
    } // </hack>

    dict = IODisplayCreateOverrides( options, vendor, product,
                                        serialNumber, manufactureDate, isDigital );

#define makeInt( key, value )	\
	num = CFNumberCreate( kCFAllocatorDefault, kCFNumberSInt32Type, &value );	\
	CFDictionaryAddValue( dict, key,  num );					\
	CFRelease( num );

#define addFloat( key )	\
	num = CFNumberCreate( kCFAllocatorDefault, kCFNumberFloatType, &fnum  );	\
	CFDictionaryAddValue( dict, key, num );						\
	CFRelease( num );

    do {
        if( !dict)
	    continue;

	makeInt( CFSTR( kDisplayVendorID ), vendor );
	makeInt( CFSTR( kDisplayProductID ), product );

	if( serialNumber) {
            makeInt( CFSTR( kDisplaySerialNumber ), serialNumber );
        }

        kr = IORegistryEntryGetPath( service, kIOServicePlane, path );
        if( KERN_SUCCESS == kr) {
            CFStringRef string;
            string = CFStringCreateWithCString( kCFAllocatorDefault, path,
                                        kCFStringEncodingMacRoman );
            if( string) {
                CFDictionaryAddValue( dict, CFSTR(kIODisplayLocationKey), string);
                CFRelease(string);
            }
        }

	if ((kIODisplayNoProductName | kIODisplayMatchingInfo) & options)
	    // the raw override form is not what clients expect
	    CFDictionaryRemoveValue( dict, CFSTR(kDisplayProductName) );

        // -- that's all for matching --
        if( options & kIODisplayMatchingInfo)
            continue;

        // if !exist add display edid
        if( data)
            CFDictionaryAddValue( dict, CFSTR(kIODisplayEDIDKey), data);
        // get final edid
        data = CFDictionaryGetValue(dict, CFSTR(kIODisplayEDIDKey));
        if (data)
            edid = (EDID *) CFDataGetBytePtr(data);
            // no point in serial# / manufacture date from override
        else
            edid = 0;

        obj = CFDictionaryGetValue( regDict, CFSTR(kIODisplayConnectFlagsKey) );
        if( obj)
            CFDictionarySetValue( dict, CFSTR(kIODisplayConnectFlagsKey), obj );

        obj = CFDictionaryGetValue( regDict, CFSTR(kIODisplayPrefKeyKey) );
        if( obj)
            CFDictionarySetValue( dict, CFSTR(kIODisplayPrefKeyKey), obj );

        if( IOObjectConformsTo( service, "IOBacklightDisplay"))
            CFDictionarySetValue( dict, CFSTR(kIODisplayHasBacklightKey), kCFBooleanTrue );

        kr = IORegistryEntryCreateCFProperties(framebuffer, &fbRegDict,
                                                kCFAllocatorDefault, kNilOptions );
        if (kIOReturnSuccess == kr)
	{
	    if( (obj = CFDictionaryGetValue(fbRegDict, CFSTR(kIOFBTransformKey))))
	    {
		CFNumberGetValue(obj, kCFNumberSInt32Type, &sint);
		sint = (sint & kIOFBRotateFlags) | ((sint >> 4) & kIOFBRotateFlags);
                makeInt( CFSTR(kIOFBTransformKey), sint );
	    }
	    if( (obj = CFDictionaryGetValue(fbRegDict, CFSTR("graphic-options"))))
		CFDictionaryAddValue(dict, CFSTR("graphic-options"), obj);
	    CFRelease(fbRegDict);
	}

        data = CFDictionaryGetValue( dict, CFSTR("dmdg") );
        if( data)
            sint = OSReadBigInt32((void *) CFDataGetBytePtr(data), 0);
        else
            sint = kDisplayGestaltBrightnessAffectsGammaMask;

        if( kDisplayGestaltBrightnessAffectsGammaMask & sint)
            CFDictionaryAddValue( dict, CFSTR(kDisplayBrightnessAffectsGamma), kCFBooleanTrue );
        if( kDisplayGestaltViewAngleAffectsGammaMask & sint)
            CFDictionaryAddValue( dict, CFSTR(kDisplayViewAngleAffectsGamma), kCFBooleanTrue );

	if (!(kIODisplayNoProductName & options))
	    GenerateProductName( dict, edid, displayType, options );

        if( !edid)
            continue;

        if( 0x80 & edid->displayParams[0]) {
            CFDictionarySetValue( dict, CFSTR(kIODisplayIsDigitalKey), kCFBooleanTrue );

            if( kDisplayAppleVendorID == vendor) {
                CFDictionaryAddValue( dict, CFSTR(kDisplayFixedPixelFormat), kCFBooleanTrue );
                sint = kDisplaySubPixelLayoutRGB;
                makeInt( CFSTR( kDisplaySubPixelLayout ), sint );

                CFDictionaryRemoveValue( dict, CFSTR(kDisplayBrightnessAffectsGamma) );
                CFDictionarySetValue( dict, CFSTR(kDisplayViewAngleAffectsGamma), kCFBooleanTrue );
            }
        }

	// EDID timing range
	for( i = 0; i < 4; i++ ) {
	    if( EDIDDescToDisplayTimingRangeRec( edid, 
				&edid->descriptors[i].general,
				&displayRange )) {
				
		if( !CFDictionaryGetValue( dict, CFSTR("drng"))) {
		    data = CFDataCreate( kCFAllocatorDefault,
				(UInt8 *) &edid->descriptors[i].general, sizeof(EDIDGeneralDesc));
		    if( data) {
			CFDictionarySetValue(dict, CFSTR("drng"), data);
			CFRelease(data);
		    }
		}

		if( !CFDictionaryGetValue( dict, CFSTR("trng"))) {
		    data = CFDataCreate( kCFAllocatorDefault,
				(UInt8 *) &displayRange, sizeof(displayRange));
		    if( data) {
			CFDictionarySetValue(dict, CFSTR("trng"), data);
			CFRelease(data);
		    }
		}
		break;
	    }
	}

	sint = edid->weekOfManufacture;
	makeInt( CFSTR( kDisplayWeekOfManufacture ), sint );
	sint = edid->yearOfManufacture + 1990;
	makeInt( CFSTR( kDisplayYearOfManufacture ), sint );

	sint = edid->displayParams[1] * 10;
	makeInt( CFSTR( kDisplayHorizontalImageSize ), sint );
	sint = edid->displayParams[2] * 10;
	makeInt( CFSTR( kDisplayVerticalImageSize ), sint );

        // color info
        low = edid->colorCharacteristics[0];

        fnum = (edid->colorCharacteristics[2] << 2) | ((low >> 6) & 3);
        fnum /= (1 << 10);
        addFloat( CFSTR( kDisplayRedPointX ) );
        fnum = (edid->colorCharacteristics[3] << 2) | ((low >> 4) & 3);
        fnum /= (1 << 10);
        addFloat( CFSTR( kDisplayRedPointY ) );

        fnum = (edid->colorCharacteristics[4] << 2) | ((low >> 2) & 3);
        fnum /= (1 << 10);
        addFloat( CFSTR( kDisplayGreenPointX ) );
        fnum = (edid->colorCharacteristics[5] << 2) | ((low >> 0) & 3);
        fnum /= (1 << 10);
        addFloat( CFSTR( kDisplayGreenPointY ) );

        low = edid->colorCharacteristics[1];

        fnum = (edid->colorCharacteristics[6] << 2) | ((low >> 6) & 3);
        fnum /= (1 << 10);
        addFloat( CFSTR( kDisplayBluePointX ) );
        fnum = (edid->colorCharacteristics[7] << 2) | ((low >> 4) & 3);
        fnum /= (1 << 10);
        addFloat( CFSTR( kDisplayBluePointY ) );

        fnum = (edid->colorCharacteristics[8] << 2) | ((low >> 2) & 3);
        fnum /= (1 << 10);
        addFloat( CFSTR( kDisplayWhitePointX ) );
        fnum = (edid->colorCharacteristics[9] << 2) | ((low >> 0) & 3);
        fnum /= (1 << 10);
        addFloat( CFSTR( kDisplayWhitePointY ) );

        fnum = edid->displayParams[3];
        fnum = (fnum + 100.0) / 100.0;
        addFloat( CFSTR( kDisplayWhiteGamma ) );

    } while( false );

    if( regDict)
        CFRelease( regDict );
        
    return( dict );
}

IOReturn
IODisplayCopyParameters(
	io_service_t	  service,
	IOOptionBits	  options,
        CFDictionaryRef * params )
{
    if( (service = IODisplayForFramebuffer( service, options)))
        *params = IORegistryEntryCreateCFProperty( service, CFSTR(kIODisplayParametersKey),
                                                    kCFAllocatorDefault, kNilOptions );
    else
        *params = 0;

    return( *params ? kIOReturnSuccess : kIOReturnUnsupported );
}

IOReturn
IODisplayCopyFloatParameters(
	io_service_t	  service,
	IOOptionBits	  options,
        CFDictionaryRef * params )
{
    return( kIOReturnUnsupported );
}

IOReturn
IODisplayGetIntegerRangeParameter(
	io_service_t	service,
	IOOptionBits	options,
        CFStringRef	parameterName,
        SInt32 *	value,
        SInt32 *	min,
        SInt32 *	max )
{
    IOReturn		err;
    CFDictionaryRef	params;
    CFDictionaryRef	param;
    CFNumberRef		num;

#if DEBUGPARAMS
    const char *	cStr = 0;
    
    if( (cStr = CFStringGetCStringPtr( parameterName, kCFStringEncodingMacRoman))
    && (cStr = getenv(cStr)))
        parameterName =  CFStringCreateWithCString( kCFAllocatorDefault, cStr,
                                                    kCFStringEncodingMacRoman );
#endif

    do {
        err = IODisplayCopyParameters( service, options, &params );
        if( err != kIOReturnSuccess)
            continue;

        param = CFDictionaryGetValue( params, parameterName );

        if( !param) {
            err = kIOReturnUnsupported;
            continue;
        }
        if( value && (num = CFDictionaryGetValue( param, CFSTR(kIODisplayValueKey))))
            CFNumberGetValue( num, kCFNumberSInt32Type, value );
        if( min && (num = CFDictionaryGetValue( param, CFSTR(kIODisplayMinValueKey))))
            CFNumberGetValue( num, kCFNumberSInt32Type, min );
        if( max && (num = CFDictionaryGetValue( param, CFSTR(kIODisplayMaxValueKey))))
            CFNumberGetValue( num, kCFNumberSInt32Type, max );

    } while( false );

    if( params)
        CFRelease(params);

#if DEBUGPARAMS
    if( cStr)
        CFRelease(parameterName);
#endif

    return( err );
}

IOReturn
IODisplayGetFloatParameter(
	io_service_t	service,
	IOOptionBits	options,
        CFStringRef	parameterName,
        float *		value )
{
    IOReturn	err;
    SInt32	ivalue, min, max;

    err = IODisplayGetIntegerRangeParameter( service, options, parameterName,
                                             &ivalue, &min, &max );
    if( err)
        return( err);

    if( min == max)
        *value = 0;
    else
        *value = (((float) ivalue) - ((float) min)) / (((float) max) - ((float) min));

    return( err );
}


IOReturn
IODisplaySetParameters(
	io_service_t	service,
	IOOptionBits	options,
        CFDictionaryRef	params )
{
    IOReturn err;

    if( !(service = IODisplayForFramebuffer( service, options)))
	return( kIOReturnUnsupported );

    err = IORegistryEntrySetCFProperties( service, params );

    return( err );
}

IOReturn
IODisplaySetIntegerParameter(
	io_service_t	service,
	IOOptionBits	options,
        CFStringRef	parameterName,
        SInt32 		value )
{
    IOReturn		err;
    CFDictionaryRef	dict;
    CFNumberRef		num;

#if DEBUGPARAMS
    const char *	cStr;

    if( (cStr = CFStringGetCStringPtr( parameterName, kCFStringEncodingMacRoman))
     && (cStr = getenv(cStr)))
        parameterName =  CFStringCreateWithCString( kCFAllocatorDefault, cStr,
                                                    kCFStringEncodingMacRoman );
#endif

    num = CFNumberCreate( kCFAllocatorDefault, kCFNumberSInt32Type, &value );
    if( !num)
        return( kIOReturnNoMemory );

    dict = CFDictionaryCreate( kCFAllocatorDefault,
                                (const void **) &parameterName, (const void **) &num, 1,
                                &kCFTypeDictionaryKeyCallBacks,
                                &kCFTypeDictionaryValueCallBacks );
    CFRelease(num);
    if( !dict)
        return( kIOReturnNoMemory );

    err = IODisplaySetParameters( service, kNilOptions, dict );

    CFRelease(dict);

#if DEBUGPARAMS
    if( cStr)
        CFRelease(parameterName);
#endif

    return( err );
}

IOReturn
IODisplaySetFloatParameter(
	io_service_t	service,
	IOOptionBits	options,
        CFStringRef	parameterName,
        float 		value )
{
    IOReturn	err;
    SInt32	ivalue, min, max;

    err = IODisplayGetIntegerRangeParameter( service, options, parameterName,
                                             NULL, &min, &max );
    if( err)
        return( err);

    ivalue = roundf((value * (((float) max) - ((float) min)) + ((float) min)));

    err = IODisplaySetIntegerParameter( service, options, parameterName, ivalue );

    return( err );
}

IOReturn
IODisplayCommitParameters(
	io_service_t	service,
	IOOptionBits	options )
{
    return( IODisplaySetIntegerParameter( service, options,
                CFSTR(kIODisplayParametersCommitKey), 1));
}

#undef IOCreateDisplayInfoDictionary
CFDictionaryRef
IOCreateDisplayInfoDictionary(
	io_service_t		framebuffer,
	IOOptionBits		options )
{
    return( _IODisplayCreateInfoDictionary( NULL, framebuffer, options));
}

CFDictionaryRef
IODisplayCreateInfoDictionary(
	io_service_t		framebuffer,
	IOOptionBits		options )
{
    return( _IODisplayCreateInfoDictionary( NULL, framebuffer, options));
}
