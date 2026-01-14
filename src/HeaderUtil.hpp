#pragma once

#include <cstddef>

#include "HeaderFields.hpp"

struct ContentLengthResult
{
	bool present = false;
	bool invalid = false;
	size_t length = 0;
};

ContentLengthResult getContentLength(const HeaderFields &header);

struct TransferEncodingResult
{
	bool present = false;
	bool chunked = false;
	bool chunkedNotFinal = false;
	bool unknown = false;
};

TransferEncodingResult getTransferEncoding(const HeaderFields &header);
