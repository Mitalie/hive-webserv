#include "DeleteRequestHandler.hpp"

#include <cstddef>
#include <span>
#include <string>

#include <unistd.h>

#include "IRequestManager.hpp"

DeleteRequestHandler::DeleteRequestHandler(IRequestManager &manager, const char *filePath)
{
	int res = unlink(filePath);
	if (res)
		sendResponse(manager, 500, "Internal Server Error", "Failed to delete file");
	else
		sendResponse(manager, 200, "OK", "File deleted successfully");
}

void DeleteRequestHandler::onBodyData(std::span<const char> /*data*/)
{
	// DELETE requests typically don't have a body, ignore any data
}

void DeleteRequestHandler::onBodyDone()
{
	// No body to handle for DELETE
}

void DeleteRequestHandler::notifyResponseBuffer(size_t /*bufferSize*/)
{
	// No buffering logic needed for DELETE
}

void DeleteRequestHandler::sendResponse(IRequestManager &manager, int code, const std::string &statusText, const std::string &message)
{
	std::string response =
		"HTTP/1.1 " +
		std::to_string(code) +
		" " +
		statusText +
		"\r\n"
		"Content-Type: text/plain\r\n"
		"Content-Length: " +
		std::to_string(message.size()) +
		"\r\n\r\n" +
		message;
	manager.writeResponseData(response);
	manager.onRequestDone();
}
