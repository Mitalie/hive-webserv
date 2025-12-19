#pragma once

#include "IRequestHandler.hpp"
#include "IRequestManager.hpp"
#include "RequestHeader.hpp"
#include "Config.hpp"
#include <string>
#include <span>
#include <cstddef>

/**
 * Handler for HTTP DELETE requests.
 * Deletes a file from the server's filesystem within the configured route root.
 */
class DeleteRequestHandler : public IRequestHandler {
public:
    DeleteRequestHandler(IRequestManager& manager, const RequestHeader& header, const RouteConfig& route);
    virtual ~DeleteRequestHandler() = default;
    void onBodyData(std::span<const char> data) override;
    void notifyResponseBuffer(size_t bufferSize) override;

private:
    IRequestManager& manager_;
    RequestHeader header_;
    RouteConfig route_;
    std::string filePath_;

    void sendResponse(int code, const std::string& statusText, const std::string& message);
    void deleteFile();
};
