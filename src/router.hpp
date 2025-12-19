#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include <map>

#include "Config.hpp"
#include "IRequestHandler.hpp"
#include "IRequestManager.hpp"
#include "RequestHeader.hpp"

// Type alias for request handler pointer
typedef std::unique_ptr<IRequestHandler> RequestHandlerPtr;

/**
 * Main router function. Selects the appropriate route, then returns the correct handler.
 * @param manager The request manager.
 * @param header The HTTP request header object.
 * @param config The server configuration for the current request.
 * @return A pointer to the appropriate request handler.
 */
RequestHandlerPtr router(IRequestManager &manager, const RequestHeader &header, const ServerConfig &config);
