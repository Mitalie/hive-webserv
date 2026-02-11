#include <Timeout.hpp>

#include <chrono>
#include <functional>
#include <utility>

#include "DelayedCleanup.hpp"

// Singleton instance
TimeoutManager TimeoutManager::instance;

void TimeoutManager::processTimeouts()
{
	auto now = std::chrono::steady_clock::now();
	while (!instance.queue.empty())
	{
		// Callback could remove/add entries, so reload head of the queue on every iteration
		QueueEntry entry = instance.queue.begin();
		if (entry->expiry > now)
			break ;
		// Callback could also remove current entry and invalidate the iterator, so erase before executing callback.
		// But erase invalidates owner->queueEntry, so unset owner->callback to maintain invariant.
		auto callback = std::move(entry->owner->callback);
		entry->owner->callback = nullptr;
		instance.queue.erase(entry);
		if (callback)
			handleDelayedCleanup<DelayedCleanupBase>(callback);
	}
}

TimeoutOwner::~TimeoutOwner()
{
	// If callback is non-empty, we own a queue entry
	if (callback)
		TimeoutManager::instance.queue.erase(queueEntry);
}

void TimeoutOwner::resetTimeout(
	std::chrono::steady_clock::duration duration,
	std::function<void()> newCallback)
{
	// Remove old queue entry, new expiry won't match anyway
	if (callback)
		TimeoutManager::instance.queue.erase(queueEntry);
	callback = std::move(newCallback);
	if (callback)
	{
		// Callback is non-empty, so insert into queue and store iterator
		auto expiry = std::chrono::steady_clock::now() + duration;
		auto [entry, didInsert] = TimeoutManager::instance.queue.insert({expiry, this});
		(void)didInsert;
		queueEntry = entry;
	}
}
