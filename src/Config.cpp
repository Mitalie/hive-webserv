#include "Config.hpp" // RouteConfig, ServerConfig, PortServerMap

#include <string>	 // std::string, std::stoi, std::stoul
#include <vector>	 // std::vector
#include <map>		 // std::map
#include <fstream>	 // std::ifstream
#include <stdexcept> // std::runtime_error
#include <algorithm> // std::sort

#include "Tokenizer.hpp"

PortServerMap parseConfig(const std::string &filename)
{
	std::ifstream file(filename);
	if (!file.is_open())
		throw std::runtime_error("Could not open config file");
	PortServerMap serversByPort;
	Tokenizer tokenizer(file);
	while (true)
	{
		Token token = tokenizer.nextToken();
		if (token.type == TokenType::Eof)
			break;
		if (token.type == TokenType::String && token.value == "server")
		{
			ServerConfig server(tokenizer);
			serversByPort[server.listener].push_back(server);
		}
		else
		{
			throw std::runtime_error("ConfigParser: unexpected token outside server block");
		}
	}
	return serversByPort;
}

ServerConfig::ServerConfig(Tokenizer &tokenizer)
	: clientMaxBodySize(0)
{
	Token next = tokenizer.nextToken();
	if (next.type != TokenType::Open)
	{
		throw std::runtime_error("ConfigParser: expected '{' after 'server'");
	}
	while (true)
	{
		Token t = tokenizer.nextToken();
		if (t.type == TokenType::Eof)
			throw std::runtime_error("ConfigParser: unexpected end of file in server block (missing closing brace)");
		if (t.type == TokenType::Close)
		{
			// Sort routes by descending path length for efficient matching
			std::sort(routes.begin(), routes.end(), [](const RouteConfig &a, const RouteConfig &b)
					  { return a.path.length() > b.path.length(); });
			return;
		}
		if (t.type != TokenType::String)
			throw std::runtime_error("ConfigParser: expected directive name in server block");

		auto it = serverDirectiveMap.find(t.value);
		ServerDirective directive = (it != serverDirectiveMap.end()) ? it->second : ServerDirective::Unknown;
		std::string currentStatement = t.value;
		switch (directive)
		{
		case ServerDirective::Location:
		{
			RouteConfig route(tokenizer);
			routes.push_back(route);
			continue; // location blocks don't end with semicolon
		}
		case ServerDirective::Listen:
			handleListen(tokenizer);
			break;
		case ServerDirective::ServerName:
			handleServerName(tokenizer);
			break;
		case ServerDirective::ErrorPage:
			handleErrorPage(tokenizer);
			break;
		case ServerDirective::ClientMaxBodySize:
			handleClientMaxBodySize(tokenizer);
			break;
		default:
			throw std::runtime_error("ConfigParser: unknown directive '" + t.value + "' in server block");
		}
		Token semi = tokenizer.nextToken();
		if (semi.type == TokenType::Eof)
			throw std::runtime_error("ConfigParser: unexpected end of file in server block (missing semicolon)");
		if (semi.type != TokenType::Semicolon)
			throw std::runtime_error("Malformed " + currentStatement + " statement: missing semicolon");
	}
}

// ServerConfig directive handlers
void ServerConfig::handleListen(Tokenizer &tokenizer)
{
	Token hostToken = tokenizer.nextToken();
	if (hostToken.type != TokenType::String)
		throw std::runtime_error("Malformed listen statement: missing host");
	Token portToken = tokenizer.nextToken();
	if (portToken.type != TokenType::String)
		throw std::runtime_error("Malformed listen statement: missing port");
	listener.host = hostToken.value;
	// Validate port is numeric and in valid range
	int portNum;
	try
	{
		portNum = std::stoi(portToken.value);
	}
	catch (...)
	{
		throw std::runtime_error("Invalid port: must be a valid number");
	}
	if (portNum < 0 || portNum > 65535)
		throw std::runtime_error("Invalid port number: must be between 0 and 65535");
	listener.port = portToken.value;
}

void ServerConfig::handleServerName(Tokenizer &tokenizer)
{
	Token nameToken = tokenizer.nextToken();
	if (nameToken.type != TokenType::String)
		throw std::runtime_error("Malformed server_name statement: missing name");
	serverNames.push_back(nameToken.value);
}

void ServerConfig::handleErrorPage(Tokenizer &tokenizer)
{
	Token codeToken = tokenizer.nextToken();
	Token pathToken = tokenizer.nextToken();
	if (codeToken.type != TokenType::String || pathToken.type != TokenType::String)
		throw std::runtime_error("Malformed error_page statement: expected code and path");
	try
	{
		int code = std::stoi(codeToken.value);
		if (code < 100 || code > 599)
			throw std::runtime_error("Invalid HTTP error code: must be between 100 and 599");
		errorPages[code] = pathToken.value;
	}
	catch (const std::invalid_argument &e)
	{
		throw std::runtime_error("error_page directive: invalid code value '" + codeToken.value + "'");
	}
	catch (const std::out_of_range &e)
	{
		throw std::runtime_error("error_page directive: code value out of range '" + codeToken.value + "'");
	}
}

void ServerConfig::handleClientMaxBodySize(Tokenizer &tokenizer)
{
	Token sizeToken = tokenizer.nextToken();
	if (sizeToken.type != TokenType::String)
		throw std::runtime_error("Malformed client_max_body_size statement: missing size");
	try
	{
		clientMaxBodySize = std::stoul(sizeToken.value);
	}
	catch (const std::invalid_argument &e)
	{
		throw std::runtime_error("Invalid value for client_max_body_size: '" + sizeToken.value + "' is not a valid number");
	}
	catch (const std::out_of_range &e)
	{
		throw std::runtime_error("Value for client_max_body_size out of range: '" + sizeToken.value + "'");
	}
}

