#include "CgiRequestHandler.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <exception>
#include <iostream>
#include <memory>
#include <span>
#include <string>
#include <string_view>

#include "CgiHandler.hpp"
#include "Config.hpp"
#include "HeaderFields.hpp"
#include "IRequestManager.hpp"
#include "ReadWriteFD.hpp"
#include "RequestHeader.hpp"

CgiRequestHandler::CgiRequestHandler(IRequestManager &manager, const RequestHeader &header, const RouteConfig &route)
	: manager_(manager),
	  storedHeader_(header),
	  startTime_(std::chrono::steady_clock::now())
{
	// Determine script path and interpreter
	scriptPath_ = route.root + header.path();
	interpreter_ = findInterpreter(scriptPath_, route);

	if (interpreter_.empty())
	{
		manager_.onRequestError();
		return;
	}

	// Check if client is using chunked transfer encoding
	std::string te = header.get("transfer-encoding");
	if (te.find("chunked") != std::string::npos)
	{
		// Defer CGI launch until we have the full body
		bufferingRequestBody_ = true;
		return;
	}

	// No chunked encoding, launch CGI immediately
	launchCgiProcess();
}

void CgiRequestHandler::launchCgiProcess()
{
	try
	{
		cgiHandler_ = std::make_unique<CgiHandler>(
			storedHeader_,
			scriptPath_,
			interpreter_,
			[this](std::span<const char> data)
			{
				handleCgiOutput(data);
			},
			[this]()
			{
				handleCgiEof();
			},
			[this]()
			{
				if (!responseFinished_)
					manager_.onRequestError();
			},
			[this](size_t bufferSize)
			{
				if (bufferSize < PIPE_WRITE_LOW_WATER_MARK)
					manager_.setReadingBody(true);
			},
			// Ignore child closing std in pipe.
			ReadWriteFD::WritableErrorCallback{});
	}
	catch (const std::exception &e)
	{
		std::cerr << "[CgiRequestHandler] Error starting CGI: " << e.what() << std::endl;
		manager_.onRequestError();
		return;
	}
}

CgiRequestHandler::~CgiRequestHandler() {}

size_t CgiRequestHandler::sendChunkedData(std::span<const char> data)
{
	if (data.empty())
		return 0;

	// Format: SIZE_IN_HEX\r\nDATA\r\n
	// Convert to hex
	char hexBuf[32];
	snprintf(hexBuf, sizeof(hexBuf), "%zx\r\n", data.size());
	manager_.writeResponseData(std::string(hexBuf));
	manager_.writeResponseData(data);
	return manager_.writeResponseData(std::string_view("\r\n"));
}

void CgiRequestHandler::sendBodyData(std::span<const char> data)
{
	size_t bufferSize = 0;

	if (useChunkedEncoding_)
	{
		bufferSize = sendChunkedData(data);
	}
	else
	{
		// Using Content-Length: track bytes and limit output
		size_t toSend = std::min(data.size(), remainingResponseContentLength_);
		if (toSend > 0)
		{
			bufferSize = manager_.writeResponseData(data.subspan(0, toSend));
			remainingResponseContentLength_ -= toSend;
		}
		// If CGI sends more than promised, we ignore the excess
	}

	// Backpressure: If send buffer is full, stop reading from CGI
	if (bufferSize > CLIENT_SEND_HIGH_WATER_MARK)
		cgiHandler_->stopReading();
}

void CgiRequestHandler::handleCgiEof()
{
	// 1. Check exit status
	int exitcode = cgiHandler_->waitForExit();
	bool scriptFailed = (exitcode > 0);

	// 2. No headers sent yet
	if (!headersParsed_)
	{
		std::cerr << "[CGI] Error: Premature EOF before headers (Exit: " << exitcode << ")" << std::endl;
		manager_.onAsyncRequestError();
		return;
	}

	// 3. Content-length mismatch
	if (!useChunkedEncoding_ && remainingResponseContentLength_ > 0)
	{
		std::cerr << "[CGI] Error: Missing " << remainingResponseContentLength_ << " bytes (Exit: " << exitcode << ")" << std::endl;
		manager_.onAsyncRequestError();
		return;
	}

	// 4. Script error during chunking
	if (scriptFailed)
	{
		std::cerr << "[CGI] Error: Script exited with error " << exitcode << std::endl;
		manager_.onAsyncRequestError();
		return;
	}
	
	// 5. Success
	if (useChunkedEncoding_)
	{
		manager_.writeResponseData(std::string_view("0\r\n\r\n"));
	}
	responseFinished_ = true;
	manager_.onRequestDone();
}

