#pragma once

#include "lexer/token.h"
#include <memory>
#include <string>
#include <vector>

namespace qle {
namespace ast {

enum class NodeType {
    QUERY,
    SOURCE,
    WHERE,
    SELECT,
    EXPRESSION,
    ORDER_BY,
    GROUP_BY,
    JOIN
};

enum class OrderDirection {
    ASC,
    DESC
};

class AstNode {
public:
    virtual ~AstNode() = default;
    virtual NodeType GetType() const = 0;
};

// Expressions: binary ops or literals
class ExpressionNode : public AstNode {
public:
    ExpressionNode(lexer::Token op, std::unique_ptr<ExpressionNode> left, std::unique_ptr<ExpressionNode> right);
    explicit ExpressionNode(lexer::Token literal); // identifier, string, number
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
};

class SourceNode : public AstNode {
public:
    explicit SourceNode(const std::string& source_name);
    
    NodeType GetType() const override { return NodeType::SOURCE; }
    const std::string& GetSourceName() const { return source_name_; }

private:
    std::string source_name_;
};

class WhereNode : public AstNode {
public:
    explicit WhereNode(std::unique_ptr<ExpressionNode> condition);
    
    NodeType GetType() const override { return NodeType::WHERE; }
    const ExpressionNode* GetCondition() const { return condition_.get(); }

private:
    std::unique_ptr<ExpressionNode> condition_;
};

class SelectNode : public AstNode {
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
};

class OrderByNode : public AstNode {
public:
    OrderByNode(const std::string& field, OrderDirection direction);

    NodeType GetType() const override { return NodeType::ORDER_BY; }
    const std::string& GetField() const { return field_; }
    OrderDirection GetDirection() const { return direction_; }

private:
    std::string field_;
    OrderDirection direction_;
};


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

class QueryNode : public AstNode {
public:
    QueryNode(std::unique_ptr<SourceNode> source,
              std::unique_ptr<JoinNode> join_clause,
              std::unique_ptr<WhereNode> where_clause,
              std::unique_ptr<SelectNode> select_clause,
              size_t limit,
              std::unique_ptr<OrderByNode> order_by,
              std::unique_ptr<GroupByNode> group_by = nullptr);
              
    NodeType GetType() const override { return NodeType::QUERY; }
    
    const SourceNode* GetSource() const { return source_.get(); }
    const WhereNode* GetWhere() const { return where_clause_.get(); }
    const SelectNode* GetSelect() const { return select_clause_.get(); }
    size_t GetLimit() const { return limit_; }
    const OrderByNode* GetOrderBy() const { return order_by_.get(); }
    const GroupByNode* GetGroupBy() const { return group_by_.get(); }
    const JoinNode* GetJoin() const { return join_clause_.get(); }

private:
    std::unique_ptr<SourceNode> source_;
    std::unique_ptr<JoinNode> join_clause_;
    std::unique_ptr<WhereNode> where_clause_;
    std::unique_ptr<SelectNode> select_clause_;
    size_t limit_;
    std::unique_ptr<OrderByNode> order_by_;
    std::unique_ptr<GroupByNode> group_by_;
};

} // namespace ast
} // namespace qle

