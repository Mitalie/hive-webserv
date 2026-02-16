#include "FileRequestHandler.hpp"
#include "MimeTypes.hpp"

#include <cstddef>
#include <span>
#include <string>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "IRequestManager.hpp"

FileRequestHandler::FileRequestHandler(IRequestManager &manager, const char *filePath)
	: manager_(manager)
{
	start(filePath);
}

void FileRequestHandler::onBodyData(std::span<const char> /*data*/)
{
	// Ignore body data for GET
}

void FileRequestHandler::onBodyDone()
{
	// No body to handle for file downloads (GET)
}

void FileRequestHandler::notifyResponseBuffer(size_t bufferSize)
{
	// Resume bulk file sending when buffer is available
	if (bufferSize < BUFFER_LIMIT - CHUNK_SIZE)
		sendData();
}

void FileRequestHandler::start(const char *filePath)
{
	fd_ = UnixFD(open(filePath, O_RDONLY));
	if (fd_ == -1)
	{
		switch (errno)
		{
		case ENOENT:
			manager_.onRequestError(404);
			break;
		case EACCES:
			manager_.onRequestError(403);
			break;
		default:
			manager_.onRequestError(500);
			break;
		}
		return;
	}
	struct stat st;
	// Should use fstat here to prevent rename race condition, but assignment doesn't allow it...
	if (stat(filePath, &st) == -1)
	{
		manager_.onRequestError(500);
		return;
	}
	bytesRemaining_ = st.st_size;
	std::string header =
		"HTTP/1.1 200 OK\r\n"
		"Content-Type: " + MimeTypes::getType(filePath) + "\r\n"
									   "Content-Length: " +
		std::to_string(st.st_size) +
		"\r\n\r\n";
	manager_.writeResponseData(header);
	sendData();
}

void FileRequestHandler::sendData()
{
	if (fd_ == -1)
		return;
	char readBuffer[CHUNK_SIZE];

	while (bytesRemaining_)
	{
		ssize_t result = read(fd_, readBuffer, CHUNK_SIZE);
		if (result <= 0)
		{
			// Read error or unexpected EOF (file shrunk)
			manager_.onRequestError(500);
			return;
		}
		size_t bytesRead = result;
		if (bytesRead > bytesRemaining_)
			// race condition - file grew, we limit to size we sent in C-L header
			bytesRead = bytesRemaining_;
		bytesRemaining_ -= bytesRead;
		size_t bufferSize = manager_.writeResponseData(std::span(readBuffer, bytesRead));
		if (bytesRemaining_ && bufferSize >= BUFFER_LIMIT)
			// buffer full, wait for notifyResponseBuffer
			return;
	}
	manager_.onRequestDone();
}
