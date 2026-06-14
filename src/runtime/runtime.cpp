#include <unordered_map>
#include <string>
#include <cstdlib>

static bool TryParseDouble(const std::string& str, double& out_val) {
    if (str.empty()) return false;
    char* endptr = nullptr;
    out_val = std::strtod(str.c_str(), &endptr);
    return endptr != str.c_str() && *endptr == '\0';
}
#include <limits>
#include <map>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <functional>
#include "adapters/sqlite/sqlite_adapter.h"
#include "runtime/runtime.h"
#include "adapters/csv/csv_adapter.h"
#include "adapters/json/json_adapter.h"
#include "adapters/yaml/yaml_adapter.h"
#include "adapters/xml/xml_adapter.h"
#include "errors/errors.h"
#include "security/limits.h"
#include "logger/logger.h"
#include "utils/suggestions.h"
#include <iostream>
#include <functional>

namespace qle {
namespace runtime {

namespace {
class MemoryAdapter : public adapters::IAdapter {
public:
    explicit MemoryAdapter(std::vector<adapters::Row> rows) : rows_(std::move(rows)), index_(0) {}
    void Open(const std::string& source) override {}
    void Close() override {}
    bool HasNext() override { return index_ < rows_.size(); }
    adapters::Row Next() override { return rows_[index_++]; }
private:
    std::vector<adapters::Row> rows_;
    size_t index_;
};
}

Runtime::Runtime()
    : rows_processed_(0), format_(utils::OutputFormat::CSV) {}

void Runtime::SetFormat(utils::OutputFormat format) {
    format_ = format;
}

std::vector<adapters::Row> Runtime::ExecuteToMemory(const ast::QueryNode* query) {
    execute_to_memory_ = true;
    memory_results_.clear();
    Execute(query);
    return std::move(memory_results_);
}

void Runtime::Execute(const ast::QueryNode* query) {
    Debug::DebugLog("Runtime starting execution");
    rows_processed_ = 0;
    start_time_ = std::chrono::steady_clock::now();

    const ast::SourceNode* source_node = query->GetSource();
    std::unique_ptr<adapters::IAdapter> adapter;
    
    if (source_node->IsSubquery()) {
        Runtime sub_rt;
        auto rows = sub_rt.ExecuteToMemory(source_node->GetSubquery());
        adapter = std::make_unique<MemoryAdapter>(std::move(rows));
    } else {
        std::string source_name = source_node->GetSourceName();
        adapter = GetAdapterForSource(source_name);
        adapter->Open(source_name);
    }

    const ast::WhereNode* where_node = query->GetWhere();
    const std::vector<std::unique_ptr<ast::JoinNode>>& join_nodes = query->GetJoins();
    const ast::SelectNode* select_node = query->GetSelect();
    const ast::OrderByNode* order_by = query->GetOrderBy();
    const ast::GroupByNode* group_by = query->GetGroupBy();
    size_t limit = query->GetLimit();

    if (group_by || HasAggregate(select_node)) {
        ExecuteWithGroupBy(*adapter, select_node, where_node, group_by, order_by, limit);
    } else if (order_by) {
        ExecuteWithOrderBy(*adapter, select_node, where_node, order_by, limit);
    } else {
        ExecuteStreaming(*adapter, select_node, where_node, join_nodes, limit);
    }

    adapter->Close();
    if (!execute_to_memory_) {
        formatter_.Flush(format_);
    }
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
        std::transform(func_name.begin(), func_name.end(), func_name.begin(), [](unsigned char c) { return std::tolower(c); });
        
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
                double val;
                if (TryParseDouble(val_str, val)) {
                    sum += val;
                    if (val < min_val) min_val = val;
                    if (val > max_val) max_val = val;
                    has_val = true;
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
                std::transform(val.begin(), val.end(), val.begin(), [](unsigned char c) { return std::toupper(c); });
                return val;
            } else if (func_name == "lower") {
                if (expr->GetArgs().size() != 1) throw errors::RuntimeError("lower() requires 1 argument", t.line, t.col);
                std::string val = EvaluateAggregate(expr->GetArgs()[0].get(), bucket);
                std::transform(val.begin(), val.end(), val.begin(), [](unsigned char c) { return std::tolower(c); });
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
            } else if (func_name == "abs") {
                if (expr->GetArgs().size() != 1) throw errors::RuntimeError("abs() requires 1 argument", t.line, t.col);
                std::string val = EvaluateAggregate(expr->GetArgs()[0].get(), bucket);
                double num; if (TryParseDouble(val, num)) { return std::to_string(std::abs(num)); } throw errors::RuntimeError("Invalid argument for abs(): " + val, t.line, t.col);
            } else if (func_name == "round") {
                if (expr->GetArgs().size() != 1) throw errors::RuntimeError("round() requires 1 argument", t.line, t.col);
                std::string val = EvaluateAggregate(expr->GetArgs()[0].get(), bucket);
                double num; if (TryParseDouble(val, num)) { return std::to_string(std::round(num)); } throw errors::RuntimeError("Invalid argument for round(): " + val, t.line, t.col);
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
        if (rows_processed_ % 10000 == 0) CheckTimeout();

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
    
    if (!execute_to_memory_) formatter_.PrintHeader(field_names, format_);

    size_t output_count = 0;
    for (const auto& row : results) {
        if (execute_to_memory_) memory_results_.push_back(row);
        else formatter_.PrintRow(row, field_names, format_);
        output_count++;
        if (limit > 0 && output_count >= limit) break;
    }
}

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

            double num_a, num_b;
            if (TryParseDouble(val_a, num_a) && TryParseDouble(val_b, num_b)) {
                return descending ? (num_a > num_b) : (num_a < num_b);
            }
            return descending ? (val_a > val_b) : (val_a < val_b);
        });
}


