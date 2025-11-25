#include <iostream> // std::cout
#include <string>	// std::string, std::stoi, std::stoul
#include <vector>	// std::vector
#include <map>		// std::map
#include <fstream>	// std::ifstream
#include <cstddef>	  // size_t
#include "Config.hpp" // RouteConfig, ServerConfig, PortServerMap
#include <stdexcept>  // std::runtime_error
#include <iterator>	  // std::istreambuf_iterator

#include "Tokenizer.hpp"

// ServerConfig directive handlers
void ServerConfig::handleListen(Tokenizer &tokenizer) {
	Token hostToken = tokenizer.nextToken();
	if (hostToken.type != TokenType::String)
		throw std::runtime_error("Malformed listen statement: missing host");
	Token portToken = tokenizer.nextToken();
	if (portToken.type != TokenType::String)
		throw std::runtime_error("Malformed listen statement: missing port");
	listener.host = hostToken.value;
	listener.port = portToken.value;
}

void ServerConfig::handleServerName(Tokenizer &tokenizer) {
	Token nameToken = tokenizer.nextToken();
	if (nameToken.type != TokenType::String)
		throw std::runtime_error("Malformed server_name statement: missing name");
	serverNames.push_back(nameToken.value);
}

void ServerConfig::handleErrorPage(Tokenizer &tokenizer) {
	Token codeToken = tokenizer.nextToken();
	Token pathToken = tokenizer.nextToken();
	if (codeToken.type != TokenType::String || pathToken.type != TokenType::String)
		throw std::runtime_error("Malformed error_page statement: expected code and path");
	errorPages[std::stoi(codeToken.value)] = pathToken.value;
}

void ServerConfig::handleClientMaxBodySize(Tokenizer &tokenizer) {
	Token sizeToken = tokenizer.nextToken();
	if (sizeToken.type != TokenType::String)
		throw std::runtime_error("Malformed client_max_body_size statement: missing size");
	clientMaxBodySize = std::stoul(sizeToken.value);
}

PortServerMap ConfigParser::parse(const std::string& filename)
{
	std::ifstream file(filename.c_str());
	if (!file.is_open())
		throw std::runtime_error("Could not open config file");
	PortServerMap serversByPort; // Declare and initialize the variable here
	Tokenizer tokenizer(file); // Now streams directly from file
	while (true) {
		Token token = tokenizer.nextToken();
		if (token.type == TokenType::Eof)
			break;
		if (token.type == TokenType::String && token.value == "server") {
			Token next = tokenizer.nextToken();
			if (next.type == TokenType::Open) {
				ServerConfig server = parseServer(tokenizer);
				serversByPort[server.listener].push_back(server);
			}
			else {
				throw std::runtime_error("ConfigParser: expected '{' after 'server'");
			}
		}
		else {
			throw std::runtime_error("ConfigParser: unexpected token outside server block");
		}
	}
	return serversByPort;
}

ServerConfig ConfigParser::parseServer(Tokenizer& tokenizer)
{
	ServerConfig server;
	while (true) {
		Token t = tokenizer.nextToken();
		if (t.type == TokenType::Eof)
			throw std::runtime_error("ConfigParser: unexpected end of file in server block (missing closing brace)");
		if (t.type == TokenType::Close) {
			return server;
		}
		if (t.type != TokenType::String)
			throw std::runtime_error("ConfigParser: unexpected token in server block");

		auto it = serverDirectiveMap.find(t.value);
		ServerDirective directive = (it != serverDirectiveMap.end()) ? it->second : ServerDirective::Unknown;
		std::string currentStatement = t.value;
		switch (directive) {
		case ServerDirective::Location: {
			Token pathToken = tokenizer.nextToken();
			if (pathToken.type != TokenType::String)
				throw std::runtime_error("Malformed location statement: missing path");
			Token openToken = tokenizer.nextToken();
			if (openToken.type != TokenType::Open)
				throw std::runtime_error("Malformed location statement: missing opening brace");
			RouteConfig route = parseLocation(tokenizer, pathToken.value);
			server.routes.push_back(route);
			continue; // location blocks don't end with semicolon
		}
		case ServerDirective::Listen:
			server.handleListen(tokenizer);
			break;
		case ServerDirective::ServerName:
			server.handleServerName(tokenizer);
			break;
		case ServerDirective::ErrorPage:
			server.handleErrorPage(tokenizer);
			break;
		case ServerDirective::ClientMaxBodySize:
			server.handleClientMaxBodySize(tokenizer);
			break;
		default:
			throw std::runtime_error("ConfigParser: unexpected token in server block");
		}
		Token semi = tokenizer.nextToken();
		if (semi.type == TokenType::Eof)
			throw std::runtime_error("ConfigParser: unexpected end of file in server block (missing semicolon)");
		if (semi.type != TokenType::Semicolon)
			throw std::runtime_error("Malformed " + currentStatement + " statement: missing semicolon");
	}
	return server;
}

