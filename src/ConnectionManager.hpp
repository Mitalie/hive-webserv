#pragma once

#include <set>
#include <string>

#include "Config.hpp"
#include "DelayedCleanup.hpp"
#include "Listener.hpp"
#include "UnixFD.hpp"
#include "Utils.hpp"

class ClientHandler;

class ConnectionManager
{
public:
	ConnectionManager(const HostPort &hostPort, const ListenerConfig &config);
	void destroyConnection(ClientHandler &connection);

private:
	const HostPort &hostPort;
	const ListenerConfig &config;
	Listener listener;
	std::set<ClientHandler, OrderByAddr<ClientHandler>> connections;

	void onAccept(UnixFD &&connFd, std::string &&clientIp);

	class DestroyConnectionException : public DelayedCleanupBase
	{
	public:
		DestroyConnectionException(
			ConnectionManager &manager,
			ClientHandler &connection);

	private:
		ConnectionManager &manager;
		ClientHandler &connection;
		void cleanup() const override;
	};
};
