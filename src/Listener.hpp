#pragma once

#include "UnixFD.hpp"

class Poll;

class Listener
{
public:
	Listener(const char *addr, const char *port); // TODO: listener args
	Listener(const Listener &other) = delete;
	Listener &operator=(const Listener &other) = delete;
	~Listener();

private:
	UnixFD fd;

	// Max queue of unaccepted connections - see `man 2 listen`
	static const int acceptBacklog = 10;
	void onReadable();
};
