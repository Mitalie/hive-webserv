#include "CgiHandler.hpp"
#include "Header.hpp"
#include "Poll.hpp"
#include "ReadWriteFD.hpp"

#include <sys/types.h>
#include <stdlib.h>
#include <signal.h>
#include <cstdlib>
#include <cctype>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <utility>
#include <vector>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>

CgiHandler::CgiHandler(Header header, std::string scriptPath, std::string interpreterPath)
	: header(std::move(header)),
	  scriptPath(std::move(scriptPath)),
	  interpreterPath(std::move(interpreterPath)),
	  pid(-1)
{
	pipeIn[0] = -1;
	pipeIn[1] = -1;
	pipeOut[0] = -1;
	pipeOut[1] = -1;
}

CgiHandler::~CgiHandler()
{
	// Reset streams to close the parent-side file descriptors immediately
	stdoutStream.reset();
	stdinStream.reset();
	cleanupPipes();

	// Reap the child process to prevent zombie processes
	if (pid > 0)
	{
		int status;
		pid_t res = waitpid(pid, &status, WNOHANG);

		if (res == 0)
		{
			// If child is still running during destruction, kill it
			kill(pid, SIGKILL);
			waitpid(pid, nullptr, 0);
		}
	}
}

ReadWriteFD *CgiHandler::getStdoutStream() { return stdoutStream.get(); }
ReadWriteFD *CgiHandler::getStdinStream() { return stdinStream.get(); }

bool CgiHandler::start(ReadWriteFD::ReadableDataCallback stdoutReadCallback,
					   ReadWriteFD::ReadableEofCallback stdoutEofCallback,
					   ReadWriteFD::ReadableErrorCallback stdoutErrorCallback,
					   ReadWriteFD::WritableDrainCallback stdinDrainCallback,
					   ReadWriteFD::WritableErrorCallback stdinErrorCallback)
{
	// Create two unidirectional pipes:
	// 1. pipeIn: Parent writes -> Child reads (Stdin)
	// 2. pipeOut: Child writes -> Parent reads (Stdout)
	if (pipe(pipeIn) < 0 || pipe(pipeOut) < 0)
	{
		std::cerr << "CgiHandler: pipe() failed" << std::endl;
		return false;
	}

	// Parent ends must be non-blocking to integrate with the main Poll loop
	if (fcntl(pipeIn[1], F_SETFL, O_NONBLOCK) < 0 ||
		fcntl(pipeOut[0], F_SETFL, O_NONBLOCK) < 0)
	{
		std::cerr << "CgiHandler: fcntl() failed" << std::endl;
		cleanupPipes();
		return false;
	}

	pid = fork();
	if (pid < 0)
	{
		std::cerr << "CgiHandler: fork() failed" << std::endl;
		cleanupPipes();
		return false;
	}

	if (pid == 0)
	{
		setupChild(); // Enter child process logic; does not return
	}

	// Parent Logic:
	// Close the ends of the pipes used by the child
	close(pipeIn[0]);
	pipeIn[0] = -1;
	close(pipeOut[1]);
	pipeOut[1] = -1;

	// Wrap the remaining FDs in ReadWriteFD objects for the Event Loop
	stdoutStream = std::make_unique<ReadWriteFD>(
		pipeOut[0],
		stdoutReadCallback,
		stdoutEofCallback,
		stdoutErrorCallback,
		ReadWriteFD::WritableDrainCallback{},
		ReadWriteFD::WritableErrorCallback{});
	stdinStream = std::make_unique<ReadWriteFD>(
		pipeIn[1],
		ReadWriteFD::ReadableDataCallback{},
		ReadWriteFD::ReadableEofCallback{},
		ReadWriteFD::ReadableErrorCallback{},
		stdinDrainCallback,
		stdinErrorCallback);

	return true;
}

void CgiHandler::cleanupPipes()
{
	if (pipeIn[0] != -1)
	{
		close(pipeIn[0]);
		pipeIn[0] = -1;
	}
	if (pipeIn[1] != -1)
	{
		close(pipeIn[1]);
		pipeIn[1] = -1;
	}
	if (pipeOut[0] != -1)
	{
		close(pipeOut[0]);
		pipeOut[0] = -1;
	}
	if (pipeOut[1] != -1)
	{
		close(pipeOut[1]);
		pipeOut[1] = -1;
	}
}

void CgiHandler::setupChild()
{
	// 1. Duplicate file descriptors to standard streams
	if (dup2(pipeIn[0], STDIN_FILENO) < 0)
		exit(1);
	if (dup2(pipeOut[1], STDOUT_FILENO) < 0)
		exit(1);

	// 2. Close all original pipe FDs as they are now duplicated
	close(pipeIn[1]);
	close(pipeOut[0]);
	close(pipeIn[0]);
	close(pipeOut[1]);

	// 3. Close all other FDs inherited from the parent (sockets, etc.)
	// This ensures the CGI script doesn't hang onto the server's connections
	Poll::closeAllRegisteredFds();

	// 4. Construct Environment Variables (CGI/1.1 Standard)
	std::vector<std::string> envStrs;
	envStrs.push_back("REQUEST_METHOD=" + header.method());
	envStrs.push_back("SERVER_PROTOCOL=HTTP/1.1");
	envStrs.push_back("PATH_INFO=" + header.path());
	envStrs.push_back("SCRIPT_FILENAME=" + scriptPath);
	envStrs.push_back("REDIRECT_STATUS=200"); // Required for some PHP-CGI configurations

	for (const auto &pair : header.all())
	{
		std::string key = pair.first;
		std::string val;
		// Join multiple values for the same header with commas
		for (size_t i = 0; i < pair.second.size(); ++i)
		{
			if (i > 0)
				val += ", ";
			val += pair.second[i];
		}

		// Convert header format (User-Agent) to Env format (HTTP_USER_AGENT)
		std::string envKey;
		for (char c : key)
		{
			if (c == '-')
				envKey += '_';
			else
				envKey += std::toupper(c);
		}

		if (envKey == "CONTENT_LENGTH" || envKey == "CONTENT_TYPE")
			envStrs.push_back(envKey + "=" + val);
		else
			envStrs.push_back("HTTP_" + envKey + "=" + val);
	}

	// 5. Convert std::string vector to char* array for execve
	std::vector<char *> envp;
	envp.reserve(envStrs.size() + 1);
	for (const auto &s : envStrs)
		envp.push_back(const_cast<char *>(s.c_str()));
	envp.push_back(nullptr);

	char *args[] = {
		const_cast<char *>(interpreterPath.c_str()),
		const_cast<char *>(scriptPath.c_str()),
		nullptr};

	// Replace process image with the interpreter
	execve(args[0], args, envp.data());
	exit(1); // Only reached if execve fails
}
