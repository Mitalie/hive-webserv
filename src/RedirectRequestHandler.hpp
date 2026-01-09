#pragma once

#include <string>

#include "IRequestHandler.hpp"
#include "IRequestManager.hpp"
#include "RequestHeader.hpp"

class RedirectRequestHandler : public IRequestHandler
{
public:
	RedirectRequestHandler(IRequestManager &manager, const RequestHeader &header, const std::string &location, int code);
	~RedirectRequestHandler();
	// IRequestHandler interface
	void onBodyData(std::span<const char> data) override;
	void onBodyDone() override;
	void notifyResponseBuffer(size_t bufferSize) override;

private:
	IRequestManager &manager_;
	RequestHeader header_;
	std::string location_;
	int code_;
};
