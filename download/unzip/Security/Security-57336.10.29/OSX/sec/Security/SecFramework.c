/*
 * Copyright (c) 2006-2010,2012-2014 Apple Inc. All Rights Reserved.
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
 * SecFramework.c - generic non API class specific functions
 */

#ifdef STANDALONE
/* Allows us to build genanchors against the BaseSDK. */
#undef __ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__
#undef __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__
#endif

#include "SecFramework.h"
#include <dispatch/dispatch.h>
#include <CoreFoundation/CFBundle.h>
#include <CoreFoundation/CFURLAccess.h>
#include <Security/SecRandom.h>
#include <CommonCrypto/CommonDigest.h>
#include <CommonCrypto/CommonDigestSPI.h>
#include <Security/SecAsn1Coder.h>
#include <Security/oidsalg.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <utilities/debugging.h>
#include <utilities/SecCFWrappers.h>
#include <Security/SecBase.h>
#include <errno.h>
#include <inttypes.h>

#if !(TARGET_IPHONE_SIMULATOR && defined(IPHONE_SIMULATOR_HOST_MIN_VERSION_REQUIRED) && IPHONE_SIMULATOR_HOST_MIN_VERSION_REQUIRED < 1090)
#include <sys/guarded.h>
#define USE_GUARDED_OPEN 1
#else
#define USE_GUARDED_OPEN 0
#endif


/* Security.framework's bundle id. */
static CFStringRef kSecFrameworkBundleID = CFSTR("com.apple.Security");

CFGiblisGetSingleton(CFBundleRef, SecFrameworkGetBundle, bundle,  ^{
    *bundle = CFRetainSafe(CFBundleGetBundleWithIdentifier(kSecFrameworkBundleID));
})

CFStringRef SecFrameworkCopyLocalizedString(CFStringRef key,
    CFStringRef tableName) {
    CFBundleRef bundle = SecFrameworkGetBundle();
    if (bundle)
        return CFBundleCopyLocalizedString(bundle, key, key, tableName);

    return CFRetainSafe(key);
}

CFURLRef SecFrameworkCopyResourceURL(CFStringRef resourceName,
	CFStringRef resourceType, CFStringRef subDirName) {
    CFURLRef url = NULL;
    CFBundleRef bundle = SecFrameworkGetBundle();
    if (bundle) {
        url = CFBundleCopyResourceURL(bundle, resourceName,
			resourceType, subDirName);
		if (!url) {
            secwarning("resource: %@.%@ in %@ not found", resourceName,
                resourceType, subDirName);
		}
    }

	return url;
}

CFDataRef SecFrameworkCopyResourceContents(CFStringRef resourceName,
	CFStringRef resourceType, CFStringRef subDirName) {
    CFURLRef url = SecFrameworkCopyResourceURL(resourceName, resourceType,
        subDirName);
	CFDataRef data = NULL;
    if (url) {
        SInt32 error;
        if (!CFURLCreateDataAndPropertiesFromResource(kCFAllocatorDefault,
            url, &data, NULL, NULL, &error)) {
            secwarning("read: %ld", (long) error);
        }
        CFRelease(url);
    }

	return data;
}

/* Return the SHA1 digest of a chunk of data as newly allocated CFDataRef. */
CFDataRef SecSHA1DigestCreate(CFAllocatorRef allocator,
	const UInt8 *data, CFIndex length) {
	CFMutableDataRef digest = CFDataCreateMutable(allocator,
		CC_SHA1_DIGEST_LENGTH);
	CFDataSetLength(digest, CC_SHA1_DIGEST_LENGTH);
	CCDigest(kCCDigestSHA1, data, length, CFDataGetMutableBytePtr(digest));
	return digest;
}

CFDataRef SecSHA256DigestCreate(CFAllocatorRef allocator,
                                const UInt8 *data, CFIndex length) {
    CFMutableDataRef digest = CFDataCreateMutable(allocator,
                                                  CC_SHA256_DIGEST_LENGTH);
    CFDataSetLength(digest, CC_SHA256_DIGEST_LENGTH);
    CCDigest(kCCDigestSHA256, data, length, CFDataGetMutableBytePtr(digest));
    return digest;
}

CFDataRef SecSHA256DigestCreateFromData(CFAllocatorRef allocator, CFDataRef data) {
    CFMutableDataRef digest = CFDataCreateMutable(allocator,
                                                  CC_SHA256_DIGEST_LENGTH);
    CFDataSetLength(digest, CC_SHA256_DIGEST_LENGTH);
    CCDigest(kCCDigestSHA256, CFDataGetBytePtr(data), CFDataGetLength(data), CFDataGetMutableBytePtr(digest));
    return digest;
}

