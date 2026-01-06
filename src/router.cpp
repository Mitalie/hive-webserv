#include "router.hpp"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Config.hpp"
#include "IRequestHandler.hpp"
#include "IRequestManager.hpp"
#include "RequestHeader.hpp"

#include "FileRequestHandler.hpp"
#include "CgiRequestHandler.hpp"
#include "UploadRequestHandler.hpp"
#include "RedirectRequestHandler.hpp"
#include "ErrorRequestHandler.hpp"
#include "AutoindexRequestHandler.hpp"
#include "DeleteRequestHandler.hpp"

// =========================
// Helper: Strip Query String
// =========================
/**
 * Strips the query string from a path.
 * @param path The request path (may include query string).
 * @return The path without query string.
 */
std::string stripQueryString(const std::string &path)
{
	size_t pos = path.find('?');
	if (pos != std::string::npos)
		return path.substr(0, pos);
	return path;
}

// =========================
// Helper: Validate Path Within Root
// =========================
/**
 * Validates that a resolved path stays within the root directory.
 * Prevents path traversal attacks (e.g., /../../../etc/passwd).
 * @param resolvedPath The canonicalized path to check.
 * @param root The root directory that the path must stay within.
 * @return True if path is within root, false otherwise.
 */
bool isPathWithinRoot(const std::filesystem::path &resolvedPath, const std::filesystem::path &root)
{
	// Get canonical forms for comparison
	std::filesystem::path canonicalRoot = std::filesystem::weakly_canonical(root);
	std::filesystem::path canonicalPath = std::filesystem::weakly_canonical(resolvedPath);

	// Check if the resolved path starts with the root
	auto rootStr = canonicalRoot.string();
	auto pathStr = canonicalPath.string();

	if (pathStr.length() < rootStr.length())
		return false;
	if (pathStr.compare(0, rootStr.length(), rootStr) != 0)
		return false;
	// Ensure it's not just a prefix match (e.g., /var/www vs /var/www2)
	if (pathStr.length() > rootStr.length() && pathStr[rootStr.length()] != '/')
		return false;
	return true;
}

// =========================
// Helper: Method Allowed
// =========================
/**
 * Checks if the HTTP method is allowed for the given route.
 */
bool isMethodAllowed(const std::string &method, const std::vector<std::string> &allowedMethods)
{
	return std::find(allowedMethods.begin(), allowedMethods.end(), method) != allowedMethods.end();
}

// =========================
// Helper: CGI Request
// =========================
/**
 * Determines if the request path matches any configured CGI extension.
 */
bool isCgiRequest(const std::string &path, const std::map<std::string, std::string> &cgiInterpreters)
{
	// Check if path ends with any CGI extension
	for (const auto &pair : cgiInterpreters)
	{
		const std::string &ext = pair.first;
		if (path.size() >= ext.size() && path.compare(path.size() - ext.size(), ext.size(), ext) == 0)
		{
			return true;
		}
	}
	return false;
}

// =========================
// Helper: Handle Request for Route
// =========================
/**
 * Handles a request for a specific route, returning the appropriate handler.
 * Checks method, redirect, CGI, upload, and file serving in order.
 */
