
#include "ErrorRequestHandler.hpp"

#include <span>
#include <cstddef>
#include <string>
#include <fstream>
#include <sstream>
#include <filesystem>

#include "Config.hpp"
#include "IRequestManager.hpp"
#include "RequestHeader.hpp"

// Constructor without custom error pages (backward compatibility)
ErrorRequestHandler::ErrorRequestHandler(
	IRequestManager& manager,
	const RequestHeader& header,
	const ServerConfig& config,
	int code)
	: manager_(manager), header_(header), config_(config), code_(code) {
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

std::string ErrorRequestHandler::getStatusText() const {
	switch (code_) {
	case 400: return "Bad Request";
	case 403: return "Forbidden";
	case 404: return "Not Found";
	case 405: return "Method Not Allowed";
	case 413: return "Payload Too Large";
	case 500: return "Internal Server Error";
	case 502: return "Bad Gateway";
	case 503: return "Service Unavailable";
	case 504: return "Gateway Timeout";
	default: return "Error";
	}
}

bool ErrorRequestHandler::tryServeCustomErrorPage() {
	// Check if we have a custom error page for this code
	auto it = config_.errorPages.find(code_);
	if (it == config_.errorPages.end()) {
		return false;
	}

	const std::string& errorPagePath = it->second;

	// Check if the file exists and is readable
	std::filesystem::path path(errorPagePath);
	if (!std::filesystem::exists(path) || !std::filesystem::is_regular_file(path)) {
		return false;
	}

	// Try to read the file
	std::ifstream file(errorPagePath, std::ifstream::binary);
	if (!file.is_open()) {
		return false;
	}

	std::ostringstream buffer;
	buffer << file.rdbuf();
	std::string body = buffer.str();
	file.close();

	std::string statusText = getStatusText();
	std::string response =
		"HTTP/1.1 " + std::to_string(code_) + " " + statusText + "\r\n"
		"Content-Type: text/html; charset=UTF-8\r\n"
		"Content-Length: " + std::to_string(body.size()) + "\r\n"
		"Connection: close\r\n\r\n" + body;
	manager_.writeResponseData(response);
	return true;
}

void ErrorRequestHandler::sendDefaultResponse(const std::string& statusText) {
	std::string body = "<html><head><title>" + std::to_string(code_) + " " + statusText + "</title></head>"
		"<body><h1>" + std::to_string(code_) + " " + statusText + "</h1></body></html>";
	std::string response =
		"HTTP/1.1 " + std::to_string(code_) + " " + statusText + "\r\n"
		"Content-Type: text/html; charset=UTF-8\r\n"
		"Content-Length: " + std::to_string(body.size()) + "\r\n"
		"Connection: close\r\n\r\n" + body;
	manager_.writeResponseData(response);
}

void ErrorRequestHandler::sendResponse() {
	// Try to serve custom error page first
	if (tryServeCustomErrorPage()) {
		return;
	}
	// Fall back to default HTML response
	sendDefaultResponse(getStatusText());
}
