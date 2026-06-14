#pragma once

#include "ast/ast.h"
#include <unordered_map>
#include <vector>
#include <string>
#include <memory>

namespace qle {
namespace runtime {

struct VectorBatch {
    size_t num_rows = 0;
    std::unordered_map<std::string, std::vector<double>> numeric_columns;
    std::unordered_map<std::string, std::vector<std::string>> string_columns;
};

// Evaluates a WHERE condition on a columnar batch.
// Returns a bitmask vector of size `batch.num_rows`, where true means the row passed.
std::vector<bool> EvaluateConditionVectorized(const ast::ExpressionNode* condition, const VectorBatch& batch);

} // namespace runtime
} // namespace qle
