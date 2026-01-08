#include "Utils.hpp"

#include <cstddef>
#include <string_view>

std::string_view trim(std::string_view s)
{
	size_t start = s.find_first_not_of(" \t\r\n");
	if (start == std::string_view::npos)
		return {};

	size_t end = s.find_last_not_of(" \t\r\n");
	return s.substr(start, end - start + 1);
}
