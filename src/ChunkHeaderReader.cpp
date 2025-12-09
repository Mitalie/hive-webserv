#include "ChunkHeaderReader.hpp"

#include <cstddef>
#include <optional>
#include <span>
#include <stdexcept>

/*
	A chunked body consists of
	- Zero or more chunks of non-zero length
	- Chunk header indicating zero length
	- Zero or more trailer lines
	- CRLF
	A chunk consists of
	- Chunk header
	- Any bytes, length indicated by the header
	- CRLF
	A chunk header consists of:
	- One or more ASCII hex digits of length
	- Optional extension
		- Semicolon, surrounded by optional spaces/tabs
		- Extension name token
		- Optional value
			- Equals sign, surrounded by optional spaces/tabs
			- Extension value token or quoted-string
	- CRLF
	Trailer lines are similar to HTTP header lines, consisting of:
	- Field name token
	- Colon
	- Field value, surrounded by optional spaces/tabs

	0. General rules:
		- Caller must consume data bytes before attempting to parse next header.
		- Caller must handle trailers after zero chunk length is returned.
		- If input doesn't match expected format, report bad client data.
		- If (end of) expected element is not found, buffer and consume all input and return no value.
	1. If we're currently in a chunk:
		- Consume end-of-chunk CRLF
		- Note that we're not in a chunk anymore
	2. Consume and parse chunk header
	3. If length is non-zero, note that we're in a chunk now
	4. Return parsed length
*/

std::optional<size_t> ChunkHeaderReader::tryParse(std::span<const char> &incomingData)
{
	(void)incomingData;
	(void)expectChunkTerminator;
	throw std::logic_error("ChunkHeaderReader::tryParse is not yet implemented");
}
