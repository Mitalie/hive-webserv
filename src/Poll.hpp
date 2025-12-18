#pragma once

#include <functional>
#include <map>

/*
	Wrapper around the poll(2) system call.
	Manages a collection of file descriptors and dispatches events to registered callbacks.
*/
class Poll
{
public:
	/*
		Executes poll() to check for events on registered file descriptors.

		timeout: Time in milliseconds to wait.
				 -1 waits indefinitely (blocking).
				 0 returns immediately (non-blocking).
				 >0 waits for specific duration.
	*/
	static void doPoll(int timeout = -1);

	using Callback = std::function<void()>;

	/*
		Closes all file descriptors currently tracked by Poll (except stdin/out/err).
		Crucial for child processes to avoid inheriting open server sockets.
	*/
	static void closeAllRegisteredFds();

	class FDs // Poll FD management, intended for use by UnixFD class only
	{
		friend class UnixFD; // Allow UnixFD to call the private functions

		static void addFd(
			int fd,
			Callback readable,
			Callback writable,
			Callback error);
		static void removeFd(int fd);
		static void setReadableInterest(int fd, bool interest);
		static void setWritableInterest(int fd, bool interest);
	};

private:
	Poll();
	Poll(const Poll &) = delete;
	Poll operator=(const Poll &) = delete;
	~Poll();

	struct Callbacks
	{
		Callback readable;
		Callback writable;
		Callback error;
		bool readableInterest;
		bool writableInterest;
	};
	std::map<int, Callbacks> fdMap;

	static Poll instance;
};
