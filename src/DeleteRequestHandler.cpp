#include "DeleteRequestHandler.hpp"

#include <cerrno>
#include <cstddef>
#include <span>
#include <string>

#include <unistd.h>

#include "IRequestManager.hpp"

DeleteRequestHandler::DeleteRequestHandler(IRequestManager &manager, const char *filePath)
{
	if (std::string(filePath).length() > 1024)
	{
		manager.onRequestError(414);
		return;
	}
	if (unlink(filePath) == 0)
	{
		sendResponse(manager, 200, "OK", "File deleted successfully");
	}
	else
	{
		switch (errno)
		{
		case ENOENT:
			manager.onRequestError(404);
			break;
		case EACCES:
		case EPERM:
		case EROFS:
			manager.onRequestError(403);
			break;
		case EISDIR:
			// Specifically noted in man page: Linux returns EISDIR if pathname refers to a directory
			manager.onRequestError(403);
			break;
		case ENAMETOOLONG:
			manager.onRequestError(414);
			break;
		case ELOOP:
		case ENOTDIR:
			// Path components are invalid or circular
			manager.onRequestError(400);
			break;
		case EBUSY:
		case ETXTBSY:
			// File is a mount point or an active executable
			manager.onRequestError(503);
			break;
		case EIO:
			manager.onRequestError(500);
			break;
		default:
			manager.onRequestError(500);
			break;
		}
	}
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
