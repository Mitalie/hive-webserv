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
	Open,
	Close,
	Semicolon
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
			throw std::runtime_error("Tokenizer: no more tokens available");
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
			if (c == '{')
				return Token{TokenType::Open, "{"};
			else if (c == '}')
				return Token{TokenType::Close, "}"};
			else if (c == ';')
				return Token{TokenType::Semicolon, ";"};
		}
		size_t start = pos;
		while (pos < input.size() && !isWhitespace(input[pos]) && !isSymbol(input[pos]))
			++pos;
		std::string val = input.substr(start, pos - start);
		// If we hit a comment, skip the rest of the line
		if (pos < input.size() && input[pos] == '#')
		{
			while (pos < input.size() && input[pos] != '\n')
				++pos;
		}
		return Token{TokenType::String, val};
	}

private:
	std::string input;
	size_t pos;
	void skipWhitespace()
	{
		while (pos < input.size())
		{
			while (pos < input.size() && isWhitespace(input[pos]))
				++pos;
			// Skip comments
			if (pos < input.size() && input[pos] == '#')
			{
				while (pos < input.size() && input[pos] != '\n')
					++pos;
				// Skip the newline after the comment
				if (pos < input.size() && input[pos] == '\n')
					++pos;
			}
			else
			{
				break;
			}
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
			if (next.type == TokenType::Open)
			{
				ServerConfig server = parseServer(tokenizer);
				serversByPort[server.listener].push_back(server);
			}
			else
			{
				throw std::runtime_error("ConfigParser: expected '{' after 'server'");
			}
		}
		else
		{
			throw std::runtime_error("ConfigParser: unexpected token outside server block");
		}
	}
	return serversByPort;
}

ServerConfig ConfigParser::parseServer(Tokenizer &tokenizer)
{
	ServerConfig server;
	while (tokenizer.hasNext())
	{
		Token t = tokenizer.nextToken();
		if (t.type == TokenType::Close)
		{
			return server;
		}
		std::string currentStatement; // directive name for error context
		if (t.type == TokenType::String && t.value == "location")
		{
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
		else if (t.type == TokenType::String && t.value == "listen")
		{
			currentStatement = "listen";
			Token hostToken = tokenizer.nextToken();
			if (hostToken.type != TokenType::String)
				throw std::runtime_error("Malformed listen statement: missing host");
			Token portToken = tokenizer.nextToken();
			if (portToken.type != TokenType::String)
				throw std::runtime_error("Malformed listen statement: missing port");
			server.listener.host = hostToken.value;
			server.listener.port = portToken.value;
		}
		else if (t.type == TokenType::String && t.value == "server_name")
		{
			currentStatement = "server_name";
			Token nameToken = tokenizer.nextToken();
			if (nameToken.type != TokenType::String)
				throw std::runtime_error("Malformed server_name statement: missing name");
			server.serverNames.push_back(nameToken.value);
		}
		else if (t.type == TokenType::String && t.value == "error_page")
		{
			currentStatement = "error_page";
			Token codeToken = tokenizer.nextToken();
			Token pathToken = tokenizer.nextToken();
			if (codeToken.type != TokenType::String || pathToken.type != TokenType::String)
				throw std::runtime_error("Malformed error_page statement: expected code and path");
			server.errorPages[std::stoi(codeToken.value)] = pathToken.value;
		}
		else if (t.type == TokenType::String && t.value == "client_max_body_size")
		{
			currentStatement = "client_max_body_size";
			Token sizeToken = tokenizer.nextToken();
			if (sizeToken.type != TokenType::String)
				throw std::runtime_error("Malformed client_max_body_size statement: missing size");
			server.clientMaxBodySize = std::stoul(sizeToken.value);
		}
		else
		{
			throw std::runtime_error("ConfigParser: unexpected token in server block");
		}
		Token semi = tokenizer.nextToken();
		if (semi.type != TokenType::Semicolon)
		{
			throw std::runtime_error("Malformed " + currentStatement + " statement: missing semicolon");
		}
	}
	return server;
}

RouteConfig ConfigParser::parseLocation(Tokenizer &tokenizer, const std::string &locationPath)
{
	RouteConfig route;
	route.path = locationPath;
	while (tokenizer.hasNext())
	{
		Token t = tokenizer.nextToken();
		if (t.type == TokenType::Close)
		{
			return route;
		}
		if (t.type == TokenType::String && t.value == "redirect")
		{
			Token codeToken = tokenizer.nextToken();
			Token locationToken = tokenizer.nextToken();
			Token semi = tokenizer.nextToken();
			if (codeToken.type != TokenType::String || locationToken.type != TokenType::String || semi.type != TokenType::Semicolon)
				throw std::runtime_error("Malformed redirect statement: expected status code, location and semicolon");
			route.redirectCode = std::stoi(codeToken.value);
			route.redirect = locationToken.value;
			continue;
		}
		else if (t.type == TokenType::String && (t.value == "root" || t.value == "index" || t.value == "autoindex" || t.value == "upload_store"))
		{
			Token valueToken = tokenizer.nextToken();
			Token semi = tokenizer.nextToken();
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
		else if (t.type == TokenType::String && t.value == "methods")
		{
			while (true)
			{
				Token methodToken = tokenizer.nextToken();
				if (methodToken.type == TokenType::Semicolon)
					break;
				if (methodToken.type != TokenType::String)
					throw std::runtime_error("Malformed methods statement");
				route.allowedMethods.push_back(methodToken.value);
			}
			continue;
		}
		else if (t.type == TokenType::String && t.value == "cgi_ext")
		{
			Token extToken = tokenizer.nextToken();
			Token pathToken = tokenizer.nextToken();
			Token semi = tokenizer.nextToken();
			if (extToken.type != TokenType::String || pathToken.type != TokenType::String || semi.type != TokenType::Semicolon)
				throw std::runtime_error("Malformed cgi_ext statement");
			route.cgiInterpreters[extToken.value] = pathToken.value;
			continue;
		}
		else
		{
			throw std::runtime_error("ConfigParser: unexpected token in location block");
		}
	}
	return route;
}
