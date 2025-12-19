#include <cstddef>
#include <exception>
#include <iostream>
#include <span>
#include <string>
#include <sstream>

#include "CgiRequestHandler.hpp"
#include "Config.hpp"
#include "IRequestManager.hpp"
#include "Poll.hpp"
#include "Header.hpp"
#include "Tokenizer.hpp"

// Mock Manager
class MockRequestManager : public IRequestManager
{
public:
	bool isDone = false;
	bool gotError = false;

	size_t writeResponseData(std::span<const char> data) override
	{
		std::cout << "   [MOCK] Received " << data.size() << " bytes." << std::endl;
		return 0;
	}
	void onRequestDone() override
	{
		std::cout << "   [MOCK] Request marked as DONE." << std::endl;
		isDone = true;
	}
	void onRequestError() override
	{
		std::cout << "   [MOCK] Request marked as ERROR (Timeout/Crash)." << std::endl;
		isDone = true;
		gotError = true;
	}
	void setReadingBody(bool) override {}
};

void runIntegrationTest(const std::string &testName, const std::string &scriptFile, bool expectSuccess)
{
	std::cout << "\n==========================================" << std::endl;
	std::cout << "TEST: " << testName << std::endl;
	std::cout << "Script: " << scriptFile << std::endl;
	std::cout << "Expect: " << (expectSuccess ? "SUCCESS (Done)" : "FAILURE (Error/Timeout)") << std::endl;
	std::cout << "------------------------------------------" << std::endl;

	MockRequestManager mockManager;
	std::stringstream dummyStream("/dummy_path { }");
	Tokenizer tok(dummyStream);
	RouteConfig route(tok);
	route.root = "testdata";
	route.cgiInterpreters[".py"] = "/usr/bin/python3";

	std::string rawHeader =
		"POST /cgi-bin/" +
		scriptFile +
		" HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"Content-Length: 5\r\n"
		"Content-Type: text/plain";
	Header header(rawHeader);

	try
	{
		CgiRequestHandler handler(mockManager, header, route);
		std::string body = "hello";
		handler.onBodyData(std::span<const char>(body.data(), body.size()));

		int maxCycles = 350; // 35s max wait
		while (!mockManager.isDone && maxCycles-- > 0)
		{
			// Poll for max 100ms, then return to let us check timeouts
			Poll::doPoll(100);
			handler.checkTimeout();
		}

		if (maxCycles <= 0)
			std::cerr << "   [RESULT] FAILED: Test hung!" << std::endl;
		else if (expectSuccess && mockManager.gotError)
			std::cerr << "   [RESULT] FAILED: Expected Success, got Error." << std::endl;
		else if (!expectSuccess && !mockManager.gotError)
			std::cerr << "   [RESULT] FAILED: Expected Error, got Success." << std::endl;
		else
			std::cout << "   [RESULT] PASSED." << std::endl;
	}
	catch (const std::exception &e)
	{
		std::cerr << "   [EXCEPTION] " << e.what() << std::endl;
	}
}

int main()
{
	runIntegrationTest("Normal Execution", "test.py", true);
	runIntegrationTest("Timeout Safety (Waits 30s)", "timeout.py", false);
	return 0;
}
