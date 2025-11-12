#include "ReadWriteFD.hpp"

#include <cstddef>
#include <stdexcept>

#include <sys/types.h>
#include <unistd.h>

#include "Poll.hpp"

ReadWriteFD::ReadWriteFD(Poll &poll, int fd)
	: poll(poll), fd(fd)
{
	poll.addFd(
		fd,
		[this]()
		{ onReadable(); },
		[this]()
		{ onWritable(); });
}

ReadWriteFD::~ReadWriteFD()
{
	poll.removeFd(fd);
}

void ReadWriteFD::startReading(ReadableDataCallback callback)
{
	readCallback = callback;
	poll.setReadableInterest(fd, true);
}

void ReadWriteFD::stopReading()
{
	readCallback = {};
	poll.setReadableInterest(fd, false);
}

void ReadWriteFD::onReadable()
{
	char readBuffer[maxReadSize];
	ssize_t bytesRead = ::read(fd, readBuffer, maxReadSize);
	if (bytesRead < 0)
		// TODO: proper error handling
		throw std::runtime_error("read");
	readCallback(readBuffer, bytesRead);
}

void ReadWriteFD::startWriting(WritableDrainCallback callback)
{
	writeCallback = callback;
	if (!writeBuffer.empty())
		poll.setWritableInterest(fd, true);
}

void ReadWriteFD::stopWriting()
{
	writeCallback = {};
	poll.setWritableInterest(fd, false);
}

size_t ReadWriteFD::queueWrite(const char *data, size_t length)
{
	if (writeBuffer.empty() && length)
		poll.setWritableInterest(fd, true);
	writeBuffer.insert(writeBuffer.end(), data, data + length);
	return writeBuffer.size();
}

void ReadWriteFD::onWritable()
{
	ssize_t bytesWritten = ::write(fd, writeBuffer.data(), writeBuffer.size());
	if (bytesWritten < 0)
		// TODO: proper error handling
		throw std::runtime_error("write");
	writeBuffer.erase(writeBuffer.begin(), writeBuffer.begin() + bytesWritten);
	writeCallback(writeBuffer.size());
	if (writeBuffer.empty())
		poll.setWritableInterest(fd, false);
}
