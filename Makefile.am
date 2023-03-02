#
#  This file is part of nzbget. See <http://nzbget.net>.
#
# Copyright (C) 2008-2019 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

bin_PROGRAMS = nzbget

nzbget_SOURCES = \
	daemon/connect/Connection.cpp \
	daemon/connect/Connection.h \
	daemon/connect/TlsSocket.cpp \
	daemon/connect/TlsSocket.h \
	daemon/connect/WebDownloader.cpp \
	daemon/connect/WebDownloader.h \
	daemon/extension/FeedScript.cpp \
	daemon/extension/FeedScript.h \
	daemon/extension/CommandScript.cpp \
	daemon/extension/CommandScript.h \
	daemon/extension/NzbScript.cpp \
	daemon/extension/NzbScript.h \
	daemon/extension/PostScript.cpp \
	daemon/extension/PostScript.h \
	daemon/extension/QueueScript.cpp \
	daemon/extension/QueueScript.h \
	daemon/extension/ScanScript.cpp \
	daemon/extension/ScanScript.h \
	daemon/extension/SchedulerScript.cpp \
	daemon/extension/SchedulerScript.h \
	daemon/extension/ScriptConfig.cpp \
	daemon/extension/ScriptConfig.h \
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
	daemon/main/CommandLineParser.cpp \
	daemon/main/CommandLineParser.h \
	daemon/main/DiskService.cpp \
	daemon/main/DiskService.h \
	daemon/main/Maintenance.cpp \
	daemon/main/Maintenance.h \
	daemon/main/nzbget.cpp \
	daemon/main/nzbget.h \
	daemon/main/Options.cpp \
	daemon/main/Options.h \
	daemon/main/WorkState.cpp \
	daemon/main/WorkState.h \
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
	daemon/nntp/NntpConnection.cpp \
	daemon/nntp/NntpConnection.h \
	daemon/nntp/ServerPool.cpp \
	daemon/nntp/ServerPool.h \
	daemon/nntp/StatMeter.cpp \
	daemon/nntp/StatMeter.h \
	daemon/postprocess/Cleanup.cpp \
	daemon/postprocess/Cleanup.h \
	daemon/postprocess/DupeMatcher.cpp \
	daemon/postprocess/DupeMatcher.h \
	daemon/postprocess/ParChecker.cpp \
	daemon/postprocess/ParChecker.h \
	daemon/postprocess/ParParser.cpp \
	daemon/postprocess/ParParser.h \
	daemon/postprocess/ParRenamer.cpp \
	daemon/postprocess/ParRenamer.h \
	daemon/postprocess/PrePostProcessor.cpp \
	daemon/postprocess/PrePostProcessor.h \
	daemon/postprocess/RarRenamer.cpp \
	daemon/postprocess/RarRenamer.h \
	daemon/postprocess/RarReader.cpp \
	daemon/postprocess/RarReader.h \
	daemon/postprocess/Rename.cpp \
	daemon/postprocess/Rename.h \
	daemon/postprocess/Repair.cpp \
	daemon/postprocess/Repair.h \
	daemon/postprocess/Unpack.cpp \
	daemon/postprocess/Unpack.h \
	daemon/postprocess/DirectUnpack.cpp \
	daemon/postprocess/DirectUnpack.h \
	daemon/queue/DirectRenamer.cpp \
	daemon/queue/DirectRenamer.h \
	daemon/queue/DiskState.cpp \
	daemon/queue/DiskState.h \
	daemon/queue/DownloadInfo.cpp \
	daemon/queue/DownloadInfo.h \
	daemon/queue/DupeCoordinator.cpp \
	daemon/queue/DupeCoordinator.h \
	daemon/queue/HistoryCoordinator.cpp \
	daemon/queue/HistoryCoordinator.h \
	daemon/queue/NzbFile.cpp \
	daemon/queue/NzbFile.h \
	daemon/queue/QueueCoordinator.cpp \
	daemon/queue/QueueCoordinator.h \
	daemon/queue/QueueEditor.cpp \
	daemon/queue/QueueEditor.h \
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
	daemon/util/NString.cpp \
	daemon/util/NString.h \
	daemon/util/Container.h \
	daemon/util/Observer.cpp \
	daemon/util/Observer.h \
	daemon/util/Script.cpp \
	daemon/util/Script.h \
	daemon/util/Thread.cpp \
	daemon/util/Thread.h \
	daemon/util/Service.cpp \
	daemon/util/Service.h \
	daemon/util/FileSystem.cpp \
	daemon/util/FileSystem.h \
	daemon/util/Util.cpp \
	daemon/util/Util.h \
	daemon/nserv/NServMain.h \
	daemon/nserv/NServMain.cpp \
	daemon/nserv/NServFrontend.h \
	daemon/nserv/NServFrontend.cpp \
	daemon/nserv/NntpServer.h \
	daemon/nserv/NntpServer.cpp \
	daemon/nserv/NzbGenerator.h \
	daemon/nserv/NzbGenerator.cpp \
	daemon/nserv/YEncoder.h \
	daemon/nserv/YEncoder.cpp

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

