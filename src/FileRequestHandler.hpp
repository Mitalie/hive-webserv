#pragma once

#include <cstddef>
#include <span>
#include <string>

#include "IRequestHandler.hpp"
#include "IRequestManager.hpp"
#include "UnixFD.hpp"

class FileRequestHandler : public IRequestHandler
{
public:
	// Router is expected to map and validate file path already, so we take it in the form we need for syscalls
	FileRequestHandler(IRequestManager &manager, const char *filePath);
	~FileRequestHandler() = default;
	void onBodyData(std::span<const char> data) override;
	void onBodyDone() override;
	void notifyResponseBuffer(size_t bufferSize) override;

private:
	IRequestManager &manager_;
	size_t bytesRemaining_;
	UnixFD fd_;

	void sendErrorResponse(int code, const std::string &message);
	void start(const char *filePath);
	void sendData();

	static constexpr size_t CHUNK_SIZE = 16384;	   // 16 KiB
	static constexpr size_t BUFFER_LIMIT = 262144; // 256 KiB
};
