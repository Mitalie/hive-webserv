#include <iostream>
#include <utility>
#include <vector>

#include "CallbackQueue.hpp"
#include "ClientHandler.hpp"
#include "Config.hpp"
#include "Listener.hpp"
#include "Poll.hpp"
#include "UnixFD.hpp"

int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		const char *name = argc >= 1 ? argv[0] : "webserv";
		std::cerr << "Usage: " << name << " [config file]\n";
		return -1;
	}
	PortServerMap config = parseConfig(argv[1]);
	std::vector<Listener> listeners;
	listeners.reserve(config.size());
	for (const auto &[hostPort, listenerConfig] : config)
	{
		listeners.emplace_back(
			hostPort,
			// C++20 allows capturing structured bindings but Clang <16 doesn't support it
			[&listenerConfig = listenerConfig](UnixFD &&connFd)
			{
				// TODO: store connections to facilitate clean exit
				new ClientHandler(listenerConfig, std::move(connFd));
			});
	}
	while (true)
	{
		// TODO: clean exit mechanism
		Poll::doPoll();
		CallbackQueue::handleQueue();
	}
}
