#include "ConnectionManager.hpp"

#include <utility>
#include <string>

#include "ClientHandler.hpp"
#include "Config.hpp"
#include "UnixFD.hpp"

ConnectionManager::ConnectionManager(const HostPort &hostPort, const ListenerConfig &config)
	: config(config),
	  listener(
		  hostPort,
		  [this](UnixFD &&connFd, std::string &&clientIp)
		  { onAccept(std::move(connFd), std::move(clientIp)); })
{
}

void ConnectionManager::onAccept(UnixFD &&connFd, std::string &&clientIp)
{
	connections.emplace(config, *this, std::move(connFd), std::move(clientIp));
}

void ConnectionManager::destroyConnection(ClientHandler &connection)
{
	throw DestroyConnectionException(*this, connection);
}

ConnectionManager::DestroyConnectionException::DestroyConnectionException(
	ConnectionManager &manager,
	ClientHandler &connection)
	: manager(manager),
	  connection(connection)
{
}

void ConnectionManager::DestroyConnectionException::cleanup() const
{
	// When this is called, the exception should have propagated out of any
	// function that might still access the object.
	manager.connections.erase(connection);
}
