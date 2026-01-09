#pragma once

#include <cstddef>
#include <filesystem>
#include <span>
#include <string>

#include "Config.hpp"
#include "IRequestHandler.hpp"
#include "IRequestManager.hpp"
#include "RequestHeader.hpp"

/**
 * Request handler that generates an HTML directory listing (autoindex).
 * Used when a directory is requested, no index file exists, and autoindex is enabled.
 */
class AutoindexRequestHandler : public IRequestHandler
{
public:
	AutoindexRequestHandler(
		IRequestManager &manager,
		const std::filesystem::path &dir,
		const std::string &dirName,
		bool allowParentDirLink);
	~AutoindexRequestHandler() = default;
	void onBodyData(std::span<const char> data) override;
	void onBodyDone() override;
	void notifyResponseBuffer(size_t bufferSize) override;

private:
	IRequestManager &manager_;
	const std::filesystem::path &dir_;
	const std::string &dirName_;
	bool allowParentDirLink_;
	void generateDirectoryListing();
	std::string buildHtmlListing();
};
