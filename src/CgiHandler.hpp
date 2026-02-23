#pragma once

#include <memory>
#include <span>
#include <string>
#include <vector>

#include <sys/types.h>

#include "CallbackQueue.hpp"
#include "Config.hpp"
#include "ReadWriteFD.hpp"
#include "RequestHeader.hpp"

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
	/*
		Constructor initializes the handler and immediately starts the CGI process.

		Throws std::runtime_error if system calls (pipe, fork, fcntl) fail.

		Upon successful construction:
		1. Anonymous pipes are created.
		2. Child process is forked and script is running.
		3. Reading from the script's output (stdout) is automatically started.
	*/
	CgiHandler(RequestHeader header,
			   std::string scriptPath,
			   std::string pathInfo,
			   std::string interpreterPath,
			   std::string clientIp,
			   const HostPort &hostPort,
			   bool brokenPathinfo,
			   ReadWriteFD::ReadCallback stdoutReadCallback,
			   ReadWriteFD::EofCallback stdoutEofCallback,
			   ReadWriteFD::ErrorCallback stdoutErrorCallback,
			   ReadWriteFD::DrainCallback stdinDrainCallback);

	~CgiHandler();
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

	/*
		Signals that no more data will be written to the script's input.
		The pipe will be closed once all buffered data has been drained.
	*/
	void finishInput();

private:
	void setupChild();
	void cleanupPipes();

	// Extracted logic for handling drain events
	void onStdinDrain(size_t bufferSize);

	// Helper to translate HTTP headers into CGI environment format
	std::vector<std::string> createEnvVariables(const std::string &scriptName);

	RequestHeader header;
	std::string scriptPath;
	std::string pathInfo;
	std::string interpreterPath;
	std::string clientIp;
	const HostPort &hostPort;
	bool brokenPathinfo;
	ReadWriteFD::ErrorCallback stdoutErrorCallback;
	ReadWriteFD::DrainCallback stdinDrainCallback;

	CallbackQueue::CallbackOwner cbOwner;

	std::unique_ptr<ReadWriteFD> stdoutStream;
	std::unique_ptr<ReadWriteFD> stdinStream;

	pid_t pid;
	int pipeIn[2];	// Server -> Script
	int pipeOut[2]; // Script -> Server

	bool inputFinished;
	size_t currentStdinQueueSize;
};
