#pragma once

#include "ast/ast.h"
#include "adapters/adapter.h"
#include <memory>
#include <vector>

namespace qle {
namespace runtime {

class Runtime {
public:
    Runtime();
    
    void Execute(const ast::QueryNode* query);

private:
    std::unique_ptr<adapters::IAdapter> GetAdapterForSource(const std::string& source);
    bool EvaluateCondition(const ast::ExpressionNode* expr, const adapters::Row& row);
    std::string EvaluateExpression(const ast::ExpressionNode* expr, const adapters::Row& row);
    void PrintRow(const adapters::Row& row, const std::vector<std::string>& fields);
    
    size_t rows_processed_;
};

} // namespace runtime
} // namespace qle
