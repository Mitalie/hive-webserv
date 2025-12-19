#include "ClientHandler.hpp"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>

#include "CallbackQueue.hpp"
#include "Config.hpp"
#include "RequestHeader.hpp"
#include "UnixFD.hpp"
#include "router.hpp"

ClientHandler::ClientHandler(const ListenerConfig &config, UnixFD &&fd)
	: config(config),
	  socket(
		  std::move(fd),
		  [this](std::span<const char> newData)
		  { socketReadCallback(newData); },
		  {}, // TODO: handle EOF
		  {}, // TODO: handle read error
		  [this](size_t bufferSize)
		  { socketWriteCallback(bufferSize); },
		  {}) // TODO: handle write error
{
	leftoverData.reserve(socket.maxReadSize);
	updateWakeup();
}

ClientHandler::~ClientHandler()
{
}

void ClientHandler::setReadingBody(bool reading)
{
	readingPaused = !reading;
	updateWakeup();
}

size_t ClientHandler::writeResponseData(std::span<const char> data)
{
	// TODO: implement max buffer size even if simple handlers don't?
	return socket.queueWrite(data);
}

void ClientHandler::onRequestDone()
{
	request = nullptr;
	requestDone = true;
	readingPaused = false;
	updateWakeup();
}

void ClientHandler::onRequestError()
{
	// TODO: abort connection
}

void ClientHandler::createRequestHandler(RequestHeader &&header)
{
	// TODO: maybe process body length / mode within header parser?
	std::string cl = header.get("content-length");
	bodyLen = 0;
	if (!cl.empty())
		bodyLen = std::stoull(cl);
	std::string te = header.get("transfer-encoding");
	if (!te.empty())
	{
		// TODO: delete content-length if transfer-encoding exists
		bodyLen = 0;
		// TODO: incomplete check, doesn't parse value properly
		if (te.find("chunked") != std::string::npos)
			chunked = true;
	}
	requestDone = false;
	// Use router to select and create the appropriate handler
	request = router(*this, header, config);
	if (requestDone) // request completed during its construction
		request = nullptr;
}

void ClientHandler::handleDataRequestHeader()
{
	// The reader will consume bytes until end of header
	std::optional<RequestHeader> header = headerReader.tryParse(availableData);
	if (header)
		createRequestHandler(std::move(*header));
}

void ClientHandler::handleDataChunkHeader()
{
	// The reader will consume bytes until end of header
	std::optional<size_t> chunkLen = chunkHeaderReader.tryParse(availableData);
	if (chunkLen)
	{
		bodyLen = *chunkLen;
		if (bodyLen == 0)
		{
			// End of chunked body
			// TODO: parse and consume trailers
			chunked = false;
			
		// 	// NEW: Signal handler that body is complete
		// 	if (request)
		// 		request->onBodyDone();
		}
	}
}

void ClientHandler::handleDataBody()
{
	size_t usableLen = std::min(bodyLen, availableData.size());
	if (request)
		request->onBodyData(availableData.subspan(0, usableLen));
	
	// If there is no current handler, the previous one completed without
	// waiting for entire body. Consume the body bytes either way.
	availableData = availableData.subspan(usableLen);
	bodyLen -= usableLen;

	// // NEW: Check if this was the last piece of a Content-Length body
	// if (bodyLen == 0 && !chunked && request)
	// {
	// 	request->onBodyDone();
	// }
}

void ClientHandler::handleData()
{
	while (!availableData.empty() && !readingPaused)
	{
		if (bodyLen)
			// Next byte(s) are body data
			handleDataBody();
		else if (chunked)
			// Next byte(s) are chunk header
			handleDataChunkHeader();
		else
			// Next bytes are request header
			handleDataRequestHeader();
	}
	// No more data available, or request handler paused input
}

void ClientHandler::processData()
{
	processingData = true;
	handleData();
	processingData = false;
	updateWakeup();
}

void ClientHandler::socketReadCallback(std::span<const char> newData)
{
	// Use read buffer directly to avoid unnecessary copying
	availableData = newData;
	processData();
	if (!availableData.empty())
	{
		// Read not fully consumed, store in buffer
		leftoverData.assign(availableData.begin(), availableData.end());
		availableData = leftoverData;
	}
}

void ClientHandler::bufferedDataCallback()
{
	bufferedDataCallbackPending = false;
	processData();
	if (availableData.empty())
		// Buffer fully consumed
		leftoverData.clear();
}

void ClientHandler::socketWriteCallback(size_t bufferSize)
{
	if (request)
		request->notifyResponseBuffer(bufferSize);
}

/*
	The ClientHandler needs to know how it will be woken up to do more work:
	- If reading is paused, the request handler is responsible for wakeup.
		ClientHandler remains inactive until request handler requests unpause
		or reports completion or error.
	- If leftover buffer is empty, ClientHandler needs to wait for new data from
		the connection and process it.
	- If leftover buffer is not empty, ClientHandler needs to process the
		buffered data.

	When called at the end of processData, we always fall into one of the first
	two cases, thus always updating socket reading state. The third case can
	only happen when the request handler pauses/unpauses or terminates. If we're
	still inside handleData, we should do nothing here and let handleData loop
	continue processing.

	Even outside of handleData, if the handler is unpausing, we should avoid
	directly calling into the request handler because it might not be reentrant.
	If the handler is terminating, we could continue processing immediately,
	although there is a risk of stack overflow if the handler terminated with a
	very deep call stack. CallbackQueue allows waking up ClientHandler from the
	main loop instead.
*/
void ClientHandler::updateWakeup()
{
	if (processingData)
		// Still processing, we'll update wakeup when we stop
		return;
	if (readingPaused)
		// Request handler takes responsibility for wakeup
		// No need to cancel pending callback, it will be a one-off and a no-op
		socket.stopReading();
	else if (availableData.empty())
		// Data fully consumed, wake up for read
		socket.startReading();
	else if (!bufferedDataCallbackPending)
	{
		// Queue callback to handle buffered data
		bufferedDataCallbackPending = true;
		CallbackQueue::queueCallback(
			[this]()
			{
				bufferedDataCallback();
			});
	}
}
