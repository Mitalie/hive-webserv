#pragma once

#include <cstddef>
#include <functional>
#include <span>
#include <vector>

#include "UnixFD.hpp"

class Poll;

/*
	Implementation of IReadable and IWritable for standard Unix file descriptors.
	Provides non-blocking I/O with internal buffering for writes.
*/
class ReadWriteFD
{
public:
	/*
		ReadCallback is called whenever data is read from the file, and
		receives a view of the data chunk that was just read. The data is not
		retained, so the user callback must process or store the entire chunk.
	*/
	using ReadCallback = std::function<void(std::span<const char> data)>;
	using EofCallback = std::function<void()>;
	/*
		DrainCallback is called whenever queued data is written to the
		file, and receives the remaining size of the queue. If the user halted
		processing to limit the size of the queue, it might now want to resume.
	*/
	using DrainCallback = std::function<void(size_t bufferSize)>;
	using ErrorCallback = std::function<void()>;

	ReadWriteFD(
		UnixFD &&fd,
		ReadCallback readCallback,
		EofCallback eofCallback,
		DrainCallback drainCallback,
		ErrorCallback errorCallback);
	ReadWriteFD(const ReadWriteFD &other) = delete;
	ReadWriteFD &operator=(const ReadWriteFD &other) = delete;
	~ReadWriteFD();

	/*
		Enable reading. Whenever Poll reports the file as readable, a chunk is
		read and passed to the read callback. Initial state is disabled.
	*/
	void startReading();
	/*
		Disable reading. Poll stops monitoring this file for readable status,
		the file will not be read, and the read callback will not be called.
	*/
	void stopReading();

	/*
		Append data to the queue to be written to the file. Returns the current
		size of the queue. The user might want to halt processing to prevent the
		queue from growing excessively large.
	*/
	size_t queueWrite(std::span<const char> data);

	static const size_t maxReadSize = 4096;

private:
	UnixFD fd;

	ReadCallback readCallback;
	EofCallback eofCallback;
	DrainCallback drainCallback;
	ErrorCallback errorCallback;
	std::vector<char> writeBuffer;

	void onReadable();
	void onWritable();
	void onError();
};
