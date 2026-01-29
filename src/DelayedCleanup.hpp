#pragma once

#include <concepts>
#include <exception>
#include <functional>

/*
	Utility exception to release resources after bailing out of functions that
	might otherwise still be using those resources.

	Users must create a derived class that overrides the `cleanup` member
	function to release the resource(s) in question and stores any data
	necessary for the cleanup as member variables.

	Define the point where the exception is caught and cleanup is performed with
	`handleDelayedCleanup<Ex>(callable, args...)`, where `Ex` specifies the
	exception class to catch.
*/
class DelayedCleanupBase : public std::exception
{
private:
	/*
		Cleanup function to release the resource once the exception is caught.
	*/
	virtual void cleanup() const = 0;

	// Allow cleanup helper to call private member function `cleanup`
	template <typename Ex, typename Callable, typename... Args>
		requires(
			std::derived_from<Ex, DelayedCleanupBase> &&
			std::invocable<Callable, Args...>)
	friend void handleDelayedCleanup(Callable &&callable, Args &&...args);
};

/*
	Invoke `callable` with `args` and automatically handle exceptions of type
	`Ex` by calling their `cleanup` member function.

	`Ex` must be manually specified as `DelayedCleanupBase` or a derived class.

	Use a lambda for `callable` to wrap more than a single function call in the
	same try-catch.
*/
template <typename Ex, typename Callable, typename... Args>
	requires(
		std::derived_from<Ex, DelayedCleanupBase> &&
		std::invocable<Callable, Args...>)
void handleDelayedCleanup(Callable &&callable, Args &&...args)
{
	try
	{
		std::invoke(
			std::forward<Callable>(callable),
			std::forward<Args>(args)...);
	}
	catch (const Ex &ex)
	{
		// Calling private Ex::cleanup directly is not allowed if Ex does not
		// declare handleDelayedCleanup as friend. Cast to base class, which
		// contains the appropriate friend declaration. The virtual function
		// call is dispatched dynamically either way.
		static_cast<const DelayedCleanupBase &>(ex).cleanup();
	}
}
