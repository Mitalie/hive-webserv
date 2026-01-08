#include "Tokenizer.hpp"

#include <istream>
#include <stdexcept>
#include <string>

Tokenizer::Tokenizer(std::istream &in) : in(in), buffer(0), hasBuffered(false) {}

Token Tokenizer::nextToken()
{
	skipWhitespace();
	if (!in || in.eof())
		return Token{TokenType::Eof, ""};
	char c = peek();
	if (c == '"')
	{
		get(); // consume opening quote
		std::string val;
		while (in && !in.eof())
		{
			char ch = get();
			if (ch == '"')
			{
				return Token{TokenType::String, val};
			}
			val += ch;
		}
		throw std::runtime_error("Unterminated quoted string");
	}
	else if (isSymbol(c))
	{
		get();
		if (c == '{')
			return Token{TokenType::Open, "{"};
		if (c == '}')
			return Token{TokenType::Close, "}"};
		if (c == ';')
			return Token{TokenType::Semicolon, ";"};
		// Defensive: if isSymbol is extended, handle unknown symbols explicitly
		return Token{TokenType::Unknown, std::string(1, c)};
	}
	// Read a string token
	std::string val;
	while (in && !in.eof())
	{
		char ch = peek();
		if (isWhitespace(ch) || isSymbol(ch) || ch == '#')
			break;
		val += get();
	}
	if (val.empty())
		return Token{TokenType::Eof, ""};
	return Token{TokenType::String, val};
}

char Tokenizer::peek()
{
	if (!hasBuffered)
	{
		buffer = in.get();
		hasBuffered = true;
	}
	return buffer;
}
char Tokenizer::get()
{
	if (hasBuffered)
	{
		hasBuffered = false;
		return buffer;
	}
	return in.get();
}

void Tokenizer::skipWhitespace()
{
	while (in && !in.eof())
	{
		char c = peek();
		if (isWhitespace(c))
		{
			get();
			continue;
		}
		// Skip comments
		if (c == '#')
		{
			while (in && !in.eof())
			{
				char ch = get();
				if (ch == '\n')
					break;
			}
			continue;
		}
		break;
	}
}

bool Tokenizer::isWhitespace(char c) const { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
bool Tokenizer::isSymbol(char c) const { return c == '{' || c == '}' || c == ';'; }
