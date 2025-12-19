#pragma once

#include "IRequestHandler.hpp"
#include "IRequestManager.hpp"
#include "Header.hpp"
#include <string>
#include <span>
#include <cstddef>

class ErrorRequestHandler : public IRequestHandler {
public:
    ErrorRequestHandler(IRequestManager& manager, const Header& header, int code);
    virtual ~ErrorRequestHandler();
    void onBodyData(std::span<const char> data) override;
    void notifyResponseBuffer(size_t bufferSize) override;
private:
    IRequestManager& manager_;
    Header header_;
    int code_;
    void sendResponse();
};
