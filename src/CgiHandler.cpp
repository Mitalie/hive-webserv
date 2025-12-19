#include <cctype>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "CgiHandler.hpp"
#include "Header.hpp"
#include "Poll.hpp"
#include "ReadWriteFD.hpp"
#include "UnixFD.hpp"

CgiHandler::CgiHandler(Header header, 
					   std::string scriptPath, 
					   std::string interpreterPath,
					   ReadWriteFD::ReadableDataCallback stdoutReadCallback,
					   ReadWriteFD::ReadableEofCallback stdoutEofCallback,
					   ReadWriteFD::ReadableErrorCallback stdoutErrorCallback,
					   ReadWriteFD::WritableDrainCallback stdinDrainCallback,
					   ReadWriteFD::WritableErrorCallback stdinErrorCallback)
	: header(std::move(header)),
	  scriptPath(std::move(scriptPath)),
	  interpreterPath(std::move(interpreterPath)),
	  pid(-1)
{
	pipeIn[0] = -1;
	pipeIn[1] = -1;
	pipeOut[0] = -1;
	pipeOut[1] = -1;

	// 1. Create Pipes
	if (pipe(pipeIn) < 0 || pipe(pipeOut) < 0)
	{
		cleanupPipes();
		throw std::runtime_error("CgiHandler: pipe creation failed");
	}

	// 2. Set Non-Blocking on Parent Ends
	if (fcntl(pipeIn[1], F_SETFL, O_NONBLOCK) < 0 ||
		fcntl(pipeOut[0], F_SETFL, O_NONBLOCK) < 0)
	{
		cleanupPipes();
		throw std::runtime_error("CgiHandler: fcntl failed");
	}

	// 3. Fork Process
	pid = fork();
	if (pid < 0)
	{
		cleanupPipes();
		throw std::runtime_error("CgiHandler: fork failed");
	}

	if (pid == 0)
	{
		setupChild();
	}

	close(pipeIn[0]);
	pipeIn[0] = -1;
	close(pipeOut[1]);
	pipeOut[1] = -1;

	// 4. Wrap FDs and Auto-Start
	stdoutStream = std::make_unique<ReadWriteFD>(
		UnixFD(pipeOut[0]),
		stdoutReadCallback,
		stdoutEofCallback,
		stdoutErrorCallback,
		ReadWriteFD::WritableDrainCallback{},
		ReadWriteFD::WritableErrorCallback{});
	pipeOut[0] = -1;

	stdinStream = std::make_unique<ReadWriteFD>(
		UnixFD(pipeIn[1]),
		ReadWriteFD::ReadableDataCallback{},
		ReadWriteFD::ReadableEofCallback{},
		ReadWriteFD::ReadableErrorCallback{},
		stdinDrainCallback,
		stdinErrorCallback);
	pipeIn[1] = -1;

	stdoutStream->startReading();
}

CgiHandler::~CgiHandler()
{
	// Close wrapper objects first to release FDs
	stdoutStream.reset();
	stdinStream.reset();
	cleanupPipes();

	// Reap zombie process if necessary
	if (pid > 0)
	{
		int status;
		pid_t res = waitpid(pid, &status, WNOHANG);
		if (res == 0)
		{
			kill(pid, SIGKILL);
			waitpid(pid, nullptr, 0);
		}
	}
}

// Forwarding methods
void CgiHandler::startReading()
{
	if (stdoutStream)
		stdoutStream->startReading();
}

void CgiHandler::stopReading()
{
	if (stdoutStream)
		stdoutStream->stopReading();
}

size_t CgiHandler::queueWrite(std::span<const char> data)
{
	if (stdinStream)
		return stdinStream->queueWrite(data);
	return 0;
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
	// 1. Redirect Standard IO
	if (dup2(pipeIn[0], STDIN_FILENO) < 0)
		exit(1);
	if (dup2(pipeOut[1], STDOUT_FILENO) < 0)
		exit(1);

	// 2. Close pipe ends
	close(pipeIn[1]);
	close(pipeOut[0]);
	close(pipeIn[0]);
	close(pipeOut[1]);

	// 3. Safety: Close all other server sockets inherited from parent
	Poll::closeAllRegisteredFds();

	// 4. Separate Query String from Script Path
	std::string cleanScriptPath = scriptPath;
	std::string queryString = "";
	size_t qPos = scriptPath.find('?');
	if (qPos != std::string::npos)
	{
		cleanScriptPath = scriptPath.substr(0, qPos);
		queryString = scriptPath.substr(qPos + 1);
	}

	// 5. Prepare Environment and Args
	std::vector<std::string> envStrs = createEnvVariables(cleanScriptPath, queryString);

	std::vector<char *> envp;
	envp.reserve(envStrs.size() + 1);
	
	for (auto &s : envStrs)
		envp.push_back(s.data());
	envp.push_back(nullptr);

	char *args[] = {
		interpreterPath.data(),
		cleanScriptPath.data(),
		nullptr};

	execve(args[0], args, envp.data());
	exit(1);
}

std::vector<std::string> CgiHandler::createEnvVariables(const std::string &scriptName, const std::string &queryStr)
{
	std::vector<std::string> env;
	
	constexpr size_t fixedVarsCount = 6;
	env.reserve(header.all().size() + fixedVarsCount);

	env.push_back("REQUEST_METHOD=" + header.method());
	env.push_back("SERVER_PROTOCOL=HTTP/1.1");
	env.push_back("PATH_INFO=" + header.path());
	env.push_back("SCRIPT_FILENAME=" + scriptName);
	env.push_back("QUERY_STRING=" + queryStr);
	env.push_back("REDIRECT_STATUS=200");

	for (const auto &pair : header.all())
	{
		std::string key = pair.first;
		std::string val;
		for (size_t i = 0; i < pair.second.size(); ++i)
		{
			if (i > 0)
				val += ", ";
			val += pair.second[i];
		}

		std::string envKey;
		for (char c : key)
		{
			if (c == '-')
				envKey += '_';
			else
				envKey += std::toupper(c);
		}

		if (envKey == "CONTENT_LENGTH" || envKey == "CONTENT_TYPE")
			env.push_back(envKey + "=" + val);
		else
			env.push_back("HTTP_" + envKey + "=" + val);
	}
	return env;
}
