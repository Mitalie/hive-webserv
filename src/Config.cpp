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
	std::vector<Token> tokens;
	while (tokenizer.hasNext())
	{
		tokens.clear();
		// Gather tokens for one statement/block
		while (tokenizer.hasNext())
		{
			Token t = tokenizer.nextToken();
			if (t.type == TokenType::Symbol && (t.value == ";" || t.value == "{" || t.value == "}"))
			{
				tokens.push_back(t);
				break;
			}
			tokens.push_back(t);
		}
		if (tokens.empty())
			continue;
		if (tokens.size() == 1 && tokens[0].type == TokenType::Symbol && tokens[0].value == "}")
			break;
		// Remove all semicolon tokens for statement validation
		std::vector<Token> filtered;
		for (size_t i = 0; i < tokens.size(); ++i)
		{
			if (!(tokens[i].type == TokenType::Symbol && tokens[i].value == ";"))
				filtered.push_back(tokens[i]);
		}
		// Debug output: print filtered tokens for each statement
		std::cout << "Statement tokens: ";
		for (size_t i = 0; i < filtered.size(); ++i)
		{
			std::cout << filtered[i].value << " ";
		}
		std::cout << std::endl;
		// listen host port (or listen host:port)
		if (filtered[0].type == TokenType::String && filtered[0].value == "listen")
		{
			if (filtered.size() == 3 && filtered[1].type == TokenType::String && filtered[2].type == TokenType::String)
			{
				server.listener.host = filtered[1].value;
				server.listener.port = filtered[2].value;
			}
			else if (filtered.size() == 2 && filtered[1].type == TokenType::String)
			{
				// Split host:port
				size_t colon = filtered[1].value.find(":");
				if (colon != std::string::npos)
				{
					server.listener.host = filtered[1].value.substr(0, colon);
					server.listener.port = filtered[1].value.substr(colon + 1);
				}
				else
				{
					throw std::runtime_error("Malformed listen statement: missing port");
				}
			}
		}
		// server_name name1 [name2 ...]
		else if (filtered[0].type == TokenType::String && filtered[0].value == "server_name" && filtered.size() == 2)
		{
			for (size_t i = 1; i < filtered.size(); ++i)
				if (filtered[i].type == TokenType::String)
					server.serverNames.push_back(filtered[i].value);
		}
		// error_page code path
		else if (filtered[0].type == TokenType::String && filtered[0].value == "error_page" && filtered.size() == 3 && filtered[1].type == TokenType::String && filtered[2].type == TokenType::String)
		{
			server.errorPages[std::stoi(filtered[1].value)] = filtered[2].value;
		}
		// client_max_body_size size
		else if (filtered[0].type == TokenType::String && filtered[0].value == "client_max_body_size" && filtered.size() == 2 && filtered[1].type == TokenType::String)
		{
			server.clientMaxBodySize = std::stoul(filtered[1].value); // Always bytes
		}
		// location path {
		else if (
			filtered.size() == 3 &&
			filtered[0].type == TokenType::String && filtered[0].value == "location" &&
			filtered[1].type == TokenType::String &&
			filtered[2].type == TokenType::Symbol && filtered[2].value == "{")
		{
			std::cout << "Entering parseLocation for path: " << filtered[1].value << std::endl;
			server.routes.push_back(parseLocation(tokenizer));
		}
		else
		{
			std::cout << "Malformed server statement. Filtered tokens:" << std::endl;
			for (size_t i = 0; i < filtered.size(); ++i)
			{
				std::cout << "  [" << i << "] type: " << (filtered[i].type == TokenType::String ? "String" : "Symbol") << ", value: '" << filtered[i].value << "'" << std::endl;
			}
			throw std::runtime_error("Invalid or malformed server statement");
		}
	}
	return server;
}

RouteConfig ConfigParser::parseLocation(Tokenizer &tokenizer)
{
	std::cout << "parseLocation called" << std::endl;
	RouteConfig route;
	std::vector<Token> tokens;
	while (tokenizer.hasNext())
	{
		tokens.clear();
		while (tokenizer.hasNext())
		{
			Token t = tokenizer.nextToken();
			if (t.type == TokenType::Symbol && (t.value == ";" || t.value == "{" || t.value == "}"))
			{
				tokens.push_back(t);
				break;
			}
			tokens.push_back(t);
		}
		if (tokens.empty())
			continue;
		if (tokens.size() == 1 && tokens[0].type == TokenType::Symbol && tokens[0].value == "}")
			break;
		// Remove all semicolon tokens for statement validation
		std::vector<Token> filtered;
		for (size_t i = 0; i < tokens.size(); ++i)
		{
			if (!(tokens[i].type == TokenType::Symbol && tokens[i].value == ";"))
				filtered.push_back(tokens[i]);
		}
		// root path
		if (filtered[0].type == TokenType::String && filtered[0].value == "root" && filtered.size() == 2 && filtered[1].type == TokenType::String)
		{
			route.root = filtered[1].value;
		}
		// index file
		else if (filtered[0].type == TokenType::String && filtered[0].value == "index" && filtered.size() == 2 && filtered[1].type == TokenType::String)
		{
			route.index = filtered[1].value;
		}
		// autoindex on|off
		else if (filtered[0].type == TokenType::String && filtered[0].value == "autoindex" && filtered.size() == 2 && filtered[1].type == TokenType::String)
		{
			route.autoindex = (filtered[1].value == "on");
		}
		// methods method1 [method2 ...]
		else if (filtered[0].type == TokenType::String && filtered[0].value == "methods" && filtered.size() >= 2)
		{
			for (size_t i = 1; i < filtered.size(); ++i)
				if (filtered[i].type == TokenType::String)
					route.allowedMethods.push_back(filtered[i].value);
		}
		// return code url
		else if (filtered[0].type == TokenType::String && filtered[0].value == "return" && filtered.size() == 3 && filtered[1].type == TokenType::String && filtered[2].type == TokenType::String)
		{
			route.redirect = filtered[2].value;
		}
		// cgi_ext ext interpreter
		else if (filtered[0].type == TokenType::String && filtered[0].value == "cgi_ext" && filtered.size() == 3 && filtered[1].type == TokenType::String && filtered[2].type == TokenType::String)
		{
			std::string ext = filtered[1].value;
			std::string interpreter = filtered[2].value;
			route.cgiInterpreters[ext] = interpreter;
		}
		// upload_store path
		else if (filtered[0].type == TokenType::String && filtered[0].value == "upload_store" && filtered.size() == 2 && filtered[1].type == TokenType::String)
		{
			route.uploadStore = filtered[1].value;
		}
		else
		{
			throw std::runtime_error("Invalid or malformed location statement");
		}
	}
	return route;
}
