#pragma once

#include <memory>
#include <span>
#include <string>
#include <vector>

#include "Header.hpp"
#include "ReadWriteFD.hpp"

/*
	CgiHandler: The Low-Level Process Manager.

	Responsibilities:
	- Establishing anonymous pipes for IPC.
	- Forking the child process.
	- Setting up the CGI environment variables.
	- Executing the target script using execve.
	- Encapsulating I/O streams to the child process.
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
		4. Automatically starts reading from the script's output (stdout).
		Returns false if system calls (pipe/fork) fail.
	*/
	bool start(ReadWriteFD::ReadableDataCallback stdoutReadCallback,
			   ReadWriteFD::ReadableEofCallback stdoutEofCallback,
			   ReadWriteFD::ReadableErrorCallback stdoutErrorCallback,
			   ReadWriteFD::WritableDrainCallback stdinDrainCallback,
			   ReadWriteFD::WritableErrorCallback stdinErrorCallback);

	/*
		Flow control for the script's output (Server <- Script).
		Forwarded to the internal stdout stream.
	*/
	void startReading();
	void stopReading();

	/*
		Queue data to be written to the script's input (Server -> Script).
		Forwarded to the internal stdin stream.
	*/
	size_t queueWrite(std::span<const char> data);

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
