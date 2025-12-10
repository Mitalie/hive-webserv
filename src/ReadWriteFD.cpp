#include "ReadWriteFD.hpp"

#include <cstddef>
#include <span>
#include <stdexcept>
#include <utility>

#include <sys/types.h>
#include <unistd.h>

#include "Poll.hpp"

ReadWriteFD::ReadWriteFD(
	int fd,
	ReadableDataCallback readCallback,
	WritableDrainCallback writeCallback)
	: fd(fd),
	  readCallback(std::move(readCallback)),
	  writeCallback(std::move(writeCallback))
{
	Poll::addFd(
		fd,
		[this]()
		{ onReadable(); },
		[this]()
		{ onWritable(); });
}

ReadWriteFD::~ReadWriteFD()
{
	Poll::removeFd(fd);
}

void ReadWriteFD::startReading()
{
	Poll::setReadableInterest(fd, true);
}

void ReadWriteFD::stopReading()
{
	Poll::setReadableInterest(fd, false);
}

void ReadWriteFD::onReadable()
{
	char readBuffer[maxReadSize];
	ssize_t bytesRead = ::read(fd, readBuffer, maxReadSize);
	if (bytesRead < 0)
		// TODO: proper error handling
		throw std::runtime_error("read");
	readCallback(std::span(readBuffer, bytesRead));
}

size_t ReadWriteFD::queueWrite(std::span<const char> data)
{
	if (writeBuffer.empty() && data.size())
		Poll::setWritableInterest(fd, true);
	writeBuffer.insert(writeBuffer.end(), data.begin(), data.end());
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
		Poll::setWritableInterest(fd, false);
}
