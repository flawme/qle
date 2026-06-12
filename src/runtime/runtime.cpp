#include <limits>
#include <map>
#include "adapters/sqlite/sqlite_adapter.h"
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
    const ast::GroupByNode* group_by = query->GetGroupBy();
    size_t limit = query->GetLimit();

    if (group_by || HasAggregate(select_node)) {
        ExecuteWithGroupBy(*adapter, select_node, where_node, group_by, order_by, limit);
    } else if (order_by) {
        ExecuteWithOrderBy(*adapter, select_node, where_node, order_by, limit);
    } else {
        ExecuteStreaming(*adapter, select_node, where_node, limit);
    }

    adapter->Close();
    formatter_.Flush(format_);
    Debug::DebugLog("Execution finished. Rows processed: " + std::to_string(rows_processed_));
}

bool Runtime::CheckIfContainsAggregate(const ast::ExpressionNode* expr) {
    if (!expr) return false;
    if (expr->IsFunctionCall()) {
        std::string name = expr->GetToken().value;
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);
        if (name == "sum" || name == "avg" || name == "min" || name == "max" || name == "count") return true;
        for (const auto& arg : expr->GetArgs()) {
            if (CheckIfContainsAggregate(arg.get())) return true;
        }
    }
    if (!expr->IsLiteral() && !expr->IsFunctionCall()) {
        return CheckIfContainsAggregate(expr->GetLeft()) || CheckIfContainsAggregate(expr->GetRight());
    }
    return false;
}

bool Runtime::HasAggregate(const ast::SelectNode* select_node) {
    if (select_node->IsWildcard()) return false;
    for (const auto& expr : select_node->GetFields()) {
        if (CheckIfContainsAggregate(expr.get())) return true;
    }
    return false;
}

std::string Runtime::FormatExpression(const ast::ExpressionNode* expr) {
    if (expr->IsLiteral()) {
        return expr->GetToken().value;
    }
    if (expr->IsFunctionCall()) {
        std::string res = expr->GetToken().value + "(";
        const auto& args = expr->GetArgs();
        for (size_t i = 0; i < args.size(); ++i) {
            res += FormatExpression(args[i].get());
            if (i < args.size() - 1) res += ", ";
        }
        res += ")";
        return res;
    }
    return FormatExpression(expr->GetLeft()) + " " + expr->GetToken().value + " " + FormatExpression(expr->GetRight());
}

