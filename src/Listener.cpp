#include "Listener.hpp"

#include <cerrno>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>

#include <netdb.h>
#include <sys/socket.h>

#include "ClientHandler.hpp"
#include "Config.hpp"

struct AddrinfoDeleter
{
	void operator()(addrinfo *p)
	{
		if (p)
			freeaddrinfo(p);
	}
};
using Addrinfo = std::unique_ptr<addrinfo, AddrinfoDeleter>;

Listener::Listener(const HostPort &hostport, const ListenerConfig &config)
	: config(config)
{
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
		{},	 // listening socket is never writable
		{}); // ignore errors in test
	fd.setReadableInterest(true);
}

Listener::~Listener()
{
}

void Listener::onReadable()
{
	sockaddr_storage addr;
	socklen_t addrlen = sizeof(addr);
	int connFd = accept(fd, (sockaddr *)&addr, &addrlen);
	if (connFd < 0)
		throw std::runtime_error(std::string("accept: ") + strerror(errno));
	// TODO: manage ClientHandler lifecycle somehow
	new ClientHandler(config, UnixFD(connFd));
}
