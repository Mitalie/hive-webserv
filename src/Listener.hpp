#pragma once

#include <functional>

#include "Config.hpp"
#include "UnixFD.hpp"

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
