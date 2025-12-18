#include <exception>
#include <iostream>

#include "CallbackQueue.hpp"
#include "Config.hpp"
#include "Listener.hpp"
#include "Poll.hpp"

int main()
try
{
	Listener l(HostPort{"127.0.0.1", "8042"}, {});

	std::cout << "In another terminal, run \n\n    nc 127.0.0.1 8042 < testdata/clientHandler.txt\n\n"
			  << "Yes, the server never terminates the connection\n"
			  << "Yes, it leaks a ClientHandler for every new connection\n"
			  << "But hey, it kinda works! (hopefully)\n"
			  << std::endl;
	while (true)
	{
		Poll::doPoll();
		CallbackQueue::handleQueue();
	}
}
catch (std::exception &e)
{
	std::cerr << e.what() << std::endl;
	return 1;
}
catch (...)
{
	std::cerr << "Unknown exception" << std::endl;
	return 1;
}
