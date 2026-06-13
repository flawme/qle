import os

parser_h = "src/parser/parser.h"
with open(parser_h, "r") as f:
    content = f.read()

if "ParseJoin" not in content:
    content = content.replace("ParseFrom();", "ParseFrom();\n    std::unique_ptr<ast::JoinNode> ParseJoin();")

with open(parser_h, "w") as f:
    f.write(content)

parser_cpp = "src/parser/parser.cpp"
with open(parser_cpp, "r") as f:
    content = f.read()

if "std::unique_ptr<ast::JoinNode> join_clause = nullptr;" not in content:
    content = content.replace("std::unique_ptr<ast::SourceNode> source = nullptr;", "std::unique_ptr<ast::SourceNode> source = nullptr;\n    std::unique_ptr<ast::JoinNode> join_clause = nullptr;")

if "Match({lexer::TokenType::JOIN})" not in content:
    content = content.replace("} else if (Match({lexer::TokenType::WHERE})) {", """} else if (Match({lexer::TokenType::JOIN})) {
            if (join_clause) {
                throw errors::ParserError("Multiple JOIN clauses found", Previous().line, Previous().col);
            }
            join_clause = ParseJoin();
        } else if (Match({lexer::TokenType::WHERE})) {""")

if "return std::make_unique<ast::QueryNode>(std::move(source), std::move(where_clause)" not in content:
    content = content.replace("return std::make_unique<ast::QueryNode>(std::move(source), std::move(where_clause)", "return std::make_unique<ast::QueryNode>(std::move(source), std::move(join_clause), std::move(where_clause)")

join_method = """
std::unique_ptr<ast::JoinNode> Parser::ParseJoin() {
    Consume(lexer::TokenType::IDENTIFIER, "Expect source name after JOIN.");
    std::string join_source = Previous().value;
    Consume(lexer::TokenType::ON, "Expect ON after JOIN source.");
    auto condition = ParseExpression();
    TrackNodeCreation();
    return std::make_unique<ast::JoinNode>(join_source, std::move(condition));
}
"""

if "Parser::ParseJoin()" not in content:
    content = content.replace("std::unique_ptr<ast::WhereNode> Parser::ParseWhere()", join_method + "\nstd::unique_ptr<ast::WhereNode> Parser::ParseWhere()")

with open(parser_cpp, "w") as f:
    f.write(content)

