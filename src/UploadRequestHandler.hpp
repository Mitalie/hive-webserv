#pragma once

#include <cstddef>
#include <fstream>
#include <span>
#include <string>

#include "Config.hpp"
#include "IRequestHandler.hpp"
#include "IRequestManager.hpp"
#include "RequestHeader.hpp"

class UploadRequestHandler : public IRequestHandler
{
public:
	UploadRequestHandler(IRequestManager &manager, const RequestHeader &header, const RouteConfig &route);
	~UploadRequestHandler();
	void onBodyData(std::span<const char> data) override;
	void onBodyDone() override;
	void notifyResponseBuffer(size_t bufferSize) override;

private:
	IRequestManager &manager_;
	RequestHeader header_;
	RouteConfig route_;
	std::ofstream outFile_;
	bool done_ = false;
	bool fileOpen_ = false;
	std::string targetPath_;
	std::string boundary_;
	std::string multipartBuffer_;
	void uploadComplete(const std::string &message);
};
