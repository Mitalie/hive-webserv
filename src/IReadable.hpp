#pragma once

#include <cstddef>
#include <functional>

class IReadable
{
public:
	/*
		ReadableDataCallback is called whenever data is read from the source,
		and receives a view of the data chunk that was just read. The data is
		already consumed from the source and can not be retrieved later, so the
		callback must store or process the entire chunk.
	*/
	using ReadableDataCallback = std::function<void(const char *data, size_t length)>;

	/*
		Start consuming data from the source whenever some is available, and
		register a callback to handle the data chunks as they are read.
	*/
	virtual void startReading(ReadableDataCallback callback) = 0;

	/*
		Stop consuming data from the source.
	*/
	virtual void stopReading() = 0;
};
