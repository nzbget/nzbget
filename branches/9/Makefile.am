#
#  This file if part of nzbget
#
# Copyright (C) 2008-2012 Andrey Prygunkov <hugbug@users.sourceforge.net>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#
#

bin_PROGRAMS = nzbget

nzbget_SOURCES = \
	ArticleDownloader.cpp ArticleDownloader.h BinRpc.cpp BinRpc.h \
	ColoredFrontend.cpp ColoredFrontend.h Connection.cpp Connection.h Decoder.cpp Decoder.h \
	DiskState.cpp DiskState.h DownloadInfo.cpp DownloadInfo.h Frontend.cpp Frontend.h \
	Log.cpp Log.h LoggableFrontend.cpp LoggableFrontend.h MessageBase.h \
	NCursesFrontend.cpp NCursesFrontend.h NNTPConnection.cpp NNTPConnection.h NZBFile.cpp \
	NZBFile.h NewsServer.cpp NewsServer.h Observer.cpp \
	Observer.h Options.cpp Options.h ParChecker.cpp ParChecker.h \
	PrePostProcessor.cpp PrePostProcessor.h QueueCoordinator.cpp \
	QueueCoordinator.h QueueEditor.cpp QueueEditor.h RemoteClient.cpp RemoteClient.h \
	RemoteServer.cpp RemoteServer.h Scanner.cpp Scanner.h Scheduler.cpp Scheduler.h ScriptController.cpp \
	ScriptController.h ServerPool.cpp ServerPool.h svn_version.cpp TLS.cpp TLS.h Thread.cpp Thread.h \
	Util.cpp Util.h XmlRpc.cpp XmlRpc.h WebDownloader.cpp WebDownloader.h WebServer.cpp WebServer.h \
	UrlCoordinator.cpp UrlCoordinator.h nzbget.cpp nzbget.h

EXTRA_DIST = \
	Makefile.cvs nzbgetd nzbget-postprocess.sh \
	$(patches_FILES) $(windows_FILES)

patches_FILES = \
	libpar2-0.2-bugfixes.patch libpar2-0.2-cancel.patch \
	libpar2-0.2-MSVC8.patch libsigc++-2.0.18-MSVC8.patch

windows_FILES = \
	win32.h NTService.cpp NTService.h nzbget.sln nzbget.vcproj nzbget-shell.bat

doc_FILES = \
	README ChangeLog COPYING

exampleconf_FILES = \
	nzbget.conf nzbget-postprocess.conf

webui_FILES = \
	webui/index.html webui/index.js webui/downloads.js webui/edit.js webui/fasttable.js \
	webui/history.js webui/messages.js webui/status.js webui/style.css webui/upload.js \
	webui/util.js webui/config.js \
	webui/lib/bootstrap.js webui/lib/bootstrap.min.js webui/lib/bootstrap.css  \
	webui/lib/jquery.js webui/lib/jquery.min.js \
	webui/img/icons.png webui/img/icons-2x.png \
	webui/img/transmit.gif webui/img/transmit-file.gif webui/img/favicon.ico \
	webui/img/download-anim-green-2x.png webui/img/download-anim-orange-2x.png \
	webui/img/transmit-reload-2x.gif

# Install
sbin_SCRIPTS = nzbgetd
bin_SCRIPTS = nzbget-postprocess.sh
dist_doc_DATA = $(doc_FILES)
exampleconfdir = $(datadir)/nzbget
dist_exampleconf_DATA = $(exampleconf_FILES)
webuiconfdir = $(datadir)/nzbget/webui
dist_webuiconf_DATA = $(exampleconf_FILES)
webuidir = $(datadir)/nzbget
nobase_dist_webui_DATA = $(webui_FILES)

# Note about "sed": 
# We need to make some changes in installed files.
# On Linux "sed" has option "-i" for in-place-edit. Unfortunateley the BSD version of "sed"
# has incompatible syntax. To solve the problem we perform in-place-edit in three steps:
# 1) copy the original file to original.temp (delete existing original.temp, if any);
# 2) sed < original.temp > original
# 3) delete original.temp
# These steps ensure that the output file has the same permissions as the original file.

# Configure installed script
install-exec-hook:
	rm -f "$(DESTDIR)$(sbindir)/nzbgetd.temp"
	cp "$(DESTDIR)$(sbindir)/nzbgetd" "$(DESTDIR)$(sbindir)/nzbgetd.temp"
	sed 's?/usr/local/bin?$(bindir)?' < "$(DESTDIR)$(sbindir)/nzbgetd.temp" > "$(DESTDIR)$(sbindir)/nzbgetd"
	rm "$(DESTDIR)$(sbindir)/nzbgetd.temp"

