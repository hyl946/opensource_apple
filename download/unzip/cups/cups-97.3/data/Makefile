#
# "$Id: Makefile,v 1.6 2005/01/04 22:10:41 jlovell Exp $"
#
#   Datafile makefile for the Common UNIX Printing System (CUPS).
#
#   Copyright 1993-2005 by Easy Software Products.
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
#       Hollywood, Maryland 20636 USA
#
#       Voice: (301) 373-9600
#       EMail: cups-info@cups.org
#         WWW: http://www.cups.org
#

include ../Makedefs

#
# Data files...
#

BANNERS	=	classified \
		confidential \
		secret \
		standard \
		topsecret \
		unclassified

CHARSETS =	iso-8859-1 \
		iso-8859-2 \
		iso-8859-3 \
		iso-8859-4 \
		iso-8859-5 \
		iso-8859-6 \
		iso-8859-7 \
		iso-8859-8 \
		iso-8859-9 \
		iso-8859-10 \
		iso-8859-13 \
		iso-8859-14 \
		iso-8859-15 \
		koi8-r \
		koi8-u \
		utf-8 \
		windows-874 \
		windows-1250 \
		windows-1251 \
		windows-1252 \
		windows-1253 \
		windows-1254 \
		windows-1255 \
		windows-1256 \
		windows-1257 \
		windows-1258

DATAFILES =	HPGLprolog psglyphs testprint.ps


#
# Make everything...
#

all:


#
# Clean all config and object files...
#

clean:


#
# Install files...
#

install:
	$(INSTALL_DIR) $(DATADIR)/banners
	for file in $(BANNERS); do \
		$(INSTALL_DATA) $$file $(DATADIR)/banners; \
	done
	$(INSTALL_DIR) $(DATADIR)/charsets
	for file in $(CHARSETS); do \
		$(INSTALL_DATA) $$file $(DATADIR)/charsets; \
	done
	$(INSTALL_DIR) $(DATADIR)/data
	for file in $(DATAFILES); do \
		$(INSTALL_DATA) $$file $(DATADIR)/data; \
	done
	$(INSTALL_DIR) $(DATADIR)/profiles


#
# End of "$Id: Makefile,v 1.6 2005/01/04 22:10:41 jlovell Exp $".
#
