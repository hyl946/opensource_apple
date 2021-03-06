/* -*- Mode: C; tab-width: 4 -*-
 *
 * Copyright (c) 2002-2003 Apple Computer, Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.

    Change History (most recent first):

$Log: DNSCommon.h,v $
Revision 1.57  2007/12/13 20:20:17  cheshire
Minor efficiency tweaks -- converted IdenticalResourceRecord, IdenticalSameNameRecord, and
SameRData from functions to macros, which allows the code to be inlined (the compiler can't
inline a function defined in a different compilation unit) and therefore optimized better.

Revision 1.56  2007/12/13 00:13:03  cheshire
Simplified RDataHashValue to take a single ResourceRecord pointer, instead of separate rdlength and RDataBody

Revision 1.55  2007/12/13 00:09:28  cheshire
For completeness added MX, AFSDB, RT, KX to list of RRTYPES that are considered to have a target domainname in their rdata

Revision 1.54  2007/10/05 17:56:10  cheshire
Move CountLabels and SkipLeadingLabels to DNSCommon.c so they're callable from other files

Revision 1.53  2007/09/27 17:42:49  cheshire
Fix naming: for consistency, "kDNSFlag1_RC" should be "kDNSFlag1_RC_Mask"

Revision 1.52  2007/09/26 00:49:46  cheshire
Improve packet logging to show sent and received packets,
transport protocol (UDP/TCP/TLS) and source/destination address:port

Revision 1.51  2007/09/21 21:12:36  cheshire
<rdar://problem/5498009> BTMM: Need to log updates and query packet contents

Revision 1.50  2007/09/20 01:12:06  cheshire
Moved HashSlot(X) from mDNS.c to DNSCommon.h so it's usable in other files

Revision 1.49  2007/08/30 00:31:20  cheshire
Improve "locking failure" debugging messages to show function name using __func__ macro

Revision 1.48  2007/05/25 00:25:44  cheshire
<rdar://problem/5227737> Need to enhance putRData to output all current known types

Revision 1.47  2007/05/01 21:46:31  cheshire
Move GetLLQOptData/GetPktLease from uDNS.c into DNSCommon.c so that dnsextd can use them

Revision 1.46  2007/04/22 20:18:10  cheshire
Add comment about mDNSRandom()

Revision 1.45  2007/04/22 06:02:02  cheshire
<rdar://problem/4615977> Query should immediately return failure when no server

Revision 1.44  2007/03/28 01:20:05  cheshire
<rdar://problem/4883206> Improve/create logging for secure browse

Revision 1.43  2007/03/20 17:07:15  cheshire
Rename "struct uDNS_TCPSocket_struct" to "TCPSocket", "struct uDNS_UDPSocket_struct" to "UDPSocket"

Revision 1.42  2007/03/10 03:26:44  cheshire
<rdar://problem/4961667> uDNS: LLQ refresh response packet causes cached records to be removed from cache

Revision 1.41  2007/01/18 23:18:17  cheshire
Source code tidying: Delete extraneous white space

Revision 1.40  2007/01/05 08:30:40  cheshire
Trim excessive "$Log" checkin history from before 2006
(checkin history still available via "cvs log ..." of course)

Revision 1.39  2007/01/04 21:45:20  cheshire
Added mDNS_DropLockBeforeCallback/mDNS_ReclaimLockAfterCallback macros,
to do additional lock sanity checking around callback invocations

Revision 1.38  2006/12/22 20:59:49  cheshire
<rdar://problem/4742742> Read *all* DNS keys from keychain,
 not just key for the system-wide default registration domain

Revision 1.37  2006/08/14 23:24:22  cheshire
Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0

Revision 1.36  2006/07/05 22:56:07  cheshire
<rdar://problem/4472014> Add Private DNS client functionality to mDNSResponder
Update mDNSSendDNSMessage() to use uDNS_TCPSocket type instead of "int"

Revision 1.35  2006/06/29 07:42:14  cheshire
<rdar://problem/3922989> Performance: Remove unnecessary SameDomainName() checks

Revision 1.34  2006/03/18 21:47:56  cheshire
<rdar://problem/4073825> Improve logic for delaying packets after repeated interface transitions

Revision 1.33  2006/03/10 21:51:41  cheshire
<rdar://problem/4111464> After record update, old record sometimes remains in cache
Split out SameRDataBody() into a separate routine so it can be called from other code

*/

