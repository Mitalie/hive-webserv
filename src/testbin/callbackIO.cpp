#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

#include <assert.h>
#include <dlfcn.h>
#include <unistd.h>
#include <sys/types.h>

#include "Poll.hpp"
#include "ReadWriteFD.hpp"

// Wrap read to emulate short reads
static auto _read = (decltype(read) *)dlsym(RTLD_NEXT, "read");
extern "C" ssize_t read(int fd, void *buf, size_t bufSz)
{
	bufSz = std::min(size_t(std::rand()) % 50 + 100, bufSz);
	return _read(fd, buf, bufSz);
}

// Wrap write to emulate short writes
static auto _write = (decltype(write) *)dlsym(RTLD_NEXT, "write");
extern "C" ssize_t write(int fd, const void *buf, size_t bufSz)
{
	bufSz = std::min(size_t(std::rand()) % 4 + 1, bufSz);
	return _write(fd, buf, bufSz);
}

int main()
{
	assert(_read != nullptr);
	assert(_write != nullptr);
	Poll poll;

	// For input, just process whatever we receive
	bool doneR = false;
	ReadWriteFD in(poll, 0);
	in.startReading(
		[&doneR, &in](const char *data, size_t length)
		{
			std::cout << "\x1b[33mRead block of " << length << " bytes: >\x1b[31m" << std::string_view(data, length) << "\x1b[33m<\x1b[0m" << std::endl;
			if (length == 0)
			{
				doneR = true;
				in.stopReading();
			}
		});

	// For output, we could just queue the entire message, but let's instead keep the buffer size limited
	ReadWriteFD out(poll, 1);
	std::string outputData = "some example data, and more example data.\n";
	const size_t outputChunkSize = 4;
	const size_t outputBufSize = outputChunkSize * 2;
	out.queueWrite(outputData.data(), outputBufSize);
	std::cout << "\x1b[33mWrite buffer has " << outputBufSize << " bytes queued\x1b[0m" << std::endl;
	size_t outputPos = outputBufSize;
	bool doneW = false;
	out.startWriting(
		[&doneW, &out, &outputData, &outputPos](size_t bufferSize)
		{
			std::cout << "\x1b[33mWrite buffer has " << bufferSize << " bytes remaining after write\x1b[0m" << std::endl;
			size_t space = outputBufSize - bufferSize;
			while (space >= outputChunkSize && outputPos < outputData.length())
			{
				std::string chunk = outputData.substr(outputPos, outputChunkSize);
				outputPos += outputChunkSize;
				bufferSize += outputChunkSize;
				space -= outputChunkSize;
				out.queueWrite(chunk.data(), chunk.length());
			}
			if (outputPos >= outputData.length() && bufferSize == 0)
				doneW = true;
			std::cout << "\x1b[33mWrite buffer has " << bufferSize << " bytes queued\x1b[0m" << std::endl;
		});

	// Pump the FDs with poll until both operations are done
	while (!doneR || !doneW)
	{
		poll.doPoll();
		sleep(1);
	}
}
