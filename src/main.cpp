#include <iostream>
#include <vector>

#include "CallbackQueue.hpp"
#include "ClientHandler.hpp"
#include "Config.hpp"
#include "ConnectionManager.hpp"
#include "Poll.hpp"
#include "Signals.hpp"
#include "Timeout.hpp"

int main(int argc, char *argv[])
{
	setupSignals();
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
		// Signal may arrive between gotExitSignal and poll syscall.
		// Timeout may expire before or during poll syscall.
		// Set poll timeout to ensure we react to signals and timeouts reasonably quickly.
		Poll::doPoll(100);
		CallbackQueue::handleQueue();
		TimeoutManager::processTimeouts();
		if (gotExitSignal())
			break;
	}
}
