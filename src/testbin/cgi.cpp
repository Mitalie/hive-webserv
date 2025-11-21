#include <exception>
#include <iostream>
#include <string>
#include <unistd.h>
#include <span>
#include "CgiHandler.hpp"
#include "Poll.hpp"
#include "Header.hpp"

void runTest(const std::string &testName, const std::string &scriptPath, const std::string &interpreter)
{
	std::cout << "==========================================" << std::endl;
	std::cout << "TEST: " << testName << std::endl;
	std::cout << "==========================================" << std::endl;

	if (access(scriptPath.c_str(), F_OK) == -1)
	{
		std::cerr << "[ERROR] Script not found" << std::endl;
		return;
	}

	std::string rawHeader =
		"POST /" +
		scriptPath +
		" HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"Content-Length: 5\r\n"
		"Content-Type: text/plain";

	try
	{
		Header header(rawHeader);
		CgiHandler cgi(header, scriptPath, interpreter);

		bool running = true;

		if (cgi.start(
				[](std::span<const char> data)
				{
					std::cout.write(data.data(), data.size());
				},
				[&running]()
				{
					std::cout << "\n[EOF] CGI Output stream closed." << std::endl;
					running = false;
				},
				[&running]()
				{
					std::cerr << "\n[ERR] Read error." << std::endl;
					running = false;
				},
				{},
				[&running]()
				{
					std::cerr << "\n[ERR] Write error." << std::endl;
					running = false;
				}))
		{
			cgi.getStdoutStream()->startReading();

			std::string body = "hello";
			cgi.getStdinStream()->queueWrite(std::span<const char>(body.data(), body.size()));

			while (running)
			{
				Poll::doPoll();
			}
		}
	}
	catch (const std::exception &e)
	{
		std::cerr << "[EXCEPTION] " << e.what() << std::endl;
	}
	std::cout << "\n"
			  << std::endl;
}

int main()
{
	runTest("PYTHON", "cgi-bin/test.py", "/usr/bin/python3");
	runTest("PHP", "cgi-bin/phpinfo", "/usr/bin/php-cgi");
	return 0;
}
