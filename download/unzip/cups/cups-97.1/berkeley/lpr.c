/*
 * "$Id: lpr.c,v 1.6 2005/01/04 22:10:37 jlovell Exp $"
 *
 *   "lpr" command for the Common UNIX Printing System (CUPS).
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
 * Contents:
 *
 *   main()       - Parse options and send files for printing.
 *   sighandler() - Signal catcher for when we print from stdin...
 */

/*
 * Include necessary headers...
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <cups/string.h>
#include <cups/cups.h>


#ifndef WIN32
#  include <unistd.h>
#  include <signal.h>


/*
 * Local functions.
 */

void	sighandler(int);
#endif /* !WIN32 */


/*
 * Globals...
 */

char	tempfile[1024];		/* Temporary file for printing from stdin */


/*
 * 'main()' - Parse options and send files for printing.
 */

int
main(int  argc,		/* I - Number of command-line arguments */
     char *argv[])	/* I - Command-line arguments */
{
  int		i, j;		/* Looping var */
  int		job_id;		/* Job ID */
  char		ch;		/* Option character */
  char		*printer,	/* Destination printer or class */
		*instance;	/* Instance */
  const char	*title,		/* Job title */
		*val;		/* Environment variable name */
  int		num_copies;	/* Number of copies per file */
  int		num_files;	/* Number of files to print */
  const char	*files[1000];	/* Files to print */
  int		num_dests;	/* Number of destinations */
  cups_dest_t	*dests,		/* Destinations */
		*dest;		/* Selected destination */
  int		num_options;	/* Number of options */
  cups_option_t	*options;	/* Options */
  int		deletefile;	/* Delete file after print? */
  char		buffer[8192];	/* Copy buffer */
  int		temp;		/* Temporary file descriptor */
  off_t		size;		/* Temporary file size */
#if defined(HAVE_SIGACTION) && !defined(HAVE_SIGSET)
  struct sigaction action;	/* Signal action */
  struct sigaction oldaction;	/* Old signal action */
#endif /* HAVE_SIGACTION && !HAVE_SIGSET */


  deletefile  = 0;
  printer     = NULL;
  num_dests   = 0;
  dests       = NULL;
  num_options = 0;
  options     = NULL;
  num_files   = 0;
  title       = NULL;

  for (i = 1; i < argc; i ++)
    if (argv[i][0] == '-')
      switch (ch = argv[i][1])
      {
        case 'E' : /* Encrypt */
#ifdef HAVE_SSL
	    cupsSetEncryption(HTTP_ENCRYPT_REQUIRED);
#else
            fprintf(stderr, "%s: Sorry, no encryption support compiled in!\n",
	            argv[0]);
#endif /* HAVE_SSL */
	    break;

	case '1' : /* TROFF font set 1 */
	case '2' : /* TROFF font set 2 */
	case '3' : /* TROFF font set 3 */
	case '4' : /* TROFF font set 4 */
	case 'i' : /* indent */
	case 'w' : /* width */
	    if (argv[i][2] == '\0')
	    {
	      i ++;

	      if (i >= argc)
	      {
		fprintf(stderr, "lpr: error - expected value after -%c option!\n", ch);
		return (1);
	      }
	    }

        case 'c' : /* CIFPLOT */
	case 'd' : /* DVI */
	case 'f' : /* FORTRAN */
	case 'g' : /* plot */
	case 'n' : /* Ditroff */
	case 't' : /* Troff */
	case 'v' : /* Raster image */
	    fprintf(stderr, "lpr: warning - \'%c\' format modifier not supported - output may not be correct!\n",
	            ch);
	    break;

	case 'o' : /* Option */
	    if (argv[i][2] != '\0')
	      num_options = cupsParseOptions(argv[i] + 2, num_options, &options);
	    else
	    {
	      i ++;
	      if (i >= argc)
	      {
	        fputs("lpr: error - expected option=value after -o option!\n", stderr);
		return (1);
	      }

	      num_options = cupsParseOptions(argv[i], num_options, &options);
	    }
	    break;

	case 'l' : /* Literal/raw */
            num_options = cupsAddOption("raw", "", num_options, &options);
	    break;

	case 'p' : /* Prettyprint */
            num_options = cupsAddOption("prettyprint", "", num_options, &options);
	    break;

	case 'h' : /* Suppress burst page */
            num_options = cupsAddOption("job-sheets", "none", num_options, &options);
	    break;

	case 's' : /* Don't use symlinks */
	    break;

	case 'm' : /* Mail on completion */
	    fputs("lpr: warning - email notification is not currently supported!\n", stderr);
	    break;

	case 'q' : /* Queue file but don't print */
            num_options = cupsAddOption("job-hold-until", "indefinite",
	                                num_options, &options);
	    break;

	case 'r' : /* Remove file after printing */
	    deletefile = 1;
	    break;

        case 'P' : /* Destination printer or class */
	    if (argv[i][2] != '\0')
	      printer = argv[i] + 2;
	    else
	    {
	      i ++;
	      if (i >= argc)
	      {
	        fputs("lpr: error - expected destination after -P option!\n", stderr);
		return (1);
	      }

	      printer = argv[i];
	    }

            if ((instance = strrchr(printer, '/')) != NULL)
	      *instance++ = '\0';

	    if (num_dests == 0)
	      num_dests = cupsGetDests(&dests);

            if ((dest = cupsGetDest(printer, instance, num_dests, dests)) != NULL)
	    {
	      for (j = 0; j < dest->num_options; j ++)
	        if (cupsGetOption(dest->options[j].name, num_options, options) == NULL)
	          num_options = cupsAddOption(dest->options[j].name,
		                              dest->options[j].value,
					      num_options, &options);
	    }
	    break;

	case '#' : /* Number of copies */
	    if (argv[i][2] != '\0')
	      num_copies = atoi(argv[i] + 2);
	    else
	    {
	      i ++;
	      if (i >= argc)
	      {
	        fputs("lpr: error - expected copy count after -# option!\n", stderr);
		return (1);
	      }

	      num_copies = atoi(argv[i]);
	    }

            sprintf(buffer, "%d", num_copies);
            num_options = cupsAddOption("copies", buffer, num_options, &options);
	    break;

	case 'C' : /* Class */
	case 'J' : /* Job name */
	case 'T' : /* Title */
	    if (argv[i][2] != '\0')
	      title = argv[i] + 2;
	    else
	    {
	      i ++;
	      if (i >= argc)
	      {
		fprintf(stderr, "lpr: error - expected name after -%c option!\n", ch);
		return (1);
	      }

	      title = argv[i];
	    }
	    break;

	case 'U' : /* User */
	    if (argv[i][2] != '\0')
	      cupsSetUser(argv[i] + 2);
	    else
	    {
	      i ++;
	      if (i >= argc)
	      {
	        fputs("lpr: error - expected username after -U option!\n", stderr);
		return (1);
	      }

	      cupsSetUser(argv[i]);
	    }
	    break;

	default :
	    fprintf(stderr, "lpr: error - unknown option \'%c\'!\n", argv[i][1]);
	    return (1);
      }
    else if (num_files < 1000)
    {
     /*
      * Print a file...
      */

      if (access(argv[i], R_OK) != 0)
      {
        fprintf(stderr, "lpr: error - unable to access \"%s\" - %s\n", argv[i],
	        strerror(errno));
        return (1);
      }

      files[num_files] = argv[i];
      num_files ++;

      if (title == NULL)
      {
        if ((title = strrchr(argv[i], '/')) != NULL)
	  title ++;
	else
          title = argv[i];
      }
    }
    else
      fprintf(stderr, "lpr: error - too many files - \"%s\"\n", argv[i]);
 /*
  * See if we have any files to print; if not, print from stdin...
  */

  if (printer == NULL)
  {
    if (num_dests == 0)
      num_dests = cupsGetDests(&dests);

    if ((dest = cupsGetDest(NULL, NULL, num_dests, dests)) != NULL)
    {
      printer = dest->name;

      for (j = 0; j < dest->num_options; j ++)
	if (cupsGetOption(dest->options[j].name, num_options, options) == NULL)
	  num_options = cupsAddOption(dest->options[j].name,
		                      dest->options[j].value,
				      num_options, &options);
    }
  }

  if (printer == NULL)
  {
    val = NULL;

    if ((printer = getenv("LPDEST")) == NULL)
    {
      if ((printer = getenv("PRINTER")) != NULL)
      {
        if (!strcmp(printer, "lp"))
          printer = NULL;
	else
	  val = "PRINTER";
      }
    }
    else
      val = "LPDEST";

    if (printer && !cupsGetDest(printer, NULL, num_dests, dests))
      fprintf(stderr, "lpr: error - %s environment variable names non-existent destination \"%s\"!\n",
              val, printer);
    else if (cupsLastError() == IPP_NOT_FOUND)
      fputs("lpr: error - no default destination available.\n", stderr);
    else
      fputs("lpr: error - scheduler not responding!\n", stderr);

    return (1);
  }

  if (num_files > 0)
  {
    job_id = cupsPrintFiles(printer, num_files, files, title, num_options, options);

    if (deletefile && job_id > 0)
    {
     /*
      * Delete print files after printing...
      */

      for (i = 0; i < num_files; i ++)
        unlink(files[i]);
    }
  }
  else
  {
    num_files = 1;

#ifndef WIN32
#  if defined(HAVE_SIGSET)
    sigset(SIGHUP, sighandler);
    if (sigset(SIGINT, sighandler) == SIG_IGN)
      sigset(SIGINT, SIG_IGN);
    sigset(SIGTERM, sighandler);
#  elif defined(HAVE_SIGACTION)
    memset(&action, 0, sizeof(action));
    action.sa_handler = sighandler;

    sigaction(SIGHUP, &action, NULL);
    sigaction(SIGINT, NULL, &oldaction);
    if (oldaction.sa_handler != SIG_IGN)
      sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);
#  else
    signal(SIGHUP, sighandler);
    if (signal(SIGINT, sighandler) == SIG_IGN)
      signal(SIGINT, SIG_IGN);
    signal(SIGTERM, sighandler);
#  endif
#endif /* !WIN32 */

    if ((temp = cupsTempFd(tempfile, sizeof(tempfile))) < 0)
    {
      fprintf(stderr, "lpr: error - unable to create temporary file \"%s\" - %s\n",
              tempfile, strerror(errno));
      return (1);
    }

    while ((i = read(0, buffer, sizeof(buffer))) > 0)
      write(temp, buffer, i);

    size = lseek(temp, 0, SEEK_CUR);
    close(temp);

    if (size == 0)
    {
      fputs("lpr: error - stdin is empty, so no job has been sent.\n", stderr);
      return (1);
    }

    if (title)
      job_id = cupsPrintFile(printer, tempfile, title, num_options, options);
    else
      job_id = cupsPrintFile(printer, tempfile, "(stdin)", num_options, options);

    unlink(tempfile);
  }

  if (job_id < 1)
  {
    fprintf(stderr, "lpr: error - unable to print file: %s\n",
	    ippErrorString(cupsLastError()));
    return (1);
  }

  return (0);
}


#ifndef WIN32
/*
 * 'sighandler()' - Signal catcher for when we print from stdin...
 */

void
sighandler(int s)	/* I - Signal number */
{
 /*
  * Remove the temporary file we're using to print from stdin...
  */

  unlink(tempfile);

 /*
  * Exit...
  */

  exit(s);
}
#endif /* !WIN32 */


/*
 * End of "$Id: lpr.c,v 1.6 2005/01/04 22:10:37 jlovell Exp $".
 */
