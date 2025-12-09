#pragma once

#include "IRequestHandler.hpp"
#include "IRequestManager.hpp"
#include "Header.hpp"

/*
	Dummy request handler for testing client handler.

	TODO: delete me when real request handlers exist
*/
class DummyRequestHandler : public IRequestHandler
{
public:
	DummyRequestHandler(
		IRequestManager &manager,
		Header &&header);
	DummyRequestHandler(const DummyRequestHandler &) = delete;
	DummyRequestHandler &operator=(const DummyRequestHandler &) = delete;

	virtual void onBodyData(std::span<const char> data) override;
	virtual void notifyResponseBuffer(size_t bufferSize) override;
};
