#
#  This file is part of nzbget
#
# Copyright (C) 2008-2014 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
	daemon/connect/Connection.cpp \
	daemon/connect/Connection.h \
	daemon/connect/TLS.cpp \
	daemon/connect/TLS.h \
	daemon/connect/WebDownloader.cpp \
	daemon/connect/WebDownloader.h \
	daemon/feed/FeedCoordinator.cpp \
	daemon/feed/FeedCoordinator.h \
	daemon/feed/FeedFile.cpp \
	daemon/feed/FeedFile.h \
	daemon/feed/FeedFilter.cpp \
	daemon/feed/FeedFilter.h \
	daemon/feed/FeedInfo.cpp \
	daemon/feed/FeedInfo.h \
	daemon/frontend/ColoredFrontend.cpp \
	daemon/frontend/ColoredFrontend.h \
	daemon/frontend/Frontend.cpp \
	daemon/frontend/Frontend.h \
	daemon/frontend/LoggableFrontend.cpp \
	daemon/frontend/LoggableFrontend.h \
	daemon/frontend/NCursesFrontend.cpp \
	daemon/frontend/NCursesFrontend.h \
	daemon/main/Maintenance.cpp \
	daemon/main/Maintenance.h \
	daemon/main/nzbget.cpp \
	daemon/main/nzbget.h \
	daemon/main/Options.cpp \
	daemon/main/Options.h \
	daemon/main/Scheduler.cpp \
	daemon/main/Scheduler.h \
	daemon/main/StackTrace.cpp \
	daemon/main/StackTrace.h \
	daemon/nntp/ArticleDownloader.cpp \
	daemon/nntp/ArticleDownloader.h \
	daemon/nntp/ArticleWriter.cpp \
	daemon/nntp/ArticleWriter.h \
	daemon/nntp/Decoder.cpp \
	daemon/nntp/Decoder.h \
	daemon/nntp/NewsServer.cpp \
	daemon/nntp/NewsServer.h \
	daemon/nntp/NNTPConnection.cpp \
	daemon/nntp/NNTPConnection.h \
	daemon/nntp/ServerPool.cpp \
	daemon/nntp/ServerPool.h \
	daemon/nntp/StatMeter.cpp \
	daemon/nntp/StatMeter.h \
	daemon/postprocess/ParChecker.cpp \
	daemon/postprocess/ParChecker.h \
	daemon/postprocess/ParCoordinator.cpp \
	daemon/postprocess/ParCoordinator.h \
	daemon/postprocess/ParRenamer.cpp \
	daemon/postprocess/ParRenamer.h \
	daemon/postprocess/PostScript.cpp \
	daemon/postprocess/PostScript.h \
	daemon/postprocess/PrePostProcessor.cpp \
	daemon/postprocess/PrePostProcessor.h \
	daemon/postprocess/Unpack.cpp \
	daemon/postprocess/Unpack.h \
	daemon/queue/DiskState.cpp \
	daemon/queue/DiskState.h \
	daemon/queue/DownloadInfo.cpp \
	daemon/queue/DownloadInfo.h \
	daemon/queue/DupeCoordinator.cpp \
	daemon/queue/DupeCoordinator.h \
	daemon/queue/HistoryCoordinator.cpp \
	daemon/queue/HistoryCoordinator.h \
	daemon/queue/NZBFile.cpp \
	daemon/queue/NZBFile.h \
	daemon/queue/QueueCoordinator.cpp \
	daemon/queue/QueueCoordinator.h \
	daemon/queue/QueueEditor.cpp \
	daemon/queue/QueueEditor.h \
	daemon/queue/QueueScript.cpp \
	daemon/queue/QueueScript.h \
	daemon/queue/Scanner.cpp \
	daemon/queue/Scanner.h \
	daemon/queue/UrlCoordinator.cpp \
	daemon/queue/UrlCoordinator.h \
	daemon/remote/BinRpc.cpp \
	daemon/remote/BinRpc.h \
	daemon/remote/MessageBase.h \
	daemon/remote/RemoteClient.cpp \
	daemon/remote/RemoteClient.h \
	daemon/remote/RemoteServer.cpp \
	daemon/remote/RemoteServer.h \
	daemon/remote/WebServer.cpp \
	daemon/remote/WebServer.h \
	daemon/remote/XmlRpc.cpp \
	daemon/remote/XmlRpc.h \
	daemon/util/Log.cpp \
	daemon/util/Log.h \
	daemon/util/Observer.cpp \
	daemon/util/Observer.h \
	daemon/util/Script.cpp \
	daemon/util/Script.h \
	daemon/util/Thread.cpp \
	daemon/util/Thread.h \
	daemon/util/Util.cpp \
	daemon/util/Util.h \
	svn_version.cpp

