#include <Timeout.hpp>

#include <chrono>
#include <functional>

// Singleton instance
TimeoutManager TimeoutManager::instance;

void TimeoutManager::processTimeouts()
{
	// TODO: process registered timeouts
}

TimeoutOwner::~TimeoutOwner()
{
	// TODO: remove/invalidate entry in TimeoutManager
}

void TimeoutOwner::resetTimeout(std::chrono::nanoseconds time, std::function<void()> callback)
{
	(void)time;
	(void)callback;
	// TODO: add or update entry in TimeoutManager
}
