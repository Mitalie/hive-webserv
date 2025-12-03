#pragma once

#include "Header.hpp"
#include "ReadWriteFD.hpp"
#include <string>
#include <memory>
#include <vector>

/*
	CgiHandler: The Low-Level Process Manager.

	Responsibilities:
	- Establishing anonymous pipes for IPC.
	- Forking the child process.
	- Setting up the CGI environment variables.
	- Executing the target script using execve.
*/
class CgiHandler
{
public:
	CgiHandler(Header header, std::string scriptPath, std::string interpreterPath);
	~CgiHandler();

	/*
		Starts the CGI execution flow:
		1. Creates anonymous pipes.
		2. Forks the process.
		3. Wraps the parent-side FDs in ReadWriteFD for non-blocking I/O.
		Returns false if system calls (pipe/fork) fail.
	*/
	bool start(ReadWriteFD::ReadableDataCallback stdoutReadCallback,
			   ReadWriteFD::ReadableEofCallback stdoutEofCallback,
			   ReadWriteFD::ReadableErrorCallback stdoutErrorCallback,
			   ReadWriteFD::WritableDrainCallback stdinDrainCallback,
			   ReadWriteFD::WritableErrorCallback stdinErrorCallback);

	ReadWriteFD *getStdoutStream(); // readable stream from CGI stdout
	ReadWriteFD *getStdinStream();	// writable stream to CGI stdin

private:
	void setupChild();
	void cleanupPipes();

	// Helper to translate HTTP headers into CGI environment format
	std::vector<std::string> createEnvVariables(const std::string &scriptName, const std::string &queryStr);

	Header header;
	std::string scriptPath;
	std::string interpreterPath;

	std::unique_ptr<ReadWriteFD> stdoutStream;
	std::unique_ptr<ReadWriteFD> stdinStream;

	pid_t pid;
	int pipeIn[2];	// Server -> Script
	int pipeOut[2]; // Script -> Server
};
