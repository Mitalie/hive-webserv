#include "Listener.hpp"

#include <cerrno>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>

#include <netdb.h>
#include <sys/socket.h>

#include "Config.hpp"

struct AddrinfoDeleter
{
	void operator()(addrinfo *p)
	{
		if (p)
			freeaddrinfo(p);
	}
};
// Use std::unique_ptr with custom deleter to manage getaddrinfo results
using Addrinfo = std::unique_ptr<addrinfo, AddrinfoDeleter>;

Listener::Listener(const HostPort &hostport, AcceptCallback &&onAccept)
	: onAccept(onAccept)
{
	// Use getaddrinfo to parse the host and port strings from config, then
	// create the socket, bind it to address/port, and set it to listen mode.
	// Any errors here are considered fatal.
	addrinfo gaiHint{
		.ai_flags = AI_NUMERICHOST | AI_NUMERICSERV | AI_ADDRCONFIG | AI_PASSIVE,
		.ai_family = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM,
		.ai_protocol = 0,
		.ai_addrlen = 0,
		.ai_addr = 0,
		.ai_canonname = 0,
		.ai_next = 0,
	};
	addrinfo *gaiResRaw = nullptr;
	int gaiErr = getaddrinfo(hostport.host.c_str(), hostport.port.c_str(), &gaiHint, &gaiResRaw);
	Addrinfo gaiRes(gaiResRaw);
	if (gaiErr)
		throw std::runtime_error(std::string("getaddrinfo: ") + gai_strerror(gaiErr));
	fd = UnixFD(socket(gaiRes->ai_family, gaiRes->ai_socktype, gaiRes->ai_protocol));
	if (fd < 0)
		throw std::runtime_error(std::string("socket: ") + strerror(errno));
	// Set SO_REUSEADDR to allow server to start even if there are connections
	// (but not listeners) with the same local addr/port. This allows restarting
	// the server even if previous client connections remain in TIME-WAIT state.
	int value = 1;
	int ssoErr = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value));
	if (ssoErr)
		throw std::runtime_error(std::string("setsockopt(SO_REUSEADDR): ") + strerror(errno));
	int bindErr = bind(fd, gaiRes->ai_addr, gaiRes->ai_addrlen);
	if (bindErr)
		throw std::runtime_error(std::string("bind: ") + strerror(errno));
	int listenErr = listen(fd, acceptBacklog);
	if (listenErr)
		throw std::runtime_error(std::string("listen: ") + strerror(errno));
	fd.addToPoll(
		[this]()
		{ onReadable(); },
		// Listening socket is never writable
		{},
		// Does POLLERR ever happen for listening socket?
		// If it does, we probably should consider it fatal.
		[]()
		{ throw std::runtime_error("Listener socket in error state"); });
	fd.setReadableInterest(true);
}

Listener::~Listener()
{
}

void Listener::onReadable()
{
	int connFd = accept(fd, nullptr, nullptr);
	// Assume that any errors from accept are transient and/or remove the failed
	// connection from the queue. Just wait for another onReadable callback.
	if (connFd < 0)
		return;
	onAccept(UnixFD(connFd));
}
