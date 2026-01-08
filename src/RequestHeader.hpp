#ifndef WEBSERV_REQUESTHEADER_HPP
#define WEBSERV_REQUESTHEADER_HPP

#include <map>
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

private:
	std::string method_;  // stores the HTTP method (e.g., GET, POST)
	std::string path_;	  // stores the request path/URI
	std::string version_; // stores the HTTP version string (e.g., HTTP/1.1)
};

#endif // WEBSERV_REQUESTHEADER_HPP
