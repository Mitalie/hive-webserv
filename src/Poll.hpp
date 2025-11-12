#pragma once

#include <functional>
#include <map>

class Poll
{
public:
	Poll();
	Poll(const Poll &) = delete;
	Poll operator=(const Poll &) = delete;
	~Poll();

	void doPoll();

	using Callback = std::function<void()>;
	void addFd(int fd, Callback readable, Callback writable);
	void removeFd(int fd);
	void setReadableInterest(int fd, bool interest);
	void setWritableInterest(int fd, bool interest);

private:
	struct Callbacks
	{
		Callback readable;
		Callback writable;
		bool readableInterest;
		bool writableInterest;
	};
	std::map<int, Callbacks> fdMap;
};
