#include "DummyRequestHandler.hpp"

#include <cstddef>
#include <iostream>
#include <span>

#include "Header.hpp"
#include "IRequestManager.hpp"

DummyRequestHandler::DummyRequestHandler(
	IRequestManager &manager,
	Header &&header)
{
	std::cout << "Got request!"
			  << "\n  Path: " << header.path()
			  << "\n  Body length: " << header.get("content-length")
			  << std::endl;
	manager.onRequestDone();
}

void DummyRequestHandler::onBodyData(std::span<const char> data)
{
	(void)data;
}

void DummyRequestHandler::notifyResponseBuffer(size_t bufferSize)
{
	(void)bufferSize;
}
