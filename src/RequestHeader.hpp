#pragma once

#include <cstddef>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "HeaderFields.hpp"

/*
	RequestHeader class holds parsed HTTP request header information.

	Contains:
	- Request-line fields: method, path, version
	- Header fields: stored in a public HeaderFields object

	For parsing CGI response headers (which have no request-line),
	use HeaderFields directly instead.
*/
class RequestHeader
{
public:
	// Exception thrown for invalid header values
	class InvalidHeader : public std::runtime_error
	{
	public:
		int statusCode;
		InvalidHeader(int code, const std::string &msg)
			: std::runtime_error(msg), statusCode(code) {}
	};

	// Header fields are public for direct access
	HeaderFields fields;

	// Construct and parse from raw HTTP request header text (request-line + header fields).
	// Throws std::runtime_error on parse failure.
	RequestHeader(const std::string_view &raw);

	// Accessors for request-line
	const std::string &method() const;
	const std::string &path() const;
	const std::string &version() const;

	// Convenience accessors that delegate to fields
	std::string get(const std::string &key) const;
	const std::map<std::string, std::vector<std::string>> &all() const;

	// --- Parsed header accessors with validation ---

	// Get Content-Length as a parsed size_t.
	// Returns std::nullopt if header is not present.
	// Throws InvalidHeader(400) if value is invalid (not a valid number, negative, overflow).
	std::optional<size_t> getContentLength() const;

	// Check if Transfer-Encoding includes "chunked" as the final encoding.
	// Throws InvalidHeader(501) if unsupported transfer encodings are present.
	bool hasChunkedBody() const;

	// Get the list of transfer encodings (parsed from comma/header-separated list).
	// Returns empty vector if header is not present.
	std::vector<std::string> getTransferEncodings() const;

	// Check if this request method typically has a body.
	// Returns true for POST, PUT, PATCH.
	bool methodExpectsBody() const;

	// Check if this request method should NOT have a body.
	// Returns true for GET, HEAD, DELETE, OPTIONS, TRACE.
	bool methodForbidsBody() const;

	// --- Utility for comma-separated header values ---

	// Parse a header value as a comma-separated list of items.
	// Handles both multiple header lines and comma-separated values within a line.
	// Trims whitespace from each item.
	static std::vector<std::string> parseList(const std::string &headerValue);

private:
	std::string method_;  // stores the HTTP method (e.g., GET, POST)
	std::string path_;	  // stores the request path/URI
	std::string version_; // stores the HTTP version string (e.g., HTTP/1.1)
};
