import sys

with open("src/lexer/lexer.cpp", "r") as f:
    content = f.read()

old_str = """Token Lexer::ReadString() {
    size_t start_line = line_;
    size_t start_col = col_;
    char quote = Advance(); // Consume opening quote
    std::string value;

    while (!IsAtEnd() && Peek() != quote) {
        value += Advance();
        if (value.length() > security::Limits::Get().max_string_length) {
            throw errors::SecurityError("String length exceeds maximum allowed limit", start_line, start_col);
        }
    }"""

new_str = """Token Lexer::ReadString() {
    size_t start_line = line_;
    size_t start_col = col_;
    char quote = Advance(); // Consume opening quote
    size_t start_pos = pos_;
    size_t length = 0;

    while (!IsAtEnd() && Peek() != quote) {
        Advance();
        length++;
        if (length > security::Limits::Get().max_string_length) {
            throw errors::SecurityError("String length exceeds maximum allowed limit", start_line, start_col);
        }
    }
    std::string value = source_.substr(start_pos, length);"""

if old_str in content:
    content = content.replace(old_str, new_str)
    with open("src/lexer/lexer.cpp", "w") as f:
        f.write(content)
    print("Success")
else:
    print("Failed")
