#pragma once

#include "IRequestHandler.hpp"
#include "IRequestManager.hpp"
#include "Header.hpp"
#include "Config.hpp"
#include "CgiHandler.hpp"
#include <memory>
#include <string>
#include <functional>

/*
	CgiRequestHandler acts as the bridge between the Server's Event Loop (Manager)
	and the CGI Process (CgiHandler).

	It implements flow control (Backpressure) to manage memory usage during
	large transfers.
*/
class CgiRequestHandler : public IRequestHandler
{
public:
	CgiRequestHandler(IRequestManager &manager, const Header &header, const RouteConfig &route);
	virtual ~CgiRequestHandler();

	void onBodyData(std::span<const char> data) override;
	void notifyResponseBuffer(size_t bufferSize) override;

private:
	void startCgiOutputRead();

	IRequestManager &manager_;
	std::unique_ptr<CgiHandler> cgiHandler_;
	bool responseFinished_;

	// Flow Control Thresholds
	static const size_t PIPE_WRITE_HIGH_WATER_MARK = 65536;	   // 64KB
	static const size_t PIPE_WRITE_LOW_WATER_MARK = 4096;	   // 4KB
	static const size_t CLIENT_SEND_HIGH_WATER_MARK = 1048576; // 1MB
};
