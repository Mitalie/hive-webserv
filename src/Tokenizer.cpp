#include "Tokenizer.hpp"

Tokenizer::Tokenizer(std::istream &in) : in(in), buffer(0), hasBuffered(false) {}

Token Tokenizer::nextToken() {
    skipWhitespace();
    if (!in || in.eof())
        return Token{TokenType::Eof, ""};
    char c = peek();
    if (c == '"') {
        get(); // consume opening quote
        std::string val;
        while (in && !in.eof()) {
            char ch = get();
            if (ch == '"') break;
            val += ch;
        }
        return Token{TokenType::String, val};
    } else if (isSymbol(c)) {
        get();
        if (c == '{') return Token{TokenType::Open, "{"};
        if (c == '}') return Token{TokenType::Close, "}"};
        if (c == ';') return Token{TokenType::Semicolon, ";"};
    }
    // Read a string token
    std::string val;
    while (in && !in.eof()) {
        char ch = peek();
        if (isWhitespace(ch) || isSymbol(ch) || ch == '#') break;
        val += get();
    }
    // If we hit a comment, skip the rest of the line
    if (in && !in.eof() && peek() == '#') {
        while (in && !in.eof()) {
            char ch = get();
            if (ch == '\n') break;
        }
    }
    return Token{TokenType::String, val};
}

char Tokenizer::peek() {
    if (!hasBuffered) {
        buffer = static_cast<char>(in.get());
        hasBuffered = true;
    }
    return buffer;
}
char Tokenizer::get() {
    if (hasBuffered) {
        hasBuffered = false;
        return buffer;
    }
    return static_cast<char>(in.get());
}

void Tokenizer::skipWhitespace() {
    while (in && !in.eof()) {
        char c = peek();
        if (isWhitespace(c)) {
            get();
            continue;
        }
        // Skip comments
        if (c == '#') {
            while (in && !in.eof()) {
                char ch = get();
                if (ch == '\n') break;
            }
            continue;
        }
        break;
    }
}

bool Tokenizer::isWhitespace(char c) const { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
bool Tokenizer::isSymbol(char c) const { return c == '{' || c == '}' || c == ';'; }
