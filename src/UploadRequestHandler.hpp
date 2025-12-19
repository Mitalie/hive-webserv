#pragma once

#include "IRequestHandler.hpp"
#include "IRequestManager.hpp"
#include "RequestHeader.hpp"
#include "Config.hpp"
#include <string>
#include <span>
#include <cstddef>
#include <fstream>

class UploadRequestHandler : public IRequestHandler {
public:
    UploadRequestHandler(IRequestManager& manager, const RequestHeader& header, const RouteConfig& route);
    virtual ~UploadRequestHandler();
    void onBodyData(std::span<const char> data) override;
    void notifyResponseBuffer(size_t bufferSize) override;
private:
    IRequestManager& manager_;
    RequestHeader header_;
    RouteConfig route_;
    std::ofstream outFile_;
    bool done_ = false;
    bool fileOpen_ = false;
    std::string targetPath_;
    std::string boundary_;
    std::string multipartBuffer_;
    void sendResponse(int code, const std::string& message);
};
