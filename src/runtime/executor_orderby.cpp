#include "runtime/runtime.h"
#include "runtime/merge_sorter.h"
#include "errors/errors.h"
#include "security/limits.h"
#include "utils/suggestions.h"
#include <string>
#include <cstdlib>
#include <limits>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <thread>
#include <future>
#include <atomic>

namespace qle {
namespace runtime {

void Runtime::ExecuteWithOrderBy(adapters::IAdapter& adapter,
                                 const ast::SelectNode* select_node,
                                 const ast::WhereNode* where_node,
                                 const ast::OrderByNode* order_by, size_t limit) {
    std::vector<adapters::Row> matching_rows;
    std::vector<std::string> field_names;
    bool wildcard_resolved = false;

    size_t num_threads = std::thread::hardware_concurrency();
    if (num_threads == 0) num_threads = 4;
    
    if (adapter.SupportsParallel()) {
        auto splits = adapter.Split(num_threads);
        if (!splits.empty()) {
            std::vector<std::future<std::vector<adapters::Row>>> futures;
            std::atomic<size_t> local_rows_processed{0};
            std::atomic<bool> limit_exceeded{false};
            std::atomic<bool> timeout_exceeded{false};
            
            for (auto& split_ptr : splits) {
                futures.push_back(std::async(std::launch::async, 
                    [this, select_node, where_node, order_by,
                     split = std::move(split_ptr),
                     &local_rows_processed, &limit_exceeded, &timeout_exceeded]() mutable {
                    std::vector<adapters::Row> local_matching;
                    size_t count = 0;
                    while (split->HasNext()) {
                        if (limit_exceeded.load(std::memory_order_relaxed)) {
                            break;
                        }
                        if (timeout_exceeded.load(std::memory_order_relaxed)) {
                            break;
                        }
                        
                        adapters::Row row = split->Next();
                        count++;
                        if (count % 1000 == 0) {
                            size_t total = local_rows_processed.fetch_add(count, std::memory_order_relaxed) + count;
                            count = 0;
                            if (total >= security::Limits::Get().max_rows_processed) {
                                limit_exceeded.store(true, std::memory_order_relaxed);
                                break;
                            }
                            auto now = std::chrono::steady_clock::now();
                            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - this->start_time_);
                            if (static_cast<size_t>(duration.count()) > security::Limits::Get().max_execution_time_ms) {
                                timeout_exceeded.store(true, std::memory_order_relaxed);
                                break;
                            }
                        }

                        bool include_row = true;
                        if (where_node) {
                            include_row = EvaluateCondition(where_node->GetCondition(), row);
                        }

                        if (include_row) {
                            local_matching.push_back(std::move(row));
                        }
                    }
                    if (count > 0) {
                        local_rows_processed.fetch_add(count, std::memory_order_relaxed);
                    }
                    
                    SortRows(local_matching, order_by);
                    return local_matching;
                }));
            }
            
            std::vector<std::vector<adapters::Row>> sorted_splits;
            for (auto& f : futures) {
                sorted_splits.push_back(f.get());
            }
            
            if (limit_exceeded.load()) {
                throw errors::SecurityError("Maximum row limit exceeded.");
            }
            if (timeout_exceeded.load()) {
                throw errors::SecurityError("Maximum execution time exceeded (" + std::to_string(security::Limits::Get().max_execution_time_ms) + " ms)");
            }
            
            this->rows_processed_ += local_rows_processed.load();
            
            matching_rows = MergeSorter::Merge(sorted_splits, order_by);
            
            if (select_node->IsWildcard() && !matching_rows.empty()) {
                field_names = ResolveWildcard(matching_rows.front());
                wildcard_resolved = true;
            }
            goto process_output;
        }
    }

    // fallback
    while (adapter.HasNext()) {
        if (rows_processed_ >= security::Limits::Get().max_rows_processed) {
            throw errors::SecurityError("Maximum row limit exceeded.");
        }
        adapters::Row row = adapter.Next();
        rows_processed_++;
        if (rows_processed_ % 10000 == 0) CheckTimeout();

        bool include_row = true;
        if (where_node) {
            include_row = EvaluateCondition(where_node->GetCondition(), row);
        }

        if (include_row) {
            if (select_node->IsWildcard() && !wildcard_resolved) {
                field_names = ResolveWildcard(row);
                wildcard_resolved = true;
            }
            matching_rows.push_back(std::move(row));
        }
    }
    
    SortRows(matching_rows, order_by);

process_output:
    if (!select_node->IsWildcard()) {
        for (const auto& expr : select_node->GetFields()) {
            field_names.push_back(FormatExpression(expr.get()));
        }
    }

    if (!execute_to_memory_) formatter_.PrintHeader(field_names, format_);

    size_t output_count = 0;
    for (const auto& row : matching_rows) {
        if (select_node->IsWildcard()) {
            if (execute_to_memory_) memory_results_.push_back(row);
            else formatter_.PrintRow(row, field_names, format_);
        } else {
            adapters::Row out_row;
            for (size_t i = 0; i < select_node->GetFields().size(); ++i) {
                out_row[field_names[i]] = EvaluateExpression(select_node->GetFields()[i].get(), row);
            }
            if (execute_to_memory_) memory_results_.push_back(out_row);
            else formatter_.PrintRow(out_row, field_names, format_);
        }
        output_count++;
        if (limit > 0 && output_count >= limit) break;
    }
}

void Runtime::SortRows(std::vector<adapters::Row>& rows, const ast::OrderByNode* order_by) {
    const std::string& field = order_by->GetField();
    bool descending = (order_by->GetDirection() == ast::OrderDirection::DESC);

    struct SortItem {
        double num_val;
        std::string str_val;
        bool is_num;
        adapters::Row row;
    };

    std::vector<SortItem> items;
    items.reserve(rows.size());
    for (auto& row : rows) {
        auto it = row.find(field);
        std::string val = (it != row.end()) ? it->second : "";
        double num = 0;
        bool is_num = false;
        if (!val.empty()) {
            char* endptr = nullptr;
            num = std::strtod(val.c_str(), &endptr);
            is_num = (endptr != val.c_str() && *endptr == '\0');
        }
        items.push_back({num, std::move(val), is_num, std::move(row)});
    }

    std::stable_sort(items.begin(), items.end(),
        [descending](const SortItem& a, const SortItem& b) {
            if (a.is_num && b.is_num) {
                return descending ? (a.num_val > b.num_val) : (a.num_val < b.num_val);
            }
            return descending ? (a.str_val > b.str_val) : (a.str_val < b.str_val);
        });

    rows.clear();
    for (auto& item : items) {
        rows.push_back(std::move(item.row));
    }
}

} // namespace runtime
} // namespace qle
