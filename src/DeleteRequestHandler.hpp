#pragma once

#include <cstddef>
#include <span>
#include <string>

#include "IRequestHandler.hpp"
#include "IRequestManager.hpp"

/**
 * Handler for HTTP DELETE requests.
 * Deletes a file from the server's filesystem within the configured route root.
 */
class DeleteRequestHandler : public IRequestHandler
{
public:
	DeleteRequestHandler(IRequestManager &manager, const char *filePath);
	~DeleteRequestHandler() = default;
	void onBodyData(std::span<const char> data) override;
	void onBodyDone() override;
	void notifyResponseBuffer(size_t bufferSize) override;

private:
	void sendResponse(IRequestManager &manager, int code, const std::string &statusText, const std::string &message);
};