if WITH_PAR2
nzbget_SOURCES += \
	lib/par2/commandline.cpp \
	lib/par2/commandline.h \
	lib/par2/crc.cpp \
	lib/par2/crc.h \
	lib/par2/creatorpacket.cpp \
	lib/par2/creatorpacket.h \
	lib/par2/criticalpacket.cpp \
	lib/par2/criticalpacket.h \
	lib/par2/datablock.cpp \
	lib/par2/datablock.h \
	lib/par2/descriptionpacket.cpp \
	lib/par2/descriptionpacket.h \
	lib/par2/diskfile.cpp \
	lib/par2/diskfile.h \
	lib/par2/filechecksummer.cpp \
	lib/par2/filechecksummer.h \
	lib/par2/galois.cpp \
	lib/par2/galois.h \
	lib/par2/letype.h \
	lib/par2/mainpacket.cpp \
	lib/par2/mainpacket.h \
	lib/par2/md5.cpp \
	lib/par2/md5.h \
	lib/par2/par2cmdline.h \
	lib/par2/par2creatorsourcefile.cpp \
	lib/par2/par2creatorsourcefile.h \
	lib/par2/par2fileformat.cpp \
	lib/par2/par2fileformat.h \
	lib/par2/par2repairer.cpp \
	lib/par2/par2repairer.h \
	lib/par2/par2repairersourcefile.cpp \
	lib/par2/par2repairersourcefile.h \
	lib/par2/parheaders.cpp \
	lib/par2/parheaders.h \
	lib/par2/recoverypacket.cpp \
	lib/par2/recoverypacket.h \
	lib/par2/reedsolomon.cpp \
	lib/par2/reedsolomon.h \
	lib/par2/verificationhashtable.cpp \
	lib/par2/verificationhashtable.h \
	lib/par2/verificationpacket.cpp \
	lib/par2/verificationpacket.h
endif

AM_CPPFLAGS = \
	-I$(srcdir)/daemon/connect \
	-I$(srcdir)/daemon/feed \
	-I$(srcdir)/daemon/frontend \
	-I$(srcdir)/daemon/main \
	-I$(srcdir)/daemon/nntp \
	-I$(srcdir)/daemon/postprocess \
	-I$(srcdir)/daemon/queue \
	-I$(srcdir)/daemon/remote \
	-I$(srcdir)/daemon/util \
	-I$(srcdir)/lib/par2

EXTRA_DIST = \
	Makefile.cvs \
	$(windows_FILES) \
	$(osx_FILES) \
	$(linux_FILES)

windows_FILES = \
	daemon/windows/NTService.cpp \
	daemon/windows/NTService.h \
	daemon/windows/win32.h \
	daemon/windows/WinConsole.cpp \
	daemon/windows/WinConsole.h \
	nzbget.sln \
	nzbget.vcproj \
	windows/nzbget-command-shell.bat  \
	windows/install-update.bat  \
	windows/README-WINDOWS.txt  \
	windows/package-info.json  \
	windows/resources/mainicon.ico \
	windows/resources/nzbget.rc \
	windows/resources/resource.h \
	windows/resources/trayicon_idle.ico \
	windows/resources/trayicon_paused.ico \
	windows/resources/trayicon_working.ico \
	windows/setup/nzbget-setup.nsi \
	windows/setup/install.bmp \
	windows/setup/uninstall.bmp

osx_FILES = \
	osx/App_Prefix.pch \
	osx/NZBGet-Info.plist \
	osx/DaemonController.h \
	osx/DaemonController.m \
	osx/MainApp.h \
	osx/MainApp.m \
	osx/MainApp.xib \
	osx/PFMoveApplication.h \
	osx/PFMoveApplication.m \
	osx/PreferencesDialog.h \
	osx/PreferencesDialog.m \
	osx/PreferencesDialog.xib \
	osx/RPC.h \
	osx/RPC.m \
	osx/WebClient.h \
	osx/WebClient.m \
	osx/WelcomeDialog.h \
	osx/WelcomeDialog.m \
	osx/WelcomeDialog.xib \
	osx/NZBGet.xcodeproj/project.pbxproj \
	osx/Resources/Images/mainicon.icns \
	osx/Resources/Images/statusicon.png \
	osx/Resources/Images/statusicon@2x.png \
	osx/Resources/licenses/license-bootstrap.txt \
	osx/Resources/licenses/license-jquery-GPL.txt \
	osx/Resources/licenses/license-jquery-MIT.txt \
	osx/Resources/Credits.rtf \
	osx/Resources/Localizable.strings \
	osx/Resources/Welcome.rtf

