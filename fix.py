with open("src/runtime/runtime.cpp", "r") as f:
    text = f.read()

text = text.replace("#include <unordered_map>", """#include <unordered_map>
#include <string>
#include <cstdlib>

static bool TryParseDouble(const std::string& str, double& out_val) {
    if (str.empty()) return false;
    char* endptr = nullptr;
    out_val = std::strtod(str.c_str(), &endptr);
    return endptr != str.c_str() && *endptr == '\\0';
}""")

text = text.replace("""                try {
                    double val = std::stod(val_str);
                    sum += val;
                    if (val < min_val) min_val = val;
                    if (val > max_val) max_val = val;
                    has_val = true;
                } catch(...) {
                }""", """                double val;
                if (TryParseDouble(val_str, val)) {
                    sum += val;
                    if (val < min_val) min_val = val;
                    if (val > max_val) max_val = val;
                    has_val = true;
                }""")

text = text.replace("""            try {
                double num_a = std::stod(val_a);
                double num_b = std::stod(val_b);
                return descending ? (num_a > num_b) : (num_a < num_b);
            } catch (...) {
                return descending ? (val_a > val_b) : (val_a < val_b);
            }""", """            double num_a, num_b;
            if (TryParseDouble(val_a, num_a) && TryParseDouble(val_b, num_b)) {
                return descending ? (num_a > num_b) : (num_a < num_b);
            }
            return descending ? (val_a > val_b) : (val_a < val_b);""")

text = text.replace("""    try {
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
    } catch (...) {""", """    double left_num, right_num;
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
    {""") 

text = text.replace("""                try { return std::to_string(std::abs(std::stod(val))); } catch(...) { throw errors::RuntimeError("Invalid argument for abs(): " + val, t.line, t.col); }""", """                double num; if (TryParseDouble(val, num)) { return std::to_string(std::abs(num)); } throw errors::RuntimeError("Invalid argument for abs(): " + val, t.line, t.col);""")

text = text.replace("""                try { return std::to_string(std::round(std::stod(val))); } catch(...) { throw errors::RuntimeError("Invalid argument for round(): " + val, t.line, t.col); }""", """                double num; if (TryParseDouble(val, num)) { return std::to_string(std::round(num)); } throw errors::RuntimeError("Invalid argument for round(): " + val, t.line, t.col);""")

with open("src/runtime/runtime.cpp", "w") as f:
    f.write(text)
