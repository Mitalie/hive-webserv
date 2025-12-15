#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <string>

#include "CgiHandler.hpp"
#include "Config.hpp"
#include "Header.hpp"
#include "IRequestHandler.hpp"
#include "IRequestManager.hpp"

/*
	Bridge between the Server logic and the CGI process.
	Implements flow control (backpressure) and timeout management.
*/
class CgiRequestHandler : public IRequestHandler
{
public:
	CgiRequestHandler(IRequestManager &manager, const Header &header, const RouteConfig &route);
	virtual ~CgiRequestHandler();

	void onBodyData(std::span<const char> data) override;
	void notifyResponseBuffer(size_t bufferSize) override;

	// Should be called periodically by the main loop to detect stuck scripts.
	void checkTimeout();

private:
	std::string findInterpreter(const std::string &scriptPath, const RouteConfig &route);

	void startCgiOutputRead();

	IRequestManager &manager_;
	std::unique_ptr<CgiHandler> cgiHandler_;
	bool responseFinished_;

	// Timeout tracking
	std::chrono::steady_clock::time_point startTime_;
	static constexpr std::chrono::seconds CGI_TIMEOUT_LIMIT = std::chrono::seconds(30);

	// Backpressure thresholds
	static constexpr size_t PIPE_WRITE_HIGH_WATER_MARK = 65536;
	static const size_t PIPE_WRITE_LOW_WATER_MARK = 4096;
	static const size_t CLIENT_SEND_HIGH_WATER_MARK = 1048576;
};
