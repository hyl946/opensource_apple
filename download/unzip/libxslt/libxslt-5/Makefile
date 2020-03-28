##
# Makefile for libxslt
##

# Project info
Project               = libxslt
UserType              = Administrator
ToolType              = Libraries
Extra_Configure_Flags = --without-python --disable-static
Extra_Environment     = LD_TWOLEVEL_NAMESPACE=1 
Extra_LD_Flags        = 
GnuAfterInstall       =  cleanup

# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

#Install_Target = install-strip

installhdrs:: install

cleanup:
	rm -rf "$(DSTROOT)/usr/lib/libxslt.1.dylib"
	rm -rf "$(DSTROOT)/usr/lib/libxslt.dylib"
	mv "$(DSTROOT)/usr/lib/libxslt.1.1.4.dylib" "$(DSTROOT)/usr/lib/libxslt.1.dylib"
	ln -s "$(DSTROOT)/usr/lib/libxslt.1.dylib" "$(DSTROOT)/usr/lib/libxslt.dylib"
	rm -rf "$(DSTROOT)/usr/lib/libexslt.0.dylib"
	rm -rf "$(DSTROOT)/usr/lib/libexslt.dylib"
	mv "$(DSTROOT)/usr/lib/libexslt.0.8.4.dylib" "$(DSTROOT)/usr/lib/libexslt.0.dylib"
	ln -s "$(DSTROOT)/usr/lib/libexslt.0.dylib" "$(DSTROOT)/usr/lib/libexslt.dylib"

