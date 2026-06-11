#include "ast/ast.h"

namespace qle {
namespace ast {

ExpressionNode::ExpressionNode(lexer::Token op, std::unique_ptr<ExpressionNode> left, std::unique_ptr<ExpressionNode> right)
    : token_(std::move(op)), left_(std::move(left)), right_(std::move(right)) {}

ExpressionNode::ExpressionNode(lexer::Token literal)
    : token_(std::move(literal)), left_(nullptr), right_(nullptr) {}

SourceNode::SourceNode(const std::string& source_name)
    : source_name_(source_name) {}

WhereNode::WhereNode(std::unique_ptr<ExpressionNode> condition)
    : condition_(std::move(condition)) {}

SelectNode::SelectNode(std::vector<std::string> fields)
    : fields_(std::move(fields)) {}

QueryNode::QueryNode(std::unique_ptr<SourceNode> source,
                     std::unique_ptr<WhereNode> where_clause,
                     std::unique_ptr<SelectNode> select_clause)
    : source_(std::move(source)), 
      where_clause_(std::move(where_clause)), 
      select_clause_(std::move(select_clause)) {}

} // namespace ast
} // namespace qle
