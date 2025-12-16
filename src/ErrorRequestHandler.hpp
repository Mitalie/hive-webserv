#pragma once

#include "IRequestHandler.hpp"
#include "IRequestManager.hpp"
#include "RequestHeader.hpp"
#include <string>
#include <span>
#include <cstddef>

class ErrorRequestHandler : public IRequestHandler {
public:
    ErrorRequestHandler(IRequestManager& manager, const RequestHeader& header, int code);
    virtual ~ErrorRequestHandler();
    void onBodyData(std::span<const char> data) override;
    void notifyResponseBuffer(size_t bufferSize) override;
private:
    IRequestManager& manager_;
    RequestHeader header_;
    int code_;
    void sendResponse();
};
