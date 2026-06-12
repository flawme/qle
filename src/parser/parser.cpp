#include "parser/parser.h"
#include "errors/errors.h"
#include "security/limits.h"
#include "logger/logger.h"

namespace qle {
namespace parser {

Parser::Parser(std::vector<lexer::Token> tokens)
    : tokens_(std::move(tokens)), current_(0), ast_nodes_created_(0), recursion_depth_(0) {}

Parser::RecursionGuard::RecursionGuard(Parser* parser) : parser_(parser) {
    parser_->recursion_depth_++;
    if (parser_->recursion_depth_ > security::Limits::Get().max_recursion_depth) {
        throw errors::SecurityError("Maximum recursion depth exceeded", parser_->Peek().line, parser_->Peek().col);
    }
}

Parser::RecursionGuard::~RecursionGuard() {
    parser_->recursion_depth_--;
}

std::unique_ptr<ast::QueryNode> Parser::Parse() {
    Debug::DebugLog("Starting parsing");
    
    std::unique_ptr<ast::SourceNode> source = nullptr;
    std::unique_ptr<ast::WhereNode> where_clause = nullptr;
    std::unique_ptr<ast::SelectNode> select_clause = nullptr;
    size_t limit = 0;
    bool has_limit = false;
    std::unique_ptr<ast::OrderByNode> order_by = nullptr;

    while (!IsAtEnd()) {
        if (Match({lexer::TokenType::FROM})) {
            if (source) {
                throw errors::ParserError("Multiple FROM clauses found", Previous().line, Previous().col);
            }
            source = ParseFrom();
        } else if (Match({lexer::TokenType::WHERE})) {
            if (where_clause) {
                throw errors::ParserError("Multiple WHERE clauses found", Previous().line, Previous().col);
            }
            where_clause = ParseWhere();
        } else if (Match({lexer::TokenType::SELECT})) {
            if (select_clause) {
                throw errors::ParserError("Multiple SELECT clauses found", Previous().line, Previous().col);
            }
            select_clause = ParseSelect();
        } else if (Match({lexer::TokenType::LIMIT})) {
            if (has_limit) {
                throw errors::ParserError("Multiple LIMIT clauses found", Previous().line, Previous().col);
            }
            limit = ParseLimit();
            has_limit = true;
        } else if (Match({lexer::TokenType::ORDER})) {
            if (order_by) {
                throw errors::ParserError("Multiple ORDER BY clauses found", Previous().line, Previous().col);
            }
            order_by = ParseOrderBy();
        } else {
            throw errors::ParserError("Unexpected token: " + Peek().value, Peek().line, Peek().col);
        }
    }

    if (!source) {
        throw errors::ParserError("Query must have a FROM clause");
    }
    if (!select_clause) {
        throw errors::ParserError("Query must have a SELECT clause");
    }

    TrackNodeCreation();
    Debug::DebugLog("Parsing complete");
    return std::make_unique<ast::QueryNode>(std::move(source), std::move(where_clause), std::move(select_clause), limit, std::move(order_by));
}

bool Parser::IsAtEnd() const {
    return Peek().type == lexer::TokenType::END_OF_FILE;
}

const lexer::Token& Parser::Peek() const {
    return tokens_[current_];
}

const lexer::Token& Parser::Previous() const {
    return tokens_[current_ - 1];
}

const lexer::Token& Parser::Advance() {
    if (!IsAtEnd()) current_++;
    return Previous();
}

bool Parser::Check(lexer::TokenType type) const {
    if (IsAtEnd()) return false;
    return Peek().type == type;
}

bool Parser::Match(std::initializer_list<lexer::TokenType> types) {
    for (auto type : types) {
        if (Check(type)) {
            Advance();
            return true;
        }
    }
    return false;
}

const lexer::Token& Parser::Consume(lexer::TokenType type, const std::string& message) {
    if (Check(type)) return Advance();
    throw errors::ParserError(message, Peek().line, Peek().col);
}

void Parser::TrackNodeCreation() {
    ast_nodes_created_++;
    if (ast_nodes_created_ > security::Limits::Get().max_ast_nodes) {
        throw errors::SecurityError("Maximum AST nodes exceeded", Peek().line, Peek().col);
    }
}

std::unique_ptr<ast::SourceNode> Parser::ParseFrom() {
    Consume(lexer::TokenType::IDENTIFIER, "Expect source name after FROM.");
    TrackNodeCreation();
    return std::make_unique<ast::SourceNode>(Previous().value);
}

std::unique_ptr<ast::WhereNode> Parser::ParseWhere() {
    auto condition = ParseExpression();
    TrackNodeCreation();
    return std::make_unique<ast::WhereNode>(std::move(condition));
}

std::unique_ptr<ast::SelectNode> Parser::ParseSelect() {
    std::vector<std::string> fields;

    if (Match({lexer::TokenType::STAR})) {
        fields.push_back("*");
    } else {
        do {
            Consume(lexer::TokenType::IDENTIFIER, "Expect field name in SELECT.");
            fields.push_back(Previous().value);
        } while (Match({lexer::TokenType::COMMA}));
    }
    
    TrackNodeCreation();
    return std::make_unique<ast::SelectNode>(std::move(fields));
}

size_t Parser::ParseLimit() {
    Consume(lexer::TokenType::NUMBER, "Expect number after LIMIT.");
    size_t limit = std::stoull(Previous().value);
    if (limit == 0) {
        throw errors::ParserError("LIMIT must be a positive number.", Previous().line, Previous().col);
    }
    TrackNodeCreation();
    return limit;
}

std::unique_ptr<ast::OrderByNode> Parser::ParseOrderBy() {
    Consume(lexer::TokenType::BY, "Expect BY after ORDER.");
    Consume(lexer::TokenType::IDENTIFIER, "Expect field name after ORDER BY.");
    std::string field = Previous().value;

    ast::OrderDirection direction = ast::OrderDirection::ASC;
    if (Match({lexer::TokenType::ASC})) {
        direction = ast::OrderDirection::ASC;
    } else if (Match({lexer::TokenType::DESC})) {
        direction = ast::OrderDirection::DESC;
    }

    TrackNodeCreation();
    return std::make_unique<ast::OrderByNode>(field, direction);
}

std::unique_ptr<ast::ExpressionNode> Parser::ParseExpression() {
    RecursionGuard guard(this);
    return ParseOr();
}

std::unique_ptr<ast::ExpressionNode> Parser::ParseOr() {
    RecursionGuard guard(this);
    auto expr = ParseAnd();

    while (Match({lexer::TokenType::OR})) {
        lexer::Token op = Previous();
        auto right = ParseAnd();
        TrackNodeCreation();
        expr = std::make_unique<ast::ExpressionNode>(op, std::move(expr), std::move(right));
    }

    return expr;
}

std::unique_ptr<ast::ExpressionNode> Parser::ParseAnd() {
    RecursionGuard guard(this);
    auto expr = ParseEquality();

    while (Match({lexer::TokenType::AND})) {
        lexer::Token op = Previous();
        auto right = ParseEquality();
        TrackNodeCreation();
        expr = std::make_unique<ast::ExpressionNode>(op, std::move(expr), std::move(right));
    }

    return expr;
}

std::unique_ptr<ast::ExpressionNode> Parser::ParseEquality() {
    RecursionGuard guard(this);
    auto expr = ParseComparison();

    while (Match({lexer::TokenType::EQUALS, lexer::TokenType::NOT_EQUALS})) {
        lexer::Token op = Previous();
        auto right = ParseComparison();
        TrackNodeCreation();
        expr = std::make_unique<ast::ExpressionNode>(op, std::move(expr), std::move(right));
    }

    return expr;
}

std::unique_ptr<ast::ExpressionNode> Parser::ParseComparison() {
    RecursionGuard guard(this);
    auto expr = ParsePrimary();

    while (Match({lexer::TokenType::GREATER_THAN, lexer::TokenType::GREATER_EQUALS, 
                  lexer::TokenType::LESS_THAN, lexer::TokenType::LESS_EQUALS})) {
        lexer::Token op = Previous();
        auto right = ParsePrimary();
        TrackNodeCreation();
        expr = std::make_unique<ast::ExpressionNode>(op, std::move(expr), std::move(right));
    }

    return expr;
}

std::unique_ptr<ast::ExpressionNode> Parser::ParsePrimary() {
    RecursionGuard guard(this);
    if (Match({lexer::TokenType::IDENTIFIER, lexer::TokenType::NUMBER, lexer::TokenType::STRING})) {
        TrackNodeCreation();
        return std::make_unique<ast::ExpressionNode>(Previous());
    }

    if (Match({lexer::TokenType::LEFT_PAREN})) {
        auto expr = ParseExpression();
        Consume(lexer::TokenType::RIGHT_PAREN, "Expect ')' after expression.");
        return expr;
    }

    throw errors::ParserError("Expect expression.", Peek().line, Peek().col);
}

} // namespace parser
} // namespace qle
