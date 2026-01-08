#include <algorithm>
#include <cctype>
#include <cstddef>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include "HeaderFields.hpp"
#include "Utils.hpp"

std::string HeaderFields::toLower(std::string s)
{
	std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c)
				   { return std::tolower(c); });
	return s;
}

void HeaderFields::parse(std::string_view data)
{
	size_t pos = 0;
	while (pos < data.size())
	{
		// Find end of line
		size_t lineEnd = data.find("\r\n", pos);
		if (lineEnd == std::string_view::npos)
		{
			// Strict requirement: every header line must end with CRLF
			throw std::runtime_error("malformed header: missing CRLF");
		}

		std::string_view line = data.substr(pos, lineEnd - pos);
		pos = lineEnd + 2; // Always skip \r\n

		// Find colon separator
		size_t colonPos = line.find(':');
		if (colonPos == std::string_view::npos)
			throw std::runtime_error("malformed header: no colon separator");

		std::string key(trim(line.substr(0, colonPos)));
		std::string value(trim(line.substr(colonPos + 1)));

		if (key.empty())
			throw std::runtime_error("malformed header: empty key");

		// Store with lowercase key
		headers_[toLower(key)].push_back(value);
	}
}

std::string HeaderFields::get(const std::string &key) const
{
	auto it = headers_.find(toLower(key));
	if (it == headers_.end())
		return std::string();

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

bool HeaderFields::has(const std::string &key) const
{
	return headers_.find(toLower(key)) != headers_.end();
}

bool HeaderFields::remove(const std::string &key)
{
	auto it = headers_.find(toLower(key));
	if (it == headers_.end())
		return false;
	headers_.erase(it);
	return true;
}

void HeaderFields::set(const std::string &key, const std::string &value)
{
	std::string lowerKey = toLower(key);
	headers_[lowerKey].clear();
	headers_[lowerKey].push_back(value);
}

void HeaderFields::add(const std::string &key, const std::string &value)
{
	headers_[toLower(key)].push_back(value);
}

const std::map<std::string, std::vector<std::string>> &HeaderFields::all() const
{
	return headers_;
}

std::string HeaderFields::serialize() const
{
	std::string result;
	for (const auto &pair : headers_)
	{
		if (pair.first == "set-cookie")
		{
			// Special case: Set-Cookie must NOT be combined (RFC 9110)
			for (const auto &val : pair.second)
			{
				result += pair.first + ": " + val + "\r\n";
			}
		}
		else
		{
			result += pair.first + ": ";
			for (size_t i = 0; i < pair.second.size(); ++i)
			{
				if (i > 0)
					result += ", ";
				result += pair.second[i];
			}
			result += "\r\n";
		}
	}
	return result;
}