#ifndef __DNSCOMMON_H_
#define __DNSCOMMON_H_

#include "mDNSEmbeddedAPI.h"

#ifdef	__cplusplus
	extern "C" {
#endif

// ***************************************************************************
#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark - DNS Protocol Constants
#endif

typedef enum
	{
	kDNSFlag0_QR_Mask     = 0x80,		// Query or response?
	kDNSFlag0_QR_Query    = 0x00,
	kDNSFlag0_QR_Response = 0x80,

	kDNSFlag0_OP_Mask     = 0x78,		// Operation type
	kDNSFlag0_OP_StdQuery = 0x00,
	kDNSFlag0_OP_Iquery   = 0x08,
	kDNSFlag0_OP_Status   = 0x10,
	kDNSFlag0_OP_Unused3  = 0x18,
	kDNSFlag0_OP_Notify   = 0x20,
	kDNSFlag0_OP_Update   = 0x28,

	kDNSFlag0_QROP_Mask   = kDNSFlag0_QR_Mask | kDNSFlag0_OP_Mask,

	kDNSFlag0_AA          = 0x04,		// Authoritative Answer?
	kDNSFlag0_TC          = 0x02,		// Truncated?
	kDNSFlag0_RD          = 0x01,		// Recursion Desired?
	kDNSFlag1_RA          = 0x80,		// Recursion Available?

	kDNSFlag1_Zero        = 0x40,		// Reserved; must be zero
	kDNSFlag1_AD          = 0x20,		// Authentic Data [RFC 2535]
	kDNSFlag1_CD          = 0x10,		// Checking Disabled [RFC 2535]

	kDNSFlag1_RC_Mask     = 0x0F,		// Response code
	kDNSFlag1_RC_NoErr    = 0x00,
	kDNSFlag1_RC_FmtErr   = 0x01,
	kDNSFlag1_RC_SrvErr   = 0x02,
	kDNSFlag1_RC_NXDomain = 0x03,
	kDNSFlag1_RC_NotImpl  = 0x04,
	kDNSFlag1_RC_Refused  = 0x05,
	kDNSFlag1_RC_YXDomain = 0x06,
	kDNSFlag1_RC_YXRRSet  = 0x07,
	kDNSFlag1_RC_NXRRSet  = 0x08,
	kDNSFlag1_RC_NotAuth  = 0x09,
	kDNSFlag1_RC_NotZone  = 0x0A
	} DNS_Flags;

typedef enum
	{
	TSIG_ErrBadSig  = 16,
	TSIG_ErrBadKey  = 17,
	TSIG_ErrBadTime = 18
	} TSIG_ErrorCode;

// ***************************************************************************
#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark -
#pragma mark - General Utility Functions
#endif

extern const NetworkInterfaceInfo *GetFirstActiveInterface(const NetworkInterfaceInfo *intf);
extern mDNSInterfaceID GetNextActiveInterfaceID(const NetworkInterfaceInfo *intf);

extern mDNSu32 mDNSRandom(mDNSu32 max);		// Returns pseudo-random result from zero to max inclusive
extern mDNSu32 mDNSRandomFromFixedSeed(mDNSu32 seed, mDNSu32 max);

// ***************************************************************************
#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark -
#pragma mark - Domain Name Utility Functions
#endif

#define mdnsIsDigit(X)     ((X) >= '0' && (X) <= '9')
#define mDNSIsUpperCase(X) ((X) >= 'A' && (X) <= 'Z')
#define mDNSIsLowerCase(X) ((X) >= 'a' && (X) <= 'z')
#define mdnsIsLetter(X)    (mDNSIsUpperCase(X) || mDNSIsLowerCase(X))

#define mdnsValidHostChar(X, notfirst, notlast) (mdnsIsLetter(X) || mdnsIsDigit(X) || ((notfirst) && (notlast) && (X) == '-') )

extern mDNSu16 CompressedDomainNameLength(const domainname *const name, const domainname *parent);
extern int CountLabels(const domainname *d);
extern const domainname *SkipLeadingLabels(const domainname *d, int skip);

extern mDNSu32 TruncateUTF8ToLength(mDNSu8 *string, mDNSu32 length, mDNSu32 max);
extern mDNSBool LabelContainsSuffix(const domainlabel *const name, const mDNSBool RichText);
extern mDNSu32 RemoveLabelSuffix(domainlabel *name, mDNSBool RichText);
extern void AppendLabelSuffix(domainlabel *name, mDNSu32 val, mDNSBool RichText);
#define ValidateDomainName(N) (DomainNameLength(N) <= MAX_DOMAIN_NAME)

// ***************************************************************************
#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark -
#pragma mark - Resource Record Utility Functions
#endif

// IdenticalResourceRecord returns true if two resources records have
// the same name, type, class, and identical rdata (InterfaceID and TTL may differ)

// IdenticalSameNameRecord is the same, except it skips the expensive SameDomainName() check,
// which is at its most expensive and least useful in cases where we know in advance that the names match

// Note: The dominant use of IdenticalResourceRecord is from ProcessQuery(), handling known-answer lists. In this case
// it's common to have a whole bunch or records with exactly the same name (e.g. "_http._tcp.local") but different RDATA.
// The SameDomainName() check is expensive when the names match, and in this case *all* the names match, so we
// used to waste a lot of CPU time verifying that the names match, only then to find that the RDATA is different.
// We observed mDNSResponder spending 30% of its total CPU time on this single task alone.
// By swapping the checks so that we check the RDATA first, we can quickly detect when it's different
// (99% of the time) and then bail out before we waste time on the expensive SameDomainName() check.

#define IdenticalResourceRecord(r1,r2) ( \
	(r1)->rrtype    == (r2)->rrtype      && \
	(r1)->rrclass   == (r2)->rrclass     && \
	(r1)->namehash  == (r2)->namehash    && \
	(r1)->rdlength  == (r2)->rdlength    && \
	(r1)->rdatahash == (r2)->rdatahash   && \
	SameRDataBody((r1), &(r2)->rdata->u) && \
	SameDomainName((r1)->name, (r2)->name))

