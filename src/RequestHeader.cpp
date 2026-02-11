#include "RequestHeader.hpp"

#include <cstddef>
#include <map>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

RequestHeader::RequestHeader(const std::string_view &raw)
{
	// Find end of first line (request-line)
	size_t lineEnd = raw.find("\r\n");
	if (lineEnd == std::string_view::npos)
		throw BadRequestHeader("No request-line");

	// Parse request-line: METHOD PATH VERSION
	std::istringstream rl(std::string(raw.substr(0, lineEnd)));
	if (!(rl >> method_ >> path_ >> version_))
		throw BadRequestHeader("Invalid request-line: expected 'METHOD PATH VERSION'");

	// Parse remaining header fields
	std::string_view remaining = raw.substr(lineEnd + 2);
	fields.parse(remaining);
}

const std::string &RequestHeader::method() const { return method_; }
const std::string &RequestHeader::path() const { return path_; }
const std::string &RequestHeader::version() const { return version_; }

std::string RequestHeader::get(const std::string &key) const
{
	return fields.get(key);
}

const std::map<std::string, std::vector<std::string>> &RequestHeader::all() const
{
	return fields.all();
}
