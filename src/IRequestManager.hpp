#pragma once

#include <span>


/*
	Abstract interface implemented by the request manager that allows request
	handlers to communicate its status to it. Request manager is expected to
	create a IRequestHandler instance and pass reference to itself to it.
*/
class IRequestManager
{
	/*
		Request that request manager starts or stops streaming body data to the
		request handler. Initial state is true (body is being read), but the
		handler may want to pause reading if its output is blocked.
	*/
	virtual void setReadingBody(bool reading) = 0;

	/*
		Queue response data to the output connection. The request manager
		reports the queue buffer size via IRequestHandler::notifyResponseBuffer
		whenever it changes.
	*/
	virtual void queueResponseData(std::span<const char> data) = 0;

	/*
		Report that the request handler has finished processing and can be
		cleaned up. The connection may be used for another request.
	*/
	virtual void onRequestDone() = 0;

	/*
		Report that the request handler has encountered a fatal error and cannot
		produce a valid response, not even an error response. The connection
		must be terminated.
	*/
	virtual void onRequestError() = 0;
};
