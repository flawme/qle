#include "runtime/runtime.h"
#include "adapters/csv/csv_adapter.h"
#include "adapters/json/json_adapter.h"
#include "errors/errors.h"
#include "security/limits.h"
#include "logger/logger.h"
#include <iostream>
#include <algorithm>

namespace qle {
namespace runtime {

Runtime::Runtime() : rows_processed_(0) {}

void Runtime::Execute(const ast::QueryNode* query) {
    Debug::DebugLog("Runtime starting execution");
    rows_processed_ = 0;
    
    const ast::SourceNode* source_node = query->GetSource();
    std::string source_name = source_node->GetSourceName();
    
    auto adapter = GetAdapterForSource(source_name);
    adapter->Open(source_name);
    
    const ast::WhereNode* where_node = query->GetWhere();
    const ast::SelectNode* select_node = query->GetSelect();
    const std::vector<std::string>& fields = select_node->GetFields();

    // Print headers
    for (size_t i = 0; i < fields.size(); ++i) {
        std::cout << fields[i] << (i < fields.size() - 1 ? ", " : "");
    }
    std::cout << std::endl;

    while (adapter->HasNext()) {
        if (rows_processed_ >= security::Limits::Get().max_rows_processed) {
            throw errors::SecurityError("Maximum row limit exceeded.");
        }
        
        adapters::Row row = adapter->Next();
        rows_processed_++;
        
        bool include_row = true;
        if (where_node) {
            include_row = EvaluateCondition(where_node->GetCondition(), row);
        }
        
        if (include_row) {
            PrintRow(row, fields);
        }
    }
    
    adapter->Close();
    Debug::DebugLog("Execution finished. Rows processed: " + std::to_string(rows_processed_));
}

std::unique_ptr<adapters::IAdapter> Runtime::GetAdapterForSource(const std::string& source) {
    // Basic factory logic based on extension
    if (source.size() > 4 && source.substr(source.size() - 4) == ".csv") {
        return std::make_unique<adapters::csv::CsvAdapter>();
    }
    if (source.size() > 5 && source.substr(source.size() - 5) == ".json") {
        return std::make_unique<adapters::json::JsonAdapter>();
    }
    throw errors::RuntimeError("Unsupported source format or adapter not found for: " + source);
}

bool Runtime::EvaluateCondition(const ast::ExpressionNode* expr, const adapters::Row& row) {
    if (expr->IsLiteral()) {
        throw errors::RuntimeError("Condition must be a boolean expression.", expr->GetToken().line, expr->GetToken().col);
    }
    
    lexer::TokenType op = expr->GetToken().type;
    
    if (op == lexer::TokenType::AND) {
        return EvaluateCondition(expr->GetLeft(), row) && EvaluateCondition(expr->GetRight(), row);
    } else if (op == lexer::TokenType::OR) {
        return EvaluateCondition(expr->GetLeft(), row) || EvaluateCondition(expr->GetRight(), row);
    }
    
    std::string left_val = EvaluateExpression(expr->GetLeft(), row);
    std::string right_val = EvaluateExpression(expr->GetRight(), row);

    // Simplistic numeric vs string comparison
    try {
        double left_num = std::stod(left_val);
        double right_num = std::stod(right_val);
        
        switch (op) {
            case lexer::TokenType::EQUALS: return left_num == right_num;
            case lexer::TokenType::NOT_EQUALS: return left_num != right_num;
            case lexer::TokenType::GREATER_THAN: return left_num > right_num;
            case lexer::TokenType::LESS_THAN: return left_num < right_num;
            case lexer::TokenType::GREATER_EQUALS: return left_num >= right_num;
            case lexer::TokenType::LESS_EQUALS: return left_num <= right_num;
            default: break;
        }
    } catch (...) {
        // Fallback to string comparison
        switch (op) {
            case lexer::TokenType::EQUALS: return left_val == right_val;
            case lexer::TokenType::NOT_EQUALS: return left_val != right_val;
            case lexer::TokenType::GREATER_THAN: return left_val > right_val;
            case lexer::TokenType::LESS_THAN: return left_val < right_val;
            case lexer::TokenType::GREATER_EQUALS: return left_val >= right_val;
            case lexer::TokenType::LESS_EQUALS: return left_val <= right_val;
            default: break;
        }
    }
    
    throw errors::RuntimeError("Unsupported operator in WHERE clause.", expr->GetToken().line, expr->GetToken().col);
}

std::string Runtime::EvaluateExpression(const ast::ExpressionNode* expr, const adapters::Row& row) {
    if (expr->IsLiteral()) {
        const lexer::Token& t = expr->GetToken();
        if (t.type == lexer::TokenType::STRING || t.type == lexer::TokenType::NUMBER) {
            return t.value;
        } else if (t.type == lexer::TokenType::IDENTIFIER) {
            auto it = row.find(t.value);
            if (it != row.end()) {
                return it->second;
            }
            throw errors::RuntimeError("Unknown field: " + t.value, t.line, t.col);
        }
    }
    throw errors::RuntimeError("Complex arithmetic expressions are not supported in MVP.", expr->GetToken().line, expr->GetToken().col);
}

void Runtime::PrintRow(const adapters::Row& row, const std::vector<std::string>& fields) {
    for (size_t i = 0; i < fields.size(); ++i) {
        auto it = row.find(fields[i]);
        if (it != row.end()) {
            std::cout << it->second;
        } else {
            std::cout << "NULL";
        }
        if (i < fields.size() - 1) {
            std::cout << ", ";
        }
    }
    std::cout << std::endl;
}

} // namespace runtime
} // namespace qle
