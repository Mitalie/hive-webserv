#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include "IReadable.hpp"
#include "IWritable.hpp"

class Poll;

/*
	Implementation of IReadable and IWritable on standard Unix file descriptors.

	File descriptors must support `read` and `write` system calls. Can be used
	for read-only or write-only file descriptors, but the user should take care
	to only use the applicable interface (although Poll likely never reports the
	wrong mode as ready).
*/
class ReadWriteFD : public IReadable, public IWritable
{
public:
	ReadWriteFD(int fd);
	ReadWriteFD(const ReadWriteFD &other) = delete;
	ReadWriteFD &operator=(const ReadWriteFD &other) = delete;
	~ReadWriteFD();

	virtual void startReading(ReadableDataCallback callback) override;
	virtual void stopReading() override;

	virtual void startWriting(WritableDrainCallback callback) override;
	virtual void stopWriting() override;
	virtual size_t queueWrite(std::span<const char> data) override;

private:
	int fd;

	static const size_t maxReadSize = 4096;
	ReadableDataCallback readCallback;
	void onReadable();

	std::vector<char> writeBuffer;
	WritableDrainCallback writeCallback;
	void onWritable();
};
