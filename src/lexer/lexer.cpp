#include "lexer/lexer.h"
#include "errors/errors.h"
#include "security/limits.h"
#include "logger/logger.h"
#include <cctype>

namespace qle {
namespace lexer {

Lexer::Lexer(const std::string& source)
    : source_(source), pos_(0), line_(1), col_(1) {
    if (source_.length() > security::Limits::Get().max_query_size) {
        throw errors::SecurityError("Query size exceeds maximum allowed length", line_, col_);
    }
}

char Lexer::Peek() const {
    if (IsAtEnd()) return '\0';
    return source_[pos_];
}

char Lexer::Advance() {
    if (IsAtEnd()) return '\0';
    char c = source_[pos_++];
    if (c == '\n') {
        line_++;
        col_ = 1;
    } else {
        col_++;
    }
    return c;
}

bool Lexer::IsAtEnd() const {
    return pos_ >= source_.length();
}

void Lexer::SkipWhitespace() {
    while (!IsAtEnd()) {
        char c = Peek();
        if (std::isspace(static_cast<unsigned char>(c))) {
            Advance();
        } else {
            break;
        }
    }
}

std::vector<Token> Lexer::Tokenize() {
    std::vector<Token> tokens;
    Debug::DebugLog("Starting tokenization");
    
    while (!IsAtEnd()) {
        SkipWhitespace();
        if (IsAtEnd()) break;

        tokens.push_back(NextToken());

        if (tokens.size() > security::Limits::Get().max_tokens) {
            throw errors::SecurityError("Token count exceeds maximum allowed limit", line_, col_);
        }
    }

    tokens.push_back({TokenType::END_OF_FILE, "", line_, col_});
    Debug::DebugLog("Tokenization complete. Generated " + std::to_string(tokens.size()) + " tokens.");
    return tokens;
}

Token Lexer::NextToken() {
    char c = Peek();
    size_t start_line = line_;
    size_t start_col = col_;

    if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
        return ReadIdentifierOrKeyword();
    }
    if (std::isdigit(static_cast<unsigned char>(c))) {
        return ReadNumber();
    }
    if (c == '"' || c == '\'') {
        return ReadString();
    }

    Advance(); // Consume the character
    
    switch (c) {
        case ',': return {TokenType::COMMA, ",", start_line, start_col};
        case '*': return {TokenType::STAR, "*", start_line, start_col};
        case '(': return {TokenType::LEFT_PAREN, "(", start_line, start_col};
        case ')': return {TokenType::RIGHT_PAREN, ")", start_line, start_col};
        case '>':
            if (Peek() == '=') {
                Advance();
                return {TokenType::GREATER_EQUALS, ">=", start_line, start_col};
            }
            return {TokenType::GREATER_THAN, ">", start_line, start_col};
        case '<':
            if (Peek() == '=') {
                Advance();
                return {TokenType::LESS_EQUALS, "<=", start_line, start_col};
            }
            return {TokenType::LESS_THAN, "<", start_line, start_col};
        case '=':
            if (Peek() == '=') {
                Advance();
                return {TokenType::EQUALS, "==", start_line, start_col};
            }
            throw errors::LexerError("Single '=' is not supported. Use '==' for equality.", start_line, start_col);
        case '!':
            if (Peek() == '=') {
                Advance();
                return {TokenType::NOT_EQUALS, "!=", start_line, start_col};
            }
            throw errors::LexerError("Unexpected character '!'", start_line, start_col);
        default:
            throw errors::LexerError(std::string("Unexpected character: ") + c, start_line, start_col);
    }
}

Token Lexer::ReadIdentifierOrKeyword() {
    size_t start_line = line_;
    size_t start_col = col_;
    std::string value;

    while (!IsAtEnd() && (std::isalnum(static_cast<unsigned char>(Peek())) || Peek() == '_' || Peek() == '.' || Peek() == '/')) {
        value += Advance();
    }

    // Check keywords
    std::string lower_val;
    for (char c : value) {
        lower_val += std::tolower(static_cast<unsigned char>(c));
    }

    if (lower_val == "from") return {TokenType::FROM, lower_val, start_line, start_col};
    if (lower_val == "where") return {TokenType::WHERE, lower_val, start_line, start_col};
    if (lower_val == "select") return {TokenType::SELECT, lower_val, start_line, start_col};
    if (lower_val == "limit") return {TokenType::LIMIT, lower_val, start_line, start_col};
    if (lower_val == "order") return {TokenType::ORDER, lower_val, start_line, start_col};
    if (lower_val == "by") return {TokenType::BY, lower_val, start_line, start_col};
    if (lower_val == "group") return {TokenType::GROUP, lower_val, start_line, start_col};
    if (lower_val == "asc") return {TokenType::ASC, lower_val, start_line, start_col};
    if (lower_val == "desc") return {TokenType::DESC, lower_val, start_line, start_col};
    if (lower_val == "and") return {TokenType::AND, lower_val, start_line, start_col};
    if (lower_val == "or") return {TokenType::OR, lower_val, start_line, start_col};

    return {TokenType::IDENTIFIER, value, start_line, start_col};
}

Token Lexer::ReadNumber() {
    size_t start_line = line_;
    size_t start_col = col_;
    std::string value;

    while (!IsAtEnd() && std::isdigit(static_cast<unsigned char>(Peek()))) {
        value += Advance();
    }
    
    if (!IsAtEnd() && Peek() == '.') {
        value += Advance();
        while (!IsAtEnd() && std::isdigit(static_cast<unsigned char>(Peek()))) {
            value += Advance();
        }
    }

    return {TokenType::NUMBER, value, start_line, start_col};
}

Token Lexer::ReadString() {
    size_t start_line = line_;
    size_t start_col = col_;
    char quote = Advance(); // Consume opening quote
    std::string value;

    while (!IsAtEnd() && Peek() != quote) {
        value += Advance();
        if (value.length() > security::Limits::Get().max_string_length) {
            throw errors::SecurityError("String length exceeds maximum allowed limit", start_line, start_col);
        }
    }

    if (IsAtEnd()) {
        throw errors::LexerError("Unterminated string literal", start_line, start_col);
    }

    Advance(); // Consume closing quote
    return {TokenType::STRING, value, start_line, start_col};
}

} // namespace lexer
} // namespace qle
