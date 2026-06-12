import os

with open('src/parser/parser.h', 'r') as f:
    content = f.read()

content = content.replace("std::unique_ptr<ast::OrderByNode> ParseOrderBy();", "std::unique_ptr<ast::OrderByNode> ParseOrderBy();\n    std::unique_ptr<ast::GroupByNode> ParseGroupBy();")

with open('src/parser/parser.h', 'w') as f:
    f.write(content)

with open('src/parser/parser.cpp', 'r') as f:
    content = f.read()

content = content.replace("""    std::unique_ptr<ast::SourceNode> source = nullptr;
    std::unique_ptr<ast::WhereNode> where_clause = nullptr;
    std::unique_ptr<ast::SelectNode> select_clause = nullptr;
    size_t limit = 0;
    bool has_limit = false;
    std::unique_ptr<ast::OrderByNode> order_by = nullptr;""", """    std::unique_ptr<ast::SourceNode> source = nullptr;
    std::unique_ptr<ast::WhereNode> where_clause = nullptr;
    std::unique_ptr<ast::SelectNode> select_clause = nullptr;
    size_t limit = 0;
    bool has_limit = false;
    std::unique_ptr<ast::OrderByNode> order_by = nullptr;
    std::unique_ptr<ast::GroupByNode> group_by = nullptr;""")

content = content.replace("""            if (order_by) {
                throw errors::ParserError("Multiple ORDER BY clauses found", Previous().line, Previous().col);
            }
            order_by = ParseOrderBy();
        } else {""", """            if (order_by) {
                throw errors::ParserError("Multiple ORDER BY clauses found", Previous().line, Previous().col);
            }
            order_by = ParseOrderBy();
        } else if (Match({lexer::TokenType::GROUP})) {
            if (group_by) {
                throw errors::ParserError("Multiple GROUP BY clauses found", Previous().line, Previous().col);
            }
            group_by = ParseGroupBy();
        } else {""")

content = content.replace("""    return std::make_unique<ast::QueryNode>(std::move(source), std::move(where_clause), std::move(select_clause), limit, std::move(order_by));""", """    return std::make_unique<ast::QueryNode>(std::move(source), std::move(where_clause), std::move(select_clause), limit, std::move(order_by), std::move(group_by));""")

content = content.replace("""std::unique_ptr<ast::SelectNode> Parser::ParseSelect() {
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
}""", """std::unique_ptr<ast::SelectNode> Parser::ParseSelect() {
    std::vector<std::unique_ptr<ast::ExpressionNode>> fields;
    bool is_wildcard = false;

    if (Match({lexer::TokenType::STAR})) {
        is_wildcard = true;
    } else {
        do {
            fields.push_back(ParseExpression());
        } while (Match({lexer::TokenType::COMMA}));
    }
    
    TrackNodeCreation();
    return std::make_unique<ast::SelectNode>(std::move(fields), is_wildcard);
}""")

content = content.replace("""std::unique_ptr<ast::OrderByNode> Parser::ParseOrderBy() {
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
}""", """std::unique_ptr<ast::OrderByNode> Parser::ParseOrderBy() {
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

std::unique_ptr<ast::GroupByNode> Parser::ParseGroupBy() {
    Consume(lexer::TokenType::BY, "Expect BY after GROUP.");
    Consume(lexer::TokenType::IDENTIFIER, "Expect field name after GROUP BY.");
    std::string field = Previous().value;

    TrackNodeCreation();
    return std::make_unique<ast::GroupByNode>(field);
}""")

content = content.replace("""std::unique_ptr<ast::ExpressionNode> Parser::ParsePrimary() {
    RecursionGuard guard(this);
    if (Match({lexer::TokenType::IDENTIFIER, lexer::TokenType::NUMBER, lexer::TokenType::STRING})) {
        TrackNodeCreation();
        return std::make_unique<ast::ExpressionNode>(Previous());
    }""", """std::unique_ptr<ast::ExpressionNode> Parser::ParsePrimary() {
    RecursionGuard guard(this);
    if (Match({lexer::TokenType::IDENTIFIER})) {
        lexer::Token token = Previous();
        if (Match({lexer::TokenType::LEFT_PAREN})) {
            std::vector<std::unique_ptr<ast::ExpressionNode>> args;
            if (!Check(lexer::TokenType::RIGHT_PAREN)) {
                do {
                    args.push_back(ParseExpression());
                } while (Match({lexer::TokenType::COMMA}));
            }
            Consume(lexer::TokenType::RIGHT_PAREN, "Expect ')' after arguments.");
            TrackNodeCreation();
            return std::make_unique<ast::ExpressionNode>(token, std::move(args));
        }
        TrackNodeCreation();
        return std::make_unique<ast::ExpressionNode>(token);
    }
    
    if (Match({lexer::TokenType::NUMBER, lexer::TokenType::STRING})) {
        TrackNodeCreation();
        return std::make_unique<ast::ExpressionNode>(Previous());
    }""")

with open('src/parser/parser.cpp', 'w') as f:
    f.write(content)
