
#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include <map>

#include "IRequestHandler.hpp"
#include "IRequestManager.hpp"
#include "Header.hpp"
#include "Config.hpp"

// Forward declarations for handler classes
class FileRequestHandler;
class CgiRequestHandler;
class UploadRequestHandler;
class RedirectRequestHandler;
class ErrorRequestHandler;
class DeleteRequestHandler;

// Type alias for request handler pointer
typedef std::unique_ptr<IRequestHandler> RequestHandlerPtr;

// =========================
// Router Helper Functions
// =========================

/**
 * Strips the query string from a request path.
 * @param path The request path (may include ?query=string).
 * @return The path without query string.
 */
std::string stripQueryString(const std::string& path);

/**
 * Validates that a resolved path stays within the root directory.
 * Prevents path traversal attacks (e.g., /../../../etc/passwd).
 * @param resolvedPath The path to check.
 * @param root The root directory that the path must stay within.
 * @return True if path is within root, false otherwise.
 */
bool isPathWithinRoot(const std::filesystem::path& resolvedPath, const std::filesystem::path& root);

/**
 * Selects the appropriate ServerConfig for a request based on the Host header.
 * If no match is found, returns the first server as default.
 * @param header The HTTP request header object.
 * @param servers The list of server configurations for a port.
 * @return Pointer to matching ServerConfig, or nullptr if servers is empty.
 */
const ServerConfig* findServerConfig(const Header& header, const ListenerConfig& servers);

/**
 * Checks if the HTTP method is allowed for the given route.
 * @param method The HTTP method (e.g., "GET", "POST").
 * @param allowedMethods The list of allowed methods for the route.
 * @return True if allowed, false otherwise.
 */
bool isMethodAllowed(const std::string& method, const std::vector<std::string>& allowedMethods);

/**
 * Determines if the request path matches any configured CGI extension.
 * @param path The request path.
 * @param cgiInterpreters Map of CGI extensions to interpreters.
 * @return True if the path matches a CGI extension, false otherwise.
 */
bool isCgiRequest(const std::string& path, const std::map<std::string, std::string>& cgiInterpreters);

// =========================
// Main Router Function
// =========================

/**
 * Main router function. Selects the appropriate server and route, then returns the correct handler.
 * @param manager The request manager.
 * @param header The HTTP request header object.
 * @param config The list of server configurations for the port.
 * @return A pointer to the appropriate request handler.
 */
RequestHandlerPtr router(IRequestManager& manager, const Header& header, const ListenerConfig& config);

// (Optional) Overload for direct route handling. Not implemented.
// RequestHandlerPtr router(IRequestManager& manager, const Header& header, const RouteConfig& route);
