#include "ReadWriteFD.hpp"

#include <cstddef>
#include <span>
#include <utility>

#include <sys/types.h>
#include <unistd.h>

#include "Poll.hpp"

ReadWriteFD::ReadWriteFD(
	int fd,
	ReadableDataCallback readCallback,
	ReadableEofCallback readEofCallback,
	ReadableErrorCallback readErrorCallback,
	WritableDrainCallback writeCallback,
	WritableErrorCallback writeErrorCallback)
	: fd(fd),
	  readCallback(std::move(readCallback)),
	  readEofCallback(std::move(readEofCallback)),
	  readErrorCallback(std::move(readErrorCallback)),
	  writeCallback(std::move(writeCallback)),
	  writeErrorCallback(std::move(writeErrorCallback))
{
	Poll::addFd(
		fd,
		[this]()
		{ onReadable(); },
		[this]()
		{ onWritable(); },
		[this]()
		{ onError(); });
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
	{
		if (readErrorCallback)
			readErrorCallback();
	}
	else if (bytesRead == 0)
	{
		if (readEofCallback)
			readEofCallback();
	}
	else
	{
		if (readCallback)
			readCallback(std::span(readBuffer, bytesRead));
	}
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
	{
		if (writeErrorCallback)
			writeErrorCallback();
		return;
	}

	writeBuffer.erase(writeBuffer.begin(), writeBuffer.begin() + bytesWritten);

	if (writeCallback)
		writeCallback(writeBuffer.size());

	if (writeBuffer.empty())
		Poll::setWritableInterest(fd, false);
}

void ReadWriteFD::onError()
{
	if (readErrorCallback)
		readErrorCallback();
	if (writeErrorCallback)
		writeErrorCallback();
}
