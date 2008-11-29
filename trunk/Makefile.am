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
	RemoteServer.cpp RemoteServer.h Scheduler.cpp Scheduler.h ScriptController.cpp \
	ScriptController.h ServerPool.cpp ServerPool.h TLS.cpp TLS.h Thread.cpp Thread.h Util.cpp \
	Util.h XmlRpc.cpp XmlRpc.h nzbget.cpp nzbget.h

EXTRA_DIST = nzbget.conf.example postprocess-example.sh \
	win32.h NTService.cpp NTService.h \
	libpar2-0.2-bugfixes.patch libpar2-0.2-cancel.patch \
	libpar2-0.2-MSVC8.patch libsigc++-2.0.18-MSVC8.patch \
	Makefile.cvs nzbget.kdevelop nzbget.sln nzbget.vcproj

clean-bak: rm *~
