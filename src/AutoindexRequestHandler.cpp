#include "AutoindexRequestHandler.hpp"
#include "IRequestManager.hpp"
#include "RequestHeader.hpp"
#include "Config.hpp"
#include "CallbackQueue.hpp"
#include <cstddef>
#include <span>
#include <string>
#include <sstream>
#include <filesystem>
#include <vector>
#include <algorithm>

AutoindexRequestHandler::AutoindexRequestHandler(IRequestManager& manager, const RequestHeader& header, const RouteConfig& route)
    : manager_(manager), header_(header), route_(route) {
    // Queue callback to generate directory listing from main loop
    CallbackQueue::queueCallback([this]() { generateDirectoryListing(); });
}

void AutoindexRequestHandler::onBodyData(std::span<const char> /*data*/) {
    // Ignore body data for directory listing (GET only)
}

void AutoindexRequestHandler::notifyResponseBuffer(size_t /*bufferSize*/) {
    // No buffering logic needed for directory listing
}

void AutoindexRequestHandler::generateDirectoryListing() {
    // Build the filesystem path from route root and request path
    std::string relativePath = header_.path().substr(route_.path.length());
    if (relativePath.empty() || relativePath[0] != '/')
        relativePath = "/" + relativePath;
    std::string dirPath = route_.root + relativePath;

    // Verify directory exists
    std::filesystem::path fsPath(dirPath);
    if (!std::filesystem::exists(fsPath) || !std::filesystem::is_directory(fsPath)) {
        std::string errorBody = "Directory not found";
        std::string response =
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: " + std::to_string(errorBody.size()) + "\r\n" +
            manager_.connectionHeader() + "\r\n" + errorBody;
        manager_.writeResponseData(response);
        manager_.onRequestDone();
        return;
    }

    // Generate HTML listing
    std::string htmlBody = buildHtmlListing(dirPath, header_.path());

    // Send response
    std::string response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: " + std::to_string(htmlBody.size()) + "\r\n" +
        manager_.connectionHeader() + "\r\n" + htmlBody;
    manager_.writeResponseData(response);
    manager_.onRequestDone();
}

std::string AutoindexRequestHandler::buildHtmlListing(const std::string& dirPath, const std::string& requestPath) {
    std::ostringstream html;

    // Ensure requestPath ends with / for proper link construction
    std::string basePath = requestPath;
    if (!basePath.empty() && basePath.back() != '/')
        basePath += '/';

    // HTML header
    html << "<!DOCTYPE html>\n"
         << "<html>\n"
         << "<head>\n"
         << "  <meta charset=\"utf-8\">\n"
         << "  <title>Index of " << requestPath << "</title>\n"
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
         << "  <h1>Index of " << requestPath << "</h1>\n"
         << "  <table>\n"
         << "    <thead>\n"
         << "      <tr><th>Name</th><th class=\"size\">Size</th><th>Last Modified</th></tr>\n"
         << "    </thead>\n"
         << "    <tbody>\n";

    // Parent directory link (if not at root)
    if (requestPath != "/" && requestPath != route_.path) {
        std::string parentPath = requestPath;
        if (parentPath.back() == '/')
            parentPath.pop_back();
        size_t lastSlash = parentPath.rfind('/');
        if (lastSlash != std::string::npos)
            parentPath = parentPath.substr(0, lastSlash + 1);
        else
            parentPath = "/";
        html << "      <tr><td class=\"dir\"><a href=\"" << parentPath << "\">../</a></td><td class=\"size\">-</td><td>-</td></tr>\n";
    }

    // Collect directory entries
    std::vector<std::filesystem::directory_entry> entries;
    try {
        for (const auto& entry : std::filesystem::directory_iterator(dirPath)) {
            entries.push_back(entry);
        }
    } catch (const std::filesystem::filesystem_error&) {
        // Permission denied or other error - return what we have
    }

    // Sort entries: directories first, then files, alphabetically within each group
    std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
        bool aIsDir = std::filesystem::is_directory(a.status());
        bool bIsDir = std::filesystem::is_directory(b.status());
        if (aIsDir != bIsDir)
            return aIsDir > bIsDir; // Directories first
        return a.path().filename() < b.path().filename(); // Alphabetical
    });

    // Generate table rows for each entry
    for (const auto& entry : entries) {
        std::string name = entry.path().filename().string();
        bool isDir = std::filesystem::is_directory(entry.status());
        std::string displayName = isDir ? name + "/" : name;
        std::string href = basePath + name + (isDir ? "/" : "");

        // Get file size
        std::string sizeStr = "-";
        if (!isDir) {
            try {
                auto size = std::filesystem::file_size(entry.path());
                if (size < 1024)
                    sizeStr = std::to_string(size) + " B";
                else if (size < 1024 * 1024)
                    sizeStr = std::to_string(size / 1024) + " KB";
                else if (size < 1024 * 1024 * 1024)
                    sizeStr = std::to_string(size / (1024 * 1024)) + " MB";
                else
                    sizeStr = std::to_string(size / (1024 * 1024 * 1024)) + " GB";
            } catch (...) {
                sizeStr = "-";
            }
        }

        // Get last modified time
        std::string modTime = "-";
        try {
            auto ftime = std::filesystem::last_write_time(entry.path());
            auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
            std::time_t cftime = std::chrono::system_clock::to_time_t(sctp);
            char timeBuf[64];
            std::strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M", std::localtime(&cftime));
            modTime = timeBuf;
        } catch (...) {
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
