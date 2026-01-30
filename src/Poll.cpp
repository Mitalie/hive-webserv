#include "Poll.hpp"

#include <cstddef>
#include <memory>
#include <stdexcept>

#include <sys/poll.h>
#include <unistd.h>

#include "DelayedCleanup.hpp"

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

void Poll::doPoll(int timeout)
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

	int res = poll(fds.get(), numFds, timeout);
	if (res < 0)
		throw std::runtime_error("poll");

	// Return immediately on timeout to allow the main loop to perform other tasks
	if (res == 0)
		return;

	fdsIdx = 0;
	while (fdsIdx < numFds)
	{
		pollfd &current = fds[fdsIdx++];
		// Ensure the FD is still registered (it might have been by callbacks in previous iterations)
		auto it = instance.fdMap.find(current.fd);
		if (it == instance.fdMap.end())
			continue;

		// 1. Handle Errors
		bool isError = current.revents & POLLERR;
		if (isError && it->second.error)
		{
			// This function should only be called from main, so it should be safe
			// to do *any* delayed cleanup here
			handleDelayedCleanup<DelayedCleanupBase>(it->second.error);

			// Check FD again after callback
			it = instance.fdMap.find(current.fd);
			if (it == instance.fdMap.end())
				continue;
		}

		// 2. Handle Read (or Hangup)
		bool isReadable = current.revents & (POLLIN | POLLHUP);
		if (isReadable && it->second.readable)
		{
			// This function should only be called from main, so it should be safe
			// to do *any* delayed cleanup here
			handleDelayedCleanup<DelayedCleanupBase>(it->second.readable);

			// Check FD again after callback
			it = instance.fdMap.find(current.fd);
			if (it == instance.fdMap.end())
				continue;
		}

		// 3. Handle Write
		bool isWritable = current.revents & POLLOUT;
		if (isWritable && it->second.writable)
			// This function should only be called from main, so it should be safe
			// to do *any* delayed cleanup here
			handleDelayedCleanup<DelayedCleanupBase>(it->second.writable);
	}
}

void Poll::FDs::addFd(int fd, Callback readable, Callback writable, Callback error)
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

void Poll::FDs::removeFd(int fd)
{
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

void Poll::FDs::setReadableInterest(int fd, bool interest)
{
	// throws if fd not found
	instance.fdMap.at(fd).readableInterest = interest;
}

void Poll::FDs::setWritableInterest(int fd, bool interest)
{
	// throws if fd not found
	instance.fdMap.at(fd).writableInterest = interest;
}
