#pragma once

#include "Config.hpp"
#include "UnixFD.hpp"

class Poll;

class Listener
{
public:
	Listener(const HostPort &hostport, const ListenerConfig &config);
	Listener(const Listener &other) = delete;
	Listener(Listener &&other) = default;
	Listener &operator=(const Listener &other) = delete;
	~Listener();

private:
	const ListenerConfig &config;
	UnixFD fd;

	// Max queue of unaccepted connections - see `man 2 listen`
	static const int acceptBacklog = 10;
	void onReadable();
};