linux_FILES = \
	linux/installer.sh \
	linux/install-update.sh \
	linux/package-info.json

doc_FILES = \
	lib/par2/AUTHORS \
	lib/par2/README \
	AUTHORS \
	README \
	ChangeLog \
	COPYING

exampleconf_FILES = \
	nzbget.conf

webui_FILES = \
	webui/index.html \
	webui/index.js \
	webui/downloads.js \
	webui/edit.js \
	webui/fasttable.js \
	webui/history.js \
	webui/messages.js \
	webui/status.js \
	webui/style.css \
	webui/upload.js \
	webui/util.js \
	webui/config.js \
	webui/feed.js \
	webui/lib/bootstrap.js \
	webui/lib/bootstrap.min.js \
	webui/lib/bootstrap.css  \
	webui/lib/jquery.js \
	webui/lib/jquery.min.js \
	webui/lib/raphael.js \
	webui/lib/raphael.min.js \
	webui/lib/elycharts.js \
	webui/lib/elycharts.min.js \
	webui/img/icons.png \
	webui/img/icons-2x.png \
	webui/img/transmit.gif \
	webui/img/transmit-file.gif \
	webui/img/favicon.ico \
	webui/img/download-anim-green-2x.png \
	webui/img/download-anim-orange-2x.png \
	webui/img/transmit-reload-2x.gif

scripts_FILES = \
	scripts/EMail.py \
	scripts/Logger.py

# Install
dist_doc_DATA = $(doc_FILES)
exampleconfdir = $(datadir)/nzbget
dist_exampleconf_DATA = $(exampleconf_FILES)
webuidir = $(datadir)/nzbget
nobase_dist_webui_DATA = $(webui_FILES)
scriptsdir = $(datadir)/nzbget
nobase_dist_scripts_SCRIPTS = $(scripts_FILES)

# Note about "sed": 
# We need to make some changes in installed files.
# On Linux "sed" has option "-i" for in-place-edit. Unfortunateley the BSD version of "sed"
# has incompatible syntax. To solve the problem we perform in-place-edit in three steps:
# 1) copy the original file to original.temp (delete existing original.temp, if any);
# 2) sed < original.temp > original
# 3) delete original.temp
# These steps ensure that the output file has the same permissions as the original file.

# Prepare example configuration file
install-data-hook:
	rm -f "$(DESTDIR)$(exampleconfdir)/nzbget.conf.temp"
	cp "$(DESTDIR)$(exampleconfdir)/nzbget.conf" "$(DESTDIR)$(exampleconfdir)/nzbget.conf.temp"
	sed 's:^ConfigTemplate=:ConfigTemplate=$(exampleconfdir)/nzbget.conf:' < "$(DESTDIR)$(exampleconfdir)/nzbget.conf.temp" > "$(DESTDIR)$(exampleconfdir)/nzbget.conf"
	sed 's:configuration file (typically installed:configuration file (installed:' < "$(DESTDIR)$(exampleconfdir)/nzbget.conf" > "$(DESTDIR)$(exampleconfdir)/nzbget.conf.temp"
	sed 's:/usr/local/share/nzbget/nzbget.conf):$(exampleconfdir)/nzbget.conf):' < "$(DESTDIR)$(exampleconfdir)/nzbget.conf.temp" > "$(DESTDIR)$(exampleconfdir)/nzbget.conf"
	sed 's:^WebDir=:WebDir=$(webuidir)/webui:' < "$(DESTDIR)$(exampleconfdir)/nzbget.conf" > "$(DESTDIR)$(exampleconfdir)/nzbget.conf.temp"
	sed 's:typically installed to /usr/local/share/nzbget/scripts:installed to $(scriptsdir)/scripts:' < "$(DESTDIR)$(exampleconfdir)/nzbget.conf.temp" > "$(DESTDIR)$(exampleconfdir)/nzbget.conf"
	rm "$(DESTDIR)$(exampleconfdir)/nzbget.conf.temp"

# Install configuration files into /etc
# (only if they do not exist there to prevent override by update)
install-conf:
	if test ! -f "$(DESTDIR)$(sysconfdir)/nzbget.conf" ; then \
		$(mkinstalldirs) "$(DESTDIR)$(sysconfdir)" ; \
		cp "$(DESTDIR)$(exampleconfdir)/nzbget.conf" "$(DESTDIR)$(sysconfdir)/nzbget.conf" ; \
	fi

uninstall-conf:
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
	find $(distdir)/daemon -type f -print -exec chmod -x {} \;
	find $(distdir)/webui -type f -print -exec chmod -x {} \;
	find $(distdir)/lib -type f -print -exec chmod -x {} \;

