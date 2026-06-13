import sys

with open('tests/test_main.cpp', 'r') as f:
    content = f.read()

new_tests = """
void TestHashJoinMemoryBypass() {
    std::cout << "Running Hash Join Memory Bypass Test..." << std::endl;
    std::ofstream right_table("right_huge.csv");
    right_table << "id,value\\n";
    std::string large_val(10000, 'A');
    for (int i = 0; i < 60000; i++) {
        right_table << i << "," << large_val << "\\n";
    }
    right_table.close();

    std::ofstream left_table("left_small.csv");
    left_table << "id\\n1\\n";
    left_table.close();

    size_t old_limit = security::Limits::Get().max_rows_processed;
    security::Limits::Get().max_rows_processed = 1000000;
    
    size_t old_size = security::Limits::Get().max_file_size;
    security::Limits::Get().max_file_size = 1000 * 1024 * 1024;
    
    try {
        std::string query = "from left_small.csv join right_huge.csv on id == id select id, value";
        qle::lexer::Lexer lexer(query);
        qle::parser::Parser parser(lexer.Tokenize());
        qle::runtime::Runtime rt;
        rt.Execute(parser.Parse().get());
        assert(false && "Should have thrown for memory overflow");
    } catch (const errors::SecurityError&) {}
    
    security::Limits::Get().max_rows_processed = old_limit;
    security::Limits::Get().max_file_size = old_size;
}

void TestLexerEdgeCases() {
    std::cout << "Running Lexer Edge Cases..." << std::endl;
    std::string queries[] = {
        "",
        "from \\0",
        "select .",
        "from \"unterminated string",
        "select 123.",
        "select 123.a",
        "from test.csv select *"
    };
    for (const auto& q : queries) {
        try {
            qle::lexer::Lexer lexer(q);
            lexer.Tokenize();
        } catch (const qle::errors::LexerError&) {}
    }
}
"""

if "TestHashJoinMemoryBypass" not in content:
    # insert before int main()
    pos = content.rfind("int main()")
    content = content[:pos] + new_tests + content[pos:]
    
    # insert calls in main()
    main_body = "    TestHashJoinMemoryBypass();\\n    TestLexerEdgeCases();\\n"
    pos = content.find("return 0;", pos)
    content = content[:pos] + main_body + content[pos:]
    
    with open('tests/test_main.cpp', 'w') as f:
        f.write(content)
    print("Success")
else:
    print("Already patched")
