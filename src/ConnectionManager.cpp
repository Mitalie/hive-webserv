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
	new ClientHandler(config, *this, std::move(connFd));
}

void ConnectionManager::destroyConnection(ClientHandler &connection)
{
	throw DestroyConnectionException(connection);
}

ConnectionManager::DestroyConnectionException::DestroyConnectionException(ClientHandler &connection)
	: connection(connection)
{
}

void ConnectionManager::DestroyConnectionException::cleanup() const
{
	// When this is called, the exception should have propagated out of any
	// function that might still access the object.
	delete &connection;
}
