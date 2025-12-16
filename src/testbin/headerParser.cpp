#include <cassert>
#include <cstddef>
#include <exception>
#include <iostream> // for std::cout and std::cerr used in the test harness
#include <string>

#include "RequestHeader.hpp" // include the Header declaration so we can use it

int main()
{												 // small test program to demonstrate Header parsing
	const std::string raw_request =				 // raw HTTP request string used for the smoke test
		"GET /some/path?query=1 HTTP/1.1\r\n"	 // request-line with method, path, version
		"Host: example.com\r\n"					 // Host header
		"User-Agent: UnitTest/1.0\r\n"			 // User-Agent header
		"X-Custom-Header: value with spaces\r\n"; // a custom header with spaces in the value

	try
	{
		RequestHeader h(raw_request);							// construct Header and parse in constructor (may throw)
		std::cout << "Method: " << h.method() << "\n";	// print parsed method
		std::cout << "Path:   " << h.path() << "\n";	// print parsed path
		std::cout << "Ver:    " << h.version() << "\n"; // print parsed HTTP version

		std::cout << "Headers:\n"; // header list header
		for (const auto &kv : h.all())
		{
			std::cout << kv.first << ": ";
			for (size_t i = 0; i < kv.second.size(); ++i) {
				if (i > 0) std::cout << ", ";
				std::cout << kv.second[i];
			}
			std::cout << "\n";
		}

		std::cout << "Host via getter: " << h.get("Host") << "\n"; // show lookup by header name (case-insensitive)
	}
	catch (const std::exception &e)
	{
		std::cerr << "Failed to parse request: " << e.what() << "\n"; // report parse exception
		return 2;													  // non-zero exit indicates failure in test
	}


	// RFC9110: repeated header field test
	const std::string repeated =
		"GET /repeat HTTP/1.1\r\n"
		"Example-Field: Foo, Bar\r\n"
		"Example-Field: Baz\r\n"
		"Host: test.com\r\n";

	try {
		RequestHeader h(repeated);
		assert(h.method() == "GET");
		assert(h.path() == "/repeat");
		assert(h.version() == "HTTP/1.1");
		// Should combine values in order, comma-space separated
		assert(h.get("Example-Field") == "Foo, Bar, Baz");
		assert(h.get("Host") == "test.com");
		std::cout << "Combined Example-Field: " << h.get("Example-Field") << "\n";
	} catch (const std::exception &e) {
		std::cerr << "Repeated header parse threw: " << e.what() << "\n";
		return 2;
	}

	// Error case: empty input -> should throw
	bool threw = false;
	try {
		RequestHeader h("");
	} catch (const std::exception &) {
		threw = true;
	}
	assert(threw && "Empty request should throw in constructor");

	// Error case: malformed request-line
	threw = false;
	try {
		RequestHeader h("BADREQUEST\r\nHost: x\r\n");
	} catch (const std::exception &) {
		threw = true;
	}
	assert(threw && "Malformed request-line should throw in constructor");

	std::cout << "All Header tests passed\n";


	return 0; // success
}