std::unique_ptr<adapters::IAdapter> Runtime::GetAdapterForSource(
    const std::string& source) {
    if (source.size() > 4 &&
        source.substr(source.size() - 4) == ".csv") {
        return std::make_unique<adapters::csv::CsvAdapter>();
    }
    if (source.size() > 5 &&
        (source.substr(source.size() - 5) == ".yaml" || source.substr(source.size() - 4) == ".yml")) {
        return std::make_unique<adapters::yaml::YamlAdapter>();
    }
    if (source.size() > 4 &&
        source.substr(source.size() - 4) == ".xml") {
        return std::make_unique<adapters::xml::XmlAdapter>();
    }
    if (source.size() > 5 && source.substr(source.size() - 5) == ".json") {
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

    if (op == lexer::TokenType::LIKE) {
        auto match_like = [](const std::string& text, const std::string& pattern) {
            size_t t = 0, p = 0;
            size_t t_len = text.length(), p_len = pattern.length();
            size_t star_idx = std::string::npos;
            size_t match_idx = 0;
            size_t steps = 0;

            while (t < t_len) {
                if (++steps > 10000) throw errors::SecurityError("LIKE operation time limit exceeded");
                if (p < p_len && pattern[p] == '%') {
                    star_idx = p;
                    match_idx = t;
                    p++;
                } else if (p < p_len && (pattern[p] == '_' || text[t] == pattern[p])) {
                    t++;
                    p++;
                } else if (star_idx != std::string::npos) {
                    p = star_idx + 1;
                    match_idx++;
                    t = match_idx;
                } else {
                    return false;
                }
            }
            while (p < p_len && pattern[p] == '%') {
                p++;
            }
            return p == p_len;
        };
        return match_like(left_val, right_val);
    }

    double left_num, right_num;
    if (TryParseDouble(left_val, left_num) && TryParseDouble(right_val, right_num)) {
        switch (op) {
            case lexer::TokenType::EQUALS:         return left_num == right_num;
            case lexer::TokenType::NOT_EQUALS:      return left_num != right_num;
            case lexer::TokenType::GREATER_THAN:    return left_num > right_num;
            case lexer::TokenType::LESS_THAN:       return left_num < right_num;
            case lexer::TokenType::GREATER_EQUALS:  return left_num >= right_num;
            case lexer::TokenType::LESS_EQUALS:     return left_num <= right_num;
            default: break;
        }
    }
    {
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
        std::transform(func_name.begin(), func_name.end(), func_name.begin(), [](unsigned char c) { return std::tolower(c); });
        
        if (func_name == "upper") {
            if (expr->GetArgs().size() != 1) throw errors::RuntimeError("upper() requires 1 argument", t.line, t.col);
            std::string val = EvaluateExpression(expr->GetArgs()[0].get(), row);
            std::transform(val.begin(), val.end(), val.begin(), [](unsigned char c) { return std::toupper(c); });
            return val;
        } else if (func_name == "lower") {
            if (expr->GetArgs().size() != 1) throw errors::RuntimeError("lower() requires 1 argument", t.line, t.col);
            std::string val = EvaluateExpression(expr->GetArgs()[0].get(), row);
            std::transform(val.begin(), val.end(), val.begin(), [](unsigned char c) { return std::tolower(c); });
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
        } else if (func_name == "abs") {
            if (expr->GetArgs().size() != 1) throw errors::RuntimeError("abs() requires 1 argument", t.line, t.col);
            std::string val = EvaluateExpression(expr->GetArgs()[0].get(), row);
            try { return std::to_string(std::abs(std::stod(val))); } catch(...) { throw errors::RuntimeError("Invalid argument for abs(): " + val, t.line, t.col); }
        } else if (func_name == "round") {
            if (expr->GetArgs().size() != 1) throw errors::RuntimeError("round() requires 1 argument", t.line, t.col);
            std::string val = EvaluateExpression(expr->GetArgs()[0].get(), row);
            try { return std::to_string(std::round(std::stod(val))); } catch(...) { throw errors::RuntimeError("Invalid argument for round(): " + val, t.line, t.col); }
        } else if (func_name == "now") {
            if (expr->GetArgs().size() != 0) throw errors::RuntimeError("now() requires 0 arguments", t.line, t.col);
            auto now = std::chrono::system_clock::now();
            auto time_t_now = std::chrono::system_clock::to_time_t(now);
            std::stringstream ss;
            ss << std::put_time(std::gmtime(&time_t_now), "%Y-%m-%dT%H:%M:%SZ");
            return ss.str();
        } else if (func_name == "year") {
            if (expr->GetArgs().size() != 1) throw errors::RuntimeError("year() requires 1 argument", t.line, t.col);
            std::string val = EvaluateExpression(expr->GetArgs()[0].get(), row);
            std::tm tm = {};
            std::stringstream ss(val);
            ss >> std::get_time(&tm, "%Y-%m-%d");
            if (ss.fail()) throw errors::RuntimeError("Invalid date format for year(): " + val, t.line, t.col);
            long long year_val = static_cast<long long>(tm.tm_year) + 1900;
            return std::to_string(year_val);
        } else if (func_name == "month") {
            if (expr->GetArgs().size() != 1) throw errors::RuntimeError("month() requires 1 argument", t.line, t.col);
            std::string val = EvaluateExpression(expr->GetArgs()[0].get(), row);
            std::tm tm = {};
            std::stringstream ss(val);
            ss >> std::get_time(&tm, "%Y-%m-%d");
            if (ss.fail()) throw errors::RuntimeError("Invalid date format for month(): " + val, t.line, t.col);
            std::stringstream res;
            res << std::setfill('0') << std::setw(2) << (tm.tm_mon + 1);
            return res.str();
        } else if (func_name == "day") {
            if (expr->GetArgs().size() != 1) throw errors::RuntimeError("day() requires 1 argument", t.line, t.col);
            std::string val = EvaluateExpression(expr->GetArgs()[0].get(), row);
            std::tm tm = {};
            std::stringstream ss(val);
            ss >> std::get_time(&tm, "%Y-%m-%d");
            if (ss.fail()) throw errors::RuntimeError("Invalid date format for day(): " + val, t.line, t.col);
            std::stringstream res;
            res << std::setfill('0') << std::setw(2) << tm.tm_mday;
            return res.str();
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

void Runtime::CheckTimeout() {
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time_);
    if (static_cast<size_t>(duration.count()) > security::Limits::Get().max_execution_time_ms) {
        throw errors::SecurityError("Maximum execution time exceeded (" + std::to_string(security::Limits::Get().max_execution_time_ms) + " ms)");
    }
}

} // namespace runtime
} // namespace qle
