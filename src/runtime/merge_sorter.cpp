#include "runtime/merge_sorter.h"
#include <queue>
#include <cstdlib>
#include <string>

namespace qle {
namespace runtime {

std::vector<adapters::Row> MergeSorter::Merge(
    std::vector<std::vector<adapters::Row>>& sorted_splits,
    const ast::OrderByNode* order_by) {
    
    const std::string& field = order_by->GetField();
    bool descending = (order_by->GetDirection() == ast::OrderDirection::DESC);

    struct SortItem {
        double num_val;
        std::string str_val;
        bool is_num;
        adapters::Row row;
    };
    
    std::vector<std::vector<SortItem>> sorted_items;
    sorted_items.resize(sorted_splits.size());
    
    size_t total_size = 0;
    for (size_t i = 0; i < sorted_splits.size(); ++i) {
        total_size += sorted_splits[i].size();
        sorted_items[i].reserve(sorted_splits[i].size());
        for (auto& row : sorted_splits[i]) {
            auto it = row.find(field);
            std::string val = (it != row.end()) ? it->second : "";
            double num = 0;
            bool is_num = false;
            if (!val.empty()) {
                char* endptr = nullptr;
                num = std::strtod(val.c_str(), &endptr);
                is_num = (endptr != val.c_str() && *endptr == '\0');
            }
            sorted_items[i].push_back({num, std::move(val), is_num, std::move(row)});
        }
    }

    std::vector<adapters::Row> result;
    result.reserve(total_size);

    auto comp = [descending](const SortItem& a, const SortItem& b) {
        if (a.is_num && b.is_num) {
            return descending ? (a.num_val > b.num_val) : (a.num_val < b.num_val);
        }
        return descending ? (a.str_val > b.str_val) : (a.str_val < b.str_val);
    };
    
    struct Element {
        size_t split_idx;
        size_t row_idx;
    };
    
    auto pq_comp = [&sorted_items, &comp](const Element& a, const Element& b) {
        const auto& item_a = sorted_items[a.split_idx][a.row_idx];
        const auto& item_b = sorted_items[b.split_idx][b.row_idx];
        return comp(item_b, item_a);
    };

    std::priority_queue<Element, std::vector<Element>, decltype(pq_comp)> pq(pq_comp);

    for (size_t i = 0; i < sorted_items.size(); ++i) {
        if (!sorted_items[i].empty()) {
            pq.push({i, 0});
        }
    }

    while (!pq.empty()) {
        Element top = pq.top();
        pq.pop();

        result.push_back(std::move(sorted_items[top.split_idx][top.row_idx].row));
        top.row_idx++;

        if (top.row_idx < sorted_items[top.split_idx].size()) {
            pq.push(top);
        }
    }

    return result;
}

} // namespace runtime
} // namespace qle
