
#include "DeleteRequestHandler.hpp"
#include "IRequestManager.hpp"
#include "RequestHeader.hpp"
#include "Config.hpp"
#include "CallbackQueue.hpp"
#include <filesystem>
#include <string>

DeleteRequestHandler::DeleteRequestHandler(IRequestManager& manager, const RequestHeader& header, const RouteConfig& route)
    : manager_(manager), header_(header), route_(route)
{
    // Build the file path from route root and request path
    // Strip the route path prefix to get relative path
    std::string requestPath = header_.path();
    // Strip query string if present
    size_t queryPos = requestPath.find('?');
    if (queryPos != std::string::npos)
        requestPath = requestPath.substr(0, queryPos);
    
    std::string relativePath = requestPath.substr(route_.path.length());
    if (relativePath.empty() || relativePath[0] != '/')
        relativePath = "/" + relativePath;
    filePath_ = route_.root + relativePath;

    // Queue callback to do delete work from main loop
    CallbackQueue::queueCallback([this]() { deleteFile(); });
}

void DeleteRequestHandler::onBodyData(std::span<const char> /*data*/)
{
    // DELETE requests typically don't have a body, ignore any data
}

void DeleteRequestHandler::notifyResponseBuffer(size_t /*bufferSize*/)
{
    // No buffering logic needed for DELETE
}

void DeleteRequestHandler::sendResponse(int code, const std::string& statusText, const std::string& message)
{
    std::string response =
        "HTTP/1.1 " + std::to_string(code) + " " + statusText + "\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: " + std::to_string(message.size()) + "\r\n"
        "Connection: close\r\n\r\n" + message;
    manager_.writeResponseData(response);
    manager_.onRequestDone();
}

void DeleteRequestHandler::deleteFile()
{
    std::filesystem::path path(filePath_);

    // Check if file exists
    if (!std::filesystem::exists(path)) {
        sendResponse(404, "Not Found", "File not found");
        return;
    }

    // Check if it's a regular file (don't delete directories)
    if (!std::filesystem::is_regular_file(path)) {
        sendResponse(403, "Forbidden", "Cannot delete: not a regular file");
        return;
    }

    // Attempt to delete the file
    std::error_code ec;
    if (!std::filesystem::remove(path, ec)) {
        sendResponse(500, "Internal Server Error", "Failed to delete file: " + ec.message());
        return;
    }

    // Success - return 200 OK with confirmation message
    // (204 No Content is also valid, but 200 with body is more informative)
    sendResponse(200, "OK", "File deleted successfully");
}
