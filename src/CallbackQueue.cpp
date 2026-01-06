#include "CallbackQueue.hpp"

#include <cstddef>

#include "AbortWorkException.hpp"

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
	{
		try
		{
			instance.queue[i]();
		}
		catch (const AbortWorkException &e)
		{
			// Do nothing but stop exception propagation. Derived classes of AbortWorkException
			// may perform additional cleanup in their destructor.
		}
	}
	instance.queue.clear();
}
