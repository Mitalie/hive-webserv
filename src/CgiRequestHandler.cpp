#include "CgiRequestHandler.hpp"
#include "IRequestManager.hpp"
#include "Header.hpp"
#include "Config.hpp"
#include <cstddef>
#include "CgiHandler.hpp"
#include <string>
#include <memory>
#include <span>

CgiRequestHandler::CgiRequestHandler(IRequestManager &manager, const Header &header, const RouteConfig &route)
	: manager_(manager),
	  responseFinished_(false)
{
	// 1. Determine interpreter
	std::string scriptPath = route.root + header.path();
	std::string extension = "";
	size_t dotPos = scriptPath.find_last_of('.');
	if (dotPos != std::string::npos)
	{
		extension = scriptPath.substr(dotPos);
	}

	std::string interpreter;
	auto it = route.cgiInterpreters.find(extension);
	if (it != route.cgiInterpreters.end())
	{
		interpreter = it->second;
	}
	else
	{
		// Strict Check: Do not guess. If extension is unknown, fail.
		manager_.onRequestError();
		return;
	}

	// 2. Init process manager
	cgiHandler_ = std::make_unique<CgiHandler>(header, scriptPath, interpreter);

	// 3. Start process
	if (!cgiHandler_->start(
			[this](std::span<const char> data)
			{
				manager_.writeResponseData(data);
			},
			[this]()
			{
				responseFinished_ = true;
				manager_.onRequestDone();
			},
			[this]()
			{
				if (!responseFinished_)
					manager_.onRequestError();
			},
			[this](size_t bufferSize)
			{
				if (bufferSize < PIPE_WRITE_LOW_WATER_MARK)
					manager_.setReadingBody(true);
			},
			{} // ignore script closing its stdin
			))
	{
		manager_.onRequestError();
		return;
	}

	// 4. Wire Output (CGI Stdout -> Server Response)
	cgiHandler_->getStdoutStream()->startReading();
}

CgiRequestHandler::~CgiRequestHandler() {}

void CgiRequestHandler::onBodyData(std::span<const char> data)
{
	if (!cgiHandler_)
		return;

	size_t queued = cgiHandler_->getStdinStream()->queueWrite(data);

	// Backpressure: If pipe is full, stop reading from client socket
	if (queued > PIPE_WRITE_HIGH_WATER_MARK)
	{
		manager_.setReadingBody(false);
	}
}

void CgiRequestHandler::notifyResponseBuffer(size_t bufferSize)
{
	if (!cgiHandler_)
		return;

	// Backpressure: If client socket is full, stop reading from CGI
	if (bufferSize > CLIENT_SEND_HIGH_WATER_MARK)
	{
		cgiHandler_->getStdoutStream()->stopReading();
	}
	else
	{
		cgiHandler_->getStdoutStream()->startReading();
	}
}
