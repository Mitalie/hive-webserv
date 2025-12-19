
#include "ErrorRequestHandler.hpp"
#include "IRequestManager.hpp"
#include "RequestHeader.hpp"
#include <span>
#include <cstddef>
#include <string>

ErrorRequestHandler::ErrorRequestHandler(IRequestManager& manager, const RequestHeader& header, int code)
    : manager_(manager), header_(header), code_(code) {
    // Do all work in constructor - send error response immediately
    sendResponse();
    manager_.onRequestDone();
}

ErrorRequestHandler::~ErrorRequestHandler() {}

void ErrorRequestHandler::onBodyData(std::span<const char> /*data*/) {
    // Ignore body data for errors
}

void ErrorRequestHandler::notifyResponseBuffer(size_t /*bufferSize*/) {
    // No buffering logic needed for error
}

void ErrorRequestHandler::sendResponse() {
    std::string statusText;
    switch (code_) {
        case 400: statusText = "Bad Request"; break;
        case 403: statusText = "Forbidden"; break;
        case 404: statusText = "Not Found"; break;
        case 405: statusText = "Method Not Allowed"; break;
        case 500: statusText = "Internal Server Error"; break;
        default: statusText = "Error"; break;
    }
    std::string body = "<html><head><title>" + std::to_string(code_) + " " + statusText + "</title></head>"
        "<body><h1>" + std::to_string(code_) + " " + statusText + "</h1></body></html>";
    std::string response =
        "HTTP/1.1 " + std::to_string(code_) + " " + statusText + "\r\n"
        "Content-Type: text/html; charset=UTF-8\r\n"
        "Content-Length: " + std::to_string(body.size()) + "\r\n"
        "Connection: close\r\n\r\n" + body;
    manager_.writeResponseData(response);
}
