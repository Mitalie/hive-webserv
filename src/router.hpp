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
 * Main router function. Selects the appropriate server and route, then returns the correct handler.
 * @param manager The request manager.
 * @param header The HTTP request header object.
 * @param config The list of server configurations for the port.
 * @return A pointer to the appropriate request handler.
 */
RequestHandlerPtr router(IRequestManager& manager, const RequestHeader& header, const ListenerConfig& config);
