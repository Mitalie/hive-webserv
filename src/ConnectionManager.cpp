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
	connections.emplace(config, *this, std::move(connFd));
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
