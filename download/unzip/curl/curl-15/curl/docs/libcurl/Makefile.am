#
# $Id: Makefile.am,v 1.1.1.1 2002/11/26 19:07:44 zarzycki Exp $
#

AUTOMAKE_OPTIONS = foreign no-dependencies

man_MANS = \
	curl_easy_cleanup.3 \
	curl_easy_getinfo.3 \
	curl_easy_init.3 \
	curl_easy_perform.3 \
	curl_easy_setopt.3 \
	curl_easy_duphandle.3 \
	curl_formparse.3 \
	curl_formadd.3 \
	curl_formfree.3 \
	curl_getdate.3 \
	curl_getenv.3 \
	curl_slist_append.3 \
	curl_slist_free_all.3 \
	curl_version.3 \
	curl_version_info.3 \
	curl_escape.3 \
	curl_unescape.3 \
	curl_free.3 \
	curl_strequal.3 \
	curl_strnequal.3 \
	curl_mprintf.3 \
	curl_global_init.3 \
	curl_global_cleanup.3 \
	libcurl.3 \
	curl_multi_add_handle.3 \
	curl_multi_cleanup.3 \
	curl_multi_fdset.3 \
	curl_multi_info_read.3 \
	curl_multi_init.3 \
	curl_multi_perform.3 \
	curl_multi_remove_handle.3 \
	libcurl-multi.3 \
	libcurl-errors.3

HTMLPAGES = \
	curl_easy_cleanup.html \
	curl_easy_getinfo.html \
	curl_easy_init.html \
	curl_easy_perform.html \
	curl_easy_setopt.html \
	curl_easy_duphandle.html \
	curl_formadd.html \
	curl_formparse.html \
	curl_formfree.html \
	curl_getdate.html \
	curl_getenv.html \
	curl_slist_append.html \
	curl_slist_free_all.html \
	curl_version.html \
	curl_version_info.html \
	curl_escape.html \
	curl_unescape.html \
	curl_free.html \
	curl_strequal.html \
	curl_strnequal.html \
	curl_mprintf.html \
	curl_global_init.html \
	curl_global_cleanup.html \
	libcurl.html \
	curl_multi_add_handle.html \
	curl_multi_cleanup.html \
	curl_multi_fdset.html \
	curl_multi_info_read.html \
	curl_multi_init.html \
	curl_multi_perform.html \
	curl_multi_remove_handle.html \
	libcurl-multi.html \
	libcurl-errors.html \
	index.html

PDFPAGES = \
	curl_easy_cleanup.pdf \
	curl_easy_getinfo.pdf \
	curl_easy_init.pdf \
	curl_easy_perform.pdf \
	curl_easy_setopt.pdf \
	curl_easy_duphandle.pdf \
	curl_formadd.pdf \
	curl_formparse.pdf \
	curl_formfree.pdf \
	curl_getdate.pdf \
	curl_getenv.pdf \
	curl_slist_append.pdf \
	curl_slist_free_all.pdf \
	curl_version.pdf \
	curl_version_info.pdf \
	curl_escape.pdf \
	curl_unescape.pdf \
	curl_free.pdf \
	curl_strequal.pdf \
	curl_strnequal.pdf \
	curl_mprintf.pdf \
	curl_global_init.pdf \
	curl_global_cleanup.pdf \
	libcurl.pdf \
	curl_multi_add_handle.pdf \
	curl_multi_cleanup.pdf \
	curl_multi_fdset.pdf \
	curl_multi_info_read.pdf \
	curl_multi_init.pdf \
	curl_multi_perform.pdf \
	curl_multi_remove_handle.pdf \
	libcurl-multi.pdf \
	libcurl-errors.pdf

EXTRA_DIST = $(man_MANS) $(HTMLPAGES) $(PDFPAGES)

MAN2HTML= gnroff -man $< | man2html >$@

SUFFIXES = .1 .3 .html

html: $(HTMLPAGES)

.3.html:
	$(MAN2HTML)

.1.html:
	$(MAN2HTML)

pdf:
	for file in $(man_MANS); do \
	  foo=`echo $$file | sed -e 's/\.[0-9]$$//g'`; \
	  groff -Tps -man $$file >$$foo.ps; \
	  ps2pdf $$foo.ps $$foo.pdf; \
	done
