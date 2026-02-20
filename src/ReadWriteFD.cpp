#include "ReadWriteFD.hpp"

#include <cstddef>
#include <span>
#include <utility>

#include <sys/types.h>
#include <unistd.h>

#include "UnixFD.hpp"

ReadWriteFD::ReadWriteFD(
	UnixFD &&fd,
	ReadCallback readCallback,
	EofCallback eofCallback,
	DrainCallback drainCallback,
	ErrorCallback errorCallback)
	: fd(std::move(fd)),
	  readCallback(std::move(readCallback)),
	  eofCallback(std::move(eofCallback)),
	  drainCallback(std::move(drainCallback)),
	  errorCallback(std::move(errorCallback))
{
	this->fd.addToPoll(
		[this]()
		{ onReadable(); },
		[this]()
		{ onWritable(); },
		[this]()
		{ onError(); });
}

ReadWriteFD::~ReadWriteFD()
{
}

void ReadWriteFD::startReading()
{
	fd.setReadableInterest(true);
}

void ReadWriteFD::stopReading()
{
	fd.setReadableInterest(false);
}

void ReadWriteFD::onReadable()
{
	char readBuffer[maxReadSize];
	ssize_t bytesRead = ::read(fd, readBuffer, maxReadSize);

	if (bytesRead < 0)
	{
		if (errorCallback)
			errorCallback();
	}
	else if (bytesRead == 0)
	{
		if (eofCallback)
			eofCallback();
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
		fd.setWritableInterest(true);
	writeBuffer.insert(writeBuffer.end(), data.begin(), data.end());
	return writeBuffer.size();
}

void ReadWriteFD::onWritable()
{
	ssize_t bytesWritten = ::write(fd, writeBuffer.data(), writeBuffer.size());

	if (bytesWritten < 0)
	{
		if (errorCallback)
			errorCallback();
		return;
	}

	writeBuffer.erase(writeBuffer.begin(), writeBuffer.begin() + bytesWritten);
	if (writeBuffer.empty())
		fd.setWritableInterest(false);

	if (drainCallback)
		drainCallback(writeBuffer.size());
}

void ReadWriteFD::onError()
{
	if (errorCallback)
		errorCallback();
}
