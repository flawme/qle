import os

with open('src/ast/ast.h', 'r') as f:
    content = f.read()

content = content.replace(
    "ORDER_BY\n};",
    "ORDER_BY,\n    GROUP_BY\n};"
)

content = content.replace(
    "explicit ExpressionNode(lexer::Token literal); // identifier, string, number\n    \n    NodeType GetType() const override { return NodeType::EXPRESSION; }\n    \n    const lexer::Token& GetToken() const { return token_; }\n    const ExpressionNode* GetLeft() const { return left_.get(); }\n    const ExpressionNode* GetRight() const { return right_.get(); }\n    bool IsLiteral() const { return left_ == nullptr && right_ == nullptr; }\n\nprivate:\n    lexer::Token token_;\n    std::unique_ptr<ExpressionNode> left_;\n    std::unique_ptr<ExpressionNode> right_;\n};",
    """explicit ExpressionNode(lexer::Token literal); // identifier, string, number
    ExpressionNode(lexer::Token function_name, std::vector<std::unique_ptr<ExpressionNode>> args);
    
    NodeType GetType() const override { return NodeType::EXPRESSION; }
    
    const lexer::Token& GetToken() const { return token_; }
    const ExpressionNode* GetLeft() const { return left_.get(); }
    const ExpressionNode* GetRight() const { return right_.get(); }
    bool IsLiteral() const { return left_ == nullptr && right_ == nullptr && args_.empty() && !is_function_call_; }
    bool IsFunctionCall() const { return is_function_call_; }
    const std::vector<std::unique_ptr<ExpressionNode>>& GetArgs() const { return args_; }

private:
    lexer::Token token_;
    std::unique_ptr<ExpressionNode> left_;
    std::unique_ptr<ExpressionNode> right_;
    std::vector<std::unique_ptr<ExpressionNode>> args_;
    bool is_function_call_ = false;
};"""
)

content = content.replace(
    "class SelectNode : public AstNode {\npublic:\n    explicit SelectNode(std::vector<std::string> fields);\n    \n    NodeType GetType() const override { return NodeType::SELECT; }\n    const std::vector<std::string>& GetFields() const { return fields_; }\n\nprivate:\n    std::vector<std::string> fields_;\n};",
    """class SelectNode : public AstNode {
public:
    explicit SelectNode(std::vector<std::unique_ptr<ExpressionNode>> fields, bool is_wildcard = false);
    
    NodeType GetType() const override { return NodeType::SELECT; }
    const std::vector<std::unique_ptr<ExpressionNode>>& GetFields() const { return fields_; }
    bool IsWildcard() const { return is_wildcard_; }

private:
    std::vector<std::unique_ptr<ExpressionNode>> fields_;
    bool is_wildcard_ = false;
};

class GroupByNode : public AstNode {
public:
    explicit GroupByNode(const std::string& field);

    NodeType GetType() const override { return NodeType::GROUP_BY; }
    const std::string& GetField() const { return field_; }

private:
    std::string field_;
};"""
)

content = content.replace(
    "QueryNode(std::unique_ptr<SourceNode> source,\n              std::unique_ptr<WhereNode> where_clause,\n              std::unique_ptr<SelectNode> select_clause,\n              size_t limit,\n              std::unique_ptr<OrderByNode> order_by);",
    "QueryNode(std::unique_ptr<SourceNode> source,\n              std::unique_ptr<WhereNode> where_clause,\n              std::unique_ptr<SelectNode> select_clause,\n              size_t limit,\n              std::unique_ptr<OrderByNode> order_by,\n              std::unique_ptr<GroupByNode> group_by = nullptr);"
)

content = content.replace(
    "const OrderByNode* GetOrderBy() const { return order_by_.get(); }\n\nprivate:\n    std::unique_ptr<SourceNode> source_;\n    std::unique_ptr<WhereNode> where_clause_;\n    std::unique_ptr<SelectNode> select_clause_;\n    size_t limit_;\n    std::unique_ptr<OrderByNode> order_by_;\n};",
    "const OrderByNode* GetOrderBy() const { return order_by_.get(); }\n    const GroupByNode* GetGroupBy() const { return group_by_.get(); }\n\nprivate:\n    std::unique_ptr<SourceNode> source_;\n    std::unique_ptr<WhereNode> where_clause_;\n    std::unique_ptr<SelectNode> select_clause_;\n    size_t limit_;\n    std::unique_ptr<OrderByNode> order_by_;\n    std::unique_ptr<GroupByNode> group_by_;\n};"
)

with open('src/ast/ast.h', 'w') as f:
    f.write(content)

with open('src/ast/ast.cpp', 'r') as f:
    content = f.cpp_read = f.read()

content = content.replace(
    "ExpressionNode::ExpressionNode(lexer::Token op, std::unique_ptr<ExpressionNode> left, std::unique_ptr<ExpressionNode> right)\n    : token_(std::move(op)), left_(std::move(left)), right_(std::move(right)) {}",
    "ExpressionNode::ExpressionNode(lexer::Token op, std::unique_ptr<ExpressionNode> left, std::unique_ptr<ExpressionNode> right)\n    : token_(std::move(op)), left_(std::move(left)), right_(std::move(right)), is_function_call_(false) {}"
)

content = content.replace(
    "ExpressionNode::ExpressionNode(lexer::Token literal)\n    : token_(std::move(literal)), left_(nullptr), right_(nullptr) {}",
    "ExpressionNode::ExpressionNode(lexer::Token literal)\n    : token_(std::move(literal)), left_(nullptr), right_(nullptr), is_function_call_(false) {}\n\nExpressionNode::ExpressionNode(lexer::Token function_name, std::vector<std::unique_ptr<ExpressionNode>> args)\n    : token_(std::move(function_name)), left_(nullptr), right_(nullptr), args_(std::move(args)), is_function_call_(true) {}"
)

content = content.replace(
    "SelectNode::SelectNode(std::vector<std::string> fields)\n    : fields_(std::move(fields)) {}",
    "SelectNode::SelectNode(std::vector<std::unique_ptr<ExpressionNode>> fields, bool is_wildcard)\n    : fields_(std::move(fields)), is_wildcard_(is_wildcard) {}\n\nGroupByNode::GroupByNode(const std::string& field)\n    : field_(field) {}"
)

content = content.replace(
    "QueryNode::QueryNode(std::unique_ptr<SourceNode> source,\n                     std::unique_ptr<WhereNode> where_clause,\n                     std::unique_ptr<SelectNode> select_clause,\n                     size_t limit,\n                     std::unique_ptr<OrderByNode> order_by)\n    : source_(std::move(source)), \n      where_clause_(std::move(where_clause)), \n      select_clause_(std::move(select_clause)),\n      limit_(limit),\n      order_by_(std::move(order_by)) {}",
    "QueryNode::QueryNode(std::unique_ptr<SourceNode> source,\n                     std::unique_ptr<WhereNode> where_clause,\n                     std::unique_ptr<SelectNode> select_clause,\n                     size_t limit,\n                     std::unique_ptr<OrderByNode> order_by,\n                     std::unique_ptr<GroupByNode> group_by)\n    : source_(std::move(source)), \n      where_clause_(std::move(where_clause)), \n      select_clause_(std::move(select_clause)),\n      limit_(limit),\n      order_by_(std::move(order_by)),\n      group_by_(std::move(group_by)) {}"
)

with open('src/ast/ast.cpp', 'w') as f:
    f.write(content)

