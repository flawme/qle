import os

with open('src/lexer/token.h', 'r') as f:
    content = f.read()

content = content.replace("BY,", "BY,\n    GROUP,")

with open('src/lexer/token.h', 'w') as f:
    f.write(content)

with open('src/lexer/token.cpp', 'r') as f:
    content = f.read()

content = content.replace('case TokenType::BY: return "BY";', 'case TokenType::BY: return "BY";\n        case TokenType::GROUP: return "GROUP";')

with open('src/lexer/token.cpp', 'w') as f:
    f.write(content)

with open('src/lexer/lexer.cpp', 'r') as f:
    content = f.read()

content = content.replace('if (lower_val == "by") return {TokenType::BY, lower_val, start_line, start_col};', 'if (lower_val == "by") return {TokenType::BY, lower_val, start_line, start_col};\n    if (lower_val == "group") return {TokenType::GROUP, lower_val, start_line, start_col};')

with open('src/lexer/lexer.cpp', 'w') as f:
    f.write(content)
