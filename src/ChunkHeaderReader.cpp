#include "ChunkHeaderReader.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

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
	- CRLF

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

static constexpr std::string_view CRLF = "\r\n";

// Parse hex digit, returns -1 if not a hex digit
static int hexDigitValue(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

static size_t parseHeaderLine(std::string_view headerLine)
{
	if (headerLine.empty())
		throw BadChunkedBody("Empty chunk header");

	size_t chunkSize = 0;
	size_t i = 0;

	// Must start with at least one hex digit
	int digit = hexDigitValue(headerLine[i]);
	if (digit < 0)
		throw BadChunkedBody("Invalid chunk size: no hex digits");

	// Parse hex digits
	while (i < headerLine.length())
	{
		digit = hexDigitValue(headerLine[i]);
		if (digit < 0)
			break;
		// Check for overflow
		if (chunkSize > (SIZE_MAX >> 4))
			throw BadChunkedBody("Chunk size overflow");
		chunkSize = (chunkSize << 4) | digit;
		i++;
	}
	// Chunk length may be followed by an optional extension. We could validate its
	// form but for now we just ignore everything from first non-hex char until CRLF.
	return chunkSize;
}

std::optional<size_t> ChunkHeaderReader::tryParse(std::span<const char> &incomingData)
{
	constexpr size_t scanBacktrack = CRLF.length() - 1;
	if (consumingTrailers)
		return consumeTrailers(incomingData);
	// Buffer incoming data
	size_t oldBufLen = buffer.length();
	size_t scanStart = 0;
	if (oldBufLen > scanBacktrack)
		scanStart = oldBufLen - scanBacktrack;
	size_t maxToBuf = std::min(incomingData.size(), maxChunkHeaderLen - oldBufLen);
	buffer.append(incomingData.begin(), incomingData.begin() + maxToBuf);

	// If we expect chunk terminator (CRLF after chunk data), check and skip it first
	if (expectChunkTerminator && scanStart < 2)
	{
		if (buffer.size() < 2)
		{
			incomingData = {};
			return std::nullopt;
		}
		if (buffer[0] == CRLF[0] && buffer[1] == CRLF[1])
			scanStart = 2;
		else
			throw BadChunkedBody("Expected CRLF after chunk data");
	}

	// Look for end of chunk header line (CRLF)
	size_t headerEnd = buffer.find(CRLF, scanStart);
	if (headerEnd == std::string::npos)
	{
		// Header line not complete
		if (buffer.size() >= maxChunkHeaderLen)
			throw BadChunkedBody("Chunk header too long");
		incomingData = {};
		return std::nullopt;
	}
	size_t totalConsumed = headerEnd + CRLF.length();
	size_t consumedFromInput = totalConsumed - oldBufLen;
	incomingData = incomingData.subspan(consumedFromInput);

	// Make sure to skip the CRLF between previous data and next header
	const char *headerStart = buffer.data() + (expectChunkTerminator ? 2 : 0);
	size_t chunkSize = parseHeaderLine(std::string_view(headerStart, headerEnd));

	buffer.clear();
	if (chunkSize > 0)
	{
		expectChunkTerminator = true;
		return chunkSize;
	}
	// If chunkSize == 0 this was the last chunk header, but we need to consume trailers
	consumingTrailers = true;
	expectChunkTerminator = false;
	return consumeTrailers(incomingData);
}

// returns either nullopt or zero, for passthrough returns in tryParse
std::optional<size_t> ChunkHeaderReader::consumeTrailers(std::span<const char> &incomingData)
{
	constexpr std::string_view terminator = "\r\n\r\n";
	constexpr size_t scanBacktrack = terminator.length() - 1;

	size_t oldBufLen = buffer.length();
	size_t scanStart = 0;
	if (oldBufLen > scanBacktrack)
		scanStart = oldBufLen - scanBacktrack;
	size_t maxToBuf = std::min(incomingData.size(), maxTrailerLen - oldBufLen);
	buffer.append(incomingData.begin(), incomingData.begin() + maxToBuf);

	// Check for immediate second CRLF (no trailers) after the zero-length chunk header
	if (scanStart < 2)
	{
		if (buffer.size() < 2)
		{
			incomingData = {};
			return std::nullopt;
		}
		if (buffer[0] == CRLF[0] && buffer[1] == CRLF[1])
		{
			// No trailers, just final CRLF
			incomingData = incomingData.subspan(CRLF.length() - oldBufLen);
			buffer.clear();
			consumingTrailers = false;
			return 0;
		}
	}

	size_t trailerEnd = buffer.find(terminator, scanStart);
	if (trailerEnd == std::string::npos)
	{
		if (buffer.size() >= maxTrailerLen)
			throw BadChunkedBody("Trailers too long");
		incomingData = {};
		return std::nullopt;
	}

	// Found end of trailers
	size_t totalConsumed = trailerEnd + terminator.length();
	size_t consumedFromInput = totalConsumed - oldBufLen;
	incomingData = incomingData.subspan(consumedFromInput);
	buffer.clear();
	consumingTrailers = false;
	return 0;
}
