#include "UploadRequestHandler.hpp"

#include <cerrno>
#include <cstddef>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>

#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>

#include "Config.hpp"
#include "IRequestManager.hpp"
#include "RequestHeader.hpp"

/*
	Restrict to characters that should be unproblematic for both URLs and for a
	server admin using a shell.
*/
static std::string sanitizeFilename(std::string filename)
{
	for (char &c : filename)
	{
		if (c >= 'a' && c <= 'z')
			continue;
		if (c >= 'A' && c <= 'Z')
			continue;
		if (c >= '0' && c <= '9')
			continue;
		switch (c)
		{
			case '(':
			case ')':
			case '+':
			case ',':
			case '-':
			case '.':
			case '=':
			case '_':
				continue;
		}
		c = '_';
	}
	return filename;
}

UploadRequestHandler::UploadRequestHandler(IRequestManager &manager, const RequestHeader &header, const RouteConfig &route)
	: manager_(manager), header_(header), route_(route), done_(false), fileDataStarted_(false)
{
	// Extract path relative to route root
	std::string_view filename = std::string_view(header.path()).substr(route.path.length());
	while (filename.size() && filename.front() == '/')
		filename.remove_prefix(1);

	// Only allow plain filename, no directory traversal
	if (filename == "." || filename == ".." || filename.find('/') != filename.npos)
	{
		manager.onRequestError(403);
		return;
	}

	// Ensure upload store exists
	if (!std::filesystem::exists(route.uploadStore))
	{
		manager_.onRequestError(500);
		return;
	}

	// Split and sanitize the filename
	size_t lastPeriod = filename.find_last_of('.');
	if (lastPeriod == std::string::npos || lastPeriod == 0)
		lastPeriod = filename.length();
	std::string base = sanitizeFilename(std::string(filename.substr(0, lastPeriod)));
	std::string ext = sanitizeFilename(std::string(filename.substr(lastPeriod)));
	if (base.empty())
	{
		// Use a default name if uploading directly to the upload route
		base = "upload";
		ext = ".bin";
	}

	// MIME type check (basic, for multipart/form-data)
	std::string contentType = header_.get("Content-Type");
	if (contentType.find("multipart/form-data") == std::string::npos)
	{
		manager_.onRequestError(415);
		return;
	}

	// Parse boundary from Content-Type
	size_t pos = contentType.find("boundary=");
	if (pos == std::string::npos)
	{
		manager_.onRequestError(400);
		return;
	}
	boundary_ = "\r\n--" + contentType.substr(pos + 9);

	// Unique filename logic (document.txt, document(1).txt, ...)
	targetFilename_ = base + ext;
	targetPath_ = route_.uploadStore + "/" + targetFilename_;
	int fd = ::open(targetPath_.c_str(), O_CREAT | O_EXCL | O_WRONLY, 0644);
	int count = 1;
	while (fd < 0 && errno == EEXIST)
	{
		targetFilename_ = base + "(" + std::to_string(count++) + ")" + ext;
		targetPath_ = route_.uploadStore + "/" + targetFilename_;
		fd = ::open(targetPath_.c_str(), O_CREAT | O_EXCL | O_WRONLY, 0644);
	}
	if (fd < 0)
	{
		switch (errno)
		{
		case ENOENT:
		case ENOTDIR:
			manager.onRequestError(404);
			return;
		case EACCES:
		case EPERM:
		case EROFS:
		case EISDIR:
			manager.onRequestError(403);
			return;
		default:
			manager.onRequestError(500);
			return;
		}
	}
	outFile_ = UnixFD(fd);
}

UploadRequestHandler::~UploadRequestHandler()
{
	// Remove file if upload didn't complete successfully.
	// Can't do anything useful if unlink fails.
	if (!done_)
		unlink(targetPath_.c_str());
}

void UploadRequestHandler::writeData()
{
	// Look for boundary (end of file data)
	size_t dataEnd = multipartBuffer_.find(boundary_);
	bool endFound = false;
	if (dataEnd == multipartBuffer_.npos)
	{
		// Boundary not found, but we might have a part of it at the end of the buffer
		// Avoid writing that part to file until we're sure
		if (boundary_.size() > multipartBuffer_.size())
			dataEnd = 0;
		else
			dataEnd = multipartBuffer_.size() - boundary_.size() + 1;
	}
	else
		// Boundary found, upload is complete after this write
		endFound = true;

	// Write to file
	// TODO: check errno for better status code, retry EINTR and short write?
	ssize_t written = ::write(outFile_, multipartBuffer_.data(), dataEnd);
	if (written < 0)
	{
		manager_.onRequestError(500);
		return;
	}
	if (endFound)
		uploadComplete();
	else
		multipartBuffer_.erase(0, written);
}

void UploadRequestHandler::onBodyData(std::span<const char> data)
{
	if (done_)
		return;

	// Buffer to accumulate multipart data (instance member, not static)
	multipartBuffer_.append(data.data(), data.size());

	// Find start of actual file data, after the multipart header
	if (!fileDataStarted_)
	{
		// Find the double CRLF after the part headers
		// TODO: parse multipart properly instead
		size_t headerEnd = multipartBuffer_.find("\r\n\r\n");
		if (headerEnd == std::string::npos)
			return; // Wait for more data
		size_t fileDataStart = headerEnd + 4;
		// Remove processed header portion, keep remaining data
		multipartBuffer_.erase(0, fileDataStart);
		fileDataStarted_ = true;
	}
	// Start of data found, now write to file
	if (fileDataStarted_)
		writeData();
}

void UploadRequestHandler::onBodyDone()
{
	if (!done_)
	{
		// Request body ended without reaching the closing boundary
		manager_.onRequestError(400);
	}
}

void UploadRequestHandler::notifyResponseBuffer(size_t /*bufferSize*/)
{
	// No buffering logic for upload
}

void UploadRequestHandler::uploadComplete()
{
	done_ = true;

	std::string message = "File uploaded successfully";
	std::string response =
		"HTTP/1.1 201 Created\r\n"
		"Content-Type: text/plain\r\n"
		"Content-Length: " + std::to_string(message.size()) + "\r\n"
		"Location: " + route_.path + "/" + targetFilename_ +
		"Connection: close\r\n\r\n" +
		message;

	manager_.writeResponseData(response);
	manager_.onRequestDone();
}
