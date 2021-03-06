/*
 * "$Id: usersys.c,v 1.9 2005/02/13 19:02:44 jlovell Exp $"
 *
 *   User, system, and password routines for the Common UNIX Printing
 *   System (CUPS).
 *
 *   Copyright 1997-2005 by Easy Software Products.
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
 *
 *   This file is subject to the Apple OS-Developed Software exception.
 *
 * Contents:
 *
 *   cupsEncryption()    - Get the default encryption settings...
 *   cupsGetPassword()   - Get a password from the user...
 *   cupsServer()        - Return the hostname of the default server...
 *   cupsSetEncryption() - Set the encryption preference.
 *   cupsSetPasswordCB() - Set the password callback for CUPS.
 *   cupsSetServer()     - Set the default server name...
 *   cupsSetUser()       - Set the default user name...
 *   cupsUser()          - Return the current users name.
 *   cups_get_password() - Get a password from the user...
 *   cups_get_line()     - Get a line from a file...
 */

/*
 * Include necessary headers...
 */

#include "cups.h"
#include "string.h"
#include "http-private.h"
#include "globals.h"
#include <stdlib.h>
#include <ctype.h>

#ifdef WIN32
#  include <windows.h>
#endif /* WIN32 */


/*
 * Local functions...
 */

static char		*cups_get_line(char *buf, int buflen, FILE *fp);


/*
 * 'cupsEncryption()' - Get the default encryption settings...
 */

http_encryption_t
cupsEncryption(void)
{
  FILE		*fp;			/* client.conf file */
  char		*encryption;		/* CUPS_ENCRYPTION variable */
  const char	*home;			/* Home directory of user */
  char		line[1024];		/* Line from file */
  cups_globals_t *cg = _cups_globals();	/* Pointer to library globals */


 /*
  * First see if we have already set the encryption stuff...
  */

  if (cg->cups_encryption == (http_encryption_t)-1)
  {
   /*
    * Then see if the CUPS_ENCRYPTION environment variable is set...
    */

    if ((encryption = getenv("CUPS_ENCRYPTION")) == NULL)
    {
     /*
      * Next check to see if we have a $HOME/.cupsrc or client.conf file...
      */

      if ((home = getenv("HOME")) != NULL)
      {
	snprintf(line, sizeof(line), "%s/.cupsrc", home);
	fp = fopen(line, "r");
      }
      else
	fp = NULL;

      if (fp == NULL)
      {
	if ((home = getenv("CUPS_SERVERROOT")) != NULL)
	{
	  snprintf(line, sizeof(line), "%s/client.conf", home);
	  fp = fopen(line, "r");
	}
	else
	  fp = fopen(CUPS_SERVERROOT "/client.conf", "r");
      }

      encryption = "IfRequested";

      if (fp != NULL)
      {
       /*
	* Read the config file and look for an Encryption line...
	*/

	while (cups_get_line(line, sizeof(line), fp) != NULL)
	  if (strncmp(line, "Encryption ", 11) == 0 ||
	      strncmp(line, "Encryption\t", 11) == 0)
	  {
	   /*
	    * Got it!  Drop any trailing newline and find the name...
	    */

	    encryption = line + strlen(line) - 1;
	    if (*encryption == '\n')
              *encryption = '\0';

	    for (encryption = line + 11; isspace(*encryption & 255); encryption ++);
	    break;
	  }

	fclose(fp);
      }
    }

   /*
    * Set the encryption preference...
    */

    if (strcasecmp(encryption, "never") == 0)
      cg->cups_encryption = HTTP_ENCRYPT_NEVER;
    else if (strcasecmp(encryption, "always") == 0)
      cg->cups_encryption = HTTP_ENCRYPT_ALWAYS;
    else if (strcasecmp(encryption, "required") == 0)
      cg->cups_encryption = HTTP_ENCRYPT_REQUIRED;
    else
      cg->cups_encryption = HTTP_ENCRYPT_IF_REQUESTED;
  }

  return (cg->cups_encryption);
}


/*
 * 'cupsGetPassword()' - Get a password from the user...
 */

const char *				/* O - Password */
cupsGetPassword(const char *prompt)	/* I - Prompt string */
{
  return ((*_cups_globals()->cups_pwdcb)(prompt));
}


/*
 * 'cupsSetEncryption()' - Set the encryption preference.
 */

void
cupsSetEncryption(http_encryption_t e)	/* I - New encryption preference */
{
  _cups_globals()->cups_encryption = e;
}


/*
 * 'cupsServer()' - Return the hostname of the default server...
 */

