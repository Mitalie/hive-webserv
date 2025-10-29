#pragma once

#include <vector>

/*
	Connection represents a single client connection which can be used for multiple requests.
*/
class Connection
{
public:
	/*
		Bytes that have not yet been consumed by the parser.

		TODO: don't expose read-write container, expose a const view or iterator and consume bytes separately.
	*/
	std::vector<char> receivedBytes;
};
