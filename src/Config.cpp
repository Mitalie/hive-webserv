#include <iostream> // std::cout
#include <string>	// std::string, std::stoi, std::stoul
#include <vector>	// std::vector
#include <map>		// std::map
#include <fstream>	// std::ifstream
// #include <sstream>          // std::stringstream
#include <cstddef>	  // size_t
#include "Config.hpp" // RouteConfig, ServerConfig, PortServerMap
#include <stdexcept>  // std::runtime_error
#include <iterator>	  // std::istreambuf_iterator

enum class TokenType
{
	String,
	Symbol
};
struct Token
{
	TokenType type;
	std::string value;
};

class Tokenizer
{
public:
	Tokenizer(std::istream &in)
	{
		std::string fileContent((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
		input = fileContent;
		pos = 0;
	}
	bool hasNext()
	{
		skipWhitespace();
		return pos < input.size();
	}
	Token nextToken()
	{
		skipWhitespace();
		if (pos >= input.size())
			return Token{TokenType::String, ""};
		char c = input[pos];
		if (c == '"')
		{
			size_t start = ++pos;
			while (pos < input.size() && input[pos] != '"')
				++pos;
			std::string val = input.substr(start, pos - start);
			++pos;
			return Token{TokenType::String, val};
		}
		else if (isSymbol(c))
		{
			++pos;
			return Token{TokenType::Symbol, std::string(1, c)};
		}
		else
		{
			size_t start = pos;
			while (pos < input.size() && !isWhitespace(input[pos]) && !isSymbol(input[pos]) && input[pos] != '#')
				++pos;
			std::string val = input.substr(start, pos - start);
			// If we hit a comment, skip the rest of the line
			if (pos < input.size() && input[pos] == '#')
			{
				while (pos < input.size() && input[pos] != '\n')
					++pos;
			}
			// If the last character is a semicolon and not in quotes, split it as a symbol
			if (!val.empty() && val.back() == ';')
			{
				val.pop_back();
				pos = start + val.size();
				if (!val.empty())
				{
					return Token{TokenType::String, val};
				}
				else
				{
					return Token{TokenType::Symbol, ";"};
				}
			}
			return Token{TokenType::String, val};
		}
	}

private:
	std::string input;
	size_t pos;
	void skipWhitespace()
	{
		while (pos < input.size() && isWhitespace(input[pos]))
			++pos;
		if (pos < input.size() && input[pos] == '#')
		{
			while (pos < input.size() && input[pos] != '\n')
				++pos;
		}
	}
	bool isWhitespace(char c) const { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
	bool isSymbol(char c) const { return c == '{' || c == '}' || c == ';'; }
};

PortServerMap ConfigParser::parse(const std::string &filename)
{
	std::ifstream file(filename.c_str());
	if (!file.is_open())
		throw std::runtime_error("Could not open config file");
	Tokenizer tokenizer(file);
	PortServerMap serversByPort;
	while (tokenizer.hasNext())
	{
		Token token = tokenizer.nextToken();
		if (token.type == TokenType::String && token.value == "server")
		{
			Token next = tokenizer.nextToken();
			if (next.type == TokenType::Symbol && next.value == "{")
			{
				ServerConfig server = parseServer(tokenizer);
				serversByPort[server.listener].push_back(server);
			}
		}
	}
	return serversByPort;
}

ServerConfig ConfigParser::parseServer(Tokenizer &tokenizer)
{
	ServerConfig server;
	while (tokenizer.hasNext()) {
		Token t = tokenizer.nextToken();
		if (t.type == TokenType::Symbol && t.value == "}") {
			return server;
		}
		if (t.type == TokenType::String && t.value == "location") {
			Token pathToken = tokenizer.nextToken();
			if (pathToken.type != TokenType::String)
				throw std::runtime_error("Malformed location statement: missing path");
			Token braceToken = tokenizer.nextToken();
			if (braceToken.type != TokenType::Symbol || braceToken.value != "{")
				throw std::runtime_error("Malformed location statement: missing opening brace");
			RouteConfig route = parseLocation(tokenizer, pathToken.value);
			server.routes.push_back(route);
			continue;
		}
		if (t.type == TokenType::String && t.value == "listen") {
			Token hostToken = tokenizer.nextToken();
			if (hostToken.type != TokenType::String)
				throw std::runtime_error("Malformed listen statement: missing host");
			Token portToken = tokenizer.nextToken();
			if (portToken.type == TokenType::String) {
				server.listener.host = hostToken.value;
				server.listener.port = portToken.value;
				Token semi = tokenizer.nextToken();
				if (semi.type != TokenType::Symbol || semi.value != ";")
					throw std::runtime_error("Malformed listen statement: missing semicolon");
			} else if (portToken.type == TokenType::Symbol && portToken.value == ";") {
				size_t colon = hostToken.value.find(":");
				if (colon != std::string::npos) {
					server.listener.host = hostToken.value.substr(0, colon);
					server.listener.port = hostToken.value.substr(colon + 1);
				} else {
					throw std::runtime_error("Malformed listen statement: missing port");
				}
			} else {
				throw std::runtime_error("Malformed listen statement");
			}
			continue;
		}
		if (t.type == TokenType::String && t.value == "server_name") {
			Token nameToken = tokenizer.nextToken();
			if (nameToken.type != TokenType::String)
				throw std::runtime_error("Malformed server_name statement");
			server.serverNames.push_back(nameToken.value);
			Token semi = tokenizer.nextToken();
			if (semi.type != TokenType::Symbol || semi.value != ";")
				throw std::runtime_error("Malformed server_name statement: missing semicolon");
			continue;
		}
		if (t.type == TokenType::String && t.value == "error_page") {
			Token codeToken = tokenizer.nextToken();
			Token pathToken = tokenizer.nextToken();
			if (codeToken.type != TokenType::String || pathToken.type != TokenType::String)
				throw std::runtime_error("Malformed error_page statement");
			server.errorPages[std::stoi(codeToken.value)] = pathToken.value;
			Token semi = tokenizer.nextToken();
			if (semi.type != TokenType::Symbol || semi.value != ";")
				throw std::runtime_error("Malformed error_page statement: missing semicolon");
			continue;
		}
		if (t.type == TokenType::String && t.value == "client_max_body_size") {
			Token sizeToken = tokenizer.nextToken();
			if (sizeToken.type != TokenType::String)
				throw std::runtime_error("Malformed client_max_body_size statement");
			server.clientMaxBodySize = std::stoul(sizeToken.value);
			Token semi = tokenizer.nextToken();
			if (semi.type != TokenType::Symbol || semi.value != ";")
				throw std::runtime_error("Malformed client_max_body_size statement: missing semicolon");
			continue;
		}
		// Skip unknown tokens and statements
		if (t.type == TokenType::Symbol && t.value == "{")
			continue;
	}
	return server;
}

RouteConfig ConfigParser::parseLocation(Tokenizer &tokenizer, const std::string &locationPath)
{
	RouteConfig route;
	route.path = locationPath;
	while (tokenizer.hasNext()) {
		Token t = tokenizer.nextToken();
		if (t.type == TokenType::Symbol && t.value == "}") {
			return route;
		}
		if (t.type == TokenType::String && (t.value == "root" || t.value == "index" || t.value == "autoindex" || t.value == "upload_store" || t.value == "redirect")) {
			Token valueToken = tokenizer.nextToken();
			Token semi = tokenizer.nextToken();
			if (semi.type != TokenType::Symbol || semi.value != ";")
				throw std::runtime_error("Malformed location statement: missing semicolon");
			if (t.value == "root") route.root = valueToken.value;
			else if (t.value == "index") route.index = valueToken.value;
			else if (t.value == "autoindex") route.autoindex = (valueToken.value == "on");
			else if (t.value == "upload_store") route.uploadStore = valueToken.value;
			else if (t.value == "redirect") route.redirect = valueToken.value;
			continue;
		}
		if (t.type == TokenType::String && (t.value == "allowed_methods" || t.value == "methods")) {
			while (true) {
				Token methodToken = tokenizer.nextToken();
				if (methodToken.type == TokenType::Symbol && methodToken.value == ";") break;
				if (methodToken.type != TokenType::String)
					throw std::runtime_error("Malformed allowed_methods statement");
				route.allowedMethods.push_back(methodToken.value);
			}
			continue;
		}
		if (t.type == TokenType::String && t.value == "cgi_interpreter") {
			Token extToken = tokenizer.nextToken();
			Token pathToken = tokenizer.nextToken();
			Token semi = tokenizer.nextToken();
			if (extToken.type != TokenType::String || pathToken.type != TokenType::String || semi.type != TokenType::Symbol || semi.value != ";")
				throw std::runtime_error("Malformed cgi_interpreter statement");
			route.cgiInterpreters[extToken.value] = pathToken.value;
			continue;
		}
		// Skip unknown tokens and statements
		if (t.type == TokenType::Symbol && t.value == "{")
			continue;
	}
	return route;
}
