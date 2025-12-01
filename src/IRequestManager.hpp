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
		Request that the request manager starts streaming response data to the
		client. Initially data is queued but not sent to allow for sending an
		error response instead if the handler exits with an error. Once writing
		has been started, errors require terminating the connection.
	*/
	virtual void startWritingResponse() = 0;

	/*
		Queue response data to the output connection. The request manager
		reports the queue buffer size via IRequestHandler::notifyResponseBuffer
		whenever it changes.
	*/
	virtual void queueResponseData(std::span<const char> data) = 0;

	/*
		Report that the request handler has finished processing and has queued
		a valid response. The manager will start writing the response if it was
		not already started. The handler will be destroyed.
	*/
	virtual void onRequestDone() = 0;

	/*
		Report that the request handler has encountered a fatal error and cannot
		produce a valid response. The manager will either produce an error
		response or terminate the connection. The handler will be destroyed.
	*/
	virtual void onRequestError() = 0;
};
