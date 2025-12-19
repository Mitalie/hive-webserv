#pragma once

#include <span>

/*
	Abstract interface implemented by the request manager that allows a request
	handler to communicate its status to the manager. The manager is expected to
	create a IRequestHandler instance and pass a reference to itself as a
	constructor parameter.
*/
class IRequestManager
{
public:
	/*
		Request that the request manager starts or stops streaming body data to
		the request handler. Initial state is true (body is being read), but the
		handler may want to pause reading if its output is blocked.
	*/
	virtual void setReadingBody(bool reading) = 0;

	/*
		Write response data to the output connection. The data is buffered, and
		the length of the buffer is returned so that the handler can pause
		processing if the buffer grows too large.

		The returned length may include not-yet-flushed data written by previous
		requests, but it doesn't include data already accepted into operating
		system buffers.
	*/
	virtual size_t writeResponseData(std::span<const char> data) = 0;

	/*
		Report that the request handler has finished processing and has written
		a valid response. The handler will be destroyed.
	*/
	virtual void onRequestDone() = 0;

	/*
		Report that the request handler has encountered a fatal error and cannot
		produce a valid response. The manager will either produce an error
		response or terminate the connection. The handler will be destroyed.
	*/
	virtual void onRequestError() = 0;
};
