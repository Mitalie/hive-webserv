#pragma once

#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Config.hpp"
#include "IRequestHandler.hpp"
#include "IRequestManager.hpp"
#include "RequestHeader.hpp"

// Type alias for request handler pointer
typedef std::unique_ptr<IRequestHandler> RequestHandlerPtr;

/**
 * Finds the matching route configuration for a given path and method.
 * Prioritizes routes that match both path and method.
 *
 * @param path The request path.
 * @param method The HTTP method (GET, POST, etc.).
 * @param config The server configuration.
 * @return A pointer to the matching RouteConfig, or nullptr if no match found.
 */
const RouteConfig *matchRoute(const std::string &path, const std::string &method, const ServerConfig &config);

/**
 * Construct the appropriate request handler for the request, given the matched route config.
 * @param manager The request manager.
 * @param header The HTTP request header object.
 * @param route Route configuration selected by `matchRoute`.
 * @return A pointer to the appropriate request handler.
 */
RequestHandlerPtr handleRequestForRoute(IRequestManager &manager, const RequestHeader &header, const RouteConfig &route);
