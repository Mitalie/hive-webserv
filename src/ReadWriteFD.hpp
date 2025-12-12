#pragma once

#include <cstddef>
#include <functional>
#include <span>
#include <vector>

class Poll;

/*
	Implementation of IReadable and IWritable on standard Unix file descriptors.

	File descriptors must support `read` and `write` system calls. Can be used
	for read-only or write-only file descriptors, but the user should take care
	to only use the applicable interface (although Poll likely never reports the
	wrong mode as ready).
*/
class ReadWriteFD
{
public:
	/*
		ReadableDataCallback is called whenever data is read from the file, and
		receives a view of the data chunk that was just read. The data is not
		retained, so the user callback must process or store the entire chunk.
	*/
	using ReadableDataCallback = std::function<void(std::span<const char> data)>;
	/*
		WritableDrainCallback is called whenever queued data is written to the
		file, and receives the remaining size of the queue. If the user halted
		processing to limit the size of the queue, it might now want to resume.
	*/
	using WritableDrainCallback = std::function<void(size_t bufferSize)>;

	ReadWriteFD(
		int fd,
		ReadableDataCallback readCallback,
		WritableDrainCallback writeCallback);
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
	int fd;

	ReadableDataCallback readCallback;
	void onReadable();

	std::vector<char> writeBuffer;
	WritableDrainCallback writeCallback;
	void onWritable();
};
