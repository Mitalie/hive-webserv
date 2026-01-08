#pragma once

#include <cstddef>
#include <map>
#include <span>
#include <string>

#include "Config.hpp"
#include "IRequestHandler.hpp"
#include "IRequestManager.hpp"
#include "RequestHeader.hpp"

class ErrorRequestHandler : public IRequestHandler
{
public:
	ErrorRequestHandler(
		IRequestManager &manager,
		const RequestHeader &header,
		const ServerConfig &config,
		int code);
	virtual ~ErrorRequestHandler();
	void onBodyData(std::span<const char> data) override;
	void notifyResponseBuffer(size_t bufferSize) override;

private:
	IRequestManager &manager_;
	const RequestHeader &header_;
	const ServerConfig &config_;
	int code_;
	void sendResponse();
	void sendDefaultResponse(const std::string &statusText);
	bool tryServeCustomErrorPage();
	std::string getStatusText() const;
};
