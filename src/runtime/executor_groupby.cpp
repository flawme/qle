#include "runtime/runtime.h"
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
#include <map>
#include <thread>
#include <mutex>
#include <unordered_map>

namespace qle {
namespace runtime {

void Runtime::ExecuteWithGroupBy(adapters::IAdapter& adapter,
                                 const ast::SelectNode* select_node,
                                 const ast::WhereNode* where_node,
                                 const ast::GroupByNode* group_by,
                                 const ast::OrderByNode* order_by, size_t limit) {
    std::vector<const ast::ExpressionNode*> agg_exprs;
    CollectAggregates(select_node, agg_exprs);

    size_t num_threads = adapter.SupportsParallel() ? std::thread::hardware_concurrency() : 1;
    if (num_threads == 0) num_threads = 1;
    
    auto splits = adapter.Split(num_threads);
    if (splits.empty()) {
        num_threads = 1;
    } else {
        num_threads = splits.size();
    }
    
    std::vector<std::unordered_map<std::string, AggState>> thread_buckets(num_threads);
    std::vector<std::vector<std::string>> thread_orders(num_threads);
    std::mutex timeout_mutex;
    
    auto worker = [&](adapters::IAdapter& my_adapter, size_t tid) {
        auto& my_buckets = thread_buckets[tid];
        auto& my_order = thread_orders[tid];
        size_t my_rows_processed = 0;
        
        while (my_adapter.HasNext()) {
            adapters::Row row = my_adapter.Next();
            my_rows_processed++;
            if (my_rows_processed % 10000 == 0) {
                std::lock_guard<std::mutex> lock(timeout_mutex);
                rows_processed_ += 10000;
                CheckTimeout();
                if (rows_processed_ >= security::Limits::Get().max_rows_processed) {
                    throw errors::SecurityError("Maximum row limit exceeded.");
                }
            }

            bool include_row = true;
            if (where_node) {
                include_row = EvaluateCondition(where_node->GetCondition(), row);
            }

            if (include_row) {
                std::string key = "";
                if (group_by) {
                    auto it = row.find(group_by->GetField());
                    if (it != row.end()) {
                        key = it->second;
                    }
                }
                
                if (my_buckets.find(key) == my_buckets.end()) {
                    my_order.push_back(key);
                    my_buckets[key].first_row = row;
                }
                
                auto& state = my_buckets[key];
                state.count++;
                
                for (const auto* expr : agg_exprs) {
                    if (expr->GetToken().value == "count") continue;
                    
                    std::string val_str = EvaluateExpression(expr->GetArgs()[0].get(), row);
                    double val;
                    if (TryParseDouble(val_str, val)) {
                        state.sums[expr] += val;
                        if (!state.has_vals[expr]) {
                            state.mins[expr] = val;
                            state.maxs[expr] = val;
                            state.has_vals[expr] = true;
                        } else {
                            if (val < state.mins[expr]) state.mins[expr] = val;
                            if (val > state.maxs[expr]) state.maxs[expr] = val;
                        }
                    }
                }
            }
        }
    };
    
    if (num_threads > 1) {
        std::vector<std::thread> threads;
        for (size_t i = 0; i < num_threads; ++i) {
            threads.emplace_back(worker, std::ref(*splits[i]), i);
        }
        for (auto& t : threads) t.join();
    } else {
        worker(adapter, 0);
    }
    
    std::unordered_map<std::string, AggState> buckets;
    std::vector<std::string> order;
    
    for (size_t i = 0; i < num_threads; ++i) {
        for (const auto& k : thread_orders[i]) {
            auto& v = thread_buckets[i][k];
            if (buckets.find(k) == buckets.end()) {
                order.push_back(k);
                buckets[k].first_row = v.first_row;
            }
            auto& dest = buckets[k];
            dest.count += v.count;
            for (auto& kv : v.sums) dest.sums[kv.first] += kv.second;
            for (auto& kv : v.mins) {
                if (!dest.has_vals[kv.first] || kv.second < dest.mins[kv.first]) dest.mins[kv.first] = kv.second;
                dest.has_vals[kv.first] = true;
            }
            for (auto& kv : v.maxs) {
                if (!dest.has_vals[kv.first] || kv.second > dest.maxs[kv.first]) dest.maxs[kv.first] = kv.second;
                dest.has_vals[kv.first] = true;
            }
        }
    }
    
    std::vector<std::string> field_names;
    if (select_node->IsWildcard()) {
        if (!order.empty()) {
            field_names = ResolveWildcard(buckets[order.front()].first_row);
        }
    } else {
        for (const auto& expr : select_node->GetFields()) {
            field_names.push_back(FormatExpression(expr.get()));
        }
    }
    
    std::vector<adapters::Row> results;
    for (const auto& key : order) {
        const auto& state = buckets[key];
        adapters::Row res_row;
        if (select_node->IsWildcard()) {
            res_row = state.first_row;
        } else {
            for (size_t i = 0; i < select_node->GetFields().size(); ++i) {
                res_row[field_names[i]] = EvaluateAggregate(select_node->GetFields()[i].get(), state);
            }
        }
        results.push_back(std::move(res_row));
    }
    
    if (order_by) {
        SortRows(results, order_by);
    }
    
    if (!execute_to_memory_) formatter_.PrintHeader(field_names, format_);

    size_t output_count = 0;
    for (const auto& row : results) {
        if (execute_to_memory_) memory_results_.push_back(row);
        else formatter_.PrintRow(row, field_names, format_);
        output_count++;
        if (limit > 0 && output_count >= limit) break;
    }
}

} // namespace runtime
} // namespace qle
