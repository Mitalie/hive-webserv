#include <iostream>
#include <vector>

#include "CallbackQueue.hpp"
#include "Config.hpp"
#include "Listener.hpp"
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
	std::vector<Listener> listeners;
	listeners.reserve(config.size());
	for (const auto &entry : config)
	{
		listeners.emplace_back(entry.first, entry.second);
	}
	while (true)
	{
		// TODO: clean exit mechanism
		Poll::doPoll();
		CallbackQueue::handleQueue();
	}
}
