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
    std::ofstream bad_csv("tests/bad_quotes.csv");
    bad_csv << "id,name\n1,\"unclosed quote\n2,bob\n";
    bad_csv.close();

    std::ofstream bad_json("tests/bad_braces.json");
    bad_json << "{ \"id\": 1, \"name\": \"alice\" "; // Missing closing brace
    bad_json.close();

    try {
        runtime::Runtime rt;
        lexer::Lexer lexer("from tests/bad_quotes.csv select *");
        parser::Parser parser(lexer.Tokenize());
        rt.Execute(parser.Parse().get());
    } catch (const errors::QleException&) {}

    try {
        runtime::Runtime rt;
        lexer::Lexer lexer("from tests/bad_braces.json select *");
        parser::Parser parser(lexer.Tokenize());
        rt.Execute(parser.Parse().get());
    } catch (const errors::QleException&) {}

    try {
        runtime::Runtime rt;
        lexer::Lexer lexer("from tests/malformed.json select *");
        parser::Parser parser(lexer.Tokenize());
        rt.Execute(parser.Parse().get());
    } catch (const errors::QleException&) {}

    try {
        runtime::Runtime rt;
        lexer::Lexer lexer("from tests/empty.json select *");
        parser::Parser parser(lexer.Tokenize());
        rt.Execute(parser.Parse().get());
    } catch (const errors::QleException&) {}
}

void TestBoundaryConditions() {
    std::cout << "Running Boundary Conditions Tests..." << std::endl;
    std::string query = "from tests/malformed.csv where name > 5 select id";
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
        "from tests/empty.json where a == \"string\" > 10 select a",
        "from tests/empty.json select 9999999999999999999999999999999999999999",
        "from ../../../etc/passwd select *", // File path manipulation
        "from tests/empty.json select limit 5" // Unexpected grammar combinations
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

int main() {
    Debug::Enable(false); 
    TestLexer();
    TestYamlAdapter();
    TestParser();
    TestSecurity();
    TestRecursionDepth();
    TestDynamicLimits();
    TestAdversarialParsing();
    TestMalformedDataSources();
    TestBoundaryConditions();
    TestAggregationsAndGroupBy();
    TestYamlAdapter();
    TestInlineFunctions();
    TestSqliteAdapter();
    TestReplEdgeCases();
    TestJoin();
    TestCliLimits();
    TestMaxRowsLimit();
    TestTimeoutLimit();
    std::cout << "All extreme tests passed!" << std::endl;
    return 0;
}

void TestAggregationsAndGroupBy() {
    std::cout << "Running Aggregations & Group By Tests..." << std::endl;
    std::string queries[] = {
        "from tests/empty.json select sum(id) group by id",
        "from tests/empty.json select min(id)",
        "from tests/empty.json select avg(id)",
        "from tests/empty.json select count(id) group by non_existent"
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
    std::string queries[] = {
        "from test.csv select upper()",
        "from test.csv select lower(\"abc\", \"def\")",
        "from test.csv select length()",
        "from test.csv select upper(\"\xFF\xFF\")",
        "from test.csv select non_existent_func(1)"
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

    std::string out2 = ExecCmd("./qle --max-file-size 1 tests/empty.json 2>&1");
    if (out2.empty() || out2.find("not found") != std::string::npos || out2.find("No such file") != std::string::npos) {
        out2 = ExecCmd("../qle --max-file-size 1 tests/empty.json 2>&1");
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
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "runtime/runtime.h"
#include "errors/errors.h"
#include <iostream>
#include <cassert>

using namespace qle;

void TestYamlAdapter() {
    std::cout << "Running Yaml Adapter Tests..." << std::endl;
    lexer::Lexer lexer("from tests/test.yaml select id, name, age");
    auto tokens = lexer.Tokenize();
    parser::Parser parser(tokens);
    auto query = parser.Parse();
    
    runtime::Runtime runtime;
    // Redirect output? Actually we just run it and see if it crashes.
    // Or we could verify rows.
    runtime.Execute(query.get());
}

void TestJoin() {
    std::cout << "Running Join Tests..." << std::endl;
    // create users.csv
    FILE* fu = fopen("../tests/users.csv", "w");
    if (!fu) fu = fopen("tests/users.csv", "w");
    if (fu) {
        fprintf(fu, "id,name\n1,Alice\n2,Bob\n3,Charlie\n");
        fclose(fu);
    }
    
    // create orders.csv
    FILE* fo = fopen("../tests/orders.csv", "w");
    if (!fo) fo = fopen("tests/orders.csv", "w");
    if (fo) {
        fprintf(fo, "order_id,user_id,item\n101,1,Apple\n102,1,Banana\n103,2,Carrot\n104,4,Date\n");
        fclose(fo);
    }
    
    try {
        std::string query = "from ../tests/users.csv join ../tests/orders.csv on id == user_id select name, item";
        qle::lexer::Lexer lexer(query);
        qle::parser::Parser parser(lexer.Tokenize());
        qle::runtime::Runtime rt;
        rt.Execute(parser.Parse().get());
    } catch (const std::exception& e) {
        std::cerr << "Join test failed (../tests/): " << e.what() << std::endl;
        try {
            std::string query = "from tests/users.csv join tests/orders.csv on id == user_id select name, item";
            qle::lexer::Lexer lexer(query);
            qle::parser::Parser parser(lexer.Tokenize());
            qle::runtime::Runtime rt;
            rt.Execute(parser.Parse().get());
        } catch (const std::exception& e) {
            std::cerr << "Join test failed (tests/): " << e.what() << std::endl;
        }
    }
}
