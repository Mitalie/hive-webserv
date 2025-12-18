#include <cstdlib>

#include <fcntl.h>

#include "CallbackQueue.hpp"
#include "ClientHandler.hpp"
#include "Poll.hpp"
#include "UnixFD.hpp"

int main()
{
	int fd = open("testdata/clientHandler.txt", O_RDONLY);
	if (fd < 0)
		return EXIT_FAILURE;

	ClientHandler s({}, UnixFD(fd));

	while (true)
	{
		Poll::doPoll();
		CallbackQueue::handleQueue();
	}
}
