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
		Release all resources of the request handler. The request handler may be
		destructed before it reports completion or error if the client
		connection is broken.
	*/
	virtual ~IRequestHandler() = default;

	/*
		Pass incoming request body data from connection to the request handler.
	*/
	virtual void onBodyData(std::span<const char> data) = 0;

	/*
		Update the write buffer length (of IRequestManager::writeResponseData)
		whenever data is moved from the internal buffer to the operating system.
		If the handler previously paused processing due to large buffer size, it
		can use this information to resume.
	*/
	virtual void notifyResponseBuffer(size_t bufferSize) = 0;
};
