#include "HeaderUtil.hpp"

#include <charconv>
#include <cstddef>
#include <system_error>

#include "HeaderFields.hpp"
#include "Utils.hpp"

ContentLengthResult getContentLength(const HeaderFields &header)
{
	const auto &strValues = header.getRaw("content-length");
	if (strValues.size() == 0)
		return { .present = false, .invalid = false, .length = 0};
	// Multiple Content-Length headers are not allowed
	if (strValues.size() > 1)
		return { .present = true, .invalid = true, .length = 0};

	const auto &strValue = strValues[0];
	auto start = strValue.data();
	auto end = strValue.data() + strValue.size();
	size_t parsedValue = 0;
	auto [parseEnd, ec] = std::from_chars(start, end, parsedValue);
	// Parse must succeed with no garbage (string is already whitespace-trimmed)
	if (ec == std::errc() && parseEnd == end)
		return { .present = true, .invalid = false, .length = parsedValue};
	return { .present = true, .invalid = true, .length = 0};
}

TransferEncodingResult getTransferEncoding(const HeaderFields &header)
{
	TransferEncodingResult res;
	if (!header.has("transfer-encoding"))
		return res;
	res.present = true;
	auto encodings = header.getList("transfer-encoding");

	for (auto encoding : encodings)
	{
		// We already got chunked, but there's another entry?
		// For requests, chunked MUST be final transfer encoding
		if (res.chunked)
			res.chunkedNotFinal = true;
		std::string encodingLower(toLower(std::string(encoding)));
		if (encodingLower == "chunked")
			res.chunked = true;
		else
			res.unknown = true;
	}
	// If chunked is not final, we can't use it
	if (res.chunkedNotFinal)
		res.chunked = false;
	return res;
}
