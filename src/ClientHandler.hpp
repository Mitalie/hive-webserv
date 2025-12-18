#pragma once

#include <cstddef>
#include <memory>
#include <span>
#include <vector>

#include "ChunkHeaderReader.hpp"
#include "HeaderReader.hpp"
#include "IRequestManager.hpp"
#include "ReadWriteFD.hpp"
#include "UnixFD.hpp"

class Poll;
class DummyRequestHandler;

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
	ClientHandler(UnixFD &&fd);
	ClientHandler(const ClientHandler &other) = delete;
	ClientHandler &operator=(const ClientHandler &other) = delete;
	~ClientHandler();

	/* IRequestManager functions */

	virtual void setReadingBody(bool reading) override;
	virtual void writeResponseData(std::span<const char> data) override;
	virtual void onRequestDone() override;
	virtual void onRequestError() override;

private:
	/* IO handling and buffering */

	ReadWriteFD socket;
	std::vector<char> leftoverData;
	std::span<const char> availableData;
	bool bufferedDataCallbackPending = false;
	bool processingData = false;
	void processData();
	void socketReadCallback(std::span<const char> newData);
	void bufferedDataCallback();
	void socketWriteCallback(size_t bufferSize);
	void updateWakeup();

	/* Incoming data processing */

	ChunkHeaderReader chunkHeaderReader;
	HeaderReader headerReader;
	std::unique_ptr<DummyRequestHandler> request;
	// Keep track of completed request even if request ptr wasn't set yet
	bool requestDone = false;
	bool readingPaused = false;
	bool chunked = false;
	size_t bodyLen = 0;
	void createRequestHandler(Header &&header);
	void handleDataBody();
	void handleDataChunkHeader();
	void handleDataRequestHeader();
	void handleData();
};
