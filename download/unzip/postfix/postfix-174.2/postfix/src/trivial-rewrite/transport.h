/*++
/* NAME
/*	transport 3h
/* SUMMARY
/*	transport mapping
/* SYNOPSIS
/*	#include "transport.h"
/* DESCRIPTION
/* .nf

 /*
  * Utility library.
  */
#include <vstring.h>

 /*
  * Global library.
  */
#include <maps.h>

 /*
  * External interface.
  */
typedef struct TRANSPORT_INFO {
    MAPS   *transport_path;
    VSTRING *wildcard_channel;
    VSTRING *wildcard_nexthop;
    int     transport_errno;
} TRANSPORT_INFO;

extern TRANSPORT_INFO *transport_pre_init(const char *, const char *);
extern void transport_post_init(TRANSPORT_INFO *);
extern int transport_lookup(TRANSPORT_INFO *, const char *, const char *, VSTRING *, VSTRING *);
extern void transport_free(TRANSPORT_INFO *);

/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Wietse Venema
/*	IBM T.J. Watson Research
/*	P.O. Box 704
/*	Yorktown Heights, NY 10598, USA
/*--*/
