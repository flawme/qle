import sys

with open("src/lexer/lexer.cpp", "r") as f:
    content = f.read()

# Replace ReadIdentifierOrKeyword
old_ident = """Token Lexer::ReadIdentifierOrKeyword() {
    size_t start_line = line_;
    size_t start_col = col_;
    std::string value;

    while (!IsAtEnd() && (std::isalnum(static_cast<unsigned char>(Peek())) || Peek() == '_' || Peek() == '.' || Peek() == '/')) {
        value += Advance();
    }"""

new_ident = """Token Lexer::ReadIdentifierOrKeyword() {
    size_t start_line = line_;
    size_t start_col = col_;
    size_t start_pos = pos_;

    while (!IsAtEnd() && (std::isalnum(static_cast<unsigned char>(Peek())) || Peek() == '_' || Peek() == '.' || Peek() == '/')) {
        Advance();
    }
    std::string value = source_.substr(start_pos, pos_ - start_pos);"""

# Replace ReadNumber
old_num = """Token Lexer::ReadNumber() {
    size_t start_line = line_;
    size_t start_col = col_;
    std::string value;

    while (!IsAtEnd() && std::isdigit(static_cast<unsigned char>(Peek()))) {
        value += Advance();
    }
    
    if (!IsAtEnd() && Peek() == '.') {
        value += Advance();
        while (!IsAtEnd() && std::isdigit(static_cast<unsigned char>(Peek()))) {
            value += Advance();
        }
    }"""

new_num = """Token Lexer::ReadNumber() {
    size_t start_line = line_;
    size_t start_col = col_;
    size_t start_pos = pos_;

    while (!IsAtEnd() && std::isdigit(static_cast<unsigned char>(Peek()))) {
        Advance();
    }
    
    if (!IsAtEnd() && Peek() == '.') {
        Advance();
        while (!IsAtEnd() && std::isdigit(static_cast<unsigned char>(Peek()))) {
            Advance();
        }
    }
    std::string value = source_.substr(start_pos, pos_ - start_pos);"""

if old_ident in content and old_num in content:
    content = content.replace(old_ident, new_ident)
    content = content.replace(old_num, new_num)
    with open("src/lexer/lexer.cpp", "w") as f:
        f.write(content)
    print("Success")
else:
    print("Failed")
