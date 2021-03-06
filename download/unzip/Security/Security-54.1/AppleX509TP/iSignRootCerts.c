/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


/*
	File:		iSignRootCerts.c

	Contains:	embedded iSign root certs - subject name and public keys 

	Written by:	Doug Mitchell. 

	Copyright:	Copyright 1999 by Apple Computer, Inc., all rights reserved.

*/

#include <Security/cssmtype.h>
#include "rootCerts.h"

#if		TP_ROOT_CERT_ENABLE 

/* 
 * this static data is generated by extractCertFields, copy&pasted from
 * its output into this source file 
 */

/***********************
Cert File Name: serverbasic.crt
Subject Name       :
    Country        : ZA
    State          : Western Cape
    Locality       : Cape Town
    Org            : Thawte Consulting cc
    OrgUnit        : Certification Services Division
    Common Name    : Thawte Server CA
    Email addrs    : server-certs@thawte.com
 ***********************/
static const uint8 serverbasic_subject_bytes[] = {
   0x30,  0x81,  0xc4,  0x31,  0x0b,  0x30,  0x09,  0x06,  
   0x03,  0x55,  0x04,  0x06,  0x13,  0x02,  0x5a,  0x41,  
   0x31,  0x15,  0x30,  0x13,  0x06,  0x03,  0x55,  0x04,  
   0x08,  0x13,  0x0c,  0x57,  0x45,  0x53,  0x54,  0x45,  
   0x52,  0x4e,  0x20,  0x43,  0x41,  0x50,  0x45,  0x31,  
   0x12,  0x30,  0x10,  0x06,  0x03,  0x55,  0x04,  0x07,  
   0x13,  0x09,  0x43,  0x41,  0x50,  0x45,  0x20,  0x54,  
   0x4f,  0x57,  0x4e,  0x31,  0x1d,  0x30,  0x1b,  0x06,  
   0x03,  0x55,  0x04,  0x0a,  0x13,  0x14,  0x54,  0x48,  
   0x41,  0x57,  0x54,  0x45,  0x20,  0x43,  0x4f,  0x4e,  
   0x53,  0x55,  0x4c,  0x54,  0x49,  0x4e,  0x47,  0x20,  
   0x43,  0x43,  0x31,  0x28,  0x30,  0x26,  0x06,  0x03,  
   0x55,  0x04,  0x0b,  0x13,  0x1f,  0x43,  0x45,  0x52,  
   0x54,  0x49,  0x46,  0x49,  0x43,  0x41,  0x54,  0x49,  
   0x4f,  0x4e,  0x20,  0x53,  0x45,  0x52,  0x56,  0x49,  
   0x43,  0x45,  0x53,  0x20,  0x44,  0x49,  0x56,  0x49,  
   0x53,  0x49,  0x4f,  0x4e,  0x31,  0x19,  0x30,  0x17,  
   0x06,  0x03,  0x55,  0x04,  0x03,  0x13,  0x10,  0x54,  
   0x48,  0x41,  0x57,  0x54,  0x45,  0x20,  0x53,  0x45,  
   0x52,  0x56,  0x45,  0x52,  0x20,  0x43,  0x41,  0x31,  
   0x26,  0x30,  0x24,  0x06,  0x09,  0x2a,  0x86,  0x48,  
   0x86,  0xf7,  0x0d,  0x01,  0x09,  0x01,  0x16,  0x17,  
   0x73,  0x65,  0x72,  0x76,  0x65,  0x72,  0x2d,  0x63,  
   0x65,  0x72,  0x74,  0x73,  0x40,  0x74,  0x68,  0x61,  
   0x77,  0x74,  0x65,  0x2e,  0x63,  0x6f,  0x6d
};
const CSSM_DATA serverbasic_subject = { 199, (uint8 *)serverbasic_subject_bytes };
static const uint8 serverbasic_pubKey_bytes[] = {
   0x30,  0x81,  0x89,  0x02,  0x81,  0x81,  0x00,  0xd3,  
   0xa4,  0x50,  0x6e,  0xc8,  0xff,  0x56,  0x6b,  0xe6,  
   0xcf,  0x5d,  0xb6,  0xea,  0x0c,  0x68,  0x75,  0x47,  
   0xa2,  0xaa,  0xc2,  0xda,  0x84,  0x25,  0xfc,  0xa8,  
   0xf4,  0x47,  0x51,  0xda,  0x85,  0xb5,  0x20,  0x74,  
   0x94,  0x86,  0x1e,  0x0f,  0x75,  0xc9,  0xe9,  0x08,  
   0x61,  0xf5,  0x06,  0x6d,  0x30,  0x6e,  0x15,  0x19,  
   0x02,  0xe9,  0x52,  0xc0,  0x62,  0xdb,  0x4d,  0x99,  
   0x9e,  0xe2,  0x6a,  0x0c,  0x44,  0x38,  0xcd,  0xfe,  
   0xbe,  0xe3,  0x64,  0x09,  0x70,  0xc5,  0xfe,  0xb1,  
   0x6b,  0x29,  0xb6,  0x2f,  0x49,  0xc8,  0x3b,  0xd4,  
   0x27,  0x04,  0x25,  0x10,  0x97,  0x2f,  0xe7,  0x90,  
   0x6d,  0xc0,  0x28,  0x42,  0x99,  0xd7,  0x4c,  0x43,  
   0xde,  0xc3,  0xf5,  0x21,  0x6d,  0x54,  0x9f,  0x5d,  
   0xc3,  0x58,  0xe1,  0xc0,  0xe4,  0xd9,  0x5b,  0xb0,  
   0xb8,  0xdc,  0xb4,  0x7b,  0xdf,  0x36,  0x3a,  0xc2,  
   0xb5,  0x66,  0x22,  0x12,  0xd6,  0x87,  0x0d,  0x02,  
   0x03,  0x01,  0x00,  0x01
};
const CSSM_DATA serverbasic_pubKey = { 140, (uint8 *)serverbasic_pubKey_bytes };


