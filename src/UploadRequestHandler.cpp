#include "UploadRequestHandler.hpp"

#include <cstddef>
#include <filesystem>
#include <iostream>
#include <span>
#include <string>
#include <system_error>

#include "Config.hpp"
#include "IRequestManager.hpp"
#include "RequestHeader.hpp"

UploadRequestHandler::UploadRequestHandler(IRequestManager &manager, const RequestHeader &header, const RouteConfig &route)
	: manager_(manager), header_(header), route_(route), done_(false), fileOpen_(false)
{
	// 1. Ensure upload directory exists
	std::error_code ec;
	if (!std::filesystem::exists(route_.uploadStore, ec))
	{
		if (!std::filesystem::create_directories(route_.uploadStore, ec))
		{
			manager_.onRequestError(403);
			return;
		}
	}

	// 2. Sanitize filename (no directory traversal, no empty, no special chars)
	std::string filename = std::filesystem::path(header_.path()).filename().string();
	if (filename.empty() || filename == "." || filename == "..")
		filename = "upload.bin";
	// Remove dangerous characters (very basic)
	for (char &c : filename)
	{
		if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|')
			c = '_';
	}

	// 3. Unique filename logic (document.txt, document(1).txt, ...)
	std::string base = filename;
	std::string ext;
	size_t dot = filename.find_last_of('.');
	if (dot != std::string::npos && dot != 0)
	{
		base = filename.substr(0, dot);
		ext = filename.substr(dot);
	}
	targetPath_ = route_.uploadStore + "/" + filename;
	int count = 1;
	while (std::filesystem::exists(targetPath_))
	{
		targetPath_ = route_.uploadStore + "/" + base + "(" + std::to_string(count++) + ")" + ext;
	}

	// 4. MIME type check (basic, for multipart/form-data)
	std::string contentType = header_.get("Content-Type");
	if (contentType.find("multipart/form-data") == std::string::npos)
	{
		manager_.onRequestError(415);
		return;
	}

	// 5. Parse boundary from Content-Type
	size_t pos = contentType.find("boundary=");
	if (pos == std::string::npos)
	{
		manager_.onRequestError(400);
		return;
	}
	boundary_ = "--" + contentType.substr(pos + 9);
}

UploadRequestHandler::~UploadRequestHandler()
{
	if (outFile_.is_open())
		outFile_.close();
}

void UploadRequestHandler::onBodyData(std::span<const char> data)
{
	if (done_)
		return;

	// Buffer to accumulate multipart data (instance member, not static)
	multipartBuffer_.append(data.data(), data.size());

	// If not open, look for start of file part
	if (!fileOpen_)
	{
		// Find the double CRLF after the part headers
		size_t headerEnd = multipartBuffer_.find("\r\n\r\n");
		if (headerEnd == std::string::npos)
			return; // Wait for more data
		size_t fileDataStart = headerEnd + 4;
		outFile_.open(targetPath_, std::ios::binary | std::ios::out);
		if (!outFile_)
		{
			manager_.onRequestError(500);
			return;
		}
		fileOpen_ = true;
		// Write any file data already received
		size_t boundaryPos = multipartBuffer_.find(boundary_, fileDataStart);
		size_t fileDataEnd = (boundaryPos != std::string::npos) ? boundaryPos - 2 : std::string::npos; // -2 for \r\n before boundary
		if (fileDataEnd != std::string::npos)
			outFile_.write(multipartBuffer_.data() + fileDataStart, fileDataEnd - fileDataStart);
		else
			outFile_.write(multipartBuffer_.data() + fileDataStart, multipartBuffer_.size() - fileDataStart);
		if (!outFile_)
		{
			manager_.onRequestError(507);
			return;
		}
		// If boundary found, upload is done
		if (boundaryPos != std::string::npos)
		{
			outFile_.close();
			uploadComplete("File uploaded successfully");

			return;
		}
		// Remove processed header portion, keep remaining data
		multipartBuffer_.erase(0, fileDataStart);
	}
	else
	{
		// File is open, look for boundary (end of file data)
		size_t boundaryPos = multipartBuffer_.find(boundary_);
		if (boundaryPos != std::string::npos)
		{
			// Ensure there are at least two bytes (\r\n) before the boundary
			if (boundaryPos < 2)
			{
				// Malformed multipart data: not enough data before boundary
				if (outFile_.is_open())
				{
					outFile_.close();
				}
				manager_.onRequestError(400);
				return;
			}
			// Write up to boundary (excluding trailing \r\n)
			size_t fileDataEnd = boundaryPos - 2;
			outFile_.write(multipartBuffer_.data(), fileDataEnd);
			outFile_.close();
			uploadComplete("File uploaded successfully");
			return;
		}
		else
		{
			// Keep potential partial boundary at end (boundary_.size() bytes)
			size_t safeWrite = multipartBuffer_.size() > boundary_.size()
								   ? multipartBuffer_.size() - boundary_.size()
								   : 0;
			if (safeWrite > 0)
			{
				outFile_.write(multipartBuffer_.data(), safeWrite);
				multipartBuffer_.erase(0, safeWrite);
			}
		}
		if (!outFile_)
		{
			manager_.onRequestError(500);
			return;
		}
	}
}

void UploadRequestHandler::onBodyDone()
{
	// If done_ is false, it means the stream ended (or content length was reached)
	// but we never found the multipart boundary. The upload is incomplete/corrupt.
	if (!done_)
	{
		// 1. Close the file if open
		if (outFile_.is_open())
			outFile_.close();

		// 2. Delete the partial/corrupt file
		std::filesystem::remove(targetPath_);

		// 3. Send error and mark done
		manager_.onRequestError(400);
	}
}

void UploadRequestHandler::notifyResponseBuffer(size_t /*bufferSize*/)
{
	// No buffering logic for upload
}

void UploadRequestHandler::uploadComplete(const std::string &message)
{
	std::string response =
		"HTTP/1.1 201 Created\r\n"
		"Content-Type: text/plain\r\n"
		"Content-Length: " + std::to_string(message.size()) + "\r\n"
		"Connection: close\r\n\r\n" +
		message;

	manager_.writeResponseData(response);
	manager_.onRequestDone();
	done_ = true;
}
