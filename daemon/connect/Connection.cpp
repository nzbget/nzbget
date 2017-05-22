/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2004 Sven Henkel <sidddy@users.sourceforge.net>
 *  Copyright (C) 2007-2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "nzbget.h"
#include "Connection.h"
#include "Log.h"

static const int CONNECTION_READBUFFER_SIZE = 1024;
#ifndef HAVE_GETADDRINFO
#ifndef HAVE_GETHOSTBYNAME_R
std::unique_ptr<Mutex> Connection::m_getHostByNameMutex;
#endif
#endif

class ConnectionFinalizer
{
public:
	~ConnectionFinalizer()
	{
		Connection::Final();
	}
};

std::unique_ptr<ConnectionFinalizer> m_connectionFinalizer;

void closesocket_gracefully(SOCKET socket)
{
	char buf[1024];
	struct linger linger;

	// Set linger option to avoid socket hanging out after close. This prevent
	// ephemeral port exhaust problem under high QPS.
	linger.l_onoff = 1;
	linger.l_linger = 1;
	setsockopt(socket, SOL_SOCKET, SO_LINGER, (char *) &linger, sizeof(linger));

	// Send FIN to the client
	shutdown(socket, SHUT_WR);

	// Set non-blocking mode
#ifdef WIN32
	u_long on = 1;
	ioctlsocket(socket, FIONBIO, &on);
#else
	int flags;
	flags = fcntl(socket, F_GETFL, 0);
	fcntl(socket, F_SETFL, flags | O_NONBLOCK);
#endif

	// Read and discard pending incoming data. If we do not do that and close the
	// socket, the data in the send buffer may be discarded. This
	// behaviour is seen on Windows, when client keeps sending data
	// when server decides to close the connection; then when client
	// does recv() it gets no data back.
	int n;
	do {
		n = recv(socket, buf, sizeof(buf), 0);
	} while (n > 0);

	// Now we know that our FIN is ACK-ed, safe to close
	closesocket(socket);
}

void Connection::Init()
{
	debug("Initializing global connection data");

#ifdef WIN32
	WSADATA wsaData;
	int err = WSAStartup(MAKEWORD(2, 0), &wsaData);
	if (err != 0)
	{
		error("Could not initialize socket library");
		return;
	}
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE( wsaData.wVersion ) != 0)
	{
		error("Could not initialize socket library");
		WSACleanup();
		return;
	}
#endif

#ifndef HAVE_GETADDRINFO
#ifndef HAVE_GETHOSTBYNAME_R
	m_getHostByNameMutex = std::make_unique<Mutex>();
#endif
#endif

	m_connectionFinalizer = std::make_unique<ConnectionFinalizer>();
}

void Connection::Final()
{
#ifdef WIN32
	WSACleanup();
#endif
}

Connection::Connection(const char* host, int port, bool tls) :
	m_host(host), m_port(port), m_tls(tls)
{
	debug("Creating Connection");

	m_readBuf.Reserve(CONNECTION_READBUFFER_SIZE + 1);
}

Connection::Connection(SOCKET socket, bool tls)
{
	debug("Creating Connection");

	m_port = 0;
	m_tls = tls;
	m_status = csConnected;
	m_socket = socket;
	m_bufAvail = 0;
	m_timeout = 60;
	m_suppressErrors = true;
	m_readBuf.Reserve(CONNECTION_READBUFFER_SIZE + 1);
#ifndef DISABLE_TLS
	m_tlsSocket = nullptr;
	m_tlsError = false;
#endif
}

Connection::~Connection()
{
	debug("Destroying Connection");

	Disconnect();
}

void Connection::SetSuppressErrors(bool suppressErrors)
{
	m_suppressErrors = suppressErrors;
#ifndef DISABLE_TLS
	if (m_tlsSocket)
	{
		m_tlsSocket->SetSuppressErrors(suppressErrors);
	}
#endif
}

bool Connection::Connect()
{
	debug("Connecting");

	if (m_status == csConnected)
	{
		return true;
	}

	bool res = DoConnect();

	if (res)
	{
		m_status = csConnected;
	}
	else
	{
		DoDisconnect();
	}

	return res;
}

