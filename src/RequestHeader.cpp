#include "RequestHeader.hpp"

#include <cstddef>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "Utils.hpp"

RequestHeader::RequestHeader(const std::string_view &raw)
{
	// Find end of first line (request-line)
	size_t lineEnd = raw.find("\r\n");
	if (lineEnd == std::string_view::npos)
		throw std::runtime_error("empty request (no request-line)");

	// Use Utils::trim on string_view
	std::string line(trim(raw.substr(0, lineEnd)));
	if (line.empty())
		throw std::runtime_error("empty request-line");

	// Parse request-line: METHOD PATH VERSION
	std::istringstream rl(line);
	if (!(rl >> method_ >> path_ >> version_))
		throw std::runtime_error("invalid request-line: expected 'METHOD PATH VERSION'");

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
