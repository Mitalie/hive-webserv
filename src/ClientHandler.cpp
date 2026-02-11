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
#include "ConnectionManager.hpp"
#include "DelayedCleanup.hpp"
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

ClientHandler::ClientHandler(
	const ListenerConfig &config,
	ConnectionManager &manager,
	UnixFD &&fd)
	: config(config),
	  manager(manager),
	  socket(
		  std::move(fd),
		  [this](std::span<const char> newData)
		  { socketReadCallback(newData); },
		  [this]()
		  { socketEofCallback(); },
		  [this]()
		  { destroyConnection(); }, // Handle Read Error
		  [this](size_t bufferSize)
		  { socketWriteCallback(bufferSize); },
		  [this]()
		  { destroyConnection(); }) // Handle Write Error
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
	throw TerminateRequestException(*this);
}

void ClientHandler::onRequestError(int errorStatus)
{
	throw TerminateRequestException(*this, errorStatus);
}

void ClientHandler::destroyConnection()
{
	manager.destroyConnection(*this);
}

void ClientHandler::createRequestHandler(RequestHeader &&header)
{
	const ServerConfig &serverConfig = findServerConfig(header, config);
	currentRequestConfig = &serverConfig;
	chunked = false;
	bodyLen = 0;
	// partialHeaderPending remains set until request framing is determined to
	// prevent further request processing in case of framing error

	// Determine message framing
	TransferEncodingResult te = getTransferEncoding(header.fields);
	ContentLengthResult cl = getContentLength(header.fields);
	if (te.present && cl.present) // Both T-E and C-L specified? Might be malicious
		return terminateRequest(400);
	if (te.chunkedNotFinal) // Chunked is not final T-E, can't use it to detect end
		return terminateRequest(400);
	if (te.unknown) // Unsupported T-E value, don't know how to decode
		return terminateRequest(501);
	if (cl.invalid) // C-L does not parse cleanly, can't use it to detect end
		return terminateRequest(400);
	// Framing successfully determined
	chunked = te.chunked;
	bodyLen = cl.length;
	partialHeaderPending = false;

	const RouteConfig *route = matchRoute(header.path(), header.method(), serverConfig);
	if (!route)
		return terminateRequest(404);

	// Determine effective max body size
	if (route->clientMaxBodySize.has_value())
		bodyLenMax = *route->clientMaxBodySize;
	else
		bodyLenMax = serverConfig.clientMaxBodySize;

	// Early check for excess body size if bodyLen already known
	useBodyLenMax = bodyLenMax > 0;
	if (useBodyLenMax && bodyLen > bodyLenMax)
		return terminateRequest(413);

	// Use router function to select and construct the appropriate handler
	// Handle exception in case the handler completes or errors within constructor
	handleDelayedCleanup<TerminateRequestException>(
		[this, &header, route]
		{
			request = handleRequestForRoute(*this, std::move(header), *route);
		});
}

void ClientHandler::handleDataRequestHeader()
{
	if (!availableData.empty())
		partialHeaderPending = true;
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

		// Check if this chunk would exceed maxBodySize
		if (useBodyLenMax && bodyLen > bodyLenMax)
			return terminateRequest(413);
		bodyLenMax -= bodyLen;

		if (bodyLen == 0)
			// End of chunked body
			chunked = false;
	}
}

void ClientHandler::handleDataBody()
{
	// Consume body bytes even if request handler has already terminated
	size_t usableLen = std::min(bodyLen, availableData.size());
	std::span<const char> bodyData = availableData.subspan(0, usableLen);
	availableData = availableData.subspan(usableLen);
	bodyLen -= usableLen;

	if (request)
		handleDelayedCleanup<TerminateRequestException>(
			&IRequestHandler::onBodyData, request, bodyData);
}

void ClientHandler::handleData()
{
	while (!availableData.empty() && !readingPaused && !isBodyDone())
	{
		if (bodyLen)
			// Next byte(s) are body data
			handleDataBody();
		else if (chunked)
			// Next byte(s) are chunk header
			handleDataChunkHeader();
		else if (!request)
			// Next byte(s) are request header, and we're ready for a new request
			handleDataRequestHeader();

		if (isBodyDone())
			// Inform handler that no more body data is coming
			// This is only runs once because of the loop condition
			handleDelayedCleanup<TerminateRequestException>(
				&IRequestHandler::onBodyDone, request);
	}
}

bool ClientHandler::isBodyDone()
{
	return (request && !bodyLen && !chunked);
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
	activityTimer.resetTimeout(std::chrono::seconds(60), [this]()
							   { destroyConnection(); });
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
		terminateRequest(400);
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
	activityTimer.resetTimeout(std::chrono::seconds(60), [this]()
							   { destroyConnection(); });
	bufferedResponseBytes = bufferSize;
	if (request)
		handleDelayedCleanup<TerminateRequestException>(
			&IRequestHandler::notifyResponseBuffer, request, bufferSize);
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
			destroyConnection();
		// Don't wake up for input anymore, just waiting for request handler to
		// terminate and for finished responses to send.
		socket.stopReading();
	}
	else if (readingPaused || isBodyDone())
		// Wait for request handler to unpause or finish through its own wakeup
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

ClientHandler::TerminateRequestException::TerminateRequestException(ClientHandler &handler, std::optional<int> errorStatus)
	: handler(handler), errorStatus(errorStatus)
{
}

void ClientHandler::TerminateRequestException::cleanup() const
{
	handler.terminateRequest(errorStatus);
}

void ClientHandler::terminateRequest(std::optional<int> errorStatus)
{
	request = nullptr;
	if (!errorStatus)
	{
		// Request handler completed successfully
		readingPaused = false;
		unfinishedResponseBytes = 0;
		currentError = std::nullopt;
	}
	else if (unfinishedResponseBytes > 0)
	{
		// Request handler failed with output
		// The unfinished output makes the connection unusable, so just wait
		// for any finished responses to drain and then terminate connection.
		terminateConnection = true;
	}
	else if (!currentError)
	{
		// Request handler failed without output, send error response instead
		currentError = errorStatus;
		handleDelayedCleanup<TerminateRequestException>(
			[this, errorStatus]
			{
				request = std::make_unique<ErrorRequestHandler>(*this, *currentRequestConfig, *errorStatus);
			});
	}
	else
	{
		// ErrorRequestHandler failed, output minimal error instead of trying again
		std::string errorBody =
			"Error handler for status code " +
			std::to_string(*currentError) +
			"failed.";
		writeResponseData(
			"HTTP/1.1 500 Internal Server Error\r\n"
			"Content-Type: text/plain\r\n"
			"Content-Length: " +
			std::to_string(errorBody.size()) + "\r\n" +
			"\r\n" +
			errorBody);
		terminateRequest(std::nullopt);
	}
	updateWakeup();
}
