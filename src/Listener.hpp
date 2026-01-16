#pragma once

#include <functional>

#include "Config.hpp"
#include "UnixFD.hpp"

/*
	A class to create a listening socket and accept connections from it.
	The listening socket is created in constructor and closed on destruction.
	The listening socket is monitored through Poll for incoming connections,
	and accepted connections are passed to the `onAccept` callback.
*/
class Listener
{
public:
	using AcceptCallback = std::function<void(UnixFD &&connFd)>;

	Listener(const HostPort &hostport, AcceptCallback &&onAccept);
	Listener(const Listener &other) = delete;
	Listener(Listener &&other) = default;
	Listener &operator=(const Listener &other) = delete;
	~Listener();

private:
	AcceptCallback onAccept;
	UnixFD fd;

	// Max queue of unaccepted connections - see `man 2 listen`
	static const int acceptBacklog = 10;
	void onReadable();
};