bool Connection::Disconnect()
{
	debug("Disconnecting");

	if (m_status == csDisconnected)
	{
		return true;
	}

	bool res = DoDisconnect();

	m_status = csDisconnected;
	m_socket = INVALID_SOCKET;
	m_bufAvail = 0;

	return res;
}

bool Connection::Bind()
{
	debug("Binding");

	if (m_status == csListening)
	{
		return true;
	}

#ifdef HAVE_GETADDRINFO
	struct addrinfo addr_hints, *addr_list, *addr;

	memset(&addr_hints, 0, sizeof(addr_hints));
	addr_hints.ai_family = m_ipVersion == ipV4 ? AF_INET : m_ipVersion == ipV6 ? AF_INET6 : AF_UNSPEC;
	addr_hints.ai_socktype = SOCK_STREAM,
	addr_hints.ai_flags = AI_PASSIVE;    // For wildcard IP address

	BString<100> portStr("%d", m_port);

	int res = getaddrinfo(m_host, portStr, &addr_hints, &addr_list);
	if (res != 0)
	{
		ReportError("Could not resolve hostname %s", m_host, true
#ifndef WIN32
			, res != EAI_SYSTEM ? res : 0
			, res != EAI_SYSTEM ? gai_strerror(res) : nullptr
#endif
			);
		return false;
	}

	m_broken = false;
	m_socket = INVALID_SOCKET;
	for (addr = addr_list; addr != nullptr; addr = addr->ai_next)
	{
		m_socket = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
#ifdef WIN32
		SetHandleInformation((HANDLE)m_socket, HANDLE_FLAG_INHERIT, 0);
#endif
		if (m_socket != INVALID_SOCKET)
		{
			int opt = 1;
			setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
			res = bind(m_socket, addr->ai_addr, addr->ai_addrlen);
			if (res != -1)
			{
				// Connection established
				break;
			}
			// Connection failed
			closesocket(m_socket);
			m_socket = INVALID_SOCKET;
		}
	}

	freeaddrinfo(addr_list);

#else

	struct sockaddr_in	sSocketAddress;
	memset(&sSocketAddress, 0, sizeof(sSocketAddress));
	sSocketAddress.sin_family = AF_INET;
	if (!m_host || strlen(m_host) == 0)
	{
		sSocketAddress.sin_addr.s_addr = htonl(INADDR_ANY);
	}
	else
	{
		sSocketAddress.sin_addr.s_addr = ResolveHostAddr(m_host);
		if (sSocketAddress.sin_addr.s_addr == INADDR_NONE)
		{
			return false;
		}
	}
	sSocketAddress.sin_port = htons(m_port);

	m_socket = socket(PF_INET, SOCK_STREAM, 0);
	if (m_socket == INVALID_SOCKET)
	{
		ReportError("Socket creation failed for %s", m_host, true);
		return false;
	}

	int opt = 1;
	setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

	int res = bind(m_socket, (struct sockaddr *) &sSocketAddress, sizeof(sSocketAddress));
	if (res == -1)
	{
		// Connection failed
		closesocket(m_socket);
		m_socket = INVALID_SOCKET;
	}
#endif

	if (m_socket == INVALID_SOCKET)
	{
		ReportError("Binding socket failed for %s", m_host, true);
		return false;
	}

	if (listen(m_socket, 100) < 0)
	{
		ReportError("Listen on socket failed for %s", m_host, true);
		return false;
	}

	m_status = csListening;

	return true;
}

int Connection::WriteLine(const char* buffer)
{
	//debug("Connection::WriteLine");

	if (m_status != csConnected)
	{
		return -1;
	}

	int res = send(m_socket, buffer, strlen(buffer), 0);
	if (res <= 0)
	{
		m_broken = true;
	}

	return res;
}

bool Connection::Send(const char* buffer, int size)
{
	debug("Sending data");

	if (m_status != csConnected)
	{
		return false;
	}

	int bytesSent = 0;
	while (bytesSent < size)
	{
		int res = send(m_socket, buffer + bytesSent, size-bytesSent, 0);
		if (res <= 0)
		{
			m_broken = true;
			return false;
		}
		bytesSent += res;
	}

	return true;
}

