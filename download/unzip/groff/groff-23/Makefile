##
# Makefile for groff
##

# Project info
Project             = groff
UserType            = Administrator
ToolType            = Commands
Extra_Install_Flags = INSTALL_PROGRAM="$(INSTALL) -c -s"
GnuAfterInstall     = symlink remove-dir

# GNU build setup
install:: makeprefix
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make
Install_Target = install

# Satisfy bogus check during installation.
makeprefix:
	mkdir -p $(DSTROOT)/usr

# Create links for "missing" manpages.
symlink:
	$(LN) $(DSTROOT)$(MANDIR)/man1/grohtml.1 $(DSTROOT)$(MANDIR)/man1/pre-grohtml.1
	$(LN) $(DSTROOT)$(MANDIR)/man1/grohtml.1 $(DSTROOT)$(MANDIR)/man1/post-grohtml.1

# Remove the info/dir file.
remove-dir:
	rm $(DSTROOT)/usr/share/info/dir