std::string Runtime::EvaluateAggregate(const ast::ExpressionNode* expr, const std::vector<adapters::Row>& bucket) {
    if (bucket.empty()) return "";

    if (expr->IsLiteral()) {
        const lexer::Token& t = expr->GetToken();
        if (t.type == lexer::TokenType::STRING || t.type == lexer::TokenType::NUMBER) {
            return t.value;
        } else if (t.type == lexer::TokenType::IDENTIFIER) {
            return EvaluateExpression(expr, bucket.front());
        }
    }

    if (expr->IsFunctionCall()) {
        const lexer::Token& t = expr->GetToken();
        std::string func_name = t.value;
        std::transform(func_name.begin(), func_name.end(), func_name.begin(), ::tolower);
        
        bool is_agg = (func_name == "sum" || func_name == "avg" || func_name == "min" || func_name == "max" || func_name == "count");
        if (is_agg) {
            if (func_name == "count") {
                return std::to_string(bucket.size());
            }
            
            if (expr->GetArgs().size() != 1) {
                throw errors::RuntimeError("Aggregate function requires exactly 1 argument", t.line, t.col);
            }
            const auto* arg = expr->GetArgs()[0].get();
            
            double sum = 0.0;
            double min_val = std::numeric_limits<double>::max();
            double max_val = std::numeric_limits<double>::lowest();
            bool has_val = false;
            
            for (const auto& row : bucket) {
                std::string val_str = EvaluateExpression(arg, row);
                try {
                    double val = std::stod(val_str);
                    sum += val;
                    if (val < min_val) min_val = val;
                    if (val > max_val) max_val = val;
                    has_val = true;
                } catch(...) {
                }
            }
            
            if (!has_val) return "0";
            
            if (func_name == "sum") {
                std::string res = std::to_string(sum);
                res.erase(res.find_last_not_of('0') + 1, std::string::npos);
                if (res.back() == '.') res.pop_back();
                return res;
            } else if (func_name == "avg") {
                double avg = sum / bucket.size();
                std::string res = std::to_string(avg);
                res.erase(res.find_last_not_of('0') + 1, std::string::npos);
                if (res.back() == '.') res.pop_back();
                return res;
            } else if (func_name == "min") {
                std::string res = std::to_string(min_val);
                res.erase(res.find_last_not_of('0') + 1, std::string::npos);
                if (res.back() == '.') res.pop_back();
                return res;
            } else if (func_name == "max") {
                std::string res = std::to_string(max_val);
                res.erase(res.find_last_not_of('0') + 1, std::string::npos);
                if (res.back() == '.') res.pop_back();
                return res;
            }
        } else {
            if (func_name == "upper") {
                if (expr->GetArgs().size() != 1) throw errors::RuntimeError("upper() requires 1 argument", t.line, t.col);
                std::string val = EvaluateAggregate(expr->GetArgs()[0].get(), bucket);
                std::transform(val.begin(), val.end(), val.begin(), ::toupper);
                return val;
            } else if (func_name == "lower") {
                if (expr->GetArgs().size() != 1) throw errors::RuntimeError("lower() requires 1 argument", t.line, t.col);
                std::string val = EvaluateAggregate(expr->GetArgs()[0].get(), bucket);
                std::transform(val.begin(), val.end(), val.begin(), ::tolower);
                return val;
            } else if (func_name == "concat") {
                std::string res = "";
                for (const auto& arg : expr->GetArgs()) {
                    res += EvaluateAggregate(arg.get(), bucket);
                }
                return res;
            } else if (func_name == "length") {
                if (expr->GetArgs().size() != 1) throw errors::RuntimeError("length() requires 1 argument", t.line, t.col);
                std::string val = EvaluateAggregate(expr->GetArgs()[0].get(), bucket);
                return std::to_string(val.length());
            }
            throw errors::RuntimeError("Unsupported function in aggregate context", t.line, t.col);
        }
    }
    
    // binary op
    std::string left_val = EvaluateAggregate(expr->GetLeft(), bucket);
    std::string right_val = EvaluateAggregate(expr->GetRight(), bucket);
    // Simple concat for binary op since we don't fully support arithmetic yet
    if (expr->GetToken().type == lexer::TokenType::EQUALS) return left_val == right_val ? "1" : "0";
    throw errors::RuntimeError("Complex binary operators not supported in aggregates", expr->GetToken().line, expr->GetToken().col);
}

void Runtime::ExecuteWithGroupBy(adapters::IAdapter& adapter,
                                 const ast::SelectNode* select_node,
                                 const ast::WhereNode* where_node,
                                 const ast::GroupByNode* group_by,
                                 const ast::OrderByNode* order_by, size_t limit) {
    std::map<std::string, std::vector<adapters::Row>> buckets;
    std::vector<std::string> order; // To preserve insertion order if order_by is null
    
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
            std::string key = "";
            if (group_by) {
                auto it = row.find(group_by->GetField());
                if (it != row.end()) {
                    key = it->second;
                }
            }
            if (buckets.find(key) == buckets.end()) {
                order.push_back(key);
            }
            buckets[key].push_back(std::move(row));
        }
    }
    
    std::vector<std::string> field_names;
    if (select_node->IsWildcard()) {
        // Can't wildcard an aggregate without explicit fields, just take first bucket first row
        if (!buckets.empty()) {
            field_names = ResolveWildcard(buckets.begin()->second.front());
        }
    } else {
        for (const auto& expr : select_node->GetFields()) {
            field_names.push_back(FormatExpression(expr.get()));
        }
    }
    
    std::vector<adapters::Row> results;
    for (const auto& key : order) {
        const auto& bucket = buckets[key];
        adapters::Row res_row;
        if (select_node->IsWildcard()) {
            res_row = bucket.front();
        } else {
            for (size_t i = 0; i < select_node->GetFields().size(); ++i) {
                res_row[field_names[i]] = EvaluateAggregate(select_node->GetFields()[i].get(), bucket);
            }
        }
        results.push_back(std::move(res_row));
    }
    
    if (order_by) {
        SortRows(results, order_by);
    }
    
    formatter_.PrintHeader(field_names, format_);
    size_t output_count = 0;
    for (const auto& row : results) {
        formatter_.PrintRow(row, field_names, format_);
        output_count++;
        if (limit > 0 && output_count >= limit) break;
    }
}

