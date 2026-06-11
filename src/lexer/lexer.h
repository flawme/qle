#pragma once

#include "lexer/token.h"
#include <string>
#include <vector>

namespace qle {
namespace lexer {

class Lexer {
public:
    Lexer(const std::string& source);
    
    std::vector<Token> Tokenize();

private:
    std::string source_;
    size_t pos_;
    size_t line_;
    size_t col_;

    char Peek() const;
    char Advance();
    bool IsAtEnd() const;
    void SkipWhitespace();
    
    Token NextToken();
    Token ReadIdentifierOrKeyword();
    Token ReadNumber();
    Token ReadString();
};

} // namespace lexer
} // namespace qle
