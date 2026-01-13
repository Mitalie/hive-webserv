#pragma once

#include <map>
#include <string>
#include <string_view>
#include <vector>

/*
	HeaderFields stores and parses HTTP header key-value pairs.

	This class is designed to be usable both:
	- As a child of RequestHeader for parsing HTTP request headers
	- Standalone for parsing CGI response headers (which have no request-line)

	Keys are stored in lowercase for case-insensitive lookup.
	Multiple values for the same key are stored as a vector and combined
	with ", " when retrieved via get().
*/
class HeaderFields
{
public:
	// Default constructor - creates empty header fields
	HeaderFields() = default;

	// Parse header fields from a string_view (does not expect request-line)
	// Each line should be in "Key: Value" format, separated by \r\n
	void parse(std::string_view data);

	// Get a header value by key (case-insensitive)
	// Returns empty string if not found
	// Multiple values are concatenated with ", "
	std::string get(const std::string &key) const;

	// Get individual comma-separated fields of a header value by key (case-insensitive)
	// Note that this does not support any kind of escaping or quoting of commas
	std::vector<std::string_view> getList(const std::string &key) const;

	// Check if a header exists
	bool has(const std::string &key) const;

	// Remove a header by key (case-insensitive)
	// Returns true if the header was found and removed
	bool remove(const std::string &key);

	// Set a header value (replaces any existing values)
	void set(const std::string &key, const std::string &value);

	// Add a value to a header (appends to existing values)
	void add(const std::string &key, const std::string &value);

	// Get all headers as a const reference
	const std::map<std::string, std::vector<std::string>> &all() const;

	// Serialize all headers to HTTP format (Key: Value\r\n for each)
	// Does NOT include the trailing \r\n that marks end of headers
	std::string serialize() const;

private:
	std::map<std::string, std::vector<std::string>> headers_;
};
