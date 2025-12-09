#pragma once

/*
	Include all request handler headers for convenience.
	Used by router.cpp to access all handler implementations.
*/

#include "FileRequestHandler.hpp"
#include "CgiRequestHandler.hpp"
#include "UploadRequestHandler.hpp"
#include "RedirectRequestHandler.hpp"
#include "ErrorRequestHandler.hpp"
#include "AutoindexRequestHandler.hpp"
#include "DeleteRequestHandler.hpp"
