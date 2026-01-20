#pragma once

#include <cstddef>
#include <memory>
#include <span>
#include <vector>

#include "CallbackQueue.hpp"
#include "ChunkHeaderReader.hpp"
#include "Config.hpp"
#include "DelayedCleanup.hpp"
#include "RequestHeaderReader.hpp"
#include "IRequestManager.hpp"
#include "IRequestHandler.hpp"
#include "ReadWriteFD.hpp"
#include "UnixFD.hpp"

class Poll;

/*
	ClientHandler class is instantiated for each client connection.
	It reads incoming data from the connection, determines header and body
	boundaries, parses the header, instantiates the applicable request handler,
	and transfers the request body data and all response data between the
	connection and the handler.

	Implements IRequestManager for use by the request handlers.
*/
class ClientHandler : public IRequestManager
{
public:
	ClientHandler(const ListenerConfig &config, UnixFD &&fd);
	ClientHandler(const ClientHandler &other) = delete;
	ClientHandler &operator=(const ClientHandler &other) = delete;
	virtual ~ClientHandler();

	/* IRequestManager functions */

	virtual void setReadingBody(bool reading) override;
	virtual size_t writeResponseData(std::span<const char> data) override;
	virtual void onRequestDone() override;
	virtual void onRequestError() override;

private:
	const ListenerConfig &config;
	CallbackQueue::CallbackOwner cbOwner;

	/* IO handling and buffering */

	ReadWriteFD socket;
	std::vector<char> leftoverData;
	std::span<const char> availableData;
	bool bufferedDataCallbackPending = false;
	bool processingData = false;
	bool terminateConnection = false;
	size_t unfinishedResponseBytes = 0;
	size_t bufferedResponseBytes = 0;
	void processData();
	void socketReadCallback(std::span<const char> newData);
	void socketEofCallback();
	void bufferedDataCallback();
	void socketWriteCallback(size_t bufferSize);
	bool partialHeaderPending = false;
	void updateWakeup();
	void setupMainLoopCallback();

	/* Incoming data processing */

	ChunkHeaderReader chunkHeaderReader;
	RequestHeaderReader headerReader;
	std::unique_ptr<IRequestHandler> request;
	// Keep track of completed request even if request ptr wasn't set yet
	bool requestDone = false;
	bool readingPaused = false;
	bool chunked = false;
	bool useBodyLenMax = false;
	size_t bodyLen = 0;
	size_t bodyLenMax = 0;
	std::unique_ptr<IRequestHandler> createRequestHandler(RequestHeader &&header);
	void handleDataBody();
	void handleDataChunkHeader();
	void handleDataRequestHeader();
	void handleData();

	class TerminateClientException : public DelayedCleanupBase
	{
	public:
		TerminateClientException(ClientHandler *handler);

	private:
		ClientHandler *handler;
		void cleanup() const override;
	};
};
