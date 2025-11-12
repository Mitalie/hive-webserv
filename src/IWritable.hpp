#pragma once

#include <cstddef>
#include <functional>

class IWritable
{
public:
	/*
		WritableDrainCallback is called whenever data is written from the buffer
		into the sink, and receives the remaining size of the buffer. The
		callback can use this information to decide to halt processing so that
		the buffer does not grow excessively large.
	*/
	using WritableDrainCallback = std::function<void(size_t bufferSize)>;

	/*
		Start writing data from the internal buffer to the sink whenever it can
		accept some, and register a callback to be notified of the buffer level.
	*/
	virtual void startWriting(WritableDrainCallback callback) = 0;

	/*
		Stop writing data from the internal buffer to the sink. Data can still
		be appended to the internal buffer.
	*/
	virtual void stopWriting() = 0;

	/*
		Append data to the internal buffer to be written to the sink later.
		Returns the current size of the buffer. The caller can use this
		information to decide to halt processing so that the buffer does not
		grow excessively large.
	*/
	virtual size_t queueWrite(const char *data, size_t length) = 0;
};
