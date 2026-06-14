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

bool TryParseDouble(const std::string& str, double& out_val) {
    if (str.empty()) return false;
    char* endptr = nullptr;
    out_val = std::strtod(str.c_str(), &endptr);
    return endptr != str.c_str() && *endptr == '\0';
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

void Runtime::CollectIdentifiers(const ast::AstNode* node, std::vector<std::string>& cols) {
    if (!node) return;
    if (const auto* expr = dynamic_cast<const ast::ExpressionNode*>(node)) {
        if (expr->GetToken().type == lexer::TokenType::IDENTIFIER && !expr->IsFunctionCall()) {
            cols.push_back(expr->GetToken().value);
        }
        for (const auto& arg : expr->GetArgs()) {
            CollectIdentifiers(arg.get(), cols);
        }
        CollectIdentifiers(expr->GetLeft(), cols);
        CollectIdentifiers(expr->GetRight(), cols);
    } else if (const auto* select = dynamic_cast<const ast::SelectNode*>(node)) {
        if (!select->IsWildcard()) {
            for (const auto& f : select->GetFields()) CollectIdentifiers(f.get(), cols);
        }
    } else if (const auto* where = dynamic_cast<const ast::WhereNode*>(node)) {
        CollectIdentifiers(where->GetCondition(), cols);
    } else if (const auto* order = dynamic_cast<const ast::OrderByNode*>(node)) {
        cols.push_back(order->GetField());
    } else if (const auto* group = dynamic_cast<const ast::GroupByNode*>(node)) {
        cols.push_back(group->GetField());
    } else if (const auto* join = dynamic_cast<const ast::JoinNode*>(node)) {
        CollectIdentifiers(join->GetCondition(), cols);
    }
}

void Runtime::CollectAggregates(const ast::AstNode* node, std::vector<const ast::ExpressionNode*>& aggs) {
    if (!node) return;
    if (const auto* expr = dynamic_cast<const ast::ExpressionNode*>(node)) {
        if (expr->IsFunctionCall()) {
            std::string name = expr->GetToken().value;
            std::transform(name.begin(), name.end(), name.begin(), ::tolower);
            if (name == "sum" || name == "avg" || name == "min" || name == "max" || name == "count") {
                aggs.push_back(expr);
            }
        }
        for (const auto& arg : expr->GetArgs()) {
            CollectAggregates(arg.get(), aggs);
        }
        CollectAggregates(expr->GetLeft(), aggs);
        CollectAggregates(expr->GetRight(), aggs);
    } else if (const auto* select = dynamic_cast<const ast::SelectNode*>(node)) {
        if (!select->IsWildcard()) {
            for (const auto& f : select->GetFields()) CollectAggregates(f.get(), aggs);
        }
    }
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

std::string Runtime::EvaluateAggregate(const ast::ExpressionNode* expr, const AggState& state) {
    if (state.count == 0) return "";

    if (expr->IsLiteral()) {
        const lexer::Token& t = expr->GetToken();
        if (t.type == lexer::TokenType::STRING || t.type == lexer::TokenType::NUMBER) {
            return t.value;
        } else if (t.type == lexer::TokenType::IDENTIFIER) {
            return EvaluateExpression(expr, state.first_row);
        }
    }

    if (expr->IsFunctionCall()) {
        const lexer::Token& t = expr->GetToken();
        std::string func_name = t.value;
        std::transform(func_name.begin(), func_name.end(), func_name.begin(), [](unsigned char c) { return std::tolower(c); });
        
        bool is_agg = (func_name == "sum" || func_name == "avg" || func_name == "min" || func_name == "max" || func_name == "count");
        if (is_agg) {
            if (func_name == "count") {
                return std::to_string(state.count);
            }
            
            auto it_has = state.has_vals.find(expr);
            if (it_has == state.has_vals.end() || !it_has->second) return "0";
            
            if (func_name == "sum") {
                std::string res = std::to_string(state.sums.at(expr));
                res.erase(res.find_last_not_of('0') + 1, std::string::npos);
                if (res.back() == '.') res.pop_back();
                return res;
            } else if (func_name == "avg") {
                double avg = state.sums.at(expr) / state.count;
                std::string res = std::to_string(avg);
                res.erase(res.find_last_not_of('0') + 1, std::string::npos);
                if (res.back() == '.') res.pop_back();
                return res;
            } else if (func_name == "min") {
                std::string res = std::to_string(state.mins.at(expr));
                res.erase(res.find_last_not_of('0') + 1, std::string::npos);
                if (res.back() == '.') res.pop_back();
                return res;
            } else if (func_name == "max") {
                std::string res = std::to_string(state.maxs.at(expr));
                res.erase(res.find_last_not_of('0') + 1, std::string::npos);
                if (res.back() == '.') res.pop_back();
                return res;
            }
        } else {
            if (func_name == "upper") {
                if (expr->GetArgs().size() != 1) throw errors::RuntimeError("upper() requires 1 argument", t.line, t.col);
                std::string val = EvaluateAggregate(expr->GetArgs()[0].get(), state);
                std::transform(val.begin(), val.end(), val.begin(), [](unsigned char c) { return std::toupper(c); });
                return val;
            } else if (func_name == "lower") {
                if (expr->GetArgs().size() != 1) throw errors::RuntimeError("lower() requires 1 argument", t.line, t.col);
                std::string val = EvaluateAggregate(expr->GetArgs()[0].get(), state);
                std::transform(val.begin(), val.end(), val.begin(), [](unsigned char c) { return std::tolower(c); });
                return val;
            } else if (func_name == "concat") {
                std::string res = "";
                for (const auto& arg : expr->GetArgs()) {
                    res += EvaluateAggregate(arg.get(), state);
                }
                return res;
            } else if (func_name == "length") {
                if (expr->GetArgs().size() != 1) throw errors::RuntimeError("length() requires 1 argument", t.line, t.col);
                std::string val = EvaluateAggregate(expr->GetArgs()[0].get(), state);
                return std::to_string(val.length());
            } else if (func_name == "abs") {
                if (expr->GetArgs().size() != 1) throw errors::RuntimeError("abs() requires 1 argument", t.line, t.col);
                std::string val = EvaluateAggregate(expr->GetArgs()[0].get(), state);
                double num; if (TryParseDouble(val, num)) { return std::to_string(std::abs(num)); } throw errors::RuntimeError("Invalid argument for abs(): " + val, t.line, t.col);
            } else if (func_name == "round") {
                if (expr->GetArgs().size() != 1) throw errors::RuntimeError("round() requires 1 argument", t.line, t.col);
                std::string val = EvaluateAggregate(expr->GetArgs()[0].get(), state);
                double num; if (TryParseDouble(val, num)) { return std::to_string(std::round(num)); } throw errors::RuntimeError("Invalid argument for round(): " + val, t.line, t.col);
            }
            throw errors::RuntimeError("Unsupported function in aggregate context", t.line, t.col);
        }
    }
    
    // binary op
    std::string left_val = EvaluateAggregate(expr->GetLeft(), state);
    std::string right_val = EvaluateAggregate(expr->GetRight(), state);
    // Simple concat for binary op since we don't fully support arithmetic yet
    if (expr->GetToken().type == lexer::TokenType::EQUALS) return left_val == right_val ? "1" : "0";
    throw errors::RuntimeError("Complex binary operators not supported in aggregates", expr->GetToken().line, expr->GetToken().col);
}


bool Runtime::EvaluateHavingCondition(const ast::ExpressionNode* condition, const AggState& agg, const std::string& group_val) {
    if (!condition) return true;
    adapters::Row row = agg.first_row;
    
    row["count(*)"] = std::to_string(agg.count);
    row["count(1)"] = std::to_string(agg.count);
    
    for (const auto& kv : agg.sums) {
        row["sum(" + FormatExpression(kv.first) + ")"] = std::to_string(kv.second);
        row["avg(" + FormatExpression(kv.first) + ")"] = std::to_string(kv.second / agg.count);
    }
    for (const auto& kv : agg.mins) {
        row["min(" + FormatExpression(kv.first) + ")"] = std::to_string(kv.second);
    }
    for (const auto& kv : agg.maxs) {
        row["max(" + FormatExpression(kv.first) + ")"] = std::to_string(kv.second);
    }
    
    return EvaluateCondition(condition, row);
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
    std::string formatted = FormatExpression(expr);
    auto it = row.find(formatted);
    if (it != row.end()) return it->second;

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

} // namespace runtime
} // namespace qle
