#include "lexer/token.h"

namespace qle {
namespace lexer {

std::string ToString(TokenType type) {
    switch (type) {
        case TokenType::IDENTIFIER: return "IDENTIFIER";
        case TokenType::NUMBER: return "NUMBER";
        case TokenType::STRING: return "STRING";
        case TokenType::FROM: return "FROM";
        case TokenType::WHERE: return "WHERE";
        case TokenType::SELECT: return "SELECT";
        case TokenType::LIMIT: return "LIMIT";
        case TokenType::ORDER: return "ORDER";
        case TokenType::BY: return "BY";
        case TokenType::GROUP: return "GROUP";
        case TokenType::ASC: return "ASC";
        case TokenType::DESC: return "DESC";
        case TokenType::GREATER_THAN: return ">";
        case TokenType::LESS_THAN: return "<";
        case TokenType::EQUALS: return "==";
        case TokenType::NOT_EQUALS: return "!=";
        case TokenType::GREATER_EQUALS: return ">=";
        case TokenType::LESS_EQUALS: return "<=";
        case TokenType::AND: return "AND";
        case TokenType::OR: return "OR";
        case TokenType::COMMA: return ",";
        case TokenType::STAR: return "*";
        case TokenType::LEFT_PAREN: return "(";
        case TokenType::RIGHT_PAREN: return ")";
        case TokenType::END_OF_FILE: return "EOF";
        case TokenType::UNKNOWN: return "UNKNOWN";
        default: return "UNKNOWN";
    }
}

} // namespace lexer
} // namespace qle
