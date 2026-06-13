#pragma once

#include <string>

namespace qle {
namespace lexer {

enum class TokenType {
    IDENTIFIER,
    NUMBER,
    STRING,
    
    // Keywords
    FROM,
    WHERE,
    SELECT,
    LIMIT,
    ORDER,
    BY,
    GROUP,
    ASC,
    DESC,
    JOIN,
    ON,
    
    // Operators
    GREATER_THAN,
    LESS_THAN,
    EQUALS,
    NOT_EQUALS,
    GREATER_EQUALS,
    LESS_EQUALS,
    AND,
    OR,
    
    // Punctuation
    COMMA,
    STAR,
    LEFT_PAREN,
    RIGHT_PAREN,
    
    END_OF_FILE,
    UNKNOWN
};

struct Token {
    TokenType type;
    std::string value;
    size_t line;
    size_t col;
};

std::string ToString(TokenType type);

} // namespace lexer
} // namespace qle
