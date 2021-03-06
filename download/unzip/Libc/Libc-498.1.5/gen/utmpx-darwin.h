/*
 * Copyright (c) 2005 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 2004 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#ifdef UTMP_COMPAT
#define UTMP_COMPAT_UTMP0	0x01
#define UTMP_COMPAT_UTMP1	0x02
#define UTMP_COMPAT_WTMP	0x04
#define UTMP_COMPAT_LASTLOG	0x08
#endif /* UTMP_COMPAT */

#define LASTLOG_FACILITY	"com.apple.system.lastlog"
#define UTMPX_FACILITY		"com.apple.system.utmpx"

extern int utfile_system; /* are we using _PATH_UTMPX? */

#ifdef __LP64__
#define __need_struct_timeval32
#include <_structs.h>

/*
 * these structures assume natural alignment so they are the same size
 * and layout as their 32-bit counterpart
 */
#ifdef UTMP_COMPAT
struct lastlog32 {
	__int32_t	ll_time;
	char	ll_line[UT_LINESIZE];
	char	ll_host[UT_HOSTSIZE];
};

struct utmp32 {
	char	ut_line[UT_LINESIZE];
	char	ut_name[UT_NAMESIZE];
	char	ut_host[UT_HOSTSIZE];
	__int32_t	ut_time;
};
#endif /* UTMP_COMPAT */

struct utmpx32 {
	char ut_user[_UTX_USERSIZE];	/* login name */
	char ut_id[_UTX_IDSIZE];	/* id */
	char ut_line[_UTX_LINESIZE];	/* tty name */
	pid_t ut_pid;			/* process id creating the entry */
	short ut_type;			/* type of this entry */
	struct timeval32 ut_tv;		/* time entry was created */
	char ut_host[_UTX_HOSTSIZE];	/* host name */
	__uint32_t ut_pad[16];		/* reserved for future use */
};
#endif /* __LP64__ */

struct utmpx *_pututxline(const struct utmpx *);
#ifdef __LP64__
void _utmpx32_64(const struct utmpx32 *, struct utmpx *);
void _utmpx64_32(const struct utmpx *, struct utmpx32 *);
#endif /* __LP64__ */
void _utmpx_asl(const struct utmpx *);
const struct utmpx *_utmpx_working_copy(const struct utmpx *, struct utmpx *, int);

#ifdef UTMP_COMPAT
#ifdef __LP64__
void _getutmp32(const struct utmpx *, struct utmp32 *);
int _utmp_compat(const struct utmpx *, struct utmp32 *);
void _write_lastlog(const struct utmp32 *, const struct utmpx *);
void _write_utmp(const struct utmp32 *, int);
#else /* __LP64__ */
int _utmp_compat(const struct utmpx *, struct utmp *);
void _write_lastlog(const struct utmp *, const struct utmpx *);
void _write_utmp(const struct utmp *, int);
#endif /* __LP64__ */
void _write_utmp_compat(const struct utmpx *);
#ifdef __LP64__
void _write_wtmp(const struct utmp32 *);
#else /* __LP64__ */
void _write_wtmp(const struct utmp *);
#endif /* __LP64__ */
#endif /* UTMP_COMPAT */
