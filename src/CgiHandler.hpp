#pragma once

#include "Header.hpp"
#include "ReadWriteFD.hpp"
#include <string>
#include <memory>

/*
	CgiHandler: The Process Manager

	Responsibility:
	- Forks the child process and sets up the environment variables.
	- Manages the Unix Pipes for Inter-Process Communication (IPC).
	- Wraps those pipes in ReadWriteFD to expose them as standard IReadable/IWritable streams.
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

	Header header;
	std::string scriptPath;
	std::string interpreterPath;

	// Standard ReadWriteFD wrappers for Poll integration
	std::unique_ptr<ReadWriteFD> stdoutStream;
	std::unique_ptr<ReadWriteFD> stdinStream;

	pid_t pid;
	int pipeIn[2];	// Parent writes -> Child reads (stdin)
	int pipeOut[2]; // Child writes -> Parent reads (stdout)
};
