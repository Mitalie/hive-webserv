#pragma once

#include <string>
#include <string_view>

std::string toLower(std::string s);
std::string_view trim(std::string_view s);

template <class T>
struct OrderByAddr
{
	bool operator()(const T &lhs, const T &rhs) const
	{
		return std::less<const T *>{}(&lhs, &rhs);
	}
};
