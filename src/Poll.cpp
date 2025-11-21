#include "Poll.hpp"

#include <cstddef>
#include <memory>
#include <stdexcept>

#include <sys/poll.h>
#include <unistd.h>

// Singleton instance
Poll Poll::instance;

// TODO: persist pollfd vector instead of rebuilding, and store idx in map
// TODO: or switch to epoll?

Poll::Poll()
{
	// epoll: allocate instance
}

Poll::~Poll()
{
	// epoll: release instance
}

void Poll::doPoll()
{
	size_t numFds = instance.fdMap.size();
	std::unique_ptr<pollfd[]> fds(new pollfd[numFds]);
	size_t fdsIdx = 0;

	// Transform internal map to pollfd array required by the syscall
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

	int res = poll(fds.get(), numFds, -1);
	if (res < 0)
		throw std::runtime_error("poll");

	fdsIdx = 0;
	while (fdsIdx < numFds)
	{
		pollfd &current = fds[fdsIdx++];

		// Ensure the FD is still registered (it might have been by callbacks in previous iterations)
		auto it = instance.fdMap.find(current.fd);
		if (it == instance.fdMap.end())
			continue;

		// 1. Handle Errors
		if (current.revents & POLLERR)
			if (it->second.error)
				it->second.error();

		// Check FD again after callback
		it = instance.fdMap.find(current.fd);
		if (it == instance.fdMap.end())
			continue;

		// 2. Handle Read (or Hangup)
		if (current.revents & (POLLIN | POLLHUP))
			if (it->second.readable)
				it->second.readable();

		// Check FD again after callback
		it = instance.fdMap.find(current.fd);
		if (it == instance.fdMap.end())
			continue;

		// 3. Handle Write
		if (current.revents & POLLOUT)
			if (it->second.writable)
				it->second.writable();
	}
}

void Poll::addFd(int fd, Callback readable, Callback writable, Callback error)
{
	if (instance.fdMap.find(fd) != instance.fdMap.end())
		throw std::logic_error("Poll::addFd: fd already registered");
	instance.fdMap[fd] = {
		.readable = readable,
		.writable = writable,
		.error = error,
		.readableInterest = false,
		.writableInterest = false,
	};
	// epoll: register fd
}

void Poll::removeFd(int fd)
{
	// throws if fd not found
	instance.fdMap.at(fd);
	instance.fdMap.erase(fd);
	// epoll: unregister fd
}

void Poll::closeAllRegisteredFds()
{
	for (const auto &entry : instance.fdMap)
	{
		int fd = entry.first;
		close(fd);
	}
}

void Poll::setReadableInterest(int fd, bool interest)
{
	// throws if fd not found
	instance.fdMap.at(fd).readableInterest = interest;
}

void Poll::setWritableInterest(int fd, bool interest)
{
	// throws if fd not found
	instance.fdMap.at(fd).writableInterest = interest;
}