/***********************
Cert File Name: serverpremium.crt
Subject Name       :
    Country        : ZA
    State          : Western Cape
    Locality       : Cape Town
    Org            : Thawte Consulting cc
    OrgUnit        : Certification Services Division
    Common Name    : Thawte Premium Server CA
    Email addrs    : premium-server@thawte.com
 ***********************/
static const uint8 serverpremium_subject_bytes[] = {
   0x30,  0x81,  0xce,  0x31,  0x0b,  0x30,  0x09,  0x06,  
   0x03,  0x55,  0x04,  0x06,  0x13,  0x02,  0x5a,  0x41,  
   0x31,  0x15,  0x30,  0x13,  0x06,  0x03,  0x55,  0x04,  
   0x08,  0x13,  0x0c,  0x57,  0x45,  0x53,  0x54,  0x45,  
   0x52,  0x4e,  0x20,  0x43,  0x41,  0x50,  0x45,  0x31,  
   0x12,  0x30,  0x10,  0x06,  0x03,  0x55,  0x04,  0x07,  
   0x13,  0x09,  0x43,  0x41,  0x50,  0x45,  0x20,  0x54,  
   0x4f,  0x57,  0x4e,  0x31,  0x1d,  0x30,  0x1b,  0x06,  
   0x03,  0x55,  0x04,  0x0a,  0x13,  0x14,  0x54,  0x48,  
   0x41,  0x57,  0x54,  0x45,  0x20,  0x43,  0x4f,  0x4e,  
   0x53,  0x55,  0x4c,  0x54,  0x49,  0x4e,  0x47,  0x20,  
   0x43,  0x43,  0x31,  0x28,  0x30,  0x26,  0x06,  0x03,  
   0x55,  0x04,  0x0b,  0x13,  0x1f,  0x43,  0x45,  0x52,  
   0x54,  0x49,  0x46,  0x49,  0x43,  0x41,  0x54,  0x49,  
   0x4f,  0x4e,  0x20,  0x53,  0x45,  0x52,  0x56,  0x49,  
   0x43,  0x45,  0x53,  0x20,  0x44,  0x49,  0x56,  0x49,  
   0x53,  0x49,  0x4f,  0x4e,  0x31,  0x21,  0x30,  0x1f,  
   0x06,  0x03,  0x55,  0x04,  0x03,  0x13,  0x18,  0x54,  
   0x48,  0x41,  0x57,  0x54,  0x45,  0x20,  0x50,  0x52,  
   0x45,  0x4d,  0x49,  0x55,  0x4d,  0x20,  0x53,  0x45,  
   0x52,  0x56,  0x45,  0x52,  0x20,  0x43,  0x41,  0x31,  
   0x28,  0x30,  0x26,  0x06,  0x09,  0x2a,  0x86,  0x48,  
   0x86,  0xf7,  0x0d,  0x01,  0x09,  0x01,  0x16,  0x19,  
   0x70,  0x72,  0x65,  0x6d,  0x69,  0x75,  0x6d,  0x2d,  
   0x73,  0x65,  0x72,  0x76,  0x65,  0x72,  0x40,  0x74,  
   0x68,  0x61,  0x77,  0x74,  0x65,  0x2e,  0x63,  0x6f,  
   0x6d
};
const CSSM_DATA serverpremium_subject = { 209, (uint8 *)serverpremium_subject_bytes };
static const uint8 serverpremium_pubKey_bytes[] = {
   0x30,  0x81,  0x89,  0x02,  0x81,  0x81,  0x00,  0xd2,  
   0x36,  0x36,  0x6a,  0x8b,  0xd7,  0xc2,  0x5b,  0x9e,  
   0xda,  0x81,  0x41,  0x62,  0x8f,  0x38,  0xee,  0x49,  
   0x04,  0x55,  0xd6,  0xd0,  0xef,  0x1c,  0x1b,  0x95,  
   0x16,  0x47,  0xef,  0x18,  0x48,  0x35,  0x3a,  0x52,  
   0xf4,  0x2b,  0x6a,  0x06,  0x8f,  0x3b,  0x2f,  0xea,  
   0x56,  0xe3,  0xaf,  0x86,  0x8d,  0x9e,  0x17,  0xf7,  
   0x9e,  0xb4,  0x65,  0x75,  0x02,  0x4d,  0xef,  0xcb,  
   0x09,  0xa2,  0x21,  0x51,  0xd8,  0x9b,  0xd0,  0x67,  
   0xd0,  0xba,  0x0d,  0x92,  0x06,  0x14,  0x73,  0xd4,  
   0x93,  0xcb,  0x97,  0x2a,  0x00,  0x9c,  0x5c,  0x4e,  
   0x0c,  0xbc,  0xfa,  0x15,  0x52,  0xfc,  0xf2,  0x44,  
   0x6e,  0xda,  0x11,  0x4a,  0x6e,  0x08,  0x9f,  0x2f,  
   0x2d,  0xe3,  0xf9,  0xaa,  0x3a,  0x86,  0x73,  0xb6,  
   0x46,  0x53,  0x58,  0xc8,  0x89,  0x05,  0xbd,  0x83,  
   0x11,  0xb8,  0x73,  0x3f,  0xaa,  0x07,  0x8d,  0xf4,  
   0x42,  0x4d,  0xe7,  0x40,  0x9d,  0x1c,  0x37,  0x02,  
   0x03,  0x01,  0x00,  0x01
};
const CSSM_DATA serverpremium_pubKey = { 140, (uint8 *)serverpremium_pubKey_bytes };


