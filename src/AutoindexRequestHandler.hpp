#pragma once

#include "IRequestHandler.hpp"
#include "IRequestManager.hpp"
#include "RequestHeader.hpp"
#include "Config.hpp"
#include <string>
#include <span>
#include <cstddef>

/**
 * Request handler that generates an HTML directory listing (autoindex).
 * Used when a directory is requested, no index file exists, and autoindex is enabled.
 */
class AutoindexRequestHandler : public IRequestHandler {
public:
    AutoindexRequestHandler(IRequestManager& manager, const RequestHeader& header, const RouteConfig& route);
    virtual ~AutoindexRequestHandler() = default;
    void onBodyData(std::span<const char> data) override;
    void notifyResponseBuffer(size_t bufferSize) override;
private:
    IRequestManager& manager_;
    RequestHeader header_;
    RouteConfig route_;
    void generateDirectoryListing();
    std::string buildHtmlListing(const std::string& dirPath, const std::string& requestPath);
};
