#ifndef _REC_TYPE_H_INCLUDED_
#define _REC_TYPE_H_INCLUDED_

/*++
/* NAME
/*	rec_type 3h
/* SUMMARY
/*	Postfix record types
/* SYNOPSIS
/*	#include <rec_type.h>
/* DESCRIPTION
/* .nf

 /*
  * Diagnostic codes, not real record lookup results.
  */
#define REC_TYPE_EOF	-1		/* no record */
#define REC_TYPE_ERROR	-2		/* bad record */

 /*
  * A queue file or IPC mail message consists of a sequence of typed records.
  * The first record group contains time stamp, full name, sender envelope
  * information, and optionally contains recipient information. The second
  * record group contains data records with the message content. The last
  * record group is optional; it contains information extracted from message
  * headers, such as recipients, errors-to and return-receipt.
  * 
  * Note: REC_TYPE_FILT and REC_TYPE_CONT are encoded with the same 'L'
  * constant, and it  is too late to change that now.
  */
#define REC_TYPE_SIZE	'C'		/* first record, created by cleanup */
#define REC_TYPE_TIME	'T'		/* time stamp, required */
#define REC_TYPE_FULL	'F'		/* full name, optional */
#define REC_TYPE_INSP	'I'		/* inspector transport */
#define REC_TYPE_FILT	'L'		/* loop filter transport */
#define REC_TYPE_FROM	'S'		/* sender, required */
#define REC_TYPE_DONE	'D'		/* delivered recipient, optional */
#define REC_TYPE_RCPT	'R'		/* todo recipient, optional */
#define REC_TYPE_ORCP	'O'		/* original recipient, optional */
#define REC_TYPE_WARN	'W'		/* warning message time */
#define REC_TYPE_ATTR	'A'		/* named attribute for extensions */
#define REC_TYPE_KILL	'K'		/* killed record */

#define REC_TYPE_RDR	'>'		/* redirect target */
#define REC_TYPE_FLGS	'f'		/* cleanup processing flags */

#define REC_TYPE_MESG	'M'		/* start message records */

#define REC_TYPE_CONT	'L'		/* long data record */
#define REC_TYPE_NORM	'N'		/* normal data record */

#define REC_TYPE_XTRA	'X'		/* start extracted records */

#define REC_TYPE_RRTO	'r'		/* return-receipt, from headers */
#define REC_TYPE_ERTO	'e'		/* errors-to, from headers */
#define REC_TYPE_PRIO	'P'		/* priority */
#define REC_TYPE_VERP	'V'		/* VERP delimiters */

#define REC_TYPE_END	'E'		/* terminator, required */

 /*
  * What I expect to see in a "pure recipient" sequence at the end of the
  * initial or extracted envelope segments, respectively. When a queue file
  * contains pure recipient sequences only, then the queue manager will not
  * have to read all the queue file records before starting delivery. This
  * is often the case with list mail, where such optimization is desirable.
  */
#define REC_TYPE_ENV_RECIPIENT	"MDROK"
#define REC_TYPE_EXT_RECIPIENT	"EDROK"

 /*
  * The types of records that I expect to see while processing different
  * record groups. The first member in each set is the record type that
  * indicates the end of that record group.
  * 
  * XXX A records in the extracted segment are generated only by the cleanup
  * server, and are not supposed to be present in locally submitted mail, as
  * this is "postfix internal" information. However, the pickup server has to
  * allow for the presence of A records in the extracted segment, because it
  * can be requested to re-process already queued mail with `postsuper -r'.
  * 
  * Note: REC_TYPE_FILT and REC_TYPE_CONT are encoded with the same 'L'
  * constant, and it  is too late to change that now.
  */
#define REC_TYPE_ENVELOPE	"MCTFILSDROWVA>K"
#define REC_TYPE_CONTENT	"XLN"
#define REC_TYPE_EXTRACT	"EDROPreAFIL>K"

 /*
  * The subset of inputs that the postdrop command allows.
  */
#define REC_TYPE_POST_ENVELOPE	"MFSRVA"
#define REC_TYPE_POST_CONTENT	"XLN"
#define REC_TYPE_POST_EXTRACT	"ER"

 /*
  * The record at the start of the queue file specifies the message content
  * size (number of bytes between the REC_TYPE_MESG and REC_TYPE_XTRA meta
  * records), data offset (offset of the first REC_TYPE_NORM or REC_TYPE_CONT
  * text record), recipient count, and queue manager hints. These are all
  * fixed-width fields so they can be updated in place. Queue manager hints
  * are defined in qmgr_user.h
  */
#define REC_TYPE_SIZE_FORMAT	"%15ld %15ld %15ld %15ld"
#define REC_TYPE_SIZE_CAST1	long
#define REC_TYPE_SIZE_CAST2	long	/* Postfix 1.0, a.k.a. 20010228 */
#define REC_TYPE_SIZE_CAST3	long	/* Postfix 1.0, a.k.a. 20010228 */
#define REC_TYPE_SIZE_CAST4	long	/* Postfix 2.1 */

 /*
  * The warn record specifies when the next warning that the message was
  * deferred should be sent.  It is updated in place by qmgr, so changing
  * this value when there are deferred mesages in the queue is dangerous!
  */
#define REC_TYPE_WARN_FORMAT	"%15ld"	/* warning time format */
#define REC_TYPE_WARN_CAST	long

 /*
  * Programmatic interface.
  */
extern const char *rec_type_name(int);

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

#endif