/***********************
Cert File Name: PCA3ss_v4.cer
Subject Name       :
    Country        : US
    Org            : VeriSign, Inc.
    OrgUnit        : Class 3 Public Primary Certification Authority
 ***********************/
static const uint8 PCA3ss_v4_subject_bytes[] = {
   0x30,  0x5f,  0x31,  0x0b,  0x30,  0x09,  0x06,  0x03,  
   0x55,  0x04,  0x06,  0x13,  0x02,  0x55,  0x53,  0x31,  
   0x17,  0x30,  0x15,  0x06,  0x03,  0x55,  0x04,  0x0a,  
   0x13,  0x0e,  0x56,  0x45,  0x52,  0x49,  0x53,  0x49,  
   0x47,  0x4e,  0x2c,  0x20,  0x49,  0x4e,  0x43,  0x2e,  
   0x31,  0x37,  0x30,  0x35,  0x06,  0x03,  0x55,  0x04,  
   0x0b,  0x13,  0x2e,  0x43,  0x4c,  0x41,  0x53,  0x53,  
   0x20,  0x33,  0x20,  0x50,  0x55,  0x42,  0x4c,  0x49,  
   0x43,  0x20,  0x50,  0x52,  0x49,  0x4d,  0x41,  0x52,  
   0x59,  0x20,  0x43,  0x45,  0x52,  0x54,  0x49,  0x46,  
   0x49,  0x43,  0x41,  0x54,  0x49,  0x4f,  0x4e,  0x20,  
   0x41,  0x55,  0x54,  0x48,  0x4f,  0x52,  0x49,  0x54,  
   0x59
};
const CSSM_DATA PCA3ss_v4_subject = { 97, (uint8 *)PCA3ss_v4_subject_bytes };
static const uint8 PCA3ss_v4_pubKey_bytes[] = {
   0x30,  0x81,  0x89,  0x02,  0x81,  0x81,  0x00,  0xc9,  
   0x5c,  0x59,  0x9e,  0xf2,  0x1b,  0x8a,  0x01,  0x14,  
   0xb4,  0x10,  0xdf,  0x04,  0x40,  0xdb,  0xe3,  0x57,  
   0xaf,  0x6a,  0x45,  0x40,  0x8f,  0x84,  0x0c,  0x0b,  
   0xd1,  0x33,  0xd9,  0xd9,  0x11,  0xcf,  0xee,  0x02,  
   0x58,  0x1f,  0x25,  0xf7,  0x2a,  0xa8,  0x44,  0x05,  
   0xaa,  0xec,  0x03,  0x1f,  0x78,  0x7f,  0x9e,  0x93,  
   0xb9,  0x9a,  0x00,  0xaa,  0x23,  0x7d,  0xd6,  0xac,  
   0x85,  0xa2,  0x63,  0x45,  0xc7,  0x72,  0x27,  0xcc,  
   0xf4,  0x4c,  0xc6,  0x75,  0x71,  0xd2,  0x39,  0xef,  
   0x4f,  0x42,  0xf0,  0x75,  0xdf,  0x0a,  0x90,  0xc6,  
   0x8e,  0x20,  0x6f,  0x98,  0x0f,  0xf8,  0xac,  0x23,  
   0x5f,  0x70,  0x29,  0x36,  0xa4,  0xc9,  0x86,  0xe7,  
   0xb1,  0x9a,  0x20,  0xcb,  0x53,  0xa5,  0x85,  0xe7,  
   0x3d,  0xbe,  0x7d,  0x9a,  0xfe,  0x24,  0x45,  0x33,  
   0xdc,  0x76,  0x15,  0xed,  0x0f,  0xa2,  0x71,  0x64,  
   0x4c,  0x65,  0x2e,  0x81,  0x68,  0x45,  0xa7,  0x02,  
   0x03,  0x01,  0x00,  0x01
};
const CSSM_DATA PCA3ss_v4_pubKey = { 140, (uint8 *)PCA3ss_v4_pubKey_bytes };


/* end of static data generated by extractCertFields */

const tpRootCert iSignRootCerts[] = {
    { &serverbasic_subject, &serverbasic_pubKey, 1024 },
    { &serverpremium_subject, &serverpremium_pubKey, 1024 },
    { &PCA3ss_v4_subject, &PCA3ss_v4_pubKey, 1024 }
};

unsigned const numiSignRootCerts = sizeof(iSignRootCerts) / sizeof(tpRootCert);

#endif	/* TP_ROOT_CERT_ENABLE */
