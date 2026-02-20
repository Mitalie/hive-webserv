#include "AutoindexRequestHandler.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <ctime>
#include <filesystem>
#include <span>
#include <sstream>
#include <string>
#include <vector>

#include "IRequestManager.hpp"

AutoindexRequestHandler::AutoindexRequestHandler(
	IRequestManager &manager,
	const std::filesystem::path &dir,
	const std::string &dirName,
	bool allowParentDirLink)
	: manager_(manager), dir_(dir), dirName_(dirName), allowParentDirLink_(allowParentDirLink)
{
	generateDirectoryListing();
}

void AutoindexRequestHandler::onBodyData(std::span<const char> /*data*/)
{
	// Ignore body data for directory listing (GET only)
}

void AutoindexRequestHandler::onBodyDone()
{
	// No body to handle for directory listing
}

void AutoindexRequestHandler::notifyResponseBuffer(size_t /*bufferSize*/)
{
	// No buffering logic needed for directory listing
}

void AutoindexRequestHandler::generateDirectoryListing()
{
	// Verify directory exists
	if (!std::filesystem::is_directory(dir_))
	{
		manager_.onRequestError(404);
		return;
	}

	// Generate HTML listing
	std::string htmlBody = buildHtmlListing();

	// Send response
	std::string response =
		"HTTP/1.1 200 OK\r\n"
		"Content-Type: text/html; charset=utf-8\r\n"
		"Content-Length: " +
		std::to_string(htmlBody.size()) + "\r\n" +
		std::string(manager_.connectionHeader()) +
		"\r\n" +
		htmlBody;

	manager_.writeResponseData(response);
	manager_.onRequestDone();
}

std::string AutoindexRequestHandler::buildHtmlListing()
{
	std::ostringstream html;

	// HTML header
	html << "<!DOCTYPE html>\n"
		 << "<html>\n"
		 << "<head>\n"
		 << "  <meta charset=\"utf-8\">\n"
		 << "  <title>Index of " << dirName_ << "</title>\n"
		 << "  <style>\n"
		 << "    body { font-family: monospace; margin: 20px; }\n"
		 << "    h1 { border-bottom: 1px solid #ccc; padding-bottom: 10px; }\n"
		 << "    table { border-collapse: collapse; width: 100%; max-width: 800px; }\n"
		 << "    th, td { text-align: left; padding: 8px 12px; }\n"
		 << "    th { background-color: #f0f0f0; }\n"
		 << "    tr:hover { background-color: #f5f5f5; }\n"
		 << "    a { text-decoration: none; color: #0066cc; }\n"
		 << "    a:hover { text-decoration: underline; }\n"
		 << "    .size { text-align: right; }\n"
		 << "    .dir { color: #0066cc; font-weight: bold; }\n"
		 << "  </style>\n"
		 << "</head>\n"
		 << "<body>\n"
		 << "  <h1>Index of " << dirName_ << "</h1>\n"
		 << "  <table>\n"
		 << "    <thead>\n"
		 << "      <tr><th>Name</th><th class=\"size\">Size</th><th>Last Modified</th></tr>\n"
		 << "    </thead>\n"
		 << "    <tbody>\n";

	// Parent directory link (if not at root)
	if (allowParentDirLink_)
		html << "      <tr><td class=\"dir\"><a href=\"..\">../</a></td><td class=\"size\">-</td><td>-</td></tr>\n";

	// Collect directory entries
	std::vector<std::filesystem::directory_entry> entries;
	try
	{
		for (const auto &entry : std::filesystem::directory_iterator(dir_))
		{
			entries.push_back(entry);
		}
	}
	catch (const std::filesystem::filesystem_error &)
	{
		// Permission denied or other error - return what we have
	}

	// Sort entries: directories first, then files, alphabetically within each group
	std::sort(entries.begin(), entries.end(), [](const auto &a, const auto &b)
			  {
				  bool aIsDir = std::filesystem::is_directory(a.status());
				  bool bIsDir = std::filesystem::is_directory(b.status());
				  if (aIsDir != bIsDir)
					  return aIsDir > bIsDir;						// Directories first
				  return a.path().filename() < b.path().filename(); // Alphabetical
			  });

	// Generate table rows for each entry
	for (const auto &entry : entries)
	{
		std::string name = entry.path().filename().string();
		bool isDir = std::filesystem::is_directory(entry.status());
		std::string displayName = isDir ? name + "/" : name;
		std::string href = "./" + name + (isDir ? "/" : "");

		// Get file size
		std::string sizeStr = "-";
		if (!isDir)
		{
			try
			{
				auto size = std::filesystem::file_size(entry.path());
				if (size < 1024)
					sizeStr = std::to_string(size) + " B";
				else if (size < 1024 * 1024)
					sizeStr = std::to_string(size / 1024) + " KB";
				else if (size < 1024 * 1024 * 1024)
					sizeStr = std::to_string(size / (1024 * 1024)) + " MB";
				else
					sizeStr = std::to_string(size / (1024 * 1024 * 1024)) + " GB";
			}
			catch (...)
			{
				sizeStr = "-";
			}
		}

		// Get last modified time
		std::string modTime = "-";
		try
		{
			auto ftime = std::filesystem::last_write_time(entry.path());
			// auto sctp = std::chrono::clock_cast<std::chrono::system_clock>(ftime);
			// C++ standard library on school computers is missing std::chrono::clock_cast.
			// Standard requires file_clock to have either to_sys or to_utc; to_sys seems to work.
			auto sctp = std::chrono::file_clock::to_sys(ftime);
			std::time_t cftime = std::chrono::system_clock::to_time_t(sctp);
			char timeBuf[64];
			std::strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M", std::localtime(&cftime));
			modTime = timeBuf;
		}
		catch (...)
		{
			modTime = "-";
		}

		// Output row
		std::string cssClass = isDir ? " class=\"dir\"" : "";
		html << "      <tr><td" << cssClass << "><a href=\"" << href << "\">" << displayName << "</a></td>"
			 << "<td class=\"size\">" << sizeStr << "</td>"
			 << "<td>" << modTime << "</td></tr>\n";
	}

	// HTML footer
	html << "    </tbody>\n"
		 << "  </table>\n"
		 << "  <hr>\n"
		 << "  <p><em>webserv</em></p>\n"
		 << "</body>\n"
		 << "</html>\n";

	return html.str();
}
