import os

parser_cpp = "src/parser/parser.cpp"
with open(parser_cpp, "r") as f:
    content = f.read()

content = content.replace("return std::make_unique<ast::QueryNode>(std::move(source), std::move(where_clause)", "return std::make_unique<ast::QueryNode>(std::move(source), std::move(join_clause), std::move(where_clause)")

with open(parser_cpp, "w") as f:
    f.write(content)