#define IdenticalSameNameRecord(r1,r2) ( \
	(r1)->rrtype    == (r2)->rrtype      && \
	(r1)->rrclass   == (r2)->rrclass     && \
	(r1)->rdlength  == (r2)->rdlength    && \
	(r1)->rdatahash == (r2)->rdatahash   && \
	SameRDataBody((r1), &(r2)->rdata->u))

extern mDNSu32 RDataHashValue(const ResourceRecord *const rr);
extern mDNSBool SameRDataBody(const ResourceRecord *const r1, const RDataBody *const r2);
#define SameRData(r1,r2) ((r1)->rrtype == (r2)->rrtype && (r1)->rdlength == (r2)->rdlength && (r1)->rdatahash == (r2)->rdatahash && SameRDataBody((r1), &(r2)->rdata->u))
extern mDNSBool ResourceRecordAnswersQuestion(const ResourceRecord *const rr, const DNSQuestion *const q);
extern mDNSBool SameNameRecordAnswersQuestion(const ResourceRecord *const rr, const DNSQuestion *const q);
extern mDNSu16 GetRDLength(const ResourceRecord *const rr, mDNSBool estimate);
extern mDNSBool ValidateRData(const mDNSu16 rrtype, const mDNSu16 rdlength, const RData *const rd);

#define GetRRDomainNameTarget(RR) (                                                                          \
	((RR)->rrtype == kDNSType_NS || (RR)->rrtype == kDNSType_CNAME || (RR)->rrtype == kDNSType_PTR || (RR)->rrtype == kDNSType_DNAME) ? &(RR)->rdata->u.name        : \
	((RR)->rrtype == kDNSType_MX || (RR)->rrtype == kDNSType_AFSDB || (RR)->rrtype == kDNSType_RT  || (RR)->rrtype == kDNSType_KX   ) ? &(RR)->rdata->u.mx.exchange : \
	((RR)->rrtype == kDNSType_SRV                                  ) ? &(RR)->rdata->u.srv.target : mDNSNULL )

