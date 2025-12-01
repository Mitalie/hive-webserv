#include "Poll.hpp"

#include <cstddef>
#include <memory>
#include <stdexcept>

#include <sys/poll.h>

// Singleton instance
Poll Poll::instance;

// TODO: persist pollfd vector instead of rebuilding, and store idx in map
// TODO: or switch to epoll?
// TODO: handle POLLERR and/or POLLHUP?
// TODO: take ownership of FDs for cleanup?

Poll::Poll()
{
	// TODO: initialize poll mechanism
}

Poll::~Poll()
{
	// TODO: cleanup poll mechanism
}

void Poll::doPoll()
{
	size_t numFds = instance.fdMap.size();
	// Build pollfd vector for system call
	std::unique_ptr<pollfd[]> fds(new pollfd[numFds]);
	size_t fdsIdx = 0;
	for (auto fdCallbacks : instance.fdMap)
	{
		pollfd &current = fds[fdsIdx++];
		current.fd = fdCallbacks.first;
		current.events = 0;
		if (fdCallbacks.second.readableInterest)
			current.events |= POLLIN;
		if (fdCallbacks.second.writableInterest)
			current.events |= POLLOUT;
	}
	// Perform system call
	int res = poll(fds.get(), numFds, -1);
	if (res < 0)
		throw std::runtime_error("poll");
	// Process results
	fdsIdx = 0;
	while (fdsIdx < numFds)
	{
		// TODO: what if one callback modifies an entry that a later callback uses?
		pollfd &current = fds[fdsIdx++];
		if (current.revents & POLLIN)
			instance.fdMap[current.fd].readable();
		if (current.revents & POLLOUT)
			instance.fdMap[current.fd].writable();
	}
}

void Poll::addFd(int fd, Callback readable, Callback writable)
{
	// TODO: error if exists
	instance.fdMap[fd] = {
		.readable = readable,
		.writable = writable,
		.readableInterest = false,
		.writableInterest = false,
	};
	// TODO: register with poll mechanism
}

void Poll::removeFd(int fd)
{
	// TODO: error if doesn't exist
	instance.fdMap.erase(fd);
	// TODO: unregister with poll mechanism
}

void Poll::setReadableInterest(int fd, bool interest)
{
	instance.fdMap.at(fd).readableInterest = interest;
}

void Poll::setWritableInterest(int fd, bool interest)
{
	instance.fdMap.at(fd).writableInterest = interest;
}