RouteConfig::RouteConfig(Tokenizer &tokenizer)
	: autoindex(false), redirectCode(0)
{
	Token pathToken = tokenizer.nextToken();
	if (pathToken.type != TokenType::String)
		throw std::runtime_error("Malformed location statement: missing path");
	Token openToken = tokenizer.nextToken();
	if (openToken.type != TokenType::Open)
		throw std::runtime_error("Malformed location statement: missing opening brace");
	path = pathToken.value;
	isDirectoryRoute = path.back() == '/';
	if (isDirectoryRoute)
		path.pop_back();
	while (true)
	{
		Token t = tokenizer.nextToken();
		if (t.type == TokenType::Eof)
			throw std::runtime_error("ConfigParser: unexpected end of file in location block (missing closing brace)");
		if (t.type == TokenType::Close)
		{
			return;
		}
		if (t.type != TokenType::String)
			throw std::runtime_error("ConfigParser: expected directive name in location block");

		auto it = locationDirectiveMap.find(t.value);
		LocationDirective directive = (it != locationDirectiveMap.end()) ? it->second : LocationDirective::Unknown;
		std::string currentStatement = t.value;
		switch (directive)
		{
		case LocationDirective::Redirect:
			handleLocationRedirect(tokenizer);
			break;
		case LocationDirective::Root:
			handleLocationRoot(tokenizer);
			break;
		case LocationDirective::Index:
			handleLocationIndex(tokenizer);
			break;
		case LocationDirective::Autoindex:
			handleLocationAutoindex(tokenizer);
			break;
		case LocationDirective::UploadStore:
			handleLocationUploadStore(tokenizer);
			break;
		case LocationDirective::Methods:
			handleLocationMethods(tokenizer);
			continue; // methods ends with semicolon, handled inside
		case LocationDirective::CgiExt:
			handleLocationCgiExt(tokenizer);
			break;
		default:
			throw std::runtime_error("ConfigParser: unknown directive '" + t.value + "' in location block");
		}
		Token semi = tokenizer.nextToken();
		if (semi.type == TokenType::Eof)
			throw std::runtime_error("ConfigParser: unexpected end of file in location block (missing semicolon)");
		if (semi.type != TokenType::Semicolon)
			throw std::runtime_error("Malformed " + currentStatement + " statement: missing semicolon");
	}
}

// RouteConfig directive helpers
void RouteConfig::handleLocationRedirect(Tokenizer &tokenizer)
{
	Token codeToken = tokenizer.nextToken();
	Token locationToken = tokenizer.nextToken();
	if (codeToken.type != TokenType::String || locationToken.type != TokenType::String)
		throw std::runtime_error("Malformed redirect statement: expected status code and location");
	try
	{
		int code = std::stoi(codeToken.value);
		if (code != 301 && code != 302 && code != 303 && code != 307 && code != 308)
			throw std::runtime_error("Invalid redirect code: must be a valid HTTP redirect status (301, 302, 303, 307, 308)");
		redirectCode = code;
	}
	catch (const std::invalid_argument &e)
	{
		throw std::runtime_error("Malformed redirect statement: invalid status code '" + codeToken.value + "'");
	}
	catch (const std::out_of_range &e)
	{
		throw std::runtime_error("Malformed redirect statement: status code out of range '" + codeToken.value + "'");
	}
	redirect = locationToken.value;
}

void RouteConfig::handleLocationRoot(Tokenizer &tokenizer)
{
	Token valueToken = tokenizer.nextToken();
	if (valueToken.type != TokenType::String)
		throw std::runtime_error("Malformed root statement: missing value");
	root = valueToken.value;
}

void RouteConfig::handleLocationIndex(Tokenizer &tokenizer)
{
	Token valueToken = tokenizer.nextToken();
	if (valueToken.type != TokenType::String)
		throw std::runtime_error("Malformed index statement: missing value");
	index = valueToken.value;
}

void RouteConfig::handleLocationAutoindex(Tokenizer &tokenizer)
{
	Token valueToken = tokenizer.nextToken();
	if (valueToken.type != TokenType::String)
		throw std::runtime_error("Malformed autoindex statement: missing value");
	autoindex = (valueToken.value == "on");
}

void RouteConfig::handleLocationUploadStore(Tokenizer &tokenizer)
{
	Token valueToken = tokenizer.nextToken();
	if (valueToken.type != TokenType::String)
		throw std::runtime_error("Malformed upload_store statement: missing value");
	uploadStore = valueToken.value;
}

void RouteConfig::handleLocationMethods(Tokenizer &tokenizer)
{
	while (true)
	{
		Token methodToken = tokenizer.nextToken();
		if (methodToken.type == TokenType::Eof)
			throw std::runtime_error("ConfigParser: unexpected end of file in location block (missing semicolon)");
		if (methodToken.type == TokenType::Semicolon)
			break;
		if (methodToken.type != TokenType::String)
			throw std::runtime_error("Malformed methods statement");
		allowedMethods.push_back(methodToken.value);
	}
}

void RouteConfig::handleLocationCgiExt(Tokenizer &tokenizer)
{
	Token extToken = tokenizer.nextToken();
	Token pathToken = tokenizer.nextToken();
	if (extToken.type != TokenType::String || pathToken.type != TokenType::String)
		throw std::runtime_error("Malformed cgi_ext statement");
	cgiInterpreters[extToken.value] = pathToken.value;
}
