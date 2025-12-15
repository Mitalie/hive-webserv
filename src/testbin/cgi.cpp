#include <exception>
#include <iostream>
#include <fstream>
#include <string>
#include <unistd.h>
#include <vector>
#include <span>
#include <cstdlib> // For getenv
#include "CgiHandler.hpp"
#include "Poll.hpp"
#include "Header.hpp"

void runTest(const std::string &testName, const std::string &scriptPath,
			 const std::string &interpreter, std::ostream &out = std::cout,
			 const std::string &extraHeaders = "")
{
	out << "\n==========================================" << std::endl;
	out << "TEST: " << testName << std::endl;
	out << "Interp: " << interpreter << std::endl;
	out << "Script: " << scriptPath << std::endl;
	out << "------------------------------------------" << std::endl;

	// Remove query string for access() check, as the physical file on disk
	// does not include the "?foo=bar" part.
	std::string cleanPath = scriptPath;
	size_t qPos = cleanPath.find('?');
	if (qPos != std::string::npos)
	{
		cleanPath = cleanPath.substr(0, qPos);
	}

	if (access(cleanPath.c_str(), F_OK) == -1)
	{
		std::cerr << "[ERROR] Script not found: " << cleanPath << std::endl;
		return;
	}
	if (access(interpreter.c_str(), X_OK) == -1)
	{
		std::cerr << "[ERROR] Interpreter not found: " << interpreter << std::endl;
		return;
	}

	// Define body first to use its size in the header
	std::string body = "potato";

	std::string rawHeader =
		"POST /" +
		scriptPath +
		" HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"Content-Length: " +
		std::to_string(body.size()) +
		"\r\n"
		"Content-Type: text/plain";

	if (!extraHeaders.empty())
	{
		rawHeader += "\r\n" + extraHeaders;
	}

	try
	{
		Header header(rawHeader);
		// Pass full scriptPath (with query string) so the Handler can parse it
		CgiHandler cgi(header, scriptPath, interpreter);

		bool running = true;
		bool writing = true;

		if (cgi.start(
				[&out](std::span<const char> data)
				{
					out.write(data.data(), data.size());
				},
				[&running, &out]()
				{
					out << "\n[EOF] CGI Output stream closed." << std::endl;
					running = false;
				},
				[&running, &out]()
				{
					out << "\n[ERR] Read error." << std::endl;
					running = false;
				},
				[&writing](size_t bufferSize)
				{
					if (bufferSize == 0 && writing)
					{
						writing = false;
					}
				},
				[&running, &writing, &out]()
				{
					if (!writing)
						return;
					out << "\n[ERR] Write error." << std::endl;
					running = false;
				}))
		{
			cgi.queueWrite(std::span<const char>(body.data(), body.size()));

			int cycles = 0;
			while (running && cycles++ < 200)
			{
				Poll::doPoll(100);
			}
			if (cycles >= 200)
				std::cerr << "\n[TIMEOUT] Test hung." << std::endl;
		}
	}
	catch (const std::exception &e)
    {
        std::cerr << "[EXCEPTION] " << e.what() << std::endl;
    }
}

int main()
{
    struct TestCase
    {
        std::string name;
        std::string script;
        std::string interp;
        std::string headers;
    };

    // Check local env for specific ruby, default to system ruby.
    const char *homeDir = std::getenv("HOME");
    std::string rubyPath = "/usr/bin/ruby";
    if (homeDir)
    {
        std::string localRuby = std::string(homeDir) + "/local_ruby/bin/ruby";
        if (access(localRuby.c_str(), X_OK) == 0)
            rubyPath = localRuby;
    }

    std::vector<TestCase> tests = {
        {"PYTHON", "testdata/cgi-bin/test.py", "/usr/bin/python3", ""},
        {"PHP", "testdata/cgi-bin/phpinfo", "/usr/bin/php-cgi", ""},
        {"BASH", "testdata/cgi-bin/test.sh", "/usr/bin/bash", ""},
        {"PERL", "testdata/cgi-bin/test.pl", "/usr/bin/perl", ""},
        {"RUBY", "testdata/cgi-bin/test.rb", rubyPath, ""},
        {"SESSION", "testdata/cgi-bin/session_test.py?foo=bar", "/usr/bin/python3", "Cookie: visit_count=5"}};

    while (true)
    {
        std::cout << "\nSelect CGI Test Case:\n";
        for (size_t i = 0; i < tests.size(); ++i)
        {
            std::cout << i + 1 << ". " << tests[i].name << "\n";
        }
        std::cout << tests.size() + 1 << ". RUN ALL (to output.txt)\n";
        std::cout << "0. Exit\n> ";

        int choice;
        if (!(std::cin >> choice))
            break;
        if (choice == 0)
            break;

        if (choice > 0 && choice <= (int)tests.size())
        {
            const auto &t = tests[choice - 1];
            runTest(t.name, t.script, t.interp, std::cout, t.headers);
        }
        else if (choice == (int)tests.size() + 1)
        {
            std::ofstream outfile("output.txt");
            if (outfile.is_open())
            {
                for (const auto &t : tests)
                    runTest(t.name, t.script, t.interp, outfile, t.headers);
                std::cout << "Done! See output.txt.\n";
            }
        }
    }
    return 0;
}
