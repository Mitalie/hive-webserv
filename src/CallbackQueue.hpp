#pragma once

#include <functional>
#include <vector>

/*
	Queue for asynchronous callbacks that are ready to execute without waiting
	for an event through Poll.

	This allows dispatching tasks whose input data is already available through
	the main loop instead of recursively executing the next task at the end of
	the previous one.
*/
class CallbackQueue
{
public:
	using Callback = std::function<void()>;
	static void handleQueue();

	/*
		Owner class for callbacks added to queue.
		All callbacks must be queued through an owner.
		The owner removes pending callbacks from the queue on destruction.

		The owner is identified by its address, so it is not copyable or movable.
	*/
	class CallbackOwner
	{
	public:
		CallbackOwner() = default;
		~CallbackOwner();
		CallbackOwner(const CallbackOwner &) = delete;
		CallbackOwner &operator=(const CallbackOwner &) = delete;

		void queueCallback(CallbackQueue::Callback cb);
	};

private:
	CallbackQueue() = default;
	CallbackQueue(const CallbackQueue &other) = delete;
	CallbackQueue &operator=(const CallbackQueue &other) = delete;

	struct CallbackEntry {
		CallbackOwner *owner;
		Callback cb;
	};
	std::vector<CallbackEntry> queue;
	static CallbackQueue instance;
};
