#include "runtime/runtime.h"
#include "adapters/csv/csv_adapter.h"
#include "adapters/json/json_adapter.h"
#include "errors/errors.h"
#include "security/limits.h"
#include "logger/logger.h"
#include "utils/suggestions.h"
#include <iostream>
#include <algorithm>

namespace qle {
namespace runtime {

Runtime::Runtime()
    : rows_processed_(0), format_(utils::OutputFormat::CSV) {}

void Runtime::SetFormat(utils::OutputFormat format) {
    format_ = format;
}

void Runtime::Execute(const ast::QueryNode* query) {
    Debug::DebugLog("Runtime starting execution");
    rows_processed_ = 0;

    const ast::SourceNode* source_node = query->GetSource();
    std::string source_name = source_node->GetSourceName();

    auto adapter = GetAdapterForSource(source_name);
    adapter->Open(source_name);

    const ast::WhereNode* where_node = query->GetWhere();
    const ast::SelectNode* select_node = query->GetSelect();
    const ast::OrderByNode* order_by = query->GetOrderBy();
    size_t limit = query->GetLimit();
    std::vector<std::string> fields = select_node->GetFields();
    bool is_wildcard = (fields.size() == 1 && fields[0] == "*");

    if (order_by) {
        ExecuteWithOrderBy(*adapter, fields, is_wildcard, where_node, order_by, limit);
    } else {
        ExecuteStreaming(*adapter, fields, is_wildcard, where_node, limit);
    }

    adapter->Close();
    formatter_.Flush(format_);
    Debug::DebugLog("Execution finished. Rows processed: " + std::to_string(rows_processed_));
}

void Runtime::ExecuteStreaming(adapters::IAdapter& adapter,
                               std::vector<std::string>& fields, bool is_wildcard,
                               const ast::WhereNode* where_node, size_t limit) {
    bool header_printed = false;
    size_t output_count = 0;
    bool is_count = (fields.size() == 1 && fields[0] == "count");
    size_t match_count = 0;

    while (adapter.HasNext()) {
        if (rows_processed_ >= security::Limits::Get().max_rows_processed) {
            throw errors::SecurityError("Maximum row limit exceeded.");
        }

        adapters::Row row = adapter.Next();
        rows_processed_++;

        bool include_row = true;
        if (where_node) {
            include_row = EvaluateCondition(where_node->GetCondition(), row);
        }

        if (include_row) {
            if (is_count) {
                match_count++;
            } else {
                if (is_wildcard && !header_printed) {
                    fields = ResolveWildcard(row);
                }
                if (!header_printed) {
                    formatter_.PrintHeader(fields, format_);
                    header_printed = true;
                }
                formatter_.PrintRow(row, fields, format_);
            }
            output_count++;
            if (limit > 0 && output_count >= limit) break;
        }
    }

    if (is_count) {
        std::cout << match_count << std::endl;
    } else if (!header_printed) {
        if (!is_wildcard) {
            formatter_.PrintHeader(fields, format_);
        }
    }
}

void Runtime::ExecuteWithOrderBy(adapters::IAdapter& adapter,
                                  std::vector<std::string>& fields, bool is_wildcard,
                                  const ast::WhereNode* where_node,
                                  const ast::OrderByNode* order_by, size_t limit) {
    std::vector<adapters::Row> matching_rows;

    while (adapter.HasNext()) {
        if (rows_processed_ >= security::Limits::Get().max_rows_processed) {
            throw errors::SecurityError("Maximum row limit exceeded.");
        }

        adapters::Row row = adapter.Next();
        rows_processed_++;

        bool include_row = true;
        if (where_node) {
            include_row = EvaluateCondition(where_node->GetCondition(), row);
        }

        if (include_row) {
            if (is_wildcard && fields.size() == 1 && fields[0] == "*") {
                fields = ResolveWildcard(row);
            }
            matching_rows.push_back(std::move(row));
        }
    }
    
    bool is_count = (fields.size() == 1 && fields[0] == "count");
    if (is_count) {
        std::cout << matching_rows.size() << std::endl;
        return;
    }

    SortRows(matching_rows, order_by);

    formatter_.PrintHeader(fields, format_);
    size_t output_count = 0;
    for (const auto& row : matching_rows) {
        formatter_.PrintRow(row, fields, format_);
        output_count++;
        if (limit > 0 && output_count >= limit) break;
    }
}

std::vector<std::string> Runtime::ResolveWildcard(const adapters::Row& row) {
    std::vector<std::string> fields;
    fields.reserve(row.size());
    for (const auto& pair : row) {
        fields.push_back(pair.first);
    }
    return fields;
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

            // Try numeric comparison first
            try {
                double num_a = std::stod(val_a);
                double num_b = std::stod(val_b);
                return descending ? (num_a > num_b) : (num_a < num_b);
            } catch (...) {
                // Fallback to string comparison
                return descending ? (val_a > val_b) : (val_a < val_b);
            }
        });
}

