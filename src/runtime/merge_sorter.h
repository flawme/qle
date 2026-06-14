#pragma once

#include "adapters/adapter.h"
#include "ast/ast.h"
#include <vector>

namespace qle {
namespace runtime {

class MergeSorter {
public:
    static std::vector<adapters::Row> Merge(
        std::vector<std::vector<adapters::Row>>& sorted_splits,
        const ast::OrderByNode* order_by);
};

} // namespace runtime
} // namespace qle
