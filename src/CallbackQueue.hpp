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
	static void queueCallback(Callback cb);
	static void handleQueue();

private:
	CallbackQueue() = default;
	CallbackQueue(const CallbackQueue &other) = delete;
	CallbackQueue &operator=(const CallbackQueue &other) = delete;

	std::vector<Callback> queue;
	static CallbackQueue instance;
};