#define LocalRecordReady(X) ((X)->resrec.RecordType != kDNSRecordTypeUnique && (X)->resrec.RecordType != kDNSRecordTypeDeregistering)

// ***************************************************************************
#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark -
#pragma mark - DNS Message Creation Functions
#endif

extern void InitializeDNSMessage(DNSMessageHeader *h, mDNSOpaque16 id, mDNSOpaque16 flags);
extern const mDNSu8 *FindCompressionPointer(const mDNSu8 *const base, const mDNSu8 *const end, const mDNSu8 *const domname);
extern mDNSu8 *putDomainNameAsLabels(const DNSMessage *const msg, mDNSu8 *ptr, const mDNSu8 *const limit, const domainname *const name);
extern mDNSu8 *putRData(const DNSMessage *const msg, mDNSu8 *ptr, const mDNSu8 *const limit, const ResourceRecord *const rr);

// If we have a single large record to put in the packet, then we allow the packet to be up to 9K bytes,
// but in the normal case we try to keep the packets below 1500 to avoid IP fragmentation on standard Ethernet
extern mDNSu8 *PutResourceRecordTTLWithLimit(DNSMessage *const msg, mDNSu8 *ptr, mDNSu16 *count, ResourceRecord *rr, mDNSu32 ttl, const mDNSu8 *limit);
#define PutResourceRecordTTL(msg, ptr, count, rr, ttl) PutResourceRecordTTLWithLimit((msg), (ptr), (count), (rr), (ttl), \
	((msg)->h.numAnswers || (msg)->h.numAuthorities || (msg)->h.numAdditionals) ? (msg)->data + NormalMaxDNSMessageData : (msg)->data + AbsoluteMaxDNSMessageData)
#define PutResourceRecordTTLJumbo(msg, ptr, count, rr, ttl) PutResourceRecordTTLWithLimit((msg), (ptr), (count), (rr), (ttl), \
	(msg)->data + AbsoluteMaxDNSMessageData)
extern mDNSu8 *PutResourceRecordCappedTTL(DNSMessage *const msg, mDNSu8 *ptr, mDNSu16 *count, ResourceRecord *rr, mDNSu32 maxttl);
extern mDNSu8 *putEmptyResourceRecord(DNSMessage *const msg, mDNSu8 *ptr, const mDNSu8 *const limit, mDNSu16 *count, const AuthRecord *rr);

extern mDNSu8 *putQuestion(DNSMessage *const msg, mDNSu8 *ptr, const mDNSu8 *const limit, const domainname *const name, mDNSu16 rrtype, mDNSu16 rrclass);
extern mDNSu8 *putZone(DNSMessage *const msg, mDNSu8 *ptr, mDNSu8 *limit, const domainname *zone, mDNSOpaque16 zoneClass);
extern mDNSu8 *putPrereqNameNotInUse(const domainname *const name, DNSMessage *msg, mDNSu8 *ptr, mDNSu8 *end);
extern mDNSu8 *putDeletionRecord(DNSMessage *msg, mDNSu8 *ptr, ResourceRecord *rr);
extern  mDNSu8 *putDeleteRRSet(DNSMessage *msg, mDNSu8 *ptr, const domainname *name, mDNSu16 rrtype);
extern mDNSu8 *putDeleteAllRRSets(DNSMessage *msg, mDNSu8 *ptr, const domainname *name);
extern mDNSu8 *putUpdateLease(DNSMessage *msg, mDNSu8 *end, mDNSu32 lease);
#define PutResourceRecord(MSG, P, C, RR) PutResourceRecordTTL((MSG), (P), (C), (RR), (RR)->rroriginalttl)

// ***************************************************************************
#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark -
#pragma mark - DNS Message Parsing Functions
#endif

#define HashSlot(X) (DomainNameHashValue(X) % CACHE_HASH_SLOTS)
extern mDNSu32 DomainNameHashValue(const domainname *const name);
extern void SetNewRData(ResourceRecord *const rr, RData *NewRData, mDNSu16 rdlength);
extern const mDNSu8 *skipDomainName(const DNSMessage *const msg, const mDNSu8 *ptr, const mDNSu8 *const end);
extern const mDNSu8 *getDomainName(const DNSMessage *const msg, const mDNSu8 *ptr, const mDNSu8 *const end,
	domainname *const name);