RouteConfig ConfigParser::parseLocation(Tokenizer& tokenizer, const std::string& locationPath)
{
	RouteConfig route;
	route.path = locationPath;
	while (true) {
		Token t = tokenizer.nextToken();
		if (t.type == TokenType::Eof)
			throw std::runtime_error("ConfigParser: unexpected end of file in location block (missing closing brace)");
		if (t.type == TokenType::Close) {
			return route;
		}
		if (t.type == TokenType::String && t.value == "redirect") {
			Token codeToken = tokenizer.nextToken();
			Token locationToken = tokenizer.nextToken();
			Token semi = tokenizer.nextToken();
			if (semi.type == TokenType::Eof)
				throw std::runtime_error("ConfigParser: unexpected end of file in location block (missing semicolon)");
			if (codeToken.type != TokenType::String || locationToken.type != TokenType::String || semi.type != TokenType::Semicolon)
				throw std::runtime_error("Malformed redirect statement: expected status code, location and semicolon");
			route.redirectCode = std::stoi(codeToken.value);
			route.redirect = locationToken.value;
			continue;
		}
		else if (t.type == TokenType::String && (t.value == "root" || t.value == "index" || t.value == "autoindex" || t.value == "upload_store")) {
			Token valueToken = tokenizer.nextToken();
			Token semi = tokenizer.nextToken();
			if (semi.type == TokenType::Eof)
				throw std::runtime_error("ConfigParser: unexpected end of file in location block (missing semicolon)");
			if (semi.type != TokenType::Semicolon)
				throw std::runtime_error("Malformed " + t.value + " statement: missing semicolon");
			if (t.value == "root")
				route.root = valueToken.value;
			else if (t.value == "index")
				route.index = valueToken.value;
			else if (t.value == "autoindex")
				route.autoindex = (valueToken.value == "on");
			else if (t.value == "upload_store")
				route.uploadStore = valueToken.value;
			continue;
		}
		else if (t.type == TokenType::String && t.value == "methods") {
			while (true) {
				Token methodToken = tokenizer.nextToken();
				if (methodToken.type == TokenType::Eof)
					throw std::runtime_error("ConfigParser: unexpected end of file in location block (missing semicolon)");
				if (methodToken.type == TokenType::Semicolon)
					break;
				if (methodToken.type != TokenType::String)
					throw std::runtime_error("Malformed methods statement");
				route.allowedMethods.push_back(methodToken.value);
			}
			continue;
		}
		else if (t.type == TokenType::String && t.value == "cgi_ext") {
			Token extToken = tokenizer.nextToken();
			Token pathToken = tokenizer.nextToken();
			Token semi = tokenizer.nextToken();
			if (semi.type == TokenType::Eof)
				throw std::runtime_error("ConfigParser: unexpected end of file in location block (missing semicolon)");
			if (extToken.type != TokenType::String || pathToken.type != TokenType::String || semi.type != TokenType::Semicolon)
				throw std::runtime_error("Malformed cgi_ext statement");
			route.cgiInterpreters[extToken.value] = pathToken.value;
			continue;
		}
		else {
			throw std::runtime_error("ConfigParser: unexpected token in location block");
		}
	}
	return route;
}
