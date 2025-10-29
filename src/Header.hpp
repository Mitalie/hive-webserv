#ifndef WEBSERV_HEADER_HPP // include guard start to prevent multiple inclusion
#define WEBSERV_HEADER_HPP // define the include guard macro

#include <map>	  // std::map to store header key/value pairs
#include <string> // std::string for storing request-line parts and values
#include <string_view>
#include <vector>    // std::vector to store multiple header values

class Header
{ // Header class holds parsed HTTP header information
private:
	std::string method_;						 // stores the HTTP method (e.g., GET, POST)
	std::string path_;							 // stores the request path/URI
	std::string version_;						 // stores the HTTP version string (e.g., HTTP/1.1)
	std::map<std::string, std::vector<std::string>> headers_; // map of header key->list of values

public:
	// Construct and parse from raw HTTP request header text (request-line + header fields).
	// Throws std::runtime_error on parse failure.
	Header(const std::string_view &raw); // parse in constructor and throw on error

	// Note: parsing is done in the constructor `Header(const std::string&)` which throws on error.

	// Accessors
	const std::string &method() const;	// returns stored method_
	const std::string &path() const;	// returns stored path_
	const std::string &version() const; // returns stored version_
	// Returns empty string if header not found
	std::string get(const std::string &key) const;		   // lookup a header by key (case-insensitive)
	const std::map<std::string, std::vector<std::string>> &all() const; // return const-ref to all headers
};

#endif // WEBSERV_HEADER_HPP // include guard end