char* Connection::ReadLine(char* buffer, int size, int* bytesReadOut)
{
	if (m_status != csConnected)
	{
		return nullptr;
	}

	char* inpBuffer = buffer;
	size--; // for trailing '0'
	int bytesRead = 0;
	int bufAvail = m_bufAvail; // local variable is faster
	char* bufPtr = m_bufPtr; // local variable is faster
	while (size)
	{
		if (!bufAvail)
		{
			bufAvail = recv(m_socket, m_readBuf, m_readBuf.Size() - 1, 0);
			if (bufAvail < 0)
			{
				ReportError("Could not receive data on socket from %s", m_host, true);
				m_broken = true;
				break;
			}
			else if (bufAvail == 0)
			{
				break;
			}
			bufPtr = m_readBuf;
			m_readBuf[bufAvail] = '\0';
		}

		int len = 0;
		char* p = (char*)memchr(bufPtr, '\n', bufAvail);
		if (p)
		{
			len = (int)(p - bufPtr + 1);
		}
		else
		{
			len = bufAvail;
		}

		if (len > size)
		{
			len = size;
		}

		memcpy(inpBuffer, bufPtr, len);
		inpBuffer += len;
		bufPtr += len;
		bufAvail -= len;
		bytesRead += len;
		size -= len;

		if (p)
		{
			break;
		}
	}
	*inpBuffer = '\0';

	m_bufAvail = bufAvail > 0 ? bufAvail : 0; // copy back to member
	m_bufPtr = bufPtr; // copy back to member

	if (bytesReadOut)
	{
		*bytesReadOut = bytesRead;
	}

	m_totalBytesRead += bytesRead;

	if (inpBuffer == buffer)
	{
		return nullptr;
	}

	return buffer;
}

std::unique_ptr<Connection> Connection::Accept()
{
	debug("Accepting connection");

	if (m_status != csListening)
	{
		return nullptr;
	}

	SOCKET socket = accept(m_socket, nullptr, nullptr);
	if (socket == INVALID_SOCKET && m_status != csCancelled)
	{
		ReportError("Could not accept connection for %s", m_host, true);
	}
	if (socket == INVALID_SOCKET)
	{
		return nullptr;
	}

	return std::make_unique<Connection>(socket, m_tls);
}

int Connection::TryRecv(char* buffer, int size)
{
	debug("Receiving data");

	memset(buffer, 0, size);

	int received = recv(m_socket, buffer, size, 0);

	if (received < 0)
	{
		ReportError("Could not receive data on socket from %s", m_host, true);
	}

	return received;
}

bool Connection::Recv(char * buffer, int size)
{
	debug("Receiving data (full buffer)");

	memset(buffer, 0, size);

	char* bufPtr = (char*)buffer;
	int NeedBytes = size;

	if (m_bufAvail > 0)
	{
		int len = size > m_bufAvail ? m_bufAvail : size;
		memcpy(bufPtr, m_bufPtr, len);
		bufPtr += len;
		m_bufPtr += len;
		m_bufAvail -= len;
		NeedBytes -= len;
	}

	// Read from the socket until nothing remains
	while (NeedBytes > 0)
	{
		int received = recv(m_socket, bufPtr, NeedBytes, 0);
		// Did the recv succeed?
		if (received <= 0)
		{
			ReportError("Could not receive data on socket from %s", m_host, true);
			return false;
		}
		bufPtr += received;
		NeedBytes -= received;
	}
	return true;
}

