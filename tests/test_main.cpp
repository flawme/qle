#include "lexer/lexer.h"
#include "parser/parser.h"
#include "runtime/runtime.h"
#include "errors/errors.h"
#include "security/limits.h"
#include "logger/logger.h"
#include <iostream>
#include <cassert>
#include <fstream>
#include <memory>
#include <array>
#include <cstdio>
#include <thread>

using namespace qle;
void TestAggregationsAndGroupBy();
void TestInlineFunctions();
void TestSqliteAdapter();
void TestJoin();
void TestReplEdgeCases();
void TestCliLimits();
void TestMaxRowsLimit();
void TestTimeoutLimit();

void TestYamlAdapter();
void TestParquetAdapter();
void TestLike();
void TestXmlAdapter();
void TestSubqueries();
void TestAdversarialNewFeatures();

void TestLexer() {
    std::cout << "Running Lexer Tests..." << std::endl;
    lexer::Lexer lexer("from users where age > 18 select name");
    auto tokens = lexer.Tokenize();
    assert(tokens.size() == 9);
}

void TestParser() {
    std::cout << "Running Parser Tests..." << std::endl;
    lexer::Lexer lexer("from users where age > 18 select name");
    auto tokens = lexer.Tokenize();
    parser::Parser parser(tokens);
    auto ast = parser.Parse();
    assert(ast != nullptr);
}

void TestSecurity() {
    std::cout << "Running Security Tests..." << std::endl;
    std::string long_string = "from a where b == \"";
    long_string.append(9000, 'A'); 
    long_string += "\" select c";
    bool caught = false;
    try {
        lexer::Lexer lexer(long_string);
        lexer.Tokenize();
    } catch (const errors::SecurityError&) {
        caught = true;
    }
    assert(caught);
}

void TestRecursionDepth() {
    std::cout << "Running Recursion Depth Tests..." << std::endl;
    std::string query = "from users where ";
    for(int i = 0; i < 150; ++i) query += "(";
    query += "a > b";
    for(int i = 0; i < 150; ++i) query += ")";
    query += " select a";
    bool caught = false;
    try {
        lexer::Lexer lexer(query);
        parser::Parser parser(lexer.Tokenize());
        parser.Parse();
    } catch (const errors::SecurityError&) { caught = true; }
    assert(caught);
}

void TestDynamicLimits() {
    std::cout << "Running Dynamic Limits Tests..." << std::endl;
    size_t old_limit = security::Limits::Get().max_ast_nodes;
    security::Limits::Get().max_ast_nodes = 3;
    bool caught = false;
    try {
        lexer::Lexer lexer("from users where a > b select c, d");
        parser::Parser parser(lexer.Tokenize());
        parser.Parse();
    } catch (const errors::SecurityError&) { caught = true; }
    security::Limits::Get().max_ast_nodes = old_limit;
    assert(caught);
}

void TestAdversarialParsing() {
    std::cout << "Running Adversarial Parsing Tests..." << std::endl;
    std::string queries[] = {
        "from users order by",
        "from users select",
        "from users where",
        "from users limit",
        "from users limit abc",
        "from users limit 0",
        "from users limit 9999999999999999999999999999999999999999999"
    };
    for (const auto& q : queries) {
        try {
            lexer::Lexer lexer(q);
            parser::Parser parser(lexer.Tokenize());
            parser.Parse();
            assert(false && "Should have thrown for invalid query");
        } catch (const errors::QleException&) {}
    }
}

