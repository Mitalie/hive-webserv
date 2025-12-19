#pragma once

#include <optional>
#include <span>
#include <string>

#include "RequestHeader.hpp"

/*
	HTTP header reader that buffers incoming data until it finds the end of a
	header, and then parses the contents.
*/
class RequestHeaderReader
{
public:
	/*
		Receive a chunk of incoming data, scan for the end of the header, and
		update the span to skip over any consumed data.

		If the end of the header is not found, consume the entire chunk, and
		return nothing. If the end of the header is found, consume data until
		the end (including the terminating empty line), and return a RequestHeader
		instance parsed from the consumed data.
	*/
	std::optional<RequestHeader> tryParse(std::span<const char> &incomingData);

private:
	std::string buffer;
	static const size_t maxHeaderLen = 16384;
};
