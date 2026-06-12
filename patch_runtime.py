import os

with open('src/runtime/runtime.h', 'r') as f:
    content = f.read()

content = content.replace(
    "void ExecuteWithOrderBy(adapters::IAdapter& adapter,\n                            std::vector<std::string>& fields, bool is_wildcard,\n                            const ast::WhereNode* where_node,\n                            const ast::OrderByNode* order_by, size_t limit);",
    """void ExecuteWithOrderBy(adapters::IAdapter& adapter,
                            const ast::SelectNode* select_node,
                            const ast::WhereNode* where_node,
                            const ast::OrderByNode* order_by, size_t limit);
                            
    void ExecuteStreaming(adapters::IAdapter& adapter,
                         const ast::SelectNode* select_node,
                         const ast::WhereNode* where_node, size_t limit);
                         
    void ExecuteWithGroupBy(adapters::IAdapter& adapter,
                            const ast::SelectNode* select_node,
                            const ast::WhereNode* where_node,
                            const ast::GroupByNode* group_by,
                            const ast::OrderByNode* order_by, size_t limit);

    std::string FormatExpression(const ast::ExpressionNode* expr);
    std::string EvaluateAggregate(const ast::ExpressionNode* expr, const std::vector<adapters::Row>& bucket);
    bool HasAggregate(const ast::SelectNode* select_node);
    bool CheckIfContainsAggregate(const ast::ExpressionNode* expr);"""
)

content = content.replace(
    "void ExecuteStreaming(adapters::IAdapter& adapter,\n                         std::vector<std::string>& fields, bool is_wildcard,\n                         const ast::WhereNode* where_node, size_t limit);",
    ""
)

with open('src/runtime/runtime.h', 'w') as f:
    f.write(content)
