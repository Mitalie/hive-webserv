#include <algorithm>
#include <cstddef>
#include <optional>
#include <span>
#include <stdexcept>
#include <string_view>

#include "RequestHeaderReader.hpp"
#include "RequestHeader.hpp"

constexpr std::string_view terminator = "\r\n\r\n";
// We may have all except last char of terminator already in buffer.
constexpr size_t scanBacktrack = terminator.length() - 1;

std::optional<RequestHeader> RequestHeaderReader::tryParse(std::span<const char> &incomingData)
{
	size_t oldBufLen = buffer.length();
	size_t scanStart = 0;
	if (oldBufLen > scanBacktrack)
		scanStart = oldBufLen - scanBacktrack;
	size_t maxToBuf = std::min(incomingData.size(), maxHeaderLen - oldBufLen);
	buffer.append(incomingData.begin(), incomingData.begin() + maxToBuf);
	size_t headerLen = buffer.find(terminator, scanStart);
	if (headerLen == std::string::npos)
	{
		// End not found at max size - reject client
		if (buffer.size() == maxHeaderLen)
			throw std::runtime_error("Request headers too long");
		// End not found - consume all and return nothing
		incomingData = {};
		return std::nullopt;
	}
	// End found - consume until end of terminator and return parsed header
	size_t remainderStartInBuf = headerLen + terminator.length();
	size_t remainderStartInChunk = remainderStartInBuf - oldBufLen;
	incomingData = incomingData.subspan(remainderStartInChunk);
	RequestHeader parsed(std::string_view(buffer.data(), headerLen + 2));
	buffer.clear();
	return parsed;
}
