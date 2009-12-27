bin_PROGRAMS = nzbget
nzbget_SOURCES = ArticleDownloader.cpp ArticleDownloader.h BinRpc.cpp BinRpc.h \
	ColoredFrontend.cpp ColoredFrontend.h Connection.cpp Connection.h Decoder.cpp Decoder.h \
	DiskState.cpp DiskState.h DownloadInfo.cpp DownloadInfo.h Frontend.cpp Frontend.h \
	Log.cpp Log.h LoggableFrontend.cpp LoggableFrontend.h MessageBase.h \
	NCursesFrontend.cpp NCursesFrontend.h NNTPConnection.cpp NNTPConnection.h NZBFile.cpp \
	NZBFile.h NetAddress.cpp NetAddress.h NewsServer.cpp NewsServer.h Observer.cpp \
	Observer.h Options.cpp Options.h ParChecker.cpp ParChecker.h \
	PrePostProcessor.cpp PrePostProcessor.h QueueCoordinator.cpp \
	QueueCoordinator.h QueueEditor.cpp QueueEditor.h RemoteClient.cpp RemoteClient.h \
	RemoteServer.cpp RemoteServer.h Scanner.cpp Scanner.h Scheduler.cpp Scheduler.h ScriptController.cpp \
	ScriptController.h ServerPool.cpp ServerPool.h svn_version.cpp TLS.cpp TLS.h Thread.cpp Thread.h Util.cpp \
	Util.h XmlRpc.cpp XmlRpc.h nzbget.cpp nzbget.h

EXTRA_DIST = nzbget.conf.example postprocess-example.sh postprocess-example.conf \
	win32.h NTService.cpp NTService.h \
	libpar2-0.2-bugfixes.patch libpar2-0.2-cancel.patch \
	libpar2-0.2-MSVC8.patch libsigc++-2.0.18-MSVC8.patch \
	Makefile.cvs nzbget.kdevelop nzbget.sln nzbget.vcproj \
	nzbgetd nzbget-shell.bat

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

clean-bak: rm *~
