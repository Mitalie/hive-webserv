#pragma once

#include <cstddef>
#include <span>

/*
	Abstract interface implemented by all request handlers that allows request
	manager to stream the request body to the handler and inform it of the
	response buffer status. Request handler implementations should accept a
	IRequestManager reference as a constructor parameter and store it for
	communicating their status back to the manager.
*/
class IRequestHandler
{
public:
	/*
		Release all resources of the request handler. This may be called before
		the request handling is finished if the client connection is broken.
	*/
	virtual ~IRequestHandler() = default;

	/*
		Pass incoming data from connection to the request handler.
	*/
	virtual void onBodyData(std::span<const char> data) = 0;

	/*
		Report the status of the response buffer to the request handler so that
		it can pause processing if the buffer grows too large.
	*/
	virtual void notifyResponseBuffer(size_t bufferSize) = 0;
};