bool Connection::DoConnect()
{
	debug("Do connecting");

	m_socket = INVALID_SOCKET;
	m_broken = false;

#ifdef HAVE_GETADDRINFO
	struct addrinfo addr_hints, *addr_list, *addr;

	memset(&addr_hints, 0, sizeof(addr_hints));
	addr_hints.ai_family = m_ipVersion == ipV4 ? AF_INET : m_ipVersion == ipV6 ? AF_INET6 : AF_UNSPEC;
	addr_hints.ai_socktype = SOCK_STREAM;

	BString<100> portStr("%d", m_port);

	int res = getaddrinfo(m_host, portStr, &addr_hints, &addr_list);
	if (res != 0)
	{
		ReportError("Could not resolve hostname %s", m_host, true
#ifndef WIN32
					, res != EAI_SYSTEM ? res : 0
					, res != EAI_SYSTEM ? gai_strerror(res) : nullptr
#endif
					);
		return false;
	}

	std::vector<SockAddr> triedAddr;
	bool connected = false;

	for (addr = addr_list; addr != nullptr; addr = addr->ai_next)
	{
		// don't try the same combinations of ai_family, ai_socktype, ai_protocol multiple times
		SockAddr sa = { addr->ai_family, addr->ai_socktype, addr->ai_protocol };
		if (std::find(triedAddr.begin(), triedAddr.end(), sa) != triedAddr.end())
		{
			continue;
		}
		triedAddr.push_back(sa);

		if (m_socket != INVALID_SOCKET)
		{
			closesocket(m_socket);
		}

		m_socket = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
#ifdef WIN32
		SetHandleInformation((HANDLE)m_socket, HANDLE_FLAG_INHERIT, 0);
#endif
		if (m_socket == INVALID_SOCKET)
		{
			// try another addr/family/protocol
			continue;
		}

		if (ConnectWithTimeout(addr->ai_addr, addr->ai_addrlen))
		{
			// Connection established
			connected = true;
			break;
		}
	}

	if (m_socket == INVALID_SOCKET && addr_list)
	{
		ReportError("Socket creation failed for %s", m_host, true);
	}

	if (!connected && m_socket != INVALID_SOCKET)
	{
		ReportError("Connection to %s failed", m_host, true);
		closesocket(m_socket);
		m_socket = INVALID_SOCKET;
	}

	freeaddrinfo(addr_list);

	if (m_socket == INVALID_SOCKET)
	{
		return false;
	}

#else

	struct sockaddr_in	sSocketAddress;
	memset(&sSocketAddress, 0, sizeof(sSocketAddress));
	sSocketAddress.sin_family = AF_INET;
	sSocketAddress.sin_port = htons(m_port);
	sSocketAddress.sin_addr.s_addr = ResolveHostAddr(m_host);
	if (sSocketAddress.sin_addr.s_addr == INADDR_NONE)
	{
		return false;
	}

	m_socket = socket(PF_INET, SOCK_STREAM, 0);
	if (m_socket == INVALID_SOCKET)
	{
		ReportError("Socket creation failed for %s", m_host, true);
		return false;
	}

	if (!ConnectWithTimeout(&sSocketAddress, sizeof(sSocketAddress)))
	{
		ReportError("Connection to %s failed", m_host, true);
		closesocket(m_socket);
		m_socket = INVALID_SOCKET;
		return false;
	}
#endif

	if (!InitSocketOpts())
	{
		return false;
	}

#ifndef DISABLE_TLS
	if (m_tls && !StartTls(true, nullptr, nullptr))
	{
		return false;
	}
#endif

	return true;
}

bool Connection::InitSocketOpts()
{
	char* optbuf = nullptr;
	int optsize = 0;
#ifdef WIN32
	int MSecVal = m_timeout * 1000;
	optbuf = (char*)&MSecVal;
	optsize = sizeof(MSecVal);
#else
	struct timeval TimeVal;
	TimeVal.tv_sec = m_timeout;
	TimeVal.tv_usec = 0;
	optbuf = (char*)&TimeVal;
	optsize = sizeof(TimeVal);
#endif
	int err = setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, optbuf, optsize);
	if (err != 0)
	{
		ReportError("Socket initialization failed for %s", m_host, true);
		return false;
	}
	err = setsockopt(m_socket, SOL_SOCKET, SO_SNDTIMEO, optbuf, optsize);
	if (err != 0)
	{
		ReportError("Socket initialization failed for %s", m_host, true);
		return false;
	}
	return true;
}

