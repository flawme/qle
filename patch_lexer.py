import os

token_h = "src/lexer/token.h"
with open(token_h, "r") as f:
    content = f.read()
if "JOIN," not in content:
    content = content.replace("DESC,", "DESC,\n    JOIN,\n    ON,")
with open(token_h, "w") as f:
    f.write(content)

lexer_cpp = "src/lexer/lexer.cpp"
with open(lexer_cpp, "r") as f:
    content = f.read()
if "lower_val == \"join\"" not in content:
    content = content.replace("if (lower_val == \"and\")", "if (lower_val == \"join\") return {TokenType::JOIN, lower_val, start_line, start_col};\n    if (lower_val == \"on\") return {TokenType::ON, lower_val, start_line, start_col};\n    if (lower_val == \"and\")")
with open(lexer_cpp, "w") as f:
    f.write(content)

token_cpp = "src/lexer/token.cpp"
with open(token_cpp, "r") as f:
    content = f.read()
if "TokenType::JOIN" not in content:
    content = content.replace("case TokenType::DESC: return \"DESC\";", "case TokenType::DESC: return \"DESC\";\n        case TokenType::JOIN: return \"JOIN\";\n        case TokenType::ON: return \"ON\";")
with open(token_cpp, "w") as f:
    f.write(content)

