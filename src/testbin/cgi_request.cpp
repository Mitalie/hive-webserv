#include <cstddef>
#include <exception>
#include <iostream>
#include <span>
#include <string>
#include <thread>
#include <chrono>
#include <algorithm>
#include <sstream>

#include "CgiRequestHandler.hpp"
#include "Config.hpp"
#include "IRequestManager.hpp"
#include "Poll.hpp"
#include "Header.hpp"
#include "Tokenizer.hpp"

// ==========================================
// 1. THE MOCK MANAGER
// ==========================================
class MockRequestManager : public IRequestManager
{
public:
	bool isDone = false;
	bool isReadingBody = true;

	void writeResponseData(std::span<const char> data) override
	{
		std::cout << "[MOCK MANAGER] Received " << data.size() << " bytes of response: ";
		std::string view(data.data(), std::min((size_t)50, data.size()));
		std::cout << "\"" << view << (data.size() > 50 ? "..." : "") << "\"" << std::endl;
	}

	void onRequestDone() override
	{
		std::cout << "[MOCK MANAGER] Request marked as DONE." << std::endl;
		isDone = true;
	}

	void onRequestError() override
	{
		std::cerr << "[MOCK MANAGER] Request marked as ERROR." << std::endl;
		isDone = true;
	}

	void setReadingBody(bool reading) override
	{
		if (isReadingBody != reading)
		{
			std::cout << "[MOCK MANAGER] Backpressure update: Stop reading from client? "
					  << (!reading ? "YES (Pipe Full)" : "NO (Pipe Drained)") << std::endl;
			isReadingBody = reading;
		}
	}
};

// ==========================================
// 2. THE TEST RUNNER
// ==========================================
int main()
{
	std::cout << "==========================================" << std::endl;
	std::cout << "TEST: CgiRequestHandler Integration" << std::endl;
	std::cout << "==========================================" << std::endl;

	MockRequestManager mockManager;

	// RouteConfig expects: <PATH> <OPEN_BRACE> <DIRECTIVES...> <CLOSE_BRACE>
	std::stringstream dummyStream("/dummy_path { }");
	Tokenizer tok(dummyStream);
	RouteConfig route(tok);

	// Manual Setup (Overwrite the empty defaults)
	// UPDATE: Point root to the testbin folder so it finds cgi-bin inside it
	route.root = "src/testbin";

	route.cgiInterpreters[".py"] = "/usr/bin/python3";
	route.cgiInterpreters[".php"] = "/usr/bin/php-cgi";
	// --------------------------------------------

	std::string rawHeader =
		"POST /cgi-bin/test.py HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"Content-Length: 10\r\n"
		"Content-Type: text/plain";
	Header header(rawHeader);

	try
	{
		// Instantiate the Handler
		CgiRequestHandler handler(mockManager, header, route);
		std::cout << "[TEST] Handler Created. CGI process should be running.\n"
				  << std::endl;

		// Simulate Data Flow
		std::string bodyChunk = "helloworld";
		std::cout << "[TEST] Sending body data: " << bodyChunk << std::endl;

		handler.onBodyData(std::span<const char>(bodyChunk.data(), bodyChunk.size()));

		// Run the Poll Loop
		int maxCycles = 100;
		while (!mockManager.isDone && maxCycles-- > 0)
		{
			Poll::doPoll();
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}

		if (maxCycles <= 0)
			std::cerr << "[TEST] Timed out waiting for CGI to finish!" << std::endl;
	}
	catch (const std::exception &e)
	{
		std::cerr << "[EXCEPTION] " << e.what() << std::endl;
	}

	std::cout << "\n==========================================" << std::endl;
	std::cout << "TEST FINISHED" << std::endl;
	return 0;
}