# Prepare example configuration files
install-data-hook:
	rm -f "$(DESTDIR)$(exampleconfdir)/nzbget.conf.temp"
	cp "$(DESTDIR)$(exampleconfdir)/nzbget.conf" "$(DESTDIR)$(exampleconfdir)/nzbget.conf.temp"
	sed 's:"nzbget-postprocess.sh":"nzbget-postprocess.sh" (installed into $(bindir)):' < "$(DESTDIR)$(exampleconfdir)/nzbget.conf" > "$(DESTDIR)$(exampleconfdir)/nzbget.conf.temp"
	sed 's:^WebDir=:WebDir=$(webuidir)/webui:' < "$(DESTDIR)$(exampleconfdir)/nzbget.conf.temp" > "$(DESTDIR)$(exampleconfdir)/nzbget.conf"
	rm "$(DESTDIR)$(exampleconfdir)/nzbget.conf.temp"

# Install configuration files into /etc
# (only if they do not exist there to prevent override by update)
install-conf:
	if test ! -f "$(DESTDIR)$(sysconfdir)/nzbget.conf" ; then \
		$(mkinstalldirs) "$(DESTDIR)$(sysconfdir)" ; \
		cp "$(DESTDIR)$(exampleconfdir)/nzbget.conf" "$(DESTDIR)$(sysconfdir)/nzbget.conf" ; \
		rm -f "$(DESTDIR)$(sysconfdir)/nzbget.conf.temp" ; \
		cp "$(DESTDIR)$(sysconfdir)/nzbget.conf" "$(DESTDIR)$(sysconfdir)/nzbget.conf.temp" ; \
		sed 's:^PostProcess=:PostProcess=$(bindir)/nzbget-postprocess.sh:' < "$(DESTDIR)$(sysconfdir)/nzbget.conf.temp" > "$(DESTDIR)$(sysconfdir)/nzbget.conf" ; \
		rm "$(DESTDIR)$(sysconfdir)/nzbget.conf.temp" ; \
	fi
	if test ! -f "$(DESTDIR)$(sysconfdir)/nzbget-postprocess.conf" ; then \
		$(mkinstalldirs) "$(DESTDIR)$(sysconfdir)" ; \
		cp "$(DESTDIR)$(exampleconfdir)/nzbget-postprocess.conf" "$(DESTDIR)$(sysconfdir)/nzbget-postprocess.conf" ; \
	fi

uninstall-conf:
	rm -f "$(DESTDIR)$(sysconfdir)/nzbget-postprocess.conf"
	rm -f "$(DESTDIR)$(sysconfdir)/nzbget.conf"

# Determining subversion revision:
# 1) If directory ".svn" exists we take revision from it using program svnversion (part of subversion package)
#    File is recreated only if revision number was changed.
# 2) If directory ".svn" doesn't exists we keep and reuse file "svn_version.cpp",
#    which was possibly created early.
# 3) If neither directory ".svn" nor file "svn_version.cpp" are available
#    we create new file "svn_version.c" with empty revision number.
svn_version.cpp: FORCE
	@ if test -d ./.svn ; then \
		V="$(shell svnversion -n .)"; \
		H="$(shell test -f ./svn_version.cpp && head -n 1 svn_version.cpp)"; \
		if test "/* $$V */" != "$$H" ; then \
			( \
			echo "/* $$V */" ;\
			echo "/* This file is automatically regenerated on each build. Do not edit it. */" ;\
			echo "const char* svn_version(void)" ;\
			echo "{" ;\
			echo "	const char* SVN_Version = \"$$V\";" ;\
			echo "	return SVN_Version;" ;\
			echo "}" ;\
			) > svn_version.cpp ; \
		fi \
	elif test -f ./svn_version.cpp ; then \
		test "ok, reuse existing file"; \
	else \
		( \
		echo "/*  */" ;\
		echo "/* This file is automatically regenerated on each build. Do not edit it. */" ;\
		echo "const char* svn_version(void)" ;\
		echo "{" ;\
		echo "	const char* SVN_Version = \"\";" ;\
		echo "	return SVN_Version;" ;\
		echo "}" ;\
		) > svn_version.cpp ; \
	fi
FORCE:

# Ignore "svn_version.cpp" in distcleancheck
distcleancheck_listfiles = \
	find . -type f -exec sh -c 'test -f $(srcdir)/$$1 || echo $$1' \
	sh '{}' ';'

clean-bak: rm *~

# Fix premissions
dist-hook:
	chmod -x $(distdir)/*.cpp $(distdir)/*.h
	find $(distdir)/webui -type f -print -exec chmod -x {} \;