void TestMalformedDataSources() {
    std::cout << "Running Malformed Data Sources Tests..." << std::endl;
    // Broken CSV quotes, missing JSON braces
    std::ofstream bad_csv("bad_quotes.csv");
    bad_csv << "id,name\n1,\"unclosed quote\n2,bob\n";
    bad_csv.close();

    std::ofstream bad_json("bad_braces.json");
    bad_json << "{ \"id\": 1, \"name\": \"alice\" "; // Missing closing brace
    bad_json.close();

    try {
        runtime::Runtime rt;
        lexer::Lexer lexer("from bad_quotes.csv select *");
        parser::Parser parser(lexer.Tokenize());
        rt.Execute(parser.Parse().get());
    } catch (const errors::QleException&) {}

    try {
        runtime::Runtime rt;
        lexer::Lexer lexer("from bad_braces.json select *");
        parser::Parser parser(lexer.Tokenize());
        rt.Execute(parser.Parse().get());
    } catch (const errors::QleException&) {}

    try {
        runtime::Runtime rt;
        lexer::Lexer lexer("from malformed.json select *");
        parser::Parser parser(lexer.Tokenize());
        rt.Execute(parser.Parse().get());
    } catch (const errors::QleException&) {}

    try {
        runtime::Runtime rt;
        lexer::Lexer lexer("from empty.json select *");
        parser::Parser parser(lexer.Tokenize());
        rt.Execute(parser.Parse().get());
    } catch (const errors::QleException&) {}
}

void TestBoundaryConditions() {
    std::cout << "Running Boundary Conditions Tests..." << std::endl;
    std::string query = "from malformed.csv where name > 5 select id";
    try {
        runtime::Runtime rt;
        lexer::Lexer lexer(query);
        parser::Parser parser(lexer.Tokenize());
        rt.Execute(parser.Parse().get());
    } catch (const errors::QleException&) {}
    
    try {
        lexer::Lexer lexer("from \xff select *");
        lexer.Tokenize();
    } catch (const errors::QleException&) {}

    // Type coercion (string vs float), Arithmetic overflow
    std::string queries[] = {
        "from empty.json where a == \"string\" > 10 select a",
        "from empty.json select 9999999999999999999999999999999999999999",
        "from ../../../etc/passwd select *", // File path manipulation
        "from empty.json select limit 5" // Unexpected grammar combinations
    };
    for (const auto& q : queries) {
        try {
            lexer::Lexer lexer(q);
            parser::Parser parser(lexer.Tokenize());
            runtime::Runtime rt;
            rt.Execute(parser.Parse().get());
        } catch (const errors::QleException&) {}
    }
}