CFDataRef SecDigestCreate(CFAllocatorRef allocator,
    const SecAsn1Oid *algorithm, const SecAsn1Item *params,
	const UInt8 *data, CFIndex length) {
    unsigned char *(*digestFcn)(const void *data, CC_LONG len, unsigned char *md);
    CFIndex digestLen;

    if (SecAsn1OidCompare(algorithm, &CSSMOID_SHA1)) {
        digestFcn = CC_SHA1;
        digestLen = CC_SHA1_DIGEST_LENGTH;
    } else if (SecAsn1OidCompare(algorithm, &CSSMOID_SHA224)) {
        digestFcn = CC_SHA224;
        digestLen = CC_SHA224_DIGEST_LENGTH;
    } else if (SecAsn1OidCompare(algorithm, &CSSMOID_SHA256)) {
        digestFcn = CC_SHA256;
        digestLen = CC_SHA256_DIGEST_LENGTH;
    } else if (SecAsn1OidCompare(algorithm, &CSSMOID_SHA384)) {
        digestFcn = CC_SHA384;
        digestLen = CC_SHA384_DIGEST_LENGTH;
    } else if (SecAsn1OidCompare(algorithm, &CSSMOID_SHA512)) {
        digestFcn = CC_SHA512;
        digestLen = CC_SHA512_DIGEST_LENGTH;
    } else {
        return NULL;
    }

	CFMutableDataRef digest = CFDataCreateMutable(allocator, digestLen);
	CFDataSetLength(digest, digestLen);
	//FIXME: Cast from CFIndex to CC_LONG
	digestFcn(data, (CC_LONG)length, CFDataGetMutableBytePtr(digest));
	return digest;
}


#include <CommonCrypto/CommonRandomSPI.h>
const SecRandomRef kSecRandomDefault = NULL;

int SecRandomCopyBytes(SecRandomRef rnd, size_t count, uint8_t *bytes) {
    if (rnd != kSecRandomDefault)
        return errSecParam;
    return CCRandomCopyBytes(kCCRandomDefault, bytes, count);
}

#if 0
#include <CommonCrypto/CommonDigest.h>
#include <stdlib.h>

/* FIPS rng declarations. */
typedef struct __SecRandom *SecRandomRef;
SecRandomRef SecRandomCreate(CFIndex randomAlg, CFIndex seedLength,
	const UInt8 *seed);
void SecRandomCopyBytes(SecRandomRef randomref, CFIndex numBytes, UInt8 *outBytes);

/* FIPS Rng implementation. */
struct __SecRandom {
	CC_SHA1_CTX sha1;
	CFIndex bytesLeft;
	UInt8 block[64];
};

SecRandomRef SecRandomCreate(CFIndex randomAlg, CFIndex seedLength,
	const UInt8 *seed) {
	SecRandomRef result = (SecRandomRef)malloc(sizeof(struct __SecRandom));
	CC_SHA1_Init(&result->sha1);
	memset(result->block + 20, 0, 44);
	result->bytesLeft = 0;

	if (seedLength) {
		/* Digest the seed and put it into output. */
		CCDigest(kCCDigestSHA1, seed, seedLength, result->block);
	} else {
		/* Seed 20 bytes from "/dev/srandom". */
		int fd = open("/dev/srandom", O_RDONLY);
		if (fd < 0)
			goto errOut;

		if (read(fd, result->block, 20) != 20)
			goto errOut;

		close(fd);
	}

	CC_SHA1_Update(&result->sha1, result->block, 64);

	return result;

errOut:
	free(result);
	return NULL;
}

void SecRandomCopyBytes(SecRandomRef randomref, CFIndex numBytes,
	UInt8 *outBytes) {
	while (numBytes > 0) {
		if (!randomref->bytesLeft) {
			CC_SHA1_Update(&randomref->sha1, randomref->block, 64);
			OSWriteBigInt32(randomref->block, 0, randomref->sha1.h0);
			OSWriteBigInt32(randomref->block, 4, randomref->sha1.h1);
			OSWriteBigInt32(randomref->block, 8, randomref->sha1.h2);
			OSWriteBigInt32(randomref->block, 12, randomref->sha1.h3);
			OSWriteBigInt32(randomref->block, 16, randomref->sha1.h4);
			randomref->bytesLeft = 20;
		}
		CFIndex outLength = (numBytes > randomref->bytesLeft ?
							 randomref->bytesLeft : numBytes);
		memcpy(outBytes, randomref->block + 20 - randomref->bytesLeft,
			outLength);
		randomref->bytesLeft -= outLength;
		outBytes += outLength;
		numBytes -= outLength;
	}
}
#endif
