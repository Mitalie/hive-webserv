#pragma once

#include <exception>

class AbortWorkException : public std::exception
{
public:
	virtual ~AbortWorkException() = default;
};