void CgiRequestHandler::handleCgiOutput(std::span<const char> data)
{
	if (headersParsed_)
	{
		// State 2: Headers already sent, stream body
		sendBodyData(data);
		return;
	}

	// State 1: Buffer headers
	responseBuffer_.append(data.begin(), data.end());

	// Look for end of headers
	size_t headerEnd = responseBuffer_.find("\r\n\r\n");
	if (headerEnd != std::string::npos)
	{
		// 1. Parse CGI headers into HeaderFields
		std::string_view rawHeaders(responseBuffer_.data(), headerEnd + 2);
		HeaderFields cgiHeaders;
		cgiHeaders.parse(rawHeaders);

		// 2. Extract and remove CGI Status header (not forwarded to client)
		std::string statusLine = "HTTP/1.1 200 OK\r\n";
		std::string cgiStatus = cgiHeaders.get("status");
		if (!cgiStatus.empty())
		{
			statusLine = "HTTP/1.1 " + cgiStatus + "\r\n";
			cgiHeaders.remove("status");
		}

		// 3. Check for Content-Length to decide encoding mode
		std::string contentLengthStr = cgiHeaders.get("content-length");
		if (!contentLengthStr.empty())
		{
			// CGI provided Content-Length, forward it and track bytes
			remainingResponseContentLength_ = std::stoull(contentLengthStr);
			useChunkedEncoding_ = false;
		}
		else
		{
			// No Content-Length from CGI, use chunked encoding
			useChunkedEncoding_ = true;
			cgiHeaders.set("transfer-encoding", "chunked");
		}

		// 4. Send HTTP response: Status Line + Headers + Empty Line
		manager_.writeResponseData(statusLine);
		manager_.writeResponseData(cgiHeaders.serialize());
		manager_.writeResponseData(std::string_view("\r\n"));

		// 5. Update state
		headersParsed_ = true;

		// 6. Send any body data caught in the buffer
		std::string leftovers = responseBuffer_.substr(headerEnd + 4);
		responseBuffer_.clear();

		if (!leftovers.empty())
		{
			sendBodyData(std::span<const char>(leftovers.data(), leftovers.size()));
		}
	}
}

std::string CgiRequestHandler::findInterpreter(const std::string &scriptPath, const RouteConfig &route)
{
	std::string extension = "";
	size_t dotPos = scriptPath.find_last_of('.');
	if (dotPos != std::string::npos)
	{
		extension = scriptPath.substr(dotPos);
	}

	size_t qPos = extension.find('?');
	if (qPos != std::string::npos)
		extension = extension.substr(0, qPos);

	auto it = route.cgiInterpreters.find(extension);
	if (it != route.cgiInterpreters.end())
	{
		return it->second;
	}

	return "";
}

void CgiRequestHandler::checkTimeout()
{
	if (responseFinished_)
		return;

	auto now = std::chrono::steady_clock::now();
	if (now - startTime_ > CGI_TIMEOUT_LIMIT)
	{
		std::cerr << "[CGI] Timeout reached (" << CGI_TIMEOUT_LIMIT.count() << "s). Terminating." << std::endl;
		manager_.onRequestError();
	}
}

void CgiRequestHandler::onBodyData(std::span<const char> data)
{
	if (bufferingRequestBody_)
	{
		// Buffer the request body for deferred CGI launch
		requestBodyBuffer_.append(data.begin(), data.end());
		return;
	}

	if (!cgiHandler_)
		return;
	size_t queued = cgiHandler_->queueWrite(data);
	if (queued > PIPE_WRITE_HIGH_WATER_MARK)
		manager_.setReadingBody(false);
}

void CgiRequestHandler::onBodyDone()
{
	if (bufferingRequestBody_)
	{
		// 1. Update stored header with actual content length now that we have it all
		storedHeader_.fields.remove("transfer-encoding");
		storedHeader_.fields.set("content-length", std::to_string(requestBodyBuffer_.size()));

		// 2. Launch the process
		launchCgiProcess();

		// 3. Write the buffered body if launch was successful
		if (cgiHandler_ && !requestBodyBuffer_.empty())
		{
			cgiHandler_->queueWrite(std::span<const char>(requestBodyBuffer_.data(), requestBodyBuffer_.size()));
			requestBodyBuffer_.clear();
			requestBodyBuffer_.shrink_to_fit();
		}

		bufferingRequestBody_ = false;
	}
	// For non-chunked requests, body is streamed directly so nothing to do
}

void CgiRequestHandler::notifyResponseBuffer(size_t bufferSize)
{
	if (!cgiHandler_)
		return;

	// Backpressure: Send buffer is no longer full, resume reading from CGI
	if (bufferSize <= CLIENT_SEND_HIGH_WATER_MARK)
	{
		cgiHandler_->startReading();
	}
}
