#include "RedirectRequestHandler.hpp"
#include "IRequestManager.hpp"
#include "RequestHeader.hpp"
#include <span>
#include <cstddef>
#include <string>


RedirectRequestHandler::RedirectRequestHandler(IRequestManager& manager, const RequestHeader& header, const std::string& location, int code)
    : manager_(manager), header_(header), location_(location), code_(code) {
    // Do all work in constructor - send redirect response immediately
    
    // Build HTTP status line
    std::string statusText;
    switch (code_) {
        case 301: statusText = "Moved Permanently"; break;
        case 302: statusText = "Found"; break;
        case 303: statusText = "See Other"; break;
        case 307: statusText = "Temporary Redirect"; break;
        case 308: statusText = "Permanent Redirect"; break;
        default: statusText = "Redirect"; break;
    }
    std::string response =
        "HTTP/1.1 " + std::to_string(code_) + " " + statusText + "\r\n"
        "Location: " + location_ + "\r\n"
        "Content-Type: text/html; charset=UTF-8\r\n";

    // Minimal HTML body for user agents
    std::string body =
        "<html><head><title>" + std::to_string(code_) + " " + statusText + "</title></head>"
        "<body><h1>" + std::to_string(code_) + " " + statusText + "</h1>"
        "<p>Redirecting to <a href=\"" + location_ + "\">" + location_ + "</a></p>"
        "</body></html>";

    response += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    response += "Connection: close\r\n\r\n";
    response += body;

    manager_.writeResponseData(response);
    manager_.onRequestDone();
}

RedirectRequestHandler::~RedirectRequestHandler() {}

void RedirectRequestHandler::onBodyData(std::span<const char> /*data*/) {
    // Ignore body data for redirects
}

void RedirectRequestHandler::notifyResponseBuffer(size_t /*bufferSize*/) {
    // No buffering logic needed for simple redirect
}
