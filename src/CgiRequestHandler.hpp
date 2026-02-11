#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <string>

#include "CgiHandler.hpp"
#include "Config.hpp"
#include "IRequestHandler.hpp"
#include "IRequestManager.hpp"
#include "RequestHeader.hpp"
#include "Timeout.hpp"

/*
	Bridge between the Server logic and the CGI process.
	Implements flow control (backpressure) and timeout management.

	For chunked request bodies, the CGI process launch is deferred until
	the entire body is received, since CGI requires CONTENT_LENGTH.
*/
class CgiRequestHandler : public IRequestHandler
{
public:
	CgiRequestHandler(IRequestManager &manager, const RequestHeader &header, const RouteConfig &route, const std::string &scriptPath);
	~CgiRequestHandler();

	void onBodyData(std::span<const char> data) override;
	void onBodyDone() override;
	void notifyResponseBuffer(size_t bufferSize) override;

private:
	std::string findInterpreter(const std::string &scriptPath, const RouteConfig &route);
	void launchCgiProcess();

	void handleCgiOutput(std::span<const char> data);
	void handleCgiEof();
	void checkTimeout();
	size_t sendChunkedData(std::span<const char> data);
	void sendBodyData(std::span<const char> data); // Helper to deduplicate logic

	// We use unique_ptr for delayed initialization.
	// The handler must only be created AFTER we have validated the interpreter path
	// in the constructor, AND after we have the full body for chunked requests.
	IRequestManager &manager_;
	std::unique_ptr<CgiHandler> cgiHandler_;
	bool responseFinished_ = false;

	// Stored for deferred CGI launch (chunked request body case)
	RequestHeader storedHeader_;
	std::string scriptPath_;
	std::string interpreter_;

	// Request body buffering (for chunked transfer from client)
	bool bufferingRequestBody_ = false; // True if client used chunked encoding
	std::string requestBodyBuffer_;		// Buffer for chunked request body

	// Buffering for CGI headers
	std::string responseBuffer_;
	bool headersParsed_ = false;

	// Response body handling
	bool useChunkedEncoding_ = false;			// True if CGI didn't provide Content-Length
	size_t remainingResponseContentLength_ = 0; // Bytes remaining to send when Content-Length is known

	// Timeout tracking via global TimeoutManager
	TimeoutOwner cgiTimer;
	static constexpr std::chrono::seconds CGI_TIMEOUT_LIMIT = std::chrono::seconds(30);

	// Backpressure thresholds
	static constexpr size_t PIPE_WRITE_HIGH_WATER_MARK = 65536;
	static const size_t PIPE_WRITE_LOW_WATER_MARK = 4096;
	static const size_t CLIENT_SEND_HIGH_WATER_MARK = 1048576;
};
