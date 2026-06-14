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
#include <functional>
#include <chrono>

namespace qle {
namespace runtime {

void Runtime::ExecuteStreaming(adapters::IAdapter& adapter,
                               const ast::SelectNode* select_node,
                               const ast::WhereNode* where_node, const std::vector<std::unique_ptr<ast::JoinNode>>& join_nodes, size_t limit) {
    bool header_printed = false;
    size_t output_count = 0;
    std::vector<std::string> field_names;

    auto process_row = [&](adapters::Row& row) {
        if (limit > 0 && output_count >= limit) return;
        bool include_row = true;
        if (where_node) {
            include_row = EvaluateCondition(where_node->GetCondition(), row);
        }

        if (include_row) {
            if (select_node->IsWildcard() && !header_printed) {
                field_names = ResolveWildcard(row);
            } else if (!select_node->IsWildcard() && !header_printed) {
                for (const auto& expr : select_node->GetFields()) {
                    field_names.push_back(FormatExpression(expr.get()));
                }
            }
            if (!header_printed) {
                if (!execute_to_memory_) formatter_.PrintHeader(field_names, format_);
                header_printed = true;
            }
            
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
        }
    };

    std::vector<std::unordered_multimap<std::string, adapters::Row>> join_hash_maps(join_nodes.size());
    std::vector<std::vector<adapters::Row>> joined_rows_fallbacks(join_nodes.size());
    std::vector<bool> is_simple_equi_joins(join_nodes.size(), false);
    std::vector<std::string> primary_key_names(join_nodes.size());
    std::vector<std::string> secondary_key_names(join_nodes.size());
    size_t estimated_memory = 0;

    for (size_t i = 0; i < join_nodes.size(); ++i) {
        const auto& join_node = join_nodes[i];
        auto joined_adapter = GetAdapterForSource(join_node->GetSource());
        joined_adapter->Open(join_node->GetSource());

        const ast::ExpressionNode* cond = join_node->GetCondition();
        std::string left_id, right_id;

        if (cond->GetToken().type == lexer::TokenType::EQUALS && cond->GetLeft()->IsLiteral() && cond->GetRight()->IsLiteral()) {
            if (cond->GetLeft()->GetToken().type == lexer::TokenType::IDENTIFIER && cond->GetRight()->GetToken().type == lexer::TokenType::IDENTIFIER) {
                is_simple_equi_joins[i] = true;
                left_id = cond->GetLeft()->GetToken().value;
                right_id = cond->GetRight()->GetToken().value;
            }
        }

        while (joined_adapter->HasNext()) {
            if (rows_processed_ >= security::Limits::Get().max_rows_processed) {
                throw errors::SecurityError("Maximum row limit exceeded.");
            }
            adapters::Row joined_row = joined_adapter->Next();
            rows_processed_++;
            if (rows_processed_ % 10000 == 0) CheckTimeout();

            if (is_simple_equi_joins[i]) {
                if (secondary_key_names[i].empty()) {
                    if (joined_row.find(left_id) != joined_row.end()) {
                        secondary_key_names[i] = left_id;
                        primary_key_names[i] = right_id;
                    } else if (joined_row.find(right_id) != joined_row.end()) {
                        secondary_key_names[i] = right_id;
                        primary_key_names[i] = left_id;
                    } else {
                        secondary_key_names[i] = left_id;
                        primary_key_names[i] = right_id;
                    }
                }
                std::string key_val;
                auto it = joined_row.find(secondary_key_names[i]);
                if (it != joined_row.end()) {
                    key_val = it->second;
                }
                
                size_t row_memory = key_val.capacity() + 64;
                for (const auto& kv : joined_row) {
                    row_memory += kv.first.capacity() + kv.second.capacity() + 64;
                }
                estimated_memory += row_memory;
                if (estimated_memory > security::Limits::Get().max_memory_usage) throw errors::SecurityError("Memory limit exceeded.");
                
                join_hash_maps[i].insert({key_val, std::move(joined_row)});
            } else {
                size_t row_memory = 64;
                for (const auto& kv : joined_row) {
                    row_memory += kv.first.capacity() + kv.second.capacity() + 64;
                }
                estimated_memory += row_memory;
                if (estimated_memory > security::Limits::Get().max_memory_usage) throw errors::SecurityError("Memory limit exceeded.");
                joined_rows_fallbacks[i].push_back(std::move(joined_row));
            }
        }
        joined_adapter->Close();
    }

    std::function<void(size_t, adapters::Row)> evaluate_joins = [&](size_t join_idx, adapters::Row current_row) {
        if (limit > 0 && output_count >= limit) return;
        if (join_idx == join_nodes.size()) {
            process_row(current_row);
            return;
        }
        
        const auto& join_node = join_nodes[join_idx];
        if (is_simple_equi_joins[join_idx]) {
            std::string lookup_val;
            auto it = current_row.find(primary_key_names[join_idx]);
            if (it != current_row.end()) {
                lookup_val = it->second;
            }

            auto range = join_hash_maps[join_idx].equal_range(lookup_val);
            for (auto hash_it = range.first; hash_it != range.second; ++hash_it) {
                adapters::Row next_row = current_row;
                for (const auto& kv : hash_it->second) {
                    next_row[kv.first] = kv.second;
                }
                if (EvaluateCondition(join_node->GetCondition(), next_row)) {
                    evaluate_joins(join_idx + 1, std::move(next_row));
                }
            }
        } else {
            for (const auto& joined_row : joined_rows_fallbacks[join_idx]) {
                adapters::Row next_row = current_row;
                for (const auto& kv : joined_row) {
                    next_row[kv.first] = kv.second;
                }
                if (EvaluateCondition(join_node->GetCondition(), next_row)) {
                    evaluate_joins(join_idx + 1, std::move(next_row));
                }
            }
        }
    };

    while (adapter.HasNext()) {
        if (rows_processed_ >= security::Limits::Get().max_rows_processed) throw errors::SecurityError("Row limit exceeded.");
        adapters::Row primary_row = adapter.Next();
        rows_processed_++;
        if (rows_processed_ % 10000 == 0) CheckTimeout();

        if (!join_nodes.empty()) {
            evaluate_joins(0, std::move(primary_row));
        } else {
            process_row(primary_row);
        }
        if (limit > 0 && output_count >= limit) break;
    }
    
    if (!header_printed && !select_node->IsWildcard()) {
        for (const auto& expr : select_node->GetFields()) {
            field_names.push_back(FormatExpression(expr.get()));
        }
        if (!execute_to_memory_) formatter_.PrintHeader(field_names, format_);
    }
}

} // namespace runtime
} // namespace qle
