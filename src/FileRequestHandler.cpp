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

void FileRequestHandler::sendErrorResponse(int code, const std::string &message)
{
	std::string statusText = (code == 200) ? "OK" : "Error";
	std::string header =
		"HTTP/1.1 " + std::to_string(code) + " " + statusText +
		"\r\n"
		"Content-Type: text/plain\r\n"
		"Content-Length: " +
		std::to_string(message.size()) +
		"\r\n\r\n" +
		message;
	manager_.writeResponseData(std::span<const char>(header.data(), header.size()));
	manager_.onRequestDone();
}

void FileRequestHandler::start(const char *filePath)
{
	fd_ = UnixFD(open(filePath, O_RDONLY));
	if (fd_ == -1)
	{
		if (errno == ENOENT)
			sendErrorResponse(404, "File not found");
		else
			sendErrorResponse(500, "Could not open file");
		return;
	}
	struct stat st;
	// Should use fstat here to prevent rename race condition, but assignment doesn't allow it...
	if (stat(filePath, &st) == -1)
	{
		sendErrorResponse(500, "Could not stat file");
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
			// error or unexpected EOF - we may have sent data already so trash the connection
			return manager_.onRequestError();
		size_t bytesRead = result;
		if (bytesRead > bytesRemaining_)
			// race condition - file grew, we limit to size we sent in C-L header
			bytesRead = bytesRemaining_;
		bytesRemaining_ -= bytesRead;
		size_t bufferSize = manager_.writeResponseData(std::span(readBuffer, bytesRead));
		if (bytesRemaining_ == 0)
			return manager_.onRequestDone();
		if (bufferSize >= BUFFER_LIMIT)
			// buffer full, wait for notifyResponseBuffer
			return;
	}
}
