#include "router.hpp"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <sstream>
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
// Helper: Resolve Route Path
// =========================
/**
 * Converts the incoming request path (URL) into a filesystem path
 * based on the route configuration.
 */
std::filesystem::path resolveRoutePath(const std::string &urlPath, const RouteConfig &route)
{
	std::string relativePath = urlPath.substr(route.path.length());
	if (relativePath.empty() || relativePath[0] != '/')
		relativePath = "/" + relativePath;
	return std::filesystem::path(route.root) / relativePath.substr(1);
}

// Helper: Check if file has CGI extension
bool hasCgiExtension(const std::filesystem::path &path, const std::map<std::string, std::string> &interpreters)
{
	std::string extension = path.extension().string();
	return interpreters.find(extension) != interpreters.end();
}

// =========================
// Helper: Handle Request On Path
// =========================
/**
 * Handles the actual request logic (Upload, Delete, Static, Autoindex)
 * on a specific filesystem path.
 */
std::unique_ptr<IRequestHandler> handleRequestOnPath(
	IRequestManager &manager,
	const RequestHeader &header,
	const ServerConfig &server,
	const RouteConfig &route,
	std::filesystem::path path,
	const std::string &urlPath)
{
	// 4. Handle file upload (if POST and upload store is set)
	if (!route.uploadStore.empty() && header.method() == "POST")
		return std::make_unique<UploadRequestHandler>(manager, header, route);

	// 5. Handle DELETE method
	if (header.method() == "DELETE")
		return std::make_unique<DeleteRequestHandler>(manager, path.c_str());

	// 6. Default: serve static file
	bool isRoot = (path == std::filesystem::path(route.root));

	// Check if path is a directory
	if (std::filesystem::exists(path) && std::filesystem::is_directory(path))
	{
		if (urlPath.back() != '/')
			return std::make_unique<RedirectRequestHandler>(manager, header, urlPath + '/', 301);

		// Try to serve the index file if configured
		if (!route.index.empty())
		{
			std::filesystem::path indexPath = path / route.index;
			if (std::filesystem::exists(indexPath) && std::filesystem::is_regular_file(indexPath))
				// Index file exists, update path to serve it
				path = indexPath;
			else if (route.autoindex)
				// Index file doesn't exist but autoindex is enabled
				return std::make_unique<AutoindexRequestHandler>(manager, path, header.path(), !isRoot);
			else
				// No index file and autoindex disabled
				return std::make_unique<ErrorRequestHandler>(manager, header, server, 404); // Forbidden
		}
		else if (route.autoindex)
			// No index configured but autoindex is enabled
			return std::make_unique<AutoindexRequestHandler>(manager, path, header.path(), !isRoot);
		else
			// Directory request with no index and no autoindex
			return std::make_unique<ErrorRequestHandler>(manager, header, server, 403); // Forbidden
	}

	// Check for CGI match (after directory logic, so index.php is caught)
	if (hasCgiExtension(path, route.cgiInterpreters))
	{
		return std::make_unique<CgiRequestHandler>(manager, header, route, path.string()); // Forbidden
	}

	// 7. Final File Checks
	if (!std::filesystem::exists(path) || !std::filesystem::is_regular_file(path))
		return std::make_unique<ErrorRequestHandler>(manager, header, server, 404);

	auto perms = std::filesystem::status(path).permissions();
	if ((perms & std::filesystem::perms::owner_read) == std::filesystem::perms::none)
		// No read permission
		return std::make_unique<ErrorRequestHandler>(manager, header, server, 403); // Forbidden

	return std::make_unique<FileRequestHandler>(manager, path.c_str());
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
	std::string urlPath = stripQueryString(header.path());

	// 1. Check if method is allowed for this route
	if (!isMethodAllowed(header.method(), route.allowedMethods))
		return std::make_unique<ErrorRequestHandler>(manager, header, server, 405); // Method Not Allowed

	// 2. Handle redirect if specified
	if (!route.redirect.empty())
		return std::make_unique<RedirectRequestHandler>(manager, header, route.redirect, route.redirectCode);

	// 3. Resolve Path & Check CGI (Filesystem Traversal)
	std::filesystem::path current = route.root;
	std::string relativeSuffix = urlPath.substr(route.path.length());
	if (relativeSuffix.empty() || relativeSuffix[0] != '/')
		relativeSuffix = "/" + relativeSuffix;

	std::stringstream ss(relativeSuffix);
	std::string segment;
	while (true)
	{
		if (!isPathWithinRoot(current, route.root))
			return std::make_unique<ErrorRequestHandler>(manager, header, server, 403);

		// Check for CGI match (Priority over existence for "Allow Nonexistent")
		if (hasCgiExtension(current, route.cgiInterpreters))
		{
			// If it exists as a directory, we must continue traversing (it's not the script)
			if (std::filesystem::exists(current) && std::filesystem::is_directory(current))
				continue;

			// Otherwise (File exists, or File doesn't exist), treat as CGI script
			// TODO: pass remaining segments to CGI script as PATH_INFO
			return std::make_unique<CgiRequestHandler>(manager, header, route, current.string());
		}
		if (!std::getline(ss, segment, '/'))
			break;
		if (segment.empty())
			continue;
		current /= segment;
	}

	// Fallback: If traversal finished without returning, use the full resolved path.
	std::filesystem::path fullPath = resolveRoutePath(urlPath, route);
	return handleRequestOnPath(manager, header, server, route, fullPath, urlPath);
}

// =========================
// Route Matching Logic
// =========================
const RouteConfig *matchRoute(const std::string &path, const std::string &method, const ServerConfig &server)
{
	const std::string requestPath = stripQueryString(path);
	const RouteConfig *candidate = nullptr;

	// Assumes server.routes is sorted by descending path length
	for (const RouteConfig &route : server.routes)
	{
		const std::string &routePath = route.path;

		// 1. Check Prefix Match
		if (requestPath.compare(0, routePath.size(), routePath) != 0)
			continue;

		// 2. Check Directory Boundary (e.g., prevent /foo matching /foobar)
		if (requestPath.size() != routePath.size() &&
			(!route.isDirectoryRoute || requestPath[routePath.size()] != '/'))
			continue;

		// 3. Check "Best Match" logic

		// If we already have a candidate, and this new match is shorter (less specific),
		// we stop looking. The previous candidate is the best we will find (even if method didn't match).
		if (candidate && routePath.size() < candidate->path.size())
			return candidate;

		// If method matches, this is the perfect route. Return immediately.
		if (isMethodAllowed(method, route.allowedMethods))
			return &route;

		// If method didn't match, store as a candidate (fallback).
		// We keep looking in case there is another route with the SAME path length
		// that DOES support the method.
		if (!candidate)
			candidate = &route;
	}

	return candidate;
}

// =========================
// Main Router Function
// =========================
std::unique_ptr<IRequestHandler> router(IRequestManager &manager, const RequestHeader &header, const ServerConfig &server, const RouteConfig &route)
{
	return handleRequestForRoute(manager, header, server, route);
}
