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

namespace qle {
namespace runtime {

void Runtime::ExecuteWithOrderBy(adapters::IAdapter& adapter,
                                 const ast::SelectNode* select_node,
                                 const ast::WhereNode* where_node,
                                 const ast::OrderByNode* order_by, size_t limit) {
    std::vector<adapters::Row> matching_rows;
    std::vector<std::string> field_names;
    bool wildcard_resolved = false;

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
    
    if (!select_node->IsWildcard()) {
        for (const auto& expr : select_node->GetFields()) {
            field_names.push_back(FormatExpression(expr.get()));
        }
    }

    SortRows(matching_rows, order_by);

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

    std::stable_sort(rows.begin(), rows.end(),
        [&field, descending](const adapters::Row& a, const adapters::Row& b) {
            auto it_a = a.find(field);
            auto it_b = b.find(field);
            std::string val_a = (it_a != a.end()) ? it_a->second : "";
            std::string val_b = (it_b != b.end()) ? it_b->second : "";

            double num_a, num_b;
            // Hack to access TryParseDouble since it was local to evaluator.cpp. Wait.
            // Oh right, TryParseDouble is needed here.
            char* endptr_a = nullptr; char* endptr_b = nullptr;
            num_a = std::strtod(val_a.c_str(), &endptr_a);
            num_b = std::strtod(val_b.c_str(), &endptr_b);
            bool is_num_a = (!val_a.empty() && endptr_a != val_a.c_str() && *endptr_a == '\0');
            bool is_num_b = (!val_b.empty() && endptr_b != val_b.c_str() && *endptr_b == '\0');

            if (is_num_a && is_num_b) {
                return descending ? (num_a > num_b) : (num_a < num_b);
            }
            return descending ? (val_a > val_b) : (val_a < val_b);
        });
}

} // namespace runtime
} // namespace qle
