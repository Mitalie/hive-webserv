#include "Utils.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <string>
#include <string_view>

std::string toLower(std::string s)
{
	std::transform(
		s.begin(), s.end(), s.begin(),
		[](unsigned char c)
		{ return std::tolower(c); });
	return s;
}

std::string_view trim(std::string_view s)
{
	size_t start = s.find_first_not_of(" \t\r\n");
	if (start == std::string_view::npos)
		return {};

	size_t end = s.find_last_not_of(" \t\r\n");
	return s.substr(start, end - start + 1);
}
