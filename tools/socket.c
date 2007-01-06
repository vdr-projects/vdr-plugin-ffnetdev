#include "tools/socket.h"

#include <vdr/tools.h>

#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#define UDP_TX_BUF_SIZE ((188*7+3)*20)

cTBSocket::cTBSocket(int Type) {
	memset(&m_LocalAddr, 0, sizeof(m_LocalAddr));
	memset(&m_RemoteAddr, 0, sizeof(m_RemoteAddr));
	m_Type = Type;
}

cTBSocket::~cTBSocket() {
	if (IsOpen()) Close();
}

bool cTBSocket::OpenUDP(const std::string &Host, unsigned int Port) {
	int socket, tmp;
	struct sockaddr_in my_addr;

	if (IsOpen()) Close();
	
	if ((socket = ::socket(AF_INET, m_Type, 0)) == -1)
		return false;
		
	m_RemoteAddr.sin_family = AF_INET;
	m_RemoteAddr.sin_port   = htons(Port);
	m_RemoteAddr.sin_addr.s_addr = inet_addr(Host.c_str());
	
	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(Port);
	my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
		    
/*	tmp = 1;
	if (setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, (char*)&tmp, sizeof(tmp)) < 0)
	    goto closefd;*/
		    
	/* limit the tx buf size to limit latency */
	tmp = UDP_TX_BUF_SIZE;
	if (setsockopt(socket, SOL_SOCKET, SO_SNDBUF, (char*)&tmp, sizeof(tmp)) < 0)
	    goto closefd;

	/* the bind is needed to give a port to the socket now */
/*	if (bind(socket,(struct sockaddr *)&my_addr, sizeof(my_addr)) < 0) 
	    goto closefd;*/

	if (!cTBSource::Open(socket))
	    goto closefd; // return false;

	return true;

closefd:
    ::close(socket);
    return false;
}

bool cTBSocket::Connect(const std::string &Host, unsigned int Port) {
	socklen_t len;
	int socket;

	if (IsOpen()) Close();
		
	if ((socket = ::socket(PF_INET, m_Type, IPPROTO_IP)) == -1)
		return false;

	m_LocalAddr.sin_family = AF_INET;
	m_LocalAddr.sin_port   = 0;
	m_LocalAddr.sin_addr.s_addr = INADDR_ANY;
	if (::bind(socket, (struct sockaddr*)&m_LocalAddr, sizeof(m_LocalAddr)) 
			== -1)
		goto closefd; //return false;

	m_RemoteAddr.sin_family = AF_INET;
	m_RemoteAddr.sin_port   = htons(Port);
	m_RemoteAddr.sin_addr.s_addr = inet_addr(Host.c_str());
	if (::connect(socket, (struct sockaddr*)&m_RemoteAddr, 
			sizeof(m_RemoteAddr)) == -1) 
		goto closefd; //return false;

	len = sizeof(struct sockaddr_in);
	if (::getpeername(socket, (struct sockaddr*)&m_RemoteAddr, &len) == -1) 
		goto closefd; //return false;
	
	len = sizeof(struct sockaddr_in);
	if (::getsockname(socket, (struct sockaddr*)&m_LocalAddr, &len) == -1) 
		goto closefd; //return false;

	return cTBSource::Open(socket);

closefd:
    ::close(socket);
    return false;
}

bool cTBSocket::Listen(const std::string &Ip, unsigned int Port, int BackLog) {
	int val;
	socklen_t len;
	int socket;

	if (IsOpen()) Close();
	
	if ((socket = ::socket(PF_INET, m_Type, IPPROTO_IP)) == -1)
		return false;

	val = 1;
	if (::setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) == -1)
		goto closefd; //return false;

	m_LocalAddr.sin_family = AF_INET;
	m_LocalAddr.sin_port   = htons(Port);
	m_LocalAddr.sin_addr.s_addr = inet_addr(Ip.c_str());
	if (::bind(socket, (struct sockaddr*)&m_LocalAddr, sizeof(m_LocalAddr)) 
			== -1)
		goto closefd; //return false;

	len = sizeof(struct sockaddr_in);
	if (::getsockname(socket, (struct sockaddr*)&m_LocalAddr, &len) == -1) 
		goto closefd; //return false;
	
	if (m_Type == SOCK_STREAM && ::listen(socket, BackLog) == -1)
		goto closefd; //return false;
	
	if (!cTBSource::Open(socket))
		goto closefd; // return false;

	return true;

closefd:
    ::close(socket);
    return false;
}

bool cTBSocket::Accept(const cTBSocket &Listener) {
	socklen_t addrlen;
	int socket;

	if (IsOpen()) Close();

	addrlen = sizeof(struct sockaddr_in);
	if ((socket = ::accept(Listener, (struct sockaddr*)&m_RemoteAddr,
			&addrlen)) == -1)
		return false;

	addrlen = sizeof(struct sockaddr_in);
	if (::getsockname(socket, (struct sockaddr*)&m_LocalAddr, &addrlen) == -1)
		return false;
	
	if (!cTBSource::Open(socket))
		return false;
	
	return true;
}

RETURNS(cTBSocket, cTBSocket::Accept(void) const, ret)
	ret.Accept(*this);
RETURN(ret)

bool cTBSocket::Close(void) {
	bool ret = true;
	
	if (!IsOpen())
		ERRNUL(EBADF);

	if (::close(*this) == -1)
		ret = false;

	if (!cTBSource::Close())
		ret = false;

	memset(&m_LocalAddr, 0, sizeof(m_LocalAddr));
	memset(&m_RemoteAddr, 0, sizeof(m_RemoteAddr));

	return ret;
}

bool cTBSocket::Shutdown(int how) {
	if (!IsOpen())
		ERRNUL(EBADF);

	return ::shutdown(*this, how) != -1;
}
