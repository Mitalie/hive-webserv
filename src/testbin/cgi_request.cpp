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
#include "RequestHeader.hpp"
#include "Tokenizer.hpp"

// Mock Manager: Captures output instead of just printing it
class MockRequestManager : public IRequestManager
{
public:
	bool isDone = false;
	bool gotError = false;
	std::string capturedOutput; // Store the response here

	size_t writeResponseData(std::span<const char> data) override
	{
		// Accumulate data to verify later
		capturedOutput.append(data.begin(), data.end());
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
	bool shouldKeepAlive() const override { return false; }
};

void runIntegrationTest(const std::string &testName, const std::string &scriptFile, const std::string &expectedStatus)
{
	std::cout << "\n==========================================" << std::endl;
	std::cout << "TEST: " << testName << std::endl;
	std::cout << "Script: " << scriptFile << std::endl;
	std::cout << "Expect: " << (!expectedStatus.empty() ? "SUCCESS" : "FAILURE") << std::endl;
	std::cout << "------------------------------------------" << std::endl;

	MockRequestManager mockManager;
	std::stringstream dummyStream("/dummy_path { }");
	Tokenizer tok(dummyStream);
	RouteConfig route(tok);
	route.root = "testdata";
	route.cgiInterpreters[".py"] = "/usr/bin/python3";

	// The RequestHeader string must contain ONLY the header lines, each terminated by \r\n.
	// The delimiter that separates headers from body is NOT part of the header block.
	std::string rawHeader =
		"POST /cgi-bin/" +
		scriptFile +
		" HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"Content-Length: 5\r\n"
		"Content-Type: text/plain\r\n"; 
	
	try
	{
		RequestHeader header(rawHeader);

		bool expectSuccess = !expectedStatus.empty();

		CgiRequestHandler handler(mockManager, header, route);
		std::string body = "hello";
		
		handler.onBodyData(std::span<const char>(body.data(), body.size()));
		
		// Signal that the body is complete (simulating ClientHandler behavior)
		handler.onBodyDone();

		int maxCycles = 350; // 35s max wait
		while (!mockManager.isDone && maxCycles-- > 0)
		{
			// Poll for max 100ms, then return to let us check timeouts
			Poll::doPoll(100);
			handler.checkTimeout();
		}

		if (maxCycles <= 0)
		{
			std::cerr << "   [RESULT] FAILED: Test hung!" << std::endl;
		}
		else if (expectSuccess)
		{
			if (mockManager.gotError)
			{
				std::cerr << "   [RESULT] FAILED: Expected Success, got Error." << std::endl;
			}
			else
			{
				// --- VISUAL VERIFICATION ---
				if (mockManager.capturedOutput.empty())
				{
					std::cerr << "   [FAILED] No output received." << std::endl;
				}
				else
				{
					// Print preview
					std::cout << "   [RAW RESPONSE PREVIEW]:" << std::endl;
					size_t headerEnd = mockManager.capturedOutput.find("\r\n\r\n");
					std::string preview = (headerEnd != std::string::npos)
											  ? mockManager.capturedOutput.substr(0, headerEnd)
											  : mockManager.capturedOutput.substr(0, 100);
					std::cout << "   >> " << preview << std::endl;

					// CRITICAL: Check if the output is valid HTTP matches expected status
					if (mockManager.capturedOutput.find(expectedStatus) == 0)
					{
						std::cout << "   [RESULT] PASSED: Found expected Status Line." << std::endl;
					}
					else
					{
						std::cerr << "   [RESULT] FAILED: Expected '" << expectedStatus << "'." << std::endl;
					}
				}
			}
		}
		else
		{
			if (!mockManager.gotError)
				std::cerr << "   [RESULT] FAILED: Expected Error, got Success." << std::endl;
			else
				std::cout << "   [RESULT] PASSED: Correctly timed out." << std::endl;
		}
	}
	catch (const std::exception &e)
	{
		std::cerr << "   [EXCEPTION] " << e.what() << std::endl;
	}
}

int main()
{
	runIntegrationTest("Normal Execution", "test.py", "HTTP/1.1 200 OK");
	runIntegrationTest("Timeout Safety", "timeout.py", "");
	runIntegrationTest("Default Status", "simple.py", "HTTP/1.1 200 OK");
	runIntegrationTest("Custom Status", "custom.py", "HTTP/1.1 418 I'm a teapot");
	return 0;
}
