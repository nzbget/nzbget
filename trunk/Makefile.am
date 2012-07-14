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
	Makefile.cvs nzbgetd nzbget-postprocess.sh 
	$(patches_FILES) $(windows_FILES) $(doc_FILES) $(exampleconf_FILES)

patches_FILES = \
	libpar2-0.2-bugfixes.patch libpar2-0.2-cancel.patch \
	libpar2-0.2-MSVC8.patch libsigc++-2.0.18-MSVC8.patch

windows_FILES = \
	win32.h NTService.cpp NTService.h nzbget.sln nzbget.vcproj nzbget-shell.bat

doc_FILES = \
	README ChangeLog COPYING

exampleconf_FILES = \
	nzbget.conf nzbget-postprocess.conf

# Install
sbin_SCRIPTS = nzbgetd
bin_SCRIPTS = nzbget-postprocess.sh
dist_doc_DATA = $(doc_FILES)
exampleconfdir = $(datadir)/nzbget
dist_exampleconf_DATA = $(exampleconf_FILES)

# Configure installed script
install-exec-hook:
	sed -i 's?/usr/local/bin?$(bindir)?' "$(DESTDIR)$(sbindir)/nzbgetd"

# Prepare example configuration files and copy them into /etc (only if 
# they do not exist there to prevent override by update)
install-data-hook:
	sed -i 's:"nzbget-postprocess.sh": "nzbget-postprocess.sh" (installed into $(sbindir)/nzbget-postprocess.sh):' "$(DESTDIR)$(exampleconfdir)/nzbget.conf" ; \
	sed -i 's:/usr/local/share/nzbget/webui:$(webuidir)/webui:' "$(DESTDIR)$(exampleconfdir)/nzbget.conf" ;

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

