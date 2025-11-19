#pragma once
#include <string>
#include <istream>
#include <stdexcept>

enum class TokenType {
    String,
    Open,
    Close,
    Semicolon,
    Eof,
    Unknown
};
struct Token {
    TokenType type;
    std::string value;
};

class Tokenizer {
public:
    Tokenizer(std::istream& in);
    Token nextToken();
private:
    std::istream& in;
    char buffer;
    bool hasBuffered;
    char peek();
    char get();
    void skipWhitespace();
    bool isWhitespace(char c) const;
    bool isSymbol(char c) const;
};
