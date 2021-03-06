#
# Makefile for showmount
#

Project = showmount

pathsearch = $(firstword $(wildcard $(addsuffix /$(1),$(subst :, ,$(PATH)))))
TCONF := $(call pathsearch,tconf)

ifneq "$(TCONF)" ""
Embedded=$(shell tconf --test TARGET_OS_EMBEDDED)
endif

inplace:
	@$(MAKE) all SRCROOT=. OBJROOT=. SYMROOT=. DSTROOT=/

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

SDKROOT ?= /

WARNING_FLAGS = -Wall -Wextra -Wformat=2 -Wpointer-arith -Wcast-align \
		-Wwrite-strings -Wno-unknown-pragmas \
		-Wno-missing-field-initializers -Wno-shadow
Extra_CC_Flags = $(WARNING_FLAGS) -D_FORTIFY_SOURCE=2 -F$(SDKROOT)/System/Library/PrivateFrameworks -fPIE
Extra_LD_Flags = -Wl,-pie -framework oncrpc -framework CoreFoundation -dead_strip

ifeq "$(Embedded)" "YES"
Extra_CC_Flags += -isysroot $(SDKROOT)
endif

DST_DIR = $(DSTROOT)/usr/bin
MAN_DIR = $(DSTROOT)/usr/share/man/man8

#
# Standard B&I targets
#
all: $(SYMROOT)/showmount

install:: all
	install -d -o root -g wheel -m 755 $(DST_DIR)
	install -c -o root -g wheel -m 555 -s $(SYMROOT)/showmount $(DST_DIR)
	install -d -o root -g wheel -m 755 $(MAN_DIR)
	install -c -o root -g wheel -m 444 $(SRCROOT)/showmount.8 $(MAN_DIR)

clean::
	-rm -f *.o showmount

#
# Build
#
CFILES = showmount.c mshow.c
HFILES = showmount.h
OFILES = $(CFILES:.c=.o)

$(OFILES): $(HFILES)

$(OBJROOT)/%.o: $(SRCROOT)/%.c
	$(CC) -c $(CFLAGS) -o $@ $<

$(SYMROOT)/showmount: $(addprefix $(OBJROOT)/, $(OFILES))
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

