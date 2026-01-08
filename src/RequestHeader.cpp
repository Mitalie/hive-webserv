#include "RequestHeader.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstddef>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "Utils.hpp"

RequestHeader::RequestHeader(const std::string_view &raw)
{
	// Find end of first line (request-line)
	size_t lineEnd = raw.find("\r\n");
	if (lineEnd == std::string_view::npos)
		throw std::runtime_error("empty request (no request-line)");

	// Use Utils::trim on string_view
	std::string line(trim(raw.substr(0, lineEnd)));
	if (line.empty())
		throw std::runtime_error("empty request-line");

	// Parse request-line: METHOD PATH VERSION
	std::istringstream rl(line);
	if (!(rl >> method_ >> path_ >> version_))
		throw std::runtime_error("invalid request-line: expected 'METHOD PATH VERSION'");

	// Parse remaining header fields
	std::string_view remaining = raw.substr(lineEnd + 2);
	fields.parse(remaining);
}

const std::string &RequestHeader::method() const { return method_; }
const std::string &RequestHeader::path() const { return path_; }
const std::string &RequestHeader::version() const { return version_; }

std::string RequestHeader::get(const std::string &key) const
{
	return fields.get(key);
}

const std::map<std::string, std::vector<std::string>> &RequestHeader::all() const
{
	return fields.all();
}

// --- Utility for comma-separated header values ---

std::vector<std::string> RequestHeader::parseList(const std::string &headerValue)
{
	std::vector<std::string> result;
	if (headerValue.empty())
		return result;

	size_t start = 0;
	while (start < headerValue.size())
	{
		// Find the next comma
		size_t end = headerValue.find(',', start);
		if (end == std::string::npos)
			end = headerValue.size();

		// Extract and trim the item
		std::string_view item(headerValue.data() + start, end - start);
		std::string trimmed(trim(item));
		if (!trimmed.empty())
			result.push_back(trimmed);

		start = end + 1;
	}
	return result;
}

// --- Content-Length ---

std::optional<size_t> RequestHeader::getContentLength() const
{
	std::string value = fields.get("content-length");
	if (value.empty())
		return std::nullopt;

	// Trim whitespace
	std::string_view trimmed = trim(value);
	if (trimmed.empty())
		throw InvalidHeader(400, "Content-Length header is empty");

	// Check for invalid characters (only digits allowed)
	for (char c : trimmed)
	{
		if (!std::isdigit(static_cast<unsigned char>(c)))
			throw InvalidHeader(400, "Content-Length contains non-digit characters");
	}

	// Parse using std::from_chars for safety
	size_t result = 0;
	auto [ptr, ec] = std::from_chars(trimmed.data(), trimmed.data() + trimmed.size(), result);

	if (ec == std::errc::result_out_of_range)
		throw InvalidHeader(400, "Content-Length value too large");
	if (ec != std::errc{} || ptr != trimmed.data() + trimmed.size())
		throw InvalidHeader(400, "Content-Length is not a valid number");

	return result;
}

// --- Transfer-Encoding ---

std::vector<std::string> RequestHeader::getTransferEncodings() const
{
	std::string value = fields.get("transfer-encoding");
	if (value.empty())
		return {};

	std::vector<std::string> encodings = parseList(value);

	// Convert to lowercase for comparison
	for (std::string &enc : encodings)
	{
		std::transform(enc.begin(), enc.end(), enc.begin(),
			[](unsigned char c)
			{ return std::tolower(c); });
	}

	return encodings;
}

bool RequestHeader::hasChunkedBody() const
{
	std::vector<std::string> encodings = getTransferEncodings();
	if (encodings.empty())
		return false;

	// Check for unsupported encodings (we only support "chunked")
	// Per HTTP/1.1, chunked must be the final encoding if present
	for (size_t i = 0; i < encodings.size(); ++i)
	{
		const std::string &enc = encodings[i];
		if (enc == "chunked")
		{
			// chunked must be the last encoding
			if (i != encodings.size() - 1)
				throw InvalidHeader(400, "chunked must be the final transfer encoding");
			return true;
		}
		// Only identity is allowed without processing
		if (enc != "identity")
			throw InvalidHeader(501, "Unsupported transfer encoding: " + enc);
	}

	return false;
}

// --- Method body expectations ---

bool RequestHeader::methodExpectsBody() const
{
	return method_ == "POST" || method_ == "PUT" || method_ == "PATCH";
}

bool RequestHeader::methodForbidsBody() const
{
	// Note: HTTP/1.1 allows bodies on any request, but these methods
	// typically shouldn't have one and many servers ignore them
	return method_ == "GET" || method_ == "HEAD" ||
		   method_ == "DELETE" || method_ == "OPTIONS" ||
		   method_ == "TRACE";
}
