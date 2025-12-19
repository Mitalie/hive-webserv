#include <chrono>
#include <cstddef>
#include <exception>
#include <iostream>
#include <memory>
#include <span>
#include <string>

#include "CgiHandler.hpp"
#include "CgiRequestHandler.hpp"
#include "Config.hpp"
#include "Header.hpp"
#include "IRequestManager.hpp"
#include "ReadWriteFD.hpp"

CgiRequestHandler::CgiRequestHandler(IRequestManager &manager, const Header &header, const RouteConfig &route)
	: manager_(manager),
	  responseFinished_(false),
	  startTime_(std::chrono::steady_clock::now())
{
	// Config Logic (Helper)
	std::string scriptPath = route.root + header.path();
	std::string interpreter = findInterpreter(scriptPath, route);

	if (interpreter.empty())
	{
		manager_.onRequestError();
		return;
	}

	// Process Initiation
	try
	{
		cgiHandler_ = std::make_unique<CgiHandler>(
			header,
			scriptPath,
			interpreter,
			[this](std::span<const char> data)
			{
				size_t bufferSize = manager_.writeResponseData(data);
				// Backpressure: If send buffer is full, stop reading from CGI
				if (bufferSize > CLIENT_SEND_HIGH_WATER_MARK)
					cgiHandler_->stopReading();
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
			ReadWriteFD::WritableErrorCallback{});
	}
	catch (const std::exception &e)
	{
		std::cerr << "[CgiRequestHandler] Error starting CGI: " << e.what() << std::endl;
		manager_.onRequestError();
		return;
	}
}

CgiRequestHandler::~CgiRequestHandler() {}

std::string CgiRequestHandler::findInterpreter(const std::string &scriptPath, const RouteConfig &route)
{
	std::string extension = "";
	size_t dotPos = scriptPath.find_last_of('.');
	if (dotPos != std::string::npos)
	{
		extension = scriptPath.substr(dotPos);
	}

	size_t qPos = extension.find('?');
	if (qPos != std::string::npos)
		extension = extension.substr(0, qPos);

	auto it = route.cgiInterpreters.find(extension);
	if (it != route.cgiInterpreters.end())
	{
		return it->second;
	}

	return "";
}

void CgiRequestHandler::checkTimeout()
{
	if (responseFinished_)
		return;

	auto now = std::chrono::steady_clock::now();
	if (now - startTime_ > CGI_TIMEOUT_LIMIT)
	{
		std::cerr << "[CGI] Timeout reached (" << CGI_TIMEOUT_LIMIT.count() << "s). Terminating." << std::endl;
		manager_.onRequestError();
	}
}

void CgiRequestHandler::onBodyData(std::span<const char> data)
{
	if (!cgiHandler_)
		return;
	size_t queued = cgiHandler_->queueWrite(data);
	if (queued > PIPE_WRITE_HIGH_WATER_MARK)
		manager_.setReadingBody(false);
}

void CgiRequestHandler::notifyResponseBuffer(size_t bufferSize)
{
	if (!cgiHandler_)
		return;

	// Backpressure: Send buffer is no longer full, resume reading from CGI
	if (bufferSize <= CLIENT_SEND_HIGH_WATER_MARK)
	{
		cgiHandler_->startReading();
	}
}