extern const mDNSu8 *skipResourceRecord(const DNSMessage *msg, const mDNSu8 *ptr, const mDNSu8 *end);
extern const mDNSu8 *GetLargeResourceRecord(mDNS *const m, const DNSMessage * const msg, const mDNSu8 *ptr,
    const mDNSu8 * end, const mDNSInterfaceID InterfaceID, mDNSu8 RecordType, LargeCacheRecord *largecr);
extern const mDNSu8 *skipQuestion(const DNSMessage *msg, const mDNSu8 *ptr, const mDNSu8 *end);
extern const mDNSu8 *getQuestion(const DNSMessage *msg, const mDNSu8 *ptr, const mDNSu8 *end, const mDNSInterfaceID InterfaceID,
	DNSQuestion *question);
extern const mDNSu8 *LocateAnswers(const DNSMessage *const msg, const mDNSu8 *const end);
extern const mDNSu8 *LocateAuthorities(const DNSMessage *const msg, const mDNSu8 *const end);
extern const mDNSu8 *LocateAdditionals(const DNSMessage *const msg, const mDNSu8 *const end);
extern const mDNSu8 *LocateLLQOptData(const DNSMessage *const msg, const mDNSu8 *const end);
extern const rdataOPT *GetLLQOptData(mDNS *const m, const DNSMessage *const msg, const mDNSu8 *const end);
extern const mDNSu8 *LocateLeaseOptData(const DNSMessage *const msg, const mDNSu8 *const end);
extern mDNSu32 GetPktLease(mDNS *m, DNSMessage *msg, const mDNSu8 *end);
extern void DumpPacket(mDNS *const m, mDNSBool sent, char *type, const mDNSAddr *addr, mDNSIPPort port, const DNSMessage *const msg, const mDNSu8 *const end);

// ***************************************************************************
#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark -
#pragma mark - Packet Sending Functions
#endif

extern mStatus mDNSSendDNSMessage(mDNS *const m, DNSMessage *const msg, mDNSu8 *end,
	mDNSInterfaceID InterfaceID, const mDNSAddr *dst, mDNSIPPort dstport, TCPSocket *sock, DomainAuthInfo *authInfo);

// ***************************************************************************
#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark -
#pragma mark - RR List Management & Task Management
#endif

extern void mDNS_Lock_(mDNS *const m);
extern void mDNS_Unlock_(mDNS *const m);

#define mDNS_Lock(X) do { \
	if ((X)->mDNS_busy != (X)->mDNS_reentrancy) LogMsg("%s: mDNS_Lock locking failure! mDNS_busy (%ld) != mDNS_reentrancy (%ld)", __func__, (X)->mDNS_busy, (X)->mDNS_reentrancy); \
	mDNS_Lock_(X); } while (0)

#define mDNS_Unlock(X) do { mDNS_Unlock_(X); \
	if ((X)->mDNS_busy != (X)->mDNS_reentrancy) LogMsg("%s: mDNS_Unlock locking failure! mDNS_busy (%ld) != mDNS_reentrancy (%ld)", __func__, (X)->mDNS_busy, (X)->mDNS_reentrancy); \
	} while (0)

#define mDNS_DropLockBeforeCallback() do { m->mDNS_reentrancy++; \
	if (m->mDNS_busy != m->mDNS_reentrancy) LogMsg("%s: Locking Failure! mDNS_busy (%ld) != mDNS_reentrancy (%ld)", __func__, m->mDNS_busy, m->mDNS_reentrancy); \
	} while (0)

#define mDNS_ReclaimLockAfterCallback() do { \
	if (m->mDNS_busy != m->mDNS_reentrancy) LogMsg("%s: Unlocking Failure! mDNS_busy (%ld) != mDNS_reentrancy (%ld)", __func__, m->mDNS_busy, m->mDNS_reentrancy); \
	m->mDNS_reentrancy--; } while (0)

#ifdef	__cplusplus
	}
#endif

#endif // __DNSCOMMON_H_
