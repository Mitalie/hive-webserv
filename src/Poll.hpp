#pragma once

#include <functional>
#include <map>

class Poll
{
public:
	static void doPoll();

	using Callback = std::function<void()>;

	static void addFd(
		int fd,
		Callback readable,
		Callback writable,
		Callback error);

	static void removeFd(int fd);
	static void closeAllRegisteredFds();
	static void setReadableInterest(int fd, bool interest);
	static void setWritableInterest(int fd, bool interest);

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