# Simd decoder and Crc32
nzbget_SOURCES += \
	lib/yencode/YEncode.h \
	lib/yencode/SimdInit.cpp \
	lib/yencode/SimdDecoder.cpp \
	lib/yencode/ScalarDecoder.cpp \
	lib/yencode/Sse2Decoder.cpp \
	lib/yencode/Ssse3Decoder.cpp \
	lib/yencode/PclmulCrc.cpp \
	lib/yencode/NeonDecoder.cpp \
	lib/yencode/AcleCrc.cpp \
	lib/yencode/SliceCrc.cpp

lib/yencode/Sse2Decoder.$(OBJEXT) : CXXFLAGS+=$(SSE2_CXXFLAGS)
lib/yencode/Ssse3Decoder.$(OBJEXT) : CXXFLAGS+=$(SSSE3_CXXFLAGS)
lib/yencode/PclmulCrc.$(OBJEXT) : CXXFLAGS+=$(PCLMUL_CXXFLAGS)
lib/yencode/NeonDecoder.$(OBJEXT) : CXXFLAGS+=$(NEON_CXXFLAGS)
lib/yencode/AcleCrc.$(OBJEXT) : CXXFLAGS+=$(ACLECRC_CXXFLAGS)

AM_CPPFLAGS = \
	-I$(srcdir)/daemon/connect \
	-I$(srcdir)/daemon/extension \
	-I$(srcdir)/daemon/feed \
	-I$(srcdir)/daemon/frontend \
	-I$(srcdir)/daemon/main \
	-I$(srcdir)/daemon/nntp \
	-I$(srcdir)/daemon/postprocess \
	-I$(srcdir)/daemon/queue \
	-I$(srcdir)/daemon/remote \
	-I$(srcdir)/daemon/util \
	-I$(srcdir)/daemon/nserv \
	-I$(srcdir)/lib/par2 \
	-I$(srcdir)/lib/yencode

if WITH_TESTS
nzbget_SOURCES += \
	lib/catch/catch.h \
	tests/suite/TestMain.cpp \
	tests/suite/TestMain.h \
	tests/suite/TestUtil.cpp \
	tests/suite/TestUtil.h \
	tests/main/CommandLineParserTest.cpp \
	tests/main/OptionsTest.cpp \
	tests/feed/FeedFilterTest.cpp \
	tests/postprocess/DupeMatcherTest.cpp \
	tests/postprocess/RarRenamerTest.cpp \
	tests/postprocess/RarReaderTest.cpp \
	tests/postprocess/DirectUnpackTest.cpp \
	tests/queue/NzbFileTest.cpp \
	tests/nntp/ServerPoolTest.cpp \
	tests/util/FileSystemTest.cpp \
	tests/util/NStringTest.cpp \
	tests/util/UtilTest.cpp

if WITH_PAR2
nzbget_SOURCES += \
	tests/postprocess/ParCheckerTest.cpp \
	tests/postprocess/ParRenamerTest.cpp
endif

AM_CPPFLAGS += \
	-I$(srcdir)/lib/catch \
	-I$(srcdir)/tests/suite
endif

EXTRA_DIST = \
	$(windows_FILES) \
	$(osx_FILES) \
	$(linux_FILES) \
	$(testdata_FILES) \
	$(par2doc_FILES)

windows_FILES = \
	daemon/windows/StdAfx.cpp \
	daemon/windows/WinService.cpp \
	daemon/windows/WinService.h \
	daemon/windows/WinConsole.cpp \
	daemon/windows/WinConsole.h \
	nzbget.vcxproj \
	windows/nzbget-command-shell.bat  \
	windows/install-update.bat  \
	windows/README.md  \
	windows/package-info.json  \
	windows/resources/mainicon.ico \
	windows/resources/nzbget.rc \
	windows/resources/resource.h \
	windows/resources/trayicon_idle.ico \
	windows/resources/trayicon_paused.ico \
	windows/resources/trayicon_working.ico \
	windows/resources/install.bmp \
	windows/resources/uninstall.bmp \
	windows/nzbget-setup.nsi

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
	linux/package-info.json \
	linux/README.md \
	linux/build-nzbget \
	linux/build-unpack \
	linux/build-toolchain-android \
	linux/build-toolchain-freebsd

