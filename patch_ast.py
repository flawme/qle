import os

ast_h = "src/ast/ast.h"
with open(ast_h, "r") as f:
    content = f.read()

if "NodeType::JOIN" not in content:
    content = content.replace("GROUP_BY\n};", "GROUP_BY,\n    JOIN\n};")

join_node_class = """
class JoinNode : public AstNode {
public:
    JoinNode(const std::string& source, std::unique_ptr<ExpressionNode> condition);
    
    NodeType GetType() const override { return NodeType::JOIN; }
    const std::string& GetSource() const { return source_; }
    const ExpressionNode* GetCondition() const { return condition_.get(); }

private:
    std::string source_;
    std::unique_ptr<ExpressionNode> condition_;
};
"""
if "class JoinNode" not in content:
    content = content.replace("class QueryNode : public AstNode {", join_node_class + "\nclass QueryNode : public AstNode {")

if "std::unique_ptr<JoinNode> join_clause_;" not in content:
    content = content.replace("std::unique_ptr<SourceNode> source;", "std::unique_ptr<SourceNode> source,\n              std::unique_ptr<JoinNode> join_clause,")
    content = content.replace("const GroupByNode* GetGroupBy() const { return group_by_.get(); }", "const GroupByNode* GetGroupBy() const { return group_by_.get(); }\n    const JoinNode* GetJoin() const { return join_clause_.get(); }")
    content = content.replace("std::unique_ptr<SourceNode> source_;", "std::unique_ptr<SourceNode> source_;\n    std::unique_ptr<JoinNode> join_clause_;")

with open(ast_h, "w") as f:
    f.write(content)

ast_cpp = "src/ast/ast.cpp"
with open(ast_cpp, "r") as f:
    content = f.read()

join_node_impl = """
JoinNode::JoinNode(const std::string& source, std::unique_ptr<ExpressionNode> condition)
    : source_(source), condition_(std::move(condition)) {}
"""
if "JoinNode::JoinNode" not in content:
    content = content.replace("QueryNode::QueryNode", join_node_impl + "\nQueryNode::QueryNode")

if "std::unique_ptr<JoinNode> join_clause," not in content:
    content = content.replace("std::unique_ptr<SourceNode> source,", "std::unique_ptr<SourceNode> source,\n                     std::unique_ptr<JoinNode> join_clause,")
    content = content.replace("source_(std::move(source)),", "source_(std::move(source)), \n      join_clause_(std::move(join_clause)),")

with open(ast_cpp, "w") as f:
    f.write(content)

