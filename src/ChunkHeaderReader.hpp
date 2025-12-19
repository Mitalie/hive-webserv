#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string>

/*
	HTTP chunked transfer coding reader that buffers incoming data until it
	finds the end of a chunk header, and then parses the length.
*/
class ChunkHeaderReader
{
public:
	/*
		Receive a span of incoming data, scan for the end of the header, and
		update the span to skip over any consumed data.

		If the end of the header is not found, consume the entire span, and
		return nothing. If the end of the header is found, consume data until
		the end, and return the parsed length.
	*/
	std::optional<size_t> tryParse(std::span<const char> &incomingData);

private:
	std::string buffer;
	bool expectChunkTerminator = false;
	bool consumingTrailers = false;
	std::optional<size_t> consumeTrailers(std::span<const char> &incomingData);
	static const size_t maxChunkHeaderLen = 64; // over 16 hex digits already overflows 64-bit size_t, but allow some leading zeroes
	static const size_t maxTrailerLen = 16384;
};
