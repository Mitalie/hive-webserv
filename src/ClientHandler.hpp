#pragma once

#include <cstddef>
#include <memory>
#include <span>
#include <vector>

#include "CallbackQueue.hpp"
#include "ChunkHeaderReader.hpp"
#include "Config.hpp"
#include "ConnectionManager.hpp"
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
	ClientHandler(
		const ListenerConfig &config,
		ConnectionManager &manager,
		UnixFD &&fd);
	ClientHandler(const ClientHandler &other) = delete;
	ClientHandler &operator=(const ClientHandler &other) = delete;
	virtual ~ClientHandler();

	/* IRequestManager functions */

	virtual void setReadingBody(bool reading) override;
	virtual size_t writeResponseData(std::span<const char> data) override;
	virtual void onRequestDone() override;
	virtual void onRequestError(int errorStatus) override;

private:
	const ListenerConfig &config;
	ConnectionManager &manager;
	CallbackQueue::CallbackOwner cbOwner;
	void destroyConnection();

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
	void updateWakeup();
	void setupMainLoopCallback();

	/* Incoming data processing */

	ChunkHeaderReader chunkHeaderReader;
	RequestHeaderReader headerReader;
	std::unique_ptr<IRequestHandler> request;
	bool partialHeaderPending = false;
	bool readingPaused = false;
	bool chunked = false;
	bool useBodyLenMax = false;
	size_t bodyLen = 0;
	size_t bodyLenMax = 0;
	void createRequestHandler(RequestHeader &&header);
	void handleDataBody();
	void handleDataChunkHeader();
	void handleDataRequestHeader();
	void handleData();
	bool isBodyDone();

	/* Error handling and termination */

	const ServerConfig *currentRequestConfig;
	std::optional<int> currentError = std::nullopt;
	void terminateRequest(std::optional<int> errorStatus);

	class TerminateRequestException : public DelayedCleanupBase
	{
	public:
		TerminateRequestException(ClientHandler &handler, std::optional<int> errorStatus = std::nullopt);

	private:
		ClientHandler &handler;
		std::optional<int> errorStatus;
		void cleanup() const override;
	};
};