void TestHashJoinMemoryBypass() {
    std::cout << "Running Hash Join Memory Bypass Test..." << std::endl;
    std::ofstream right_table("right_huge.csv");
    right_table << "id,value\n";
    std::string large_val(20000, 'A');
    for (int i = 0; i < 60000; i++) {
        right_table << i << "," << large_val << "\n";
    }
    right_table.close();

    std::ofstream left_table("left_small.csv");
    left_table << "id\n1\n";
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
        "from \0",
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
int main() {
    Debug::Enable(false); 
    TestLexer();
    TestYamlAdapter();
    TestXmlAdapter();
    TestSqliteAdapter();
    TestSecurity();
    TestRecursionDepth();
    TestDynamicLimits();
    TestAdversarialParsing();
    TestMalformedDataSources();
    TestBoundaryConditions();
    TestSubqueries();
    TestAdversarialNewFeatures();
    TestAggregationsAndGroupBy();
    TestInlineFunctions();
    TestReplEdgeCases();
    TestJoin();
    TestLike();
    TestCliLimits();
    TestMaxRowsLimit();
    TestTimeoutLimit();
    TestParquetAdapter();
    std::cout << "All tests passed successfully!" << std::endl;
}

void TestParquetAdapter() {
    std::cout << "Running Parquet Adapter Tests..." << std::endl;
    std::ifstream src("../third_party/tinyparquet/testing/parquet-testing/data/alltypes_plain.parquet", std::ios::binary);
    if (!src.good()) {
        src.open("third_party/tinyparquet/testing/parquet-testing/data/alltypes_plain.parquet", std::ios::binary);
    }
    std::ofstream dst("alltypes_plain.parquet", std::ios::binary);
    dst << src.rdbuf();
    dst.close();
    std::string parquet_file = "alltypes_plain.parquet";
    
    std::string query = "from \"" + parquet_file + "\" select id, int_col, string_col limit 5";
    try {
        lexer::Lexer lexer(query);
        auto tokens = lexer.Tokenize();
        parser::Parser parser(tokens);
        auto ast = parser.Parse();
        
        runtime::Runtime runtime;
        runtime.Execute(ast.get());
    } catch (const std::exception& e) {
        std::cerr << "Parquet adapter test failed: " << e.what() << std::endl;
        assert(false);
    }
}

void TestXmlAdapter() {
    std::cout << "Running XML Adapter Tests..." << std::endl;
    std::ofstream out("test.xml");
    out << "<?xml version=\"1.0\"?>\n<rows>\n  <row>\n    <id>1</id>\n    <name>Alice</name>\n    <age>30</age>\n  </row>\n  <row>\n    <id>2</id>\n    <name>Bob</name>\n    <age>25</age>\n  </row>\n</rows>";
    out.close();

    lexer::Lexer lexer("from test.xml select id, name, age");
    auto tokens = lexer.Tokenize();
    parser::Parser parser(tokens);
    auto query = parser.Parse();
    
    runtime::Runtime runtime;
    runtime.Execute(query.get());
    
    // Extreme XML Tests
    std::ofstream malformed("malformed.xml");
    malformed << "<rows><row><id>1</id><name></row></rows>";
    malformed.close();
    
    try {
        lexer::Lexer l("from malformed.xml select *");
        parser::Parser p(l.Tokenize());
        runtime::Runtime rt;
        rt.Execute(p.Parse().get());
    } catch (const errors::QleException&) {}

    // Limits in XML
    std::ofstream big_xml("big.xml");
    big_xml << "<rows>";
    for(int i=0; i<50000; i++) big_xml << "<row><a>1</a></row>";
    big_xml << "</rows>";
    big_xml.close();
    
    size_t old_size = security::Limits::Get().max_file_size;
    security::Limits::Get().max_file_size = 100;
    try {
        lexer::Lexer l("from big.xml select *");
        parser::Parser p(l.Tokenize());
        runtime::Runtime rt;
        rt.Execute(p.Parse().get());
        assert(false && "Should have thrown file size limit");
    } catch (const errors::SecurityError&) {}
    security::Limits::Get().max_file_size = old_size;
}

void TestLike() {
    std::cout << "Running LIKE Operator Tests..." << std::endl;
    std::ofstream tmp("like_test.csv");
    tmp << "id,name\n1,Alice\n2,Bob\n3,Charlie\n4,David\n5,Eve\n";
    tmp.close();

    std::string queries[] = {
        "from like_test.csv where name like \"A%\" select id",
        "from like_test.csv where name like \"%e\" select id",
        "from like_test.csv where name like \"_o_\" select id",
        "from like_test.csv where name like \"%i%\" select id"
    };
    
    for (const auto& q : queries) {
        try {
            lexer::Lexer lexer(q);
            parser::Parser parser(lexer.Tokenize());
            runtime::Runtime rt;
            rt.Execute(parser.Parse().get());
        } catch (const errors::QleException& e) {
            std::cerr << "LIKE test failed: " << e.what() << std::endl;
            assert(false);
        }
    }
}


void TestAdversarialNewFeatures() {
    std::cout << "Running Adversarial Tests for New Features..." << std::endl;
    { std::ofstream out("empty.json"); out << "[{\"id\":1}]"; out.close(); }

    // 1. LIKE ReDoS / hanging pattern test
    std::string text(500, 'a');
    std::string pattern = "%";
    for(int i=0; i<400; i++) pattern += "a";
    pattern += "b";
    std::string like_query = "from empty.json where \"" + text + "\" like \"" + pattern + "\" select id";
    bool like_caught = false;
    try {
        qle::lexer::Lexer lexer(like_query);
        qle::parser::Parser parser(lexer.Tokenize());
        qle::runtime::Runtime rt;
        rt.Execute(parser.Parse().get());
    } catch (const qle::errors::SecurityError&) {
        like_caught = true;
    }
    assert(like_caught && "LIKE ReDoS should throw SecurityError");

    // 2. Date parser with potentially dangerous large inputs (UB on overflow)
    std::string date_query = "from empty.json where year(\"2147483647-01-01\") == \"2147485547\" select id";
    try {
        qle::lexer::Lexer lexer(date_query);
        qle::parser::Parser parser(lexer.Tokenize());
        qle::runtime::Runtime rt;
        rt.Execute(parser.Parse().get());
    } catch (const qle::errors::QleException&) {}

    // 3. Subqueries recursive infinite loops test
    std::string sub_query = "from ";
    for (int i = 0; i < 200; ++i) {
        sub_query += "(from ";
    }
    sub_query += "empty.json select id";
    for (int i = 0; i < 200; ++i) {
        sub_query += ") select id";
    }
    bool sub_caught = false;
    try {
        qle::lexer::Lexer lexer(sub_query);
        qle::parser::Parser parser(lexer.Tokenize());
        parser.Parse();
    } catch (const qle::errors::SecurityError&) {
        sub_caught = true;
    }
    assert(sub_caught && "Deeply nested subqueries should throw SecurityError due to max recursion depth");
    
    // 4. XML adapter infinite depth / unclosed tags test
    std::ofstream malformed_xml("deep.xml");
    malformed_xml << "<rows><row>";
    for (int i = 0; i < 100; i++) malformed_xml << "<col>";
    malformed_xml.close();
    try {
        qle::lexer::Lexer lexer("from deep.xml select *");
        qle::parser::Parser parser(lexer.Tokenize());
        qle::runtime::Runtime rt;
        rt.Execute(parser.Parse().get());
    } catch (const qle::errors::QleException&) {}
}

void TestSubqueries() {
    std::cout << "Running Subqueries Tests..." << std::endl;
    std::ofstream out("sub_users.csv");
    out << "id,name,age\n1,Alice,30\n2,Bob,25\n3,Charlie,35\n";
    out.close();

    std::string queries[] = {
        "from (from sub_users.csv where age > 25 select id, name) select name",
        "from (from sub_users.csv select id) join sub_users.csv on id == id select id",
        "from (from sub_users.csv select id limit 1) select id"
    };

    for (const auto& q : queries) {
        try {
            lexer::Lexer lexer(q);
            parser::Parser parser(lexer.Tokenize());
            runtime::Runtime rt;
            rt.Execute(parser.Parse().get());
        } catch (const errors::QleException& e) {
            std::cerr << "Subquery test failed: " << e.what() << std::endl;
            assert(false);
        }
    }
}

void TestAggregationsAndGroupBy() {
    std::cout << "Running Aggregations & Group By Tests..." << std::endl;
    std::string queries[] = {
        "from empty.json select sum(id) group by id",
        "from empty.json select min(id)",
        "from empty.json select avg(id)",
        "from empty.json select count(id) group by non_existent"
    };
    for (const auto& q : queries) {
        try {
            lexer::Lexer lexer(q);
            parser::Parser parser(lexer.Tokenize());
            runtime::Runtime rt;
            rt.Execute(parser.Parse().get());
        } catch (const errors::QleException&) {}
    }
}

void TestInlineFunctions() {
    std::cout << "Running Inline Functions Tests..." << std::endl;
    
    std::ofstream tmp("math_test.csv");
    tmp << "id,val\n1,10.5\n2,-5.2\n3,inf\n4,nan\n5,1e1000\n6,not_a_number\n";
    tmp.close();

    std::string queries[] = {
        "from math_test.csv select upper()",
        "from math_test.csv select lower(\"abc\", \"def\")",
        "from math_test.csv select length()",
        "from math_test.csv select upper(\"\xFF\xFF\")",
        "from math_test.csv select non_existent_func(1)",
        "from math_test.csv select abs(val)",
        "from math_test.csv select round(val)",
        "from math_test.csv select now()",
        "from math_test.csv select year(\"2023-05-15\")",
        "from math_test.csv select month(\"2023-05-15\")",
        "from math_test.csv select day(\"2023-05-15\")",
        "from math_test.csv select abs(\"extremely_long_string_that_is_not_a_number_at_all\")"
    };
    for (const auto& q : queries) {
        try {
            lexer::Lexer lexer(q);
            parser::Parser parser(lexer.Tokenize());
            runtime::Runtime rt;
            rt.Execute(parser.Parse().get());
        } catch (const errors::QleException&) {}
    }
}

void TestSqliteAdapter() {
    std::cout << "Running SQLite Adapter Tests..." << std::endl;
    std::string queries[] = {
        "from nonexistent.sqlite:nonexistent_table select *",
        "from :memory::nonexistent_table select *",
        "from test.db:my_table select *"
    };
    for (const auto& q : queries) {
        try {
            lexer::Lexer lexer(q);
            parser::Parser parser(lexer.Tokenize());
            runtime::Runtime rt;
            rt.Execute(parser.Parse().get());
        } catch (const errors::QleException&) {}
    }
}

void TestReplEdgeCases() {
    std::cout << "Running REPL Edge Cases..." << std::endl;
}

// Helper to run shell commands and capture output
std::string ExecCmd(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) return "";
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

void TestCliLimits() {
    std::cout << "Running CLI Limits Tests..." << std::endl;
    // We assume `qle` binary is available in the parent directory (build dir) or current dir
    // Test that passing invalid numeric values does not crash the program but outputs an error
    std::string out1 = ExecCmd("./qle --max-rows abc 2>&1");
    if (out1.empty() || out1.find("not found") != std::string::npos || out1.find("No such file") != std::string::npos) {
        out1 = ExecCmd("../qle --max-rows abc 2>&1");
    }
    assert(out1.find("Command line parsing error") != std::string::npos || out1.find("Error") != std::string::npos);

    std::string out2 = ExecCmd("./qle --max-file-size 1 empty.json 2>&1");
    if (out2.empty() || out2.find("not found") != std::string::npos || out2.find("No such file") != std::string::npos) {
        out2 = ExecCmd("../qle --max-file-size 1 empty.json 2>&1");
    }
    assert(out2.find("exceeds maximum allowed size") != std::string::npos || out2.find("Error") != std::string::npos);
}

void TestMaxRowsLimit() {
    std::cout << "Running Max Rows Limit Tests..." << std::endl;
    std::remove("million.csv");
    std::ofstream out("million.csv");
    out << "id,val\n";
    for (int i = 0; i < 20000; ++i) {
        out << i << ",test\n";
    }
    out.close();

    size_t old_limit = security::Limits::Get().max_rows_processed;
    security::Limits::Get().max_rows_processed = 10000; // Set to lower threshold
    bool caught = false;
    try {
        runtime::Runtime rt;
        lexer::Lexer lexer("from million.csv select *");
        parser::Parser parser(lexer.Tokenize());
        rt.Execute(parser.Parse().get());
    } catch (const errors::SecurityError& e) {
        caught = true;
    }
    security::Limits::Get().max_rows_processed = old_limit;
    assert(caught && "Processing beyond max rows limit should throw SecurityError");
}

void TestTimeoutLimit() {
    std::cout << "Running Timeout Limit Tests..." << std::endl;
    size_t old_timeout = security::Limits::Get().max_execution_time_ms;
    security::Limits::Get().max_execution_time_ms = 0; // Extremely low timeout
    bool caught = false;
    try {
        runtime::Runtime rt;
        lexer::Lexer lexer("from million.csv select *");
        parser::Parser parser(lexer.Tokenize());
        // Simulating slow processing by relying on the 0ms timeout check inside the first 10k rows
        rt.Execute(parser.Parse().get());
    } catch (const errors::SecurityError& e) {
        if (std::string(e.what()).find("Maximum execution time exceeded") != std::string::npos) {
            caught = true;
        }
    }
    security::Limits::Get().max_execution_time_ms = old_timeout;
    assert(caught && "Hanging or slow query should throw SecurityError on timeout");
}


void TestYamlAdapter() {
    std::cout << "Running Yaml Adapter Tests..." << std::endl;
    std::ofstream out("test.yaml");
    out << "- id: 1\n  name: John\n  age: 30\n- id: 2\n  name: Jane\n  age: 25\n";
    out.close();

    lexer::Lexer lexer("from test.yaml select id, name, age");
    auto tokens = lexer.Tokenize();
    parser::Parser parser(tokens);
    auto query = parser.Parse();
    
    runtime::Runtime runtime;
    runtime.Execute(query.get());
    
    // Extreme Yaml Tests
    std::ofstream malformed("malformed.yaml");
    malformed << "- id: 1\n  name: [\n- id: 2";
    malformed.close();
    
    try {
        lexer::Lexer l("from malformed.yaml select *");
        parser::Parser p(l.Tokenize());
        runtime::Runtime rt;
        rt.Execute(p.Parse().get());
    } catch (const errors::QleException&) {}

    // Limits in Yaml
    std::ofstream big_yaml("big.yaml");
    for(int i=0; i<50000; i++) big_yaml << "- a: 1\n";
    big_yaml.close();
    
    size_t old_size = security::Limits::Get().max_file_size;
    security::Limits::Get().max_file_size = 100;
    try {
        lexer::Lexer l("from big.yaml select *");
        parser::Parser p(l.Tokenize());
        runtime::Runtime rt;
        rt.Execute(p.Parse().get());
        assert(false && "Should have thrown file size limit");
    } catch (const errors::SecurityError&) {}
    security::Limits::Get().max_file_size = old_size;
}

void TestJoin() {
    std::cout << "Running Join Tests..." << std::endl;
    std::ofstream fu("users.csv");
    fu << "id,name\n1,Alice\n2,Bob\n3,Charlie\n";
    fu.close();
    
    std::ofstream fo("orders.csv");
    fo << "order_id,user_id,item\n101,1,Apple\n102,1,Banana\n103,2,Carrot\n104,4,Date\n";
    fo.close();
    
    try {
        std::string query = "from users.csv join orders.csv on id == user_id select name, item";
        qle::lexer::Lexer lexer(query);
        qle::parser::Parser parser(lexer.Tokenize());
        qle::runtime::Runtime rt;
        rt.Execute(parser.Parse().get());
    } catch (const std::exception& e) {
        std::cerr << "Join test failed: " << e.what() << std::endl;
    }
    
    // Edge case: missing keys, invalid conditions
    try {
        std::string query = "from users.csv join orders.csv on id == missing_key select name";
        qle::lexer::Lexer lexer(query);
        qle::parser::Parser parser(lexer.Tokenize());
        qle::runtime::Runtime rt;
        rt.Execute(parser.Parse().get());
        assert(false && "Should have thrown for missing key");
    } catch (const errors::QleException&) {}

    // Security: Cartesian product memory/time bypass
    size_t old_limit = security::Limits::Get().max_rows_processed;
    security::Limits::Get().max_rows_processed = 100;
    
    std::ofstream t1("t1.csv"); t1 << "id\n1\n2\n"; t1.close();
    std::ofstream t2("t2.csv"); t2 << "id\n";
    for(int i=0; i<1000; i++) t2 << i << "\n";
    t2.close();
    
    try {
        std::string query = "from t1.csv join t2.csv on id == id select id";
        qle::lexer::Lexer lexer(query);
        qle::parser::Parser parser(lexer.Tokenize());
        qle::runtime::Runtime rt;
        rt.Execute(parser.Parse().get());
        assert(false && "Join should have respected max rows processed");
    } catch (const errors::SecurityError&) {}
    security::Limits::Get().max_rows_processed = old_limit;
}

// Ensure TestInlineFunctions has the extreme math functions
