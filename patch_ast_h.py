import os

ast_h = "src/ast/ast.h"
with open(ast_h, "r") as f:
    content = f.read()

content = content.replace("QueryNode(std::unique_ptr<SourceNode> source,\\n              std::unique_ptr<WhereNode> where_clause,", "QueryNode(std::unique_ptr<SourceNode> source,\\n              std::unique_ptr<JoinNode> join_clause,\\n              std::unique_ptr<WhereNode> where_clause,")

with open(ast_h, "w") as f:
    f.write(content)

