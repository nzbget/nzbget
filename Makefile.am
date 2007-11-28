bin_PROGRAMS = nzbget
nzbget_SOURCES = ArticleDownloader.cpp ArticleDownloader.h ColoredFrontend.cpp \
	ColoredFrontend.h Connection.cpp Connection.h Decoder.cpp Decoder.h DiskState.cpp \
	DiskState.h DownloadInfo.cpp DownloadInfo.h Frontend.cpp Frontend.h Log.cpp Log.h \
	LoggableFrontend.cpp LoggableFrontend.h MessageBase.h RemoteServer.cpp RemoteServer.h \
	NCursesFrontend.cpp NCursesFrontend.h NNTPConnection.cpp NNTPConnection.h RemoteClient.cpp \
	RemoteClient.h NZBFile.cpp NZBFile.h NetAddress.cpp NetAddress.h NewsServer.cpp \
	NewsServer.h Observer.cpp Observer.h Options.cpp Options.h ParChecker.cpp \
	ParChecker.h PrePostProcessor.cpp PrePostProcessor.h QueueCoordinator.cpp \
	QueueCoordinator.h ServerPool.cpp ServerPool.h Thread.cpp Thread.h Util.cpp Util.h \
	nzbget.cpp nzbget.h

EXTRA_DIST = nzbget.conf.example \
	win32.h NTService.cpp NTService.h \
	libpar2-0.2-MSVC8.patch libsigc++-2.0.18-MSVC8.patch \
	nzbget.kdevelop nzbget.sln nzbget.vcproj

clean-bak: rm *~
