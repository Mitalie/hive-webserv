#include "Listener.hpp"

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>

#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include "ClientHandler.hpp"
#include "Poll.hpp"

Listener::Listener(const char *addr, const char *port)
	: fd(-1)
{
	addrinfo gaiHint{
		.ai_flags = AI_NUMERICHOST | AI_NUMERICSERV | AI_ADDRCONFIG | AI_PASSIVE,
		.ai_family = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM,
	};
	addrinfo *gaiRes = nullptr;
	try
	{
		int gaiErr = getaddrinfo(addr, port, &gaiHint, &gaiRes);
		if (gaiErr)
			throw std::runtime_error(std::string("getaddrinfo: ") + gai_strerror(gaiErr));
		fd = socket(gaiRes->ai_family, gaiRes->ai_socktype, gaiRes->ai_protocol);
		if (fd < 0)
			throw std::runtime_error(std::string("socket: ") + strerror(errno));
		int bindErr = bind(fd, gaiRes->ai_addr, gaiRes->ai_addrlen);
		if (bindErr)
			throw std::runtime_error(std::string("bind: ") + strerror(errno));
		int listenErr = listen(fd, acceptBacklog);
		if (listenErr)
			throw std::runtime_error(std::string("listen: ") + strerror(errno));
	}
	catch (...)
	{
		// TODO: wrap fd and gaiRes with classes to avoid manual catch-cleanup-rethrow
		if (gaiRes)
			freeaddrinfo(gaiRes);
		if (fd >= 0)
			close(fd);
		throw;
	}
	freeaddrinfo(gaiRes);
	Poll::addFd(
		fd,
		[this]()
		{ onReadable(); },
		{}); // empty std::function as listening socket is never writable
	Poll::setReadableInterest(fd, true);
}

Listener::~Listener()
{
	Poll::removeFd(fd);
	close(fd);
}

void Listener::onReadable()
{
	sockaddr_storage addr;
	socklen_t addrlen = sizeof(addr);
	int connFd = accept(fd, (sockaddr *)&addr, &addrlen);
	if (connFd < 0)
		throw std::runtime_error(std::string("accept: ") + strerror(errno));
	// TODO: manage ClientHandler lifecycle somehow
	new ClientHandler(connFd);
}
