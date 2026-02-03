#include "ConnectionManager.hpp"

#include <utility>

#include "ClientHandler.hpp"
#include "Config.hpp"
#include "UnixFD.hpp"

ConnectionManager::ConnectionManager(const HostPort &hostPort, const ListenerConfig &config)
	: config(config),
	  listener(
		  hostPort,
		  [this](UnixFD &&connFd)
		  { onAccept(std::move(connFd)); })
{
}

void ConnectionManager::onAccept(UnixFD &&connFd)
{
	// TODO: store connections to facilitate clean exit
	new ClientHandler(config, std::move(connFd));
}
