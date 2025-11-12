#include "Header.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

// Trim whitespace from both ends
static inline std::string trim(const std::string &s)
{
	size_t start = s.find_first_not_of(" \t\r\n");
	if (start == std::string::npos)
		return "";
	size_t end = s.find_last_not_of(" \t\r\n");
	return s.substr(start, end - start + 1);
}

// Lowercase a string in-place and return it
static inline std::string to_lower(std::string s)
{
	std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c)
		{ return std::tolower(c); });
	return s;
}

Header::Header(const std::string_view &raw) // parse constructor
	: method_(), path_(), version_(), headers_()
{
	std::istringstream in(std::string{raw}); // input stream for parsing
	std::string line;			// temporary line buffer

	// Parse request-line
	if (!std::getline(in, line))									 // read request-line
		throw std::runtime_error("empty request (no request-line)"); // error if missing
	line = trim(line);												 // trim whitespace
	if (line.empty())												 // check for empty request-line
		throw std::runtime_error("empty request-line");				 // throw on error

	std::istringstream rl(line);														  // stream for request-line parsing
	if (!(rl >> method_ >> path_ >> version_))											  // parse method, path, version
		throw std::runtime_error("invalid request-line: expected 'METHOD PATH VERSION'"); // throw on error

	// Parse header fields
	// Note: caller is responsible for removing empty line that marks end of headers before passing to constructor
	while (std::getline(in, line)) // read each header line
	{
		line = trim(line);								  // trim whitespace
		size_t pos = line.find(':');					  // find colon separator
		if (pos == std::string::npos)					  // no colon found
			throw std::runtime_error("malformed header"); // malformed header, skip
		std::string key = trim(line.substr(0, pos));	  // extract key
		std::string value = trim(line.substr(pos + 1));	  // extract value
		key = to_lower(key);							  // lowercase key for case-insensitive storage
		headers_[key].push_back(value);					  // store in map
	}
}

const std::string &Header::method() const { return method_; }	// returns stored method_
const std::string &Header::path() const { return path_; }		// returns stored path_
const std::string &Header::version() const { return version_; } // returns stored version_

std::string Header::get(const std::string &key) const
{
	std::string lk = to_lower(key);
	auto it = headers_.find(lk);
	if (it == headers_.end())
		return std::string();
	// Concatenate all values with commas
	const std::vector<std::string> &values = it->second;
	std::string result;
	for (size_t i = 0; i < values.size(); ++i)
	{
		if (i > 0)
			result += ", ";
		result += values[i];
	}
	return result;
}

const std::map<std::string, std::vector<std::string>> &Header::all() const { return headers_; }
