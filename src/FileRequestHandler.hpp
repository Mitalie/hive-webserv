#pragma once

#include "IRequestHandler.hpp"
#include "IRequestManager.hpp"
#include "Header.hpp"
#include "Config.hpp"
#include <string>
#include <fstream>
#include <span>
#include <cstddef>

class FileRequestHandler : public IRequestHandler {
public:
    FileRequestHandler(IRequestManager& manager, const Header& header, const RouteConfig& route);
    virtual ~FileRequestHandler();
    void onBodyData(std::span<const char> data) override;
    void notifyResponseBuffer(size_t bufferSize) override;
private:
    IRequestManager& manager_;
    Header header_;
    RouteConfig route_;
    std::ifstream inFile_;
    std::string filePath_;
    void sendErrorResponse(int code, const std::string& message);
    void sendFile();
};
