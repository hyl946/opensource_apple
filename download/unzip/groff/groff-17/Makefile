##
# Makefile for groff
##

# Project info
Project             = groff
UserType            = Administration
ToolType            = Commands
Extra_CC_Flags      = -no-cpp-precomp
Extra_Environment   = LIBM="" manroot="$(MANDIR)"
Extra_Install_Flags = INSTALL_PROGRAM="$(INSTALL) -c -s"	\
		      manroot="$(DSTROOT)$(MANDIR)"
GnuAfterInstall     = remove-dir

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Target = install

remove-dir :
	rm $(DSTROOT)/usr/share/info/dir