void Runtime::ExecuteStreaming(adapters::IAdapter& adapter,
                               const ast::SelectNode* select_node,
                               const ast::WhereNode* where_node, size_t limit) {
    bool header_printed = false;
    size_t output_count = 0;
    std::vector<std::string> field_names;

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
            if (select_node->IsWildcard() && !header_printed) {
                field_names = ResolveWildcard(row);
            } else if (!select_node->IsWildcard() && !header_printed) {
                for (const auto& expr : select_node->GetFields()) {
                    field_names.push_back(FormatExpression(expr.get()));
                }
            }
            if (!header_printed) {
                formatter_.PrintHeader(field_names, format_);
                header_printed = true;
            }
            
            if (select_node->IsWildcard()) {
                formatter_.PrintRow(row, field_names, format_);
            } else {
                adapters::Row out_row;
                for (size_t i = 0; i < select_node->GetFields().size(); ++i) {
                    out_row[field_names[i]] = EvaluateExpression(select_node->GetFields()[i].get(), row);
                }
                formatter_.PrintRow(out_row, field_names, format_);
            }
            output_count++;
            if (limit > 0 && output_count >= limit) break;
        }
    }

    if (!header_printed && !select_node->IsWildcard()) {
        for (const auto& expr : select_node->GetFields()) {
            field_names.push_back(FormatExpression(expr.get()));
        }
        formatter_.PrintHeader(field_names, format_);
    }
}

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

    formatter_.PrintHeader(field_names, format_);
    size_t output_count = 0;
    for (const auto& row : matching_rows) {
        if (select_node->IsWildcard()) {
            formatter_.PrintRow(row, field_names, format_);
        } else {
            adapters::Row out_row;
            for (size_t i = 0; i < select_node->GetFields().size(); ++i) {
                out_row[field_names[i]] = EvaluateExpression(select_node->GetFields()[i].get(), row);
            }
            formatter_.PrintRow(out_row, field_names, format_);
        }
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

            try {
                double num_a = std::stod(val_a);
                double num_b = std::stod(val_b);
                return descending ? (num_a > num_b) : (num_a < num_b);
            } catch (...) {
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
    if (source.find(".sqlite") != std::string::npos || source.find(".db") != std::string::npos) {
        return std::make_unique<adapters::SQLiteAdapter>();
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
    
    if (expr->IsFunctionCall()) {
        const lexer::Token& t = expr->GetToken();
        std::string func_name = t.value;
        std::transform(func_name.begin(), func_name.end(), func_name.begin(), ::tolower);
        
        if (func_name == "upper") {
            if (expr->GetArgs().size() != 1) throw errors::RuntimeError("upper() requires 1 argument", t.line, t.col);
            std::string val = EvaluateExpression(expr->GetArgs()[0].get(), row);
            std::transform(val.begin(), val.end(), val.begin(), ::toupper);
            return val;
        } else if (func_name == "lower") {
            if (expr->GetArgs().size() != 1) throw errors::RuntimeError("lower() requires 1 argument", t.line, t.col);
            std::string val = EvaluateExpression(expr->GetArgs()[0].get(), row);
            std::transform(val.begin(), val.end(), val.begin(), ::tolower);
            return val;
        } else if (func_name == "concat") {
            std::string res = "";
            for (const auto& arg : expr->GetArgs()) {
                res += EvaluateExpression(arg.get(), row);
            }
            return res;
        } else if (func_name == "length") {
            if (expr->GetArgs().size() != 1) throw errors::RuntimeError("length() requires 1 argument", t.line, t.col);
            std::string val = EvaluateExpression(expr->GetArgs()[0].get(), row);
            return std::to_string(val.length());
        }
        
        // Aggregate function used outside aggregate context, just return an empty string or evaluate the inner part?
        // Let's just return what EvaluateAggregate does for one row.
        bool is_agg = (func_name == "sum" || func_name == "avg" || func_name == "min" || func_name == "max");
        if (is_agg) {
            if (expr->GetArgs().size() != 1) throw errors::RuntimeError("Aggregate requires 1 argument", t.line, t.col);
            return EvaluateExpression(expr->GetArgs()[0].get(), row);
        }
        if (func_name == "count") return "1";
        
        throw errors::RuntimeError("Unsupported function: " + func_name, t.line, t.col);
    }
    
    throw errors::RuntimeError(
        "Complex arithmetic expressions are not supported in MVP.",
        expr->GetToken().line, expr->GetToken().col);
}

} // namespace runtime
} // namespace qle
