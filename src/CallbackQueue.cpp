#include "CallbackQueue.hpp"

#include <cstddef>

// Singleton instance
CallbackQueue CallbackQueue::instance;

void CallbackQueue::queueCallback(Callback cb)
{
	instance.queue.push_back(cb);
}

void CallbackQueue::handleQueue()
{
	/*
		Use indexing instead of range-based for loop to avoid problems with
		iterator invalidation if a callback function queues more callbacks.
		Keep iterating until there are no more entries and then clear the queue.
	*/
	for (size_t i = 0; i < instance.queue.size(); ++i)
		instance.queue[i]();
	instance.queue.clear();
}