bool Connection::ConnectWithTimeout(void* address, int address_len)
{
	int flags = 0, error = 0, ret = 0;
	fd_set rset, wset;
	socklen_t len = sizeof(error);

	struct timeval ts;
	ts.tv_sec = m_timeout;
	ts.tv_usec = 0;

	//clear out descriptor sets for select
	//add socket to the descriptor sets
	FD_ZERO(&rset);
	FD_SET(m_socket, &rset);
	wset = rset;    //structure assignment ok

	//set socket nonblocking flag
#ifdef WIN32
	u_long mode = 1;
	if (ioctlsocket(m_socket, FIONBIO, &mode) != 0)
	{
		return false;
	}
#else
	flags = fcntl(m_socket, F_GETFL, 0);
	if (flags < 0)
	{
		return false;
	}

	if (fcntl(m_socket, F_SETFL, flags | O_NONBLOCK) < 0)
	{
		return false;
	}
#endif

	//initiate non-blocking connect
	ret = connect(m_socket, (struct sockaddr*)address, address_len);
	if (ret < 0)
	{
#ifdef WIN32
		int err = WSAGetLastError();
		if (err != WSAEWOULDBLOCK)
		{
			return false;
		}
#else
		if (errno != EINPROGRESS)
		{
			return false;
		}
#endif
	}

	//connect succeeded right away?
	if (ret != 0)
	{
		ret = select(m_socket + 1, &rset, &wset, nullptr, m_timeout ? &ts : nullptr);
		//we are waiting for connect to complete now
		if (ret < 0)
		{
			return false;
		}
		if (ret == 0)
		{
			//we had a timeout
#ifdef WIN32
			WSASetLastError(WSAETIMEDOUT);
#else
			errno = ETIMEDOUT;
#endif
			return false;
		}

		if (!(FD_ISSET(m_socket, &rset) || FD_ISSET(m_socket, &wset)))
		{
			return false;
		}
		//we had a positivite return so a descriptor is ready

		if (getsockopt(m_socket, SOL_SOCKET, SO_ERROR, (char*)&error, &len) < 0)
		{
			return false;
		}

		//check if we had a socket error
		if (error)
		{
			errno = error;
			return false;
		}
	}

	//put socket back in blocking mode
#ifdef WIN32
	mode = 0;
	if (ioctlsocket(m_socket, FIONBIO, &mode) != 0)
	{
		return false;
	}
#else
	if (fcntl(m_socket, F_SETFL, flags) < 0)
	{
		return false;
	}
#endif

	return true;
}

bool Connection::DoDisconnect()
{
	debug("Do disconnecting");

	if (m_socket != INVALID_SOCKET)
	{
#ifndef DISABLE_TLS
		CloseTls();
#endif
		if (m_gracefull)
		{
			closesocket_gracefully(m_socket);
		}
		else
		{
			closesocket(m_socket);
		}
		m_socket = INVALID_SOCKET;
	}

	m_status = csDisconnected;
	return true;
}

void Connection::ReadBuffer(char** buffer, int *bufLen)
{
	*bufLen = m_bufAvail;
	*buffer = m_bufPtr;
	m_bufAvail = 0;
};

void Connection::Cancel()
{
	debug("Cancelling connection");
	if (m_socket != INVALID_SOCKET)
	{
		m_status = csCancelled;
		int r = shutdown(m_socket, SHUT_RDWR);
		if (r == -1)
		{
			ReportError("Could not shutdown connection for %s", m_host, true);
		}
	}
}

void Connection::ReportError(const char* msgPrefix, const char* msgArg, bool PrintErrCode, int herrno, const char* herrMsg)
{
#ifndef DISABLE_TLS
	if (m_tlsError)
	{
		// TLS-Error was already reported
		m_tlsError = false;
		return;
	}
#endif

	BString<1024> errPrefix(msgPrefix, msgArg);

	if (PrintErrCode)
	{
#ifdef WIN32
		int ErrCode = WSAGetLastError();
		char errMsg[1024];
		errMsg[0] = '\0';
		FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, ErrCode, 0, errMsg, 1024, nullptr);
		errMsg[1024-1] = '\0';
#else
		const char* errMsg = herrMsg;
		int ErrCode = herrno;
		if (herrno == 0)
		{
			ErrCode = errno;
			errMsg = strerror(ErrCode);
		}
		else if (!herrMsg)
		{
			errMsg = hstrerror(ErrCode);
		}
#endif
		if (m_suppressErrors)
		{
			debug("%s: ErrNo %i, %s", *errPrefix, ErrCode, errMsg);
		}
		else
		{
			PrintError(BString<1024>("%s: ErrNo %i, %s", *errPrefix, ErrCode, errMsg));
		}
	}
	else
	{
		if (m_suppressErrors)
		{
			debug("%s", *errPrefix);
		}
		else
		{
			PrintError(errPrefix);
		}
	}
}

