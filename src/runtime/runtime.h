#pragma once

#include "ast/ast.h"
#include "adapters/adapter.h"
#include "utils/formatter.h"
#include <memory>
#include <vector>

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

    void ExecuteStreaming(adapters::IAdapter& adapter,
                         std::vector<std::string>& fields, bool is_wildcard,
                         const ast::WhereNode* where_node, size_t limit);
    void ExecuteWithOrderBy(adapters::IAdapter& adapter,
                            std::vector<std::string>& fields, bool is_wildcard,
                            const ast::WhereNode* where_node,
                            const ast::OrderByNode* order_by, size_t limit);

    size_t rows_processed_;
    utils::OutputFormat format_;
    utils::Formatter formatter_;
};

} // namespace runtime
} // namespace qle