doc_FILES = \
	README \
	ChangeLog \
	COPYING

par2doc_FILES = \
	lib/par2/AUTHORS \
	lib/par2/README

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
	webui/img/transmit-reload-2x.gif \
	webui/img/favicon-256x256-opaque.png \
	webui/img/favicon-256x256.png

scripts_FILES = \
	scripts/EMail.py \
	scripts/Logger.py

testdata_FILES = \
	tests/testdata/dupematcher1/testfile.part01.rar \
	tests/testdata/dupematcher1/testfile.part24.rar \
	tests/testdata/dupematcher2/testfile.part04.rar \
	tests/testdata/dupematcher2/testfile.part43.rar \
	tests/testdata/nzbfile/dotless.nzb \
	tests/testdata/nzbfile/dotless.txt \
	tests/testdata/nzbfile/plain.nzb \
	tests/testdata/nzbfile/plain.txt \
	tests/testdata/parchecker/crc.txt \
	tests/testdata/parchecker/testfile.dat \
	tests/testdata/parchecker/testfile.nfo \
	tests/testdata/parchecker/testfile.par2 \
	tests/testdata/parchecker/testfile.vol00+1.PAR2 \
	tests/testdata/parchecker/testfile.vol01+2.PAR2 \
	tests/testdata/parchecker/testfile.vol03+3.PAR2 \
	tests/testdata/parchecker2/crc.txt \
	tests/testdata/parchecker2/testfile.7z.001 \
	tests/testdata/parchecker2/testfile.7z.002 \
	tests/testdata/parchecker2/testfile.7z.003 \
	tests/testdata/parchecker2/testfile.7z.par2 \
	tests/testdata/parchecker2/testfile.7z.vol0+1.PAR2 \
	tests/testdata/parchecker2/testfile.7z.vol1+2.PAR2 \
	tests/testdata/parchecker2/testfile.7z.vol3+3.PAR2 \
	tests/testdata/rarrenamer/testfile3.part01.rar \
	tests/testdata/rarrenamer/testfile3.part02.rar \
	tests/testdata/rarrenamer/testfile3.part03.rar \
	tests/testdata/rarrenamer/testfile5.part01.rar \
	tests/testdata/rarrenamer/testfile5.part02.rar \
	tests/testdata/rarrenamer/testfile5.part03.rar \
	tests/testdata/rarrenamer/testfile3oldnam.rar \
	tests/testdata/rarrenamer/testfile3oldnam.r00 \
	tests/testdata/rarrenamer/testfile3oldnam.r01 \
	tests/testdata/rarrenamer/testfile3encdata.part01.rar \
	tests/testdata/rarrenamer/testfile3encdata.part02.rar \
	tests/testdata/rarrenamer/testfile3encdata.part03.rar \
	tests/testdata/rarrenamer/testfile3encnam.part01.rar \
	tests/testdata/rarrenamer/testfile3encnam.part02.rar \
	tests/testdata/rarrenamer/testfile3encnam.part03.rar \
	tests/testdata/rarrenamer/testfile5encdata.part01.rar \
	tests/testdata/rarrenamer/testfile5encdata.part02.rar \
	tests/testdata/rarrenamer/testfile5encdata.part03.rar \
	tests/testdata/rarrenamer/testfile5encnam.part01.rar \
	tests/testdata/rarrenamer/testfile5encnam.part02.rar \
	tests/testdata/rarrenamer/testfile5encnam.part03.rar

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
# On Linux "sed" has option "-i" for in-place-edit. Unfortunately the BSD version of "sed"
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

distcleancheck_listfiles = \
	find . -type f -exec sh -c 'test -f $(srcdir)/$$1 || echo $$1' \
	sh '{}' ';'

clean-bak: rm *~

# Fix premissions
dist-hook:
	find $(distdir)/daemon -type f -print -exec chmod -x {} \;
	find $(distdir)/webui -type f -print -exec chmod -x {} \;
	find $(distdir)/lib -type f -print -exec chmod -x {} \;
	find $(distdir)/tests -type f -print -exec chmod -x {} \;

include rebuild.mk
