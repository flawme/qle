#include "runtime/merge_sorter.h"
#include <queue>
#include <cstdlib>
#include <string>

namespace qle {
namespace runtime {

std::vector<adapters::Row> MergeSorter::Merge(
    std::vector<std::vector<adapters::Row>>& sorted_splits,
    const ast::OrderByNode* order_by) {
    
    std::vector<adapters::Row> result;
    size_t total_size = 0;
    for (const auto& split : sorted_splits) {
        total_size += split.size();
    }
    result.reserve(total_size);

    const std::string& field = order_by->GetField();
    bool descending = (order_by->GetDirection() == ast::OrderDirection::DESC);

    auto comp = [&field, descending](const adapters::Row& a, const adapters::Row& b) {
        auto it_a = a.find(field);
        auto it_b = b.find(field);
        std::string val_a = (it_a != a.end()) ? it_a->second : "";
        std::string val_b = (it_b != b.end()) ? it_b->second : "";

        double num_a, num_b;
        char* endptr_a = nullptr; char* endptr_b = nullptr;
        num_a = std::strtod(val_a.c_str(), &endptr_a);
        num_b = std::strtod(val_b.c_str(), &endptr_b);
        bool is_num_a = (!val_a.empty() && endptr_a != val_a.c_str() && *endptr_a == '\0');
        bool is_num_b = (!val_b.empty() && endptr_b != val_b.c_str() && *endptr_b == '\0');

        if (is_num_a && is_num_b) {
            return descending ? (num_a > num_b) : (num_a < num_b);
        }
        return descending ? (val_a > val_b) : (val_a < val_b);
    };
    
    struct Element {
        size_t split_idx;
        size_t row_idx;
    };
    
    auto pq_comp = [&sorted_splits, &comp](const Element& a, const Element& b) {
        const auto& row_a = sorted_splits[a.split_idx][a.row_idx];
        const auto& row_b = sorted_splits[b.split_idx][b.row_idx];
        return comp(row_b, row_a);
    };

    std::priority_queue<Element, std::vector<Element>, decltype(pq_comp)> pq(pq_comp);

    for (size_t i = 0; i < sorted_splits.size(); ++i) {
        if (!sorted_splits[i].empty()) {
            pq.push({i, 0});
        }
    }

    while (!pq.empty()) {
        Element top = pq.top();
        pq.pop();

        result.push_back(std::move(sorted_splits[top.split_idx][top.row_idx]));
        top.row_idx++;

        if (top.row_idx < sorted_splits[top.split_idx].size()) {
            pq.push(top);
        }
    }

    return result;
}

} // namespace runtime
} // namespace qle
