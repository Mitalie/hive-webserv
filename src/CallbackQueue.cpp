#include "CallbackQueue.hpp"

#include <cstddef>

#include "DelayedCleanup.hpp"

// Singleton instance
CallbackQueue CallbackQueue::instance;

void CallbackQueue::CallbackOwner::queueCallback(Callback cb)
{
	instance.queue.push_back({this, cb});
}

CallbackQueue::CallbackOwner::~CallbackOwner()
{
	std::erase_if(
		instance.queue,
		[this](CallbackEntry entry)
		{ return entry.owner == this; });
}

void CallbackQueue::handleQueue()
{
	/*
		Use indexing instead of range-based for loop to avoid problems with
		iterator invalidation if a callback function queues more callbacks.
		Keep iterating until there are no more entries and then clear the queue.
	*/
	for (size_t i = 0; i < instance.queue.size(); ++i)
	{
		// This function should only be called from main, so it should be safe
		// to do *any* delayed cleanup here
		handleDelayedCleanup<DelayedCleanupBase>(instance.queue[i].cb);
	}
	instance.queue.clear();
}
