#pragma once

#include "Poll.hpp"

/*
	A wrapper to manage Unix file descriptor ownership.

	Prevents accidental copying, closes the file on destruction, and converts
	to int for code that expects a raw file descriptor.

	Also proxies Poll handling and automatically unregisters on destruction.
*/
class UnixFD
{
public:
	// Avoid implicit construction to avoid unintentionally taking ownership

	explicit UnixFD(int fd = -1);
	UnixFD(const UnixFD &) = delete;
	UnixFD(UnixFD &&);
	UnixFD &operator=(const UnixFD &) = delete;
	UnixFD &operator=(UnixFD &&);
	~UnixFD();

	// Implicit conversion for passing to Unix functions
	operator int() const;

	/*
		Register poll event callbacks for the file descriptor
		readable: Called when POLLIN or POLLHUP occurs.
		writable: Called when POLLOUT occurs.
		error: Called when POLLERR occurs (e.g., broken pipe).
	*/
	void addToPoll(
		Poll::Callback readable,
		Poll::Callback writable,
		Poll::Callback error);
	void setReadableInterest(bool interest);
	void setWritableInterest(bool interest);

private:
	int fd;
};
