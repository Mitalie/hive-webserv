#pragma once

#include "IRequestHandler.hpp"
#include "IRequestManager.hpp"
#include "Header.hpp"
#include <string>

class RedirectRequestHandler : public IRequestHandler {
public:
    RedirectRequestHandler(IRequestManager& manager, const Header& header, const std::string& location, int code);
    virtual ~RedirectRequestHandler();
	// IRequestHandler interface
    void onBodyData(std::span<const char> data) override;
    void notifyResponseBuffer(size_t bufferSize) override;
private:
    IRequestManager& manager_;
    Header header_;
    std::string location_;
    int code_;
};