void Connection::PrintError(const char* errMsg)
{
	error("%s", errMsg);
}

#ifndef DISABLE_TLS
bool Connection::StartTls(bool isClient, const char* certFile, const char* keyFile)
{
	debug("Starting TLS");

	m_tlsSocket = std::make_unique<ConTlsSocket>(m_socket, isClient, m_host, certFile, keyFile, m_cipher, this);
	m_tlsSocket->SetSuppressErrors(m_suppressErrors);

	return m_tlsSocket->Start();
}

void Connection::CloseTls()
{
	if (m_tlsSocket)
	{
		m_tlsSocket->Close();
		m_tlsSocket.reset();
	}
}

int Connection::recv(SOCKET s, char* buf, int len, int flags)
{
	int received = 0;

	if (m_tlsSocket)
	{
		m_tlsError = false;
		received = m_tlsSocket->Recv(buf, len);
		if (received < 0)
		{
			m_tlsError = true;
			return -1;
		}
	}
	else
	{
		received = ::recv(s, buf, len, flags);
	}
	return received;
}

int Connection::send(SOCKET s, const char* buf, int len, int flags)
{
	int sent = 0;

	if (m_tlsSocket)
	{
		m_tlsError = false;
		sent = m_tlsSocket->Send(buf, len);
		if (sent < 0)
		{
			m_tlsError = true;
			return -1;
		}
		return sent;
	}
	else
	{
		sent = ::send(s, buf, len, flags);
		return sent;
	}
}
#endif

#ifndef HAVE_GETADDRINFO
in_addr_t Connection::ResolveHostAddr(const char* host)
{
	in_addr_t uaddr = inet_addr(host);
	if (uaddr == INADDR_NONE)
	{
		struct hostent* hinfo;
		bool err = false;
		int h_errnop = 0;
#ifdef HAVE_GETHOSTBYNAME_R
		struct hostent hinfobuf;
		char strbuf[1024];
#ifdef HAVE_GETHOSTBYNAME_R_6
		err = gethostbyname_r(host, &hinfobuf, strbuf, sizeof(strbuf), &hinfo, &h_errnop);
		err = err || (hinfo == nullptr); // error on null hinfo (means 'no entry')
#endif
#ifdef HAVE_GETHOSTBYNAME_R_5
		hinfo = gethostbyname_r(host, &hinfobuf, strbuf, sizeof(strbuf), &h_errnop);
		err = hinfo == nullptr;
#endif
#ifdef HAVE_GETHOSTBYNAME_R_3
		//NOTE: gethostbyname_r with three parameters were not tested
		struct hostent_data hinfo_data;
		hinfo = gethostbyname_r((char*)host, (struct hostent*)hinfobuf, &hinfo_data);
		err = hinfo == nullptr;
#endif
#else
		Guard guard(m_getHostByNameMutex);
		hinfo = gethostbyname(host);
		err = hinfo == nullptr;
#endif
		if (err)
		{
			ReportError("Could not resolve hostname %s", host, true, h_errnop);
			return INADDR_NONE;
		}

		memcpy(&uaddr, hinfo->h_addr_list[0], sizeof(uaddr));
	}
	return uaddr;
}
#endif

const char* Connection::GetRemoteAddr()
{
	struct sockaddr_in PeerName;
	int peerNameLength = sizeof(PeerName);
	if (getpeername(m_socket, (struct sockaddr*)&PeerName, (SOCKLEN_T*) &peerNameLength) >= 0)
	{
#ifdef WIN32
		m_remoteAddr = inet_ntoa(PeerName.sin_addr);
#else
		inet_ntop(AF_INET, &PeerName.sin_addr, m_remoteAddr, m_remoteAddr.Capacity());
		m_remoteAddr[m_remoteAddr.Capacity() - 1] = '\0';
#endif
	}

	return m_remoteAddr;
}

int Connection::FetchTotalBytesRead()
{
	int total = m_totalBytesRead;
	m_totalBytesRead = 0;
	return total;
}