const char *				/* O - Server name */
cupsServer(void)
{
  FILE		*fp;			/* client.conf file */
  char		*server;		/* Pointer to server name */
  const char	*home;			/* Home directory of user */
  char		line[1024];		/* Line from file */
  cups_globals_t *cg = _cups_globals();	/* Pointer to library globals */


 /*
  * First see if we have already set the server name...
  */

  if (!cg->cups_server[0])
  {
   /*
    * Then see if the CUPS_SERVER environment variable is set...
    */

    if ((server = getenv("CUPS_SERVER")) == NULL)
    {
     /*
      * Next check to see if we have a $HOME/.cupsrc or client.conf file...
      */

      if ((home = getenv("HOME")) != NULL)
      {
	snprintf(line, sizeof(line), "%s/.cupsrc", home);
	fp = fopen(line, "r");
      }
      else
	fp = NULL;

      if (fp == NULL)
      {
	if ((home = getenv("CUPS_SERVERROOT")) != NULL)
	{
	  snprintf(line, sizeof(line), "%s/client.conf", home);
	  fp = fopen(line, "r");
	}
	else
	  fp = fopen(CUPS_SERVERROOT "/client.conf", "r");
      }

      server = "localhost";

      if (fp != NULL)
      {
       /*
	* Read the config file and look for a ServerName line...
	*/

	while (cups_get_line(line, sizeof(line), fp) != NULL)
	  if (strncmp(line, "ServerName ", 11) == 0 ||
	      strncmp(line, "ServerName\t", 11) == 0)
	  {
	   /*
	    * Got it!  Drop any trailing newline and find the name...
	    */

	    server = line + strlen(line) - 1;
	    if (*server == '\n')
              *server = '\0';

	    for (server = line + 11; isspace(*server & 255); server ++);
	    break;
	  }

	fclose(fp);
      }
    }

   /*
    * Copy the server name over...
    */

    strlcpy(cg->cups_server, server, sizeof(cg->cups_server));

#ifdef HAVE_DOMAINSOCKETS
    if (cg->cups_server[0] == '/')
    {
      strlcpy(cg->cups_server_domainsocket, cg->cups_server, sizeof(cg->cups_server));
      strlcpy(cg->cups_server, "localhost", sizeof(cg->cups_server));
    }
#endif /* HAVE_DOMAINSOCKETS */
  }

  return (cg->cups_server);
}


/*
 * 'cupsSetPasswordCB()' - Set the password callback for CUPS.
 */

void
cupsSetPasswordCB(const char *(*cb)(const char *))	/* I - Callback function */
{
  cups_globals_t *cg = _cups_globals();	/* Pointer to library globals */

  if (cb == (const char *(*)(const char *))0)
    cg->cups_pwdcb = cups_get_password;
  else
    cg->cups_pwdcb = cb;
}


/*
 * 'cupsSetServer()' - Set the default server name...
 */

void
cupsSetServer(const char *server)	/* I - Server name */
{
  cups_globals_t *cg = _cups_globals();	/* Pointer to library globals */

  if (server)
    strlcpy(cg->cups_server, server, sizeof(cg->cups_server));
  else
    cg->cups_server[0] = '\0';
}


/*
 * 'cupsSetUser()' - Set the default user name...
 */

void
cupsSetUser(const char *user)		/* I - User name */
{
  cups_globals_t *cg = _cups_globals();	/* Pointer to library globals */

  if (user)
    strlcpy(cg->cups_user, user, sizeof(cg->cups_user));
  else
    cg->cups_user[0] = '\0';
}


#if defined(WIN32)
/*
 * WIN32 username and password stuff...
 */

/*
 * 'cupsUser()' - Return the current user's name.
 */

const char *				/* O - User name */
cupsUser(void)
{
  cups_globals_t *cg = _cups_globals();	/* Pointer to library globals */

  if (!cg->cups_user[0])
  {
    DWORD	size;		/* Size of string */


    size = sizeof(cg->cups_user);
    if (!GetUserName(cg->cups_user, &size))
    {
     /*
      * Use the default username...
      */

      strcpy(cg->cups_user, "unknown");
    }
  }

  return (cg->cups_user);
}


/*
 * 'cups_get_password()' - Get a password from the user...
 */

const char *			/* O - Password */
cups_get_password(const char *prompt)	/* I - Prompt string */
{
  return (NULL);
}
#else
/*
 * UNIX username and password stuff...
 */

#  include <pwd.h>

/*
 * 'cupsUser()' - Return the current user's name.
 */

const char *				/* O - User name */
cupsUser(void)
{
  struct passwd	*pwd;			/* User/password entry */
  cups_globals_t *cg = _cups_globals();	/* Pointer to library globals */


  if (!cg->cups_user[0])
  {
   /*
    * Rewind the password file...
    */

    setpwent();

   /*
    * Lookup the password entry for the current user.
    */

    if ((pwd = getpwuid(getuid())) == NULL)
      strcpy(cg->cups_user, "unknown");	/* Unknown user! */
    else
    {
     /*
      * Copy the username...
      */

      setpwent();

      strlcpy(cg->cups_user, pwd->pw_name, sizeof(cg->cups_user));
    }

   /*
    * Rewind the password file again...
    */

    setpwent();
  }

  return (cg->cups_user);
}


/*
 * 'cups_get_password()' - Get a password from the user...
 */

const char *			/* O - Password */
cups_get_password(const char *prompt)	/* I - Prompt string */
{
  return (getpass(prompt));
}
#endif /* WIN32 */


/*
 * 'cups_get_line()' - Get a line from a file.
 */

static char *			/* O - Line from file */
cups_get_line(char *buf,	/* I - Line buffer */
              int  buflen,	/* I - Size of line buffer */
	      FILE *fp)		/* I - File to read from */
{
  char	*bufptr;		/* Pointer to end of buffer */


 /*
  * Get the line from a file...
  */

  if (fgets(buf, buflen, fp) == NULL)
    return (NULL);

 /*
  * Remove all trailing whitespace...
  */

  bufptr = buf + strlen(buf) - 1;
  if (bufptr < buf)
    return (NULL);

  while (bufptr >= buf && isspace(*bufptr & 255))
    *bufptr-- = '\0';

  return (buf);
}


/*
 * End of "$Id: usersys.c,v 1.9 2005/02/13 19:02:44 jlovell Exp $".
 */
