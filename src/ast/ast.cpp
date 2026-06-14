#include "ast/ast.h"

namespace qle {
namespace ast {

ExpressionNode::ExpressionNode(lexer::Token op, std::unique_ptr<ExpressionNode> left, std::unique_ptr<ExpressionNode> right)
    : token_(std::move(op)), left_(std::move(left)), right_(std::move(right)), is_function_call_(false) {}

ExpressionNode::ExpressionNode(lexer::Token literal)
    : token_(std::move(literal)), left_(nullptr), right_(nullptr), is_function_call_(false) {}

ExpressionNode::ExpressionNode(lexer::Token function_name, std::vector<std::unique_ptr<ExpressionNode>> args)
    : token_(std::move(function_name)), left_(nullptr), right_(nullptr), args_(std::move(args)), is_function_call_(true) {}

SourceNode::SourceNode(const std::string& source_name)
    : source_name_(source_name) {}

SourceNode::SourceNode(std::unique_ptr<QueryNode> subquery)
    : source_name_(""), subquery_(std::move(subquery)) {}

WhereNode::WhereNode(std::unique_ptr<ExpressionNode> condition)
    : condition_(std::move(condition)) {}

SelectNode::SelectNode(std::vector<std::unique_ptr<ExpressionNode>> fields, bool is_wildcard)
    : fields_(std::move(fields)), is_wildcard_(is_wildcard) {}

GroupByNode::GroupByNode(const std::string& field)
    : field_(field) {}

OrderByNode::OrderByNode(const std::string& field, OrderDirection direction)
    : field_(field), direction_(direction) {}


JoinNode::JoinNode(const std::string& source, std::unique_ptr<ExpressionNode> condition)
    : source_(source), condition_(std::move(condition)) {}

QueryNode::QueryNode(std::unique_ptr<SourceNode> source,
                     std::vector<std::unique_ptr<JoinNode>> join_clauses,
                     std::unique_ptr<WhereNode> where_clause,
                     std::unique_ptr<SelectNode> select_clause,
                     size_t limit,
                     std::unique_ptr<OrderByNode> order_by,
                     std::unique_ptr<GroupByNode> group_by)
    : source_(std::move(source)), 
      join_clauses_(std::move(join_clauses)), 
      where_clause_(std::move(where_clause)), 
      select_clause_(std::move(select_clause)),
      limit_(limit),
      order_by_(std::move(order_by)),
      group_by_(std::move(group_by)) {}

} // namespace ast
} // namespace qle

