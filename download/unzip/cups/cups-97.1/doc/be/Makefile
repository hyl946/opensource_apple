#
# "$Id: Makefile,v 1.1.1.4 2004/10/31 16:15:36 jlovell Exp $"
#
#   Documentation makefile for the Common UNIX Printing System (CUPS).
#
#   Copyright 1993-2004 by Easy Software Products.
#
#   These coded instructions, statements, and computer programs are the
#   property of Easy Software Products and are protected by Federal
#   copyright law.  Distribution and use rights are outlined in the file
#   "LICENSE.txt" which should have been included with this file.  If this
#   file is missing or damaged please contact Easy Software Products
#   at:
#
#       Attn: CUPS Licensing Information
#       Easy Software Products
#       44141 Airport View Drive, Suite 204
#       Hollywood, Maryland 20636-3142 USA
#
#       Voice: (301) 373-9600
#       EMail: cups-info@cups.org
#         WWW: http://www.cups.org
#

include ../../Makedefs

#
# Document files...
#

WEBPAGES	=	index.html documentation.html
WEBIMAGES	=	images/accept-jobs.gif \
			images/add-class.gif \
			images/add-printer.gif \
			images/cancel-job.gif \
			images/config-printer.gif \
			images/continue.gif \
			images/delete-class.gif \
			images/delete-printer.gif \
			images/hold-job.gif \
			images/manage-classes.gif \
			images/manage-jobs.gif \
			images/manage-printers.gif \
			images/modify-class.gif \
			images/modify-printer.gif \
			images/navbar.gif \
			images/print-test-page.gif \
			images/reject-jobs.gif \
			images/release-job.gif \
			images/restart-job.gif \
			images/show-active.gif \
			images/show-completed.gif \
			images/start-class.gif \
			images/start-printer.gif \
			images/stop-class.gif \
			images/stop-printer.gif


#
# Make all documents...
#

all:


#
# Remove all generated files...
#

clean:


#
# Install all documentation files...
#

install:
	$(INSTALL_DIR) $(DOCDIR)/be
	for file in $(WEBPAGES); do \
		$(INSTALL_MAN) $$file $(DOCDIR)/be; \
	done
	$(INSTALL_DIR) $(DOCDIR)/be/images
	for file in $(WEBIMAGES); do \
		$(INSTALL_MAN) $$file $(DOCDIR)/be/images; \
	done


#
# End of Makefile.
#