std::unique_ptr<IRequestHandler> handleRequestForRoute(
	IRequestManager &manager,
	const RequestHeader &header,
	const ServerConfig &server,
	const RouteConfig &route)
{
	// 1. Check if method is allowed for this route
	if (!isMethodAllowed(header.method(), route.allowedMethods))
		return std::make_unique<ErrorRequestHandler>(manager, header, server, 405); // Method Not Allowed

	// 2. Handle redirect if specified
	if (!route.redirect.empty())
		return std::make_unique<RedirectRequestHandler>(manager, header, route.redirect, route.redirectCode);

	// 3. Handle CGI if path matches a CGI extension
	// Note: CGI takes priority over uploads. A POST to a .cgi file will execute CGI,
	// even if uploadStore is configured. To upload a CGI script, use a different route.
	// Strip query string for filesystem operations (CGI handler will parse QUERY_STRING from original path)
	std::string cleanPath = stripQueryString(header.path());
	if (isCgiRequest(cleanPath, route.cgiInterpreters))
	{
		// Verify the CGI script exists before routing to CgiRequestHandler
		std::string cgiRelativePath = cleanPath.substr(route.path.length());
		if (cgiRelativePath.empty() || cgiRelativePath[0] != '/')
			cgiRelativePath = "/" + cgiRelativePath;
		std::string cgiFilePath = route.root + cgiRelativePath;
		std::filesystem::path cgiPath(cgiFilePath);

		// Validate path stays within root (prevent path traversal)
		if (!isPathWithinRoot(cgiPath, route.root))
			return std::make_unique<ErrorRequestHandler>(manager, header, server, 403); // Forbidden - path traversal attempt

		if (!std::filesystem::exists(cgiPath))
			return std::make_unique<ErrorRequestHandler>(manager, header, server, 404); // CGI script not found
		return std::make_unique<CgiRequestHandler>(manager, header, route);
	}

	// 4. Handle file upload (if POST and upload store is set)
	if (!route.uploadStore.empty() && header.method() == "POST")
		return std::make_unique<UploadRequestHandler>(manager, header, route);

	// 5. Handle DELETE method
	if (header.method() == "DELETE")
	{
		// Build and validate path for DELETE
		std::string deleteRelativePath = cleanPath.substr(route.path.length());
		if (deleteRelativePath.empty() || deleteRelativePath[0] != '/')
			deleteRelativePath = "/" + deleteRelativePath;
		std::string deleteFilePath = route.root + deleteRelativePath;
		std::filesystem::path deletePath(deleteFilePath);

		// Validate path stays within root (prevent path traversal)
		if (!isPathWithinRoot(deletePath, route.root))
			return std::make_unique<ErrorRequestHandler>(manager, header, server, 403); // Forbidden - path traversal attempt

		return std::make_unique<DeleteRequestHandler>(manager, header, route);
	}

	// 6. Default: serve static file
	// Strip the route path prefix from the request path to get the relative file path
	// Use cleanPath (query string already stripped above)
	std::string relativePath = cleanPath.substr(route.path.length());
	if (relativePath.empty() || relativePath[0] != '/')
		relativePath = "/" + relativePath;
	std::string filePath = route.root + relativePath;
	std::filesystem::path path(filePath);

	// Validate path stays within root (prevent path traversal)
	if (!isPathWithinRoot(path, route.root))
		return std::make_unique<ErrorRequestHandler>(manager, header, server, 403); // Forbidden - path traversal attempt

	// Check if path is a directory
	if (std::filesystem::exists(path) && std::filesystem::is_directory(path))
	{
		// Try to serve the index file if configured
		if (!route.index.empty())
		{
			std::filesystem::path indexPath = path / route.index;
			if (std::filesystem::exists(indexPath) && std::filesystem::is_regular_file(indexPath))
			{
				// Index file exists, update path to serve it
				path = indexPath;
				filePath = indexPath.string();
			}
			else if (route.autoindex)
			{
				// Index file doesn't exist but autoindex is enabled
				return std::make_unique<AutoindexRequestHandler>(manager, header, route);
			}
			else
			{
				// No index file and autoindex disabled
				return std::make_unique<ErrorRequestHandler>(manager, header, server, 403); // Forbidden
			}
		}
		else if (route.autoindex)
		{
			// No index configured but autoindex is enabled
			return std::make_unique<AutoindexRequestHandler>(manager, header, route);
		}
		else
		{
			// Directory request with no index and no autoindex
			return std::make_unique<ErrorRequestHandler>(manager, header, server, 403); // Forbidden
		}
	}

	if (!std::filesystem::exists(path))
	{
		// File does not exist
		return std::make_unique<ErrorRequestHandler>(manager, header, server, 404); // Not Found
	}
	if (!std::filesystem::is_regular_file(path))
	{
		// Not a regular file (e.g., symlink, device, etc.)
		return std::make_unique<ErrorRequestHandler>(manager, header, server, 403); // Forbidden
	}
	auto perms = std::filesystem::status(path).permissions();
	if ((perms & std::filesystem::perms::owner_read) == std::filesystem::perms::none &&
		(perms & std::filesystem::perms::group_read) == std::filesystem::perms::none &&
		(perms & std::filesystem::perms::others_read) == std::filesystem::perms::none)
	{
		// No read permission
		return std::make_unique<ErrorRequestHandler>(manager, header, server, 403); // Forbidden
	}
	return std::make_unique<FileRequestHandler>(manager, path.c_str());
}

// =========================
// Main Router Function
// =========================
/**
 * Main router function. Finds the matching server and route, then delegates to the appropriate handler.
 */
std::unique_ptr<IRequestHandler> router(IRequestManager &manager, const RequestHeader &header, const ServerConfig &server)
{
	// Find the matching route (by request path prefix)
	// Assumes server.routes is sorted by descending path length
	// Strip query string for route matching
	const std::string requestPath = stripQueryString(header.path());
	for (const RouteConfig &route : server.routes)
	{
		const std::string &routePath = route.path;
		if (requestPath.compare(0, routePath.size(), routePath) == 0 &&
			(requestPath.size() == routePath.size() ||
			 (requestPath.size() > routePath.size() && requestPath[routePath.size()] == '/')))
		{
			return handleRequestForRoute(manager, header, server, route);
		}
	}
	// No matching route found: return 404 handler
	return std::make_unique<ErrorRequestHandler>(manager, header, server, 404);
}
