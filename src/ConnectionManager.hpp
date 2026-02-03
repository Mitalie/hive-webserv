#pragma once

#include "Config.hpp"
#include "DelayedCleanup.hpp"
#include "Listener.hpp"
#include "UnixFD.hpp"

class ClientHandler;

class ConnectionManager
{
public:
	ConnectionManager(const HostPort &hostPort, const ListenerConfig &config);
	void destroyConnection(ClientHandler &connection);

private:
	const ListenerConfig &config;
	Listener listener;

	void onAccept(UnixFD &&connFd);

	class DestroyConnectionException : public DelayedCleanupBase
	{
	public:
		DestroyConnectionException(ClientHandler &connection);

	private:
		ClientHandler &connection;
		void cleanup() const override;
	};
};
