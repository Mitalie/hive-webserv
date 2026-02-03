#include <iostream>
#include <vector>

#include "CallbackQueue.hpp"
#include "ClientHandler.hpp"
#include "Config.hpp"
#include "ConnectionManager.hpp"
#include "Poll.hpp"

int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		const char *name = argc >= 1 ? argv[0] : "webserv";
		std::cerr << "Usage: " << name << " [config file]\n";
		return -1;
	}
	PortServerMap config = parseConfig(argv[1]);
	std::vector<ConnectionManager> connManagers;
	connManagers.reserve(config.size());
	for (const auto &[hostPort, listenerConfig] : config)
	{
		connManagers.emplace_back(hostPort, listenerConfig);
	}
	while (true)
	{
		// TODO: clean exit mechanism
		Poll::doPoll();
		CallbackQueue::handleQueue();
	}
}
