#pragma once

#include "ast/ast.h"
#include "adapters/adapter.h"
#include "utils/formatter.h"
#include <memory>
#include <vector>
#include <chrono>

namespace qle {
namespace runtime {

class Runtime {
public:
    Runtime();

    void SetFormat(utils::OutputFormat format);
    void Execute(const ast::QueryNode* query);

private:
    std::unique_ptr<adapters::IAdapter> GetAdapterForSource(const std::string& source);
    bool EvaluateCondition(const ast::ExpressionNode* expr, const adapters::Row& row);
    std::string EvaluateExpression(const ast::ExpressionNode* expr, const adapters::Row& row);

    std::vector<std::string> ResolveWildcard(const adapters::Row& row);
    void SortRows(std::vector<adapters::Row>& rows, const ast::OrderByNode* order_by);

    
    void ExecuteWithOrderBy(adapters::IAdapter& adapter,
                            const ast::SelectNode* select_node,
                            const ast::WhereNode* where_node,
                            const ast::OrderByNode* order_by, size_t limit);
                            
    void ExecuteStreaming(adapters::IAdapter& adapter,
                         const ast::SelectNode* select_node,
                         const ast::WhereNode* where_node, const ast::JoinNode* join_node, size_t limit);
                         
    void ExecuteWithGroupBy(adapters::IAdapter& adapter,
                            const ast::SelectNode* select_node,
                            const ast::WhereNode* where_node,
                            const ast::GroupByNode* group_by,
                            const ast::OrderByNode* order_by, size_t limit);

    std::string FormatExpression(const ast::ExpressionNode* expr);
    std::string EvaluateAggregate(const ast::ExpressionNode* expr, const std::vector<adapters::Row>& bucket);
    bool HasAggregate(const ast::SelectNode* select_node);
    bool CheckIfContainsAggregate(const ast::ExpressionNode* expr);

    void CheckTimeout();

    size_t rows_processed_;
    std::chrono::time_point<std::chrono::steady_clock> start_time_;
    utils::OutputFormat format_;
    utils::Formatter formatter_;
};

} // namespace runtime
} // namespace qle
