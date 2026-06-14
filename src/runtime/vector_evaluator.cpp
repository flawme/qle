#include "runtime/vector_evaluator.h"
#include "errors/errors.h"
#include "lexer/token.h"
#include <cstdlib>
#include <algorithm>
#include <stdexcept>
#include <iostream>

namespace qle {
namespace runtime {

struct VectorValue {
    enum class Type { BOOLEAN, NUMERIC, STRING, NONE };
    Type type = Type::NONE;
    bool is_scalar = false;

    // Owned data for intermediate results
    std::vector<bool> bool_data;
    std::vector<double> numeric_data;
    std::vector<std::string> string_data;

    // References to avoid copying base columns
    const std::vector<double>* numeric_ref = nullptr;
    const std::vector<std::string>* string_ref = nullptr;

    bool bool_scalar = false;
    double numeric_scalar = 0.0;
    std::string string_scalar;

    const double* GetNumericPtr() const {
        if (is_scalar) return nullptr;
        return numeric_ref ? numeric_ref->data() : numeric_data.data();
    }
    
    const std::string* GetStringPtr() const {
        if (is_scalar) return nullptr;
        return string_ref ? string_ref->data() : string_data.data();
    }
};

static bool TryParseDouble(const std::string& str, double& out_val) {
    if (str.empty()) return false;
    char* endptr = nullptr;
    out_val = std::strtod(str.c_str(), &endptr);
    return endptr != str.c_str() && *endptr == '\0';
}

static VectorValue EvalVectorized(const ast::ExpressionNode* expr, const VectorBatch& batch) {
    if (expr->IsLiteral()) {
        const lexer::Token& t = expr->GetToken();
        if (t.type == lexer::TokenType::NUMBER) {
            VectorValue val;
            val.type = VectorValue::Type::NUMERIC;
            val.is_scalar = true;
            TryParseDouble(t.value, val.numeric_scalar);
            return val;
        } else if (t.type == lexer::TokenType::STRING) {
            VectorValue val;
            val.type = VectorValue::Type::STRING;
            val.is_scalar = true;
            val.string_scalar = t.value;
            return val;
        } else if (t.type == lexer::TokenType::IDENTIFIER) {
            auto num_it = batch.numeric_columns.find(t.value);
            if (num_it != batch.numeric_columns.end()) {
                VectorValue val;
                val.type = VectorValue::Type::NUMERIC;
                val.is_scalar = false;
                val.numeric_ref = &num_it->second;
                return val;
            }
            auto str_it = batch.string_columns.find(t.value);
            if (str_it != batch.string_columns.end()) {
                VectorValue val;
                val.type = VectorValue::Type::STRING;
                val.is_scalar = false;
                val.string_ref = &str_it->second;
                return val;
            }
            throw errors::RuntimeError("Column not found in batch: " + t.value, t.line, t.col);
        }
    }

    lexer::TokenType op = expr->GetToken().type;
    
    // Logical operators
    if (op == lexer::TokenType::AND || op == lexer::TokenType::OR) {
        VectorValue left = EvalVectorized(expr->GetLeft(), batch);
        VectorValue right = EvalVectorized(expr->GetRight(), batch);
        
        if (left.type != VectorValue::Type::BOOLEAN || right.type != VectorValue::Type::BOOLEAN) {
            throw errors::RuntimeError("Logical operators require boolean operands.", expr->GetToken().line, expr->GetToken().col);
        }

        VectorValue result;
        result.type = VectorValue::Type::BOOLEAN;
        result.is_scalar = false;
        result.bool_data.resize(batch.num_rows);

        if (op == lexer::TokenType::AND) {
            for (size_t i = 0; i < batch.num_rows; ++i) {
                result.bool_data[i] = left.bool_data[i] && right.bool_data[i];
            }
        } else {
            for (size_t i = 0; i < batch.num_rows; ++i) {
                result.bool_data[i] = left.bool_data[i] || right.bool_data[i];
            }
        }
        return result;
    }

    // Comparison operators
    VectorValue left = EvalVectorized(expr->GetLeft(), batch);
    VectorValue right = EvalVectorized(expr->GetRight(), batch);

    VectorValue result;
    result.type = VectorValue::Type::BOOLEAN;
    result.is_scalar = false;
    result.bool_data.resize(batch.num_rows);

    if (left.type == VectorValue::Type::NUMERIC && right.type == VectorValue::Type::NUMERIC) {
        if (!left.is_scalar && right.is_scalar) {
            const double* l_ptr = left.GetNumericPtr();
            double r_val = right.numeric_scalar;
            switch (op) {
                case lexer::TokenType::GREATER_THAN:   for(size_t i=0; i<batch.num_rows; ++i) result.bool_data[i] = l_ptr[i] > r_val; break;
                case lexer::TokenType::LESS_THAN:      for(size_t i=0; i<batch.num_rows; ++i) result.bool_data[i] = l_ptr[i] < r_val; break;
                case lexer::TokenType::GREATER_EQUALS: for(size_t i=0; i<batch.num_rows; ++i) result.bool_data[i] = l_ptr[i] >= r_val; break;
                case lexer::TokenType::LESS_EQUALS:    for(size_t i=0; i<batch.num_rows; ++i) result.bool_data[i] = l_ptr[i] <= r_val; break;
                case lexer::TokenType::EQUALS:         for(size_t i=0; i<batch.num_rows; ++i) result.bool_data[i] = l_ptr[i] == r_val; break;
                case lexer::TokenType::NOT_EQUALS:     for(size_t i=0; i<batch.num_rows; ++i) result.bool_data[i] = l_ptr[i] != r_val; break;
                default: throw errors::RuntimeError("Unsupported operator", expr->GetToken().line, expr->GetToken().col);
            }
        } else if (left.is_scalar && !right.is_scalar) {
            double l_val = left.numeric_scalar;
            const double* r_ptr = right.GetNumericPtr();
            switch (op) {
                case lexer::TokenType::GREATER_THAN:   for(size_t i=0; i<batch.num_rows; ++i) result.bool_data[i] = l_val > r_ptr[i]; break;
                case lexer::TokenType::LESS_THAN:      for(size_t i=0; i<batch.num_rows; ++i) result.bool_data[i] = l_val < r_ptr[i]; break;
                case lexer::TokenType::GREATER_EQUALS: for(size_t i=0; i<batch.num_rows; ++i) result.bool_data[i] = l_val >= r_ptr[i]; break;
                case lexer::TokenType::LESS_EQUALS:    for(size_t i=0; i<batch.num_rows; ++i) result.bool_data[i] = l_val <= r_ptr[i]; break;
                case lexer::TokenType::EQUALS:         for(size_t i=0; i<batch.num_rows; ++i) result.bool_data[i] = l_val == r_ptr[i]; break;
                case lexer::TokenType::NOT_EQUALS:     for(size_t i=0; i<batch.num_rows; ++i) result.bool_data[i] = l_val != r_ptr[i]; break;
                default: throw errors::RuntimeError("Unsupported operator", expr->GetToken().line, expr->GetToken().col);
            }
        } else if (!left.is_scalar && !right.is_scalar) {
            const double* l_ptr = left.GetNumericPtr();
            const double* r_ptr = right.GetNumericPtr();
            switch (op) {
                case lexer::TokenType::GREATER_THAN:   for(size_t i=0; i<batch.num_rows; ++i) result.bool_data[i] = l_ptr[i] > r_ptr[i]; break;
                case lexer::TokenType::LESS_THAN:      for(size_t i=0; i<batch.num_rows; ++i) result.bool_data[i] = l_ptr[i] < r_ptr[i]; break;
                case lexer::TokenType::GREATER_EQUALS: for(size_t i=0; i<batch.num_rows; ++i) result.bool_data[i] = l_ptr[i] >= r_ptr[i]; break;
                case lexer::TokenType::LESS_EQUALS:    for(size_t i=0; i<batch.num_rows; ++i) result.bool_data[i] = l_ptr[i] <= r_ptr[i]; break;
                case lexer::TokenType::EQUALS:         for(size_t i=0; i<batch.num_rows; ++i) result.bool_data[i] = l_ptr[i] == r_ptr[i]; break;
                case lexer::TokenType::NOT_EQUALS:     for(size_t i=0; i<batch.num_rows; ++i) result.bool_data[i] = l_ptr[i] != r_ptr[i]; break;
                default: throw errors::RuntimeError("Unsupported operator", expr->GetToken().line, expr->GetToken().col);
            }
        } else {
            // scalar vs scalar
            double l_val = left.numeric_scalar;
            double r_val = right.numeric_scalar;
            bool cmp_res = false;
            switch (op) {
                case lexer::TokenType::GREATER_THAN:   cmp_res = l_val > r_val; break;
                case lexer::TokenType::LESS_THAN:      cmp_res = l_val < r_val; break;
                case lexer::TokenType::GREATER_EQUALS: cmp_res = l_val >= r_val; break;
                case lexer::TokenType::LESS_EQUALS:    cmp_res = l_val <= r_val; break;
                case lexer::TokenType::EQUALS:         cmp_res = l_val == r_val; break;
                case lexer::TokenType::NOT_EQUALS:     cmp_res = l_val != r_val; break;
                default: throw errors::RuntimeError("Unsupported operator", expr->GetToken().line, expr->GetToken().col);
            }
            for(size_t i=0; i<batch.num_rows; ++i) result.bool_data[i] = cmp_res;
        }
    } else if (left.type == VectorValue::Type::STRING || right.type == VectorValue::Type::STRING) {
        // Evaluate strings
        auto get_str = [&](const VectorValue& v, size_t idx) -> std::string {
            if (v.is_scalar) return v.string_scalar;
            const std::string* ptr = v.GetStringPtr();
            if (ptr) return ptr[idx];
            // If it was a number compared to a string, convert on the fly
            if (v.type == VectorValue::Type::NUMERIC) {
                if (v.is_scalar) return std::to_string(v.numeric_scalar);
                return std::to_string(v.GetNumericPtr()[idx]);
            }
            return "";
        };

        for (size_t i = 0; i < batch.num_rows; ++i) {
            std::string l_val = get_str(left, i);
            std::string r_val = get_str(right, i);
            bool cmp_res = false;
            if (op == lexer::TokenType::LIKE) {
                 // Simple LIKE implementation for strings
                 // A real SIMD engine would optimize this, but for now we fallback to scalar execution per row
                 auto match_like = [](const std::string& text, const std::string& pattern) {
                    size_t t = 0, p = 0;
                    size_t t_len = text.length(), p_len = pattern.length();
                    size_t star_idx = std::string::npos;
                    size_t match_idx = 0;
                    while (t < t_len) {
                        if (p < p_len && pattern[p] == '%') {
                            star_idx = p;
                            match_idx = t;
                            p++;
                        } else if (p < p_len && (pattern[p] == '_' || text[t] == pattern[p])) {
                            t++; p++;
                        } else if (star_idx != std::string::npos) {
                            p = star_idx + 1;
                            match_idx++;
                            t = match_idx;
                        } else {
                            return false;
                        }
                    }
                    while (p < p_len && pattern[p] == '%') p++;
                    return p == p_len;
                };
                cmp_res = match_like(l_val, r_val);
            } else {
                switch (op) {
                    case lexer::TokenType::GREATER_THAN:   cmp_res = l_val > r_val; break;
                    case lexer::TokenType::LESS_THAN:      cmp_res = l_val < r_val; break;
                    case lexer::TokenType::GREATER_EQUALS: cmp_res = l_val >= r_val; break;
                    case lexer::TokenType::LESS_EQUALS:    cmp_res = l_val <= r_val; break;
                    case lexer::TokenType::EQUALS:         cmp_res = l_val == r_val; break;
                    case lexer::TokenType::NOT_EQUALS:     cmp_res = l_val != r_val; break;
                    default: throw errors::RuntimeError("Unsupported operator", expr->GetToken().line, expr->GetToken().col);
                }
            }
            result.bool_data[i] = cmp_res;
        }
    } else {
        throw errors::RuntimeError("Unsupported types for vector evaluation", expr->GetToken().line, expr->GetToken().col);
    }
    
    return result;
}

std::vector<bool> EvaluateConditionVectorized(const ast::ExpressionNode* condition, const VectorBatch& batch) {
    VectorValue result = EvalVectorized(condition, batch);
    if (result.type != VectorValue::Type::BOOLEAN) {
        throw errors::RuntimeError("Condition must evaluate to boolean", condition->GetToken().line, condition->GetToken().col);
    }
    if (result.is_scalar) {
        return std::vector<bool>(batch.num_rows, result.bool_scalar);
    }
    return result.bool_data;
}

} // namespace runtime
} // namespace qle
