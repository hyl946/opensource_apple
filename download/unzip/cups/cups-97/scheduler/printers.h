/*
 * "$Id: printers.h,v 1.12 2005/02/16 17:58:02 jlovell Exp $"
 *
 *   Printer definitions for the Common UNIX Printing System (CUPS) scheduler.
 *
 *   Copyright 1997-2005 by Easy Software Products, all rights reserved.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Easy Software Products and are protected by Federal
 *   copyright law.  Distribution and use rights are outlined in the file
 *   "LICENSE.txt" which should have been included with this file.  If this
 *   file is missing or damaged please contact Easy Software Products
 *   at:
 *
 *       Attn: CUPS Licensing Information
 *       Easy Software Products
 *       44141 Airport View Drive, Suite 204
 *       Hollywood, Maryland 20636 USA
 *
 *       Voice: (301) 373-9600
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 */

#ifdef HAVE_DNSSD
#  include <dns_sd.h>
#endif /* HAVE_DNSSD */

/*
 * Quota data...
 */

typedef struct
{
  char		username[33];	/* User data */
  time_t	next_update;	/* Next update time */
  int		page_count,	/* Count of pages */
		k_count;	/* Count of kilobytes */
} quota_t;


/*
 * Printer/class information structure...
 */

typedef struct printer_str
{
  struct printer_str *next;		/* Next printer in list */
  char		*uri,			/* Printer URI */
		*hostname,		/* Host printer resides on */
		*name,			/* Printer name */
		*location,		/* Location code */
		*make_model,		/* Make and model */
		*info;			/* Description */
  int		accepting;		/* Accepting jobs? */
  int		shared;			/* Printer is shared? */
  ipp_pstate_t	state;			/* Printer state */
  char		state_message[1024];	/* Printer state message */
  int		num_reasons;		/* Number of printer-state-reasons */
  char		*reasons[16];		/* printer-state-reasons strings */
  time_t	state_time;		/* Time at this state */
  char		*job_sheets[2];		/* Banners/job sheets */
  cups_ptype_t	type;			/* Printer type (color, small, etc.) */
  time_t	browse_time;		/* Last time update was sent/received */
  char		*device_uri;		/* Device URI */
  int		raw;			/* Raw queue? */
  mime_type_t	*filetype;		/* Pseudo-filetype for printer */
  void		*job;			/* Current job in queue */
  ipp_t		*attrs;			/* Attributes supported by this printer */
  int		num_printers,		/* Number of printers in class */
		last_printer;		/* Last printer job was sent to */
  struct printer_str **printers;	/* Printers in class */
  int		quota_period,		/* Period for quotas */
		page_limit,		/* Maximum number of pages */
		k_limit,		/* Maximum number of kilobytes */
		num_quotas;		/* Number of quota records */
  quota_t	*quotas;		/* Quota records */
  int		deny_users,		/* 1 = deny, 0 = allow */
		num_users;		/* Number of allowed/denied users */
  const char	**users;		/* Allowed/denied users */
  int		num_history;		/* Number of history collections */
  ipp_t		**history;		/* History data */
  int		sequence_number;	/* Increasing sequence number */

#ifdef HAVE_DNSSD
  int		browse_protocol;	/* Which protocol was this registered under */
  char		*reg_name;		/* Name used for service registration */
  char		*service_name;		/* Final name the service is registered as */
					/* (usually the same as reg_name but may have  */
					/* digits appended to be unique) */
  char		*host_target,		/* hostname of the machine that provides the service */
		*product,		/* PPD Product string */
		*pdl;			/* pdl value for TXT record */

  char		*txt_record;		/* TXT record contents */
  int		txt_len;		/* TXT record length */

  DNSServiceRef	dnssd_ipp_ref,		/* DNSServiceRegister ref for _ipp */
		dnssd_query_ref;		/* DNSServiceQueryRecord reference for txt records */

  int		dnssd_ipp_fd,		/* File descriptors for the above references */
		dnssd_query_fd;

#endif /* HAVE_DNSSD */
} printer_t;


/*
 * Globals...
 */

VAR ipp_t		*CommonData	VALUE(NULL);
					/* Common printer object attrs */
VAR int			NumPrinters	VALUE(0);
					/* Number of printers */
VAR printer_t		*Printers	VALUE(NULL);
					/* Printer list */
VAR printer_t		*DefaultPrinter VALUE(NULL);
					/* Default printer */


/*
 * Prototypes...
 */

extern printer_t	*AddPrinter(const char *name, int update);
extern void		AddPrinterFilter(printer_t *p, const char *filter);
extern void		AddPrinterHistory(printer_t *p);
extern void		AddPrinterUser(printer_t *p, const char *username);
extern quota_t		*AddQuota(printer_t *p, const char *username);
extern void		CreateCommonData(void);
extern void		DeleteAllPrinters(void);
extern void		DeletePrinter(printer_t *p, int update);
extern void		DeletePrinterFilters(printer_t *p);
extern printer_t	*FindDest(const char *name);
extern printer_t	*FindPrinter(const char *name);
extern quota_t		*FindQuota(printer_t *p, const char *username);
extern void		FreePrinterUsers(printer_t *p);
extern void		FreeQuotas(printer_t *p);
extern void		LoadAllPrinters(void);
extern void		SaveAllPrinters(void);
extern void		SetPrinterAttrs(printer_t *p);
extern void		SetPrinterReasons(printer_t *p, const char *s);
extern void		SetPrinterState(printer_t *p, ipp_pstate_t s, int update);
extern void		SortPrinters(void);
#define			StartPrinter(p,u) SetPrinterState((p), IPP_PRINTER_IDLE, (u))
extern void		StopPrinter(printer_t *p, int update);
extern quota_t		*UpdateQuota(printer_t *p, const char *username,
			             int pages, int k);
extern const char	*ValidateDest(const char *hostname,
			              const char *resource,
			              cups_ptype_t *dtype);
extern void		WritePrintcap(void);

extern char		*cupsdSanitizeURI(const char *uri, char *buffer,
			                  int buflen);


/*
 * End of "$Id: printers.h,v 1.12 2005/02/16 17:58:02 jlovell Exp $".
 */
