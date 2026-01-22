#include "ClientHandler.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>

#include "Config.hpp"
#include "ErrorRequestHandler.hpp"
#include "HeaderUtil.hpp"
#include "IRequestHandler.hpp"
#include "RequestHeader.hpp"
#include "UnixFD.hpp"
#include "router.hpp"

/**
 * Selects the appropriate ServerConfig for a request based on the Host header.
 * If no match is found, returns the first server as default.
 * Note: The port is stripped from the Host header before comparison, since
 * serverNames typically don't include ports (per HTTP/1.1, Host can be "host:port").
 * @return Reference to matching ServerConfig.
 */
static const ServerConfig &findServerConfig(const RequestHeader &header, const ListenerConfig &servers)
{

	std::string hostHeader = header.get("Host");
	// Strip port from Host header if present (e.g., "example.com:8080" -> "example.com")
	std::string::size_type colonPos = hostHeader.rfind(':');
	if (colonPos != std::string::npos)
	{
		// Check if everything after the colon is numeric (i.e., it's a port, not part of IPv6)
		bool isPort = true;
		for (std::string::size_type i = colonPos + 1; i < hostHeader.size(); ++i)
		{
			if (!std::isdigit(static_cast<unsigned char>(hostHeader[i])))
			{
				isPort = false;
				break;
			}
		}
		if (isPort && colonPos + 1 < hostHeader.size())
			hostHeader = hostHeader.substr(0, colonPos);
	}
	for (const auto &s : servers)
	{
		if (std::find(s.serverNames.begin(), s.serverNames.end(), hostHeader) != s.serverNames.end())
			return s;
	}
	// If no match, return the first server as default
	// There is always a first server, or we wouldn't be able to receive a request and get here
	return servers[0];
}

ClientHandler::ClientHandler(const ListenerConfig &config, UnixFD &&fd)
	: config(config),
	  socket(
		  std::move(fd),
		  [this](std::span<const char> newData)
		  { socketReadCallback(newData); },
		  [this]()
		  { socketEofCallback(); },
		  [this]()
		  { throw TerminateClientException(this); }, // Handle Read Error
		  [this](size_t bufferSize)
		  { socketWriteCallback(bufferSize); },
		  [this]()
		  { throw TerminateClientException(this); }) // Handle Write Error
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
	unfinishedResponseBytes += data.size();
	bufferedResponseBytes = socket.queueWrite(data);
	return bufferedResponseBytes;
}

void ClientHandler::onRequestDone()
{
	request = nullptr;
	requestDone = true;
	readingPaused = false;
	unfinishedResponseBytes = 0;
	updateWakeup();
}

void ClientHandler::onRequestError()
{
	// TODO: terminate only current handler, send error response
	throw TerminateClientException(this);
}

std::unique_ptr<IRequestHandler> ClientHandler::createRequestHandler(RequestHeader &&header)
{
	const ServerConfig &serverConfig = findServerConfig(header, config);
	chunked = false;
	bodyLen = 0;
	// partialHeaderPending remains set until request framing is determined to
	// prevent further request processing in case of framing error

	// Determine message framing
	TransferEncodingResult te = getTransferEncoding(header.fields);
	ContentLengthResult cl = getContentLength(header.fields);
	if (te.present && cl.present) // Both T-E and C-L specified? Might be malicious
		return std::make_unique<ErrorRequestHandler>(*this, std::move(header), serverConfig, 400);
	if (te.chunkedNotFinal) // Chunked is not final T-E, can't use it to detect end
		return std::make_unique<ErrorRequestHandler>(*this, std::move(header), serverConfig, 400);
	if (te.unknown) // Unsupported T-E value, don't know how to decode
		return std::make_unique<ErrorRequestHandler>(*this, std::move(header), serverConfig, 501);
	if (cl.invalid) // C-L does not parse cleanly, can't use it to detect end
		return std::make_unique<ErrorRequestHandler>(*this, std::move(header), serverConfig, 400);
	// Framing successfully determined
	chunked = te.chunked;
	bodyLen = cl.length;
	partialHeaderPending = false;

	// Early check for excess body size
	bodyLenMax = serverConfig.clientMaxBodySize;
	useBodyLenMax = bodyLenMax > 0;
	if (useBodyLenMax && bodyLen > bodyLenMax)
		return std::make_unique<ErrorRequestHandler>(*this, std::move(header), serverConfig, 413);

	// Use router to select and create the appropriate handler
	return router(*this, std::move(header), serverConfig);
}

void ClientHandler::handleDataRequestHeader()
{
	if (!availableData.empty())
		partialHeaderPending = true;
	// The reader will consume bytes until end of header
	std::optional<RequestHeader> header = headerReader.tryParse(availableData);
	if (header)
	{
		requestDone = false;
		request = createRequestHandler(std::move(*header));
		if (requestDone) // request completed during its construction
			request = nullptr;
	}
}

void ClientHandler::handleDataChunkHeader()
{
	// The reader will consume bytes until end of header
	std::optional<size_t> chunkLen = chunkHeaderReader.tryParse(availableData);
	if (chunkLen)
	{
		bodyLen = *chunkLen;

		// Check if this chunk would exceed maxBodySize
		if (useBodyLenMax && bodyLen > bodyLenMax)
			onRequestError();
		bodyLenMax -= bodyLen;

		if (bodyLen == 0)
		{
			// End of chunked body
			chunked = false;

			// Signal handler that body is complete
			if (request)
				request->onBodyDone();
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

	// Check if this was the last piece of a Content-Length body
	if (bodyLen == 0 && !chunked && request)
	{
		request->onBodyDone();
	}
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

void ClientHandler::socketEofCallback()
{
	terminateConnection = true;
	if (bodyLen > 0 || chunked || partialHeaderPending)
		// EOF received in the middle of a request
		// TODO: respond with 400 Bad Request
		request = nullptr;
	updateWakeup();
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
	bufferedResponseBytes = bufferSize;
	if (request)
		request->notifyResponseBuffer(bufferSize);
	updateWakeup();
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
	if (terminateConnection)
	{
		// Terminate requested
		if (!request && unfinishedResponseBytes >= bufferedResponseBytes)
			// No request handler active, and no finished responses in buffer.
			throw TerminateClientException(this);
		// Don't wake up for input anymore, just waiting for request handler to
		// terminate and for finished responses to send.
		socket.stopReading();
	}
	else if (readingPaused)
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
		cbOwner.queueCallback(
			[this]()
			{
				bufferedDataCallback();
			});
	}
}

void ClientHandler::setupMainLoopCallback()
{
}

ClientHandler::TerminateClientException::TerminateClientException(ClientHandler *handler)
	: handler(handler)
{
}

ClientHandler::TerminateClientException::~TerminateClientException()
{
	// When the exception object is destroyed, we should be out of any call
	// stack that might still access the object.
	delete handler;
}
