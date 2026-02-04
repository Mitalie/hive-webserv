#pragma once

#include <chrono>
#include <functional>

class TimeoutManager
{
public:
	static void processTimeouts();

private:
	TimeoutManager() = default;
	~TimeoutManager() = default;
	TimeoutManager(const TimeoutManager &) = delete;
	TimeoutManager operator=(const TimeoutManager &) = delete;

	// Singleton instance
	static TimeoutManager instance;
};

class TimeoutOwner
{
public:
	TimeoutOwner() = default;
	~TimeoutOwner();
	TimeoutOwner(const TimeoutOwner &) = delete;
	TimeoutOwner operator=(const TimeoutOwner &) = delete;

	/*
		Set `callback` to be called `time` from now, replacing any previous
		timeout set with this TimeoutOwner instance. Pass an empty function
		to disable a previous timeout without setting a new one.
	*/
	void resetTimeout(std::chrono::nanoseconds time, std::function<void()> callback);
};
