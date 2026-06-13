import os

runtime_h = "src/runtime/runtime.h"
with open(runtime_h, "r") as f:
    content = f.read()

if "const ast::JoinNode* join_node" not in content:
    content = content.replace("const ast::WhereNode* where_node, size_t limit);", "const ast::WhereNode* where_node, const ast::JoinNode* join_node, size_t limit);")

with open(runtime_h, "w") as f:
    f.write(content)

runtime_cpp = "src/runtime/runtime.cpp"
with open(runtime_cpp, "r") as f:
    content = f.read()

if "const ast::JoinNode* join_node = query->GetJoin();" not in content:
    content = content.replace("const ast::WhereNode* where_node = query->GetWhere();", "const ast::WhereNode* where_node = query->GetWhere();\n    const ast::JoinNode* join_node = query->GetJoin();")
    content = content.replace("ExecuteStreaming(*adapter, select_node, where_node, limit);", "ExecuteStreaming(*adapter, select_node, where_node, join_node, limit);")
    content = content.replace("const ast::WhereNode* where_node, size_t limit) {", "const ast::WhereNode* where_node, const ast::JoinNode* join_node, size_t limit) {")

with open(runtime_cpp, "w") as f:
    f.write(content)

