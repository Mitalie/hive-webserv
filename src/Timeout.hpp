#pragma once

#include <chrono>
#include <functional>
#include <set>

class TimeoutOwner;

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

	friend class TimeoutOwner;

	struct Entry
	{
		std::chrono::steady_clock::time_point expiry;
		TimeoutOwner *owner;
		auto operator<=>(const Entry &other) const = default;
	};

	/*
		Use sorted set as a priority queue. First entry in the set is the first
		timeout to expire. A BST has more overhead than a heap-based priority
		queue, but it has stable iterators and can easily adjust priority with
		remove-edit-reinsert. With a heap we'd need to update backreferences
		whenever entries are moved to avoid O(n) search, and either manually
		implement remove-middle or priority adjustment, or allow stale entries
		which inflate the queue size until they bubble to the front.
	*/

	using Queue = std::set<Entry>;
	using QueueEntry = Queue::const_iterator;
	Queue queue;
};

class TimeoutOwner
{
public:
	TimeoutOwner() = default;
	~TimeoutOwner();
	TimeoutOwner(const TimeoutOwner &) = delete;
	TimeoutOwner operator=(const TimeoutOwner &) = delete;

	/*
		Set `newCallback` to be called `duration` from now, replacing any
		previous timeout set with this TimeoutOwner instance. Pass an empty
		function to disable a previous timeout without setting a new one.
	*/
	void resetTimeout(
		std::chrono::steady_clock::duration duration,
		std::function<void()> newCallback);

private:
	friend class TimeoutManager;

	std::function<void()> callback;
	// queueEntry must be valid whenever callback is non-empty
	TimeoutManager::QueueEntry queueEntry;
};
