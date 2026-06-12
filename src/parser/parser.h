#pragma once

#include "lexer/lexer.h"
#include "ast/ast.h"
#include <vector>
#include <memory>

namespace qle {
namespace parser {

class Parser {
public:
    explicit Parser(std::vector<lexer::Token> tokens);
    
    std::unique_ptr<ast::QueryNode> Parse();

private:
    std::vector<lexer::Token> tokens_;
    size_t current_;
    size_t ast_nodes_created_;
    size_t recursion_depth_;

    class RecursionGuard {
    public:
        RecursionGuard(Parser* parser);
        ~RecursionGuard();
    private:
        Parser* parser_;
    };

    bool IsAtEnd() const;
    const lexer::Token& Peek() const;
    const lexer::Token& Previous() const;
    const lexer::Token& Advance();
    
    bool Check(lexer::TokenType type) const;
    bool Match(std::initializer_list<lexer::TokenType> types);
    const lexer::Token& Consume(lexer::TokenType type, const std::string& message);

    void TrackNodeCreation();

    std::unique_ptr<ast::SourceNode> ParseFrom();
    std::unique_ptr<ast::WhereNode> ParseWhere();
    std::unique_ptr<ast::SelectNode> ParseSelect();
    size_t ParseLimit();
    std::unique_ptr<ast::OrderByNode> ParseOrderBy();
    
    std::unique_ptr<ast::ExpressionNode> ParseExpression();
    std::unique_ptr<ast::ExpressionNode> ParseOr();
    std::unique_ptr<ast::ExpressionNode> ParseAnd();
    std::unique_ptr<ast::ExpressionNode> ParseEquality();
    std::unique_ptr<ast::ExpressionNode> ParseComparison();
    std::unique_ptr<ast::ExpressionNode> ParsePrimary();
};

} // namespace parser
} // namespace qle