std::unique_ptr<adapters::IAdapter> Runtime::GetAdapterForSource(
    const std::string& source) {
    if (source.size() > 4 &&
        source.substr(source.size() - 4) == ".csv") {
        return std::make_unique<adapters::csv::CsvAdapter>();
    }
    if (source.size() > 5 &&
        source.substr(source.size() - 5) == ".json") {
        return std::make_unique<adapters::json::JsonAdapter>();
    }
    throw errors::RuntimeError(
        "Unsupported source format or adapter not found for: " + source);
}

bool Runtime::EvaluateCondition(const ast::ExpressionNode* expr,
                                const adapters::Row& row) {
    if (expr->IsLiteral()) {
        throw errors::RuntimeError("Condition must be a boolean expression.",
                                   expr->GetToken().line, expr->GetToken().col);
    }

    lexer::TokenType op = expr->GetToken().type;

    if (op == lexer::TokenType::AND) {
        return EvaluateCondition(expr->GetLeft(), row) &&
               EvaluateCondition(expr->GetRight(), row);
    } else if (op == lexer::TokenType::OR) {
        return EvaluateCondition(expr->GetLeft(), row) ||
               EvaluateCondition(expr->GetRight(), row);
    }

    std::string left_val = EvaluateExpression(expr->GetLeft(), row);
    std::string right_val = EvaluateExpression(expr->GetRight(), row);

    try {
        double left_num = std::stod(left_val);
        double right_num = std::stod(right_val);

        switch (op) {
            case lexer::TokenType::EQUALS:         return left_num == right_num;
            case lexer::TokenType::NOT_EQUALS:      return left_num != right_num;
            case lexer::TokenType::GREATER_THAN:    return left_num > right_num;
            case lexer::TokenType::LESS_THAN:       return left_num < right_num;
            case lexer::TokenType::GREATER_EQUALS:  return left_num >= right_num;
            case lexer::TokenType::LESS_EQUALS:     return left_num <= right_num;
            default: break;
        }
    } catch (...) {
        switch (op) {
            case lexer::TokenType::EQUALS:         return left_val == right_val;
            case lexer::TokenType::NOT_EQUALS:      return left_val != right_val;
            case lexer::TokenType::GREATER_THAN:    return left_val > right_val;
            case lexer::TokenType::LESS_THAN:       return left_val < right_val;
            case lexer::TokenType::GREATER_EQUALS:  return left_val >= right_val;
            case lexer::TokenType::LESS_EQUALS:     return left_val <= right_val;
            default: break;
        }
    }

    throw errors::RuntimeError("Unsupported operator in WHERE clause.",
                               expr->GetToken().line, expr->GetToken().col);
}

std::string Runtime::EvaluateExpression(const ast::ExpressionNode* expr,
                                        const adapters::Row& row) {
    if (expr->IsLiteral()) {
        const lexer::Token& t = expr->GetToken();
        if (t.type == lexer::TokenType::STRING ||
            t.type == lexer::TokenType::NUMBER) {
            return t.value;
        } else if (t.type == lexer::TokenType::IDENTIFIER) {
            auto it = row.find(t.value);
            if (it != row.end()) {
                return it->second;
            }
            std::vector<std::string> available;
            available.reserve(row.size());
            for (const auto& pair : row) {
                available.push_back(pair.first);
            }
            std::string suggestion = utils::SuggestField(t.value, available);
            throw errors::RuntimeError(
                "Unknown field: " + t.value + suggestion, t.line, t.col);
        }
    }
    throw errors::RuntimeError(
        "Complex arithmetic expressions are not supported in MVP.",
        expr->GetToken().line, expr->GetToken().col);
}

} // namespace runtime
} // namespace qle
