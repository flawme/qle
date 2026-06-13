import sys

with open('tests/test_main.cpp', 'r') as f:
    lines = f.readlines()

# find first `#include "lexer/lexer.h"` after line 100
dup_idx = -1
for i in range(300, len(lines)):
    if '#include "lexer/lexer.h"' in lines[i]:
        dup_idx = i
        break

if dup_idx != -1:
    lines = lines[:dup_idx]

extreme_tests = """

void TestYamlAdapter() {
    std::cout << "Running Yaml Adapter Tests..." << std::endl;
    std::ofstream out("tests/test.yaml");
    out << "- id: 1\\n  name: John\\n  age: 30\\n- id: 2\\n  name: Jane\\n  age: 25\\n";
    out.close();

    lexer::Lexer lexer("from tests/test.yaml select id, name, age");
    auto tokens = lexer.Tokenize();
    parser::Parser parser(tokens);
    auto query = parser.Parse();
    
    runtime::Runtime runtime;
    runtime.Execute(query.get());
    
    // Extreme Yaml Tests
    std::ofstream malformed("tests/malformed.yaml");
    malformed << "- id: 1\\n  name: [\\n- id: 2";
    malformed.close();
    
    try {
        lexer::Lexer l("from tests/malformed.yaml select *");
        parser::Parser p(l.Tokenize());
        runtime::Runtime rt;
        rt.Execute(p.Parse().get());
    } catch (const errors::QleException&) {}

    // Limits in Yaml
    std::ofstream big_yaml("tests/big.yaml");
    for(int i=0; i<50000; i++) big_yaml << "- a: 1\\n";
    big_yaml.close();
    
    size_t old_size = security::Limits::Get().max_file_size;
    security::Limits::Get().max_file_size = 100;
    try {
        lexer::Lexer l("from tests/big.yaml select *");
        parser::Parser p(l.Tokenize());
        runtime::Runtime rt;
        rt.Execute(p.Parse().get());
        assert(false && "Should have thrown file size limit");
    } catch (const errors::SecurityError&) {}
    security::Limits::Get().max_file_size = old_size;
}

void TestJoin() {
    std::cout << "Running Join Tests..." << std::endl;
    std::ofstream fu("tests/users.csv");
    fu << "id,name\\n1,Alice\\n2,Bob\\n3,Charlie\\n";
    fu.close();
    
    std::ofstream fo("tests/orders.csv");
    fo << "order_id,user_id,item\\n101,1,Apple\\n102,1,Banana\\n103,2,Carrot\\n104,4,Date\\n";
    fo.close();
    
    try {
        std::string query = "from tests/users.csv join tests/orders.csv on id == user_id select name, item";
        qle::lexer::Lexer lexer(query);
        qle::parser::Parser parser(lexer.Tokenize());
        qle::runtime::Runtime rt;
        rt.Execute(parser.Parse().get());
    } catch (const std::exception& e) {
        std::cerr << "Join test failed: " << e.what() << std::endl;
    }
    
    // Edge case: missing keys, invalid conditions
    try {
        std::string query = "from tests/users.csv join tests/orders.csv on id == missing_key select name";
        qle::lexer::Lexer lexer(query);
        qle::parser::Parser parser(lexer.Tokenize());
        qle::runtime::Runtime rt;
        rt.Execute(parser.Parse().get());
        assert(false && "Should have thrown for missing key");
    } catch (const errors::QleException&) {}

    // Security: Cartesian product memory/time bypass
    size_t old_limit = security::Limits::Get().max_rows_processed;
    security::Limits::Get().max_rows_processed = 100;
    
    std::ofstream t1("tests/t1.csv"); t1 << "id\\n1\\n2\\n"; t1.close();
    std::ofstream t2("tests/t2.csv"); t2 << "id\\n";
    for(int i=0; i<1000; i++) t2 << i << "\\n";
    t2.close();
    
    try {
        std::string query = "from tests/t1.csv join tests/t2.csv on id == id select id";
        qle::lexer::Lexer lexer(query);
        qle::parser::Parser parser(lexer.Tokenize());
        qle::runtime::Runtime rt;
        rt.Execute(parser.Parse().get());
        assert(false && "Join should have respected max rows processed");
    } catch (const errors::SecurityError&) {}
    security::Limits::Get().max_rows_processed = old_limit;
}

// Ensure TestInlineFunctions has the extreme math functions
"""

with open('tests/test_main.cpp', 'w') as f:
    f.writelines(lines)
    f.write(extreme_tests)
