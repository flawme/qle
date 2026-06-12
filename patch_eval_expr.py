import os

with open('src/runtime/runtime.cpp', 'r') as f:
    content = f.read()

eval_expr_body = """std::string Runtime::EvaluateExpression(const ast::ExpressionNode* expr,
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
}"""

start_idx = content.find("std::string Runtime::EvaluateExpression")
end_idx = content.find("} // namespace runtime")

content = content[:start_idx] + eval_expr_body + "\n\n" + content[end_idx:]

with open('src/runtime/runtime.cpp', 'w') as f:
    f.write(content)

