#pragma once

#include "Config.hpp"
#include "Listener.hpp"
#include "UnixFD.hpp"

class ConnectionManager
{
public:
	ConnectionManager(const HostPort &hostPort, const ListenerConfig &config);

private:
	const ListenerConfig &config;
	Listener listener;

	void onAccept(UnixFD &&connFd);
};
